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
 * $$Id: smrLogMgr.cpp 88191 2020-07-27 03:08:54Z mason.lee $
 *
 * �αװ����� ���������Դϴ�.
 *
 * �α״� �� �������� ��� �ڶ󳪴�, Durable�� ��������̴�.
 * ������ Durable�� ��ü�� ���������� ����ϴ� Disk�� ���,
 * �� �뷮�� �����Ǿ� �־, �α׸� ������ �ڶ󳪰� ����� ���� ����.
 *
 * �׷��� �������� �������� �α����ϵ��� �������� �ϳ��� �α׷�
 * ����� �� �ֵ��� ������ �ϴµ�,
 * �� �� �ʿ��� �������� �α������� smrLogFile �� ǥ���Ѵ�.
 * �������� �α����ϵ��� ����Ʈ�� �����ϴ� ������ smrLogFileMgr�� ����ϰ�,
 * �α������� Durable�� �Ӽ��� ������Ű�� ������ smrLFThread�� ����Ѵ�.
 * �������� �������� �α����ϵ���
 * �ϳ��� ������ �α׷� �߻�ȭ �ϴ� ������ smrLogMgr�� ����Ѵ�.
 **********************************************************************/

#include <smErrorCode.h>
#include <smDef.h>
#include <smm.h>
#include <smu.h>
#include <smr.h>
#include <smrReq.h>
#include <sct.h>
#include <smxTrans.h>

iduMutex smrLogMgr::mMtxLoggingMode;
UInt     smrLogMgr::mMaxLogOffset;

smrLFThread                smrLogMgr::mLFThread;
smrArchThread              smrLogMgr::mArchiveThread;
const SChar*               smrLogMgr::mLogPath ;
const SChar*               smrLogMgr::mArchivePath ;
iduMutex                   smrLogMgr::mMutex;
smrLogFile*                smrLogMgr::mCurLogFile;
smrLSN4Union               smrLogMgr::mLstLSN;
smrLSN4Union               smrLogMgr::mLstWriteLSN;
smrLogFileMgr              smrLogMgr::mLogFileMgr;
smrFileBeginLog            smrLogMgr::mFileBeginLog;
smrFileEndLog              smrLogMgr::mFileEndLog;
UInt                       smrLogMgr::mUpdateTxCount;
UInt                       smrLogMgr::mLogSwitchCount;
iduMutex                   smrLogMgr::mLogSwitchCountMutex;
idBool                     smrLogMgr::mAvailable;
/* BUG-35392 */
iduMutex                   smrLogMgr::mMutex4NullTrans;
smrUCSNChkThread           smrLogMgr::mUCSNChkThread;
smrUncompletedLogInfo      smrLogMgr::mUncompletedLSN;
smrUncompletedLogInfo    * smrLogMgr::mFstChkLSNArr;
UInt                       smrLogMgr::mFstChkLSNArrSize;
UInt                     * smrLogMgr::mFstChkLSNUpdateCnt;

smrCompResPool             smrLogMgr::mCompResPool;

/***********************************************************************
 * Description : �α��� �ʱ�ȭ
 **********************************************************************/
