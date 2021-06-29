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
 * $Id: smrLogAnchorMgr.cpp 90259 2021-03-19 01:22:22Z emlee $
 *
 * Description :
 *
 * �� ������ loganchor �������� ���������̴�.
 *
 * # Algorithm
 *
 * 1. ���� loganchor�� ����
 *
 *  - createdb���������� �����ϸ�, hidden ������Ƽ��
 *    ���ǵ� ��ŭ �α׾�Ŀ ������ log DIR�� ����
 *
 * 2. loganchor ���� �� �÷��� ó��
 *
 *  - ������ �ִ� loganchor �ڷῡ ���� ����
 *    mLogAnchor�� �̿��Ͽ� mBuffer�� ����� �α׾�Ŀ
 *    ������ �����Ǹ� tablespace ������ �״�� ���
 *
 *  - tablespace ���� DDL�� �߻��� ���
 *    mBuffer�� tablespace ������ �����
 *    �κи� reset�ϰ� mLogAnchor�� tablespace������
 *    �ٽ� ���
 *
 * 3. CheckSum ��� 100
 *
 *   ������ ���� ��� (b)������ ���� foldBinary�� �����
 *   (c)������ ���� foldBinary�� ����� ���ؼ� ����Ѵ�.
 *
 *    : (b) + (c) = CheckSum
 *
 *        __mLogAnchor(��������)
 *   _____|______________________
 *   | |       |                 |
 *   |_|__(b)__|________(c)______|
 *    |                     |
 *    |__ CheckSum(4bytes)  |__ tablespace����(��������)
 *
 *
 * 4.loganchor�� validate ����
 *
 *  �����ܰ�� ������ ����
 *
 *  - ����� checksum�� �ٽ� ����� checksum�� ��
 *  - �������� ���ļ� corrupt���� ���� loganchor ���ϵ� �߿�
 *    ����� LSN�� ���� ũ��, UpdateAndFlush Ƚ���� ���� ū
 *    loganchor ���� ����
 *
 **********************************************************************/

#include <idl.h>
#include <idu.h>
#include <ide.h>
#include <idp.h>
#include <smDef.h>
#include <smm.h>
#include <smErrorCode.h>
#include <sdd.h>
#include <smu.h>
#include <smrDef.h>
#include <smrLogAnchorMgr.h>
#include <smrCompareLSN.h>
#include <smrReq.h>
#include <sct.h>
#include <sdcTXSegMgr.h>
#include <smiMain.h>
#include <svm.h>
#include <sdsFile.h>
#include <sdsBufferArea.h>
#include <sdsMeta.h>
#include <sdsBufferMgr.h>


smrLogAnchorMgr::smrLogAnchorMgr()
{

}

smrLogAnchorMgr::~smrLogAnchorMgr()
{

}

/***********************************************************************
 * Description: ���� ���� loganchor ���� ����
 *
 * createdb���������� �����Ѵ�.
 * SM _LOGANCHOR_FILE_COUNT : hidden property (3�� ����)
 * ���ǵǾ� �ִ� ������ŭ loganchor ������ ����
 *
 * + 2nd. code design
 *   - if durability level 0�� �ƴ϶�� then
 *        - SMU_LOGANCHOR_FILE_COUNT ������ŭ�� loganchor ������ ���� (only create)
 *          �� loganchor ���ϵ��� open �Ѵ�.
 *        - mBuffer�� SM_PAGE_SIZE ũ��� �ʱ�ȭ��
 *        - dummy loganchor�� �ʱ�ȭ�Ѵ�.
 *        - mBuffer�� SM_PAGE_SIZE ũ��� �ʱ�ȭ��
 *        - loganchor buffer�� write offset �ʱ�ȭ ���Ŀ� dummy loganchor�� ���
 *        - loganchor buffer�� ��� loganchor ���Ͽ� flush�Ѵ�.
 *        - loganchor ���ϵ��� close �ϰ� �����Ѵ�.
 *        - mBuffer�� �����Ѵ�.
 *   - endif
 **********************************************************************/
IDE_RC smrLogAnchorMgr::create()
{
    UInt          sWhich;
    UInt          sState = 0;
    smrLogAnchor  sLogAnchor;
    SChar         sFileName[SM_MAX_FILE_NAME];
    UInt          i;

    /* BUG-16214: SM���� startup process, create db, startup service,
     * shutdown immediate�Ҷ� �߻��ϴ� UMR���ֱ� */
    idlOS::memset( &sLogAnchor, 0x00, ID_SIZEOF(smrLogAnchor));

    /* INITIALIZE PROPERTIES */

    IDE_TEST(checkLogAnchorDirExist() != IDE_SUCCESS );

    /* ------------------------------------------------
     * �������� loganchor ������ ���� (only create)
     * ----------------------------------------------*/
    for ( sWhich = 0 ; sWhich < SM_LOGANCHOR_FILE_COUNT ; sWhich++ )
    {
        idlOS::memset( sFileName, 0x00, SM_MAX_FILE_NAME );
        /* ���ϸ��� loganchor1 .. loganchorN ���·� ���� */
        idlOS::snprintf( sFileName, SM_MAX_FILE_NAME,
                         "%s%c%s%"ID_UINT32_FMT"",
                         smuProperty::getLogAnchorDir(sWhich),
                         IDL_FILE_SEPARATOR,
                         SMR_LOGANCHOR_NAME,
                         sWhich );

        IDE_TEST_RAISE(idf::access(smuProperty::getLogAnchorDir(sWhich),
                                     W_OK) !=0, err_no_write_perm_path);

        IDE_TEST_RAISE(idf::access(smuProperty::getLogAnchorDir(sWhich),
                                     X_OK) !=0, err_no_execute_perm_path);

        IDE_TEST_RAISE(idf::access(sFileName,
                                     F_OK) ==0, err_already_exist_file);

        IDE_TEST( mFile[sWhich].initialize( IDU_MEM_SM_SMR,
                                            1, /* Max Open FD Count */
                                            IDU_FIO_STAT_OFF,
                                            IDV_WAIT_INDEX_NULL )
                  != IDE_SUCCESS );

        IDE_TEST( mFile[sWhich].setFileName(sFileName)
                  != IDE_SUCCESS );

        IDE_TEST( mFile[sWhich].createUntilSuccess( smLayerCallback::setEmergency )
                  != IDE_SUCCESS );

        //====================================================================
        // To Fix BUG-13924
        //====================================================================

        ideLog::log( SM_TRC_LOG_LEVEL_MRECOV,
                     SM_TRC_DISK_FILE_CREATE,
                     sFileName );
    }

    SM_LSN_INIT( sLogAnchor.mBeginChkptLSN );
    SM_LSN_INIT( sLogAnchor.mEndChkptLSN );

    SM_LSN_INIT( sLogAnchor.mMemEndLSN );

    sLogAnchor.mLstCreatedLogFileNo = 0;
    sLogAnchor.mFstDeleteFileNo     = 0;
    sLogAnchor.mLstDeleteFileNo     = 0;
    SM_LSN_MAX(sLogAnchor.mResetLSN);

    SM_LSN_INIT( sLogAnchor.mMediaRecoveryLSN );
    SM_LSN_INIT( sLogAnchor.mDiskRedoLSN );
    sLogAnchor.mCheckSum            = 0;
    sLogAnchor.mServerStatus        = SMR_SERVER_SHUTDOWN;
    sLogAnchor.mArchiveMode         = SMI_LOG_NOARCHIVE;

    IDE_TEST( sdcTXSegMgr::adjustEntryCount( smuProperty::getTXSEGEntryCnt(),
                                             &(sLogAnchor.mTXSEGEntryCnt) ) 
              != IDE_SUCCESS );

    sLogAnchor.mNewTableSpaceID     = SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC;
    sLogAnchor.mSmVersionID         = smVersionID;

    sLogAnchor.mUpdateAndFlushCount = 0; // ������ 0���� �ʱ�ȭ

    for( i = 0 ; i < SMR_LOGANCHOR_RESERV_SIZE ; i++ )
    {
        sLogAnchor.mReserveArea[i] = 0;
    }

    /* proj-1608 recovery from replication
     * BUG-31488, �ʱ�ȭ�� ���� �ʾ� NULL�� �������� �ʰ�, 0���� ������
     * RP������ mReplRecoveryLSN�� �ʱ�ȭ �� �׻� SM_SN_NULL�� �����ϰ� ����..
     */
    SM_LSN_MAX( sLogAnchor.mReplRecoveryLSN );

    /* PROJ-2133 incremental backup */
    idlOS::memset( &sLogAnchor.mCTFileAttr, 0x00, ID_SIZEOF( smriCTFileAttr ));
    idlOS::memset( &sLogAnchor.mBIFileAttr, 0x00, ID_SIZEOF( smriBIFileAttr ));

    sLogAnchor.mCTFileAttr.mCTMgrState = SMRI_CT_MGR_DISABLED;
    sLogAnchor.mBIFileAttr.mBIMgrState = SMRI_BI_MGR_FILE_REMOVED;
    
    sLogAnchor.mBIFileAttr.mDeleteArchLogFileNo = ID_UINT_MAX;

    /* ------------------------------------------------
     * mBuffer�� SM_PAGE_SIZE ũ��� �ʱ�ȭ��
     * ----------------------------------------------*/
    IDE_TEST( allocBuffer( SM_PAGE_SIZE ) != IDE_SUCCESS );
    sState = 1;

    initBuffer();

    IDE_TEST( writeToBuffer( (UChar*)&sLogAnchor,
                             (UInt)ID_SIZEOF(smrLogAnchor) )
              != IDE_SUCCESS );

    /* ------------------------------------------------
     * ��� loganchor ���Ͽ� flush
     * ----------------------------------------------*/
    IDE_TEST(flushAll() != IDE_SUCCESS );

    for ( sWhich = 0 ; sWhich < SM_LOGANCHOR_FILE_COUNT ; sWhich++ )
    {
        IDE_TEST( mFile[sWhich].destroy() != IDE_SUCCESS );
    }

    sState = 0;
    IDE_TEST( freeBuffer() != IDE_SUCCESS );


    return IDE_SUCCESS;

    IDE_EXCEPTION( err_already_exist_file );
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_AlreadyExistLogAnchorFile ) );
    }
    IDE_EXCEPTION( err_no_write_perm_path );
    {
        IDE_SET( ideSetErrorCode(smERR_ABORT_NoWritePermFile,
                                 smuProperty::getLogAnchorDir(sWhich)));
    }
    IDE_EXCEPTION( err_no_execute_perm_path );
    {
        IDE_SET( ideSetErrorCode(smERR_ABORT_NoExecutePermFile,
                                 smuProperty::getLogAnchorDir(sWhich)));
    }
    IDE_EXCEPTION_END;

    if (sState != 0)
    {
        (void)freeBuffer();
    }

    return IDE_FAILURE;

}

/***********************************************************************
 * Description : loganchor online backup
 *
 * loganchor backup�ÿ��� backup ���丮�� �ѹ��� loganchor ������ �����ϸ�,
 * restore�ÿ� ���� loganchor ���丮�� �����Ѵ�.
 **********************************************************************/
IDE_RC smrLogAnchorMgr::backup( UInt   aWhich,
                                SChar* aBackupFilePath )
{
    UInt sState = 0;

    IDE_DASSERT( aBackupFilePath != NULL );

    IDE_TEST(mFile[aWhich].open() != IDE_SUCCESS );
    sState = 1;
    
    // source�� destination�� �����ϸ� �ȵȴ�.
    IDE_TEST_RAISE( idlOS::strcmp( aBackupFilePath,
                                   mFile[aWhich].getFileName() )
                    == 0, error_self_copy );

    IDE_TEST( mFile[aWhich].copy( NULL, /* idvSQL* */
                                  aBackupFilePath,
                                  ID_FALSE )
             != IDE_SUCCESS );

   sState = 0;
   IDE_ASSERT(mFile[aWhich].close() == IDE_SUCCESS );
   
   return IDE_SUCCESS;

   IDE_EXCEPTION(error_self_copy);
   {
       IDE_SET( ideSetErrorCode(smERR_ABORT_SelfCopy));
   }
   IDE_EXCEPTION_END;

   if ( sState == 1 )
   {
       IDE_ASSERT( mFile[aWhich].close() == IDE_SUCCESS );
   }

   return IDE_FAILURE;
}

/***********************************************************************
 * Description : loganchor online backup
 * PRJ-1149, log anchor���ϵ��� backup �Ѵ�.
 **********************************************************************/
IDE_RC smrLogAnchorMgr::backup( idvSQL * aStatistics,
                                SChar  * aBackupPath )
{
    UInt   sState = 0;
    UInt   sWhich;
    UInt   sNameLen;
    UInt   sPathLen;
    SChar  sBackupFilePath[SM_MAX_FILE_NAME];

    IDE_TEST_RAISE( lock() != IDE_SUCCESS, error_mutex_lock);

    ideLog::log(SM_TRC_LOG_LEVEL_MRECOV, SM_TRC_MRECOVERY_LOGANC_BACKUP_START);

    sState =1;

    for ( sWhich = 0 ; sWhich < SM_LOGANCHOR_FILE_COUNT ; sWhich++ )
    {
        idlOS::memset( sBackupFilePath, 0x00, SM_MAX_FILE_NAME );

        /* ���ϸ��� loganchor1 .. loganchorN ���·� ���� */
        sPathLen = idlOS::strlen( aBackupPath );

        // BUG-11206�� ���� source�� destination�� ���Ƽ�
        // ���� ����Ÿ ������ ���ǵǴ� ��츦 �������Ͽ�
        // ������ ���� ��Ȯ�� path�� ������.
        if ( aBackupPath[sPathLen -1] == IDL_FILE_SEPARATOR )
        {
            idlOS::snprintf( sBackupFilePath, SM_MAX_FILE_NAME,
                             "%s%s%"ID_UINT32_FMT"",
                             aBackupPath,
                             SMR_LOGANCHOR_NAME,
                             sWhich );
        }
        else
        {
            idlOS::snprintf( sBackupFilePath, SM_MAX_FILE_NAME,
                             "%s%c%s%"ID_UINT32_FMT"",
                             aBackupPath,
                             IDL_FILE_SEPARATOR,
                             SMR_LOGANCHOR_NAME,
                             sWhich );
        }

        sNameLen = idlOS::strlen( sBackupFilePath );

        IDE_TEST( sctTableSpaceMgr::makeValidABSPath( ID_TRUE,
                                                      sBackupFilePath,
                                                      &sNameLen,
                                                      SMI_TBS_DISK )
                  != IDE_SUCCESS );

        IDE_TEST( backup( sWhich,sBackupFilePath ) != IDE_SUCCESS );

        ideLog::log( SM_TRC_LOG_LEVEL_MRECOV,
                     SM_TRC_MRECOVERY_LOGANC_BACKUP_TO,
                     mFile[sWhich].getFileName(),
                     sBackupFilePath );

        IDE_TEST_RAISE( iduCheckSessionEvent(aStatistics) != IDE_SUCCESS,
                        error_backup_abort );

    }

    sState =0;
    IDE_TEST_RAISE( unlock() != IDE_SUCCESS, error_mutex_unlock);


    ideLog::log(SM_TRC_LOG_LEVEL_MRECOV,
                SM_TRC_MRECOVERY_LOGANC_BACKUP_END);

    return IDE_SUCCESS;

    IDE_EXCEPTION(error_mutex_lock);
    {
        IDE_SET(ideSetErrorCode(smERR_FATAL_ThrMutexLock));
    }
    IDE_EXCEPTION(error_mutex_unlock);
    {
        IDE_SET(ideSetErrorCode(smERR_FATAL_ThrMutexUnlock));
    }
    IDE_EXCEPTION(error_backup_abort);
    {
        ideLog::log(SM_TRC_LOG_LEVEL_ABORT,
                    SM_TRC_MRECOVERY_LOGANC_BACKUP_ABORT);
    }
    IDE_EXCEPTION_END;

    IDE_PUSH();
    {
        ideLog::log(SM_TRC_LOG_LEVEL_WARNNING,
                    SM_TRC_MRECOVERY_LOGANC_BACKUP_FAIL);

        if ( sState != 0 )
        {
            IDE_ASSERT( unlock() == IDE_SUCCESS );
        }
    }
    IDE_POP();

    return IDE_FAILURE;
}

/*
   PRJ-1548 User Memory Tablespace

   ��� : Loganchor ������ �ʱ�ȭ�ϱ� ���� ��ȿ�� loganchor ���� ����

   Loganchor ������ ������� �޸𸮹��ۿ� �ε��Ͽ� checksum �˻� ��
   Latest Update Loganchor�� �����Ѵ�.

   CheckSum ������ ������ Loganchor�� Lasted Update Loganchor �����Ҷ�
   ������� �ʴ´�. ��� CheckSum ������ �߻��ߴٸ� Exception�� ��ȯ�ϰ�,
   ���������� �����ϰ� �Ѵ�.

   # Lasted Update Loganchor ����
   1. LSN�� ���� ū ��(�ֽ�)�� ����
   2. ��� ���ٸ� UpdateAndFlushCount�� ����Ͽ� ����

*/
IDE_RC smrLogAnchorMgr::checkAndGetValidAnchorNo(UInt*  aWhich)
{
    UInt          sWhich;
    UInt          sAllocSize;
    ULong         sFileSize;
    idBool        sFound;
    UChar*        sBuffer;
    SChar         sAnchorFileName[SM_MAX_FILE_NAME];
    iduFile       sFile[SM_LOGANCHOR_FILE_COUNT];
    smrLogAnchor* sLogAnchor;
    smLSN         sMaxLSN;
    smLSN         sLogAnchorLSN;
    ULong         sUpdateAndFlushCount;

    IDE_DASSERT( aWhich != NULL );

    sBuffer    = NULL;
    sLogAnchor = NULL;
    sFileSize  = 0;
    sAllocSize = 0;
    sFound     = ID_FALSE;
    sUpdateAndFlushCount = 0;
    SM_LSN_INIT( sMaxLSN );

    // �α׾�Ŀ���� writeoffset �ʱ�ȭ ���Ŀ� ��Ͻ���

    for ( sWhich = 0 ; sWhich < SM_LOGANCHOR_FILE_COUNT ; sWhich++ )
    {
        // ���ϸ��� loganchor1 .. loganchorN ���·� ����
        idlOS::snprintf( sAnchorFileName, SM_MAX_FILE_NAME,
                         "%s%c%s%"ID_UINT32_FMT"",
                         smuProperty::getLogAnchorDir(sWhich),
                         IDL_FILE_SEPARATOR,
                         SMR_LOGANCHOR_NAME,
                         sWhich );

        IDE_TEST( sFile[sWhich].initialize(IDU_MEM_SM_SMR,
                                           1, /* Max Open FD Count */
                                           IDU_FIO_STAT_OFF,
                                           IDV_WAIT_INDEX_NULL )
                  != IDE_SUCCESS );
        IDE_TEST( sFile[sWhich].setFileName( sAnchorFileName )
                  != IDE_SUCCESS );

        if ( sFile[sWhich].open() != IDE_SUCCESS )
        {
            // ������ ���µ��� ������ �����ϰ�, ���� ���� ó��
            continue;
        }

        if ( sFile[sWhich].getFileSize( &sFileSize ) != IDE_SUCCESS )
        {
            // ����ũ�⸦ ������ ������ �����ϰ�, ���� ���� ó��
            continue;
        }

        //fix BUG-27232 [CodeSonar] Null Pointer Dereference
        if ( sFileSize == 0 )
        {
            continue;
        }
        
        if ( sFileSize > sAllocSize )
        {
            // �ӽù��ۺ��� �ε��� ����ũ�Ⱑ ū ��츸 �ӽù��۸�
            // ���Ҵ� �Ѵ�.

            if ( sBuffer != NULL )
            {
                IDE_DASSERT( sAllocSize != 0);

                IDE_TEST( iduMemMgr::free(sBuffer) != IDE_SUCCESS );

                sBuffer = NULL;
            }

            sAllocSize = sFileSize;

            /* TC/FIT/Limit/sm/smr/smrLogAnchorMgr_checkAndGetValidAnchorNo_malloc.tc */
            IDU_FIT_POINT_RAISE( "smrLogAnchorMgr::checkAndGetValidAnchorNo::malloc",
                                  insufficient_memory );

            IDE_TEST_RAISE( iduMemMgr::malloc( IDU_MEM_SM_SMR,
                                               sAllocSize,
                                               (void**)&sBuffer) != IDE_SUCCESS,
                            insufficient_memory );
        }
        
        // �ӽù��ۿ� �ε�
        IDE_TEST( sFile[sWhich].read( NULL, /* idvSQL* */
                                      0,
                                      sBuffer,
                                      sFileSize )
                  != IDE_SUCCESS );

        sLogAnchor = (smrLogAnchor*)sBuffer;

        // Checksum ���� �˻�
        if (sLogAnchor->mCheckSum == makeCheckSum(sBuffer, sFileSize))
        {
            sFound  = ID_TRUE;
            // Loganchor�� EndLSN�� 
            // checkpoint���� Memory RedoLSN Ȥ�� Shutdown���� EndLSN��. 
            SM_GET_LSN( sLogAnchorLSN, sLogAnchor->mMemEndLSN );

            if (  smrCompareLSN::isLT( &sMaxLSN, &sLogAnchorLSN ) )
            {
                // ū LSN �����ϰ�, �ش� Loganchor�� �����Ѵ�.
                SM_GET_LSN( sMaxLSN, sLogAnchorLSN );

                /* BUG-31485  [sm-disk-recovery]when the server starts,  
                 *            a wrong loganchor file is selected.  
                 */ 
                sUpdateAndFlushCount = sLogAnchor->mUpdateAndFlushCount; 

                *aWhich = sWhich;
            }
            else
            {
                // LSN�� ������ ���� UpdateAndFlush Ƚ���� ���Ѵ�.
                // STARTUP CONTROL �ܰ迡�� DDL�� ���� Loganchor�� ����ɼ�
                // �ִµ� �̶�, LSN�� ��� ������ �� �ֱ� ������ Count��
                // �ֽ� Loganchor�� �����Ѵ�.
                if ( smrCompareLSN::isEQ( &sMaxLSN, &sLogAnchorLSN ) )
                {
                    if ( sUpdateAndFlushCount < sLogAnchor->mUpdateAndFlushCount )
                    {
                        sUpdateAndFlushCount = sLogAnchor->mUpdateAndFlushCount;

                        *aWhich = sWhich;
                    }
                    else
                    {
                        // continue
                    }
                }
                else
                {

                }
                // continue..
            }
        }
        else
        {
            // CheckSum ���� ����..
        }
    }

    for ( sWhich = 0 ; sWhich < SM_LOGANCHOR_FILE_COUNT ; sWhich++ )
    {
        IDE_TEST( sFile[sWhich].destroy() != IDE_SUCCESS );
    }

    // �ӽø޸𸮹��� ����
    if ( sBuffer != NULL )
    {
        IDE_TEST( iduMemMgr::free(sBuffer) != IDE_SUCCESS );

        sBuffer = NULL;
    }
    else
    {
        /* nothing to do */
    }

    // ��� Loganchor�� CheckSum ������ �߻��� ��쿡 Exception ó��
    IDE_TEST_RAISE( sFound != ID_TRUE, error_invalid_loganchor_file );

    return IDE_SUCCESS;

    IDE_EXCEPTION( error_invalid_loganchor_file );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_InvalidLogAnchorFile));
    }
    IDE_EXCEPTION( insufficient_memory );
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_InsufficientMemory));
    }
    IDE_EXCEPTION_END;

    IDE_PUSH();

    if ( sBuffer != NULL )
    {
        IDE_ASSERT( iduMemMgr::free( sBuffer ) == IDE_SUCCESS );
        sBuffer = NULL;
    }

    IDE_POP();

    return IDE_FAILURE;
}

