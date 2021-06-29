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
 * $Id: smiBackup.cpp 82075 2018-01-17 06:39:52Z jina.kim $
 **********************************************************************/

/**********************************************************************
 * FILE DESCRIPTION : smiBackup.cpp
 *--------------------------------------------------------------------- 
 *  backup����� ����ϴ� ����̸� ������ ���� ����� �����Ѵ�.
 *
 * -  table space�� ���� hot backup���.
 *  : disk table space.
 *  : memory table space.
 * -  log anchor���� backup���.
 * -  DB��ü�� ����  backup��  ���.
 **********************************************************************/

#include <ide.h>
#include <smErrorCode.h>
#include <smr.h>
#include <smiBackup.h>
#include <smiTrans.h>
#include <smiMain.h>
#include <sctTableSpaceMgr.h>
#include <smriChangeTrackingMgr.h>
#include <smriBackupInfoMgr.h>

/*********************************************************
 * function description: archivelog mode ����
 * archivelog ��带 Ȱ��ȭ �Ǵ� ��Ȱ��ȭ ��ŵ�ϴ�.
 * alter database �������� startup control �ܰ迡���� ����� ����
 * �մϴ�.
 *
 * sql - alter database [archivelog|noarchivelog]
 * 
 * + archivelog ����� Ȱ��ȭ
 *  - ����Ÿ���̽��� �����մϴ�.
 *    : shutdown abort
 *  - startup control ���·� ����ϴ�.
 *  - alter database [archivelog|noarchivelog] �����մϴ�.
 *  - ����Ÿ���̽��� �����մϴ�.
 *    : shutdown abort
 *  - ����Ÿ���̽��� ��ü ���� ����� �����մϴ�.
 *    : ��Ȱ��ȭ => Ȱ��ȭ ����� ��� ��� ����Ÿ������ ����ؾ��Ѵ�.
 *    ����Ÿ���̽��� noarchive ��忡�� ��Ǵ� ������
 *    ��� ����� �������� ���̻� ����� �� ����.
 *    archivelog ���� �ٲ��Ŀ� ������ ���ο� ����� ��������
 *    ��� archive redo �α� ������ ����� �Ϳ� ����� ����̴�.
 * 
 * + archivelog ����� ��Ȱ��ȭ
 *  - ����Ÿ���̽��� �����մϴ�.
 *    : shutdown abort
 *  - startup control ���·� ����ϴ�.
 *  - alter database [archivelog|noarchivelog] �����մϴ�.
 *  - ����Ÿ���̽��� �����մϴ�.
 *    : shutdown abort
 *********************************************************/
IDE_RC smiBackup::alterArchiveMode( smiArchiveMode  aArchiveMode,
                                    idBool          aCheckPhase )
{

    IDE_DASSERT((aArchiveMode == SMI_LOG_ARCHIVE) ||
                (aArchiveMode == SMI_LOG_NOARCHIVE));

    if (aCheckPhase == ID_TRUE)
    {
        // �ٴܰ� startup�ܰ� ��, control�ܰ迡��
        // archive mode�� �����Ҽ� �ִ�.
        IDE_TEST_RAISE(smiGetStartupPhase() != SMI_STARTUP_CONTROL,
                       err_cannot_alter_archive_mode);
        
    }
    
    IDE_TEST( smrRecoveryMgr::updateArchiveMode(aArchiveMode) 
            != IDE_SUCCESS );

    return IDE_SUCCESS;
    
    IDE_EXCEPTION(err_cannot_alter_archive_mode);
    {        
        IDE_SET(ideSetErrorCode(smERR_ABORT_MediaRecoDataFile,
                           "ALTER DATABASE ARCHIVELOG or NOARCHIVELOG"));
    }
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
    
}

/*********************************************************
 * function description: alter system archive log start ����
 * startup open meta ���� ���Ŀ����� archivelog thread��
 * Ȱ��ȭ��ų�� �ִ�.
 *
 * + archivelog thread�� Ȱ��ȭ
 *   - ����Ÿ���̽��� startup meta �������� Ȯ���Ѵ�.
 *   - ����Ÿ���̽��� archivelog ���� Ȱ��ȭ �Ǿ� �ִ���
 *     Ȯ���Ѵ�. (archive log list)
 *   - alter system archivelog start�� �����Ͽ� thread��
 *     �����Ų��.
 *********************************************************/
