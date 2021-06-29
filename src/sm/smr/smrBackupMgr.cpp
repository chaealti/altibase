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
 * $Id: smrBackupMgr.cpp 86343 2019-11-11 01:28:10Z jiwon.kim $
 *
 * Description :
 *
 * �� ������ ��� �����ڿ� ���� ���������̴�.
 *
 **********************************************************************/

#include <smErrorCode.h>
#include <smDef.h>
#include <smm.h>
#include <smr.h>
#include <smrReq.h>
#include <sdd.h>

//For Read Online Backup Status
iduMutex           smrBackupMgr::mMtxOnlineBackupStatus;
SChar              smrBackupMgr::mOnlineBackupStatus[256];
smrBackupState     smrBackupMgr::mOnlineBackupState;

// PRJ-1548 User Memory Tablespace
UInt               smrBackupMgr::mBeginBackupDiskTBSCount;
UInt               smrBackupMgr::mBeginBackupMemTBSCount;

// PROJ-2133 Incremental Backup
UInt               smrBackupMgr::mBackupBISlotCnt;
SChar              smrBackupMgr::mLastBackupTagName[ SMI_MAX_BACKUP_TAG_NAME_LEN ];


/***********************************************************************
 * Description : ��������� �ʱ�ȭ
 **********************************************************************/
IDE_RC smrBackupMgr::initialize()
{
    IDE_TEST( mMtxOnlineBackupStatus.initialize(
                        (SChar*)"BACKUP_MANAGER_MUTEX",
                        IDU_MUTEX_KIND_NATIVE,
                        IDV_WAIT_INDEX_NULL ) != IDE_SUCCESS );

    idlOS::strcpy(mOnlineBackupStatus, "");

    mOnlineBackupState = SMR_BACKUP_NONE;

   // BACKUP �������� ��ũ ���̺����̽��� ����
    mBeginBackupDiskTBSCount = 0;
    mBeginBackupMemTBSCount  = 0;

    // incremental backup�� �����ϱ��� backupinfo������ slot ������ ����
    mBackupBISlotCnt = SMRI_BI_INVALID_SLOT_CNT;
    idlOS::memset( mLastBackupTagName, 0x00, SMI_MAX_BACKUP_TAG_NAME_LEN );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : ��� ������ ����
 **********************************************************************/
IDE_RC smrBackupMgr::destroy()
{
    IDE_TEST(mMtxOnlineBackupStatus.destroy() != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/*
   ONLINE BACKUP �÷��׿� �����ϰ��� �ϴ� ���°��� �����Ѵ�.

   [IN] aOR : �����ϰ����ϴ� ���°�
*/
void smrBackupMgr::setOnlineBackupStatusOR( UInt  aOR )
{
    IDE_ASSERT( gSmrChkptThread.lock( NULL /* idvSQL* */)
                == IDE_SUCCESS );

    mOnlineBackupState |=  (aOR);

    IDE_ASSERT( gSmrChkptThread.unlock() == IDE_SUCCESS );

    return;
}


/*
   ONLINE BACKUP �÷��׿� �����ϰ��� �ϴ� ���°��� �����Ѵ�.

   [IN] aNOT : �����ϰ����ϴ� ���°�
*/
void smrBackupMgr::setOnlineBackupStatusNOT( UInt aNOT )
{
    IDE_ASSERT( gSmrChkptThread.lock( NULL /* idvSQL* */)
                == IDE_SUCCESS );

    mOnlineBackupState &= ~(aNOT);

    IDE_ASSERT( gSmrChkptThread.unlock() == IDE_SUCCESS );

    return;
}

/***********************************************************************
 * Description : �ش� path���� memory db ���� ���� ����
 * destroydb, onlineBackup��� ȣ��ȴ�.
 **********************************************************************/
IDE_RC smrBackupMgr::unlinkChkptImages( SChar* aPathName,
                                        SChar* aTBSName )
{
    SInt               sRc;
    DIR               *sDIR     = NULL;
    struct  dirent    *sDirEnt  = NULL;
    struct  dirent    *sResDirEnt;
    SInt               sOffset;
    SChar              sFileName[SM_MAX_FILE_NAME];
    SChar              sFullFileName[SM_MAX_FILE_NAME];

    //To fix BUG-4980
    SChar              sPrefix[SM_MAX_FILE_NAME];
    UInt               sLen;

    idlOS::snprintf(sPrefix, SM_MAX_FILE_NAME, "%s", "ALTIBASE_INDEX_");
    sLen = idlOS::strlen(sPrefix);

    /* smrBackupMgr_unlinkChkptImages_malloc_DirEnt.tc */
    IDU_FIT_POINT_RAISE( "smrBackupMgr::unlinkChkptImages::malloc::DirEnt",
                         insufficient_memory );
    IDE_TEST_RAISE( iduMemMgr::malloc( IDU_MEM_SM_SMR,
                                       ID_SIZEOF(struct dirent) + SM_MAX_FILE_NAME,
                                       (void**)&sDirEnt,
                                       IDU_MEM_FORCE ) != IDE_SUCCESS,
                    insufficient_memory );

    /* smrBackupMgr_unlinkChkptImages_opendir.tc */
    IDU_FIT_POINT_RAISE( "smrBackupMgr::unlinkChkptImages::opendir", err_open_dir );
    sDIR = idf::opendir(aPathName);
    IDE_TEST_RAISE(sDIR == NULL, err_open_dir);

    /* For each file in Backup Path, unlink */
    sResDirEnt = NULL;
    errno = 0;

    /* smrBackupMgr_unlinkChkptImages_readdir_r.tc */
    IDU_FIT_POINT_RAISE( "smrBackupMgr::unlinkChkptImages::readdir_r", err_read_dir );
    sRc = idf::readdir_r(sDIR, sDirEnt, &sResDirEnt);
    IDE_TEST_RAISE( (sRc != 0) && (errno != 0), err_read_dir );
    errno = 0;

    while(sResDirEnt != NULL)
    {
        idlOS::strcpy(sFileName, sResDirEnt->d_name);

        if (idlOS::strcmp(sFileName, ".") == 0 ||
            idlOS::strcmp(sFileName, "..") == 0)
        {
            sResDirEnt = NULL;
            errno = 0;
            sRc = idf::readdir_r(sDIR, sDirEnt, &sResDirEnt);
            IDE_TEST_RAISE( (sRc != 0) && (errno != 0), err_read_dir );
            errno = 0;
            continue;
        }

        /* ----------------------------------------------------------------
           [1] unlink backup_version file, loganchor file, db file, log file
           ---------------------------------------------------------------- */
        if (idlOS::strcmp(sFileName, SMR_BACKUP_VER_NAME) == 0 ||
            idlOS::strncmp(sFileName, aTBSName, idlOS::strlen(aTBSName)) == 0 )
        {
            idlOS::snprintf(sFullFileName, SM_MAX_FILE_NAME, "%s%c%s",
                           aPathName, IDL_FILE_SEPARATOR, sFileName);
            IDE_TEST_RAISE(idf::unlink(sFullFileName) != 0,
                           err_file_delete);

            // To Fix BUG-13924
            ideLog::log(SM_TRC_LOG_LEVEL_MRECOV,
                        SM_TRC_MRECORD_FILE_UNLINK,
                        sFullFileName);

            sResDirEnt = NULL;
            errno = 0;
            sRc = idf::readdir_r(sDIR, sDirEnt, &sResDirEnt);
            IDE_TEST_RAISE( (sRc != 0) && (errno != 0), err_read_dir );
            errno = 0;

            continue;
        }

        // [2] unlink internal database file
        sOffset = (SInt)(idlOS::strlen(sFileName) - 4);
        if(sOffset > 4)
        {
            if(idlOS::strncmp(sFileName + sOffset,
                              SM_TABLE_BACKUP_EXT, 4) == 0)
            {
                idlOS::snprintf(sFullFileName, SM_MAX_FILE_NAME, "%s%c%s",
                               aPathName, IDL_FILE_SEPARATOR, sFileName);
                IDE_TEST_RAISE(idf::unlink(sFullFileName) != 0,
                               err_file_delete);

                // To Fix BUG-13924
                ideLog::log(SM_TRC_LOG_LEVEL_MRECOV,
                            SM_TRC_MRECORD_FILE_UNLINK,
                            sFullFileName);
            }
        }

        // To fix BUG-4980
        // [3] unlink index file
        if(idlOS::strlen(sFileName) >= sLen)
        {
            if(idlOS::strncmp(sFileName, sPrefix, sLen) == 0)
            {
                idlOS::snprintf(sFullFileName, SM_MAX_FILE_NAME, "%s%c%s",
                                aPathName, IDL_FILE_SEPARATOR, sFileName);
                IDE_TEST_RAISE(idf::unlink(sFullFileName) != 0,
                               err_file_delete);

                // To Fix BUG-13924
                ideLog::log(SM_TRC_LOG_LEVEL_MRECOV,
                            SM_TRC_MRECORD_FILE_UNLINK,
                            sFullFileName);
            }
        }

        sResDirEnt = NULL;
        errno = 0;
        sRc = idf::readdir_r(sDIR, sDirEnt, &sResDirEnt);
        IDE_TEST_RAISE( (sRc != 0) && (errno != 0), err_read_dir );
        errno = 0;
    }

    idf::closedir(sDIR);
    sDIR = NULL;

    IDE_TEST( iduMemMgr::free( sDirEnt ) != IDE_SUCCESS );
    sDirEnt = NULL;

    return IDE_SUCCESS;

    IDE_EXCEPTION( insufficient_memory );
    {
        IDE_SET( ideSetErrorCode( idERR_ABORT_InsufficientMemory ) );
    }
    IDE_EXCEPTION(err_open_dir);
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_NotDir));
    }
    IDE_EXCEPTION(err_read_dir);
    {
        IDE_SET( ideSetErrorCode( smERR_FATAL_CannotReadDir, aPathName ) );
    }
    IDE_EXCEPTION(err_file_delete);
    {
        IDE_SET(ideSetErrorCode(smERR_FATAL_FileDelete, sFullFileName));
    }
    IDE_EXCEPTION_END;

    IDE_PUSH();
    {
        if ( sDIR != NULL )
        {
            idf::closedir( sDIR );
            sDIR = NULL;
        }
        else
        {
            /* Nothing to do */
        }

        if ( sDirEnt != NULL )
        {
            IDE_ASSERT( iduMemMgr::free( sDirEnt ) == IDE_SUCCESS );
            sDirEnt = NULL;
        }
        else
        {
            /* Nothing to do */
        }
    }
    IDE_POP();

    return IDE_FAILURE;
}


/***********************************************************************
 * Description : �ش� path���� disk db ���� ���� ����
 * destroydb, onlineBackup��� ȣ��ȴ�.
 **********************************************************************/
IDE_RC smrBackupMgr::unlinkDataFile( SChar*  aDataFileName )
{
    IDE_TEST_RAISE( idf::unlink(aDataFileName) != 0, err_file_delete );
    //=====================================================================
    // To Fix BUG-13924
    //=====================================================================

    ideLog::log( SM_TRC_LOG_LEVEL_MRECOV, SM_TRC_MRECORD_FILE_UNLINK,
                 aDataFileName );

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_file_delete);
    {
        IDE_SET(ideSetErrorCode(smERR_FATAL_FileDelete, aDataFileName));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 * Description : �ش� path���� memory db ���� ���� ����
 * destroydb, onlineBackup��� ȣ��ȴ�.
 **********************************************************************/
IDE_RC smrBackupMgr::unlinkAllLogFiles( SChar* aPathName )
{

    SInt               sRc;
    DIR               *sDIR     = NULL;
    struct  dirent    *sDirEnt  = NULL;
    struct  dirent    *sResDirEnt;
    SChar              sFileName[SM_MAX_FILE_NAME];
    SChar              sFullFileName[SM_MAX_FILE_NAME];

    IDE_DASSERT( aPathName != NULL );

    /* smrBackupMgr_unlinkAllLogFiles_malloc_DirEnt.tc */
    IDU_FIT_POINT_RAISE( "smrBackupMgr::unlinkAllLogFiles::malloc::DirEnt",
                         insufficient_memory );
    IDE_TEST_RAISE( iduMemMgr::malloc( IDU_MEM_SM_SMR,
                                       ID_SIZEOF(struct dirent) + SM_MAX_FILE_NAME,
                                       (void**)&sDirEnt,
                                       IDU_MEM_FORCE ) != IDE_SUCCESS,
                    insufficient_memory );

    /* smrBackupMgr_unlinkAllLogFiles_opendir.tc */
    IDU_FIT_POINT_RAISE( "smrBackupMgr::unlinkAllLogFiles::opendir", err_open_dir );
    sDIR = idf::opendir(aPathName);
    IDE_TEST_RAISE(sDIR == NULL, err_open_dir);

    /* For each file in Backup Path, unlink */
    sResDirEnt = NULL;
    errno = 0;

    /* smrBackupMgr_unlinkAllLogFiles_readdir_r.tc */
    IDU_FIT_POINT_RAISE( "smrBackupMgr::unlinkAllLogFiles::readdir_r", err_read_dir );
    sRc = idf::readdir_r(sDIR, sDirEnt, &sResDirEnt);
    IDE_TEST_RAISE( (sRc != 0) && (errno != 0), err_read_dir );
    errno = 0;

    while(sResDirEnt != NULL)
    {
        idlOS::strcpy(sFileName, sResDirEnt->d_name);

        if (idlOS::strcmp(sFileName, ".") == 0 ||
            idlOS::strcmp(sFileName, "..") == 0)
        {
            sResDirEnt = NULL;
            errno = 0;
            sRc = idf::readdir_r(sDIR, sDirEnt, &sResDirEnt);
            IDE_TEST_RAISE( (sRc != 0) && (errno != 0), err_read_dir );
            errno = 0;
            continue;
        }

        // [1] unlink backup_version file, loganchor file, db file, log file
        if (idlOS::strncmp(sFileName,
                    SMR_LOGANCHOR_NAME,
                    idlOS::strlen(SMR_LOGANCHOR_NAME)) == 0 ||
            idlOS::strncmp(sFileName,
                    SMR_LOG_FILE_NAME,
                    idlOS::strlen(SMR_LOG_FILE_NAME)) == 0 )
        {
            idlOS::snprintf(sFullFileName, SM_MAX_FILE_NAME, "%s%c%s",
                           aPathName, IDL_FILE_SEPARATOR, sFileName);

            IDE_TEST_RAISE(idf::unlink(sFullFileName) != 0,
                           err_file_delete);

            // To Fix BUG-13924
            ideLog::log(SM_TRC_LOG_LEVEL_MRECOV,
                        SM_TRC_MRECORD_FILE_UNLINK,
                        sFullFileName);

            sResDirEnt = NULL;
            errno = 0;
            sRc = idf::readdir_r(sDIR, sDirEnt, &sResDirEnt);
            IDE_TEST_RAISE( (sRc != 0) && (errno != 0), err_read_dir );
            errno = 0;

            continue;
        }

        sResDirEnt = NULL;
        errno = 0;
        sRc = idf::readdir_r(sDIR, sDirEnt, &sResDirEnt);
        IDE_TEST_RAISE( (sRc != 0) && (errno != 0), err_read_dir );
        errno = 0;
    }

    idf::closedir( sDIR );
    sDIR = NULL;

    IDE_TEST( iduMemMgr::free( sDirEnt ) != IDE_SUCCESS );
    sDirEnt = NULL;

    return IDE_SUCCESS;

    IDE_EXCEPTION( insufficient_memory );
    {
        IDE_SET( ideSetErrorCode( idERR_ABORT_InsufficientMemory ) );
    }
    IDE_EXCEPTION(err_open_dir);
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_NotDir));
    }
    IDE_EXCEPTION(err_read_dir);
    {
        IDE_SET( ideSetErrorCode( smERR_FATAL_CannotReadDir, aPathName ) );
    }
    IDE_EXCEPTION(err_file_delete);
    {
        IDE_SET(ideSetErrorCode(smERR_FATAL_FileDelete, sFullFileName));
    }
    IDE_EXCEPTION_END;

    IDE_PUSH();
    {
        if ( sDIR != NULL )
        {
            idf::closedir( sDIR );
            sDIR = NULL;
        }
        else
        {
            /* Nothing to do */
        }

        if ( sDirEnt != NULL )
        {
            IDE_ASSERT( iduMemMgr::free( sDirEnt ) == IDE_SUCCESS );
            sDirEnt = NULL;
        }
        else
        {
            /* Nothing to do */
        }
    }
    IDE_POP();

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : PROJ-2133 incremental backup 
 *               �ش� path���� change tracking���� ����
 *               destroydb ���� ȣ��ȴ�.
 **********************************************************************/
