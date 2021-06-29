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
 * $$Id: smrArchThread.cpp 83870 2018-09-03 04:32:39Z kclee $
 **********************************************************************/

#include <idl.h>
#include <ideErrorMgr.h>
#include <smErrorCode.h>
#include <smDef.h>
#include <smm.h>
#include <smu.h>
#include <smrDef.h>
#include <smrArchThread.h>
#include <smrArchMultiplexThread.h>
#include <smrReq.h>

smrArchThread::smrArchThread() : idtBaseThread()
{
    
}

smrArchThread::~smrArchThread()
{
    
}

// ��ī�̺� ������ ��ü�� �ʱ�ȭ �Ѵ�.
// aArchivePath   - [IN] ��ī�̺� �αװ� ����� ���丮
// aLogFileMgr    - [IN] �� ��ī�̺� �����尡 ��ī�̺��� �α����ϵ��� �����ϴ�
//                     �α����� ������.
// aLstArchFileNo - [IN] ���������� Archive�� File No
IDE_RC smrArchThread::initialize( const SChar   * aArchivePath,
                                  smrLogFileMgr * aLogFileMgr,
                                  UInt            aLstArchFileNo)
{
    const SChar ** sArchiveMultiplexPath;
    UInt           sArchPathIdx;

    IDE_DASSERT( aArchivePath != NULL );
    IDE_DASSERT( aLogFileMgr != NULL );

    /*
     * PROJ-2232 Multiplex archivelog
     * ������Ƽ�κ��� archive�� ���丮 path�� ������ �����´�. 
     */ 
    sArchiveMultiplexPath = smuProperty::getArchiveMultiplexDirPath();
    mArchivePathCnt       = smuProperty::getArchiveMultiplexCount() + 1;

    /* 
     * ARCHIVE_DIR? ARCHIVE_MULTIPLEX_DIR���� ������ ���丮path��
     * mArchivePath������ �����Ѵ�. 
     */ 
    for( sArchPathIdx = 0; 
         sArchPathIdx < mArchivePathCnt; 
         sArchPathIdx++ )
    {
        if( sArchPathIdx == SMR_ORIGINAL_ARCH_DIR_IDX )
        {
            mArchivePath[sArchPathIdx] = aArchivePath;
        }
        else
        {
            mArchivePath[sArchPathIdx] = 
                            sArchiveMultiplexPath[ sArchPathIdx-1 ];
        }

        IDE_TEST_RAISE(idf::access( mArchivePath[sArchPathIdx], F_OK) != 0,
                       path_exist_error);
        IDE_TEST_RAISE(idf::access( mArchivePath[sArchPathIdx], W_OK) != 0,
                       err_no_write_perm_path);
        IDE_TEST_RAISE(idf::access( mArchivePath[sArchPathIdx], X_OK) != 0,
                       err_no_execute_perm_path);
    }

    IDE_DASSERT( mArchivePathCnt != 0 );
    
    /*PROJ-2232 Multiplex archive log*/
    IDE_TEST( smrArchMultiplexThread::initialize( &mArchMultiplexThreadArea,
                                                  mArchivePath,
                                                  mArchivePathCnt )
              != IDE_SUCCESS );

    mLogFileMgr = aLogFileMgr;
    
    IDE_TEST(mMtxArchList.initialize((SChar*)"Archive Log List Mutex",
                                     IDU_MUTEX_KIND_POSIX,
                                     IDV_WAIT_INDEX_NULL )
             != IDE_SUCCESS);
    
    IDE_TEST(mMtxArchThread.initialize((SChar*)"Archive Log Thread Mutex",
                                       IDU_MUTEX_KIND_POSIX,
                                       IDV_WAIT_INDEX_NULL )
             != IDE_SUCCESS);

    IDE_TEST(mMemPool.initialize(IDU_MEM_SM_SMR,
                                 (SChar *)"Archive Log Thread Mem Pool",
                                 1,
                                 ID_SIZEOF(smrArchLogFile),
                                 SMR_ARCH_THREAD_POOL_SIZE,
                                 IDU_AUTOFREE_CHUNK_LIMIT,			/* ChunkLimit */
                                 ID_TRUE,							/* UseMutex */
                                 IDU_MEM_POOL_DEFAULT_ALIGN_SIZE,	/* AlignByte */
                                 ID_FALSE,							/* ForcePooling */
                                 ID_TRUE,							/* GarbageCollection */
                                 ID_TRUE,                           /* HWCacheLine */
                                 IDU_MEMPOOL_TYPE_LEGACY            /* mempool type */) 
             != IDE_SUCCESS);			

    IDE_TEST_RAISE(mCv.initialize((SChar *)"Archive Log Thread Cond")
                   != IDE_SUCCESS, err_cond_var_init);
    
    mResume      = ID_TRUE;

    mArchFileList.mArchPrvLogFile = &mArchFileList;
    mArchFileList.mArchNxtLogFile = &mArchFileList;
    
    mLstArchFileNo = aLstArchFileNo;
    
    return IDE_SUCCESS;

    IDE_EXCEPTION(path_exist_error);
    {               
        IDE_SET(ideSetErrorCode(smERR_ABORT_NoExistPath,
                                mArchivePath[sArchPathIdx] ));
    }
    IDE_EXCEPTION(err_no_write_perm_path);
    {               
        IDE_SET(ideSetErrorCode(smERR_ABORT_NoWritePermFile,
                                mArchivePath[sArchPathIdx] ));
    }
    IDE_EXCEPTION(err_no_execute_perm_path);
    {               
        IDE_SET(ideSetErrorCode(smERR_ABORT_NoExecutePermFile,
                                mArchivePath[sArchPathIdx] ));
    }
    IDE_EXCEPTION(err_cond_var_init);
    {
        IDE_SET(ideSetErrorCode(smERR_FATAL_ThrCondInit));
    }
    IDE_EXCEPTION_END;

    ideLog::log(SM_TRC_LOG_LEVEL_WARNNING,
                SM_TRC_MRECOVERY_ARCH_WARNING);
    
    return IDE_FAILURE;
    
}