IDE_RC smiBackup::startArchThread()
{

    IDE_TEST_RAISE(smiGetStartupPhase() < SMI_STARTUP_META,
                   err_startup_phase);

    IDE_TEST_RAISE(smrRecoveryMgr::getArchiveMode() != SMI_LOG_ARCHIVE,
                   err_archivelog_mode);
    
    if ( smrLogMgr::getArchiveThread().isStarted() == ID_FALSE )
    {
        ideLog::log(SM_TRC_LOG_LEVEL_INTERFACE,
                    SM_TRC_INTERFACE_ARCHIVE_THREAD_START);
        IDE_TEST_RAISE( smrLogMgr::startupLogArchiveThread() != IDE_SUCCESS,
                       err_cannot_start_thread);
        ideLog::log(SM_TRC_LOG_LEVEL_INTERFACE,
                    SM_TRC_INTERFACE_ARCHIVE_THREAD_SUCCESS);
    }
    else
    {
        /* nothing to do ... */
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION(err_archivelog_mode);
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_ErrArchiveLogMode));
    }
    IDE_EXCEPTION( err_startup_phase );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_WrongStartupPhase));
    }
    IDE_EXCEPTION( err_cannot_start_thread );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_CanStartARCH));
    }
    IDE_EXCEPTION_END;
    
    ideLog::log(SM_TRC_LOG_LEVEL_INTERFACE,
                SM_TRC_INTERFACE_ARCHIVE_THREAD_FAILURE);

    return IDE_FAILURE;
}

/*********************************************************
 * function description: alter system archive log stop ����
 * startup open meta ���� ���Ŀ����� archivelog thread��
 * Ȱ��ȭ��ų�� �ִ�.
 *
 * + archivelog thread�� Ȱ��ȭ
 *   - ����Ÿ���̽��� startup meta �������� Ȯ���Ѵ�.
 *   - ����Ÿ���̽��� archivelog ���� Ȱ��ȭ �Ǿ� �ִ���
 *     Ȯ���Ѵ�. (archive log list)
 *   - alter system archivelog stop�� �����Ͽ� thread��
 *     ������Ų��.
 *********************************************************/
IDE_RC smiBackup::stopArchThread()
{

    IDE_TEST_RAISE(smiGetStartupPhase() < SMI_STARTUP_META,
                   err_startup_phase);

    IDE_TEST_RAISE(smrRecoveryMgr::getArchiveMode()
                   != SMI_LOG_ARCHIVE,
                   err_archivelog_mode);
    
    if ( smrLogMgr::getArchiveThread().isStarted() == ID_TRUE )
    {
        
        ideLog::log(SM_TRC_LOG_LEVEL_INTERFACE,
                    SM_TRC_INTERFACE_ARCHIVE_THREAD_SHUTDOWN);
        IDE_TEST( smrLogMgr::shutdownLogArchiveThread() != IDE_SUCCESS );
        ideLog::log(SM_TRC_LOG_LEVEL_INTERFACE,
                    SM_TRC_INTERFACE_ARCHIVE_THREAD_SUCCESS);
    }
    else
    {
        /* nothing to do ... */
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION(err_archivelog_mode);
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_ErrArchiveLogMode));
    }
    IDE_EXCEPTION( err_startup_phase );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_WrongStartupPhase));
    }
    IDE_EXCEPTION_END;
    
    ideLog::log(SM_TRC_LOG_LEVEL_INTERFACE,
                SM_TRC_INTERFACE_ARCHIVE_THREAD_FAILURE);
    
    return IDE_FAILURE;
}


/*********************************************************
  function description: smiBackup::backupLogAnchor
  - logAnchor������ destnation directroy�� copy�Ѵ�.
  *********************************************************/