IDE_RC smrBackupMgr::unlinkChangeTrackingFile( SChar * aChangeTrackingFileName )
{
    IDE_TEST_RAISE( idf::unlink(aChangeTrackingFileName) != 0, err_file_delete );

    ideLog::log( SM_TRC_LOG_LEVEL_MRECOV, SM_TRC_MRECORD_FILE_UNLINK,
                 aChangeTrackingFileName );

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_file_delete);
    {
        IDE_SET(ideSetErrorCode(smERR_FATAL_FileDelete, 
                                aChangeTrackingFileName));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : PROJ-2133 incremental backup
 *               �ش� path���� backup info���� ����
 *               destroydb ���� ȣ��ȴ�.
 **********************************************************************/
IDE_RC smrBackupMgr::unlinkBackupInfoFile( SChar * aBackupInfoFileName )
{
    IDE_TEST_RAISE( idf::unlink(aBackupInfoFileName) != 0, err_file_delete );

    ideLog::log( SM_TRC_LOG_LEVEL_MRECOV, SM_TRC_MRECORD_FILE_UNLINK,
                 aBackupInfoFileName );

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_file_delete);
    {
        IDE_SET(ideSetErrorCode(smERR_FATAL_FileDelete, 
                                aBackupInfoFileName));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************
 * Description:
 * LogAnchor������ destnation directroy�� copy�Ѵ�.
 * A4, PRJ-1149���� log anchor,  tablespace, database backup API
 *********************************************************/
IDE_RC smrBackupMgr::backupLogAnchor( idvSQL* aStatistics,
                                      SChar * aDestFilePath )
{
    smrLogAnchorMgr*    sAnchorMgr;
    SInt                sSystemErrno;
    UInt                sDestFilePathLen;

    IDE_DASSERT( aDestFilePath != NULL );

    sDestFilePathLen = idlOS::strlen(aDestFilePath);
    IDE_TEST_RAISE( sDestFilePathLen == 0, error_backupdir_path_null);
    // ��� ��ο� loganchor full name �� ���� ����
    IDE_TEST_RAISE( (sDestFilePathLen+idlOS::strlen(SMR_LOGANCHOR_NAME)+1+1) > SM_MAX_FILE_NAME , error_backupdir_path_too_long );

    //log anchor ������ ȹ��
    sAnchorMgr = smrRecoveryMgr::getLogAnchorMgr();

    errno = 0;
    if ( sAnchorMgr->backup( aStatistics,
                             aDestFilePath ) != IDE_SUCCESS )
    {
        sSystemErrno = ideGetSystemErrno();
        
        if ( isRemainSpace(sSystemErrno) == ID_TRUE )
        {
            IDE_RAISE( err_logAnchor_backup_write );

        } 
        else
        {
            IDE_RAISE( err_online_backup_write );
        }
    }

    //BUG-47538: �α׾�Ŀ ��� ���� �޽��� �׽�Ʈ
    IDU_FIT_POINT_RAISE( "BUG-47538@smrBackupMgr::backupLogAnchor", err_online_backup_write );

    return IDE_SUCCESS;

    IDE_EXCEPTION( error_backupdir_path_null );
    {
        IDE_SET( ideSetErrorCode(smERR_ABORT_PathIsNullString) );
    }
    IDE_EXCEPTION( error_backupdir_path_too_long );
    {
        IDE_SET( ideSetErrorCode(smERR_ABORT_TooLongFilePath, aDestFilePath,SMR_LOGANCHOR_NAME) );
    }
    IDE_EXCEPTION( err_online_backup_write );
    {
        ideLog::log( SM_TRC_LOG_LEVEL_ABORT,
                     SM_TRC_MRECOVERY_BACKUP_ABORT );
        IDE_SET(ideSetErrorCode(smERR_ABORT_BackupWrite));
    }
    IDE_EXCEPTION( err_logAnchor_backup_write );
    {
        ideLog::log( SM_TRC_LOG_LEVEL_ABORT,
                     SM_TRC_MRECOVERY_BACKUP_ABORT );
        //log anchor���� �α� ���Ͽ������ error�ڵ带 settin�ϱ� ������ skip.
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************
 * Description:
 * - ���̺� �����̽� media ������ ���� backupTableSpace��
 *  �б��Ų��.
 * -> memory table space,
 * -> disk tables space.
 *********************************************************/
IDE_RC smrBackupMgr::backupTableSpace( idvSQL*   aStatistics,
                                       void*     aTrans,
                                       scSpaceID aSpaceID,
                                       SChar*    aBackupDir )
{
    UInt     sState = 0;
    UInt     sRestoreState = 0;
    idBool   sLocked;
    sctTableSpaceNode *sSpaceNode = NULL;

    IDE_DASSERT( aTrans != NULL );
    IDE_DASSERT( aBackupDir != NULL );

    IDE_TEST( mMtxOnlineBackupStatus.trylock(sLocked) != IDE_SUCCESS );

    IDE_TEST_RAISE( sLocked == ID_FALSE, error_backup_going);
    sState = 1;

    // alter database backup tablespace or database�Ӹ��ƴ϶�,
    // alter tablespace begin backup ��η� �ü� �ֱ� ������,
    // ������ ���� �˻��Ѵ�.
    IDE_TEST_RAISE( mOnlineBackupState != SMR_BACKUP_NONE,
                    error_backup_going );

    // Aging ����������� �� Ʈ������� ������� �ʰ� ����������
    // �����ϰ� �Ѵ�. (����) smxTransMgr::getMemoryMinSCN()
    smLayerCallback::updateSkipCheckSCN( aTrans, ID_TRUE );
    sState = 2;

    // üũ����Ʈ�� �߻���Ų��.
    IDE_TEST( gSmrChkptThread.resumeAndWait( aStatistics )
              != IDE_SUCCESS );

    ideLog::log( SM_TRC_LOG_LEVEL_MRECOV, SM_TRC_MRECOVERY_BACKUP_START);

    sSpaceNode = sctTableSpaceMgr::getSpaceNodeBySpaceID( aSpaceID );

    IDE_TEST_RAISE( sSpaceNode == NULL ,
                    error_not_found_tablespace_node );

    IDE_TEST_RAISE( ( (sSpaceNode->mState & SMI_TBS_DROPPED)   == SMI_TBS_DROPPED ) ||
                    ( (sSpaceNode->mState & SMI_TBS_DISCARDED) == SMI_TBS_DISCARDED ),
                    error_not_found_tablespace_node );

    IDE_TEST( sctTableSpaceMgr::startTableSpaceBackup( aStatistics,
                                                       sSpaceNode )
              != IDE_SUCCESS );
    sState = 3;

    // Backup Flag ���� �� �ѹ� �� Ȯ����.
    // Discard�� ���� �߿� ���� ������ Drop�� �ٽ� Ȯ���ϸ� ��
    IDE_TEST_RAISE(( sSpaceNode->mState & SMI_TBS_DROPPED ) == SMI_TBS_DROPPED,
                    error_not_found_tablespace_node );

    switch( sctTableSpaceMgr::getTBSLocation( sSpaceNode ) )
    {
        case SMI_TBS_MEMORY:
            {
                setOnlineBackupStatusOR( SMR_BACKUP_MEMTBS );
                sRestoreState = 2;

                IDE_TEST( backupMemoryTBS( aStatistics,
                                           (smmTBSNode*)sSpaceNode,
                                           aBackupDir )
                          != IDE_SUCCESS );

                setOnlineBackupStatusNOT( SMR_BACKUP_MEMTBS );
                sRestoreState = 0;
            }
            break;
        case SMI_TBS_DISK:
            {
                setOnlineBackupStatusOR( SMR_BACKUP_DISKTBS );
                sRestoreState = 1;

                IDE_TEST( backupDiskTBS( aStatistics,
                                         (sddTableSpaceNode*)sSpaceNode,
                                         aBackupDir )
                          != IDE_SUCCESS );

                setOnlineBackupStatusNOT( SMR_BACKUP_DISKTBS );
                sRestoreState = 0;
            }
            break;
        case SMI_TBS_VOLATILE:
            {
                // Nothing to do...
                // volatile tablespace�� ���ؼ��� ����� �������� �ʴ´�.
            }
            break;
        default:
            IDE_ASSERT(0);
            break;
    }

    sState = 2;
    IDE_TEST( sctTableSpaceMgr::endTableSpaceBackup( aStatistics,
                                                     sSpaceNode ) != IDE_SUCCESS );

    // ������ �ش� �α������� switch �Ͽ� archive ��Ų��.
    IDE_TEST( smrLogMgr::switchLogFileByForce() != IDE_SUCCESS );
    sState = 1;

    smLayerCallback::updateSkipCheckSCN( aTrans, ID_FALSE );

    sState = 0;
    IDE_TEST( unlock() != IDE_SUCCESS );

    ideLog::log( SM_TRC_LOG_LEVEL_MRECOV, SM_TRC_MRECOVERY_BACKUP_END );

    return IDE_SUCCESS;

    IDE_EXCEPTION( error_not_found_tablespace_node );
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_NotFoundTableSpaceNode,
                                  aSpaceID) );
    }
    IDE_EXCEPTION(error_backup_going);
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_BACKUP_GOING));
    }
    IDE_EXCEPTION_END;

    IDE_PUSH();
    {
        switch( sRestoreState )
        {
            case 2:
                setOnlineBackupStatusNOT ( SMR_BACKUP_MEMTBS );
                break;
            case 1:
                setOnlineBackupStatusNOT ( SMR_BACKUP_DISKTBS );
                break;
            default:
                break;
        }

        switch( sState )
        {
            case 3:
                sctTableSpaceMgr::endTableSpaceBackup( aStatistics,
                                                       sSpaceNode ) ;
            case 2:
                smLayerCallback::updateSkipCheckSCN( aTrans, ID_FALSE );
            case 1:
                IDE_ASSERT( unlock() == IDE_SUCCESS );
                break;
            default:
                break;
        }

        ideLog::log(SM_TRC_LOG_LEVEL_ABORT, SM_TRC_MRECOVERY_BACKUP_FAIL);
    }
    IDE_POP();

    return IDE_FAILURE;
}

/*********************************************************
 * Description: smrBackup::backupMemoryTBS
 * memory table space�� ����Ÿ ������ backup��Ų��.
 *  1. check point�� disable ��Ų��.
 *  2. memory database file���� copy�Ѵ�.
 *  3. alter table������ �߻���  Internal backup database File copy.
 *  4. check point�� �簳�Ѵ�.
 **********************************************************/
