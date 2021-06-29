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
 * $Id: smiMediaRecovery.cpp 86110 2019-09-02 04:52:04Z et16 $
 **********************************************************************/
/**********************************************************************
 * FILE DESCRIPTION : smiMediaRecovery.cpp                            *
 * -------------------------------------------------------------------*
 * media recovey�� ���� inteface ����̸�, ������ ���� ����� �����Ѵ�.
 *
 * (1) backup�� ���� ����Ÿ ������ �ջ�, ������ �� ��쿡 ���ο� empty
 *     ����Ÿ���� �����Ѵ�.
 * (2) disk �� ������, �ٸ� ��ġ�� ����Ÿ ������ �ű�� ��츦 ����
 *     ����Ÿ���� �̸� ����.
 * (3) incompleted recovery ������ ���� redo log���ϵ鿡 ���� redo all/undo
 *     all�� skip�ϱ� ���Ͽ�, �α� ���ϵ��� �ʱ�ȭ �۾�.
 * (4) database/tablespace/datafile���� media recovery
 *     ��, datafile������ completed media recovery�� �����ϴ�.
 **********************************************************************/

#include <ideErrorMgr.h>
#include <smErrorCode.h>
#include <smiMain.h>
#include <smiMediaRecovery.h>
#include <smiBackup.h>
#include <sdd.h>
#include <smr.h>
#include <sct.h>