IDE_RC smiBackup::backupLogAnchor( idvSQL*   aStatistics,
                                   SChar*    aDestFilePath )
{

    IDE_DASSERT( aDestFilePath != NULL );
    
    IDE_TEST_RAISE(smiGetStartupPhase() < SMI_STARTUP_META,
                   err_startup_phase);

    IDE_TEST_RAISE(smrRecoveryMgr::getArchiveMode() != SMI_LOG_ARCHIVE,
                   err_backup_mode);
        
    // logAnchor������ destnation directroy�� copy�Ѵ�.
    IDE_TEST( smrBackupMgr::backupLogAnchor( aStatistics,
                                             aDestFilePath )
              != IDE_SUCCESS );
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION( err_startup_phase );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_WrongStartupPhase));
    }
    IDE_EXCEPTION( err_backup_mode );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_BackupLogMode,"ALTER DATABASE BACKUP LOGANCHOR"));
    }
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

/*********************************************************
 * function description: smiBackup::backupTableSpace
 * ���ڷ� �־��� ���̺� �����̽��� ���Ͽ� hot backup
 * �� �����ϸ�, �־���  ���̺� �����̽��� ����Ÿ ���ϵ��� �����Ѵ�.       
 **********************************************************/
IDE_RC smiBackup::backupTableSpace(idvSQL*      aStatistics,
                                   smiTrans*    aTrans,
                                   scSpaceID    aTbsID,
                                   SChar*       aBackupDir)
{

    IDE_DASSERT(aTrans != NULL);
    IDE_DASSERT(aBackupDir != NULL);
    
    IDE_TEST_RAISE(smiGetStartupPhase() < SMI_STARTUP_META,
                   err_startup_phase);

    IDE_TEST_RAISE(smrRecoveryMgr::getArchiveMode() != SMI_LOG_ARCHIVE,
                   err_backup_mode);
    
    // BUG-17213 Volatile TBS�� ���ؼ��� backup ����� ���´�.
    IDE_TEST_RAISE(sctTableSpaceMgr::isVolatileTableSpace(aTbsID)
                   == ID_TRUE, err_volatile_backup);

    IDE_TEST(smrBackupMgr::backupTableSpace(aStatistics,
                                            aTrans->getTrans(),
                                            aTbsID,
                                            aBackupDir)
             != IDE_SUCCESS);
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION( err_startup_phase );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_WrongStartupPhase));
    }
    IDE_EXCEPTION( err_backup_mode );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_BackupLogMode,"ALTER DATABASE BACKUP TABLESPACE"));
    }
    IDE_EXCEPTION( err_volatile_backup );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_UNABLE_TO_BACKUP_FOR_VOLATILE_TABLESPACE));
    }
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

/*********************************************************
  function description: smiBackup::backupDatabase
  - Database�� �ִ� ��� ���̺� �����̽��� ���Ͽ�
    hot backup�� �����Ͽ�, �� ���̺� �����̽���
    ����Ÿ ������ �����Ѵ�.
    
  - log anchor���ϵ��� �����Ѵ�.
  
  - DB backup�� �޴� ����  ���� active log ���ϵ���
    archive log�� �����  �����Ѵ�.
  *********************************************************/
IDE_RC smiBackup::backupDatabase( idvSQL* aStatistics,
                                  smiTrans* aTrans,
                                  SChar*    aBackupDir )
{

    IDE_DASSERT( aTrans != NULL );
    IDE_DASSERT( aBackupDir != NULL );
    
    IDE_TEST_RAISE(smiGetStartupPhase() < SMI_STARTUP_META,
                   err_startup_phase);

    IDE_TEST_RAISE(smrRecoveryMgr::getArchiveMode() != SMI_LOG_ARCHIVE,
                   err_backup_mode);

    IDE_TEST(smrBackupMgr::backupDatabase(aStatistics,
                                          aTrans->getTrans(),
                                          aBackupDir)
             != IDE_SUCCESS);
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION( err_startup_phase );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_WrongStartupPhase));
    }
    IDE_EXCEPTION( err_backup_mode );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_BackupLogMode,"ALTER DATABASE BACKUP DATABASE"));
    }
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
    
}

