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
 * $Id: rpdXLogfileMgr.cpp
 ******************************************************************************/

#include <smi.h>
#include <mtc.h>
#include <rpdXLogfileMgr.h>
#include <rpuProperty.h>

/*********************************************************************
 * FUNCTION DESCRIPTION : rpdXLogfileMgr::initialize                 *
 * ------------------------------------------------------------------*
 * ���� ���ڸ� �ʱ�ȭ ��Ų �� replication meta���� read ������ �����´�.
 * ���� write ������ read ������ �����.
 *
 * aReplName        - [IN] �ش� replication�� �̸�
 * aXLogLSN         - [IN] replication meta�� ����� ���������� ���� xlog�� ��ġ
 * aExceedTimeout   - [IN] read ��� �ð� ����(�ش� �ð����� ��� ����� ��� timeout ó��)
 * aExitFlag        - [IN] read ���� ��⸦ ���� ���� ���� flag�� ������
 * aReceiverPtr     - [IN] �ش� xlogfile manager�� ����ϴ� receiver�� ������
 *********************************************************************/
IDE_RC rpdXLogfileMgr::initialize( SChar *      aReplName,
                                   rpXLogLSN    aInitXLogLSN,
                                   UInt         aExceedTimeout,
                                   idBool *     aExitFlag,
                                   void *       aReceiverPtr,
                                   rpXLogLSN    aLastXLogLSN)
{
    UInt    sReadXLogfileNo = 0;
    SChar   sBuffer[IDU_MUTEX_NAME_LEN];
    UInt    sState = 0;
    SInt    sLengthCheck = 0;
    UInt    sLastFileNo;
    /* Ŭ���� ���� ���� �ʱ�ȭ */

    /* write ���� ���� */
    mWriteXLogLSN  = RP_XLOGLSN_INIT;
    mWriteFileNo   = 0;
    mWriteBase     = NULL;
    mCurWriteXLogfile = NULL;

    /* read ���� ���� */
    mReadXLogLSN   = RP_XLOGLSN_INIT;
    mReadFileNo    = 0;
    mReadBase      = NULL;
    mCurReadXLogfile = NULL;

    /* file list ���� ���� */
    mXLogfileListHeader = NULL;
    mXLogfileListTail = NULL;

    /* thread ���� ���� ���� */
    mExitFlag = aExitFlag;
    mEndFlag = ID_FALSE;
    mIsReadSleep = ID_FALSE;
    mIsWriteSleep = ID_FALSE;
    mWaitWrittenXLog = ID_TRUE;

    /* ��Ÿ ���� */
    mFileMaxSize = RPU_REPLICATION_XLOGFILE_SIZE;
    mExceedTimeout = aExceedTimeout;
    IDE_TEST( idp::readPtr( "XLOGFILE_DIR", (void**)&mXLogDirPath ) != IDE_SUCCESS );
    idlOS::strncpy( mRepName, aReplName, QCI_MAX_NAME_LEN + 1 );

    /* write info�� ���ü� ��� ���� mutex */
    sLengthCheck = idlOS::snprintf( sBuffer,
                                    IDU_MUTEX_NAME_LEN,
                                    "XLOGFILE_WRITEINFO_%s",
                                    aReplName );
    IDE_TEST_RAISE( sLengthCheck == IDU_MUTEX_NAME_LEN, too_long_mutex_name );
    IDE_TEST( mWriteInfoMutex.initialize( sBuffer,
                                          IDU_MUTEX_KIND_POSIX,
                                          IDV_WAIT_INDEX_NULL) != IDE_SUCCESS );
    sState = 1;

    /* read info�� ���ü� ��� ���� mutex */
    idlOS::memset( sBuffer, 0, IDU_MUTEX_NAME_LEN );
    sLengthCheck = idlOS::snprintf( sBuffer,
                                    IDU_MUTEX_NAME_LEN,
                                    "XLOGFILE_READINFO_%s",
                                    aReplName );
    IDE_TEST_RAISE( sLengthCheck == IDU_MUTEX_NAME_LEN, too_long_mutex_name );
    IDE_TEST( mReadInfoMutex.initialize( sBuffer,
                                          IDU_MUTEX_KIND_POSIX,
                                          IDV_WAIT_INDEX_NULL) != IDE_SUCCESS );
    sState = 2;

    /* xlogfile list�� ���ü� ��� ���� mutex */
    idlOS::memset( sBuffer, 0, IDU_MUTEX_NAME_LEN );
    sLengthCheck = idlOS::snprintf( sBuffer,
                                    IDU_MUTEX_NAME_LEN,
                                    "XLOGFILE_FILELIST_%s",
                                    aReplName );
    IDE_TEST_RAISE( sLengthCheck == IDU_MUTEX_NAME_LEN, too_long_mutex_name );
    IDE_TEST( mXLogfileListMutex.initialize( sBuffer,
                                             IDU_MUTEX_KIND_POSIX,
                                             IDV_WAIT_INDEX_NULL) != IDE_SUCCESS );
    sState = 3;

    /* read�� conditional variable �� ���� mutex �ʱ�ȭ */
    IDE_TEST( mReadWaitCV.initialize() != IDE_SUCCESS );

    idlOS::memset( sBuffer, 0, IDU_MUTEX_NAME_LEN );
    sLengthCheck = idlOS::snprintf( sBuffer,
                                    IDU_MUTEX_NAME_LEN,
                                    "XLOGFILE_READ_CV_%s",
                                    aReplName );
    IDE_TEST_RAISE( sLengthCheck == IDU_MUTEX_NAME_LEN, too_long_mutex_name );
    IDE_TEST( mReadWaitMutex.initialize( sBuffer,
                                         IDU_MUTEX_KIND_POSIX,
                                         IDV_WAIT_INDEX_NULL) != IDE_SUCCESS );
    sState = 4;

    /* write�� conditional variable �� ���� mutex �ʱ�ȭ */
    IDE_TEST( mWriteWaitCV.initialize() != IDE_SUCCESS );

    sLengthCheck = idlOS::snprintf( sBuffer,
                                    IDU_MUTEX_NAME_LEN,
                                    "XLOGFILE_WRITE_CV_%s",
                                    aReplName );
    IDE_TEST_RAISE( sLengthCheck == IDU_MUTEX_NAME_LEN, too_long_mutex_name );
    IDE_TEST( mWriteWaitMutex.initialize( sBuffer,
                                          IDU_MUTEX_KIND_POSIX,
                                          IDV_WAIT_INDEX_NULL) != IDE_SUCCESS );
    sState = 5;

    if( RP_IS_INIT_XLOGLSN( aInitXLogLSN ) == ID_FALSE )
    {
        /* ������ ����ϴ� xlogfile�� �ִٸ� �ش� ���Ϻ��� ����ؾ� �Ѵ�.           *
         * �� ��� read/write������ ���� ������ �� creater�� �����ؾ�               *
         * creater�� �� ������ ������ ����ϴ� ���� �������� ���������� �����Ѵ�.   */

        /* ���� meta data�� �ִٸ� �ش� ���Ϻ��� �����Ѵ�. */
        RP_GET_FILENO_FROM_XLOGLSN( sReadXLogfileNo, aInitXLogLSN );

        /* ���� ����ϴ� ������ ���� ��� �ش� ������ open �Ѵ�. */
        IDE_TEST( openXLogfileForRead( sReadXLogfileNo ) != IDE_SUCCESS );

        setReadInfoWithLock( aInitXLogLSN, sReadXLogfileNo );

        /* read ������ ���� �� �Ŀ� write ������ �����Ѵ�. */
        IDE_TEST( findLastWrittenFileAndSetWriteInfo() != IDE_SUCCESS );
        if ( aReceiverPtr != NULL )
        {
            /* file create thread ���� �� �ʱ�ȭ */
            IDE_TEST_RAISE( iduMemMgr::malloc( IDU_MEM_RP_RPD,
                                               ID_SIZEOF( rpdXLogfileCreater ),
                                               (void**)&(mXLFCreater),
                                               IDU_MEM_IMMEDIATE ) != IDE_SUCCESS, err_AllocFail );
            sState = 6;

            new (mXLFCreater) rpdXLogfileCreater;

            IDE_TEST( mXLFCreater->initialize( aReplName,
                                               aInitXLogLSN,
                                               this,
                                               aReceiverPtr ) != IDE_SUCCESS );

            /* creater�� ���������� ������ ���� ��ȣ�� �����Ѵ�. */
            mXLFCreater->mLastCreatedFileNo = mWriteFileNo;

            IDE_TEST( mXLFCreater->start() != IDE_SUCCESS );
            sState = 7;

            ideLog::log( IDE_RP_0, "[Receiver] Replication %s start XLogfile Creater thread", mRepName );
        }
    }
    else
    {
        /* ������ ����ϴ� xlogfile�� ���ٸ� creater�� �� ������ ���� �����ؾ� �ϹǷ�           *
         * creater�� ���� �����ϰ� creater�� ���� �� ������ �̿��� read/write ������ �����Ѵ�.  */

        /* file create thread ���� �� �ʱ�ȭ */
        IDE_TEST_RAISE( iduMemMgr::malloc( IDU_MEM_RP_RPD,
                                           ID_SIZEOF( rpdXLogfileCreater ),
                                           (void**)&(mXLFCreater),
                                           IDU_MEM_IMMEDIATE ) != IDE_SUCCESS, err_AllocFail );
        sState = 6;

        new (mXLFCreater) rpdXLogfileCreater;

        IDE_TEST( mXLFCreater->initialize( aReplName,
                                           aInitXLogLSN,
                                           this,
                                           aReceiverPtr ) != IDE_SUCCESS );

        IDE_TEST( mXLFCreater->start() != IDE_SUCCESS );   
        sState = 7;

        ideLog::log( IDE_RP_0, "[Receiver] Replication %s start XLogfile Creater thread", mRepName );

        /* ���� ������ ���ٸ� creater�� �������� ������ ����ؾ� �Ѵ�. */
        IDE_TEST( openFirstXLogfileForRead() != IDE_SUCCESS );

        /* write info�� read info�� �����ֵ��� �Ѵ�. */
        setWriteInfoWithLock( mReadXLogLSN, mReadFileNo );

        mWriteBase        = mReadBase;
        mCurWriteXLogfile = mCurReadXLogfile;

        incFileRefCnt( mCurWriteXLogfile );
    }

    if ( RP_IS_INIT_XLOGLSN( aLastXLogLSN ) == ID_FALSE )
    {
        RP_GET_FILENO_FROM_XLOGLSN( sLastFileNo, aLastXLogLSN );
        ACP_UNUSED(sLastFileNo);
        //setWriteInfoWithLock( aLastXLogLSN, sLastFileNo );
    }

    if ( aReceiverPtr != NULL )
    {
        /* flusher thread ���� �� �ʱ�ȭ */
        IDE_TEST_RAISE( iduMemMgr::malloc( IDU_MEM_RP_RPD,
                                           ID_SIZEOF( rpdXLogfileFlusher ),
                                           (void**)&(mXLFFlusher),
                                           IDU_MEM_IMMEDIATE ) != IDE_SUCCESS, err_AllocFail );
        sState = 8;

        new (mXLFFlusher) rpdXLogfileFlusher;

        IDE_TEST( mXLFFlusher->initialize( aReplName,
                                           this ) != IDE_SUCCESS );;
        IDE_TEST( mXLFFlusher->start() != IDE_SUCCESS );
        sState = 9;

        ideLog::log( IDE_RP_0, "[Receiver] Replication %s start XLogfile Flusher thread", mRepName );
    }
    IDE_DASSERT( mReadBase != NULL );
    IDE_DASSERT( mWriteBase != NULL );

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_AllocFail )
    {
        IDE_ERRLOG(IDE_RP_0);
        IDE_SET(ideSetErrorCode(rpERR_ABORT_MEMORY_ALLOC,
                                "rpdXLogfileMgr::initialize",
                                "thread & mutex"));
    }
    IDE_EXCEPTION( too_long_mutex_name )
    {
        IDE_SET( ideSetErrorCode( rpERR_ABORT_RPD_TOO_LONG_MUTEX_NAME, sBuffer ) );
        IDE_ERRLOG(IDE_RP_0);
    }
    IDE_EXCEPTION_END;

    IDE_PUSH();

    switch( sState )
    {
        case 9:
            mXLFFlusher->mExitFlag = ID_TRUE;
            (void)mXLFFlusher->join();
            (void)mXLFFlusher->finalize();
        case 8:
            (void)iduMemMgr::free( mXLFFlusher );
        case 7:
            mXLFCreater->mExitFlag = ID_TRUE;
            (void)mXLFCreater->join();
            mXLFCreater->finalize();
        case 6:
            (void)iduMemMgr::free( mXLFCreater );
        case 5:
            (void)mWriteWaitMutex.destroy();
        case 4:
            (void)mReadWaitMutex.destroy();
        case 3:
            (void)mXLogfileListMutex.destroy();
        case 2:
            (void)mReadInfoMutex.destroy();
        case 1:
            (void)mWriteInfoMutex.destroy();
        default:
            break;
    }

    while( mXLogfileListHeader != NULL )
    {
        if( idlOS::munmap( mXLogfileListHeader->mBase, RPU_REPLICATION_XLOGFILE_SIZE ) == IDE_SUCCESS )
        {
            (void)mXLogfileListHeader->mRefCntMutex.destroy();
            (void)mXLogfileListHeader->mFile.close();
            (void)mXLogfileListHeader->mFile.destroy();

            mXLogfileListHeader = mXLogfileListHeader->mNextXLogfile;
            (void)iduMemMgr::free( mXLogfileListHeader->mPrevXLogfile );
        }
        else
        {
            IDE_ERRLOG(IDE_RP_0);
            (void)mXLogfileListHeader->mRefCntMutex.destroy();

            mXLogfileListHeader = mXLogfileListHeader->mNextXLogfile;
        }

        mXLogfileListHeader->mPrevXLogfile = NULL;
    }
    
    IDE_POP();

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : rpdXLogfileMgr::finalize                   *
 * ------------------------------------------------------------------*
 * rpdXLogfileMgr Ŭ������ �����Ѵ�.
 *********************************************************************/