/*
  BACKUP�� ���� ��ũ ����Ÿ ������ �ջ�, ������
  �� ��쿡 ���ο� �� ����Ÿ ���� �����Ѵ�.

  SQL���� : ALTER DATABASE CREATE DATAFILE 'OLD-FILESPEC' [ AS 'NEW-FILESPEC' ];

  - aNewFileSpec�� null�� ��쿡��  ���� ����Ÿ���ϰ�
  ������ �̸��� �Ӽ��� ������ �� ����Ÿ �����������Ѵ�.

  - aNewFileSpec�� null�� �ƴ� ��쿡�� ���� ����Ÿ ���ϰ�
  �����ѼӼ������� , ���� ��ġ�� �ٸ� ����̴�.

  [ �˰��� ]

  ���� �����̸����� hash���� ����Ÿ ���ϳ�带 �˻�;
  if(�˻��� fail )  then
      return failure
  fi

  if( ���ο� ��ġ�� �����Ǿ����� ) // aNewFileSpec !=NULL
  then
      ����Ÿ ���� ����� file�̸��� �����Ѵ�.
      ���ο� ����Ÿ  ������ġ�� ����
      // BUGBUG- logAnchor  sync�� restart recovery
      // �Ŀ� �ϴ°����� ����.
  else
      // ���� ��ġ�� ���� �������� �����Ѵ�.
      if( ���� ������ �����ϴ��� Ȯ��) // aSrcFileName.
      then
          return failure
      fi
      ���ο� ����Ÿ ���� ����;
  fi //else

  [����]

  [IN] aOldFileSpec - �α׾�Ŀ���� ���ϰ�� [ FILESPEC ]
  [IN] aNewFileSpec - �ٸ����� ������ �����ϴ� ��� ���ϰ�� [ FILESPEC ]
*/
IDE_RC smiMediaRecovery::createDatafile( SChar* aOldFileSpec,
                                         SChar* aNewFileSpec )
{
    UInt             sStrLen;
    scSpaceID        sSpaceID;
    sddDataFileHdr   sDBFileHdr;
    sddDataFileNode* sOldFileNode;
    sddDataFileNode* sNewFileNode;
    SChar            sOldFileSpec[ SMI_MAX_DATAFILE_NAME_LEN + 1 ];
    SChar            sNewFileSpec[ SMI_MAX_DATAFILE_NAME_LEN + 1 ];
    smLSN            sDiskRedoLSN;

    IDE_ASSERT( aOldFileSpec != NULL );

    idlOS::memset( sOldFileSpec, 0x00, SMI_MAX_DATAFILE_NAME_LEN + 1 );
    idlOS::memset( sNewFileSpec, 0x00, SMI_MAX_DATAFILE_NAME_LEN + 1 );

    // �ٴܰ�startup�ܰ��� control �ܰ迡���� �Ҹ��� �ִ�.
    IDE_TEST_RAISE( smiGetStartupPhase() != SMI_STARTUP_CONTROL,
                    err_startup_phase );

    // [1] ���� FILESPEC�� �����Ѵ�.
    idlOS::strncpy( sOldFileSpec,
                    aOldFileSpec,
                    SMI_MAX_DATAFILE_NAME_LEN );
    sOldFileSpec[ SMI_MAX_DATAFILE_NAME_LEN ] = '\0';

    sStrLen = idlOS::strlen( sOldFileSpec );

    // [2] ���� �۹̼� Ȯ�� �� �����η� ��ȯ�Ѵ�.
    IDE_TEST( sctTableSpaceMgr::makeValidABSPath( ID_TRUE,
                                                  sOldFileSpec,
                                                  &sStrLen,
                                                  SMI_TBS_DISK )
              != IDE_SUCCESS );

    // [3] ���� �����̸����� hash���� ����Ÿ ���ϳ�带 �˻��Ѵ�.
    sctTableSpaceMgr::getDataFileNodeByName( sOldFileSpec,
                                             &sOldFileNode,
                                             &sSpaceID );

    IDE_TEST_RAISE( sOldFileNode == NULL, err_not_found_filenode );

    // [4] Empty ����Ÿ������ ����� �����ϱ� ���� ��������� �����Ѵ�.
    idlOS::memset(&sDBFileHdr, 0x00, ID_SIZEOF(sddDataFileHdr));
    sDBFileHdr.mSmVersion = smVersionID;
    SM_GET_LSN( sDBFileHdr.mCreateLSN,
                sOldFileNode->mDBFileHdr.mCreateLSN );

    /* BUG-40371
     * ������ Empty File�� CreateLSN ���� LogAnchor�� RedoLSN ������ ū ���
     * Empty File�� RedoLSN �� CreateLSN�� ���� �����Ͽ�
     * Restart Recovery�� ����ɼ� �ֵ��� �Ѵ�. */
    smrRecoveryMgr::getDiskRedoLSNFromLogAnchor( &sDiskRedoLSN );
    if ( smrCompareLSN::isGT( &sDBFileHdr.mCreateLSN , &sDiskRedoLSN ) == ID_TRUE )
    {
         SM_GET_LSN( sDBFileHdr.mRedoLSN,
                     sDBFileHdr.mCreateLSN );
    }
    else
    {
        /* nothing to do ... */
    }

    // [5] Empty ����Ÿ������ �����Ѵ�.
    if( aNewFileSpec != NULL)
    {
        // ���ο� ��ο� ����Ÿ������ �����Ѵ�.
        // [5-1] ���ο� FILESPEC�� �����Ѵ�.
        idlOS::strncpy( sNewFileSpec,
                        aNewFileSpec,
                        SMI_MAX_DATAFILE_NAME_LEN );
        sNewFileSpec[ SMI_MAX_DATAFILE_NAME_LEN ] = '\0';

        sStrLen = idlOS::strlen(sNewFileSpec);

        // [5-2] ���� �۹̼� Ȯ�� �� �����η� ��ȯ�Ѵ�.
        IDE_TEST( sctTableSpaceMgr::makeValidABSPath(
                                    ID_TRUE,
                                    sNewFileSpec,
                                    &sStrLen,
                                    SMI_TBS_DISK )
                 != IDE_SUCCESS );

        // [5-3] ���ο� �����̸����� hash���� ����Ÿ ���ϳ�带 �˻��Ѵ�.
        sctTableSpaceMgr::getDataFileNodeByName( sNewFileSpec,
                                                 &sNewFileNode,
                                                 &sSpaceID );

        IDE_TEST_RAISE(sNewFileNode != NULL, err_inuse_filename );

        // [5-4] ���ο� �����̸����� ����.
        IDE_TEST(sddDataFile::setDataFileName( sOldFileNode,
                                               sNewFileSpec,
                                               ID_FALSE )
                 != IDE_SUCCESS);
    }
    else
    {
        // ���� ��ο� ����Ÿ������ �����Ѵ�.

        // ���� ��ο� ����Ÿ������ �̹� �����ϴ��� �˻�
        IDE_TEST_RAISE( idf::access(sOldFileSpec, F_OK) == 0,
                        err_already_exist_datafile );
    }

    // [6] ���� ���Ϸ� ������ ������������ ,
    // ���ο� ����Ÿ������ �����Ѵ�.
    IDE_TEST( sddDataFile::create( NULL, sOldFileNode, &sDBFileHdr )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_startup_phase);
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_MediaRecoDataFile,
                                "ALTER DATABASE CRAEATE DATAFILE"));
    }
    IDE_EXCEPTION( err_not_found_filenode );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_NotFoundDataFileNode,
                                aOldFileSpec));
    }
    IDE_EXCEPTION( err_already_exist_datafile );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_AlreadyExistFile, sOldFileSpec));
    }
    IDE_EXCEPTION( err_inuse_filename );
    {
        IDE_SET( ideSetErrorCode(smERR_ABORT_UseFileInOtherTBS,
                 sNewFileSpec) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/*
  BACKUP�� ���� ��ũ ����Ÿ ������ �ջ�, ������
  �� ��쿡 ���ο� �� ����Ÿ ���� �����Ѵ�.

  SQL���� : ALTER DATABASE CREATE CHECKPOINT IMAGE 'FILESPEC';

  [IN] aOldFileSpec - �α׾�Ŀ���� ���ϰ�� [ FILESPEC ]
  [IN] aNewFileSpec - �ٸ����� ������ �����ϴ� ��� ���ϰ�� [ FILESPEC ]

  [ ������� ]
  STABLE�� üũ����Ʈ �̹����� �Է¹޾Ƽ� 2���� üũ����Ʈ �̹�����
  �����Ѵ�.
*/
IDE_RC smiMediaRecovery::createChkptImage( SChar * aFileName )
{
    SInt                 sPingPongNum;
    UInt                 sLoop;
    UInt                 sFileNum;
    UInt                 sStrLength;
    smmChkptImageHdr     sChkptImageHdr;
    sctTableSpaceNode  * sSpaceNode;
    smmDatabaseFile    * sDatabaseFile[SMM_PINGPONG_COUNT];
    SChar              * sChrPtr1;
    SChar              * sChrPtr2;
    SChar                sSpaceName[ SMI_MAX_TABLESPACE_NAME_LEN + 1 ];
    SChar                sNumberStr[ SM_MAX_FILE_NAME + 1 ];
    SChar                sCreateDir[ SM_MAX_FILE_NAME + 1 ];
    SChar                sFileName [ SM_MAX_FILE_NAME + 1 ];
    SChar                sFullPath [ SM_MAX_FILE_NAME + 1 ];
    smLSN                sInitLSN;
    smmTBSNode         * sTBSNode    = NULL;
    smmTBSNode         * sDicTBSNode = NULL;
    smmMemBase           sDicMemBase;
    void               * sPageBuffer = NULL;
    void               * sDummyPage  = NULL;
    UChar              * sBasePage   = NULL;
    UInt                 sStage      = 0;

    IDE_DASSERT( aFileName != NULL );

    SM_LSN_INIT( sInitLSN );

    // �ٴܰ�startup�ܰ��� control �ܰ迡���� �Ҹ��� �ִ�.
    IDE_TEST_RAISE( smiGetStartupPhase() != SMI_STARTUP_CONTROL,
                    err_startup_phase );

    idlOS::memset( sSpaceName,  // ���̺����̽� ��
                   0x00,
                   SMI_MAX_TABLESPACE_NAME_LEN + 1 );

    idlOS::memset( sFileName, // üũ����Ʈ �̹��� ��
                   0x00,
                   SM_MAX_FILE_NAME + 1 );

    idlOS::memset( sCreateDir, // ���丮 ��
                   0x00,
                   SM_MAX_FILE_NAME + 1 );

    idlOS::memset( sFullPath, // üũ����Ʈ �̹��� ���
                   0x00,
                   SM_MAX_FILE_NAME + 1 );

    // [1] �Էµ� FileSpec�� �����Ѵ�.
    idlOS::strncpy( sFileName,
                    aFileName,
                    SM_MAX_FILE_NAME );
    sFileName[ SM_MAX_FILE_NAME ] = '\0';

    sStrLength = idlOS::strlen( sFileName );

    // [2] ���̺����̽� �̸��� �����Ѵ�.
    sChrPtr1 = idlOS::strchr( sFileName, '-' );
    IDE_TEST_RAISE( sChrPtr1 == NULL, err_invalie_filespec_format );

    idlOS::strncpy( sSpaceName, sFileName, (sChrPtr1 - sFileName) );
    sSpaceName[ SMI_MAX_TABLESPACE_NAME_LEN ] = '\0';

    sChrPtr1++;

    // [3] FILESPEC���κ��� ������ȣ�� ��´�.
    sChrPtr2 = idlOS::strchr( sChrPtr1, '-' );
    IDE_TEST_RAISE( sChrPtr2 == NULL, err_invalie_filespec_format );

    idlOS::memset( sNumberStr, 0x00, SM_MAX_FILE_NAME + 1 );
    idlOS::strncpy( sNumberStr,
                    sChrPtr1,
                    (sChrPtr2 - sChrPtr1) );

    sNumberStr[ SM_MAX_FILE_NAME ] = '\0';
    sPingPongNum = (UInt)idlOS::atoi( sNumberStr );

    IDE_TEST_RAISE( sPingPongNum >= SMM_PINGPONG_COUNT,
                    err_not_exist_datafile );

    sChrPtr2++;

    // [4] FILESPEC���κ��� ���Ϲ�ȣ�� ��´�.
    idlOS::memset( sNumberStr, 0x00, SM_MAX_FILE_NAME + 1 );
    idlOS::strncpy( sNumberStr,
                    sChrPtr2, (sStrLength - (UInt)(sChrPtr2 - sFileName)) );

    sNumberStr[ SM_MAX_FILE_NAME ] = '\0';

    IDE_TEST_RAISE( smuUtility::isDigitForString(
                           &sNumberStr[0],
                           sStrLength - (UInt)(sChrPtr2 - sFileName) ) == ID_FALSE,
                    err_not_found_tablespace_by_name );

    sFileNum = (UInt)idlOS::atoi( sNumberStr );

    // [5] ���̺����̽� �˻�
    IDE_TEST( sctTableSpaceMgr::findSpaceNodeByName( sSpaceName,
                                                     (void**)&sSpaceNode,
                                                     ID_FALSE ) // lock
              != IDE_SUCCESS );

    IDE_TEST_RAISE( sSpaceNode == NULL, err_not_found_tablespace_by_name );

    // �޸����̺����̽����� Ȯ�� => ���� : ���Ͼ���
    IDE_TEST_RAISE( sctTableSpaceMgr::isMemTableSpace( sSpaceNode ) != ID_TRUE,
                    err_not_found_tablespace_by_name );

    // ������ȣ Ȯ�� => ���� : ���� ����
    IDE_TEST_RAISE( sPingPongNum >= SMM_PINGPONG_COUNT,
                    err_not_exist_datafile );

    for ( sLoop = 0 ; sLoop < SMM_PINGPONG_COUNT ; sLoop ++ )
    {
        // CREATE LSN Ȯ�� => ���� : ���� ����
        IDE_TEST( smmManager::getDBFile( (smmTBSNode*)sSpaceNode,
                                         sLoop,
                                         sFileNum,
                                         SMM_GETDBFILEOP_NONE,
                                         &sDatabaseFile[sLoop] )
                  != IDE_SUCCESS );

        sDatabaseFile[sLoop]->getChkptImageHdr( &sChkptImageHdr );

        IDE_TEST_RAISE( sDatabaseFile[sLoop]->checkValuesOfDBFHdr( &sChkptImageHdr ),
                        err_not_exist_datafile );

        // [6] ����Ÿ������ �����ϴ��� �˻��Ѵ�.
        IDE_TEST( smmDatabaseFile::makeDBDirForCreate(
                                   (smmTBSNode*)sSpaceNode,
                                   sFileNum,
                                   sCreateDir ) != IDE_SUCCESS );

        idlOS::snprintf( sFullPath, SM_MAX_FILE_NAME,
                                    "%s%c%s-%"ID_INT32_FMT"-%"ID_INT32_FMT,
                                    sCreateDir,
                                    IDL_FILE_SEPARATOR,
                                    sSpaceName,
                                    sLoop,
                                    sFileNum );

        // [7] ���� ��ο� ����Ÿ������ �̹� �����ϴ��� �˻�
        IDE_TEST_RAISE( idf::access( sFullPath, F_OK ) == 0,
                        err_already_exist_datafile );
    }

    // STABLE DB Ȯ�� => ���� : STABLE�� ���ϸ� �Է� �䱸
    IDE_TEST_RAISE( sPingPongNum != smmManager::getCurrentDB( (smmTBSNode*)sSpaceNode ),
                    err_input_unstable_chkpt_image );

    // [8] ���� ���Ϸ� ������ ������������, Empty ����Ÿ������
    // �����Ѵ�. ����Ÿ������ ����� RedoLSN�� ��� INIT����
    // �����ϰ�, �������� �α׾�Ŀ�� ����� �������� �����Ѵ�.
    /* BUG-39354 
     * Empty Memory Checkpoint Image ������ RedoLSN�� INIT���� �����ϸ�
     * ���������� ��ȿ�� �˻縦 ������� ���մϴ�.
     * Empty ������ RedoLSN�� CreateLSN�� ���� ������ �����Ͽ�
     * RedoLSN�� CreateLSN�� ������ Empty ���Ϸ� �˼� �ְ� �մϴ�. */
    SM_GET_LSN( sChkptImageHdr.mMemRedoLSN, sChkptImageHdr.mMemCreateLSN );

    for ( sLoop = 0 ; sLoop < SMM_PINGPONG_COUNT ; sLoop ++ )
    {
        /* BUG-39354
         * �α׾�Ŀ�� ������ �״�� ����� ���
         * ����Ÿ������ �����Ѵٰ� ��ϵǾ� �ֱ� ������
         * ���� ����Ÿ������ �������� �ʾ� ������ �߻��մϴ�.
         * ���� ������ ������ �������� �ʴٰ� ������ ������ �־���մϴ�. */
        smmManager::setCreateDBFileOnDisk( (smmTBSNode*)sSpaceNode,
                                           sLoop,
                                           sFileNum,
                                           ID_FALSE );

        IDE_TEST( sDatabaseFile[sLoop]->createDbFile(
                                            (smmTBSNode*)sSpaceNode,
                                            sLoop,
                                            sFileNum,
                                            0 /* ��������������� ��� */,
                                            &sChkptImageHdr )
                  != IDE_SUCCESS );
    }

    /* BUG-39354
     * Memory Checkpoint Image ������ 0�� �������� Membase ������ ������ �ֽ��ϴ�.
     * Membase�� �������� ���� �־�߸� Media Recovery�� ������ �� �ֱ� ������
     * Membase�� ������ �־���մϴ�.
     * DicMemBase�� �о�ͼ� �ʿ��� ���� �����ϰ� �ʱⰪ�� �ʿ��� ���� �ʱ�ȭ �����ݴϴ�. */
    sDicTBSNode = (smmTBSNode*)sctTableSpaceMgr::getFirstSpaceNode();
    IDE_TEST( smmManager::readMemBaseFromFile( sDicTBSNode,
                                               &sDicMemBase )
              != IDE_SUCCESS);

    IDE_TEST( iduFile::allocBuff4DirectIO( IDU_MEM_SM_SMI,
                                           SM_PAGE_SIZE,
                                           (void**)&sPageBuffer,
                                           (void**)&sDummyPage )
              != IDE_SUCCESS );
    sStage = 1;
    IDU_FIT_POINT( "BUG-45283@smiMediaRecovery::createChkptImage" );

    idlOS::memset( sDummyPage, 0, SM_PAGE_SIZE );
    sBasePage = (UChar*) sDummyPage;

    sTBSNode           = (smmTBSNode*) sSpaceNode;
    sTBSNode->mMemBase = (smmMemBase *)(sBasePage + SMM_MEMBASE_OFFSET);

    /* BUG-39354
     * DataFile Signature�� ������ �� TBS ���� ������ ���̳�
     * Valid Check�ÿ� ���Ǵ� ���� �׻� DicMemBase���̱� ������
     * �׳� �������־����ϴ�. */
    idlOS::memcpy( sTBSNode->mMemBase,
                   &sDicMemBase,
                   ID_SIZEOF(smmMemBase) );

    sTBSNode->mMemBase->mAllocPersPageCount    = 0;
    sTBSNode->mMemBase->mCurrentExpandChunkCnt = 0;
    SM_INIT_SCN( &(sTBSNode->mMemBase->mSystemSCN) );

    for ( sLoop = 0 ; sLoop < SMM_PINGPONG_COUNT ; sLoop++ )
    {
        (void)sDatabaseFile[sLoop]->writePage( sTBSNode, SMM_MEMBASE_PAGEID, (UChar*) sDummyPage );
        (void)sDatabaseFile[sLoop]->syncUntilSuccess();
    }

    sStage = 0;
    IDE_TEST( iduMemMgr::free( sPageBuffer ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_startup_phase );
    {
        IDE_SET(ideSetErrorCode( smERR_ABORT_MediaRecoDataFile,
                                 "ALTER DATABASE CRAEATE CHECKPOINT IMAGE") );
    }
    IDE_EXCEPTION( err_invalie_filespec_format );
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_INVALID_CIMAGE_FILESPEC_FORMAT,
                                  sFileName ) );
    }
    IDE_EXCEPTION( err_not_found_tablespace_by_name );
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_NotFoundTableSpaceNodeByName,
                                  sSpaceName ) );
    }
    IDE_EXCEPTION( err_already_exist_datafile );
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_AlreadyExistFile, sFullPath ) );
    }
    IDE_EXCEPTION( err_not_exist_datafile );
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_NotExistFile ) );
    }
    IDE_EXCEPTION( err_input_unstable_chkpt_image );
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_INPUT_UNSTABLE_CIMAGE,
                                  sFileName ));
    }
    IDE_EXCEPTION_END;

    switch (sStage)
    {
        case 1:
            IDE_ASSERT( iduMemMgr::free( sPageBuffer ) == IDE_SUCCESS );
        default:
            break;
    }

    return IDE_FAILURE;

}