IDE_RC smrBackupMgr::backupMemoryTBS( idvSQL*      aStatistics,
                                      smmTBSNode * aSpaceNode,
                                      SChar*       aBackupDir )
{

    UInt                 i;
    UInt                 sWhichDB;
    UInt                 sDataFileNameLen;
    UInt                 sBackupPathLen;
    SInt                 sSystemErrno;
    smmDatabaseFile    * sDatabaseFile;
    SChar                sStrFullFileName[ SMI_MAX_CHKPT_PATH_NAME_LEN ];

    IDE_DASSERT( aBackupDir != NULL );
    IDE_DASSERT( sctTableSpaceMgr::isMemTableSpace( aSpaceNode ) == ID_TRUE );

    // [2] memory database file���� copy�Ѵ�.
    sWhichDB = smmManager::getCurrentDB( (smmTBSNode*)aSpaceNode );
    sBackupPathLen = idlOS::strlen( aBackupDir );

    /* BUG-34357
     * [sBackupPathLen - 1] causes segmentation fault
     * */
    IDE_TEST_RAISE( sBackupPathLen == 0, error_backupdir_path_null );
    //                                                   SYS_TBS_MEM_DATA -0-0
    IDE_TEST_RAISE( (sBackupPathLen+idlOS::strlen(aSpaceNode->mHeader.mName)+4+1) > SMI_MAX_CHKPT_PATH_NAME_LEN, error_backupdir_path_too_long );
    

    for ( i = 0; i <= aSpaceNode->mLstCreatedDBFile; i++ )
    {
        idlOS::memset(sStrFullFileName, 0x00, SMI_MAX_CHKPT_PATH_NAME_LEN);

        /* ------------------------------------------------
         * BUG-11206�� ���� source�� destination�� ���Ƽ�
         * ���� ����Ÿ ������ ���ǵǴ� ��츦 �������Ͽ�
         * ������ ���� ��Ȯ�� path�� ������.
         * ----------------------------------------------*/
        if( aBackupDir[sBackupPathLen - 1] == IDL_FILE_SEPARATOR )
        {
            idlOS::snprintf(sStrFullFileName, SMI_MAX_CHKPT_PATH_NAME_LEN,
                            "%s%s-%"ID_UINT32_FMT"-%"ID_UINT32_FMT,
                            aBackupDir,
                            aSpaceNode->mHeader.mName,
                            sWhichDB, 
                            i);
        }
        else
        {
            idlOS::snprintf(sStrFullFileName, SMI_MAX_CHKPT_PATH_NAME_LEN,
                            "%s%c%s-%"ID_UINT32_FMT"-%"ID_UINT32_FMT,
                            aBackupDir,
                            IDL_FILE_SEPARATOR,
                            aSpaceNode->mHeader.mName,
                            sWhichDB,
                            i);
        }

        sDataFileNameLen = idlOS::strlen(sStrFullFileName);

        IDE_TEST( sctTableSpaceMgr::makeValidABSPath(
                                              ID_TRUE,
                                              sStrFullFileName,
                                              &sDataFileNameLen,
                                              SMI_TBS_DISK)
                  != IDE_SUCCESS );

        IDE_TEST( smmManager::openAndGetDBFile( aSpaceNode,
                                                sWhichDB,
                                                i,
                                                &sDatabaseFile )
                  != IDE_SUCCESS );

        errno = 0;
        IDE_TEST_RAISE( idlOS::strcmp(sStrFullFileName,
                                      sDatabaseFile->getFileName())
                        == 0, error_self_copy);

        /* BUG-18678: Memory/Disk DB File�� /tmp�� Online Backup�Ҷ� Disk��
           Backup�� �����մϴ�.*/
        if ( sDatabaseFile->copy( aStatistics,
                                  sStrFullFileName )
             != IDE_SUCCESS )
        {
            sSystemErrno = ideGetSystemErrno();

            IDE_TEST_RAISE(isRemainSpace(sSystemErrno) == ID_TRUE,
                           error_backup_datafile);

            IDE_RAISE(err_online_backup_write);
        }

        ideLog::log(SM_TRC_LOG_LEVEL_MRECOV,
                    SM_TRC_MRECOVERY_BACKUP_INFO1,
                    sDatabaseFile->getFileName(),
                    sStrFullFileName);

        IDE_TEST_RAISE(iduCheckSessionEvent(aStatistics) != IDE_SUCCESS,
                       error_abort_backup);
    }

    /* ------------------------------------------------
     * [2] alter table������ �߻��� Backup Internal Database File
     * ----------------------------------------------*/
    errno = 0;
    if ( smLayerCallback::copyAllTableBackup( smLayerCallback::getBackupDir(),
                                              aBackupDir )
         != IDE_SUCCESS )
    {
        sSystemErrno = ideGetSystemErrno();
        IDE_TEST_RAISE(isRemainSpace(sSystemErrno) == ID_TRUE,
                       err_backup_internalbackup_file);

        IDE_RAISE(err_online_backup_write);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(error_self_copy);
    {
        IDE_SET( ideSetErrorCode(smERR_ABORT_SelfCopy) );
    }
    IDE_EXCEPTION(err_online_backup_write);
    {
        ideLog::log( SM_TRC_LOG_LEVEL_ABORT, SM_TRC_MRECOVERY_BACKUP_ABORT1 );

        IDE_SET( ideSetErrorCode(smERR_ABORT_BackupWrite) );
    }
    IDE_EXCEPTION(error_backup_datafile);
    {
        ideLog::log( SM_TRC_LOG_LEVEL_ABORT,
                     SM_TRC_MRECOVERY_BACKUP_ABORT2,
                     sStrFullFileName );

        IDE_SET( ideSetErrorCode( smERR_ABORT_BackupDatafile,
                                  "MEMORY TABLESPACE",
                                  sStrFullFileName ) );
    }
    IDE_EXCEPTION(err_backup_internalbackup_file);
    {
        ideLog::log( SM_TRC_LOG_LEVEL_ABORT,
                     SM_TRC_MRECOVERY_BACKUP_ABORT3,
                     sStrFullFileName );
        IDE_SET( ideSetErrorCode( smERR_ABORT_BackupDatafile,
                                 "BACKUPED INTERNAL FILE",
                                 "" ) );
    }
    IDE_EXCEPTION(error_abort_backup);
    {
        ideLog::log(SM_TRC_LOG_LEVEL_ABORT, SM_TRC_MRECOVERY_BACKUP_ABORT4);
    }
    IDE_EXCEPTION(error_backupdir_path_null);
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_PathIsNullString));
    }
    IDE_EXCEPTION( error_backupdir_path_too_long );
    {
        idlOS::snprintf(sStrFullFileName, SMI_MAX_CHKPT_PATH_NAME_LEN,
                        "%s-%"ID_UINT32_FMT"-%"ID_UINT32_FMT,
                        aSpaceNode->mHeader.mName,
                        sWhichDB, 
                        0);
        IDE_SET( ideSetErrorCode(smERR_ABORT_TooLongFilePath, aBackupDir, sStrFullFileName ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************
 * Description:
 * disk table space�� ����Ÿ ������ backup��Ų��.
 **********************************************************/
IDE_RC smrBackupMgr::backupDiskTBS( idvSQL            * aStatistics,
                                    sddTableSpaceNode * aSpaceNode,
                                    SChar             * aBackupDir )
{
    SInt                  sSystemErrno;
    UInt                  sState = 0;
    UInt                  sDataFileNameLen;
    UInt                  sBackupPathLen;
    sddDataFileNode     * sDataFileNode = NULL;
    SChar               * sDataFileName;
    SChar                 sStrDestFileName[ SM_MAX_FILE_NAME ];
    UInt                  i;

    IDE_DASSERT( aBackupDir != NULL );
    IDE_DASSERT( sctTableSpaceMgr::isDiskTableSpace( aSpaceNode ) == ID_TRUE );

    if ( ( (aSpaceNode->mHeader.mState & SMI_TBS_DROPPED)   != SMI_TBS_DROPPED ) &&
         ( (aSpaceNode->mHeader.mState & SMI_TBS_DISCARDED) != SMI_TBS_DISCARDED ) )
    {
        /* ------------------------------------------------
         * ���̺� �����̽�  ����Ÿ ���ϵ�  copy.
         * ----------------------------------------------*/
        for (i=0; i < aSpaceNode->mNewFileID ; i++ )
        {
            sDataFileNode = aSpaceNode->mFileNodeArr[i] ;

            if( sDataFileNode == NULL)
            {
                continue;
            }

            if( SMI_FILE_STATE_IS_DROPPED( sDataFileNode->mState ) )
            {
                continue;
            }

            // XXX backup �߿� file�� creating, resize, dropping �� �����Ѱ�?
            // �׷��� ���� ���Ϸ� �Ѱܹ����� �ǳ�?
            // ���� ���Ϸ� �ѱ�� sleep�� ������
            /* Drop File�� Ȯ���غ��� TableSpace �δ� File�� unlink�Ǿ
             * Drop File�δ� File�� ������ �������� �ʴ´�.
             * ���⸦ ��� �Ѵٰ� �ص� ����� ���� �� �ϴ�.
             * �ٸ� rollback �ÿ� ������ �� �� �ִ�.
             * */
            if( SMI_FILE_STATE_IS_CREATING( sDataFileNode->mState ) ||
                SMI_FILE_STATE_IS_RESIZING( sDataFileNode->mState ) ||
                SMI_FILE_STATE_IS_DROPPING( sDataFileNode->mState ) )
            {
                idlOS::sleep(1);
                continue;
            }

            idlOS::memset(sStrDestFileName,0x00,SM_MAX_FILE_NAME);
            sDataFileName = idlOS::strrchr( sDataFileNode->mName,
                                            IDL_FILE_SEPARATOR );
            IDE_TEST_RAISE(sDataFileName == NULL,error_backupdir_file_path);

            /* ------------------------------------------------
             * BUG-11206�� ���� source�� destination�� ���Ƽ�
             * ���� ����Ÿ ������ ���ǵǴ� ��츦 �������Ͽ�
             * ������ ���� ��Ȯ�� path�� ������.
             * ----------------------------------------------*/
            sDataFileName = sDataFileName+1;
            sBackupPathLen = idlOS::strlen(aBackupDir);

            /* BUG-34357
             * [sBackupPathLen - 1] causes segmentation fault
             * */
            IDE_TEST_RAISE( sBackupPathLen == 0, error_backupdir_path_null );

            if( aBackupDir[sBackupPathLen -1 ] == IDL_FILE_SEPARATOR)
            {
                idlOS::snprintf(sStrDestFileName, SM_MAX_FILE_NAME, "%s%s",
                                aBackupDir,
                                sDataFileName);
            }
            else
            {
                idlOS::snprintf(sStrDestFileName, SM_MAX_FILE_NAME, "%s%c%s",
                                aBackupDir,
                                IDL_FILE_SEPARATOR,
                                sDataFileName);
            }

            sDataFileNameLen = idlOS::strlen(sStrDestFileName);
        IDE_TEST_RAISE( (sBackupPathLen+idlOS::strlen(sDataFileName)+1) > SM_MAX_FILE_NAME, error_backupdir_path_too_long );
        

            IDE_TEST( sctTableSpaceMgr::makeValidABSPath(
                          ID_TRUE,
                          sStrDestFileName,
                          &sDataFileNameLen,
                          SMI_TBS_DISK)
                      != IDE_SUCCESS );

            IDE_TEST_RAISE(idlOS::strcmp(sDataFileNode->mFile.getFileName(),
                                         sStrDestFileName) == 0,
                           error_self_copy);

            /* ------------------------------------------------
             * [3] ����Ÿ ������ ���¸� backup begin���� �����Ѵ�.
             * ----------------------------------------------*/
            IDE_TEST(sddDiskMgr::prepareFileBackup( aStatistics,
                                                    aSpaceNode,
                                                    sDataFileNode )
                     != IDE_SUCCESS);
            sState = 1;

            /* FIT/ART/sm/Bugs/BUG-32218/BUG-32218.sql */
            IDU_FIT_POINT( "1.BUG-32218@smrBackupMgr::backupDiskTBS" );

            if ( sddDiskMgr::copyDataFile( aStatistics,
                                           aSpaceNode,
                                           sDataFileNode,
                                           sStrDestFileName )
                 != IDE_SUCCESS )
            {
                sSystemErrno = ideGetSystemErrno();
                IDE_TEST_RAISE( isRemainSpace(sSystemErrno) == ID_TRUE,
                                error_backup_datafile );
                IDE_RAISE(err_online_backup_write);
            }

            ideLog::log(SM_TRC_LOG_LEVEL_MRECOV,
                        SM_TRC_MRECOVERY_BACKUP_INFO2,
                        aSpaceNode->mHeader.mName,
                        sDataFileNode->mFile.getFileName(),
                        sStrDestFileName);

            IDE_TEST_RAISE(iduCheckSessionEvent(aStatistics) != IDE_SUCCESS,
                           error_abort_backup);

            /* ------------------------------------------------
             * [4] ����Ÿ ������ ����� ������ ������, PIL hash��
             * logg������ ����Ÿ ���Ͽ� �����Ѵ�.
             * ----------------------------------------------*/

            sState = 0;
            sddDiskMgr::completeFileBackup( aStatistics,
                                            aSpaceNode,
                                            sDataFileNode );
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(error_self_copy);
    {
       IDE_SET( ideSetErrorCode(smERR_ABORT_SelfCopy));
    }
    IDE_EXCEPTION(err_online_backup_write);
    {
        ideLog::log(SM_TRC_LOG_LEVEL_ABORT,
                    SM_TRC_MRECOVERY_BACKUP_ABORT1);
        IDE_SET(ideSetErrorCode(smERR_ABORT_BackupWrite));
    }
    IDE_EXCEPTION(error_backup_datafile);
    {
        ideLog::log(SM_TRC_LOG_LEVEL_ABORT,
                    SM_TRC_MRECOVERY_BACKUP_ABORT5,
                    sStrDestFileName);
        IDE_SET(ideSetErrorCode(smERR_ABORT_BackupDatafile,"DISK TABLESPACE",
                                sStrDestFileName ));
    }
    IDE_EXCEPTION(error_backupdir_file_path);
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_InvalidFilePathABS));
    }
    IDE_EXCEPTION(error_backupdir_path_null);
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_PathIsNullString));
    }
    IDE_EXCEPTION( error_backupdir_path_too_long );
    {
        IDE_SET( ideSetErrorCode(smERR_ABORT_TooLongFilePath,aBackupDir,sDataFileName) );
    }
    IDE_EXCEPTION(error_abort_backup);
    {
        ideLog::log(SM_TRC_LOG_LEVEL_ABORT,
                    SM_TRC_MRECOVERY_BACKUP_ABORT4);

    }
    IDE_EXCEPTION_END;

    if( sState != 0 )
    {
        sddDiskMgr::completeFileBackup( aStatistics,
                                        aSpaceNode,
                                        sDataFileNode );
    }

    return IDE_FAILURE;
}

