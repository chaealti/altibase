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
 * $Id: smrLogFileMgr.cpp 89697 2021-01-05 10:29:13Z et16 $
 **********************************************************************/

#include <smErrorCode.h>
#include <smm.h>
#include <smr.h>
#include <smiMain.h>

smrLogMultiplexThread * smrLogFileMgr::mSyncThread;
smrLogMultiplexThread * smrLogFileMgr::mCreateThread;
smrLogMultiplexThread * smrLogFileMgr::mDeleteThread;
smrLogMultiplexThread * smrLogFileMgr::mAppendThread;

smrLogFileMgr::smrLogFileMgr() : idtBaseThread()
{

}

smrLogFileMgr::~smrLogFileMgr()
{

}

/* CREATE DB ����ÿ� ȣ��Ǹ�, 0��° �α������� �����Ѵ�.
 *
 * aLogPath - [IN] �α����ϵ��� ������ ���
 */
IDE_RC smrLogFileMgr::create( const SChar * aLogPath )
{
    smrLogFile     sLogFile;
    SChar          sLogFileName[SM_MAX_FILE_NAME];
    SChar        * sAlignedsLogFileInitBuffer = NULL;
    SChar        * sLogFileInitBuffer = NULL;
    const SChar ** sLogMultiplexPath;
    UInt           sLogMultiplexIdx;
    UInt           sMultiplexCnt;

    // BUG-14625 [WIN-ATAF] natc/TC/Server/sm4/sm4.ts��
    // �������� �ʽ��ϴ�.
    //
    // ���� Direct I/O�� �Ѵٸ� Log Buffer�� ���� �ּ� ����
    // Direct I/O Pageũ�⿡ �°� Align�� ���־�� �Ѵ�.
    // �̿� ����Ͽ� �α� ���� �Ҵ�� Direct I/O Page ũ�⸸ŭ
    // �� �Ҵ��Ѵ�.

    IDE_TEST( iduFile::allocBuff4DirectIO(
                  IDU_MEM_SM_SMR,
                  smuProperty::getFileInitBufferSize(),
                  (void**)&sLogFileInitBuffer,
                  (void**)&sAlignedsLogFileInitBuffer )
              != IDE_SUCCESS );

    //create log file 0
    idlOS::snprintf( sLogFileName, 
                     SM_MAX_FILE_NAME,
                     "%s%c%s0",
                     aLogPath,
                     IDL_FILE_SEPARATOR,
                     SMR_LOG_FILE_NAME );

    IDE_TEST( sLogFile.initialize() != IDE_SUCCESS );

    IDE_TEST_RAISE( idf::access(sLogFileName, F_OK) == 0,
                    error_already_exist_logfile );

    IDE_TEST( sLogFile.create( sLogFileName,
                               sAlignedsLogFileInitBuffer,
                               smuProperty::getLogFileSize() ) != IDE_SUCCESS );

    sLogMultiplexPath = smuProperty::getLogMultiplexDirPath();
    sMultiplexCnt     = smuProperty::getLogMultiplexCount();

    for( sLogMultiplexIdx = 0; 
         sLogMultiplexIdx < sMultiplexCnt;
         sLogMultiplexIdx++ )
    {
        idlOS::snprintf(sLogFileName, SM_MAX_FILE_NAME,
                        "%s%c%s0",
                        sLogMultiplexPath[sLogMultiplexIdx],
                        IDL_FILE_SEPARATOR,
                        SMR_LOG_FILE_NAME);

        IDE_TEST( sLogFile.create( sLogFileName,
                                   sAlignedsLogFileInitBuffer,
                                   smuProperty::getLogFileSize() ) 
                  != IDE_SUCCESS );
    }


    IDE_TEST( sLogFile.destroy() != IDE_SUCCESS );

    IDE_TEST( iduMemMgr::free(sLogFileInitBuffer) != IDE_SUCCESS );

    sLogFileInitBuffer = NULL;
    sAlignedsLogFileInitBuffer = NULL;

    return IDE_SUCCESS;

    IDE_EXCEPTION( error_already_exist_logfile )
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_AlreadyExistLogFile ) );
    }
    IDE_EXCEPTION_END;

    if ( sLogFileInitBuffer != NULL )
    {
        IDE_ASSERT( iduMemMgr::free(sLogFileInitBuffer) == IDE_SUCCESS );
        sLogFileInitBuffer = NULL;
        sAlignedsLogFileInitBuffer = NULL;
    }

    return IDE_FAILURE;

}
/***********************************************************************
 * Description : �α����� �����ڸ� �ʱ�ȭ �Ѵ�.
 *
 * aLogPath     - [IN] �αװ� ����� ����
 * aArchLogPath - [IN] ��ī�̺� �αװ� ����� ����
 * aLFThread    - [IN] �� �α����� �����ڿ� ���� �α����ϵ���
                       Flush���� �α����� Flush ������
 ***********************************************************************/
IDE_RC smrLogFileMgr::initialize( const SChar     * aLogPath,
                                  const SChar     * aArchivePath,
                                  smrLFThread     * aLFThread )
{

    UInt sPageSize;
    UInt sPageCount;
    UInt sBufferSize;

    IDE_DASSERT( aArchivePath != NULL);
    IDE_DASSERT( aLogPath  != NULL );
    IDE_DASSERT( aLFThread != NULL );
    
    mLFThread    = aLFThread;
    mSrcLogPath  = aLogPath;
    mArchivePath = aArchivePath; 
    
    mLogFileInitBufferPtr = NULL;
    mLogFileInitBuffer    = NULL;

    /* BUG-48409 prepare logfile�� ����� �ӽ� �α� ���ϸ���
     * ogfile Group ���� ���� ���ϸ��� �̸� �����д�.*/
    idlOS::snprintf( mTempLogFileName,
                     SM_MAX_FILE_NAME,
                     "%s%c%s%",
                     aLogPath,
                     IDL_FILE_SEPARATOR,
                     SMR_TEMP_LOG_FILE_NAME );

    IDE_TEST( smrLogMultiplexThread::initialize( &mSyncThread,
                                                 &mCreateThread,
                                                 &mDeleteThread,
                                                 &mAppendThread,
                                                 mSrcLogPath )
              != IDE_SUCCESS );

    
    IDE_TEST( mMutex.initialize((SChar*)"LOGFILE_MANAGER_MUTEX",
                               IDU_MUTEX_KIND_POSIX,
                               IDV_WAIT_INDEX_NULL ) != IDE_SUCCESS );
    IDE_TEST( mMtxList.initialize((SChar*)"OPEN_LOGFILE_LIST_MUTEX",
                                 IDU_MUTEX_KIND_POSIX,
                                 IDV_WAIT_INDEX_NULL ) != IDE_SUCCESS );
    IDE_TEST( mMemPool.initialize(IDU_MEM_SM_SMR,
                                 (SChar*)"OPEN_LOGFILE_MEM_LIST",
                                 1,                                 /* aListCount */
                                 ID_SIZEOF(smrLogFile),             /* aElemSize  */
                                 SMR_LOG_FULL_SIZE,                 /* aElemCount */
                                 IDU_AUTOFREE_CHUNK_LIMIT,			/* ChunkLimit */
                                 ID_TRUE,							/* UseMutex   */
                                 IDU_MEM_POOL_DEFAULT_ALIGN_SIZE,	/* AlignByte  */
                                 ID_FALSE,							/* ForcePooling */
                                 ID_TRUE,							/* GarbageCollection */
                                 ID_TRUE,                           /* HWCacheLine */
                                 IDU_MEMPOOL_TYPE_LEGACY            /* mempool type */) 
               != IDE_SUCCESS);			
    
    IDE_TEST_RAISE( mCV.initialize((SChar *)"LOGFILE_MANAGER_COND")
                    != IDE_SUCCESS, err_cond_var_init );

    sPageSize   = idlOS::getpagesize();
    sPageCount  = (smuProperty::getFileInitBufferSize() + sPageSize - 1) / sPageSize;
    sBufferSize = sPageCount * sPageSize;

    // BUG-14625 [WIN-ATAF] natc/TC/Server/sm4/sm4.ts��
    // �������� �ʽ��ϴ�.
    //
    // ���� Direct I/O�� �Ѵٸ� Log Buffer�� ���� �ּ� ����
    // Direct I/O Pageũ�⿡ �°� Align�� ���־�� �Ѵ�.
    // �̿� ����Ͽ� �α� ���� �Ҵ�� Direct I/O Page ũ�⸸ŭ
    // �� �Ҵ��Ѵ�.
    IDE_TEST( iduFile::allocBuff4DirectIO( IDU_MEM_SM_SMR,
                                           sBufferSize,
                                           (void**)&mLogFileInitBufferPtr,
                                           (void**)&mLogFileInitBuffer )
              != IDE_SUCCESS );

    mFstLogFile.mPrvLogFile = &mFstLogFile;
    mFstLogFile.mNxtLogFile = &mFstLogFile;

    mFinish = ID_FALSE;
    mResume = ID_FALSE;

    // ���� Open�� LogFile�� ����
    mLFOpenCnt = 0;

    // log switch �߻��� wait event�� �߻��� Ƚ�� 
    mLFPrepareWaitCnt = 0;

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_cond_var_init);
    {
        IDE_SET(ideSetErrorCode(smERR_FATAL_ThrCondInit));
    }
    IDE_EXCEPTION_END;
    
    if ( mLogFileInitBufferPtr != NULL )
    {
        IDE_ASSERT( iduMemMgr::free( mLogFileInitBufferPtr )
                    == IDE_SUCCESS );
        
        mLogFileInitBuffer = NULL;
        mLogFileInitBufferPtr = NULL;
    }
    
    return IDE_FAILURE;

}