/*********************************************************
  function description: smiBackup::beginBackupTBS
  - veritas�� ���� backup ������ ���Ͽ� �߰��� �Լ��̸�,
   �־��� ���̺� �����̽��� ���¸� ��� ����(BACKUP_BEGIN)
   ���� �����Ѵ�.
  �޸� tablespace�� ��쿡�� checkpoint�� disable ��Ų��. 
  *********************************************************/
IDE_RC smiBackup::beginBackupTBS( scSpaceID aSpaceID )
{

    
    IDE_TEST_RAISE(smiGetStartupPhase() < SMI_STARTUP_META,
                   err_startup_phase);

    IDE_TEST_RAISE(smrRecoveryMgr::getArchiveMode() != SMI_LOG_ARCHIVE,
                   err_backup_mode);
    
    IDE_TEST(smrBackupMgr::beginBackupTBS(aSpaceID)
             != IDE_SUCCESS);
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION( err_startup_phase );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_WrongStartupPhase));
    }
    IDE_EXCEPTION( err_backup_mode );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_BackupLogMode,
                              "ALTER TABLESPACE BEGIN BACKUP"));
    }
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
    
}

/*********************************************************
  function description: smiBackup::endBackupTBS
  - veritas�� ���� backup ������ ���Ͽ� �߰��� �Լ��̸�,
   �־��� ���̺� �����̽��� ���¸� ��� ������ online���·�
   �����Ų��.
   
  �޸� tablespace�� ��쿡�� checkpoint�� enable ��Ų��. 
  *********************************************************/
IDE_RC smiBackup::endBackupTBS( scSpaceID aSpaceID )
{

    
    IDE_TEST_RAISE(smiGetStartupPhase() < SMI_STARTUP_META,
                   err_startup_phase);

    IDE_TEST_RAISE(smrRecoveryMgr::getArchiveMode() != SMI_LOG_ARCHIVE,
                   err_backup_mode);
    
    IDE_TEST(smrBackupMgr::endBackupTBS(aSpaceID)
             != IDE_SUCCESS);
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION( err_startup_phase );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_WrongStartupPhase));
    }
    IDE_EXCEPTION( err_backup_mode );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_BackupLogMode,
                              "ALTER TABLESPACE END BACKUP"));
    }
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
    
}

/*********************************************************
  function description: smiBackup::switchLogFile
  - ����� ������ �Ŀ� ���� �¶��� �α������� archive log��
   ����޵��� �Ѵ�.
  *********************************************************/
IDE_RC smiBackup::switchLogFile()
{
    
    IDE_TEST_RAISE(smiGetStartupPhase() < SMI_STARTUP_META,
                   err_startup_phase);
    
    IDE_TEST_RAISE(smrRecoveryMgr::getArchiveMode() != SMI_LOG_ARCHIVE,
                   err_backup_mode);
    
    
    IDE_TEST( smrBackupMgr::swithLogFileByForces()
              != IDE_SUCCESS);
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION( err_startup_phase );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_WrongStartupPhase));
    }
    IDE_EXCEPTION( err_backup_mode );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_BackupLogMode,
                                "ALTER SYSTEM SWITCH LOGFILE"));
    }
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
    
}

/*********************************************************
  function description: smiBackup::incrementalBackupDatabase
    PROJ-2133 incremental backup
  - Database�� �ִ� ��� ���̺� �����̽��� ���Ͽ�
    incrementalb backup�� �����Ѵ�.
  *********************************************************/