/*********************************************************
 * Description : database ���� ���
 *
 * - Database�� �ִ� ��� ���̺� �����̽��� ���Ͽ�
 *  hot backup�� �����Ͽ�, �� ���̺� �����̽���
 *  ����Ÿ ������ �����Ѵ�.
 * - log anchor���ϵ��� �����Ѵ�.
 **********************************************************/
IDE_RC smrBackupMgr::backupDatabase( idvSQL *  aStatistics,
                                     void   *  aTrans,
                                     SChar  *  aBackupDir )
{
    idBool sLocked;
    UInt   sState = 0;
    UInt   sRestoreState = 0;

    IDE_DASSERT( aTrans != NULL );
    IDE_DASSERT( aBackupDir != NULL );

    ideLog::log(SM_TRC_LOG_LEVEL_MRECOV, SM_TRC_MRECOVERY_BACKUP_START2);

    // BUG-29861 [WIN] Zero Length File Name�� ����� �˻����� ���մϴ�.
    IDE_TEST_RAISE( idlOS::strlen(aBackupDir) == 0, error_backupdir_path_null );

    IDE_TEST( mMtxOnlineBackupStatus.trylock( sLocked )
                    != IDE_SUCCESS );

    IDE_TEST_RAISE( sLocked == ID_FALSE, error_backup_going );
    sState = 1;

    // ALTER DATABASE BACKUP TABLESPACE OR DATABASE ������
    // ALTER TABLESPACE .. BEGIN BACKUP ���� ����� ������ üũ�Ѵ�.
    IDE_TEST_RAISE( mOnlineBackupState != SMR_BACKUP_NONE,
                    error_backup_going );

    // Aging ����������� �� Ʈ������� ������� �ʰ� ����������
    // �����ϰ� �Ѵ�. (����) smxTransMgr::getMemoryMinSCN()
    smLayerCallback::updateSkipCheckSCN( aTrans, ID_TRUE );
    sState = 2;

    // [1] üũ����Ʈ�� �߻���Ų��.
    IDE_TEST( gSmrChkptThread.resumeAndWait( aStatistics )
              != IDE_SUCCESS );

    // [2] BACKUP �������� MEMORY TABLESPACE�� ������¸� �����Ѵ�.
    setOnlineBackupStatusOR( SMR_BACKUP_MEMTBS );
    sRestoreState = 2;

    IDE_TEST( gSmrChkptThread.lock( aStatistics ) != IDE_SUCCESS );
    sState = 3;

    // [3] ������ MEMORY TABLESPACE�� ���¼����� ����� �����Ѵ�.
    // BUG-27204 Database ��� �� session event check���� ����
    IDE_TEST( smmTBSMediaRecovery::backupAllMemoryTBS(
                             aStatistics,
                             aTrans,
                             aBackupDir ) != IDE_SUCCESS );

    // [4] Loganchor ������ ����Ѵ�.
    IDE_TEST( smrBackupMgr::backupLogAnchor( aStatistics,
                                             aBackupDir )
              != IDE_SUCCESS );

    sState = 2;
    IDE_TEST( gSmrChkptThread.unlock() != IDE_SUCCESS );

    // [5] BACKUP �������� MEMORY TABLESPACE�� ������¸� �����Ѵ�.
    setOnlineBackupStatusNOT( SMR_BACKUP_MEMTBS );
    sRestoreState = 0;

    // [6] BACKUP �������� DISK TABLESPACE�� ������¸� �����Ѵ�.
    setOnlineBackupStatusOR( SMR_BACKUP_DISKTBS );
    sRestoreState = 1;

    // [7] DISK TABLESPACE���� ������¸� �����ϰ� ����Ѵ�.
    IDE_TEST( sddDiskMgr::backupAllDiskTBS( aStatistics,
                                            aBackupDir )
              != IDE_SUCCESS );

    setOnlineBackupStatusNOT( SMR_BACKUP_DISKTBS );
    sRestoreState = 0;

    // [8] ������ �ش� �α������� switch �Ͽ� archive ��Ų��.
    IDE_TEST( smrLogMgr::switchLogFileByForce() != IDE_SUCCESS);
    sState = 1;

    smLayerCallback::updateSkipCheckSCN( aTrans, ID_FALSE );

    sState = 0;
    IDE_TEST( unlock() != IDE_SUCCESS );

    ideLog::log( SM_TRC_LOG_LEVEL_MRECOV, SM_TRC_MRECOVERY_BACKUP_END2 );

    return IDE_SUCCESS;

    IDE_EXCEPTION( error_backup_going );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_BACKUP_GOING));
    }
    // BUG-29861 [WIN] Zero Length File Name�� ����� �˻����� ���մϴ�.
    IDE_EXCEPTION( error_backupdir_path_null );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_PathIsNullString));
    }
    IDE_EXCEPTION_END;

    IDE_PUSH();
    {
        switch ( sRestoreState )
        {
            case 2:
                if ( sState == 3 )
                {
                    IDE_ASSERT( gSmrChkptThread.unlock()
                                == IDE_SUCCESS );
                    sState = 2;
                }
                setOnlineBackupStatusNOT( SMR_BACKUP_MEMTBS );
                break;
            case 1:
                setOnlineBackupStatusNOT( SMR_BACKUP_DISKTBS );
                break;
            default:
                break;
        }

        switch(sState)
        {
            case 3:
                IDE_ASSERT( gSmrChkptThread.unlock()  == IDE_SUCCESS );
            case 2:
                smLayerCallback::updateSkipCheckSCN( aTrans, ID_FALSE );
            case 1:
                IDE_ASSERT( unlock() == IDE_SUCCESS );
                break;
            default:
                break;
        }

        ideLog::log( SM_TRC_LOG_LEVEL_ABORT, SM_TRC_MRECOVERY_BACKUP_FAIL2 );
    }
    IDE_POP();

    return IDE_FAILURE;
}

/*********************************************************
  function description: smrBackup::switchLogFile
  - ����� ������ �Ŀ� ���� �¶��� �α������� archive log��
   ����޵��� �Ѵ�.
  - ���� alter database backup�������� ����� �ް� �ְų�,
    begin backup ���̸� ������ �ø���.
    -> �̱����� ����� �Ϸ�Ȼ��¿��� �����Ͽ��� �Ѵ�.
  *********************************************************/
IDE_RC smrBackupMgr::swithLogFileByForces()
{

    UInt      sState = 0;
    idBool    sLocked;

    IDE_TEST( mMtxOnlineBackupStatus.trylock( sLocked )
              != IDE_SUCCESS );

    IDE_TEST_RAISE( sLocked == ID_FALSE, error_backup_going );

    sState =1;

    IDE_TEST_RAISE( mOnlineBackupState != SMR_BACKUP_NONE, error_backup_going );

    ideLog::log(SM_TRC_LOG_LEVEL_MRECOV,
                SM_TRC_MRECOVERY_BACKUP_SWITCH_START);

    IDE_TEST( smrLogMgr::switchLogFileByForce() != IDE_SUCCESS );

    ideLog::log( SM_TRC_LOG_LEVEL_MRECOV, SM_TRC_MRECOVERY_BACKUP_SWITCH_END );

    sState = 0;
    IDE_TEST( unlock() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( error_backup_going );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_BACKUP_GOING));
    }
    IDE_EXCEPTION_END;

    IDE_PUSH();
    {
        if ( sState != 0 )
        {
            IDE_ASSERT( unlock() == IDE_SUCCESS );
        }

        ideLog::log( SM_TRC_LOG_LEVEL_ABORT, SM_TRC_MRECOVERY_BACKUP_SWITCH_FAIL );
    }
    IDE_POP();

    return IDE_FAILURE;

}

/*
   ALTER TABLESPACE ... BEGIN BACKUP ��� �����Ѵ�.

  ��� :
  ����Ÿ���� ���� BACKUP ������ ���Ͽ� �߰��� �Լ��̸�, �־���
  ���̺����̽��� ���¸� ��� ����(BACKUP_BEGIN)���� �����Ѵ�.

  PRJ-1548 User Memroy Tablespace ���䵵��

  ���̺����̽��� ������ ������¿� ���� üũ����Ʈ Ÿ���� �޶����ų�
  Ȱ��ȭ/��Ȱ��ȭ�� �ȴ�.

  A. ����������� �޸����̺����̽��� ���� ���
  1. 1���̻��� ��ũ ���̺����̽��� ����������� ���
    => MRDB üũ����Ʈ�� ����ȴ�.
  2. ������� ��ũ ���̺����̽��� ���� ���
    => BOTH üũ����Ʈ�� ����ȴ�.

  B. ����������� �޸����̺����̽��� �ִ� ���
  1. 1���̻��� ��ũ ���̺����̽��� ����������� ���
    => üũ����Ʈ ������� ���� �ֱ���� ����Ѵ�.
  2. ������� ��ũ ���̺����̽��� ���� ���
    =>: DRDB üũ����Ʈ�� ����ȴ�.

  ���� :
  [IN] aSpaceID : BACKUP ������ ���̺����̽� ID

*/
IDE_RC smrBackupMgr::beginBackupTBS( scSpaceID aSpaceID )
{
    UInt                 sRestoreState = 0;
    idBool               sLockedBCKMgr = ID_FALSE;
    sctTableSpaceNode  * sSpaceNode;

    IDE_TEST( lock( NULL /* idvSQL*3 */) != IDE_SUCCESS );
    sLockedBCKMgr = ID_TRUE;

    ideLog::log(SM_TRC_LOG_LEVEL_MRECOV,
                SM_TRC_MRECOVERY_BACKUP_TABLESPACE_BEGIN,
                aSpaceID);

    sSpaceNode = sctTableSpaceMgr::getSpaceNodeBySpaceID( aSpaceID );

    IDE_TEST_RAISE( sSpaceNode == NULL, error_not_found_tablespace_node );

    if( sctTableSpaceMgr::isDiskTableSpace( sSpaceNode ) == ID_TRUE )
    {
        setOnlineBackupStatusOR( SMR_BACKUP_DISKTBS );
        sRestoreState = 1;

        IDE_TEST( beginBackupDiskTBS( NULL,
                                      (sddTableSpaceNode*)sSpaceNode ) != IDE_SUCCESS );

        // PRJ-1548 User Memory Tablespace ���䵵��
        // BACKUP ������ Disk Tablespace ������ ī��Ʈ�Ѵ�.
        mBeginBackupDiskTBSCount++;
    }
    else
    {
        IDE_ERROR( sctTableSpaceMgr::isMemTableSpace( sSpaceNode ) == ID_TRUE );

        setOnlineBackupStatusOR( SMR_BACKUP_MEMTBS );
        sRestoreState = 2;

        // [1] disk table space backup ���� �����
        // ù��° ����Ÿ������ ���¸� backup���� �����Ѵ�.
        IDE_TEST( sctTableSpaceMgr::startTableSpaceBackup(
                      NULL,
                      sSpaceNode ) != IDE_SUCCESS );

        // PRJ-1548 User Memory Tablespace ���䵵��
        // BACKUP ������ Memory Tablespace ������ ī��Ʈ�Ѵ�.
        mBeginBackupMemTBSCount++;
    }

    sLockedBCKMgr = ID_FALSE;
    IDE_TEST( unlock() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( error_not_found_tablespace_node );
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_NotFoundTableSpaceNode,
                                  aSpaceID ));
    }
    IDE_EXCEPTION_END;

    IDE_PUSH();
    {
        switch ( sRestoreState )
        {
            case 2: /* Memory TBS */
            {
                // To fix BUG-21671
                if ( mBeginBackupMemTBSCount == 0 )
                {
                    setOnlineBackupStatusNOT( SMR_BACKUP_MEMTBS );
                }
                else
                {
                    // BUG-21671
                    // ���� begin �������� ���� ������ ���
                    // mOnlineBackupState�� NOT���� �������� ����
                }
                break;
            }
            case 1: /* Disk TBS */
            {
                // To fix BUG-21671
                if ( mBeginBackupDiskTBSCount == 0 )
                {
                    setOnlineBackupStatusNOT( SMR_BACKUP_DISKTBS );
                }
                else
                {
                    // BUG-21671
                    // ���� begin �������� ���� ������ ���
                    // mOnlineBackupState�� NOT���� �������� ����
                }
                break;
            }
            default :
                break;
        }

        if ( sLockedBCKMgr == ID_TRUE )
        {
            IDE_ASSERT( unlock() == IDE_SUCCESS );
        }
    }
    IDE_POP();

    return IDE_FAILURE;
}


/*
  ALTER TABLESPACE ... BEGIN BACKUP ����� ��ũ���̺����̽���
  ����Ÿ������ ���¸� BEGIN_BACKUP ���·� �����Ѵ�.

  ���� :
  [IN] aSpaceID : BACKUP ������ ���̺����̽� ID
*/
IDE_RC smrBackupMgr::beginBackupDiskTBS( idvSQL            * aStatistics,
                                         sddTableSpaceNode * aSpaceNode )
{
    UInt                  sState = 0;
    sddDataFileNode*      sDataFileNode;
    UInt                  i;

    // [1] disk table space backup ���� �����
    // ù��° ����Ÿ������ ���¸� backup���� �����Ѵ�.
    IDE_TEST( sctTableSpaceMgr::startTableSpaceBackup( aStatistics,
                                                       (sctTableSpaceNode*)aSpaceNode ) != IDE_SUCCESS );
    sState = 1;

    for (i=0; i < aSpaceNode->mNewFileID ; i++ )
    {
        sDataFileNode = aSpaceNode->mFileNodeArr[i] ;

        if( sDataFileNode == NULL)
        {
            continue;
        }

        if( SMI_FILE_STATE_IS_DROPPED( sDataFileNode->mState ) )
        {
            continue;
        }

        if( SMI_FILE_STATE_IS_CREATING( sDataFileNode->mState ) ||
            SMI_FILE_STATE_IS_RESIZING( sDataFileNode->mState ) ||
            SMI_FILE_STATE_IS_DROPPING( sDataFileNode->mState ) )
        {
            idlOS::sleep(1);
            continue;
        }

        IDE_DASSERT( SMI_FILE_STATE_IS_ONLINE( sDataFileNode->mState ) );

        // [2] ����Ÿ ������ ���¸� BACKUP ���·� ����
        IDE_TEST( sddDiskMgr::prepareFileBackup(
                      aStatistics,  /* idvSQL* */
                      aSpaceNode,
                      sDataFileNode ) != IDE_SUCCESS );

    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sState != 0 ) // XXXX �� �ּ� ó�� �߾���?
    {
        sddDiskMgr::abortBackupTableSpace( NULL,
                                           aSpaceNode );
    }

    return IDE_FAILURE;
}