/*
  PRJ-1548 User Memory Tablespace

  Loganchor�� ù��° Node Attribute Type�� �ǵ��Ѵ�.

  [IN ] aLogAnchorFile : ���µ� loganchor ���� ��ü
  [OUT] aBeginOffset   : �������� ���� ������
  [OUT] aAttrType      : ù��° Node Attribute Type
*/
IDE_RC smrLogAnchorMgr::getFstNodeAttrType( iduFile         * aLogAnchorFile,
                                            UInt            * aBeginOffset,
                                            smiNodeAttrType * aAttrType )
{
    UInt             sBeginOffset;
    smiNodeAttrType  sAttrType;

    IDE_DASSERT( aLogAnchorFile != NULL );
    IDE_DASSERT( aBeginOffset   != NULL );
    IDE_DASSERT( aAttrType      != NULL );

    sBeginOffset = ID_SIZEOF(smrLogAnchor);

    // attribute type �ǵ�
    IDE_TEST( aLogAnchorFile->read( NULL, /* idvSQL* */
                                    sBeginOffset,
                                    (SChar*)&sAttrType,
                                    ID_SIZEOF(smiNodeAttrType) )
              != IDE_SUCCESS );

    *aBeginOffset = sBeginOffset;
    *aAttrType    = sAttrType;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/*
   PRJ-1548 User Memory Tablespace

   aNextOffset�� ����� Node Attribute Ÿ���� �ǵ��Ͽ� ��ȯ�Ѵ�.

  [IN ] aLogAnchorFile : ���µ� loganchor ���� ��ü
  [IN ] aCurrOffset    : �ǵ��� ������
  [OUT] aAttrType      : Node Attribute Type
*/
IDE_RC smrLogAnchorMgr::getNxtNodeAttrType( iduFile         * aLogAnchorFile,
                                            UInt              aNextOffset,
                                            smiNodeAttrType * aNextAttrType )
{
    smiNodeAttrType  sAttrType;

    IDE_DASSERT( aLogAnchorFile != NULL );
    IDE_DASSERT( aNextAttrType  != NULL );

    // attribute type �ǵ�
    IDE_TEST( aLogAnchorFile->read( NULL, /* idvSQL* */
                                    aNextOffset,
                                    (SChar*)&sAttrType,
                                    ID_SIZEOF(smiNodeAttrType) )
              != IDE_SUCCESS );

    *aNextAttrType    = sAttrType;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 * Description : loganchor ������ �ʱ�ȭ
 *
 * + 2nd. code design
 *   - mutex �ʱ�ȭ�Ѵ�.
 *   - tablespace �Ӽ� �� datafile �Ӽ��� �����ϱ� ���� �ڷᱸ��
 *     �Ҵ��Ѵ�.
 *   - mBuffer�� SM_PAGE_SIZE ũ��� �ʱ�ȭ�Ѵ�.
 *   - if (durability level 0 �� �ƴѰ��) then
 *         - ��ȿ�� �α׾�Ŀ������ ����
 *         - ��� loganchor ������ �����Ѵ�.
 *         - loganchor ���Ϸκ��� smrLogAnchor�� �ǵ��Ͽ�
 *           loganchor ���ۿ� �����Ѵ�.
 *         - mLogAnchor�� assign�Ѵ�.
 *         - while ()
 *           {
 *               - tablespace �Ӽ� ������ tablespace ������ŭ �ǵ��Ͽ�
 *                 loganchor ���ۿ� ����Ѵ�.
 *               -  for (datafile ������ŭ)
 *                  datafile �Ӽ� ������ �ǵ��Ͽ� loganchor ���ۿ� ����Ѵ�.
 *           }
 *         - ��� loganchor ���Ͽ� ��ȿ�� loganchor ���۸� flush �Ѵ�.
 *     else
 *         - loganchor ���۸� �ʱ�ȭ�Ѵ�.
 *     endif
 **********************************************************************/
IDE_RC smrLogAnchorMgr::initialize()
{
    UInt                  i;
    UInt                  sWhich    = 0;
    UInt                  sState    = 0;
    ULong                 sFileSize = 0;
    smrLogAnchor          sLogAnchor;
    SChar                 sAnchorFileName[SM_MAX_FILE_NAME];
    UInt                  sFileState = 0;

    // LOGANCHOR_DIR Ȯ��
    IDE_TEST( checkLogAnchorDirExist() != IDE_SUCCESS );

    for ( i = 0 ; i < SM_LOGANCHOR_FILE_COUNT ; i++ )
    {
        idlOS::snprintf( sAnchorFileName, SM_MAX_FILE_NAME,
                         "%s%c%s%"ID_UINT32_FMT"",
                         smuProperty::getLogAnchorDir(i),
                         IDL_FILE_SEPARATOR,
                         SMR_LOGANCHOR_NAME,
                         i );

        // Loganchor ���� ���� ���� Ȯ��
        IDE_TEST_RAISE( idf::access(sAnchorFileName, (F_OK|W_OK|R_OK) ) != 0,
                        error_file_not_exist );
    }

    IDE_TEST_RAISE( mMutex.initialize( (SChar*)"LOGANCHOR_MGR_MUTEX",
                                       IDU_MUTEX_KIND_NATIVE,
                                       IDV_WAIT_INDEX_NULL )
                    != IDE_SUCCESS,
                    error_mutex_init );

    idlOS::memset( &mTableSpaceAttr,
                   0x00,
                   ID_SIZEOF(smiTableSpaceAttr) );

    idlOS::memset( &mDataFileAttr,
                   0x00,
                   ID_SIZEOF(smiDataFileAttr) );

    // �޸� ����(mBuffer)�� SM_PAGE_SIZE ũ��� �Ҵ� �� �ʱ�ȭ
    IDE_TEST( allocBuffer( SM_PAGE_SIZE ) != IDE_SUCCESS );
    sState = 1;

    // ��ȿ�� loganchor������ �����Ѵ�.
    IDE_TEST( checkAndGetValidAnchorNo( &sWhich ) != IDE_SUCCESS );

    // ��� loganchor ������ �����Ѵ�.
    for ( i = 0; i < SM_LOGANCHOR_FILE_COUNT; i++ )
    {
        idlOS::snprintf( sAnchorFileName, SM_MAX_FILE_NAME,
                         "%s%c%s%"ID_UINT32_FMT"",
                         smuProperty::getLogAnchorDir(i),
                         IDL_FILE_SEPARATOR,
                         SMR_LOGANCHOR_NAME,
                         i );
        IDE_TEST( mFile[i].initialize( IDU_MEM_SM_SMR,
                                       1, /* Max Open FD Count */
                                       IDU_FIO_STAT_OFF,
                                       IDV_WAIT_INDEX_NULL )
                  != IDE_SUCCESS );
        IDE_TEST( mFile[i].setFileName( sAnchorFileName ) != IDE_SUCCESS );
        IDE_TEST( mFile[i].open() != IDE_SUCCESS );
        sFileState++;
    }

    // �޸𸮹��� ������ �ʱ�ȭ
    initBuffer();

    /*
       PRJ-1548 �������� ������ �޸� ���ۿ� �ε��ϴ� �ܰ�
    */

    IDE_TEST( mFile[sWhich].getFileSize(&sFileSize) != IDE_SUCCESS );

    // CREATE DATABASE �����߿� ���������� �������� ���� ���� ������,
    // ���������� ����Ǿ� �־�� �Ѵ�.
    IDE_ASSERT ( sFileSize >= ID_SIZEOF(smrLogAnchor) );

    // ���Ϸ� ���� Loganchor ���������� �ε��Ѵ�.
    IDE_TEST( mFile[sWhich].read( NULL, /* idvSQL* */
                                  0,
                                  (SChar*)&sLogAnchor,
                                  ID_SIZEOF(smrLogAnchor) )
              != IDE_SUCCESS );

    // �޸� ���ۿ� Loganchor ���������� �ε��Ѵ�.
    IDE_TEST( writeToBuffer( (SChar*)&sLogAnchor,
                             ID_SIZEOF(smrLogAnchor) )
              != IDE_SUCCESS );

    mLogAnchor = (smrLogAnchor*)mBuffer;

    /* DISK REDO LSN�� MEMORY REDO LSN�� ���̺����̽� �����ڿ�
       �����Ѵ�. Empty ����Ÿ������ �����ؼ� �̵�� ������
       �ϴ� ��쿡 ��������� ����� �� �ֵ��� �ϱ� ���� */
    sctTableSpaceMgr::setRedoLSN4DBFileMetaHdr( &mLogAnchor->mDiskRedoLSN,
                                                &mLogAnchor->mMemEndLSN );

    // ���̺����̽� ��ü ���� ����
    sctTableSpaceMgr::setNewTableSpaceID( (scSpaceID)mLogAnchor->mNewTableSpaceID );

    // [ �޸� ���̺����̽� ] STATE�ܰ���� �ʱ�ȭ�� �ȴ�
    IDE_TEST( readAllTBSAttrs( sWhich, SMI_TBS_ATTR ) != IDE_SUCCESS );
    IDE_TEST( readAllTBSAttrs( sWhich, SMI_DBF_ATTR ) != IDE_SUCCESS );
    IDE_TEST( readAllTBSAttrs( sWhich, SMI_CHKPTPATH_ATTR ) != IDE_SUCCESS );

    // [ �޸� ���̺����̽� ] MEDIA, PAGE�ܰ���� �ʱ�ȭ�� �ȴ�
    //   Checkpoint Path Attribute�� �ε�� ���¿����� MEDIA�ܰ���
    //   �ʱ�ȭ�� �����ϴ�.
    IDE_TEST( smmTBSStartupShutdown::initFromStatePhase4AllTBS()
              != IDE_SUCCESS );

    // [ �޸� ���̺����̽� ]
    //  MEDIA�ܰ���� �ʱ�ȭ �� ���¿��� Checkpoint Image Attribute��
    // �ε��� �� �ִ�.
    //
    // <����>
    //   - MEDIA�ܰ���� �ʱ�ȭ�� �� ���¿���
    //     DB File ��ü ����� �����ϴ�
    //   - DB File��ü ����� ������ ��
    //     Checkpoint Image Attribute���� �ε��� �� �ִ�
    IDE_TEST( readAllTBSAttrs( sWhich, SMI_CHKPTIMG_ATTR ) != IDE_SUCCESS );

    //  [  Secondary Buffer �ʱ�ȭ  ]
    IDE_TEST( readAllTBSAttrs( sWhich, SMI_SBUFFER_ATTR ) != IDE_SUCCESS );

    /* ------------------------------------------------
     * 5. ��ȿ�� loganchor ���۸� ��� anchor ���Ͽ� flush �Ѵ�.
     * ----------------------------------------------*/
    IDE_TEST( readLogAnchorToBuffer( sWhich ) != IDE_SUCCESS );
    IDE_ASSERT( checkLogAnchorBuffer() == ID_TRUE );

    for ( i = 0 ; i < SM_LOGANCHOR_FILE_COUNT ; i++ )
    {
        sFileState--;
        IDE_ASSERT(mFile[i].close() == IDE_SUCCESS );
    }

    mIsProcessPhase = ID_FALSE;

    return IDE_SUCCESS;

    IDE_EXCEPTION( error_file_not_exist );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_InvalidLogAnchorFile));
    }
    IDE_EXCEPTION( error_mutex_init );
    {
        IDE_SET(ideSetErrorCode(smERR_FATAL_ThrMutexInit));
    }
    IDE_EXCEPTION_END;

    while ( sFileState > 0 )
    {
        sFileState--;
        IDE_ASSERT( mFile[sFileState].close() == IDE_SUCCESS ); 
    }
    
    if (sState != 0)
    {
        (void)freeBuffer();
    }

    return IDE_FAILURE;
}


/***********************************************************************
 * Description : loganchor ������ ����
 *
 * + 2nd. code design
 *   - ��� loganchor ������ �ݴ´�
 *   - loganchor ���۸� �޸� �����Ѵ�.
 *   - tablespace �Ӽ� �� datafile �Ӽ��� loganchor��
 *     �����ϱ� ���� �ڷᱸ���� �����Ѵ�.
 *   - mutex�� �����Ѵ�.
 ***********************************************************************/