/* ���� ��ŸƮ�� �ÿ�
 * ��ī�̺��� �α����� ����Ʈ�� �籸���Ѵ�.
 *
 * aStartNo - [IN] ��ī�̺� ��� �α��������� üũ�� ù��° �α������� ��ȣ
 * aEndNo   - [IN] ��ī�̺� ��� �α��������� üũ�� ������ �α������� ��ȣ
 *
 * ��ī�̺��� �α��������� üũ�� ����� �Ǵ� �α����ϵ��� ������ ����.
 *
 * [ ������ ���� Shutdown�� ��� ]
 *     aStartNo - ������ ������ (��ī�̺��) �α����� ��ȣ
 *     aEndNo   - ������ �α����� ��ȣ
 *
 * [ ������ ������ Shutdown�� ��� ]
 *     aStartNo - ������ ������ (��ī�̺��) �α����� ��ȣ
 *     aEndNo   - Restart Recovery�� RedoLSN
 */
IDE_RC smrArchThread::recoverArchiveLogList( UInt aStartNo,
                                             UInt aEndNo )
    
{
    iduFile     sFile;
    UInt        sState      = 0;
    UInt        sFileCount  = 0;
    UInt        sCurFileNo;
    UInt        i;
    UInt        sAlreadyArchivedCnt = 0;    /* BUG-39746 */
    SChar       sLogFileName[SM_MAX_FILE_NAME];
    ULong       sFileSize = 0;
    idBool      sIsArchived;
    idBool      sCanUpdateLstArchLogFileNum = ID_TRUE;  /* BUG-39746 */
    
    /* ------------------------------------------------
     * BUG-12246 ���� ������ ARCHIVE�� �α������� ���ؾ���
     * 1) ������ ������������ ���
     * lst delete logfile no ���� recovery lsn���� archive dir��
     * �˻��Ͽ� �������� �ʴ� �α����ϸ� �ٽ� archive list�� ����Ѵ�.
     * �� ���Ĵ� redoAll �ܰ迡�� ����ϵ��� �Ѵ�.
     * 2) ������ ���������� ���
     * lst delete logfile no���� end lsn���� archive dir�� �˻��Ͽ�
     * �������� �ʴ� �α����ϸ� �ٽ� archive list�� ����Ѵ�. 
     * ----------------------------------------------*/

    IDE_ASSERT(aStartNo <= aEndNo);

    for (sCurFileNo = aStartNo; sCurFileNo < aEndNo; sCurFileNo++)
    {
        sIsArchived         = ID_TRUE;
        sAlreadyArchivedCnt = 0;

        for( i = 0; i < mArchivePathCnt; i++ )
        {
            idlOS::memset(sLogFileName, 0x00, SM_MAX_FILE_NAME);

            idlOS::snprintf(sLogFileName,
                            SM_MAX_FILE_NAME,
                            "%s%c%s%"ID_UINT32_FMT,
                            mArchivePath[i],
                            IDL_FILE_SEPARATOR,
                            SMR_LOG_FILE_NAME,
                            sCurFileNo);

            if (idf::access(sLogFileName, F_OK) == 0)
            {
                IDE_TEST(sFile.initialize(IDU_MEM_SM_SMR,
                                          1, /* Max Open FD Count */
                                          IDU_FIO_STAT_OFF,
                                          IDV_WAIT_INDEX_NULL)
                         != IDE_SUCCESS);
                sState = 1;

                IDE_TEST(sFile.setFileName(sLogFileName) != IDE_SUCCESS);
                IDE_TEST(sFile.open() != IDE_SUCCESS);
                sState = 2;

                IDE_TEST( sFile.getFileSize( &sFileSize ) != IDE_SUCCESS );

                sState = 1;
                IDE_TEST(sFile.close() != IDE_SUCCESS);

                sState = 0;
                IDE_TEST(sFile.destroy() != IDE_SUCCESS);

                // ��ī�̺� �α� ���丮�� �α� ������ ������ ���
                if ( sFileSize == (ULong)smuProperty::getLogFileSize() )
                {
                    IDE_DASSERT( mLstArchFileNo <= sCurFileNo );

                    /* BUG-39746
                     * �� archive Path ���� ���� ��ī�̺� �ƴ��� �˻��Ͽ�
                     * sAlreadyArchivedCnt�� ������Ų��. */
                    sAlreadyArchivedCnt++;

                    /* BUG-39746
                     * ��� ArchivePath���� ��ī�̺� �Ǿ� �ְ�,
                     * �� �α����� ������ ��� ������ 
                     * ��ī�̺��ϴµ� ������ ���ٸ�,
                     * ��, LstArchLogFileNo�� �����ص� �����ٸ� �����Ѵ�. */
                    if( (sAlreadyArchivedCnt == mArchivePathCnt) &&
                        (sCanUpdateLstArchLogFileNum == ID_TRUE ) )
                    {
                        // ������ ��ī�̺�� �α����Ϲ�ȣ ����
                        setLstArchLogFileNo( sCurFileNo );
                    }
                    else
                    {
                        // do nothing
                    }
                }
                else
                {
                    // ����� �ٸ��� ��ī�̺� �ؾ� ��
                    sIsArchived = ID_FALSE;
                    break;
                }
            }
            else
            {
                /* �ѹ��̶� ��ī�̺� �ؾ� �� ������ ã�Ҵٸ�,
                 * ���̻� LstArchLogFileNo�� �������� ���Ѵ�. */
                sCanUpdateLstArchLogFileNum = ID_FALSE;

                sIsArchived = ID_FALSE;
                break;
            }
        }

        if( sIsArchived == ID_FALSE )
        {
            // ��ī�̺� �α� ���丮�� �α������� ������
            // ��ī�̺� ��� �α����� ����Ʈ�� �߰�
            IDE_TEST(addArchLogFile(sCurFileNo) != IDE_SUCCESS);

            sFileCount++;
        }
        else
        {
            // ������ ��ī�̺�� �α����Ϲ�ȣ ����
            setLstArchLogFileNo( sCurFileNo );
        }
    }

    if (aStartNo < aEndNo)
    {
        ideLog::log(SM_TRC_LOG_LEVEL_MRECOV,
                    SM_TRC_MRECOVERY_ARCH_REBUILD_LOGFILE,
                    sFileCount);
    }


    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();
    switch(sState)
    {
    case 2:
        IDE_ASSERT(sFile.close() == IDE_SUCCESS); 
        
    case 1:
        IDE_ASSERT(sFile.destroy() == IDE_SUCCESS);
    }
    IDE_POP();
    
    return IDE_FAILURE;
}