/*
  ALTER TABLESPACE ... END BACKUP ����� ��ũ���̺����̽���
  ����Ÿ������ ���¸� END_BACKUP ���·� �����Ѵ�.

  PRJ-1548 User Memroy Tablespace ���䵵��

  ���̺����̽��� ������ ������¿� ���� üũ����Ʈ Ÿ���� �޶����ų�
  Ȱ��ȭ/��Ȱ��ȭ�� �ȴ�.

  A. ����������� �޸����̺����̽��� ���� ���
  1. 1���̻��� ��ũ ���̺����̽��� ����������� ���
    => MRDB üũ����Ʈ�� ����ȴ�.
  2. ������� ��ũ ���̺����̽��� ���� ���
    => BOTH üũ����Ʈ�� ����ȴ�.

  B. ����������� �޸����̺����̽��� �ִ� ���
  1. 1���̻��� ��ũ ���̺����̽��� ����������� ���
    => üũ����Ʈ ������� ���� �ֱ���� ����Ѵ�.
  2. ������� ��ũ ���̺����̽��� ���� ���
    =>: DRDB üũ����Ʈ�� ����ȴ�.

  ���� :
  [IN] aSpaceID : BACKUP ������ ���̺����̽� ID
*/
IDE_RC smrBackupMgr::endBackupTBS( scSpaceID aSpaceID )
{
    idBool              sLockedBCKMgr = ID_FALSE;
    sctTableSpaceNode * sSpaceNode;

    IDE_TEST( lock( NULL /* idvSQL* */) != IDE_SUCCESS );
    sLockedBCKMgr = ID_TRUE;

    IDE_TEST_RAISE( mOnlineBackupState == SMR_BACKUP_NONE,
                    error_no_begin_backup );

    ideLog::log( SM_TRC_LOG_LEVEL_MRECOV,
                 SM_TRC_MRECOVERY_BACKUP_TABLESPACE_END,
                 aSpaceID );
    sSpaceNode = sctTableSpaceMgr::getSpaceNodeBySpaceID( aSpaceID );

    IDE_TEST_RAISE( sSpaceNode == NULL, error_not_found_tablespace_node );

    if ( sctTableSpaceMgr::isDiskTableSpace( sSpaceNode ) == ID_TRUE )
    {
        // ������� ��ũ ���̺����̽��� ���°�� Exception ó��
        IDE_TEST_RAISE( (mOnlineBackupState & SMR_BACKUP_DISKTBS)
                        != SMR_BACKUP_DISKTBS,
                        error_not_begin_backup);

        // ��ũ ���̺����̽��� ����Ϸ������ �����Ѵ�.
        IDE_TEST( sddDiskMgr::endBackupDiskTBS(
                      NULL,  /* idvSQL* */
                      (sddTableSpaceNode*)sSpaceNode ) != IDE_SUCCESS );

        // ���̺����̽��� ������¸� �����ϰ�, MIN PI ��带 �����Ѵ�.
        IDE_TEST( sctTableSpaceMgr::endTableSpaceBackup( NULL,
                                                         sSpaceNode )
                  != IDE_SUCCESS );

        mBeginBackupDiskTBSCount--;

        if ( mBeginBackupDiskTBSCount == 0 )
        {
            setOnlineBackupStatusNOT( SMR_BACKUP_DISKTBS );
        }
    }
    else
    {
        IDE_ERROR (sctTableSpaceMgr::isMemTableSpace( sSpaceNode ) == ID_TRUE );

        // ������� �޸� ���̺����̽��� ���°�� Exception ó��
        IDE_TEST_RAISE((mOnlineBackupState & SMR_BACKUP_MEMTBS)
                       != SMR_BACKUP_MEMTBS,
                       error_not_begin_backup);

        // ���̺����̽��� ������¸� �����Ѵ�.
        IDE_TEST( sctTableSpaceMgr::endTableSpaceBackup( NULL,
                                                         sSpaceNode )
                  != IDE_SUCCESS );

        mBeginBackupMemTBSCount--;

        if ( mBeginBackupMemTBSCount == 0 )
        {
            setOnlineBackupStatusNOT( SMR_BACKUP_MEMTBS );
        }
    }

    sLockedBCKMgr = ID_FALSE;
    IDE_TEST( unlock() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( error_not_found_tablespace_node );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_NotFoundTableSpaceNode,
                                aSpaceID));
    }
    IDE_EXCEPTION(error_no_begin_backup);
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_NoActiveBeginBackup));
    }
    IDE_EXCEPTION(error_not_begin_backup);
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_NotBeginBackup, aSpaceID));
    }
    IDE_EXCEPTION_END;

    IDE_PUSH();
    {
        if ( sLockedBCKMgr == ID_TRUE )
        {
            IDE_ASSERT( unlock() == IDE_SUCCESS );
        }
    }
    IDE_POP();

    return IDE_FAILURE;

}

/*********************************************************************** 
 * Database ��ü�� Incremental Backup�Ѵ�.
 * PROJ-2133 Incremental backup
 *
 * aTrans       - [IN]  
 * aBackupDir   - [IN] incremental backup ��ġ -- <���� �������ʴ� ����>
 * aBackupLevel - [IN] ����Ǵ� incremental backup Level
 * aBackupType  - [IN] ����Ǵ� incremental backup type
 * aBackupTag   - [IN] ����Ǵ� incremental backup tag �̸�
 *
 **********************************************************************/
IDE_RC smrBackupMgr::incrementalBackupDatabase(
                                     idvSQL            * aStatistics,
                                     void              * aTrans,
                                     SChar             * aBackupDir,
                                     smiBackuplevel      aBackupLevel,
                                     UShort              aBackupType,
                                     SChar             * aBackupTag )
{
    idBool              sLocked;
    idBool              sIsDirCreate = ID_FALSE;
    smriBISlot          sCommonBackupInfo;
    SChar               sIncrementalBackupPath[ SM_MAX_FILE_NAME ];
    smrLogAnchorMgr   * sAnchorMgr;
    smriBIFileHdr     * sBIFileHdr;
    smLSN               sMemRecvLSNOfDisk;    
    smLSN               sDiskRedoLSN;
    smLSN               sMemEndLSN;
    SInt                sSystemErrno;
    UInt                sDeleteArchLogFileNo = 0;
    UInt                sState = 0;
    UInt                sRestoreState = 0;
    smLSN               sLastBackupLSN;

    IDE_DASSERT( aTrans != NULL );

    mBackupBISlotCnt = SMRI_BI_INVALID_SLOT_CNT;

    ideLog::log(SM_TRC_LOG_LEVEL_MRECOV, SM_TRC_MRECOVERY_BACKUP_START2);

    IDE_TEST( mMtxOnlineBackupStatus.trylock( sLocked )
                    != IDE_SUCCESS );

    IDE_TEST_RAISE( sLocked == ID_FALSE, error_backup_going );
    sState = 1;

    // ALTER DATABASE BACKUP TABLESPACE OR DATABASE ������
    // ALTER TABLESPACE .. BEGIN BACKUP ���� ����� ������ üũ�Ѵ�.
    IDE_TEST_RAISE( mOnlineBackupState != SMR_BACKUP_NONE,
                    error_backup_going );

    /* aBackupDir�� �����ϴ��� Ȯ���Ѵ�. �������� �ʴٸ� ���丮�� �����Ѵ�. */
    IDE_TEST( setBackupInfoAndPath( aBackupDir, 
                                    aBackupLevel, 
                                    aBackupType, 
                                    SMRI_BI_BACKUP_TARGET_DATABASE, 
                                    aBackupTag,
                                    &sCommonBackupInfo,
                                    sIncrementalBackupPath,
                                    ID_TRUE )
              != IDE_SUCCESS );
    sIsDirCreate = ID_TRUE;

    // Aging ����������� �� Ʈ������� ������� �ʰ� ����������
    // �����ϰ� �Ѵ�. (����) smxTransMgr::getMemoryMinSCN()
    smLayerCallback::updateSkipCheckSCN( aTrans, ID_TRUE );
    sState = 2;

    // [1] üũ����Ʈ�� �߻���Ų��.
    IDE_TEST( gSmrChkptThread.resumeAndWait( aStatistics )
              != IDE_SUCCESS );

    // [2] BACKUP �������� MEMORY TABLESPACE�� ������¸� �����Ѵ�.
    setOnlineBackupStatusOR( SMR_BACKUP_MEMTBS );
    sRestoreState = 2;

    IDE_TEST( smriBackupInfoMgr::lock() != IDE_SUCCESS );
    sState = 3;

    IDE_TEST( smriBackupInfoMgr::getBIFileHdr( &sBIFileHdr ) != IDE_SUCCESS );
    mBackupBISlotCnt = sBIFileHdr->mBISlotCnt;

    /* BUG-37371
     * level 0 ����� �������� �ʴ°�� level 1 ����� �����ϵ��� �Ѵ�. */
    IDE_TEST_RAISE( (mBackupBISlotCnt == 0) && 
                    (aBackupLevel == SMI_BACKUP_LEVEL_1),
                    there_is_no_level0_backup );

    IDE_TEST( gSmrChkptThread.lock( aStatistics ) != IDE_SUCCESS );
    sState = 4;

    /* 
     * incremental backup ���� archiving�� log file���� �����ص� ������
     * archive log file��ȣ�� ���Ѵ�. logAnchor�� ����� DiskRedoLSN��
     * MemEndLSN�� ���� LSN�� ���ϰ�, ���� LSN�� ���� log file ��ȣ
     * ���� 1 ���� log���ϱ��� ���� �Ҽ��ִ� archivelogfile�� �����Ѵ�.
     */
    sAnchorMgr = smrRecoveryMgr::getLogAnchorMgr();

    sAnchorMgr->getEndLSN( &sMemEndLSN );

    sMemRecvLSNOfDisk = sMemEndLSN;
    sDiskRedoLSN      = sAnchorMgr->getDiskRedoLSN();

    if ( smrCompareLSN::isGTE(&sMemRecvLSNOfDisk, &sDiskRedoLSN)
         == ID_TRUE )
    {
        sMemEndLSN = sDiskRedoLSN;
    }

    if ( sMemEndLSN.mFileNo != 0 )
    {
        /* 
         * recovery�� �ʿ��� log���� ��ȣ���� 1 ���� log������ �����Ҽ�
         * �ִ� archivelog���Ϸ� �����Ѵ�.
         */
        sDeleteArchLogFileNo = sMemEndLSN.mFileNo - 1;
    }
    else
    {
        /* 
         * log���� ��ȣ�� 0�̸� ���� ������ log file�� �������� 
         * ID_UINT_MAX�� �����Ѵ�.
         */
        sDeleteArchLogFileNo = ID_UINT_MAX;
    }

    // [3] ������ MEMORY TABLESPACE�� ���¼����� ����� �����Ѵ�.
    // BUG-27204 Database ��� �� session event check���� ����
    IDE_TEST( smmTBSMediaRecovery::incrementalBackupAllMemoryTBS(
                             aStatistics,
                             aTrans,
                             &sCommonBackupInfo,
                             sIncrementalBackupPath) != IDE_SUCCESS );

    smrLogMgr::getLstLSN( &sLastBackupLSN );

    IDE_TEST( smrRecoveryMgr::updateBIFileAttrToLogAnchor( 
                                                    NULL,
                                                    NULL,
                                                    &sLastBackupLSN,
                                                    NULL,
                                                    &sDeleteArchLogFileNo )
              != IDE_SUCCESS );

    // [4] Loganchor ������ ����Ѵ�.
    IDE_TEST( smrBackupMgr::backupLogAnchor( aStatistics,
                                             sIncrementalBackupPath )
              != IDE_SUCCESS );

    sState = 3;
    IDE_TEST( gSmrChkptThread.unlock() != IDE_SUCCESS );

    // [5] BACKUP �������� MEMORY TABLESPACE�� ������¸� �����Ѵ�.
    setOnlineBackupStatusNOT( SMR_BACKUP_MEMTBS );
    sRestoreState = 0;

    // [6] BACKUP �������� DISK TABLESPACE�� ������¸� �����Ѵ�.
    setOnlineBackupStatusOR( SMR_BACKUP_DISKTBS );
    sRestoreState = 1;

    // [7] DISK TABLESPACE���� ������¸� �����ϰ� ����Ѵ�.
    IDE_TEST( sddDiskMgr::incrementalBackupAllDiskTBS( aStatistics,
                                                       &sCommonBackupInfo,
                                                       sIncrementalBackupPath )
              != IDE_SUCCESS );
    
    IDE_TEST( smriBackupInfoMgr::flushBIFile( SMRI_BI_INVALID_SLOT_IDX, 
                                              &sLastBackupLSN ) 
              != IDE_SUCCESS );

    /* backup info ������ ����Ѵ�. */
    if( smriBackupInfoMgr::backup( sIncrementalBackupPath ) 
        != IDE_SUCCESS )
    {
        sSystemErrno = ideGetSystemErrno();
 
        IDE_TEST_RAISE(isRemainSpace(sSystemErrno) == ID_TRUE,
                       error_backup_datafile);
 
        IDE_RAISE(err_online_backup_write);
    }

    sState = 2;
    IDE_TEST( smriBackupInfoMgr::unlock() != IDE_SUCCESS );

    setOnlineBackupStatusNOT( SMR_BACKUP_DISKTBS );
    sRestoreState = 0;

    // [8] ������ �ش� �α������� switch �Ͽ� archive ��Ų��.
    IDE_TEST( smrLogMgr::switchLogFileByForce() != IDE_SUCCESS );

    sState = 1;
    smLayerCallback::updateSkipCheckSCN( aTrans, ID_FALSE );

    sState = 0;
    IDE_TEST( unlock() != IDE_SUCCESS );

    ideLog::log( SM_TRC_LOG_LEVEL_MRECOV, SM_TRC_MRECOVERY_BACKUP_END2 );

    return IDE_SUCCESS;

    IDE_EXCEPTION(there_is_no_level0_backup);
    {
        IDE_SET( ideSetErrorCode(smERR_ABORT_Cannot_Perform_Level1_Backup) );
    }
    IDE_EXCEPTION(err_online_backup_write);
    {
        ideLog::log( SM_TRC_LOG_LEVEL_ABORT, SM_TRC_MRECOVERY_BACKUP_ABORT1 );

        IDE_SET( ideSetErrorCode(smERR_ABORT_BackupWrite) );
    }
    IDE_EXCEPTION(error_backup_datafile);
    {
        ideLog::log( SM_TRC_LOG_LEVEL_ABORT,
                     SM_TRC_MRECOVERY_BACKUP_ABORT2,
                     smrRecoveryMgr::getBIFileName() );

        IDE_SET( ideSetErrorCode( smERR_ABORT_BackupDatafile,
                                  "BACKUP INFOMATON FILE",
                                  smrRecoveryMgr::getBIFileName() ) );
    }
    IDE_EXCEPTION( error_backup_going );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_BACKUP_GOING));
    }
    IDE_EXCEPTION_END;

    IDE_PUSH();
    {
        switch ( sRestoreState )
        {
            case 2:
                if ( sState == 4 )
                {
                    IDE_ASSERT( gSmrChkptThread.unlock()
                                == IDE_SUCCESS );
                    sState = 3;
                }
                setOnlineBackupStatusNOT( SMR_BACKUP_MEMTBS );
                break;
            case 1:
                setOnlineBackupStatusNOT( SMR_BACKUP_DISKTBS );
                break;
            default:
                break;
        }

        switch(sState)
        {
            case 4:
                IDE_ASSERT( gSmrChkptThread.unlock()  == IDE_SUCCESS );
            case 3:
                IDE_ASSERT( smriBackupInfoMgr::unlock() == IDE_SUCCESS );
            case 2:
                smLayerCallback::updateSkipCheckSCN( aTrans, ID_FALSE );
            case 1:
                IDE_ASSERT( unlock() == IDE_SUCCESS );
                break;
            default:
                break;
        }

        if( mBackupBISlotCnt != SMRI_BI_INVALID_SLOT_CNT )
        {
            IDE_ASSERT( smriBackupInfoMgr::rollbackBIFile( mBackupBISlotCnt ) 
                        == IDE_SUCCESS );
        }

        if( sIsDirCreate == ID_TRUE )
        {
           IDE_ASSERT( smriBackupInfoMgr::removeBackupDir( 
                                        sIncrementalBackupPath ) 
                       == IDE_SUCCESS );
        }

        ideLog::log( SM_TRC_LOG_LEVEL_ABORT, SM_TRC_MRECOVERY_BACKUP_FAIL2 );
    }
    IDE_POP();

    return IDE_FAILURE;
}