/*********************************************************
 * function description:
 * - disk �� ������, �ٸ� ��ġ�� ����Ÿ ������ �ű��
 *   ��츦 ���� ����Ÿ ���� �̸� ����.
 *
 *   alter database rename file 'old-file-name' to 'new_file_name'
 *
 * - pseudo code
 *   ���� �����̸����� hash���� ����Ÿ ���ϳ�带 �˻�;
 *   if(�˻��� ����)
 *   then
 *       ����Ÿ ���� ����� file�̸��� �����Ѵ�.
 *      // BUGBUG- logAnchor  sync�� restart recovery
 *     // �Ŀ� �ϴ°����� ����.
 *   else
 *    return failture
 *   fi
 *********************************************************/
IDE_RC smiMediaRecovery::renameDataFile(SChar* aOldFileSpec,
                                        SChar* aNewFileSpec)
{

    UInt             sStrLength;
    scSpaceID        sSpaceID;
    sddDataFileNode* sOldFileNode;
    sddDataFileNode* sNewFileNode;
    SChar            sOldFileSpec[SMI_MAX_DATAFILE_NAME_LEN + 1];
    SChar            sNewFileSpec[SMI_MAX_DATAFILE_NAME_LEN + 1];

    IDE_ASSERT( aOldFileSpec != NULL);
    IDE_ASSERT( aNewFileSpec != NULL);

    // �ٴܰ�startup�ܰ��� control �ܰ迡���� �Ҹ��� �ִ�.
    IDE_TEST_RAISE(smiGetStartupPhase() != SMI_STARTUP_CONTROL,
                   err_startup_phase);

    idlOS::memset(sOldFileSpec, 0x00,SMI_MAX_DATAFILE_NAME_LEN);
    idlOS::memset(sNewFileSpec, 0x00,SMI_MAX_DATAFILE_NAME_LEN);

    // [1] ���� FILESPEC�� �����Ѵ�.
    idlOS::strncpy( sOldFileSpec,
                    aOldFileSpec,
                    SMI_MAX_DATAFILE_NAME_LEN );
    sOldFileSpec[ SMI_MAX_DATAFILE_NAME_LEN ] = '\0';

    sStrLength = idlOS::strlen(sOldFileSpec);

    IDE_TEST( sctTableSpaceMgr::makeValidABSPath(
                                ID_FALSE,
                                sOldFileSpec,
                                &sStrLength,
                                SMI_TBS_DISK )
              != IDE_SUCCESS );

    // [2] �� FILESPEC�� �����Ѵ�.
    idlOS::strncpy( sNewFileSpec,
                    aNewFileSpec,
                    SMI_MAX_DATAFILE_NAME_LEN );
    sNewFileSpec[ SMI_MAX_DATAFILE_NAME_LEN ] = '\0';

    sStrLength = idlOS::strlen(sNewFileSpec);

    IDE_TEST( sctTableSpaceMgr::makeValidABSPath(
                                ID_TRUE,
                                sNewFileSpec,
                                &sStrLength,
                                SMI_TBS_DISK )
              != IDE_SUCCESS );

    //���� �����̸����� hash���� ����Ÿ ���ϳ�带 �˻�.
    sctTableSpaceMgr::getDataFileNodeByName( sOldFileSpec,
                                             &sOldFileNode,
                                             &sSpaceID );

    IDE_TEST_RAISE(sOldFileNode == NULL, err_not_found_filenode);

    sctTableSpaceMgr::getDataFileNodeByName( sNewFileSpec,
                                             &sNewFileNode,
                                             &sSpaceID );

    IDE_TEST_RAISE(sNewFileNode != NULL, err_inuse_filename );

    IDE_TEST( sddDiskMgr::alterDataFileName( NULL, /* idvSQL* */
                                             sSpaceID,
                                             sOldFileSpec,
                                             sNewFileSpec )
             != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_startup_phase);
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_MediaRecoDataFile,
                                  "ALTER DATABASE RENAME DATAFILE") );
    }
    IDE_EXCEPTION( err_not_found_filenode );
    {
        IDE_SET(ideSetErrorCode( smERR_ABORT_NotFoundDataFileNode,
                                 sOldFileSpec ) );
    }
    IDE_EXCEPTION( err_inuse_filename );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_UseFileInOtherTBS, sNewFileSpec));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/*********************************************************
 * function description: smiMediaRecovery::recoverDatabase
 * - ���� �������� media recovery �Ϸ��� aUntilTime��
 *   ULong max��.
 *
 * - ������ Ư���������� DB���� ��������
 *   date�� idlOS::time(0)���� ��ȯ�� ���� �ѱ��.
 *   recover database recover database until time '2005-01-29:17:55:00'
 *
 *********************************************************/