IDE_RC smrLogFileMgr::destroy()
{
    
    smrLogFile *sCurLogFile;
    smrLogFile *sNxtLogFile;

    IDE_TEST( smrLogMultiplexThread::destroy( mSyncThread, 
                                              mCreateThread, 
                                              mDeleteThread, 
                                              mAppendThread )
              != IDE_SUCCESS );
    
    sCurLogFile = mFstLogFile.mNxtLogFile;
    
    while ( sCurLogFile != &mFstLogFile )
    {
        sNxtLogFile = sCurLogFile->mNxtLogFile;
       
        IDE_TEST( sCurLogFile->close() != IDE_SUCCESS );
        IDE_TEST( sCurLogFile->destroy() != IDE_SUCCESS );
        
        IDE_TEST( mMemPool.memfree(sCurLogFile) != IDE_SUCCESS );
        
        sCurLogFile = sNxtLogFile;
    }

    IDE_TEST( mMutex.destroy() != IDE_SUCCESS );
    IDE_TEST( mMtxList.destroy() != IDE_SUCCESS );
    IDE_TEST( mMemPool.destroy() != IDE_SUCCESS );

    IDE_ASSERT(mLogFileInitBuffer != NULL);

    IDE_TEST( iduMemMgr::free( mLogFileInitBufferPtr )
              != IDE_SUCCESS );
    
    mLogFileInitBuffer = NULL;
    mLogFileInitBufferPtr = NULL;

    IDE_TEST_RAISE( mCV.destroy() != IDE_SUCCESS, err_cond_var_destroy );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION(err_cond_var_destroy);
    {
        IDE_SET(ideSetErrorCode(smERR_FATAL_ThrCondDestroy));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/* ------------------------------------------------
 * Description :
 *
 * �ý��ۿ��� �αװ����� �ʱ�ȭ�ÿ� ȣ��Ǵ� �Լ��ν�
 * ����ϴ� �α����� �غ��ϰ�, �α����� ������ thread��
 * �����Ѵ�.
 *
 * - �α������� �������� ���� ���, �̸� Ư�� ����
 * (PREPARE_LOG_FILE_COUNT)��ŭ�� �� �α����ϵ��� �����Ͽ�
 * �����Ѵ�.
 * - ������ ����� �α������� �����Ѵ�.
 * - ���� �����ϰ� ������ �α������� �ƴϰ�, �׳� ������ �α������̸�
 * sync �α����� list�� ����Ѵ�.
 * - �̸� ������ �α������� �����Ͽ� �α����� list�� sync 
 * �α����� list ����Ѵ�.
 * - �α����ϰ����� thread�� �����Ѵ�.
 *
 * aEndLSN          [IN] - Redo�� �Ϸ�� ������ LSN (������ ����) Ȥ��
 *                         Loganchor�� ����� �������� ������ LSN (���� ����)
 * aLSTCreatedLF    [IN] - Redo�� �Ϸ�� ������ �α����� ��ȣ(����������) Ȥ��
 *                         Loganchor�� ����� ���� �ֽſ� ������ 
 *                         �α����Ϲ�ȣ(��������)
 * aRecovery        [IN] - Recover���� ����
 *
 * ----------------------------------------------*/
IDE_RC smrLogFileMgr::startAndWait( smLSN       * aEndLSN, 
                                    UInt          aLstCreatedLF,
                                    idBool        aRecovery,
                                    smrLogFile ** aLogFile )
{

    smrLogFile    * sLogFile          = NULL;
    smrLogFile    * sMultiplexLogFile = NULL;
    UInt            sLogMultiplexIdx;
    SChar           sLogFileName[ SM_MAX_FILE_NAME ];
    SInt            sState  = 0;

    setFileNo( aEndLSN->mFileNo, 
               aEndLSN->mFileNo + 1, 
               aLstCreatedLF );

    idlOS::snprintf( sLogFileName,
                     SM_MAX_FILE_NAME,
                     "%s%c%s%"ID_UINT32_FMT"", 
                     mSrcLogPath, 
                     IDL_FILE_SEPARATOR,
                     SMR_LOG_FILE_NAME, 
                     aEndLSN->mFileNo );

    /* -------------------------------------------------------
     * [1] If the last log file doesn't exist, 
     *     the log file must be created before any processing
     *
     * aEndLSN�� ���ϴ� �α������� �������� ���� ���,
     * �ش� log ������ ���� �������ش�.
     *
     * aEndLSN�� �ش��ϴ� �α������� ���� ����
     * �ش� �α������� flush�Ǳ� ���� ������ ������ ��� 
     * Ȥ�� ���� ���������� �α������� ������ ����̴�.
     ------------------------------------------------------- */
    if ( idf::access(sLogFileName, F_OK) != 0 )
    {
        mLstFileNo = mCurFileNo -1;

        /* �α������� �������ָ鼭 ������ �α������� open�Ѵ�. */
        IDE_TEST( addEmptyLogFile() != IDE_SUCCESS );
        sState = 1; 
    }
    else
    {
        /*  file exists */
    }

    /* -----------------------------------------------------
     * [2] open the last used log file 
     *
     * �α����� ����Ʈ�� �˻��Ͽ� �̹� �α������� open�Ǿ�������� 
     * open�����ʰ� �α����� ����Ʈ�� �����ϴ� �α������� �����´�.
     ----------------------------------------------------- */
    IDE_TEST( openLstLogFile( aEndLSN->mFileNo, 
                              aEndLSN->mOffset, 
                              &sLogFile) 
              != IDE_SUCCESS );

    *aLogFile = sLogFile;

    if ( smrLogMultiplexThread::mMultiplexCnt != 0 )
    {
        smrLogMultiplexThread::setOriginalCurLogFileNo( aEndLSN->mFileNo );

        for( sLogMultiplexIdx = 0; 
             sLogMultiplexIdx < smrLogMultiplexThread::mMultiplexCnt;
             sLogMultiplexIdx++ )
        { 
            /* ����ȭ �α������� ������ log file ���� log file�� invalid�� 
             * log file�� �����ϸ� �����Ѵ�.*/
            IDE_TEST( mCreateThread[sLogMultiplexIdx].recoverMultiplexLogFile( 
                                                                aEndLSN->mFileNo)
                      != IDE_SUCCESS );

            /* ������ ����ȭ log file�� open�Ѵ�. */
            IDE_TEST( mCreateThread[sLogMultiplexIdx].openLstLogFile( 
                                                  aEndLSN->mFileNo,
                                                  aEndLSN->mOffset,
                                                  sLogFile,
                                                  &sMultiplexLogFile )
                      != IDE_SUCCESS );

            mAppendThread[sLogMultiplexIdx].setCurLogFile( 
                                                    sMultiplexLogFile,
                                                    sLogFile );

            IDE_TEST( mAppendThread[sLogMultiplexIdx].startThread() 
                      != IDE_SUCCESS );
        }
    }
    else
    {
        /* nothing to do */
    }
    
    if ( sState == 0 )
    {
        // log flush �����忡�� flush(sync)��� �α����Ϸ� ����Ѵ�.
        IDE_TEST( mLFThread->addSyncLogFile(sLogFile) != IDE_SUCCESS );

        /* -----------------------------------------------------
         * [3] ������ prepare �� �ξ��� �α����ϵ��� ��� open�Ѵ�.
         *
         * BUG-35043 LOG_DIR�� �α����� ���ٸ�, ���� ���۽ÿ� ������ �α������� 
         * �� �� open�˴ϴ�. 
         * preOpenLogFile�Լ����� open�� �α������� �α����� ����Ʈ�� �����ϴ���
         * Ȯ������ �ʽ��ϴ�. �̷����� ���������� �α������� ���� ���¿���
         * ������ �����ϸ� [1]���� open�� prepare�α������� �ٽ��ѹ� open�Ǿ�
         * �α����ϸ���Ʈ�� ������ �α������� 2�� �����ϰԵ˴ϴ�.
         ----------------------------------------------------- */
        if ( ( mTargetFileNo <= mLstFileNo ) && 
             ( aRecovery == ID_FALSE ) ) 
        {
            IDE_TEST( preOpenLogFile() != IDE_SUCCESS );
        }
        else
        {
            /* nothing to do */
        }
    }

    /* ------------------------------------------------------------------------
     * [4] Log File Create Thread would be created 
     ---------------------------------------------------------------------- */
    IDE_TEST( start() != IDE_SUCCESS );
    IDE_TEST( waitToStart(0) != IDE_SUCCESS );

    while(1)
    {
        // smuProperty::getLogFilePrepareCount() ������ŭ
        // �α������� �̸� ������ �д�.
        if ( mTargetFileNo + smuProperty::getLogFilePrepareCount() <=
            mLstFileNo + 1)
        {
            break;
        }
        // log file prepare thread���� �α������� �ϳ� ���鵵�� ��û�ϰ�,
        // �α������� �ϳ� �� ������� ������ ��ٸ���.
        IDE_TEST( preCreateLogFile(ID_TRUE) != IDE_SUCCESS );
        idlOS::thr_yield();
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : aFileNo�� ����Ű�� LogFile(���� ������ ����)�� Open�Ѵ�.
 *               aLogFilePtr�� Open�� Logfile Pointer�� Setting���ش�.
 *
 * aFileNo     - [IN] open�� LogFile No
 * aOffset     - [IN] �αװ� ��ϵ� ��ġ 
 * aLogFilePtr - [OUT] open�� logfile�� ����Ų��.
 **********************************************************************/
IDE_RC smrLogFileMgr::openLstLogFile( UInt          aFileNo,
                                      UInt          aOffset,
                                      smrLogFile ** aLogFile )
{
    /* ---------------------------------------------
     * In server booting, mRestart set to ID_TRUE
     * apart from restart recovery.
     * In durability-1, only when server booting,
     * log file is read from disk.
     * --------------------------------------------- */
    IDE_TEST( open( aFileNo,
                    ID_TRUE /*For Write*/,
                    aLogFile ) != IDE_SUCCESS);

    (*aLogFile)->setPos(aFileNo, aOffset);

    /* PROJ-2162 RestartRiskReduction
     * DB�� Consistent���� ������, LogFile�� �������� �ʴ´�. */
    if( ( smrRecoveryMgr::getConsistency() == ID_TRUE ) ||
        ( smuProperty::getCrashTolerance() == 2 ) )
    {
        /* ������ Redo�� ��ġ ���ĸ� �� ��������.
         * ������ Inconsistency�ϱ� ������ ����� �ȵ� */
        (*aLogFile)->clear(aOffset);
    }
    else
    {
        /* nothing to do ... */
    }

    
    if ( smuProperty::getLogBufferType() == SMU_LOG_BUFFER_TYPE_MMAP )
    {
        (void)(*aLogFile)->touchMMapArea();
    }
    else
    {
        IDE_TEST( (*aLogFile)->readFromDisk( 0,
                                            (SChar*)((*aLogFile)->mBase),
                                            /* BUG-15532: ������ �÷������� DirectIO�� ����Ұ��
                                             * ���� ���۽��� IOũ�Ⱑ Sector Size�� Align�Ǿ�ߵ� */
                                            idlOS::align( aOffset, 
                                                          iduProperty::getDirectIOPageSize() ) )
                   != IDE_SUCCESS);
    }
    /*
     * To Fix BUG-11450  LOG_DIR, ARCHIVE_DIR �� ������Ƽ ������ ����Ǹ�
     *                   DB�� ����
     *
     * �α������� File Begin Log�� �������� üũ�Ѵ�.
     *
     * ����ڰ� ���� Shutdown�� �Ŀ� Log Directory��ġ���� ���� �ٲ�ĥ ��쿡
     * ����Ͽ� ���⿡�� Log File�� Ư�� FileNo�� �ش��ϴ�
     * �α��������� üũ�Ѵ�.
     */
    IDE_TEST( (*aLogFile)->checkFileBeginLog( aFileNo )
              != IDE_SUCCESS );

    // If the last log file is created in redoALL(),
    // the current m_cRef is 2 !!
    (*aLogFile)->mRef = 1;

    smrLogMgr::setLstLSN( aFileNo, aOffset );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* ������ prepare �� �ξ��� �α����ϵ��� ��� open�Ѵ�.
 *
 * �� �Լ������� log file list�� ���� ���ü��� ������ �ʿ䰡 ����.
 * �ֳ��ϸ�, log file list�� �����ϰ� �ִ� �ٸ� thread�� ���� ��Ȳ,
 * ��, smrLogFileMgr::startAndWait ������ ȣ��Ǳ� �����̴�.
 */
IDE_RC smrLogFileMgr::preOpenLogFile()
{
    SChar           sLogFileName[SM_MAX_FILE_NAME];
    smrLogFile    * sNewLogFile;
    UInt            sFstFileNo;
    UInt            sLstFileNo;
    UInt            sFileNo;
    smrLogFile    * sDummyLogFile;
    UInt            sLogMultiplexIdx;
    UInt            sState = 0;
    
    sFstFileNo = mCurFileNo + 1;  // prepare�� ù��° �α�����
    sLstFileNo = mLstFileNo;      // �� ������ �α�����

    for ( sFileNo=sFstFileNo ; sFileNo<=sLstFileNo ; sFileNo++ )
    {
        sState = 0;
        idlOS::snprintf(sLogFileName,
                        SM_MAX_FILE_NAME,
                        "%s%c%s%"ID_UINT32_FMT,
                        mSrcLogPath, 
                        IDL_FILE_SEPARATOR, 
                        SMR_LOG_FILE_NAME, 
                        sFileNo);

        /* ----------------------------------------------------------------- 
         * In case server boot 
         * if some prepared log file does not exist this is not fatal error, 
         * so the logfiles
         * which are between the next log file of not existing log file
         * and the last created log file will be newly created!!
         * ---------------------------------------------------------------- */
        if ( idf::access(sLogFileName, F_OK) != 0 )
        {
            setFileNo(mCurFileNo, 
                      sFileNo,
                      sFileNo - 1);
            break;
        }

        /* �̹� ��ϵ� ��� */
        if ( findLogFile( sFileNo, &sDummyLogFile ) == ID_TRUE )
        {
            /* nothing to do */
        }
        else
        {
            /* smrLogFileMgr_preOpenLogFile_alloc_NewLogFile.tc */
            IDU_FIT_POINT("smrLogFileMgr::preOpenLogFile::alloc::NewLogFile");
            IDE_TEST( mMemPool.alloc((void**)&sNewLogFile) != IDE_SUCCESS );
            sState = 1;
            IDE_TEST( sNewLogFile->initialize() != IDE_SUCCESS );
            sState = 2;
            
            IDE_TEST( sNewLogFile->open( sFileNo,
                                         sLogFileName,
                                         ID_FALSE, /* aIsMultiplexLogFile */
                                         (UInt)smuProperty::getLogFileSize(),
                                         ID_TRUE) != IDE_SUCCESS );
            sState = 3;
 
            //init log file info
            sNewLogFile->mFileNo   = sFileNo;
            sNewLogFile->mOffset   = 0;
            sNewLogFile->mFreeSize = smuProperty::getLogFileSize();
 
            (void)sNewLogFile->touchMMapArea();
            
            IDE_TEST( addLogFile(sNewLogFile) != IDE_SUCCESS );
            sState = 4;
        }
        
        for( sLogMultiplexIdx = 0; 
             sLogMultiplexIdx < smrLogMultiplexThread::mMultiplexCnt;
             sLogMultiplexIdx++ )
        {
            IDE_TEST( mCreateThread[sLogMultiplexIdx].preOpenLogFile( 
                                                sFileNo,
                                                mLogFileInitBuffer,
                                                smuProperty::getLogFileSize() )
                      != IDE_SUCCESS );
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch(sState)
    {
    case 4:
        (void)lockListMtx();
        (void)removeLogFileFromList(sNewLogFile);
        (void)unlockListMtx();

    case 3:
        (void)sNewLogFile->close();
        
    case 2:
        (void)sNewLogFile->destroy();
        
    case 1:
        (void)mMemPool.memfree(sNewLogFile); 
        break;
    }
    
    return IDE_FAILURE;
    
}

/* �α������� �����Ͽ� �ش� �α�������
 * ���µ� �α����� ����Ʈ�� ������ �����Ѵ�.
 *
 * �α����� �ϳ��� open�ϴ� ������ DISK I/O�� �����ϴ� �۾�����,
 * �α������� open �� �������� �α����� list Mutex��,
 * mMtxList �� ��� �ְ� �Ǹ� ���ü��� ������ �������� �ȴ�.
 *
 * �׷��� �α������� list�� ������ Ȯ���� �� ���ο� �α����ϰ�ü�� �ϳ� �����
 * �ش� �α����Ͽ� Mutex�� ���� ���·� �α����� ����Ʈ�� �߰��� ��,
 * �ٷ� �α����� list Mutex�� Ǯ���ش�.
 * �׸��� �α������� open�Ǿ� ��� �޸𸮷� �ö�� �Ŀ�,
 * �α������� Mutex�� Ǯ���ֵ��� �Ѵ�.
 *
 * �̷��� �Ǹ�, ���� open���� ���� �α������� ��ü�� �α����� list�� ��
 * ���� �� �ִ�.
 * �α����� list�ȿ� �ִ� �ϳ��� �α����Ͽ� ����
 * �α����� Mutex�� �ѹ� ��ƺ����� �Ѵ�.
 *
 * �̴�, �α������� open�ϴ� �����尡 �ش� �α������� open���ΰ��,
 * �α������� open�� �Ϸ�� ������ ��ٸ����� �Ѵ�.
 *
 * aFileNo  - [IN] open�� �α����� ��ȣ
 * aWrite   - [IN] ������� open���� ����
 * aLogFile - [OUT] open�� �α������� ��ü
 *
 */
IDE_RC smrLogFileMgr::open( UInt           aFileNo,
                            idBool         aWrite,
                            smrLogFile  ** aLogFile )
{

    smrLogFile* sNewLogFile;
    smrLogFile* sPrvLogFile;
    SChar       sLogFileName[SM_MAX_FILE_NAME];

    *aLogFile = NULL;

    IDE_ASSERT( lockListMtx() == IDE_SUCCESS );

    // aFileNo���� �α������� open�� �α����� list�� �����ϴ� ���
    if ( findLogFile(aFileNo, &sPrvLogFile) == ID_TRUE)
    {
        sPrvLogFile->mRef++;
        *aLogFile = sPrvLogFile;

        IDE_ASSERT( unlockListMtx() == IDE_SUCCESS );
        
        /* ���� open���� ���� �α������� ��ü�� �α����� list�� ��
         * ���� �� �ִ�.
         * �α����� list�ȿ� �ִ� �ϳ��� �α����Ͽ� ����
         * �α����� Mutex�� �ѹ� ��ƺ����� �Ѵ�.
         *
         * �̸� ���� �α������� open�ϴ� �����尡 �ش� �α������� open���ΰ��,
         * �α������� open�� �Ϸ�� ������ ��ٸ����� �Ѵ�.
         */
        IDE_ASSERT( (*aLogFile)->lock() == IDE_SUCCESS );
        IDE_TEST_RAISE( (*aLogFile)->isOpened() == ID_FALSE,
                        err_wait_open_file);
        IDE_ASSERT( (*aLogFile)->unlock() == IDE_SUCCESS );
    }
    // aFileNo���� �α������� open�� �α����� list�� �������� �ʴ� ���
    // sPrvLogFile �� aFileNo�α� ������ �ٷ� �� �α������̴�.
    else
    {
        /* BUG-36744
         * ���ο� smrLogFile ��ü�� ������ �Ѵ�.
         * �ϴ� unlockListMtx()�� �ϰ�, ��ü�� �����Ѵ�. */
        IDE_ASSERT( unlockListMtx() == IDE_SUCCESS );

        if ( ( smrRecoveryMgr::getArchiveMode() == SMI_LOG_ARCHIVE ) &&
             ( smrRecoveryMgr::getLstDeleteLogFileNo() > aFileNo ) )
        {

            // ���� ������ ������ ���� 4.3.7�� �����ϰ�
            // ASSERT�ڵ带 �ֽ��ϴ�.
            //
            // BUG-17541 [4.3.7] ��� �α����� Open�ϴٰ�
            // �̹� ������ �α������̾ ASSERT�°� ���
            IDE_ASSERT( smiGetStartupPhase() == SMI_STARTUP_CONTROL );
            
            idlOS::snprintf( sLogFileName,
                             SM_MAX_FILE_NAME,
                             "%s%c%s%"ID_UINT32_FMT,
                             mArchivePath,
                             IDL_FILE_SEPARATOR, 
                             SMR_LOG_FILE_NAME, 
                             aFileNo );
        }
        else
        {
            idlOS::snprintf(sLogFileName,
                            SM_MAX_FILE_NAME,
                            "%s%c%s%"ID_UINT32_FMT, 
                            mSrcLogPath, 
                            IDL_FILE_SEPARATOR, 
                            SMR_LOG_FILE_NAME, 
                            aFileNo);
        }

        IDE_ASSERT(mMemPool.alloc((void**)&sNewLogFile)
                   == IDE_SUCCESS );
        
        new (sNewLogFile) smrLogFile();
 
        IDE_ASSERT( sNewLogFile->initialize() == IDE_SUCCESS );
        
        sNewLogFile->mFileNo             = aFileNo;

        IDE_ASSERT( lockListMtx() == IDE_SUCCESS );

        /* List�� �����ϴ��� �ٽ� �ѹ� Ȯ���Ѵ�. */
        if ( findLogFile(aFileNo, &sPrvLogFile) == ID_TRUE)
        {
            sPrvLogFile->mRef++;
            *aLogFile = sPrvLogFile;

            IDE_ASSERT( unlockListMtx() == IDE_SUCCESS );

            IDE_ASSERT( sNewLogFile->destroy() == IDE_SUCCESS );
            IDE_ASSERT( mMemPool.memfree(sNewLogFile)
                        == IDE_SUCCESS );

            IDE_ASSERT( (*aLogFile)->lock() == IDE_SUCCESS );
            IDE_TEST_RAISE( (*aLogFile)->isOpened() == ID_FALSE,
                            err_wait_open_file);
            IDE_ASSERT( (*aLogFile)->unlock() == IDE_SUCCESS );
        }
        else
        {
            *aLogFile = sNewLogFile;

            // Add log file to log file list
            (void)AddLogFileToList(sPrvLogFile, sNewLogFile); 

            // �α����� list�� Mutex�� Ǯ�� �α������� open�ϴ� �۾���
            // ��� �����ϱ� ���� �ش� �α����Ͽ� Mutex�� ��´�.
            IDE_ASSERT( sNewLogFile->lock() == IDE_SUCCESS );

            sNewLogFile->mRef++;

            // �α������� open�ϱ� ���� �α����� list�� Mutex�� Ǯ���ش�.
            IDE_ASSERT( unlockListMtx() == IDE_SUCCESS );

            IDE_TEST( sNewLogFile->open( aFileNo,
                                         sLogFileName,
                                         ID_FALSE, /* aIsMultiplexLogFile */
                                         smuProperty::getLogFileSize(),
                                         aWrite ) != IDE_SUCCESS );

            IDE_ASSERT( sNewLogFile->unlock() == IDE_SUCCESS );
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_wait_open_file);
    {
        errno = 0;
        IDE_SET( ideSetErrorCode( smERR_ABORT_WaitLogFileOpen,
                                 (*aLogFile)->getFileName() ) );
    }
    IDE_EXCEPTION_END;

    IDE_ASSERT( lockListMtx() == IDE_SUCCESS );

    (*aLogFile)->mRef--;

    if ( (*aLogFile)->mRef == 0 )
    {
        removeLogFileFromList(*aLogFile);
        /* open�� �����ϴ� Thread�� open�� �������� OpenFileList����
           ��ü�� �����ϰ� ����. ������ Resource�� ���� ������ �ʿ���.*/
        IDE_ASSERT( (*aLogFile)->unlock() == IDE_SUCCESS );
        IDE_ASSERT( (*aLogFile)->destroy() == IDE_SUCCESS );
        IDE_ASSERT( mMemPool.memfree((*aLogFile)) == IDE_SUCCESS );
    }
    else
    {
        IDE_ASSERT( (*aLogFile)->unlock() == IDE_SUCCESS );
    }

    IDE_ASSERT( unlockListMtx() == IDE_SUCCESS );
    
    *aLogFile = NULL;
    
    return IDE_FAILURE;

}

/* �α������� �����ϴ� �����尡 �� �̻� ���� ���,
 * �ش� �α������� close�ϰ� �α����� list���� close�� �α������� �����Ѵ�.
 *
 * aLogFile - [IN] close�� �α�����
 */ 
IDE_RC smrLogFileMgr::close(smrLogFile *aLogFile)
{

    UInt sState = 0;
    
    if ( aLogFile != NULL )
    {
        IDE_TEST( lockListMtx() != IDE_SUCCESS );
        sState = 1;

        IDE_ASSERT( aLogFile->mRef != 0 );
        
        aLogFile->mRef--;

        // �� �̻� �α������� �����ϴ� �����尡 ���� ���
        if ( aLogFile->mRef == 0 )
        {
            //remove log file from log file list
            removeLogFileFromList(aLogFile);

            sState = 0;

            /* BUG-17254 DEC������ log file�� ���� �� ���� open�� ������������ ���Ѵ�.
               ���� log file�� list���� ���� mutex�� Ǯ�� ����
               close�� �����ν� �ٸ� �����尡 mutex�� ��� list�� ���� ��
               ����Ʈ�� �� ������ ������ close()�Ǿ����� ������ �� �ִ�. */
#if defined(DEC_TRU64)
            IDE_TEST( aLogFile->close() != IDE_SUCCESS );

            IDE_TEST( unlockListMtx() != IDE_SUCCESS );

            /* FIT/ART/rp/Bugs/BUG-17254/BUG-17254.tc */
            IDU_FIT_POINT( "1.BUG-17254@smrLogFileMgr::close" );
#else
            IDE_TEST( unlockListMtx() != IDE_SUCCESS );

            /* FIT/ART/rp/Bugs/BUG-17254/BUG-17254.tc */
            IDU_FIT_POINT( "1.BUG-17254@smrLogFileMgr::close" );

            IDE_TEST( aLogFile->close() != IDE_SUCCESS );
#endif
            IDE_TEST( aLogFile->destroy() != IDE_SUCCESS );

            // mMemPool�� thread safe�� ��ü��, ���ü� ��� ���ʿ��ϴ�.
            IDE_TEST( mMemPool.memfree(aLogFile) != IDE_SUCCESS );
        }
        else // ���� �α������� �����ϴ� �����尡 �ϳ��� �ִ� ���
        {
            sState = 0;
            IDE_TEST( unlockListMtx() != IDE_SUCCESS );
        }
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sState != 0 )
    {
        (void)unlockListMtx();
    }

    return IDE_FAILURE;

}

/* log file prepare �������� run�Լ�.
 *
 * �α����� �ϳ��� �� ����ؼ� ���� �α� ���Ϸ� switch�� ����� �����
 * �ּ�ȭ �ϱ� ���ؼ� ������ ����� �α����ϵ��� �̸� �غ��� ���´�.
 *
 * �α������� �غ��ϴ� ���� ������ ���� �ΰ��� ��쿡 �ǽ��Ѵ�.
 *   1. Ư�� �ð����� ��ٷ����� �ֱ������� �ǽ�
 *   2. preCreateLogFile �Լ� ȣ�⿡ ���� 
 */
void smrLogFileMgr::run()
{
    UInt              sState = 0;
    PDL_Time_Value    sTV;

  startPos:
    sState=0;

    // �����带 �����ؾ� �ϴ� ��찡 �ƴ� ����...
    while(mFinish == ID_FALSE)
    {
        IDE_TEST( lock() != IDE_SUCCESS );
        sState = 1;

        IDE_TEST_RAISE(mCV.broadcast() != IDE_SUCCESS, err_cond_signal);
        mResume = ID_FALSE;

        while(mResume != ID_TRUE)
        {
            sTV.set(idlOS::time(NULL) + smuProperty::getLogFilePreCreateInterval());

            sState = 0;
            IDE_TEST_RAISE(mCV.timedwait(&mMutex, &sTV, IDU_IGNORE_TIMEDOUT)
                           != IDE_SUCCESS, err_cond_wait);
            mResume = ID_TRUE;
        }

        if ( mFinish == ID_TRUE )
        {
            sState = 0;
            IDE_TEST( unlock() != IDE_SUCCESS );

            break;
        }

        if ( smuProperty::isRunLogPrepareThread() == SMU_THREAD_OFF )
        {
            // To Fix PR-14783
            // System Thread�� �۾��� �������� �ʵ��� �Ѵ�.
            continue;
        }
        else
        {
            // Go Go
        }

        sState = 0;
        IDE_TEST( unlock() != IDE_SUCCESS );

        IDE_TEST( addEmptyLogFile() != IDE_SUCCESS );
    }

    return ;

    IDE_EXCEPTION(err_cond_signal);
    {
        IDE_SET(ideSetErrorCode(smERR_FATAL_ThrCondSignal));
    }
    IDE_EXCEPTION(err_cond_wait);
    {
        IDE_SET(ideSetErrorCode(smERR_FATAL_ThrCondWait));
    }
    IDE_EXCEPTION_END;

    if ( sState != 0 )
    {
        (void)unlock();
    }

    ideLog::log(SM_TRC_LOG_LEVEL_WARNNING,
                SM_TRC_MRECOVERY_LOGFILEMGR_INSUFFICIENT_MEMORY);

    goto startPos;

}

/* ------------------------------------------------
 * Description :
 *
 * smuProperty::getLogFilePrepareCount()�� �����ϵ��� �α�������
 * �̸� �����Ѵ�.
 * ----------------------------------------------*/
IDE_RC smrLogFileMgr::addEmptyLogFile()
{

    SChar           sLogFileName[SM_MAX_FILE_NAME];
    smrLogFile*     sNewLogFile;
    UInt            sLstFileNo  = 0;
    UInt            sTargetFileNo;
    UInt            sState      = 0;
    
    /* BUG-39764 : Log file add ���� */
    idBool          sLogFileAdd = ID_FALSE;    

    sTargetFileNo = mTargetFileNo;

    smrLogMultiplexThread::mIsOriginalLogFileCreateDone = ID_FALSE;
    
    IDE_TEST( smrLogMultiplexThread::wakeUpCreateThread( 
                                            mCreateThread,
                                            &sTargetFileNo,
                                            mLstFileNo,
                                            mLogFileInitBuffer,
                                            smuProperty::getLogFileSize() )
              != IDE_SUCCESS );


    // ���� �α������� smuProperty::getLogFilePrepareCount() ��ŭ
    // �̸� �غ���� ���� ��쿡 ���ؼ�
    while ( sTargetFileNo + smuProperty::getLogFilePrepareCount() >
            ( mLstFileNo + 1 ) )
    {
        sLstFileNo = mLstFileNo + 1;

        if ( sLstFileNo == ID_UINT_MAX )
        {
            sLstFileNo = 0;
        }

        idlOS::snprintf(sLogFileName,
                        SM_MAX_FILE_NAME,
                        "%s%c%s%"ID_UINT32_FMT,
                        mSrcLogPath,
                        IDL_FILE_SEPARATOR,
                        SMR_LOG_FILE_NAME,
                        sLstFileNo);

        /* smrLogFileMgr_addEmptyLogFile_alloc_NewLogFile.tc */
        IDU_FIT_POINT("smrLogFileMgr::addEmptyLogFile::alloc::NewLogFile");
        IDE_TEST( mMemPool.alloc((void**)&sNewLogFile) != IDE_SUCCESS );
        sState = 1;
        IDE_TEST( sNewLogFile->initialize() != IDE_SUCCESS );
        sState = 2;

        /* BUG-48409 temp ������ �̿��� logfile ���� ��� ���� */
        if ( smuProperty::getUseTempForPrepareLogFile() == ID_TRUE )
        {
            IDE_TEST( sNewLogFile->prepare( sLogFileName,
                                            mTempLogFileName,
                                            mLogFileInitBuffer,
                                            smuProperty::getLogFileSize() )
                      != IDE_SUCCESS );
        }
        else
        {
            IDE_TEST( sNewLogFile->create( sLogFileName,
                                           mLogFileInitBuffer,
                                           smuProperty::getLogFileSize() )
                      != IDE_SUCCESS );
        }

        IDE_TEST( sNewLogFile->open( sLstFileNo,
                                     sLogFileName,
                                     ID_FALSE, /* aIsMultiplexLogFile */
                                     smuProperty::getLogFileSize(),
                                     ID_TRUE) != IDE_SUCCESS );

        //init log file info
        sNewLogFile->mFileNo   = sLstFileNo;
        sNewLogFile->mOffset   = 0;
        sNewLogFile->mFreeSize = smuProperty::getLogFileSize();

        (void)sNewLogFile->touchMMapArea();

        // �α����� ����Ʈ�� ���� ���ü� ����� �� �Լ� �ȿ��� �Ѵ�.
        IDE_TEST( addLogFile( sNewLogFile ) != IDE_SUCCESS );
        sState = 3;

        IDE_TEST( lock() != IDE_SUCCESS );
        sState = 4;

        mLstFileNo = sLstFileNo;

        // �α����� ������ �Ϸ�Ǿ����� �ٸ� ������鿡�� �˸���.
        IDE_TEST_RAISE(mCV.broadcast() != IDE_SUCCESS, err_cond_signal);

        sState = 3;
        IDE_TEST( unlock() != IDE_SUCCESS );
        
        sTargetFileNo = mTargetFileNo;

        sLogFileAdd = ID_TRUE;
    }

    smrLogMultiplexThread::mOriginalLstLogFileNo        = mLstFileNo;
    smrLogMultiplexThread::mIsOriginalLogFileCreateDone = ID_TRUE;

    IDE_TEST( smrLogMultiplexThread::wait( mCreateThread ) != IDE_SUCCESS );

    /* BUG-39764 : ���ο� �α� ���� ������ �Ϸ�Ǹ� �α׾�Ŀ�� 
     * "���������� ������ �α� ���� ��ȣ (Last Created Logfile Num)" �׸��� 
     * ������Ʈ�ϰ� �α׾�Ŀ�� ���Ͽ� �÷����Ѵ�.  */
    if ( sLogFileAdd == ID_TRUE )
    {
        IDE_TEST( smrRecoveryMgr::getLogAnchorMgr()->updateLastCreatedLogFileNumAndFlush( sLstFileNo ) 
                  != IDE_SUCCESS );
    }
    else
    {
        /* do nothing */
    }
     

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_cond_signal);
    {
        IDE_SET(ideSetErrorCode(smERR_FATAL_ThrCondSignal));
    }
    IDE_EXCEPTION_END;

    switch (sState)
    {
    case 4:
        (void)unlock();

    case 3:
        (void)lockListMtx();
        (void)removeLogFileFromList(sNewLogFile);
        (void)unlockListMtx();

    case 2:
        (void)sNewLogFile->destroy();

    case 1:
        (void)mMemPool.memfree(sNewLogFile);
        break;
    }

    return IDE_FAILURE;
}

/* log file prepare �����带 ������ �α������� �̸� ������ �д�.
 *
 * aWait => ID_TRUE �̸� �ش� �α������� ������ ������ ��ٸ���.
 *          ID_FALSE �̸� �α������� ������ ��û�� �ϰ� �ٷ� �����Ѵ�.
 */
IDE_RC smrLogFileMgr::preCreateLogFile(idBool aWait)
{

    UInt sState = 0;

    if ( aWait == ID_TRUE )
    {
        IDE_TEST( lock() != IDE_SUCCESS );
        sState = 1;
    
        if ( mFinish == ID_FALSE )
        {
            if ( mResume == ID_TRUE ) 
            {
                sState = 0;
                IDE_TEST_RAISE(mCV.wait(&mMutex) != IDE_SUCCESS, err_cond_wait);
                sState = 1;
            }

            mResume = ID_TRUE;
            
            IDE_TEST_RAISE(mCV.broadcast() != IDE_SUCCESS, err_cond_signal);
        }

        sState = 0;
        IDE_TEST( unlock() != IDE_SUCCESS );
    }
    else
    {
        if ( mResume != ID_TRUE )
        {
            mResume = ID_TRUE;
            IDE_TEST_RAISE(mCV.broadcast() != IDE_SUCCESS, err_cond_signal);
        }
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION(err_cond_wait);
    {
        IDE_SET(ideSetErrorCode(smERR_FATAL_ThrCondWait));
    }
    IDE_EXCEPTION(err_cond_signal);
    {
        IDE_SET(ideSetErrorCode(smERR_FATAL_ThrCondSignal));
    }
    IDE_EXCEPTION_END;

    if ( sState != 0 )
    {
        (void)unlock();
    }
    
    return IDE_FAILURE;

}

/* �α����� prepare �����带 �����Ѵ�.
 */
IDE_RC smrLogFileMgr::shutdown()
{

    UInt         sState = 0;

    IDE_TEST( lock() != IDE_SUCCESS );
    sState = 1;

    // �α����� prepare�����尡 ���� �α������� �����ϰ� �ִ� ���̸�

    /* BUG-44834 Ư�� ��񿡼� sprious wakeup ������ �߻��ϹǷ� 
                 wakeup �Ŀ��� �ٽ� Ȯ�� �ϵ��� while������ üũ�Ѵ�.*/
    while ( mResume == ID_TRUE ) 
    {
        // �ش� �α����� ������ �Ϸ�ɶ����� ����Ѵ�.
        sState = 0;
        IDE_TEST_RAISE(mCV.wait(&mMutex) != IDE_SUCCESS, err_cond_wait);
        sState = 1;
    }

    // �����尡 ����ǵ��� ������ ����� ����
    mFinish = ID_TRUE;
    mResume = ID_TRUE;

    // sleep������ �����带 ������ mFinish == ID_TRUE���� üũ�ϰ�
    // �����尡 ����ǵ��� �Ѵ�.
    IDE_TEST_RAISE(mCV.broadcast() != IDE_SUCCESS, err_cond_signal);
    
    sState = 0;
    IDE_TEST( unlock() != IDE_SUCCESS );

    // �����尡 ������ ����� ������ ��ٸ���.
    IDE_TEST_RAISE(join() != IDE_SUCCESS, err_thr_join);

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_cond_wait);
    {
        IDE_SET(ideSetErrorCode(smERR_FATAL_ThrCondWait));
    }
    IDE_EXCEPTION(err_thr_join);
    {
        IDE_SET(ideSetErrorCode(smERR_FATAL_Systhrjoin));
    }
    IDE_EXCEPTION(err_cond_signal);
    {
        IDE_SET(ideSetErrorCode(smERR_FATAL_ThrCondSignal));    
    }
    IDE_EXCEPTION_END;

    if ( sState != 0 )
    {
        (void)unlock();
    }

    return IDE_FAILURE;
}

/* ���� ������� �α������� ���� �α������� ���� ����� �α����Ϸ� �����Ѵ�.
 * switch�� �α������� �������� ���� ���, �α������� ���� �����Ѵ�.
 */
IDE_RC smrLogFileMgr::switchLogFile( smrLogFile **aCurLogFile )
{
    smrLogFile*  sPrvLogFile;
    
    mCurFileNo++;

    sPrvLogFile = (*aCurLogFile);

    mTargetFileNo = mCurFileNo + 1;
        
    while(1)
    {
        // Switch�� �α������� �����ϴ� ���
        if ( mCurFileNo <= mLstFileNo )
        {
            // prepare�� �α����� �ϳ��� ����� ���̹Ƿ�,
            // �α����� prepare �����带 ������
            // �α����� �ϳ��� ���� �����ϵ��� �����Ѵ�.
            // �α������� �����ɶ����� ��ٸ��� �ʴ´�.
            IDE_TEST( preCreateLogFile(ID_FALSE) != IDE_SUCCESS );

            *aCurLogFile = (*aCurLogFile)->mNxtLogFile;
            
            IDE_ASSERT((*aCurLogFile)->mFileNo == mCurFileNo);
            
            (*aCurLogFile)->mRef++;

            sPrvLogFile->mSwitch = ID_TRUE;

            break;
        }
        else
        {
            // �α������� �����Ѵ�.
            // �α������� ������ ������ ��ٸ���.
            mLFPrepareWaitCnt++;
            IDE_TEST( preCreateLogFile(ID_TRUE) != IDE_SUCCESS );
        }
        
        idlOS::thr_yield();
    }
    
    /* PROJ-2232 log multiplex
     * ����ȭ �����忡 ���� switch�Ǿ� �αװ� ���� logfile��ȣ�� �����Ѵ�.*/
    smrLogMultiplexThread::setOriginalCurLogFileNo( mCurFileNo );

    IDU_FIT_POINT( "1.BUG-38801@smrLogFileMgr::switchLogFile" );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/* �α����� ����Ʈ���� �α������� ã�´�.
 * aLogFileNo - [IN] ã�� �α�����
 *
 * Return�� - ID_TRUE : aLogFileNo�� ��ġ�ϴ� �α������� ã�Ҵ�.
 *                      *aLogFile �� ã�� �α����� ��ü�� ����
 *            ID_FALSE : aLogFileNo�� ��ġ�ϴ� �α������� ã�� ���ߴ�.
 *                       *aLogFile�� �����Ǵ� �α����� ��ü �ٷ� ������
 *                       (���� �ʿ��ϴٸ�)aLogFileNo�� �ش��ϴ�
 *                       �α������� �����ϸ� �ȴ�.
 */
idBool smrLogFileMgr::findLogFile(UInt           aLogFileNo, 
                                  smrLogFile**   aLogFile)
{

    smrLogFile*   sCurLogFile;

    IDE_ASSERT(aLogFile != NULL);

    *aLogFile = NULL;
    
    sCurLogFile = mFstLogFile.mNxtLogFile;
    
    while(sCurLogFile != &mFstLogFile)
    {
        // ��Ȯ�� ��ġ�ϴ� �α����� �߰�
        if ( sCurLogFile->mFileNo == aLogFileNo )
        {
            *aLogFile = sCurLogFile;
            
            return ID_TRUE;
        }
        // �α����� ����Ʈ�� �α����� ��ȣ������ ���ĵǾ��ִ�.
        // �α����� ��ȣ�� ã���� �ϴ°ͺ��� ũ�ٸ� �α������� ã�� ���� ����
        else if ( sCurLogFile->mFileNo > aLogFileNo )
        {
            // aLogFileNo���� ���� �α����� ��ȣ�� ������
            // �α����� ��ü�� ���� ���������� ����.
            *aLogFile = sCurLogFile->mPrvLogFile;
            break;
        } 

        sCurLogFile = sCurLogFile->mNxtLogFile;
    }

    // aLogFileNo���� �α������� �������� ���� ���� ���
    if ( *aLogFile == NULL )
    {
        // �α����� ����Ʈ�� �� ������ �α������� ����.
        *aLogFile = sCurLogFile->mPrvLogFile; 
    }
    
    return ID_FALSE;
    
}

/*
 * �α����� ��ü�� �α����� ����Ʈ�� �߰��Ѵ�.
 */
IDE_RC smrLogFileMgr::addLogFile( smrLogFile *aNewLogFile )
{

    smrLogFile*  sPrvLogFile;
    UInt         sState = 0;

    IDE_TEST( lockListMtx() != IDE_SUCCESS );
    sState = 1;

    // �α������� ������ �α������� ã�Ƴ���.
#ifdef DEBUG
    IDE_ASSERT( findLogFile( aNewLogFile->mFileNo, &sPrvLogFile )
                == ID_FALSE );
#else
    (void)findLogFile( aNewLogFile->mFileNo, &sPrvLogFile );
#endif

    // sPrevLogFile �� next�� aNewLogFile�� �Ŵܴ�.
    AddLogFileToList( sPrvLogFile, aNewLogFile ); //add log file to list

    sState = 0;
    IDE_TEST( unlockListMtx() != IDE_SUCCESS );

    // Sync�� �α����Ϸ� ����Ѵ�.
    IDE_TEST( mLFThread->addSyncLogFile( aNewLogFile )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sState != 0 )
    {
        IDE_PUSH();
        IDE_ASSERT( unlockListMtx() == IDE_SUCCESS );
        IDE_POP();
    }

    return IDE_FAILURE;
}

/* �α����ϵ��� DISK���� �����Ѵ�.
 *
 * smrLogFile::remove �� 
 * checkpoint���� �α����� ������ �����ϸ� ������Ȳ���� ó���ϰ�,
 * restart recovery�ÿ� �α����� ������ �����ϸ� �����Ȳ���� ó���Ѵ�.
 */
void smrLogFileMgr::removeLogFile( UInt     aFstFileNo, 
                                   UInt     aLstFileNo, 
                                   idBool   aIsCheckPoint)
{
    smrLogMultiplexThread::wakeUpDeleteThread( mDeleteThread,
                                               aFstFileNo,
                                               aLstFileNo,
                                               aIsCheckPoint );

    removeLogFiles( aFstFileNo, 
                    aLstFileNo, 
                    aIsCheckPoint, 
                    ID_FALSE, 
                    mSrcLogPath );

    (void)smrLogMultiplexThread::wait( mDeleteThread );
}

/* �α����� ��ȣ�� �ش��ϴ� �α� �����Ͱ� �ִ��� üũ�Ѵ�.
 */
IDE_RC smrLogFileMgr::checkLogFile(UInt aFileNo)
{

    iduFile         sFile;
    SChar           sLogFileName[SM_MAX_FILE_NAME];

    // �α׸� ��ũ�� ���Ͽ� ����ϴ� ���
    // �α������� �ѹ� �����.
    idlOS::snprintf(sLogFileName,
                    SM_MAX_FILE_NAME,
                    "%s%c%s%"ID_UINT32_FMT, 
                    mSrcLogPath,
                    IDL_FILE_SEPARATOR, 
                    SMR_LOG_FILE_NAME, 
                    aFileNo);
    IDE_TEST( sFile.initialize( IDU_MEM_SM_SMR,
                                1, /* Max Open FD Count */
                                IDU_FIO_STAT_OFF,
                                IDV_WAIT_INDEX_NULL ) != IDE_SUCCESS );
    
    IDE_TEST( sFile.setFileName(sLogFileName) != IDE_SUCCESS );
    IDE_TEST( sFile.open() != IDE_SUCCESS );
    IDE_TEST( sFile.close() != IDE_SUCCESS );
    IDE_TEST( sFile.destroy() != IDE_SUCCESS );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
    
}

/* ���� �α����� ����Ʈ�� �ִ� ��� �α������� close�ϰ�
 * �α����� ����Ʈ���� �����Ѵ�.
 */
IDE_RC smrLogFileMgr::closeAllLogFile()
{

    smrLogFile*     sCurLogFile;
    smrLogFile*     sNxtLogFile;

    sCurLogFile = mFstLogFile.mNxtLogFile;
    while(sCurLogFile != &mFstLogFile)
    {
        sNxtLogFile = sCurLogFile->mNxtLogFile;

        sCurLogFile->mRef = 1;
        IDE_TEST( close(sCurLogFile) != IDE_SUCCESS );

        sCurLogFile = sNxtLogFile;
    }

    IDE_ASSERT(&mFstLogFile == mFstLogFile.mNxtLogFile);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

IDE_RC smrLogFileMgr::AddLogFileToList(smrLogFile*  aPrvLogFile,
                                       smrLogFile*  aNewLogFile)
{

    smrLogFile *sNxtLogFile;

    mLFOpenCnt++;
    
    sNxtLogFile = aPrvLogFile->mNxtLogFile;

    aPrvLogFile->mNxtLogFile = aNewLogFile;
    sNxtLogFile->mPrvLogFile = aNewLogFile;

    aNewLogFile->mNxtLogFile = sNxtLogFile;
    aNewLogFile->mPrvLogFile = aPrvLogFile;

    return IDE_SUCCESS;
}

#if 0 // not used
/***********************************************************************
 * Description : aFileNo�� �ش��ϴ� Logfile�� �����ϴ��� Check�Ѵ�.
 *
 * aFileNo   - [IN]  Log File Number
 * aIsExist  - [OUT] aFileNo�� �ش��ϴ� File�� �ִ� ID_TRUE, �ƴϸ�
 *                   ID_FALSE
*/ 
IDE_RC smrLogFileMgr::isLogFileExist(UInt aFileNo, idBool & aIsExist)
{
    SChar    sLogFilename[SM_MAX_FILE_NAME];
    iduFile  sFile;
    ULong    sLogFileSize = 0;
    
    aIsExist = ID_TRUE;
    
    idlOS::snprintf(sLogFilename,
                    SM_MAX_FILE_NAME,
                    "%s%c%s%"ID_UINT32_FMT,
                    mSrcLogPath,
                    IDL_FILE_SEPARATOR,
                    SMR_LOG_FILE_NAME,
                    aFileNo);
    
    if ( idf::access(sLogFilename, F_OK) != 0 )
    {
        aIsExist = ID_FALSE;
    }
    else
    {
        IDE_TEST( sFile.initialize( IDU_MEM_SM_SMR,
                                    1, /* Max Open FD Count */
                                    IDU_FIO_STAT_OFF,
                                    IDV_WAIT_INDEX_NULL ) 
                 != IDE_SUCCESS );
        
        IDE_TEST( sFile.setFileName(sLogFilename) != IDE_SUCCESS );

        IDE_TEST( sFile.open() != IDE_SUCCESS );
        IDE_TEST( sFile.getFileSize(&sLogFileSize) != IDE_SUCCESS );
        IDE_TEST( sFile.close() != IDE_SUCCESS );
        IDE_TEST( sFile.destroy() != IDE_SUCCESS );
        
        if ( sLogFileSize != smuProperty::getLogFileSize() )
        {
            aIsExist = ID_FALSE;
        }
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    /* ��� Error�� Fatal�� */
    return IDE_FAILURE;
}
#endif

/***********************************************************************
 * Description : aFileNo�� �ش��ϴ� Logfile�� Open�Ǿ����� check�ϰ� 
 *               Open�Ǿ��ִٸ� LogFile�� Reference Count�� ������Ų��.
 *
 * aFileNo      - [IN]  Check�� LogFile Number
 * aAlreadyOpen - [OUT] aFileNo�� �ش��ϴ� LogFile�� �����Ѵٸ� ID_TRUE, �ƴϸ�
 *                      ID_FALSE.
 * aLogFile     - [OUT] aFileNo�� �ش��ϴ� LogFile�� �����Ѵٸ� aLogFile��
 *                      smrLogFile Ponter�� �Ѱ��ְ�, �ƴϸ� NULL
*/
IDE_RC smrLogFileMgr::checkLogFileOpenAndIncRefCnt( UInt         aFileNo,
                                                    idBool     * aAlreadyOpen,
                                                    smrLogFile** aLogFile )
{
    SInt sState = 0;
    smrLogFile* sPrvLogFile;

    IDE_DASSERT( aAlreadyOpen != NULL );
    IDE_DASSERT( aLogFile != NULL );

    // Output Parameter �ʱ�ȭ
    *aAlreadyOpen = ID_FALSE;
    *aLogFile     = NULL;
    
    IDE_TEST( lockListMtx() != IDE_SUCCESS );
    sState = 1;
        
    // aFileNo���� �α������� open�� �α����� list�� �����ϴ� ���
    if ( findLogFile( aFileNo, &sPrvLogFile ) == ID_TRUE )
    {
        sPrvLogFile->mRef++;
        *aLogFile = sPrvLogFile;

        *aAlreadyOpen = ID_TRUE;
        
        sState = 0;
        IDE_TEST( unlockListMtx() != IDE_SUCCESS );
        
        /* ���� open���� ���� �α������� ��ü�� �α����� list�� ��
         * ���� �� �ִ�.
         * �α����� list�ȿ� �ִ� �ϳ��� �α����Ͽ� ����
         * �α����� Mutex�� �ѹ� ��ƺ����� �Ѵ�.
         *
         * �̸� ���� �α������� open�ϴ� �����尡 �ش� �α������� open���ΰ��,
         * �α������� open�� �Ϸ�� ������ ��ٸ����� �Ѵ�.
         */
        IDE_TEST( (*aLogFile)->lock() != IDE_SUCCESS );
        IDE_TEST( (*aLogFile)->unlock() != IDE_SUCCESS );
    }
    else
    {
        // aFileNo�� �ش��ϴ� LogFile�� �������� �ʴ´�.
        sState = 0;
        IDE_TEST( unlockListMtx() != IDE_SUCCESS );
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sState != 0 )
    {
        IDE_PUSH();
        (void)unlockListMtx();
        IDE_POP();
    }
    
    return IDE_FAILURE;
}