IDE_RC smrLogMgr::initialize()
{
    UInt        sState  = 0;
    UInt        i;
    UInt        sFstChkLSNUpdateCntSize;
    

    mLogPath        = smuProperty::getLogDirPath();
    mArchivePath    = smuProperty::getArchiveDirPath();
    mLogSwitchCount = 0;

    IDE_TEST( checkLogDirExist() != IDE_SUCCESS );
 
    IDE_TEST( mLogSwitchCountMutex.initialize((SChar*)"LOG_SWITCH_COUNT_MUTEX",
                                              IDU_MUTEX_KIND_NATIVE,
                                              IDV_WAIT_INDEX_NULL)
              != IDE_SUCCESS );

    // static����� �ʱ�ȭ
    IDE_TEST( initializeStatic() != IDE_SUCCESS );

    // ���� �α����� �ʱ�ȭ
    mCurLogFile = NULL;

    mUpdateTxCount = 0;
    
    // ������ LSN�ʱ�ȭ
    SM_LSN_INIT( mLstWriteLSN.mLSN );
    // ������ LSN�ʱ�ȭ
    SM_LSN_INIT( mLstLSN.mLSN );

    // ���� �α������� ���ü� ��� ���� Mutex�ʱ�ȭ
    IDE_TEST( mMutex.initialize((SChar*)"LOG_ALLOCATION_MUTEX",
                                (iduMutexKind)smuProperty::getLogAllocMutexType(),
                                IDV_WAIT_INDEX_NULL )
              != IDE_SUCCESS );
    sState = 1;

    /* BUG-35392 */
    IDE_TEST( mMutex4NullTrans.initialize((SChar *)"LOG_FILE_GROUP_MUTEX_FOR_NULL_TRANS",
                                         IDU_MUTEX_KIND_NATIVE,
                                         IDV_WAIT_INDEX_NULL ) != IDE_SUCCESS );
    sState = 2;

    /* BUG-35392 */
    if ( smuProperty::isFastUnlockLogAllocMutex() == ID_TRUE )
    {
        /* Null Transaction�� 1 �߰� */
        mFstChkLSNArrSize = smLayerCallback::getCurTransCnt() + 1; 

        /* smrLogMgr_initialize_malloc_mFstChkLSNArr.tc */
        IDU_FIT_POINT("smrLogMgr::initialize::malloc::mFstChkLSNArr");
        IDE_TEST( iduMemMgr::malloc(IDU_MEM_SM_SMR,
                                   (ULong)ID_SIZEOF( smrUncompletedLogInfo ) * mFstChkLSNArrSize,
                                   (void **)&mFstChkLSNArr) != IDE_SUCCESS );
        sState = 3;

        for ( i = 0 ; i < mFstChkLSNArrSize ; i++)
        {
            SM_LSN_MAX( mFstChkLSNArr[i].mLstLSN.mLSN );
            SM_LSN_MAX( mFstChkLSNArr[i].mLstWriteLSN.mLSN );
        }
    }    

    // �α������� �� ó���� ����� FileBeginLog�� �ʱ�ȭ
    initializeFileBeginLog ( &mFileBeginLog );
    // �α������� �� ���� ����� FileEndLog�� �ʱ�ȭ
    initializeFileEndLog   ( &mFileEndLog );

    /******* �α� ���� �Ŵ����� ���� ��������� ����  ********/

    // �α����� ������ �ʱ�ȭ �� Prepare ������ ����
    // ����! initialize�Լ��� �Ҹ�� �����带 ���۽�Ű�� �ʴ´�.
    IDE_TEST( mLogFileMgr.initialize( mLogPath,
                                      mArchivePath, 
                                      &mLFThread )
              != IDE_SUCCESS );
    sState = 4;

    // ��ī�̺� ������ ��ü�� �׻� �ʱ�ȭ�Ѵ�.
    // ( mLFThread�� ��ī�̺� �����带 �����ϱ� ���� )
    // �α����� ��ī�̺� ������ ��ü �ʱ�ȭ
    // ����! ��ī�̺� ������� initialize�Լ��� �Ҹ��
    // �����带 ���۽�Ű�� �ʴ´�.
    //
    // smiMain.cpp���� ������ �ڵ����� ������Ƽ��
    // smuProperty::getArchiveThreadAutoStart() �� ������ ��쿡��
    // ������ ��ī�̺� �����带 startup��Ų��.
    IDE_TEST( mArchiveThread.initialize( mArchivePath,
                                         &mLogFileMgr,
                                         smrRecoveryMgr::getLstDeleteLogFileNo() )
              != IDE_SUCCESS );
    sState = 5;

    /* BUG-35392 */
    if ( smuProperty::isFastUnlockLogAllocMutex() == ID_TRUE )
    {
        sFstChkLSNUpdateCntSize = ( mFstChkLSNArrSize / SMR_CHECK_LSN_UPDATE_GROUP ) + 1; 

        IDE_TEST( iduMemMgr::malloc( IDU_MEM_SM_SMR,
                                     (ULong)ID_SIZEOF( UInt ) * sFstChkLSNUpdateCntSize,
                                     (void **)&mFstChkLSNUpdateCnt ) != IDE_SUCCESS );
        sState = 6;
        
        idlOS::memset( mFstChkLSNUpdateCnt, 0x00, ID_SIZEOF( UInt ) * sFstChkLSNUpdateCntSize );
    }
    else
    {
        /* nothing to do */
    }

    // �α����� Flush ������ �ʱ�ȭ�� ����
    ideLog::log(IDE_SERVER_0," [SM-PREPARE] Log Flush Thread Startup");
    IDE_TEST( mLFThread.initialize( &mLogFileMgr,
                                    &mArchiveThread )
              != IDE_SUCCESS );

    /* BUG-35392 */
    if ( smuProperty::isFastUnlockLogAllocMutex() == ID_TRUE )
    {
        /* Uncompleted LSN�� �����ϴ� Thread�� Init & Start �Ѵ�. */
        IDE_TEST( mUCSNChkThread.initialize() != IDE_SUCCESS );
    }
    else
    {
        /* nothing to do */
    }

    mAvailable = ID_TRUE;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();

    switch( sState )
    {
        case 6:
            if ( smuProperty::isFastUnlockLogAllocMutex() == ID_TRUE )
            {
                IDE_ASSERT( iduMemMgr::free(mFstChkLSNUpdateCnt) == IDE_SUCCESS ); 
            }
        case 5:
            IDE_ASSERT( mArchiveThread.destroy() == IDE_SUCCESS );
        case 4:
            IDE_ASSERT( mLogFileMgr.destroy() == IDE_SUCCESS );
        case 3:
            /* BUG-35392 */
            if ( smuProperty::isFastUnlockLogAllocMutex() == ID_TRUE )
            {
                IDE_ASSERT( iduMemMgr::free(mFstChkLSNArr) == IDE_SUCCESS );
            }
        case 2:
            IDE_ASSERT( mMutex4NullTrans.destroy() == IDE_SUCCESS );
        case 1:
            IDE_ASSERT( mMutex.destroy() == IDE_SUCCESS );
        default:
            break;
    }

    IDE_POP();

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : �α��� ����
 **********************************************************************/
IDE_RC smrLogMgr::destroy()
{
    mAvailable = ID_FALSE;

    /* BUG-35392 */
    if ( smuProperty::isFastUnlockLogAllocMutex() == ID_TRUE )
    {
        /* UncompletedLSN�� ���ϴ� Thread Stop & Destroy */
        IDE_TEST( mUCSNChkThread.destroy() != IDE_SUCCESS );

        IDE_TEST( iduMemMgr::free(mFstChkLSNUpdateCnt) != IDE_SUCCESS ); 
    }
    else
    {
        /* nothing to do ... */
    }

    // To Fix BUG-14185
    // initialize���� ��ī�̺� ���� �������
    // ��ī�̺� �����尡 �׻� �ʱ�ȭ �Ǿ��ֱ� ������
    // �׻� destroy ���־�� ��
    IDE_TEST( mArchiveThread.destroy() != IDE_SUCCESS );

    // �α����� Flush ������ ����
    IDE_TEST( mLFThread.destroy() != IDE_SUCCESS );

    // �α����� ������ �� Prepare ������ ����
    IDE_TEST( mLogFileMgr.destroy() != IDE_SUCCESS );

    // ���� �α������� ���ü� ��� ���� Mutex ����
    IDE_TEST( mMutex.destroy() != IDE_SUCCESS );

    /* BUG-35392 */
    IDE_TEST( mMutex4NullTrans.destroy() != IDE_SUCCESS );

    /* BUG-35392 */
    if ( smuProperty::isFastUnlockLogAllocMutex() == ID_TRUE )
    {
        IDE_TEST( iduMemMgr::free( mFstChkLSNArr ) != IDE_SUCCESS );
    }

    IDE_TEST( destroyStatic() != IDE_SUCCESS );

    IDE_TEST( mLogSwitchCountMutex.destroy() != IDE_SUCCESS );


    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

IDE_RC smrLogMgr::initializeStatic()
{
    const  SChar* sLogDirPtr;
    // �α� ���丮�� �ִ��� Ȯ��  
    sLogDirPtr = smuProperty::getLogDirPath();
        
    IDE_TEST_RAISE( idf::access(sLogDirPtr, F_OK) != 0,
                    err_logdir_not_exist )
    
    /* ------------------------------------------------
     * �αװ������� mutex �ʱ�ȭ
     * - �α� ��� mutex �ʱ�ȭ
     * ----------------------------------------------*/
    IDE_TEST( mMtxLoggingMode.initialize((SChar*)"LOG_MODE_MUTEX",
                                         IDU_MUTEX_KIND_NATIVE,
                                         IDV_WAIT_INDEX_NULL ) != IDE_SUCCESS );
    IDE_TEST( smuDynArray::initializeStatic(SMU_DA_BASE_SIZE)
             != IDE_SUCCESS );

    // BUG-29329 ���� Codding Convention������
    // static ���� �������� static ��������� ����
    mMaxLogOffset = smuProperty::getLogFileSize() - ID_SIZEOF(smrLogHead)
                    - ID_SIZEOF(smrLogTail);


    /* BUG-31114 mismatch between real log type and display log type
     *           in dumplf.
     * LogType������ ����, �ѹ� �ʱ�ȭ �غ���. ������ Ȯ���� �޸� �̱�
     * ������, �ʱ�ȭ ���شٰ� �ؼ� �߰����� �޸𸮸� ���� �ʴ´�. */

    IDE_TEST( smrLogFileDump::initializeStatic() != IDE_SUCCESS );

    IDE_TEST( smrLogFile::initializeStatic( (UInt)smuProperty::getLogFileSize())
              != IDE_SUCCESS );

    IDE_TEST( mCompResPool.initialize(
                                  (SChar*)"NONTRANS_LOG_COMP_RESOURCE_POOL",
                                  1,   // aInitialResourceCount
                                  1,   // aMinimumResourceCount
                                  smuProperty::getCompressionResourceGCSecond() )
              != IDE_SUCCESS );


    return IDE_SUCCESS;

    IDE_EXCEPTION(err_logdir_not_exist);
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_NoExistPath, sLogDirPtr));
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC smrLogMgr::destroyStatic()
{
    IDE_TEST( mCompResPool.destroy() != IDE_SUCCESS );

    // Do nothing
    IDE_TEST( smrLogFile::destroyStatic() != IDE_SUCCESS );

    // Do nothing
    IDE_TEST( smrLogFileDump::destroyStatic() != IDE_SUCCESS );

    IDE_TEST( smuDynArray::destroyStatic() != IDE_SUCCESS );

    IDE_TEST( mMtxLoggingMode.destroy() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 * Description :  �� �Լ��� createdb�ÿ� �θ��� �ȴ�.
 **********************************************************************/
IDE_RC smrLogMgr::create()
{
    IDE_TEST( checkLogDirExist() != IDE_SUCCESS );
    
    IDE_TEST( smrLogFileMgr::create( smuProperty::getLogDirPath() ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : �α� ���� �Ŵ����� ���� ��������� ����
 *
 * smrLogMgr::initialize���� ���� �Ͱ� �ݴ��� ������ �����Ѵ�.
 **********************************************************************/
IDE_RC smrLogMgr::shutdown()
{
    if ( mLFThread.isStarted() == ID_TRUE )
    {
        // �α����� Flush ������ ����

        ideLog::log(IDE_SERVER_0,"      [SM-PREPARE] Log Flush Thread Shutdown...");
        {
            IDE_TEST( mLFThread.shutdown() != IDE_SUCCESS );
        }
        ideLog::log(IDE_SERVER_0,"[SUCCESS]\n");
    }
    else
    {
        /* nothing to do */
    }

    // �α����� ������ �� Prepare ������ ����

    if ( mLogFileMgr.isStarted() == ID_TRUE )
    {
        ideLog::log(IDE_SERVER_0,"      [SM-PREPARE] Log Prepare Thread Shutdown...");
        {
            IDE_TEST( mLogFileMgr.shutdown() != IDE_SUCCESS );
        }
        ideLog::log(IDE_SERVER_0,"[SUCCESS]\n");
    }
    else
    {
        /* nothing to do */
    }

    // ��ī�̺� ������ ����
    if ( smrRecoveryMgr::getArchiveMode() == SMI_LOG_ARCHIVE )
    {
        if ( mArchiveThread.isStarted() == ID_TRUE )
        {

            ideLog::log(IDE_SERVER_0,"      [SM-PREPARE] Log Archive Thread Shutdown...");
            {
                IDE_TEST( mArchiveThread.shutdown() != IDE_SUCCESS );
            }
            ideLog::log(IDE_SERVER_0,"[SUCCESS]\n");

            // ��ī�̺긦 ���������� �ѹ� �� �ǽ��Ͽ�
            // ��� ��ī�̺갡 �������� �Ѵ�.
            IDE_TEST( mArchiveThread.archLogFile() != IDE_SUCCESS );
        }
    }
    else
    {
        /* nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Check Log DIR Exist
 *
 ***********************************************************************/
IDE_RC smrLogMgr::checkLogDirExist()
{
    const  SChar* sLogDirPtr;
    
    sLogDirPtr = smuProperty::getLogDirPath();
        
    IDE_TEST_RAISE( idf::access(sLogDirPtr, F_OK) != 0,
                    err_logdir_not_exist )
    
    return IDE_SUCCESS;

    IDE_EXCEPTION(err_logdir_not_exist);
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_NoExistPath, sLogDirPtr));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/********************************************************************
 * aLSN�� ����Ű�� Log�� �о Log�� ��ġ�� Log Buffer��
 * �����͸� aLogPtr�� Setting�Ѵ�. �׸��� Log�� ������ �ִ�
 * Log���������͸� aLogFile�� Setting�Ѵ�.
 *
 * [IN]  aDecompBufferHandle - �α� ���������� ����� ������ �ڵ�
 * [IN]  aLSN        - �о���� Log Record��ġ
 * [IN]  aIsRecovery - Recovery�ÿ� ȣ��Ǿ�����
 *                     ID_TRUE, �ƴϸ� ID_FALSE
 * [INOUT] aLogFile  - ���� Log���ڵ带 ������ �ִ� LogFile
 * [OUT] aLogHeadPtr - Log�� Header�� ����
 * [OUT] aLogPtr     - Log Record�� ��ġ�� Log������ Pointer
 * [OUT] aIsValid    - Log�� Valid�ϸ� ID_TRUE, �ƴϸ� ID_FALSE
 * [OUT] aLogSizeAtDisk     - ���ϻ󿡼� �о �α��� ũ��
 *                      ( ����� �α��� ��� �α��� ũ���
 *                        ���ϻ��� ũ�Ⱑ �ٸ� �� �ִ� )
 *******************************************************************/
IDE_RC smrLogMgr::readLog( iduMemoryHandle  * aDecompBufferHandle,
                           smLSN            * aLSN,
                           idBool             aIsCloseLogFile,
                           smrLogFile      ** aLogFile,
                           smrLogHead       * aLogHeadPtr,
                           SChar           ** aLogPtr,
                           idBool           * aIsValid,
                           UInt             * aLogSizeAtDisk)
{
    UInt      sOffset   = 0;

    // ����� �α׸� �д� ��� aDecompBufferHandle�� NULL�� ���´�

    IDE_ASSERT( aLSN        != NULL );
    IDE_ASSERT( (aIsCloseLogFile == ID_TRUE) ||
                (aIsCloseLogFile == ID_FALSE) );
    IDE_ASSERT( aLogFile    != NULL );
    IDE_ASSERT( aLogPtr     != NULL );
    IDE_ASSERT( aLogHeadPtr != NULL );
    IDE_ASSERT( aLogSizeAtDisk != NULL );
     
    // BUG-29329 �� ���� ����� �ڵ� �߰�
    // aLSN->mOffset�� sMaxLogOffset���� ũ�ٰ� �Ǵ��Ͽ� Fatal�߻�.
    // ������, FATAL err Msg�� ��ϵ� Offset�� �����̾���.
    // ó�� ���߿� ���� ����Ǵ����� Ȯ���մϴ�.
    sOffset = aLSN->mOffset;

    // BUG-20062
    IDE_TEST_RAISE(aLSN->mOffset > getMaxLogOffset(), error_invalid_lsn_offset);

    IDE_TEST( readLogInternal( aDecompBufferHandle,
                               aLSN,
                               aIsCloseLogFile,
                               aLogFile,
                               aLogHeadPtr,
                               aLogPtr,
                               aLogSizeAtDisk )
              != IDE_SUCCESS );

    if ( aIsValid != NULL )
    {
        /* BUG-35392 */
        if ( smrLogHeadI::isDummyLog( aLogHeadPtr ) == ID_FALSE )
        {
            *aIsValid = smrLogFile::isValidLog( aLSN,
                                                aLogHeadPtr,
                                                *aLogPtr,
                                                *aLogSizeAtDisk );
        }
        else
        {
            /* BUG-37018 There is some mistake on logfile Offset calculation 
             * Dummy log�� valid���� �˻��ϴ� ����� MagicNumber�˻�ۿ� ����.*/
            *aIsValid = smrLogFile::isValidMagicNumber( aLSN, aLogHeadPtr );
        }
    }
    else
    {
        /* aIsValid�� NULL�̸� Valid�� Check���� �ʴ´� */
    }

    // BUG-29329 �� ���� ����� �ڵ� �߰�
    IDE_TEST_RAISE( ( smuProperty::getLogFileSize() 
                      - ID_SIZEOF(smrLogHead)
                      - ID_SIZEOF(smrLogTail) ) 
                    != getMaxLogOffset(),
                    error_invalid_lsn_offset);

    IDE_TEST_RAISE(sOffset != aLSN->mOffset, error_invalid_lsn_offset);

    return IDE_SUCCESS;

    IDE_EXCEPTION(error_invalid_lsn_offset);
    {
        // BUG-29329 �� ���� ����� �ڵ� �߰�
        ideLog::log(IDE_SERVER_0,
                    SM_TRC_DRECOVER_INVALID_LOG_OFFSET,
                    sOffset,
                    getMaxLogOffset(),
                    smuProperty::getLogFileSize());

        IDE_SET( ideSetErrorCode( smERR_FATAL_InvalidLSNOffset,
                                  aLSN->mFileNo,
                                  aLSN->mOffset) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC smrLogMgr::readLog4RP( smLSN            * aLSN,
                              idBool             aIsCloseLogFile,
                              smrLogFile      ** aLogFile,
                              smrLogHead       * aLogHeadPtr,
                              SChar           ** aLogPtr,
                              idBool           * aIsValid,
                              UInt             * aLogSizeAtDisk )
{
    UInt         sOffset      = 0;
    static UInt  sLogFileSize = smuProperty::getLogFileSize();


    IDE_ASSERT( aLSN        != NULL );
    IDE_ASSERT( (aIsCloseLogFile == ID_TRUE) ||
                (aIsCloseLogFile == ID_FALSE) );
    IDE_ASSERT( aLogFile    != NULL );
    IDE_ASSERT( aLogPtr     != NULL );
    IDE_ASSERT( aLogHeadPtr != NULL );
    IDE_ASSERT( aLogSizeAtDisk != NULL );
     
    // BUG-29329 �� ���� ����� �ڵ� �߰�
    // aLSN->mOffset�� sMaxLogOffset���� ũ�ٰ� �Ǵ��Ͽ� Fatal�߻�.
    // ������, FATAL err Msg�� ��ϵ� Offset�� �����̾���.
    // ó�� ���߿� ���� ����Ǵ����� Ȯ���մϴ�.
    sOffset = aLSN->mOffset;

    // BUG-20062
    IDE_TEST_RAISE(aLSN->mOffset > getMaxLogOffset(), error_invalid_lsn_offset);

    IDE_TEST( readLogInternal4RP( aLSN,
                                  aIsCloseLogFile,
                                  aLogFile,
                                  aLogHeadPtr,
                                  aLogPtr,
                                  aLogSizeAtDisk )
              != IDE_SUCCESS );

    if ( aIsValid != NULL )
    {
        // Log�� ������ Ǯ�� �ʾұ� ������ Size�� MagicNumber ������ �����ؾ� �Ѵ�.
        if ( ( smrLogHeadI::getSize(aLogHeadPtr) < ( ID_SIZEOF(smrLogHead) + ID_SIZEOF(smrLogTail) ) ) ||
             ( *aLogSizeAtDisk > (sLogFileSize - aLSN->mOffset) ) )
        {
            *aIsValid = ID_FALSE;
        }

        /* BUG-37018 There is some mistake on logfile Offset calculation 
         * Dummy log�� valid���� �˻��ϴ� ����� MagicNumber�˻�ۿ� ����.*/
        *aIsValid = smrLogFile::isValidMagicNumber( aLSN, aLogHeadPtr );
    }
    else
    {
        /* aIsValid�� NULL�̸� Valid�� Check���� �ʴ´� */
    }

    // BUG-29329 �� ���� ����� �ڵ� �߰�
    IDE_TEST_RAISE( ( smuProperty::getLogFileSize() 
                      - ID_SIZEOF(smrLogHead)
                      - ID_SIZEOF(smrLogTail) ) 
                    != getMaxLogOffset(),
                    error_invalid_lsn_offset);

    IDE_TEST_RAISE(sOffset != aLSN->mOffset, error_invalid_lsn_offset);

    return IDE_SUCCESS;

    IDE_EXCEPTION(error_invalid_lsn_offset);
    {
        // BUG-29329 �� ���� ����� �ڵ� �߰�
        ideLog::log(IDE_SERVER_0,
                    SM_TRC_DRECOVER_INVALID_LOG_OFFSET,
                    sOffset,
                    getMaxLogOffset(),
                    smuProperty::getLogFileSize());

        IDE_SET( ideSetErrorCode( smERR_FATAL_InvalidLSNOffset,
                                  aLSN->mFileNo,
                                  aLSN->mOffset) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC smrLogMgr::readLogInternal4RP( smLSN            * aLSN,
                                      idBool             aIsCloseLogFile,
                                      smrLogFile      ** aLogFile,
                                      smrLogHead       * aLogHead,
                                      SChar           ** aLogPtr,
                                      UInt             * aLogSizeAtDisk )
{
    IDE_DASSERT( aLSN     != NULL );
    IDE_DASSERT( aLogFile != NULL );
    IDE_DASSERT( aLogHead != NULL );
    IDE_DASSERT( aLogPtr  != NULL );
    IDE_DASSERT( aLogSizeAtDisk != NULL );
    
    smrLogFile    * sLogFilePtr;

    sLogFilePtr = *aLogFile;
   
    if ( sLogFilePtr != NULL )
    {
        if ( (aLSN->mFileNo != sLogFilePtr->getFileNo()) )
        {
            if ( aIsCloseLogFile == ID_TRUE )
            {
                IDE_TEST( closeLogFile(sLogFilePtr) != IDE_SUCCESS );
                *aLogFile = NULL; 
            }
            else
            {
                /* nothing to do ... */
            }

            /* FIT/ART/rp/Bugs/BUG-17185/BUG-17185.tc */
            IDU_FIT_POINT( "1.BUG-17185@smrLFGMgr::readLog" );

            IDE_TEST( openLogFile( aLSN->mFileNo,
                                   ID_FALSE,
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
                               ID_FALSE,
                               &sLogFilePtr )
                  != IDE_SUCCESS );
        *aLogFile = sLogFilePtr;
    }

    IDE_TEST( smrLogComp::readLog4RP( sLogFilePtr,
                                      aLSN->mOffset,
                                      aLogHead,
                                      aLogPtr,
                                      aLogSizeAtDisk )
              != IDE_SUCCESS );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

/***********************************************************************
 * Description : disk �α׸� �α׹��ۿ� ����Ѵ�.
 *
 * mtx�� commit�������� NTA�� �������� ���� ��쿡 ȣ��Ǵ�
 * �Լ��ν�, SMR_DLT_REDOONLY�̳� SMR_DLT_UNDOABLE Ÿ����
 * �α׸� ����Ѵ�.
 *
 * �α� header�� �����ϱ� ���� ��� aIsDML, aReplicate, aUndable
 * ���� ���¿� ���� �α� ����� ������ ���� �ٸ���.
 * ��, Ʈ������� logFlag�� ���󼭵� �ٸ���.
 * replication�� ������ ��, isDML�� true�� ��쿡 N�� ���ؼ���
 * replicator�� �ǵ��ؾ� �� �α׸� ��Ÿ����.
 *
 * - �α������ ������ �� (N:NORMAL, R:REPL_NONE, C:REPL_RECOVERY)
 *   ___________________________________________________
 *             | insert   | delete   | update  | etc.
 *   __________|__________|__________|_________|_______
 *   mIsDML    | T        | T        | T       |   F
 *             |          |          |         |
 *   mReplicate| T   F    | T   F    | T   F   |  (F)
 *             |          |          |         |
 *   mUndoable | T F T F  | T F T F  | T F T F |  (F)
 *  ___________|__________|__________|_________|_______
 *  N-Trans���  R N R R    R N R R    N N R R     R
 *  C-Trans���  R C R R    R C R R    C C R R     R
 *  R-Trans���  R R R R    R R R R    R R R R     R
 *
 * - 2nd. code design
 *   + Ʈ����� �α� ���� �ʱ�ȭ
 *   + �α� header ����
 *   + ������ �α� header�� Ʈ����� �α׹��ۿ� ���
 *   + mtx �α� ���ۿ� ����� �α׸� Ʈ����� �α׹��ۿ� ���
 *   + �α��� tail�� �����Ѵ�.
 *   + ���������� Ʈ����� �α� ���ۿ� �ۼ��� �α׸� ���
 *     �α����Ͽ� ����Ѵ�.
 **********************************************************************/
IDE_RC smrLogMgr::writeDiskLogRec( idvSQL           * aStatistics,
                                   void             * aTrans,
                                   smLSN            * aPrvLSN,
                                   smuDynArrayBase  * aLogBuffer,
                                   UInt               aWriteOption,
                                   UInt               aLogAttr,
                                   UInt               aContType,
                                   UInt               aRefOffset,
                                   smOID              aTableOID,
                                   UInt               aRedoType,
                                   smLSN            * aBeginLSN,
                                   smLSN            * aEndLSN )
{

    UInt         sLength;
    UInt         sLogTypeFlag;
    smTID        sTransID;
    smrDiskLog   sDiskLog;
    smrLogType   sLogType;
    void       * sTableHeader;

    IDE_DASSERT( aTrans != NULL );
    IDE_DASSERT( aLogBuffer != NULL );
    IDE_DASSERT( aBeginLSN != NULL );
    IDE_DASSERT( aEndLSN != NULL );

    idlOS::memset(&sDiskLog, 0x00, SMR_LOGREC_SIZE(smrDiskLog));

    smLayerCallback::initLogBuffer( aTrans );

    sLogType =
        ((aLogAttr & SM_DLOG_ATTR_LOGTYPE_MASK) == SM_DLOG_ATTR_UNDOABLE) ?
        SMR_DLT_UNDOABLE : SMR_DLT_REDOONLY;

    smLayerCallback::getTxIDAnLogType( aTrans,
                                       &sTransID,
                                       &sLogTypeFlag );

    smrLogHeadI::setType(&sDiskLog.mHead, sLogType);
    sLength  = smuDynArray::getSize(aLogBuffer);

    smrLogHeadI::setSize(&sDiskLog.mHead,
                         SMR_LOGREC_SIZE(smrDiskLog) +
                         sLength + ID_SIZEOF(smrLogTail));

    smrLogHeadI::setTransID(&sDiskLog.mHead, sTransID);

    if ((aLogAttr & SM_DLOG_ATTR_DML_MASK) == SM_DLOG_ATTR_DML)
    {
        //PROJ-1608 recovery From Replication
        //REPL_RECOVERY�� Recovery Sender�� �����Ѵ�.
        if ( ( (aLogAttr & SM_DLOG_ATTR_TRANS_MASK) == SM_DLOG_ATTR_REPLICATE) &&
            ( (sLogTypeFlag == SMR_LOG_TYPE_NORMAL) ||
              (sLogTypeFlag == SMR_LOG_TYPE_REPL_RECOVERY) ) )
        {
            smrLogHeadI::setFlag( &sDiskLog.mHead, sLogTypeFlag );

            /* BUG-17033: �ֻ��� Statement�� �ƴ� Statment�� ���ؼ���
             * Partial Rollback�� �����ؾ� �մϴ�. */
            if ( smLayerCallback::checkAndSetImplSVPStmtDepth4Repl( aTrans )
                 == ID_FALSE )
            {
                smrLogHeadI::setFlag(&sDiskLog.mHead,
                                     smrLogHeadI::getFlag(&sDiskLog.mHead) | SMR_LOG_SAVEPOINT_OK);

                smrLogHeadI::setReplStmtDepth( &sDiskLog.mHead,
                                               smLayerCallback::getLstReplStmtDepth( aTrans ) );
            }

            // To Fix BUG-12512
            if ( smLayerCallback::isPsmSvpReserved( aTrans ) == ID_TRUE )
            {
                IDE_TEST( smLayerCallback::writePsmSvp( aTrans ) != IDE_SUCCESS );
            }
            else
            {
                // do nothing
            }
        }
        else
        {
            smrLogHeadI::setFlag(&sDiskLog.mHead, SMR_LOG_TYPE_REPLICATED);
        }
    }
    else
    {
        /* ------------------------------------------------
         * DML�� ���þ��� �α��ϰ�쿡, SMR_LOG_TYPE_NORMAL�� ������ ��쿡
         * replacator�� �ǵ��� ����� �ִ�. �ֳ��ϸ�, �α� ����� ������ �
         * disk �α� Ÿ�������� �Ǵ����� ���ϱ� �����̴�.
         * �׷��� ������ N �� ���Ͽ� R�� ǥ���Ͽ� etc��
         * disk �α��� ���� ��� �ǵ����� ���ϵ��� �����.
         * ����: (N->R??) �ƴϸ� �α� �÷��� Ÿ���� Ȯ���ϴ� ����� ����غ�
         * �ʿ䰡 �ִ�.
         * ----------------------------------------------*/
        smrLogHeadI::setFlag(&sDiskLog.mHead, sLogTypeFlag);
    }

    /* TASK-5030 
     * �α� ��忡 FXLog �÷��׸� ���� �Ѵ� */
    IDE_TEST( smLayerCallback::getTableHeaderFromOID( aTableOID, (void**)(&sTableHeader) )
              != IDE_SUCCESS );

    if ( smcTable::isSupplementalTable((smcTableHeader *)sTableHeader) == ID_TRUE )
    {
        smrLogHeadI::setFlag( &sDiskLog.mHead,
                              smrLogHeadI::getFlag(&sDiskLog.mHead) | SMR_LOG_FULL_XLOG_OK);
    }

    IDE_TEST( decideLogComp( aWriteOption, &sDiskLog.mHead )
              != IDE_SUCCESS );

    sDiskLog.mTableOID    = aTableOID;
    sDiskLog.mContType    = aContType;
    sDiskLog.mRedoLogSize = sLength;
    sDiskLog.mRefOffset   = aRefOffset;
    sDiskLog.mRedoType    = aRedoType;

    IDE_TEST( ((smxTrans*)aTrans)->writeLogToBuffer(
                                   &sDiskLog,
                                   SMR_LOGREC_SIZE(smrDiskLog))
              != IDE_SUCCESS );

    IDE_TEST( ((smxTrans*)aTrans)->writeLogToBufferUsingDynArr( aLogBuffer,
                                                                sLength )
              != IDE_SUCCESS );

    IDE_TEST( ((smxTrans*)aTrans)->writeLogToBuffer( &sLogType,
                                                     ID_SIZEOF(smrLogType))
              != IDE_SUCCESS );

    IDE_TEST( writeLog( aStatistics,
                        aTrans,
                        smLayerCallback::getLogBufferOfTrans( aTrans ),
                        aPrvLSN,  // Previous LSN Ptr
                        aBeginLSN,// Log LSN Ptr 
                        aEndLSN,  // End LSN Ptr
                        sDiskLog.mTableOID ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/***********************************************************************
 * Description : disk NTA �α�(SMR_DLT_NTA)�� �α׹��ۿ� ����Ѵ�.
 *
 * disk NTA �α״� MRDB�� DRDB�� ��ģ ���꿡 ���� actomic ������
 * �����ϱ� ���� ����ϴ� �α��̴�. table segment �����̳�, index
 * segment ����� ���ȴ�.
 * mtx�� commit�������� NTA�� �����Ǿ� �ִ� ��쿡 ȣ��Ǵ�
 * �Լ��ν�, SMR_DLT_NTAŸ���� �α׸� ����Ѵ�.
 *
 * - 2nd. code design
 *   + Ʈ����� �α� ���� �ʱ�ȭ
 *   + �α� header ����
 *   + ������ �α� header�� Ʈ����� �α׹��ۿ� ���
 *   + mtx �α� ���ۿ� ����� �α׸� Ʈ����� �α׹��ۿ� ���
 *   + �α��� tail�� �����Ѵ�.
 *   + ���������� Ʈ����� �α� ���ۿ� �ۼ��� �α׸� ���
 *     �α����Ͽ� ����Ѵ�.
 **********************************************************************/
IDE_RC smrLogMgr::writeDiskNTALogRec( idvSQL          * aStatistics,
                                      void            * aTrans,
                                      smuDynArrayBase * aLogBuffer,
                                      UInt              aWriteOption,
                                      UInt              aOPType,
                                      smLSN           * aPPrevLSN,
                                      scSpaceID         aSpaceID,
                                      ULong           * aArrData,
                                      UInt              aDataCount,
                                      smLSN           * aBeginLSN,
                                      smLSN           * aEndLSN,
                                      smOID             aTableOID )
{

    UInt           sLength;
    UInt           sLogTypeFlag;
    smTID          sTransID;
    smrDiskNTALog  sNTALog;
    smrLogType     sLogType;

    IDE_DASSERT( aTrans != NULL );
    IDE_DASSERT( aLogBuffer != NULL );
    IDE_DASSERT( aBeginLSN != NULL );
    IDE_DASSERT( aEndLSN != NULL ); 
    IDE_ASSERT(  aDataCount < SM_DISK_NTALOG_DATA_COUNT );/*BUG-27516 Klocwork*/

    idlOS::memset(&sNTALog, 0x00, SMR_LOGREC_SIZE(smrDiskNTALog));

    smLayerCallback::initLogBuffer( aTrans );

    smLayerCallback::getTxIDAnLogType( aTrans,
                                       &sTransID,
                                       &sLogTypeFlag );

    sLogType = SMR_DLT_NTA;

    smrLogHeadI::setType(&sNTALog.mHead, sLogType);
    sLength  = smuDynArray::getSize(aLogBuffer);

    smrLogHeadI::setSize(&sNTALog.mHead,
                         SMR_LOGREC_SIZE(smrDiskNTALog) +
                         sLength + ID_SIZEOF(smrLogTail));

    smrLogHeadI::setTransID(&sNTALog.mHead, sTransID);

    /* ------------------------------------------------
     * DML�� ���þ��� �α�������, SMR_LOG_TYPE_NORMAL�� ������ ��쿡
     * replacator�� �ǵ��� ����� �ִ�.
     * ����: (N->R??) �ƴϸ� �α� �÷��� Ÿ���� Ȯ���ϴ� ����� ����غ�
     * �ʿ䰡 �ִ�.
     * ----------------------------------------------*/
    smrLogHeadI::setFlag(&sNTALog.mHead, sLogTypeFlag);

    if ( (smrLogHeadI::getFlag(&sNTALog.mHead) & SMR_LOG_SAVEPOINT_MASK)
         == SMR_LOG_SAVEPOINT_OK)
    {
        smrLogHeadI::setReplStmtDepth(
                        &sNTALog.mHead,
                        smLayerCallback::getLstReplStmtDepth( aTrans ) );
    }
    else
    {
        smrLogHeadI::setReplStmtDepth( &sNTALog.mHead,
                                       SMI_STATEMENT_DEPTH_NULL );
    }

    IDE_TEST( decideLogComp( aWriteOption, &sNTALog.mHead )
              != IDE_SUCCESS );

    sNTALog.mOPType     = aOPType;  // ���� Ÿ���� �����Ѵ�.
    sNTALog.mSpaceID    = aSpaceID;
    sNTALog.mDataCount  = aDataCount;

    idlOS::memcpy( sNTALog.mData,
                   aArrData,
                   (size_t)ID_SIZEOF(ULong)*aDataCount );

    sLength = smuDynArray::getSize(aLogBuffer);
    sNTALog.mRedoLogSize = sLength;

    IDE_TEST( ((smxTrans*)aTrans)->writeLogToBuffer(
                                   &sNTALog,
                                   SMR_LOGREC_SIZE(smrDiskNTALog))
              != IDE_SUCCESS );

    if ( sLength != 0 )
    {
        IDE_TEST( ((smxTrans*)aTrans)->writeLogToBufferUsingDynArr(
                                       aLogBuffer,
                                       sLength)
                != IDE_SUCCESS );
    }

    IDE_TEST( ((smxTrans*)aTrans)->writeLogToBuffer(
                                   &sLogType,
                                   ID_SIZEOF(smrLogType))
              != IDE_SUCCESS );

    IDE_TEST( writeLog( aStatistics,
                        aTrans,
                        smLayerCallback::getLogBufferOfTrans( aTrans ),
                        aPPrevLSN,
                        aBeginLSN,
                        aEndLSN,
                        aTableOID ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/***********************************************************************
 * Description : Referenced NTA �α�(SMR_DLT_REF_NTA)�� �α׹��ۿ�
 * ����Ѵ�.
 *
 **********************************************************************/
IDE_RC smrLogMgr::writeDiskRefNTALogRec( idvSQL          * aStatistics,
                                         void            * aTrans,
                                         smuDynArrayBase * aLogBuffer,
                                         UInt              aWriteOption,
                                         UInt              aOPType,
                                         UInt              aRefOffset,
                                         smLSN           * aPPrevLSN,
                                         scSpaceID         aSpaceID,
                                         smLSN           * aBeginLSN,
                                         smLSN           * aEndLSN,
                                         smOID             aTableOID )
{

    UInt              sLength;
    UInt              sLogTypeFlag;
    smTID             sTransID;
    smrDiskRefNTALog  sNTALog;
    smrLogType        sLogType;

    IDE_DASSERT( aTrans     != NULL );
    IDE_DASSERT( aLogBuffer != NULL );
    IDE_DASSERT( aBeginLSN  != NULL );
    IDE_DASSERT( aEndLSN    != NULL );

    idlOS::memset(&sNTALog, 0x00, SMR_LOGREC_SIZE(smrDiskRefNTALog));

    smLayerCallback::initLogBuffer( aTrans );

    smLayerCallback::getTxIDAnLogType( aTrans,
                                       &sTransID,
                                       &sLogTypeFlag );

    sLogType = SMR_DLT_REF_NTA;
    smrLogHeadI::setType(&sNTALog.mHead, sLogType);
    sLength  = smuDynArray::getSize(aLogBuffer);

    smrLogHeadI::setSize(&sNTALog.mHead,
                         SMR_LOGREC_SIZE(smrDiskRefNTALog) +
                         sLength + ID_SIZEOF(smrLogTail));

    smrLogHeadI::setTransID(&sNTALog.mHead, sTransID);

    /* ------------------------------------------------
     * DML�� ���þ��� �α�������, SMR_LOG_TYPE_NORMAL�� ������ ��쿡
     * replacator�� �ǵ��� ����� �ִ�.
     * ����: (N->R??) �ƴϸ� �α� �÷��� Ÿ���� Ȯ���ϴ� ����� ����غ�
     * �ʿ䰡 �ִ�.
     * ----------------------------------------------*/
    smrLogHeadI::setFlag(&sNTALog.mHead, sLogTypeFlag);

    if ( (smrLogHeadI::getFlag(&sNTALog.mHead) & SMR_LOG_SAVEPOINT_MASK)
         == SMR_LOG_SAVEPOINT_OK)
    {
        smrLogHeadI::setReplStmtDepth(
                        &sNTALog.mHead,
                        smLayerCallback::getLstReplStmtDepth( aTrans ) );
    }
    else
    {
        smrLogHeadI::setReplStmtDepth( &sNTALog.mHead,
                                       SMI_STATEMENT_DEPTH_NULL );
    }

    IDE_TEST( decideLogComp( aWriteOption, &sNTALog.mHead )
              != IDE_SUCCESS );

    sNTALog.mOPType      = aOPType;  // ���� Ÿ���� �����Ѵ�.
    sNTALog.mSpaceID     = aSpaceID;
    sNTALog.mRefOffset   = aRefOffset;
    sNTALog.mRedoLogSize = sLength;

    IDE_TEST( ((smxTrans*)aTrans)->writeLogToBuffer( 
                             &sNTALog,
                             SMR_LOGREC_SIZE(smrDiskRefNTALog))
              != IDE_SUCCESS );

    if ( sLength != 0 )
    {
        IDE_TEST( ((smxTrans*)aTrans)->writeLogToBufferUsingDynArr(
                                      aLogBuffer,
                                      sLength )
                != IDE_SUCCESS );
    }

    IDE_TEST( ((smxTrans*)aTrans)->writeLogToBuffer(
                                   &sLogType,
                                   ID_SIZEOF(smrLogType))
              != IDE_SUCCESS );

    IDE_TEST( writeLog( aStatistics,
                        aTrans,
                        smLayerCallback::getLogBufferOfTrans( aTrans ),
                        aPPrevLSN,
                        aBeginLSN,
                        aEndLSN,
                        aTableOID ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/*
  PRJ-1548 User Memory Tablespace
  ���̺����̽� UPDATE ���� ���� CLR �α׸� �α׹��ۿ� ���
*/
IDE_RC smrLogMgr::writeCMPSLogRec4TBSUpt( idvSQL*        aStatistics,
                                          void*          aTrans,
                                          smLSN*         aPrvUndoLSN,
                                          smrTBSUptLog*  aUpdateLog,
                                          SChar*         aBeforeImage )
{
    smrCMPSLog     sCSLog;
    smTID          sSMTID;
    UInt           sLogFlag;
    smrLogType     sLogType;

    IDE_DASSERT(aPrvUndoLSN != NULL);
    IDE_DASSERT(aUpdateLog != NULL);
    IDE_DASSERT(aBeforeImage != NULL);

    idlOS::memset(&sCSLog, 0x00, SMR_LOGREC_SIZE(smrDiskCMPSLog));
    smLayerCallback::initLogBuffer( aTrans );

    sLogType = SMR_LT_COMPENSATION;

    smrLogHeadI::setType(&sCSLog.mHead, sLogType);

    //get tx id and logtype flag from a transaction
    smLayerCallback::getTxIDAnLogType( aTrans,
                                       &sSMTID,
                                       &sLogFlag );

    smrLogHeadI::setTransID(&sCSLog.mHead, sSMTID);

    smrLogHeadI::setFlag(&sCSLog.mHead, sLogFlag);

    if ( (smrLogHeadI::getFlag(&sCSLog.mHead) & SMR_LOG_SAVEPOINT_MASK)
         == SMR_LOG_SAVEPOINT_OK)
    {
        smrLogHeadI::setReplStmtDepth(
                        &sCSLog.mHead, 
                        smLayerCallback::getLstReplStmtDepth( aTrans ) );
    }
    else
    {
        smrLogHeadI::setReplStmtDepth( &sCSLog.mHead,
                                       SMI_STATEMENT_DEPTH_NULL );
    }

    smrLogHeadI::setSize (&sCSLog.mHead,
                          SMR_LOGREC_SIZE(smrCMPSLog) +
                          aUpdateLog->mBImgSize +
                          ID_SIZEOF(smrLogTail) );

    SC_MAKE_GRID(sCSLog.mGRID, aUpdateLog->mSpaceID, 0, 0);

    sCSLog.mFileID      = aUpdateLog->mFileID;
    sCSLog.mTBSUptType  = aUpdateLog->mTBSUptType;
    sCSLog.mBImgSize    = aUpdateLog->mBImgSize;

    IDE_TEST( ((smxTrans*)aTrans)->writeLogToBuffer(
                                   &sCSLog,
                                   SMR_LOGREC_SIZE(smrCMPSLog))
              != IDE_SUCCESS );

    if (aUpdateLog->mBImgSize > 0)
    {
        IDE_TEST( ((smxTrans*)aTrans)->writeLogToBuffer(
                                       aBeforeImage,
                                       aUpdateLog->mBImgSize)
                  != IDE_SUCCESS );
    }

    IDE_TEST( ((smxTrans*)aTrans)->writeLogToBuffer(
                                   &sLogType,
                                   ID_SIZEOF(smrLogType))
              != IDE_SUCCESS );

    IDE_TEST( writeLog( aStatistics,
                        aTrans,
                        smLayerCallback::getLogBufferOfTrans( aTrans ),
                        aPrvUndoLSN,
                        NULL,  // Log LSN Ptr
                        NULL,  // End LSN Ptr
                        SM_NULL_OID )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/***********************************************************************
 * Description : disk CLR �α�(SMR_DLT_COMPENSATION)�� �α׹��ۿ� ����Ѵ�.
 * 1. Ʈ����� �α� ���� �ʱ�ȭ
 * 2. �α��� header ����
 * 3. disk logs���� �� ���̸� ����Ѵ�.
 * 4. ������ �α� header�� Ʈ����� �α׹��ۿ� ���
 * 5. mtx �α׹��ۿ� ����� disk log ���� ����Ѵ�.
 * 6. �α��� tail�� �����Ѵ�.
 **********************************************************************/
IDE_RC smrLogMgr::writeDiskCMPSLogRec( idvSQL          * aStatistics,
                                       void            * aTrans,
                                       smuDynArrayBase * aLogBuffer,
                                       UInt              aWriteOption,
                                       smLSN           * aPrevLSN,
                                       smLSN           * aBeginLSN,
                                       smLSN           * aEndLSN,
                                       smOID             aTableOID )
{

    UInt           sLength;
    UInt           sLogTypeFlag;
    smTID          sTransID;
    smrDiskCMPSLog sCSLog;
    smrLogType     sLogType;

    IDE_DASSERT( aTrans     != NULL );
    IDE_DASSERT( aLogBuffer != NULL );
    IDE_DASSERT( aBeginLSN  != NULL );
    IDE_DASSERT( aEndLSN    != NULL );

    idlOS::memset(&sCSLog, 0x00, SMR_LOGREC_SIZE(smrDiskCMPSLog));

    smLayerCallback::initLogBuffer( aTrans );
    smLayerCallback::getTxIDAnLogType( aTrans,
                                       &sTransID,
                                       &sLogTypeFlag );

    sLogType = SMR_DLT_COMPENSATION;

    smrLogHeadI::setType(&sCSLog.mHead, sLogType);
    sLength = smuDynArray::getSize(aLogBuffer);

    smrLogHeadI::setSize(&sCSLog.mHead,
                         SMR_LOGREC_SIZE(smrDiskCMPSLog) +
                         sLength + ID_SIZEOF(smrLogTail));

    smrLogHeadI::setTransID(&sCSLog.mHead, sTransID);

    /* ------------------------------------------------
     * DML�� ���þ��� �α�������, SMR_LOG_TYPE_NORMAL�� ������ ��쿡
     * replacator�� �ǵ��� ����� �ִ�.
     * ����: (N->R??) �ƴϸ� �α� �÷��� Ÿ���� Ȯ���ϴ� ����� ����غ�
     * �ʿ䰡 �ִ�.
     * ----------------------------------------------*/
    smrLogHeadI::setFlag(&sCSLog.mHead, sLogTypeFlag);

    if ( (smrLogHeadI::getFlag(&sCSLog.mHead) & SMR_LOG_SAVEPOINT_MASK)
         == SMR_LOG_SAVEPOINT_OK)
    {
        smrLogHeadI::setReplStmtDepth(
                        &sCSLog.mHead,
                        smLayerCallback::getLstReplStmtDepth( aTrans ) );
    }
    else
    {
        smrLogHeadI::setReplStmtDepth( &sCSLog.mHead,
                                       SMI_STATEMENT_DEPTH_NULL );
    }

    IDE_TEST( decideLogComp( aWriteOption, &sCSLog.mHead )
              != IDE_SUCCESS );

    sCSLog.mRedoLogSize = sLength;

    IDE_TEST( ((smxTrans*)aTrans)->writeLogToBuffer(
                                   &sCSLog,
                                   SMR_LOGREC_SIZE(smrDiskCMPSLog))
              != IDE_SUCCESS );

    if ( sLength != 0 )
    {
        IDE_TEST( ((smxTrans*)aTrans)->writeLogToBufferUsingDynArr( 
                                                          aLogBuffer,
                                                          sLength )
                != IDE_SUCCESS );
    }
    else
    {
        /* nothing do to */
    }

    IDE_TEST( ((smxTrans*)aTrans)->writeLogToBuffer(
                                   &sLogType,
                                   ID_SIZEOF(smrLogType))
              != IDE_SUCCESS );

    IDE_TEST( writeLog( aStatistics,
                        aTrans,
                        smLayerCallback::getLogBufferOfTrans( aTrans ),
                        aPrevLSN,
                        aBeginLSN,
                        aEndLSN,
                        aTableOID )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/***********************************************************************
 * Description : tx�� ������� disk �α׸� �α�
 * tx commit �������� �߻��ϴ� commit SCN �α�� ȣ���.
 **********************************************************************/
IDE_RC smrLogMgr::writeDiskDummyLogRec( idvSQL           * aStatistics,
                                        smuDynArrayBase  * aLogBuffer,
                                        UInt               aWriteOption,
                                        UInt               aContType,
                                        UInt               aRedoType,
                                        smOID              aTableOID,
                                        smLSN            * aBeginLSN,
                                        smLSN            * aEndLSN )
{

    smrDiskLog sDiskLog;
    UInt       sLength;
    ULong      sBuffer[(SD_PAGE_SIZE * 2)/ID_SIZEOF(ULong)];
    SChar*     sWritePtr;

    IDE_DASSERT( aLogBuffer != NULL );
    IDE_DASSERT( aBeginLSN  != NULL );
    IDE_DASSERT( aEndLSN    != NULL );

    idlOS::memset(&sDiskLog, 0x00, SMR_LOGREC_SIZE(smrDiskLog));

    smrLogHeadI::setType(&sDiskLog.mHead, SMR_DLT_REDOONLY);
    sLength  = smuDynArray::getSize(aLogBuffer);
    IDE_ASSERT( sLength > 0 );

    /* BUG-32623 When index statistics is rebuilded, the log's size
     * can exceed stack log buffer's size in mini-transaction. */
    IDE_TEST_RAISE(
            (SMR_LOGREC_SIZE(smrDiskLog) + sLength + ID_SIZEOF(smrLogTail))
            > ID_SIZEOF(sBuffer), error_exceed_stack_buffer_size);

    smrLogHeadI::setSize(&sDiskLog.mHead,
                         SMR_LOGREC_SIZE(smrDiskLog) +
                         sLength + ID_SIZEOF(smrLogTail) );

    smrLogHeadI::setTransID(&sDiskLog.mHead, SM_NULL_TID);

    smrLogHeadI::setFlag(&sDiskLog.mHead, SMR_LOG_TYPE_NORMAL);

    smrLogHeadI::setReplStmtDepth( &sDiskLog.mHead,
                                   SMI_STATEMENT_DEPTH_NULL );

    IDE_TEST( decideLogComp( aWriteOption, &sDiskLog.mHead )
              != IDE_SUCCESS );

    /* ------------------------------------------------
     * !!] Ʈ������� �Ҵ���� tss�� RID�� �����Ѵ�.
     * Ʈ������� TSS�� �Ҵ���� ���� ���� SD_NULL_SID��
     * ��ȯ�ȴ�.
     * ----------------------------------------------*/
    sDiskLog.mTableOID    = aTableOID;
    sDiskLog.mContType    = aContType;
    sDiskLog.mRedoType    = aRedoType;

    /* 3. disk logs���� �� ���̸� ����Ѵ�. */
    sDiskLog.mRedoLogSize = sLength;

    sWritePtr = (SChar*)sBuffer;

    idlOS::memset(sWritePtr, 0x00, smrLogHeadI::getSize(&sDiskLog.mHead));
    idlOS::memcpy(sWritePtr, &sDiskLog, SMR_LOGREC_SIZE(smrDiskLog));
    sWritePtr += SMR_LOGREC_SIZE(smrDiskLog);

    smuDynArray::load(aLogBuffer, (void*)sWritePtr, sLength );
    sWritePtr += sLength;

    smrLogHeadI::copyTail(sWritePtr, &(sDiskLog.mHead));

    IDE_TEST( writeLog( aStatistics,
                        NULL,      // aTrans
                        (SChar*)sBuffer,
                        NULL,      // Previous LSN Ptr
                        aBeginLSN, // Log LSN Ptr
                        aEndLSN,   // End LSN Ptr
                        aTableOID ) != IDE_SUCCESS );


    return IDE_SUCCESS;

    IDE_EXCEPTION(error_exceed_stack_buffer_size)
    {
        IDE_SET( ideSetErrorCode(smERR_ABORT_INTERNAL_ARG, 
                                 "exceed stack buffer size") );
    }
    IDE_EXCEPTION_END;
        
    return IDE_FAILURE;

}
/***********************************************************************
 * Description : tablespace BACKUP �����߿�
 * tx�κ��� flsuh�Ǵ� Page�� ���� image �α�
 **********************************************************************/
IDE_RC smrLogMgr::writeDiskPILogRec( idvSQL*           aStatistics,
                                     UChar*            aBuffer,
                                     scGRID            aPageGRID )
{
    IDE_TEST( writePageImgLogRec( aStatistics,
                                  aBuffer,
                                  aPageGRID,
                                  SDR_SDP_WRITE_PAGEIMG,
                                  NULL) // end LSN
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/***********************************************************************
 * Description : ��ũ ���� �Ҵ�, ���� �� ���濡 ���� �α� Ÿ��
 **********************************************************************/
IDE_RC smrLogMgr::writeTBSUptLogRec( idvSQL           * aStatistics,
                                     void             * aTrans,
                                     smuDynArrayBase  * aLogBuffer,
                                     scSpaceID          aSpaceID,
                                     UInt               aFileID,
                                     sctUpdateType      aTBSUptType,
                                     UInt               aAImgSize,
                                     UInt               aBImgSize,
                                     smLSN            * aBeginLSN )
{
    UInt            sLength;
    UInt            sLogTypeFlag;
    smTID           sTransID;
    smrTBSUptLog    sTBSUptLog;
    smrLogType      sLogType;
    smLSN           sEndLSN;

    IDE_DASSERT( aTrans     != NULL );
    IDE_DASSERT( aLogBuffer != NULL );

    idlOS::memset(&sTBSUptLog, 0x00, SMR_LOGREC_SIZE(smrTBSUptLog));

    smLayerCallback::initLogBuffer( aTrans );

    smLayerCallback::getTxIDAnLogType( aTrans,
                                       &sTransID,
                                       &sLogTypeFlag );

    sLogType = SMR_LT_TBS_UPDATE;

    smrLogHeadI::setType(&sTBSUptLog.mHead, sLogType );
    sLength = smuDynArray::getSize(aLogBuffer);

    smrLogHeadI::setSize(&sTBSUptLog.mHead,
                         SMR_LOGREC_SIZE(smrTBSUptLog) +
                         sLength + ID_SIZEOF(smrLogTail));

    smrLogHeadI::setTransID(&sTBSUptLog.mHead, sTransID);

    smrLogHeadI::setFlag(&sTBSUptLog.mHead, sLogTypeFlag);

    if ( (smrLogHeadI::getFlag(&sTBSUptLog.mHead) & SMR_LOG_SAVEPOINT_MASK)
         == SMR_LOG_SAVEPOINT_OK)
    {
        smrLogHeadI::setReplStmtDepth(
                        &sTBSUptLog.mHead,
                        smLayerCallback::getLstReplStmtDepth( aTrans ) );
    }
    else
    {
        smrLogHeadI::setReplStmtDepth( &sTBSUptLog.mHead,
                                       SMI_STATEMENT_DEPTH_NULL );
    }

    sTBSUptLog.mSpaceID    = aSpaceID;
    sTBSUptLog.mFileID     = aFileID;
    sTBSUptLog.mTBSUptType = aTBSUptType;
    sTBSUptLog.mAImgSize   = aAImgSize;
    sTBSUptLog.mBImgSize   = aBImgSize;

    IDE_TEST( ((smxTrans*)aTrans)->writeLogToBuffer(
                                   &sTBSUptLog,
                                   SMR_LOGREC_SIZE(smrTBSUptLog))
              != IDE_SUCCESS );

    if ( sLength != 0 )
    {
        IDE_TEST( ((smxTrans*)aTrans)->writeLogToBufferUsingDynArr(
                                 aLogBuffer,
                                 sLength )
                != IDE_SUCCESS );
    }

    IDE_TEST( ((smxTrans*)aTrans)->writeLogToBuffer(
                                   &sLogType,
                                   ID_SIZEOF(smrLogType))
              != IDE_SUCCESS );

    IDE_TEST( writeLog( aStatistics,
                        aTrans,
                        smLayerCallback::getLogBufferOfTrans( aTrans ),
                        NULL,      // Prev LSN Ptr
                        aBeginLSN, 
                        &sEndLSN,  // END LSN Ptr
                        SM_NULL_OID )
              != IDE_SUCCESS );

    IDE_TEST( smrLogMgr::syncLFThread( SMR_LOG_SYNC_BY_TRX,
                                       &sEndLSN )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : MRDB NTA �α�(SMR_LT_NTA)�� �α׹��ۿ� ���
 **********************************************************************/
IDE_RC smrLogMgr::writeNTALogRec(idvSQL   * aStatistics,
                                 void     * aTrans,
                                 smLSN    * aLSN,
                                 scSpaceID  aSpaceID,
                                 smrOPType  aOPType,
                                 vULong     aData1,
                                 vULong     aData2)
{

    smrNTALog sNTALog;
    smTID     sTID;
    UInt      sLogTypeFlag;

    smLayerCallback::getTxIDAnLogType( aTrans,
                                       &sTID,
                                       &sLogTypeFlag );
    smrLogHeadI::setType(&sNTALog.mHead, SMR_LT_NTA);
    smrLogHeadI::setSize(&sNTALog.mHead, SMR_LOGREC_SIZE(smrNTALog));
    smrLogHeadI::setTransID(&sNTALog.mHead, sTID);
    smrLogHeadI::setFlag(&sNTALog.mHead, sLogTypeFlag);

    if ( (smrLogHeadI::getFlag(&sNTALog.mHead) & SMR_LOG_SAVEPOINT_MASK)
         == SMR_LOG_SAVEPOINT_OK)
    {
        smrLogHeadI::setReplStmtDepth(
                        &sNTALog.mHead,
                        smLayerCallback::getLstReplStmtDepth( aTrans ) );
    }
    else
    {
        smrLogHeadI::setReplStmtDepth( &sNTALog.mHead,
                                       SMI_STATEMENT_DEPTH_NULL );
    }
    sNTALog.mSpaceID           = aSpaceID;
    sNTALog.mData1             = aData1;
    sNTALog.mData2             = aData2;
    sNTALog.mOPType            = aOPType;
    sNTALog.mTail              = SMR_LT_NTA;

    return writeLog( aStatistics,
                     aTrans,
                     (SChar*)&sNTALog,
                     aLSN, // Previous LSN Ptr
                     NULL, // Log LSN Ptr
                     NULL, // END LSN Ptr
                     SM_NULL_OID );
}

/***********************************************************************
 * Description : savepoint ���� �α׸� �α׹��ۿ� ���
 **********************************************************************/
IDE_RC smrLogMgr::writeSetSvpLog(idvSQL*      aStatistics,
                                 void*        aTrans,
                                 const SChar* aSVPName)
{

    ULong        sLogBuffer[SM_PAGE_SIZE / ID_SIZEOF(ULong)];
    SChar*       sLogPtr;
    smrLogHead*  sLogHead;
    UInt         sLen;
    smTID        sTID;
    UInt         sLogFlag;

    sLogPtr         = (SChar*)sLogBuffer;
    sLogHead        = (smrLogHead*)sLogPtr;

    smrLogHeadI::setType(sLogHead, SMR_LT_SAVEPOINT_SET);

    //get tx id and logtype flag from a transaction
    smLayerCallback::getTxIDAnLogType( aTrans,
                                       &sTID,
                                       &sLogFlag );

    smrLogHeadI::setTransID(sLogHead, sTID);
    smrLogHeadI::setFlag(sLogHead, sLogFlag);
    if ( (smrLogHeadI::getFlag(sLogHead) & SMR_LOG_SAVEPOINT_MASK)
         == SMR_LOG_SAVEPOINT_OK)
    {
        smrLogHeadI::setReplStmtDepth(
                        sLogHead,
                        smLayerCallback::getLstReplStmtDepth( aTrans ) );
    }
    else
    {
        smrLogHeadI::setReplStmtDepth( sLogHead,
                                       SMI_STATEMENT_DEPTH_NULL );
    }

    // get m_lstUndoNxtLSN of a transaction
    smrLogHeadI::setPrevLSN( sLogHead, smLayerCallback::getLstUndoNxtLSN( aTrans ) );
    sLogPtr += ID_SIZEOF(smrLogHead);

    if ( aSVPName != NULL)
    {
        sLen = idlOS::strlen(aSVPName);
        *((UInt*)sLogPtr) = sLen;
        sLogPtr += ID_SIZEOF(UInt);

        idlOS::memcpy(sLogPtr, aSVPName, sLen);
    }
    else
    {
        sLen = SMR_IMPLICIT_SVP_NAME_SIZE;

        SMR_LOG_APPEND_4( sLogPtr, sLen );

        idlOS::memcpy(sLogPtr, SMR_IMPLICIT_SVP_NAME, SMR_IMPLICIT_SVP_NAME_SIZE);
    }

    sLogPtr += sLen;
    smrLogHeadI::setSize(sLogHead, ID_SIZEOF(smrLogHead) + ID_SIZEOF(UInt) + sLen  + ID_SIZEOF(smrLogTail));

    smrLogHeadI::copyTail(sLogPtr, sLogHead);

    return writeLog( aStatistics,
                     aTrans,
                     (SChar*)sLogBuffer,
                     NULL,   // Previous LSN Ptr
                     NULL,   // Log LSN Ptr
                     NULL,   // End LSN Ptr
                     SM_NULL_OID );
}

/***********************************************************************
 * Description : savepoint ���� �α׸� �α׹��ۿ� ���
 **********************************************************************/
IDE_RC smrLogMgr::writeAbortSvpLog( idvSQL*      aStatistics,
                                    void*        aTrans,
                                    const SChar* aSVPName)
{

    ULong        sLogBuffer[SM_PAGE_SIZE / ID_SIZEOF(ULong)];
    SChar*       sLogPtr;
    smrLogHead*  sLogHead;
    UInt         sLen;
    smTID        sTID;
    UInt         sLogFlag;

    sLogPtr         = (SChar*)sLogBuffer;
    sLogHead        = (smrLogHead*)sLogPtr;

    smrLogHeadI::setType(sLogHead, SMR_LT_SAVEPOINT_ABORT);

    //get tx id and logtype flag from a transaction
    smLayerCallback::getTxIDAnLogType( aTrans,
                                       &sTID,
                                       &sLogFlag );

    smrLogHeadI::setTransID(sLogHead, sTID);
    smrLogHeadI::setFlag(sLogHead, sLogFlag);
    if ( (smrLogHeadI::getFlag(sLogHead) & SMR_LOG_SAVEPOINT_MASK)
         == SMR_LOG_SAVEPOINT_OK)
    {
        smrLogHeadI::setReplStmtDepth(
                        sLogHead,
                        smLayerCallback::getLstReplStmtDepth( aTrans ) );
    }
    else
    {
        smrLogHeadI::setReplStmtDepth( sLogHead,
                                       SMI_STATEMENT_DEPTH_NULL );
    }

    // get m_lstUndoNxtLSN of a transaction
    smrLogHeadI::setPrevLSN( sLogHead, smLayerCallback::getLstUndoNxtLSN( aTrans ) );

    sLogPtr += ID_SIZEOF(smrLogHead);

    if ( aSVPName != NULL)
    {
        sLen = idlOS::strlen(aSVPName);

        SMR_LOG_APPEND_4( sLogPtr, sLen );

        idlOS::memcpy(sLogPtr, aSVPName, sLen);
    }
    else
    {
        sLen = SMR_IMPLICIT_SVP_NAME_SIZE;

        SMR_LOG_APPEND_4( sLogPtr, sLen );

        idlOS::memcpy(sLogPtr, SMR_IMPLICIT_SVP_NAME, SMR_IMPLICIT_SVP_NAME_SIZE);
    }

    sLogPtr += sLen;
    smrLogHeadI::setSize(sLogHead, ID_SIZEOF(smrLogHead) + ID_SIZEOF(UInt) + sLen  + ID_SIZEOF(smrLogTail));

    smrLogHeadI::copyTail(sLogPtr, sLogHead);

    return writeLog( aStatistics,
                     aTrans,
                     (SChar*)sLogBuffer,
                     NULL,   // Previous LSN Ptr
                     NULL,   // Log LSN Ptr
                     NULL,   // End LSN Ptr
                     SM_NULL_OID );
}

/***********************************************************************
 * Description : CLR �α׸� �α׹��ۿ� ���
 **********************************************************************/
IDE_RC smrLogMgr::writeCMPSLogRec( idvSQL*       aStatistics,
                                   void*         aTrans,
                                   smrLogType    aType,
                                   smLSN*        aPrvUndoLSN,
                                   smrUpdateLog* aUpdateLog,
                                   SChar*        aBeforeImage )
{

    smrCMPSLog sCSLog;
    smTID      sTID;
    UInt       sLogFlag;
    SChar*     sLogPtr;                 
    UInt       sFlagLogSize = 0;        
    UInt       sColCnt = 0;
    UInt       sPieceCnt = 0;
    smOID      sVCPieceOID;
    UShort     sBefFlag;
    UInt       sFullXLogSize;
    UInt       sPrimaryKeySize;
    UInt       sOffset = 0;
    UInt       sFlag;
    UInt       i;
    UInt       j;

    idlOS::memset(&sCSLog, 0x00, SMR_LOGREC_SIZE(smrCMPSLog));
    smLayerCallback::initLogBuffer( aTrans );

    smrLogHeadI::setType(&sCSLog.mHead, aType);

    //get tx id and logtype flag from a transaction
    smLayerCallback::getTxIDAnLogType( aTrans,
                                       &sTID,
                                       &sLogFlag );

    smrLogHeadI::setTransID(&sCSLog.mHead, sTID);
    smrLogHeadI::setFlag(&sCSLog.mHead, sLogFlag);
    
    if ( (smrLogHeadI::getFlag(&sCSLog.mHead) & SMR_LOG_SAVEPOINT_MASK)
         == SMR_LOG_SAVEPOINT_OK)
    {
        smrLogHeadI::setReplStmtDepth(
                        &sCSLog.mHead,
                        smLayerCallback::getLstReplStmtDepth( aTrans ) );
    }
    else
    {
        smrLogHeadI::setReplStmtDepth( &sCSLog.mHead,
                                       SMI_STATEMENT_DEPTH_NULL );
    }

    if (aUpdateLog != NULL)
    {
        /* BUG-46854: �α� ����� size ���� �ڷ� �̵���. VCPiece �÷���
         * ���� �α׷��� ����ؾ��ϱ� �����̴�. 
         */
        
        smrLogHeadI::setFlag(&sCSLog.mHead,
                             smrLogHeadI::getFlag(&aUpdateLog->mHead) & SMR_LOG_TYPE_MASK);
        sCSLog.mGRID       = aUpdateLog->mGRID;
        sCSLog.mBImgSize   = aUpdateLog->mBImgSize;
        sCSLog.mUpdateType = aUpdateLog->mType;
        sCSLog.mData       = aUpdateLog->mData;

        // PRJ-1548 User Memory Tablespace
        // ���̺����̽� UPDATE ���꿡 ���� CLR�� �ƴϹǷ� ������ ����
        // �ʱ�ȭ���ش�.
        sCSLog.mTBSUptType = SCT_UPDATE_MAXMAX_TYPE;
    }
    else
    {
        // dummy CLR�� ���

        smrLogHeadI::setSize( &sCSLog.mHead,
                              SMR_LOGREC_SIZE(smrCMPSLog) +
                              ID_SIZEOF(smrLogTail) );
        // PRJ-1548 User Memory Tablespace
        // ���̺����̽� UPDATE ���꿡 ���� CLR�� �ƴϹǷ� ������ ����
        // �ʱ�ȭ���ش�.
        sCSLog.mTBSUptType = SCT_UPDATE_MAXMAX_TYPE;
    }

    // BUG-46854i: CMPS�α� ��� write �ڷ� �̵�

    sOffset += SMR_LOGREC_SIZE(smrCMPSLog);

    if ( aUpdateLog != NULL )
    {
        if ( aUpdateLog->mBImgSize > 0 )
        {
            IDE_TEST( ((smxTrans*)aTrans)->writeLogToBuffer( aBeforeImage,
                                                             sOffset,
                                                             aUpdateLog->mBImgSize )
                      != IDE_SUCCESS );
            
            // BUG-46854
            sOffset += aUpdateLog->mBImgSize;
        }

        // BUG-47366
        if ( ( aUpdateLog->mType == SMR_SMC_PERS_INSERT_ROW ) ||
             ( aUpdateLog->mType == SMR_SMC_PERS_UPDATE_VERSION_ROW ) ||
             ( aUpdateLog->mType == SMR_SMC_PERS_UPDATE_INPLACE_ROW ) )
        {
            IDE_TEST( ((smxTrans*)aTrans)->writeLogToBuffer( aBeforeImage + aUpdateLog->mBImgSize,
                                                             sOffset,
                                                             aUpdateLog->mAImgSize )
                      != IDE_SUCCESS );
            
            sOffset      += aUpdateLog->mAImgSize;
            sFlagLogSize += aUpdateLog->mAImgSize;
        }
     
        // BUG-46854: delete version �� �� �÷��� �α� �����
        if ( aUpdateLog->mType == SMR_SMC_PERS_DELETE_VERSION_ROW ) 
        {
            // BUG-46854: Var �ǽ� �÷��� �α� ó�� 
            // before + after image ũ�� ��ŭ ������ �̵�
            sLogPtr = aBeforeImage;
            sLogPtr += ( aUpdateLog->mBImgSize + aUpdateLog->mAImgSize );  
            
            // �����̸Ӹ� Ű ���� ��
            if( ( smrLogHeadI::getFlag(&aUpdateLog->mHead) & SMR_LOG_RP_INFO_LOG_MASK ) 
                == SMR_LOG_RP_INFO_LOG_OK )
            {
                idlOS::memcpy(&sPrimaryKeySize, sLogPtr, ID_SIZEOF(UInt));
                sLogPtr += sPrimaryKeySize;
            }

            // Full XLog���� ��
            if( ( smrLogHeadI::getFlag(&aUpdateLog->mHead) & SMR_LOG_FULL_XLOG_MASK ) 
                == SMR_LOG_FULL_XLOG_OK )
            {
                idlOS::memcpy(&sFullXLogSize, sLogPtr, ID_SIZEOF(UInt));
                sLogPtr += sFullXLogSize;
            }    

            // col cnt �������� 
            idlOS::memcpy(&sColCnt, sLogPtr, ID_SIZEOF(UInt));
            sLogPtr += ID_SIZEOF(UInt);

            // col cnt �α� 
            IDE_TEST( ((smxTrans*)aTrans)->writeLogToBuffer( &sColCnt,
                                                             sOffset,
                                                             ID_SIZEOF(UInt) )
                      != IDE_SUCCESS );
            sFlagLogSize += ID_SIZEOF(UInt);
            sOffset += ID_SIZEOF(UInt);

            for ( i = 0 ; i < sColCnt ; i++ )
            {
                // �ǽ� ���� �������� 
                idlOS::memcpy(&sPieceCnt, sLogPtr, ID_SIZEOF(UInt));
                sLogPtr += ID_SIZEOF(UInt);

                // �ǽ� ���� �α�
                IDE_TEST( ((smxTrans*)aTrans)->writeLogToBuffer( &sPieceCnt,
                                                                 sOffset,
                                                                 ID_SIZEOF(UInt) )
                      != IDE_SUCCESS );
                sFlagLogSize += ID_SIZEOF(UInt);
                sOffset += ID_SIZEOF(UInt);

                for ( j = 0 ; j < sPieceCnt ; j++ )
                {
                    // OID ��������
                    idlOS::memcpy(&sVCPieceOID, sLogPtr, ID_SIZEOF(smOID));
                    sLogPtr += ID_SIZEOF(smOID);

                    // OID �α� 
                    IDE_TEST( ((smxTrans*)aTrans)->writeLogToBuffer( &sVCPieceOID,
                                                                     sOffset,
                                                                     ID_SIZEOF(smOID) )
                              != IDE_SUCCESS );
                    sFlagLogSize += ID_SIZEOF(smOID);
                    sOffset += ID_SIZEOF(smOID);

                    // before �÷��� �������� 
                    idlOS::memcpy(&sBefFlag, sLogPtr, ID_SIZEOF(UShort));
                    sLogPtr += ID_SIZEOF(UShort);

                    // before �÷��� �α�
                    IDE_TEST( ((smxTrans*)aTrans)->writeLogToBuffer( &sBefFlag,
                                                                     sOffset,
                                                                     ID_SIZEOF(UShort) )
                              != IDE_SUCCESS );
                    sFlagLogSize += ID_SIZEOF(UShort);
                    sOffset += ID_SIZEOF(UShort);

                    // after �÷��� �Ѿ�� 
                    sLogPtr += ID_SIZEOF(UShort);
                }
            }
            
            // BUG-46854: CMPS log ����.
            sFlag = smrLogHeadI::getFlag(&sCSLog.mHead);
            sFlag &= SMR_LOG_CMPS_LOG_MASK;
            sFlag |= SMR_LOG_CMPS_LOG_OK;
            smrLogHeadI::setFlag( &sCSLog.mHead, sFlag );
        }
    
        // BUG-46854: size ���� �Ű���    
        smrLogHeadI::setSize(&sCSLog.mHead, SMR_LOGREC_SIZE(smrCMPSLog) 
                                            + aUpdateLog->mBImgSize 
                                            + sFlagLogSize   /* flag �α� ũ�� */ 
                                            + ID_SIZEOF(smrLogTail));
    }
   
    // BUG-46854: CMPS �α� ��� write 
    IDE_TEST( ((smxTrans*)aTrans)->writeLogToBuffer( (SChar*)&sCSLog,
                                                     0,
                                                     SMR_LOGREC_SIZE(smrCMPSLog))
              != IDE_SUCCESS );

    IDE_TEST( ((smxTrans*)aTrans)->writeLogToBuffer( &aType,
                                                     sOffset,
                                                     ID_SIZEOF(smrLogType) )
              != IDE_SUCCESS );

    IDE_TEST( writeLog( aStatistics,
                        aTrans,
                        smLayerCallback::getLogBufferOfTrans( aTrans ),
                        aPrvUndoLSN,
                        NULL,  // Log LSN Ptr
                        NULL,  // End LSN Ptr
                        SM_NULL_OID )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

// PROJ-1362 LOB CURSOR OPEN LOG for replication.
// lob cursor open log
// smrLogHead| lob locator | mOp(lob operation) | tableOID | column id | pk info
IDE_RC smrLogMgr::writeLobCursorOpenLogRec( idvSQL*           aStatistics,
                                            void*             aTrans,
                                            smrLobOpType      aLobOp,
                                            smOID             aTable,
                                            UInt              aLobColID,
                                            smLobLocator      aLobLocator,
                                            const void       *aPKInfo)
{
    UInt                      sLogTypeFlag;
    smTID                     sTransID;
    smrLobLog                 sLobCursorLog;
    smrLogType                sLogType;
    const sdcPKInfo          *sPKInfo = (const sdcPKInfo*)aPKInfo;
    const sdcColumnInfo4PK   *sColumnInfo;
    UShort                    sPKInfoSize;
    UShort                    sPKColCount;
    UShort                    sPKColSeq;
    UInt                      sColumnId;
    UChar                     sPrefix;
    UChar                     sSmallLen;
    UShort                    sLargeLen;

    IDE_DASSERT( aTrans  != NULL );
    IDE_DASSERT( aPKInfo != NULL );

    IDE_ASSERT(aLobOp == SMR_DISK_LOB_CURSOR_OPEN);

    idlOS::memset(&sLobCursorLog, 0x00, SMR_LOGREC_SIZE(smrLobLog));
    smLayerCallback::initLogBuffer( aTrans );


    smLayerCallback::getTxIDAnLogType( aTrans,
                                       &sTransID,
                                       &sLogTypeFlag );

    IDE_DASSERT (sLogTypeFlag == SMR_LOG_TYPE_NORMAL);
    sLogType =  SMR_LT_LOB_FOR_REPL;

    smrLogHeadI::setType(&sLobCursorLog.mHead,sLogType);
    smrLogHeadI::setSize(&sLobCursorLog.mHead,
                         SMR_LOGREC_SIZE(smrLobLog) +
                         ID_SIZEOF(smOID)+   // table
                         ID_SIZEOF(UInt) +   // lob column id
                         sdcRow::calcPKLogSize(sPKInfo) +
                         ID_SIZEOF(smrLogTail) );

    smrLogHeadI::setTransID(&sLobCursorLog.mHead, sTransID);

    smrLogHeadI::setPrevLSN( &sLobCursorLog.mHead,
                             smLayerCallback::getLstUndoNxtLSN( aTrans ) );

    smrLogHeadI::setFlag(&sLobCursorLog.mHead, SMR_LOG_TYPE_NORMAL);

    if ( (smrLogHeadI::getFlag(&sLobCursorLog.mHead) & SMR_LOG_SAVEPOINT_MASK)
         == SMR_LOG_SAVEPOINT_OK)
    {
        smrLogHeadI::setReplStmtDepth(
                        &sLobCursorLog.mHead,
                        smLayerCallback::getLstReplStmtDepth( aTrans ) );
    }
    else
    {
        smrLogHeadI::setReplStmtDepth( &sLobCursorLog.mHead,
                                       SMI_STATEMENT_DEPTH_NULL );
    }

    sLobCursorLog.mOpType = aLobOp;
    sLobCursorLog.mLocator = aLobLocator;

    IDE_TEST( ((smxTrans*)aTrans)->writeLogToBuffer(
                                   &sLobCursorLog,
                                   SMR_LOGREC_SIZE(smrLobLog))
              != IDE_SUCCESS );
    // table
    IDE_TEST( ((smxTrans*)aTrans)->writeLogToBuffer(
                                   &aTable ,
                                   ID_SIZEOF(smOID))
              != IDE_SUCCESS );
    // column-id
    IDE_TEST( ((smxTrans*)aTrans)->writeLogToBuffer(
                                   &aLobColID ,
                                   ID_SIZEOF(UInt))
              != IDE_SUCCESS );

    /***************************************************/
    /*            write pk info                        */
    /*                                                 */
    /***************************************************/
    sPKColCount = sPKInfo->mTotalPKColCount;
    sPKInfoSize = sdcRow::calcPKLogSize(sPKInfo) - (2);

    IDE_TEST( ((smxTrans*)aTrans)->writeLogToBuffer(
                                   &sPKInfoSize,
                                   ID_SIZEOF(sPKInfoSize))
              != IDE_SUCCESS );

    IDE_TEST( ((smxTrans*)aTrans)->writeLogToBuffer(
                                   &sPKColCount,
                                   ID_SIZEOF(sPKColCount))
              != IDE_SUCCESS );

    for ( sPKColSeq = 0; sPKColSeq < sPKColCount; sPKColSeq++ )
    {
        sColumnInfo = sPKInfo->mColInfoList + sPKColSeq;
        sColumnId   = (UInt)sColumnInfo->mColumn->id;

        IDE_TEST( ((smxTrans*)aTrans)->writeLogToBuffer(
                                       &sColumnId,
                                       ID_SIZEOF(sColumnId))
                  != IDE_SUCCESS );

        if ( sColumnInfo->mValue.length == 0)
        {
            sPrefix = (UChar)SDC_NULL_COLUMN_PREFIX;
            IDE_TEST( ((smxTrans*)aTrans)->writeLogToBuffer(
                                           &sPrefix,
                                           ID_SIZEOF(sPrefix))
                      != IDE_SUCCESS );
        }
        else
        {
            if ( sColumnInfo->mValue.length >
                 SDC_COLUMN_LEN_STORE_SIZE_THRESHOLD )
            {
                sPrefix = (UChar)SDC_LARGE_COLUMN_PREFIX;
                IDE_TEST( ((smxTrans*)aTrans)->writeLogToBuffer(
                                               &sPrefix,
                                               ID_SIZEOF(sPrefix))
                          != IDE_SUCCESS );

                sLargeLen = (UShort)sColumnInfo->mValue.length;
                IDE_TEST( ((smxTrans*)aTrans)->writeLogToBuffer(
                                               &sLargeLen,
                                               ID_SIZEOF(sLargeLen))
                          != IDE_SUCCESS );
            }
            else
            {
                sSmallLen = (UChar)sColumnInfo->mValue.length;
                IDE_TEST( ((smxTrans*)aTrans)->writeLogToBuffer(
                                               &sSmallLen,
                                               ID_SIZEOF(sSmallLen))
                          != IDE_SUCCESS );
            }

            IDE_TEST( ((smxTrans*)aTrans)->writeLogToBuffer(
                          sColumnInfo->mValue.value,
                          sColumnInfo->mValue.length)
                      != IDE_SUCCESS );
        }
    }

    // tail
    IDE_TEST( ((smxTrans*)aTrans)->writeLogToBuffer(
                                   &sLogType,
                                   ID_SIZEOF(smrLogType))
              != IDE_SUCCESS );

    IDE_TEST( writeLog( aStatistics,
                        aTrans,
                        smLayerCallback::getLogBufferOfTrans( aTrans ),
                        NULL,    // Previous LSN Ptr
                        NULL,    // Log LSN Ptr 
                        NULL,    // End LSN Ptr
                        aTable ) // TableOID
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC smrLogMgr::writeLobCursorCloseLogRec( idvSQL*         aStatistics,
                                             void*           aTrans,
                                             smLobLocator    aLobLocator,
                                             smOID           aTableOID )
{
    IDE_TEST( writeLobOpLogRec(aStatistics,
                               aTrans,
                               aLobLocator,
                               SMR_LOB_CURSOR_CLOSE,
                               aTableOID )
              != IDE_SUCCESS );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


// TODO: aOldSize �����ϱ�
// PROJ-1362 LOB PREPARE4WRITE  LOG for replication.
// smrLogHead| lob locator | mOp | offset(4) | old size(4) | newSize
IDE_RC smrLogMgr::writeLobPrepare4WriteLogRec(idvSQL*          aStatistics,
                                              void*            aTrans,
                                              smLobLocator     aLobLocator,
                                              UInt             aOffset,
                                              UInt             aOldSize,
                                              UInt             aNewSize,
                                              smOID            aTableOID )
{
    UInt       sLogTypeFlag;
    smTID      sTransID;
    smrLobLog  sLobCursorLog;
    smrLogType sLogType;


    IDE_DASSERT( aTrans != NULL );
    idlOS::memset(&sLobCursorLog, 0x00, SMR_LOGREC_SIZE(smrLobLog));
    smLayerCallback::initLogBuffer( aTrans );


    smLayerCallback::getTxIDAnLogType( aTrans,
                                       &sTransID,
                                       &sLogTypeFlag );

    IDE_DASSERT (sLogTypeFlag == SMR_LOG_TYPE_NORMAL);
    sLogType =  SMR_LT_LOB_FOR_REPL;

    smrLogHeadI::setType(&sLobCursorLog.mHead,sLogType);
    smrLogHeadI::setSize(&sLobCursorLog.mHead,
                         SMR_LOGREC_SIZE(smrLobLog) +
                         ID_SIZEOF(UInt) +   // offset
                         ID_SIZEOF(UInt) +   // old size
                         ID_SIZEOF(UInt) +   // new size
                         ID_SIZEOF(smrLogTail) );

    smrLogHeadI::setTransID(&sLobCursorLog.mHead, sTransID);

    smrLogHeadI::setPrevLSN( &sLobCursorLog.mHead,
                             smLayerCallback::getLstUndoNxtLSN( aTrans ) );

    smrLogHeadI::setFlag(&sLobCursorLog.mHead, SMR_LOG_TYPE_NORMAL);

    if ( (smrLogHeadI::getFlag(&sLobCursorLog.mHead) & SMR_LOG_SAVEPOINT_MASK)
         == SMR_LOG_SAVEPOINT_OK)
    {
        smrLogHeadI::setReplStmtDepth(
                        &sLobCursorLog.mHead,
                        smLayerCallback::getLstReplStmtDepth( aTrans ) );
    }
    else
    {
        smrLogHeadI::setReplStmtDepth( &sLobCursorLog.mHead,
                                       SMI_STATEMENT_DEPTH_NULL );
    }

    sLobCursorLog.mOpType = SMR_PREPARE4WRITE;
    sLobCursorLog.mLocator = aLobLocator;

    IDE_TEST( ((smxTrans*)aTrans)->writeLogToBuffer(
                                   &sLobCursorLog,
                                   SMR_LOGREC_SIZE(smrLobLog))
              != IDE_SUCCESS );
    // offset
    IDE_TEST( ((smxTrans*)aTrans)->writeLogToBuffer(
                                   &aOffset ,
                                   ID_SIZEOF(UInt))
              != IDE_SUCCESS );

    // old size
    IDE_TEST( ((smxTrans*)aTrans)->writeLogToBuffer(
                                   &aOldSize ,
                                   ID_SIZEOF(UInt))
              != IDE_SUCCESS );

    // new size
    IDE_TEST( ((smxTrans*)aTrans)->writeLogToBuffer(
                                   &aNewSize ,
                                   ID_SIZEOF(UInt))
              != IDE_SUCCESS );


    // tail
    IDE_TEST( ((smxTrans*)aTrans)->writeLogToBuffer(
                                   &sLogType,
                                   ID_SIZEOF(smrLogType))
              != IDE_SUCCESS );

    IDE_TEST( writeLog( aStatistics,
                        aTrans,
                        smLayerCallback::getLogBufferOfTrans( aTrans ),
                        NULL,  // Previous LSN Ptr
                        NULL,  // Log LSN Ptr 
                        NULL,  // End LSN Ptr  
                        aTableOID )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC smrLogMgr::writeLobFinish2WriteLogRec( idvSQL*       aStatistics,
                                              void*         aTrans,
                                              smLobLocator  aLobLocator,
                                              smOID         aTableOID )
{
    IDE_TEST( writeLobOpLogRec(aStatistics,
                               aTrans,
                               aLobLocator,
                               SMR_FINISH2WRITE,
                               aTableOID )
              != IDE_SUCCESS );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

// PROJ-1362 LOB CURSOR CLOSE LOG,SMR_FINISH2WRITE  for replication.
IDE_RC smrLogMgr::writeLobOpLogRec( idvSQL*           aStatistics,
                                    void*             aTrans,
                                    smLobLocator      aLobLocator,
                                    smrLobOpType      aLobOp,
                                    smOID             aTableOID )
{

    UInt       sLogTypeFlag;
    smTID      sTransID;
    smrLobLog  sLobCursorLog;
    smrLogType sLogType;


    IDE_DASSERT( aTrans != NULL );
    idlOS::memset(&sLobCursorLog, 0x00, SMR_LOGREC_SIZE(smrLobLog));
    smLayerCallback::initLogBuffer( aTrans );

    IDE_ASSERT(( aLobOp == SMR_LOB_CURSOR_CLOSE)  ||
               (aLobOp ==  SMR_FINISH2WRITE ));

    smLayerCallback::getTxIDAnLogType( aTrans,
                                       &sTransID,
                                       &sLogTypeFlag );

    IDE_DASSERT (sLogTypeFlag == SMR_LOG_TYPE_NORMAL);


    sLogType =  SMR_LT_LOB_FOR_REPL;

    smrLogHeadI::setType(&sLobCursorLog.mHead,sLogType);
    smrLogHeadI::setSize(&sLobCursorLog.mHead,
                         SMR_LOGREC_SIZE(smrLobLog) +ID_SIZEOF(smrLogTail) );
    smrLogHeadI::setTransID(&sLobCursorLog.mHead, sTransID);

    smrLogHeadI::setPrevLSN( &sLobCursorLog.mHead,
                             smLayerCallback::getLstUndoNxtLSN( aTrans ) );

    smrLogHeadI::setFlag(&sLobCursorLog.mHead, SMR_LOG_TYPE_NORMAL);

    if ( (smrLogHeadI::getFlag(&sLobCursorLog.mHead) & SMR_LOG_SAVEPOINT_MASK)
         == SMR_LOG_SAVEPOINT_OK)
    {
        smrLogHeadI::setReplStmtDepth(
                        &sLobCursorLog.mHead,
                        smLayerCallback::getLstReplStmtDepth( aTrans ) );
    }
    else
    {
        smrLogHeadI::setReplStmtDepth( &sLobCursorLog.mHead,
                                       SMI_STATEMENT_DEPTH_NULL );
    }

    sLobCursorLog.mOpType = aLobOp;
    sLobCursorLog.mLocator = aLobLocator;

    IDE_TEST( ((smxTrans*)aTrans)->writeLogToBuffer(
                                   &sLobCursorLog,
                                   SMR_LOGREC_SIZE(smrLobLog))
              != IDE_SUCCESS );

    IDE_TEST( ((smxTrans*)aTrans)->writeLogToBuffer(
                                   &sLogType,
                                   ID_SIZEOF(smrLogType))
              != IDE_SUCCESS );

    IDE_TEST( writeLog( aStatistics,
                        aTrans,
                        smLayerCallback::getLogBufferOfTrans( aTrans ),
                        NULL,  // Previous LSN Ptr
                        NULL,  // Log LSN Ptr 
                        NULL,  // End LSN Ptr  
                        aTableOID )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

// PROJ-2047 Strengthening LOB
// smrLogHead| lob locator | mOp | offset(8) | size(8) | value(1)
IDE_RC smrLogMgr::writeLobEraseLogRec( idvSQL       * aStatistics,
                                       void         * aTrans,
                                       smLobLocator   aLobLocator,
                                       ULong          aOffset,
                                       ULong          aSize,
                                       smOID          aTableOID )
{
    UInt       sLogTypeFlag;
    smTID      sTransID;
    smrLobLog  sLobCursorLog;
    smrLogType sLogType;


    IDE_DASSERT( aTrans != NULL );
    idlOS::memset(&sLobCursorLog, 0x00, SMR_LOGREC_SIZE(smrLobLog));
    smLayerCallback::initLogBuffer( aTrans );


    smLayerCallback::getTxIDAnLogType( aTrans,
                                       &sTransID,
                                       &sLogTypeFlag );

    IDE_DASSERT (sLogTypeFlag == SMR_LOG_TYPE_NORMAL);
    sLogType =  SMR_LT_LOB_FOR_REPL;

    smrLogHeadI::setType( &sLobCursorLog.mHead,sLogType);
    smrLogHeadI::setSize( &sLobCursorLog.mHead,
                          SMR_LOGREC_SIZE(smrLobLog) +
                          ID_SIZEOF(ULong) +   // offset
                          ID_SIZEOF(ULong) +   // size
                          ID_SIZEOF(smrLogTail) );

    smrLogHeadI::setTransID( &sLobCursorLog.mHead, sTransID );

    smrLogHeadI::setPrevLSN( &sLobCursorLog.mHead,
                             smLayerCallback::getLstUndoNxtLSN( aTrans ) );

    smrLogHeadI::setFlag(&sLobCursorLog.mHead, SMR_LOG_TYPE_NORMAL);

    if ( (smrLogHeadI::getFlag(&sLobCursorLog.mHead) & SMR_LOG_SAVEPOINT_MASK)
         == SMR_LOG_SAVEPOINT_OK)
    {
        smrLogHeadI::setReplStmtDepth(
                        &sLobCursorLog.mHead,
                        smLayerCallback::getLstReplStmtDepth( aTrans ) );
    }
    else
    {
        smrLogHeadI::setReplStmtDepth( &sLobCursorLog.mHead,
                                       SMI_STATEMENT_DEPTH_NULL );
    }

    sLobCursorLog.mOpType = SMR_LOB_ERASE;
    sLobCursorLog.mLocator = aLobLocator;

    IDE_TEST( ((smxTrans*)aTrans)->writeLogToBuffer(
                                   &sLobCursorLog,
                                   SMR_LOGREC_SIZE(smrLobLog))
              != IDE_SUCCESS );
    // offset
    IDE_TEST( ((smxTrans*)aTrans)->writeLogToBuffer(
                                   &aOffset ,
                                   ID_SIZEOF(ULong))
              != IDE_SUCCESS );

    // size
    IDE_TEST( ((smxTrans*)aTrans)->writeLogToBuffer(
                                   &aSize ,
                                   ID_SIZEOF(ULong))
              != IDE_SUCCESS );

    // tail
    IDE_TEST( ((smxTrans*)aTrans)->writeLogToBuffer(
                                   &sLogType,
                                   ID_SIZEOF(smrLogType))
              != IDE_SUCCESS );

    IDE_TEST( writeLog( aStatistics,
                        aTrans,
                        smLayerCallback::getLogBufferOfTrans( aTrans ),
                        NULL,  // Previous LSN Ptr
                        NULL,  // Log LSN Ptr 
                        NULL,  // End LSN Ptr  
                        aTableOID )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


// PROJ-2047 Strengthening LOB
// smrLogHead| lob locator | mOp | offset(8)
IDE_RC smrLogMgr::writeLobTrimLogRec(idvSQL         * aStatistics,
                                     void*            aTrans,
                                     smLobLocator     aLobLocator,
                                     ULong            aOffset,
                                     smOID            aTableOID )
{
    UInt       sLogTypeFlag;
    smTID      sTransID;
    smrLobLog  sLobCursorLog;
    smrLogType sLogType;


    IDE_DASSERT( aTrans != NULL );
    idlOS::memset(&sLobCursorLog, 0x00, SMR_LOGREC_SIZE(smrLobLog));
    smLayerCallback::initLogBuffer( aTrans );


    smLayerCallback::getTxIDAnLogType( aTrans,
                                       &sTransID,
                                       &sLogTypeFlag );

    IDE_DASSERT (sLogTypeFlag == SMR_LOG_TYPE_NORMAL);
    sLogType =  SMR_LT_LOB_FOR_REPL;

    smrLogHeadI::setType( &sLobCursorLog.mHead,sLogType);
    smrLogHeadI::setSize( &sLobCursorLog.mHead,
                          SMR_LOGREC_SIZE(smrLobLog) +
                          ID_SIZEOF(ULong) +    // offset
                          ID_SIZEOF(smrLogTail) );

    smrLogHeadI::setTransID( &sLobCursorLog.mHead, sTransID );

    smrLogHeadI::setPrevLSN( &sLobCursorLog.mHead,
                             smLayerCallback::getLstUndoNxtLSN( aTrans ) );

    smrLogHeadI::setFlag( &sLobCursorLog.mHead, SMR_LOG_TYPE_NORMAL );

    if ( (smrLogHeadI::getFlag(&sLobCursorLog.mHead) & SMR_LOG_SAVEPOINT_MASK)
         == SMR_LOG_SAVEPOINT_OK)
    {
        smrLogHeadI::setReplStmtDepth(
                        &sLobCursorLog.mHead,
                        smLayerCallback::getLstReplStmtDepth( aTrans ) );
    }
    else
    {
        smrLogHeadI::setReplStmtDepth( &sLobCursorLog.mHead,
                                       SMI_STATEMENT_DEPTH_NULL );
    }

    sLobCursorLog.mOpType = SMR_LOB_TRIM;
    sLobCursorLog.mLocator = aLobLocator;

    IDE_TEST( ((smxTrans*)aTrans)->writeLogToBuffer( &sLobCursorLog,
                                                     SMR_LOGREC_SIZE(smrLobLog) )
              != IDE_SUCCESS );
    // offset
    IDE_TEST( ((smxTrans*)aTrans)->writeLogToBuffer( &aOffset ,
                                                     ID_SIZEOF(ULong) )
              != IDE_SUCCESS );

    // tail
    IDE_TEST( ((smxTrans*)aTrans)->writeLogToBuffer( &sLogType,
                                                     ID_SIZEOF(smrLogType) )
              != IDE_SUCCESS );

    IDE_TEST( writeLog( aStatistics,
                        aTrans,
                        smLayerCallback::getLogBufferOfTrans( aTrans ),
                        NULL,  // Previous LSN Ptr
                        NULL,  // Log LSN Ptr 
                        NULL,  // End LSN Ptr  
                        aTableOID )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * PROJ-1665
 * Description : Direct-Path Insert�� ����� Page ��ü�� Logging
 * Implementation :
 *    Direct-Path Buffer Manager�� flush �Ҷ�,
 *    Page ��ü�� ���Ͽ� Log�� ���涧 ���
 *
 *   +--------------------------------------------------+
 *   | smrDiskLog | sdrLogHdr | Page ���� | smrLogTail |
 *   +--------------------------------------------------+
 *
 *    - In
 *      aStatistics : statistics
 *      aTrans      : Page Log�� ������ �ϴ� transaction
 *      aBuffer     : buffer
 *      aPageGRID   : Page GRID
 *      aLSN        : LSN
 *    - Out
 **********************************************************************/
IDE_RC smrLogMgr::writeDPathPageLogRec( idvSQL * aStatistics,
                                        UChar  * aBuffer,
                                        scGRID   aPageGRID,
                                        smLSN  * aEndLSN )
{
    return writePageImgLogRec( aStatistics,
                               aBuffer,
                               aPageGRID,
                               SDR_SDP_WRITE_DPATH_INS_PAGE,
                               aEndLSN );
}



/***********************************************************************
 * PROJ-1867
 * Page Img Log�� ����Ѵ�.
 **********************************************************************/
IDE_RC smrLogMgr::writePageImgLogRec( idvSQL     * aStatistics,
                                      UChar      * aBuffer,
                                      scGRID       aPageGRID,
                                      sdrLogType   aLogType,
                                      smLSN      * aEndLSN )
{
    smrDiskPILog sPILog;

    IDE_DASSERT( aBuffer != NULL );

    //------------------------
    // smrDiskLog ����
    //------------------------

    idlOS::memset( &sPILog, 0x00, SMR_LOGREC_SIZE(smrDiskPILog) );

    smrLogHeadI::setType( &sPILog.mHead, SMR_DLT_REDOONLY );

    smrLogHeadI::setSize( &sPILog.mHead,
                          SMR_LOGREC_SIZE(smrDiskPILog) );

    smrLogHeadI::setTransID( &sPILog.mHead, SM_NULL_TID );
    smrLogHeadI::setFlag( &sPILog.mHead, SMR_LOG_TYPE_NORMAL );

    smrLogHeadI::setReplStmtDepth( &sPILog.mHead,
                                   SMI_STATEMENT_DEPTH_NULL );

    sPILog.mTableOID    = SM_NULL_OID;     // not use
    sPILog.mContType    = SMR_CT_END;      // ���� �α���
    sPILog.mRedoLogSize = ID_SIZEOF(sdrLogHdr) + SD_PAGE_SIZE;
    sPILog.mRefOffset   = 0;               // replication ����
    sPILog.mRedoType    = SMR_RT_DISKONLY; // runtime memory�� ���Ͽ� �Բ� ����� ����

    //------------------------
    // sdrLogHdr ����
    //------------------------
    sPILog.mDiskLogHdr.mGRID   = aPageGRID;
    sPILog.mDiskLogHdr.mLength = SD_PAGE_SIZE;
    sPILog.mDiskLogHdr.mType   = aLogType;

    //------------------------
    // page data
    //------------------------
    idlOS::memcpy( sPILog.mPage,
                   aBuffer,
                   SD_PAGE_SIZE );

    //------------------------
    // tail ( smrLogType == smrLogTail )
    //------------------------
    sPILog.mTail = SMR_DLT_REDOONLY;

    //------------------------
    // write log
    //------------------------
    IDE_TEST( writeLog( aStatistics,
                        NULL,         // transaction
                        (SChar*)&sPILog,
                        NULL,         // Previous LSN Ptr
                        NULL,         // Log LSN Ptr 
                        aEndLSN,      // End LSN Ptr  
                        SM_NULL_OID ) // TableOID
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/***********************************************************************
 * PROJ-1864
 * Page Consistent Log�� ����Ѵ�.
 **********************************************************************/
IDE_RC smrLogMgr::writePageConsistentLogRec( idvSQL     * aStatistics,
                                             scSpaceID    aSpaceID,
                                             scPageID     aPageID,
                                             UChar        aIsPageConsistent)
{
    scGRID               sPageGRID;
    smrPageCinsistentLog sPCLog;

    SC_MAKE_GRID( sPageGRID, aSpaceID, aPageID, 0 ) ;

    //------------------------
    // smrDiskLog ����
    //------------------------

    idlOS::memset( &sPCLog, 0x00, SMR_LOGREC_SIZE(smrPageCinsistentLog) );

    smrLogHeadI::setType( &sPCLog.mHead, SMR_DLT_REDOONLY );

    smrLogHeadI::setSize( &sPCLog.mHead,
                          SMR_LOGREC_SIZE(smrPageCinsistentLog) );

    smrLogHeadI::setTransID( &sPCLog.mHead, SM_NULL_TID );
    smrLogHeadI::setFlag( &sPCLog.mHead, SMR_LOG_TYPE_NORMAL );

    smrLogHeadI::setReplStmtDepth( &sPCLog.mHead,
                                   SMI_STATEMENT_DEPTH_NULL );

    sPCLog.mTableOID    = SM_NULL_OID;     // not use
    sPCLog.mContType    = SMR_CT_END;      // ���� �α���
    sPCLog.mRedoLogSize = ID_SIZEOF(sdrLogHdr) + ID_SIZEOF(aIsPageConsistent);
    sPCLog.mRefOffset   = 0;               // replication ����
    sPCLog.mRedoType    = SMR_RT_DISKONLY; // runtime memory�� ���Ͽ� �Բ� ����� ����
    //------------------------
    // sdrLogHdr ����
    //------------------------
    sPCLog.mDiskLogHdr.mGRID   = sPageGRID;
    sPCLog.mDiskLogHdr.mLength = ID_SIZEOF(aIsPageConsistent);
    sPCLog.mDiskLogHdr.mType   = SDR_SDP_PAGE_CONSISTENT;

    //------------------------
    // page data
    //------------------------
    sPCLog.mPageConsistent = aIsPageConsistent;

    //------------------------
    // tail ( smrLogType == smrLogTail )
    //------------------------
    sPCLog.mTail = SMR_DLT_REDOONLY;

    //------------------------
    // write log
    //------------------------
    IDE_TEST( writeLog( aStatistics,
                        NULL,         // transaction
                        (SChar*)&sPCLog,
                        NULL,         // Previous LSN Ptr
                        NULL,         // Log LSN Ptr 
                        NULL,         // End LSN Ptr  
                        SM_NULL_OID ) // TableOID
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/***********************************************************************
 * Description : SMR_OP_NULL Ÿ���� NTA �α� ���
 **********************************************************************/
IDE_RC smrLogMgr::writeNullNTALogRec( idvSQL* aStatistics,
                                      void  * aTrans,
                                      smLSN * aLSN )
{

    return writeNTALogRec( aStatistics,
                           aTrans,
                           aLSN,
                           0,             // meaningless
                           SMR_OP_NULL );

}

/***********************************************************************
 * Description : SMR_OP_SMM_PERS_LIST_ALLOC Ÿ���� NTA �α� ���
 **********************************************************************/
IDE_RC smrLogMgr::writeAllocPersListNTALogRec( idvSQL*    aStatistics,
                                               void     * aTrans,
                                               smLSN    * aLSN,
                                               scSpaceID  aSpaceID,
                                               scPageID   aFirstPID,
                                               scPageID   aLastPID )
{

    return writeNTALogRec( aStatistics,
                           aTrans,
                           aLSN,
                           aSpaceID,
                           SMR_OP_SMM_PERS_LIST_ALLOC,
                           aFirstPID,
                           aLastPID );

}


IDE_RC smrLogMgr::writeCreateTbsNTALogRec( idvSQL*    aStatistics,
                                           void     * aTrans,
                                           smLSN    * aLSN,
                                           scSpaceID  aSpaceID)
{
    return writeNTALogRec( aStatistics,
                           aTrans,
                           aLSN,
                           aSpaceID,
                           SMR_OP_SMM_CREATE_TBS,
                           0,
                           0 );

}


/* Disk�α��� Log ���� ���θ� �����Ѵ�

   [IN] aDiskLogWriteOption - �α��� ���� ���ΰ� ����ִ� Option����
   [IN] aLogHead - �α׾��� ���ΰ� ������ Log�� Head
 */
IDE_RC smrLogMgr::decideLogComp( UInt         aDiskLogWriteOption,
                                 smrLogHead * aLogHead )
{
    UChar  sFlag;

    if ( ( aDiskLogWriteOption & SMR_DISK_LOG_WRITE_OP_COMPRESS_MASK )
         == SMR_DISK_LOG_WRITE_OP_COMPRESS_TRUE )
    {
        // Log Head�� �ƹ��� �÷��׵� �������� ������
        // �⺻������ �α׸� �����Ѵ�.
    }
    else
    {
        // �α׸� �������� �ʵ��� Log Head�� Flag�� �����Ѵ�.
        sFlag = smrLogHeadI::getFlag( aLogHead );
        smrLogHeadI::setFlag( aLogHead, sFlag|SMR_LOG_FORBID_COMPRESS_OK );
    }

    return IDE_SUCCESS;

}

/* PROJ-1723 [MDW/INTEGRATOR] Altibase Plugin ����

   Table/Index/Sequence��
   Create/Alter/Drop DDL�� ���� Query String�� �α��Ѵ�.

   �α볻�� ================================================
   4/8 byte - smOID
   4 byte - UserName Length
   n byte - UserName
   4 byte - Statement Length
   n byte - Statement
 */
IDE_RC smrLogMgr::writeDDLStmtTextLog( idvSQL         * aStatistics,
                                       void           * aTrans,
                                       smrDDLStmtMeta * aDDLStmtMeta,
                                       SChar          * aStmtText,
                                       SInt             aStmtTextLen )
{

    UInt       sLogSize;
    UInt       sLogTypeFlag;
    smTID      sTransID;
    smrLogHead sLogHead;
    smrLogType sLogType;


    IDE_DASSERT( aTrans != NULL );
    IDE_DASSERT( aStmtText != NULL );
    IDE_DASSERT( aStmtTextLen > 0 );

    idlOS::memset(&sLogHead, 0x00, ID_SIZEOF(sLogHead));

    smLayerCallback::initLogBuffer( aTrans );

    smLayerCallback::getTxIDAnLogType( aTrans,
                                       &sTransID,
                                       &sLogTypeFlag );

    sLogSize = (ID_SIZEOF( smrLogHead )
                + ID_SIZEOF( smrDDLStmtMeta )
                + ID_SIZEOF( aStmtTextLen ) + aStmtTextLen + 1
                + ID_SIZEOF( smrLogType )) ;

    sLogType = SMR_LT_DDL_QUERY_STRING;
    smrLogHeadI::setType(&sLogHead, sLogType);
    smrLogHeadI::setSize(&sLogHead, sLogSize );
    smrLogHeadI::setTransID(&sLogHead, sTransID);

    smrLogHeadI::setPrevLSN( &sLogHead,
                             smLayerCallback::getLstUndoNxtLSN( aTrans ) );

    smrLogHeadI::setFlag(&sLogHead, SMR_LOG_TYPE_NORMAL);

    if ( (smrLogHeadI::getFlag(&sLogHead) & SMR_LOG_SAVEPOINT_MASK)
         == SMR_LOG_SAVEPOINT_OK)
    {
        smrLogHeadI::setReplStmtDepth(
                        &sLogHead,
                        smLayerCallback::getLstReplStmtDepth( aTrans ) );
    }
    else
    {
        smrLogHeadI::setReplStmtDepth( &sLogHead,
                                       SMI_STATEMENT_DEPTH_NULL );
    }

    // Log Head ���
    IDE_TEST( ((smxTrans*)aTrans)->writeLogToBuffer( &sLogHead,
                                                     ID_SIZEOF(sLogHead) )
              != IDE_SUCCESS );

    // Log Body ��� : DDLStmtMeta
    IDE_TEST( ((smxTrans*)aTrans)->writeLogToBuffer( aDDLStmtMeta,
                                                     ID_SIZEOF(smrDDLStmtMeta) )
              != IDE_SUCCESS );

    // Log Body ��� : 4 byte - Statement Text Length
    IDE_TEST( ((smxTrans*)aTrans)->writeLogToBuffer( &aStmtTextLen,
                                                     ID_SIZEOF(aStmtTextLen) )
              != IDE_SUCCESS );

    // Log Body ��� : aStmtTextLen bytes - Statement Text
    IDE_TEST( ((smxTrans*)aTrans)->writeLogToBuffer( aStmtText,
                                                     aStmtTextLen )
              != IDE_SUCCESS );
    IDE_TEST( ((smxTrans*)aTrans)->writeLogToBuffer("", 1)
              != IDE_SUCCESS );

    // Log Tail ���
    IDE_TEST( ((smxTrans*)aTrans)->writeLogToBuffer( &sLogType,
                                                     ID_SIZEOF(smrLogType) )
              != IDE_SUCCESS );

    IDE_TEST( writeLog( aStatistics,
                        aTrans,
                        smLayerCallback::getLogBufferOfTrans( aTrans ),
                        NULL,  // Previous LSN Ptr
                        NULL,  // Log LSN Ptr 
                        NULL,  // End LSN Ptr  
                        SM_NULL_OID )
              != IDE_SUCCESS );


    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : �α� Header(smrLogHead)�� previous undo LSN�� ����
 *
 * �α��� header�� previous undo LSN�� �����ϰ�, �α��� body
 * ����� ��ȯ�Ѵ�.
 *
 * + 2nd. code design
 *   - �־��� aPPrvLSN�� NULL�ƴϸ�, �α� header�� prvUndoLSN�� �־���
 *     aPPrvLSN���� �����Ѵ�.
 *   - ���� NULL �̸�, Ʈ������� ������ �α��� LSN�� ��� �α� header��
 *     prvUndoLSN�� �����ϸ�, Ʈ������� ���� ��쿡�� ID_UINT_MAX��
 *     �����Ѵ�.
 **********************************************************************/
void smrLogMgr::setLogHdrPrevLSN( void       * aTrans,
                                  smrLogHead * aLogHead,
                                  smLSN      * aPPrvLSN )
{
    UInt sLogFlag;
    UInt sLogTypeFlag;
    IDE_DASSERT( aLogHead != NULL );

    if ( aTrans != NULL )
    {
        // ������ �α�� ���� �ѹ��� ����,
        if ( smLayerCallback::getIsFirstLog( aTrans ) == ID_TRUE )
        {
            // Transaction ID�� �ο��Ǿ� �ִٸ�
            if ( smLayerCallback::getTransID( aTrans ) != SM_NULL_TID )
            {
                // �α� ����� BEGIN �÷��׸� �޾��ش�.
                smrLogHeadI::setFlag(aLogHead,
                                     smrLogHeadI::getFlag(aLogHead) | SMR_LOG_BEGINTRANS_OK);

                // BUG-15109
                // normal tx begin�� �� ù��° log�� ���
                // ������ replication flag�� 0���� �����Ѵ�.
                //PROJ-1608 Recovery From Replication
                sLogTypeFlag = smLayerCallback::getLogTypeFlagOfTrans(aTrans);
                if ( (sLogTypeFlag == SMR_LOG_TYPE_NORMAL) ||
                     (sLogTypeFlag == SMR_LOG_TYPE_REPL_RECOVERY) )
                {
                    sLogFlag = smrLogHeadI::getFlag(aLogHead) & ~SMR_LOG_TYPE_MASK;
                    sLogFlag = sLogFlag | (sLogTypeFlag & SMR_LOG_TYPE_MASK);
                    smrLogHeadI::setFlag(aLogHead, sLogFlag);
                }

                smLayerCallback::setIsFirstLog( aTrans, ID_FALSE );
            }
        }
    }

    /* ------------------------------------------------
     * 1. prev LSN ����
     * ----------------------------------------------*/
    if ( aPPrvLSN != NULL )
    {
        /* ------------------------------------------------
         *  prev LSN�� ���� �����ϱ� ���� ���ڷ� �޴� ����
         *  NTA �α��� preUndoLSN�� �����ϱ� ���ؼ��̴�.
         * ----------------------------------------------*/
        smrLogHeadI::setPrevLSN( aLogHead, *aPPrvLSN );
    }
    else
    {
        /* ------------------------------------------------
         * �Ϲ������� preUndoLSN�� �ش� Ʈ������� ����������
         * �ۼ��ߴ� �α��� LSN�� �� �����Ѵ�.
         * ������, Ʈ������� �ƴѰ��� ID_UINT_MAX�� �����Ѵ�.
         * ----------------------------------------------*/
        if ( aTrans != NULL )
        {
            smrLogHeadI::setPrevLSN( aLogHead, smLayerCallback::getLstUndoNxtLSN(aTrans) );
        }
        else
        {
            // To fix PR-3562
            smrLogHeadI::setPrevLSN( aLogHead,
                                     ID_UINT_MAX,  // FILEID
                                     ID_UINT_MAX ); // OFFSET
        }
    }

    return;
}

/* SMR_LT_FILE_BEGIN �α� ���
 *
 * �� �Լ��� ������ ���� ��η� ȣ��Ǹ�, �ΰ��� ��� lock() ȣ���� ���´�.
 *
 * 1. writeLog ���� lock() ��� -> reserveLogSpace -> writeFileBeginLog
 * 2. switchLogFileByForce ���� lock() ���  -> writeFileBeginLog */
void smrLogMgr::writeFileBeginLog()
{
    smLSN       sLSN;

    IDE_ASSERT( mCurLogFile != NULL );

    mFileBeginLog.mFileNo = mCurLogFile->mFileNo ;

    IDE_DASSERT( mLstLSN.mLSN.mOffset == 0 );
    // ���߿� �α׸� ������ Log�� Validity check�� ����
    // �αװ� ��ϵǴ� ���Ϲ�ȣ�� �α׷��ڵ��� ���ϳ� Offset�� �̿��Ͽ�
    // Magic Number�� �����صд�.
    smrLogHeadI::setMagic(&mFileBeginLog.mHead,
                          smrLogFile::makeMagicNumber(mLstLSN.mLSN.mFileNo,
                                                      mLstLSN.mLSN.mOffset));
    SM_SET_LSN( sLSN,
                mFileBeginLog.mFileNo,
                mLstLSN.mLSN.mOffset );
    
    smrLogHeadI::setLSN( &mFileBeginLog.mHead, sLSN );

    // File Begin Log�� �׻� �α������� �� ó���� ��ϵȴ�.
    IDE_ASSERT( mCurLogFile->mOffset == 0 );

    // File Begin Log�� �������� �ʰ� �ٷ� ����Ѵ�.
    // ���� :
    //     File�� ù��° Log�� LSN�� �д� �۾���
    //     ������ �����ϱ� ����
    mCurLogFile->append( (SChar *)&mFileBeginLog,
                         smrLogHeadI::getSize(&mFileBeginLog.mHead) );

    // ���� ��� LSN���� �α׸� ����ߴ����� Setting�Ѵ�.
    setLstWriteLSN( sLSN );

    // ���� ��� LSN���� �α׸� ����ߴ����� Setting�Ѵ�.
    setLstLSN( mCurLogFile->mFileNo,
               mCurLogFile->mOffset );
}

/* SMR_LT_FILE_END �α� ���
 *
 * �� �Լ��� ������ ���� ��η� ȣ��Ǹ�, �ΰ��� ��� lock() ȣ���� ���´�.
 *
 * 1. writeLog ���� lock() ��� -> reserveLogSpace -> writeFileEndLog
 * 2. switchLogFileByForce ���� lock() ���  -> writeFileEndLog */
void smrLogMgr::writeFileEndLog()
{
    smLSN       sLSN;

    // ���߿� �α׸� ������ Log�� Validity check�� ����
    // �αװ� ��ϵǴ� ���Ϲ�ȣ�� �α׷��ڵ��� ���ϳ� Offset�� �̿��Ͽ�
    // Magic Number�� �����صд�.
    smrLogHeadI::setMagic (&mFileEndLog.mHead,
                           smrLogFile::makeMagicNumber( mLstLSN.mLSN.mFileNo,
                                                        mLstLSN.mLSN.mOffset ) );

    // ���� �αװ� ��ϵ� LSN
    SM_SET_LSN( sLSN,
                mLstLSN.mLSN.mFileNo,
                mLstLSN.mLSN.mOffset );

    smrLogHeadI::setLSN( &mFileEndLog.mHead, sLSN );

    // �α������� FILE END LOG�� �������� ����ä�� ����Ѵ�.
    // ���� : �α������� Offset���� üũ�� ���� �� �� �ֵ��� �ϱ� ����
    mCurLogFile->append( (SChar *)&mFileEndLog,
                         smrLogHeadI::getSize(&mFileEndLog.mHead) );

    // ���� ��� LSN���� �α׸� ����ߴ����� Setting�Ѵ�.
    setLstWriteLSN( sLSN );

    /* BUG-35392 */
    if ( smrRecoveryMgr::mCopyToRPLogBufFunc != NULL )
    {
        /* BUG-32137     [sm-disk-recovery] The setDirty operation in DRDB causes
         * contention of LOG_ALLOCATION_MUTEX.
         * writeEndFileLog������ setLsnLSN�� ���� �ʴ´�.
         * FileEndLog ��� switchLogFile ������ �Ŀ� setLstLSN�ؾ� �ùٸ� LSN��
         * �����ȴ�. */

        // �������� ���� �����α׸� Replication Log Buffer�� ����
        copyLogToReplBuffer( NULL,
                             (SChar *)&mFileEndLog,
                             smrLogHeadI::getSize(&mFileEndLog.mHead),
                             sLSN );
    }
    else
    {
        /* nothing to do */
    }
}

/***********************************************************************
 * Description : �α׸� ����� ���� Ȯ��
 *
 * �α׸� ����� ������ ���� �α������� �˻��Ͽ�  ���� ��� �α�������
 * switch�Ѵ�. �� ��, mLstLSN�� ���ο� �α������� LSN���� �缳���Ѵ�.
 * ����, switch Ƚ���� checkpoint interval�� �����ϸ�, checkpoint��
 * �����Ѵ�.
 * !!) �α� �������� mutex�� ȹ���� ���Ŀ� ȣ��Ǿ�� �Ѵ�.
 *
 * + 2nd. code design
 *   - �α������� free ������ ������� �ʴٸ� �α������� switch��Ų��.
 *
 * aLogSize           - [IN]  ���� ����Ϸ��� �α� ���ڵ��� ũ��
 * aIsLogFileSwitched - [OUT] aLogSize��ŭ ����Ҹ��� ������ Ȯ���ϴ� �߿�
 *                            �α����� Switch�� �߻��ߴ����� ����
 **********************************************************************/
IDE_RC smrLogMgr::reserveLogSpace( UInt     aLogSize,
                                   idBool * aIsLogFileSwitched )
{
    static UInt    sLogFileEndSize;

    IDE_DASSERT( aLogSize > 0 );

    *aIsLogFileSwitched = ID_FALSE;

    /* �α����Ͽ� ���� ������ �α��� ���� */
    sLogFileEndSize = SMR_LOGREC_SIZE(smrFileEndLog);

    /* ------------------------------------------------
     * �α������� free ������ ���� ����� �αױ��̿�
     * ������ �αױ��̸� ��� ����� �� �ִ����� �Ǵ��Ͽ�
     * �����ϸ� switch ��Ų��.
     * ----------------------------------------------*/
    if ( mCurLogFile->mFreeSize < ((UInt)aLogSize + sLogFileEndSize) )
    {
        writeFileEndLog();

        //switch log file
        IDE_TEST( mLogFileMgr.switchLogFile(&mCurLogFile) != IDE_SUCCESS );

        // �� �α����Ϸ� switch�� �߻��߱� ������
        // �α׷��ڵ尡 ��ϵ� ��ġ�� �α������� offset�� 0�̾�� ��.
        IDE_ASSERT( mCurLogFile->mOffset == 0 );

        /* BUG-32137 [sm-disk-recovery] The setDirty operation in DRDB causes
         * contention of LOG_ALLOCATION_MUTEX. */

        // �α����� Swtich�� �߻��Ͽ����Ƿ�, ���� �αװ� ��ϵ� LSN�� ����
        setLstLSN( mCurLogFile->mFileNo,
                   mCurLogFile->mOffset );

        *aIsLogFileSwitched = ID_TRUE;
    }
    else
    {
        /* nothing to do */
    }

    // ���� �αװ� �ϳ��� ��ϵ��� ���� ����.
    // ������ ù��° �α׷��ڵ�� File Begin Log�� ����Ѵ�.

    // �α������� ������ �� File Begin Log�� ������� ������,
    // �̴� 0��° �α������� ��쿡�� ���������̴�.
    // �Ϲ� �α� ����߿� 0��° �α����Ͽ� ���ؼ��� ����ϱ� ����,
    // ���� log file switch��ƾ�� ������ �����Ѵ�.
    if ( mCurLogFile->mOffset == 0 )
    {
        writeFileBeginLog();
                                             
        // BUG-24701 ������ �α� ���� ������ ���� aLogSize�� Ŭ��� Error ó���Ѵ�.
        IDE_TEST_RAISE( ((UInt)aLogSize + sLogFileEndSize) >
                        mCurLogFile->mFreeSize, ERROR_INVALID_LOGSIZE );
    }
    else
    {
        /* nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERROR_INVALID_LOGSIZE );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_LogSizeExceedLogFileSize,
                                smuProperty::getLogFileSize(),
                                aLogSize));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : �ۼ��� MRDB/DRDB �α׸� �α����Ͽ� ���
 *
 * �α����Ͽ� �α׸� ����ϰ�, DRDB �α뿡 ���ؼ���
 * Begin LSN�� End LSN�� ��ȯ�ϰ�, MRDB �α뿡 ���ؼ��� Begin LSN��
 * ��ȯ�Ѵ�.
 *
 * - 2nd. code design
 *   + smrLogHead�� prev undo LSN�� �����Ѵ�.
 *   + �α��� �ϱ� ���� �α� �������� lock�� ȹ���ϰ�,
 *     �α׸� ����� ������ Ȯ���Ѵ�.
 *   + �α��� begin LSN�� out ���ڿ� �����Ѵ�.
 *   + current �α����Ͽ� �α׸� ����Ѵ�.
 *   + ����� �α��� vaildation�� �˻��Ѵ�.
 *   + ���ο� LSN�� last LSN ������ �ϰ�,
 *     DRDB �α��� ��쿡�� aEndLSN�� ������ ����
 *     �α� �������� lock�� Ǭ��
 *   + Ʈ������� ��� ����� �α��� begin LSN��
 *     Ʈ����ǿ� �����Ѵ�.
 *   + durability Type�� ���� commit �α״� sync��
 *     �����ϱ⵵ �Ѵ�.
 *
 **********************************************************************
 *   PROJ-1464 �з��� �α� ���� ����
 *
 *   aIsLogFileSwitched - [OUT] �α׸� ����ϴ� ���߿�
 *                              �α����� Switch�� �߻��ߴ����� ����
 *
 **********************************************************************/
IDE_RC smrLogMgr::writeLog( idvSQL   * aStatistics,
                            void     * aTrans,
                            SChar    * aRawLog,
                            smLSN    * aPPrvLSN,
                            smLSN    * aBeginLSN,
                            smLSN    * aEndLSN,
                            smOID      aTableOID )
{
    /* aTrans�� NULL�� �� �ִ�. */
    IDE_DASSERT( aRawLog != NULL );
    /* aBeginLSN, aEndLSN�� NULL�� ���� �� �ִ�. */

    smrLogHead    * sLogHead;
    UInt            sRawLogSize;
    SChar         * sLogToWrite     = NULL;     /* �α����Ͽ� ����� �α� */
    UInt            sLogSizeToWrite = 0;
    smLSN           sWrittenLSN;
    smrCompRes    * sCompRes    = NULL;
    UInt            sStage      = 0;
    UInt            sMinLogRecordSize;
    idBool          sIsLogFileSwitched;

    sIsLogFileSwitched = ID_FALSE;

    /* ------------------------------------------------
     * �־��� ���۸� smrLogHead�� casting�Ѵ�.
     * ----------------------------------------------*/
    sLogHead = (smrLogHead *)aRawLog;

    sRawLogSize = smrLogHeadI::getSize(sLogHead);

    /* �α� ������� ������ �۾��� ó�� */
    onBeforeWriteLog( aTrans,
                      aRawLog,
                      aPPrvLSN );

    sMinLogRecordSize = smuProperty::getMinLogRecordSizeForCompress();

    if ( (sRawLogSize >= sMinLogRecordSize) && (sMinLogRecordSize != 0) )
    {
        /* ���� ���ҽ��� �����´� */
        IDE_TEST( allocCompRes( aTrans, & sCompRes ) != IDE_SUCCESS );
        sStage = 1;

        IDE_TEST( tryLogCompression( sCompRes,
                                     aRawLog,
                                     sRawLogSize,
                                     &sLogToWrite,
                                     &sLogSizeToWrite,
                                     aTableOID ) != IDE_SUCCESS );
    }
    else
    {
        sLogToWrite     = aRawLog;
        sLogSizeToWrite = sRawLogSize;
    }

    if ( smuProperty::isFastUnlockLogAllocMutex() == ID_FALSE )
    {
        IDE_TEST( lockAndWriteLog ( aStatistics,  /* for replication */
                                    aTrans,
                                    sLogToWrite,
                                    sLogSizeToWrite,
                                    aRawLog,      /* for replication */
                                    sRawLogSize,  /* for replication */
                                    &sWrittenLSN,
                                    aEndLSN,
                                    &sIsLogFileSwitched )
                  != IDE_SUCCESS );
    }
    else
    {
        IDE_TEST( lockAndWriteLog4FastUnlock ( aStatistics,  /* for replication */
                                               aTrans,
                                               sLogToWrite,
                                               sLogSizeToWrite,
                                               aRawLog,      /* for replication */
                                               sRawLogSize,  /* for replication */
                                               &sWrittenLSN,
                                               aEndLSN,
                                               &sIsLogFileSwitched )
                  != IDE_SUCCESS );
    }

    /* ������ �α�� ���� �ѹ��� ����,
     * �α� ����Ŀ� ������ �۾��� ó�� */
    onAfterWriteLog( aStatistics,
                     aTrans,
                     sLogHead,
                     sWrittenLSN,
                     sLogSizeToWrite );

    if ( sStage == 1 )
    {
        /* ���� ���ҽ��� �ݳ��Ѵ�.
         * - �α� ����߿� �ٸ� Thread�� ������� ���ϵ���
         *   �α� ����� �Ϸ�� �Ŀ� �ݳ��ؾ� �Ѵ�. */
        sStage = 0;
        IDE_TEST( freeCompRes( aTrans, sCompRes ) != IDE_SUCCESS );
    }
    else
    {
        /* nothing to do */
    }

    if ( aBeginLSN != NULL )
    {
       *aBeginLSN = sWrittenLSN;
    }
    else
    {
        /* nothing to do */
    }

    // ���� �α����� Switch�� �߻��ߴٸ� �α����� SwitchȽ���� �����ϰ�
    // üũ����Ʈ�� �����ؾ� �� ���� ���θ� �����Ѵ�.
    if ( sIsLogFileSwitched == ID_TRUE )
    {
        IDE_TEST( onLogFileSwitched() != IDE_SUCCESS );
    }
    else
    {
        /* nothing to do ... */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();
    {
        switch (sStage)
        {
            case 1:
                IDE_ASSERT( freeCompRes( aTrans, sCompRes )
                            == IDE_SUCCESS );
                break;

            default:
                break;
        }
    }
    IDE_POP();

    return IDE_FAILURE;
}

/*
    ���� ���ҽ��� �����´�
    - Transaction�� ������ ��� Transaction�� �Ŵ޷��ִ�
      ���� ���ҽ��� ���
    - Transaction�� �������� ���� ���
      �����ϴ� ���� ���ҽ� Ǯ�� mCompResPool�� ���

   [IN] aTrans - Ʈ�����
   [OUT] aCompRes - �Ҵ�� ���� ���ҽ�
 */
IDE_RC smrLogMgr::allocCompRes( void        * aTrans,
                                smrCompRes ** aCompRes )
{
    IDE_DASSERT( aCompRes != NULL );

    smrCompRes * sCompRes ;

    if ( aTrans != NULL )
    {
        // Transaction�� ���� Resource�� �����´�.
        IDE_TEST( smLayerCallback::getTransCompRes( aTrans,
                                                    & sCompRes )
                  != IDE_SUCCESS );

        IDE_DASSERT( sCompRes != NULL );
    }
    else
    {
        IDE_TEST( mCompResPool.allocCompRes( & sCompRes ) != IDE_SUCCESS );
        IDE_DASSERT( sCompRes != NULL );
    }

    *aCompRes = sCompRes;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/*
    ���� ���ҽ��� �ݳ��Ѵ�.
    - Transaction�� ������ ��� �ݳ����� �ʾƵ� �ȴ�.
      ( ���� Transaction commit/rollback�� �ݳ��� )
    - Transaction�� �������� ���� ���
      �����ϴ� ���� ���ҽ� Ǯ�� mCompResPool�� �ݳ�

   [IN] aTrans - Ʈ�����
   [OUT] aCompRes - �ݳ��� ���� ���ҽ�
 */
IDE_RC smrLogMgr::freeCompRes( void       * aTrans,
                               smrCompRes * aCompRes )
{
    IDE_DASSERT( aCompRes != NULL );

    if ( aTrans != NULL )
    {
        // Transaction Rollback/Commit�� �ڵ� �ݳ���
        // �ƹ��͵� ���� �ʴ´�.
    }
    else
    {
        IDE_TEST( mCompResPool.freeCompRes( aCompRes ) != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/*
   �α��� ������ ������ ��� ���� �ǽ�

   [IN] aCompRes           - ���� ���ҽ�
   [IN] aRawLog            - ����Ǳ� ���� ���� �α�
   [IN] aRawLogSize        - ����Ǳ� ���� ���� �α��� ũ��
   [OUT] aLogToWrite       - �α����Ͽ� ����� �α�
   [OUT] aLogSizeToWrite   - �α����Ͽ� ����� �α��� ũ��
 */
IDE_RC smrLogMgr::tryLogCompression( smrCompRes    * aCompRes,
                                     SChar         * aRawLog,
                                     UInt            aRawLogSize,
                                     SChar        ** aLogToWrite,
                                     UInt          * aLogSizeToWrite,
                                     smOID           aTableOID )
{
    idBool       sDoCompLog;

    SChar      * sCompLog;        /* ����� �α� */
    UInt         sCompLogSize;

    SChar      * sLogToWrite;
    UInt         sLogSizeToWrite;

    // aTrans�� NULL�� �� �ִ�.
    IDE_DASSERT( aRawLog != NULL );
    IDE_DASSERT( aRawLogSize > 0 );
    IDE_DASSERT( aLogToWrite != NULL );
    IDE_DASSERT( aLogSizeToWrite != NULL );

    // �⺻������ ����� �α׸� ���
    sLogToWrite = aRawLog;
    sLogSizeToWrite = aRawLogSize;

    // �α� �����ؾ��ϴ��� ���� ����
    IDE_TEST( smrLogComp::shouldLogBeCompressed( aRawLog,
                                                 aRawLogSize,
                                                 &sDoCompLog )
              != IDE_SUCCESS );

    // �α� ���� �õ�
    if ( sDoCompLog == ID_TRUE )
    {
        /* BUG-31009 - [SM] Compression buffer allocation need
         *                  exception handling.
         * �α� ���࿡ ������ ��� ���ܰ� �ƴ϶� �׳� ����� �α׸� ����ϵ���
         * �Ѵ�. */
        if ( smrLogComp::createCompLog( & aCompRes->mCompBufferHandle,
                                        aRawLog,
                                        aRawLogSize,
                                        & sCompLog,
                                        & sCompLogSize,
                                        aTableOID ) == IDE_SUCCESS )
        {
            // ������ �αװ� �� �۾��� ��쿡�� ����α׷� ���
            if ( sCompLogSize < aRawLogSize  )
            {
                sLogToWrite = sCompLog;
                sLogSizeToWrite = sCompLogSize;
            }
            else
            {
                /* nothing to do */
            }
        }
        else
        {
            /* ���������� ����� �α� ��� */
        }
    }
    else
    {
        /* nothing to do */
    }

    *aLogToWrite     = sLogToWrite;
    *aLogSizeToWrite = sLogSizeToWrite;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* Log�� ���� Mutex�� ���� ���·� �α� ��� */
IDE_RC smrLogMgr::lockAndWriteLog( idvSQL   * aStatistics,
                                   void     * aTrans,
                                   SChar    * aRawOrCompLog,
                                   UInt       aRawOrCompLogSize,
                                   SChar    * aRawLog4Repl,
                                   UInt       aRawLogSize4Repl,
                                   smLSN    * aBeginLSN,
                                   smLSN    * aEndLSN,
                                   idBool   * aIsLogFileSwitched )
{
    smLSN        sLSN;
    idBool       sIsLocked          = ID_FALSE;
    idBool       sIsLock4NullTrans  = ID_FALSE;

    /* aTrans �� NULL�� ���� �� �ִ�. */
    IDE_DASSERT( aRawOrCompLog      != NULL );
    IDE_DASSERT( aRawOrCompLogSize   > 0 );
    IDE_DASSERT( aRawLog4Repl       != NULL );
    IDE_DASSERT( aRawLogSize4Repl    > 0 );
    IDE_DASSERT( aBeginLSN          != NULL );
    /* aEndLSN�� NULL�� ���� �� �ִ�. */
    IDE_DASSERT( aIsLogFileSwitched != NULL );

    IDE_DASSERT( smuProperty::isFastUnlockLogAllocMutex() == ID_FALSE );

    /* 1. �α��� �ϱ� ���� �α� �������� lock�� ȹ���ϰ�,
     *    �α׸� ����� ������ Ȯ���Ѵ�.
     *  - ���� �α����� ������ �����ϴٸ�, END �α����� �α׸�
     *      ����� ���� �α����Ϸ� switch �Ѵ�.
     *  - last LSN�� ���ο� �α������� LSN���� �缳���Ѵ�.
     *  - �α����� switch Ƚ���� checkpoint interval�� �����ϸ�
     *      checkpoint�� �����Ѵ�. */

    IDE_TEST( lock() != IDE_SUCCESS );
    sIsLocked = ID_TRUE;

    /* 1.check log size */
    /* �� �ӿ��� File End Log�� �����鼭 Log File Switch�� �߻��� �� �ִ�.
     * LSN���� �̺��� ���� ���� �Ǹ�, LSN���� �Ųٷ� ��ϵǴ� ������ �߻��Ѵ�. */
    IDE_TEST( reserveLogSpace( aRawOrCompLogSize,
                               aIsLogFileSwitched ) != IDE_SUCCESS );

    /* 2.�α��� ��� Begin LSN�� �����Ѵ�. */
    SM_SET_LSN( *aBeginLSN,
                mCurLogFile->mFileNo,
                mCurLogFile->mOffset );

    /* 3.���� �αװ� ��ϵ� LSN */
    SM_SET_LSN( sLSN,
                mLstLSN.mLSN.mFileNo,
                mLstLSN.mLSN.mOffset );

    /* Log File�� ���� ���� LSN�� mLstLSN�� ���� ���� LSN�� ���ƾ� �Ѵ� */
    IDE_DASSERT( smrCompareLSN::isEQ( aBeginLSN, &sLSN )
                 == ID_TRUE );

    /* �α������� �� ó������ FIle Begin �αװ� �����Ƿ�,
     * �Ϲݷαװ� �α������� Offset : 0 �� ��ϵǾ�� �ȵȴ�. */
    IDE_ASSERT( sLSN.mOffset != 0 );
    IDE_ASSERT( sLSN.mFileNo != ID_UINT_MAX );

    /* 4.Log�� LSN�� ����Ѵ�. 
     * smrCompResPool�� 8Byte align �� �´� ���� : memcpy ���� �����Ѵ� */
    smrLogHeadI::setLSN( (smrLogHead*)aRawOrCompLog, sLSN );

    /* 5.Log�� Magic Number�� ����Ѵ�.
     * smrCompResPool�� 8Byte align �� �´� ���� : memcpy ���� �����Ѵ� */
    smrLogHeadI::setMagic( (smrLogHead *)aRawOrCompLog,
                           smrLogFile::makeMagicNumber( sLSN.mFileNo, sLSN.mOffset) );
 
    /* 6.�α����Ͽ� �α׸� ����Ѵ�. */
    mCurLogFile->append( aRawOrCompLog, aRawOrCompLogSize );

    /* 7.���� ��� LSN���� �α׸� ����ߴ����� Setting�Ѵ�. */
    setLstWriteLSN( sLSN );
 
    /* 8.���ο� end offset�� last LSN ������ �ϰ�, 
      lstLSN�� ������ ���� �α� �������� lock�� Ǭ��. */
    setLstLSN( mCurLogFile->mFileNo,
               mCurLogFile->mOffset );
  
    /* BUG-35392 */
    if ( smrRecoveryMgr::mCopyToRPLogBufFunc != NULL )
    {
        /* �������� ���� �����α׸� Replication Log Buffer�� ���� */
        copyLogToReplBuffer( aStatistics,
                             aRawLog4Repl,
                             aRawLogSize4Repl,
                             sLSN );
    }
    else
    {
        /* nothing to do */
    }

    sIsLocked = ID_FALSE;
    IDE_TEST( unlock() != IDE_SUCCESS );

    // 9.���� ���� sync �ؾ� �Ѵ�. 
    if ( aEndLSN != NULL )
    {
        SM_SET_LSN( *aEndLSN,
                    mLstLSN.mLSN.mFileNo,
                    mLstLSN.mLSN.mOffset);
    }
    else
    {
        /* nothing to do */
    }

    if ( smuProperty::getLogBufferType() == SMU_LOG_BUFFER_TYPE_MEMORY )
    {
        /* Log Buffer Type�� memory�� ���,
         * Update Transaction ����
         * �������Ѿ� �ϴ��� üũ�� ������Ŵ */
        checkIncreaseUpdateTxCount(aTrans);
    }
    else
    {
        /* nothing to do ... */
    }

    /* PROJ-2453 Eager Replication performance enhancement */
    if ( smrRecoveryMgr::mSendXLogFunc != NULL )
    {
        smrRecoveryMgr::mSendXLogFunc( aRawLog4Repl );
    }
    else
    {
        /* Nothing to do */
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sIsLocked == ID_TRUE )
    {
        IDE_PUSH();
        IDE_ASSERT( unlock() == IDE_SUCCESS );
        IDE_POP();
    }

    if ( sIsLock4NullTrans == ID_TRUE )
    {
        IDE_DASSERT( aTrans == NULL );

        sIsLock4NullTrans = ID_FALSE;
        IDE_ASSERT( mMutex4NullTrans.unlock() == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

/* Log�� ���� Mutex�� ���� ���·� �α� ��� */
IDE_RC smrLogMgr::lockAndWriteLog4FastUnlock( idvSQL   * aStatistics,
                                              void     * aTrans,
                                              SChar    * aRawOrCompLog,
                                              UInt       aRawOrCompLogSize,
                                              SChar    * aRawLog4Repl,
                                              UInt       aRawLogSize4Repl,
                                              smLSN    * aBeginLSN,
                                              smLSN    * aEndLSN,
                                              idBool   * aIsLogFileSwitched )
{
    smLSN        sLSN;
    UInt         sSlotID            = 0;
    idBool       sIsLocked          = ID_FALSE;
    idBool       sIsSetFstChkLSN    = ID_FALSE;
    idBool       sIsLock4NullTrans  = ID_FALSE;
    smrLogFile * sCurLogFile        = NULL;
    smLSN        sEndLSN;
#ifdef DEBUG
    smLSN        sEndLSN4Debug;
#endif

    /* aTrans �� NULL�� ���� �� �ִ�. */
    IDE_DASSERT( aRawOrCompLog      != NULL );
    IDE_DASSERT( aRawOrCompLogSize   > 0 );
    IDE_DASSERT( aRawLog4Repl       != NULL );
    IDE_DASSERT( aRawLogSize4Repl    > 0 );
    IDE_DASSERT( aBeginLSN          != NULL );
    /* aEndLSN�� NULL�� ���� �� �ִ�. */
    IDE_DASSERT( aIsLogFileSwitched != NULL );

    IDE_DASSERT( smuProperty::isFastUnlockLogAllocMutex() == ID_TRUE );
    /* 1. �α��� �ϱ� ���� �α� �������� lock�� ȹ���ϰ�,
     *    �α׸� ����� ������ Ȯ���Ѵ�.
     *  - ���� �α����� ������ �����ϴٸ�, END �α����� �α׸�
     *      ����� ���� �α����Ϸ� switch �Ѵ�.
     *  - last LSN�� ���ο� �α������� LSN���� �缳���Ѵ�.
     *  - �α����� switch Ƚ���� checkpoint interval�� �����ϸ�
     *      checkpoint�� �����Ѵ�. */

    /* BUG-35392 */
    /* aTrans�� NULL�� ���� �� �ִ�. */
    if ( aTrans != NULL )
    {
        sSlotID = (UInt)smLayerCallback::getTransSlot( aTrans );
    }
    else
    {   /* �ý��� Ʈ������� �ѹ��� �ϳ��� ���� ���� */
        sSlotID = mFstChkLSNArrSize - 1 ;

        IDE_TEST( mMutex4NullTrans.lock( NULL ) != IDE_SUCCESS );
        sIsLock4NullTrans = ID_TRUE;
    }

    /* �����ϰ� ������ �ִ� LSN�� ���ϱ� ���� üũ */
    setFstCheckLSN( sSlotID );
    sIsSetFstChkLSN = ID_TRUE;

    IDE_TEST( lock() != IDE_SUCCESS );
    sIsLocked = ID_TRUE;

    /* 1.check log size */
    /* �� �ӿ��� File End Log�� �����鼭 Log File Switch�� �߻��� �� �ִ�.
     * LSN���� �̺��� ���� ���� �Ǹ�, LSN���� �Ųٷ� ��ϵǴ� ������ �߻��Ѵ�. */
    IDE_TEST( reserveLogSpace( aRawOrCompLogSize,
                               aIsLogFileSwitched ) != IDE_SUCCESS );

    /* 2.Ȯ���� �α� ũ�� ��ŭ �α� ������ ũ�⸦ �����Ѵ�. */
    mCurLogFile->mFreeSize -= aRawOrCompLogSize;
    mCurLogFile->mOffset   += aRawOrCompLogSize;

    /* 3.���� �αװ� ��ϵ� LSN */
    SM_SET_LSN( sLSN,
                mLstLSN.mLSN.mFileNo,
                mLstLSN.mLSN.mOffset );

    /* 4.���� �α��� ������ offset */
    SM_SET_LSN( sEndLSN,
                mLstLSN.mLSN.mFileNo,
                mLstLSN.mLSN.mOffset+aRawOrCompLogSize );

#ifdef DEBUG
    SM_SET_LSN( sEndLSN4Debug,
                mCurLogFile->mFileNo,
                mCurLogFile->mOffset );

    /* Log File�� ���� ���� LSN�� mLstLSN�� ���� ���� LSN�� ���ƾ� �Ѵ� */
    IDE_DASSERT( smrCompareLSN::isEQ( &sEndLSN4Debug, &sEndLSN )
                 == ID_TRUE );
#endif   

    /* 5.���� ��� LSN���� �α׸� ����ߴ����� Setting�Ѵ�. */
    setLstWriteLSN( sLSN );
 
    /* 6.���ο� end offset�� last LSN ������ �ϰ�, 
      lstLSN�� ������ ���� �α� �������� lock�� Ǭ��. */
    setLstLSN( sEndLSN.mFileNo,
               sEndLSN.mOffset );

    /* BUG-35392 */
    /* �������� ���� �����α׸� Replication Log Buffer�� ���� */
    if ( smrRecoveryMgr::mCopyToRPLogBufFunc != NULL )
    {
        copyLogToReplBuffer( aStatistics,
                             aRawLog4Repl,
                             aRawLogSize4Repl,
                             sLSN );
    }
    else
    {
        /* nothing to do */
    }

    /* 7.AllocMutex�� unlock�ϸ� mCurLogFile ���� ���� �� �� �ִ�.
     *    ���� ó���� ���� �����صд�. */
    sCurLogFile = mCurLogFile;

    sIsLocked = ID_FALSE;
    IDE_TEST( unlock() != IDE_SUCCESS );

    /* �α������� �� ó������ FIle Begin �αװ� �����Ƿ�,
     * �Ϲݷαװ� �α������� Offset : 0 �� ��ϵǾ�� �ȵȴ�. */
    IDE_ASSERT( sLSN.mOffset != 0 );

    /* 1.Log�� LSN�� ����Ѵ�.
     * smrCompResPool�� 8Byte align �� �´� ���� : memcpy ���� �����Ѵ� */
    smrLogHeadI::setLSN( (smrLogHead*)aRawOrCompLog, sLSN );

    /* 2.Log�� Magic Number�� ��� �Ѵ�.
     * smrCompResPool�� 8Byte align �� �´� ���� : memcpy ���� �����Ѵ� */
    smrLogHeadI::setMagic( (smrLogHead *)aRawOrCompLog,
                           smrLogFile::makeMagicNumber( sLSN.mFileNo, sLSN.mOffset) );

    /* 3.�α����Ͽ� �α׸� ����Ѵ�. */
    sCurLogFile->scribble( aRawOrCompLog,
                           aRawOrCompLogSize,
                           sLSN.mOffset );

    /* 4.�α��� ��� Begin LSN�� �����Ѵ�. */
    SM_GET_LSN( *aBeginLSN, sLSN );

    /* 5.���� ���� sync �ؾ� �Ѵ�. */
    if ( aEndLSN != NULL )
    {
        SM_GET_LSN( *aEndLSN, sEndLSN );
    }
    else
    {
        /* nothing to do */
    }

    sIsSetFstChkLSN = ID_FALSE;
    unsetFstCheckLSN( sSlotID );

    if ( sIsLock4NullTrans == ID_TRUE )
    {
        IDE_DASSERT( aTrans == NULL );

        sIsLock4NullTrans = ID_FALSE;
        IDE_TEST( mMutex4NullTrans.unlock() != IDE_SUCCESS );
    }
    else
    {
        /* nothing to do */
    }

    if ( smuProperty::getLogBufferType() == SMU_LOG_BUFFER_TYPE_MEMORY )
    {
        /* Log Buffer Type�� memory�� ���,
         * Update Transaction ����
         * �������Ѿ� �ϴ��� üũ�� ������Ŵ */
        checkIncreaseUpdateTxCount(aTrans);
    }
    else
    {
        /* nothing to do ... */
    }

    /* PROJ-2453 Eager Replication performance enhancement */
    if ( smrRecoveryMgr::mSendXLogFunc != NULL )
    {
        smrRecoveryMgr::mSendXLogFunc( aRawLog4Repl );
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sIsLocked == ID_TRUE )
    {
        IDE_PUSH();
        IDE_ASSERT( unlock() == IDE_SUCCESS );
        IDE_POP();
    }

    if ( sIsSetFstChkLSN == ID_TRUE )
    {
        sIsSetFstChkLSN = ID_FALSE;
        unsetFstCheckLSN( sSlotID );
    }

    if ( sIsLock4NullTrans == ID_TRUE )
    {
        IDE_DASSERT( aTrans == NULL );

        sIsLock4NullTrans = ID_FALSE;
        IDE_ASSERT( mMutex4NullTrans.unlock() == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

/*********************************************************************
 *  �������� ���� �����α׸� Replication Log Buffer�� ����
 *
 *  - Replication Log Buffer�� Sender�� ���� ����ȭ�� ����
 *    �α׸� Memory�κ��� ��� �б� ���� �����̴�.
 *
 *  - ������� ���� ���� �α׸� ��� �Ͽ� ���� ó���� �����Ѵ�.
 *
 * [IN] aRawLog     - ������� ���� �����α�
 * [IN] aRawLogSize - ������� ���� �����α��� ũ��
 * [IN] aLSN        - �α��� LSN
 *********************************************************************/
void smrLogMgr::copyLogToReplBuffer( idvSQL * aStatistics,
                                     SChar  * aRawLog,
                                     UInt     aRawLogSize,
                                     smLSN    aLSN )
{
    IDE_DASSERT( aRawLog    != NULL );
    IDE_DASSERT( aRawLogSize > 0 );
    IDE_DASSERT( smrLogComp::isCompressedLog( aRawLog ) == ID_FALSE );

    /* ���� Log�� LSN�� ����Ѵ�. */
    smrLogHeadI::setLSN( (smrLogHead*)aRawLog, aLSN );

    /* ���� Log�� Magic Number�� ����Ѵ�. */
    smrLogHeadI::setMagic( (smrLogHead *)aRawLog,
                           smrLogFile::makeMagicNumber( aLSN.mFileNo, aLSN.mOffset) );

    /* Replication log Buffer manager���� ó���� ��û�Ѵ�. */
    smrRecoveryMgr::mCopyToRPLogBufFunc( aStatistics,
                                         aRawLogSize,
                                         aRawLog,
                                         aLSN );
}


/*
    �α� ������� ������ �۾� ó��
      - Prev LSN���
      - Commit Log�� �ð� ���
 */
void smrLogMgr::onBeforeWriteLog( void     * aTrans,
                                  SChar    * aRawLog,
                                  smLSN    * aPPrvLSN )
{
    // aTrans �� NULL�� ���� �� �ִ�.
    IDE_DASSERT( aRawLog != NULL );
    // aPPrvLSN �� NULL�� ���� �� �ִ�.

    smrLogHead*              sLogHead;
    smrTransCommitLog      * sCommitLog;
    smrTransGroupCommitLog * sGroupCommitLog;

    sLogHead = (smrLogHead*)aRawLog;

    // �����α״� ������� ���� �α׿��� �Ѵ�.
    IDE_DASSERT( (smrLogHeadI::getFlag(sLogHead) & SMR_LOG_COMPRESSED_MASK)
                 ==  SMR_LOG_COMPRESSED_NO );

    /* ------------------------------------------------
     * 1. smrLogHead�� prev undo LSN�� �����Ѵ�.
     * ----------------------------------------------*/
    setLogHdrPrevLSN(aTrans, sLogHead, aPPrvLSN);

    /* ���� tx commit �α׶�� time value�� �����Ѵ�. */
    if ( (smrLogHeadI::getType(sLogHead) == SMR_LT_MEMTRANS_COMMIT) ||
         (smrLogHeadI::getType(sLogHead) == SMR_LT_DSKTRANS_COMMIT) )
    {
        sCommitLog = (smrTransCommitLog*)aRawLog;
        sCommitLog->mTxCommitTV = smLayerCallback::getCurrTime();
    }
    else
    {
        /* BUG-47525 Group Commit�� time value�� �����ؾ��Ѵ�. */
        if ( smrLogHeadI::getType(sLogHead) == SMR_LT_MEMTRANS_GROUPCOMMIT )
        {
            sGroupCommitLog = (smrTransGroupCommitLog*)aRawLog;
            sGroupCommitLog->mTxCommitTV = smLayerCallback::getCurrTime();
        }
    }

#ifdef DEBUG
   /* ------------------------------------------------
    * 2. ���ۿ� ��ϵ� �α��� Head�� Tail�� ��ġ�ϴ��� ��
    * ----------------------------------------------*/
    validateLogRec( aRawLog );
#endif
}

/*
    �α� ����Ŀ� ������ �۾��� ó��

    [IN] aTrans   - Transaction
    [IN] aLogHead - ����� �α��� Head
    [IN] aLSN     - ����� �α��� LSN
    [IN] aWrittenLogSize - ����� �α��� ũ��
 */
void smrLogMgr::onAfterWriteLog( idvSQL     * aStatistics,
                                 void       * aTrans,
                                 smrLogHead * aLogHead,
                                 smLSN        aLSN,
                                 UInt         aWrittenLogSize )
{

    /* 1. Ʈ������� ��� ����� �α��� begin LSN�� Last LogLSN��
     *    Ʈ����ǿ� �����Ѵ�. */
    updateTransLSNInfo( aStatistics,
                        aTrans,
                        &aLSN,
                        aWrittenLogSize );

    if ( smuProperty::getLogBufferType() == SMU_LOG_BUFFER_TYPE_MEMORY )
    {
        // BUG-15396
        // Log Buffer Type�� memory�� ���, Update Transaction ����
        // ���ҽ��Ѿ� �ϴ��� üũ�� ���ҽ�Ŵ
        // Group Commit �� �ʿ��� ������, mmap�� ��쿡�� �ӵ���
        // ������ ������ group commit �� �ʿ䰡 ��� �������� ����
        checkDecreaseUpdateTxCount(aTrans, aLogHead);
    }
    else
    {
        /* nothing to do ... */
    }
}



/*
  Transaction�� �α׸� ����� ��
  Update Transaction�� ���� �������Ѿ� �ϴ��� üũ�Ѵ�.

  - �ϳ��� Ʈ����ǿ� ���� �αװ� ���� ��ϵ� �� 1 ����
  - Restart Recovery�߿��� Count���� ����
  - ���⼭ ����� Update Transaction�� ���� Group Commit�� �ǽ������� ���ΰ���

  aTrans   [IN] �α׸� ����Ϸ��� Ʈ����� ��ü
  aLogHead [IN] ����Ϸ��� �α��� Head
*/
inline void smrLogMgr::checkIncreaseUpdateTxCount( void       * aTrans )
{
    // Restart Recovery�߿��� Active Transaction���� Rollback��
    // �α׸� �ѹ��� ������� �ʰ� Abort Log�� ����� �� �ִ�.
    //
    // Normal Processing�϶��� Update Transaction�� Count�Ѵ�.
    if ( smrRecoveryMgr::isRestartRecoveryPhase() == ID_FALSE )
    {
        if ( aTrans != NULL )
        {
            // Log�� ����ϴ� �������� ReadOnly���,
            // �� Ʈ������� ���ʷ� �α׸� ����ϴ� ���̴�.
            if ( smLayerCallback::isReadOnly( aTrans ) == ID_TRUE )
            {
                // Update Transaction ���� �ϳ� ������Ų��.
                incUpdateTxCount();

                // smrLogMgr::writeLog����
                // smLayerCallback::setLstUndoNxtLSN(aTrans, &sLSN)
                // �� ȣ���� �� Ʈ������� Update Transaction���� �����ȴ�.
            }
            else
            {
                /* nothing to do */
            }
        }
        else
        {
            /* nothing to do */
        }
    }
    else
    {
        /* nothing to do */
    }
}

/*
  Transaction�� �α׸� ����� ��
  Update Transaction�� ���� ���ҽ��Ѿ� �ϴ��� üũ�Ѵ�.

  - Commit�̳� Abort�αװ� ��ϵ� �� 1����
  - Restart Recovery�߿��� Count���� ����
  - ���⼭ ����� Update Transaction�� ���� Group Commit�� �ǽ������� ���ΰ���

  aTrans   [IN] �α׸� ����Ϸ��� Ʈ����� ��ü
  aLogHead [IN] ����Ϸ��� �α��� Head
*/
inline void smrLogMgr::checkDecreaseUpdateTxCount( void       * aTrans,
                                                   smrLogHead * aLogHead )
{
    UInt sCnt;
    UInt sGroupCnt;
    smrTransGroupCommitLog * sGroupCommitLog;

    // Restart Recovery�߿��� Active Transaction���� Rollback��
    // �α׸� �ѹ��� ������� �ʰ� Abort Log�� ����� �� �ִ�.
    //
    // Normal Processing�϶��� Update Transaction�� Count�Ѵ�.
    if ( smrRecoveryMgr::isRestartRecoveryPhase() == ID_FALSE )
    {
        if ( aTrans != NULL )
        {
            // Log�� ����ϴ� �������� ReadOnly���,
            // �� Ʈ������� ���ʷ� �α׸� ����ϴ� ���̴�.
            if ( smLayerCallback::isReadOnly( aTrans ) == ID_FALSE )
            {
                // Update Transaction�� ���������� ��� �α׶��?
                if ( (smrLogHeadI::getType( aLogHead ) == SMR_LT_MEMTRANS_COMMIT) ||
                     (smrLogHeadI::getType( aLogHead ) == SMR_LT_DSKTRANS_COMMIT) ||
                     (smrLogHeadI::getType( aLogHead ) == SMR_LT_MEMTRANS_ABORT)  ||
                     (smrLogHeadI::getType( aLogHead ) == SMR_LT_DSKTRANS_ABORT) )
                {
                    // Update Transaction�� ���� �ϳ� �����Ѵ�.
                    decUpdateTxCount();
                }
                else
                {
                    /* BUG-47525 Group Commit�� ��� Group ȭ �� Tx ����ŭ
                     * Update Transaction ���� �ٿ��־�� �Ѵ�. 
                     * Groupȭ�� ������ Tx���� writeLog �� ȣ������ �ʱ� ���� */
                    if ( smrLogHeadI::getType( aLogHead ) == SMR_LT_MEMTRANS_GROUPCOMMIT )
                    {
                        sGroupCommitLog = (smrTransGroupCommitLog*) aLogHead;
                        sGroupCnt =  sGroupCommitLog->mGroupCnt;
                        for ( sCnt = 0; sCnt < sGroupCnt; sCnt++)
                        {
                            decUpdateTxCount();
                        }
                    }
                }
            }
            else
            {
                /* nothing to do */
            }
        }
        else
        {
            /* nothing to do */
        }
// assertion code
#if defined(DEBUG)
        // Update Transaction�� ���������� ��� �α׶��?
        if ( (smrLogHeadI::getType( aLogHead ) == SMR_LT_MEMTRANS_COMMIT) ||
             (smrLogHeadI::getType( aLogHead ) == SMR_LT_DSKTRANS_COMMIT) ||
             (smrLogHeadI::getType( aLogHead ) == SMR_LT_MEMTRANS_ABORT)  ||
             (smrLogHeadI::getType( aLogHead ) == SMR_LT_DSKTRANS_ABORT)  ||
             (smrLogHeadI::getType( aLogHead ) == SMR_LT_MEMTRANS_GROUPCOMMIT) )
        {
            // Transaction ���� Commit/Abort �Ұ�
            IDE_DASSERT( aTrans != NULL );
            // Readonly Transaction�� Commit/Abort�α׸� ���� �� ����
            IDE_DASSERT( smLayerCallback::isReadOnly( aTrans ) == ID_FALSE );
        }
        else
        {
            /* nothing to do */
        }
#endif /* DEBUG */
    }
    else
    {
        /* nothing to do */
    }
}

/***********************************************************************
 * Description : Ư�� �α������� ù��° �α��� Head�� �д´�.
 *
 * aFileNo  - [IN] �о���� �α������� ��ȣ
 * aLogHead - [OUT] �о���� �α��� Header�� ���� Output Parameter
 ***********************************************************************/
IDE_RC smrLogMgr::readFirstLogHeadFromDisk( UInt         aFileNo,
                                            smrLogHead * aLogHead )
{
    SChar    sLogFileName[SM_MAX_FILE_NAME];
    iduFile  sFile;
    SInt     sState = 0;

    IDE_TEST( sFile.initialize( IDU_MEM_SM_SMR,
                                1, /* Max Open FD Count */
                                IDU_FIO_STAT_OFF,
                                IDV_WAIT_INDEX_NULL )
              != IDE_SUCCESS );
    sState = 1;

    idlOS::snprintf( sLogFileName,
                     SM_MAX_FILE_NAME,
                     "%s%c%s%"ID_UINT32_FMT,
                     getLogPath(),
                     IDL_FILE_SEPARATOR,
                     SMR_LOG_FILE_NAME,
                     aFileNo );

    IDE_TEST( sFile.setFileName( sLogFileName )
              != IDE_SUCCESS );

    IDE_TEST( sFile.open( ID_FALSE ) != IDE_SUCCESS );
    sState = 2;

    // �α����� ��ü�� �ƴϰ� ù��° �α��� Head�� �о� ���δ�.
    IDE_TEST( sFile.read( NULL,
                          0,
                          (void*)aLogHead,
                          ID_SIZEOF(smrLogHead) )
              != IDE_SUCCESS );

    // �α� ������ ù��° �α״� �������� �ʴ´�.
    // ���� :
    //     File�� ù��° Log�� LSN�� �д� �۾���
    //     ������ �����ϱ� ����
    IDE_ASSERT( smrLogComp::isCompressedLog( (SChar*)aLogHead ) == ID_FALSE );

    sState = 1;
    IDE_TEST( sFile.close() != IDE_SUCCESS );

    sState = 0;
    IDE_TEST( sFile.destroy() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sState != 0 )
    {
        IDE_PUSH();
        switch( sState )
        {
            case 2:
                (void)sFile.close();
            case 1:
                (void)sFile.destroy();
            default:
                break;
        }
        IDE_POP();
    }
    else
    {
        /* nothing to do */
    }

    return IDE_FAILURE;
}


/***********************************************************************
 * Description :
 * ������ ���� �α������� switch��Ų��.
 * Switch�� �α������� ������ sync�� ��, archive ��Ų��.
 **********************************************************************/
IDE_RC smrLogMgr::switchLogFileByForce()
{

    UInt               sSwitchedLogFileNo;
    UInt               sSwitchedLogOffset;
    UInt               sLogFileEndSize;
    UInt               sState = 0;

    IDE_TEST( lock() != IDE_SUCCESS );
    sState = 1;

    sLogFileEndSize = SMR_LOGREC_SIZE(smrFileEndLog);

    IDE_ASSERT( mCurLogFile->mFreeSize >= sLogFileEndSize );

    // �α������� ���̹Ƿ� File End Log�� ����Ѵ�.
    writeFileEndLog();

    sSwitchedLogFileNo = mCurLogFile->mFileNo;
    sSwitchedLogOffset = mCurLogFile->mOffset;

    IDE_TEST( mLogFileMgr.switchLogFile( &mCurLogFile ) != IDE_SUCCESS );

    // �� �α����Ϸ� switch�� �߻��߱� ������
    // �α׷��ڵ尡 ��ϵ� ��ġ�� �α������� offset�� 0�̾�� ��.
    IDE_ASSERT( mCurLogFile->mOffset == 0 );
    IDE_DASSERT( sSwitchedLogFileNo + 1 == mCurLogFile->mFileNo );

    /* BUG-32137 [sm-disk-recovery] The setDirty operation in DRDB causes
     * contention of LOG_ALLOCATION_MUTEX. */
    // �α����� Swtich�� �߻��Ͽ����Ƿ�, ���� �αװ� ��ϵ� LSN�� ����
    setLstLSN( mCurLogFile->mFileNo,
               mCurLogFile->mOffset );

    // ���� �αװ� �ϳ��� ��ϵ��� ���� ����.
    // ������ ù��° �α׷��ڵ�� File Begin Log�� ����Ѵ�.
    writeFileBeginLog();

    sState = 0;
    IDE_TEST( unlock() != IDE_SUCCESS );

    // �ش� �α����ϱ��� sync��Ű�� archive list�� �߰��Ѵ�.
    IDE_TEST( mLFThread.syncOrWait4SyncLogToLSN(
                                      SMR_LOG_SYNC_BY_SYS,
                                      sSwitchedLogFileNo,
                                      sSwitchedLogOffset,
                                      NULL )   // aSyncedLFCnt
              != IDE_SUCCESS );

    // �ش� �α������� archive �ɶ����� ��ٸ���.
    IDE_TEST( mArchiveThread.wait4EndArchLF( sSwitchedLogFileNo )
              != IDE_SUCCESS );

    // lock/unlock���̿��� ������ Raise�� �� �����Ƿ�, stage ó�� ���Ѵ�.
    IDE_TEST( lockLogSwitchCount() != IDE_SUCCESS );

    // �α����� switch�� �����Ͽ����Ƿ�,
    // �α����� switch counter�� �������ش�.
    mLogSwitchCount++;

    IDE_TEST( unlockLogSwitchCount() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sState != 0 )
    {
        (void)unlock();
    }
    else
    {
        /* nothing to do */
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : ��� LogFile�� �����ؼ� aMinLSN���� �۰ų� ���� LSN�� ������ �α׸�
 *               ù��°�� ������ LogFile No�� ���ؼ� aNeedFirstFileNo�� �־��ش�.
 *
 * aMinLSN          - [IN]  Minimum Log Sequence Number
 * aFirstFileNo     - [IN]  check�� Logfile �� ù��° File No��
 * aEndFileNo       - [IN]  check�� Logfile �� ������ File No��
 * aNeedFirstFileNo - [OUT] aMinLSN������ ū ���� ���� ù��° �α� File No
 ***********************************************************************/
IDE_RC smrLogMgr::getFirstNeedLFN( smLSN          aMinLSN,
                                   const UInt     aFirstFileNo,
                                   const UInt     aEndFileNo,
                                   UInt         * aNeedFirstFileNo )
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

/*
 * ������ LSN���� Sync�Ѵ�.
 */
IDE_RC smrLogMgr::syncToLstLSN( smrSyncByWho   aWhoSyncLog )
{
    smLSN sSyncLstLSN ;
    SM_LSN_INIT( sSyncLstLSN );
    getLstLSN( &sSyncLstLSN );

    IDE_TEST( syncLFThread( aWhoSyncLog, & sSyncLstLSN )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/*
 * File Begin Log�� �����Ѵ�.
 *
 * aFileBeginLog [IN] - �ʱ�ȭ�� File Begin Log�� �ּ�
 */
void smrLogMgr::initializeFileBeginLog( smrFileBeginLog * aFileBeginLog )
{
    IDE_DASSERT( aFileBeginLog != NULL );

    smrLogHeadI::setType( &aFileBeginLog->mHead, SMR_LT_FILE_BEGIN );
    smrLogHeadI::setTransID( &aFileBeginLog->mHead, SM_NULL_TID );
    smrLogHeadI::setSize( &aFileBeginLog->mHead, SMR_LOGREC_SIZE(smrFileBeginLog) );
    smrLogHeadI::setFlag( &aFileBeginLog->mHead, SMR_LOG_TYPE_NORMAL );
    aFileBeginLog->mFileNo = ID_UINT_MAX;
    smrLogHeadI::setPrevLSN( &aFileBeginLog->mHead,
                             ID_UINT_MAX,  // FILENO
                             ID_UINT_MAX ); // OFFSET
    aFileBeginLog->mTail = SMR_LT_FILE_BEGIN;
    smrLogHeadI::setReplStmtDepth( &aFileBeginLog->mHead,
                                   SMI_STATEMENT_DEPTH_NULL );
}

/*
 * File End Log�� �����Ѵ�.
 *
 * aFileEndLog [IN] - �ʱ�ȭ�� File End Log�� �ּ�
 */
void smrLogMgr::initializeFileEndLog( smrFileEndLog * aFileEndLog )
{
    IDE_DASSERT( aFileEndLog != NULL );

    smrLogHeadI::setType( &aFileEndLog->mHead, SMR_LT_FILE_END );
    smrLogHeadI::setTransID( &aFileEndLog->mHead, SM_NULL_TID );
    smrLogHeadI::setSize( &aFileEndLog->mHead, SMR_LOGREC_SIZE(smrFileEndLog) );
    smrLogHeadI::setFlag( &aFileEndLog->mHead, SMR_LOG_TYPE_NORMAL );
    smrLogHeadI::setPrevLSN( &aFileEndLog->mHead,
                             ID_UINT_MAX,  // FILENO
                             ID_UINT_MAX); // OFFSET
    aFileEndLog->mTail = SMR_LT_FILE_END;
    smrLogHeadI::setReplStmtDepth( &aFileEndLog->mHead,
                                   SMI_STATEMENT_DEPTH_NULL );
}

/* Transaction�� �α׸� ����Ŀ� ������ ���� ������ �����Ѵ�.
 * 1. Transaction�� ù��° �α׶�� Transaction�� Begin LSN������ ����
 * 2. Transaction�� ���������� ����� �α� LSN
 *
 * aTrans  - [IN] Transaction ������
 * aLstLSN - [IN] Log�� LSN
 */
void smrLogMgr::updateTransLSNInfo( idvSQL  * aStatistics,
                                    void    * aTrans,
                                    smLSN   * aLstLSN,
                                    UInt      aLogSize )
{
    idvSQL     *sSqlStat;
    idvSession *sSession;

    if ( aTrans != NULL )
    {
        sSqlStat = aStatistics;

        if (sSqlStat != NULL )
        {
            sSession = sSqlStat->mSess;

            IDV_SESS_ADD( sSession, IDV_STAT_INDEX_REDOLOG_COUNT, 1 );
            IDV_SESS_ADD( sSession, IDV_STAT_INDEX_REDOLOG_SIZE,  (ULong)aLogSize );
        }
        else
        {
            /* nothing to do */
        }

        IDE_ASSERT( aLstLSN != NULL );
        smLayerCallback::setLstUndoNxtLSN( aTrans, *aLstLSN );
    }
    else
    {
        /* nothing to do */
    }
}

/* ���� �αװ� Valid���� �����Ѵ�. �α� Header�� �ִ� Type��
 * Tail���� �������� ���� ���� ������ ������ ���ư��ð� �Ѵ�. */
void smrLogMgr::validateLogRec( SChar * aRawLog )
{
    smrLogHead    * sTmpLogHead;
    smrLogType      sLogTypeInTail;
    smrLogType      sLogTypeInHead;

    sTmpLogHead = (smrLogHead *)aRawLog;

    idlOS::memcpy( &sLogTypeInTail,
                   ((SChar *)(aRawLog + smrLogHeadI::getSize(sTmpLogHead) -
                            (UInt)ID_SIZEOF(smrLogTail))),
                   ID_SIZEOF(smrLogTail) );

    sLogTypeInHead = smrLogHeadI::getType(sTmpLogHead);

    /* invalid log �� ��� */
    if ( sLogTypeInHead != sLogTypeInTail )
    {
        ideLog::log( SM_TRC_LOG_LEVEL_DEBUG,
                     SM_TRC_MRECOVERY_LOGFILEGROUP_INVALID_LOGTYPE,
                     smrLogHeadI::getType(sTmpLogHead),
                     sLogTypeInTail,
                     mCurLogFile->mFileNo,
                     mCurLogFile->mOffset );

        IDE_ASSERT(0);
    }
    else
    {
        /* nothing to do */
    }
}

/***********************************************************************
 * Description : interval�� ���� checkpoint ������ switch count �ʱ�ȭ
 *
 ***********************************************************************/
IDE_RC smrLogMgr::clearLogSwitchCount()
{
    
    IDE_TEST( lockLogSwitchCount() != IDE_SUCCESS );

    mLogSwitchCount = mLogSwitchCount % (smuProperty::getChkptIntervalInLog());

    IDE_TEST( unlockLogSwitchCount() != IDE_SUCCESS );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description :  �α������� Switch�� ������ �Ҹ����.�α�����
 *                switch Count�� 1 ������Ű�� üũ����Ʈ�� �����ؾ�
 *                �� ���� ���θ� �����Ѵ�.
 *
 **********************************************************************/
IDE_RC smrLogMgr::onLogFileSwitched()
{
    UInt              sStage  = 0;

    IDE_TEST( lockLogSwitchCount() != IDE_SUCCESS );
    sStage = 1;

    // �α� ���� Switch�� �߻������Ƿ�, Count�� 1����
    mLogSwitchCount++;

    /* ------------------------------------------------
     * �α����� switch Ƚ���� checkpoint interval�� �����ϸ�
     * checkpoint�� �����Ѵ�.
     * ----------------------------------------------*/
    if ( smuProperty::getChkptEnabled() != 0 )
    {
        /* BUG-18740 server shutdown�� checkpoint�����Ͽ� ����������
         * �� �� �ֽ��ϴ�. smrRecoveryMgr::destroy�ÿ��� �̹� checkpoint
         * thread�� ���� �Ǿ���. ������ checkpoint thread�� ���� resumeAndnoWait
         * �� ȣ���ϸ� �ȵȴ�. */
        if ( ( smrRecoveryMgr::isRestart() == ID_FALSE ) &&
             ( smrRecoveryMgr::isFinish() == ID_FALSE ) )
        {
            if ( (mLogSwitchCount >= smuProperty::getChkptIntervalInLog() ) &&
                 (smuProperty::getChkptEnabled() != 0))
            {
                IDE_TEST( gSmrChkptThread.resumeAndNoWait(
                                             SMR_CHECKPOINT_BY_LOGFILE_SWITCH,
                                             SMR_CHKPT_TYPE_BOTH )
                          != IDE_SUCCESS );
            }
            else
            {
                /* nothing to do ... */
            }
        }
        else
        {
            /* nothing to do ... */
        }
    }
    else
    {
        /* nothing to do ... */
    }

    sStage = 0;
    IDE_TEST( unlockLogSwitchCount() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();

    switch ( sStage )
    {
        case 1 :
            (void) unlockLogSwitchCount();
    }
    
    IDE_POP();
    
    return IDE_FAILURE;
}

/*
 * Log FlushThread�� aLSNToSync�� ������ LSN���� Sync����.
 *
 * ���� �� �Լ��� �Է����� ���� LSN���� �� ���� sync�ǵ���
 * �����Ǿ� ������, �̴� DBMS�� consistency�� ��ġ�� �ʴ´�.
 *
 * aLSNToSync    - [IN] sync��ų LSN
 */
IDE_RC smrLogMgr::syncLFThread( smrSyncByWho   aWhoSync,
                                smLSN        * aLSNToSync )
{
    // ���� smrLFThread�� Ư�� LSN���� Log�� Flush�ϴ� interface��
    // �������� ������, Ư�� Log File�� ������ Log�� Flush�ϴ�
    // �������̽��� �����Ѵ�.
    //
    // ���� �� �Լ��� �Է����� ���� LSN���� �� ���� sync�ǵ���
    // �����Ǿ� ������, �̴� DBMS�� consistency�� ��ġ�� �ʴ´�.
    //
    // ������ ���ؼ��� �ش� �α������� �ش� offset����, ��, Ư�� LSN����
    // sync�� �ϵ��� ������ �ʿ䰡 �ִ�.

    /* BUG-17702:[MCM] Sync Thread�� ���� ��� log�� Flush�Ҷ�����
     * Checkpint Thread�� ��ٸ��� ��찡 �߻���.
     *
     * ���� Sync�� �ʿ��� LSN������ Sync�Ǹ� ������ checkpoint�۾���
     * �����Ͽ��� �մϴ�.
     */
    IDE_TEST( mLFThread.syncOrWait4SyncLogToLSN(
                                          aWhoSync,
                                          aLSNToSync->mFileNo,
                                          aLSNToSync->mOffset,
                                          NULL)
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

/***********************************************************************
 * Description : sync�� ������ ó�� LSN (file begin�� LSN) �� ��ȯ�Ѵ�. 
 * aLSN - [OUT] sync�� file begin�� LSN
 **********************************************************************/
IDE_RC smrLogMgr::getSyncedMinFirstLogLSN( smLSN *aLSN )
{
    smLSN           sSyncedLSN;
    smLSN           sLSN;
    smrLogHead      sLogHead;
    smLSN           sMinLSN;
    smLSN           sTmpLSN;
    smrLogFile    * sLogFilePtr = NULL;
    SChar         * sLogPtr     = NULL;
    UInt            sLogSizeAtDisk;
    idBool          sIsValid    = ID_FALSE;
    idBool          sIsFileOpen = ID_FALSE;

    IDE_TEST( mLFThread.getSyncedLSN (&sSyncedLSN ) != IDE_SUCCESS );
    
    SM_LSN_MAX( sMinLSN );

    sLSN.mFileNo = sSyncedLSN.mFileNo;
    sLSN.mOffset = 0;

    sIsFileOpen = ID_TRUE;
    IDE_TEST( readLogInternal( NULL,
                               &sLSN,
                               ID_TRUE,
                               &sLogFilePtr,
                               &sLogHead,
                               &sLogPtr,
                               &sLogSizeAtDisk )
              != IDE_SUCCESS );

    /*
     * fix BUG-21725
     */
    sIsValid = smrLogFile::isValidLog( &sLSN,
                                       &sLogHead,
                                       sLogPtr,
                                       sLogSizeAtDisk );

    sIsFileOpen = ID_FALSE;
    IDE_TEST( closeLogFile( sLogFilePtr ) != IDE_SUCCESS );
    sLogFilePtr = NULL;


    /* getSyncedLSN�� ���� ��ȯ�Ǵ� �� ��, ���� �ƹ��͵� sync���� ����
     * ������ offset 0���� ��ȯ�� �� �ִ�.
     * �׷��Ƿ� �� ������ ù ��° �α׸� �о��� �� invalid�� ���°� �� �� �ִ�.
     */
    if ( sIsValid == ID_TRUE )
    {
        sTmpLSN = smrLogHeadI::getLSN( &sLogHead );
        if ( SM_IS_LSN_MAX( sMinLSN ) )
        {
            SM_GET_LSN( sMinLSN, sTmpLSN );
        }
        else
        {
            /* �̷��� �ֳ� ? 
             * ��¼ư ����׿����� Ȯ������ */
            IDE_DASSERT_MSG( SM_IS_LSN_MAX( sMinLSN ),
                             "Invalid Log LSN \n"
                             "LogLSN : %"ID_UINT32_FMT", %"ID_UINT32_FMT"\n"
                             "TmpLSN : %"ID_UINT32_FMT", %"ID_UINT32_FMT"\n"
                             "MinLSN : %"ID_UINT32_FMT", %"ID_UINT32_FMT"\n",
                             sLSN.mFileNo, 
                             sLSN.mOffset,
                             sTmpLSN.mFileNo,
                             sTmpLSN.mOffset,
                             sMinLSN.mFileNo,
                             sMinLSN.mOffset );
                             
            if ( smrCompareLSN::isLT( &sTmpLSN, &sMinLSN ) )
            {
                SM_GET_LSN( sMinLSN, sTmpLSN );
            }
            else
            {
                /* nothing to do ... */
            }
        }
    }
    else
    {
        /* ���� file begin �αװ� ��ϵ��� �ʾҴ�.
         * Sync�� file�� begin�α��� LSN�� �� �� ���ٸ�
         * ��ũ�� sync�Ǿ��ٴ� ���� ������ �� �ִ� �ּ� LSN�� �� �� �����Ƿ�
         * SM_SN_NULL�� ��ȯ�� �ش�.*/
        SM_LSN_MAX( sMinLSN );
    }

    SM_GET_LSN( *aLSN, sMinLSN );
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    if ( ( sIsFileOpen != ID_FALSE ) && ( sLogFilePtr != NULL ) )
    {
        IDE_PUSH();
        (void)closeLogFile( sLogFilePtr );
        IDE_POP();
    }
    else
    {
        /* nothing to do ... */
    }
    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Ư�� LSN�� log record�� �ش� log record�� ���� �α�
 *               ������ �����Ѵ�.
 *
 * aDecompBufferHandle  - [IN] ���� ���� ������ �ڵ�
 * aLSN            - [IN] log record�� �о�� LSN.
 *                   LSN���� Log File Group�� ID�� �����Ƿ�, �̸� ����
 *                   �������� Log File Group�� �ϳ��� �����ϰ�,
 *                   �� �ӿ� ��ϵ� log record�� �о�´�.
 * aIsCloseLogFile - [IN] aLSN�� *aLogFile�� ����Ű�� LogFile�� ���ٸ�
 *                   aIsCloseLogFile�� TRUE�� ��� *aLogFile�� Close�ϰ�,
 *                   ���ο� LogFile�� ����� �Ѵ�.
 * aLogFile - [IN-OUT] �α� ���ڵ尡 ���� �α����� ������
 * aLogHead - [OUT] �α� ���ڵ��� Head
 * aLogPtr  - [OUT] �α� ���ڵ尡 ��ϵ� �α� ���� ������
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
IDE_RC smrLogMgr::readLogInternal( iduMemoryHandle  * aDecompBufferHandle,
                                   smLSN            * aLSN,
                                   idBool             aIsCloseLogFile,
                                   smrLogFile      ** aLogFile,
                                   smrLogHead       * aLogHead,
                                   SChar           ** aLogPtr,
                                   UInt             * aLogSizeAtDisk )
{
    // ����� �α׸� �д� ��� aDecompBufferHandle�� NULL�� ���´�
    IDE_DASSERT( aLSN     != NULL );
    IDE_DASSERT( aLogFile != NULL );
    IDE_DASSERT( aLogHead != NULL );
    IDE_DASSERT( aLogPtr  != NULL );
    IDE_DASSERT( aLogSizeAtDisk != NULL );
    
    smrLogFile    * sLogFilePtr;

    sLogFilePtr = *aLogFile;
   
    if ( sLogFilePtr != NULL )
    {
        if ( (aLSN->mFileNo != sLogFilePtr->getFileNo()) )
        {
            if ( aIsCloseLogFile == ID_TRUE )
            {
                IDE_TEST( closeLogFile(sLogFilePtr) != IDE_SUCCESS );
                *aLogFile = NULL; 
            }
            else
            {
                /* nothing to do ... */
            }

            /* FIT/ART/rp/Bugs/BUG-17185/BUG-17185.tc */
            IDU_FIT_POINT( "1.BUG-17185@smrLFGMgr::readLog" );

            IDE_TEST( openLogFile( aLSN->mFileNo,
                                   ID_FALSE,
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
                               ID_FALSE,
                               &sLogFilePtr )
                  != IDE_SUCCESS );
        *aLogFile = sLogFilePtr;
    }

    IDE_TEST( smrLogComp::readLog( aDecompBufferHandle,
                                   sLogFilePtr,
                                   aLSN->mOffset,
                                   aLogHead,
                                   aLogPtr,
                                   aLogSizeAtDisk )
              != IDE_SUCCESS );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

/***********************************************************************
 * Description : aLSN�� ����Ű�� �α������� ù��° Log �� Head�� �д´�
 *
 * aDecompBufferHandle  - [IN] ���� ���� ������ �ڵ�
 * aLSN      - [IN] Ư�� �α����ϻ��� ù��° �α��� LSN
 * aLogHead  - [OUT] �о���� Log�� Head�� �Ѱ��� Parameter
 **********************************************************************/
IDE_RC smrLogMgr::readFirstLogHead( smLSN      *aLSN,
                                    smrLogHead *aLogHead )
{
    IDE_DASSERT( aLSN != NULL );
    IDE_DASSERT( aLogHead != NULL );
    
    smrLogFile *sLogFilePtr = NULL;
    idBool      sIsOpen     = ID_FALSE;
    SChar      *sLogPtr     = NULL;
    SInt        sState      = 0;

    // �α����ϻ��� ù��° �α��̹Ƿ� Offset�� 0�̾�� �Ѵ�.
    IDE_ASSERT( aLSN->mOffset == 0 );
    
    /* aLSN�� �ش��ϴ� Log�� ���� LogFile�� open�Ǿ� ������ Reference
       Count������ ������Ű�� ������ �׳� �����Ѵ�. */
    IDE_TEST( mLogFileMgr.checkLogFileOpenAndIncRefCnt( aLSN->mFileNo,
                                                        &sIsOpen,
                                                        &sLogFilePtr )
              != IDE_SUCCESS );

    if ( sIsOpen == ID_FALSE )
    {
        /* open�Ǿ����� �ʱ⶧���� */
        IDE_TEST( readFirstLogHeadFromDisk( aLSN->mFileNo,
                                            aLogHead )
                 != IDE_SUCCESS );
    }
    else
    {
        sState = 1;
        
        IDE_ASSERT(sLogFilePtr != NULL);
        
        sLogFilePtr->read( 0, &sLogPtr );
        
        /* �α������� ù��° �α״� �������� �ʴ´�.
           ���� ������� �ʰ� �ٷ� �д´�.
           
           ���� :
              File�� ù��° Log�� LSN�� �д� �۾���,
              ������ �����ϱ� ����
         */
        IDE_ASSERT( smrLogComp::isCompressedLog( sLogPtr ) == ID_FALSE );

        idlOS::memcpy( aLogHead, sLogPtr, ID_SIZEOF( *aLogHead ) );
        
        sState = 0;
        IDE_TEST( closeLogFile(sLogFilePtr) != IDE_SUCCESS );
        sLogFilePtr = NULL;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if (sState != 0)
    {
        IDE_PUSH();
        (void)closeLogFile(sLogFilePtr);
        IDE_POP();
    }
    else
    {
        /* nothing to do ... */
    }
    
    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Dummy Log�� ����Ѵ�.
 *
 ***********************************************************************/
IDE_RC smrLogMgr::writeDummyLog()
{
    smrDummyLog       sDummyLog;

    smrLogHeadI::setType( &sDummyLog.mHead, SMR_LT_DUMMY );
    smrLogHeadI::setTransID( &sDummyLog.mHead, SM_NULL_TID );
    smrLogHeadI::setSize( &sDummyLog.mHead, SMR_LOGREC_SIZE(smrDummyLog) );
    smrLogHeadI::setFlag( &sDummyLog.mHead, SMR_LOG_TYPE_NORMAL );
    smrLogHeadI::setPrevLSN( &sDummyLog.mHead,
                             ID_UINT_MAX,  // FILENO
                             ID_UINT_MAX ); // OFFSET
    sDummyLog.mTail = SMR_LT_DUMMY;
    smrLogHeadI::setReplStmtDepth( &sDummyLog.mHead,
                                   SMI_STATEMENT_DEPTH_NULL );

    // �ش� Transaction�� Log File Group�� �α׸� ����Ѵ�.
    IDE_TEST( writeLog( NULL,  // idvSQL* 
                        NULL,  // Transaction Ptr
                        (SChar*)&sDummyLog, 
                        NULL,  // Previous LSN Ptr
                        NULL,  // Log LSN Ptr
                        NULL,  // End LSN Ptr
                        SM_NULL_OID )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**********************************************************************
 * Description : PROJ-2118 BUG Reporting
 *               Server Fatal ������ Signal Handler �� ȣ����
 *               Debugging ���� ����Լ�
 *
 *               �̹� altibase_dump.log �� lock�� ��� �����Ƿ�
 *               lock�� �����ʴ� trace ��� �Լ��� ����ؾ� �Ѵ�.
 *
 **********************************************************************/
void smrLogMgr::writeDebugInfo()
{
    smLSN sDiskLstLSN;
    SM_LSN_INIT( sDiskLstLSN );

    if ( isAvailable() == ID_TRUE )
    {
        getLstLSN( &sDiskLstLSN );

        ideLog::logMessage( IDE_DUMP_0,
                            "====================================================\n"
                            " Storage Manager - Last Disk Log LSN\n"
                            "====================================================\n"
                            "LstRedoLSN        : [ %"ID_UINT32_FMT" , %"ID_UINT32_FMT" ]\n"
                            "====================================================\n",
                            sDiskLstLSN.mFileNo,
                            sDiskLstLSN.mOffset );
    }
    else
    {
        /* nothing to do ... */
    }
}

/* BUG-35392
 * ������ LstLSN(Offset) ���� sync�� �Ϸ�Ǳ⸦ ����Ѵ�. */
void smrLogMgr::waitLogSyncToLSN( smLSN  * aLSNToSync,
                                  UInt     aSyncWaitMin,
                                  UInt     aSyncWaitMax )
{
    UInt            sTimeOutMSec    = aSyncWaitMin;
    smLSN           sMinUncompletedLstLSN;
    PDL_Time_Value  sTimeOut;

    if ( smuProperty::isFastUnlockLogAllocMutex() == ID_TRUE )
    {
        /* Sync ������(Dummy Log�� �������� �ʴ�) �ִ��� LSN��(Offset) ã�´�. */
        getUncompletedLstLSN( &sMinUncompletedLstLSN );

        while ( smrCompareLSN::isGT( aLSNToSync, &sMinUncompletedLstLSN ) == ID_TRUE )
        {
            sTimeOut.set( 0, sTimeOutMSec );
            idlOS::sleep( sTimeOut );

            sTimeOutMSec <<= 1;

            if ( sTimeOutMSec > aSyncWaitMax )
            {
                sTimeOutMSec = aSyncWaitMax;
            }
            else
            {
                /* nothing to do ... */
            }

            getUncompletedLstLSN( &sMinUncompletedLstLSN );
        } // end of while
    }
    else
    {
        /* do nothing */
    }
}

/* BUG-35392 
 *   logfile0 �� 20���� �αװ� ��ϵǾ��ٸ� 
 *   +---------------------------------------------
 *   + [FileNo, offset] | .....   [0,10] | [0,20] |        
 *   +----------------------------------------------
 *                                      (A)      (B) 
 *   (A) : mLstWriteLSN    (B) : mLstLSN
 */

void smrLogMgr::rebuildMinUCSN()
{
    UInt                    i;
    UInt                    j;
    UInt                    sIdx;
    UInt                    sStartIdx;
    UInt                    sEndtIdx;
    smrUncompletedLogInfo   sMinUncompletedLstLSN;
    smrUncompletedLogInfo   sFstChkLSN;

    IDE_DASSERT( smuProperty::isFastUnlockLogAllocMutex() == ID_TRUE );

    /* ������ �α� ���ڵ��� offset (B) �� �����´�. */
    sMinUncompletedLstLSN.mLstLSN.mSync = mLstLSN.mSync; 

    if ( sMinUncompletedLstLSN.mLstLSN.mSync == 0 )
    {
        SM_SYNC_LSN_MAX( sMinUncompletedLstLSN.mLstLSN.mSync );
    }
    else
    {
        /* nothing to do */
    }

    /* ������ �α� ���ڵ尡 ��ϵ� LSN (A) �� �����´�.*/
    sMinUncompletedLstLSN.mLstWriteLSN.mSync = mLstWriteLSN.mSync;

    if ( sMinUncompletedLstLSN.mLstWriteLSN.mSync == 0 )
    {
        SM_SYNC_LSN_MAX( sMinUncompletedLstLSN.mLstWriteLSN.mSync );
    }
    else
    {
        /* nothing to do */
    }

    /* 1. Last LSN�� ���� ������ ������
     *    loop�� Ž���ϴ� ���߿� ��ĥ �� �ִ�.
     * 2. Last LSN�� log copy ���� LSN�ϼ��� �ִ�.
     *   ���� ���Ͽ� ���Ѵ�. */

    sIdx = ( mFstChkLSNArrSize / SMR_CHECK_LSN_UPDATE_GROUP ) + 1;
    for ( i = 0 ; i < sIdx ; i++ )
    {
        if ( mFstChkLSNUpdateCnt[i] != 0 ) 
        {
            sStartIdx = i * SMR_CHECK_LSN_UPDATE_GROUP;
            sEndtIdx  = IDL_MIN( (i + 1) * SMR_CHECK_LSN_UPDATE_GROUP, mFstChkLSNArrSize ); 

            for ( j = sStartIdx ; j < sEndtIdx ; j++ )
            {
                //mLstLSN ���ϱ� 
                sFstChkLSN.mLstLSN.mSync = mFstChkLSNArr[j].mLstLSN.mSync;

                if ( sFstChkLSN.mLstLSN.mLSN.mFileNo < sMinUncompletedLstLSN.mLstLSN.mLSN.mFileNo )
                {
                    sMinUncompletedLstLSN.mLstLSN.mSync = sFstChkLSN.mLstLSN.mSync;
                }
                else
                {
                    if ( ( sFstChkLSN.mLstLSN.mLSN.mFileNo == sMinUncompletedLstLSN.mLstLSN.mLSN.mFileNo ) &&
                         ( sFstChkLSN.mLstLSN.mLSN.mOffset < sMinUncompletedLstLSN.mLstLSN.mLSN.mOffset ) )
                    {
                        sMinUncompletedLstLSN.mLstLSN.mSync = sFstChkLSN.mLstLSN.mSync;
                    }
                    else
                    {
                        /* nothing to do */
                    }
                } //else


                sFstChkLSN.mLstWriteLSN.mSync = mFstChkLSNArr[j].mLstWriteLSN.mSync;
                if ( sFstChkLSN.mLstWriteLSN.mLSN.mFileNo < sMinUncompletedLstLSN.mLstWriteLSN.mLSN.mFileNo )
                {
                    sMinUncompletedLstLSN.mLstWriteLSN.mSync = sFstChkLSN.mLstWriteLSN.mSync;
                }
                else
                {
                    if ( ( sFstChkLSN.mLstWriteLSN.mLSN.mFileNo == sMinUncompletedLstLSN.mLstWriteLSN.mLSN.mFileNo ) &&
                         ( sFstChkLSN.mLstWriteLSN.mLSN.mOffset < sMinUncompletedLstLSN.mLstWriteLSN.mLSN.mOffset ) )
                    {
                        sMinUncompletedLstLSN.mLstWriteLSN.mSync = sFstChkLSN.mLstWriteLSN.mSync;
                    }
                    else
                    {
                        /* nothing to do */
                    }
                } //else

            } //for
        } //if
        else
        {
            /* nothing to do */
        }
    } //for

    /* ������ �α� ���ڵ��� offset (B) �� �����Ѵ� */
    mUncompletedLSN.mLstLSN.mSync = sMinUncompletedLstLSN.mLstLSN.mSync;

    /* ������ �α� ���ڵ尡 ��ϵ� LSN (A) �� �����Ѵ�.*/
    mUncompletedLSN.mLstWriteLSN.mSync = sMinUncompletedLstLSN.mLstWriteLSN.mSync;
}