IDE_RC smiMediaRecovery::recoverDB(idvSQL*        aStatistics,
                                   smiRecoverType aRecvType,
                                   UInt           aUntilTime,
                                   SChar*         aUntilTag)
{
    SChar * sLastRestoredTagName;

    IDE_TEST_RAISE(smiGetStartupPhase() != SMI_STARTUP_CONTROL,
                   err_startup_phase);

    IDE_TEST_RAISE(smrRecoveryMgr::getArchiveMode() != SMI_LOG_ARCHIVE,
                   err_archivelog_mode);

    if( aRecvType == SMI_RECOVER_UNTILTAG )
    {
        sLastRestoredTagName = smrRecoveryMgr::getLastRestoredTagName();
        IDE_TEST_RAISE( idlOS::strcmp( sLastRestoredTagName, aUntilTag ) != 0,
                        err_until_tag );

        aRecvType  = SMI_RECOVER_UNTILTIME;
        aUntilTime = smrRecoveryMgr::getCurrMediaTime();
    }
    else
    {
        /* nothing to do */
    }

    IDE_TEST( smrRecoveryMgr::recoverDB( aStatistics,
                                         aRecvType,
                                         aUntilTime ) != IDE_SUCCESS );

    smrRecoveryMgr::initLastRestoredTagName();

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_startup_phase);
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_MediaRecoDataFile,
                           "ALTER RECOVER DATABASE"));
    }
    IDE_EXCEPTION(err_archivelog_mode);
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_ErrArchiveLogMode));
    }
    IDE_EXCEPTION(err_until_tag);
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_ErrUntilTag, 
                                sLastRestoredTagName));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/*********************************************************
 * PROJ-2133 incremental backup
 * function description: smiMediaRecovery::restoreDatabase
 * - ���� �������� media restore �Ϸ��� aUntilTime��
 *   ULong max��, aUntilTag�� NULL�� �Ѵ�.
 *
 * - ������ Ư���������� DB���� ��������
 *   date�� idlOS::time(0)���� ��ȯ�� ���� �ѱ��.
 *   recover database restore database until time '2005-01-29:17:55:00'
 *
 *  or
 *
 *   tag�� ������ ����Ѵ�.
 *   recover database restore database from tag 'tag_name'
 *
 *********************************************************/
