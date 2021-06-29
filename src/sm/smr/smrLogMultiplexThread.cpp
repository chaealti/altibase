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
 * $$Id$
 * �α� ����ȭ thread ���� ����
 * �α� ����ȭ���� append thread(log buffer�� �α� ��� ����ȭ)
 *                 sync   thread(log file�� �α� ��� ����ȭ)
 *                 create thread(log file ���� ����ȭ )
 *                 delete thread(log file ���� ����ȭ )
 *  �� �ʿ� �ϰ� �� �װ��� thread�� �����Ǿ��ִ�.
 **********************************************************************/

#include <smErrorCode.h>
#include <smr.h>
#include <smrReq.h>


/* appen log parameter */
volatile UInt        smrLogMultiplexThread::mOriginalCurFileNo;

/* sync log parameter */
idBool               smrLogMultiplexThread::mSyncLstPageInLstLF;
smrSyncByWho         smrLogMultiplexThread::mWhoSync;
UInt                 smrLogMultiplexThread::mOffsetToSync;
UInt                 smrLogMultiplexThread::mFileNoToSync;

/* create logfile parameter */
SChar              * smrLogMultiplexThread::mLogFileInitBuffer;
UInt               * smrLogMultiplexThread::mTargetFileNo;
UInt                 smrLogMultiplexThread::mLstFileNo;
SInt                 smrLogMultiplexThread::mLogFileSize;

/* delete logfile parameter */
idBool               smrLogMultiplexThread::mIsCheckPoint;
UInt                 smrLogMultiplexThread::mDeleteFstFileNo;
UInt                 smrLogMultiplexThread::mDeleteLstFileNo;

/* class member variable */
idBool               smrLogMultiplexThread::mFinish;
UInt                 smrLogMultiplexThread::mMultiplexCnt;
smrLogFileOpenList * smrLogMultiplexThread::mLogFileOpenList;
const SChar        * smrLogMultiplexThread::mOriginalLogPath;
UInt                 smrLogMultiplexThread::mOriginalLstLogFileNo;
idBool               smrLogMultiplexThread::mIsOriginalLogFileCreateDone;

smrLogMultiplexThread::smrLogMultiplexThread() : idtBaseThread()
{
}

smrLogMultiplexThread::~smrLogMultiplexThread()
{
}

/***********************************************************************
 * Description :
 * log���� ����ȭ�� �����ϴ� thread�� �ʱ�ȭ�Ѵ�.
 *
 * aSyncThread      - [IN/OUT] smrFileLogMgr�� syncThread pointer
 * aCreateThread    - [IN/OUT] smrFileLogMgr�� createThread pointer
 * aDeleteThread    - [IN/OUT] smrFileLogMgr�� deleteThread pointer
 * aAppendThread    - [IN/OUT] smrFileLogMgr�� appendThread pointer
 * aOriginalLogPath - [IN] ���������� ���� ��ġ
 ***********************************************************************/
