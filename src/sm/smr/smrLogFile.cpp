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
 * $Id: smrLogFile.cpp 89697 2021-01-05 10:29:13Z et16 $
 **********************************************************************/

#include <smErrorCode.h>
#include <smu.h>
#include <smm.h>
#include <sct.h>
#include <smr.h>
#include <smrReq.h>

#ifdef ALTIBASE_ENABLE_SMARTSSD
#include <sdm_mgnt_public.h>
#endif

#define SMR_LOG_BUFFER_CHUNK_LIST_COUNT (1)
#define SMR_LOG_BUFFER_CHUNK_ITEM_COUNT (5)

iduMemPool smrLogFile::mLogBufferPool;

/*
 * TASK-6198 Samsung Smart SSD GC control
 */
#ifdef ALTIBASE_ENABLE_SMARTSSD
extern sdm_handle_t * gLogSDMHandle;
#endif

smrLogFile::smrLogFile()
{

}

smrLogFile::~smrLogFile()
{

}

IDE_RC smrLogFile::initializeStatic( UInt aLogFileSize )
{
    // ���� Direct I/O�� �Ѵٸ� Log Buffer�� ���� �ּ� ����
    // Direct I/O Pageũ�⿡ �°� Align�� ���־�� �Ѵ�.
    // �̿� ����Ͽ� �α� ���� �Ҵ�� Direct I/O Page ũ�⸸ŭ
    // �� �Ҵ��Ѵ�.
    if ( smuProperty::getLogIOType() == 1 )
    {
        aLogFileSize += iduProperty::getDirectIOPageSize();
    }

    IDE_TEST( mLogBufferPool.initialize(
                                 IDU_MEM_SM_SMR,
                                 (SChar *)"Log File Buffer Memory Pool",
                                 SMR_LOG_BUFFER_CHUNK_LIST_COUNT, 
                                 aLogFileSize, 
                                 SMR_LOG_BUFFER_CHUNK_ITEM_COUNT,
                                 IDU_AUTOFREE_CHUNK_LIMIT,			/* ChunkLimit */
                                 ID_TRUE,							/* UseMutex */
                                 IDU_MEM_POOL_DEFAULT_ALIGN_SIZE,	/* AlignByte */
                                 ID_FALSE,							/* ForcePooling */
                                 ID_TRUE,							/* GarbageCollection */
                                 ID_TRUE,                           /* HWCacheLine */
                                 IDU_MEMPOOL_TYPE_LEGACY            /* mempool type */) 
              != IDE_SUCCESS);			

    /* BUG-35392 
     * ����/����� �α׿��� �Ʒ� ����� ���� ��ġ�� �־�� �Ѵ�. */
    IDE_ASSERT( SMR_LOG_FLAG_OFFSET    == SMR_COMP_LOG_FLAG_OFFSET );
    IDE_ASSERT( SMR_LOG_LOGSIZE_OFFSET == SMR_COMP_LOG_SIZE_OFFSET );
    IDE_ASSERT( SMR_LOG_LSN_OFFSET     == SMR_COMP_LOG_LSN_OFFSET );
    IDE_ASSERT( SMR_LOG_MAGIC_OFFSET   == SMR_COMP_LOG_MAGIC_OFFSET );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC smrLogFile::destroyStatic()
{
    IDE_TEST( mLogBufferPool.destroy() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

IDE_RC smrLogFile::initialize()
{
    UInt sMultiplexIdx;

    mEndLogFlush = ID_FALSE;

    mFileNo        = ID_UINT_MAX;
    mFreeSize      = 0;
    mOffset        = 0;
    mBaseAlloced   = NULL;
    mBase          = NULL;
    mRef           = 0;
    mSyncOffset    = 0;
    mPreSyncOffset = 0;
    mSwitch        = ID_FALSE;
    mWrite         = ID_FALSE;
    mMemmap        = ID_FALSE;
    mOnDisk        = ID_FALSE;
    mIsOpened      = ID_FALSE;
    
    mPrvLogFile = NULL;
    mNxtLogFile = NULL;

    for( sMultiplexIdx = 0; 
         sMultiplexIdx < SM_LOG_MULTIPLEX_PATH_MAX_CNT; 
         sMultiplexIdx++ )
    {
        mMultiplexSwitch[ sMultiplexIdx ]  = ID_FALSE;
    }

    IDE_TEST( mFile.initialize( IDU_MEM_SM_SMR,
                                1, /* Max Open FD Count */
                                IDU_FIO_STAT_OFF,
                                IDV_WAIT_INDEX_NULL )
             != IDE_SUCCESS );

    IDE_TEST( mMutex.initialize( (SChar*)"SMR_LOGFILE_MUTEX",
                                 IDU_MUTEX_KIND_NATIVE,
                                 IDV_WAIT_INDEX_NULL )
             != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

IDE_RC smrLogFile::destroy()
{
    /* ���� Open�Ǿ� �ִ� FD�� ������ 0�� �ƴϸ� */
    if ( mFile.getCurFDCnt() != 0 )
    {
        IDE_TEST(close() != IDE_SUCCESS);
    }
    else
    {
        /* nothing to do */
    }

    IDE_TEST(mFile.destroy() != IDE_SUCCESS);
    IDE_TEST(mMutex.destroy() != IDE_SUCCESS);

    mFileNo   = ID_UINT_MAX;
    mFreeSize = 0;
    mOffset   = 0;
    mBaseAlloced = NULL;
    mBase     = NULL;
    mRef      = 0;

    mPrvLogFile = NULL;
    mNxtLogFile = NULL;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/* ------------------------------------------------
 * Description : LogFile�� �غ� �Ѵ�.(BUG-48409)
 *               logfile ���� �߿� ������ ������ ���� �ϸ� 
 *               ���� ���̴� size�� ���� �ʴ� logfile �� �����.
 *               prepare �߿� �ӽ� ���ϸ����� �����ؼ�
 *               ���� �Ϸ�� ��쿡 logfile name���� �����Ѵ�.
 *
 * aLogFileName  [IN] - �����Ϸ��� logfile�� name
 * aTempFileName [IN] - �ӽ� ���ϸ�, LFG ���� �����ϴ�.
 * aInitBuffer   [IN] - logfile �ʱ�ȭ �����Ͱ� �ִ� ����
 * aSize         [IN] - logFile�� ũ��
 * ----------------------------------------------*/
IDE_RC smrLogFile::prepare(SChar   * aLogFileName,
                           SChar   * aTempFileName,
                           SChar   * aInitBuffer,
                           SInt      aSize)
{
    UInt sState = 0;

    /* logfile.tmp�� ���� �� ��� ���� �����մϴ�.*/
    if ( idf::access( aTempFileName, F_OK ) == 0 )
    {
        (void)idf::unlink( aTempFileName );
    }

    IDE_TEST( create( aTempFileName,
                      aInitBuffer ,
                      aSize ) != IDE_SUCCESS );
    sState = 1;

    IDE_TEST( mFile.setFileName( aLogFileName ) != IDE_SUCCESS );

    IDE_TEST( idf::rename( aTempFileName,
                           aLogFileName ) != 0 );
    sState = 2;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch( sState )
    {
        case 2:
            (void)idf::unlink( aLogFileName );
            break;
        case 1:
            (void)idf::unlink( aTempFileName );
            break;
        default:
            break;
    }

    return IDE_FAILURE;
}

IDE_RC smrLogFile::create( SChar   * aStrFilename,
                           SChar   * aInitBuffer,
                           UInt      aSize )
{

    SInt       sFilePos;
    ULong      sFileSize;
    SInt       sState = 0;
    UInt       sFileInitBufferSize;
    SChar    * sBufPtr ;
    SChar    * sBufFence ;
    idBool     sUseDirectIO;

    // read properties
    sFileInitBufferSize = smuProperty::getFileInitBufferSize();

    //create log file
    mSize = aSize;

    IDE_TEST(mFile.setFileName(aStrFilename) != IDE_SUCCESS);

    /*---------------------------------------------------------------------- 
     * If disk space is insufficient, wait until disk space is sufficient. 
     * During waiting, User can enlarge the disk space.
     * If SYSTEM SHUTDOWN command is accepted,
     * server is shutdown immediately without trasaction rollback
     * because of insufficient log space for CLR!!  
     *-------------------------------------------------------------------- */
    IDE_TEST( mFile.createUntilSuccess( smLayerCallback::setEmergency,
                                        smrRecoveryMgr::isFinish() )
              != IDE_SUCCESS );
    sState = 1;

    /* BUG-15962: LOG_IO_TYPE�� DirectIO�� �ƴҶ� logfille������
     * direct io�� ����ϰ� �ֽ��ϴ�. */
    if ( smuProperty::getLogIOType() == 1 )
    {
        sUseDirectIO = ID_TRUE;
    }
    else
    {
        sUseDirectIO = ID_FALSE;
    }

    IDE_TEST( mFile.open(sUseDirectIO) != IDE_SUCCESS );
    sState = 2;

    if ( smuProperty::getCreateMethod() == SMU_LOG_FILE_CREATE_FALLOCATE )
    {
        IDE_TEST( mFile.fallocate( aSize ) != IDE_SUCCESS );
    }
    else
    {
        IDE_DASSERT( smuProperty::getCreateMethod() == SMU_LOG_FILE_CREATE_WRITE );

        switch( smuProperty::getSyncCreate() )
        {
        case 2:

            /* For BUG-13353 [LFG] SYNC_CREATE_=0�϶�
             *                     invalid log���� Ȯ���� 0�� ������ ���߾�� ��
             *
             * SYNC_CREATE=1 �� ����������, �α������� random������ ä���.
             * SYNC_CREATE=0 ���� ���������� OS�� memset�� �ؼ� �÷��ִ� �ý���
             * ������ �̸� �̿��Ͽ� SYNC_CREATE=0�� �������� �ùķ��̼� �� �� �ִ�. */
            {
                idlOS::srand( idlOS::getpid() );

                sBufFence = aInitBuffer + sFileInitBufferSize;

                for ( sBufPtr = aInitBuffer ;
                      sBufPtr < sBufFence   ;
                      sBufPtr ++ )
                {
                    * sBufPtr = (SChar) idlOS::rand();
                }
            }
        case 1:
            /* �α����� ��ü�� aInitBuffer �� �������� �ʱ�ȭ�Ѵ�. */
            {
                sFilePos = 0;

                while ( (ULong)(aSize - sFilePos) > sFileInitBufferSize )
                {
                    IDE_TEST( write( sFilePos, aInitBuffer, sFileInitBufferSize )
                              != IDE_SUCCESS );

                    sFilePos += sFileInitBufferSize;
                }


                if(aSize - sFilePos != 0)
                {
                    IDE_TEST( write( sFilePos, aInitBuffer, ( aSize - sFilePos ) )
                             != IDE_SUCCESS );
                }
            }

            break;

        case 0 :
            {
                /* �α������� ��ä�� �ʱ�ȭ���� �ʰ�
                 * �� �� �ѹ���Ʈ�� �ʱ�ȭ �Ͽ� ������ �����Ѵ�.
                 *
                 * BMT�� ���� �Ͻ������� ������ ������ �ϴ� ��쿡��
                 * SyncCreate������Ƽ�� 0���� �����Ǿ� ����� �������� �Ѵ�.
                 * ��, SyncCreate ������Ƽ�� ������ �������� �ȵȴ�.
                 *
                 * ���� 1 :  �̷��� �ϸ� ��κ��� OS������ sSize��ŭ File�� extend�ȴ�.
                 *           �׷���, � OS�� File�� ��Ÿ������ �����ϰ�
                 *           extend�� ���ϴ� ��쵵 �ִ�.
                 *
                 * ���� 2 :  �α������� �ʱ�ȭ�� ���� �ʾƼ�,
                 *           ���ϻ� ������ ���� �����ϰ� �Ǵµ�,
                 *           �� ����, �����Ե� �α� ����� �α�Ÿ�԰� �α� ������
                 *           ���� ������ ��ϵȰ�ó�� �Ǿ
                 *           ��Ŀ���� �Ŵ����� �α׷��ڵ尡 �ƴѵ���
                 *           �α׷��ڵ�� �ν��� ���� �ִ�.
                 *           �� ��� ��Ŀ���� �Ŵ����� ���� ������ ������ �� ����.
                 */

                IDE_TEST( write( aSize - iduProperty::getDirectIOPageSize(),
                                 aInitBuffer,
                                 iduProperty::getDirectIOPageSize() )
                          != IDE_SUCCESS );
            }

            break;

        default:
            break;
        } //switch 
    }// else

    /* BUG-48409 ������ Logfile�� size�� Ȯ���Ѵ�. */    
    IDE_TEST( mFile.getFileSize( &sFileSize ) != IDE_SUCCESS );
    IDE_ERROR( sFileSize == (ULong)aSize );

    sState = 1;
    IDE_TEST( mFile.close() != IDE_SUCCESS );


    mFreeSize = aSize;
    mOffset   = 0;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sState == 2 )
    {
        IDE_PUSH();
        IDE_ASSERT( mFile.close() == IDE_SUCCESS );
        IDE_POP();
    }

    return IDE_FAILURE;

}

IDE_RC smrLogFile::backup( SChar*   aBackupFullFilePath )
{
    iduFile         sDestFile;
    ULong           sLogFileSize;
    idBool          sUseDirectIO;
    ULong           sCurFileSize = 0;
    UInt            sState = 0;

    // read properties
    sLogFileSize = smuProperty::getLogFileSize();

    IDE_TEST( mFile.getFileSize( &sCurFileSize ) != IDE_SUCCESS ); 

    IDE_TEST_RAISE( sCurFileSize != sLogFileSize,
                    error_filesize_wrong);

    IDE_TEST( sDestFile.initialize( IDU_MEM_SM_SMR,
                                    1, /* Max Open FD Count */
                                    IDU_FIO_STAT_OFF,
                                    IDV_WAIT_INDEX_NULL )
              != IDE_SUCCESS );
    sState = 1;

    IDE_TEST( sDestFile.setFileName(aBackupFullFilePath)
              != IDE_SUCCESS );

    IDE_TEST( sDestFile.createUntilSuccess( smLayerCallback::setEmergency )
              != IDE_SUCCESS );

    /* BUG-15962: LOG_IO_TYPE�� DirectIO�� �ƴҶ� logfille������ direct io��
     * ����ϰ� �ֽ��ϴ�. */
    if ( smuProperty::getLogIOType() == 1 )
    {
        sUseDirectIO = ID_TRUE;
    }
    else
    {
        sUseDirectIO = ID_FALSE;
    }

    IDE_TEST( sDestFile.open(sUseDirectIO) != IDE_SUCCESS );
    sState = 2;

    IDE_TEST( sDestFile.write( NULL, /* idvSQL* */
                               0,
                               mBase,
                               sLogFileSize )
              != IDE_SUCCESS);

    sState = 1;
    IDE_TEST( sDestFile.close() != IDE_SUCCESS );

    sState = 0;
    IDE_TEST( sDestFile.destroy() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION(error_filesize_wrong);
    {
        IDE_SET(ideSetErrorCode(smERR_FATAL_WrongLogFileSize, mFile.getFileName()));
    }
    IDE_EXCEPTION_END;

    switch ( sState )
    {
        case 2:
            IDE_ASSERT(sDestFile.close() == IDE_SUCCESS);
        case 1:
            IDE_ASSERT(sDestFile.destroy() == IDE_SUCCESS);

            if ( idf::access( aBackupFullFilePath, F_OK ) == 0 )
            {
                (void)idf::unlink(aBackupFullFilePath);
            }
        case 0:
            break;
    }

    return IDE_FAILURE;

}

IDE_RC smrLogFile::mmap( UInt aSize, idBool aWrite )
{
    IDE_TEST( mFile.mmap( NULL /* idvSQL* */,
                          aSize,
                          aWrite,
                          &mBase )
              != IDE_SUCCESS );

    mMemmap = ID_TRUE;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    mMemmap = ID_FALSE;

    return IDE_FAILURE;
}

/*
 * �α������� open�Ѵ�.
 * aFileNo             [IN] - �α����� ��ȣ
 *                            �α����� ����� ��ϵ� ���� ��ġ���� ������ ����.
 * aStrFilename        [IN] - �α����� �̸�
 * aIsMultiplexLogFile [IN] - ����ȭ�α����Ϸ� ����ϱ����� open�ϴ°����� ����
 * aSize               [IN] - �α����� ũ��
 * aWrite              [IN] - ���������� ����
 */
IDE_RC smrLogFile::open( UInt       aFileNo,
                         SChar    * aStrFilename,
                         idBool     aIsMultiplexLogFile,
                         UInt       aSize,
                         idBool     aWrite )
{
    ULong       sCurFileSize = 0;
    idBool      sUseDirectIO = ID_FALSE;
    UInt        sState       = 0;

    IDE_ASSERT( mIsOpened == ID_FALSE );

    mWrite              = aWrite;
    mIsMultiplexLogFile = aIsMultiplexLogFile;

    IDE_TEST(mFile.setFileName(aStrFilename) != IDE_SUCCESS);

    // BUG-15065 Direct I/O�� �������� �ʴ� �ý��ۿ�����
    // Direct I/O�� ����ϸ� �ȵȴ�.
    sUseDirectIO = ID_FALSE;

    // Log ��Ͻ� IOŸ��
    // 0 : buffered IO, 1 : direct IO
    if ( smuProperty::getLogIOType() == 1 )
    {
        /* LOG Buffer Type�� Memory�̰ų� Log File�� Read�� ���ؼ� Open�� ����
           IO�� Direct IO�� �����Ѵ�. */
        if ( ( smuProperty::getLogBufferType() == SMU_LOG_BUFFER_TYPE_MEMORY ) || 
             ( aWrite == ID_FALSE ) )
        {
            sUseDirectIO = ID_TRUE;
        }
        else
        {
            /* nothing to do ... */
        }
    }

    if( aWrite == ID_TRUE )
    {
        IDE_TEST( mFile.openUntilSuccess( sUseDirectIO, O_RDWR ) != IDE_SUCCESS );
    }
    else
    {
        IDE_TEST( mFile.openUntilSuccess( sUseDirectIO, O_RDONLY ) != IDE_SUCCESS );
    }

    sState = 1;

    if ( aWrite == ID_TRUE )
    {
        IDE_TEST( mFile.getFileSize( &sCurFileSize ) != IDE_SUCCESS );
        IDE_TEST_RAISE( sCurFileSize != aSize, error_filesize_wrong );
    }

    mOnDisk = ID_TRUE;

    mSize = aSize;
    mBase = NULL;

    if ( ( smuProperty::getLogBufferType() == SMU_LOG_BUFFER_TYPE_MEMORY ) ||
         ( ( aWrite == ID_FALSE ) && 
           ( smuProperty::getLogReadMethodType() == 0 ) ) )
    {
        // mBase�� Direct I/O Pageũ��� �����ּҰ�
        // Align�ǵ��� �α׹��� �Ҵ�
        IDE_TEST( allocAndAlignLogBuffer() != IDE_SUCCESS );

        if (aWrite == ID_TRUE)
        {
            idlOS::memset(mBase, 0, mSize);
        }
        else
        {
            IDE_TEST(mFile.read( NULL, /* idvSQL* */
                                 0,
                                 mBase,
                                 mSize) != IDE_SUCCESS);
        }
    }
    else
    {
        IDE_TEST( mmap( aSize, aWrite ) != IDE_SUCCESS );
        sState = 2;
    }

    if ( aWrite == ID_FALSE ) // �б� ����� ���
    {
        // Read Only�̱� ������ ���� �αװ� ����.
        mEndLogFlush = ID_TRUE;

        /*
         * To Fix BUG-11450  LOG_DIR, ARCHIVE_DIR �� ������Ƽ ������ ����Ǹ�
         *                   DB�� ����
         *
         * �α������� File Begin Log�� �������� üũ�Ѵ�.
         */
        IDE_TEST( checkFileBeginLog( aFileNo ) != IDE_SUCCESS );
    }
    else
    {
        /* nothing to do */
    }

    mIsOpened = ID_TRUE;

    return IDE_SUCCESS;

    IDE_EXCEPTION( error_filesize_wrong );
    {
        IDE_SET( ideSetErrorCode( smERR_FATAL_WrongLogFileSize, mFile.getFileName() ));
    }
    IDE_EXCEPTION_END;

    /* 
     * BUG-21209 [SM: smrLogFileMgr] logfile open���� �����߻��� ����ó����
     *           �� �� �ǰ� �ֽ��ϴ�.
     */
    switch ( sState )
    {
        case 2:
            IDE_ASSERT( unmap() == IDE_SUCCESS );
        case 1:
            IDE_ASSERT( mFile.close() == IDE_SUCCESS );
        default:
            break;
    }

    mBase = NULL;

    return IDE_FAILURE;

}

/*
 * Direct I/O�� ����� ��� Direct I/O Pageũ��� Align�� �ּҸ� mBase�� ����
 */
IDE_RC smrLogFile::allocAndAlignLogBuffer()
{
    IDE_DASSERT( mBaseAlloced == NULL );
    IDE_DASSERT( mBase == NULL );
    IDE_DASSERT( mMemmap == ID_FALSE );

    /* PROJ-1915 : off-line �α��� ���
     * ������ �� Ȯ���Ͽ� ���� ��� mLogBufferPool�� �̿� �ϰ� �׷��� ���� ��� malloc�� �Ͽ� ��� �Ѵ�.
     */
    if ( mSize == smuProperty::getLogFileSize() )
    {
        /* smrLogFile_allocAndAlignLogBuffer_alloc_BaseAlloced.tc */
        IDU_FIT_POINT("smrLogFile::allocAndAlignLogBuffer::alloc::BaseAlloced");
        IDE_TEST(mLogBufferPool.alloc(&mBaseAlloced) != IDE_SUCCESS);
    }
    else 
    {
        IDE_TEST(iduMemMgr::malloc(IDU_MEM_SM_SMR,
                                   mSize + iduProperty::getDirectIOPageSize(),
                                   (void **)&mBaseAlloced,
                                   IDU_MEM_FORCE) != IDE_SUCCESS);
    }

    if ( smuProperty::getLogIOType() == 1 )
    {
        // Direct I/O�� ����� ���
        mBase = idlOS::align( mBaseAlloced,
                              iduProperty::getDirectIOPageSize() );
    }
    else
    {
        // Direct I/O�� ������� ���� ���
        mBase = mBaseAlloced;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
 *  allocAndAlignLogBuffer �� �Ҵ��� �α׹��۸� Free�Ѵ�.
 */
IDE_RC smrLogFile::freeLogBuffer()
{
    IDE_DASSERT( mBaseAlloced != NULL );
    IDE_DASSERT( mBase != NULL );

    /* PROJ-1915 */
    if ( mSize == smuProperty::getLogFileSize() )
    {
        IDE_TEST(mLogBufferPool.memfree(mBaseAlloced) != IDE_SUCCESS);
    }
    else
    {
        IDE_TEST(iduMemMgr::free(mBaseAlloced) != IDE_SUCCESS);
    }
    
    mBaseAlloced = NULL;
    mBase = NULL;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
 * �α������� File Begin Log�� �������� üũ�Ѵ�.
 *
 * To Fix BUG-11450  LOG_DIR, ARCHIVE_DIR �� ������Ƽ ������ ����Ǹ�
 *                   DB�� ����
 *
 * �α������� �� ó���� smrFileBeginLog�� Valid�� ��� ������ üũ�Ѵ�.
 * FileNo üũ => �α����� �̸��� rename�Ǿ����� ���θ� üũ
 *
 * aFileNo      [IN] - �α����� ��ȣ
 *                     �α����� ����� ��ϵ� ���� ��ġ���� ������ ����.
 */
IDE_RC smrLogFile::checkFileBeginLog( UInt aFileNo )
{
    smrFileBeginLog  sFileBeginLog;
    smLSN            sFileBeginLSN;

    idlOS::memcpy( &sFileBeginLog, mBase, SMR_LOGREC_SIZE( smrFileBeginLog ) );

    sFileBeginLSN.mFileNo = aFileNo;
    sFileBeginLSN.mOffset = 0;

    
    if ( isValidLog( & sFileBeginLSN,
                     (smrLogHead * ) & sFileBeginLog,
                     (SChar * )   mBase,
                     // File Begin Log�� ��� �������� �ʴ´�.
                     // �޸𸮻��� �α��� ũ�Ⱑ ��
                     // ��ũ���� �α� ũ���̴�.
                     smrLogHeadI::getSize(& sFileBeginLog.mHead ) )
         == ID_TRUE )
    {
        IDE_ASSERT( smrLogHeadI::getType(&sFileBeginLog.mHead) == SMR_LT_FILE_BEGIN );

        IDE_TEST_RAISE( sFileBeginLog.mFileNo  != aFileNo,
                        error_mismatched_fileno );
    }
    else
    {
        /* nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(error_mismatched_fileno);
    {
        IDE_SET(ideSetErrorCode(smERR_FATAL_MISMATCHED_FILENO_IN_LOGFILE,
                                mFile.getFileName(),
                                aFileNo,
                                sFileBeginLog.mFileNo ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 * Description: mmap�Ǿ����� Ȯ���� mapping�� �����Ѵ�.
 ***********************************************************************/
IDE_RC smrLogFile::unmap(void)
{
    if ( mBase != NULL )
    {
        if( mMemmap == ID_TRUE )
        {
            IDE_TEST_RAISE(mFile.munmap(mBase, mSize) != 0,
                           err_memory_unmap);
        }
        else
        {
            // allocAndAlignLogBuffer�� �Ҵ��� ���� ����
            IDE_TEST( freeLogBuffer() != IDE_SUCCESS );
        }

        mBase = NULL;
    }
    else
    {
        /* nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_memory_unmap);
    {
        IDE_SET(ideSetErrorCode(smERR_FATAL_MunmapFail));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC smrLogFile::close()
{
    if ( mIsOpened == ID_TRUE )
    {
        IDE_TEST( unmap() != IDE_SUCCESS );

        if ( mOnDisk == ID_TRUE )
        {
            IDE_TEST( mFile.close() != IDE_SUCCESS );
        }
        else
        {
            /* nothing to do */
        }
        mIsOpened = ID_FALSE;
    }
    else
    {
        /* nothing to do */
    }

    mFileNo   = ID_UINT_MAX;
    mFreeSize = 0;
    mOffset   = 0;
    mBase     = NULL;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC smrLogFile::readFromDisk( UInt    aOffset,
                                 SChar * aStrBuffer,
                                 UInt    aSize )
{
    return mFile.read( NULL, /* idvSQL* */
                       aOffset,
                       aStrBuffer,
                       aSize );
}

void smrLogFile::read( UInt      aOffset,
                       SChar   * aStrBuffer,
                       UInt      aSize )
{
    idlOS::memcpy(aStrBuffer, (SChar *)mBase + aOffset, aSize);
    
}

void smrLogFile::read( UInt      aOffset,
                       SChar  ** aLogPtr )
{
    *aLogPtr = (SChar *)mBase + aOffset;
}

IDE_RC smrLogFile::remove( SChar   * aStrFileName,
                           idBool    aIsCheckpoint )
{
    SInt rc;

    //ignore file unlink error during restart recovery
    rc = idf::unlink(aStrFileName);

    // üũ ����Ʈ ���� �α����� ������ �����ϸ� �����߻�
    /* BUG-42589: �α� ���� ���� �� �� üũ����Ʈ ��Ȳ�� �ƴϾ (restart recovery)
     * ������ �����ϸ� Ʈ���̽� �α׸� �����. */
    if ( rc != 0 )
    {
        ideLog::log(IDE_SM_0, " %s Remove Fail (errno=<%u>) \n", aStrFileName, (UInt)errno );

        if ( aIsCheckpoint == ID_TRUE )
        {
            IDE_RAISE(err_file_unlink);
        }
        else
        {
            // do nothing
        }
    }
    else
    {
        // do nothing
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_file_unlink);
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_FileDelete, aStrFileName));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

void smrLogFile::clear(UInt   aBegin)
{
    if ( aBegin < mSize )
    {
        idlOS::memset( (SChar *)mBase + aBegin, 0, mSize - aBegin );
    }
    else
    {
        /* nothing to do */
    }
}


// 2�� ����� ���� ���ϴ� Page�� Align�� ���� PAGE_MASK���
//    
// Page Size       =>       256 ( 0x00000100 ) �϶�
// Page Size -1    =>       255 ( 0x000000FF ) �̰�
// ~(Page Size -1) => PAGE_MASK ( 0xFFFFFF00 ) �̴�
//
// => PAGE_MASK�� Bit And�����ϴ� �͸����� Align Down�� �ȴ�.
//
#define PAGE_MASK(aPageSize)   (~((aPageSize)-1))
// sSystemPageSize ������ Align�ϴ� �Լ� ( ALIGN DOWN )
#define PAGE_ALIGN_DOWN(aToAlign, aPageSize)                     \
           ( (aToAlign              ) & (PAGE_MASK(aPageSize)) ) 
// sSystemPageSize ������ Align�ϴ� �Լ� ( ALIGN UP )
#define PAGE_ALIGN_UP(aToAlign, aPageSize)                       \
           ( (aToAlign + aPageSize-1) & (PAGE_MASK(aPageSize)) )

/*
 * �α� ��Ͻ� Align�� Page�� ũ�� ����
 */
UInt smrLogFile::getLogPageSize()
{
    UInt sLogPageSize ;

    if ( smuProperty::getLogBufferType() == SMU_LOG_BUFFER_TYPE_MMAP )
    {
        // log buffer type�� mmap�� ���,
        // System Page Size�� ����
        sLogPageSize = idlOS::getpagesize();
    }
    else
    {
        // log buffer type�� memroy�� ���,
        // Direct I/O �� ���Ǵ� page ũ��� ����
        sLogPageSize = iduProperty::getDirectIOPageSize();
    }    
    
    // Align Up/Down�� Bit Mask�� ó���ϱ� ���� üũ
    IDE_ASSERT( (sLogPageSize == 512)  || (sLogPageSize == 1024)  ||
                (sLogPageSize == 2048) || (sLogPageSize == 4096)  ||
                (sLogPageSize == 8192) || (sLogPageSize == 16384) ||
                (sLogPageSize == 32768)|| (sLogPageSize == 65536) );

    return sLogPageSize;
}
               

/* ================================================================= *
 * aSyncLastPage : sync ����� �Ǵ� ������ �������� sync�� ��������  *
 *                 ����                                              *
 *                 == a_bEnd�� ID_TRUE�� ��� ======                 *
 *             (1) commit �α� ��Ͻ� sync �����ϴ� ���             *
 *             (2) full log file sync ��                             *
 *             (3) checkpoint ���� ��                                *
 *             (4) server shutdown ��                                *
 *             (5) ���� sync�� ���� �������� sync���� ���� ���      *
 *                 (mPreSyncOffset == mSyncOffset)                   *
 *             (6) FOR A4 : ���� �Ŵ����� PAGE�� FLUSH�ϱ� �� �ش�   *
 *                          PAGE�� ����α׸� �ݵ�� SYNC            *
 * aOffsetToSync : sync�ϰ��� �ϴ� ������ �α��� offset              *
 * ================================================================= */
IDE_RC smrLogFile::syncLog( idBool   aSyncLastPage,
                            UInt     aOffsetToSync )
{
    UInt   sInvalidSize;
    UInt   sSyncSize;
    UInt   sEndOffset;
    UInt   sLastSyncOffset = mSyncOffset;
    UInt   sSyncBeginOffset;
    SInt   rc;

    // Log Page Size to Align
    static UInt sLogPageSize= 0;
    
    /* DB�� Consistent���� ������, Log Sync�� ���� */
    IDE_TEST_CONT( ( smrRecoveryMgr::getConsistency() == ID_FALSE ) &&
                   ( smuProperty::getCrashTolerance() != 2 ),
                   SKIP );

    // Align�� ������ �� Page ũ�� ����
    if ( sLogPageSize == 0 )
    {
        sLogPageSize = getLogPageSize();
    }
    else
    {
        /* nothing to do ... */
    }

    IDE_ASSERT( mOffset >= mSyncOffset );

    /* BUG-35392 */
    /* sync ����� �Ǵ� ������ �α� ���� */
    sEndOffset = getLastValidOffset();

    IDE_ASSERT( sEndOffset >= mSyncOffset );

    sInvalidSize = sEndOffset - mSyncOffset;

    if ( sInvalidSize > 0 )
    {
        IDE_ASSERT( sEndOffset >= sLastSyncOffset );

        // sync ����� �Ǵ� ù��° ������ ����
        sSyncBeginOffset = PAGE_ALIGN_DOWN( mSyncOffset, sLogPageSize );
        sSyncSize = sEndOffset - sSyncBeginOffset;

        // sync ����� �Ǵ� ������ �������� sync�� ������ ���� ����
        if ( (aOffsetToSync > (sSyncBeginOffset + sSyncSize) ) &&
             (aOffsetToSync != ID_UINT_MAX) )
        {
            aSyncLastPage = ID_TRUE;
        }
        else
        {
            /* nothing to do */
        }

        // ������ �� �Լ��� �ҷȾ�����, ������ �������� sync���� �� �� ���,
        // �̹����� ������ �������� sync�� �ǽ� �Ѵ�.
        if ( mPreSyncOffset == mSyncOffset )
        {
            aSyncLastPage = ID_TRUE;
        }
        else
        {
            /* nothing to do */
        }

        /* ���� sync�� �α� ������ ���� �����Ѵ�.
         * sSyncSize���� ������ ������ �ƴ�, sync�� ����Ʈ���� ����ȴ�. */
        if ( (aSyncLastPage == ID_TRUE) ||
             (smuProperty::getLogBufferType() == SMU_LOG_BUFFER_TYPE_MEMORY) )
        {
            // BUG-14424
            // Log Buffer Type�� memory�� ��쿡��
            // mmap������ LastPage�� sync�ϱ� ����
            // contention�� �����Ƿ�, LastPage���� �׻� sync�Ѵ�.
            sSyncSize = PAGE_ALIGN_UP( sSyncSize, sLogPageSize );

            // �������� sync�� �� offset�� �����Ѵ�.
            mSyncOffset = sEndOffset;
        }
        else
        {
            sSyncSize = PAGE_ALIGN_DOWN( sSyncSize, sLogPageSize );
            // �������� sync�� �� offset�� �����Ѵ�.
            mSyncOffset = sSyncBeginOffset + sSyncSize;
        }

        if ( sSyncSize != 0 )
        {
            if ( mMemmap == ID_TRUE )
            {
                while (1)
                {
                    rc = idlOS::msync( (SChar *)mBase + sSyncBeginOffset,
                                       sSyncSize ,
                                       MS_SYNC);
                    if ( rc == 0 )
                    {
                        break;
                    }
                    else
                    {
                        IDE_TEST_RAISE( ( (errno != EAGAIN) && (errno != ENOMEM) ),
                                        err_memory_sync);
                    }
                }
            }
            else
            {
                IDE_ASSERT( smuProperty::getLogBufferType() == SMU_LOG_BUFFER_TYPE_MEMORY );

                // BUG-14424
                // log buffer type�� memory�� ��쿡��,
                // mmap������ LastPage�� sync�ϱ� ����
                // contention�� �����Ƿ�, LastPage���� �׻� sync�Ѵ�.
                if ( smuProperty::getLogIOType() == 0 )
                {
                    // Direct I/O ��� ����.
                    // Direct I/O�� ������� ���� ���
                    // Kernel�� File cache�� ���� memcpy�� ���̱� ���ؼ�
                    IDE_TEST( write( sLastSyncOffset,
                                     (SChar *)mBase + sLastSyncOffset,
                                     sEndOffset - sLastSyncOffset )
                              != IDE_SUCCESS );
                }
                else
                {
                    // Direct I/O ���
                    // Kernel�� File cache�� ��ġ�� �ʰ� Disk�� �ٷ�
                    // ����ϵ��� OS���� ��Ʈ�� �����Ѵ�.
                    // ��� OS�� Direct I/O�� File Cache��
                    // ��ġ�� ������ �������� ���Ѵ�.
                    // Ex> Case-4658�� ����� Sun�� Direct I/O�Ŵ��� ����
                    IDE_ASSERT( smuProperty::getLogIOType() == 1 );

                    IDE_TEST( write( sSyncBeginOffset,
                                     // Offset�� Page��迡 ����
                                     (SChar *)mBase + sSyncBeginOffset,
                                     // Pageũ�� ������ ����
                                     PAGE_ALIGN_UP( sEndOffset -
                                                    sSyncBeginOffset,
                                                    sLogPageSize ) ) 
                              != IDE_SUCCESS );
                }

                // Direct I/O�� �ϴ��� Sync�� ȣ���ؾ���.
                // Direct IO�� �⺻������ Sync�� ���ʿ��ϳ� SM ���׷�
                // ���� ������ ���� Direct IO������ ��Ű�� ���� ���
                //  1. read�� write�� offset, buffer, size�� Ư�� ����
                //    �� align�Ǿ�� �Ѵ�.
                //  2. �� Ư������ OS���� �ٸ���.
                // � OS�� ������ ���� ��쵵 �ְ� SUN���� �׳� Buffered
                // IO�� ó���ϴ� ��쵵 �ֽ��ϴ�. ������ ������ Sync�մϴ�.
                IDE_TEST( sync() != IDE_SUCCESS );
            }

            /*
             * TASK-6198 Samsung Smart SSD GC control
             */
#ifdef ALTIBASE_ENABLE_SMARTSSD
            if ( smuProperty::getSmartSSDLogRunGCEnable() != 0 )
            {
                if ( sdm_run_gc( gLogSDMHandle, 
                                 smuProperty::getSmartSSDGCTimeMilliSec() ) != 0 )
                {
                    ideLog::log( IDE_SM_0,
                                 "sdm_run_gc failed" );
                }
            }
#endif
        }
        else // sSyncSize == 0
        {
            // sSyncSize�� �� ������ �ȿ� ���� ���� ũ���� ���,
            // aSyncLastPage == ID_FALSE �̸�
            // ������ �������� sync���� �ʰ� �Ǿ�
            // sSyncSize�� 0���� ū ������ 0���� �ٲ� ����̴�.
            //
            // �׷���, �̷��� ��û, ��,
            // ���� ũ���� sSyncSize��ŭ sync�϶�� ��û��
            // �������� aSyncLastPage == ID_FALSE �� �ѹ� �� ������ �Ǹ�,
            // �׶��� aSyncLastPage�� ID_TRUE�� �ٲپ,
            // ������ �������� sync�ϵ��� �ؾ� �Ѵ�.
            //
            // �������� �� �Լ��� �ҷ��� �� ������ Sync��û�� ��������
            // ������ �������� sync���� �� �� offset���� üũ�ϱ� ����,

            mPreSyncOffset = mSyncOffset;
        }
    }

    IDE_EXCEPTION_CONT( SKIP );

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_memory_sync);
    {
        IDE_SET(ideSetErrorCode(smERR_IGNORE_SyncFail));
    }
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC smrLogFile::write( SInt      aWhere,
                          void    * aBuffer,
                          SInt      aSize )
{

    IDE_ASSERT(mMemmap == ID_FALSE);

    IDE_TEST( mFile.writeUntilSuccess( NULL, /* idvSQL* */
                                       aWhere,
                                       aBuffer,
                                       aSize,
                                       smLayerCallback::setEmergency,
                                       smrRecoveryMgr::isFinish() )
              != IDE_SUCCESS );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;

}

/***********************************************************************
 * Description : aStartOffset���� aEndOffset���� mBase�� ������ ��ũ�� �ݿ���Ų��.
 *
 * aStartOffset - [IN] Disk�� ����� mBase�� ���� Offset
 * aEndOffset   - [IN] Disk�� ����� mBase�� �� Offset
 ***********************************************************************/
IDE_RC smrLogFile::syncToDisk( UInt aStartOffset, UInt aEndOffset )
{
    SInt   rc;
    
    UInt   sSyncSize;
    UInt   sSyncBeginOffset;

    /* ===================================
     * Log Page Size to Align
     * =================================== */
    static UInt sLogPageSize  = 0;

    /* DB�� Consistent���� ������, Log Sync�� ���� */
    IDE_TEST_CONT( ( smrRecoveryMgr::getConsistency() == ID_FALSE ) &&
                    ( smuProperty::getCrashTolerance() != 2 ),
                    SKIP );

    // Align�� ������ �� Page ũ�� ����
    if ( sLogPageSize == 0 )
    {
        sLogPageSize = getLogPageSize();
    }

    
    IDE_DASSERT( (aStartOffset <= smuProperty::getLogFileSize()) &&
                 (aEndOffset <= smuProperty::getLogFileSize()));

    IDE_ASSERT( aStartOffset <= aEndOffset);

    if ( aStartOffset != aEndOffset )
    {
        sSyncBeginOffset = PAGE_ALIGN_DOWN( aStartOffset, sLogPageSize );

        sSyncSize = aEndOffset - sSyncBeginOffset;

        sSyncSize = PAGE_ALIGN_UP( sSyncSize, sLogPageSize );

        if ( mMemmap == ID_TRUE )
        {
            rc = idlOS::msync( (SChar*)mBase + sSyncBeginOffset,
                               sSyncSize,
                               MS_SYNC );

            IDE_TEST_RAISE( (rc != 0) && (errno != EAGAIN) && (errno != ENOMEM),
                            err_memory_sync);
        }
        else
        {
            // Diect I/O�� ������� �ʴ� ���
            if ( smuProperty::getLogIOType() == 0 )
            {
                IDE_TEST( write( aStartOffset,
                                 (SChar*)mBase + aStartOffset,
                                 aEndOffset - aStartOffset )
                          != IDE_SUCCESS );
            }
            else
            {
                // Direct I/O ���
                IDE_ASSERT( smuProperty::getLogIOType() == 1 );

                IDE_TEST( write( sSyncBeginOffset,
                                 (SChar*)mBase + sSyncBeginOffset,
                                 sSyncSize )
                          != IDE_SUCCESS );
            }

            // Direct I/O�� fsync�� ���ʿ��ϳ�
            // Ư�� OS�� ��� Direct I/O ��û�� ���� ���� Buffer IO��
            // �����ϴ� ��찡 �߻��ϱ⶧���� ������������ fsync�� ������(ex: sun)
            IDE_TEST(sync() != IDE_SUCCESS);
        }
    }

    IDE_EXCEPTION_CONT( SKIP );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION(err_memory_sync);
    {
        IDE_SET(ideSetErrorCode(smERR_IGNORE_SyncFail));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : �ϳ��� �α� ���ڵ尡 Valid���� ���θ� �Ǻ��Ѵ�.
 *
 * aLSN        - [IN]  Log�� ��ġ�� LSN
 * aLogHeadPtr - [IN]  Log�� ��� ( Align �� �޸� )
 * aLogPtr     - [IN] Log�� Log Buffer�� Log Pointer
 * aLogSizeAtDisk - [IN] �α����ϻ� ��ϵ� �α��� ũ��
 *                       (����� �α��� ��� �����α�ũ�⺸�� �۴�)
 **********************************************************************/
idBool smrLogFile::isValidLog( smLSN       * aLSN,
                               smrLogHead  * aLogHeadPtr,
                               SChar       * aLogPtr,
                               UInt          aLogSizeAtDisk )
{
    smrLogType   sLogType;
    idBool       sIsValid = ID_TRUE;
    static UInt  sLogFileSize = smuProperty::getLogFileSize();

    IDE_ASSERT( aLSN->mOffset < sLogFileSize );
    
    /* Fix BUG-4926 */
    if ( ( smrLogHeadI::getSize(aLogHeadPtr) < ( ID_SIZEOF(smrLogHead) + ID_SIZEOF(smrLogTail) ) ) ||
         ( aLogSizeAtDisk > (sLogFileSize - aLSN->mOffset) ) )
    {
        sIsValid = ID_FALSE;
    }
    else
    {
        idlOS::memcpy( (SChar*)&sLogType,
                       aLogPtr + smrLogHeadI::getSize(aLogHeadPtr) - ID_SIZEOF(smrLogTail),
                       ID_SIZEOF(smrLogTail) );

        if (( sLogType == SMR_LT_NULL ) || // BUG-47754
            ( smrLogHeadI::getType(aLogHeadPtr) != sLogType ))
        {
            ideLog::log(SM_TRC_LOG_LEVEL_MRECOV,
                        SM_TRC_MRECOVERY_LOGFILE_INVALID_LOG1);
            ideLog::log(SM_TRC_LOG_LEVEL_MRECOV,
                        SM_TRC_MRECOVERY_LOGFILE_INVALID_LOG2,
                        smrLogHeadI::getType(aLogHeadPtr),
                        sLogType);
            ideLog::log(SM_TRC_LOG_LEVEL_MRECOV,
                        SM_TRC_MRECOVERY_LOGFILE_INVALID_LOG3);

            idlOS::memcpy((SChar*)&sLogType,
                          aLogPtr + smrLogHeadI::getSize(aLogHeadPtr) - ID_SIZEOF(smrLogTail),
                          ID_SIZEOF(smrLogTail));

            if (( sLogType == SMR_LT_NULL ) || // BUG-47754
                ( smrLogHeadI::getType(aLogHeadPtr) != sLogType ))
            {
                ideLog::log(SM_TRC_LOG_LEVEL_MRECOV, SM_TRC_MRECOVERY_LOGFILE_INVALID_LOG4);
                ideLog::log(SM_TRC_LOG_LEVEL_MRECOV,
                            SM_TRC_MRECOVERY_LOGFILE_INVALID_LOG5,
                            smrLogHeadI::getType(aLogHeadPtr),
                            sLogType);
                sIsValid = ID_FALSE;
            }
            else
            {
                ideLog::log(SM_TRC_LOG_LEVEL_MRECOV,
                            SM_TRC_MRECOVERY_LOGFILE_INVALID_LOG6);

                sIsValid = isValidMagicNumber( aLSN, aLogHeadPtr );
            }
        }
        else
        {
            // Log�� ����Ҷ� ������� MAGIC NUMBER �˻�
            sIsValid = isValidMagicNumber( aLSN, aLogHeadPtr );
        }
    }

    return sIsValid ;
}

 /***********************************************************************
 * Description : logfile�� mmap�Ǿ��� ��� file cache�� �÷����� ���� �޸𸮿�
 *               logfile����Ÿ�� �о���δ�. ������ ��� ����Ÿ�� ���� �ʿ䰡 ����
 *               ������ mmap������ �޸𸮸� Page Size�� ���� ������ Page �� ù
 *               Byte���� �о���δ�.
 **********************************************************************/
#if defined(IA64_HP_HPUX)
SInt smrLogFile::touchMMapArea()
#else
void smrLogFile::touchMMapArea()
#endif
{
    SChar   sData;
    SInt    i;
    SInt    sum = 0;

    static SInt sLoopCnt = smuProperty::getLogFileSize() / 512;

    if ( mMemmap == ID_TRUE )
    {
        for ( i = 0 ; i < sLoopCnt ; i++ )
        {
            sData = *((SChar*)mBase + 512 * i);
            sum += (SInt)sData;
        }
    }
    else
    {
        /* nothing to do */
    }
#if defined(IA64_HP_HPUX)
    /* sum�� ���� ������ Compiler�� ����ȭ�� �� function��
       dead code�� �����ϴ� ���� �����ϱ� ���� �߰��� ������
       �ƹ� �ǹ� ����.*/
    return sum;
#endif
}

/***********************************************************************
 * BUG-35392
 * Description : ���������� Copy��(logfile�� �����) Log�� ������ Offset�� �޾ƿ´�.
 *               ���� File�� �Ѿ�� ���� Log File�� ��� ���̶��
 *               mOffset�� �ѱ��.
 **********************************************************************/
UInt  smrLogFile::getLastValidOffset()
{
    smLSN    sLstLSN;
    UInt     sOffset;

    SM_LSN_MAX( sLstLSN );
    sOffset = ID_UINT_MAX;

    smrLogMgr::getLstLogOffset( &sLstLSN );

    if ( sLstLSN.mFileNo == mFileNo )
    {
        /* 
         * BUG-37018 There is some mistake on logfile Offset calculation
         * ����ȭ �α����ϰ�� �αװ� ��ϵ� mOffset�� sLstLSN���� ���� �� �ִ�.
         * ���� �ڱ��ڽ��� offset������ sync�ؾ��Ѵ�.
         */
        if ( mIsMultiplexLogFile == ID_TRUE )
        {
            sOffset = mOffset;
        }
        else
        {
            sOffset = sLstLSN.mOffset;
        }  
    }
    else
    {
        if ( sLstLSN.mFileNo > mFileNo )
        {
            /* Copy �Ϸ� LSN�� ���� Log File�� �Ѿ ���
             * ���� log file�� ��� sync �� �� �ִ�. */
            sOffset = mOffset;
        }
        else
        {
            /* ���� Log File�� Copy�� ���� �Ϸ���� ���� ���
             * mSyncOffset�� Ȯ������ �ʴ´�. */
            sOffset = mSyncOffset;

            /* �׷���, �̷����� �ֳ�?
             * �ϴ� ó�� �����ϸ� �߻� �� �� �ִ� �� ����. */
            IDE_ASSERT( mSyncOffset == 0 );
        }
    }

    return sOffset;
}