// ��ī�̺� �����带 ���۽�Ű��, �����尡 ����������
// ���۵� ������ ��ٸ���.
IDE_RC smrArchThread::startThread()
{
    mFinish      = ID_FALSE;

    IDE_TEST( smrArchMultiplexThread::startThread( mArchMultiplexThreadArea ) 
              != IDE_SUCCESS );

    IDE_TEST(start() != IDE_SUCCESS);
    IDE_TEST(waitToStart(0) != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


// ��ī�̺� �����带 �����ϰ�, �����尡 ����������
// �����Ǿ��� ������ ��ٸ���.
IDE_RC smrArchThread::shutdown()
{
    
    UInt          sState = 0;

    IDE_TEST( smrArchMultiplexThread::shutdownThread( mArchMultiplexThreadArea ) 
              != IDE_SUCCESS );

    IDE_TEST(lockThreadMtx() != IDE_SUCCESS);
    sState = 1;
    
    mFinish      = ID_TRUE;

    IDE_TEST_RAISE(mCv.signal() != IDE_SUCCESS, err_cond_signal);

    sState = 0;
    IDE_TEST(unlockThreadMtx() != IDE_SUCCESS);

    IDE_TEST_RAISE(join() != IDE_SUCCESS, err_thr_join);

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_thr_join);
    {
        IDE_SET(ideSetErrorCode(smERR_FATAL_Systhrjoin));   
    }
    IDE_EXCEPTION(err_cond_signal);
    {
        IDE_SET(ideSetErrorCode(smERR_FATAL_ThrCondSignal));
    }
    IDE_EXCEPTION_END;

    if(sState != 0)
    {
        IDE_PUSH();
        IDE_ASSERT(unlockThreadMtx() == IDE_SUCCESS);
        IDE_POP();
    }
    
    return IDE_FAILURE;
    
}

// ��ī���� ������ ��ü�� ���� �Ѵ�.
IDE_RC smrArchThread::destroy()
{
    /*PROJ-2232 Multiplex archive log*/
    IDE_TEST( smrArchMultiplexThread::destroy( mArchMultiplexThreadArea ) 
              != IDE_SUCCESS ); 

    IDE_TEST(clearArchList() != IDE_SUCCESS);
    
    IDE_TEST(mMtxArchList.destroy() != IDE_SUCCESS);
    IDE_TEST(mMtxArchThread.destroy() != IDE_SUCCESS);
    IDE_TEST(mMemPool.destroy() != IDE_SUCCESS);

    IDE_TEST_RAISE(mCv.destroy() != IDE_SUCCESS, err_cond_destroy);
                   
    return IDE_SUCCESS;

    IDE_EXCEPTION( err_cond_destroy ); 
    {
        IDE_SET(ideSetErrorCode(smERR_FATAL_ThrCondDestroy));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
    
}

void smrArchThread::run()
{

    IDE_RC         rc;
    UInt           sState = 0;
    PDL_Time_Value sCurTimeValue;
    PDL_Time_Value sFixTimeValue;
  
  startPos:
    sState=0;
    
    IDE_TEST( lockThreadMtx() != IDE_SUCCESS);
    sState = 1;

    sFixTimeValue.set(smuProperty::getSyncIntervalSec(),
                      smuProperty::getSyncIntervalMSec() * 1000);
    
    while(mFinish == ID_FALSE)
    {
        mResume = ID_FALSE;
        sCurTimeValue = idlOS::gettimeofday();
        sCurTimeValue += sFixTimeValue;
      
        sState = 0;
        rc = mCv.timedwait(&mMtxArchThread, &sCurTimeValue);
        sState = 1;

        if(mFinish == ID_TRUE)
        {
            break;
        }

        if ( smuProperty::isRunArchiveThread() == SMU_THREAD_OFF )
        {
            // To Fix PR-14783
            // System Thread�� �۾��� �������� �ʵ��� �Ѵ�.
            continue;
        }
        else
        {
            // Go Go 
        }

        if(rc != IDE_SUCCESS) // cond_wait�� ������ �ð��� ������ ��� ���
        {
            IDE_TEST_RAISE(mCv.isTimedOut() != ID_TRUE, err_cond_wait);
            mResume = ID_TRUE;
        }
        else // cond_signal�� ���� ��� ���
        {
            if (mResume == ID_FALSE)
            {
                // mResume�� ID_FALSE�̸�
                // cond_wait�ϴ� interval�� �缳���϶�� signal�� �������̴�.

                // Todo : cond_wait�ϴ� interval�� �缳�� �ϵ��� ����
                continue;
            }
            // mResume�� ID_TRUE�̸� ��ī�̺��� �ǽ��Ѵ�.
        }

        // ��ī�̺� �����尡 �����.
        // ��ī�̺��� �α����ϸ���Ʈ�� ���� ��ī�̺� �ǽ�.
        rc = archLogFile();
        
        if ( rc != IDE_SUCCESS && errno !=0 && errno != ENOSPC )
        {
            IDE_RAISE(error_archive);
        }
    }

    /* ------------------------------------------------
     * recovery manager destroy�ÿ� archive thread��
     * ���������, archive�� �����ϰ�, ������� ������,
     * archive thread destroy�ÿ� archive log list��
     * clear �ع�����.
     * ----------------------------------------------*/   
    sState = 0;
    IDE_TEST(unlockThreadMtx() != IDE_SUCCESS);
    
    return;

    IDE_EXCEPTION(err_cond_wait);
    {
        IDE_SET(ideSetErrorCode(smERR_FATAL_ThrCondWait));
    }
    IDE_EXCEPTION(error_archive);
    IDE_EXCEPTION_END;

    IDE_PUSH();
    if(sState != 0)
    {
        IDE_ASSERT(unlockThreadMtx() == IDE_SUCCESS);
    }
    IDE_POP();
    
    if (mFinish == ID_FALSE)
    {
        ideLog::log(SM_TRC_LOG_LEVEL_WARNNING,
                    SM_TRC_MRECOVERY_ARCH_WARNING3,
                    errno);

        idlOS::sleep(2);
        goto startPos;
    }
    
    return;
    
}

// ��ī�̺��� �α����� ����Ʈ�� �ִ� �α����ϵ��� ��ī�̺��Ѵ�.
// ��ī�̺� �����尡 �ֱ�������, Ȥ�� ��û�� ���� ����� �����ϴ� �Լ��̴�.
IDE_RC smrArchThread::archLogFile()
{

    UInt            sState = 0;
    UInt            sCurFileNo;
    SChar           sSrcLogFileName[SM_MAX_FILE_NAME];
    smrArchLogFile *sCurLogFilePtr;
    smrArchLogFile *sNxtLogFilePtr;
    smrLogFile     *sSrcLogFile = NULL;

    IDE_TEST(lockListMtx() != IDE_SUCCESS);
    sState = 1;

    sCurLogFilePtr = mArchFileList.mArchNxtLogFile;

    sState = 0;
    IDE_TEST(unlockListMtx() != IDE_SUCCESS);

    /* ================================================== 
     * For Each Log File in archive log file list,       
     * backup already open/synced log file to archive_dir
     * ================================================== */
    while(sCurLogFilePtr != &mArchFileList)
    {
        sNxtLogFilePtr = sCurLogFilePtr->mArchNxtLogFile;
        sCurFileNo = sCurLogFilePtr->mFileNo;

        IDE_TEST_RAISE( mLogFileMgr->open( sCurFileNo,
                                           ID_FALSE,/* For Read */
                                           &sSrcLogFile )
                        != IDE_SUCCESS, open_err );

        /*PROJ-2232 Multiplex archive log*/
        IDE_TEST( smrArchMultiplexThread::performArchving( 
                                                mArchMultiplexThreadArea,
                                                sSrcLogFile )
                  != IDE_SUCCESS );

        /* ================================================================
         * In checkpointing to determine the log file number to be removed,
         * set the last archived log file number.
         * ================================================================ */
        IDE_TEST( setLstArchLogFileNo(sCurFileNo) != IDE_SUCCESS);

        IDE_TEST( removeArchLogFile(sCurLogFilePtr) != IDE_SUCCESS);

        IDE_TEST( mLogFileMgr->close(sSrcLogFile) != IDE_SUCCESS);
        sSrcLogFile = NULL;
        sState = 0;

        sCurLogFilePtr = sNxtLogFilePtr;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( open_err )
    {
       ideLog::log( SM_TRC_LOG_LEVEL_WARNNING,
                    "fail to open logfile\n"
                    "Log File Path : %s\n"
                    "Log File No   : %"ID_UINT32_FMT"\n",
                    mLogFileMgr->getSrcLogPath(),
                    sCurFileNo );
    }

    IDE_EXCEPTION_END;

    IDE_PUSH();

    if(sState == 1)
    {
        IDE_ASSERT(unlockListMtx() == IDE_SUCCESS);
    }

    if (sSrcLogFile != NULL)
    {
        IDE_ASSERT(mLogFileMgr->close(sSrcLogFile) == IDE_SUCCESS);
        sSrcLogFile = NULL;
    }

    if( mArchMultiplexThreadArea[0].mFinish == ID_TRUE )
    {
        mFinish = mArchMultiplexThreadArea[0].mFinish;
    }


    /* BUG-36662 Add property for archive thread to kill server when doesn't
     * exist source logfile */
    if( smuProperty::getCheckSrcLogFileWhenArchThrAbort() == ID_TRUE )
    {
        idlOS::snprintf(sSrcLogFileName, SM_MAX_FILE_NAME,
                        "%s%c%s%"ID_UINT32_FMT,
                        mLogFileMgr->getSrcLogPath(),
                        IDL_FILE_SEPARATOR,
                        SMR_LOG_FILE_NAME,
                        sCurFileNo); 

        IDE_ASSERT( idf::access(sSrcLogFileName, F_OK) == 0 );
    }

    IDE_POP();

    return IDE_FAILURE;
    
}

/* ��ī�̺��� �α����� ����Ʈ�� �α������� �ϳ� ���� �߰��Ѵ�.
 *
 * aLogFileNo - [IN] ���� ��ī�̺� ��� �߰��� �α������� ��ȣ
 */
IDE_RC smrArchThread::addArchLogFile(UInt  aLogFileNo)
{

    smrArchLogFile *sArchLogFile;

    /* smrArchThread_addArchLogFile_alloc_ArchLogFile.tc */
    IDU_FIT_POINT("smrArchThread::addArchLogFile::alloc::ArchLogFile");
    IDE_TEST(mMemPool.alloc((void**)&sArchLogFile) != IDE_SUCCESS);
    sArchLogFile->mFileNo         = aLogFileNo;
    sArchLogFile->mArchNxtLogFile = NULL;
    sArchLogFile->mArchPrvLogFile = NULL;
    
    IDE_TEST(lockListMtx() != IDE_SUCCESS);
    
    sArchLogFile->mArchPrvLogFile = mArchFileList.mArchPrvLogFile;
    sArchLogFile->mArchNxtLogFile = &mArchFileList;
    
    mArchFileList.mArchPrvLogFile->mArchNxtLogFile = sArchLogFile;
    mArchFileList.mArchPrvLogFile = sArchLogFile;
    
    IDE_TEST(unlockListMtx() != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
    
}

// ��ī�̺��� �α����� ����Ʈ���� �α����� ��带 �ϳ� �����Ѵ�.
IDE_RC smrArchThread::removeArchLogFile(smrArchLogFile   *aLogFile)
{

    IDE_TEST(lockListMtx() != IDE_SUCCESS);

    aLogFile->mArchPrvLogFile->mArchNxtLogFile = aLogFile->mArchNxtLogFile;
    aLogFile->mArchNxtLogFile->mArchPrvLogFile = aLogFile->mArchPrvLogFile;
    aLogFile->mArchPrvLogFile = NULL;
    aLogFile->mArchNxtLogFile = NULL;

    IDE_TEST(unlockListMtx() != IDE_SUCCESS);
    
    IDE_TEST(mMemPool.memfree(aLogFile) != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

// ���������� ��ī�̺�� ���Ϲ�ȣ�� �����Ѵ�.
IDE_RC smrArchThread::setLstArchLogFileNo(UInt  aArchLogFileNo)
{

    IDE_TEST(lockListMtx() != IDE_SUCCESS);

    mLstArchFileNo = aArchLogFileNo;

    IDE_TEST(unlockListMtx() != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

// ���������� ��ī�̺�� ���Ϲ�ȣ�� �����´�.
IDE_RC smrArchThread::getLstArchLogFileNo(UInt *aArchLogFileNo)
{
    IDE_TEST(lockListMtx() != IDE_SUCCESS);

    *aArchLogFileNo = mLstArchFileNo;

    IDE_TEST(unlockListMtx() != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/* �������� ��ī�̺��� �α����Ϲ�ȣ�� �����´�.
 *
 * aArchFstLFileNo   - [OUT] ��ī�̺��� ù��° �α����� ��ȣ
 * aIsEmptyArchLFLst - [OUT] ��ī�̺� LogFile List�� ������� ID_TRUE�� return�Ѵ�.
 */
IDE_RC smrArchThread::getArchLFLstInfo(UInt   * aArchFstLFileNo,
                                       idBool * aIsEmptyArchLFLst )
{

    smrArchLogFile *sCurLogFilePtr;

    IDE_TEST(lockListMtx() != IDE_SUCCESS);

    sCurLogFilePtr = mArchFileList.mArchNxtLogFile;

    if (sCurLogFilePtr != &mArchFileList)
    {
        *aArchFstLFileNo   = sCurLogFilePtr->mFileNo;
        *aIsEmptyArchLFLst = ID_FALSE;
    }
    else
    {
        /* Archive LogFile List�� ��� �ִ�. */
        *aArchFstLFileNo   = ID_UINT_MAX;
        *aIsEmptyArchLFLst = ID_TRUE;
    }

    IDE_TEST(unlockListMtx() != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

// ��ī�̺��� �α����� ����Ʈ�� ��� �ʱ�ȭ �Ѵ�.
IDE_RC smrArchThread::clearArchList()
{

    smrArchLogFile *sCurLogFilePtr;
    smrArchLogFile *sNxtLogFilePtr;

    IDE_TEST(lockListMtx() != IDE_SUCCESS);

    sCurLogFilePtr = mArchFileList.mArchNxtLogFile;

    IDE_TEST(unlockListMtx() != IDE_SUCCESS);

    while(sCurLogFilePtr != &mArchFileList)
    {
        sNxtLogFilePtr = sCurLogFilePtr->mArchNxtLogFile;

        (void)removeArchLogFile(sCurLogFilePtr);

        sCurLogFilePtr = sNxtLogFilePtr;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/* ��ī�̺� �����带 ������ Archive LogFile List���� aToFileNo����
 * �α����ϵ��� ��ī�̺� ��Ų��.
 *
 * caution !!:
 * �� �Լ��� ȣ���ϱ� ���� �ݵ�� aToFileNo���� Archive LogFile List
 * �� �߰��ϰ� �� �Լ��� ȣ���Ͽ��� �Ѵ�.
 *
 * aToFileNo - [IN] aToFileNo���� Archiving�ɶ����� ����Ѵ�.
 */
IDE_RC smrArchThread::wait4EndArchLF( UInt aToFileNo )
{
    SInt    sState = 0;
    UInt    sFstLFOfArchLFLst;
    idBool  sIsEmptyArchLFLst;

    // ��ī�̺� �����尡 �۾��� �Ϸ��� ������ ��ٸ���.
    while(1)
    {
        IDE_TEST( lockThreadMtx() != IDE_SUCCESS );
        sState = 1;

        if( mResume != ID_TRUE )
        {
            // ��ī�̺� Thread�� ����� ��, signal�� ���� ��� ���
            // mResume�� ID_FALSE�̸� cond_wait�ϴ� interval�� �缳���ϰ�
            // mResume�� ID_TRUE�̸� ��ī�̺��� �ǽ��Ѵ�.
            mResume = ID_TRUE;
            IDE_TEST_RAISE( mCv.signal() != IDE_SUCCESS, err_cond_signal );
        }

        sState = 0;
        IDE_TEST( unlockThreadMtx() != IDE_SUCCESS );

        ideLog::log(SM_TRC_LOG_LEVEL_MRECOV,
                    SM_TRC_MRECOVERY_ARCH_WAITING,
                    aToFileNo);
        idlOS::sleep(5);

        IDE_TEST( getArchLFLstInfo( &sFstLFOfArchLFLst,
                                    &sIsEmptyArchLFLst )
                  != IDE_SUCCESS );

        /* BUG-23693: [SD] Online Backup�� LogFile�� ���� Switch�� ���� LogFile
         * ������ Archiving�� �ؾ� �մϴ�. */
        if( ( sIsEmptyArchLFLst == ID_TRUE ) ||
            ( sFstLFOfArchLFLst > aToFileNo ) )
        {
            break;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_cond_signal);
    {
        IDE_SET(ideSetErrorCode(smERR_FATAL_ThrCondSignal));
    }
    IDE_EXCEPTION_END;

    if(sState != 0)
    {
        IDE_PUSH();
        IDE_ASSERT(unlockThreadMtx() == IDE_SUCCESS);
        IDE_POP();
    }

    return IDE_FAILURE;

}