IDE_RC smiBackup::incrementalBackupDatabase( idvSQL*           aStatistics,
                                             smiTrans*         aTrans,
                                             smiBackupLevel    aBackupLevel,
                                             UShort            aBackupType,
                                             SChar*            aBackupDir,
                                             SChar*            aBackupTag )
{
    
    IDE_DASSERT( aTrans != NULL );

    IDE_DASSERT( ( aBackupLevel == SMI_BACKUP_LEVEL_0 ) ||
                 ( aBackupLevel == SMI_BACKUP_LEVEL_1 ) );

    IDE_DASSERT( (( aBackupType & SMI_BACKUP_TYPE_FULL ) != 0) ||
                 (( aBackupType & SMI_BACKUP_TYPE_DIFFERENTIAL ) != 0) ||
                 (( aBackupType & SMI_BACKUP_TYPE_CUMULATIVE ) != 0) );

    IDE_TEST_RAISE(smiGetStartupPhase() < SMI_STARTUP_META,
                   err_startup_phase);
    
    IDE_TEST_RAISE(smrRecoveryMgr::getArchiveMode() != SMI_LOG_ARCHIVE,
                   err_backup_mode);
    
    IDE_TEST_RAISE(smrRecoveryMgr::isCTMgrEnabled() != ID_TRUE,
                   err_change_traking_state);
    
    IDE_TEST( smrBackupMgr::incrementalBackupDatabase( aStatistics,
                                                       aTrans->getTrans(),
                                                       aBackupDir,
                                                       aBackupLevel,
                                                       aBackupType,
                                                       aBackupTag )
              != IDE_SUCCESS);
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION( err_startup_phase );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_WrongStartupPhase));
    }
    IDE_EXCEPTION( err_backup_mode );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_BackupLogMode,
                                "ALTER DATABASE INCREMENTAL BACKUP DATABASE"));
    }
    IDE_EXCEPTION( err_change_traking_state );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_ChangeTrackingState));
    }
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
    
}

/*********************************************************
  function description: smiBackup::incrementalBackupDatabase
    PROJ-2133 incremental backup
  - Database�� �ִ� ��� ���̺� �����̽��� ���Ͽ�
    incrementalb backup�� �����Ѵ�.
  *********************************************************/
IDE_RC smiBackup::incrementalBackupTableSpace( idvSQL*           aStatistics,
                                               smiTrans*         aTrans,
                                               scSpaceID         aSpaceID,
                                               smiBackupLevel    aBackupLevel,
                                               UShort            aBackupType,
                                               SChar*            aBackupDir,
                                               SChar*            aBackupTag,
                                               idBool            aCheckTagName )
{
    IDE_DASSERT( aTrans != NULL );
    IDE_DASSERT( ( aBackupLevel == SMI_BACKUP_LEVEL_0 ) ||
                 ( aBackupLevel == SMI_BACKUP_LEVEL_1 ) );

    IDE_DASSERT( (( aBackupType & SMI_BACKUP_TYPE_FULL ) != 0) ||
                 (( aBackupType & SMI_BACKUP_TYPE_DIFFERENTIAL ) != 0) ||
                 (( aBackupType & SMI_BACKUP_TYPE_CUMULATIVE ) != 0) );

    IDE_TEST_RAISE(smiGetStartupPhase() < SMI_STARTUP_META,
                   err_startup_phase);
    
    IDE_TEST_RAISE(smrRecoveryMgr::getArchiveMode() != SMI_LOG_ARCHIVE,
                   err_backup_mode);
    
    IDE_TEST_RAISE(smrRecoveryMgr::isCTMgrEnabled() != ID_TRUE,
                   err_change_traking_state);
    
    IDE_TEST( smrBackupMgr::incrementalBackupTableSpace( aStatistics,
                                                         aTrans->getTrans(),
                                                         aSpaceID,
                                                         aBackupDir,
                                                         aBackupLevel,
                                                         aBackupType,
                                                         aBackupTag,
                                                         aCheckTagName )
              != IDE_SUCCESS);
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION( err_startup_phase );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_WrongStartupPhase));
    }
    IDE_EXCEPTION( err_backup_mode );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_BackupLogMode,
                                "ALTER DATABASE INCREMENTAL BACKUP TABLESPACE"));
    }
    IDE_EXCEPTION( err_change_traking_state );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_ChangeTrackingState));
    }
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
    
}