IDE_RC rpdXLogfileMgr::finalize()
{
    /* read�� write�� ���� ���� endflag�� �����Ѵ�. */
    mEndFlag = ID_TRUE;

    /* create thread�� flush thread�� �����Ѵ�. */
    IDE_TEST( stopSubThread() != IDE_SUCCESS );

    /* create thread�� flush thread�� �����Ѵ�. */
    destroySubThread();

    IDE_ASSERT( mXLogfileListMutex.lock( NULL ) == IDE_SUCCESS );

    /* write/flush�� ������ flush thread�� �ݰ� open�� �ϰ� ������� ���� ������ create thread���� �ݾ�����
     * ���⼭�� read���� ���ϸ� ������ �ȴ�.*/

    /* read ���� ������ ���� ��� �ش� ������ �ݴ´�. */
    if( mXLogfileListHeader != NULL )
    {
        IDE_DASSERT( mXLogfileListHeader == mXLogfileListTail );
        IDE_DASSERT( mXLogfileListHeader == mCurReadXLogfile );
        IDE_DASSERT( getFileRefCnt( mXLogfileListHeader ) == 1 );

        if( idlOS::munmap( mXLogfileListHeader->mBase, RPU_REPLICATION_XLOGFILE_SIZE ) == IDE_SUCCESS )
        {
            if( mXLogfileListHeader->mRefCntMutex.destroy() != IDE_SUCCESS )
            {
                IDE_ERRLOG(IDE_RP_0);
            }
            if( mXLogfileListHeader->mFile.close() != IDE_SUCCESS )
            {
                IDE_ERRLOG(IDE_RP_0);
            }
            if( mXLogfileListHeader->mFile.destroy() != IDE_SUCCESS )
            {
                IDE_ERRLOG(IDE_RP_0);
            }
            (void)iduMemMgr::free( mXLogfileListHeader );
        }
        else
        {
            IDE_ERRLOG(IDE_RP_0);

            if( mXLogfileListHeader->mRefCntMutex.destroy() != IDE_SUCCESS )
            {
                IDE_ERRLOG(IDE_RP_0);
            }
        }
    }
    else
    {
        /* read ���� ������ ���� ���� �� �ʿ䰡 ����.*/
    }

    IDE_ASSERT( mXLogfileListMutex.unlock() == IDE_SUCCESS );

    (void)iduMemMgr::free( mXLFCreater );
    (void)iduMemMgr::free( mXLFFlusher );

    if( mWriteWaitMutex.destroy() != IDE_SUCCESS )
    {
        IDE_ERRLOG(IDE_RP_0);
    }
    if( mReadWaitMutex.destroy() != IDE_SUCCESS )
    {
        IDE_ERRLOG(IDE_RP_0);
    }
    if( mXLogfileListMutex.destroy() != IDE_SUCCESS )
    {
        IDE_ERRLOG(IDE_RP_0);
    }
    if( mReadInfoMutex.destroy() != IDE_SUCCESS )
    {
        IDE_ERRLOG(IDE_RP_0);
    }
    if( mWriteInfoMutex.destroy() != IDE_SUCCESS )
    {
        IDE_ERRLOG(IDE_RP_0);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : rpdXLogfileMgr::removeAllXLogfiles         *
 * ------------------------------------------------------------------*
 * �ش� replication�� ��� XLogfile�� �����Ѵ�.
 * ���� : �ش� �Լ��� static �Լ��� xlogfileMgr�� ������ ���¿��� ��� �����ϴ�.
 *        ������� ������ �����ϰ� �ʹٸ� ���� �ش� replication�� ���� �Ŀ� ����ؾ� �Ѵ�.
 * aReplName - [IN] ������ xlogfile�� ����� replication�� �̸�
 * aFileNo   - [IN] ������ xlogfile�� ������(replication meta�� ������ ���� ��ȣ)
 *********************************************************************/
IDE_RC rpdXLogfileMgr::removeAllXLogfiles( SChar *  aReplName,
                                           UInt     aFileNo )
{
    SChar   sFilePath[XLOGFILE_PATH_MAX_LENGTH] = {0,};
    SChar * sXLogDirPath = NULL;
    idBool  sIsFileExist = ID_TRUE;
    UInt    sFileNo;
    SInt    sLengthCheck = 0; 
    SInt    rc = 0;

    IDE_TEST( idp::readPtr( "XLOGFILE_DIR", (void**)&sXLogDirPath ) != IDE_SUCCESS );

    /* �־��� fileno���� ū ������ �����Ѵ�. */
    sFileNo = aFileNo;

    while( sIsFileExist == ID_TRUE )
    {
        sLengthCheck = idlOS::snprintf( sFilePath,
                                        XLOGFILE_PATH_MAX_LENGTH,
                                        "%s%cxlogfile%s_%"ID_UINT32_FMT,
                                        sXLogDirPath,
                                        IDL_FILE_SEPARATOR,
                                        aReplName,
                                        sFileNo );
        IDE_TEST_RAISE( sLengthCheck == XLOGFILE_PATH_MAX_LENGTH, too_long_file_path );

        if( rpdXLogfileCreater::isFileExist( sFilePath ) == ID_TRUE )
        {
            rc = idf::unlink( sFilePath );
            IDE_TEST_RAISE( rc != 0, unlink_fail );

            sFileNo++;
        }
        else
        {
            sIsFileExist = ID_FALSE;
        }
    }

    sIsFileExist = ID_TRUE;

    /* �־��� fileno���� ���� ������ �����Ѵ�. */
    if( aFileNo > 0 )
    {
        sFileNo = aFileNo - 1;

        while( sIsFileExist == ID_TRUE )
        {
            sLengthCheck = idlOS::snprintf( sFilePath,
                                            XLOGFILE_PATH_MAX_LENGTH,
                                            "%s%cxlogfile%s_%"ID_UINT32_FMT,
                                            sXLogDirPath,
                                            IDL_FILE_SEPARATOR,
                                            aReplName,
                                            sFileNo );
            IDE_TEST_RAISE( sLengthCheck == XLOGFILE_PATH_MAX_LENGTH, too_long_file_path );

            if( rpdXLogfileCreater::isFileExist( sFilePath ) == ID_TRUE )
            {
                rc = idf::unlink( sFilePath );
                IDE_TEST_RAISE( rc != 0, unlink_fail );

                if( sFileNo > 0 )
                {
                    sFileNo--;
                }
                else
                {
                    break;
                }
            }
            else
            {
                sIsFileExist = ID_FALSE;
            }
        }
    }
    return IDE_SUCCESS;

    IDE_EXCEPTION( too_long_file_path )
    {
        IDE_SET( ideSetErrorCode( rpERR_ABORT_RPD_TOO_LONG_FILE_PATH, sFilePath ) );
        IDE_ERRLOG(IDE_RP_0);
    }
    IDE_EXCEPTION( unlink_fail )
    {
        IDE_SET( ideSetErrorCode( rpERR_ABORT_RPD_FAILURE_UNLINK_FILE, sFilePath, rc ) );
        IDE_ERRLOG(IDE_RP_0);
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : rpdXLogfileMgr::checkRemainXLog            *
 * ------------------------------------------------------------------*
 * ���� xlog�� �ִ��� Ȯ���Ѵ�.
 * ���� ���� xlog�� ���� ��� ����Ѵ�.
 * �ش� �Լ��� read(receiver)���� ����Ѵ�.
 *********************************************************************/
IDE_RC rpdXLogfileMgr::checkRemainXLog()
{
    UInt    sLoopCnt = 0;
    idBool  sIsSuccess = ID_FALSE;
    rpXLogLSN sWriteXLogLSN;

    while( sIsSuccess != ID_TRUE )
    {
        IDU_FIT_POINT( "rpdXLogfileMgr::checkRemainXLog" );

        sWriteXLogLSN = getWriteXLogLSNWithLock();
        if( mReadXLogLSN < sWriteXLogLSN )
        {
            sIsSuccess = ID_TRUE;
        }
        else
        {
            IDE_TEST_RAISE( mWaitWrittenXLog == ID_FALSE, end_of_xlogfiles );
            /* ���� xlog�� �����Ƿ� ����Ѵ�. */
            sleepReader();

            IDE_TEST_RAISE( ++sLoopCnt > mExceedTimeout, err_Timeout );
            IDE_TEST_RAISE( *mExitFlag == ID_TRUE, exitFlag_on );
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_Timeout )
    {
        /* ��� �ð��� ������ �Ѱ� ���� �߻� */
        IDE_SET( ideSetErrorCode( rpERR_ABORT_TIMEOUT_EXCEED ) );
    }
    IDE_EXCEPTION( exitFlag_on )
    {
        /* ���� flag�� ���õǾ� ���� */
        IDE_SET( ideSetErrorCode( rpERR_ABORT_END_THREAD, mRepName ) );
    }
    IDE_EXCEPTION( end_of_xlogfiles )
    {
        IDE_SET( ideSetErrorCode( rpERR_IGNORE_RPX_END_OF_XLOGFILES ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : rpdXLogfileMgr::getRemainSize            *
 * ------------------------------------------------------------------*
 * ���� read���� ������ ������ ���� �������� ũ�⸦ ��ȯ�Ѵ�.
 * �ش� �Լ��� read(receiver)���� ȣ��ȴ�.
 *
 * aRemainSize  - [OUT] �ش� ������ ���� ������ ũ��
 *********************************************************************/
IDE_RC rpdXLogfileMgr::getRemainSize( UInt *    aRemainSize )
{
    UInt                sReadOffset = 0;
    rpdXLogfileHeader * sFileHeader = (rpdXLogfileHeader*)mReadBase;
    UInt                sFileWriteOffset = sFileHeader->mFileWriteOffset;

    RP_GET_OFFSET_FROM_XLOGLSN( sReadOffset, mReadXLogLSN );

    /* read�� write���� �ռ����� ����. */ 
    IDE_DASSERT( sReadOffset <= sFileWriteOffset );

    if( ( sFileWriteOffset == sReadOffset ) && ( mCurReadXLogfile->mIsWriteEnd == ID_TRUE ) )
    {
        /* �ش� ������ �� �а� ���� ���Ϸ� �Ѿ�� �� ��Ȳ */
        IDE_TEST( getNextFileRemainSize( aRemainSize ) != IDE_SUCCESS );
    }
    else
    {
        /* �ش� ���Ͽ� ������ ���Ұų� �� ������ �����ִ� ��Ȳ */
        *aRemainSize = sFileWriteOffset - sReadOffset;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : rpdXLogfileMgr::getNextFileRemainSize    *
 * ------------------------------------------------------------------*
 * ���� read ���� ������ ���� ��ȣ ������ ���� �������� ũ�⸦ ��ȯ�Ѵ�.
 * ���� �ش� ������ �޸𸮿� ���ٸ� ��� �κи� �о� ũ�⸦ üũ�Ѵ�.
 * aSize    - [OUT] ���� ������ ���� ������ ũ��
 *********************************************************************/
IDE_RC rpdXLogfileMgr::getNextFileRemainSize( UInt *    aRemainSize )
{
    UInt                sResult = 0;
    UInt                sNextFileNo = mCurReadXLogfile->mFileNo + 1;
    iduFile             sNextFile;
    SChar               sNextFilePath[XLOGFILE_PATH_MAX_LENGTH] = {0,};
    rpdXLogfileHeader * sFileHeaderPtr = NULL;
    rpdXLogfileHeader   sFileHeader;


    /* xlogfile list�� ���󰡾� �ϹǷ� list lock�� ��´�. */
    IDE_ASSERT( mXLogfileListMutex.lock( NULL ) == IDE_SUCCESS );

    if( mCurReadXLogfile->mNextXLogfile == NULL )
    {
        /* read�� write�� ����������� ���� �� ������ �����Ǿ����� �ʾ� write�� ������� ����. *
         * �� ��� 0�� �����ϸ� ���� ��⿡�� busy wait�� ������Ѵ�.                          */
        sResult = 0;
        IDE_ASSERT( mXLogfileListMutex.unlock() == IDE_SUCCESS );
    }
    else
    {
        if( mCurReadXLogfile->mNextXLogfile->mFileNo == sNextFileNo )
        {
            /* ���� ������ �޸𸮿� �ִ� ���¶�� �� ������ ������ ���� �ȴ�. */
            sFileHeaderPtr = (rpdXLogfileHeader*)mCurReadXLogfile->mNextXLogfile->mBase;
            sResult = sFileHeaderPtr->mFileWriteOffset;

            IDE_ASSERT( mXLogfileListMutex.unlock() == IDE_SUCCESS );
        }
        else
        {
            IDE_ASSERT( mXLogfileListMutex.unlock() == IDE_SUCCESS );

            idlOS::snprintf( sNextFilePath,
                             XLOGFILE_PATH_MAX_LENGTH,
                             "%s%cxlogfile%s_%"ID_UINT32_FMT,
                             mXLogDirPath,
                             IDL_FILE_SEPARATOR,
                             mRepName,
                             sNextFileNo );

            IDE_DASSERT( rpdXLogfileCreater::isFileExist( sNextFilePath ) == ID_TRUE );

            IDE_TEST( sNextFile.initialize( IDU_MEM_OTHER, //���� xlogfile�� �߰��� ��
                                            1, /* Max Open FD Count*/
                                            IDU_FIO_STAT_OFF,
                                            IDV_WAIT_INDEX_NULL )
                      != IDE_SUCCESS );

            IDE_TEST( sNextFile.setFileName( sNextFilePath ) != IDE_SUCCESS );

            /* ������ open�Ѵ�. */
            IDE_TEST( sNextFile.openUntilSuccess( ID_FALSE, O_RDWR ) != IDE_SUCCESS );

            /* header(file write offset) �� mapping�� �����Ѵ�. */
            IDE_TEST( sNextFile.read( NULL,
                                      0,
                                      &sFileHeader,
                                      XLOGFILE_HEADER_SIZE_INIT ) != IDE_SUCCESS );

            IDE_DASSERT( sFileHeader.mFileWriteOffset > XLOGFILE_HEADER_SIZE_INIT );

            sResult = sFileHeader.mFileWriteOffset;

            /* ������ �����Ѵ�. */
            IDE_TEST( sNextFile.close() != IDE_SUCCESS );
            IDE_TEST( sNextFile.destroy() != IDE_SUCCESS );
        }
    }

    *aRemainSize = sResult;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : rpdXLogfileMgr::readXLog                   *
 * ------------------------------------------------------------------*
 * read XlogLSN�� �ش��ϴ� �����͸� �о� ���ۿ� �����Ѵ�.
 * �ش� �Լ��� read������ ����Ǳ� ������ readInfo�� ���� �����
 * writeInfo�� ���� �б⿡ ���ؼ��� lock�� ��� �����ؾ� �Ѵ�.
 * ���� : �Ϲ� �۾��ÿ��� checkRemainXLog �Լ�����, recovery�ÿ��� isLastXlog �Լ����� 
 *        ���� xlog�� �ִ��� Ȯ���ϰ� ������ ������ 
 *        read offset�� write offset�� ���� ��� ���� ������ �ִ� ��Ȳ�̴�.
 *
 * aDest      - [OUT] ���� �����͸� ������ ����
 * aLength    - [IN]  ���� �������� ����
 * aIsConvert - [IN]  little endian�� ��� ��ȯ�� �ʿ����� ����
 *********************************************************************/
IDE_RC rpdXLogfileMgr::readXLog( void * aDest,
                                 UInt   aLength,
                                 idBool aIsConvert )
{
    rpdXLogfileHeader * sFileHeader = NULL;
    rpXLogLSN           sReadXLogLSN  = RP_XLOGLSN_INIT;
    UInt                sReadFileNo   = 0;
    UInt                sReadOffset   = 0;
    rpXLogLSN           sWriteXLogLSN = RP_XLOGLSN_INIT;
    UInt                sWriteFileNo  = 0;
    UInt                sFileWriteOffset;
    idBool              sIsWait = ID_FALSE;
    UChar               sTemp[500] = {0,};
    UShort              sTempSize = 0;
    ULong               sLoopCnt = 0;

    sFileHeader = (rpdXLogfileHeader*)mReadBase;
    sFileWriteOffset = sFileHeader->mFileWriteOffset;   

    IDE_DASSERT( mEndFlag == ID_FALSE );

    IDU_FIT_POINT( "rpdXLogfileMgr::readXLog" );

    sReadXLogLSN = mReadXLogLSN;
    RP_GET_XLOGLSN( sReadFileNo, sReadOffset, sReadXLogLSN );

    sWriteXLogLSN = getWriteXLogLSNWithLock();
    RP_GET_FILENO_FROM_XLOGLSN( sWriteFileNo, sWriteXLogLSN );

    IDE_DASSERT( sReadFileNo <= sWriteFileNo );

    while( ( sReadOffset == sFileWriteOffset ) && ( sReadFileNo == sWriteFileNo ) )
    {
        /* write �ϴ� ���� read�� write�� ���� ���� ���                           * 
         * file write offset�� �ٲ�ų� write�� ���� ���Ϸ� �Ѿ������ ����Ѵ�. */
        sFileHeader = (rpdXLogfileHeader*)mReadBase;
        sFileWriteOffset = sFileHeader->mFileWriteOffset;

        sWriteXLogLSN = getWriteXLogLSNWithLock();
        RP_GET_FILENO_FROM_XLOGLSN( sWriteFileNo, sWriteXLogLSN );

        IDE_TEST_RAISE( *mExitFlag == ID_TRUE, exitFlag_on );
        IDE_TEST_RAISE( mWaitWrittenXLog == ID_FALSE, err_Timeout );
        IDE_TEST_RAISE( sLoopCnt++ > 5000000, err_Timeout );
    }

    if( ( sReadOffset == sFileWriteOffset ) && (mCurReadXLogfile->mIsWriteEnd == ID_TRUE ) )
    {
        /* �ش� ������ ��� �о����Ƿ� ���� ���Ͽ� �����Ѵ�. */

        IDE_DASSERT( sReadOffset != RP_XLOGLSN_INIT );
        IDE_TEST( openXLogfileForRead( ++sReadFileNo ) != IDE_SUCCESS );

        sReadXLogLSN = mReadXLogLSN;
        RP_GET_XLOGLSN( sReadFileNo, sReadOffset, sReadXLogLSN );

        sFileHeader = (rpdXLogfileHeader*)mReadBase;
        sFileWriteOffset = sFileHeader->mFileWriteOffset;
    }

    do
    {
        if( sReadFileNo == sWriteFileNo )
        {
            /* read�� write�� �ռ����� ����. */
            IDE_DASSERT( sReadOffset <= sFileWriteOffset );

            if( ( sFileWriteOffset - sReadOffset ) >= aLength )
            {
                /* ���� �����Ͱ� ���� �������� ���̺��� �� ��� ���������� read�� �����Ѵ�. */
                sIsWait = ID_FALSE;

                /* read(memcpy)�� �����Ѵ�. */
#ifdef ENDIAN_IS_BIG_ENDIAN
                 /* big endian�� ��� �״�� �����Ѵ�. */
                idlOS::memcpy( aDest, (SChar*)mReadBase + sReadOffset, aLength );
#else
                /* little endian�� ��� ��ȯ�Ͽ� �����ؾ� �Ѵ�. */
                if( aIsConvert == ID_TRUE )
                {
                    switch( aLength )
                    {
                        case 2:
                            RP_ENDIAN_ASSIGN2( (SChar*)aDest, (SChar*)mReadBase + sReadOffset );
                            break;
                        case 4:
                            RP_ENDIAN_ASSIGN4( (SChar*)aDest, (SChar*)mReadBase + sReadOffset );
                            break;
                        case 8:
                            RP_ENDIAN_ASSIGN8( (SChar*)aDest, (SChar*)mReadBase + sReadOffset );
                            break;
                        default:
                            idlOS::memcpy( aDest, (SChar*)mReadBase + sReadOffset, aLength );
                            break;
                    }
                }
                else
                {
                    idlOS::memcpy( aDest, (SChar*)mReadBase + sReadOffset, aLength );
                }
#endif

#if defined(DEBUG)
                if( aLength <= 200 )
                {
                    byteToString( (UChar*)aDest, (UShort)aLength,
                                  sTemp, &sTempSize );
                    ideLog::log(IDE_RP_6,"[r] - %d|%d|%d : %s",sReadFileNo, sReadOffset, aLength, sTemp );
                }
                else
                {
                    ideLog::log(IDE_RP_6,"[r] - %d|%d|%d",sReadFileNo, sReadOffset, aLength );
                }
#else
     ACP_UNUSED(sTempSize);
     ACP_UNUSED(sTemp);
#endif

                /* read Offset�� �����ϰ� readXLogLSN�� �����Ѵ�. */
                sReadOffset += aLength;
                sReadXLogLSN = RP_SET_XLOGLSN( sReadFileNo , sReadOffset );
                setReadInfoWithLock( sReadXLogLSN, sReadFileNo );
            }
            else
            {
                /* ���� �����ͺ��� ���� �������� ���̰� �� ���� ����          *
                 * �ش� xlog�� ������ �������� �ƴϹǷ� busy wait�� ����Ѵ�.   */
                sIsWait = ID_TRUE;

                IDE_TEST_RAISE( *mExitFlag == ID_TRUE, exitFlag_on );
                IDE_TEST_RAISE( mWaitWrittenXLog == ID_FALSE, err_Timeout );
                IDE_TEST_RAISE( sLoopCnt++ > 5000000, err_Timeout );

                /* write�� ���� ���Ϸ� �Ѿ�� ���ɼ��� �����Ƿ� write ������ �ٽ� �޾ƿ´�. */
                sWriteXLogLSN = getWriteXLogLSNWithLock();
                RP_GET_FILENO_FROM_XLOGLSN( sWriteFileNo, sWriteXLogLSN );

                sFileHeader = (rpdXLogfileHeader*)mReadBase;
                sFileWriteOffset = sFileHeader->mFileWriteOffset;

                IDU_FIT_POINT( "rpdXLogfileMgr::readXLog::busyWait" );
            }
        }
        else
        {
            IDE_DASSERT( sReadOffset + aLength <= sFileWriteOffset );

            /* ������ �ٸ��� �Ű澵 �ʿ䰡 ����. */
            sIsWait = ID_FALSE;

            /* read(memcpy)�� �����Ѵ�. */
#ifdef ENDIAN_IS_BIG_ENDIAN
            /* big endian�� ��� �״�� �����Ѵ�. */
            idlOS::memcpy( aDest, (SChar*)mReadBase + sReadOffset, aLength );
#else
            /* little endian�� ��� ��ȯ�Ͽ� �����ؾ� �Ѵ�. */
            if( aIsConvert == ID_TRUE )
            {
                switch( aLength )
                {
                    case 2:
                        RP_ENDIAN_ASSIGN2( (SChar*)aDest, (SChar*)mReadBase + sReadOffset );
                        break;
                    case 4:
                        RP_ENDIAN_ASSIGN4( (SChar*)aDest, (SChar*)mReadBase + sReadOffset );
                        break;
                    case 8:
                        RP_ENDIAN_ASSIGN8( (SChar*)aDest, (SChar*)mReadBase + sReadOffset );
                        break;
                    default:
                        idlOS::memcpy( aDest, (SChar*)mReadBase + sReadOffset, aLength );
                        break;
                }
            }
            else
            {
                idlOS::memcpy( aDest, (SChar*)mReadBase + sReadOffset, aLength );
            }
#endif

#if defined(DEBUG)
            if( aLength <= 200 )
            {
                byteToString( (UChar*)aDest, (UShort)aLength,
                              sTemp, &sTempSize );
                ideLog::log(IDE_RP_6,"[r] - %d|%d|%d : %s",sReadFileNo, sReadOffset,aLength, sTemp );
            }
            else
            {
                ideLog::log(IDE_RP_6,"[r] - %d|%d|%d",sReadFileNo, sReadOffset,aLength );
            }
#endif

            /* read Offset�� �����ϰ� readXLogLSN�� �����Ѵ�. */
            sReadOffset += aLength;
            sReadXLogLSN = RP_SET_XLOGLSN( sReadFileNo, sReadOffset );
            setReadInfoWithLock( sReadXLogLSN, sReadFileNo );
        }

    }while( sIsWait == ID_TRUE );

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_Timeout )
    {
        /* ��� �ð��� ������ �Ѱ� ���� �߻� */
        IDE_SET( ideSetErrorCode( rpERR_ABORT_TIMEOUT_EXCEED ) );
    }
    IDE_EXCEPTION( exitFlag_on )
    {
        /* ���� flag�� ���õǾ� ���� */
        IDE_SET( ideSetErrorCode( rpERR_ABORT_END_THREAD, mRepName ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : rpdXLogfileMgr::readXLogDirect             *
 * ------------------------------------------------------------------*
 * read XlogLSN�� �ش��ϴ� �������� �����͸� ��ȯ�Ѵ�.
 * �ش� �Լ��� read������ ����Ǳ� ������ readInfo�� ���� �����
 * writeInfo�� ���� �б⿡ ���ؼ��� lock�� ��� �����ؾ� �Ѵ�.
 * diread direct�� ���� �����͸� �ѱ� ��� endian�� ���� ó���� �Ǿ����� �����Ƿ�
 * �����͸� ���� �ʿ��� ���� ����� �Ѵ�.
 * ���� : �Ϲ� �۾��ÿ��� checkRemainXLog �Լ�����, recovery�ÿ��� isLastXlog �Լ����� 
 *        ���� xlog�� �ִ��� Ȯ���ϰ� ������ ������ 
 *        read offset�� write offset�� ���� ��� ���� ������ �ִ� ��Ȳ�̴�.
 *
 * aMMapPtr - [OUT] �ش� �ϴ� �������� ������(��ȯ��)
 * aLength  - [IN]  ���� �������� ����(offset ������)
 *********************************************************************/
IDE_RC rpdXLogfileMgr::readXLogDirect( void **  aMMapPtr,
                                       UInt     aLength )
{
    rpXLogLSN sReadXLogLSN  = RP_XLOGLSN_INIT;
    UInt      sReadFileNo   = 0;
    UInt      sReadOffset   = 0;
    rpXLogLSN sWriteXLogLSN = RP_XLOGLSN_INIT;
    UInt      sWriteFileNo  = 0;
    UInt      sFileWriteOffset = *(UInt*)mReadBase;
    idBool    sIsWait = ID_FALSE;

    IDE_DASSERT( mEndFlag == ID_FALSE );

    sReadXLogLSN = mReadXLogLSN;
    RP_GET_XLOGLSN( sReadFileNo, sReadOffset, sReadXLogLSN );

    sWriteXLogLSN = getWriteXLogLSNWithLock();
    RP_GET_FILENO_FROM_XLOGLSN( sWriteFileNo, sWriteXLogLSN );

    IDE_DASSERT( sReadFileNo <= sWriteFileNo );

    if( sReadOffset == sFileWriteOffset )
    {
        /* read offset�� file write offset�� ���ٸ�                          *
         * read�� write�� ���� ��Ұų� �ش� ������ ��� ���� �����̳�       *
         * checkRemainXLog���� ���� ������ �ִ��� Ȯ���ϰ� ������ �����Ƿ� *
         * read�� write�� ���� ���� ���� ����.                               */
        //IDE_TEST_RAISE( sReadFileNo == sWriteFileNo, err_Offset );

        /* �ش� ������ ��� �о����Ƿ� ���� ���Ͽ� �����Ѵ�. */
        IDE_TEST( openXLogfileForRead( ++sReadFileNo ) != IDE_SUCCESS );
    }

    do{
        if( sReadFileNo == sWriteFileNo )
        {
            /* read�� write�� �ռ����� ����. */
            IDE_DASSERT( sReadOffset <= sFileWriteOffset );

            if( ( sFileWriteOffset - sReadOffset ) >= aLength )
            {
                /* ���� �����Ͱ� ���� �������� ���̺��� �� ��� ���������� read�� �����Ѵ�. */
                sIsWait = ID_FALSE;

                /* ���� �е��� mmap�� �����͸� �ѱ��. */
                *aMMapPtr = (SChar*)mReadBase + sReadOffset;

                /* read Offset�� �����ϰ� readXLogLSN�� �����Ѵ�. */
                sReadOffset += aLength;
                sReadXLogLSN = RP_SET_XLOGLSN( sReadFileNo, sReadOffset );
                setReadInfoWithLock( sReadXLogLSN, sReadFileNo );
            }
            else
            {
                /* ���� �����ͺ��� ���� �������� ���̰� �� ���� ����          *
                 * �ش� xlog�� ������ �������� �ƴϹǷ� busy wait�� ����Ѵ�.   */
                sIsWait = ID_TRUE;

                /* write�� ���� ���Ϸ� �Ѿ�� ���ɼ��� �����Ƿ� write ������ �ٽ� �޾ƿ´�. */
                sWriteXLogLSN = getWriteXLogLSNWithLock();
                RP_GET_FILENO_FROM_XLOGLSN( sWriteFileNo, sWriteXLogLSN );
            }
        }
        else
        {
            /* ������ �ٸ��� �Ű澵 �ʿ䰡 ����. */
            sIsWait = ID_FALSE;

            /* ���� �е��� mmap�� �����͸� �ѱ��. */
            *aMMapPtr = (SChar*)mReadBase + sReadOffset;

            /* read Offset�� �����ϰ� readXLogLSN�� �����Ѵ�. */
            sReadOffset += aLength;
            sReadXLogLSN = RP_SET_XLOGLSN( sReadFileNo, sReadOffset );
            setReadInfoWithLock( sReadXLogLSN, sReadFileNo );
        }

    }while( sIsWait == ID_TRUE );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : rpdXLogfileMgr::writeXLog                  *
 * ------------------------------------------------------------------*
 * �Է¹��� xlog�� mmap�� ���� offset�� ������Ų��.
 * ����, ���� ���Ͽ� ������ ���� �� ���� ���� ������ ��쿡�� �� ������ ���� �ش� ���Ͽ� ����.
 *
 * aSrc     - [IN] mmap�� ������ �������� ������
 * aLength  - [IN] mmap�� ������ �������� ����
 * aXSN     - [IN] �ش� xlog�� XSN
 *            (��, write ��� piece�� xlog�� ù piece�� �ƴ϶�� SM_SN_NULL���� ������.)
 *********************************************************************/
IDE_RC rpdXLogfileMgr::writeXLog( void *    aSrc,
                                  UInt      aLength,
                                  smSN      aXSN )
{
    rpXLogLSN           sWriteXLogLSN = RP_XLOGLSN_INIT;
    UInt                sWriteFileNo  = 0;
    UInt                sWriteOffset  = 0;
    rpdXLogfileHeader * sFileHeader = NULL;
#if defined(DEBUG)
    UChar               sTemp[500] ={0,};
    UShort              sTempSize = 0;
#endif

    IDU_FIT_POINT( "rpdXLogfileMgr::writeXLog" );

    IDE_DASSERT( mEndFlag == ID_FALSE );

    IDE_DASSERT( aLength <= ( mFileMaxSize - XLOGFILE_HEADER_SIZE_INIT ) );

    sWriteXLogLSN = mWriteXLogLSN;
    RP_GET_XLOGLSN( sWriteFileNo, sWriteOffset, sWriteXLogLSN );

    sFileHeader = (rpdXLogfileHeader*)mWriteBase;

    IDE_DASSERT( sWriteOffset == sFileHeader->mFileWriteOffset );
    IDE_DASSERT( sWriteOffset <= mFileMaxSize );

    /* ���� ������ xlog�� �� ������ �ִ��� üũ�Ѵ�. */
    if( sWriteOffset + aLength > mFileMaxSize )
    {
        /* ���� ���Ͽ� write�� �����ٴ� ���� ǥ���Ѵ�. */
        mCurWriteXLogfile->mIsWriteEnd = ID_TRUE;

        /* ������ ������ ��� �� ���Ͽ� �����ؾ� �Ѵ�. */
        IDE_TEST( openXLogfileForWrite() != IDE_SUCCESS );

        IDE_TEST_RAISE( *mExitFlag == ID_TRUE, exitFlag_on );

        /* �� ���Ͽ� ���� file no�� offset�� �����Ѵ�. */
        sWriteFileNo++;
        sWriteOffset = XLOGFILE_HEADER_SIZE_INIT;

        sFileHeader = (rpdXLogfileHeader*)mWriteBase;
    }

    /* ������ ���� ��� ���� ���Ͽ� write(memcpy)�� �����Ѵ�. */
    idlOS::memcpy( (SChar*)mWriteBase + sWriteOffset, aSrc, aLength );

#if defined(DEBUG)
    if( aLength <= 200 )
    {
        byteToString( (UChar*)aSrc, (UShort)aLength,
                      sTemp, &sTempSize );

        ideLog::log(IDE_RP_6,"[w] - %d|%d|%d : %s", sWriteFileNo, sWriteOffset, aLength, sTemp );
    }
    else
    {
        ideLog::log(IDE_RP_6,"[w] - %d|%d|%d", sWriteFileNo, sWriteOffset,aLength );
    }
#endif

    /* file write offset�� �����Ѵ�. */
    sWriteOffset += aLength;
    sFileHeader->mFileWriteOffset = sWriteOffset;

    /* ���� �� xlog�� �ش� ���Ͽ� ���� ù xlog�� �� �� piece��� header�� �߰� ������ �����Ѵ�.  */
    if( ( aXSN != SM_SN_NULL ) && ( sFileHeader->mFileFirstXSN == SM_SN_NULL ) )
    {

        sFileHeader->mFileFirstXLogOffset = sWriteOffset - aLength;
        sFileHeader->mFileFirstXSN = aXSN;
    }

    /* write offset�� ���� write XLogLSN�� �����Ѵ�. */
    sWriteXLogLSN = RP_SET_XLOGLSN( sWriteFileNo, sWriteOffset );
    setWriteInfoWithLock( sWriteXLogLSN, sWriteFileNo );

    wakeupReader();

    return IDE_SUCCESS;

    IDE_EXCEPTION( exitFlag_on )
    {
        /* ���� flag�� ���õǾ� ���� */
        IDE_SET( ideSetErrorCode( rpERR_ABORT_END_THREAD, mRepName ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : rpdXLogfileMgr::openXLogfileForWrite       *
 * ------------------------------------------------------------------*
 * write(transfer)���� �� xlogfile�� ����.
 * write ��� create�� xlogfile�� ���������Ƿ� �����δ� ���� �ʰ� �����͸� �̵��Ѵ�.
 * ������ ����ϴ� ������ sync �� close�� ���⼭ �������� �ʴ´�.
 *********************************************************************/
IDE_RC rpdXLogfileMgr::openXLogfileForWrite()
{
    rpdXLogfile *   sOldFile    = NULL;
    rpXLogLSN       sNewXLogLSN = RP_XLOGLSN_INIT;
    UInt            sNewFileNo  = 0;
    UInt            sNewOffset  = 0;
    UInt            sPrepareFileCnt = 0;
    idBool          sIsSuccess  = ID_FALSE;
    PDL_Time_Value  sCheckTime;
    PDL_Time_Value  sSleepSec;

    IDE_DASSERT( mCurWriteXLogfile != NULL );

    sCheckTime.initialize();
    sSleepSec.initialize();

    sSleepSec.set( 1 );

    while( ( sIsSuccess != ID_TRUE ) && ( *mExitFlag == ID_FALSE ) )
    {
        IDE_TEST_RAISE( mXLFCreater->isCreaterAlive() == ID_FALSE, creater_die );

        mXLFCreater->getPrepareFileCnt( &sPrepareFileCnt );

        /* �̸� �غ�� ������ ���� ��� �ش� ������ ����Ѵ�. */
        if( sPrepareFileCnt > 0 )
        {
            IDE_ASSERT( mXLogfileListMutex.lock( NULL ) == IDE_SUCCESS );

            IDE_ASSERT( mCurWriteXLogfile->mNextXLogfile != NULL );

            sOldFile = mCurWriteXLogfile;
            mCurWriteXLogfile = mCurWriteXLogfile->mNextXLogfile;

            IDE_ASSERT( mXLogfileListMutex.unlock() == IDE_SUCCESS );

            /* ���� ������ flusher�� ����Ѵ�. */
            IDE_TEST_RAISE( mXLFFlusher->isFlusherAlive() == ID_FALSE, flusher_die );
            mXLFFlusher->addFileToFlushList( sOldFile );

            /* flush thread���� �˶��� ������. */
            mXLFFlusher->wakeupFlusher();

            /* write offset ������ �����Ѵ�. */
            sNewFileNo = mCurWriteXLogfile->mFileNo;

            sNewOffset = XLOGFILE_HEADER_SIZE_INIT;
            sNewXLogLSN = RP_SET_XLOGLSN( sNewFileNo, sNewOffset );

            setWriteInfoWithLock( sNewXLogLSN, sNewFileNo );
            mWriteBase = mCurWriteXLogfile->mBase;

            sIsSuccess = ID_TRUE;

            IDE_TEST_RAISE( mXLFCreater->isCreaterAlive() == ID_FALSE, creater_die );

            /* prepare file count�� 1 ���ҽ�Ų��. */
            mXLFCreater->decPrepareFileCnt();
        }
        else
        {
            IDU_FIT_POINT( "rpdXLogfileMgr::openXLogfileForWrite::shortageFile" );

            /* �� ������ �����ϴ� �� ���� write�� ���� ���� ������ ���� ��Ȳ */
            /* create thread�� ����� ����Ѵ�. */
            IDE_TEST_RAISE( mXLFCreater->isCreaterAlive() == ID_FALSE, creater_die );

            mXLFCreater->wakeupCreater();

            sleepWriter();
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( creater_die )
    {
        IDE_SET( ideSetErrorCode( rpERR_ABORT_RPD_CREATER_THREAD_NOT_ALIVE, mRepName ) );
        IDE_ERRLOG(IDE_RP_0);
    }
    IDE_EXCEPTION( flusher_die )
    {
        IDE_SET( ideSetErrorCode( rpERR_ABORT_RPD_FLUSHER_THREAD_NOT_ALIVE, mRepName ) );
        IDE_ERRLOG(IDE_RP_0);
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : rpdXLogfileMgr::openXLogfileForRead        *
 * ------------------------------------------------------------------*
 * read(receiver)���� �� xlogfile�� ����.
 * ���� �� ������ write���� ������̶�� ���� ���� �ʰ� �ش� ������ �״�� ����Ѵ�.
 * ������ ����ϴ� ������ closeó�� �Ѵ�.
 *
 * aXLogfileNo - [IN] ���� �� xlogfile�� ��ȣ
 *********************************************************************/
IDE_RC rpdXLogfileMgr::openXLogfileForRead( UInt aXLogfileNo )
{
    rpdXLogfile *   sNewFile = NULL;
    rpdXLogfile *   sOldFile = NULL;
    rpXLogLSN       sNewXLogLSN = RP_XLOGLSN_INIT;
    UInt            sNewFileNo = 0;
    UInt            sNewOffset = 0;

    sOldFile = mCurReadXLogfile;

    IDE_ASSERT( mXLogfileListMutex.lock( NULL ) == IDE_SUCCESS );

    if( ( mCurReadXLogfile != NULL ) &&
        ( mCurReadXLogfile->mNextXLogfile != NULL ) &&
        ( mCurReadXLogfile->mNextXLogfile->mFileNo == aXLogfileNo ) )
    {
        /* ���� ������ ������ �� �����̶�� �״�� ����ϸ� �ȴ�. */
        IDU_FIT_POINT( "rpdXLogfileMgr::openXLogfileForRead::incFileRefCnt" );
        incFileRefCnt( mCurReadXLogfile->mNextXLogfile );

        IDE_ASSERT( mXLogfileListMutex.unlock() == IDE_SUCCESS );

        mCurReadXLogfile = mCurReadXLogfile->mNextXLogfile;
        sNewOffset = XLOGFILE_HEADER_SIZE_INIT;
        sNewFileNo = aXLogfileNo;
        sNewXLogLSN = RP_SET_XLOGLSN( sNewFileNo, sNewOffset );
        setReadInfoWithLock( sNewXLogLSN, sNewFileNo ); 
        mReadBase  = mCurReadXLogfile->mBase;
    }
    else
    {
        IDE_ASSERT( mXLogfileListMutex.unlock() == IDE_SUCCESS );

        if( ( mCurReadXLogfile != NULL ) &&
            ( mCurReadXLogfile->mNextXLogfile != NULL ) )
        {
            IDE_DASSERT( mCurReadXLogfile->mNextXLogfile->mFileNo > aXLogfileNo );
        }

        /* ������ ���� mapping�Ѵ�. */
        IDE_TEST( openAndMappingFile( aXLogfileNo, &sNewFile ) != IDE_SUCCESS );

        /* ���� open�� ������ file list�� �����Ѵ�. */
        addNewFileToFileList( sOldFile, sNewFile );

        /* �޸𸮿� ��� ���� open�� ��� �ش� ������ write�� �̹� ���� �����̴�. */
        sNewFile->mIsWriteEnd = ID_TRUE; 

        /* read ���� ������ �����Ѵ�. */
        sNewOffset = XLOGFILE_HEADER_SIZE_INIT;
        sNewFileNo = aXLogfileNo;
        sNewXLogLSN = RP_SET_XLOGLSN( sNewFileNo, sNewOffset );
        setReadInfoWithLock( sNewXLogLSN, sNewFileNo );

        mReadBase = sNewFile->mBase;
        mCurReadXLogfile = sNewFile;
    }

    if( sOldFile != NULL )
    {
        decFileRefCnt( sOldFile );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : rpdXLogfileMgr::openFirstXLogfileForRead   *
 * ------------------------------------------------------------------*
 * read(receiver)���� ù xlogfile�� ����.
 * ù ������ ��� read���� ���� ���� �ʰ� create thread���� ������ ������ ����Ѵ�.
 *********************************************************************/
IDE_RC rpdXLogfileMgr::openFirstXLogfileForRead()
{
    UInt        sPrepareFileCnt;
    rpXLogLSN   sNewXLogLSN = 0;
    UInt        sNewFileNo = 0;
    UInt        sNewOffset = 0;

    IDE_TEST_RAISE( mXLFCreater->isCreaterAlive() == ID_FALSE, creater_die );

    mXLFCreater->getPrepareFileCnt( &sPrepareFileCnt );

    /* ���� ������ ������ ���ٸ� �����ɶ����� busy wait�� ����Ѵ�. */
    while( sPrepareFileCnt == 0 )
    {
        mXLFCreater->wakeupCreater();

        mXLFCreater->getPrepareFileCnt( &sPrepareFileCnt );
    }

    IDE_DASSERT( mCurReadXLogfile != NULL );

    /* read ���� ������ �����ش�. */
    sNewOffset = XLOGFILE_HEADER_SIZE_INIT;
    sNewFileNo = mCurReadXLogfile->mFileNo;
    sNewXLogLSN = RP_SET_XLOGLSN( sNewFileNo, sNewOffset );
    setReadInfoWithLock( sNewXLogLSN, sNewFileNo );

    mReadBase = mCurReadXLogfile->mBase;

    return IDE_SUCCESS;

    IDE_EXCEPTION( creater_die )
    {
        IDE_SET( ideSetErrorCode( rpERR_ABORT_RPD_CREATER_THREAD_NOT_ALIVE, mRepName ) );
        IDE_ERRLOG(IDE_RP_0);
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : rpdXLogfileMgr::findLastWrittenFileAndSetWriteInfo  *
 * ------------------------------------------------------------------*
 * write info�� �����Ѵ�.
 * read ���� ������ ���� ������ �ִ��� ã��, ���� ��� file write offset�� üũ�Ͽ�
 * �ش� ���Ͽ� write�� ���� �Ǿ����� üũ�ϴ� ���� �ݺ��� ���� �������� write�� ����� ������ ã�´�.
 * �������� write�� ������ ã���� �ش� ������ �޸𸮿� �ø��� write ������� �����Ѵ�.
 *********************************************************************/
IDE_RC rpdXLogfileMgr::findLastWrittenFileAndSetWriteInfo()
{
    UInt                sFileNo;
    iduFile             sFile;
    UInt                sFileWriteOffset = 0;
    SChar               sFilePath[XLOGFILE_PATH_MAX_LENGTH] = {0,};
    idBool              sIsEnd = ID_FALSE;
    rpdXLogfile *       sWriteFile = NULL;
    SInt                sLengthCheck = 0;
    rpXLogLSN           sNewWriteXLogLSN;
    rpdXLogfileHeader * sFileHeaderPtr = NULL;
    rpdXLogfileHeader   sFileHeader;

    sFileNo = getReadFileNoWithLock() + 1;

    IDE_TEST( sFile.initialize( IDU_MEM_OTHER, //���� xlogfile�� �߰��� ��
                                1, /* Max Open FD Count*/
                                IDU_FIO_STAT_OFF,
                                IDV_WAIT_INDEX_NULL )
              != IDE_SUCCESS );
    do{
        sLengthCheck = idlOS::snprintf( sFilePath,
                                        XLOGFILE_PATH_MAX_LENGTH,
                                        "%s%cxlogfile%s_%"ID_UINT32_FMT,
                                        mXLogDirPath,
                                        IDL_FILE_SEPARATOR,
                                        mRepName,
                                        sFileNo );
        IDE_TEST_RAISE( sLengthCheck == XLOGFILE_PATH_MAX_LENGTH, too_long_file_path );
        
        if( rpdXLogfileCreater::isFileExist( sFilePath ) == ID_TRUE )
        {
            IDE_TEST( sFile.setFileName( sFilePath ) != IDE_SUCCESS );

            /* ������ open�Ѵ�. */
            IDE_TEST( sFile.openUntilSuccess( ID_FALSE, O_RDWR ) != IDE_SUCCESS );

            /* header(file write offset) �� read�� �����Ѵ�. */
            IDE_TEST( sFile.read( NULL,
                                  0,
                                  &sFileHeader,
                                  XLOGFILE_HEADER_SIZE_INIT ) != IDE_SUCCESS );

            if( sFileHeader.mFileWriteOffset > XLOGFILE_HEADER_SIZE_INIT )
            {
                /* �ش��ϴ� ������ �ּ� 1�� ���� �����̹Ƿ� ���� ������ üũ�ؾ� �Ѵ�. */
                sFileNo++;
                sIsEnd = ID_FALSE;
            }
            else
            {
                /* �ش��ϴ� ������ ������ �ǰ� ������ ���� �����̴�. */
                sIsEnd = ID_TRUE;
            }
        }
        else
        {
            /* ���� ������ �����Ƿ� ���������� ���� ������ sFile�̴� */
            sIsEnd = ID_TRUE;
        }

        /* ������ �ݴ´�. */
        IDE_TEST( sFile.close() != IDE_SUCCESS );

    }while( sIsEnd == ID_FALSE );

    IDE_TEST( sFile.destroy() != IDE_SUCCESS );

    sFileNo--;

    if( sFileNo == getReadFileNoWithLock() )
    {
        /* read ���� ���� �ܿ� �ٸ� ������ ���ų� ������� ���� ���    *
         * read ���� ���Ϻ��� write�� �����ϸ� �ȴ�.                    */

        mCurWriteXLogfile = mCurReadXLogfile;
        mWriteBase        = mReadBase;

        incFileRefCnt( mCurWriteXLogfile );
    }
    else
    {
        /* read���� ���� �ܿ� write�� ������ �ִ� ��� ���� ������ write�� ���Ϻ��� write�� �����Ѵ�. */
        IDE_DASSERT( sFileNo > getReadFileNoWithLock() );

        /* �� ������ open �� mapping�Ѵ�. */
        IDE_TEST( openAndMappingFile( sFileNo, &sWriteFile ) != IDE_SUCCESS );

        /* ���� open�� ������ file list�� �����Ѵ�. */
        addToFileListinLast( sWriteFile );

        mXLogfileListTail = sWriteFile;
        mCurWriteXLogfile = sWriteFile;
        mWriteBase        = sWriteFile->mBase;
    }

    /* write info�� �����Ѵ�. */
    sFileHeaderPtr = (rpdXLogfileHeader*)mWriteBase;
    sFileWriteOffset = sFileHeaderPtr->mFileWriteOffset;

    sNewWriteXLogLSN = RP_SET_XLOGLSN( sFileNo, sFileWriteOffset );
    setWriteInfoWithLock( sNewWriteXLogLSN, sFileNo );   

    mCurWriteXLogfile->mIsWriteEnd = ID_FALSE;

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
 * FUNCTION DESCRIPTION : rpdXLogfileMgr::sleepReader                *
 * ------------------------------------------------------------------*
 * read(receiver)�� write(transfer)�� ��������� �� ��� ����Ѵ�.
 * �⺻������ 1�� ����ϳ� write�� signal�� ���� ���� ���� �ִ�.
 * ��� �ð��� �����̻� �� ��쿡�� timeout�� �߻��Ѵ�.
 *********************************************************************/
void rpdXLogfileMgr::sleepReader()
{
    PDL_Time_Value  sCheckTime;
    PDL_Time_Value  sSleepSec;

    sCheckTime.initialize();
    sSleepSec.initialize();

    sSleepSec.set( 1 );

    IDE_ASSERT( mReadWaitMutex.lock( NULL ) == IDE_SUCCESS );
    sCheckTime = idlOS::gettimeofday() + sSleepSec;

    IDU_FIT_POINT( "rpdXLogfileMgr::sleepReader" );

    mIsReadSleep = ID_TRUE;
    (void)mReadWaitCV.timedwait( &mReadWaitMutex, &sCheckTime );
    mIsReadSleep = ID_FALSE;

    IDE_ASSERT( mReadWaitMutex.unlock() == IDE_SUCCESS );

    IDE_EXCEPTION_END;

    return;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : rpdXLogfileMgr::wakeupReader               *
 * ------------------------------------------------------------------*
 * ������� read(receiver)�� �����.
 * �ش� �Լ��� write(transfer)���� ȣ��ȴ�.
 *********************************************************************/
void rpdXLogfileMgr::wakeupReader()
{
    IDU_FIT_POINT( "rpdXLogfileMgr::wakeupReader" );

    IDE_ASSERT( mReadWaitMutex.lock( NULL ) == IDE_SUCCESS );

    if( mIsReadSleep == ID_TRUE )
    {
        IDE_ASSERT( mReadWaitCV.signal() == IDE_SUCCESS );
    }

    IDE_ASSERT( mReadWaitMutex.unlock() == IDE_SUCCESS );

    IDE_EXCEPTION_END;

    return;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : rpdXLogfileMgr::sleepWriter                 *
 * ------------------------------------------------------------------*
 * write(transfer)�� create thread�� ��������� �� ��� ����Ѵ�.
 * �⺻������ 1�� ����ϳ� create thread�� signal�� ���� ���� ���� �ִ�.
 *********************************************************************/
void rpdXLogfileMgr::sleepWriter()
{
    PDL_Time_Value  sCheckTime;
    PDL_Time_Value  sSleepSec;

    sCheckTime.initialize();
    sSleepSec.initialize();

    sSleepSec.set( 1 );

    IDE_ASSERT( mWriteWaitMutex.lock( NULL ) == IDE_SUCCESS );
    sCheckTime = idlOS::gettimeofday() + sSleepSec;

    mIsWriteSleep = ID_TRUE;
    (void)mWriteWaitCV.timedwait( &mWriteWaitMutex, &sCheckTime );
    mIsWriteSleep = ID_FALSE;

    IDE_ASSERT( mWriteWaitMutex.unlock() == IDE_SUCCESS );

    return; 
}

/*********************************************************************
 * FUNCTION DESCRIPTION : rpdXLogfileMgr::wakeupWriter              *
 * ------------------------------------------------------------------*
 * ������� write(transfer)�� �����.
 * �ش� �Լ��� create thread���� ȣ��ȴ�.
 *********************************************************************/
void rpdXLogfileMgr::wakeupWriter()
{
    IDE_ASSERT( mWriteWaitMutex.lock( NULL ) == IDE_SUCCESS );

    if( mIsWriteSleep == ID_TRUE )
    {
        IDE_ASSERT( mWriteWaitCV.signal() == IDE_SUCCESS );
    }

    IDE_ASSERT( mWriteWaitMutex.unlock() == IDE_SUCCESS );

    return; 
}

/*********************************************************************
 * FUNCTION DESCRIPTION : rpdXLogfileMgr::stopSubThread              *
 * ------------------------------------------------------------------*
 * create thread�� flush thread�� ������Ų��.
 *********************************************************************/
IDE_RC rpdXLogfileMgr::stopSubThread()
{
    /* ���� write ���� ������ flush list�� �����Ѵ�.*/
    IDE_TEST_RAISE( mXLFFlusher->isFlusherAlive() == ID_FALSE, flusher_die );
    mXLFFlusher->addFileToFlushList( mCurWriteXLogfile );

    /* flush thread�� �����Ѵ�. */
    if( mXLFFlusher->isFlusherAlive() == ID_TRUE )
    {
        mXLFFlusher->mExitFlag = ID_TRUE;
        mXLFFlusher->wakeupFlusher();
        IDE_TEST( mXLFFlusher->join() != IDE_SUCCESS );
        mXLFFlusher->finalize();

        ideLog::log( IDE_RP_0, "[Receiver] Replication %s finalized XLogfile Flusher Thread", mRepName );
    }

    /* create thread�� �����Ѵ�. */
    if( mXLFCreater->isCreaterAlive() == ID_TRUE )
    {
        mXLFCreater->mExitFlag = ID_TRUE;
        mXLFCreater->wakeupCreater();
        IDE_TEST( mXLFCreater->join() != IDE_SUCCESS );
        mXLFCreater->finalize();

        ideLog::log( IDE_RP_0, "[Receiver] Replication %s finalized XLogfile Creater Thread", mRepName );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( flusher_die )
    {
        IDE_SET( ideSetErrorCode( rpERR_ABORT_RPD_FLUSHER_THREAD_NOT_ALIVE, mRepName ) );
        IDE_ERRLOG(IDE_RP_0);
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : rpdXLogfileMgr::destroySubThread           *
 * ------------------------------------------------------------------*
 * create thread�� flush thread�� ������ �����Ѵ�.
 *********************************************************************/
void rpdXLogfileMgr::destroySubThread()
{
    mXLFFlusher->destroy();
    mXLFCreater->destroy();

    return;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : rpdXLogfileMgr::getFileRefCnt              *
 * ------------------------------------------------------------------*
 * �ش� xlogfile�� reference count�� �����´�.
 *
 * aFile     - [IN] reference count�� ������ rpdXlogfile�� ������
 *********************************************************************/
UInt rpdXLogfileMgr::getFileRefCnt( rpdXLogfile * aFile )
{
    SInt    sRefCnt;

    IDE_ASSERT( aFile != NULL );

    IDE_ASSERT( aFile->mRefCntMutex.lock( NULL ) == IDE_SUCCESS );
    sRefCnt = aFile->mFileRefCnt;
    IDE_ASSERT( aFile->mRefCntMutex.unlock() == IDE_SUCCESS );

    return sRefCnt;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : rpdXLogfileMgr::incFileRefCnt              *
 * ------------------------------------------------------------------*
 * �ش� xlogfile�� reference count�� ������Ų��.
 * ������Ű�� �� ���� 0�� ��� �ٸ� thread�� ���� ���ŵǴ� ���̹Ƿ� 
 * aIsSuccess�� false�� �����Ѵ�.
 *
 * aFile      - [IN]  reference count�� ������ų rpdXlogfile�� ������
 *********************************************************************/
void rpdXLogfileMgr::incFileRefCnt( rpdXLogfile * aFile )
{
    IDE_ASSERT( aFile != NULL );

    IDE_ASSERT( aFile->mRefCntMutex.lock( NULL ) == IDE_SUCCESS );

    aFile->mFileRefCnt++;

    IDE_ASSERT( aFile->mRefCntMutex.unlock() == IDE_SUCCESS );

    return;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : rpdXLogfileMgr::decFileRefCnt              *
 * ------------------------------------------------------------------*
 * �ش� xlogfile�� reference count�� ���ҽ�Ų��.
 * ���� ���ҽ�Ų �� 0�̶�� �ش� ������ �����ϰ� ����Ʈ���� �����Ѵ�.
 *
 * aFile     - [IN] reference count�� ���ҽ�ų rpdXlogfile�� ������
 *********************************************************************/
void rpdXLogfileMgr::decFileRefCnt( rpdXLogfile * aFile )
{
    IDE_DASSERT( aFile != NULL );

    IDE_ASSERT( aFile->mRefCntMutex.lock( NULL ) == IDE_SUCCESS );

    IDE_DASSERT( aFile->mFileRefCnt > 0 );

    aFile->mFileRefCnt--;

    IDE_ASSERT( aFile->mRefCntMutex.unlock() == IDE_SUCCESS );

    if( aFile->mFileRefCnt == 0 )
    {
        /* reference count�� 0�� �Ǿ��ٸ� �ش� ������ �����Ѵ�. */
        removeFile( aFile );
    }
    else
    {
        /* reference count�� 0�� �ƴ϶�� �ش� ������ �����Ѵ�. */
    }

    return;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : rpdXLogfileMgr::getReadXLogLSNWithLock     *
 * ------------------------------------------------------------------*
 * read lock�� ���� ���¿��� read xlogLSN�� �����´�.
 * �ش� �Լ��� read�ܿ��� read�� ������ �����ϱ� ���� ���ȴ�.
 *********************************************************************/
rpXLogLSN rpdXLogfileMgr::getReadXLogLSNWithLock()
{
    rpXLogLSN sReadXLogLSN = RP_XLOGLSN_INIT;

    IDE_ASSERT( mReadInfoMutex.lock( NULL ) == IDE_SUCCESS );
    sReadXLogLSN = mReadXLogLSN;
    IDE_ASSERT( mReadInfoMutex.unlock() == IDE_SUCCESS );

    return sReadXLogLSN;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : rpdXLogfileMgr::getReadFileNoWithLock     *
 * ------------------------------------------------------------------*
 * read lock�� ���� ���¿��� read���� ������ ��ȣ�� �����´�.
 * �ش� �Լ��� read�ܿ��� read�� ������ �����ϱ� ���� ���ȴ�.
 *********************************************************************/
UInt rpdXLogfileMgr::getReadFileNoWithLock()
{
    UInt    sReadFileNo;

    IDE_ASSERT( mReadInfoMutex.lock( NULL ) == IDE_SUCCESS );
    sReadFileNo = mReadFileNo;
    IDE_ASSERT( mReadInfoMutex.unlock() == IDE_SUCCESS );

    return sReadFileNo;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : rpdXLogfileMgr::setReadInfoWithLock        *
 * ------------------------------------------------------------------*
 * read lock�� ���� ���¿��� read info�� �����Ѵ�.
 * �ش� �Լ��� read���� read info�� �����Ҷ� ���ȴ�.
 * ���� : �⺻������ read info�� read������ ������ �����ϴ�.
 *********************************************************************/
void rpdXLogfileMgr::setReadInfoWithLock( rpXLogLSN aLSN,
                                          UInt      aFileNo )
{
    IDE_ASSERT( mReadInfoMutex.lock( NULL ) == IDE_SUCCESS );
    mReadXLogLSN = aLSN;
    mReadFileNo = aFileNo; 
    IDE_ASSERT( mReadInfoMutex.unlock() == IDE_SUCCESS );

    return;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : rpdXLogfileMgr::getWriteXLogLSNWithLock    *
 * ------------------------------------------------------------------*
 * write lock�� ���� ���¿��� write xlogLSN�� �����´�.
 * �ش� �Լ��� write�ܿ��� write�� ������ �����ϱ� ���� ���ȴ�.
 *********************************************************************/
rpXLogLSN rpdXLogfileMgr::getWriteXLogLSNWithLock()
{
    rpXLogLSN sWriteXLogLSN = RP_XLOGLSN_INIT;

    IDE_ASSERT( mWriteInfoMutex.lock( NULL ) == IDE_SUCCESS );
    sWriteXLogLSN = mWriteXLogLSN;
    IDE_ASSERT( mWriteInfoMutex.unlock() == IDE_SUCCESS );

    return sWriteXLogLSN;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : rpdXLogfileMgr::getWriteFileNoWithLock     *
 * ------------------------------------------------------------------*
 * read lock�� ���� ���¿��� write���� ������ ��ȣ�� �����´�.
 * �ش� �Լ��� write�ܿ��� write�� ������ �����ϱ� ���� ���ȴ�.
 *********************************************************************/
UInt rpdXLogfileMgr::getWriteFileNoWithLock()
{
    UInt    sWriteFileNo;

    IDE_ASSERT( mWriteInfoMutex.lock( NULL ) == IDE_SUCCESS );
    sWriteFileNo = mWriteFileNo;
    IDE_ASSERT( mWriteInfoMutex.unlock() == IDE_SUCCESS );

    return sWriteFileNo;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : rpdXLogfileMgr::setWriteInfoWithLock        *
 * ------------------------------------------------------------------*
 * write lock�� ���� ���¿��� write info�� �����Ѵ�.
 * �ش� �Լ��� write���� write info�� �����Ҷ� ���ȴ�.
 * ���� : �⺻������ write info�� write������ ������ �����ϴ�.
 *********************************************************************/
void rpdXLogfileMgr::setWriteInfoWithLock( rpXLogLSN aLSN,
                                           UInt      aFileNo )
{
    IDE_ASSERT( mWriteInfoMutex.lock( NULL ) == IDE_SUCCESS );
    mWriteXLogLSN = aLSN;
    mWriteFileNo = aFileNo;
    IDE_ASSERT( mWriteInfoMutex.unlock() == IDE_SUCCESS );

    return;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : rpdXLogfileMgr::addToFileListinLast        *
 * ------------------------------------------------------------------*
 * xlogfile list�� ������(tail)�� �� ������ �߰��Ѵ�.
 * �ش� �Լ��� create thread���� ���� create/open�� ������ list�� �����Ҷ� ����Ѵ�.
 *
 * aNewFile - [IN] xlogfile list�� �߰��� xlogfile(rpdXLogfile)
 *********************************************************************/
void rpdXLogfileMgr::addToFileListinLast( rpdXLogfile * aNewFile )
{
    IDE_DASSERT( aNewFile != NULL );

    IDE_ASSERT( mXLogfileListMutex.lock( NULL ) == IDE_SUCCESS );

    if( mXLogfileListTail == NULL )
    {
        mXLogfileListHeader = aNewFile;
        mXLogfileListTail = aNewFile;
    }
    else
    {
        IDE_DASSERT( mXLogfileListHeader != NULL );

        aNewFile->mPrevXLogfile = mXLogfileListTail;
        mXLogfileListTail->mNextXLogfile = aNewFile;
        mXLogfileListTail = aNewFile;
    }

    IDE_ASSERT( mXLogfileListMutex.unlock() == IDE_SUCCESS );

    return;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : rpdXLogfileMgr::openAndMappingFile         *
 * ------------------------------------------------------------------*
 * �� ������ open/mapping�� �����ϰ� xlogfile list�� �����Ѵ�.
 * ���� ��ġ�� ���ڷ� �־��� ���� �ٷ� �����̴�.
 *
 * aXLogfileNo - [IN] ���� ���� mapping�� ������ ��ȣ
 * aBeforeFile - [IN] xlogfile list�� �߰��ϱ� ���� ��ġ
                      �ش� ������ �ٷ� ������ �� ������ �����Ѵ�.
 * aNewFile   -  [OUT] ���� �߰��� ������ ������
 *********************************************************************/
IDE_RC rpdXLogfileMgr::openAndMappingFile( UInt             aXLogfileNo,
                                           rpdXLogfile **   aNewFile )
{
    rpdXLogfile *   sNewFile;
    SChar           sBuffer[IDU_MUTEX_NAME_LEN];
    SChar           sNewFilePath[XLOGFILE_PATH_MAX_LENGTH];
    SInt            sLengthCheck = 0;

    /* ���� ������ ������ �� ������ �ƴ϶�� �� ������ ���� ����ؾ� �Ѵ�. */
    IDE_TEST( iduMemMgr::malloc( IDU_MEM_RP_RPD,
                                 ID_SIZEOF( rpdXLogfile ),
                                 (void**)&(sNewFile) )
              != IDE_SUCCESS );

    /* xlogfile meta(rpdXLogfile)������ mutex�� �ʱ�ȭ �Ѵ�. */
    sLengthCheck = idlOS::snprintf( sBuffer,
                                    IDU_MUTEX_NAME_LEN,
                                    "REF_xlogfile%s_%"ID_UINT32_FMT,
                                    mRepName,
                                    aXLogfileNo );
    IDE_TEST_RAISE( sLengthCheck == IDU_MUTEX_NAME_LEN, too_long_mutex_name );

    IDE_TEST( sNewFile->mRefCntMutex.initialize( sBuffer,
                                                 IDU_MUTEX_KIND_POSIX,
                                                 IDV_WAIT_INDEX_NULL ) != IDE_SUCCESS );

    IDE_TEST( sNewFile->mFile.initialize( IDU_MEM_RP_RPD, //���� xlogfile�� �߰��� ��
                                          1, /* Max Open FD Count*/
                                          IDU_FIO_STAT_OFF,
                                          IDV_WAIT_INDEX_NULL )
              != IDE_SUCCESS );

    /* ���� �� ������ �̸��� �����Ѵ�. */
    sLengthCheck = idlOS::snprintf( sNewFilePath,
                                    XLOGFILE_PATH_MAX_LENGTH,
                                    "%s%cxlogfile%s_%"ID_UINT32_FMT,
                                    mXLogDirPath,
                                    IDL_FILE_SEPARATOR,
                                    mRepName,
                                    aXLogfileNo );
    IDE_TEST_RAISE( sLengthCheck == XLOGFILE_PATH_MAX_LENGTH, too_long_file_path );

    IDE_TEST( sNewFile->mFile.setFileName( sNewFilePath ) != IDE_SUCCESS );

    /* �� ������ open�Ѵ�. */
    IDE_TEST( sNewFile->mFile.openUntilSuccess( ID_FALSE, O_RDWR ) != IDE_SUCCESS );

    /* mapping�� �����Ѵ�. */
    IDE_TEST( sNewFile->mFile.mmap( NULL,
                                    RPU_REPLICATION_XLOGFILE_SIZE,
                                    ID_TRUE,
                                    &( sNewFile->mBase ) ) != IDE_SUCCESS );

    /* rpdXLogfile�� ��Ÿ ������ �����Ѵ�. */
    setFileMeta( sNewFile, aXLogfileNo ); 

    *aNewFile = sNewFile;

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

IDE_RC rpdXLogfileMgr::openAndReadHeader( SChar * aRepName,
                                          SChar * aPath,
                                          UInt    aXLogfileNo,
                                          rpdXLogfileHeader * aOutHeader )
{
    rpdXLogfile     sNewFile;
    SChar           sNewFilePath[XLOGFILE_PATH_MAX_LENGTH];
    SInt            sLengthCheck = 0;

    /* ���� �� ������ �̸��� �����Ѵ�. */
    sLengthCheck = idlOS::snprintf( sNewFilePath,
                                    XLOGFILE_PATH_MAX_LENGTH,
                                    "%s%cxlogfile%s_%"ID_UINT32_FMT,
                                    aPath,
                                    IDL_FILE_SEPARATOR,
                                    aRepName,
                                    aXLogfileNo );
    IDE_TEST_RAISE( sLengthCheck == XLOGFILE_PATH_MAX_LENGTH, too_long_file_path );
    IDE_TEST( sNewFile.mFile.initialize( IDU_MEM_RP_RPD, //���� xlogfile�� �߰��� ��
                                          1, /* Max Open FD Count*/
                                          IDU_FIO_STAT_OFF,
                                          IDV_WAIT_INDEX_NULL )
              != IDE_SUCCESS );
    IDE_TEST( sNewFile.mFile.setFileName( sNewFilePath ) != IDE_SUCCESS );

    /* �� ������ open�Ѵ�. */
    IDE_TEST( sNewFile.mFile.openUntilSuccess( ID_FALSE, O_RDWR ) != IDE_SUCCESS );

    IDE_TEST(sNewFile.mFile.read(NULL,
                                  0,
                                  (void*)aOutHeader,
                                  ID_SIZEOF(rpdXLogfileHeader)) != IDE_SUCCESS);
    IDE_TEST( sNewFile.mFile.destroy() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( too_long_file_path )
    {
        IDE_SET( ideSetErrorCode( rpERR_ABORT_RPD_TOO_LONG_FILE_PATH, sNewFilePath ) );
        IDE_ERRLOG(IDE_RP_0);
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : rpdXLogfileMgr::addNewFileToFileList       *
 * ------------------------------------------------------------------*
 * �� ������ xlogfile list�� �߰��Ѵ�.
 * �߰��ϴ� ��ġ�� ���ڷ� �ѱ� ���� �����̴�.
 * aBeforeFile - [IN] xlogfile list�� �߰��ϱ� ���� ��ġ
                      �ش� ������ �ٷ� ������ �� ������ �����Ѵ�.
 * aNewFile   -  [IN] ���� �߰��� ������ ������
 *********************************************************************/
void rpdXLogfileMgr::addNewFileToFileList( rpdXLogfile *  aBeforeFile,
                                           rpdXLogfile *  aNewFile )
{
    IDE_DASSERT( aNewFile != NULL );

    /* xlogfile list�� ������Ѿ� �ϹǷ� list lock�� ��´�. */
    IDE_ASSERT( mXLogfileListMutex.lock( NULL ) == IDE_SUCCESS );

    aNewFile->mPrevXLogfile = aBeforeFile;

    if( aBeforeFile != NULL )
    {
        aNewFile->mNextXLogfile = aBeforeFile->mNextXLogfile;

        if( aBeforeFile->mNextXLogfile != NULL )
        {
            aBeforeFile->mNextXLogfile->mPrevXLogfile = aNewFile;
        }

        aBeforeFile->mNextXLogfile = aNewFile;
    }

    if( mXLogfileListTail == NULL )
    {
        mXLogfileListTail = aNewFile;
    }

    if( mXLogfileListHeader == NULL )
    {
        mXLogfileListHeader = aNewFile;
    }

    IDE_ASSERT( mXLogfileListMutex.unlock() == IDE_SUCCESS );

    return;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : rpdXLogfileMgr::removeFile                 *
 * ------------------------------------------------------------------*
 * �ش��ϴ� xlogfile�� �޸𸮿��� �����Ѵ�.(������ ���������� �ʴ´�.)
 * read�� flush��� ����� ���� ������ �����Ҷ� ȣ���Ѵ�.
 * �ش� ������ file reference count�� 0�̾�� �ϸ� ���� lock�� ��� ���� 
 * �ٸ� thread�� ������ reference count�� ���������� ��쿡��
 * �ش� ������ ���Ÿ� ����Ѵ�.
 * aFile   -  [IN] ������ ������ ������ 
 *********************************************************************/
void rpdXLogfileMgr::removeFile( rpdXLogfile * aFile )
{
    IDE_DASSERT( aFile != NULL );

    /* file list�� �����ؾ� �ϹǷ� list lock�� ��´�. */
    IDE_ASSERT( mXLogfileListMutex.lock( NULL ) == IDE_SUCCESS );

    IDE_ASSERT( aFile->mRefCntMutex.lock( NULL ) == IDE_SUCCESS );

    if( aFile->mFileRefCnt > 0 )
    {
        /* list mutex�� ��� ���̿� �ٸ� thread�� �����Ͽ�  *
         * ref cnt�� ������Ų ��� ���Ÿ� �����Ѵ�.         */
        IDE_ASSERT( aFile->mRefCntMutex.unlock() == IDE_SUCCESS );
        IDE_ASSERT( mXLogfileListMutex.unlock() == IDE_SUCCESS );

        IDE_CONT( normal_Exit );
    }

    if( aFile->mPrevXLogfile != NULL )
    {
        aFile->mPrevXLogfile->mNextXLogfile = aFile->mNextXLogfile;
    }

    if( aFile->mNextXLogfile != NULL )
    {
        aFile->mNextXLogfile->mPrevXLogfile = aFile->mPrevXLogfile;
    }

    if( mXLogfileListHeader == aFile )
    {
        mXLogfileListHeader = aFile->mNextXLogfile;
    }

    if( mXLogfileListTail == aFile )
    {
        mXLogfileListTail = aFile->mPrevXLogfile;
    }

    IDE_ASSERT( aFile->mRefCntMutex.unlock() == IDE_SUCCESS );

    IDE_ASSERT( mXLogfileListMutex.unlock() == IDE_SUCCESS );

    if( idlOS::munmap( aFile->mBase, RPU_REPLICATION_XLOGFILE_SIZE ) == IDE_SUCCESS )
    {
        (void)aFile->mRefCntMutex.destroy();
        (void)aFile->mFile.close();
        (void)aFile->mFile.destroy();
        (void)iduMemMgr::free( aFile );
    }
    else
    {
        IDE_ERRLOG(IDE_RP_0);
        (void)aFile->mRefCntMutex.destroy();
    }

    RP_LABEL( normal_Exit );

    return;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : rpdXLogfileMgr::setFileMeta                *
 * ------------------------------------------------------------------*
 * ���� mapping�� ������ ��Ÿ�� �ʱⰪ���� �����Ѵ�.
 *
 * aFile    - [IN] ���� mapping�� ������ ������
 * aFileNo  - [IN] ���� mapping�� ���Ͽ� �ο��� ���� ��ȣ
 *********************************************************************/
void rpdXLogfileMgr::setFileMeta( rpdXLogfile *  aFile,
                                  UInt           aFileNo )
{
    IDE_DASSERT( aFile != NULL );

    aFile->mFileNo = aFileNo;
    aFile->mFileRefCnt = 1;
    aFile->mPrevXLogfile = NULL;
    aFile->mNextXLogfile = NULL;
    aFile->mIsWriteEnd = ID_FALSE;

    return;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : rpdXLogfileMgr::rpdXLogfileEmergency       *
 * ------------------------------------------------------------------*
 * sync, create��� ���ܰ� �߻������� smiSetEmergency�� ��ü�ϱ� ���� �Լ�
 * ����� �ƹ��� ������ ���� ������ ���� I/O ���� �۾� �� ���� �߻��� 
 * �ļ� �۾��� �߰��ϰ� ���� ��� ���⿡ �߰��ϵ��� �Ѵ�.
 *
 * ���� : ���� ��Ȳ������ ���ڰ� true�� ���´�.
 *********************************************************************/
void rpdXLogfileMgr::rpdXLogfileEmergency( idBool /* aFlag */ )
{
    return ;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : rpdXLogfileMgr::byteToString               *
 * ------------------------------------------------------------------*
 * xlogfile�� read/write�� Ȯ���ϱ� ���� trc�α׿� ����ϱ� ���� ����ϴ� �Լ�
 * qcsModule::byteToString �Լ����� ������ ����Ѵ�.
 * �ش� �Լ��� �׽�Ʈ�����θ� ���Ǿ�� �ϸ� �Ϲ����� �۾��ÿ��� ���Ǽ��� �ȵȴ�.
 *********************************************************************/
IDE_RC rpdXLogfileMgr::byteToString( UChar   * aByte,
                                     UShort    aByteSize,
                                     UChar   * aString,
                                     UShort  * aStringSize )
{
    SChar   sByteValue;
    SChar   sStringValue;
    SInt    sByteIterator;
    SInt    sStringIterator;

    for( sByteIterator = 0, sStringIterator = 0;
         sByteIterator < (SInt) aByteSize;
         sByteIterator++, sStringIterator += 2)
    {
        sByteValue = aByte[sByteIterator];

        sStringValue = (sByteValue & 0xF0) >> 4;
        aString[sStringIterator+0] =
            (sStringValue < 10) ? (sStringValue + '0') : (sStringValue + 'A' - 10);

        sStringValue = (sByteValue & 0x0F);
        aString[sStringIterator+1] =
            (sStringValue < 10) ? (sStringValue + '0') : (sStringValue + 'A' - 10);
    }
            
    *aStringSize = (UShort) sStringIterator;

    return IDE_SUCCESS;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : rpdXLogfileMgr::getXLogfileNumberInternal  *
 * ------------------------------------------------------------------*
 * Ư�� XSN�� ��� xlogfile�� �ִ��� üũ�ϴ� �Լ�
 * �־��� ���� ��ȣ���� ���������� �����ϸ鼭 xlogfile header�� mFileFirstXSN�� ������ ���Ѵ�.
 * Ž���� ������ ��� �ش� ������ ��ȣ�� �����ϰ� Ž���� ������ ��� -1�� �����Ѵ�.
 * 
 * aXSN         - [IN]  Ž���� XSN
 * aStartFileNo - [IN]  Ž���� ������ ���Ϲ�ȣ
 * aRepName     - [IN]  �ش� replication�� �̸�
 * aFileNo      - [OUT] Ž���� ������ ��ȣ
 *********************************************************************/
IDE_RC rpdXLogfileMgr::getXLogfileNumberInternal( smSN        aXSN,
                                                  SInt        aStartFileNo,
                                                  SChar *     aReplName,
												  rpXLogLSN * aXLogLSN )
{
    SInt                sFileNo = aStartFileNo;
    SChar               sFilePath[XLOGFILE_PATH_MAX_LENGTH] = {0,};
    iduFile             sFile;
    SChar *             sXLogDirPath = NULL;
    idBool              sIsFind = ID_FALSE;
    UInt                sLengthCheck = 0;
    rpdXLogfileHeader   sFileHeader;
    rpXLogLSN           sFirstXLogLSN = RP_XLOGLSN_INIT;
    idBool              sIsInitializedFile = ID_FALSE;
    idBool              sIsOpenedFile = ID_FALSE;
#if defined(DEBUG)
    rpdXLogfileHeader   sFileHeaderForCheck;
#endif

    IDE_DASSERT( aStartFileNo >= 0 );

    IDE_TEST( idp::readPtr( "XLOGFILE_DIR", (void**)&sXLogDirPath ) != IDE_SUCCESS );

    IDE_TEST( sFile.initialize( IDU_MEM_OTHER,
                                1, /* Max Open FD Count*/
                                IDU_FIO_STAT_OFF,
                                IDV_WAIT_INDEX_NULL )
              != IDE_SUCCESS );
    sIsInitializedFile = ID_TRUE;

    while( ( sFileNo >= -1 ) && ( sIsFind == ID_FALSE ) )
    {
        idlOS::memset( sFilePath, 0, XLOGFILE_PATH_MAX_LENGTH );

        sLengthCheck = idlOS::snprintf( sFilePath,
                                        XLOGFILE_PATH_MAX_LENGTH,
                                        "%s%cxlogfile%s_%"ID_UINT32_FMT,
                                        sXLogDirPath,
                                        IDL_FILE_SEPARATOR,
                                        aReplName,
                                        sFileNo );

        IDE_TEST_RAISE( sLengthCheck == XLOGFILE_PATH_MAX_LENGTH, too_long_file_path );

        if( rpdXLogfileCreater::isFileExist( sFilePath ) == ID_FALSE )
        {
            /* �� �� ���� �� Ž���ߴµ� �ش��ϴ� ������ ���� ���(Ž������) */
            break;
        }
        else
        {
            IDE_TEST( sFile.setFileName( sFilePath ) != IDE_SUCCESS );

            /* ������ open�Ѵ�. */
            IDE_TEST( sFile.openUntilSuccess( ID_FALSE, O_RDONLY ) != IDE_SUCCESS );
            sIsOpenedFile = ID_TRUE;

            /* header(file write offset) �� read�� �����Ѵ�. */
            IDE_TEST( sFile.read( NULL,
                                  0,
                                  &sFileHeader,
                                  XLOGFILE_HEADER_SIZE_INIT ) != IDE_SUCCESS );

            if( ( sFileHeader.mFileFirstXSN <= aXSN ) && ( sFileHeader.mFileFirstXSN != SM_SN_NULL ) )
            {
                sIsFind = ID_TRUE;

                IDE_DASSERT( sFileHeader.mFileFirstXLogOffset != RP_XLOGLSN_INIT );
                sFirstXLogLSN = RP_SET_XLOGLSN( sFileNo, sFileHeader.mFileFirstXLogOffset );

                sIsOpenedFile = ID_FALSE;
                IDE_TEST( sFile.close() != IDE_SUCCESS );

#if defined(DEBUG)
                /* Ȥ�ó� �ؼ� �־�δ� �׽�Ʈ *
                 * ���� ������ üũ�ؼ� XSN �񱳰� �����ϴ��� üũ�Ѵ�.  */
                sLengthCheck = idlOS::snprintf( sFilePath,
                                                XLOGFILE_PATH_MAX_LENGTH,
                                                "%s%cxlogfile%s_%"ID_UINT32_FMT,
                                                sXLogDirPath,
                                                IDL_FILE_SEPARATOR,
                                                aReplName,
                                                sFileNo + 1);
                
                if( rpdXLogfileCreater::isFileExist( sFilePath ) == ID_TRUE )
                {
                    IDE_TEST( sFile.setFileName( sFilePath ) != IDE_SUCCESS );
                    IDE_TEST( sFile.openUntilSuccess( ID_FALSE, O_RDWR ) != IDE_SUCCESS );
                    sIsOpenedFile = ID_FALSE;
                    IDE_TEST( sFile.read( NULL,
                                          0,
                                          &sFileHeaderForCheck,
                                          XLOGFILE_HEADER_SIZE_INIT ) != IDE_SUCCESS );
                    ideLog::log(IDE_RP_0,"[CHECK][nextfile] fileno = %d, aXSN = %lu, fileFirstXSN = %lu",
                                sFileNo+1, aXSN, sFileHeaderForCheck.mFileFirstXSN );
                    sIsOpenedFile = ID_FALSE;
                    IDE_TEST( sFile.close() != IDE_SUCCESS );
                }
                else
                {
                    ideLog::log(IDE_RP_0,"[CHECK] nextfile is not exist" );
                }

#endif
                break;
            }
            else
            {
                /* �ش� ���Ͽ� Ž�� XSN�� �ش��ϴ� xlog�� ���� ��� �� ���Ͽ� ���� �ݺ��Ѵ�. */
                sIsFind = ID_FALSE;
                sFileNo--;

                sIsOpenedFile = ID_FALSE;
                IDE_TEST( sFile.close() != IDE_SUCCESS );
            }
        }
    }

    sIsInitializedFile = ID_FALSE;
    IDE_TEST( sFile.destroy() != IDE_SUCCESS );

     /* �� sFileNo�� -1 �̶��, ������ �����ϳ� xlog�� �ѹ��� ������ �ȵ� ����.
      * �̶��� XLOGFILE_HEADER_SIZE_INIT ������ �����Ѵ�.
      */
     if ( sFirstXLogLSN == RP_XLOGLSN_INIT )
     {
         sFirstXLogLSN = RP_SET_XLOGLSN( 0, XLOGFILE_HEADER_SIZE_INIT );
     }

    *aXLogLSN = sFirstXLogLSN;

    return IDE_SUCCESS;

    IDE_EXCEPTION( too_long_file_path )
    {
        IDE_SET( ideSetErrorCode( rpERR_ABORT_RPD_TOO_LONG_FILE_PATH, sFilePath ) );
        IDE_ERRLOG(IDE_RP_0);
    }
    IDE_EXCEPTION_END;

    IDE_PUSH();

    if ( sIsOpenedFile == ID_TRUE )
    {
        (void)sFile.close();
    }

    if ( sIsInitializedFile == ID_TRUE )
    {
        (void)sFile.destroy();
    }

    IDE_POP();

    return IDE_FAILURE;
}


/*********************************************************************
 * FUNCTION DESCRIPTION : rpdXLogfileMgr::getXLogfileNumber          *
 * ------------------------------------------------------------------*
 * Ư�� XSN�� ��� xlogfile�� �ִ��� üũ�ϴ� �Լ�
 * �ش� �Լ��� xlogfileMgr�� �� ���� ���� ���¿����� ����ؾ� �Ѵ�.
 * ��Ÿ�� ����� read info�� ������ �ʾ� Ž�� ����� ��Ÿ���� �ڿ� ���� ���ɼ��� �����Ƿ�
 * ���� �ֽ� ������ ã�� ���������� �����ϸ鼭 xlogfile header�� mFileFirstXSN�� ������ ���Ѵ�.
 * 
 * aXSN         - [IN]  Ž���� XSN
 * aRepName     - [IN]  �ش� replication�� �̸�
 * aFileNo      - [OUT] Ž���� ������ LSN (FileNo, Offset)
 *********************************************************************/
IDE_RC rpdXLogfileMgr::getXLogLSNFromXSNAndReplName( smSN         aXSN,
                                                     SChar     * aReplName,
                                                     rpXLogLSN * aXLogLSN )
{
    UInt    sFirstFileNo = 0;
    UInt    sLastFileNo = 0;

    if( findFirstAndLastFileNo( aReplName, &sFirstFileNo, &sLastFileNo) == IDE_SUCCESS )
    {
        IDE_TEST( getXLogfileNumberInternal( aXSN,
                                             sLastFileNo,
                                             aReplName,
                                             aXLogLSN )
                  != IDE_SUCCESS );
    }
    else
    {
        IDE_TEST( ideGetErrorCode() != rpERR_ABORT_RP_CANNOT_FIND_XLOGFILE );
        aXLogLSN = RP_XLOGLSN_INIT;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : rpdXLogfileMgr::getXLogfileNumber          *
 * ------------------------------------------------------------------*
 * Ư�� XSN�� ��� xlogfile�� �ִ��� üũ�ϴ� �Լ�
 * �ش� �Լ��� xlogfileMgr�� �� �ִ� ���¿����� ����ؾ� �Ѵ�.
 * file list�� �����ϴ� ù ���Ϻ��� Ž���� �����Ͽ�
 * ���������� �����ϸ鼭 xlogfile header�� mFileFirstXSN�� ������ ���Ѵ�.
 * 
 * aXSN         - [IN]  Ž���� XSN
 * aFileNo      - [OUT] Ž���� ������ ��ȣ
 *********************************************************************/
IDE_RC rpdXLogfileMgr::getXLogLSNFromXSN( smSN        aXSN,
										  rpXLogLSN * aXLogLSN )
{
    SInt                  sFileNo = 0;

    IDE_ASSERT( mXLogfileListMutex.lock( NULL ) == IDE_SUCCESS );

    IDE_ASSERT( mXLogfileListHeader != NULL );

    sFileNo = mXLogfileListHeader->mFileNo;

    IDE_ASSERT( mXLogfileListMutex.unlock() == IDE_SUCCESS );

    IDE_TEST( getXLogfileNumberInternal( aXSN, 
                                         sFileNo,
                                         mRepName,
										 aXLogLSN ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

UInt rpdXLogfileMgr::checkXLogFileAndGetFileNoFromRepName( SChar * aReplName,
                                                           SChar * aFileName,
                                                           idBool * aIsXLogfile )
{

    UInt   i;
    UInt   sFileNo = 0;
    SChar  sChar;
    UInt   sPrefixLen;
    idBool sIsLogFile;
    SChar sPrefix[100];


    sIsLogFile = ID_TRUE;

    sPrefixLen = idlOS::snprintf( sPrefix,
                                  100,
                                  "xlogfile%s_",
                                  aReplName );

    if ( idlOS::strncmp(aFileName, sPrefix, sPrefixLen) == 0 )
    {
        for ( i = sPrefixLen; i < idlOS::strlen(aFileName); i++)
        {
            if ( (aFileName[i] >= '0') && (aFileName[i] <= '9'))
            {
                continue;
            }
            else
            {
                sIsLogFile = ID_FALSE;
            }
        }
    }
    else
    {
        sIsLogFile = ID_FALSE;
    }

    if ( sIsLogFile == ID_TRUE )
    {
        for ( i = sPrefixLen; i < idlOS::strlen(aFileName); i++)
        {
            sChar   = aFileName[i];
            sFileNo = sFileNo * 10;
            sFileNo = sFileNo + (sChar - '0');
        }
    }

    *aIsXLogfile = sIsLogFile;

    return sFileNo;
}

IDE_RC rpdXLogfileMgr::findFirstAndLastFileNo( SChar * aReplName,
                                               UInt * aFirstXLogFileNo,
                                               UInt * aLastXLogFileNo )
{
    DIR           * sDirp = NULL;
    struct dirent * sDir  = NULL;
    UInt            sCurLogFileNo;
    idBool          sIsSetTempLogFileName = ID_FALSE;
    idBool          sIsLogFile;
    UInt            sFirstFileNo = 0;
    UInt            sEndFileNo = 0;


    SChar * sXLogDirPath = NULL;

    IDE_TEST( idp::readPtr( "XLOGFILE_DIR", (void**)&sXLogDirPath ) != IDE_SUCCESS );

    sDirp = idf::opendir( sXLogDirPath );
    IDE_TEST_RAISE(sDirp == NULL, error_open_dir);

    while(1)
    {
        sDir = idf::readdir(sDirp);
        if ( sDir == NULL )
        {
            break;
        }

        sCurLogFileNo = checkXLogFileAndGetFileNoFromRepName(aReplName,
                                                             sDir->d_name,
                                                             &sIsLogFile);

        if ( sIsLogFile == ID_FALSE)
        {
            continue;
        }

        if ( sIsSetTempLogFileName == ID_FALSE)
        {
            sFirstFileNo = sCurLogFileNo;
            sEndFileNo = sCurLogFileNo;
            sIsSetTempLogFileName = ID_TRUE;
            continue;
        }

        if ( sFirstFileNo > sCurLogFileNo)
        {
            sFirstFileNo = sCurLogFileNo;
        }

        if ( sEndFileNo < sCurLogFileNo)
        {
            sEndFileNo = sCurLogFileNo;
        }

    };/*End of While*/

    idf::closedir(sDirp);

    IDE_TEST_RAISE(sIsSetTempLogFileName != ID_TRUE, ERR_CANNOT_FIND_ANY_XLOGFILE);

    *aFirstXLogFileNo = sFirstFileNo;
    *aLastXLogFileNo = sEndFileNo;

    return IDE_SUCCESS;

    IDE_EXCEPTION(error_open_dir);
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_CannotOpenDir));
    }
    IDE_EXCEPTION(ERR_CANNOT_FIND_ANY_XLOGFILE);
    {
        IDE_SET(ideSetErrorCode(rpERR_ABORT_RP_CANNOT_FIND_XLOGFILE, aReplName));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC rpdXLogfileMgr::findRemainFirstXLogLSN( SChar     * aReplName,
                                               rpXLogLSN * aFirstXLogLSN )
{
    UInt                sFirstFileNo = 0;
    UInt                sDummyFileNo = 0;
    SChar               sFilePath[XLOGFILE_PATH_MAX_LENGTH] = {0,};
    iduFile             sFile;
    SChar *             sXLogDirPath = NULL;
    UInt                sLengthCheck = 0;
    rpdXLogfileHeader   sFileHeader;
    rpXLogLSN           sFirstXLogLSN      = RP_XLOGLSN_INIT;
    idBool              sIsInitializedFile = ID_FALSE;
    idBool              sIsOpenedFile      = ID_FALSE;

    IDE_TEST( findFirstAndLastFileNo( aReplName, &sFirstFileNo, &sDummyFileNo) != IDE_SUCCESS );

    IDE_TEST( idp::readPtr( "XLOGFILE_DIR", (void**)&sXLogDirPath ) != IDE_SUCCESS );

    IDE_TEST( sFile.initialize( IDU_MEM_OTHER,
                                1, /* Max Open FD Count*/
                                IDU_FIO_STAT_OFF,
                                IDV_WAIT_INDEX_NULL )
              != IDE_SUCCESS );
    sIsInitializedFile = ID_TRUE;

    idlOS::memset( sFilePath, 0, XLOGFILE_PATH_MAX_LENGTH );
    sLengthCheck = idlOS::snprintf( sFilePath,
                                    XLOGFILE_PATH_MAX_LENGTH,
                                    "%s%cxlogfile%s_%"ID_UINT32_FMT,
                                    sXLogDirPath,
                                    IDL_FILE_SEPARATOR,
                                    aReplName,
                                    sFirstFileNo );
    IDE_TEST_RAISE( sLengthCheck == XLOGFILE_PATH_MAX_LENGTH, too_long_file_path );

    if ( rpdXLogfileCreater::isFileExist( sFilePath ) != ID_TRUE )
    {
        ideLog::log(IDE_RP_0,"[CHECK] nextfile is not exist" );
    }
    else
    {
        IDE_TEST( sFile.setFileName( sFilePath ) != IDE_SUCCESS );

        /* ������ open�Ѵ�. */
        IDE_TEST( sFile.openUntilSuccess( ID_FALSE, O_RDONLY ) != IDE_SUCCESS );
        sIsOpenedFile = ID_TRUE;

        /* header(file write offset) �� read�� �����Ѵ�. */
        IDE_TEST( sFile.read( NULL,
                              0,
                              &sFileHeader,
                              XLOGFILE_HEADER_SIZE_INIT ) != IDE_SUCCESS );
        IDE_DASSERT( sFileHeader.mFileFirstXLogOffset != RP_XLOGLSN_INIT );
        
        sFirstXLogLSN = RP_SET_XLOGLSN( sFirstFileNo, sFileHeader.mFileFirstXLogOffset );

        sIsOpenedFile = ID_FALSE;
        IDE_TEST( sFile.close() != IDE_SUCCESS );
    }

    sIsInitializedFile = ID_FALSE;
    IDE_TEST( sFile.destroy() != IDE_SUCCESS );

    *aFirstXLogLSN = sFirstXLogLSN;

    return IDE_SUCCESS;

    IDE_EXCEPTION( too_long_file_path )
    {
        IDE_SET( ideSetErrorCode( rpERR_ABORT_RPD_TOO_LONG_FILE_PATH, sFilePath ) );
        IDE_ERRLOG(IDE_RP_0);
    }
    IDE_EXCEPTION_END;

    if ( sIsOpenedFile == ID_TRUE )
    {
        (void)sFile.close();
    }

    if ( sIsInitializedFile == ID_TRUE )
    {
        (void)sFile.destroy();
    }

    return IDE_FAILURE;
}