/*********************************************************************** 
 * �����̺� �����̽��� Incremental Backup�Ѵ�.
 * PROJ-2133 Incremental backup
 *
 * aTrans       - [IN]  
 * aSpaceID     - [IN] ����Ϸ��� ���̺� �����̽� ID
 * aBackupDir   - [IN] incremental backup ��ġ --<���� ������ �ʴ� ����>
 * aBackupLevel - [IN] ����Ǵ� incremental backup Level
 * aBackupType  - [IN] ����Ǵ� incremental backup type
 * aBackupTag   - [IN] ����Ǵ� incremental backup tag
 *
 **********************************************************************/
IDE_RC smrBackupMgr::incrementalBackupTableSpace( idvSQL            * aStatistics,
                                                  void              * aTrans,    
                                                  scSpaceID           aSpaceID,
                                                  SChar             * aBackupDir,
                                                  smiBackuplevel      aBackupLevel,
                                                  UShort              aBackupType,
                                                  SChar             * aBackupTag,
                                                  idBool              aCheckTagName )
{
    smriBISlot          sCommonBackupInfo;
    sctTableSpaceNode * sSpaceNode = NULL;
    smriBIFileHdr     * sBIFileHdr;
    SChar               sIncrementalBackupPath[ SM_MAX_FILE_NAME ];
    smLSN               sLastBackupLSN;
    SInt                sSystemErrno;
    UInt                sState = 0;
    UInt                sRestoreState = 0;
    idBool              sLocked;
    idBool              sIsDirCreate = ID_FALSE;

    IDE_DASSERT( aTrans != NULL );

    if( aCheckTagName == ID_TRUE )
    {
        mBackupBISlotCnt = SMRI_BI_INVALID_SLOT_CNT;
    }
    else
    {
        /* nothing to do */
    }

    IDE_TEST( mMtxOnlineBackupStatus.trylock(sLocked) != IDE_SUCCESS );

    IDE_TEST_RAISE( sLocked == ID_FALSE, error_backup_going);
    sState = 1;

    // alter database backup tablespace or database�Ӹ��ƴ϶�,
    // alter tablespace begin backup ��η� �ü� �ֱ� ������,
    // ������ ���� �˻��Ѵ�.
    IDE_TEST_RAISE( mOnlineBackupState != SMR_BACKUP_NONE,
                    error_backup_going );

    /* aBackupDir�� �����ϴ��� Ȯ���Ѵ�. �������� �ʴٸ� ���丮�� �����Ѵ�. */
    IDE_TEST( setBackupInfoAndPath( aBackupDir, 
                                    aBackupLevel, 
                                    aBackupType, 
                                    SMRI_BI_BACKUP_TARGET_TABLESPACE, 
                                    aBackupTag,
                                    &sCommonBackupInfo,
                                    sIncrementalBackupPath,
                                    aCheckTagName )
              != IDE_SUCCESS );
    sIsDirCreate = ID_TRUE;

    // Aging ����������� �� Ʈ������� ������� �ʰ� ����������
    // �����ϰ� �Ѵ�. (����) smxTransMgr::getMemoryMinSCN()
    smLayerCallback::updateSkipCheckSCN( aTrans, ID_TRUE );
    sState = 2;

    // üũ����Ʈ�� �߻���Ų��.
    IDE_TEST( gSmrChkptThread.resumeAndWait( aStatistics )
              != IDE_SUCCESS );

    ideLog::log( SM_TRC_LOG_LEVEL_MRECOV, SM_TRC_MRECOVERY_BACKUP_START);

    IDE_TEST( smriBackupInfoMgr::lock() != IDE_SUCCESS );
    sState = 3;
    
    if( aCheckTagName == ID_TRUE )
    {
        IDE_TEST( smriBackupInfoMgr::getBIFileHdr( &sBIFileHdr ) != IDE_SUCCESS );
        mBackupBISlotCnt = sBIFileHdr->mBISlotCnt;
    }
    else
    {
        /* nothing to do */
    }

    sSpaceNode = sctTableSpaceMgr::getSpaceNodeBySpaceID( aSpaceID );

    IDE_TEST_RAISE( sSpaceNode == NULL ,
                    error_not_found_tablespace_node );

    IDE_TEST_RAISE( ( (sSpaceNode->mState & SMI_TBS_DROPPED)   == SMI_TBS_DROPPED ) ||
                    ( (sSpaceNode->mState & SMI_TBS_DISCARDED) == SMI_TBS_DISCARDED ),
                    error_not_found_tablespace_node );

    IDE_TEST( sctTableSpaceMgr::startTableSpaceBackup( aStatistics,
                                                       sSpaceNode )
              != IDE_SUCCESS );
    sState = 4;

    // Backup Flag ���� �� �ѹ� �� Ȯ����.
    IDE_TEST_RAISE( (sSpaceNode->mState & SMI_TBS_DROPPED) == SMI_TBS_DROPPED,
                    error_not_found_tablespace_node );

    switch( sctTableSpaceMgr::getTBSLocation( sSpaceNode ) )
    {
        case SMI_TBS_MEMORY:
            {
                setOnlineBackupStatusOR( SMR_BACKUP_MEMTBS );
                sRestoreState = 2;

                IDE_TEST( incrementalBackupMemoryTBS( aStatistics,
                                                      (smmTBSNode*)sSpaceNode,
                                                      sIncrementalBackupPath,
                                                      &sCommonBackupInfo )
                          != IDE_SUCCESS );

                setOnlineBackupStatusNOT( SMR_BACKUP_MEMTBS );
                sRestoreState = 0;
            }
            break;
        case SMI_TBS_DISK:
            {
                setOnlineBackupStatusOR( SMR_BACKUP_DISKTBS );
                sRestoreState = 1;

                IDE_TEST( incrementalBackupDiskTBS( aStatistics,
                                                    (sddTableSpaceNode*)sSpaceNode,
                                                    sIncrementalBackupPath,
                                                    &sCommonBackupInfo )
                          != IDE_SUCCESS );

                setOnlineBackupStatusNOT( SMR_BACKUP_DISKTBS );
                sRestoreState = 0;
            }
            break;
        case SMI_TBS_VOLATILE:
            {
                // Nothing to do...
                // volatile tablespace�� ���ؼ��� ����� �������� �ʴ´�.
            }
            break;
        default:
            IDE_ASSERT(0);
            break;
    }

    sState = 3;
    IDE_TEST( sctTableSpaceMgr::endTableSpaceBackup( aStatistics,
                                                     sSpaceNode ) != IDE_SUCCESS );
    /* backup����� tablespace�ΰ�� sDeleteArchLogFileNo ������ �������� �ʴ´�.
     * sDeleteArchLogFileNo ������ �����ϰ� archivelog�� �����Ұ�� ������ ����� 
     * database backup�� ������ �Ұ����� ����.
     */
    smrLogMgr::getLstLSN( &sLastBackupLSN );
    IDE_TEST( smrRecoveryMgr::updateBIFileAttrToLogAnchor( 
                                                    NULL,
                                                    NULL,
                                                    &sLastBackupLSN,
                                                    NULL,
                                                    NULL )
              != IDE_SUCCESS );

    IDE_TEST( smriBackupInfoMgr::flushBIFile( SMRI_BI_INVALID_SLOT_IDX, 
                                              &sLastBackupLSN ) 
              != IDE_SUCCESS );

    /* backup info ������ ����Ѵ�. */
    if( smriBackupInfoMgr::backup( sIncrementalBackupPath ) 
        != IDE_SUCCESS )
    {
        sSystemErrno = ideGetSystemErrno();
 
        IDE_TEST_RAISE(isRemainSpace(sSystemErrno) == ID_TRUE,
                       error_backup_datafile);
 
        IDE_RAISE(err_online_backup_write);
    }

    sState = 2;
    IDE_TEST( smriBackupInfoMgr::unlock() != IDE_SUCCESS );

    // ������ �ش� �α������� switch �Ͽ� archive ��Ų��.
    IDE_TEST( smrLogMgr::switchLogFileByForce() != IDE_SUCCESS );
    sState = 1;

    smLayerCallback::updateSkipCheckSCN( aTrans, ID_FALSE );

    sState = 0;
    IDE_TEST( unlock() != IDE_SUCCESS );

    ideLog::log( SM_TRC_LOG_LEVEL_MRECOV, SM_TRC_MRECOVERY_BACKUP_END );

    return IDE_SUCCESS;

    IDE_EXCEPTION( error_not_found_tablespace_node );
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_NotFoundTableSpaceNode,
                                 aSpaceID) );
    }
    IDE_EXCEPTION(err_online_backup_write);
    {
        ideLog::log( SM_TRC_LOG_LEVEL_ABORT, SM_TRC_MRECOVERY_BACKUP_ABORT1 );

        IDE_SET( ideSetErrorCode(smERR_ABORT_BackupWrite) );
    }
    IDE_EXCEPTION(error_backup_datafile);
    {
        ideLog::log( SM_TRC_LOG_LEVEL_ABORT,
                     SM_TRC_MRECOVERY_BACKUP_ABORT2,
                     smrRecoveryMgr::getBIFileName() );

        IDE_SET( ideSetErrorCode( smERR_ABORT_BackupDatafile,
                                  "BACKUP INFOMATON FILE",
                                  smrRecoveryMgr::getBIFileName() ) );
    }
    IDE_EXCEPTION(error_backup_going);
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_BACKUP_GOING));
    }
    IDE_EXCEPTION_END;

    IDE_PUSH();
    {
        switch( sRestoreState )
        {
            case 2:
                setOnlineBackupStatusNOT ( SMR_BACKUP_MEMTBS );
                break;
            case 1:
                setOnlineBackupStatusNOT ( SMR_BACKUP_DISKTBS );
                break;
            default:
                break;
        }

        switch( sState )
        {
            case 4:
                (void)sctTableSpaceMgr::endTableSpaceBackup( aStatistics,
                                                             sSpaceNode ) ;
            case 3:
                IDE_ASSERT( smriBackupInfoMgr::unlock() == IDE_SUCCESS );
            case 2:
                smLayerCallback::updateSkipCheckSCN( aTrans, ID_FALSE );
            case 1:
                IDE_ASSERT( unlock() == IDE_SUCCESS );
                break;
            default:
                break;
        }

        if( mBackupBISlotCnt != SMRI_BI_INVALID_SLOT_CNT )
        {
            IDE_ASSERT( smriBackupInfoMgr::rollbackBIFile( mBackupBISlotCnt ) 
                        == IDE_SUCCESS );
        }

        if( sIsDirCreate == ID_TRUE )
        {
           IDE_ASSERT( smriBackupInfoMgr::removeBackupDir( 
                                        sIncrementalBackupPath ) 
                       == IDE_SUCCESS );
        }

        ideLog::log(SM_TRC_LOG_LEVEL_ABORT, SM_TRC_MRECOVERY_BACKUP_FAIL);
    }
    IDE_POP();

    return IDE_FAILURE;

}


/*********************************************************************** 
 * �޸� ���̺����̽��� Incremental Backup�Ѵ�.
 * PROJ-2133 Incremental backup
 *
 * aSpaceID           - [IN] ����� ���̺����̽� ID
 * aBackupDir         - [IN] incremental backup ��ġ
 * aCommonBackupInfo  - [IN] backupinfo ���� ����
 *
 **********************************************************************/
