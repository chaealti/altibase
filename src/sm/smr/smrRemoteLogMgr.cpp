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
 * PROJ-1915
 * Off-line ������ ���� LFGMgr ���� �α� �б� ��� ���� ���� �Ѵ�.
 *
 **********************************************************************/
#include <smuProperty.h>
#include <smErrorCode.h>
#include <smDef.h>
#include <smr.h>
#include <smrReq.h>
#include <smiMain.h>

#define SMR_LOG_BUFFER_CHUNK_LIST_COUNT (1)
#define SMR_LOG_BUFFER_CHUNK_ITEM_COUNT (5)

smrRemoteLogMgr::smrRemoteLogMgr()
{
}

smrRemoteLogMgr::~smrRemoteLogMgr()
{
}

/***********************************************************************
 * Description : �α� �׷� ������ �ʱ�ȭ
 *
 * aLogFileSize - [IN] off-line Log File Size
 * aLogDirPath  - [IN] LogDirPath array
 * aNotDecomp   - [IN] ����� �α������� ��� ������ Ǯ���ʰ� ��ȯ���� ���� (����� ���·� ��ȯ)
 **********************************************************************/
IDE_RC smrRemoteLogMgr::initialize(ULong    aLogFileSize,
                                   SChar ** aLogDirPath,
                                   idBool   aNotDecomp )
{
    UInt sStage = 0;

    mNotDecomp = aNotDecomp;

    setLogFileSize(aLogFileSize);

    mLogDirPath = NULL;

    sStage = 1;
    IDE_TEST( iduMemMgr::malloc( IDU_MEM_SM_SMR,
                                 SM_MAX_FILE_NAME,
                                 (void **)&mLogDirPath,
                                 IDU_MEM_FORCE) != IDE_SUCCESS );

    idlOS::memset(mLogDirPath, 0x00, SM_MAX_FILE_NAME);

    setLogDirPath( *aLogDirPath );

    IDE_TEST( checkLogDirExist() != IDE_SUCCESS );

    IDE_TEST( mMemPool.initialize( IDU_MEM_SM_SMR,
                                   (SChar *)"OPEN_REMOTE_LOGFILE_MEM_LIST",
                                   1,/* List Count */
                                   ID_SIZEOF(smrLogFile),
                                   SMR_LOG_FULL_SIZE,
                                   IDU_AUTOFREE_CHUNK_LIMIT,			/* ChunkLimit */
                                   ID_TRUE,								/* UseMutex */
                                   IDU_MEM_POOL_DEFAULT_ALIGN_SIZE,		/* AlignByte */
                                   ID_FALSE,							/* ForcePooling */
                                   ID_TRUE,								/* GarbageCollection */
                                   ID_TRUE,                             /* HWCacheLine */
                                   IDU_MEMPOOL_TYPE_LEGACY              /* mempool type */ ) 
              != IDE_SUCCESS);			
    sStage = 2;

    IDE_TEST( setRemoteLogMgrsInfo() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();

    switch (sStage)
    {
        case 2:
            IDE_ASSERT( mMemPool.destroy() == IDE_SUCCESS );
        case 1:
            if ( mLogDirPath != NULL )
            {
                IDE_ASSERT( iduMemMgr::free(mLogDirPath)
                            == IDE_SUCCESS );
                mLogDirPath = NULL;
            }
            else
            {
                /* nothing to do ... */
            }

        default:
            break;
    }

    IDE_POP();

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : �α� �׷� ������ ����
 *
 * :initialize �� �������� �����Ѵ�.
 *
 **********************************************************************/
IDE_RC smrRemoteLogMgr::destroy()
{
    IDE_TEST( mMemPool.destroy() != IDE_SUCCESS );

    if ( mLogDirPath != NULL )
    {
        IDE_TEST( iduMemMgr::free(mLogDirPath) != IDE_SUCCESS );
        mLogDirPath = NULL;
    }
    else
    {
        /* nothing to do ... */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : aLSN�� ����Ű�� �α������� ù��° Log �� Head�� �д´�
 *
 * aLSN      - [IN]  Ư�� �α����ϻ��� ù��° �α��� LSN
 * aLogHead  - [OUT] �о���� Log�� Head�� �Ѱ��� Parameter
 * aIsValid  - [OUT] �о���� �α��� Valid ����
 **********************************************************************/
IDE_RC smrRemoteLogMgr::readFirstLogHead(smLSN      * aLSN,
                                         smrLogHead * aLogHead,
                                         idBool     * aIsValid)
{
    IDE_DASSERT(aLSN     != NULL );
    IDE_DASSERT(aLogHead != NULL );
    IDE_DASSERT( aIsValid != NULL );

    // �α����ϻ��� ù��° �α��̹Ƿ� Offset�� 0�̾�� �Ѵ�.
    IDE_ASSERT( aLSN->mOffset == 0 );

    IDE_TEST( readFirstLogHeadFromDisk( aLSN, aLogHead, aIsValid )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : aFirstFileNo���� aEndFileNo������
 *               aMinLSN.mFileNo�� aNeedFirstFileNo�� �־��ش�.
 *
 * aMinLSN          - [IN]  Minimum Log Sequence Number
 * aFirstFileNo     - [IN]  check�� Logfile �� ù��° File No
 * aEndFileNo       - [IN]  check�� Logfile �� ������ File No
 * aNeedFirstFileNo - [OUT] aMinLSN������ ū ���� ���� ù��° �α� File No
 **********************************************************************/
IDE_RC smrRemoteLogMgr::getFirstNeedLFN( smLSN        aMinLSN,
                                         const UInt   aFirstFileNo,
                                         const UInt   aEndFileNo,
                                         UInt       * aNeedFirstFileNo )
{
    IDE_DASSERT( aFirstFileNo <= aEndFileNo );
  
    if ( aFirstFileNo <= aMinLSN.mFileNo )
    {
        if ( aEndFileNo >= aMinLSN.mFileNo )
        {
            *aNeedFirstFileNo = aMinLSN.mFileNo; 
        }
        else
        {
            /* BUG-43974 EndLSN���� ū LSN�� ��û�Ͽ��� ���
             * EndLSN�� �Ѱ��־�� �Ѵ�. */
            *aNeedFirstFileNo = aEndFileNo;
        }
    }
    else
    {
        /* BUG-15803: Replication�� �������� �α��� ��ġ�� ã����
         *  �ڽ��� mLSN���� ���� ���� ���� logfile�� ������ 
         *  ù��° ������ �����Ѵ�.*/
        *aNeedFirstFileNo = aFirstFileNo;
    }

    return IDE_SUCCESS;
}

/***********************************************************************
 * Description : ���������� ����� log��
 *               LSN���� ���� �Ѵ�.
 **********************************************************************/
void smrRemoteLogMgr::getLstLSN( smLSN * aLstLSN )
{
    //setRemoteLogMgrsInfo() ���� ���� �� ���� ���� �Ѵ�.
    *aLstLSN = mRemoteLogMgrs.mLstLSN;
};

/***********************************************************************
 * Description : aLogFile�� Close�Ѵ�.
 *
 * aLogFile - [IN] close�� �α�����
 **********************************************************************/
IDE_RC smrRemoteLogMgr::closeLogFile( smrLogFile * aLogFile )
{
    IDE_DASSERT( aLogFile != NULL );

    IDE_ASSERT( aLogFile->close() == IDE_SUCCESS );

    IDE_ASSERT( aLogFile->destroy() == IDE_SUCCESS );

    IDE_TEST( mMemPool.memfree(aLogFile) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Ư�� �α������� ù��° �α׷��ڵ��� Head�� File�κ���
 *               ���� �д´�
 *
 * aLSN     - [IN]  �о���� �α��� LSN ( Offset�� 0���� ���� )
 * aLogHead - [OUT] �о���� �α��� Header�� ���� Output Parameter
 * aIsValid - [OUT] �о���� �α��� Valid ����
 ***********************************************************************/
IDE_RC smrRemoteLogMgr::readFirstLogHeadFromDisk(smLSN      * aLSN,
                                                 smrLogHead * aLogHead,
                                                 idBool     * aIsValid )
{
    SChar   sLogFileName[SM_MAX_FILE_NAME];
    iduFile sFile;
    SInt    sState = 0;

    IDE_DASSERT(aLogHead != NULL );
    IDE_DASSERT( aIsValid != NULL );

    IDE_ASSERT( aLSN != NULL );
    IDE_ASSERT( aLSN->mOffset == 0 );

    IDE_TEST( sFile.initialize(IDU_MEM_SM_SMR,
                               1, /* Max Open FD Count */
                               IDU_FIO_STAT_OFF,
                               IDV_WAIT_INDEX_NULL )
              != IDE_SUCCESS );
    sState = 1;

    idlOS::snprintf( sLogFileName,
                     SM_MAX_FILE_NAME,
                     "%s%c%s%"ID_UINT32_FMT,
                     getLogDirPath(),
                     IDL_FILE_SEPARATOR,
                     SMR_LOG_FILE_NAME,
                     aLSN->mFileNo );

    IDE_TEST( sFile.setFileName(sLogFileName) != IDE_SUCCESS );

    IDE_TEST( sFile.open(ID_FALSE, O_RDONLY) != IDE_SUCCESS );
    sState = 2;

    // �α����� ��ü�� �ƴϰ� ù��° �α��� Head�� �о� ���δ�.
    IDE_TEST( sFile.read( NULL,
                          0,
                          (void *)aLogHead,
                         ID_SIZEOF(smrLogHead) )
              != IDE_SUCCESS );

    /*
     *  BUG-39240
     */
    // �α� ������ ù��° �α״� �������� �ʴ´�.
    // ���� :
    //     File�� ù��° Log�� LSN�� �д� �۾���
    //     ������ �����ϱ� ����
    if ( ( smrLogFile::isValidMagicNumber( aLSN, aLogHead ) == ID_TRUE ) &&
         ( smrLogComp::isCompressedLog((SChar *)aLogHead) == ID_FALSE ) )
    {
        *aIsValid = ID_TRUE;
    }
    else
    {
        *aIsValid = ID_FALSE;
    }

    sState = 1;
    IDE_TEST( sFile.close() != IDE_SUCCESS );

    sState = 0;
    IDE_TEST( sFile.destroy() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sState != 0 )
    {
        IDE_PUSH();
        switch(sState)
        {
            case 2:
                IDE_ASSERT( sFile.close() == IDE_SUCCESS );
            case 1:
                IDE_ASSERT( sFile.destroy() == IDE_SUCCESS );
            default:
                break;
        }
        IDE_POP();
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Ư�� LSN�� log record�� �ش� log record�� ���� �α�
 *               ������ �����Ѵ�.
 *
 * aDecompBufferHandle  - [IN] ���� ���� ������ �ڵ�
 * aLSN                 - [IN] log record�� �о�� LSN.
 *                             LSN���� Log File Group�� ID�� �����Ƿ�,
 *                             �̸� ���� �������� Log File Group�� �ϳ���
 *                             �����ϰ�, �� �ӿ� ��ϵ� log record�� �о�´�.
 * aIsCloseLogFile      - [IN] aLSN�� *aLogFile�� ����Ű�� LogFile�� ���ٸ�
 *                             aIsCloseLogFile�� TRUE�� ��� *aLogFile��
 *                             Close�ϰ�, ���ο� LogFile�� ����� �Ѵ�.
 *
 * aLogFile  - [IN-OUT] �α� ���ڵ尡 ���� �α����� ������
 * aLogHead  - [OUT] �α� ���ڵ��� Head
 * aLogPtr   - [OUT] �α� ���ڵ尡 ��ϵ� �α� ���� ������
 * aReadSize - [OUT] ���ϻ󿡼� �о �α��� ũ��
 *                   ( ����� �α��� ��� �α��� ũ���
 *                     ���ϻ��� ũ�Ⱑ �ٸ� �� �ִ� )
 *
 * ����: ���������� Read�ϰ� �� �� aLogFile�� ����Ű�� LogFile��
 *       smrLogMgr::readLog�� ȣ���� �ʿ��� �ݵ�� Close�ؾ��մϴ�.
 *       �׸��� aIsCloseLogFile�� ID_FALSE�� ��� �������� logfile�� open
 *       �Ǿ� ���� �� �ֱ� ������ �ݵ�� �ڽ��� ������ ������ close��
 *       ��� �մϴ�. ���� ��� redo�ÿ� ID_FALSE�� �ѱ�µ� ���⼭��
 *       closeAllLogFile�� �̿��ؼ� file�� close�մϴ�.
 *
 ***********************************************************************/
IDE_RC smrRemoteLogMgr::readLog(iduMemoryHandle * aDecompBufferHandle,
                                smLSN           * aLSN,
                                idBool            aIsCloseLogFile,
                                smrLogFile     ** aLogFile,
                                smrLogHead      * aLogHead,
                                SChar          ** aLogPtr,
                                UInt            * aLogSizeAtDisk)
{
    smrLogFile * sLogFilePtr;

    // ����� �α׸� �д� ��� aDecompBufferHandle�� NULL�� ���´�
    IDE_ASSERT( aLSN     != NULL );
    IDE_ASSERT( aLogFile != NULL );
    IDE_ASSERT( aLogHead != NULL );
    IDE_ASSERT( aLogPtr  != NULL );
    IDE_ASSERT( aLogSizeAtDisk != NULL );    

    sLogFilePtr = *aLogFile;

    if ( sLogFilePtr != NULL )
    {
        if ( aLSN->mFileNo != sLogFilePtr->getFileNo() )
        {
            if ( aIsCloseLogFile == ID_TRUE )
            {
                IDE_TEST( closeLogFile( sLogFilePtr ) != IDE_SUCCESS );
                *aLogFile = NULL;
            }


            IDE_TEST( openLogFile( aLSN->mFileNo,
                                   ID_FALSE/*ReadOnly*/,
                                   &sLogFilePtr )
                      != IDE_SUCCESS );
            *aLogFile = sLogFilePtr;
        }
        else
        {
            /* aLSN�� ����Ű�� �α״� *aLogFile�� �ִ�.*/
        }
    }
    else
    {
        IDE_TEST( openLogFile( aLSN->mFileNo,
                               ID_FALSE /*ReadOnly*/,
                               &sLogFilePtr )
                  != IDE_SUCCESS );
        *aLogFile = sLogFilePtr;
    }

    if ( mNotDecomp == ID_FALSE )
    {
        /* ����� �α������� ��� ������ Ǯ� ���ڵ带 �о�´�.  */
        IDE_TEST( smrLogComp::readLog( aDecompBufferHandle,
                                       sLogFilePtr,
                                       aLSN->mOffset,
                                       aLogHead,
                                       aLogPtr,
                                       aLogSizeAtDisk)
                 != IDE_SUCCESS );
    }
    else 
    {
        /* ����� �α������� ��� ����� ���� �״�� ��ȯ�Ѵ�. */
        IDE_TEST( smrLogComp::readLog4RP( sLogFilePtr,
                                          aLSN->mOffset,
                                          aLogHead,
                                          aLogPtr,
                                          aLogSizeAtDisk)
                 != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : aLSN�� ����Ű�� Log�� �о Log�� ��ġ�� Log Buffer��
 *                �����͸� aLogPtr�� Setting�Ѵ�. �׸��� Log�� ������ �ִ�
 *                Log���������͸� aLogFile�� Setting�Ѵ�.
 *
 *  aDecompBufferHandle - [IN] �α� ���������� ����� ������ �ڵ�
 *  aLSN                - [IN] �о���� Log Record��ġ
 *  aIsRecovery         - [IN] Recovery�ÿ� ȣ��Ǿ�����
 *                             ID_TRUE, �ƴϸ� ID_FALSE
 *
 *  aLogFile    - [IN-OUT] ���� Log���ڵ带 ������ �ִ� LogFile
 *  aLogHeadPtr - [OUT] Log�� Header�� ����
 *  aLogPtr     - [OUT] Log Record�� ��ġ�� Log������ Pointer
 *  aIsValid    - [OUT] Log�� Valid�ϸ� ID_TRUE, �ƴϸ� ID_FALSE
 *  aLogSizeAtDisk   - [OUT] ���ϻ󿡼� �о �α��� ũ��
 *                      ( ����� �α��� ��� �α��� ũ���
 *                        ���ϻ��� ũ�Ⱑ �ٸ� �� �ִ� )
 ***********************************************************************/
IDE_RC smrRemoteLogMgr::readLogAndValid(iduMemoryHandle * aDecompBufferHandle,
                                        smLSN           * aLSN,
                                        idBool            aIsCloseLogFile,
                                        smrLogFile     ** aLogFile,
                                        smrLogHead      * aLogHeadPtr,
                                        SChar          ** aLogPtr,
                                        idBool          * aIsValid,
                                        UInt            * aLogSizeAtDisk)
{
    static UInt sMaxLogOffset    = getLogFileSize()
                                   - ID_SIZEOF(smrLogHead)
                                   - ID_SIZEOF(smrLogTail);
    static UInt  sLogFileSize = smuProperty::getLogFileSize();

    // ����� �α׸� �д� ��� aDecompBufferHandle�� NULL�� ���´�
    IDE_DASSERT(aLSN            != NULL );
    IDE_DASSERT(aIsCloseLogFile == ID_TRUE ||
                aIsCloseLogFile == ID_FALSE);
    IDE_DASSERT(aLogFile        != NULL );
    IDE_DASSERT(aLogPtr         != NULL );
    IDE_DASSERT(aLogHeadPtr     != NULL );


    IDE_TEST_RAISE(aLSN->mOffset > sMaxLogOffset, error_invalid_lsn_offset);

    IDE_TEST( smrRemoteLogMgr::readLog( aDecompBufferHandle,
                                        aLSN,
                                        aIsCloseLogFile,
                                        aLogFile,
                                        aLogHeadPtr,
                                        aLogPtr,
                                        aLogSizeAtDisk)
             != IDE_SUCCESS );

    if ( aIsValid != NULL )
    {
        if ( mNotDecomp == ID_FALSE )
        {
            *aIsValid = smrLogFile::isValidLog( aLSN,
                                                aLogHeadPtr,
                                                *aLogPtr,
                                                *aLogSizeAtDisk);
        }
        else
        {
            // Log�� ������ Ǯ�� �ʾұ� ������ Size�� MagicNumber ������ �����ؾ� �Ѵ�.
            if ( ( smrLogHeadI::getSize(aLogHeadPtr) < ( ID_SIZEOF(smrLogHead) + ID_SIZEOF(smrLogTail) ) ) ||
                 ( *aLogSizeAtDisk > (sLogFileSize - aLSN->mOffset) ) )
            {
                *aIsValid = ID_FALSE;
            }
            else
            {
                /* BUG-37018 There is some mistake on logfile Offset calculation 
                 * Dummy log�� valid���� �˻��ϴ� ����� MagicNumber�˻�ۿ� ����.*/
                *aIsValid = smrLogFile::isValidMagicNumber( aLSN, aLogHeadPtr );
            }
        }
    }
    else
    {
        /* aIsValid�� NUll�̸� Valid�� Check���� �ʴ´� */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(error_invalid_lsn_offset);
    {
        IDE_SET(ideSetErrorCode(smERR_FATAL_InvalidLSNOffset,
                                aLSN->mFileNo,
                                aLSN->mOffset));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : aFileNo�� ����Ű�� LogFile�� Open�Ѵ�.
 *               aLogFilePtr�� Open�� Logfile Pointer�� Setting���ش�.
 *
 * aFileNo     - [IN]  open�� LogFile No
 * aIsWrite    - [IN]  open�� logfile�� ���� write�� �Ѵٸ� ID_TRUE, �ƴϸ�
 *                     ID_FALSE
 * aLogFilePtr - [OUT] open�� logfile�� ����Ų��.
 ***********************************************************************/
IDE_RC smrRemoteLogMgr::openLogFile( UInt          aFileNo,
                                     idBool        aIsWrite,
                                     smrLogFile ** aLogFilePtr )
{
    SChar        sLogFileName[SM_MAX_FILE_NAME];
    smrLogFile * sNewLogFile;
    idBool       sIsAlloc  = ID_FALSE;
    idBool       sIsInit   = ID_FALSE;
    idBool       sIsLocked = ID_FALSE;

    idlOS::snprintf(sLogFileName,
                    SM_MAX_FILE_NAME,
                    "%s%c%s%"ID_UINT32_FMT,
                    getLogDirPath(),
                    IDL_FILE_SEPARATOR,
                    SMR_LOG_FILE_NAME,
                    aFileNo);

    IDE_ASSERT( mMemPool.alloc((void **)&sNewLogFile) == IDE_SUCCESS );
    sIsAlloc = ID_TRUE;

    new (sNewLogFile) smrLogFile();

    IDE_ASSERT( sNewLogFile->initialize() == IDE_SUCCESS );
    sIsInit = ID_TRUE;

    sNewLogFile->mFileNo = aFileNo;

    *aLogFilePtr         = sNewLogFile;

    // �α����� list�� Mutex�� Ǯ�� �α������� open�ϴ� �۾���
    // ��� �����ϱ� ���� �ش� �α����Ͽ� Mutex�� ��´�.
    IDE_ASSERT( sNewLogFile->lock() == IDE_SUCCESS );
    sIsLocked = ID_TRUE;

    sNewLogFile->mRef++;

    IDE_TEST( sNewLogFile->open( aFileNo,
                                 sLogFileName,
                                 ID_FALSE, /* aIsMultiplexLogFile */
                                 getLogFileSize(),
                                 aIsWrite ) 
              != IDE_SUCCESS );

    sIsLocked = ID_FALSE;
    IDE_ASSERT( sNewLogFile->unlock() == IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sIsLocked == ID_TRUE )
    {
        IDE_ASSERT( sNewLogFile->unlock() == IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    if ( sIsInit == ID_TRUE )
    {
        IDE_ASSERT( sNewLogFile->destroy() == IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    if ( sIsAlloc == ID_TRUE )
    {
        (void)mMemPool.memfree( sNewLogFile );
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Check Log DIR Exist
 ***********************************************************************/
IDE_RC smrRemoteLogMgr::checkLogDirExist(void)
{
    const SChar * sLogDirPtr;

    sLogDirPtr = getLogDirPath();

    IDE_TEST_RAISE(idf::access(sLogDirPtr, F_OK) != 0,
                   err_logdir_not_exist)

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_logdir_not_exist);
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_NoExistPath, sLogDirPtr));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : aIndex�� �ش��ϴ� �α� ��θ� ���� �Ѵ�.
 *
 * aIndex - [IN] LogFile Group ID
 ***********************************************************************/
SChar * smrRemoteLogMgr::getLogDirPath()
{
    return mLogDirPath;
}

/***********************************************************************
 * Description : aIndex�� �α� ��θ� ���� �Ѵ�.
 *
 * aIndex   - [IN] LogFile Group ID
 * aDirPath - [IN] LogFile Path
 ***********************************************************************/
void smrRemoteLogMgr::setLogDirPath(SChar * aDirPath)
{
    idlOS::memcpy(mLogDirPath, aDirPath, strlen(aDirPath));
}

/***********************************************************************
 * Description : �α� ���� ����� ���� �Ѵ�.
 ***********************************************************************/
ULong smrRemoteLogMgr::getLogFileSize(void)
{
    return mLogFileSize;
}

/***********************************************************************
 * Description : �α� ���� ����� ���� �Ѵ�.
 *
 * aLogFileSize - [IN] LogFile size
 ***********************************************************************/
void smrRemoteLogMgr::setLogFileSize(ULong aLogFileSize)
{
    mLogFileSize = aLogFileSize;
}

/***********************************************************************
 * Description : Check Log File Exist
 *
 * aFileNo  - [IN]  LogFile Number
 * aIsExist - [OUT] ���� ���� ����
 ***********************************************************************/
IDE_RC smrRemoteLogMgr::isLogFileExist(UInt     aFileNo,
                                       idBool * aIsExist)
{
    SChar   sLogFilename[SM_MAX_FILE_NAME];
    iduFile sFile;
    ULong   sLogFileSize = 0;

    *aIsExist = ID_TRUE;

    idlOS::snprintf(sLogFilename,
                    SM_MAX_FILE_NAME,
                    "%s%c%s%"ID_UINT32_FMT,
                    getLogDirPath(),
                    IDL_FILE_SEPARATOR,
                    SMR_LOG_FILE_NAME,
                    aFileNo);

    if ( idf::access(sLogFilename, F_OK) != 0 )
    {
        *aIsExist = ID_FALSE;
    }
    else
    {
        IDE_TEST( sFile.initialize(IDU_MEM_SM_SMR,
                                  1, /* Max Open FD Count */
                                  IDU_FIO_STAT_OFF,
                                  IDV_WAIT_INDEX_NULL )
                 != IDE_SUCCESS );

        IDE_TEST( sFile.setFileName(sLogFilename)  != IDE_SUCCESS );

        IDE_TEST( sFile.open(ID_FALSE, O_RDONLY) != IDE_SUCCESS );
        IDE_TEST( sFile.getFileSize(&sLogFileSize) != IDE_SUCCESS );
        IDE_TEST( sFile.close()                    != IDE_SUCCESS );
        IDE_TEST( sFile.destroy()                  != IDE_SUCCESS );

        if ( sLogFileSize != getLogFileSize() )
        {
            *aIsExist = ID_FALSE;
        }
        else
        {
            /* nothing to do */
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : ù��° ���Ϲ�ȣ�� ���� ������
 *               ���� ��ȣ�� ��´�.
 *               opendir �� �̿� �Ͽ� �α� ���� ����Ʈ�� ���ϰ� ���߿�
 *               ���� ���� ���� ��ȣ, ���� ū ���� ��ȣ��
 *               ��� ������ LSN�� ���Ѵ�.
 ***********************************************************************/
IDE_RC smrRemoteLogMgr::setRemoteLogMgrsInfo()
{
    idBool          sIsExist;
    UInt            sFileNo;
    smLSN           sReadLSN;
    smrLogHead      sLogHead;

    smrLogFile    * sLogFilePtr;
    SChar         * sLogPtr;
    UInt            sLogSizeAtDisk;
    UInt            sStage = 0;
    
    SChar           sLogFilename[SM_MAX_FILE_NAME];

    idBool          sIsValid = ID_FALSE;
#ifdef DEBUG
    smLSN           sDebugLSN;
#endif

    static UInt  sLogFileSize = smuProperty::getLogFileSize();

    /* �α� ���������� ���� ������ �ڵ� */
    iduReusedMemoryHandle sDecompBufferHandle;
    IDE_TEST( sDecompBufferHandle.initialize(IDU_MEM_SM_SMR)
              != IDE_SUCCESS );
    sStage = 1;

    //���� ���� ���� ��ȣ�� ã�´� / ���� ū ���� ��ȣ�� ã�´�.
    IDE_TEST( setFstFileNoAndEndFileNo( &mRemoteLogMgrs.mFstFileNo,
                                        &mRemoteLogMgrs.mEndFileNo )
              != IDE_SUCCESS );

    /*������ ���Ͽ��� ��ȿ�� �α� ���� ã�� ������ LSN�� ���Ѵ�. */
    sLogFilePtr = NULL;
    sFileNo     = mRemoteLogMgrs.mEndFileNo;

    while (1)
    {
       IDE_TEST( isLogFileExist(sFileNo, &sIsExist) != IDE_SUCCESS );

        if ( sIsExist == ID_TRUE )
        {
            SM_SET_LSN(sReadLSN, sFileNo, 0 );
            IDE_TEST( smrRemoteLogMgr::readFirstLogHead( &sReadLSN, 
                                                         &sLogHead,
                                                         &sIsValid )
                      !=IDE_SUCCESS );

            if ( sIsValid == ID_TRUE )
            {
                sReadLSN = smrLogHeadI::getLSN( &sLogHead ); 
                if ( SM_IS_LSN_INIT( sReadLSN ) )
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
                sFileNo--;
            }
        }
        else
        {
            //�α� ������ ����.
            IDE_RAISE(ERR_FILE_NOT_FOUND);
        }
    }
    mRemoteLogMgrs.mEndFileNo = sFileNo; //�αװ� ��ȿ�� ������ ���� ��ȣ

    /*������ ���Ͽ��� SN��  LSN�� ã�´�. */
    SM_SET_LSN(sReadLSN, mRemoteLogMgrs.mEndFileNo, 0 );
    while (1)
    {
            IDE_TEST( readLog( &sDecompBufferHandle,
                               &sReadLSN,
                               ID_TRUE /* Close Log File When aLogFile doesn't include aLSN */,
                               &sLogFilePtr,
                               &sLogHead,
                               &sLogPtr,
                               &sLogSizeAtDisk )
                      != IDE_SUCCESS );

        if ( mNotDecomp == ID_FALSE )
        {
            if ( smrLogFile::isValidLog( &sReadLSN,
                                         &sLogHead,
                                         sLogPtr,
                                         sLogSizeAtDisk ) == ID_TRUE )
            {
                // BUG-29115
                // log file�� ������ FILE_END �α״� �����Ѵ�.
                if ( smrLogHeadI::getType(&sLogHead) != SMR_LT_FILE_END )
                {   
#ifdef DEBUG 
                    sDebugLSN = smrLogHeadI::getLSN( &sLogHead ); 
                    IDE_ASSERT( smrCompareLSN::isEQ( &sReadLSN, &sDebugLSN ) );
#endif
                    SM_GET_LSN( mRemoteLogMgrs.mLstLSN, sReadLSN );
                    sReadLSN.mOffset += sLogSizeAtDisk;
                }
                else
                {
                    /* SMR_LT_FILE_END */
                    break;
                }
            }
            else
            {
                /* invalid Log */
                break;
            }
        } 
        else
        {
            sIsValid = ID_TRUE;
            /* Log�� ������ Ǯ�� �ʾұ� ������ Size�� MagicNumber ������ �����ؾ� �Ѵ�.  */
            if ( ( smrLogHeadI::getSize( &sLogHead ) < ( ID_SIZEOF(smrLogHead) + ID_SIZEOF(smrLogTail) ) ) ||
                 ( sLogSizeAtDisk > ( sLogFileSize - sReadLSN.mOffset) ) )
            {
                sIsValid = ID_FALSE;
            }
            else
            {
                sIsValid = smrLogFile::isValidMagicNumber( &sReadLSN, &sLogHead );
            }
            
            if( sIsValid == ID_TRUE )
            {
                if ( ( smrLogHeadI::getFlag( &sLogHead ) & SMR_LOG_COMPRESSED_MASK )
                       == SMR_LOG_COMPRESSED_OK )
                {
#ifdef DEBUG 
                    sDebugLSN = smrLogHeadI::getLSN( &sLogHead ); 
                    IDE_ASSERT( smrCompareLSN::isEQ( &sReadLSN, &sDebugLSN ) );
#endif
                    SM_GET_LSN( mRemoteLogMgrs.mLstLSN, sReadLSN )
                    sReadLSN.mOffset += sLogSizeAtDisk;
                }
                else
                {
                    if ( smrLogHeadI::getType( &sLogHead ) != SMR_LT_FILE_END )
                    {
#ifdef DEBUG 
                        sDebugLSN = smrLogHeadI::getLSN( &sLogHead ); 
                        IDE_ASSERT( smrCompareLSN::isEQ( &sReadLSN, &sDebugLSN ) );
#endif
                        SM_GET_LSN( mRemoteLogMgrs.mLstLSN, sReadLSN );
                        sReadLSN.mOffset += sLogSizeAtDisk;
                    }
                    else
                    {
                        /* SMR_LT_FILE_END */
                        break;
                    }
                }
            }
            else
            {
                /* invalod Log */
                break;
            }
        }
    }// while

    closeLogFile(sLogFilePtr);

    sStage = 0;
    IDE_TEST( sDecompBufferHandle.destroy() != IDE_SUCCESS );

    return IDE_SUCCESS;
    
    IDE_EXCEPTION(ERR_FILE_NOT_FOUND);
    {
        idlOS::snprintf(sLogFilename,
                        SM_MAX_FILE_NAME,
                        "%s%c%s%"ID_UINT32_FMT,
                        getLogDirPath(),
                        IDL_FILE_SEPARATOR,
                        SMR_LOG_FILE_NAME,
                        sFileNo);
        
        IDE_SET(ideSetErrorCode(smERR_ABORT_NoExistFile, 
                                sLogFilename));
                                
    }

    IDE_EXCEPTION_END;

    IDE_PUSH();

    switch(sStage)
    {
        case 1 :
            IDE_ASSERT( sDecompBufferHandle.destroy() == IDE_SUCCESS );
            break;
        default:
            break;
    }

    IDE_POP();

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : setRemoteLogMgrsInfo �Լ� ���� ȣ�� �ȴ�.
 *               �ش� �ϴ� ��ο��� ���� ����Ʈ �� �α� ���� ����
 *               �ּ� ���� ��ȣ �ִ� ���� ��ȣ�� ���Ѵ�.
 *
 * aFstFileNo - [OUT] Log File Group ���� ���� ���� �α� ���� ��ȣ
 * aEndFileNo - [OUT] Log File Group ���� ���� ū �α� ���� ��ȣ
 ***********************************************************************/
IDE_RC smrRemoteLogMgr::setFstFileNoAndEndFileNo(UInt * aFstFileNo,
                                                 UInt * aEndFileNo)
{
    DIR           * sDirp = NULL;
    struct dirent * sDir  = NULL;
    UInt            sCurLogFileNo;
    idBool          sIsSetTempLogFileName = ID_FALSE;
    idBool          sIsLogFile;
    UInt            sFirstFileNo = 0;
    UInt            sEndFileNo = 0;

    sDirp = idf::opendir(getLogDirPath());
    IDE_TEST_RAISE(sDirp == NULL, error_open_dir);

    while(1)
    {
        sDir = idf::readdir(sDirp);
        if ( sDir == NULL )
        {
            break;
        }

        sCurLogFileNo = chkLogFileAndGetFileNo(sDir->d_name, &sIsLogFile);
        
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

    IDE_TEST_RAISE(sIsSetTempLogFileName != ID_TRUE, error_no_log_file);

    *aFstFileNo = sFirstFileNo;
    *aEndFileNo = sEndFileNo;

    return IDE_SUCCESS;

    IDE_EXCEPTION(error_open_dir);
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_CannotOpenDir));
    }
    IDE_EXCEPTION(error_no_log_file);
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_NOT_FOUND_LOGFILE, getLogDirPath() ) );
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : ��� �α� ���� ��ȣ���� ���� ���� ��ȣ�� ���� �Ѵ�.
 *
 * aFileNo - [OUT] ù��° �α� ���� ��ȣ 
 ***********************************************************************/
void smrRemoteLogMgr::getFirstFileNo(UInt * aFileNo)
{
    *aFileNo = mRemoteLogMgrs.mFstFileNo;

    return;
}

/***********************************************************************
 * Description : logfile���� �α� ���� prefix�� ���� �ϰ� ��ȣ�� ��ȯ �Ѵ�.
 *
 * aFileName  - [IN]  �α� ���� �̸�
 * aIsLogFile - [OUT] ���� �̸��� �α� ���� ���� �� �ƴҰ�� ID_FALSE
 ***********************************************************************/
UInt smrRemoteLogMgr::chkLogFileAndGetFileNo(SChar  * aFileName,
                                             idBool * aIsLogFile)
{
    UInt   i;
    UInt   sFileNo = 0;
    SChar  sChar;
    UInt   sPrefixLen;
    idBool sIsLogFile; 

    sIsLogFile = ID_TRUE;

    sPrefixLen  = idlOS::strlen(SMR_LOG_FILE_NAME);

    if ( idlOS::strncmp(aFileName, SMR_LOG_FILE_NAME, sPrefixLen) == 0 )
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

    *aIsLogFile = sIsLogFile;

    return sFileNo;
}