IDE_RC smrLogMultiplexThread::initialize( 
                                smrLogMultiplexThread  ** aSyncThread,
                                smrLogMultiplexThread  ** aCreateThread,
                                smrLogMultiplexThread  ** aDeleteThread,
                                smrLogMultiplexThread  ** aAppendThread,
                                const SChar             * aOriginalLogPath )
{
    UInt           sMultiplexIdx;
    const SChar ** sLogMultiplexPath;
    UInt           sState                = 0;
    UInt           sSyncState            = 0;
    UInt           sCreateState          = 0;
    UInt           sDeleteState          = 0;
    UInt           sAppendState          = 0;
    UInt           sLogFileOpenListState = 0;

    IDE_ASSERT( aSyncThread    != NULL );
    IDE_ASSERT( aCreateThread  != NULL );
    IDE_ASSERT( aDeleteThread  != NULL );
    IDE_ASSERT( aAppendThread  != NULL );

    sLogMultiplexPath  = smuProperty::getLogMultiplexDirPath();
    mMultiplexCnt      = smuProperty::getLogMultiplexCount();
    mFinish            = ID_FALSE;
    mOriginalLogPath   = aOriginalLogPath;

    IDE_TEST_CONT( mMultiplexCnt == 0, skip_multiplex_log_file );

    /* smrLogMultiplexThread_initialize_calloc_SyncThread.tc */
    IDU_FIT_POINT("smrLogMultiplexThread::initialize::calloc::SyncThread");
    IDE_TEST( iduMemMgr::calloc( 
                            IDU_MEM_SM_SMR,
                            1,
                            (ULong)ID_SIZEOF( smrLogMultiplexThread ) * mMultiplexCnt,
                            (void**)aSyncThread) 
              != IDE_SUCCESS );
    sState = 1;

    /* smrLogMultiplexThread_initialize_calloc_CreateThread.tc */
    IDU_FIT_POINT("smrLogMultiplexThread::initialize::calloc::CreateThread");
    IDE_TEST( iduMemMgr::calloc( 
                            IDU_MEM_SM_SMR,
                            1,
                            (ULong)ID_SIZEOF( smrLogMultiplexThread ) * mMultiplexCnt,
                            (void**)aCreateThread) 
              != IDE_SUCCESS );
    sState = 2;

    /* smrLogMultiplexThread_initialize_calloc_DeleteThread.tc */
    IDU_FIT_POINT("smrLogMultiplexThread::initialize::calloc::DeleteThread");
    IDE_TEST( iduMemMgr::calloc( 
                            IDU_MEM_SM_SMR,
                            1,
                            (ULong)ID_SIZEOF( smrLogMultiplexThread ) * mMultiplexCnt,
                            (void**)aDeleteThread) 
              != IDE_SUCCESS );
    sState = 3;

    /* smrLogMultiplexThread_initialize_calloc_AppendThread.tc */
    IDU_FIT_POINT("smrLogMultiplexThread::initialize::calloc::AppendThread");
    IDE_TEST( iduMemMgr::calloc( 
                            IDU_MEM_SM_SMR,
                            1,
                            (ULong)ID_SIZEOF( smrLogMultiplexThread ) * mMultiplexCnt,
                            (void**)aAppendThread) 
              != IDE_SUCCESS );
    sState = 4;

    /* ����ȭ�� log���ϵ��� open log file list�� ���� �޸� �Ҵ� */
    /* smrLogMultiplexThread_initialize_calloc_LogFileOpenList.tc */
    IDU_FIT_POINT("smrLogMultiplexThread::initialize::calloc::LogFileOpenList");
    IDE_TEST( iduMemMgr::calloc( 
                            IDU_MEM_SM_SMR,
                            1,
                            (ULong)ID_SIZEOF( smrLogFileOpenList ) * mMultiplexCnt,
                            (void**)&mLogFileOpenList) 
              != IDE_SUCCESS );
    sState = 5;

    /* thread type�� ����ȭ ������ŭ ������ �ʱ�ȭ */
    for ( sMultiplexIdx = 0; sMultiplexIdx < mMultiplexCnt; sMultiplexIdx++ )
    {
        new(&((*aSyncThread)[sMultiplexIdx])) smrLogMultiplexThread();
        IDE_TEST( (*aSyncThread)[sMultiplexIdx].startAndInitializeThread( 
                                            sLogMultiplexPath[sMultiplexIdx],
                                            sMultiplexIdx,
                                            SMR_LOG_MPLEX_THREAD_TYPE_SYNC )
                  != IDE_SUCCESS );
        sSyncState++;

        new(&((*aCreateThread)[sMultiplexIdx])) smrLogMultiplexThread();
        IDE_TEST( (*aCreateThread)[sMultiplexIdx].startAndInitializeThread( 
                                            sLogMultiplexPath[sMultiplexIdx],
                                            sMultiplexIdx,
                                            SMR_LOG_MPLEX_THREAD_TYPE_CREATE )
                  != IDE_SUCCESS );
        sCreateState++;

        new(&((*aDeleteThread)[sMultiplexIdx])) smrLogMultiplexThread();
        IDE_TEST( (*aDeleteThread)[sMultiplexIdx].startAndInitializeThread( 
                                            sLogMultiplexPath[sMultiplexIdx],
                                            sMultiplexIdx,
                                            SMR_LOG_MPLEX_THREAD_TYPE_DELETE )
                  != IDE_SUCCESS );
        sDeleteState++;

        new(&((*aAppendThread)[sMultiplexIdx])) smrLogMultiplexThread();
        IDE_TEST( (*aAppendThread)[sMultiplexIdx].startAndInitializeThread( 
                                            sLogMultiplexPath[sMultiplexIdx],
                                            sMultiplexIdx,
                                            SMR_LOG_MPLEX_THREAD_TYPE_APPEND )
                  != IDE_SUCCESS );
        sAppendState++;

        /* ����ȭ �� open log file list�� �ʱ�ȭ */
        IDE_TEST( initLogFileOpenList( &mLogFileOpenList[sMultiplexIdx],
                                       sMultiplexIdx ) 
                  != IDE_SUCCESS );
        sLogFileOpenListState++;
    }
    
    IDE_EXCEPTION_CONT( skip_multiplex_log_file ); 

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sState != 0 )
    {
        for ( sMultiplexIdx = 0; 
              sMultiplexIdx <= sLogFileOpenListState; 
              sMultiplexIdx++ )
        {
            IDE_ASSERT( destroyLogFileOpenList( &mLogFileOpenList[sMultiplexIdx],
                                                &(*aSyncThread)[sMultiplexIdx] )
                        == IDE_SUCCESS );
        }
 
        for ( sMultiplexIdx = 0; 
              sMultiplexIdx <= sSyncState; 
              sMultiplexIdx++ )
        {
            IDE_ASSERT( (*aSyncThread)[sMultiplexIdx].shutdownAndDestroyThread() 
                        == IDE_SUCCESS );
        }
 
        for ( sMultiplexIdx = 0; 
              sMultiplexIdx <= sCreateState; 
              sMultiplexIdx++ )
        {
            IDE_ASSERT( (*aCreateThread)[sMultiplexIdx].shutdownAndDestroyThread() 
                        == IDE_SUCCESS );
        }
 
        for ( sMultiplexIdx = 0; 
              sMultiplexIdx <= sDeleteState; 
              sMultiplexIdx++ )
        {
            IDE_ASSERT( (*aDeleteThread)[sMultiplexIdx].shutdownAndDestroyThread() 
                        == IDE_SUCCESS );
        }
 
        for ( sMultiplexIdx = 0; 
              sMultiplexIdx <= sAppendState; 
              sMultiplexIdx++ )
        {
            IDE_ASSERT( (*aAppendThread)[sMultiplexIdx].shutdownAndDestroyThread() 
                        == IDE_SUCCESS );
        }
    }
    switch( sState )
    {
        case 5:
            IDE_ASSERT( iduMemMgr::free( mLogFileOpenList ) == IDE_SUCCESS );
        case 4:
            IDE_ASSERT( iduMemMgr::free( aAppendThread ) == IDE_SUCCESS );
        case 3:
            IDE_ASSERT( iduMemMgr::free( aDeleteThread ) == IDE_SUCCESS );
        case 2:
            IDE_ASSERT( iduMemMgr::free( aCreateThread ) == IDE_SUCCESS );
        case 1:
            IDE_ASSERT( iduMemMgr::free( aSyncThread )   == IDE_SUCCESS );
        case 0:
        default:
            break;
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description :
 * log���� ����ȭ�� �����ϴ� thread�� destroy�Ѵ�.
 *
 * aSyncThread    - [IN] smrLogMgr�� syncThread pointer
 * aCreateThread  - [IN] smrLogMgr�� createThread pointer
 * aDeleteThread  - [IN] smrLogMgr�� deleteThread pointer
 * aAppendThread  - [IN] smrLogMgr�� appendThread pointer
 ***********************************************************************/
IDE_RC smrLogMultiplexThread::destroy( 
                            smrLogMultiplexThread * aSyncThread,
                            smrLogMultiplexThread * aCreateThread,
                            smrLogMultiplexThread * aDeleteThread,
                            smrLogMultiplexThread * aAppendThread )
{
    UInt sMultiplexIdx;
    UInt sState                = 5;
    UInt sSyncState            = mMultiplexCnt;
    UInt sCreateState          = mMultiplexCnt;
    UInt sDeleteState          = mMultiplexCnt;
    UInt sAppendState          = mMultiplexCnt;
    UInt sLogFileOpenListState = mMultiplexCnt;

    IDE_TEST_CONT( mMultiplexCnt == 0, skip_destroy );

    mFinish = ID_TRUE;

    for ( sMultiplexIdx = 0; sMultiplexIdx < mMultiplexCnt; sMultiplexIdx++ )
    {
        sAppendState--;
        IDE_TEST( aAppendThread[sMultiplexIdx].shutdownAndDestroyThread()
                  != IDE_SUCCESS );

        sSyncState--;
        IDE_TEST( aSyncThread[sMultiplexIdx].shutdownAndDestroyThread()
                  != IDE_SUCCESS );

        sCreateState--;
        IDE_TEST( aCreateThread[sMultiplexIdx].shutdownAndDestroyThread()
                  != IDE_SUCCESS );

        sDeleteState--;
        IDE_TEST( aDeleteThread[sMultiplexIdx].shutdownAndDestroyThread()
                  != IDE_SUCCESS );

        sLogFileOpenListState--;
        IDE_TEST( destroyLogFileOpenList( &mLogFileOpenList[sMultiplexIdx],
                                          &aSyncThread[sMultiplexIdx] )
                  != IDE_SUCCESS );
    }
    sState = 4;
    IDE_TEST( iduMemMgr::free( aSyncThread ) != IDE_SUCCESS );

    sState = 3;
    IDE_TEST( iduMemMgr::free( aCreateThread ) != IDE_SUCCESS );

    sState = 2;
    IDE_TEST( iduMemMgr::free( aDeleteThread ) != IDE_SUCCESS );

    sState = 1;
    IDE_TEST( iduMemMgr::free( aAppendThread ) != IDE_SUCCESS );

    sState = 0;
    IDE_TEST( iduMemMgr::free( mLogFileOpenList ) != IDE_SUCCESS );

    IDE_EXCEPTION_CONT( skip_destroy ); 

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    for ( sMultiplexIdx = ( mMultiplexCnt - sAppendState ); 
          sMultiplexIdx < mMultiplexCnt; 
          sMultiplexIdx++ )
    {
        IDE_ASSERT( aAppendThread[sMultiplexIdx].shutdownAndDestroyThread()
                    == IDE_SUCCESS );
    }

    for ( sMultiplexIdx = ( mMultiplexCnt - sSyncState ); 
          sMultiplexIdx < mMultiplexCnt; 
          sMultiplexIdx++ )
    {
        IDE_ASSERT( aSyncThread[sMultiplexIdx].shutdownAndDestroyThread()
                    == IDE_SUCCESS );
    }

    for ( sMultiplexIdx = ( mMultiplexCnt - sCreateState ); 
          sMultiplexIdx < mMultiplexCnt; 
          sMultiplexIdx++ )
    {
        IDE_ASSERT( aCreateThread[sMultiplexIdx].shutdownAndDestroyThread()
                    == IDE_SUCCESS );
    }

    for ( sMultiplexIdx = ( mMultiplexCnt - sDeleteState ); 
          sMultiplexIdx < mMultiplexCnt; 
          sMultiplexIdx++ )
    {
        IDE_ASSERT( aDeleteThread[sMultiplexIdx].shutdownAndDestroyThread()
                    == IDE_SUCCESS );
    }

    for ( sMultiplexIdx = ( mMultiplexCnt - sLogFileOpenListState ); 
          sMultiplexIdx < mMultiplexCnt; 
          sMultiplexIdx++ )
    {
        IDE_ASSERT( destroyLogFileOpenList( &mLogFileOpenList[sMultiplexIdx],
                                            &aSyncThread[sMultiplexIdx] )
                    == IDE_SUCCESS );
    }

    switch( sState )
    {
        case 5:
            IDE_ASSERT( iduMemMgr::free( aSyncThread ) == IDE_SUCCESS );
        case 4:
            IDE_ASSERT( iduMemMgr::free( aCreateThread ) == IDE_SUCCESS );
        case 3:
            IDE_ASSERT( iduMemMgr::free( aDeleteThread ) == IDE_SUCCESS );
        case 2:
            IDE_ASSERT( iduMemMgr::free( aAppendThread ) == IDE_SUCCESS );
        case 1:
            IDE_ASSERT( iduMemMgr::free( mLogFileOpenList ) == IDE_SUCCESS );
        case 0:
        default:
            break;
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description :
 * ����ȭ thread�� �ʱ�ȭ�ϰ� start �Ѵ�.
 *
 * aMultiplexPath  - [IN] ����ȭ ���丮 
 * aMultiplexIdx   - [IN] �������� ����ȭ index ��ȣ
 * aThreadType     - [IN] �ʱ�ȭ�� thread Ÿ��(sync,append,create,delete)
 ***********************************************************************/
IDE_RC smrLogMultiplexThread::startAndInitializeThread( 
                                    const SChar              * aMultiplexPath, 
                                    UInt                       aMultiplexIdx,
                                    smrLogMultiplexThreadType  aThreadType )
{
    UInt    sState = 0;
    SChar   sMutexName[128] = {'\0',};
    SChar   sCVName[128] = {'\0',};

    mMultiplexIdx  = aMultiplexIdx;
    mThreadType    = aThreadType;
    mThreadState   = SMR_LOG_MPLEX_THREAD_STATE_WAIT;
    mMultiplexPath = aMultiplexPath;

    /* ����ȭ ���丮 �˻� */
    IDE_TEST( checkMultiplexPath() != IDE_SUCCESS );

    idlOS::snprintf( sCVName, 
                     128, 
                     "MULTIPLEX_LOG_TYPE_%"ID_UINT32_FMT"_COND_%"ID_UINT32_FMT, 
                     aThreadType,
                     aMultiplexIdx ); 

    IDE_TEST( mCv.initialize( sCVName ) != IDE_SUCCESS );
    sState = 1;

    idlOS::snprintf( sMutexName, 
                     128, 
                     "MULTIPLEX_LOG_TYPE_%"ID_UINT32_FMT"_MTX_%"ID_UINT32_FMT, 
                     aThreadType,
                     aMultiplexIdx ); 

    IDE_TEST( mMutex.initialize( sMutexName,
                                 IDU_MUTEX_KIND_POSIX,
                                 IDV_WAIT_INDEX_NULL )
              != IDE_SUCCESS );
    sState = 2;

    /* 
     * append thread�� ���� ���۽� log file open list�� ������
     * log file�� �߰��� ���Ŀ� �����Ѵ�. 
     */
    if ( mThreadType != SMR_LOG_MPLEX_THREAD_TYPE_APPEND )
    {
        IDE_TEST( startThread() != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch( sState )
    {
        case 2:
            IDE_ASSERT( mMutex.destroy() == IDE_SUCCESS );
        case 1:
            IDE_ASSERT( mCv.destroy() == IDE_SUCCESS );
        case 0:
        default:
            break;
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description :
 * ����ȭ log������ ������ ���丮�� �˻��Ѵ�.
 ***********************************************************************/
IDE_RC smrLogMultiplexThread::checkMultiplexPath()
{
    IDE_TEST_RAISE(idf::access( mMultiplexPath, F_OK) != 0,
                   path_exist_error);

    IDE_TEST_RAISE(idf::access( mMultiplexPath, W_OK) != 0,
                   err_no_write_perm_path);

    IDE_TEST_RAISE(idf::access( mMultiplexPath, X_OK) != 0, 
                   err_no_execute_perm_path);

    return IDE_SUCCESS;

    IDE_EXCEPTION(path_exist_error)
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_NoExistPath,
                                mMultiplexPath));
    }
    IDE_EXCEPTION(err_no_write_perm_path);
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_NoWritePermFile,
                                mMultiplexPath));
    }
    IDE_EXCEPTION(err_no_execute_perm_path);
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_NoExecutePermFile,
                                mMultiplexPath));
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description :
 * ����ȭ thread�� �����ϰ� destroy �Ѵ�.
 ***********************************************************************/