IDE_RC smrBackupMgr::incrementalBackupMemoryTBS( idvSQL     * aStatistics,
                                                 smmTBSNode * aSpaceNode,
                                                 SChar      * aBackupDir,
                                                 smriBISlot * aCommonBackupInfo )
{
    UInt                     i;
    UInt                     sWhichDB;
    UInt                     sDataFileNameLen;
    UInt                     sBmpExtListIdx;
    SInt                     sSystemErrno;
    smmDatabaseFile        * sDatabaseFile;
    smriBISlot               sBackupInfo;
    smriCTDataFileDescSlot * sDataFileDescSlot; 
    smmChkptImageHdr         sChkptImageHdr;
    smriCTState              sTrackingState;

    IDE_DASSERT( aCommonBackupInfo  != NULL );
    IDE_DASSERT( aBackupDir         != NULL );
    IDE_DASSERT( sctTableSpaceMgr::isMemTableSpace( aSpaceNode ) == ID_TRUE );

    // [2] memory database file���� copy�Ѵ�.
    sWhichDB = smmManager::getCurrentDB( aSpaceNode );

    idlOS::memcpy( &sBackupInfo, aCommonBackupInfo, ID_SIZEOF(smriBISlot) );

    sBackupInfo.mDBFilePageCnt = 
        smmDatabase::getDBFilePageCount( aSpaceNode->mMemBase);

    for ( i = 0; i <= aSpaceNode->mLstCreatedDBFile; i++ )
    {
        idlOS::snprintf(sBackupInfo.mBackupFileName, SMRI_BI_MAX_BACKUP_FILE_NAME_LEN,
                        "%s%s-%"ID_UINT32_FMT"-%"ID_UINT32_FMT"_%s%s%s",
                        aBackupDir,
                        aSpaceNode->mHeader.mName,
                        0, //incremental backup������ pinpong��ȣ�� 0���� �����Ѵ�.
                        i,
                        SMRI_BI_BACKUP_TAG_PREFIX,
                        sBackupInfo.mBackupTag,
                        SMRI_INCREMENTAL_BACKUP_FILE_EXT);

        sDataFileNameLen = idlOS::strlen(sBackupInfo.mBackupFileName);

        IDE_TEST( sctTableSpaceMgr::makeValidABSPath(
                                              ID_TRUE,
                                              sBackupInfo.mBackupFileName,
                                              &sDataFileNameLen,
                                              SMI_TBS_MEMORY) // MEMORY
                  != IDE_SUCCESS );

        IDE_TEST( smmManager::openAndGetDBFile( aSpaceNode,
                                                sWhichDB,
                                                i,
                                                &sDatabaseFile )
                  != IDE_SUCCESS );

        IDE_TEST_RAISE( idlOS::strcmp(sBackupInfo.mBackupFileName,
                                      sDatabaseFile->getFileName())  
                        == 0, error_self_copy);   

        IDE_TEST( sDatabaseFile->readChkptImageHdr( &sChkptImageHdr ) 
                  != IDE_SUCCESS );

        smriChangeTrackingMgr::getDataFileDescSlot( 
                            &sChkptImageHdr.mDataFileDescSlotID,
                            &sDataFileDescSlot );

        sBackupInfo.mBeginBackupTime = smLayerCallback::getCurrTime();
        sBackupInfo.mSpaceID         = aSpaceNode->mHeader.mID;
        sBackupInfo.mFileNo          = i;
        

        /* changeTracking manager�� Ȱ��ȭ �ȰͰ� ������� ���������Ͽ� ����
         * ���������� ������ �������� ���۵ȴ�. ������������ �߰��ǰ� �ּ���
         * �ѹ��� level 0����� ���� �Ǿ������ ���������Ͽ� ����������
         * ����ȴ�.
         */
        sTrackingState = (smriCTState)idCore::acpAtomicGet32( 
                                    &sDataFileDescSlot->mTrackingState );

        /* incremental backup�� �����Ҽ� �ִ� ������ �Ǵ��� Ȯ���Ѵ�.
         * incremental backup�� ���������� �������� ���ϸ� full backup����
         * backup�Ѵ�.
         */
        if( ( sTrackingState == SMRI_CT_TRACKING_ACTIVE ) &&
            ( sBackupInfo.mBackupLevel == SMI_BACKUP_LEVEL_1 ) )
        {
            if ( sDatabaseFile->incrementalBackup( sDataFileDescSlot,
                                                   sDatabaseFile,
                                                   &sBackupInfo )
                 != IDE_SUCCESS )
            {
                sSystemErrno = ideGetSystemErrno();
 
                IDE_TEST_RAISE(isRemainSpace(sSystemErrno) == ID_TRUE,
                               error_backup_datafile);
 
                IDE_RAISE(err_online_backup_write);
            }
        }
        else
        {
            sBackupInfo.mBackupType          |= SMI_BACKUP_TYPE_FULL;

            /* full backup�� ��������� ��� BmpExtList�� Bitmap�� 0����
             * �ʱ�ȭ�Ѵ�. 
             */
            for( sBmpExtListIdx = 0; 
                 sBmpExtListIdx < SMRI_CT_DATAFILE_DESC_BMP_EXT_LIST_CNT; 
                 sBmpExtListIdx++ )
            {
                IDE_TEST( smriChangeTrackingMgr::initBmpExtListBlocks(
                                                            sDataFileDescSlot, 
                                                            sBmpExtListIdx )
                          != IDE_SUCCESS );
            }

            (void)idCore::acpAtomicSet32( &sDataFileDescSlot->mTrackingState, 
                                          (acp_sint32_t)SMRI_CT_TRACKING_ACTIVE);

            /* BUG-18678: Memory/Disk DB File�� /tmp�� Online Backup�Ҷ� Disk��
               Backup�� �����մϴ�.*/
            if ( sDatabaseFile->copy( aStatistics,
                                      sBackupInfo.mBackupFileName )
                 != IDE_SUCCESS )
            {
                sSystemErrno = ideGetSystemErrno();
 
                IDE_TEST_RAISE(isRemainSpace(sSystemErrno) == ID_TRUE,
                               error_backup_datafile);
 
                IDE_RAISE(err_online_backup_write);
            }
        }

        sBackupInfo.mEndBackupTime = smLayerCallback::getCurrTime();

        smriBackupInfoMgr::setBI2BackupFileHdr( 
                                        &sChkptImageHdr.mBackupInfo,
                                        &sBackupInfo );

        IDE_TEST( sDatabaseFile->setDBFileHeaderByPath( 
                                        sBackupInfo.mBackupFileName, 
                                        &sChkptImageHdr ) 
                  != IDE_SUCCESS );

        IDE_TEST( sDatabaseFile->getFileSize( &sBackupInfo.mOriginalFileSize )
                  != IDE_SUCCESS );

        if( smriBackupInfoMgr::appendBISlot( &sBackupInfo ) 
            != IDE_SUCCESS )
        {
            sSystemErrno = ideGetSystemErrno();
 
            IDE_TEST_RAISE(isRemainSpace(sSystemErrno) == ID_TRUE,
                           error_backup_datafile);
 
            IDE_RAISE(err_online_backup_write);
        }

        ideLog::log(SM_TRC_LOG_LEVEL_MRECOV,
                    SM_TRC_MRECOVERY_BACKUP_INFO1,
                    sDatabaseFile->getFileName(),
                    sBackupInfo.mBackupFileName);

        IDE_TEST_RAISE(iduCheckSessionEvent(aStatistics) != IDE_SUCCESS,
                       error_abort_backup);
    }

    /* ------------------------------------------------
     * [2] alter table������ �߻��� Backup Internal Database File
     * ----------------------------------------------*/

    
    errno = 0;
    if ( smLayerCallback::copyAllTableBackup( smLayerCallback::getBackupDir(),
                                              aBackupDir )
         != IDE_SUCCESS )
    {
        sSystemErrno = ideGetSystemErrno();
        IDE_TEST_RAISE(isRemainSpace(sSystemErrno) == ID_TRUE,
                       err_backup_internalbackup_file);

        IDE_RAISE(err_online_backup_write);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(error_self_copy);
    {
        IDE_SET( ideSetErrorCode(smERR_ABORT_SelfCopy) );
    }
    IDE_EXCEPTION(err_online_backup_write);
    {
        ideLog::log( SM_TRC_LOG_LEVEL_ABORT, SM_TRC_MRECOVERY_BACKUP_ABORT1 );

        IDE_SET( ideSetErrorCode(smERR_ABORT_BackupWrite) );
    }
    IDE_EXCEPTION(error_backup_datafile);
    {
        ideLog::log( SM_TRC_LOG_LEVEL_ABORT,
                     SM_TRC_MRECOVERY_BACKUP_ABORT2,
                     sBackupInfo.mBackupFileName );

        IDE_SET( ideSetErrorCode( smERR_ABORT_BackupDatafile,
                                  "MEMORY TABLESPACE",
                                  sBackupInfo.mBackupFileName ) );
    }
    IDE_EXCEPTION(err_backup_internalbackup_file);
    {
        ideLog::log( SM_TRC_LOG_LEVEL_ABORT,
                     SM_TRC_MRECOVERY_BACKUP_ABORT3,
                     sBackupInfo.mBackupFileName );
        IDE_SET( ideSetErrorCode( smERR_ABORT_BackupDatafile,
                                 "BACKUPED INTERNAL FILE",
                                  "" ) );
    }
    IDE_EXCEPTION(error_abort_backup);
    {
        ideLog::log(SM_TRC_LOG_LEVEL_ABORT, SM_TRC_MRECOVERY_BACKUP_ABORT4);
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************** 
 * ��ũ ���̺����̽��� Incremental Backup�Ѵ�.
 * PROJ-2133 Incremental backup
 *
 * aSpaceNode         - [IN] ����� ���̺����̽� Node
 * aBackupDir         - [IN] incremental backup ��ġ
 * aCommonBackupInfo  - [IN] backupinfo ���� ����
 *
 **********************************************************************/
IDE_RC smrBackupMgr::incrementalBackupDiskTBS( idvSQL            * aStatistics,
                                               sddTableSpaceNode * aSpaceNode,
                                               SChar             * aBackupDir, 
                                               smriBISlot        * aCommonBackupInfo )
{
    SInt                        sSystemErrno;
    UInt                        sState = 0;
    UInt                        sDataFileNameLen;
    UInt                        sBmpExtListIdx;
    sddDataFileNode           * sDataFileNode = NULL;
    sddDataFileHdr              sDataFileHdr;
    smriBISlot                  sBackupInfo;
    SChar                     * sDataFileName;
    smriCTDataFileDescSlot    * sDataFileDescSlot; 
    smriCTState                 sTrackingState;
    UInt                        i;
    UShort                      sLockListID;

    IDE_DASSERT( aBackupDir != NULL );
    IDE_DASSERT( sctTableSpaceMgr::isDiskTableSpace( aSpaceNode ) == ID_TRUE );

    idlOS::memcpy( &sBackupInfo, aCommonBackupInfo, ID_SIZEOF(smriBISlot) );

    /* ------------------------------------------------
     * ���̺� �����̽�  ����Ÿ ���ϵ�  copy.
     * ----------------------------------------------*/
    for (i=0; i < aSpaceNode->mNewFileID ; i++ )
    {
        sDataFileNode = aSpaceNode->mFileNodeArr[i] ;

        if( sDataFileNode == NULL)
        {
            continue;
        }

        if( SMI_FILE_STATE_IS_DROPPED( sDataFileNode->mState ) )
        {
            continue;
        }

        if( SMI_FILE_STATE_IS_CREATING( sDataFileNode->mState ) ||
            SMI_FILE_STATE_IS_RESIZING( sDataFileNode->mState ) ||
            SMI_FILE_STATE_IS_DROPPING( sDataFileNode->mState ) )
        {
            idlOS::sleep(1);
            continue;
        }

        smriChangeTrackingMgr::getDataFileDescSlot( 
                            &sDataFileNode->mDBFileHdr.mDataFileDescSlotID,
                            &sDataFileDescSlot );

        idlOS::memset(sBackupInfo.mBackupFileName,
                      0x00,
                      SMRI_BI_MAX_BACKUP_FILE_NAME_LEN);
        sDataFileName = idlOS::strrchr( sDataFileNode->mName,
                                        IDL_FILE_SEPARATOR );
        IDE_TEST_RAISE(sDataFileName == NULL,error_backupdir_file_path);

        /* ------------------------------------------------
         * BUG-11206�� ���� source�� destination�� ���Ƽ�
         * ���� ����Ÿ ������ ���ǵǴ� ��츦 �������Ͽ�
         * ������ ���� ��Ȯ�� path�� ������.
         * ----------------------------------------------*/
        sDataFileName = sDataFileName+1;

        idlOS::snprintf(sBackupInfo.mBackupFileName, SMRI_BI_MAX_BACKUP_FILE_NAME_LEN,
                        "%s%s_%s%s%s",
                        aBackupDir,
                        sDataFileName,
                        SMRI_BI_BACKUP_TAG_PREFIX,
                        sBackupInfo.mBackupTag,
                        SMRI_INCREMENTAL_BACKUP_FILE_EXT);

        sDataFileNameLen = idlOS::strlen(sBackupInfo.mBackupFileName);

        IDE_TEST( sctTableSpaceMgr::makeValidABSPath(
                      ID_TRUE,
                      sBackupInfo.mBackupFileName,
                      &sDataFileNameLen,
                      SMI_TBS_DISK)
                  != IDE_SUCCESS );

        IDE_TEST_RAISE(idlOS::strcmp(sDataFileNode->mFile.getFileName(),
                                     sBackupInfo.mBackupFileName) == 0,
                       error_self_copy);

        /* ------------------------------------------------
         * [3] ����Ÿ ������ ���¸� backup begin���� �����Ѵ�.
         * ----------------------------------------------*/

        IDE_TEST(sddDiskMgr::prepareFileBackup( aStatistics,
                                                aSpaceNode,
                                                sDataFileNode )
                 != IDE_SUCCESS);
        sState = 1;
                        
        IDE_TEST( sddDiskMgr::readDBFHdr( NULL, 
                                          sDataFileNode, 
                                          &sDataFileHdr ) != IDE_SUCCESS );


        sBackupInfo.mBeginBackupTime = smLayerCallback::getCurrTime();
        sBackupInfo.mSpaceID         = aSpaceNode->mHeader.mID;
        sBackupInfo.mFileID          = sDataFileNode->mID;

        /* changeTracking manager�� Ȱ��ȭ �ȰͰ� ������� ���������Ͽ� ����
         * ���������� ������ �������� ���۵ȴ�. ������������ �߰��ǰ� �ּ���
         * �ѹ��� level 0����� ���� �Ǿ������ ���������Ͽ� ����������
         * ����ȴ�.
         */
        sTrackingState = (smriCTState)idCore::acpAtomicGet32( 
                                    &sDataFileDescSlot->mTrackingState );

        /* incremental backup�� �����Ҽ� �ִ� ������ �Ǵ��� Ȯ���Ѵ�.
         * incremental backup�� ���������� �������� ���ϸ� full backup����
         * backup�Ѵ�.
         */
        if( ( sTrackingState == SMRI_CT_TRACKING_ACTIVE ) &&
            ( sBackupInfo.mBackupLevel == SMI_BACKUP_LEVEL_1 ) )
        {
            /* incremental backup ���� */
            if( sddDiskMgr::incrementalBackup( aStatistics,
                                               aSpaceNode,
                                               sDataFileDescSlot,
                                               sDataFileNode,
                                               &sBackupInfo )
                != IDE_SUCCESS )
            {
                sSystemErrno = ideGetSystemErrno();
                IDE_TEST_RAISE( isRemainSpace(sSystemErrno) == ID_TRUE,
                                error_backup_datafile );
                IDE_RAISE(err_online_backup_write);
            }
        }
        else
        {
            sBackupInfo.mBackupType          |= SMI_BACKUP_TYPE_FULL;

            IDU_FIT_POINT("smrBackupMgr::incrementalBackupDiskTBS::wait");

            /* 
             * BUG-40604 The bit count in a changeTracking`s bitmap is not same 
             * with that in a changeTracking`s datafile descriptor 
             * SetBit �Լ����� ���ü� ������ �ذ��ϱ� ���� Bitmap extent��X latch�� ��´�.
             * ���ü� ������ �߻��� �� �ִ�, ���� �������� differrential bitmap list�� 
             * latch�� ȹ���ϸ� �ȴ�.
             * cumulative bitmap �� ���� �������� differrential bitmap list�� 
             * latch�� ���� ���ü� ������ �����ϴ�.
             */
            sLockListID = sDataFileDescSlot->mCurTrackingListID;
            IDE_TEST( smriChangeTrackingMgr::lockBmpExtHdrLatchX( sDataFileDescSlot, 
                        sLockListID ) 
                    != IDE_SUCCESS );
            sState = 2;
            
            /* full backup�� ��������� ��� BmpExtList�� Bitmap�� 0����
             * �ʱ�ȭ�Ѵ�. 
             */
            for( sBmpExtListIdx = 0; 
                 sBmpExtListIdx < SMRI_CT_DATAFILE_DESC_BMP_EXT_LIST_CNT; 
                 sBmpExtListIdx++ )
            {
                IDE_TEST( smriChangeTrackingMgr::initBmpExtListBlocks(
                                                            sDataFileDescSlot, 
                                                            sBmpExtListIdx )
                          != IDE_SUCCESS );
            }

            sState = 1;
            IDE_TEST( smriChangeTrackingMgr::unlockBmpExtHdrLatchX( sDataFileDescSlot, 
                        sLockListID ) 
                    != IDE_SUCCESS );
            
            (void) idCore::acpAtomicSet32( &sDataFileDescSlot->mTrackingState, 
                                           (acp_sint32_t)SMRI_CT_TRACKING_ACTIVE );
            
            /* Full backup ���� */
            if ( sddDiskMgr::copyDataFile( aStatistics,
                                           aSpaceNode,
                                           sDataFileNode,
                                           sBackupInfo.mBackupFileName )
                 != IDE_SUCCESS )
            {
                sSystemErrno = ideGetSystemErrno();
                IDE_TEST_RAISE( isRemainSpace(sSystemErrno) == ID_TRUE,
                                error_backup_datafile );
                IDE_RAISE(err_online_backup_write);
            }
        }

        sBackupInfo.mEndBackupTime = smLayerCallback::getCurrTime();

        smriBackupInfoMgr::setBI2BackupFileHdr( 
                                        &sDataFileHdr.mBackupInfo,
                                        &sBackupInfo );

        IDE_TEST( sddDataFile::writeDBFileHdrByPath( sBackupInfo.mBackupFileName,
                                                     &sDataFileHdr )
                  != IDE_SUCCESS );

        sBackupInfo.mOriginalFileSize = sDataFileNode->mCurrSize;

        if( smriBackupInfoMgr::appendBISlot( &sBackupInfo ) 
            != IDE_SUCCESS )
        {
            sSystemErrno = ideGetSystemErrno();
            IDE_TEST_RAISE( isRemainSpace(sSystemErrno) == ID_TRUE,
                            error_backup_datafile );
            IDE_RAISE(err_online_backup_write);
        }

        ideLog::log(SM_TRC_LOG_LEVEL_MRECOV,
                    SM_TRC_MRECOVERY_BACKUP_INFO2,
                    aSpaceNode->mHeader.mName,
                    sDataFileNode->mFile.getFileName(),
                    sBackupInfo.mBackupFileName);

        IDE_TEST_RAISE(iduCheckSessionEvent(aStatistics) != IDE_SUCCESS,
                       error_abort_backup);

        /* ------------------------------------------------
         * [4] ����Ÿ ������ ����� ������ ������, PIL hash��
         * logg������ ����Ÿ ���Ͽ� �����Ѵ�.
         * ----------------------------------------------*/
        sState = 0;
        sddDiskMgr::completeFileBackup( aStatistics,
                                        aSpaceNode,
                                        sDataFileNode );
    }


    return IDE_SUCCESS;

    IDE_EXCEPTION(error_self_copy);
    {
       IDE_SET( ideSetErrorCode(smERR_ABORT_SelfCopy));
    }
    IDE_EXCEPTION(err_online_backup_write);
    {
        ideLog::log(SM_TRC_LOG_LEVEL_ABORT,
                    SM_TRC_MRECOVERY_BACKUP_ABORT1);
        IDE_SET(ideSetErrorCode(smERR_ABORT_BackupWrite));
    }
    IDE_EXCEPTION(error_backup_datafile);
    {
        ideLog::log(SM_TRC_LOG_LEVEL_ABORT,
                    SM_TRC_MRECOVERY_BACKUP_ABORT5,
                    sBackupInfo.mBackupFileName);
        IDE_SET(ideSetErrorCode(smERR_ABORT_BackupDatafile,"DISK TABLESPACE",
                                sBackupInfo.mBackupFileName ));
    }
    IDE_EXCEPTION(error_backupdir_file_path);
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_InvalidFilePathABS));
    }
    IDE_EXCEPTION(error_abort_backup);
    {
        ideLog::log(SM_TRC_LOG_LEVEL_ABORT,
                    SM_TRC_MRECOVERY_BACKUP_ABORT4);

    }
    IDE_EXCEPTION_END;

    switch ( sState )
    {
        case 2:
            IDE_ASSERT( smriChangeTrackingMgr::unlockBmpExtHdrLatchX( sDataFileDescSlot, 
                        sLockListID ) 
                    == IDE_SUCCESS );
        case 1:
            sddDiskMgr::completeFileBackup( aStatistics,
                                            aSpaceNode,
                                            sDataFileNode );
            break;
        default:
            break;
    }

    return IDE_FAILURE;


}