IDE_RC smrLogAnchorMgr::destroy()
{
    UInt   i;

    for (i = 0; i < SM_LOGANCHOR_FILE_COUNT; i++)
    {
        IDE_TEST(mFile[i].destroy() != IDE_SUCCESS );
    }

    IDE_TEST( freeBuffer() != IDE_SUCCESS );

    IDE_TEST_RAISE( mMutex.destroy() != IDE_SUCCESS, error_mutex_destroy );

    return IDE_SUCCESS;

    IDE_EXCEPTION( error_mutex_destroy );
    {
        IDE_SET(ideSetErrorCode (smERR_FATAL_ThrMutexDestroy));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}


/*
    �α׾�Ŀ�κ��� Tablespace�� Ư�� Ÿ���� Attribute���� �ε��Ѵ�.

    [IN] aWhichAnchor    - ���� ���� Log Anchor File�� � ������
    [IN] aAttrTypeToLoad - �ε��� Attribute�� ����
 */
IDE_RC smrLogAnchorMgr::readAllTBSAttrs( UInt             aWhichAnchor,
                                         smiNodeAttrType  aAttrTypeToLoad )
{
    ULong             sFileSize   = 0;
    smiNodeAttrType   sAttrType   = (smiNodeAttrType)0;
    UInt              sReadOffset = 0;

    IDE_TEST( mFile[aWhichAnchor].getFileSize(&sFileSize) != IDE_SUCCESS );

    /*
       PRJ-1548 �������� ������ �޸� ���ۿ� �ε��ϴ� �ܰ�
    */
    if ( sFileSize > ID_SIZEOF(smrLogAnchor) )
    {
        // ���������� ù��° Node Attribute Type�� �ǵ��Ѵ�.
        IDE_TEST( getFstNodeAttrType( &mFile[aWhichAnchor],
                                      &sReadOffset,
                                      &sAttrType ) != IDE_SUCCESS );
        while ( 1 )
        {
            // Log Anchor�κ��� ���� Attribute��
            // ���̺����̽��� �ڷᱸ���� Load���� ���θ� ����
            if ( sAttrType == aAttrTypeToLoad )
            {
                IDE_TEST( readAttrFromLogAnchor( SMR_AAO_LOAD_ATTR,
                                                 sAttrType,
                                                 aWhichAnchor,
                                                 &sReadOffset ) != IDE_SUCCESS );
            }
            else
            {
                // �ش� Attribute�� ���� �ʰ� Skip�Ѵ�.
                sReadOffset += getAttrSize( sAttrType );
            }

            // EOF Ȯ��
            if ( sFileSize == ((ULong)sReadOffset) )
            {
                break;
            }

            IDE_ASSERT( sFileSize > ((ULong)sReadOffset) );

            // attribute type �ǵ�
            IDE_TEST( getNxtNodeAttrType( &mFile[aWhichAnchor],
                                          sReadOffset,
                                          &sAttrType ) 
                      != IDE_SUCCESS );
        } // while
    }
    else
    {
        // CREATE DATABASE ������ Loganchor�� ���������� ����� ���
        // nothing to do ...
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**
   �α׾�Ŀ ���ۿ� �α׾�Ŀ Attribute����
   Valid�� Attribute�� ����� �ٽ� ����Ѵ�.

   DROP�� Tablespace�� ���õ� Attribute�� ������� �ʰ� ������.

   [IN] aWhichAnchor    - ���� ���� Log Anchor File�� � ������
 */
IDE_RC smrLogAnchorMgr::readLogAnchorToBuffer(UInt aWhichAnchor)
{
    ULong             sFileSize = 0;
    smiNodeAttrType   sAttrType = (smiNodeAttrType)0;
    UInt              sReadOffset;

    IDE_TEST( mFile[aWhichAnchor].getFileSize(&sFileSize) != IDE_SUCCESS );

    sReadOffset  = 0;

    /*
       PRJ-1548 �������� ������ �޸� ���ۿ� �ε��ϴ� �ܰ�
    */
    if ( sFileSize > ID_SIZEOF(smrLogAnchor) )
    {
        // ���������� ù��° Node Attribute Type�� �ǵ��Ѵ�.
        IDE_TEST( getFstNodeAttrType( &mFile[aWhichAnchor],
                                      &sReadOffset,
                                      &sAttrType ) != IDE_SUCCESS );
        while ( 1 )
        {
            IDE_TEST( readAttrFromLogAnchor( SMR_AAO_REWRITE_ATTR,
                                             sAttrType,
                                             aWhichAnchor,
                                             &sReadOffset ) != IDE_SUCCESS );

            // EOF Ȯ��
            if ( sFileSize == ((ULong)sReadOffset) )
            {
                break;
            }

            IDE_ASSERT( sFileSize > ( (ULong)sReadOffset ) );

            // attribute type �ǵ�
            IDE_TEST( getNxtNodeAttrType( &mFile[aWhichAnchor],
                                          sReadOffset,
                                          &sAttrType ) 
                      != IDE_SUCCESS );
        } // while
    }
    else
    {
        // CREATE DATABASE ������ Loganchor�� ���������� ����� ���
        // nothing to do ...
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/***********************************************************************
 * Description : LogAnchor File�� ���� Attribute�Ӽ��� �о�´�.
 * aAttrOp �Ӽ��� ���� SMR_AAO_REWRITE_ATTR�̸� LogAnchor Buffer��
 * SMR_AAO_LOAD_ATTR �̸� space node�� ����Ѵ�.
 *
 *   aAttrOp      - [IN]     Attribute ��� ��� (Buffer, Node)
 *   aAttrType    - [IN]     ���� Attribute�� ����
 *   aWhichAnchor - [IN]     ���� logAnchor File
 *   aReadOffset  - [IN/OUT] ���� offset, ���� ���� offset ��ȯ
 **********************************************************************/
IDE_RC smrLogAnchorMgr::readAttrFromLogAnchor( smrAnchorAttrOption aAttrOp,
                                               smiNodeAttrType     aAttrType,
                                               UInt                aWhichAnchor,
                                               UInt              * aReadOffset )
{
    switch ( aAttrType )
    {
        case SMI_TBS_ATTR:
            // ���̺����̽� �ʱ�ȭ
            IDE_TEST( initTableSpaceAttr( aWhichAnchor,
                                          aAttrOp,
                                          aReadOffset )
                      != IDE_SUCCESS );
            break;

        case SMI_DBF_ATTR :
            // ��ũ ����Ÿ���� �ʱ�ȭ
            IDE_TEST( initDataFileAttr( aWhichAnchor,
                                        aAttrOp,
                                        aReadOffset )
                      != IDE_SUCCESS );
            break;

        case SMI_CHKPTPATH_ATTR :
            // üũ����Ʈ Path �ʱ�ȭ
            IDE_TEST( initChkptPathAttr( aWhichAnchor,
                                         aAttrOp,
                                         aReadOffset )
                      != IDE_SUCCESS );
            break;

        case SMI_CHKPTIMG_ATTR:
            IDE_TEST( initChkptImageAttr( aWhichAnchor,
                                          aAttrOp,
                                          aReadOffset )
                      != IDE_SUCCESS );
            break;

         case SMI_SBUFFER_ATTR:
             IDE_TEST( initSBufferFileAttr( aWhichAnchor,
                                            aAttrOp,
                                            aReadOffset )
                       != IDE_SUCCESS );
             break;

        default:
            ideLog::log(SM_TRC_LOG_LEVEL_MRECOV,
                        SM_TRC_INVALID_LOGANCHOR_ATTRTYPE,
                        aAttrType);

            IDE_ASSERT( 0 );
            break;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/*
  �޸�/��ũ ���̺����̽� �ʱ�ȭ

  [IN] aWhich       - ��ȿ�� �α׾�Ŀ ��ȣ
  [IN] aAttrOp      - Attribute�ʱ�ȭ ���� ������ �۾�
  [IN] aReadOffset  - �α׾�Ŀ ���ϻ󿡼� ���� �Ӽ��� ������
  [OUT] aReadOffset - �α׾�Ŀ ���ϻ󿡼� ���� �Ӽ� ���� ������

  [ PRJ-1548 User Memory TableSpace ���䵵�� ]

   * ��ũ�� �����ϰ� �̵�� ������ �����Ϸ���
     STARTUP CONTROL �ܰ迡�� Memory TableSpace Node�� �ʱ�ȭ�ؾ� �Ѵ�.

   * �̵����� ���� ��������
     [1] Checkpoint Image Hdr ������ �����ؾ��� ( Media Failure �˻� )
     [2] Empty Checkpoint Image ���� �����ؾ��� ( Media Recovery ��� )

     ��� Checkpoint Image�� �ʱ�ȭ�� �Ϸ�Ǿ�� �Ѵ�.

     �������� prepareTBS �ܰ迡�� �޸� ���̺����̽���
     smmDatabaseFile���� �Ҵ��ϰ� �ʱ�ȭ�Ͽ�����,

     �α׾�Ŀ �ʱ�ȭ�������� �� �ش� �۾��� �����ϱ�� �Ѵ�.
*/
IDE_RC smrLogAnchorMgr::initTableSpaceAttr( UInt                aWhich,
                                            smrAnchorAttrOption aAttrOp,
                                            UInt              * aReadOffset )
{
    UInt sAnchorOffset;

    IDE_DASSERT( aReadOffset != NULL );

    // ���� �Ӽ��� �������� ����Ѵ�.
    sAnchorOffset = *aReadOffset;

    // [1] �α׾�Ŀ�κ��� ���� �Ӽ��� �ǵ��Ѵ�.
    IDE_TEST( readTBSNodeAttr( &mFile[aWhich],
                               aReadOffset,
                               &mTableSpaceAttr )
              != IDE_SUCCESS );

    // PRJ-1548 User Memory Tablespace
    // [2] ������°� ������ ��� ������¸� ���ش�.
    mTableSpaceAttr.mTBSStateOnLA &= ~SMI_TBS_BACKUP;

    if ( aAttrOp == SMR_AAO_REWRITE_ATTR )
    {
        IDE_ASSERT( mWriteOffset == sAnchorOffset );
        // [3] �α׾�Ŀ �޸� ���ۿ� ����Ѵ�.
        IDE_TEST( writeToBuffer( (SChar*)&mTableSpaceAttr,
                                 ID_SIZEOF( smiTableSpaceAttr ) )
                  != IDE_SUCCESS );
    }
    else
    {
        /* nothing to do */
    }

    if ( aAttrOp == SMR_AAO_LOAD_ATTR )
    {
        // DROP�� ��츸 log anchor�κ��� Tablespace Load�� SKIP
        //
        // Discard�� Tablespace�� ��� => �ε�ǽ�
        //   ���� Drop�ø� ���� Tablespace����
        //   �ڷᱸ���� �ʱ�ȭ�Ǿ��־�� ��
        if ( ( mTableSpaceAttr.mTBSStateOnLA & SMI_TBS_DROPPED )
               != SMI_TBS_DROPPED )
        {
            // ���̺����̽��� �������� �ʾҴٸ�

            // [4] ���̺����̽��� �ʱ�ȭ�Ѵ�.
            if ( ( mTableSpaceAttr.mType == SMI_MEMORY_SYSTEM_DICTIONARY ) ||
                 ( mTableSpaceAttr.mType == SMI_MEMORY_SYSTEM_DATA )       ||
                 ( mTableSpaceAttr.mType == SMI_MEMORY_USER_DATA ) )
            {
                // memory �Ŵ����� tablespace Node�� ����
                IDE_TEST( smmTBSStartupShutdown::loadTableSpaceNode(
                                                              &mTableSpaceAttr,
                                                              sAnchorOffset )
                          != IDE_SUCCESS );
            }
            /* PROJ-1594 Volatile TBS */
            else if ( mTableSpaceAttr.mType == SMI_VOLATILE_USER_DATA )
            {
                // volatile �Ŵ����� tablespace Node�� ����
                IDE_TEST( svmTBSStartupShutdown::loadTableSpaceNode(
                                                              &mTableSpaceAttr,
                                                              sAnchorOffset )
                          != IDE_SUCCESS );
            }
            else
            {
                // disk �Ŵ�����
                // tablespace Node�� ����
                IDE_TEST( sddDiskMgr::loadTableSpaceNode( NULL, /* idvSQL* */
                                                          &mTableSpaceAttr,
                                                          sAnchorOffset )
                          != IDE_SUCCESS );
            }
        }
        else
        {
            // ������ TBS�� �ʱ�ȭ�����ʴ´�.
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/*
  ��ũ ����Ÿ���� ��Ÿ��� �ʱ�ȭ

  [IN] aWhich        - ��ȿ�� �α׾�Ŀ ��ȣ
  [IN] aAttrOp       - Attribute�ʱ�ȭ ���� ������ �۾�
  [IN] aReadOffset   - �α׾�Ŀ ���ϻ󿡼� ���� �Ӽ��� ������
  [OUT] aReadOffset  - �α׾�Ŀ ���ϻ󿡼� ���� �Ӽ� ���� ������
*/
IDE_RC smrLogAnchorMgr::initDataFileAttr( UInt                aWhich,
                                          smrAnchorAttrOption aAttrOp,
                                          UInt              * aReadOffset )
{
    UInt  sAnchorOffset;

    IDE_DASSERT( aReadOffset != NULL );

    // ���� �Ӽ��� �������� ����Ѵ�.
    sAnchorOffset = *aReadOffset;

    // [1] �α׾�Ŀ�κ��� ���� �Ӽ��� �ǵ��Ѵ�.
    IDE_TEST( readDBFNodeAttr( &mFile[aWhich],
                               aReadOffset,
                               &mDataFileAttr )  != IDE_SUCCESS );

    // [2] ������°� ������ ��� ������¸� ���ش�.
    mDataFileAttr.mState &= ~SMI_FILE_BACKUP;

    if ( aAttrOp == SMR_AAO_REWRITE_ATTR )
    {
        IDE_ASSERT( mWriteOffset == sAnchorOffset );
        // [3] �α׾�Ŀ �޸� ���ۿ� ����Ѵ�.
        IDE_TEST( writeToBuffer( (SChar*)&mDataFileAttr,
                                 ID_SIZEOF( smiDataFileAttr ) )
                  != IDE_SUCCESS );
    }
    else
    {
        /* nothing to do */
    }

    if ( aAttrOp == SMR_AAO_LOAD_ATTR )
    {
        if ( SMI_FILE_STATE_IS_NOT_DROPPED( mDataFileAttr.mState ) )
        {
            // 4.4 �ش� tablespace ��忡 datafile ��带 ����
            IDE_TEST( sddDiskMgr::loadDataFileNode( NULL, /* idvSQL* */
                                                    &mDataFileAttr,
                                                    sAnchorOffset )
                      != IDE_SUCCESS );
        }
        else
        {
            // ������ ������ �ʱ�ȭ���� �ʴ´�.
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

/*
  �޸� üũ����Ʈ PATH �ʱ�ȭ

  [IN]  aWhich       - ��ȿ�� �α׾�Ŀ ��ȣ
  [IN]  aAttrOp      - Attribute�ʱ�ȭ ���� ������ �۾�
  [IN]  aReadOffset  - �α׾�Ŀ ���ϻ󿡼� ���� �Ӽ��� ������
  [OUT] aReadOffset  - �α׾�Ŀ ���ϻ󿡼� ���� �Ӽ� ���� ������
*/
IDE_RC smrLogAnchorMgr::initChkptPathAttr( UInt                aWhich,
                                           smrAnchorAttrOption aAttrOp,
                                           UInt              * aReadOffset )
{
    UInt  sAnchorOffset;

    sctTableSpaceNode    * sSpaceNode;

    IDE_DASSERT( aReadOffset != NULL );


    // ���� �Ӽ��� �������� ����Ѵ�.
    sAnchorOffset = *aReadOffset;

    // [1] �α׾�Ŀ�κ��� ���� �Ӽ��� �ǵ��Ѵ�.
    IDE_TEST( readChkptPathNodeAttr( &mFile[aWhich],
                                     aReadOffset,
                                     &mChkptPathAttr )
              != IDE_SUCCESS );

    if ( aAttrOp == SMR_AAO_REWRITE_ATTR )
    {
        IDE_ASSERT( mWriteOffset == sAnchorOffset );

        IDE_TEST( writeToBuffer( (SChar*)&mChkptPathAttr,
                                 ID_SIZEOF( smiChkptPathAttr ) )
                  != IDE_SUCCESS );
    }
    else
    {
        /* nothing to do */
    }

    if ( aAttrOp == SMR_AAO_LOAD_ATTR )
    {
        sSpaceNode = sctTableSpaceMgr::findSpaceNodeIncludingDropped( mChkptPathAttr.mSpaceID );

        if ( sSpaceNode != NULL )
        {
            if ( SMI_TBS_IS_DROPPED(sSpaceNode->mState) )
            {
                // Drop �� Tablespace�� ���
                // Tablespace Node���� Add���� ���� �����̴�.
                //
                // Checkpoint Path add���� �ʴ´�
            }
            else
            {
                // DROP_PENDING�� ������ ��쿡�� ����� �б�ȴ�.
                // Drop Tablespace Pending���� ó������ ���� �۾� ó���� ���ؼ�
                // Checkpoint Path Node�� Tablespace�� �Ŵ޷��� �Ѵ�.

                //4.4 �ش� tablespace ��忡 Chkpt Path ��带 ����
                IDE_TEST( smmTBSStartupShutdown::createChkptPathNode(
                              (smiChkptPathAttr*)&mChkptPathAttr,
                              sAnchorOffset ) != IDE_SUCCESS );
            }
        }
        else
        {
            // DROP�� Tablespace�� ��� �ƿ� initTablespaceAttr����
            // load���� ���� �ʾұ� ������ sSpaceNode�� NULL�� ���´�.
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

/*
  �޸� ����Ÿ���� ��Ÿ��� �ʱ�ȭ

  [IN] aWhich       - ��ȿ�� �α׾�Ŀ ��ȣ
  [IN] aAttrOp      - Attribute�ʱ�ȭ ���� ������ �۾�
  [IN] aReadOffset  - �α׾�Ŀ ���ϻ󿡼� ���� �Ӽ��� ������
  [OUT] aReadOffset - �α׾�Ŀ ���ϻ󿡼� ���� �Ӽ� ���� ������
*/
IDE_RC smrLogAnchorMgr::initChkptImageAttr( UInt                aWhich,
                                            smrAnchorAttrOption aAttrOp,
                                            UInt              * aReadOffset )
{
    UInt  sAnchorOffset;
    sctTableSpaceNode    * sSpaceNode;

    IDE_DASSERT( aReadOffset != NULL );

    // ���� �Ӽ��� �������� ����Ѵ�.
    sAnchorOffset = *aReadOffset;

    IDE_TEST( readChkptImageAttr( &mFile[aWhich],
                                  aReadOffset,
                                  &mChkptImageAttr )
              != IDE_SUCCESS );

    if ( aAttrOp == SMR_AAO_REWRITE_ATTR )
    {
        IDE_ASSERT( mWriteOffset == sAnchorOffset );

        IDE_TEST( writeToBuffer( (SChar*)&mChkptImageAttr,
                                 ID_SIZEOF( smmChkptImageAttr ))
                  != IDE_SUCCESS );
    }

    if ( aAttrOp == SMR_AAO_LOAD_ATTR )
    {
        sSpaceNode = sctTableSpaceMgr::findSpaceNodeIncludingDropped( mChkptImageAttr.mSpaceID );

        if ( sSpaceNode != NULL )
        {
            if ( SMI_TBS_IS_DROPPED(sSpaceNode->mState) )
            {
                // Drop �� Tablespace�� ���
                // Tablespace Node���� Add���� ���� �����̴�.
                //
                // Checkpoint Image�� add���� �ʴ´�
            }
            else
            {
                // DROP_PENDING�� ������ ��쿡�� ����� �б�ȴ�.
                // Drop Tablespace Pending���� ó������ ���� �۾� ó���� ���ؼ�
                // Checkpoint Image Node�� ������ ����
                // Tablespace Node�� ���������� ��ü�� �����Ͽ��� �Ѵ�.

                // �α׾�Ŀ�κ��� �ǵ��� �޸� ����Ÿ���� �Ӽ���
                // �޸� ����Ÿ���� ��Ÿ������� �����Ѵ�.
                IDE_TEST( smmTBSStartupShutdown::initializeChkptImageAttr(
                                                          &mChkptImageAttr,
                                                          &mLogAnchor->mMemEndLSN,
                                                          sAnchorOffset )
                          != IDE_SUCCESS );
            }
        }
        else
        {
            // DROP�� Tablespace�� ��� �ƿ� initTablespaceAttr����
            // load���� ���� �ʾұ� ������ sSpaceNode�� NULL�� ���´�.
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
  Description :  Secondary  Buffer  ����

   [IN] aWhich        - ��ȿ�� �α׾�Ŀ ��ȣ
   [IN] aAttrOp       - Attribute�ʱ�ȭ ���� ������ �۾�
   [IN] aReadOffset   - �α׾�Ŀ ���ϻ󿡼� ���� �Ӽ��� ������
   [OUT] aReadOffset  - �α׾�Ŀ ���ϻ󿡼� ���� �Ӽ� ���� ������
 **********************************************************************/
IDE_RC smrLogAnchorMgr::initSBufferFileAttr( UInt                aWhich,
                                             smrAnchorAttrOption aAttrOp,
                                             UInt              * aReadOffset )
{
    UInt  sAnchorOffset;

    IDE_DASSERT( aReadOffset != NULL );

    // ���� �Ӽ��� �������� ����Ѵ�.
    sAnchorOffset = *aReadOffset;

    // �α׾�Ŀ�κ��� ���� �Ӽ��� �ǵ��Ѵ�.
    IDE_TEST( readSBufferFileAttr( &mFile[aWhich],
                                   aReadOffset,
                                   &mSBufferFileAttr )  
               != IDE_SUCCESS );

    if ( aAttrOp == SMR_AAO_REWRITE_ATTR )
    {
        IDE_ASSERT( mWriteOffset == sAnchorOffset );
        // �α׾�Ŀ �޸� ���ۿ� ����Ѵ�.
        IDE_TEST( writeToBuffer( (SChar*)&mSBufferFileAttr,
                                 ID_SIZEOF( smiSBufferFileAttr ) )
                 != IDE_SUCCESS );
    }
    else
    {
        /* nothing to do */
    }
    
    if ( aAttrOp == SMR_AAO_LOAD_ATTR )
    {
        IDE_TEST( sdsBufferMgr::loadFileAttr( &mSBufferFileAttr,
                                              sAnchorOffset ) 
                  != IDE_SUCCESS );    
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
 * Description : loganchor�� �������� ���� �� �α� ���� flush
 ***********************************************************************/
IDE_RC smrLogAnchorMgr::updateSVRStateAndFlush( smrServerStatus   aSvrStatus,
                                                smLSN           * aEndLSN,
                                                UInt            * aLstCrtLog )
{
    UInt   sState = 0;

    IDE_DASSERT( aEndLSN != NULL );

    IDE_TEST( lock() != IDE_SUCCESS );
    sState = 1;

    mLogAnchor->mServerStatus =  aSvrStatus;
    mLogAnchor->mMemEndLSN    = *aEndLSN;
    mLogAnchor->mLstCreatedLogFileNo = *aLstCrtLog;

    IDE_TEST( flushAll() != IDE_SUCCESS );

    sState = 0;
    IDE_TEST( unlock() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if (sState != 0)
    {
        (void)unlock();
    }

    return IDE_FAILURE;

}

/***********************************************************************
 * Description : loganchor�� �������� ���� flush
 ***********************************************************************/
IDE_RC smrLogAnchorMgr::updateSVRStateAndFlush( smrServerStatus  aSvrStatus )
{
    UInt   sState = 0;

    IDE_TEST( lock() != IDE_SUCCESS );
    sState = 1;

    mLogAnchor->mServerStatus =  aSvrStatus;

    IDE_TEST( flushAll() != IDE_SUCCESS );

    sState = 0;
    IDE_TEST( unlock() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();

    if (sState != 0)
    {
        IDE_ASSERT( unlock() == IDE_SUCCESS );
    }

    IDE_POP();

    return IDE_FAILURE;

}

/***********************************************************************
 * Description : loganchor�� ������ ����ϴ� TXSEG Entry ������
 *               �����Ѵ�.
 *
 *               ����Ǵ� ���� �������� ���� ������Ƽ���� ���� ���̴�.
 *
 ***********************************************************************/
IDE_RC smrLogAnchorMgr::updateTXSEGEntryCntAndFlush( UInt aEntryCnt )
{
    IDE_ASSERT( lock() == IDE_SUCCESS );

    mLogAnchor->mTXSEGEntryCnt = aEntryCnt;

    IDE_ASSERT( flushAll() == IDE_SUCCESS );
    IDE_ASSERT( unlock() == IDE_SUCCESS );

    return IDE_SUCCESS;
}


/***********************************************************************
 * Description : loganchor�� �������� ���� flush
 ***********************************************************************/
IDE_RC smrLogAnchorMgr::updateArchiveAndFlush( smiArchiveMode   aArchiveMode )
{
    UInt sState = 0;

    IDE_TEST( lock() != IDE_SUCCESS );
    sState = 1;

    mLogAnchor->mArchiveMode = aArchiveMode;

    IDE_TEST( flushAll() != IDE_SUCCESS );

    sState = 0;
    IDE_TEST( unlock() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if (sState != 0)
    {
        (void)unlock();
    }

    return IDE_FAILURE;

}

/*
   Loganchor�� Disk Redo LSN �� RedoLSN�� �����Ѵ� .

   [IN] aDiskRedoLSN   : ��ũ ����Ÿ���̽��� Restart Point
   [IN] aMemRedoLSN : ��ũ ����Ÿ���̽��� Restart Point
*/
IDE_RC smrLogAnchorMgr::updateRedoLSN( smLSN*  aDiskRedoLSN,
                                       smLSN*  aMemRedoLSN )
{
    UInt   sState = 0;

    IDE_TEST( lock() != IDE_SUCCESS );
    sState = 1;

    if ( aDiskRedoLSN != NULL )
    {
        mLogAnchor->mDiskRedoLSN = *aDiskRedoLSN;
    }
    else
    {
        /* nothing to do */
    }

    if ( aMemRedoLSN != NULL )
    {
        IDE_ASSERT( ( aMemRedoLSN->mFileNo != ID_UINT_MAX ) &&
                    ( aMemRedoLSN->mOffset != ID_UINT_MAX ) );

        SM_GET_LSN( mLogAnchor->mMemEndLSN,
                    *aMemRedoLSN );
    }
    else
    {
        /* nothing to do ... */
    }

    IDE_TEST( flushAll() != IDE_SUCCESS );

    sState = 0;
    IDE_TEST( unlock() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();
    {
        if ( sState != 0 )
        {
            IDE_ASSERT( unlock() == IDE_SUCCESS );
        }
    }
    IDE_POP();

    return IDE_FAILURE;
}

/**********************************************************************
 * Description : loganchor�� Disk Redo LSN ���� flush
 **********************************************************************/
IDE_RC smrLogAnchorMgr::updateResetLSN(smLSN*  aResetLSN)
{

    UInt   sState = 0;

    IDE_DASSERT ( aResetLSN != NULL );

    // Update�ϴ� ���� ���� 2���� ����̴�.
    // �� �Լ��� ȣ���ϱ����� Error üũ�� �Ѵ�.

    // (1) In-Complete Media Recovery ����Ϸ��

    // �ҿ��������̱� ������ �̵�� ������ �����ϰ�,
    // ������ Disk Redo LSN ���� �̷��� ResetLog LSN��
    // �����Ѵ�.

    // (2) Meta ResetLogs �����
    // LSN MAX�� Update�ϱ� ������, mDiskRedoLSN���� ������ ũ��.

    IDE_ASSERT( smrCompareLSN::isLT( &mLogAnchor->mDiskRedoLSN,
                                     aResetLSN )
                == ID_TRUE );

    IDE_TEST( lock() != IDE_SUCCESS );
    sState = 1;

    SM_GET_LSN( mLogAnchor->mResetLSN, *aResetLSN );

    IDE_TEST( flushAll() != IDE_SUCCESS );

    sState = 0;
    IDE_TEST( unlock() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if (sState != 0)
    {
        (void)unlock();
    }

    return IDE_FAILURE;
}

/**********************************************************************
 * Description : loganchor�� MediaRecovery�� redo�� ������ LSN ���� flush
 **********************************************************************/
IDE_RC smrLogAnchorMgr::updateMediaRecoveryLSN( smLSN * aMediaRecoveryLSN )
{

    UInt   sState = 0;

    IDE_DASSERT( aMediaRecoveryLSN != NULL );

    /* aMediaRecoveryLSN�� ���� checkpoint LSN(mDiskRedoLSN or mBeginChkptLSN )
     * ���� �۰ų� ���ƾ� �Ѵ�.
     * ��, create database�ÿ��� mDiskRedoLSN�� mBeginChkptLSN��INIT ������
     * ��� �´�. */

    if ( (SM_IS_LSN_INIT(mLogAnchor->mDiskRedoLSN) == ID_FALSE ) &&
         (SM_IS_LSN_INIT(mLogAnchor->mBeginChkptLSN) == ID_FALSE) )
    {
        IDE_ASSERT( smrCompareLSN::isLTE( aMediaRecoveryLSN,
                                          &mLogAnchor->mDiskRedoLSN )
                    == ID_TRUE );

        IDE_ASSERT( smrCompareLSN::isLTE( aMediaRecoveryLSN,
                                          &mLogAnchor->mBeginChkptLSN )
                    == ID_TRUE );
    }

    IDE_TEST( lock() != IDE_SUCCESS );
    sState = 1;

    SM_GET_LSN( mLogAnchor->mMediaRecoveryLSN, *aMediaRecoveryLSN );

    IDE_TEST( flushAll() != IDE_SUCCESS );

    sState = 0;
    IDE_TEST( unlock() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if (sState != 0)
    {
        (void)unlock();
    }

    return IDE_FAILURE;
}


/***********************************************************************
 * Description : loganchor�� checkpoint ���� flush
 ***********************************************************************/
IDE_RC smrLogAnchorMgr::updateChkptAndFlush( smLSN  * aBeginChkptLSN,
                                             smLSN  * aEndChkptLSN,
                                             smLSN  * aDiskRedoLSN,
                                             smLSN  * aEndLSN,
                                             UInt   * aFirstFileNo,
                                             UInt   * aLastFileNo )
{
    UInt sState = 0;

    IDE_TEST( lock() != IDE_SUCCESS );
    sState = 1;

    mLogAnchor->mBeginChkptLSN    = *aBeginChkptLSN;
    mLogAnchor->mDiskRedoLSN      = *aDiskRedoLSN;
    mLogAnchor->mEndChkptLSN      = *aEndChkptLSN;

    SM_GET_LSN( mLogAnchor->mMemEndLSN, *aEndLSN );

    /* BUG-39289 : ���� ������Ʈ�� LST(aLastFileNo)�� ���� �α� ��Ŀ��
       LST(mLogAnchor->mLstDeleteFileNo)�� ������ üũ����Ʈ������ �α׸�
       ������ �ʴ´ٴ� �ǹ��̴�. ���� ������Ʈ�� FST, LST���� �α׾�Ŀ��
       FST, LST���� ���ٴ� �ǹ��̹Ƿ� �� ��쿡�� �α׾�Ŀ�� ������Ʈ����
       �ʴ´�. */
    if ( mLogAnchor->mLstDeleteFileNo != *aLastFileNo )
    {
        mLogAnchor->mFstDeleteFileNo = *aFirstFileNo;
        mLogAnchor->mLstDeleteFileNo = *aLastFileNo;
    }
    else
    {
        /* do nothing */
    }

    IDE_TEST( flushAll() != IDE_SUCCESS );

    sState = 0;
    IDE_TEST( unlock() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if (sState != 0)
    {
        (void)unlock();
    }

    return IDE_FAILURE;

}

/***********************************************************************
 * Description : Recovery From Replication(proj-1608)�� ���� LSN �������� �� flush
 ***********************************************************************/
IDE_RC smrLogAnchorMgr::updateReplRecoveryLSN( smLSN aReplRecoveryLSN )
{

    UInt   sState = 0;

    IDE_TEST( lock() != IDE_SUCCESS );
    sState = 1;

    SM_GET_LSN( mLogAnchor->mReplRecoveryLSN, aReplRecoveryLSN );

    IDE_TEST( flushAll() != IDE_SUCCESS );

    sState = 0;
    IDE_TEST( unlock() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if (sState != 0)
    {
        IDE_ASSERT(unlock() == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

/*
   PRJ-1548 User Memory Tablespace
   ����� �ϳ��� TBS Node�� Loganchor�� �ݿ��Ѵ�.

   sctTableSpaceMgr::lockSpaceNode()�� ȹ���� ���¿��� ȣ��Ǿ�� �Ѵ�.
*/

IDE_RC smrLogAnchorMgr::updateTBSNodeAndFlush( sctTableSpaceNode  * aSpaceNode )
{
    UInt      sState = 0;
    UInt      sAnchorOffset;

    IDE_DASSERT( aSpaceNode != NULL );

    IDE_TEST( lock() != IDE_SUCCESS );
    sState = 1;

    getTBSAttrAndAnchorOffset( aSpaceNode->mID,
                               &mTableSpaceAttr,
                               &sAnchorOffset );

    /* write to loganchor buffer */
    IDE_TEST( updateToBuffer( (SChar*)&mTableSpaceAttr,
                              sAnchorOffset,
                              ID_SIZEOF(smiTableSpaceAttr) ) 
              != IDE_SUCCESS );
    IDE_TEST( flushAll() != IDE_SUCCESS );

    sState = 0;
    IDE_TEST( unlock() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();

    if ( sState != 0 )
    {
        IDE_ASSERT( unlock() == IDE_SUCCESS );
    }

    IDE_POP();

    return IDE_FAILURE;
}

/*
   PRJ-1548 User Memory Tablespace
   ����� �ϳ��� DRDB DBF Node�� Loganchor�� �ݿ��Ѵ�.

   SpaceNode Mutex()�� ȹ���� ���¿��� ȣ��Ǿ�� �Ѵ�.
*/

IDE_RC smrLogAnchorMgr::updateDBFNodeAndFlush( sddDataFileNode  * aFileNode )
{
    UInt      sState = 0;

    IDE_DASSERT( aFileNode != NULL );

    IDE_TEST( lock() != IDE_SUCCESS );
    sState = 1;

    /* ------------------------------------------------
     * 1. smrLogAnchor ���� ����
     * ----------------------------------------------*/
    sddDataFile::getDataFileAttr( aFileNode,
                                  &mDataFileAttr );

    IDE_TEST( updateToBuffer( (SChar *)&mDataFileAttr,
                              aFileNode->mAnchorOffset,
                              ID_SIZEOF(smiDataFileAttr) )
            != IDE_SUCCESS );

    IDE_TEST( flushAll() != IDE_SUCCESS );

    sState = 0;
    IDE_TEST( unlock() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();

    if ( sState != 0 )
    {
        IDE_ASSERT( unlock() == IDE_SUCCESS );
    }

    IDE_POP();

    return IDE_FAILURE;
}

/*
   PRJ-1548 User Memory Tablespace
   ����� �ϳ��� Checkpint Path Node�� Loganchor�� �ݿ��Ѵ�.

   SpaceNode Mutex()�� ȹ���� ���¿��� ȣ��Ǿ�� �Ѵ�.

   [IN] aChkptPathNode - ������ üũ����Ʈ ��� ���
*/

IDE_RC smrLogAnchorMgr::updateChkptPathAttrAndFlush(
                                       smmChkptPathNode * aChkptPathNode )

{
    UInt      sState = 0;

    IDE_DASSERT( aChkptPathNode != NULL );

    IDE_TEST( lock() != IDE_SUCCESS );
    sState = 1;

    /* ------------------------------------------------
     * 1. smrLogAnchor ���� ����
     * ----------------------------------------------*/
    IDE_TEST( updateToBuffer( (SChar*)& aChkptPathNode->mChkptPathAttr,
                              aChkptPathNode->mAnchorOffset,
                              ID_SIZEOF(aChkptPathNode->mChkptPathAttr) ) 
              != IDE_SUCCESS );

    IDE_TEST( flushAll() != IDE_SUCCESS );

    sState = 0;
    IDE_TEST( unlock() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();

    if ( sState != 0 )
    {
        IDE_ASSERT( unlock() == IDE_SUCCESS );
    }

    IDE_POP();

    return IDE_FAILURE;
}

/*
   PRJ-1548 User Memory Tablespace
   ����� �ϳ��� Checkpoint Path Node�� Loganchor�� �ݿ��Ѵ�.

   spaceNode Mutex�� ȹ���� ���¿��� ȣ��Ǿ�� �Ѵ�.
*/

IDE_RC smrLogAnchorMgr::updateChkptImageAttrAndFlush(
                              smmCrtDBFileInfo   * aCrtDBFileInfo,
                              smmChkptImageAttr  * aChkptImageAttr )
{
    UInt      sState = 0;

    IDE_DASSERT( aCrtDBFileInfo  != NULL );
    IDE_DASSERT( aChkptImageAttr != NULL );

    IDE_TEST( lock() != IDE_SUCCESS );
    sState = 1;

    /* ------------------------------------------------
     * 1. smrLogAnchor ���� ����
     * ----------------------------------------------*/
    IDE_TEST( updateToBuffer( (SChar*)aChkptImageAttr,
                              aCrtDBFileInfo->mAnchorOffset,
                              ID_SIZEOF(smmChkptImageAttr) ) 
              != IDE_SUCCESS );

    IDE_TEST( flushAll() != IDE_SUCCESS );

    sState = 0;
    IDE_TEST( unlock() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();

    if ( sState != 0 )
    {
        IDE_ASSERT( unlock() == IDE_SUCCESS );
    }

    IDE_POP();

    return IDE_FAILURE;
}


/*
   PROJ-2102 Fast Secondary Buffer
   ����� Secondary Buffer Node�� Loganchor�� �ݿ��Ѵ�.
*/

IDE_RC smrLogAnchorMgr::updateSBufferNodeAndFlush( sdsFileNode   * aFileNode )
{
    UInt      sState = 0;

    IDE_DASSERT( aFileNode  != NULL );

    IDE_TEST( lock() != IDE_SUCCESS );
    sState = 1;

    /* ------------------------------------------------
     * 1. smrLogAnchor ���� ����
     * ----------------------------------------------*/
    sdsBufferMgr::getFileAttr( aFileNode,
                               &mSBufferFileAttr );

    IDE_TEST( updateToBuffer( (SChar*)&mSBufferFileAttr,
                              aFileNode->mAnchorOffset,
                              ID_SIZEOF(smiSBufferFileAttr) ) 
              != IDE_SUCCESS );

    IDE_TEST( flushAll() != IDE_SUCCESS );

    sState = 0;
    IDE_TEST( unlock() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();

    if ( sState != 0 )
    {
        IDE_ASSERT( unlock() == IDE_SUCCESS );
    }

    IDE_POP();

    return IDE_FAILURE;
}

/******************************************************************************
 * PRJ-1548 User Memory Tablespace
 * loganchor�� TBS Node ������ �߰��ϴ� �Լ�
 ******************************************************************************/
IDE_RC smrLogAnchorMgr::addTBSNodeAndFlush( sctTableSpaceNode * aSpaceNode,
                                            UInt              * aAnchorOffset )
{
    UInt    sState  = 0;

    IDE_DASSERT( aSpaceNode     != NULL );
    IDE_DASSERT( aAnchorOffset  != NULL );

    IDE_TEST( lock() != IDE_SUCCESS );
    sState = 1;

    // New TableSpace ID ����
    mLogAnchor->mNewTableSpaceID = sctTableSpaceMgr::getNewTableSpaceID();

    switch( sctTableSpaceMgr::getTBSLocation( aSpaceNode ) )
    {
        case SMI_TBS_DISK:
            sddTableSpace::getTableSpaceAttr( (sddTableSpaceNode *)aSpaceNode,
                                              &mTableSpaceAttr );
            break;
        case SMI_TBS_MEMORY:
            smmManager::getTableSpaceAttr( (smmTBSNode*)aSpaceNode,
                                           &mTableSpaceAttr );
            break;
        case SMI_TBS_VOLATILE:
            svmManager::getTableSpaceAttr( (svmTBSNode*)aSpaceNode,
                                           &mTableSpaceAttr );
            break;
        default:
            IDE_ASSERT(0);
            break;
    }

    // ����ϱ� ���� �޸� ���� �������� ��ȯ
    *aAnchorOffset = mWriteOffset;

    /* write to loganchor buffer */
    IDE_TEST( writeToBuffer( (SChar *)&mTableSpaceAttr,
                             ID_SIZEOF(smiTableSpaceAttr) )
              != IDE_SUCCESS );

    IDE_TEST( flushAll() != IDE_SUCCESS );

    sState = 0;
    IDE_TEST( unlock() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();

    if ( sState != 0 )
    {
        IDE_ASSERT( unlock() == IDE_SUCCESS );
    }

    IDE_POP();

    return IDE_FAILURE;
}

/* PRJ-1548 User Memory Tablespace
 * loganchor�� DBF Node ������ �߰��ϴ� �Լ� */
IDE_RC smrLogAnchorMgr::addDBFNodeAndFlush( sddTableSpaceNode * aSpaceNode,
                                            sddDataFileNode   * aFileNode,
                                            UInt              * aAnchorOffset )
{
    UInt      sState = 0;

    IDE_DASSERT( aSpaceNode    != NULL );
    IDE_DASSERT( aFileNode     != NULL );
    IDE_DASSERT( aAnchorOffset != NULL );

    IDE_TEST( lock() != IDE_SUCCESS );
    sState = 1;

    // TBS Attr�� NewFileID ����
    sddTableSpace::getTableSpaceAttr( (sddTableSpaceNode *)aSpaceNode,
                                      &mTableSpaceAttr );

    IDE_TEST( updateToBuffer( (SChar*)&mTableSpaceAttr,
                              aSpaceNode->mAnchorOffset,
                              ID_SIZEOF(smiTableSpaceAttr) )
              != IDE_SUCCESS );

    // ���� ������ DBF Attr ���� ȹ��
    sddDataFile::getDataFileAttr( (sddDataFileNode *)aFileNode,
                                  &mDataFileAttr );


    // ����ϱ� ���� �޸� ���� �������� ��ȯ
    *aAnchorOffset = mWriteOffset;

    /* write to loganchor buffer */
    IDE_TEST( writeToBuffer( (SChar *)&mDataFileAttr,
                             ID_SIZEOF(smiDataFileAttr) )
              != IDE_SUCCESS );

    IDE_TEST( flushAll() != IDE_SUCCESS );

    sState = 0;
    IDE_TEST( unlock() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();

    if ( sState != 0 )
    {
        IDE_ASSERT( unlock() == IDE_SUCCESS );
    }

    IDE_POP();

    return IDE_FAILURE;
}

/*
   PRJ-1548 User Memory Tablespace
    loganchor�� ChkptPath Node ������ �߰��ϴ� �Լ�
*/
IDE_RC smrLogAnchorMgr::addChkptPathNodeAndFlush( smmChkptPathNode  * aChkptPathNode,
                                                  UInt              * aAnchorOffset )
{
    UInt      sState = 0;

    IDE_DASSERT( aChkptPathNode != NULL );
    IDE_DASSERT( aAnchorOffset != NULL );

    IDE_TEST( lock() != IDE_SUCCESS );
    sState = 1;

    // ����ϱ� ���� �޸� ���� �������� ��ȯ
    *aAnchorOffset = mWriteOffset;

    /* write to loganchor buffer */
    IDE_TEST( writeToBuffer( (SChar*)&(aChkptPathNode->mChkptPathAttr),
                             ID_SIZEOF(smiChkptPathAttr) )
              != IDE_SUCCESS );

    IDE_TEST( flushAll() != IDE_SUCCESS );

    sState = 0;
    IDE_TEST( unlock() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();

    if ( sState != 0 )
    {
        IDE_ASSERT( unlock() == IDE_SUCCESS );
    }

    IDE_POP();

    return IDE_FAILURE;
}

/*
    PRJ-1548 User Memory Tablespace
    �޸� üũ����Ʈ �̹��������� �α׾�Ŀ�� üũ����Ʈ�̹��� �Ӽ���
    �����Ѵ�.

    [IN[  aChkptImageAttr : üũ����Ʈ�̹����� �Ӽ�
*/
IDE_RC smrLogAnchorMgr::addChkptImageAttrAndFlush(
                                            smmChkptImageAttr * aChkptImageAttr,
                                            UInt              * aAnchorOffset )
{
    UInt      sState = 0;

    IDE_DASSERT( aChkptImageAttr != NULL );
    IDE_DASSERT( aAnchorOffset   != NULL );

    IDE_ASSERT( smmManager::isCreateDBFileAtLeastOne(
                                           aChkptImageAttr->mCreateDBFileOnDisk )
                 == ID_TRUE );

    IDE_TEST( lock() != IDE_SUCCESS );
    sState = 1;

    // ����ϱ� ���� �޸� ���� �������� ��ȯ
    *aAnchorOffset = mWriteOffset;

    /* write to loganchor buffer */
    IDE_TEST( writeToBuffer( (SChar*)aChkptImageAttr,
                             ID_SIZEOF( smmChkptImageAttr ) )
              != IDE_SUCCESS );

    IDE_TEST( flushAll() != IDE_SUCCESS );

    sState = 0;
    IDE_TEST( unlock() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();
    {
        if ( sState != 0 )
        {
            IDE_ASSERT( unlock() == IDE_SUCCESS );
        }
    }
    IDE_POP();

    return IDE_FAILURE;
}

/* Secondary Buffrer Node ������ LogAnchor�� �߰��Ѵ�. */
IDE_RC smrLogAnchorMgr::addSBufferNodeAndFlush( sdsFileNode  * aFileNode,
                                                UInt         * aAnchorOffset )
{
    UInt      sState = 0;

    IDE_DASSERT( aAnchorOffset != NULL );

    IDE_TEST( lock() != IDE_SUCCESS );
    sState = 1;

    // ���� ������ DBF Attr ���� ȹ��
    sdsBufferMgr::getFileAttr( aFileNode,
                               &mSBufferFileAttr );

    // ����ϱ� ���� �޸� ���� �������� ��ȯ
    *aAnchorOffset = mWriteOffset;

     /* write to loganchor buffer */
    IDE_TEST( writeToBuffer( (SChar*)&mSBufferFileAttr,
                             ID_SIZEOF(smiSBufferFileAttr) )
              != IDE_SUCCESS );

    IDE_TEST( flushAll() != IDE_SUCCESS );

    sState = 0;
    IDE_TEST( unlock() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();

     if ( sState != 0 )
     {
         IDE_ASSERT( unlock() == IDE_SUCCESS );
     }

    IDE_POP();

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : checkpoint�� memory tablespace�� Stable No�� �����Ѵ�.
 * checkpoint�ÿ��� ȣ��ȴ�.
 ***********************************************************************/
IDE_RC smrLogAnchorMgr::updateStableNoOfAllMemTBSAndFlush()
{
    UInt               sState = 0;
    sctTableSpaceNode* sCurrSpaceNode;
    sctTableSpaceNode* sNextSpaceNode;

    IDE_TEST( lock() != IDE_SUCCESS );
    sState = 1;

    /* ------------------------------------------------
     * 1. tablespace������ mBuffer�� ���
     * ��ũ�������� ��� tablespace ��� ����Ʈ�� ��ȸ�ϸ鼭
     * ��������� �� ����Ѵ�.
     * ----------------------------------------------*/
    sCurrSpaceNode = sctTableSpaceMgr::getFirstSpaceNode();

    while ( sCurrSpaceNode != NULL )
    {
        sNextSpaceNode = sctTableSpaceMgr::getNextSpaceNode( sCurrSpaceNode->mID );

        switch( sctTableSpaceMgr::getTBSLocation( sCurrSpaceNode ) )
        {
            case SMI_TBS_DISK:
            case SMI_TBS_VOLATILE:
                {
                    sCurrSpaceNode = sNextSpaceNode;
                    continue;
                }
                break;
            case SMI_TBS_MEMORY:
                break;
            default:
                IDE_ERROR(0);
                break;
        }

        IDE_TEST( sctTableSpaceMgr::latchSyncMutex( sCurrSpaceNode )
                  != IDE_SUCCESS );
        sState = 2;

        // drop/discarded/executing_drop_pending/offline��
        // tablespace�� skip �Ѵ�.
        if ( sctTableSpaceMgr::hasState( sCurrSpaceNode,
                                         SCT_SS_SKIP_CHECKPOINT )
             == ID_TRUE )
        {
            sState = 1;
            IDE_TEST( sctTableSpaceMgr::unlatchSyncMutex( sCurrSpaceNode )
                      != IDE_SUCCESS );

            sCurrSpaceNode = sNextSpaceNode;
            continue;
        }
        else
        {
            /* nothing to do */
        }

        // memory tablespace�� ���ؼ��� stable no�� �����Ѵ�.
        IDE_ASSERT( sctTableSpaceMgr::isMemTableSpace( sCurrSpaceNode->mID )
                    == ID_TRUE );

        // memory tablespace node�� currentdb�� switch ��Ų��.
        smmManager::switchCurrentDB((smmTBSNode*)sCurrSpaceNode );

        /* read from memory manager */
        smmManager::getTableSpaceAttr( (smmTBSNode*)sCurrSpaceNode,
                                       &mTableSpaceAttr );

        /* update to loganchor buffer */
        // [����] Current DB�� �����Ѵ�.
        // �ֳ��ϸ� CheckPoint�� TBS ��忡 Lock�� ȹ������ �ʱ� ������
        // �ٸ� TBS DDL�� ���� ���� ������, ������ �ʾҴ� ���������
        // �ݿ��� �� �ֱ� �����̴�.
        IDE_TEST( updateToBuffer(
                          (SChar*)&(mTableSpaceAttr.mMemAttr.mCurrentDB),
                          ((smmTBSNode*)sCurrSpaceNode)->mAnchorOffset +
                          offsetof(smiTableSpaceAttr, mMemAttr) +
                          offsetof(smiMemTableSpaceAttr, mCurrentDB),
                          ID_SIZEOF(mTableSpaceAttr.mMemAttr.mCurrentDB) )
                  != IDE_SUCCESS );

        sState = 1;
        IDE_TEST( sctTableSpaceMgr::unlatchSyncMutex( sCurrSpaceNode )
                  != IDE_SUCCESS );

        sCurrSpaceNode = sNextSpaceNode;
    }

    IDE_TEST( flushAll() != IDE_SUCCESS );

    sState = 0;
    IDE_TEST( unlock() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();
    {
        switch ( sState )
        {
            case 2:
            IDE_ASSERT( sctTableSpaceMgr::unlatchSyncMutex( sCurrSpaceNode )
                        == IDE_SUCCESS );
            case 1:
            IDE_ASSERT( unlock() == IDE_SUCCESS );
        }
    }
    IDE_POP();

    return IDE_FAILURE;
}

UInt smrLogAnchorMgr::getTBSAttrStartOffset()
{
    return ID_SIZEOF(smrLogAnchor);
}

/***********************************************************************
 * Description : loganchor�� tablespace ���� Sorting�Ͽ� �ٽ� Flush
 * Startup/Shutdown �ÿ��� ȣ��ȴ�.
 ***********************************************************************/
IDE_RC smrLogAnchorMgr::updateAllTBSAndFlush()
{
    UInt               sState = 0;
    UInt               sLoop;
    UInt               sWhichDB;
    smuList*           sChkptPathList;
    smuList*           sChkptPathBaseList;
    sctTableSpaceNode* sCurrSpaceNode;
    sctTableSpaceNode* sNextSpaceNode;
    sddDataFileNode*   sFileNode;
    smmChkptPathNode * sChkptPathNode;
    smmChkptImageHdr   sChkptImageHdr;
    smmChkptImageAttr  sChkptImageAttr;
    smmDatabaseFile  * sDatabaseFile;
    smmTBSNode       * sMemTBSNode;
    UInt               i;

    IDE_TEST( lock() != IDE_SUCCESS );
    sState = 1;

    /* ------------------------------------------------
     * 1. smrLogAnchor ���� ����
     * ----------------------------------------------*/
    mLogAnchor->mNewTableSpaceID = sctTableSpaceMgr::getNewTableSpaceID();

    /* ------------------------------------------------
     * TBS ������ ����� ��ġ�� ������
     * ----------------------------------------------*/
    mWriteOffset = getTBSAttrStartOffset();

    /* ------------------------------------------------
     * 2. tablespace������ mBuffer�� ���
     * ��ũ�������� ��� tablespace ��� ����Ʈ�� ��ȸ�ϸ鼭
     * ��������� �� ����Ѵ�.
     * ----------------------------------------------*/
    sCurrSpaceNode = sctTableSpaceMgr::getFirstSpaceNode();

    while ( sCurrSpaceNode != NULL )
    {
        sNextSpaceNode = sctTableSpaceMgr::getNextSpaceNode( sCurrSpaceNode->mID );

        // PRJ-1548 User Memory Tablespace
        // DROPPED ������ ���̺����̽����� loganchor��
        // �������� �ʴ´�. �׷��Ƿ�, ���������ÿ� loganchor
        // �ʱ�ȭ�� DROPPED�� ���̺����̽� ���� ����.
        if( SMI_TBS_IS_DROPPED(sCurrSpaceNode->mState) )
        {
            sCurrSpaceNode = sNextSpaceNode;
            continue;
        }

        if( sctTableSpaceMgr::isDiskTableSpace( sCurrSpaceNode->mID ) == ID_TRUE )
        {
            /* read from disk manager */
            sddTableSpace::getTableSpaceAttr(
                              (sddTableSpaceNode*)sCurrSpaceNode,
                              &mTableSpaceAttr);

            ((sddTableSpaceNode*)sCurrSpaceNode)->mAnchorOffset
                              = mWriteOffset;

            /* write to loganchor buffer */
            IDE_TEST( writeToBuffer((SChar*)&mTableSpaceAttr,
                                    (UInt)ID_SIZEOF(smiTableSpaceAttr))
                      != IDE_SUCCESS );

            /* ------------------------------------------------
             * loganchor ���ۿ� datafile attributes ������ �����Ѵ�.
             * ----------------------------------------------*/

            for ( i = 0 ; i < ((sddTableSpaceNode*)sCurrSpaceNode)->mNewFileID ; i++ )
            {
                // ���⼭�� sFileNode�� NULL�ϰ��� ����
                sFileNode=((sddTableSpaceNode*)sCurrSpaceNode)->mFileNodeArr[i];

                // alter tablespace drop datafile & restart
                if ( sFileNode == NULL )
                {
                    continue;
                }
                else
                {
                    /* nothing to do */
                }

                // PRJ-1548 User Memory Tablespace
                // DROPPED ������ ����Ÿ���ϳ��� loganchor��
                // �������� �ʴ´�. �׷��Ƿ�, ���������ÿ� loganchor
                // �ʱ�ȭ�� DROPPED�� ����Ÿ���� ���� ����.
                if ( SMI_FILE_STATE_IS_DROPPED( sFileNode->mState ) )
                {
                    continue;
                }
                else
                {
                    /* nothing to do */
                }

                /* read from disk manager */
                sddDataFile::getDataFileAttr( sFileNode, &mDataFileAttr );

                sFileNode->mAnchorOffset = mWriteOffset;

                /* write to loganchor buffer */
                IDE_TEST( writeToBuffer( (SChar*)&mDataFileAttr,
                                         ID_SIZEOF(smiDataFileAttr) )
                          != IDE_SUCCESS );

            }
        }
        else if ( sctTableSpaceMgr::isMemTableSpace( sCurrSpaceNode->mID ) )
        // memory tablespace
        {
            /* read from memory manager */
            smmManager::getTableSpaceAttr( (smmTBSNode*)sCurrSpaceNode,
                                           &mTableSpaceAttr );

            ((smmTBSNode*)sCurrSpaceNode)->mAnchorOffset = mWriteOffset;
            /* write to loganchor buffer */
            IDE_TEST( writeToBuffer( (SChar*)&mTableSpaceAttr,
                                     ID_SIZEOF(smiTableSpaceAttr) )
                      != IDE_SUCCESS );

            /* ------------------------------------------------
             * loganchor ���ۿ� checkpoint path attributes
             * ������ �����Ѵ�.
             * ----------------------------------------------*/
            sChkptPathBaseList = &( ((smmTBSNode*)sCurrSpaceNode)->mChkptPathBase );
            for ( sChkptPathList  = SMU_LIST_GET_FIRST(sChkptPathBaseList) ;
                  sChkptPathList != sChkptPathBaseList ;
                  sChkptPathList  = SMU_LIST_GET_NEXT(sChkptPathList) )
            {
                sChkptPathNode = (smmChkptPathNode*)sChkptPathList->mData;

                sChkptPathNode->mAnchorOffset = mWriteOffset;

                /* write to loganchor buffer */
                IDE_TEST( writeToBuffer(
                               (SChar*)&(sChkptPathNode->mChkptPathAttr),
                               ID_SIZEOF(smiChkptPathAttr) )
                          != IDE_SUCCESS );

            }

            // LstCreatedDBFile�� ���ԵǴ��� ������ �ȵǾ� �ִ� ��쿡��
            // �������� ���� ���� �ִ�.

            sWhichDB = smmManager::getCurrentDB( (smmTBSNode*)sCurrSpaceNode );

            for ( sLoop = 0;
                  sLoop <= ((smmTBSNode*)sCurrSpaceNode)->mLstCreatedDBFile;
                  sLoop++ )
            {
                if ( sctTableSpaceMgr::hasState( sCurrSpaceNode->mID,
                                                 SCT_SS_SKIP_AGING_DISK_TBS ) 
                     != ID_TRUE )
                {
                    IDE_ASSERT( smmManager::openAndGetDBFile(
                                                    ((smmTBSNode*)sCurrSpaceNode),
                                                    sWhichDB,
                                                    sLoop,
                                                    &sDatabaseFile ) 
                                == IDE_SUCCESS );

                    IDE_ASSERT( sDatabaseFile != NULL );

                    // ���� ����Ÿ������ ���µ� ��찡 ������ ����̹Ƿ�,
                    // �α׾�Ŀ�� �����Ѵ�.
                    IDE_ASSERT( sDatabaseFile->isOpen() == ID_TRUE );

                    // üũ����Ʈ�̹����� ��Ÿ�� ��Ÿ����� ��´�.
                    sDatabaseFile->getChkptImageHdr( &sChkptImageHdr );

                    // üũ����Ʈ�̹����� ��Ÿ����� �����Ѵ�.
                    IDE_ASSERT( sDatabaseFile->checkValuesOfDBFHdr( &sChkptImageHdr )
                                == IDE_SUCCESS );

                    // �α׾�Ŀ�� �����ϱ� ���� üũ����Ʈ�̹�����
                    // �Ӽ��� ��´�.
                    sDatabaseFile->getChkptImageAttr( (smmTBSNode*)sCurrSpaceNode,
                                                      &sChkptImageAttr );

                    // �α׾�Ŀ�� ����� Chkpt Image Attribute���
                    // Attribute�� CreateDBFileOnDisk �� ��� �ϳ��� ID_TRUE
                    // ���� ������ �Ѵ�.

                    IDE_ASSERT( smmManager::isCreateDBFileAtLeastOne(
                                             sChkptImageAttr.mCreateDBFileOnDisk )
                                == ID_TRUE );
                }
                else
                {
                    /* BUG-41689 A discarded tablespace is redone in recovery
                     * tablespace�� discard�� ���Ŀ� üũ����Ʈ�̹����� �����ϸ� �ȵȴ�.
                     * ����, �α� ��Ŀ ������ SpaceNode�� ������ �̿���
                     * sChkptImageAttr ������ ���� �Ѵ�. */
                    sChkptImageAttr.mAttrType = SMI_CHKPTIMG_ATTR;
                    sChkptImageAttr.mSpaceID  = sCurrSpaceNode->mID;
                    sChkptImageAttr.mFileNum  = sLoop;

                    /* loganchor ���� �о�� chkpt image������ ���� �´�. */
                    sMemTBSNode   = (smmTBSNode*)sCurrSpaceNode;
                    sDatabaseFile = (smmDatabaseFile*)(sMemTBSNode->mDBFile[sWhichDB][sLoop]);

                    SM_GET_LSN( sChkptImageAttr.mMemCreateLSN, sDatabaseFile->mChkptImageHdr.mMemCreateLSN );

                    for ( i = 0 ; i < SMM_PINGPONG_COUNT; i++ )
                    {
                        sChkptImageAttr.mCreateDBFileOnDisk[ i ]
                            = smmManager::getCreateDBFileOnDisk( sMemTBSNode,
                                                                 i,
                                                                 sLoop );
                    }

                    sChkptImageAttr.mDataFileDescSlotID.mBlockID = 
                        sDatabaseFile->mChkptImageHdr.mDataFileDescSlotID.mBlockID;
                    sChkptImageAttr.mDataFileDescSlotID.mSlotIdx = 
                        sDatabaseFile->mChkptImageHdr.mDataFileDescSlotID.mSlotIdx;

                }

                /* BUG-23763: Server Restart�� Memory���� �����ϴ� TBS, File Node������
                 * LogAnchor Buffer�� Node������ ���⶧ MDB File Node�� Anchor Offset��
                 * �������� �ʾƼ� ���� MDB File Node�� ������ ���Ŷ� �̻��� ��ġ�� Write�Ͽ�
                 * Loganchor�� ������ ���װ� �ֽ��ϴ�. */
                ((smmTBSNode*)sCurrSpaceNode)->mCrtDBFileInfo[sLoop].mAnchorOffset =
                    mWriteOffset;

                /* write to loganchor buffer */
                IDE_TEST( writeToBuffer( (SChar*)&sChkptImageAttr,
                                         ID_SIZEOF( smmChkptImageAttr ) )
                          != IDE_SUCCESS );
            }
        }
        /* PROJ-1594 Volatile TBS */
        else if ( sctTableSpaceMgr::isVolatileTableSpace( sCurrSpaceNode->mID ) == ID_TRUE )
        {
            /* read from volatile manager */
            svmManager::getTableSpaceAttr( (svmTBSNode*)sCurrSpaceNode,
                                           &mTableSpaceAttr );

            ((svmTBSNode*)sCurrSpaceNode)->mAnchorOffset = mWriteOffset;
            /* write to loganchor buffer */
            IDE_TEST( writeToBuffer( (SChar*)&mTableSpaceAttr,
                                     ID_SIZEOF(smiTableSpaceAttr) )
                      != IDE_SUCCESS );
        }
        else
        {
            IDE_ASSERT(0);
        }

        sCurrSpaceNode = sNextSpaceNode;
    }

    IDE_TEST( flushAll() != IDE_SUCCESS );

    sState = 0;
    IDE_TEST( unlock() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();
    {
        if ( sState != 0 )
        {
            IDE_ASSERT( unlock() == IDE_SUCCESS );
        }
    }
    IDE_POP();

    return IDE_FAILURE;

}


/* PROJ-2102 Fast Secondary Buffer */
IDE_RC smrLogAnchorMgr::updateAllSBAndFlush( )
{
    sdsFileNode     * sFileNode  = NULL;
    UInt              sState     = 0;

    IDE_TEST_CONT( sdsBufferMgr::isServiceable() != ID_TRUE,  SKIP );

    IDE_TEST( lock() != IDE_SUCCESS );
    sState = 1;

    sdsBufferMgr::getFileNode( &sFileNode );

    IDE_ASSERT( sFileNode != NULL);

    sdsBufferMgr::getFileAttr( sFileNode,
                               &mSBufferFileAttr );

    sFileNode->mAnchorOffset = mWriteOffset;

    /* write to loganchor buffer */
    IDE_TEST( writeToBuffer( (SChar*)&mSBufferFileAttr,
                             ID_SIZEOF(smiSBufferFileAttr))
              != IDE_SUCCESS );

    IDE_TEST( flushAll() != IDE_SUCCESS );

    sState = 0;
    IDE_TEST( unlock() != IDE_SUCCESS );

    IDE_EXCEPTION_CONT( SKIP );    

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();
    {
        if ( sState != 0 )
        {
            IDE_ASSERT( unlock() == IDE_SUCCESS );
        }
    }
    IDE_POP();

    return IDE_FAILURE;
}


/***********************************************************************
 * Description : �� Attribute�� ����ü�� ũ�⸦ ��ȯ�Ѵ�.
 *
 *  aAttrType - [IN] ũ�⸦ Ȯ���� AttrType
 **********************************************************************/
UInt smrLogAnchorMgr::getAttrSize( smiNodeAttrType aAttrType )
{
    UInt sAttrSize = 0;

    switch ( aAttrType )
    {
        case SMI_TBS_ATTR:
            sAttrSize = ID_SIZEOF( smiTableSpaceAttr );
            break;

        case SMI_DBF_ATTR :
            sAttrSize = ID_SIZEOF( smiDataFileAttr );
            break;

        case SMI_CHKPTPATH_ATTR :
            sAttrSize = ID_SIZEOF( smiChkptPathAttr );
            break;

        case SMI_CHKPTIMG_ATTR:
            sAttrSize = ID_SIZEOF( smmChkptImageAttr );
            break;

        case SMI_SBUFFER_ATTR:
            sAttrSize = ID_SIZEOF( smiSBufferFileAttr );
            break;

        default:
            ideLog::log(SM_TRC_LOG_LEVEL_MRECOV,
                        SM_TRC_INVALID_LOGANCHOR_ATTRTYPE,
                        aAttrType);
            IDE_ASSERT( 0 );
            break;

    }
    return sAttrSize;
}

/***********************************************************************
 * Description : �� TBS�� Attribute�� Anchor Offset�� ��ȯ�Ѵ�.
 *
 *  aSpaceID      - [IN]  Ȯ���� TableSpace�� SpaceID
 *  aSpaceAttr    - [OUT] SpaceAttr�� ��ȯ�Ѵ�.
 *  aAnchorOffset - [OUT] AnchorOffset�� ��ȯ�Ѵ�.
 **********************************************************************/
IDE_RC smrLogAnchorMgr::getTBSAttrAndAnchorOffset( scSpaceID          aSpaceID,
                                                   smiTableSpaceAttr* aSpaceAttr,
                                                   UInt             * aAnchorOffset )
{
    sctTableSpaceNode* sSpaceNode ;

    IDE_ASSERT( aSpaceAttr    != NULL );
    IDE_ASSERT( aAnchorOffset != NULL );

    sSpaceNode     = sctTableSpaceMgr::findSpaceNodeIncludingDropped( aSpaceID );
    *aAnchorOffset = ID_SINT_MAX;

    IDE_ASSERT( sSpaceNode != NULL );

    IDU_FIT_POINT("BUG-46450@smrLogAnchorMgr::getTBSAttrAndAnchorOffset::getTBSLocation");

    switch ( sctTableSpaceMgr::getTBSLocation( sSpaceNode ) )
    {
        case SMI_TBS_DISK:
            sddTableSpace::getTableSpaceAttr( (sddTableSpaceNode*)sSpaceNode,
                                              aSpaceAttr );

            *aAnchorOffset = ((sddTableSpaceNode*)sSpaceNode)->mAnchorOffset;
            break;

        case SMI_TBS_MEMORY:
            smmManager::getTableSpaceAttr( (smmTBSNode*)sSpaceNode,
                                           aSpaceAttr );

            *aAnchorOffset = ((smmTBSNode*)sSpaceNode)->mAnchorOffset;
            break;

        case SMI_TBS_VOLATILE:
            svmManager::getTableSpaceAttr( (svmTBSNode*)sSpaceNode,
                                           aSpaceAttr );

            *aAnchorOffset = ((svmTBSNode*)sSpaceNode)->mAnchorOffset;
            break;

        default:
            IDE_ASSERT_MSG( 0,
                            "Tablespace Type not found ( ID : %"ID_UINT32_FMT", Name : %s ) \n",
                            sSpaceNode->mID,
                            sSpaceNode->mName );
            break;
    }

    return IDE_SUCCESS;

#ifdef ALTIBASE_FIT_CHECK
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
#endif
}

/***********************************************************************
 * Description : smmChkptImageAttr�� ����� DataFileDescSlotID�� ��ȯ�Ѵ�.
 * PROJ-2133 incremental backup
 * TODO  �߰����� ���� �ڵ� �ʿ�?
 *
 *  aReadOffset         - [IN]  ChkptImageAttr�� ��ġ�� loganchor Buffer��
 *                              offset
 *  aDataFileDescSlotID - [OUT] logAnchor�� ����� DataFileDescSlotID��
 *                              ��ȯ�Ѵ�.
 *
 **********************************************************************/
IDE_RC smrLogAnchorMgr::getDataFileDescSlotIDFromChkptImageAttr( 
                                UInt                       aReadOffset,
                                smiDataFileDescSlotID    * aDataFileDescSlotID )
{
    smmChkptImageAttr * sChkptImageAttr;

    IDE_ASSERT( aDataFileDescSlotID    != NULL );

    sChkptImageAttr = ( smmChkptImageAttr * )( mBuffer + aReadOffset );
    IDE_DASSERT( sChkptImageAttr->mAttrType == SMI_CHKPTIMG_ATTR );

    aDataFileDescSlotID->mBlockID = 
                    sChkptImageAttr->mDataFileDescSlotID.mBlockID;
    aDataFileDescSlotID->mSlotIdx = 
                    sChkptImageAttr->mDataFileDescSlotID.mSlotIdx;


   return IDE_SUCCESS;
}

/***********************************************************************
 * Description : DBFNodeAttr�� ����� DataFileDescSlotID�� ��ȯ�Ѵ�.
 * PROJ-2133 incremental backup
 * TODO  �߰����� ���� �ڵ� �ʿ�?
 *
 *  aReadOffset         - [IN]  ChkptImageAttr�� ��ġ�� loganchor Buffer��
 *                              offset
 *  aDataFileDescSlotID - [OUT] logAnchor�� ����� DataFileDescSlotID��
 *                              ��ȯ�Ѵ�.
 *
 **********************************************************************/
IDE_RC smrLogAnchorMgr::getDataFileDescSlotIDFromDBFNodeAttr( 
                                UInt                       aReadOffset,
                                smiDataFileDescSlotID    * aDataFileDescSlotID )
{
    smiDataFileAttr * sDBFNodeAttr;

    IDE_ASSERT( aDataFileDescSlotID    != NULL );

    sDBFNodeAttr = ( smiDataFileAttr * )( mBuffer + aReadOffset );
    IDE_DASSERT( sDBFNodeAttr->mAttrType = SMI_DBF_ATTR );

    aDataFileDescSlotID->mBlockID = 
                    sDBFNodeAttr->mDataFileDescSlotID.mBlockID;
    aDataFileDescSlotID->mSlotIdx = 
                    sDBFNodeAttr->mDataFileDescSlotID.mSlotIdx;


   return IDE_SUCCESS;
}

/***********************************************************************
 * Description : log anchor buffer�� �� node�� �� �����Ѵ�.
 *               log anchor buffer���� �������� space node�� Ȯ���Ѵ�.
 **********************************************************************/
idBool smrLogAnchorMgr::checkLogAnchorBuffer()
{
    UInt               sAnchorOffset;
    smiNodeAttrType    sAttrType;
    idBool             sIsValidAttr;
    idBool             sIsValidAnchor = ID_TRUE;
    UChar*             sAttrPtr;
    smiTableSpaceAttr  sSpaceAttr;
    smiDataFileAttr    sDBFileAttr;
    smiSBufferFileAttr sSBufferFileAttr;    

    sAnchorOffset = getTBSAttrStartOffset();

    while ( sAnchorOffset < mWriteOffset )
    {
        sAttrPtr = mBuffer + sAnchorOffset;
        idlOS::memcpy( &sAttrType, sAttrPtr, ID_SIZEOF(smiNodeAttrType) );

        switch ( sAttrType )
        {
            case SMI_TBS_ATTR :
                idlOS::memcpy( &sSpaceAttr, sAttrPtr,
                               ID_SIZEOF(smiTableSpaceAttr) );

                sIsValidAttr = checkTBSAttr( &sSpaceAttr,
                                             sAnchorOffset ) ;
                break;

            case SMI_DBF_ATTR :
                idlOS::memcpy( &sDBFileAttr, sAttrPtr,
                               ID_SIZEOF(smiDataFileAttr) );

                sIsValidAttr = checkDBFAttr( &sDBFileAttr,
                                             sAnchorOffset ) ;
                break;

            case SMI_CHKPTPATH_ATTR :
                /*   XXX ChkptPath�� ChkptImageȮ����
                 *  ���ü��� Ȯ������ ���� ������ ���Ͽ�
                 *  ���ü��� Ȯ�� �� �� ���� �ϴ� �����Ѵ�. */
/*
                idlOS::memcpy( &sChkptPathAttr, sAttrPtr,
                               ID_SIZEOF(smiChkptPathAttr) );
                sIsValidAttr = checkChkptPathAttr( &sChkptPathAttr,
                                                   sAnchorOffset ) ;
*/
                sIsValidAttr = ID_TRUE;
                break;

            case SMI_CHKPTIMG_ATTR :
/*
                idlOS::memcpy( &sChkptImgAttr, sAttrPtr,
                               ID_SIZEOF(smmChkptImageAttr) );
                sIsValidAttr = checkChkptImageAttr( &sChkptImgAttr,
                                                  sAnchorOffset ) ;
*/
                sIsValidAttr = ID_TRUE;
                break;

            case SMI_SBUFFER_ATTR :
                idlOS::memcpy( &sSBufferFileAttr, 
                               sAttrPtr,
                               ID_SIZEOF(smiSBufferFileAttr) );

                sIsValidAttr = checkSBufferFileAttr( &sSBufferFileAttr,
                                                     sAnchorOffset ) ;
                break; 

            default :
                ideLog::log( SM_TRC_LOG_LEVEL_MRECOV,
                             SM_TRC_INVALID_LOGANCHOR_ATTRTYPE,
                             sAttrType );
                IDE_ASSERT( 0 );
                break;
        }

        if ( sIsValidAttr != ID_TRUE )
        {
            sIsValidAnchor = ID_FALSE;
        }
        else
        {
            /* nothing to do */
        }

        sAnchorOffset += getAttrSize( sAttrType );
    }

    if ( sAnchorOffset != mWriteOffset )
    {
        // ������ ��ġ�� ������ mWriteOffset ���� ���ƾ� �Ѵ�.
        ideLog::log( SM_TRC_LOG_LEVEL_MRECOV,
                     SM_TRC_ANCHOR_CHECK_INVALID_OFFSET,
                     sAnchorOffset,
                     mWriteOffset );

        sIsValidAnchor = ID_FALSE;
    }
    else
    {
        /* nothing to do */
    }

    return sIsValidAnchor;
}

/***********************************************************************
 * Description : TBS�� Space Node�� LogAnchor Buffer�� �� �˻��Ѵ�.
 *
 *  aSpaceAttrByAnchor - [IN] SpaceNode�� ���� LogAnchor�� Attribute
 *  aOffsetByAnchor    - [IN] �� �˻��� Attribute�� Buffer Offset
 **********************************************************************/
idBool smrLogAnchorMgr::checkTBSAttr( smiTableSpaceAttr* aSpaceAttrByAnchor,
                                      UInt               aOffsetByAnchor )
{
    sctTableSpaceNode* sSpaceNode = NULL;
    UInt               sOffsetByNode;
    UInt               sTBSState;
    idBool             sIsValid   = ID_TRUE;

    IDE_ASSERT( aSpaceAttrByAnchor != NULL );
    IDE_ASSERT( aSpaceAttrByAnchor->mAttrType == SMI_TBS_ATTR );

    sTBSState = aSpaceAttrByAnchor->mTBSStateOnLA;

    if ( ( sTBSState & SMI_TBS_DROPPED ) != SMI_TBS_DROPPED )
    {
        // Drop���� �ʾҴٸ� Attr�� ���� ���Ѵ�.

        getTBSAttrAndAnchorOffset( aSpaceAttrByAnchor->mID,
                                   &mTableSpaceAttr,
                                   &sOffsetByNode );

        // SpaceNode�� AnchorOffset�� ����� ��� �Ǿ����� Ȯ���Ѵ�.
        // �ٸ� space node create�ÿ� logAnchor File�� flush�� �Ŀ�
        // SpaceNode�� AnchorOffset�� ����ϱ� ������ ���� ��ϵ���
        // �ʾ��� ���� �ִ�.
        if ( ( sOffsetByNode != aOffsetByAnchor ) &&
             ( sOffsetByNode != SCT_UNSAVED_ATTRIBUTE_OFFSET ) )
        {
            ideLog::log( SM_TRC_LOG_LEVEL_MRECOV,
                         SM_TRC_ANCHOR_CHECK_INVALID_OFFSET_IN_VOL_TBS,
                         aSpaceAttrByAnchor->mID,
                         aOffsetByAnchor,
                         sOffsetByNode );

            sIsValid = ID_FALSE;
        }

        // offset�� �޶� ���� Ȯ���ϱ� ���ؼ� tbs attr ������
        // Ȯ���Ѵ�.
        if ( cmpTableSpaceAttr( &mTableSpaceAttr,
                                aSpaceAttrByAnchor) != ID_TRUE )
        {
            sIsValid = ID_FALSE;
        }
        else
        {
            /* nothing to do */
        }
    }
    else
    {
        // ���� state�� Drop�̸� ������ Drop�Ǿ����� Ȯ���Ѵ�.

        sSpaceNode = sctTableSpaceMgr::findSpaceNodeIncludingDropped( aSpaceAttrByAnchor->mID );

        // server start �� null�� ���� �ִ�.
        if ( sSpaceNode != NULL )
        {
            sTBSState = sSpaceNode->mState ;
            IDE_ASSERT( SMI_TBS_IS_DROPPED(sTBSState) );
        }
        else
        {
            /* nothing to do */
        }
    }

    return sIsValid;
}

/***********************************************************************
 * Description : Disk TBS�� File Node�� LogAnchor Buffer�� �� �˻��Ѵ�.
 *
 *  aFileAttrByAnchor - [IN] File Node�� ���� LogAnchor�� Attribute
 *  aOffsetByAnchor   - [IN] �� �˻��� Attribute�� Buffer Offset
 **********************************************************************/
idBool smrLogAnchorMgr::checkDBFAttr( smiDataFileAttr* aFileAttrByAnchor,
                                      UInt             aOffsetByAnchor )
{
    sddTableSpaceNode* sSpaceNode = NULL;
    sddDataFileNode  * sFileNode  = NULL;
    idBool             sIsValid   = ID_TRUE;
    UInt               sFileAttrState;

    IDE_ASSERT( aFileAttrByAnchor != NULL );
    IDE_ASSERT( aFileAttrByAnchor->mAttrType == SMI_DBF_ATTR );

    sFileAttrState = aFileAttrByAnchor->mState ;

    if ( SMI_FILE_STATE_IS_NOT_DROPPED( sFileAttrState ) )
    {
        //-----------------------------------------
        // File node�� �����´�.
        //-----------------------------------------

        IDE_ASSERT( sctTableSpaceMgr::findSpaceNodeBySpaceID(
                                                    aFileAttrByAnchor->mSpaceID,
                                                    (void**)&sSpaceNode )
                     == IDE_SUCCESS );

        IDE_ASSERT( sSpaceNode != NULL );

        IDE_ASSERT( aFileAttrByAnchor->mID < sSpaceNode->mNewFileID );

        sFileNode = sSpaceNode->mFileNodeArr[ aFileAttrByAnchor->mID ];

        IDE_ASSERT( sFileNode != NULL );

        //-----------------------------------------
        // offset �� attr �˻�
        //-----------------------------------------

        if ( ( sFileNode->mAnchorOffset != aOffsetByAnchor ) &&
             ( sFileNode->mAnchorOffset != SCT_UNSAVED_ATTRIBUTE_OFFSET ) )
        {
            ideLog::log( SM_TRC_LOG_LEVEL_MRECOV,
                         SM_TRC_ANCHOR_CHECK_INVALID_OFFSET_IN_DBFILE,
                         aFileAttrByAnchor->mSpaceID,
                         aFileAttrByAnchor->mID,
                         aOffsetByAnchor,
                         sFileNode->mAnchorOffset );

            sIsValid = ID_FALSE;
        }

        sddDataFile::getDataFileAttr( sFileNode,
                                      &mDataFileAttr );

        if ( cmpDataFileAttr( &mDataFileAttr,
                              aFileAttrByAnchor ) != ID_TRUE )
        {
            sIsValid = ID_FALSE;
        }
    }
    else
    {
        //-----------------------------------------
        // table space�� drop���� Ȯ��.
        //-----------------------------------------

        sSpaceNode = (sddTableSpaceNode*)sctTableSpaceMgr::findSpaceNodeIncludingDropped( aFileAttrByAnchor->mSpaceID );

        // server start �� drop�̸� null�� ���� �ִ�.
        if ( sSpaceNode != NULL )
        {
            sFileAttrState = sSpaceNode->mHeader.mState ;

            if ( ( sFileAttrState & SMI_TBS_DROPPED ) != SMI_TBS_DROPPED )
            {
                // TBS�� drop�� �ƴ϶�� file node�� drop���� Ȯ��

                IDE_ASSERT( aFileAttrByAnchor->mID < sSpaceNode->mNewFileID );

                sFileNode = sSpaceNode->mFileNodeArr[ aFileAttrByAnchor->mID ];

                // server start �� drop�̸� null�� ���� �ִ�.
                if ( sFileNode != NULL )
                {
                    sFileAttrState = sFileNode->mState ;

                    IDE_ASSERT( SMI_FILE_STATE_IS_DROPPED(  sFileAttrState ) );
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

    return sIsValid;
}

/***********************************************************************
 * Description : LogAnchor buffer�� Checkpoint Path Attr�� �ش��ϴ�
 *               Checkpoint Path node�� �����ϴ��� Ȯ���Ѵ�.
 *
 *  aCPPathAttrByAnchor - [IN] CP Path Node�� ���� LogAnchor�� Attribute
 *  aOffsetByAnchor     - [IN] �� �˻��� Attribute�� Buffer Offset
 **********************************************************************/
idBool smrLogAnchorMgr::checkChkptPathAttr( smiChkptPathAttr* aCPPathAttrByAnchor,
                                            UInt              aOffsetByAnchor )
{
    smuList*           sChkptPathList;
    smuList*           sChkptPathBaseList;
    smmChkptPathNode * sChkptPathNode;
    smiChkptPathAttr * sCPPathAttrByNode;
    smmTBSNode*        sSpaceNode  = NULL;
    idBool             sIsValid    = ID_TRUE;
    UInt               sDropState;

    IDE_ASSERT( aCPPathAttrByAnchor->mAttrType == SMI_CHKPTPATH_ATTR );

    sSpaceNode = (smmTBSNode*)sctTableSpaceMgr::findSpaceNodeIncludingDropped( aCPPathAttrByAnchor->mSpaceID );

    // TBS�� Drop�Ǹ� restart�� loganchor���� ������
    // space node�� null�� �� ���� �ִ�.
    IDE_TEST_CONT( sSpaceNode == NULL , Skip_Attr_Compare );

    // TBS�� Ȯ���ؼ� Drop�� �ƴ� ��쿡�� Ȯ��
    sDropState = sSpaceNode->mHeader.mState ;

    IDE_TEST_CONT( SMI_TBS_IS_DROPPED(sDropState) ||
                   SMI_TBS_IS_DROPPING(sDropState), 
                   Skip_Attr_Compare );

    sIsValid = ID_FALSE;

    sChkptPathBaseList = &(sSpaceNode->mChkptPathBase);

    for ( sChkptPathList  = SMU_LIST_GET_FIRST(sChkptPathBaseList);
          sChkptPathList != sChkptPathBaseList;
          sChkptPathList  = SMU_LIST_GET_NEXT(sChkptPathList) )
    {
        sChkptPathNode    = (smmChkptPathNode*)sChkptPathList->mData;
        sCPPathAttrByNode = &(sChkptPathNode->mChkptPathAttr);

        // ���Ͼ� ���� ���� Path Node�� �ִ��� Ȯ���Ѵ�.
        if ( ( sCPPathAttrByNode->mSpaceID != aCPPathAttrByAnchor->mSpaceID ) ||
             ( idlOS::strcmp( sCPPathAttrByNode->mChkptPath,
                             aCPPathAttrByAnchor->mChkptPath ) != 0 ) )
        {
            continue;
        }
        else
        {
            /* nothing to do */
        }

        // chkpt node�� ã�Ҵ�
        sIsValid    = ID_TRUE;

        // �� ������ ����
        if ( ( sCPPathAttrByNode->mAttrType != SMI_CHKPTPATH_ATTR ) ||
             ( sCPPathAttrByNode->mSpaceID  >= SC_MAX_SPACE_COUNT ) ||
             ( idlOS::strlen(sCPPathAttrByNode->mChkptPath)
               >  SMI_MAX_CHKPT_PATH_NAME_LEN ) )
        {
            ideLog::log( SM_TRC_LOG_LEVEL_MRECOV,
                         SM_TRC_ANCHOR_CHECK_INVALID_OFFSET_IN_CHKPT_PATH,
                         sSpaceNode->mHeader.mID,
                         aOffsetByAnchor,
                         sChkptPathNode->mAnchorOffset );

            sIsValid = ID_FALSE;
        }
        else
        {
            /* nothing to do */
        }

        // offsetȮ��
        if ( ( sChkptPathNode->mAnchorOffset != aOffsetByAnchor ) &&
             ( sChkptPathNode->mAnchorOffset != SCT_UNSAVED_ATTRIBUTE_OFFSET ) )
        {
            ideLog::log( SM_TRC_LOG_LEVEL_MRECOV,
                         SM_TRC_ANCHOR_CHECK_INVALID_OFFSET_IN_CHKPT_PATH,
                         sSpaceNode->mHeader.mID,
                         aOffsetByAnchor,
                         sChkptPathNode->mAnchorOffset );

            sIsValid = ID_FALSE;
        }
        else
        {
            /* nothing to do */
        }

        break;
    }

    if ( sIsValid != ID_TRUE )
    {
        // ���� Ʋ���ٸ� Anchor ���� ���
        ideLog::log( SM_TRC_LOG_LEVEL_MRECOV,
                     SM_TRC_ANCHOR_CHECK_CHKPT_PATH_ATTR_INVALID,
                     aCPPathAttrByAnchor->mSpaceID,
                     aCPPathAttrByAnchor->mAttrType,
                     aCPPathAttrByAnchor->mChkptPath,
                     aOffsetByAnchor,
                     mWriteOffset,
                     ID_SIZEOF( smiChkptPathAttr ) );

        for ( sChkptPathList  = SMU_LIST_GET_FIRST(sChkptPathBaseList);
              sChkptPathList != sChkptPathBaseList;
              sChkptPathList  = SMU_LIST_GET_NEXT(sChkptPathList) )
        {
            sChkptPathNode    = (smmChkptPathNode*)sChkptPathList->mData;
            sCPPathAttrByNode = &(sChkptPathNode->mChkptPathAttr);

            // ������ ���� ���� node�� ���ٸ� node ���� ���
            ideLog::log( SM_TRC_LOG_LEVEL_MRECOV,
                         SM_TRC_ANCHOR_CHECK_CHKPT_PATH_ATTR_NOT_FOUND,
                         sCPPathAttrByNode->mSpaceID,
                         sCPPathAttrByNode->mAttrType,
                         sCPPathAttrByNode->mChkptPath );
        }
        IDE_ASSERT( 0 );
    }

    IDE_EXCEPTION_CONT( Skip_Attr_Compare );

    return sIsValid;
}

/***********************************************************************
 * Description : LogAnchor buffer�� Checkpoint Image Attr�� �ش��ϴ�
 *               Checkpoint Image node�� �����ϴ��� Ȯ���Ѵ�.
 *
 *  aCPImgAttrByAnchor - [IN] CP Image Node�� ���� LogAnchor�� Attribute
 *  aOffsetByAnchor    - [IN] �� �˻��� Attribute�� Buffer Offset
 **********************************************************************/
idBool smrLogAnchorMgr::checkChkptImageAttr( smmChkptImageAttr* aCPImgAttrByAnchor,
                                             UInt               aOffsetByAnchor )
{
    UInt               sWhichDB;
    UInt               sOffsetByNode;
    smmDatabaseFile  * sDatabaseFile;
    smmTBSNode*        sSpaceNode = NULL;
    idBool             sIsValid   = ID_TRUE;
    UInt               sDropState;

    IDE_ASSERT( aCPImgAttrByAnchor->mAttrType == SMI_CHKPTIMG_ATTR );

    sSpaceNode = (smmTBSNode*)sctTableSpaceMgr::findSpaceNodeIncludingDropped( aCPImgAttrByAnchor->mSpaceID );

    // TBS�� Drop�Ǹ� restart�� loganchor���� ������
    // space node�� null�� �� ���� �ִ�.
    IDE_TEST_RAISE( sSpaceNode == NULL , Skip_Attr_Compare );

    // TBS�� Ȯ���ؼ� Drop�� �ƴ� ��쿡�� Ȯ��

    sDropState = sSpaceNode->mHeader.mState ;

    IDE_TEST_RAISE( SMI_TBS_IS_DROPPED(sDropState) ||
                    SMI_TBS_IS_DROPPING(sDropState) , 
                    Skip_Attr_Compare);

    for ( sWhichDB = 0 ; sWhichDB < SMM_PINGPONG_COUNT ; sWhichDB++ )
    {
        if ( aCPImgAttrByAnchor->mCreateDBFileOnDisk[ sWhichDB ] != ID_TRUE )
        {
            continue;
        }
        else
        {
            /* nothing to do */
        }

        if ( smmManager::getDBFile( sSpaceNode,
                                    sWhichDB,
                                    aCPImgAttrByAnchor->mFileNum,
                                    SMM_GETDBFILEOP_NONE,
                                    &sDatabaseFile ) == IDE_SUCCESS )
        {
            IDE_ASSERT( sDatabaseFile != NULL );

            sDatabaseFile->getChkptImageAttr( sSpaceNode,
                                              &mChkptImageAttr );

            sOffsetByNode = smmManager::getAnchorOffsetFromCrtDBFileInfo(
                                                        sSpaceNode,
                                                        mChkptImageAttr.mFileNum );

            if ( ( sOffsetByNode != aOffsetByAnchor ) &&
                 ( sOffsetByNode != SCT_UNSAVED_ATTRIBUTE_OFFSET ) )
            {
                ideLog::log( SM_TRC_LOG_LEVEL_MRECOV,
                             SM_TRC_ANCHOR_CHECK_INVALID_OFFSET_IN_CHKPT_IMG,
                             sSpaceNode->mHeader.mID,
                             aOffsetByAnchor,
                             sOffsetByNode );

                sIsValid = ID_FALSE;
            }
            else
            {
                /* nothing to do */
            }

            // offset�� �޶� ���� Ȯ���� ���� �񱳰˻��Ѵ�.
            if ( cmpChkptImageAttr( &mChkptImageAttr,
                                    aCPImgAttrByAnchor ) != ID_TRUE )
            {
                sIsValid  = ID_FALSE;
            }
            else
            {
                /* nothing to do */
            }
        }
        else
        {
            sIsValid = ID_FALSE;

            // ������ ���� ���� node�� ���ٸ� ChkptImageAttr ���� ���
            ideLog::log( SM_TRC_LOG_LEVEL_MRECOV,
                         SM_TRC_ANCHOR_CHECK_CHKPT_IMAGE_ATTR_NOT_FOUND,
                         aCPImgAttrByAnchor->mSpaceID,
                         aCPImgAttrByAnchor->mAttrType,
                         sWhichDB,
                         aCPImgAttrByAnchor->mFileNum,
                         aCPImgAttrByAnchor->mCreateDBFileOnDisk[0],
                         aCPImgAttrByAnchor->mCreateDBFileOnDisk[1],
                         aOffsetByAnchor,
                         mWriteOffset,
                         ID_SIZEOF( smmChkptImageAttr ) );


            ideLog::log( SM_TRC_LOG_LEVEL_MRECOV,
                         "MemCreateLSN (%"ID_UINT32_FMT"/%"ID_UINT32_FMT")",
                         aCPImgAttrByAnchor->mMemCreateLSN.mFileNo,
                         aCPImgAttrByAnchor->mMemCreateLSN.mOffset );
        }
    }

    IDE_EXCEPTION_CONT( Skip_Attr_Compare );

    /* BUG-40385 sResult ���� ���� Failure ������ �� �����Ƿ�,
     * ���� IDE_TEST_RAISE -> IDE_TEST_CONT �� ��ȯ���� �ʴ´�. */
    return sIsValid;
}


/***********************************************************************
 * Description :  File Node�� LogAnchor Buffer�� �� �˻��Ѵ�.
 *
 *  aFileAttrByAnchor - [IN] File Node�� ���� LogAnchor�� Attribute
 *  aOffsetByAnchor   - [IN] �� �˻��� Attribute�� Buffer Offset
 **********************************************************************/
idBool smrLogAnchorMgr::checkSBufferFileAttr( smiSBufferFileAttr* aFileAttrByAnchor,
                                              UInt                aOffsetByAnchor )
{
    sdsFileNode     * sFileNode  = NULL;
    idBool            sIsValid   = ID_TRUE;

    IDE_ASSERT( aFileAttrByAnchor != NULL );
    IDE_ASSERT( aFileAttrByAnchor->mAttrType == SMI_SBUFFER_ATTR );

    //-----------------------------------------
    // File node�� �����´�.
    //-----------------------------------------

    sdsBufferMgr::getFileNode ( &sFileNode );

    IDE_ASSERT( sFileNode != NULL );

    //-----------------------------------------
    // offset �� attr �˻�
    //-----------------------------------------

    if ( ( sFileNode->mAnchorOffset != aOffsetByAnchor ) &&
         ( sFileNode->mAnchorOffset != SCT_UNSAVED_ATTRIBUTE_OFFSET ) )
    {
        ideLog::log( SM_TRC_LOG_LEVEL_MRECOV,
                     SM_TRC_ANCHOR_CHECK_INVALID_OFFSET_IN_SECONDARY_BUFFER,
                     aOffsetByAnchor,
                     sFileNode->mAnchorOffset );

        sIsValid = ID_FALSE;
    }

    sdsBufferMgr::getFileAttr( sFileNode,
                               &mSBufferFileAttr );

    if ( cmpSBufferFileAttr( &mSBufferFileAttr,
                             aFileAttrByAnchor ) != ID_TRUE )
    {
        sIsValid = ID_FALSE;
    }
    return sIsValid;
}

/***********************************************************************
 * Description : TBS Node�� LogAnchor Buffer�� ���Ѵ�.
 *
 *  aSpaceAttrByNode   - [IN] Space Node���� ������ Attribute
 *  aSpaceAttrByAnchor - [IN] Anchor Buffer���� Attribute
 **********************************************************************/
idBool smrLogAnchorMgr::cmpTableSpaceAttr( smiTableSpaceAttr* aSpaceAttrByNode,
                                           smiTableSpaceAttr* aSpaceAttrByAnchor)

{
    idBool sIsValid = ID_TRUE;
    idBool sIsEqual = ID_TRUE;

    IDE_DASSERT( aSpaceAttrByNode   != NULL );
    IDE_DASSERT( aSpaceAttrByAnchor != NULL );

    // ���Ǵ� ����, ��� ���
    if ( ( aSpaceAttrByAnchor->mAttrFlag     != aSpaceAttrByNode->mAttrFlag ) ||
         ( aSpaceAttrByAnchor->mTBSStateOnLA != aSpaceAttrByNode->mTBSStateOnLA ) )
    {
        sIsEqual = ID_FALSE;
    }
    else
    {
        /* nothing to do */
    }

    // �������ʴ� ����, ���� ��ȯ
    if ( ( aSpaceAttrByAnchor->mAttrType != aSpaceAttrByNode->mAttrType ) ||
         ( aSpaceAttrByAnchor->mID       != aSpaceAttrByNode->mID ) ||
         ( aSpaceAttrByAnchor->mType     != aSpaceAttrByNode->mType ) )
    {
        sIsValid = ID_FALSE;
    }
    else
    {
        /* nothing to do */
    }

    // ���� ������ �ʰ�, ���� ��ȯ
    if ( ( aSpaceAttrByNode->mAttrType   != SMI_TBS_ATTR ) ||
         ( aSpaceAttrByNode->mNameLength >  SMI_MAX_TABLESPACE_NAME_LEN ) ||
         ( aSpaceAttrByNode->mID         >= SC_MAX_SPACE_COUNT ) ||
         ( aSpaceAttrByNode->mType       >= SMI_TABLESPACE_TYPE_MAX ) )
    {
        sIsValid = ID_FALSE;
    }
    else
    {
        /* nothing to do */
    }

    IDU_FIT_POINT("BUG-46450@smrLogAnchorMgr::cmpTableSpaceAttr::getTBSLocation");

    switch ( sctTableSpaceMgr::getTBSLocation( aSpaceAttrByAnchor->mID ) )
    {
        case SMI_TBS_DISK:
            cmpDiskTableSpaceAttr( aSpaceAttrByNode,
                                   aSpaceAttrByAnchor,
                                   sIsEqual,
                                   &sIsValid );
            break;

        case SMI_TBS_MEMORY:
            cmpMemTableSpaceAttr( aSpaceAttrByNode,
                                  aSpaceAttrByAnchor,
                                  sIsEqual,
                                  &sIsValid );
            break;

        case SMI_TBS_VOLATILE:
            cmpVolTableSpaceAttr( aSpaceAttrByNode,
                                  aSpaceAttrByAnchor,
                                  sIsEqual,
                                  &sIsValid  );
            break;

        default:
            IDE_ASSERT_MSG( 0,
                            "Tablespace Type not found ( ID : %"ID_UINT32_FMT", Name : %s ) \n",
                            aSpaceAttrByAnchor->mID,
                            aSpaceAttrByAnchor->mName );
            break;
    }

#ifdef ALTIBASE_FIT_CHECK
    IDE_EXCEPTION_END;
#endif

    return sIsValid;
}

/***********************************************************************
 * Description : Disk TBS Node�� LogAnchor Buffer�� ���Ѵ�.
 *
 *  aSpaceAttrByNode   - [IN] Space Node���� ������ Attribute
 *  aSpaceAttrByAnchor - [IN] Anchor Buffer���� Attribute
 *  aIsEqual           - [IN] ���� ������ Space Attr�� ���Ǵ� ����
 *                            �ȿ��� ���̰� �ִ�.
 *  aIsValid           - [IN/OUT] ���� ������ TableSpace Attr�� ġ������
 *                                ������ �ִ��� ���θ� ��Ÿ���� ����
 *                                ���� ������ �� ����� ��ȯ�Ѵ�.
 **********************************************************************/
void smrLogAnchorMgr::cmpDiskTableSpaceAttr( smiTableSpaceAttr* aSpaceAttrByNode,
                                             smiTableSpaceAttr* aSpaceAttrByAnchor,
                                             idBool             aIsEqual,
                                             idBool           * aIsValid )
{
    smiDiskTableSpaceAttr * sDiskAttrByNode;
    smiDiskTableSpaceAttr * sDiskAttrByAnchor;

    IDE_DASSERT( aSpaceAttrByNode   != NULL );
    IDE_DASSERT( aSpaceAttrByAnchor != NULL );

    sDiskAttrByNode   = &aSpaceAttrByNode->mDiskAttr;
    sDiskAttrByAnchor = &aSpaceAttrByAnchor->mDiskAttr;

    // ���Ǵ� ����, ��� ���
    if ( sDiskAttrByAnchor->mNewFileID != sDiskAttrByNode->mNewFileID )
    {
        aIsEqual = ID_FALSE;
    }
    else
    {
        /* nothing to do */
    }

    // �������ʴ� ����, ���� ��ȯ
    if ( ( sDiskAttrByAnchor->mExtMgmtType  != sDiskAttrByNode->mExtMgmtType ) ||
         ( sDiskAttrByAnchor->mExtPageCount != sDiskAttrByNode->mExtPageCount ) ||
         ( sDiskAttrByAnchor->mSegMgmtType  != sDiskAttrByNode->mSegMgmtType ) ||
         ( sDiskAttrByAnchor->mNewFileID    >  SD_MAX_FID_COUNT ) ||
         ( sDiskAttrByAnchor->mExtMgmtType  >= SMI_EXTENT_MGMT_MAX ) ||
         ( sDiskAttrByAnchor->mSegMgmtType  >= SMI_SEGMENT_MGMT_MAX ) )
    {
        *aIsValid = ID_FALSE;
    }
    else
    {
        /* nothing to do */
    }

    // ���� ���
    if ( ( aIsEqual  != ID_TRUE ) ||
         ( *aIsValid != ID_TRUE ) )
    {
        ideLog::log( SM_TRC_LOG_LEVEL_MRECOV,
                     SM_TRC_ANCHOR_CHECK_DISK_TBS_ATTR,

                     "SpaceNode",
                     aSpaceAttrByNode->mAttrType,
                     aSpaceAttrByNode->mID,
                     aSpaceAttrByNode->mName,
                     idlOS::strlen(aSpaceAttrByNode->mName),
                     aSpaceAttrByNode->mType,
                     aSpaceAttrByNode->mTBSStateOnLA,
                     aSpaceAttrByNode->mAttrFlag,
                     sDiskAttrByNode->mNewFileID,
                     sDiskAttrByNode->mExtMgmtType,
                     sDiskAttrByNode->mExtPageCount );

        ideLog::log( SM_TRC_LOG_LEVEL_MRECOV,
                     SM_TRC_ANCHOR_CHECK_DISK_TBS_ATTR,

                     "LogAnchor",
                     aSpaceAttrByAnchor->mAttrType,
                     aSpaceAttrByAnchor->mID,
                     aSpaceAttrByAnchor->mName,
                     aSpaceAttrByAnchor->mNameLength,
                     aSpaceAttrByAnchor->mType,
                     aSpaceAttrByAnchor->mTBSStateOnLA,
                     aSpaceAttrByAnchor->mAttrFlag,
                     sDiskAttrByAnchor->mNewFileID,
                     sDiskAttrByAnchor->mExtMgmtType,
                     sDiskAttrByAnchor->mExtPageCount );
    }
    else
    {
        /* nothing to do */
    }
}

/***********************************************************************
 * Description : Memory TBS Node�� LogAnchor Buffer�� ���Ѵ�.
 *
 *  aSpaceAttrByNode   - [IN] Space Node���� ������ Attribute
 *  aSpaceAttrByAnchor - [IN] Anchor Buffer���� Attribute
 *  aIsEqual           - [IN] ���� ������ Space Attr�� ���Ǵ� ����
 *                            �ȿ��� ���̰� �ִ�.
 *  aIsValid           - [IN/OUT] ���� ������ TableSpace Attr�� ġ������
 *                                ������ �ִ��� ���θ� ��Ÿ����, ����
 *                                ���� ������ �� ����� ��ȯ�Ѵ�.
 **********************************************************************/
void smrLogAnchorMgr::cmpMemTableSpaceAttr( smiTableSpaceAttr* aSpaceAttrByNode,
                                            smiTableSpaceAttr* aSpaceAttrByAnchor,
                                            idBool             aIsEqual,
                                            idBool           * aIsValid )
{
    smiMemTableSpaceAttr * sMemAttrByNode;
    smiMemTableSpaceAttr * sMemAttrByAnchor;

    IDE_ASSERT( aSpaceAttrByNode   != NULL );
    IDE_ASSERT( aSpaceAttrByAnchor != NULL );

    sMemAttrByNode   = &aSpaceAttrByNode->mMemAttr;
    sMemAttrByAnchor = &aSpaceAttrByAnchor->mMemAttr;

    // ���Ǵ� ����, ��� ���
    if ( ( sMemAttrByAnchor->mIsAutoExtend   != sMemAttrByNode->mIsAutoExtend )  ||
         ( sMemAttrByAnchor->mMaxPageCount   != sMemAttrByNode->mMaxPageCount )  ||
         ( sMemAttrByAnchor->mNextPageCount  != sMemAttrByNode->mNextPageCount ) ||
         ( sMemAttrByAnchor->mShmKey         != sMemAttrByNode->mShmKey )    ||
         ( sMemAttrByAnchor->mCurrentDB      != sMemAttrByNode->mCurrentDB ) ||
         ( sMemAttrByAnchor->mChkptPathCount != sMemAttrByNode->mChkptPathCount ) ||
         ( sMemAttrByAnchor->mSplitFilePageCount
          != sMemAttrByNode->mSplitFilePageCount ) )
    {
        aIsEqual = ID_FALSE;
    }
    else
    {
        /* nothing to do */
    }

    // �������ʴ� ����, ���� ��ȯ
    if ( ( sMemAttrByAnchor->mInitPageCount  != sMemAttrByNode->mInitPageCount ) ||
         ( ( sMemAttrByAnchor->mIsAutoExtend != ID_TRUE ) &&
           ( sMemAttrByAnchor->mIsAutoExtend != ID_FALSE ) ) )
    {
        *aIsValid = ID_FALSE;
    }
    else
    {
        /* nothing to do */
    }

    // ���� ���
    if ( ( aIsEqual  != ID_TRUE ) ||
         ( *aIsValid != ID_TRUE ) )
    {
        ideLog::log( SM_TRC_LOG_LEVEL_MRECOV,
                     SM_TRC_ANCHOR_CHECK_MEM_TBS_ATTR,

                     "SpaceNode",
                     aSpaceAttrByNode->mAttrType,
                     aSpaceAttrByNode->mID,
                     aSpaceAttrByNode->mName,
                     idlOS::strlen(aSpaceAttrByNode->mName),
                     aSpaceAttrByNode->mType,
                     aSpaceAttrByNode->mTBSStateOnLA,
                     aSpaceAttrByNode->mAttrFlag,

                     sMemAttrByNode->mShmKey,
                     sMemAttrByNode->mCurrentDB,
                     sMemAttrByNode->mChkptPathCount,
                     sMemAttrByNode->mSplitFilePageCount,

                     sMemAttrByNode->mIsAutoExtend,
                     sMemAttrByNode->mMaxPageCount,
                     sMemAttrByNode->mNextPageCount,
                     sMemAttrByNode->mInitPageCount );

        ideLog::log( SM_TRC_LOG_LEVEL_MRECOV,
                     SM_TRC_ANCHOR_CHECK_MEM_TBS_ATTR,

                     "LogAnchor",
                     aSpaceAttrByAnchor->mAttrType,
                     aSpaceAttrByAnchor->mID,
                     aSpaceAttrByAnchor->mName,
                     aSpaceAttrByAnchor->mNameLength,
                     aSpaceAttrByAnchor->mType,
                     aSpaceAttrByAnchor->mTBSStateOnLA,
                     aSpaceAttrByAnchor->mAttrFlag,

                     sMemAttrByNode->mShmKey,
                     sMemAttrByNode->mCurrentDB,
                     sMemAttrByNode->mChkptPathCount,
                     sMemAttrByNode->mSplitFilePageCount,

                     sMemAttrByAnchor->mIsAutoExtend,
                     sMemAttrByAnchor->mMaxPageCount,
                     sMemAttrByAnchor->mNextPageCount,
                     sMemAttrByAnchor->mInitPageCount );
    }
    else
    {
        /* nothing to do */
    }
}

/***********************************************************************
 * Description : Volatile TBS Node�� LogAnchor Buffer�� ���Ѵ�.
 *
 *  aSpaceAttrByNode   - [IN] Space Node���� ������ Attribute
 *  aSpaceAttrByAnchor - [IN] Anchor Buffer���� Attribute
 *  aIsEqual           - [IN] ���� ������ Space Attr�� ���Ǵ� ����
 *                            �ȿ��� ���̰� �ִ�.
 *  aIsValid           - [IN/OUT] ���� ������ TableSpace Attr�� ġ������
 *                                ������ �ִ��� ���θ� ��Ÿ���� ����
 *                                ���� ������ �� ����� ��ȯ�Ѵ�.
 **********************************************************************/
void smrLogAnchorMgr::cmpVolTableSpaceAttr( smiTableSpaceAttr* aSpaceAttrByNode,
                                            smiTableSpaceAttr* aSpaceAttrByAnchor,
                                            idBool             aIsEqual,
                                            idBool           * aIsValid )
{
    smiVolTableSpaceAttr * sVolAttrByAnchor;
    smiVolTableSpaceAttr * sVolAttrByNode;

    IDE_ASSERT( aSpaceAttrByNode   != NULL );
    IDE_ASSERT( aSpaceAttrByAnchor != NULL );

    sVolAttrByNode   = &aSpaceAttrByNode->mVolAttr;
    sVolAttrByAnchor = &aSpaceAttrByAnchor->mVolAttr;

    // ���Ǵ� ����, ��� ���
    if ( ( sVolAttrByAnchor->mIsAutoExtend  != sVolAttrByNode->mIsAutoExtend ) ||
         ( sVolAttrByAnchor->mMaxPageCount  != sVolAttrByNode->mMaxPageCount ) ||
         ( sVolAttrByAnchor->mNextPageCount != sVolAttrByNode->mNextPageCount ) )
    {
        aIsEqual = ID_FALSE;
    }
    else
    {
        /* nothing to do */
    }

    // �������ʴ� ����, ���� ��ȯ
    if ( ( sVolAttrByAnchor->mInitPageCount  != sVolAttrByNode->mInitPageCount ) ||
         ( ( sVolAttrByAnchor->mIsAutoExtend != ID_TRUE ) &&
           ( sVolAttrByAnchor->mIsAutoExtend != ID_FALSE ) ) )
    {
        *aIsValid = ID_FALSE;
    }
    else
    {
        /* nothing to do */
    }

    // ���� ���
    if ( ( aIsEqual  != ID_TRUE ) ||
         ( *aIsValid != ID_TRUE ) )
    {
        ideLog::log( SM_TRC_LOG_LEVEL_MRECOV,
                     SM_TRC_ANCHOR_CHECK_VOL_TBS_ATTR,

                     "SpaceNode",
                     aSpaceAttrByNode->mAttrType,
                     aSpaceAttrByNode->mID,
                     aSpaceAttrByNode->mName,
                     idlOS::strlen(aSpaceAttrByNode->mName),
                     aSpaceAttrByNode->mType,
                     aSpaceAttrByNode->mTBSStateOnLA,
                     aSpaceAttrByNode->mAttrFlag,
                     sVolAttrByNode->mIsAutoExtend,
                     sVolAttrByNode->mMaxPageCount,
                     sVolAttrByNode->mNextPageCount,
                     sVolAttrByNode->mInitPageCount );

        ideLog::log( SM_TRC_LOG_LEVEL_MRECOV,
                     SM_TRC_ANCHOR_CHECK_VOL_TBS_ATTR,

                     "LogAnchor",
                     aSpaceAttrByAnchor->mAttrType,
                     aSpaceAttrByAnchor->mID,
                     aSpaceAttrByAnchor->mName,
                     aSpaceAttrByAnchor->mNameLength,
                     aSpaceAttrByAnchor->mType,
                     aSpaceAttrByAnchor->mTBSStateOnLA,
                     aSpaceAttrByAnchor->mAttrFlag,
                     sVolAttrByAnchor->mIsAutoExtend,
                     sVolAttrByAnchor->mMaxPageCount,
                     sVolAttrByAnchor->mNextPageCount,
                     sVolAttrByAnchor->mInitPageCount );
    }
    else
    {
        /* nothing to do */
    }
}

/***********************************************************************
 * Description : Disk TBS�� DBFile Node�� LogAnchor Buffer�� ���Ѵ�.
 *
 *  aFileAttrByNode   - [IN] DBFile Node���� ������ Attribute
 *  aFileAttrByAnchor - [IN] Anchor Buffer���� Attribute
 **********************************************************************/
idBool smrLogAnchorMgr::cmpDataFileAttr( smiDataFileAttr*   aFileAttrByNode,
                                         smiDataFileAttr*   aFileAttrByAnchor )
{
    idBool sIsEqual = ID_TRUE;
    idBool sIsValid = ID_TRUE;

    IDE_DASSERT( aFileAttrByNode != NULL );
    IDE_DASSERT( aFileAttrByAnchor != NULL );

    // ���� �����Ѱ�, �Ͻ������� ���̰� �� �� �ִ�. �α׸� ����Ѵ�.
    if ( ( idlOS::strcmp(aFileAttrByAnchor->mName, aFileAttrByNode->mName) != 0 ) ||
         ( aFileAttrByAnchor->mIsAutoExtend != aFileAttrByNode->mIsAutoExtend ) ||
         ( aFileAttrByAnchor->mNameLength   != idlOS::strlen(aFileAttrByNode->mName) ) ||
         ( aFileAttrByAnchor->mState        != aFileAttrByNode->mState ) ||
         ( aFileAttrByAnchor->mMaxSize      != aFileAttrByNode->mMaxSize ) ||
         ( aFileAttrByAnchor->mNextSize     != aFileAttrByNode->mNextSize ) ||
         ( aFileAttrByAnchor->mCurrSize     != aFileAttrByNode->mCurrSize ) )
    {
        sIsEqual = ID_FALSE;
    }
    else
    {
        /* nothing to do */
    }

    // ���� �� �����Ѱ�, ���̰� �� �� ����.
    if ( ( aFileAttrByAnchor->mAttrType   != aFileAttrByNode->mAttrType ) ||
         ( aFileAttrByAnchor->mSpaceID    != aFileAttrByNode->mSpaceID ) ||
         ( aFileAttrByAnchor->mID         != aFileAttrByNode->mID ) ||
         ( aFileAttrByAnchor->mInitSize   != aFileAttrByNode->mInitSize ) ||
         ( aFileAttrByAnchor->mCreateMode != aFileAttrByNode->mCreateMode ) ||
         ( smrCompareLSN::isEQ( &(aFileAttrByAnchor->mCreateLSN),
                               &(aFileAttrByNode->mCreateLSN) ) != ID_TRUE ) )
    {
        sIsValid = ID_FALSE;
    }
    else
    {
        /* nothing to do */
    }

    // �� ������ �˻�
    if ( ( aFileAttrByNode->mAttrType   != SMI_DBF_ATTR ) ||
         ( aFileAttrByNode->mSpaceID    >= SC_MAX_SPACE_COUNT ) ||
         ( aFileAttrByNode->mID         >= SD_MAX_FID_COUNT ) ||
         ( aFileAttrByNode->mCreateMode >= SMI_DATAFILE_CREATE_MODE_MAX ) ||
         ( aFileAttrByNode->mNameLength >  SMI_MAX_DATAFILE_NAME_LEN ) ||
         ( aFileAttrByNode->mState      >  SMI_DATAFILE_STATE_MAX ) ||
         ( ( aFileAttrByNode->mIsAutoExtend != ID_TRUE ) &&
           ( aFileAttrByNode->mIsAutoExtend != ID_FALSE ) ) )
    {
        sIsValid = ID_FALSE;
    }
    else
    {
        /* nothing to do */
    }

    if ( ( sIsEqual != ID_TRUE ) ||
         ( sIsValid != ID_TRUE ) )
    {
        ideLog::log( SM_TRC_LOG_LEVEL_MRECOV,
                     SM_TRC_ANCHOR_CHECK_DBFILE_ATTR_BY_NODE,
                     aFileAttrByNode->mAttrType,
                     aFileAttrByNode->mSpaceID,
                     aFileAttrByNode->mID,
                     aFileAttrByNode->mName,
                     idlOS::strlen(aFileAttrByNode->mName),
                     aFileAttrByNode->mIsAutoExtend,
                     aFileAttrByNode->mState,
                     aFileAttrByNode->mMaxSize,
                     aFileAttrByNode->mNextSize,
                     aFileAttrByNode->mCurrSize,
                     aFileAttrByNode->mInitSize,
                     aFileAttrByNode->mCreateMode,
                     aFileAttrByNode->mCreateLSN.mFileNo ,
                     aFileAttrByNode->mCreateLSN.mOffset );


        ideLog::log( SM_TRC_LOG_LEVEL_MRECOV,
                     SM_TRC_ANCHOR_CHECK_DBFILE_ATTR_BY_ANCHOR,
                     aFileAttrByAnchor->mAttrType,
                     aFileAttrByAnchor->mSpaceID,
                     aFileAttrByAnchor->mID,
                     aFileAttrByAnchor->mName,
                     aFileAttrByAnchor->mNameLength,
                     aFileAttrByAnchor->mIsAutoExtend,
                     aFileAttrByAnchor->mState,
                     aFileAttrByAnchor->mMaxSize,
                     aFileAttrByAnchor->mNextSize,
                     aFileAttrByAnchor->mCurrSize,
                     aFileAttrByAnchor->mInitSize,
                     aFileAttrByAnchor->mCreateMode,
                     aFileAttrByAnchor->mCreateLSN.mFileNo,
                     aFileAttrByAnchor->mCreateLSN.mOffset );
    }
    else
    {
        /* nothing to do */
    }

    return sIsValid;
}

/***********************************************************************
 * Description : Memory TBS�� Checkpoint Image Node��
 *               LogAnchor Buffer�� ���Ѵ�.
 *
 *  aFileAttrByNode   - [IN] Checkpoint Image Node���� ������ Attribute
 *  aFileAttrByAnchor - [IN] Anchor Buffer���� Attribute
 **********************************************************************/
idBool smrLogAnchorMgr::cmpChkptImageAttr( smmChkptImageAttr*   aImageAttrByNode,
                                           smmChkptImageAttr*   aImageAttrByAnchor )
{
    idBool sIsValid = ID_TRUE;
    idBool sIsEqual = ID_TRUE;

    // ���� �����Ѱ�, �Ͻ������� ���̰� �� �� �ִ�. �α׸� ���
    if ( ( aImageAttrByAnchor->mCreateDBFileOnDisk[0]
           != aImageAttrByNode->mCreateDBFileOnDisk[0] ) ||
         ( aImageAttrByAnchor->mCreateDBFileOnDisk[1]
           != aImageAttrByNode->mCreateDBFileOnDisk[1] ) ||
         ( aImageAttrByAnchor->mFileNum != aImageAttrByNode->mFileNum ) )
    {
        sIsEqual = ID_FALSE;
    }

    // ���� �� �����Ѱ�, ���̰� �� �� ����.
    if ( ( aImageAttrByAnchor->mAttrType != aImageAttrByNode->mAttrType ) ||
         ( aImageAttrByAnchor->mSpaceID  != aImageAttrByNode->mSpaceID ) )
    {
        sIsValid = ID_FALSE;
    }
    else
    {
        /* nothing to do */
    }

    // �� ������ �˻�
    if ( ( aImageAttrByNode->mSpaceID  >= SC_MAX_SPACE_COUNT ) ||
         ( aImageAttrByNode->mAttrType != SMI_CHKPTIMG_ATTR )  ||
         ( ( aImageAttrByNode->mCreateDBFileOnDisk[0] != ID_TRUE ) &&
           ( aImageAttrByNode->mCreateDBFileOnDisk[0] != ID_FALSE ) ) ||
         ( ( aImageAttrByNode->mCreateDBFileOnDisk[1] != ID_TRUE ) &&
           ( aImageAttrByNode->mCreateDBFileOnDisk[1] != ID_FALSE ) ) )
    {
        sIsValid = ID_FALSE;
    }
    else
    {
        /* nothing to do */
    }

    // SMI_MAX_DATAFILE_NAME_LEN
    if ( ( sIsEqual != ID_TRUE ) ||
         ( sIsValid != ID_TRUE ) )
    {
        ideLog::log(SM_TRC_LOG_LEVEL_MRECOV,
                    SM_TRC_ANCHOR_CHECK_CHKPT_IMG_ATTR,
                    aImageAttrByNode->mSpaceID,
                    aImageAttrByNode->mAttrType,
                    aImageAttrByNode->mFileNum,
                    aImageAttrByNode->mCreateDBFileOnDisk[0],
                    aImageAttrByNode->mCreateDBFileOnDisk[1],
                    aImageAttrByAnchor->mSpaceID,
                    aImageAttrByAnchor->mAttrType,
                    aImageAttrByAnchor->mFileNum,
                    aImageAttrByAnchor->mCreateDBFileOnDisk[0],
                    aImageAttrByAnchor->mCreateDBFileOnDisk[1]);
    }
    else
    {
        /* nothing to do */
    }

    if ( smrCompareLSN::isEQ( &(aImageAttrByAnchor->mMemCreateLSN),
                              &(aImageAttrByNode->mMemCreateLSN) )
         != ID_TRUE )
    {
        ideLog::log( SM_TRC_LOG_LEVEL_MRECOV,
                     SM_TRC_ANCHOR_CHECK_CRT_LSN_IN_CHKPT_IMG_ATTR,
                     aImageAttrByNode->mSpaceID,
                     aImageAttrByNode->mMemCreateLSN.mFileNo,
                     aImageAttrByNode->mMemCreateLSN.mOffset,
                     aImageAttrByAnchor->mSpaceID,
                     aImageAttrByAnchor->mMemCreateLSN.mFileNo,
                     aImageAttrByAnchor->mMemCreateLSN.mOffset );

        sIsValid = ID_FALSE;
    }
    else
    {
        /* nothing to do ... */
    }

    return sIsValid;
}


/***********************************************************************
 * Description : Secondary Buffer�� File Node�� LogAnchor Buffer�� ���Ѵ�.
 *
 *  aFileAttrByNode   - [IN] DBFile Node���� ������ Attribute
 *  aFileAttrByAnchor - [IN] Anchor Buffer���� Attribute
 **********************************************************************/
idBool smrLogAnchorMgr::cmpSBufferFileAttr( smiSBufferFileAttr*   aFileAttrByNode,
                                            smiSBufferFileAttr*   aFileAttrByAnchor )
{
    idBool sIsValid = ID_TRUE;

    IDE_DASSERT( aFileAttrByNode != NULL );
    IDE_DASSERT( aFileAttrByAnchor != NULL );

    if ( ( aFileAttrByAnchor->mAttrType     != aFileAttrByNode->mAttrType )            ||
        ( idlOS::strcmp(aFileAttrByAnchor->mName, aFileAttrByNode->mName) != 0 )      ||
        ( aFileAttrByAnchor->mNameLength   != idlOS::strlen(aFileAttrByNode->mName) ) ||
        ( aFileAttrByAnchor->mPageCnt      != aFileAttrByNode->mPageCnt )             ||
        ( smrCompareLSN::isEQ( &(aFileAttrByAnchor->mCreateLSN),                     
                               &(aFileAttrByNode->mCreateLSN) ) != ID_TRUE ) )
    {
        sIsValid = ID_FALSE;
    }
    else
    {
        /* nothing to do */
    }

    if ( ( aFileAttrByNode->mAttrType   != SMI_SBUFFER_ATTR )          ||
         ( aFileAttrByNode->mNameLength >  SMI_MAX_DATAFILE_NAME_LEN ) ||
         ( aFileAttrByNode->mState      >  SMI_DATAFILE_STATE_MAX ) ) 
    {
        sIsValid = ID_FALSE;
    }
    else
    {
        /* nothing to do */
    }

    if ( sIsValid != ID_TRUE )
    {
        ideLog::log( SM_TRC_LOG_LEVEL_MRECOV,
                     SM_TRC_ANCHOR_CHECK_SBFILE_ATTR_BY_NODE,
                     aFileAttrByNode->mAttrType,
                     aFileAttrByNode->mName,
                     idlOS::strlen(aFileAttrByNode->mName),
                     aFileAttrByNode->mPageCnt,
                     aFileAttrByNode->mState,
                     aFileAttrByNode->mCreateLSN.mFileNo,
                     aFileAttrByNode->mCreateLSN.mOffset );


        ideLog::log( SM_TRC_LOG_LEVEL_MRECOV,
                     SM_TRC_ANCHOR_CHECK_SBFILE_ATTR_BY_ANCHOR,
                     aFileAttrByNode->mAttrType,
                     aFileAttrByNode->mName,
                     idlOS::strlen(aFileAttrByNode->mName),
                     aFileAttrByNode->mPageCnt,
                     aFileAttrByNode->mState,
                     aFileAttrByNode->mCreateLSN.mFileNo ,
                     aFileAttrByNode->mCreateLSN.mOffset );
    }
    else
    {
        /* nothing to do */
    }

    return sIsValid;
}

// fix BUG-20241
/***********************************************************************
 * Description : loganchor�� FstDeleteFile ���� ���� �� flush
 *
 ***********************************************************************/
IDE_RC smrLogAnchorMgr::updateFstDeleteFileAndFlush()
{
    UInt sState = 0;
    UInt sDelLogCnt = 0;
    

    IDE_TEST( lock() != IDE_SUCCESS );
    sState = 1;

    /* BUG-39289 : �α׾�Ŀ�� delete logfile range�� N+1 ~ N���� ��µǴ� ����.
       ������ �α׾�Ŀ�� FST(mFstDeleteFileNo)�� LST(mLstDeleteFileNo)
       �� ������Ʈ �ϴ� ���� �α׾�Ŀ�� FST�� LST + ������ �α� �� ��
       ������Ʈ �Ѵ�. �α׾�Ŀ�� delete logfile range ��½� ���� ������
       �α� ������ ����ϱ� ����.*/
    /* BUG-40323 �α������� ������ �ʰ�, �� �Լ��� ȣ���ϸ� �ȵȴ�.
     * BUG-39289 �������� Fst >= Lst ���� ������ �ϴµ�,
     * ���ο� �α������� ����, �� Lst ���� �������� �ʰ� �ٽ� ȣ��Ǹ�
     * Fst <= Lst�� ������ �� �ִ�. */

    sDelLogCnt = mLogAnchor->mLstDeleteFileNo - mLogAnchor->mFstDeleteFileNo;

    mLogAnchor->mFstDeleteFileNo = mLogAnchor->mLstDeleteFileNo + sDelLogCnt;

    IDE_TEST( flushAll() != IDE_SUCCESS );

    sState = 0;
    IDE_TEST( unlock() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sState != 0 )
    {
        IDE_ASSERT( unlock() == IDE_SUCCESS );
    }

    return IDE_FAILURE;

}

/*************************************************************************************
 * Description : �������� ������ �α� ���� ��ȣ�� loganchor�� ������Ʈ�Ѵ�.(BUG-39764)
 *               
 * [IN] aLstCrtFileNo : �������� ������ �α� ���� ��ȣ
 *************************************************************************************/
IDE_RC smrLogAnchorMgr::updateLastCreatedLogFileNumAndFlush( UInt aLstCrtFileNo )
{
    UInt sState = 0;

    IDE_TEST( lock() != IDE_SUCCESS );
    sState = 1;

    mLogAnchor->mLstCreatedLogFileNo = aLstCrtFileNo;

    IDE_TEST( flushAll() != IDE_SUCCESS );
    
    sState = 0;
    IDE_TEST( unlock() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sState != 0 )
    {
        IDE_ASSERT( unlock() == IDE_SUCCESS );
    }

    return IDE_FAILURE;

}

/***********************************************************************
 * Description : loganchor ���۸� loganchor ���Ͽ� flush
 *
 * mBuffer�� ��ϵ� loganchor�� ��� loganchor ���ϵ鿡
 * ��� flush�Ѵ�.
 *
 * - flush�� loganchor�� ������ �����ϴ� mBuffer��
 *   checksum�� ����Ͽ� mBuffer�� �ش� �κ��� ����
 *   + MUST : checksum�� ������ ������ loganchor ��������
 *            ��� �����Ǿ� �־�� �Ѵ�.
 * - ��� loganchor���Ͽ� ���Ͽ� mBuffer�� flush ��
 ***********************************************************************/
IDE_RC smrLogAnchorMgr::flushAll()
{
    UInt      i;
    ULong     sFileSize = 0;
#ifdef DEBUG
    idBool    sIsValidLogAnchor;
#endif
    UInt      sState = 0;

    /* PROJ-2162 RestartRiskReduction
     * DB�� �������϶����� LogAnchor�� �����Ѵ�. */
    if ( ( smrRecoveryMgr::getConsistency() == ID_TRUE ) || 
         ( smuProperty::getCrashTolerance() == 2 ) )
    {
        // BUG-23136 LogAnchorBuffer�� �� node�� �� �˻� �Ͽ�
        // �߸��� ���� LogAnchor File�� ��ϵǴ� ���� �����Ѵ�.
        // XXX Tablespace�� ���� ���ü� ��� ������� �ʴ� ������ �־�
        // release������ ��� ��ϸ� �ϰ� DASSERT�� ó���Ѵ�.
        // ���� ���ü� ��� ����Ǹ� �ٷ� ASSERT�ϵ��� �����ؾ� �Ѵ�.
#ifdef DEBUG
        if ( mIsProcessPhase == ID_FALSE )
        {
            sIsValidLogAnchor = checkLogAnchorBuffer() ;
            IDE_DASSERT( sIsValidLogAnchor == ID_TRUE );
        }
#endif
        /* ------------------------------------------------
         * updateAndFlushCount�� checksum ����Ͽ� mBuffer�� �����Ѵ�.
         * ----------------------------------------------*/
        mLogAnchor->mUpdateAndFlushCount++;
        mLogAnchor->mCheckSum = makeCheckSum( mBuffer, mWriteOffset );

        /* ----------------------------------------------------------------
         * In case createdb, SMU_TRANSACTION_DURABILITY_LEVEL is always 3.
         * ---------------------------------------------------------------- */

        /* ------------------------------------------------
         * ���� ��� loganchor ���Ͽ� mBuffer�� ����ϰ� sync�Ѵ�.
         * ----------------------------------------------*/
        for ( i = 0 ; i < SM_LOGANCHOR_FILE_COUNT ; i++ )
        {
            /* ------------------------------------------------
             * [BUG-24236] ��� �α� ��Ŀ ������ ���� ����� �ʿ��մϴ�.
             * - �α� ��Ŀ ������ �����Ǿ��ٸ� �ٽ� �����Ѵ�.
             * ----------------------------------------------*/
            if ( mFile[i].open() != IDE_SUCCESS )
            {
                ideLog::log( IDE_SM_0, SM_TRC_MEMORY_LOGANCHOR_RECREATE );

                IDE_TEST( mFile[i].createUntilSuccess( smLayerCallback::setEmergency )
                          != IDE_SUCCESS );

                /* ------------------------------------------------
                 * �׷��� �����ϸ� IDE_FAILURE
                 * ----------------------------------------------*/
                IDE_TEST( mFile[i].open() != IDE_SUCCESS );
            }
            sState = 1;

            IDE_TEST( mFile[i].writeUntilSuccess(
                                            NULL, /* idvSQL* */
                                            0,
                                            mBuffer,
                                            mWriteOffset,
                                            smLayerCallback::setEmergency,
                                            smrRecoveryMgr::isFinish())
                      != IDE_SUCCESS );

            IDE_TEST( mFile[i].syncUntilSuccess(
                                            smLayerCallback::setEmergency)
                      != IDE_SUCCESS );

            IDE_TEST( mFile[i].getFileSize( &sFileSize ) != IDE_SUCCESS );

            if ( (SInt)( mWriteOffset - (UInt)sFileSize ) < 0 )
            {
                IDE_TEST( mFile[i].truncate( mWriteOffset )
                          != IDE_SUCCESS );
            }

            sState = 0;
            /* ------------------------------------------------
             * [BUG-24236] ��� �α� ��Ŀ ������ ���� ����� �ʿ��մϴ�.
             * ----------------------------------------------*/
            IDE_ASSERT( mFile[i].close() == IDE_SUCCESS );
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sState == 1 )
    {
        IDE_ASSERT( mFile[i].close() == IDE_SUCCESS );
    }
    
    return IDE_FAILURE;

}

/***********************************************************************
 * Description :  Check LogAnchor Dir Exist
 **********************************************************************/
IDE_RC smrLogAnchorMgr::checkLogAnchorDirExist()
{
    SChar    sLogAnchorDir[SM_MAX_FILE_NAME];
    SInt     i;

    for( i = 0 ; i < SM_LOGANCHOR_FILE_COUNT ; i++ )
    {
        idlOS::snprintf( sLogAnchorDir,
                         SM_MAX_FILE_NAME,
                         "%s",
                         smuProperty::getLogAnchorDir(i) );

        IDE_TEST_RAISE( idf::access(sLogAnchorDir, F_OK) != 0,
                        err_loganchordir_not_exist )
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_loganchordir_not_exist);
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_NoExistPath, sLogAnchorDir));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 * Description :  read loganchor header
 *
 * [IN]  aLogAnchorFile  : �α׾�Ŀ���� ��ü
 * [IN]  aCurOffset      : �α����ϻ󿡼� ���� �Ӽ��� ������
 * [OUT] aCurOffset      : �α����ϻ󿡼� ���� �Ӽ��� ������
 * [OUT] aHeader         : �α׾�Ŀ ��������
 **********************************************************************/
IDE_RC smrLogAnchorMgr::readLogAnchorHeader( iduFile *      aLogAnchorFile,
                                             UInt *         aCurOffset,
                                             smrLogAnchor * aHeader )
{
    IDE_DASSERT(*aCurOffset == 0);

    IDE_TEST( aLogAnchorFile->read( NULL, /* idvSQL* */
                                    *aCurOffset,
                                    (SChar*)aHeader,
                                    (UInt)ID_SIZEOF(smrLogAnchor) )
              != IDE_SUCCESS );

    *aCurOffset = ID_SIZEOF(smrLogAnchor);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 * Description :  read one tablespace attribute
 *
 * [IN]  aLogAnchorFile  : �α׾�Ŀ���� ��ü
 * [IN]  aCurOffset      : �α����ϻ󿡼� ���� �Ӽ��� ������
 * [OUT] aCurOffset      : �α����ϻ󿡼� ���� �Ӽ��� ������
 * [OUT] aTBSAttr        : Disk/Memory Tablespace Attribute
 **********************************************************************/
IDE_RC smrLogAnchorMgr::readTBSNodeAttr(
                                   iduFile *           aLogAnchorFile,
                                   UInt *              aCurOffset,
                                   smiTableSpaceAttr * aTBSAttr )
{
    IDE_DASSERT( aLogAnchorFile != NULL );
    IDE_DASSERT( aCurOffset != NULL );
    IDE_DASSERT( aTBSAttr != NULL );

    IDE_TEST( aLogAnchorFile->read( NULL, /* idvSQL* */
                                    *aCurOffset,
                                    (SChar*)aTBSAttr,
                                    ID_SIZEOF(smiTableSpaceAttr) )
             != IDE_SUCCESS );

    *aCurOffset += (UInt)ID_SIZEOF(smiTableSpaceAttr);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description :  read one datafile attribute
 *
 * [IN]  aLogAnchorFile  : �α׾�Ŀ���� ��ü
 * [IN]  aCurOffset      : �α����ϻ󿡼� ���� �Ӽ��� ������
 * [OUT] aCurOffset      : �α����ϻ󿡼� ���� �Ӽ��� ������
 * [OUT] aFileAttr       : Disk Datafile Attribute
 **********************************************************************/
IDE_RC smrLogAnchorMgr::readDBFNodeAttr(
                               iduFile *           aLogAnchorFile,
                               UInt *              aCurOffset,
                               smiDataFileAttr *   aFileAttr )
{
    IDE_DASSERT( aLogAnchorFile != NULL );
    IDE_DASSERT( aCurOffset != NULL );
    IDE_DASSERT( aFileAttr != NULL );

    IDE_TEST( aLogAnchorFile->read( NULL, /* idvSQL* */
                                    *aCurOffset,
                                    (SChar*)aFileAttr,
                                    ID_SIZEOF(smiDataFileAttr) )
             != IDE_SUCCESS );

    *aCurOffset += (UInt)ID_SIZEOF(smiDataFileAttr);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description :  read one checkpoint path attribute
 *
 * [IN]  aLogAnchorFile  : �α׾�Ŀ���� ��ü
 * [IN]  aCurOffset      : �α����ϻ󿡼� ���� �Ӽ��� ������
 * [OUT] aCurOffset      : �α����ϻ󿡼� ���� �Ӽ��� ������
 * [OUT] aChkptImageAttr : Checkpoint Path Attribute
 **********************************************************************/
IDE_RC smrLogAnchorMgr::readChkptPathNodeAttr(
                                     iduFile *          aLogAnchorFile,
                                     UInt *             aCurOffset,
                                     smiChkptPathAttr * aChkptPathAttr )
{
    IDE_DASSERT( aLogAnchorFile != NULL );
    IDE_DASSERT( aCurOffset != NULL );
    IDE_DASSERT( aChkptPathAttr != NULL );

    IDE_TEST( aLogAnchorFile->read( NULL, /* idvSQL* */
                                    *aCurOffset,
                                    (SChar*)aChkptPathAttr,
                                    ID_SIZEOF(smiChkptPathAttr) )
             != IDE_SUCCESS );

    *aCurOffset += (UInt)ID_SIZEOF(smiChkptPathAttr);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description :  read one checkpoint image attribute
 *
 * [IN]  aLogAnchorFile  : �α׾�Ŀ���� ��ü
 * [IN]  aCurOffset      : �α����ϻ󿡼� ���� �Ӽ��� ������
 * [OUT] aCurOffset      : �α����ϻ󿡼� ���� �Ӽ��� ������
 * [OUT] aChkptImageAttr : Checkpoint Image Attribute
 **********************************************************************/
IDE_RC smrLogAnchorMgr::readChkptImageAttr(
                                   iduFile *           aLogAnchorFile,
                                   UInt *              aCurOffset,
                                   smmChkptImageAttr * aChkptImageAttr )
{
    IDE_DASSERT( aLogAnchorFile != NULL );
    IDE_DASSERT( aCurOffset != NULL );
    IDE_DASSERT( aChkptImageAttr != NULL );

    IDE_TEST( aLogAnchorFile->read( NULL, /* idvSQL* */
                                    *aCurOffset,
                                    (SChar*)aChkptImageAttr,
                                    ID_SIZEOF( smmChkptImageAttr ) )
              != IDE_SUCCESS );

    IDE_ASSERT( smmManager::isCreateDBFileAtLeastOne(
                                    aChkptImageAttr->mCreateDBFileOnDisk )
                == ID_TRUE );

    *aCurOffset += (UInt)ID_SIZEOF(smmChkptImageAttr);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 * Description :  read one SBufferFile attribute
 *
 * [IN]  aLogAnchorFile  : �α׾�Ŀ���� ��ü
 * [IN]  aCurOffset      : �α����ϻ󿡼� ���� �Ӽ��� ������
 * [OUT] aCurOffset      : �α����ϻ󿡼� ���� �Ӽ��� ������
 * [OUT] aFileAttr       : Disk Datafile Attribute
 **********************************************************************/
IDE_RC smrLogAnchorMgr::readSBufferFileAttr(
                               iduFile              * aLogAnchorFile,
                               UInt                 * aCurOffset,
                               smiSBufferFileAttr   * aFileAttr )
{
    IDE_DASSERT( aLogAnchorFile != NULL );
    IDE_DASSERT( aCurOffset != NULL );
    IDE_DASSERT( aFileAttr != NULL );

    IDE_TEST( aLogAnchorFile->read( NULL, /* idvSQL* */
                                    *aCurOffset,
                                    (SChar*)aFileAttr,
                                    ID_SIZEOF(smiSBufferFileAttr) )
            != IDE_SUCCESS ); 

    *aCurOffset += (UInt)ID_SIZEOF(smiSBufferFileAttr);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : loganchor ���۸� resize��
 **********************************************************************/
IDE_RC smrLogAnchorMgr::resizeBuffer( UInt aBufferSize )
{
    UChar * sBuffer;
    UInt    sState  = 0;

    IDE_DASSERT( aBufferSize >= idlOS::align((UInt)ID_SIZEOF(smrLogAnchorMgr)));

    sBuffer = NULL;

    /* TC/FIT/Limit/sm/smr/smrLogAnchorMgr_resizeBuffer_malloc.tc */
    IDU_FIT_POINT_RAISE( "smrLogAnchorMgr::resizeBuffer::malloc",
                          insufficient_memory );

    IDE_TEST_RAISE( iduMemMgr::malloc( IDU_MEM_SM_SMR,
                                       aBufferSize,
                                       (void**)&sBuffer ) != IDE_SUCCESS,
                    insufficient_memory );
    sState = 1;

    IDU_FIT_POINT_RAISE( "1.BUG-30024@smrLogAnchorMgr::resizeBuffer", 
                          insufficient_memory );

    idlOS::memset( sBuffer, 0x00, aBufferSize );

    if ( mWriteOffset != 0 )
    {
        /* ------------------------------------------------
         * loganchor ���۰� ������϶� ��ϵ� ������
         * ���ο� ���ۿ� memcpy �Ѵ�.
         * ----------------------------------------------*/
        idlOS::memcpy( sBuffer, mBuffer, mWriteOffset );
    }
    else
    {
        /* nothing to do */
    }

    // To Fix BUG-18268
    //     disk tablespace create/alter/drop ���� checkpoint�ϴٰ� ���
    //
    // mBuffer�� �����ϱ� ���� mLogAnchor�� ���� �Ҵ��� �޸𸮸�
    // ����Ű���� ����
    mLogAnchor = (smrLogAnchor*)sBuffer;

    IDE_ASSERT( iduMemMgr::free( mBuffer ) == IDE_SUCCESS );
    mBuffer = sBuffer;

    return IDE_SUCCESS;

    IDE_EXCEPTION( insufficient_memory );
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_InsufficientMemory));
    }
    IDE_EXCEPTION_END;

    switch ( sState )
    {
        case 1:
            IDE_ASSERT( iduMemMgr::free( sBuffer ) == IDE_SUCCESS );
            sBuffer = NULL;
        default:
            break;
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : loganchor ���۸� ������
 **********************************************************************/
IDE_RC smrLogAnchorMgr::freeBuffer()
{
    IDE_TEST( iduMemMgr::free( mBuffer) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description: loganchor ���ۿ� ���
 **********************************************************************/
IDE_RC smrLogAnchorMgr::writeToBuffer( void*  aBuffer,
                                       UInt   aWriteSize )
{
    UInt sBufferSize;

    IDE_DASSERT( aBuffer != NULL );
    IDE_DASSERT( aWriteSize > 0 );

    /* To fix BUG-30024 : resizeBuffer() ���� malloc���н� �����������մϴ�. */
    if ( ( mWriteOffset + aWriteSize ) >= mBufferSize )
    {
        sBufferSize = idlOS::align( (mWriteOffset + aWriteSize), SM_PAGE_SIZE);

        if ( sBufferSize == ( mWriteOffset + aWriteSize) )
        {
            sBufferSize += SM_PAGE_SIZE;
        }
        else
        {
            /* nothing to do */
        }

        IDE_TEST( resizeBuffer( sBufferSize ) != IDE_SUCCESS );
        mBufferSize = sBufferSize;
    }
    else
    {
        /* nothing to do */
    }

    idlOS::memcpy( mBuffer + mWriteOffset, 
                   aBuffer, 
                   aWriteSize );
    mWriteOffset += aWriteSize;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/*
   PRJ-1548 User Memory Tablespace
   Loganchor �޸𸮹����� Ư�������¿��� �־��� ���̸�ŭ
   UPDATE �Ѵ�.
 */
IDE_RC smrLogAnchorMgr::updateToBuffer( void      * aBuffer,
                                        UInt        aOffset,
                                        UInt        aWriteSize )
{
    IDE_DASSERT( aBuffer != NULL );

    IDE_ASSERT( ( aOffset + aWriteSize ) <= mWriteOffset );

    // Tablespace�� DBF Node�� Update�ϴٰ�
    // Log Anchor�� ���ۺκп� ��ϵǴ� �������� ������
    // smrLogAnchor����ü�� �����
    // �ý����� ���������� ������ �Ұ���������.
    // ASSERT�� �̸� Ȯ���Ѵ�.
    IDE_ASSERT( aOffset >= ID_SIZEOF(smrLogAnchor) );

    idlOS::memcpy( mBuffer + aOffset, 
                   aBuffer, 
                   aWriteSize);

    return IDE_SUCCESS;
}

/*
   PROJ-2133 incremental backup
   Change tracking attr�� �����Ѵ�.
 */
IDE_RC smrLogAnchorMgr::updateCTFileAttr( SChar          * aCTFileName, 
                                          smriCTMgrState * aCTMgrState, 
                                          smLSN          * aChkptLSN )
{
    
    IDE_TEST( lock() != IDE_SUCCESS );

    if ( aCTFileName != NULL )
    {
        idlOS::memcpy( mLogAnchor->mCTFileAttr.mCTFileName, 
                       aCTFileName, 
                       SM_MAX_FILE_NAME );
    }
    else
    {
        /* nothing to do */
    }
    
    if ( aCTMgrState != NULL )
    {
        mLogAnchor->mCTFileAttr.mCTMgrState = *aCTMgrState;
    }
    else
    {
        /* nothing to do */
    }

    if ( aChkptLSN != NULL )
    {
        mLogAnchor->mCTFileAttr.mLastFlushLSN.mFileNo = aChkptLSN->mFileNo;
        mLogAnchor->mCTFileAttr.mLastFlushLSN.mOffset = aChkptLSN->mOffset;
    }
    else
    {
        /* nothing to do */
    }

    IDE_TEST( flushAll() != IDE_SUCCESS );

    IDE_TEST( unlock() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
   PROJ-2133 incremental backup
   Backup Info attr�� �����Ѵ�.
 */
IDE_RC smrLogAnchorMgr::updateBIFileAttr( SChar          * aBIFileName, 
                                          smriBIMgrState * aBIMgrState, 
                                          smLSN          * aBackupLSN,
                                          SChar          * aBackupDir,
                                          UInt           * aDeleteArchLogFileNo )
{

    IDE_TEST( lock() != IDE_SUCCESS );

    if ( aBIFileName != NULL )
    {
        idlOS::memcpy( mLogAnchor->mBIFileAttr.mBIFileName, 
                       aBIFileName, 
                       SM_MAX_FILE_NAME);
    }
    else
    {
        /* nothing to do */
    }
    
    if ( aBIMgrState != NULL )
    {
        mLogAnchor->mBIFileAttr.mBIMgrState = *aBIMgrState;
    }
    else
    {
        /* nothing to do */
    }

    if ( aBackupLSN != NULL )
    {
        mLogAnchor->mBIFileAttr.mBeforeBackupLSN.mFileNo = 
                                mLogAnchor->mBIFileAttr.mLastBackupLSN.mFileNo;
        mLogAnchor->mBIFileAttr.mBeforeBackupLSN.mOffset = 
                                mLogAnchor->mBIFileAttr.mLastBackupLSN.mOffset;

        mLogAnchor->mBIFileAttr.mLastBackupLSN.mFileNo = aBackupLSN->mFileNo;
        mLogAnchor->mBIFileAttr.mLastBackupLSN.mOffset = aBackupLSN->mOffset;
    }
    else
    {
        /* nothing to do */
    }

    if ( aBackupDir != NULL )
    {
        idlOS::memcpy( mLogAnchor->mBIFileAttr.mBackupDir, 
                       aBackupDir, 
                       SM_MAX_FILE_NAME );
    }
    else
    {
        /* nothing to do */
    }

    if ( aDeleteArchLogFileNo != NULL )
    {
        mLogAnchor->mBIFileAttr.mDeleteArchLogFileNo = *aDeleteArchLogFileNo;
    }
    else
    {
        /* nothing to do */
    }

    IDE_TEST( flushAll() != IDE_SUCCESS );

    IDE_TEST( unlock() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC smrLogAnchorMgr::initialize4ProcessPhase()
{
    UInt              i;
    UInt              sWhich;
    UInt              sState    = 0;
    ULong             sFileSize = 0;
    smrLogAnchor      sLogAnchor;
    SChar             sAnchorFileName[SM_MAX_FILE_NAME];
    UInt              sFileState = 0;

    // LOGANCHOR_DIR Ȯ��
    IDE_TEST( checkLogAnchorDirExist() != IDE_SUCCESS );

    for ( i = 0 ; i < SM_LOGANCHOR_FILE_COUNT ; i++ )
    {
        idlOS::snprintf( sAnchorFileName, SM_MAX_FILE_NAME,
                         "%s%c%s%"ID_UINT32_FMT"",
                         smuProperty::getLogAnchorDir(i),
                         IDL_FILE_SEPARATOR,
                         SMR_LOGANCHOR_NAME,
                         i );

        // Loganchor ���� ���� ���� Ȯ��
        IDE_TEST_RAISE( idf::access( sAnchorFileName, F_OK|W_OK|R_OK ) != 0,
                        error_file_not_exist );
    }

    IDE_TEST_RAISE( mMutex.initialize( (SChar*)"LOGANCHOR_MGR_MUTEX",
                                       IDU_MUTEX_KIND_NATIVE,
                                       IDV_WAIT_INDEX_NULL )
                    != IDE_SUCCESS,
                    error_mutex_init );

    idlOS::memset( &mTableSpaceAttr,
                   0x00,
                   ID_SIZEOF(smiTableSpaceAttr) );

    idlOS::memset( &mDataFileAttr,
                   0x00,
                   ID_SIZEOF(smiDataFileAttr) );

    // �޸� ����(mBuffer)�� SM_PAGE_SIZE ũ��� �Ҵ� �� �ʱ�ȭ
    IDE_TEST( allocBuffer( SM_PAGE_SIZE ) != IDE_SUCCESS );
    sState = 1;

    // ��ȿ�� loganchor������ �����Ѵ�.
    IDE_TEST( checkAndGetValidAnchorNo( &sWhich ) != IDE_SUCCESS );

    // ��� loganchor ������ �����Ѵ�.
    for ( i = 0 ; i < SM_LOGANCHOR_FILE_COUNT ; i++ )
    {
        idlOS::snprintf( sAnchorFileName, SM_MAX_FILE_NAME,
                         "%s%c%s%"ID_UINT32_FMT"",
                         smuProperty::getLogAnchorDir(i),
                         IDL_FILE_SEPARATOR,
                         SMR_LOGANCHOR_NAME,
                         i );
        IDE_TEST( mFile[i].initialize( IDU_MEM_SM_SMR,
                                       1, /* Max Open FD Count */
                                       IDU_FIO_STAT_OFF,
                                       IDV_WAIT_INDEX_NULL )
                  != IDE_SUCCESS );
        IDE_TEST( mFile[i].setFileName( sAnchorFileName ) != IDE_SUCCESS );
        IDE_TEST( mFile[i].open() != IDE_SUCCESS );
        sFileState++;
    }

    // �޸𸮹��� ������ �ʱ�ȭ
    initBuffer();

    /*
       PRJ-1548 �������� ������ �޸� ���ۿ� �ε��ϴ� �ܰ�
    */

    IDE_TEST( mFile[sWhich].getFileSize( &sFileSize ) != IDE_SUCCESS );

    // CREATE DATABASE �����߿� ���������� �������� ���� ���� ������,
    // ���������� ����Ǿ� �־�� �Ѵ�.
    IDE_ASSERT ( sFileSize >= ID_SIZEOF(smrLogAnchor) );

    // ���Ϸ� ���� Loganchor ���������� �ε��Ѵ�.
    IDE_TEST( mFile[sWhich].read( NULL, /* idvSQL* */
                                  0,
                                  (SChar*)&sLogAnchor,
                                  ID_SIZEOF(smrLogAnchor) )
             != IDE_SUCCESS );

    // �޸� ���ۿ� Loganchor ���������� �ε��Ѵ�.
    IDE_TEST( writeToBuffer( (SChar*)&sLogAnchor,
                             ID_SIZEOF(smrLogAnchor) )
              != IDE_SUCCESS );

    mLogAnchor = (smrLogAnchor*)mBuffer;

    /* ------------------------------------------------
     * 5. ��ȿ�� loganchor ���۸� ��� anchor ���Ͽ� flush �Ѵ�.
     * ----------------------------------------------*/
    IDE_TEST( readLogAnchorToBuffer( sWhich ) != IDE_SUCCESS );

    for ( i = 0 ; i < SM_LOGANCHOR_FILE_COUNT ; i++)
    {
        sFileState--;
        IDE_ASSERT( mFile[i].close() == IDE_SUCCESS );
    }

    mIsProcessPhase = ID_TRUE;

    return IDE_SUCCESS;

    IDE_EXCEPTION( error_file_not_exist );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_InvalidLogAnchorFile));
    }
    IDE_EXCEPTION( error_mutex_init );
    {
        IDE_SET(ideSetErrorCode(smERR_FATAL_ThrMutexInit));
    }
    IDE_EXCEPTION_END;

    while ( sFileState > 0 )
    {
        sFileState--;
        IDE_ASSERT( mFile[sFileState].close() == IDE_SUCCESS ); 
    }
    
    if ( sState != 0 )
    {
        (void)freeBuffer();
    }

    return IDE_FAILURE;
}