/*********************************************************
  function description: smiBackup::enableChangeTracking
    PROJ-2133 incremental backup
  - incremental backup�� �����ϱ� ���� �����ͺ��̽��� 
    ���� ��� ���������ϵ��� ���� ������ �����ϴ� �����
    enable ��Ų��.
  *********************************************************/
IDE_RC smiBackup::enableChangeTracking()
{
    UInt    sState = 0;

    IDE_TEST_RAISE( smiGetStartupPhase() < SMI_STARTUP_META,
                    err_startup_phase );

    IDE_TEST_RAISE( smrRecoveryMgr::getArchiveMode() != SMI_LOG_ARCHIVE,
                    err_backup_mode );
    
    IDE_TEST_RAISE( smrRecoveryMgr::isCTMgrEnabled() != ID_FALSE,
                    err_change_traking_state );

    IDE_TEST( smriChangeTrackingMgr::createCTFile() != IDE_SUCCESS );
    sState = 1;

    IDE_TEST( smriBackupInfoMgr::createBIFile() !=IDE_SUCCESS );
    sState = 2;
    
    IDE_TEST( smriBackupInfoMgr::begin() != IDE_SUCCESS );
    sState = 3;

    return IDE_SUCCESS;
    
    IDE_EXCEPTION( err_startup_phase );
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_WrongStartupPhase ) );
    }
    IDE_EXCEPTION( err_backup_mode );
    {
        IDE_SET(ideSetErrorCode( smERR_ABORT_BackupLogMode,
                                 "ALTER DATABASE ENABLE INCREMENTAL CHUNK CHANGE TRACKING" ) );
    }
    IDE_EXCEPTION( err_change_traking_state );
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_ChangeTrackingState ) );
    }
    IDE_EXCEPTION_END;

    switch( sState )
    {
        case 3: 
            IDE_ASSERT( smriBackupInfoMgr::end() == IDE_SUCCESS );
        case 2:
            IDE_ASSERT( smriBackupInfoMgr::removeBIFile() == IDE_SUCCESS );
        case 1:
            IDE_ASSERT( smriChangeTrackingMgr::removeCTFile() == IDE_SUCCESS );
        default:
            break;
    }
    return IDE_FAILURE;
}

/*********************************************************
  function description: smiBackup::disableChangeTracking
    PROJ-2133 incremental backup
  - incremental backup�� �����ϱ� ���� �����ͺ��̽��� 
    ���� ��� ���������ϵ��� ���� ������ �����ϴ� ����� 
    disable ��Ų��.
  *********************************************************/
IDE_RC smiBackup::disableChangeTracking()
{
    IDE_TEST( smriChangeTrackingMgr::removeCTFile() != IDE_SUCCESS );

    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************
  function description: smiBackup::removeObsoleteBackupFile
    PROJ-2133 incremental backup
  - �����Ⱓ�� ���� incremental backupfile���� �����Ѵ�.
  *********************************************************/
IDE_RC smiBackup::removeObsoleteBackupFile()
{
    IDE_TEST_RAISE(smiGetStartupPhase() < SMI_STARTUP_CONTROL,
                   err_startup_phase);

    IDE_TEST( smriBackupInfoMgr::removeObsoleteBISlots() != IDE_SUCCESS );

    return IDE_SUCCESS;
    
    IDE_EXCEPTION( err_startup_phase );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_WrongStartupPhase));
    }
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

/*********************************************************
  function description: smiBackup::changeBackupDir
    PROJ-2133 incremental backup
  - incremental backupfile���� ������ directory�� �����Ѵ�.
  *********************************************************/