IDE_RC smrLogMultiplexThread::shutdownAndDestroyThread()
{
    UInt sState = 2;

    IDE_ASSERT( mFinish == ID_TRUE );

    if ( mThreadType != SMR_LOG_MPLEX_THREAD_TYPE_APPEND )
    {
        /* condwait�ϰ��ִ� thread�� ����� */
        IDE_TEST( wakeUp() != IDE_SUCCESS );
    }

    IDE_TEST( join() != IDE_SUCCESS );

    sState = 1;
    IDE_TEST( mCv.destroy() != IDE_SUCCESS );

    sState = 0;
    IDE_TEST( mMutex.destroy() != IDE_SUCCESS );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch( sState )
    {
        case 2:
            IDE_ASSERT( mCv.destroy() == IDE_SUCCESS );
        case 1:
            IDE_ASSERT( mMutex.destroy() == IDE_SUCCESS );
        case 0:
        default:
            break;
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description :
 * smrLFThread::syncToLSN���� ȣ��ȴ�.
 * ����ȭ�� log�� ������ ����ȭ�� log file�� sync�Ѵ�.
 *
 * aSyncThread             - [IN] log sync thread ��
 * aWhoSync,               - [IN] smrLFThread�� ���� sync���� tx������ sync����
 * aSyncLstPageInLstLF     - [IN] log file�� ������ ���̸� ��������
 * aFileNoToSync           - [IN] sync�� fileno ������
 * aOffsetToSync           - [IN] sync�� ������ ������ offset
 ***********************************************************************/
IDE_RC smrLogMultiplexThread::wakeUpSyncThread( 
                                smrLogMultiplexThread  * aSyncThread,
                                smrSyncByWho             aWhoSync,
                                idBool                   aSyncLstPageInLstLF,
                                UInt                     aFileNoToSync,
                                UInt                     aOffsetToSync )
{
    UInt sMultiplexIdx;

    IDE_TEST_CONT( mMultiplexCnt == 0, skip_sync );

    IDE_ASSERT( aSyncThread != NULL );

    IDE_ASSERT( aSyncThread[0].mThreadType == SMR_LOG_MPLEX_THREAD_TYPE_SYNC );

    /* log file sync�� ���� ���� ���� */
    mSyncLstPageInLstLF = aSyncLstPageInLstLF;
    mWhoSync            = aWhoSync;
    mFileNoToSync       = aFileNoToSync;
    mOffsetToSync       = aOffsetToSync;

    /* ����ȭ�� �� ��ŭ sync thread�� wakeup ��Ų��. */
    for ( sMultiplexIdx = 0; sMultiplexIdx < mMultiplexCnt; sMultiplexIdx++ )
    {
        IDE_TEST( aSyncThread[sMultiplexIdx].wakeUp() != IDE_SUCCESS );
    }

    IDE_EXCEPTION_CONT( skip_sync ); 

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description :
 * smrLogFileMgr::addEmptyLogFile���� ȣ��ȴ�.
 * ����ȭ �α������� �̸� �����Ѵ�.
 *
 * aCreateThread       - [IN] log file create thread ��
 * aTargetFileNo       - [IN] log file ������ ������ file ��ȣ
 * aLstFileNo          - [IN] �������� ������ file ��ȣ
 * aLogFileInitBuffer  - [IN] log file�� ������ �ʱ�ȭ�� buffer
 * aLogFileSize        - [IN] ������ log file ũ��
 ***********************************************************************/
IDE_RC smrLogMultiplexThread::wakeUpCreateThread( 
                                    smrLogMultiplexThread * aCreateThread,
                                    UInt                  * aTargetFileNo,
                                    UInt                    aLstFileNo,
                                    SChar                 * aLogFileInitBuffer,
                                    SInt                    aLogFileSize )
{
    UInt sMultiplexIdx;

    IDE_TEST_CONT( mMultiplexCnt == 0, skip_create );

    IDE_ASSERT( aCreateThread      != NULL );
    IDE_ASSERT( aLogFileInitBuffer != NULL );

    IDE_ASSERT( aCreateThread[0].mThreadType == SMR_LOG_MPLEX_THREAD_TYPE_CREATE );
    
    /* log file create�� ���� ���� ���� */
    mLogFileInitBuffer = aLogFileInitBuffer;
    mTargetFileNo      = aTargetFileNo;      
    mLstFileNo         = aLstFileNo;      
    mLogFileSize       = aLogFileSize; 
    
    /* ����ȭ�� �� ��ŭ log file create thread�� wakeup ��Ų��. */
    for ( sMultiplexIdx = 0; sMultiplexIdx < mMultiplexCnt; sMultiplexIdx++ )
    {
        IDE_TEST( aCreateThread[sMultiplexIdx].wakeUp() != IDE_SUCCESS );
    }

    IDE_EXCEPTION_CONT( skip_create ); 

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description :
 * smrLogFileMgr::removeLogFile���� ȣ��ȴ�.
 * ����ȭ �α����� �����Ѵ�.
 *
 * aDeleteThread        - [IN] log file delete thread ��
 * aDeleteFstFileNo     - [IN] ������ log������ ù��° file ��ȣ
 * aDeleteLstFileNo     - [IN] ������ log������ ������ file ��ȣ
 * aIsCheckPoint        - [IN] checkpoint������ ȣ��Ǵ��� ����
 ***********************************************************************/
IDE_RC smrLogMultiplexThread::wakeUpDeleteThread( 
                                        smrLogMultiplexThread * aDeleteThread,
                                        UInt                    aDeleteFstFileNo,
                                        UInt                    aDeleteLstFileNo,
                                        idBool                  aIsCheckPoint )
{
    UInt sMultiplexIdx;

    IDE_TEST_CONT( mMultiplexCnt == 0, skip_delete );

    IDE_ASSERT( aDeleteThread != NULL );

    IDE_ASSERT( aDeleteThread[0].mThreadType == SMR_LOG_MPLEX_THREAD_TYPE_DELETE );
    
    /* log file delete�� ���� ���� ���� */
    mDeleteFstFileNo = aDeleteFstFileNo;
    mDeleteLstFileNo = aDeleteLstFileNo;
    mIsCheckPoint    = aIsCheckPoint;     

    /* ����ȭ�� �� ��ŭ log file delete thread�� wakeup ��Ų��. */
    for ( sMultiplexIdx = 0; sMultiplexIdx < mMultiplexCnt; sMultiplexIdx++ )
    {
        IDE_TEST( aDeleteThread[sMultiplexIdx].wakeUp() != IDE_SUCCESS );
    }

    IDE_EXCEPTION_CONT( skip_delete ); 

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description :
 * append, sync, create, delete thread ��� run�Լ��� �̿��Ѵ�. 
 * thread type�� �´� �Լ��� �����Ѵ�.
 * �׻� condwait�����̸� �� thread�� wakeup�Լ����ؼ��� �����.(append
 * Thread�� ���� )
 * wakeup�Ǿ����� thread�� ���´� SMR_LOG_MPLEX_THREAD_STATE_WAKEUP�̿��� �ϰ�
 * sleep(condwait)�϶��� thread�� ���°� SMR_LOG_MPLEX_THREAD_STATE_WAIT�̿��� �Ѵ�.
 ***********************************************************************/
void smrLogMultiplexThread::run()
{     
    PDL_Time_Value  sTV;
    UInt            sSkipCnt = 0;
    UInt            sSpinCount;
    SInt            sRC;
    UInt            sState = 0;

    sSpinCount = smuProperty::getLogMultiplexThreadSpinCount();
    sTV.set( 0, 1 );

    IDE_TEST( lock() != IDE_SUCCESS ); 
    sState = 1;

    while( mFinish != ID_TRUE )
    {
        IDE_ASSERT( mThreadState == SMR_LOG_MPLEX_THREAD_STATE_WAIT );
    
        if ( mThreadType != SMR_LOG_MPLEX_THREAD_TYPE_APPEND )
        { 
            IDE_TEST_RAISE( mCv.wait(&mMutex) != IDE_SUCCESS, error_cond_wait );

            /* BUG-44779 AIX���� Spurious wakeups ��������
             * ������ ���� Thread�� Wakeup�Ǵ� ��찡 �ֽ��ϴ�.*/
            while ( mThreadState != SMR_LOG_MPLEX_THREAD_STATE_WAKEUP )
            {
                ideLog::log( IDE_SM_0,
                             "MultiplexThread spurious wakeups (Thread State : %"ID_UINT32_FMT")\n",
                             mThreadState );

                IDE_TEST_RAISE( mCv.wait(&mMutex) != IDE_SUCCESS, error_cond_wait );
            }
        }

        if ( mFinish == ID_TRUE )
        {
            break;
        }

        switch( mThreadType )
        {
            case SMR_LOG_MPLEX_THREAD_TYPE_CREATE:
            {
                sRC = createLogFile();
                break;
            }
            case SMR_LOG_MPLEX_THREAD_TYPE_DELETE:
            {
                sRC = deleteLogFile();
                break;
            }
            case SMR_LOG_MPLEX_THREAD_TYPE_SYNC:
            {
                sRC = syncLog();
                break;
            }
            case SMR_LOG_MPLEX_THREAD_TYPE_APPEND:
            {
                sRC = appendLog( &sSkipCnt );

                if ( (sSkipCnt > sSpinCount) && (sSpinCount != ID_UINT_MAX) )
                {
                    idlOS::sleep( sTV );
                    sSkipCnt = 0;
                }
                else
                {
                    /* sleep���� ���� */
                }

                break;
            }
            default:
            {
                IDE_ASSERT(0);
            }
        }

        if ( ( sRC != IDE_SUCCESS ) && ( errno !=0  ) && ( errno != ENOSPC ) )
        {
            mThreadState = SMR_LOG_MPLEX_THREAD_STATE_ERROR;
            IDE_RAISE(error_log_multiplex);  
        }
        else
        {
            mThreadState = SMR_LOG_MPLEX_THREAD_STATE_WAIT;
        }
    }

    sState = 0;
    IDE_TEST( unlock() != IDE_SUCCESS ); 

    return;

    IDE_EXCEPTION(error_cond_wait);
    {
        IDE_SET(ideSetErrorCode(smERR_FATAL_ThrCondWait));
    }
    IDE_EXCEPTION(error_log_multiplex);
    IDE_EXCEPTION_END;

    if ( sState != 0 )
    {
        IDE_ASSERT( unlock() == IDE_SUCCESS ); 
    }

    ideLog::log( IDE_SERVER_0, 
                 "Multiplex Thread Type: %u\n"
                 "Multiplex index      : %u\n"
                 "Multiplex LogFileNo  : %u\n"
                 "MultiplexPath        : %s\n",
                 mThreadType,
                 mMultiplexIdx,
                 mMultiplexLogFile->mFileNo,
                 mMultiplexPath ); 

    IDE_ASSERT(0);

    return;
}

/***********************************************************************
 * Description :
 * condwait�ϰ��ִ� thread�� �����.
 * wakeup�Ǳ����� thread�� ���¸� SMR_LOG_MPLEX_THREAD_STATE_WAKEUP���·�
 * �����Ѵ�.
 ***********************************************************************/
IDE_RC smrLogMultiplexThread::wakeUp()
{
    UInt sState = 0;

    IDE_TEST( lock() != IDE_SUCCESS ); 
    sState = 1;

    /* thread ���¸� wakeup���·� ���� */
    mThreadState = SMR_LOG_MPLEX_THREAD_STATE_WAKEUP;

    IDE_TEST_RAISE( mCv.signal() != IDE_SUCCESS, error_cond_signal );

    sState = 0;
    IDE_TEST( unlock() != IDE_SUCCESS ); 

    return IDE_SUCCESS;
 
    IDE_EXCEPTION(error_cond_signal);
    {
        IDE_SET(ideSetErrorCode(smERR_FATAL_ThrCondSignal));
    }
    IDE_EXCEPTION_END;

    switch( sState )
    {
        case 1:
            IDE_ASSERT( unlock() == IDE_SUCCESS ); 
        case 0:
        default:
            break;
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description :
 * thread ������ �Ϸ�ɶ����� ����Ѵ�.
 * aThread      - [IN] ������ �Ϸ�Ǳ⸦ ��ٸ� thread��
 ***********************************************************************/
IDE_RC smrLogMultiplexThread::wait( smrLogMultiplexThread * aThread )
{
    UInt            sMultiplexIdx;
    PDL_Time_Value  sTV;
    idBool          sIsDone  = ID_FALSE;
    idBool          sIsError = ID_FALSE;
     
    IDE_TEST_CONT( mMultiplexCnt == 0, skip_wait );

    IDE_ASSERT( aThread != NULL );
    IDE_ASSERT( aThread[0].mThreadType != SMR_LOG_MPLEX_THREAD_TYPE_APPEND );

    sTV.set( 0, 10 );

    while( (sIsDone == ID_FALSE) && (mFinish != ID_TRUE) )
    {
        sIsDone = ID_TRUE;
        for ( sMultiplexIdx = 0; sMultiplexIdx < mMultiplexCnt; sMultiplexIdx++ )
        {
            /* ���ܰ� �߻��� thread���ִ��� Ȯ�� */
            if ( aThread[sMultiplexIdx].mThreadState == 
                                        SMR_LOG_MPLEX_THREAD_STATE_ERROR )
            {
                sIsError = ID_TRUE;
            }

            /* SMR_LOG_MPLEX_THREAD_STATE_WAKEUP������ thread�� 
             * �����ϸ� ��� ���
             */
            if ( aThread[sMultiplexIdx].mThreadState == 
                                        SMR_LOG_MPLEX_THREAD_STATE_WAKEUP )
            {
                sIsDone = ID_FALSE;
                break;
            }
        }

        idlOS::sleep( sTV );
    }

    IDE_TEST_RAISE( sIsError == ID_TRUE, error_log_multiplex );

    IDE_EXCEPTION_CONT( skip_wait ); 

    return IDE_SUCCESS;

    IDE_EXCEPTION(error_log_multiplex);
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description :
 * ����ȭ�� log file buffer�� ���� log file buffer�� �α׸� �����Ѵ�.
 *
 * aSkipCnt - [OUT] �Լ��� ȣ��Ǿ����� �αװ� ������� ���� Ƚ��
 ***********************************************************************/
IDE_RC smrLogMultiplexThread::appendLog( UInt * aSkipCnt )
{
    smrLogFileOpenList  * sOpenLogFileList;
    smrLogFile          * sPrevOriginalLogFile;
    volatile UInt         sOriginalCurFileNo;

    SInt        sCopySize;
    UInt        sLogFileSize;
    UInt        sBarrier        = 0;
    SChar     * sCopyData;
    idBool      sIsSwitchOriginalLogFile;
    idBool      sIsNeedRestart  = ID_FALSE; /* BUG-38801 */
    smLSN       sLstLSN;                    /* BUG-38801 */
    smLSN       sSyncLSN;                   /* BUG-35392 */

    IDE_ASSERT( mThreadType == SMR_LOG_MPLEX_THREAD_TYPE_APPEND );
    SM_LSN_MAX ( sLstLSN );
    SM_LSN_MAX ( sSyncLSN );

    sOpenLogFileList = getLogFileOpenList();

    IDE_ASSERT( sOpenLogFileList->mLogFileOpenListLen != 0 );
    IDE_ASSERT( (mMultiplexLogFile  != NULL) && 
                (mOriginalLogFile   != NULL) );
    IDE_ASSERT( mOriginalLogFile->mFileNo == mMultiplexLogFile->mFileNo );

    sLogFileSize        = smuProperty::getLogFileSize();
    sOriginalCurFileNo  = mOriginalCurFileNo;

    while( mMultiplexLogFile->mFileNo <= sOriginalCurFileNo )
    {
        IDE_ASSERT( mMultiplexLogFile->mFileNo == mOriginalLogFile->mFileNo );

        /* code reodering�� ���� */
        IDE_ASSERT( sBarrier == 0 );
        if ( sBarrier++ == 0 )
        {
            sIsSwitchOriginalLogFile = mOriginalLogFile->mSwitch;

            /* BUG-35392 
             * ���� log file�� switch�Ǵ��� dummy log�� ������ �� �ִ�.
             * ����ȭ log file�� dummy log�� ������ log�� ����� ������ ����.
             * ���� switch�� ���� log file�� dummy log�� �������� ������
             * �����ؾ��Ѵ�. */
            if ( sIsSwitchOriginalLogFile == ID_TRUE )
            {
                SM_SET_LSN( sSyncLSN,
                            mOriginalLogFile->mFileNo,
                            mOriginalLogFile->mOffset );

                // Sync ������ �ִ��� LSN ���� ���
                smrLogMgr::waitLogSyncToLSN( &sSyncLSN,
                                             smuProperty::getLFThrSyncWaitMin(),
                                             smuProperty::getLFThrSyncWaitMax() );
            }
        }

        /* BUG-35392
         * logfile�� ����� Log�� ������ Offset�� �޾ƿ´�. 
         * FAST_UNLOCK_LOG_ALLOC_MUTEXT = 1�� ��� ���̸� �������� �ʴ� �α� ���ڵ��� Offset */
        smrLogMgr::getLstLogOffset( &sLstLSN );

        if ( sBarrier++ == 1 )
        {
            /* ����ȭ �α׹��۷� ������ ũ�� ��� */
            if ( sLstLSN.mFileNo == mMultiplexLogFile->mFileNo )
            {
                /* LstLSN �� ���� ������ mMultiplex�� ���� �α����� �̶��
                 * LstLSN ������ �����ص� �ȴ�. */
                sCopySize = sLstLSN.mOffset - mMultiplexLogFile->mOffset;
            }
            else
            {
                /* LstLSN�� mMultiplex �� ���� �ʴٸ�, ( LstLSN > mMuiltiplex )
                 * multiplex �� �α������� 1�� �̻� �׿��ٴ� �ǹ�.
                 * ����, LstLSN�� �ƴ�, mOriginal ���� ������ ������ �޾ƿ´�. */
                sCopySize = mOriginalLogFile->mOffset - mMultiplexLogFile->mOffset;

                IDE_ASSERT( mOriginalLogFile->mOffset >= mMultiplexLogFile->mOffset );
            }
        }

        /* ����ȭ �α� ���۷� ������ �α� ������ ��ġ ��� */
        sCopyData = ((SChar*)mOriginalLogFile->mBase)  + mMultiplexLogFile->mOffset;

        /* BUG-45711 UncompletedLstLSN�� ���ÿ� �����ϸ鼭 
         * sLstLSN�� mMultiplexLogFile.LSN���� �۰� ���ü� �ִ�. 
           �̹� �ش� �α� ���ı��� multiplexing �ȰŴϱ� �Ѿ�� ��. */
        if ( sCopySize > 0 )
        {
            IDE_ASSERT( (mMultiplexLogFile->mOffset + sCopySize) <= sLogFileSize );
            mMultiplexLogFile->append( sCopyData, sCopySize );
            (*aSkipCnt) = 0;
        }
        else
        {
            if ( sIsSwitchOriginalLogFile == ID_TRUE )
            {
                checkOriginalLogSwitch( &sIsNeedRestart );

                if ( sIsNeedRestart == ID_TRUE )
                {
                    sOriginalCurFileNo = mOriginalCurFileNo;
                    sBarrier = 0;
                    continue;
                }

                /* ���� �α������� log file open list�� �������� ������
                 * ����ȭ create Thread�� ���� �α������� ����� list��
                 * �Ŵ޶����� ����Ѵ�. */
                if ( (mMultiplexLogFile->mFileNo + 1) 
                    != mMultiplexLogFile->mNxtLogFile->mFileNo )
                {
                    wait4NextLogFile( mMultiplexLogFile );
                }

                /* ����ȭ �α������� switch�ϰ� ���� �α������� �����´�. */
                mMultiplexLogFile->mSwitch = ID_TRUE;
                mMultiplexLogFile          = mMultiplexLogFile->mNxtLogFile;

                sPrevOriginalLogFile = mOriginalLogFile;
                mOriginalLogFile     = mOriginalLogFile->mNxtLogFile;

                /* 
                 * ����ȭ �α׹��ۿ� ��� �αװ� �ݿ��� �Ǿ�����
                 * �˸���.(SMR_LT_FILE_END����)
                 * �αװ� ����ȭ ���� �ʾҴٸ� ����ȭ�� �Ϸ�ɶ����� 
                 * smrLFThread���� �α� ������ close���� �ʴ´�.
                 */
                sPrevOriginalLogFile->mMultiplexSwitch[mMultiplexIdx] = ID_TRUE;
            }
            else
            {
                if ( mMultiplexLogFile->mFileNo == sOriginalCurFileNo )
                {
                    /* ��� ���� �α׸� ����ȭ �� */
                    (*aSkipCnt)++;
                    break;
                }
                else
                {
                    IDE_ASSERT( mMultiplexLogFile->mFileNo <
                                sOriginalCurFileNo );
                    /* 
                     * ����ȭ �ؾ��� �αװ� �������� 
                     * continue 
                     */
                }
            }
        }

        sOriginalCurFileNo = mOriginalCurFileNo;
        sBarrier = 0;
    }

    return IDE_SUCCESS;
}

/***********************************************************************
 * Description :
 * ����ȭ�� log file buffer�ǳ����� log file�� sync�Ѵ�.
 ***********************************************************************/
IDE_RC smrLogMultiplexThread::syncLog()
{
    smrLogFile         * sNxtLogFile;
    smrLogFile         * sCurLogFile;
    smrLogFile         * sOpenLogFileListHdr;
    UInt                 sSyncOffset;
    UInt                 sCurFileNo;
    idBool               sSyncLstPage;
    idBool               sIsSwitchLF;

    IDE_ASSERT( mThreadType == SMR_LOG_MPLEX_THREAD_TYPE_SYNC );

    sOpenLogFileListHdr = getOpenFileListHdr();

    sCurLogFile = sOpenLogFileListHdr->mNxtLogFile;

    /* 
     * BUG-36025 Server can be abnormally shutdown when multiplex log sync thread
     * works slowly than LFThread 
     * ����ȭ �α� sync thread�� ����� �����ϰ� �Ǹ� ���� �α����Ϻ��� ������
     * �α������� sync�� �� �ִ�. ���� assert code�� �����ϰ� if������ ��ü�Ͽ�
     * altibase_sm.log�� �޼����� ����ְ� �ش� ����ȣ �α����� sync�� skip�Ѵ�.
     * �޼����� page�� ���ۿ��� �Ѱܳ� �αװ� sync�ɶ����� ��ϵȴ�.(WAL)
     * LFThread�� ������ �����Ͽ� �α׸� sync�Ҷ����� mFileNoToSync�� UINT MAX��
     * �Ǳ⶧���� �ش� �޼����� ������ �ʴ´�.
     */
    if ( sCurLogFile->mFileNo > mFileNoToSync )
    {
        ideLog::log( SM_TRC_LOG_LEVEL_WARNNING,
                     "Mutiplex %s%clogfile%u is already synced",
                     mMultiplexPath, 
                     IDL_FILE_SEPARATOR, 
                     mFileNoToSync );

        IDE_CONT( skip_sync );
    }
    else
    {
        /* nothing to do */
    }

    /* FIT/ART/sm/Bugs/BUG-36025/BUG-36025.sql */
    IDU_FIT_POINT( "1.BUG-36025@smrLogMultiplexThread::syncLog" );

    while( sCurLogFile != sOpenLogFileListHdr )
    {
        sNxtLogFile = sCurLogFile->mNxtLogFile;
        sCurFileNo  = sCurLogFile->mFileNo;

        if ( sCurLogFile->getEndLogFlush() == ID_FALSE )
        {
            sIsSwitchLF = sCurLogFile->mSwitch; 

            if ( ( sCurFileNo == mFileNoToSync ) &&
                 ( sIsSwitchLF == ID_FALSE ) )
            {
                /* BUG-37018 There is some mistake on logfile Offset calculation 
                 * log file�� sync �Ҷ� ����ȭ log file buffer�� ��ϵ� log��
                 * offset�� ��û�� mOffsetToSync���� Ŭ������ ����Ѵ�. */
                while( sCurLogFile->mOffset < mOffsetToSync )
                {
                    idlOS::thr_yield();
                }

                sSyncOffset  = mOffsetToSync;
                sSyncLstPage = mSyncLstPageInLstLF;
            }
            else
            {
                sSyncOffset  = sCurLogFile->mOffset;
                sSyncLstPage = ID_TRUE;
            }

            /* log file�� sync�Ѵ�. */
            IDE_TEST( sCurLogFile->syncLog( sSyncLstPage, sSyncOffset ) 
                      != IDE_SUCCESS );

            if ( sIsSwitchLF == ID_TRUE )
            {
                /* �� �̻� �α����� �� ��ũ�� ����� �αװ� ����
                 * ��� ��ũ�� �ݿ��� �Ǿ���. */
                sCurLogFile->setEndLogFlush( ID_TRUE );
            }
            else
            {
                break;
            }
        }

        /* 
         * sync�� log file�� switch�Ǿ��ִٸ� 
         * log file�� list�� ������ �ʿ䰡 ����
         */
        if ( ( sCurLogFile->getEndLogFlush() == ID_TRUE ) && 
             ( mWhoSync == SMR_LOG_SYNC_BY_LFT ) )
        {
            IDE_TEST( closeLogFile( sCurLogFile, 
                                    ID_TRUE ) /*aRemoveFromList*/
                      != IDE_SUCCESS );
        }

        if ( sCurFileNo == mFileNoToSync )
        {
            break;
        }

        sCurLogFile = sNxtLogFile;
    }

    IDE_EXCEPTION_CONT( skip_sync );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description :
 * ����ȭ log file�� �����ϰ� log file open list�� �߰��Ѵ�.
 ***********************************************************************/
IDE_RC smrLogMultiplexThread::createLogFile()
{
    smrLogFile     * sNewLogFile;
    SChar            sLogFileName[SM_MAX_FILE_NAME] = {'\0',};
    PDL_Time_Value   sTV;
    UInt             sLstFileNo;
    UInt             sState = 0;

    IDE_ASSERT( mThreadType == SMR_LOG_MPLEX_THREAD_TYPE_CREATE );

    sLstFileNo = mLstFileNo;

    idlOS::snprintf(sLogFileName,
                    SM_MAX_FILE_NAME,
                    "%s%c%s%"ID_UINT32_FMT"",
                    mMultiplexPath,
                    IDL_FILE_SEPARATOR,
                    SMR_LOG_FILE_NAME,
                    sLstFileNo);

    /* �����α������� ������ �α����� ��ȣ�� ���� �α������� �����ϴ��� Ȯ�� */
    IDE_ASSERT( idf::access(sLogFileName, F_OK) == 0 );

    sTV.set( 0, 1 ); 

    while(1)
    {
        while( (*mTargetFileNo) + smuProperty::getLogFilePrepareCount() >
               sLstFileNo + 1 )
        {
            sLstFileNo++;
 
            if ( sLstFileNo == ID_UINT_MAX )
            {
                sLstFileNo = 0;
            }
 
            idlOS::snprintf(sLogFileName,
                            SM_MAX_FILE_NAME,
                            "%s%c%s%"ID_UINT32_FMT"",
                            mMultiplexPath,
                            IDL_FILE_SEPARATOR,
                            SMR_LOG_FILE_NAME,
                            sLstFileNo);
  
            /* smrLogMultiplexThread_createLogFile_calloc_NewLogFile.tc */
            IDU_FIT_POINT("smrLogMultiplexThread::createLogFile::calloc::NewLogFile");
            IDE_TEST( iduMemMgr::calloc( IDU_MEM_SM_SMR,
                                         1,
                                         ID_SIZEOF( smrLogFile ),
                                         (void**)&sNewLogFile)
                              != IDE_SUCCESS );
            sState = 1;
  
            IDE_TEST( sNewLogFile->initialize() != IDE_SUCCESS ); 
            sState = 2;
  
            IDE_TEST( sNewLogFile->create( sLogFileName,
                                           mLogFileInitBuffer,
                                           mLogFileSize )
                      != IDE_SUCCESS );
            sState = 3;
  
            IDE_TEST( sNewLogFile->open( sLstFileNo,
                                         sLogFileName,
                                         ID_TRUE, /* aIsMultiplexLogFile */
                                         smuProperty::getLogFileSize(),
                                         ID_TRUE) 
                      != IDE_SUCCESS );
            sState = 4;
  
            sNewLogFile->mFileNo   = sLstFileNo; 
            sNewLogFile->mOffset   = 0;
            sNewLogFile->mFreeSize = smuProperty::getLogFileSize();
            sNewLogFile->mRef++;
            (void)sNewLogFile->touchMMapArea();
  
            IDE_TEST( add2LogFileOpenList( sNewLogFile, ID_TRUE ) 
                      != IDE_SUCCESS );
        }
 
        /* 
         * ���� �α������� mTargetFileNo�� �α����� ���� ���� �ٲ�� ��������
         * ������� ������ ���� �α������� ��ȣ�� Ȯ���Ѵ�.
         */
        if ( ( mIsOriginalLogFileCreateDone != ID_TRUE ) || 
             ( mOriginalLstLogFileNo > sLstFileNo ) )
        {
            idlOS::sleep( sTV );
        }
        else
        {
            break;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch( sState )
    {
        case 4:
            IDE_ASSERT( sNewLogFile->close() == IDE_SUCCESS );
        case 3:
            IDE_ASSERT( sNewLogFile->remove( sLogFileName, ID_FALSE ) 
                        == IDE_SUCCESS );
        case 2:
            IDE_ASSERT( sNewLogFile->destroy() == IDE_SUCCESS );
        case 1:
            IDE_ASSERT( iduMemMgr::free( sNewLogFile ) == IDE_SUCCESS );
        case 0:
        default:
            break;
    }

    return IDE_FAILURE;

}

/***********************************************************************
 * Description :
 * ����ȭ log file�� �����Ѵ�.
 ***********************************************************************/
IDE_RC smrLogMultiplexThread::deleteLogFile()
{
    smrLogFileMgr::removeLogFiles( mDeleteFstFileNo, 
                                   mDeleteLstFileNo, 
                                   mIsCheckPoint,
                                   ID_TRUE,
                                   mMultiplexPath );

    return IDE_SUCCESS;
}

/***********************************************************************
 * Description :
 * log file�� open�ϰ� log file open list�� �߰��Ѵ�.
 * server startup�ÿ��� ȣ��ȴ�.
 *
 * aLogFileNo - [IN]  open�� log file ��ȣ
 * aAddToList - [IN]  open�� log file�� list�� �߰��Ұ����� ����
 * aLogFile   - [OUT] open�� log file
 * aISExist   - [OUT] open�� log file�� �����ϴ��� ��ȯ
 ***********************************************************************/
IDE_RC smrLogMultiplexThread::openLogFile( UInt          aLogFileNo, 
                                           idBool        aAddToList,
                                           smrLogFile ** aLogFile,
                                           idBool      * aIsExist )
{
    smrLogFileOpenList * sLogFileOpenList;
    smrLogFile         * sNewLogFile;
    SChar                sLogFileName[SM_MAX_FILE_NAME] = {'\0',};
    UInt                 sState = 0;

    IDE_ASSERT( (mThreadType == SMR_LOG_MPLEX_THREAD_TYPE_CREATE) ||
                (mThreadType == SMR_LOG_MPLEX_THREAD_TYPE_SYNC) );

    IDE_ASSERT( aLogFile != NULL );

    idlOS::snprintf(sLogFileName,
                    SM_MAX_FILE_NAME,
                    "%s%c%s%"ID_UINT32_FMT,
                    mMultiplexPath,
                    IDL_FILE_SEPARATOR,
                    SMR_LOG_FILE_NAME,
                    aLogFileNo);

    sLogFileOpenList = getLogFileOpenList();

    IDE_TEST( sLogFileOpenList->mLogFileOpenListMutex.lock( NULL ) 
              != IDE_SUCCESS );
    sState = 1;
    
    if ( idf::access(sLogFileName, F_OK) == 0 )
    {
        sNewLogFile = findLogFileInList( aLogFileNo, sLogFileOpenList );

        if ( sNewLogFile == NULL )
        {
            /* smrLogMultiplexThread_openLogFile_calloc_NewLogFile.tc */
            IDU_FIT_POINT("smrLogMultiplexThread::openLogFile::calloc::NewLogFile");
            IDE_TEST( iduMemMgr::calloc( IDU_MEM_SM_SMR,
                                         1,
                                         ID_SIZEOF( smrLogFile ),
                                         (void**)&sNewLogFile)
                      != IDE_SUCCESS );
            sState = 2;
  
            IDE_TEST( sNewLogFile->initialize() != IDE_SUCCESS ); 
            sState = 3;
  
            IDE_TEST( sNewLogFile->open( aLogFileNo,
                                         sLogFileName,
                                         ID_TRUE, /* aIsMultiplexLogFile */
                                         smuProperty::getLogFileSize(),
                                         ID_TRUE ) 
                      != IDE_SUCCESS );
            sState = 4;
  
            sNewLogFile->mFileNo   = aLogFileNo; 
            sNewLogFile->mOffset   = 0;
            sNewLogFile->mFreeSize = smuProperty::getLogFileSize();
            sNewLogFile->mRef++;
            (void)sNewLogFile->touchMMapArea();
 
            if ( aAddToList == ID_TRUE )
            {
                IDE_TEST( add2LogFileOpenList( sNewLogFile, ID_FALSE ) 
                          != IDE_SUCCESS );
            }
            else
            {
                /* nothing to do */
            }
        }

        *aIsExist = ID_TRUE;
        *aLogFile = sNewLogFile;
    }
    else
    {
        *aLogFile = NULL;
        *aIsExist = ID_FALSE;
    }

    sState = 0;
    IDE_TEST( sLogFileOpenList->mLogFileOpenListMutex.unlock() 
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch( sState )
    {
        case 4:
            IDE_ASSERT( sNewLogFile->close() == IDE_SUCCESS );
        case 3:
            IDE_ASSERT( sNewLogFile->destroy() == IDE_SUCCESS );
        case 2:
            IDE_ASSERT( iduMemMgr::free( sNewLogFile ) == IDE_SUCCESS );
        case 1:
            IDE_ASSERT( sLogFileOpenList->mLogFileOpenListMutex.unlock() 
                        == IDE_SUCCESS );
        case 0:
        default:
            break;
    }
    
    return IDE_FAILURE;
}

/***********************************************************************
 * Description :
 * BUG-35051 ���� �������� �ٷ� �α� ����ȭ ������Ƽ�� �����ϸ� ������
 * �����մϴ�. 
 *
 * ���� ���� ���� ���� �ٽ� ���۽� smrLogFileMgr::preOpenLogFile()�� ����
 * ȣ���. 
 * ���� ���۽� prepare�� ����ȭ �α������� open�Ѵ�. prepare�� �α������� 
 * �������� �ʴ´ٸ� �������ְ� open�Ѵ�.
 * �� �Լ��� �α����Ͼȿ� �αװ� ��ϵ������� Prepare�� �α������� 
 * open������ ������ �����ϴ��� ������ ����.
 *
 * aLogFileNo           - [IN] open�� log file ��ȣ
 * aLogFileInitBuffer   - [IN] �α����� ������ ���� �ʱ�ȭ ���� 
 * aLogFileSize         - [IN] �α����� ũ��
 ***********************************************************************/
IDE_RC smrLogMultiplexThread::preOpenLogFile( UInt    aLogFileNo,
                                              SChar * aLogFileInitBuffer,
                                              UInt    aLogFileSize )
{
    smrLogFile * sMultiplexLogFile;
    smrLogFile   sNewLogFile;
    idBool       sLogFileExist                  = ID_FALSE;
    SChar        sLogFileName[SM_MAX_FILE_NAME] = {'\0',};
    UInt         sState = 0;
    idBool       sLogFileState = ID_FALSE;

    IDE_ASSERT( aLogFileInitBuffer != NULL );
    IDE_ASSERT( mThreadType == SMR_LOG_MPLEX_THREAD_TYPE_CREATE );

    idlOS::snprintf(sLogFileName,
                    SM_MAX_FILE_NAME,
                    "%s%c%s%"ID_UINT32_FMT,
                    mMultiplexPath,
                    IDL_FILE_SEPARATOR,
                    SMR_LOG_FILE_NAME,
                    aLogFileNo);

    /* prepare�� aLogFileNo�� ���� �α������� ����ȭ ���丮�� 
     * �����ϴ��� Ȯ���Ѵ�. ���ٸ� �ش� �α������� ����� �ش� */ 
    if ( idf::access(sLogFileName, F_OK) != 0 )
    {
        IDE_TEST( sNewLogFile.initialize() != IDE_SUCCESS ); 
        sState = 1;

        IDE_TEST( sNewLogFile.create( sLogFileName,
                                      aLogFileInitBuffer,
                                      aLogFileSize )
                  != IDE_SUCCESS );
        sLogFileState = ID_TRUE;

        sState = 0;
        IDE_TEST( sNewLogFile.destroy() != IDE_SUCCESS ); 
    }
    else
    {
        /* nothing to do */
    }

    /* ����ȭ �α������� open�Ѵ� */
    IDE_TEST( openLogFile( aLogFileNo,
                           ID_TRUE, /*addToList*/
                           &sMultiplexLogFile,
                           &sLogFileExist )
              != IDE_SUCCESS );

    IDE_ASSERT( (sLogFileExist == ID_TRUE) && (sMultiplexLogFile != NULL) );

    IDE_ASSERT( sMultiplexLogFile->mFileNo == aLogFileNo );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch( sState )
    {
        case 1:
            IDE_ASSERT( sNewLogFile.destroy() == IDE_SUCCESS ); 
        case 0:
        default:
            break;
    }

    if ( sLogFileState == ID_TRUE )
    {
        IDE_ASSERT( smrLogFile::remove( sLogFileName,
                                        ID_FALSE ) /*aIsCheckpoint*/
                    == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description :
 * log file�� close�ϰ� log file open list���� �����Ѵ�.
 *
 * aLogFile        - [IN] close�� log file
 * aRemoceFromList - [IN] close�� log file�� list���� ���� ���� ����
 ***********************************************************************/
IDE_RC smrLogMultiplexThread::closeLogFile( smrLogFile * aLogFile, 
                                            idBool aRemoveFromList )
{
    UInt sState = 3;

    IDE_ASSERT( aLogFile != NULL );
    
    if ( aRemoveFromList == ID_TRUE )
    {
        IDE_TEST( removeFromLogFileOpenList( aLogFile ) != IDE_SUCCESS );
    }

    IDE_ASSERT( aLogFile->mNxtLogFile == NULL );
    IDE_ASSERT( aLogFile->mPrvLogFile == NULL );

    aLogFile->mRef--;

    IDE_ASSERT( aLogFile->mRef  == 0 );

    sState = 2;
    IDE_TEST( aLogFile->close() != IDE_SUCCESS );

    sState = 1;
    IDE_TEST( aLogFile->destroy() != IDE_SUCCESS );

    sState = 0;
    IDE_TEST( iduMemMgr::free( aLogFile ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch( sState )
    {
        case 3:
            IDE_ASSERT( aLogFile->close() == IDE_SUCCESS );
        case 2:
            IDE_ASSERT( aLogFile->destroy() == IDE_SUCCESS );
        case 1:
            IDE_ASSERT( iduMemMgr::free( aLogFile ) == IDE_SUCCESS );
        case 0:
        default:
            break;
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description :
 * ����������(smrLogFileMgr::startAndWait) �������� ����� log file�� open�Ѵ�.
 * smrLogMgr::openLstLogFile �Լ� ����
 *
 *
 * aLogFileNo       - [IN]  �������� ����� log file ��ȣ
 * aOffset          - [IN] ���� log�� ����� offset
 * aOriginalLogFile - [IN] ���� �α�����
 * aLogFile         - [OUT] open�� last log file
 ***********************************************************************/
IDE_RC smrLogMultiplexThread::openLstLogFile( UInt          aLogFileNo,
                                              UInt          aOffset,
                                              smrLogFile  * aOriginalLogFile,
                                              smrLogFile ** aLogFile )
{
    smrLogFile * sLogFile;
    idBool       sLogFileExist;
    SChar        sLogFileName[SM_MAX_FILE_NAME] = {'\0',};
    UInt         sState = 0;

    IDE_ASSERT( aLogFile         != NULL );
    IDE_ASSERT( aOriginalLogFile != NULL );
    IDE_ASSERT( mThreadType      == SMR_LOG_MPLEX_THREAD_TYPE_CREATE );

    IDE_TEST( openLogFile( aLogFileNo, 
                           ID_TRUE, /*addToList*/
                           &sLogFile, 
                           &sLogFileExist ) 
              != IDE_SUCCESS );

    if ( sLogFileExist == ID_TRUE )
    {
        sState = 1;
        IDE_ASSERT( sLogFile != NULL );

        IDE_TEST( recoverLogFile( sLogFile ) != IDE_SUCCESS );

        sLogFile->setPos( aLogFileNo, aOffset );

        sLogFile->clear( aOffset );
    
        if ( smuProperty::getLogBufferType() == SMU_LOG_BUFFER_TYPE_MEMORY )
        {
            /* log buffer type�� memory�ϰ�� ������ ����ȭ �α������� ������ 
             * ���� �α������� ����� �ٸ��� �ִ�.(smrLFThread���ۿ��ο� ����)
             * ���� �����α������� ������ ����ȭ �α����Ͽ� copy�Ѵ�. */

            idlOS::memcpy( sLogFile->mBase, 
                           aOriginalLogFile->mBase, 
                           smuProperty::getLogFileSize() ); 
        }

        IDE_TEST( sLogFile->checkFileBeginLog( aLogFileNo ) 
                  != IDE_SUCCESS );

        (void)sLogFile->touchMMapArea();

    }
    else
    {
        /* 
         * ���� ���۽� ������ ����ȭ �α������� �������� �ʴ� ����̴�. 
         * ����ȭ ���丮�� �����ϰų� �߰��������� ��Ȳ ������
         * ���� �α������� ������ �α������� �����Ͽ� ����ȭ �Ѵ�. 
         */
        idlOS::snprintf(sLogFileName,
                        SM_MAX_FILE_NAME,
                        "%s%c%s%"ID_UINT32_FMT,
                        mMultiplexPath,
                        IDL_FILE_SEPARATOR,
                        SMR_LOG_FILE_NAME,
                        aLogFileNo);
        
        IDE_TEST( aOriginalLogFile->mFile.copy( NULL, sLogFileName ) 
                  != IDE_SUCCESS );

        IDE_TEST( openLogFile( aLogFileNo, 
                               ID_TRUE, /*addToList*/
                               &sLogFile, 
                               &sLogFileExist ) 
                  != IDE_SUCCESS );

        IDE_ASSERT( sLogFileExist == ID_TRUE );
    }

    *aLogFile = sLogFile;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sState != 0 )
    {
        IDE_ASSERT( closeLogFile( sLogFile, 
                                  ID_TRUE ) /*aRemoveFromList*/
                    == IDE_SUCCESS );
    }
    
    return IDE_FAILURE;
}

/***********************************************************************
 * Description :
 * logFileOpenList�� initialize�Ѵ�.
 * 
 * aLogFileOpenList - [IN] initialize�� log file open list
 * aLostIdx         - [IN] ����ȭ Idx��ȣ 
 ***********************************************************************/
IDE_RC smrLogMultiplexThread::initLogFileOpenList( 
                            smrLogFileOpenList * aLogFileOpenList,
                            UInt                 aListIdx )
{
    smrLogFile * sLogFileOpenListHdr;
    SChar        sMutexName[128] = {'\0',};
    UInt         sState = 0;

    IDE_ASSERT( aLogFileOpenList != NULL );
    IDE_ASSERT( aListIdx < mMultiplexCnt );

    sLogFileOpenListHdr = &aLogFileOpenList->mLogFileOpenListHdr;
    
    /* list �ʱ�ȭ */
    sLogFileOpenListHdr->mPrvLogFile = sLogFileOpenListHdr;
    sLogFileOpenListHdr->mNxtLogFile = sLogFileOpenListHdr;
    
    idlOS::snprintf( sMutexName, 
                     128, 
                     "LOG_FILE_OPEN_LIST_IDX%"ID_UINT32_FMT, 
                     aListIdx );

    IDE_TEST( aLogFileOpenList->mLogFileOpenListMutex.initialize(
                                                        sMutexName,
                                                        IDU_MUTEX_KIND_NATIVE,
                                                        IDV_WAIT_INDEX_NULL )
              != IDE_SUCCESS );
    sState = 1;

    aLogFileOpenList->mLogFileOpenListLen = 0;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch( sState )
    {
        case 1:
            IDE_ASSERT( aLogFileOpenList->mLogFileOpenListMutex.destroy()
                        == IDE_SUCCESS );
        case 0:
        default:
            break;
    }
    
    return IDE_FAILURE;
}

/***********************************************************************
 * Description :
 * logFileOpenList�� destroy�Ѵ�.
 * 
 * aLogFileOpenList - [IN] destroy�� log file open list
 * aSyncThread      - [IN] smrLogMultiplexThread ��ü 
 ***********************************************************************/
IDE_RC smrLogMultiplexThread::destroyLogFileOpenList( 
                            smrLogFileOpenList    * aLogFileOpenList,
                            smrLogMultiplexThread * aSyncThread )
{
    smrLogFile         * sCurLogFile;
    smrLogFile         * sNxtLogFile;
    smrLogFileOpenList * sLogFileOpenList;
    UInt                 sLogFileOpenListLen;
    UInt                 sLogFileOpenListCnt;
    UInt                 sState = 1;

    IDE_ASSERT( aLogFileOpenList           != NULL );
    IDE_ASSERT( aSyncThread                != NULL );
    IDE_ASSERT( aSyncThread[0].mThreadType == SMR_LOG_MPLEX_THREAD_TYPE_SYNC );

    sLogFileOpenList    = aSyncThread->getLogFileOpenList();
    sCurLogFile         = sLogFileOpenList->mLogFileOpenListHdr.mNxtLogFile;
    sLogFileOpenListLen = aLogFileOpenList->mLogFileOpenListLen;

    for ( sLogFileOpenListCnt = 0; 
         sLogFileOpenListCnt < sLogFileOpenListLen; 
         sLogFileOpenListCnt++ )
    {
        sNxtLogFile = sCurLogFile->mNxtLogFile;
        IDE_TEST( aSyncThread->closeLogFile( sCurLogFile, 
                                             ID_TRUE ) /*aRemoveFromList*/
                  != IDE_SUCCESS );
        sCurLogFile = sNxtLogFile;
    }

    IDE_ASSERT( sLogFileOpenList->mLogFileOpenListLen == 0 );
    IDE_ASSERT( (sLogFileOpenList->mLogFileOpenListHdr.mNxtLogFile == 
                &sLogFileOpenList->mLogFileOpenListHdr) && 
                (sLogFileOpenList->mLogFileOpenListHdr.mPrvLogFile ==
                &sLogFileOpenList->mLogFileOpenListHdr) );

    sState = 0;
    IDE_TEST( aLogFileOpenList->mLogFileOpenListMutex.destroy()
              != IDE_SUCCESS );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch( sState )
    {
        case 1:
            IDE_ASSERT( aLogFileOpenList->mLogFileOpenListMutex.destroy()
                        == IDE_SUCCESS );
        case 0:
        default:
            break;
    }
    
    return IDE_FAILURE;
}

/***********************************************************************
 * Description :
 * log file�� log file open list �������� �߰��Ѵ�.
 *
 * aNewLogFile - [IN] list�� �߰��� log file
 * aWithLock   - [IN] list�� log file�� �߰��Ҷ� lock �� ���������� ����
 ***********************************************************************/
IDE_RC smrLogMultiplexThread::add2LogFileOpenList( smrLogFile * aNewLogFile,
                                                   idBool       aWithLock )
{
    smrLogFile         * sPrvLogFile;
    smrLogFile         * sLogFileOpenListHdr;
    smrLogFileOpenList * sLogFileOpenList;
    UInt                 sState = 0;
#ifdef DEBUG 
    smrLogFile         * sNxtLogFile;
#endif

    IDE_ASSERT( aNewLogFile            != NULL );
    IDE_ASSERT( aNewLogFile->mIsOpened == ID_TRUE );
    IDE_ASSERT( mThreadType            == SMR_LOG_MPLEX_THREAD_TYPE_CREATE );

    sLogFileOpenList    = getLogFileOpenList();
    sLogFileOpenListHdr = &sLogFileOpenList->mLogFileOpenListHdr;

    if ( aWithLock == ID_TRUE )
    {
        IDE_TEST( sLogFileOpenList->mLogFileOpenListMutex.lock( NULL ) 
                  != IDE_SUCCESS );
        sState = 1;
    }

#ifdef DEBUG 
    sNxtLogFile = sLogFileOpenListHdr->mNxtLogFile;
    while( sLogFileOpenListHdr != sNxtLogFile )
    {
        IDE_ASSERT( sNxtLogFile->mFileNo != aNewLogFile->mFileNo );
        sNxtLogFile = sNxtLogFile->mNxtLogFile;
    } 
#endif

    sPrvLogFile = sLogFileOpenListHdr->mPrvLogFile;

    sPrvLogFile->mNxtLogFile = aNewLogFile;    
    sLogFileOpenListHdr->mPrvLogFile = aNewLogFile;

    aNewLogFile->mNxtLogFile = sLogFileOpenListHdr;
    aNewLogFile->mPrvLogFile = sPrvLogFile;

    sLogFileOpenList->mLogFileOpenListLen++;

#ifdef DEBUG 
    checkLogFileOpenList( sLogFileOpenList );
#endif

    if ( aWithLock == ID_TRUE )
    {
        sState = 0;
        IDE_TEST( sLogFileOpenList->mLogFileOpenListMutex.unlock() 
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sState != 0 )
    {
        IDE_ASSERT( sLogFileOpenList->mLogFileOpenListMutex.unlock() 
                    == IDE_SUCCESS );
    }
    
    return IDE_FAILURE;
}

/*
 * prepare logfile�� ���� 0�ϰ�� list ���ü� ���� ��� �ʿ�
 */
/***********************************************************************
 * Description :
 * log file�� log file open list�κ��� �����Ѵ�.
 *
 * aRemoveLogFile - [IN] list���� ������ log file
 ***********************************************************************/
IDE_RC smrLogMultiplexThread::removeFromLogFileOpenList( 
                                    smrLogFile * aRemoveLogFile )
{
    smrLogFile         * sPrvLogFile;
    smrLogFile         * sNxtLogFile;
    smrLogFileOpenList * sLogFileOpenList;
    UInt                 sState = 0;
    
    IDE_ASSERT( aRemoveLogFile != NULL );
    IDE_ASSERT( mThreadType    == SMR_LOG_MPLEX_THREAD_TYPE_SYNC );

    sLogFileOpenList = getLogFileOpenList();

    IDE_ASSERT( &sLogFileOpenList->mLogFileOpenListHdr != aRemoveLogFile );

    IDE_TEST( sLogFileOpenList->mLogFileOpenListMutex.lock( NULL ) 
              != IDE_SUCCESS );
    sState = 1;

    sPrvLogFile = aRemoveLogFile->mPrvLogFile;
    sNxtLogFile = aRemoveLogFile->mNxtLogFile;

    sPrvLogFile->mNxtLogFile = aRemoveLogFile->mNxtLogFile;    
    sNxtLogFile->mPrvLogFile = aRemoveLogFile->mPrvLogFile;

    sLogFileOpenList->mLogFileOpenListLen--;

#ifdef DEBUG 
    checkLogFileOpenList( sLogFileOpenList );
#endif

    sState = 0;
    IDE_TEST( sLogFileOpenList->mLogFileOpenListMutex.unlock() 
              != IDE_SUCCESS );

    aRemoveLogFile->mNxtLogFile = NULL;
    aRemoveLogFile->mPrvLogFile = NULL;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sState != 0 )
    {
        IDE_ASSERT( sLogFileOpenList->mLogFileOpenListMutex.unlock() 
                    == IDE_SUCCESS );
    }
    
    return IDE_FAILURE;
}

/***********************************************************************
 * Description :
 * ����ȭ log file�� �����Ѵ�.
 * ����ȭ log file ������ ������ log file ������ log������ invalid�� ��쿡��
 * �����ϸ� valid�� ���� log file�� ������ �����Ѵ�.
 *
 * aLstLogFileNo - [IN] ������ log�� ��ϵ� ����ȭ log file ��ȣ
 ***********************************************************************/
IDE_RC smrLogMultiplexThread::recoverMultiplexLogFile( UInt aLstLogFileNo )
{
    idBool       sIsCompleteLogFile;
    idBool       sLogFileExist;
    smrLogFile * sLogFile;
    UInt         sLogFileNo;
    UInt         sState = 0;

    IDE_ASSERT( mThreadType == SMR_LOG_MPLEX_THREAD_TYPE_CREATE );

    sLogFileNo = aLstLogFileNo;

    while( sLogFileNo != 0 )
    {
        sIsCompleteLogFile = ID_FALSE;

        /* ������ log file�� ������ openLstLogFile�Լ����� �����Ѵ�. */
        sLogFileNo--;

        IDE_TEST( openLogFile( sLogFileNo, 
                               ID_FALSE, /*addToList*/
                               &sLogFile, 
                               &sLogFileExist ) 
                  != IDE_SUCCESS )

        /* 
         * log ����ȭ ���������� ������ log file������ 
         * log file�� �������� ������ �ִ�.
         */
        if ( sLogFileExist != ID_TRUE )
        {
            /* ������ �������� ������ open�����ʱ� ������ close���� �ʴ´� */
            IDE_CONT( skip_recover );
        }   
        sState = 1;

        IDE_TEST( isCompleteLogFile( sLogFile, &sIsCompleteLogFile ) 
                  != IDE_SUCCESS );

        if ( sIsCompleteLogFile == ID_FALSE )
        {
            /* log file�� ����� sync���� ���ߴ�. log file ���� ���� */
            IDE_TEST( recoverLogFile( sLogFile ) != IDE_SUCCESS );
        }
        else
        {
            /* 
             * log file�� valid�ϴ�. valid�� log file�� ���� log file���� 
             * logfile thread������ ���������� sync�Ǿ��ִ� �����̹Ƿ� 
             * ���� ������ ���̻� �� �ʿ䰡 ����.
             */
        }
        
        sState = 0;
        IDE_TEST( closeLogFile( sLogFile, 
                                ID_FALSE ) /*aRemoveFromList*/
                  != IDE_SUCCESS );

        if ( (sLogFileNo == 0) || (sIsCompleteLogFile == ID_TRUE) )
        {
            break;
        }
    }

    IDE_EXCEPTION_CONT( skip_recover );
     
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch( sState )
    {
        case 1:
            IDE_ASSERT( closeLogFile( sLogFile, 
                                      ID_FALSE ) /*aRemoveFromList*/
                        == IDE_SUCCESS );
        case 0:
        default:
            break;
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description :
 * ������ �ʿ��� ����ȭ log file���� �˻��Ѵ�. log file��
 * SMR_LT_FILE_END log�� ��� �Ǿ����� ������ false�� ��ȯ�Ѵ�.
 *
 * aLogFile             - [IN]  valid���� �˻��� log file
 * aIsCompleteLogFile   - [OUT] log file�� ������ sync�Ǿ��ִ��� ��� ����
 ***********************************************************************/
IDE_RC smrLogMultiplexThread::isCompleteLogFile( 
                                    smrLogFile * aLogFile,
                                    idBool     * aIsCompleteLogFile )
{
    smrLogHead   sLogHead;
    SChar      * sLogPtr;
    smLSN        sLSN;
    idBool       sFindFileEndLog;
    UInt         sLogOffset;
    UInt         sLogSize;
    UInt         sLogHeadSize;

    IDE_ASSERT( mThreadType == SMR_LOG_MPLEX_THREAD_TYPE_CREATE );
    
    sLogOffset      = 0;
    sFindFileEndLog = ID_FALSE;
    sLogHeadSize    = ID_SIZEOF( smrLogHead );

    while(1)
    {
        aLogFile->read( sLogOffset, &sLogPtr );

        if ( smrLogComp::isCompressedLog( sLogPtr ) == ID_TRUE )
        {
            sLogSize    = smrLogComp::getCompressedLogSize( sLogPtr );
            sLogOffset += sLogSize;
        }
        else
        {
            //BUG-34791 SIGBUS occurs when recovering multiplex logfiles 
            aLogFile->read( sLogOffset, (SChar*)&sLogHead, sLogHeadSize );
            sLogSize = smrLogHeadI::getSize( &sLogHead );

            /* BUG-35392 */
            if ( smrLogHeadI::isDummyLog( &sLogHead ) == ID_FALSE )
            {
                SM_SET_LSN( sLSN, aLogFile->mFileNo, sLogOffset );

                if ( aLogFile->isValidLog( &sLSN,
                                           &sLogHead,
                                           sLogPtr,
                                           sLogSize ) == ID_TRUE )
                {
                    /* SMR_LT_FILE_END�� ���������� ��ϵǾ��ִٸ� 
                     * ������ sync��log file�̴�. */
                    if ( smrLogHeadI::getType( &sLogHead ) == SMR_LT_FILE_END )
                    {
                        sFindFileEndLog = ID_TRUE;
                        break;
                    }

                }
                else
                {
                    /* nothing to do */
                }
            }

            sLogOffset += sLogSize;
        }

        if ( (sLogOffset > smuProperty::getLogFileSize()) || (sLogSize == 0) )
        {
            break;
        }
    }

    *aIsCompleteLogFile = sFindFileEndLog;

    return IDE_SUCCESS;
}

/***********************************************************************
 * Description :
 * ���� �籸���� sync���� ���� ����ȭ log file�� �����Ѵ�.
 * ���� log file�� ��ü ������ ����ȭ log file�� copy�Ѵ�.
 *
 * aLogFile - [IN]  ������ ����ȭ log file
 ***********************************************************************/
IDE_RC smrLogMultiplexThread::recoverLogFile( smrLogFile * aLogFile )
{
    SChar       sOriginalLogFileName[SM_MAX_FILE_NAME] = {'\0',};
    smrLogFile  sOriginalLogFile; 
    UInt        sState = 0;

    idlOS::snprintf( sOriginalLogFileName,
                     SM_MAX_FILE_NAME,
                     "%s%c%s%"ID_UINT32_FMT,
                     mOriginalLogPath,
                     IDL_FILE_SEPARATOR,
                     SMR_LOG_FILE_NAME,
                     aLogFile->mFileNo );

    IDE_TEST( sOriginalLogFile.initialize() != IDE_SUCCESS ); 
    sState = 1;

    IDE_TEST( sOriginalLogFile.open( aLogFile->mFileNo,
                                     sOriginalLogFileName,
                                     ID_TRUE, /* aIsMultiplexLogFile */
                                     smuProperty::getLogFileSize(),
                                     ID_FALSE ) 
              != IDE_SUCCESS ); 
    sState = 2;
    
    /* ����ȭ log buffer�� ���� log buffer�� ������ copy�Ѵ�. */
    idlOS::memcpy( aLogFile->mBase, 
                   sOriginalLogFile.mBase, 
                   smuProperty::getLogFileSize() );

    /* ����ȭ log buffer�� ������ logfile�� ����Ѵ�. */
    IDE_TEST( aLogFile->syncToDisk( 0, smuProperty::getLogFileSize() ) 
              != IDE_SUCCESS );

    sState = 0;
    IDE_TEST( sOriginalLogFile.destroy() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch( sState )
    {
        case 2:
        case 1:
            IDE_ASSERT( sOriginalLogFile.destroy() == IDE_SUCCESS );
        case 0:
        default:
            break;
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description :
 * resetlog����� reset�� �α׹����� ������ ���Ϸ� ����Ѵ�.
 *
 * aSyncThread      - [IN] 
 * aOffset          - [IN] sync�� ���� offset
 * aLogFileSize     - [IN] �α����� ũ��
 * aOriginalLogFile - [IN] reset�� ���� �α�����
 ***********************************************************************/
IDE_RC smrLogMultiplexThread::syncToDisk( 
                                    smrLogMultiplexThread * aSyncThread,
                                    UInt                    aOffset,
                                    UInt                    aLogFileSize,
                                    smrLogFile            * aOriginalLogFile )
{
    smrLogFile * sLogFile;
    UInt         sMultiplexIdx;
    UInt         sState = 0;
    idBool       sLogFileExist;

    IDE_TEST_CONT( mMultiplexCnt == 0, skip_sync );

    IDE_ASSERT( aSyncThread      != NULL );
    IDE_ASSERT( aOriginalLogFile != NULL );
    IDE_ASSERT( aSyncThread[0].mThreadType == SMR_LOG_MPLEX_THREAD_TYPE_SYNC );

    for ( sMultiplexIdx = 0; sMultiplexIdx < mMultiplexCnt; sMultiplexIdx++ )
    {
        IDE_TEST( aSyncThread[sMultiplexIdx].openLogFile( 
                                                     aOriginalLogFile->mFileNo, 
                                                     ID_FALSE,  /*addToList*/
                                                     &sLogFile,
                                                     &sLogFileExist ) 
                  != IDE_SUCCESS );
        if ( sLogFileExist != ID_TRUE )
        {
            continue;
        }
        sState = 1;

        sLogFile->clear( aOffset ); 
 
        IDE_TEST( sLogFile->syncToDisk( aOffset, aLogFileSize ) != IDE_SUCCESS );
 
        sState = 0;
        IDE_TEST( aSyncThread[sMultiplexIdx].closeLogFile( 
                                                sLogFile, 
                                                ID_FALSE ) /*aRemoveFromList*/
                  != IDE_SUCCESS );
    }

    IDE_EXCEPTION_CONT( skip_sync ); 

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch( sState )
    {
        case 1:
            IDE_ASSERT( aSyncThread[sMultiplexIdx].closeLogFile( 
                                                        sLogFile, 
                                                        ID_FALSE ) /*aRemoveFromList*/
                        == IDE_SUCCESS );
        case 0:
        default:
            break;
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description :
 * log file open list�� aLogFileNo�� ���� �α������� �̹� �����ϴ��� �˻��Ѵ�.
 *
 * aLogFileNo       - [IN] �˻��� logfile��ȣ 
 * aLogFileOpenList - [IN] �˻��� list
 ***********************************************************************/
smrLogFile * smrLogMultiplexThread::findLogFileInList( 
                                UInt                 aLogFileNo,
                                smrLogFileOpenList * aLogFileOpenList)
{
    smrLogFile * sCurLogFile;
    smrLogFile * sLogFileOpenListHdr;
    smrLogFile * sFindLogFile = NULL;

    sLogFileOpenListHdr =  &aLogFileOpenList->mLogFileOpenListHdr;

    sCurLogFile = sLogFileOpenListHdr->mNxtLogFile;

    while( sLogFileOpenListHdr != sCurLogFile )
    {
        if ( sCurLogFile->mFileNo == aLogFileNo )
        {
            sFindLogFile = sCurLogFile;
            break;
        }
        sCurLogFile = sCurLogFile->mNxtLogFile;
    }

    return sFindLogFile;
}

#ifdef DEBUG
/***********************************************************************
 * Description :
 * DEBUG������ ����
 * LogFileOpenList�� ����� list �渮�� ���� list ���̸� �˻��Ѵ�.
 *
 * aLogFileOpenList - [IN] �˻��� LogFileOpenList
 ***********************************************************************/
void smrLogMultiplexThread::checkLogFileOpenList( 
                                smrLogFileOpenList * aLogFileOpenList)
{
    smrLogFile * sCurLogFile;
    smrLogFile * sLogFileOpenListHdr;
    UInt         sLogFileOpenListLen;

    sLogFileOpenListHdr =  &aLogFileOpenList->mLogFileOpenListHdr;

    sCurLogFile         =  sLogFileOpenListHdr->mNxtLogFile;
    sLogFileOpenListLen = 0;

    while( sLogFileOpenListHdr != sCurLogFile )
    {
        sCurLogFile = sCurLogFile->mNxtLogFile;

        sLogFileOpenListLen++;
    }

    IDE_DASSERT( sLogFileOpenListLen == aLogFileOpenList->mLogFileOpenListLen );

    sCurLogFile         =  sLogFileOpenListHdr->mPrvLogFile;
    sLogFileOpenListLen = 0;
    while( sLogFileOpenListHdr != sCurLogFile )
    {
        sCurLogFile = sCurLogFile->mPrvLogFile;

        sLogFileOpenListLen++;
    }

    IDE_DASSERT( sLogFileOpenListLen == aLogFileOpenList->mLogFileOpenListLen );
}
#endif

/***********************************************************************
 * Description : BUG-38801
 *      mOriginalLogFile�� Switch�� �Ϸ�ɶ����� ���
 *
 * aIsNeedRestart [IN/OUT] - mOriginalLogFile �� Switch �ϷḦ ��ٷ�������
 *                              + LogMultiplex�� ��õ� �ؾ� ���� ����
 *          i ) ó�� aIsNeedRestart�� ID_FALSE�� �Ѿ�´�.
 *              �� ��� LstLSN �� mOriginalLogFile �� ���� �α׷�
 *              �����ɶ����� ��� �ϰ�, aIsNeedRestart�� ID_TRUE��
 *              �����Ͽ� LogMultiplex�� ��õ� �ϵ��� �Ѵ�.
 *          ii) aIsNeedRestart�� ID_TRUE�� �Ѿ�´ٸ�,
 *              �̹� �� �Լ��� �ѹ� ��� �Ͽ� ����� �� ����̹Ƿ�,
 *              �ٽ� ����� ���� �ʵ��� aIsNeedRestart�� ID_FALSE�� ����
 ***********************************************************************/
void smrLogMultiplexThread::checkOriginalLogSwitch( idBool    * aIsNeedRestart )
{
    smLSN           sNewLstLSN;     /* BUG-38801 */
    PDL_Time_Value  sSleepTimeVal;  /* BUG-38801 */

    sSleepTimeVal.set( 0, 100 );
    SM_LSN_MAX( sNewLstLSN );

    if ( *aIsNeedRestart == ID_FALSE )
    {
        while( 1 )
        {
            /* logfile�� ����� Log�� ������ Offset�� �޾ƿ´�. 
             * FAST_UNLOCK_LOG_ALLOC_MUTEXT = 1�� ��� ���̸� �������� �ʴ� �α� ���ڵ��� Offset */
            smrLogMgr::getLstLogOffset( &sNewLstLSN );

            if ( (sNewLstLSN.mFileNo > mOriginalLogFile->mFileNo) &&
                 (sNewLstLSN.mOffset != 0) )
            {
                /* ���� ���Ϸ� �Ѿ�� break */
                *aIsNeedRestart = ID_TRUE;
                break;
            }
            else
            {
                idlOS::sleep( sSleepTimeVal );
            }
        } // end of while
    }
    else
    {
        /* aIsNeedRestart�� TRUE�̸�, �� ȣ�� �̹Ƿ� �Ѿ�� */
        *aIsNeedRestart = ID_FALSE;
    }
}