IDE_RC smiMediaRecovery::restoreDB( smiRestoreType    aRestoreType,
                                    UInt              aUntilTime,
                                    SChar           * aUntilTag )
{
    IDE_TEST_RAISE(smiGetStartupPhase() != SMI_STARTUP_CONTROL,
                   err_startup_phase);

    IDE_TEST_RAISE(smrRecoveryMgr::getArchiveMode() != SMI_LOG_ARCHIVE,
                   err_archivelog_mode);

    IDE_TEST( smrRecoveryMgr::restoreDB( aRestoreType,
                                         aUntilTime,  
                                         aUntilTag ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_startup_phase);
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_MediaRecoDataFile,
                           "ALTER RESTORE DATABASE"));
    }
    IDE_EXCEPTION(err_archivelog_mode);
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_ErrArchiveLogMode));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************
 * PROJ-2133 incremental backup
 * function description: smiMediaRecovery::restoreDatabase
 * TBS�� ������ ������ ���������� �����ϱ� ������
 * �ҿ��������� ���õ� ������ �ʿ� ����.
 *
 *
 *********************************************************/
IDE_RC smiMediaRecovery::restoreTBS( scSpaceID    aSpaceID )
{

    IDE_TEST_RAISE(smiGetStartupPhase() != SMI_STARTUP_CONTROL,
                   err_startup_phase);

    IDE_TEST_RAISE(smrRecoveryMgr::getArchiveMode() != SMI_LOG_ARCHIVE,
                   err_archivelog_mode);

    IDE_TEST( smrRecoveryMgr::restoreTBS( aSpaceID ) != IDE_SUCCESS );
        
    return IDE_SUCCESS;

    IDE_EXCEPTION(err_startup_phase);
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_MediaRecoDataFile,
                           "ALTER RECOVER DATABASE"));
    }
    IDE_EXCEPTION(err_archivelog_mode);
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_ErrArchiveLogMode));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}