/*********************************************************************** 
 * ����� backupinfo�� �����ϰ� incremental backup������ ������ ���丮
 * �� �����Ѵ�.
 * PROJ-2133 Incremental backup
 *
 * aBackupDir               - [IN] backup�� ��ġ
 * aBackupLevel             - [IN] ����Ǵ� backup Level
 * aBackupType              - [IN] ����Ǵ� backup Type
 * aBackupTarget            - [IN] backup�� ������ ���
 * aBackupTag               - [IN] backup�� ������ tag�̸�
 * aCommonBackupInfo        - [IN/OUT] ����� ��� ����
 * aIncrementalBackupPath   - [OUT] backup�� ��ġ 

 **********************************************************************/
IDE_RC smrBackupMgr::setBackupInfoAndPath( 
                                    SChar            * aBackupDir,
                                    smiBackupLevel     aBackupLevel,
                                    UShort             aBackupType,
                                    smriBIBackupTarget aBackupTarget,
                                    SChar            * aBackupTag,
                                    smriBISlot       * aCommonBackupInfo,
                                    SChar            * aIncrementalBackupPath,
                                    idBool             aCheckTagName )
{
    UInt        sBackupPathLen;
    time_t      sBackupBeginTime;
    struct tm   sBackupBegin;
    SChar     * sBackupDir;

    idlOS::memset( aCommonBackupInfo, 0x0, ID_SIZEOF( smriBISlot ) );
    idlOS::memset( &sBackupBegin, 0, ID_SIZEOF( struct tm ) );

    sBackupBeginTime = (time_t)smLayerCallback::getCurrTime();
    idlOS::localtime_r( (time_t *)&sBackupBeginTime, &sBackupBegin );
    
    if( aBackupTag == NULL )
    {
        if( aCheckTagName == ID_TRUE )
        {
            idlOS::snprintf( aCommonBackupInfo->mBackupTag, 
                             SMI_MAX_BACKUP_TAG_NAME_LEN,
                             "%04"ID_UINT32_FMT
                             "%02"ID_UINT32_FMT
                             "%02"ID_UINT32_FMT
                             "_%02"ID_UINT32_FMT
                             "%02"ID_UINT32_FMT
                             "%02"ID_UINT32_FMT,
                             sBackupBegin.tm_year + 1900,
                             sBackupBegin.tm_mon + 1,
                             sBackupBegin.tm_mday,
                             sBackupBegin.tm_hour,
                             sBackupBegin.tm_min,
                             sBackupBegin.tm_sec );

            idlOS::strncpy( mLastBackupTagName, 
                            aCommonBackupInfo->mBackupTag, 
                            SMI_MAX_BACKUP_TAG_NAME_LEN );
        }
        else
        {
            idlOS::strncpy( aCommonBackupInfo->mBackupTag, 
                            mLastBackupTagName, 
                            SMI_MAX_BACKUP_TAG_NAME_LEN );
        }
    }
    else
    {
        idlOS::snprintf( aCommonBackupInfo->mBackupTag, 
                         SMI_MAX_BACKUP_TAG_NAME_LEN,
                         "%s",
                         aBackupTag );

    }
    aCommonBackupInfo->mBackupLevel  = aBackupLevel;
    aCommonBackupInfo->mBackupType  |= aBackupType;
    aCommonBackupInfo->mBackupTarget = aBackupTarget;

    if( aBackupDir == NULL )
    {

        sBackupDir = smrRecoveryMgr::getIncrementalBackupDirFromLogAnchor(); 
        
        IDE_TEST_RAISE( idlOS::strcmp( sBackupDir, 
                                       SMRI_BI_INVALID_BACKUP_PATH ) == 0,
                         error_not_defined_backup_path );        

        idlOS::snprintf(aIncrementalBackupPath, 
                        SM_MAX_FILE_NAME,
                        "%s%s%s%c",
                        sBackupDir,
                        SMRI_BI_BACKUP_TAG_PREFIX,
                        aCommonBackupInfo->mBackupTag,
                        IDL_FILE_SEPARATOR );
        
    }
    else
    {
        sBackupPathLen = idlOS::strlen( aBackupDir );

        if( aBackupDir[sBackupPathLen -1 ] == IDL_FILE_SEPARATOR)
        {

            idlOS::snprintf(aIncrementalBackupPath, 
                            SM_MAX_FILE_NAME,
                            "%s%s%s%c",
                            aBackupDir,
                            SMRI_BI_BACKUP_TAG_PREFIX,
                            aCommonBackupInfo->mBackupTag,
                            IDL_FILE_SEPARATOR );
        }
        else
        {
            idlOS::snprintf(aIncrementalBackupPath, 
                            SM_MAX_FILE_NAME,
                            "%s%c%s%s%c",
                            aBackupDir,
                            IDL_FILE_SEPARATOR,
                            SMRI_BI_BACKUP_TAG_PREFIX,
                            aCommonBackupInfo->mBackupTag,
                            IDL_FILE_SEPARATOR );
        }

    }

    if( aCheckTagName == ID_TRUE )
    {
        IDE_TEST_RAISE( idf::access(aIncrementalBackupPath, F_OK) == 0,
                        error_incremental_backup_dir_already_exist );

        IDE_TEST_RAISE( idlOS::mkdir( aIncrementalBackupPath, 
                        S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH ) != 0, // 755����
                        error_create_path );
    }
    else
    {
        /* nothing to do */
    }

    IDE_TEST_RAISE( idf::access(aIncrementalBackupPath, F_OK) != 0,
                    error_not_exist_path );

    return IDE_SUCCESS;

    IDE_EXCEPTION( error_not_exist_path );
    {  
        IDE_SET(ideSetErrorCode( smERR_ABORT_NoExistPath, 
                                 aIncrementalBackupPath));
    }
    IDE_EXCEPTION( error_create_path );
    {  
        IDE_SET(ideSetErrorCode( smERR_ABORT_FailToCreateDirectory, 
                                 aIncrementalBackupPath));
    }
    IDE_EXCEPTION( error_not_defined_backup_path );
    {  
        IDE_SET(ideSetErrorCode( smERR_ABORT_NotDefinedIncrementalBackupPath ));
    }  
    IDE_EXCEPTION( error_incremental_backup_dir_already_exist );
    {  
        IDE_SET(ideSetErrorCode( smERR_ABORT_AlreadyExistIncrementalBackupPath, 
                                 aIncrementalBackupPath ));
    }  

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
