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
 * $Id$
 *
 * Description :
 *
 * - backupinfo(BI) manager
 *
 **********************************************************************/

#include <iduFile.h>
#include <smuUtility.h>
#include <smmDatabase.h>
#include <smrRecoveryMgr.h>
#include <smrCompareLSN.h>
#include <smriBackupInfoMgr.h>
#include <smrReq.h>
#include <smiMain.h>

smriBIFileHdr       smriBackupInfoMgr::mBIFileHdr;
smriBISlot        * smriBackupInfoMgr::mBISlotArea;
idBool              smriBackupInfoMgr::mIsBISlotAreaLoaded;
iduFile             smriBackupInfoMgr::mFile;
iduMutex            smriBackupInfoMgr::mMutex;

IDE_RC smriBackupInfoMgr::initializeStatic()
{
    idBool sFileState  = ID_FALSE;
    idBool sMutexState = ID_FALSE;

    /*BI���� ���*/
    idlOS::memset( &mBIFileHdr, 0x00, ID_SIZEOF( smriBIFileHdr) );

    /*BI slot�޸� ������ ���� ������*/
    mBISlotArea = NULL;

    mIsBISlotAreaLoaded = ID_FALSE;

    IDE_TEST( mFile.initialize( IDU_MEM_SM_SMR,
                                1,
                                IDU_FIO_STAT_OFF,
                                IDV_WAIT_INDEX_NULL )
              != IDE_SUCCESS );
    sFileState = ID_TRUE;

    IDE_TEST( mMutex.initialize( (SChar*)"Backup Info Manager Mutex",
                                 IDU_MUTEX_KIND_POSIX,
                                 IDV_WAIT_INDEX_NULL )
              != IDE_SUCCESS ); 
    sMutexState = ID_TRUE;
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sFileState == ID_TRUE )
    {
        IDE_ASSERT( mFile.destroy() == IDE_SUCCESS );
    }

    if ( sMutexState == ID_TRUE ) /* BUG-39855 */
    {
        IDE_ASSERT( mMutex.destroy() == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

IDE_RC smriBackupInfoMgr::destroyStatic()
{
    idBool sFileState  = ID_TRUE;
    idBool sMutexState = ID_TRUE;

    sFileState = ID_FALSE;
    IDE_TEST( mFile.destroy() != IDE_SUCCESS );

    sMutexState = ID_FALSE;
    IDE_TEST( mMutex.destroy() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sFileState == ID_TRUE )
    {
        IDE_ASSERT( mFile.destroy() == IDE_SUCCESS );
    }

    if ( sMutexState == ID_TRUE ) /* BUG-39855 */
    {
        IDE_ASSERT( mMutex.destroy() == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}
/***********************************************************************
 * backupinfo manager�� �ʱ�ȭ�Ѵ�.
 * 
 **********************************************************************/
IDE_RC smriBackupInfoMgr::begin()
{
    SChar             * sFileName;
    UInt                sState = 0;
    smLSN               sLastBackupLSN;
    smLSN               sBeforeBackupLSN;
    smriBIMgrState      sBIMgrState;
    ULong               sFileSize        = 0;
    UInt                sValidationState = 0;

    /* logAnchor���� �����̸��� �����´�. */
    sFileName = smrRecoveryMgr::getBIFileName();

    IDE_TEST_RAISE( idf::access( sFileName, F_OK ) != 0,
                    error_not_exist_backup_info_file );

    IDE_TEST( mFile.setFileName( sFileName ) != IDE_SUCCESS );

    IDE_TEST( mFile.open() != IDE_SUCCESS );
    sState = 1;

    IDE_TEST( mFile.read( NULL,
                          SMRI_BI_HDR_OFFSET,
                          (void *)&mBIFileHdr,
                          SMRI_BI_FILE_HDR_SIZE,
                          NULL )
              != IDE_SUCCESS );

    IDE_TEST( checkBIFileHdrCheckSum() != IDE_SUCCESS );

    IDE_TEST( mFile.getFileSize( &sFileSize ) != IDE_SUCCESS );

    IDE_TEST_RAISE( sFileSize != ( ( mBIFileHdr.mBISlotCnt 
                                     * ID_SIZEOF( smriBISlot ) ) 
                                  + SMRI_BI_FILE_HDR_SIZE ),
                    error_invalid_backup_info_file );

    sState = 0;
    IDE_TEST( mFile.close() != IDE_SUCCESS );

    /* 
     * logAnchor���� BackupLSN�� ������ backup info���Ͽ� ����� LSN��
     * ���Ѵ�.
     */
    sLastBackupLSN = smrRecoveryMgr::getBIFileLastBackupLSNFromLogAnchor();

    if ( smrCompareLSN::isEQ( &mBIFileHdr.mLastBackupLSN, 
                              &sLastBackupLSN ) != ID_TRUE )
    {
        sBeforeBackupLSN = smrRecoveryMgr::getBIFileBeforeBackupLSNFromLogAnchor();

        IDE_TEST_RAISE( smrCompareLSN::isEQ( &mBIFileHdr.mLastBackupLSN, 
                                             &sBeforeBackupLSN ) != ID_TRUE,
                        error_invalid_backup_info_file );

        IDE_TEST( smrRecoveryMgr::updateBIFileAttrToLogAnchor( 
                                                    NULL,  //aFileName
                                                    NULL,  //aBackupLSN
                                                    &mBIFileHdr.mLastBackupLSN,
                                                    NULL,  //aBackupDir
                                                    NULL ) //aDeleteArchLogFile
              != IDE_SUCCESS );
    }

    mBISlotArea = NULL;

    /* backupInfo slot ���� */
    IDE_TEST_RAISE( loadBISlotArea() != IDE_SUCCESS, 
                    error_invalid_backup_info_file );
    sValidationState = 1;

    sValidationState = 0;
    IDE_TEST( unloadBISlotArea() != IDE_SUCCESS );
    
    /* logAnchor�� backup info manger�� ���¸� �����Ѵ�. */
    sBIMgrState = SMRI_BI_MGR_INITIALIZED;
    IDE_TEST( smrRecoveryMgr::updateBIFileAttrToLogAnchor( 
                                                    NULL,  //aFileName
                                                    &sBIMgrState,
                                                    NULL,  //aBackupLSN
                                                    NULL,  //aBackupDir
                                                    NULL ) //aDeleteArchLogFile
              != IDE_SUCCESS );
    
    mIsBISlotAreaLoaded = ID_FALSE; 

    return IDE_SUCCESS;

    IDE_EXCEPTION( error_not_exist_backup_info_file );
    {  
        IDE_SET( ideSetErrorCode( smERR_ABORT_NotFoundDataFileByPath, sFileName ) ); 
    }  
    IDE_EXCEPTION( error_invalid_backup_info_file );
    {  
        IDE_SET( ideSetErrorCode( smERR_ABORT_InvalidBackupInfoFile, sFileName ) );
    }  
    IDE_EXCEPTION_END;

    switch( sState )
    {
        case 1:
            IDE_ASSERT( mFile.close() == IDE_SUCCESS );
        case 0:
        default:
            break;
    }
    
    switch( sValidationState )
    {
        case 1:
            IDE_ASSERT( unloadBISlotArea() == IDE_SUCCESS );
        case 0:
        default:
            break;
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * backupinfo manager�� �ı��Ѵ�.
 * 
 **********************************************************************/
IDE_RC smriBackupInfoMgr::end()
{
    UInt                sState = 1;
    smriBIMgrState      sBIMgrState;

    sBIMgrState = SMRI_BI_MGR_DESTROYED;
    IDE_TEST( smrRecoveryMgr::updateBIFileAttrToLogAnchor( 
                                                    NULL,  //aFileName
                                                    &sBIMgrState,
                                                    NULL,  //aBackupLSN
                                                    NULL,  //aBackupDir
                                                    NULL ) //aDeleteArchLogFile
              != IDE_SUCCESS );

    sState = 0;
    IDE_TEST( unloadBISlotArea() != IDE_SUCCESS );

    IDE_ERROR( mIsBISlotAreaLoaded == ID_FALSE );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    switch( sState )
    {
        case 1:
            IDE_ASSERT( unloadBISlotArea() == IDE_SUCCESS );
        case 0:
        default:
            break;
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * backupinfo ������ �����Ѵ�.
 * 
 *
 **********************************************************************/
IDE_RC smriBackupInfoMgr::createBIFile()
{
    smriBIMgrState      sBIMgrState;
    smLSN               sLastBackupLSN;
    UInt                sState = 0;
    UInt                sValidationState = 0;
    SChar               sFileName[SM_MAX_FILE_NAME] = "\0";
    SChar               sInitBackupDir[SM_MAX_FILE_NAME] = "\0";
    UInt                sDeleteArchLogFileNo;

    /* BUG-41521: logAnchor���� �����̸��� �����´�. */
    /* BUG-42338: BI file name�� "backupinfo" ��� ������ �̸��� ����Ѵ�.
     * �׷��� createBIFile() �Լ��� �̿��� ������ log anchor ������ �ٽ� copy �ϴµ�,
     * �̶� �̹� ������ ������ ������ �����Ƿ� �޸��� overlap warning�� �߻��Ѵ�.
     * �̸� �ذ��ϱ� ���� backupinfo ������ ��θ� ������ ������ ���� �����Ѵ�. */
    idlOS::strncpy( sFileName, 
                    smrRecoveryMgr::getBIFileName(), 
                    SM_MAX_FILE_NAME );
    sFileName[SM_MAX_FILE_NAME-1] = '\0';

    if ( idf::access( sFileName, F_OK ) == 0 )
    {
        /* backup slot������ valid���� �˻��Ѵ�. */
        IDE_TEST_RAISE( begin() != IDE_SUCCESS, 
                        error_invalid_backup_info_file );
        sValidationState = 1;

        sValidationState = 0;
        IDE_TEST( end() != IDE_SUCCESS );
    }
    else
    {
        /* BUG-41521: ���� �̸� ���� */
        idlOS::snprintf( sFileName,
                         SM_MAX_FILE_NAME,
                         "%s%c%s",
                         smuProperty::getDBDir(0),
                         IDL_FILE_SEPARATOR,
                         SMRI_BI_FILE_NAME );

        IDE_TEST( mFile.setFileName( sFileName ) != IDE_SUCCESS );
 
        IDE_TEST( mFile.create() != IDE_SUCCESS );
        sState = 1;
 
        IDE_TEST( mFile.open() != IDE_SUCCESS );
        sState = 2;
        
        SM_LSN_INIT( sLastBackupLSN );
        sBIMgrState = SMRI_BI_MGR_FILE_CREATED;
        sDeleteArchLogFileNo = ID_UINT_MAX;

        idlOS::strncpy( sInitBackupDir, 
                        SMRI_BI_INVALID_BACKUP_PATH, 
                        SM_MAX_FILE_NAME );
        sInitBackupDir[SM_MAX_FILE_NAME-1] = '\0';
     
        IDE_TEST( smrRecoveryMgr::updateBIFileAttrToLogAnchor( sFileName,
                                                               &sBIMgrState,
                                                               &sLastBackupLSN,
                                                               sInitBackupDir,
                                                               &sDeleteArchLogFileNo )
                  != IDE_SUCCESS );
 
        initBIFileHdr();
 
        SM_GET_LSN( mBIFileHdr.mLastBackupLSN, sLastBackupLSN );
 
        setBIFileHdrCheckSum();

        IDE_TEST( mFile.write( NULL,
                               SMRI_BI_HDR_OFFSET,
                               &mBIFileHdr,
                               SMRI_BI_FILE_HDR_SIZE )
                  != IDE_SUCCESS );
 
        sState = 1;
        IDE_TEST( mFile.close() != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( error_invalid_backup_info_file );
    {  
        IDE_SET(ideSetErrorCode(smERR_ABORT_InvalidBackupInfoFile, sFileName));
    }
    IDE_EXCEPTION_END;

    switch ( sState )
    {
        case 2:
            IDE_ASSERT( mFile.close() == IDE_SUCCESS );
        case 1:
            IDE_ASSERT( idf::unlink( sFileName ) == IDE_SUCCESS ); 
        case 0:
        default:
            break;
    }

    switch( sValidationState )
    {
        case 1:
            IDE_ASSERT( end() == IDE_SUCCESS );
        case 0:
        default:
            break;
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * backupinfo file hdr�� �ʱ�ȭ�Ѵ�.
 * 
 *
 **********************************************************************/
void smriBackupInfoMgr::initBIFileHdr()
{
    SChar     * sDBName;

    mBIFileHdr.mBISlotCnt = 0;
    
    sDBName = smmDatabase::getDBName();

    idlOS::strncpy( mBIFileHdr.mDBName, 
                    sDBName, 
                    SM_MAX_DB_NAME );

    SM_LSN_INIT( mBIFileHdr.mLastBackupLSN );

    return;
}

/***********************************************************************
 * backupinfo file�� �����Ѵ�.
 * 
 *
 **********************************************************************/
IDE_RC smriBackupInfoMgr::removeBIFile()
{
    SChar               sNULLFileName[ SM_MAX_FILE_NAME ] = "\0";
    SChar               sFileName[ SM_MAX_FILE_NAME ] = "\0";
    smriBIMgrState      sBIMgrState;
    smLSN               sLastBackupLSN; 
    UInt                sState = 0;
    smrLogAnchorMgr     sAnchorMgr4ProcessPhase;
    smrLogAnchorMgr   * sAnchorMgr;
    UInt                sDeleteArchLogFileNo = ID_UINT_MAX;

    IDE_TEST( sAnchorMgr4ProcessPhase.initialize4ProcessPhase() 
              != IDE_SUCCESS );              
    sState = 1;

    sAnchorMgr = &sAnchorMgr4ProcessPhase;

    IDE_TEST_RAISE( sAnchorMgr->getBIMgrState() == SMRI_BI_MGR_FILE_REMOVED,
                    err_backup_info_state);     

    /*CTMgr�� Ȱ��ȭ �� ���¿����� BIFile�� �����Ҽ� ����. */
    IDE_TEST_RAISE( sAnchorMgr->getCTMgrState() == SMRI_CT_MGR_ENABLED,
                    err_change_tracking_state);     

    /* BUG-41552: sFileName�� log anchor�κ��� �����ؼ� �����;� �� */
    idlOS::strncpy( sFileName, 
                    sAnchorMgr->getBIFileName(), 
                    SM_MAX_FILE_NAME );

    /* logAnchor�� backup info���� ���� ���� */
    SM_LSN_INIT( sLastBackupLSN );

    sBIMgrState = SMRI_BI_MGR_FILE_REMOVED;
    IDE_TEST( sAnchorMgr->updateBIFileAttr( sNULLFileName,
                                            &sBIMgrState,                  
                                            &sLastBackupLSN,
                                            NULL, //aBackupDir
                                            &sDeleteArchLogFileNo )                   
              != IDE_SUCCESS );

    /* ������ ���� �Ѵ�. */
    if ( idf::access( sFileName, F_OK ) == 0 )
    {
        IDE_TEST( idf::unlink( sFileName ) != IDE_SUCCESS );
    }

    idlOS::memset( &mBIFileHdr, 0, SMRI_BI_FILE_HDR_SIZE );

    sState = 0;
    IDE_TEST( sAnchorMgr->destroy() != IDE_SUCCESS ); 

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_backup_info_state );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_BackupInfoState));
    }
    IDE_EXCEPTION( err_change_tracking_state );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_ChangeTrackingState));
    }
    IDE_EXCEPTION_END;

    switch( sState )
    {
        case 1:
            IDE_ASSERT( sAnchorMgr->destroy() == IDE_SUCCESS );
        default:
            break;
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * backupinfo�� slot���� �޸𸮷� �о���δ�.
 * 
 *
 **********************************************************************/
IDE_RC smriBackupInfoMgr::loadBISlotArea()
{
    ULong           sFileSize  = 0;
    SChar         * sFileName  = NULL;
    UInt            sBISlotIdx = 0;
    UInt            sState     = 0;
    smriBISlot    * sBISlot    = NULL;

    IDE_ERROR( mIsBISlotAreaLoaded == ID_FALSE ); 

    IDE_TEST_CONT( mBIFileHdr.mBISlotCnt == 0,
                    skip_load_backup_info_slots ); 

    IDE_TEST( mFile.open() != IDE_SUCCESS );
    sState = 1;

    sFileName = mFile.getFileName();

    IDE_TEST( mFile.getFileSize( &sFileSize ) != IDE_SUCCESS );

    IDE_TEST_RAISE( sFileSize != ( ( mBIFileHdr.mBISlotCnt 
                                     * ID_SIZEOF( smriBISlot ) ) 
                                  + SMRI_BI_FILE_HDR_SIZE ),
                    error_invalid_backup_info_file );

    /* smriBackupInfoMgr_loadBISlotArea_calloc_BISlotArea.tc */
    IDU_FIT_POINT("smriBackupInfoMgr::loadBISlotArea::calloc::BISlotArea");
    IDE_TEST( iduMemMgr::calloc( IDU_MEM_SM_SMR,
                                 1,
                                 sFileSize - SMRI_BI_FILE_HDR_SIZE ,
                                 (void**)&mBISlotArea )
              != IDE_SUCCESS );
    sState = 2;
    
    IDE_TEST( mFile.read( NULL,
                          SMRI_BI_FILE_HDR_SIZE,
                          (void *)mBISlotArea,
                          sFileSize - SMRI_BI_FILE_HDR_SIZE,
                          NULL )
              != IDE_SUCCESS );

    for( sBISlotIdx = 0; sBISlotIdx < mBIFileHdr.mBISlotCnt; sBISlotIdx++ )
    {
        sBISlot = &mBISlotArea[ sBISlotIdx ];
        IDE_TEST( checkBISlotCheckSum( sBISlot ) != IDE_SUCCESS );

    }

    sState = 0;
    IDE_TEST( mFile.close() != IDE_SUCCESS );

    mIsBISlotAreaLoaded = ID_TRUE; 
    
    IDE_EXCEPTION_CONT( skip_load_backup_info_slots );

    return IDE_SUCCESS;

    IDE_EXCEPTION( error_invalid_backup_info_file );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_InvalidBackupInfoFile, sFileName));
    }
    IDE_EXCEPTION_END;

    switch( sState )
    {
        case 2:
            IDE_ASSERT( iduMemMgr::free( mBISlotArea ) == IDE_SUCCESS );
            mBISlotArea         = NULL;
            mIsBISlotAreaLoaded = ID_FALSE; 
        case 1:
            IDE_ASSERT( mFile.close() == IDE_SUCCESS );
        default:
            break;
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * backupinfo�� slot�� �Ҵ�� �޸𸮸� �����Ѵ�.
 * 
 *
 **********************************************************************/
IDE_RC smriBackupInfoMgr::unloadBISlotArea()
{
    IDE_TEST_CONT( mIsBISlotAreaLoaded == ID_FALSE,
                    already_unloaded_backupinfo_slot_area ); 

    IDE_ERROR( mIsBISlotAreaLoaded == ID_TRUE );
    IDE_DASSERT( mBISlotArea         != NULL );

    IDE_TEST( iduMemMgr::free( mBISlotArea ) != IDE_SUCCESS );

    mBISlotArea = NULL;

    mIsBISlotAreaLoaded = ID_FALSE; 

    IDE_EXCEPTION_CONT( already_unloaded_backupinfo_slot_area );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * backupinfo slot�� backupinfo������ ���� append�Ͽ� ����Ѵ�.
 * 
 * aBISlot      - [IN] ���Ͽ� append�� BISlot 
 *
 **********************************************************************/
IDE_RC smriBackupInfoMgr::appendBISlot( smriBISlot * aBISlot )
{
    ULong       sFileSize = 0;
    UInt        sState    = 0;

    IDE_DASSERT( aBISlot != NULL );

    IDE_TEST( mFile.open() != IDE_SUCCESS );
    sState = 1;

    setBISlotCheckSum( aBISlot );
    
    IDE_TEST( mFile.getFileSize( &sFileSize ) != IDE_SUCCESS );

    IDE_TEST_RAISE( sFileSize != ( ( mBIFileHdr.mBISlotCnt 
                                     * ID_SIZEOF( smriBISlot ) ) 
                                  + SMRI_BI_FILE_HDR_SIZE ),
                    error_invalid_backup_info_file );

    IDE_TEST( mFile.write( NULL,
                           sFileSize,
                           aBISlot,
                           ID_SIZEOF( smriBISlot ) )
              != IDE_SUCCESS );

    mBIFileHdr.mBISlotCnt++;
    
    IDE_TEST( mFile.sync() != IDE_SUCCESS );

    sState = 0;
    IDE_TEST( mFile.close() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( error_invalid_backup_info_file );
    {  
        IDE_SET(ideSetErrorCode(smERR_ABORT_InvalidBackupInfoFile, 
                mFile.getFileName()));
    }  

    IDE_EXCEPTION_END;

    if ( sState == 1 ) 
    {
        IDE_ASSERT( mFile.close() == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * ��ȿ�Ⱓ�� ���� incremental backup���ϰ� backupinfo slot�� �����Ѵ�.
 * 
 *
 **********************************************************************/
IDE_RC smriBackupInfoMgr::removeObsoleteBISlots()
{
    smriBISlot        * sCurrBISlot;
    smriBISlot        * sNextBISlot;
    SChar               sFilePath[SM_MAX_FILE_NAME];
    SChar             * sPtr;
    UInt                sValidBISlotIdx;
    UInt                sBISlotIdx;
    UInt                sPathLen;
    UInt                sState = 0;

    IDE_TEST( lock() != IDE_SUCCESS );
    sState = 1;

    IDE_TEST( loadBISlotArea() != IDE_SUCCESS );
    sState = 2;

    IDE_TEST ( getValidBISlotIdx( &sValidBISlotIdx ) != IDE_SUCCESS );
    IDE_TEST_CONT( sValidBISlotIdx == SMRI_BI_INVALID_SLOT_IDX, 
                   threre_is_no_obsolete_backup_info);

    IDE_TEST( getBISlot( sValidBISlotIdx, &sCurrBISlot ) != IDE_SUCCESS );

    IDE_ERROR( ( sCurrBISlot->mBackupLevel  == SMI_BACKUP_LEVEL_0 ) &&
               ( sCurrBISlot->mBackupTarget == SMRI_BI_BACKUP_TARGET_DATABASE ) );

    for ( sBISlotIdx = 0 ; sBISlotIdx < sValidBISlotIdx ; sBISlotIdx++ )
    {
        IDE_TEST( getBISlot( sBISlotIdx, &sCurrBISlot ) != IDE_SUCCESS );
        IDE_TEST( getBISlot( sBISlotIdx + 1, &sNextBISlot ) != IDE_SUCCESS );

        IDE_TEST( removeBackupFile( sCurrBISlot->mBackupFileName ) 
                  != IDE_SUCCESS );

        if ( idlOS::strcmp( sCurrBISlot->mBackupTag, sNextBISlot->mBackupTag ) 
             != 0 )
        {
            idlOS::memset( sFilePath, 0x00, SM_MAX_FILE_NAME );

            idlOS::snprintf( sFilePath, 
                             SM_MAX_FILE_NAME, 
                             "%s",
                             sCurrBISlot->mBackupFileName );

            sPtr = idlOS::strrchr( sFilePath, IDL_FILE_SEPARATOR );

            IDE_TEST_RAISE( sPtr == NULL, error_backupdir_file_path );

            sPathLen = sPtr - sFilePath;
            sPathLen = (sPathLen == 0) ? 1 : sPathLen;
            sFilePath[ sPathLen + 1 ] = '\0';

            if ( idf::access( sFilePath, F_OK ) == 0 )
            {
                IDE_TEST( processRestBackupFile( sFilePath, 
                                                 NULL, //aDestPath
                                                 NULL, //aBackupFile
                                                 ID_FALSE ) // move or delete
                        != IDE_SUCCESS );
            }
            else
            {
                /* 
                 * nothing to do 
                 * backup dir�� �̹� ������ ���� 
                 */
            }
        }
    }

    /*
     * BUG-41550
     * mBISlotCnt ���� ��ȿ�Ⱓ�� ���� BI Slot�� ������ ������ 
     * sValidBISlotIdx ��ŭ �ٿ��ش�.
     */
    mBIFileHdr.mBISlotCnt -= sValidBISlotIdx;

    IDE_TEST( flushBIFile( sValidBISlotIdx, NULL ) != IDE_SUCCESS );

    IDE_EXCEPTION_CONT( threre_is_no_obsolete_backup_info );

    sState = 1;
    IDE_TEST( unloadBISlotArea() != IDE_SUCCESS );
    sState = 0;
    IDE_TEST( unlock() != IDE_SUCCESS );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION(error_backupdir_file_path);
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_InvalidFilePathABS));
    }
    IDE_EXCEPTION_END;

    switch( sState )
    {
        case 2:
            IDE_ASSERT( unloadBISlotArea() == IDE_SUCCESS );
        case 1:
            IDE_ASSERT( unlock() == IDE_SUCCESS );
        default:
            break;
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * ���� �Ⱓ�� ������ ���� ù��° BISlot�� Idx�� ���Ѵ�.
 * 
 * aValidBISlotIdx      - [OUT] ��ȿ�� ù��° BISlot Idx
 *
 **********************************************************************/
IDE_RC smriBackupInfoMgr::getValidBISlotIdx( UInt * aValidBISlotIdx )
{
    UInt            sValidBISlotIdx;
    UInt            sCurrentTime;
    UInt            sEndBackupTime;
    UInt            sRetentionPeriod;
    UInt            sBISlotIdx;
    smriBISlot    * sCurrentBISlot;
    smriBISlot    * sBeforeBISlot;

    IDE_DASSERT( aValidBISlotIdx != NULL );

    sValidBISlotIdx      = SMRI_BI_INVALID_SLOT_IDX;
    sCurrentTime         = smLayerCallback::getCurrTime();
    
    if ( smuProperty::getBackupInfoRetentionPeriodForTest() != 0 )
    {
        /* Hidden property�� 
         * �׽�Ʈ�� backup info retention period�� ���� ���� ���, 
         * �����ͼ� �׽�Ʈ �뵵�� ��� */
        sRetentionPeriod = smuProperty::getBackupInfoRetentionPeriodForTest();
    }
    else
    {
        sRetentionPeriod = 
                convertDayToSec( smuProperty::getBackupInfoRetentionPeriod() );
    }
    
    for( sBISlotIdx = 1; sBISlotIdx < mBIFileHdr.mBISlotCnt; sBISlotIdx++ )
    {
        IDE_TEST( getBISlot( sBISlotIdx, &sCurrentBISlot ) != IDE_SUCCESS );
        IDE_TEST( getBISlot( sBISlotIdx - 1, &sBeforeBISlot ) != IDE_SUCCESS );
        
        if ( idlOS::strcmp( sBeforeBISlot->mBackupTag, 
                            sCurrentBISlot->mBackupTag ) != 0 )
         {
            if ( ( sCurrentBISlot->mBackupLevel == SMI_BACKUP_LEVEL_0 ) &&
                 ( sCurrentBISlot->mBackupTarget == SMRI_BI_BACKUP_TARGET_DATABASE ) )
            {
                sEndBackupTime = sBeforeBISlot->mEndBackupTime;

                if ( ( sCurrentTime - sEndBackupTime ) > sRetentionPeriod )
                {
                    sValidBISlotIdx = sBISlotIdx;
                }
                else
                {
                    /*
                     * BUG-41550
                     * ���������� backup�� ������ �ð��� 
                     * retention period ���� ���� ��� BI slot index�� ��ȯ
                     */
                    break;
                }
            }
        }
    }

    *aValidBISlotIdx = sValidBISlotIdx;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * �����Ⱓ�� ���� BISlot�� Backup������ �����Ѵ�.
 * 
 * aBackupFileName      - [IN] ������ ��������̸�
 *
 **********************************************************************/
IDE_RC smriBackupInfoMgr::removeBackupFile( SChar * aBackupFileName )
{
    IDE_DASSERT( aBackupFileName != NULL );

    if ( idf::access( aBackupFileName, F_OK ) == 0 )
    {
        IDE_TEST( idf::unlink( aBackupFileName ) != IDE_SUCCESS );
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
 * backupinfo slot�� ��ϵ��� ���� ������ϵ鿡 ���� ó���� �����Ѵ�.
 * (loganchor����, backupinfo����, tablebackup����)
 * 
 * aBackupPath      - [IN] backup������ �����ϴ� ���
 * aDestPath        - [IN] backup���� �̵� �� �̵� ��ų ��� 
 * aFile            - [IN] backup���� 
 * aIsMove          - [IN] backup������ �̵���ų���� �����Ұ������� ���� ����
 *
 **********************************************************************/
IDE_RC smriBackupInfoMgr::processRestBackupFile( SChar   * aBackupPath,
                                                 SChar   * aDestPath,
                                                 iduFile * aFile,
                                                 idBool    aIsMove )
{
    struct  dirent    * sResDirEnt  = NULL;
    struct  dirent    * sDirEnt     = NULL;
    DIR               * sDIR        = NULL;
    SInt                sRC;
    SInt                sOffset;
    SChar               sFileName[SM_MAX_FILE_NAME];
    SChar               sStrFullFileName[SM_MAX_FILE_NAME];
    SInt                sBackupFileExtLen;
    UInt                sState = 0;

    IDE_TEST(iduMemMgr::malloc(IDU_MEM_SM_SMC,
                       ID_SIZEOF(struct dirent) + SM_MAX_FILE_NAME,
                       (void**)&(sDirEnt),
                       IDU_MEM_FORCE)
             != IDE_SUCCESS);
    sState = 1;

    sDIR = idf::opendir(aBackupPath);
    IDE_TEST_RAISE(sDIR == NULL, err_open_dir);
    sState = 2;

    errno = 0;
    sRC = idf::readdir_r(sDIR, sDirEnt, &sResDirEnt) ;
    IDE_TEST_RAISE( (sRC != 0) && (errno != 0), err_read_dir );
    errno = 0;

    while(sResDirEnt != NULL)
    {
        idlOS::strcpy(sFileName, (const char*)sResDirEnt->d_name);
        if ( ( idlOS::strcmp(sFileName, ".")  == 0 ) ||
             ( idlOS::strcmp(sFileName, "..") == 0 ) )
        {
            errno = 0;
            sRC = idf::readdir_r(sDIR, sDirEnt, &sResDirEnt);
            IDE_TEST_RAISE( (sRC != 0) && (errno != 0), err_read_dir );
            errno = 0;

            continue;
        }

        idlOS::snprintf(sStrFullFileName, SM_MAX_FILE_NAME, "%s%c%s",
                        aBackupPath, IDL_FILE_SEPARATOR,
                        sFileName);

        if ( aIsMove == ID_TRUE )
        {
            IDE_DASSERT( aDestPath != NULL );
            IDE_DASSERT( aFile     != NULL );

            sBackupFileExtLen = idlOS::strlen(SMRI_INCREMENTAL_BACKUP_FILE_EXT);
            sOffset = (SInt)idlOS::strlen(sFileName) - sBackupFileExtLen;
            if ( ( sOffset < 0 ) ||
                 ( idlOS::strncmp( sFileName + sOffset,
                                   SMRI_INCREMENTAL_BACKUP_FILE_EXT, 
                                   sBackupFileExtLen ) != 0 ) )
            {
                IDE_TEST( moveFile( aBackupPath, 
                                    aDestPath, 
                                    sFileName, 
                                    aFile ) != IDE_SUCCESS );
            }
        }
        else
        {
            IDE_DASSERT( aDestPath == NULL );
            IDE_DASSERT( aFile     == NULL );

            IDE_TEST( removeBackupFile( sStrFullFileName ) 
                      != IDE_SUCCESS );
        }

        errno = 0;
        sRC = idf::readdir_r(sDIR, sDirEnt, &sResDirEnt);
        IDE_TEST_RAISE( (sRC != 0) && (errno != 0), err_read_dir );
        errno = 0;
    }

    sState = 1;
    idf::closedir(sDIR);
    sDIR = NULL;

    if(sDirEnt != NULL)
    {
        sState = 0;
        IDE_TEST(iduMemMgr::free(sDirEnt) != IDE_SUCCESS);
        sDirEnt = NULL;
    }

    IDE_TEST( idlOS::rmdir( aBackupPath ) != 0 );
        
    return IDE_SUCCESS;

    IDE_EXCEPTION(err_open_dir);
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_NotDir));
    }
    IDE_EXCEPTION(err_read_dir);
    {
        IDE_SET( ideSetErrorCode( smERR_FATAL_CannotReadDir, aBackupPath ) );
    }
    IDE_EXCEPTION_END;

    switch( sState )
    {
        case 2:
            idf::closedir(sDIR);
        case 1:
            IDE_ASSERT(iduMemMgr::free(sDirEnt) == IDE_SUCCESS);
        default:
            break;
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * BI������ flush�Ѵ�. �⺻������ BIFileHdr�� flush �ϸ� 
 * �ʿ��Ѱ�� BISlotArea�� flush�Ѵ�.
 * 
 * aValidBISlotIdx      - [IN] ��ȿ�� ù��° BISlot Idx
 * aLastBackupLSN       - [IN] �������� ����� backupLSN
 *
 **********************************************************************/
IDE_RC smriBackupInfoMgr::flushBIFile( UInt aValidBISlotIdx, 
                                       smLSN * aLastBackupLSN )
{
    ULong               sCurrFileSize = 0;
    ULong               sNewFileSize  = 0;
    UInt                sState        = 0;

    IDE_TEST( mFile.open() != IDE_SUCCESS );
    sState = 1;

    if ( aLastBackupLSN != NULL )
    {
        mBIFileHdr.mLastBackupLSN.mFileNo = aLastBackupLSN->mFileNo;
        mBIFileHdr.mLastBackupLSN.mOffset = aLastBackupLSN->mOffset;
    }

    setBIFileHdrCheckSum();
    IDE_TEST( mFile.write( NULL,
                           SMRI_BI_HDR_OFFSET,
                           &mBIFileHdr,
                           SMRI_BI_FILE_HDR_SIZE )
              != IDE_SUCCESS );

    if ( ( mIsBISlotAreaLoaded == ID_TRUE ) &&
         ( aValidBISlotIdx != SMRI_BI_INVALID_SLOT_IDX ) )
    {
        IDE_TEST( mFile.getFileSize( &sCurrFileSize ) != IDE_SUCCESS );

        sNewFileSize =  SMRI_BI_FILE_HDR_SIZE 
                        + ( ID_SIZEOF( smriBISlot ) * mBIFileHdr.mBISlotCnt );

        IDE_ERROR( sNewFileSize <= sCurrFileSize );

        IDE_TEST( mFile.truncate( sNewFileSize ) != IDE_SUCCESS );

        IDE_TEST( mFile.write( NULL,
                               SMRI_BI_FILE_HDR_SIZE,
                               &mBISlotArea[ aValidBISlotIdx ],
                               mBIFileHdr.mBISlotCnt * ID_SIZEOF( smriBISlot ) )
                  != IDE_SUCCESS );
    }

    IDE_TEST( mFile.sync() != IDE_SUCCESS );

    sState = 0;
    IDE_TEST( mFile.close() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sState == 1 )
    {
        IDE_ASSERT( mFile.close() == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * time��� �ҿ��� �����϶� �ش� �ð��� �ش��ϴ� BISlotIdx�� ���Ѵ�.
 * 
 * aUntilTime                   - [IN] time��� �ҿ��� ���� �ð�
 * aTargetBackupTagSlotIdx      - [IN] aUntilTime�� �ش��ϴ� BISlot�� Idx
 *
 **********************************************************************/
IDE_RC smriBackupInfoMgr::findBISlotIdxUsingTime( UInt    aUntilTime,
                                                  UInt  * aTargetBISlotIdx )
{
    UInt            sBISlotIdx;
    UInt            sTargetBISlotIdx = SMRI_BI_INVALID_SLOT_IDX;
    smriBISlot    * sBISlot;

    IDE_DASSERT( aTargetBISlotIdx != NULL );

    IDE_ERROR( mBIFileHdr.mBISlotCnt != 0 );

    IDE_TEST( getBISlot( 0, &sBISlot ) != IDE_SUCCESS );
    
    IDE_TEST_RAISE( aUntilTime <= sBISlot->mEndBackupTime,
                    error_invalid_restore_time );

    /* ������ �˻� */
    for( sBISlotIdx = mBIFileHdr.mBISlotCnt - 1; 
         sBISlotIdx != SMRI_BI_INVALID_SLOT_IDX; 
         sBISlotIdx-- )
    {
        IDE_TEST( getBISlot( sBISlotIdx, & sBISlot ) != IDE_SUCCESS );

        if ( sBISlot->mEndBackupTime <= aUntilTime )
        {
             sTargetBISlotIdx = sBISlotIdx;

            break;
        }
    }

    *aTargetBISlotIdx = sTargetBISlotIdx;

    return IDE_SUCCESS;

    IDE_EXCEPTION( error_invalid_restore_time );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_InvalidRestoreTime));
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * �����ؾ��� BISlotIdx�� ���Ѵ�.
 * 
 * aUntilBackupTag              - [IN] BackupTag��� �ҿ������� Backup tag �̸�
 * aStartScanBISlotIdx          - [IN] scan�� ������ backupinfo slot�� idx
 * aSearchUntilBackupTag        - [IN] backupTag�˻��� ���������� ���� ����
 * aRestoreTarget               - [IN] restore���(database or tablespace)
 * aSpaceID                     - [IN] �˻��� tablespace ID
 * aRestoreBISlotIdx            - [OUT] ������ �����ؾ��� ù��° BISlotIdx
 *
 **********************************************************************/
IDE_RC smriBackupInfoMgr::getRestoreTargetSlotIdx( 
                                     SChar            * aUntilBackupTag,
                                     UInt               aStartScanBISlotIdx,
                                     idBool             aSearchUntilBackupTag,
                                     smriBIBackupTarget aRestoreTarget,
                                     scSpaceID          aSpaceID,
                                     UInt             * aRestoreBISlotIdx )
{
    UInt            sBISlotIdx;
    smriBISlot    * sCurrBISlot;
    smriBISlot    * sPrevBISlot;
    UInt            sRestoreBISlotIdx = SMRI_BI_INVALID_SLOT_IDX;

    IDE_DASSERT( aRestoreBISlotIdx   != NULL ); 
    IDE_DASSERT( aStartScanBISlotIdx != SMRI_BI_INVALID_SLOT_IDX );

    for( sBISlotIdx = aStartScanBISlotIdx; 
         sBISlotIdx != SMRI_BI_INVALID_SLOT_IDX; 
         sBISlotIdx-- )
    {
        IDE_TEST( getBISlot( sBISlotIdx, &sCurrBISlot ) != IDE_SUCCESS );
    
        /* backup tag��� ���� �� ���
         * aUntilBackupTag�� ��ġ�ϴ� backupinfo slot�� ã�´�.*/
        if ( aSearchUntilBackupTag == ID_TRUE )
        {
            IDE_DASSERT( aUntilBackupTag != NULL );
            if ( idlOS::strcmp( sCurrBISlot->mBackupTag, aUntilBackupTag ) == 0 )
            {
                aSearchUntilBackupTag   = ID_FALSE;
            }
            else
            {
                /* nothing to do */
            }
        }
        else
        {
            /* BUG-37371 
             * backup tag��� ������ �ƴϰų� aUntilBackupTag����ġ�ϴ�
             * backupinfo slot�� ã�� ��� ������ ������ backup info slot��
             * ã�´�.*/
            if ( aRestoreTarget == SMRI_BI_BACKUP_TARGET_DATABASE )
            {
                /* ��������� Level0 ����̸� database ����̾���Ѵ�. */
                if ( (sCurrBISlot->mBackupTarget == SMRI_BI_BACKUP_TARGET_DATABASE) &&
                     (sCurrBISlot->mBackupLevel  == SMI_BACKUP_LEVEL_0) )
                {
                    /* sBISlotIdx�� 0�ϰ�� sBISlotIdx - 1�� UINT_MAX�� �ȴ� �̴� 
                     * SMRI_BI_INVALID_SLOT_IDX�� ������ sPrevBISlot�� NULL��
                     * ���õǾ� ��ȯ�ȴ�.  */  
                    IDE_TEST( getBISlot( sBISlotIdx - 1, &sPrevBISlot ) != IDE_SUCCESS );

                    /* �����Ϸ��� database����� ù��° backup info slot��
                     * ã�´� */
                    if ( ( ( sPrevBISlot != NULL )                            &&
                           ( ( idlOS::strcmp( sPrevBISlot->mBackupTag, 
                                              sCurrBISlot->mBackupTag )) != 0 ) ) ||
                         ( sBISlotIdx == 0 ) )
                    {
                        IDE_ASSERT( ((sPrevBISlot == NULL) && (sBISlotIdx == 0)) ||
                                    ((sPrevBISlot != NULL) && (sBISlotIdx != 0)) );
                        sRestoreBISlotIdx = sBISlotIdx;
                        break;
                    }
                }
            }
            else
            {
                // level 0 or 1�� full ����� backupinfo slot�� ã�� tablespace�� ����
                if ( ( (sCurrBISlot->mBackupType & SMI_BACKUP_TYPE_FULL) != 0 ) &&
                     ( sCurrBISlot->mSpaceID == aSpaceID ) )
                {
                    sRestoreBISlotIdx = sBISlotIdx;
                    break;
                }
            }
        }
    }

    *aRestoreBISlotIdx = sRestoreBISlotIdx;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * restore�� ������ BISlot�� ������ ���Ѵ�.
 * 
 * aScanStartBISlotIdx          - [IN] restore scan�� ������ BISlotIdx
 * aRestoreType                 - [IN] ���� Ÿ��
 * aUltilTime                   - [IN] time��� �ҿ��� ���� �ð�
 * aUntilBackupTag              - [IN] backupTag��� �ҿ��� ���� tag�̸�
 * aRestoreStartBISlotIdx       - [OUT] ������ ������ BISltoIdx
 * aRestoreEndBISlotIdx         - [OUT] ������ ���� BISlotIdx
 *
 **********************************************************************/
IDE_RC smriBackupInfoMgr::calcRestoreBISlotRange4Level1( 
                                        UInt            aScanStartBISlotIdx,
                                        smiRestoreType  aRestoreType,
                                        UInt            aUntilTime,
                                        SChar         * aUntilBackupTag,
                                        UInt          * aRestoreStartBISlotIdx,
                                        UInt          * aRestoreEndBISlotIdx )
{
    smriBISlot    * sCurrBISlot;
    smriBISlot    * sPrevBISlot;
    smriBISlot    * sCumulativeBISlot = NULL;
    UInt            sBISlotIdx;
    UInt            sCurrCumulativeBISlotIdx;
    UInt            sPrevCumulativeBISlotIdx;
    UInt            sRestoreEndBISlotIdx        = mBIFileHdr.mBISlotCnt - 1;
    idBool          sIsFindCumulativeBackup     = ID_FALSE;
    idBool          sIsFindEndBackupTag         = ID_FALSE;

    IDE_DASSERT( aRestoreStartBISlotIdx  != NULL );
    IDE_DASSERT( aRestoreEndBISlotIdx    != NULL );

    sCurrCumulativeBISlotIdx = SMRI_BI_INVALID_SLOT_IDX;
    sPrevCumulativeBISlotIdx = SMRI_BI_INVALID_SLOT_IDX;

    for( sBISlotIdx = aScanStartBISlotIdx;
         sBISlotIdx < mBIFileHdr.mBISlotCnt;
         sBISlotIdx++ )
    {
        if ( sBISlotIdx == 0 )
        {
            continue;
        }
        else
        {
            /* do nothing */
        }

        IDE_TEST( getBISlot( sBISlotIdx, &sCurrBISlot ) != IDE_SUCCESS );
        IDE_TEST( getBISlot( sBISlotIdx - 1, &sPrevBISlot ) != IDE_SUCCESS );

        /* backup tag��� �ҿ��� ���� */
        if ( aRestoreType == SMI_RESTORE_UNTILTAG )
        {
            if ( ( sIsFindEndBackupTag == ID_FALSE ) &&
                 ( idlOS::strcmp(aUntilBackupTag, sCurrBISlot->mBackupTag) == 0 ) )
            {
                sIsFindEndBackupTag = ID_TRUE;
            }
            
            if ( ( sIsFindEndBackupTag == ID_TRUE ) && 
                 ( idlOS::strcmp(aUntilBackupTag, sCurrBISlot->mBackupTag) != 0 ) )
            {
                IDE_ERROR( idlOS::strcmp( aUntilBackupTag, 
                                          sPrevBISlot->mBackupTag ) == 0 );

                sRestoreEndBISlotIdx = sBISlotIdx - 1;

                break;
            }
        }
        else
        {
            /* nothing to do */
        }

        /* �ð� ��� �ҿ��� ���� */
        if ( aRestoreType == SMI_RESTORE_UNTILTIME )
        {
            if ( sCurrBISlot->mEndBackupTime > aUntilTime )
            {
                IDE_ERROR( sPrevBISlot->mEndBackupTime <= aUntilTime );

                sRestoreEndBISlotIdx = sBISlotIdx - 1;

                break;
            }
        }
        else
        {
            /* nothing to do */
        }

        /* Cumulative ��� slot��ġ ���� */
        if ( ( ( sCurrBISlot->mBackupType & SMI_BACKUP_TYPE_CUMULATIVE ) != 0) &&
             ( sCurrBISlot->mBackupTarget == SMRI_BI_BACKUP_TARGET_DATABASE ) &&
             ( idlOS::strcmp( sCurrBISlot->mBackupTag, 
                              sPrevBISlot->mBackupTag ) != 0 ) )
        {
            sPrevCumulativeBISlotIdx    = sCurrCumulativeBISlotIdx;
            sCurrCumulativeBISlotIdx    = sBISlotIdx;
            sCumulativeBISlot           = sCurrBISlot;
            sIsFindCumulativeBackup     = ID_TRUE;
        }
        else
        {
            /* nothing to do */
        }
    }

    if ( sIsFindCumulativeBackup == ID_TRUE ) /* cumulative backup�� �ִ°�� */
    {
        /* �ҿ��� �����̸鼭 �������� �˻��� ����� cumulative backup�� ���*/
        if ( ( sBISlotIdx < ( mBIFileHdr.mBISlotCnt - 1 ) ) &&
             ( idlOS::strcmp( sCurrBISlot->mBackupTag, 
                              sCumulativeBISlot->mBackupTag ) == 0 ) )
        {
            if ( sPrevCumulativeBISlotIdx != SMRI_BI_INVALID_SLOT_IDX )
            {
                *aRestoreStartBISlotIdx = sPrevCumulativeBISlotIdx;
            }
            else
            {
                *aRestoreStartBISlotIdx = aScanStartBISlotIdx;
            }
        }
        else /* �������� or �������� �˻��� ����� cumulative backup�� �ƴ� ��� */
        {
            *aRestoreStartBISlotIdx = sCurrCumulativeBISlotIdx;
        }
    }
    else /* cumulative backup�� ���°�� */
    {
        *aRestoreStartBISlotIdx = aScanStartBISlotIdx;
    }

    *aRestoreEndBISlotIdx = sRestoreEndBISlotIdx;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * BI������ ����Ѵ�.
 * 
 * aBackupPath          - [IN] ����� path
 *
 **********************************************************************/
IDE_RC smriBackupInfoMgr::backup( SChar * aBackupPath )
{
    SChar   sBackupFileName[ SM_MAX_FILE_NAME ];
    UInt    sState = 0;

    IDE_ASSERT( aBackupPath != NULL );

    IDE_TEST( mFile.open() != IDE_SUCCESS );
    sState = 1;

    idlOS::memset( sBackupFileName, 0x00, SM_MAX_FILE_NAME );

    idlOS::snprintf( sBackupFileName, 
                     SM_MAX_FILE_NAME,
                     "%s%s",
                     aBackupPath,
                     SMRI_BI_FILE_NAME );

    IDE_TEST( mFile.copy( NULL,
                          sBackupFileName,
                          ID_FALSE ) != IDE_SUCCESS );

    sState = 0;
    IDE_TEST( mFile.close() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sState == 1 )
    {
        IDE_ASSERT( mFile.close() == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * incremental backup ������ ������ ��ġ�� �̵���Ų��.
 * 
 * aMovePath    - [IN] ������ �̵���ų path
 * aWithFile    - [IN] backupinfo slot�� ��θ� �����ų�� 
 *                     ���ϵ� �Բ� �̵���ų�� �Ǵ�
 *
 **********************************************************************/
IDE_RC smriBackupInfoMgr::moveIncrementalBackupFiles( SChar * aMovePath, 
                                                      idBool  aWithFile )
{
    UInt            sState = 0;
    UInt            sBISlotIdx;
    UInt            sMovePathLen;
    UInt            sPathLen;
    SChar         * sPtr; 
    SChar         * sBackupFileName;
    SChar           sSrcBackupFilePath[ SMRI_BI_MAX_BACKUP_FILE_NAME_LEN ];
    SChar           sDestBackupFilePath[ SMRI_BI_MAX_BACKUP_FILE_NAME_LEN ];
    SChar           sDestBackupFileName[ SMRI_BI_MAX_BACKUP_FILE_NAME_LEN ];
    SChar         * sBeforeBISlotBackupTagName = SMRI_BI_INVALID_TAG_NAME;
    smriBISlot    * sCurrBISlot;
    smriBISlot    * sNextBISlot;
    iduFile         sBackupFile;
    idBool          sIsNeedProcessRestBackupFile;

    IDE_ASSERT( aMovePath != NULL );

    IDE_TEST( lock() != IDE_SUCCESS );
    sState = 1;

    IDE_TEST( loadBISlotArea() != IDE_SUCCESS );
    sState = 2;

    IDE_TEST( sBackupFile.initialize( IDU_MEM_SM_SMR,
                                1,
                                IDU_FIO_STAT_OFF,
                                IDV_WAIT_INDEX_NULL )
              != IDE_SUCCESS );
    sState = 3;

    for( sBISlotIdx = 0; sBISlotIdx < mBIFileHdr.mBISlotCnt; sBISlotIdx++ )
    {
        IDE_TEST( getBISlot( sBISlotIdx, &sCurrBISlot ) != IDE_SUCCESS );

        if ( idlOS::strcmp( sBeforeBISlotBackupTagName, sCurrBISlot->mBackupTag ) != 0 )
        {
            sMovePathLen = idlOS::strlen(aMovePath);
 
            /* Dest ������ ��θ� ���� */
            if ( aMovePath[ sMovePathLen - 1 ] == IDL_FILE_SEPARATOR )
            {
                idlOS::snprintf( sDestBackupFilePath, 
                                 SMRI_BI_MAX_BACKUP_FILE_NAME_LEN, 
                                 "%s%s%c",
                                 aMovePath,                    
                                 sCurrBISlot->mBackupTag,
                                 IDL_FILE_SEPARATOR );                
            }
            else
            {
                idlOS::snprintf( sDestBackupFilePath, 
                                 SMRI_BI_MAX_BACKUP_FILE_NAME_LEN, 
                                 "%s%c%s%c",
                                 aMovePath,                    
                                 IDL_FILE_SEPARATOR,            
                                 sCurrBISlot->mBackupTag,
                                 IDL_FILE_SEPARATOR );                
            }
            
            /* Src ������ ��θ� ���� */ 
            idlOS::snprintf( sSrcBackupFilePath,
                             SMRI_BI_MAX_BACKUP_FILE_NAME_LEN, 
                             "%s",
                             sCurrBISlot->mBackupFileName );

            sPtr = idlOS::strrchr( sSrcBackupFilePath, IDL_FILE_SEPARATOR );

            IDE_TEST( sPtr == NULL );
            sPathLen = sPtr - sSrcBackupFilePath;
            sPathLen = (sPathLen == 0) ? 1 : sPathLen;
            sSrcBackupFilePath[ sPathLen + 1 ] = '\0';

            if ( idlOS::strcmp( sSrcBackupFilePath, sDestBackupFilePath ) == 0 )
            {
                /* �̵���ų ��������� ��ġ�� ������ ��ġ�� �����ϴ�. */
                continue;
            }
        }
        else
        {
            /* nothing to do */
        }

        /* �̵���ų ��������� �̸��� ���Ѵ�. */
        sBackupFileName = idlOS::strrchr( sCurrBISlot->mBackupFileName,
                                          IDL_FILE_SEPARATOR );        
        sBackupFileName = sBackupFileName + 1;

        if ( aWithFile == ID_TRUE )
        {
            if ( idlOS::strcmp( sBeforeBISlotBackupTagName, sCurrBISlot->mBackupTag ) != 0 )
            {
                /* TAG�̸��� ���� dir���� */
                if ( idf::access( sDestBackupFilePath, F_OK ) != 0 )
                {
                    IDE_TEST_RAISE( idlOS::mkdir( sDestBackupFilePath,  
                                                  S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH ) != 0,
                                    error_create_path );
                }
                else
                {
                    /* �̵��� dest���丮�� �̹� �����ϴ°�� */
                    IDE_RAISE( error_path_is_already_exist );
                }
            }
            else
            {
                /* nothing to do */
            }

            /* incremental backup file �̵� */
            IDE_TEST( moveFile( sSrcBackupFilePath, 
                                sDestBackupFilePath, 
                                sBackupFileName, 
                                &sBackupFile ) != IDE_SUCCESS );

            sIsNeedProcessRestBackupFile = ID_FALSE;

            if ( ( sBISlotIdx + 1 ) != mBIFileHdr.mBISlotCnt )
            {
                IDE_TEST( getBISlot( sBISlotIdx + 1, &sNextBISlot ) != IDE_SUCCESS );
                if ( idlOS::strcmp( sNextBISlot->mBackupTag, 
                                    sCurrBISlot->mBackupTag ) != 0 )
                {
                    sIsNeedProcessRestBackupFile = ID_TRUE;
                }
                else
                {
                    sIsNeedProcessRestBackupFile = ID_FALSE;
                }
            }
            else
            {
                sIsNeedProcessRestBackupFile = ID_TRUE;
            }
            
            if ( sIsNeedProcessRestBackupFile == ID_TRUE )
            {
                IDE_TEST( processRestBackupFile( sSrcBackupFilePath, 
                                                 sDestBackupFilePath, 
                                                 &sBackupFile,
                                                 ID_TRUE ) != IDE_SUCCESS );
            }
        }
        else
        {
            /* nothing to do */
        }

        /* DestFile�̸� ���� */
        idlOS::snprintf( sDestBackupFileName, 
                         SMRI_BI_MAX_BACKUP_FILE_NAME_LEN, 
                         "%s%s",
                         sDestBackupFilePath,                    
                         sBackupFileName );

        /* backupinfo slot�� ������� �̸� ���� */
        idlOS::strncpy( sCurrBISlot->mBackupFileName, 
                        sDestBackupFileName,
                        SMRI_BI_MAX_BACKUP_FILE_NAME_LEN );
        setBISlotCheckSum( sCurrBISlot );

        sBeforeBISlotBackupTagName = sCurrBISlot->mBackupTag;
    }
    
    IDE_TEST( flushBIFile(0, NULL) != IDE_SUCCESS );

    sState = 2;
    IDE_TEST( sBackupFile.destroy() != IDE_SUCCESS );

    sState = 1;
    IDE_TEST( unloadBISlotArea() != IDE_SUCCESS );

    sState = 0;
    IDE_TEST( unlock() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( error_create_path ); 
    {
        IDE_SET(ideSetErrorCode( smERR_ABORT_FailToCreateDirectory,
                                 sDestBackupFilePath));
    }
    IDE_EXCEPTION( error_path_is_already_exist );
    {  
        IDE_SET(ideSetErrorCode(smERR_ABORT_AlreadyExistPath, 
                                sDestBackupFilePath)); 
    }  
    IDE_EXCEPTION_END;

    switch( sState )
    {
        case 3:
            IDE_ASSERT( sBackupFile.destroy() == IDE_SUCCESS );
        case 2:
            IDE_ASSERT( unloadBISlotArea() == IDE_SUCCESS );
        case 1:
            IDE_ASSERT( unlock() == IDE_SUCCESS );
        default:
            break;
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * backup�� ���ϵ��� �̵���Ų��.
 * 
 * aSrcFilePath     - [IN] ���� ���� path
 * aDestFilePath    - [IN] ������ ���� path
 * aFileName        - [IN] ���� �̸�
 * aFile            - [IN]
 *
 **********************************************************************/
IDE_RC smriBackupInfoMgr::moveFile( SChar   * aSrcFilePath, 
                                    SChar   * aDestFilePath, 
                                    SChar   * aFileName, 
                                    iduFile * aFile )
{
    SChar   sDestFileName[ SMRI_BI_MAX_BACKUP_FILE_NAME_LEN ];
    SChar   sSrcFileName[ SMRI_BI_MAX_BACKUP_FILE_NAME_LEN ];
    UInt    sState = 0;

    IDE_DASSERT( aSrcFilePath  != NULL );
    IDE_DASSERT( aDestFilePath != NULL );
    IDE_DASSERT( aFileName     != NULL );
    IDE_DASSERT( aFile         != NULL );

    idlOS::snprintf( sSrcFileName, 
                     SMRI_BI_MAX_BACKUP_FILE_NAME_LEN, 
                     "%s%s",
                     aSrcFilePath,                    
                     aFileName );

    idlOS::snprintf( sDestFileName, 
                     SMRI_BI_MAX_BACKUP_FILE_NAME_LEN, 
                     "%s%s",
                     aDestFilePath,                    
                     aFileName );

    IDE_TEST_RAISE( idf::access( sSrcFileName, F_OK ) != 0, error_file_is_not_exist ); 

    IDE_TEST( aFile->setFileName( sSrcFileName ) != IDE_SUCCESS );

    IDE_TEST( aFile->open() != IDE_SUCCESS );
    sState = 1;

    IDE_TEST( aFile->copy( NULL, sDestFileName ) != IDE_SUCCESS ); 

    sState = 0;
    IDE_TEST( aFile->close() != IDE_SUCCESS );

    IDE_TEST( idf::unlink( sSrcFileName ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( error_file_is_not_exist );
    {  
        IDE_SET(ideSetErrorCode(smERR_ABORT_NoExistFile, 
                sSrcFileName));
    }  
    IDE_EXCEPTION_END;
    if ( sState == 1 )
    {
        IDE_ASSERT( aFile->close() == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * BISlotIdx�� �ش��ϴ� BISlot�� �����´�.
 * 
 * aBISlotIdx       - [IN] ������ BISlot�� Idx
 * aBISlot          - [OUT] aBISlot��ȯ
 *
 **********************************************************************/
IDE_RC smriBackupInfoMgr::getBISlot( UInt aBISlotIdx, smriBISlot ** aBISlot )
{
    smriBIFileHdr * sBIFileHdr;

    IDE_DASSERT( mIsBISlotAreaLoaded == ID_TRUE );

    IDE_ERROR( getBIFileHdr( &sBIFileHdr ) == IDE_SUCCESS );
    IDE_ERROR( (aBISlotIdx < sBIFileHdr->mBISlotCnt) || 
               (aBISlotIdx == SMRI_BI_INVALID_SLOT_IDX) );

    /* BUG-37371 */
    if ( aBISlotIdx != SMRI_BI_INVALID_SLOT_IDX )
    {
        *aBISlot = &mBISlotArea[ aBISlotIdx ];
    }
    else
    {
        *aBISlot = NULL;
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * BI�� backup���� ����� �����Ѵ�.
 * 
 * aBackupFileHdrBISlot     - [IN] ������������ BISlot
 * aBISlot                  - [IN] aBISlot��ȯ
 *
 **********************************************************************/
void smriBackupInfoMgr::setBI2BackupFileHdr( smriBISlot * aBackupFileHdrBISlot,
                                             smriBISlot * aBISlot )
{
    IDE_DASSERT( aBackupFileHdrBISlot != NULL );
    IDE_DASSERT( aBISlot              != NULL );
    
    idlOS::memcpy( aBackupFileHdrBISlot, aBISlot, ID_SIZEOF( smriBISlot ) );

    return;
}

/***********************************************************************
 * ������ ���������� ����� �����ϴ� BI������ �����Ѵ�.
 *
 * aBackupFileHdrBISlot     - [IN] ������������ BISlot
 *
 **********************************************************************/
void smriBackupInfoMgr::clearDataFileHdrBI( smriBISlot * aDataFileHdrBI )
{
    IDE_DASSERT( aDataFileHdrBI != NULL );
        
    idlOS::memset( aDataFileHdrBI, 0x00, ID_SIZEOF( smriBISlot ) );

    return;
}

/***********************************************************************
 * BIFileHdr�� �����´�.
 *
 * aBIFileHdr     - [OUT]
 *
 **********************************************************************/
IDE_RC smriBackupInfoMgr::getBIFileHdr( smriBIFileHdr ** aBIFileHdr )
{
    IDE_DASSERT( aBIFileHdr != NULL );
    IDE_ERROR( smrRecoveryMgr::getBIMgrState() == SMRI_BI_MGR_INITIALIZED );

    *aBIFileHdr = &mBIFileHdr;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Path�̸��� ������������ �Ǵ��Ѵ�.
 * sctTableSpaceMgr::makeValidABSPath�Լ� �����Ͽ� �ۼ�
 *
 * aCheckPerm   - [IN] ���丮������ ���� �˻� ����
 * aPathName    - [IN] ���丮 ���
 *
 **********************************************************************/
IDE_RC smriBackupInfoMgr::isValidABSPath( idBool aCheckPerm, 
                                          SChar  * aPathName )
{
#if !defined(VC_WINCE) && !defined(SYMBIAN)
        
    SChar*  sPtr;
    DIR*    sDir;
    UInt    sPathNameLen;
    UInt    i;

    IDE_DASSERT( aPathName  != NULL );

    sPathNameLen = idlOS::strlen(aPathName);

    // fix BUG-15502
    IDE_TEST_RAISE( sPathNameLen == 0,
                    error_filename_is_null_string );

    /* ------------------------------------------------
     * datafile �̸��� ���� �ý��� ����� �˻�
     * ----------------------------------------------*/
#if defined(VC_WIN32)
    SInt  sIterator;
    for ( sIterator = 0; aPathName[sIterator] != '\0'; sIterator++ ) 
    {
        if ( aPathName[sIterator] == '/' ) 
        {
             aPathName[sIterator] = IDL_FILE_SEPARATOR;
        }
    }
#endif

    sPtr = idlOS::strchr(aPathName, IDL_FILE_SEPARATOR);
#ifndef VC_WIN32
    IDE_TEST_RAISE( sPtr != &aPathName[0], error_invalid_filepath_abs );
#else
    /* BUG-38278 invalid datafile path at windows server
     * �������� ȯ�濡�� '/' �� '\' �� ���۵Ǵ�
     * ��� �Է��� ������ ó���Ѵ�. */
    IDE_TEST_RAISE( sPtr == &aPathName[0], error_invalid_filepath_abs );

    IDE_TEST_RAISE( ( (aPathName[1] == ':') && (sPtr != &aPathName[2]) ) ||
                    ( (aPathName[1] != ':') && (sPtr != &aPathName[0]) ), 
                    error_invalid_filepath_abs );
#endif 

    /* ------------------------------------------------
     * ������, ���� + '/'�� ����ϰ� �׿� ���ڴ� ������� �ʴ´�.
     * (��������)
     * ----------------------------------------------*/
    for (i = 0; i < sPathNameLen; i++)
    {
        if (smuUtility::isAlNum(aPathName[i]) != ID_TRUE)
        {
            /* BUG-16283: Windows���� Altibase Home�� '(', ')' �� ��
               ��� DB ������ ������ �߻��մϴ�. */
            IDE_TEST_RAISE( (aPathName[i] != IDL_FILE_SEPARATOR) &&
                            (aPathName[i] != '-') &&
                            (aPathName[i] != '_') &&
                            (aPathName[i] != '.') &&
                            (aPathName[i] != ':') &&
                            (aPathName[i] != '(') &&
                            (aPathName[i] != ')') &&
                            (aPathName[i] != ' ')
                            ,
                            error_invalid_filepath_keyword );

            if (aPathName[i] == '.')
            {
                if ((i + 1) != sPathNameLen)
                {
                    IDE_TEST_RAISE( aPathName[i+1] == '.',
                                    error_invalid_filepath_keyword );
                    IDE_TEST_RAISE( aPathName[i+1] == IDL_FILE_SEPARATOR,
                                    error_invalid_filepath_keyword );
                }
            }
        }
    }

    // [BUG-29812] dir�� �����ϴ� Ȯ���Ѵ�.
    if ( aCheckPerm == ID_TRUE ) 
    {
        // fix BUG-19977
        IDE_TEST_RAISE( idf::access(aPathName, F_OK) != 0,
                        error_not_exist_path );

        IDE_TEST_RAISE( idf::access(aPathName, R_OK) != 0,
                        error_no_read_perm_path );
        IDE_TEST_RAISE( idf::access(aPathName, W_OK) != 0,
                        error_no_write_perm_path );
        IDE_TEST_RAISE( idf::access(aPathName, X_OK) != 0,
                        error_no_execute_perm_path );

        sDir = idf::opendir(aPathName);
        IDE_TEST_RAISE( sDir == NULL, error_open_dir ); /* BUGBUG - ERROR MSG */

        (void)idf::closedir(sDir);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( error_filename_is_null_string );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_CheckpointPathIsNullString));
    }
    IDE_EXCEPTION( error_not_exist_path );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_NoExistPath, aPathName));
    }
    IDE_EXCEPTION( error_no_read_perm_path );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_NoReadPermFile, aPathName));
    }
    IDE_EXCEPTION( error_no_write_perm_path );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_NoWritePermFile, aPathName));
    }
    IDE_EXCEPTION( error_no_execute_perm_path );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_NoExecutePermFile, aPathName));
    }
    IDE_EXCEPTION( error_invalid_filepath_abs );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_InvalidFilePathABS));
    }
    IDE_EXCEPTION( error_invalid_filepath_keyword );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_InvalidFilePathKeyWord));
    }
    IDE_EXCEPTION( error_open_dir );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_NotDir));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
#else
    // Windows CE������ ������ �����ΰ� C:�� �������� �ʴ´�.
    return IDE_SUCCESS;
#endif
}

/***********************************************************************
 * BI file Hdr�� checkum�� �˻��Ѵ�.
 *
 *
 **********************************************************************/
IDE_RC smriBackupInfoMgr::checkBIFileHdrCheckSum()
{
    UChar * sCheckSumStartPtr;
    UInt    sCheckSumValue;

    sCheckSumStartPtr = (UChar *)( &mBIFileHdr.mCheckSum ) +
                        ID_SIZEOF( mBIFileHdr.mCheckSum );

    sCheckSumValue = smuUtility::foldBinary(
                                    sCheckSumStartPtr,
                                    SMRI_BI_FILE_HDR_SIZE -
                                    ID_SIZEOF( mBIFileHdr.mCheckSum ) );

    IDE_TEST_RAISE( sCheckSumValue != mBIFileHdr.mCheckSum,
                    error_invalid_backup_info_file );

    return IDE_SUCCESS;

    IDE_EXCEPTION( error_invalid_backup_info_file );
    {  
        IDE_SET(ideSetErrorCode(smERR_ABORT_InvalidBackupInfoFile, 
                mFile.getFileName()));
    }  

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * BI file Hdr�� checkum�� ����ϰ� �����Ѵ�.
 *
 *
 **********************************************************************/
void smriBackupInfoMgr::setBIFileHdrCheckSum()
{
    UChar * sCheckSumStartPtr;
    UInt    sCheckSumValue;

    sCheckSumStartPtr = (UChar *)( &mBIFileHdr.mCheckSum ) +
                        ID_SIZEOF( mBIFileHdr.mCheckSum );

    sCheckSumValue = smuUtility::foldBinary(
                                    sCheckSumStartPtr,
                                    SMRI_BI_FILE_HDR_SIZE -
                                    ID_SIZEOF( mBIFileHdr.mCheckSum ) );

    mBIFileHdr.mCheckSum = sCheckSumValue;

    return;
}

/***********************************************************************
 * BISlot�� checkum�� �˻��Ѵ�.
 *
 *
 **********************************************************************/
IDE_RC smriBackupInfoMgr::checkBISlotCheckSum( smriBISlot * aBISlot )
{
    UChar * sCheckSumStartPtr;
    UInt    sCheckSumValue;

    IDE_DASSERT( aBISlot != NULL );

    sCheckSumStartPtr = (UChar *)( &aBISlot->mCheckSum ) +
                        ID_SIZEOF( aBISlot->mCheckSum );

    sCheckSumValue = smuUtility::foldBinary(
                                    sCheckSumStartPtr,
                                    ID_SIZEOF( smriBISlot ) -
                                    ID_SIZEOF( aBISlot->mCheckSum ) );

    IDE_TEST_RAISE( sCheckSumValue != aBISlot->mCheckSum,
                    error_invalid_backup_info_file );

    return IDE_SUCCESS;

    IDE_EXCEPTION( error_invalid_backup_info_file );
    {  
        IDE_SET(ideSetErrorCode(smERR_ABORT_InvalidBackupInfoFile, 
                mFile.getFileName()));
    }  

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * BISlot�� checkum�� ����ϰ� �����Ѵ�.
 *
 *
 **********************************************************************/
void smriBackupInfoMgr::setBISlotCheckSum( smriBISlot * aBISlot )
{
    UChar * sCheckSumStartPtr;
    UInt    sCheckSumValue;

    IDE_DASSERT( aBISlot != NULL );

    sCheckSumStartPtr = (UChar *)( &aBISlot->mCheckSum ) +
                        ID_SIZEOF( aBISlot->mCheckSum );

    sCheckSumValue = smuUtility::foldBinary(
                                    sCheckSumStartPtr,
                                    ID_SIZEOF( smriBISlot ) -
                                    ID_SIZEOF( aBISlot->mCheckSum ) );

    aBISlot->mCheckSum = sCheckSumValue;

    return;
}

/***********************************************************************
 * BIFile�� ��� �������·� rollback �Ѵ�.
 *
 *
 **********************************************************************/
IDE_RC smriBackupInfoMgr::rollbackBIFile( UInt aBackupBISlotCnt )
{
    ULong    sCurrFileSize = 0;
    ULong    sNewFileSize  = 0;
    UInt     sState        = 0;

    IDE_ERROR( aBackupBISlotCnt <= mBIFileHdr.mBISlotCnt );

    mBIFileHdr.mBISlotCnt = aBackupBISlotCnt;

    IDE_TEST( mFile.open() != IDE_SUCCESS );
    sState = 1;

    IDE_TEST( mFile.getFileSize( &sCurrFileSize ) != IDE_SUCCESS );

    sNewFileSize =  SMRI_BI_FILE_HDR_SIZE 
                    + ( ID_SIZEOF( smriBISlot ) 
                        * mBIFileHdr.mBISlotCnt );

    IDE_ERROR( sNewFileSize <= sCurrFileSize );

    IDE_TEST( mFile.truncate( sNewFileSize ) != IDE_SUCCESS );

    IDE_TEST( mFile.sync() != IDE_SUCCESS );

    sState = 0;
    IDE_TEST( mFile.close() != IDE_SUCCESS );

    IDE_TEST( smrRecoveryMgr::updateBIFileAttrToLogAnchor( 
                                                    NULL,  //aFileName
                                                    NULL,  //aBackupLSN
                                                    &mBIFileHdr.mLastBackupLSN,
                                                    NULL,  //aBackupDir
                                                    NULL ) //aDeleteArchLogFile
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch( sState )
    {
        case 1:
            IDE_ASSERT( mFile.close() == IDE_SUCCESS );
        case 0:
        default:
            break;
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * ��� ���� ������ incremental backup path�� �����Ѵ�.
 *
 *
 **********************************************************************/
IDE_RC smriBackupInfoMgr::removeBackupDir( SChar * aIncrementalBackupPath )
{
    IDE_ERROR( aIncrementalBackupPath != NULL );

    if ( idf::access(aIncrementalBackupPath, F_OK) == 0 )
    {
        IDE_TEST( processRestBackupFile( aIncrementalBackupPath, 
                                         NULL, //aDestPath
                                         NULL, //aBackupFile
                                         ID_FALSE ) // move or delete
                != IDE_SUCCESS );
    }
    else
    {
        /* 
         * nothing to do 
         * backup dir�� �̹� ������ ���� 
         */
    }

    IDE_ERROR( idf::access(aIncrementalBackupPath, F_OK) != 0 );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * MemBase�� ����� DBName�� BIFile�� ����� DBName�� ���Ѵ�.
 *
 * aDBName    - [IN] MemBase�� ����� DBName
 *
 **********************************************************************/
IDE_RC smriBackupInfoMgr::checkDBName( SChar * aDBName )
{
    IDE_DASSERT( aDBName != NULL );

    IDE_TEST_RAISE( idlOS::strcmp( mBIFileHdr.mDBName,
                                   (const char *)aDBName ) != 0,
                    error_invalid_backup_info_file );

    return IDE_SUCCESS;

    IDE_EXCEPTION( error_invalid_backup_info_file );
    {  
        IDE_SET(ideSetErrorCode(smERR_ABORT_InvalidBackupInfoFile, 
                                mFile.getFileName()));
    }  

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