IDE_RC smiBackup::changeIncrementalBackupDir( SChar * aNewBackupDir )
{
    SChar sPath[SM_MAX_FILE_NAME];

    IDE_DASSERT( aNewBackupDir != NULL );

    IDE_TEST_RAISE(smiGetStartupPhase() < SMI_STARTUP_CONTROL,
                   err_startup_phase);

    IDE_TEST_RAISE(smrRecoveryMgr::isCTMgrEnabled() != ID_TRUE,
                   err_change_traking_state);

    if( smuProperty::getIncrementalBackupPathMakeABSPath() == 1 )
    {
        /* aNewBackupDir�� �����ΰ� �ƴ��� Ȯ���Ѵ�. */
        IDE_TEST( smriBackupInfoMgr::isValidABSPath( ID_TRUE, aNewBackupDir ) 
                  != IDE_FAILURE );

        idlOS::snprintf(sPath, SM_MAX_FILE_NAME,
                        "%s%c%s",                      
                        smuProperty::getDefaultDiskDBDir(),
                        IDL_FILE_SEPARATOR,            
                        aNewBackupDir);
    }
    else
    {
        idlOS::snprintf(sPath, SM_MAX_FILE_NAME,
                        "%s",
                        aNewBackupDir);
    }

    /* aNewBackupDir�� valid�� ���������� Ȯ���Ѵ�. */
    IDE_TEST( smriBackupInfoMgr::isValidABSPath( ID_TRUE, sPath ) 
              != IDE_SUCCESS );

    IDE_TEST( smrRecoveryMgr::changeIncrementalBackupDir( sPath ) 
              != IDE_SUCCESS );

    return IDE_SUCCESS;
    
    IDE_EXCEPTION( err_startup_phase );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_WrongStartupPhase));
    }
    IDE_EXCEPTION( err_change_traking_state );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_ChangeTrackingState));
    }
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

/*********************************************************
  function description: smiBackup::moveBackupFile
    PROJ-2133 incremental backup
  - incremental backupfile���� ��ġ�� �̵���Ų��.
  *********************************************************/
IDE_RC smiBackup::moveBackupFile( SChar  * aMoveDir, 
                                  idBool   aWithFile )
{
    SChar sPath[SM_MAX_FILE_NAME];

    IDE_DASSERT( aMoveDir != NULL );

    IDE_TEST_RAISE(smiGetStartupPhase() < SMI_STARTUP_CONTROL,
                   err_startup_phase);

    if( smuProperty::getIncrementalBackupPathMakeABSPath() == 1 )
    {
        /* aNewBackupDir�� �����ΰ� �ƴ��� Ȯ���Ѵ�. */
        IDE_TEST( smriBackupInfoMgr::isValidABSPath( ID_TRUE, aMoveDir ) 
                  != IDE_FAILURE );

        idlOS::snprintf(sPath, SM_MAX_FILE_NAME,
                        "%s%c%s",                      
                        smuProperty::getDefaultDiskDBDir(),
                        IDL_FILE_SEPARATOR,            
                        aMoveDir);
    }
    else
    {
        idlOS::snprintf(sPath, SM_MAX_FILE_NAME,
                        "%s",
                        aMoveDir);
    }

    /* aMoveDir�� valid�� ���������� Ȯ���Ѵ�. */
    IDE_TEST( smriBackupInfoMgr::isValidABSPath( ID_TRUE, sPath ) 
              != IDE_SUCCESS );

    IDE_TEST( smriBackupInfoMgr::moveIncrementalBackupFiles( 
                                                     sPath, 
                                                     aWithFile )
              != IDE_SUCCESS )

    return IDE_SUCCESS;
    
    IDE_EXCEPTION( err_startup_phase );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_WrongStartupPhase));
    }
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

/*********************************************************
  function description: smiBackup::removeBackupInfoFile
    PROJ-2133 incremental backup
  - backupinfo������ �����ϰ� backupinfo manager�� disable��Ų��.
    �� �Լ��� ȣ���ϸ� ������ ����� ��� incremental backup���ϵ��� 
    ����Ҽ� ���Եȴ�.
    backup info������ ������ DB startup�� ���� ���Ҷ�
    ���� ����ؾ��Ѵ�.
  *********************************************************/
IDE_RC smiBackup::removeBackupInfoFile()
{
    IDE_TEST_RAISE(smiGetStartupPhase() > SMI_STARTUP_PROCESS,
                   err_startup_phase);

    IDE_TEST( smriBackupInfoMgr::removeBIFile() != IDE_SUCCESS );

    return IDE_SUCCESS;
    
    IDE_EXCEPTION( err_startup_phase );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_WrongStartupPhase));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
