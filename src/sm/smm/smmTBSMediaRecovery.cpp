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
* $Id: smmTBSMediaRecovery.cpp 19201 2006-11-30 00:54:40Z kmkim $
**********************************************************************/

#include <idl.h>
#include <idm.h>
#include <idu.h>
#include <ideErrorMgr.h>
#include <smDef.h>
#include <smErrorCode.h>
#include <smu.h>
#include <smm.h>
#include <smmReq.h>
#include <sctTableSpaceMgr.h>

/*
  ������ (�ƹ��͵� ����)
*/
smmTBSMediaRecovery::smmTBSMediaRecovery()
{

}


/*
   PRJ-1548 User Memory Tablespace

   �޸� ���̺����̽��� ��� ����Ѵ�.

  [IN] aTrans : Ʈ�����
  [IN] aBackupDir : ��� Dest. ���丮
*/
IDE_RC smmTBSMediaRecovery::backupAllMemoryTBS( idvSQL * aStatistics,
                                                void   * aTrans,
                                                SChar  * aBackupDir )
{
    sctActBackupArgs sActBackupArgs;

    IDE_DASSERT( aTrans != NULL );
    IDE_DASSERT( aBackupDir != NULL );

    sActBackupArgs.mTrans               = aTrans;
    sActBackupArgs.mBackupDir           = aBackupDir;
    sActBackupArgs.mCommonBackupInfo    = NULL;
    sActBackupArgs.mIsIncrementalBackup = ID_FALSE;

    // BUG-27204 Database ��� �� session event check���� ����
    IDE_TEST( sctTableSpaceMgr::doAction4EachTBS(
                        aStatistics,
                        smmTBSMediaRecovery::doActOnlineBackup,
                        (void*)&sActBackupArgs, /* Action Argument*/
                        SCT_ACT_MODE_NONE) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/*
  PRJ-1548 User Memory Tablespace

  ���̺����̽��� ���¸� �����ϰ� ����� �����Ѵ�.
  CREATING ���̰ų� DROPPING ���� ��� TBS Mgr Latch�� Ǯ��
  ��� ����ϴٰ� Latch�� �ٽ� �õ��� ���� �ٽ� �õ��Ѵ�.

  �� �Լ��� ȣ��Ǳ����� TBS Mgr Latch�� ȹ��� �����̴�.

  [IN] aSpaceNode : ����� TBS Node
  [IN] aActionArg : ����� �ʿ��� ����
*/
IDE_RC smmTBSMediaRecovery::doActOnlineBackup(
                              idvSQL            * aStatistics,
                              sctTableSpaceNode * aSpaceNode,
                              void              * aActionArg )
{
    sctActBackupArgs * sActBackupArgs;
    UInt               sState = 0;

    IDE_DASSERT( aSpaceNode != NULL );
    IDE_DASSERT( aActionArg != NULL );

    sActBackupArgs = (sctActBackupArgs*)aActionArg;

    if ( sctTableSpaceMgr::isMemTableSpace( aSpaceNode ) == ID_TRUE )
    {
        if ( ((aSpaceNode->mState & SMI_TBS_DROPPED)   != SMI_TBS_DROPPED) &&
             ((aSpaceNode->mState & SMI_TBS_DISCARDED) != SMI_TBS_DISCARDED) )
        {
            /* ------------------------------------------------
             * [1] disk table space backup ���� �����
             * ù��° ����Ÿ������ ���¸� backup���� �����Ѵ�.
             * ----------------------------------------------*/
            IDE_TEST( sctTableSpaceMgr::startTableSpaceBackup(
                          aStatistics,
                          aSpaceNode ) != IDE_SUCCESS );
            sState = 1;

            if ( (aSpaceNode->mState & SMI_TBS_DROPPED) != SMI_TBS_DROPPED )
            {
                IDE_DASSERT( SMI_TBS_IS_ONLINE(aSpaceNode->mState));

                if ( sActBackupArgs->mIsIncrementalBackup == ID_TRUE )
                {
                    IDE_TEST( smLayerCallback::incrementalBackupMemoryTBS(
                                  aStatistics,
                                  (smmTBSNode*)aSpaceNode,
                                  sActBackupArgs->mBackupDir,
                                  sActBackupArgs->mCommonBackupInfo )
                              != IDE_SUCCESS );

                }
                else
                {
                    IDE_TEST( smLayerCallback::backupMemoryTBS(
                                  aStatistics,
                                  (smmTBSNode*)aSpaceNode,
                                  sActBackupArgs->mBackupDir )
                              != IDE_SUCCESS );
                }
            }
            else
            {
                // ���̺����̽��� DROPPED, DISCARDED ������ ��� ����� ���� �ʴ´�.
                // NOTHING TO DO ...
            }

            sState = 0;
            IDE_TEST( sctTableSpaceMgr::endTableSpaceBackup( aStatistics,
                                                             aSpaceNode ) != IDE_SUCCESS );
        }
    }
    else
    {
        // ��ũ ���̺����̽��� ����� �� �Լ����� ó������ �ʴ´�.
        // NOTHING TO DO..
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sState != 0 )
    {
        sctTableSpaceMgr::endTableSpaceBackup( aStatistics,
                                               aSpaceNode ) ;
    }

    return IDE_FAILURE;

}

/*
   PROJ-2133 incremental backup

   �޸� ���̺����̽��� ��� incremental ����Ѵ�.

  [IN] aTrans : Ʈ�����
  [IN] aBackupDir : ��� Dest. ���丮
*/
IDE_RC smmTBSMediaRecovery::incrementalBackupAllMemoryTBS( 
                                                idvSQL     * aStatistics,
                                                void       * aTrans,
                                                smriBISlot * aCommonBackupInfo,
                                                SChar      * aBackupDir )
{
    sctActBackupArgs sActBackupArgs;

    IDE_DASSERT( aTrans != NULL );
    IDE_DASSERT( aBackupDir != NULL );

    sActBackupArgs.mTrans               = aTrans;
    sActBackupArgs.mBackupDir           = aBackupDir;
    sActBackupArgs.mCommonBackupInfo    = aCommonBackupInfo;
    sActBackupArgs.mIsIncrementalBackup = ID_TRUE;

    // BUG-27204 Database ��� �� session event check���� ����
    IDE_TEST( sctTableSpaceMgr::doAction4EachTBS(
                        aStatistics,
                        smmTBSMediaRecovery::doActOnlineBackup,
                        (void*)&sActBackupArgs, /* Action Argument*/
                        SCT_ACT_MODE_NONE) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
    �ϳ��� Tablespace�� ���� ��� DB file�� Header�� Redo LSN�� ���

    aSpaceNode [IN] Redo LSN�� ��ϵ� Tablespace�� Node

    [����] Tablespace�� Sync Latch�� ����ä�� �� �Լ��� ȣ��ȴ�.
 */
IDE_RC smmTBSMediaRecovery::flushRedoLSN4AllDBF( smmTBSNode * aSpaceNode )
{
    IDE_ASSERT( aSpaceNode != NULL );


    UInt                sLoop;
    SInt                sWhichDB;
    smmDatabaseFile*    sDatabaseFile;

    // Stable�� UnStable ��� Memory Redo LSN�� �����Ѵ�.
    for ( sWhichDB = 0; sWhichDB < SMM_PINGPONG_COUNT; sWhichDB ++ )
    {
        for ( sLoop = 0; sLoop <= aSpaceNode->mLstCreatedDBFile;
              sLoop ++ )
        {
            IDE_TEST( smmManager::getDBFile( aSpaceNode,
                                             sWhichDB,
                                             sLoop,
                                             SMM_GETDBFILEOP_NONE,
                                             &sDatabaseFile )
                      != IDE_SUCCESS );

            IDE_ASSERT( sDatabaseFile != NULL );

            if ( sDatabaseFile->isOpen() == ID_TRUE )
            {
                // sync�� dbf ��忡 checkpoint ���� ����
                sDatabaseFile->setChkptImageHdr(
                                    sctTableSpaceMgr::getMemRedoLSN(),
                                    NULL,     // aMemCreateLSN
                                    NULL,     // aSpaceID
                                    NULL,     // aSmVersion
                                    NULL );   // aDataFileDescSlotID

                IDE_ASSERT( sDatabaseFile->flushDBFileHdr()
                            == IDE_SUCCESS );
            }
            else
            {
                // ���� �������� ���� ����Ÿ�����̴�.
            }
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}




/*
  ���̺����̽��� ����Ÿ���� ��Ÿ����� üũ����Ʈ������ �����Ѵ�.
  �� �Լ��� ȣ��Ǳ����� TBS Mgr Latch�� ȹ��� �����̴�.

  [IN] aSpaceNode : Sync�� TBS Node
  [IN] aActionArg : NULL
*/
IDE_RC smmTBSMediaRecovery::doActUpdateAllDBFileHdr(
                                       idvSQL             * /* aStatistics*/,
                                       sctTableSpaceNode * aSpaceNode,
                                       void              * aActionArg )
{
    IDE_DASSERT( aSpaceNode != NULL );
    IDE_DASSERT( aActionArg == NULL );
    
    ACP_UNUSED( aActionArg );

    UInt sStage = 0;


    if ( sctTableSpaceMgr::isMemTableSpace( aSpaceNode ) == ID_TRUE )
    {
        IDE_TEST( sctTableSpaceMgr::latchSyncMutex( aSpaceNode )
                  != IDE_SUCCESS );
        sStage = 1;


        if ( sctTableSpaceMgr::hasState( aSpaceNode,
                                         SCT_SS_SKIP_UPDATE_DBFHDR )
             == ID_TRUE )
        {
            // �޸����̺����̽��� DROPPED/DISCARDED �� ���
            // �������� ����.
        }
        else
        {
            // Tablespace�� ���� ��� DB File�� Header��
            // Redo LSN�� ����Ѵ�.
            IDE_TEST( flushRedoLSN4AllDBF( (smmTBSNode*) aSpaceNode )
                      != IDE_SUCCESS );

        }

        sStage = 0;
        IDE_TEST( sctTableSpaceMgr::unlatchSyncMutex( aSpaceNode )
                  != IDE_SUCCESS );
    }
    else
    {
        // �޸����̺����̽��� �ƴѰ�� �������� ����.
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();

    switch ( sStage )
    {
        case 1:
            IDE_ASSERT( sctTableSpaceMgr::unlatchSyncMutex( aSpaceNode )
                        == IDE_SUCCESS );
            break;
        default:
            break;
    }

    IDE_POP();

    return IDE_FAILURE;
}


/*
  �޸� ���̺����̽��� ���� üũ����Ʈ �����߿� ��ũ
  Stable/Unstable üũ����Ʈ �̹������� ��Ÿ����� �����Ѵ�.
  ������ �����Ѵ�.
*/
IDE_RC smmTBSMediaRecovery::updateDBFileHdr4AllTBS()
{
    IDE_TEST( sctTableSpaceMgr::doAction4EachTBS(
                  NULL, /* idvSQL* */
                  smmTBSMediaRecovery::doActUpdateAllDBFileHdr,
                  NULL, /* Action Argument*/
                  SCT_ACT_MODE_NONE ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
  �����ͺ��̽� Ȯ�忡 ���� ���ο� Expand Chunk�� �Ҵ�ʿ� ����
  ���� ���ܳ��� �Ǵ� DB���ϸ��� ��Ÿ�� ����� �������� ���� i
  CreateLSN�� �����Ѵ�.

  Chunk�� Ȯ��Ǵ��� �ѹ��� �ϳ��� DB���ϸ� ������ �� �ִ�.

  [IN] aSpaceNode      - �޸� ���̺����̽� ���
  [IN] aNewDBFileChunk - Ȯ��Ǹ鼭 ���� �þ ����Ÿ���� ����
  [IN] aCreateLSN      - ����Ÿ����(��)�� CreateLSN
 */
IDE_RC smmTBSMediaRecovery::setCreateLSN4NewDBFiles(
                                           smmTBSNode * aSpaceNode,
                                           smLSN      * aCreateLSN )
{
    UInt              sWhichDB;
    UInt              sCurrentDBFileNum;
    smmDatabaseFile * sDatabaseFile;

    IDE_DASSERT( aSpaceNode     != NULL );
    IDE_DASSERT( aCreateLSN  != NULL );

    // ����Ÿ������ ��Ÿ�� ����� CreateLSN ����
    for ( sWhichDB = 0; sWhichDB < SMM_PINGPONG_COUNT; sWhichDB ++ )
    {
        sCurrentDBFileNum = aSpaceNode->mMemBase->mDBFileCount[ sWhichDB ];

        IDE_TEST( smmManager::getDBFile( aSpaceNode,
                                         sWhichDB,
                                         sCurrentDBFileNum,
                                         SMM_GETDBFILEOP_NONE,
                                         &sDatabaseFile )
                  != IDE_SUCCESS );

        // ���� ������ ����Ÿ���� ����� �����Ѵ�.
        // Memory Redo LSN�� �������� �ʴ� ����
        // ���� ���� �����Ҷ� �����ϸ� �Ǳ� �����̴�.
        sDatabaseFile->setChkptImageHdr(
                                NULL,         // Memory Redo LSN �������� ����.
                                aCreateLSN,
                                &aSpaceNode->mHeader.mID,
                                (UInt*)&smVersionID,
                                NULL ); // aDataFileDescSlotID
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


// ���������ÿ� ��� �޸� ���̺����̽��� ��� �޸� DBFile����
// �̵�� ���� �ʿ� ���θ� üũ�Ѵ�.
IDE_RC smmTBSMediaRecovery::identifyDBFilesOfAllTBS( idBool aIsOnCheckPoint )
{
    sctActIdentifyDBArgs sIdentifyDBArgs;

    sIdentifyDBArgs.mIsValidDBF  = ID_TRUE;
    sIdentifyDBArgs.mIsFileExist = ID_TRUE;

    if ( aIsOnCheckPoint == ID_FALSE )
    {
        // ����������
        IDE_TEST( sctTableSpaceMgr::doAction4EachTBS(
                      NULL, /* idvSQL* */
                      smmTBSMediaRecovery::doActIdentifyAllDBFiles,
                      &sIdentifyDBArgs, /* Action Argument*/
                      SCT_ACT_MODE_NONE ) != IDE_SUCCESS );

        IDE_TEST_RAISE( sIdentifyDBArgs.mIsFileExist != ID_TRUE,
                        err_file_not_found );

        IDE_TEST_RAISE( sIdentifyDBArgs.mIsValidDBF != ID_TRUE,
                        err_need_media_recovery );
    }
    else
    {
        // üũ����Ʈ�������� ��Ÿ������� ���Ͽ�
        // ����� ����� �Ǿ����� ����Ÿ������ �ٽ� �о
        // �����Ѵ�.
        IDE_TEST( sctTableSpaceMgr::doAction4EachTBS(
                      NULL, /* idvSQL* */
                      doActIdentifyAllDBFiles,
                      &sIdentifyDBArgs, /* Action Argument*/
                      SCT_ACT_MODE_LATCH ) != IDE_SUCCESS );

        IDE_ASSERT( sIdentifyDBArgs.mIsValidDBF  == ID_TRUE );
        IDE_ASSERT( sIdentifyDBArgs.mIsFileExist == ID_TRUE );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_file_not_found );
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_NotExistFile ) );
    }
    IDE_EXCEPTION( err_need_media_recovery );
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_NeedMediaRecovery ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
  ��� ����Ÿ������ ��Ÿ����� �ǵ��Ͽ� �̵�� �������θ� Ȯ���Ѵ�.

  [ �߿� ]
  Stable ����Ÿ���ϸ� �˻��ϴ� ���� �ƴϰ�, UnStable ����Ÿ������
  ���翩�ο� Stable ����Ÿ���ϰ� ������ ���������� Ȯ���� �ʿ䰡 �ִ�.
  �������� �ʰ� ���������� �ǽ��Ѵٸ� �ٸ� Unstable ����Ÿ���Ϸ� ����
  ����Ÿ���̽� �ϰ����� ������ �ִ�.

  [ �˰��� ***** ]

  loganchor�� ����Ǿ� �ִ� ����Ÿ���Ͽ� ���ؼ� �˻��Ѵ�.
  1. ��� Tablespace�� ���ؼ� loganchor�� ����� Stable Version��
     �ݵ�� �����ؾ��Ѵ�.
  2. ��� Tablespace�� ���ؼ� loganchor�� ����� Unstable Version��
     ������ �� ������, ������  �ݵ�� ��ȿ��  Stable Version����
     ��ȿ�� �����̾�� �Ѵ�.
  3. ���� ���� �������� ���� Checkpoint Image File�� �ش� ������ �����ϸ�
     ��ȿ�� �˻縦 ���� �ʴ´�. (BUG-29607)
     create cp image���� ����� �ٽ� �����Ѵ�.

  [IN]  aTBSNode   - ������ TBS Node
  [OUT] aActionArg - ����Ÿ������ ���翩�� �� ��ȿ���� ����
*/
IDE_RC smmTBSMediaRecovery::doActIdentifyAllDBFiles(
                           idvSQL             * /* aStatistics*/,
                           sctTableSpaceNode  * aTBSNode,
                           void               * aActionArg )
{
    UInt                     sWhichDB;
    UInt                     sFileNum;
    idBool                   sIsMediaFailure;
    smmTBSNode             * sTBSNode;
    smmDatabaseFile        * sDatabaseFile;
    smmChkptImageHdr         sChkptImageHdr;
    smmChkptImageAttr        sChkptImageAttr;
    sctActIdentifyDBArgs   * sIdentifyDBArgs;
    SChar                    sMsgBuf[ SM_MAX_FILE_NAME ];

    IDE_DASSERT( aTBSNode != NULL );
    IDE_DASSERT( aActionArg != NULL );

    // �޸� ���̺����̽��� �ƴ� ��� üũ���� �ʴ´�.
    IDE_TEST_CONT( sctTableSpaceMgr::isMemTableSpace( aTBSNode ) != ID_TRUE ,
                   CONT_SKIP_IDENTIFY );

    // ���̺����̽��� DISCARD�� ��� üũ���� �ʴ´�.
    // ���̺����̽��� ������ ��� üũ���� �ʴ´�.
    IDE_TEST_CONT( sctTableSpaceMgr::hasState( aTBSNode,
                                                SCT_SS_SKIP_IDENTIFY_DB )
                    == ID_TRUE , CONT_SKIP_IDENTIFY );

    sIdentifyDBArgs = (sctActIdentifyDBArgs*)aActionArg;
    sTBSNode        = (smmTBSNode*)aTBSNode;

    // LstCreatedDBFile�� RESTART�� �Ϸ�����Ŀ� ����Ǹ�,
    // �α׾�Ŀ�� �ʱ�ȭ�ÿ��� ����� �ȴ�.
    for ( sFileNum = 0;
          sFileNum <= sTBSNode->mLstCreatedDBFile;
          sFileNum ++ )
    {
        // Stable�� Unstable ����Ÿ���� ��� �˻��Ѵ�.
        for ( sWhichDB = 0; sWhichDB < SMM_PINGPONG_COUNT; sWhichDB ++ )
        {
            // PRJ-1548 User Memory Tablespace
            // Loganchor�� ����� Checkpoint Image�� �ݵ�� �����ؾ��Ѵ�.

            // ���� �������θ� Ȯ���Ѵ�.
            if ( smmManager::getCreateDBFileOnDisk( sTBSNode,
                                                    sWhichDB,
                                                    sFileNum )
                 == ID_FALSE )
            {
                /* BUG-23700: [SM] Stable DB�� DB File���� Last Create File
                 * ������ ��� ���������� �ʽ��ϴ�.
                 *
                 * Table�� Drop�Ǹ� �Ҵ�� Page�� Free�� �ǰ� Checkpoint�ÿ���
                 * Free�� �������� ������ �ʽ��ϴ�. �ؼ� TBS�� mLstCreatedDBFile
                 * ���� �����߿��� Table���� Drop�Ǿ �߰� File���� �ѹ���
                 * �������� �������� �ʾƼ� File������ ���� �ʰ� �� ���ϵ���
                 * �������� ����� ��찡 �����Ͽ� Stable DB���� mLstCreatedDBFile
                 * ������ ��� DBFile�� ���������� �ʽ��ϴ�. */

                // BUG-29607 Checkpoint�� �Ϸ� �ϰ� ��ȿ�� �˻縦 �� ��
                // üũ����Ʈ���� ������� ����, �ڽ��� ���������� ���� CP Image File��
                // ��ȿ���� �˻��ϴ�, ������ �߻��ϰ� FATAL�� Server Start�� ���� �ʴ�
                // ������ �־����ϴ�. �ڽ��� ������ CP Image File�� ��ȿ���� �˻��մϴ�.
                continue;
            }

            // ������ ���� �Ͽ��ٸ� OPEN�� �����ؾ� �Ѵ�.
            if ( smmManager::openAndGetDBFile( sTBSNode,
                                               sWhichDB,
                                               sFileNum,
                                               &sDatabaseFile )
                 != IDE_SUCCESS )
            {
                // fix BUG-17343 ���� ���ؼ�
                // Unstable�� ���� �����Ǿ����� �����ϰ� Ȯ��
                // �����ϴ�.
                sIdentifyDBArgs->mIsFileExist = ID_FALSE;

                idlOS::snprintf(sMsgBuf, SM_MAX_FILE_NAME,
                                "  [SM-WARNING] CANNOT IDENTIFY DATAFILE\n\
                                [TBS:%s, PPID-%"ID_UINT32_FMT"-FID-%"ID_UINT32_FMT"] Datafile Not Found\n",
                                sTBSNode->mHeader.mName,
                                sWhichDB,
                                sFileNum );

                IDE_CALLBACK_SEND_MSG(sMsgBuf);

                continue;
            }

            // version, oldest lsn, create lsn ��ġ���� ������
            // media recovery�� �ʿ��ϴ�.
            IDE_TEST_RAISE( sDatabaseFile->checkValidationDBFHdr(
                                                    &sChkptImageHdr,
                                                    &sIsMediaFailure ) != IDE_SUCCESS,
                            err_invalid_data_file_header );

            if ( sIsMediaFailure == ID_TRUE )
            {
                // �̵�� ������ �ִ� ����Ÿ����
                sDatabaseFile->getChkptImageAttr( sTBSNode,
                                                  &sChkptImageAttr );

                idlOS::snprintf( sMsgBuf, 
                                 SM_MAX_FILE_NAME,
                                 "  [SM-WARNING] CANNOT IDENTIFY DATAFILE\n\
                            [TBS:%s, PPID-%"ID_UINT32_FMT"-FID:%"ID_UINT32_FMT"] Mismatch Datafile Version\n",
                                 sTBSNode->mHeader.mName,
                                 sFileNum,
                                 sChkptImageAttr.mFileNum );

                IDE_CALLBACK_SEND_MSG( sMsgBuf );

                sIdentifyDBArgs->mIsValidDBF = ID_FALSE;
            }
            else
            {
                // �̵�� ������ ���� ����Ÿ����
            }
        }
    }

    IDE_EXCEPTION_CONT( CONT_SKIP_IDENTIFY );

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_invalid_data_file_header );
    {
        /* BUG-33353 */
        IDE_SET( ideSetErrorCode( smERR_ABORT_InvalidDatafileHeader,
                                  sDatabaseFile->getFileName(),
                                  sDatabaseFile->mSpaceID,
                                  sDatabaseFile->mFileNum,
                                  (sDatabaseFile->mChkptImageHdr).mMemRedoLSN.mFileNo,
                                  (sDatabaseFile->mChkptImageHdr).mMemRedoLSN.mOffset,
                                  sChkptImageHdr.mMemRedoLSN.mFileNo,
                                  sChkptImageHdr.mMemRedoLSN.mOffset,
                                  (sDatabaseFile->mChkptImageHdr).mMemCreateLSN.mFileNo,
                                  (sDatabaseFile->mChkptImageHdr).mMemCreateLSN.mOffset,
                                  sChkptImageHdr.mMemCreateLSN.mFileNo,
                                  sChkptImageHdr.mMemCreateLSN.mOffset,
                                  smVersionID & SM_CHECK_VERSION_MASK,
                                  (sDatabaseFile->mChkptImageHdr).mSmVersion & SM_CHECK_VERSION_MASK,
                                  sChkptImageHdr.mSmVersion & SM_CHECK_VERSION_MASK) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
  �޸� ���̺����̽��� �̵������� �ִ� ����Ÿ���� ����� �����.

  [ �߿� ]
  �̵�� ���������� Stable ����Ÿ���ϵ鿡 ���ؼ��� �̵�����
  �����Ѵ�. Unstable ����Ÿ������ Indentify�� ���� ��������������
  �̷������.

  [IN]  aTBSNode              - ���̺����̽� ���
  [IN]  aRecoveryType         - �̵��� Ÿ��
  [OUT] aMediaFailureDBFCount - �̵������� �߻��� ����Ÿ���ϳ�� ����
  [OUT] aFromRedoLSN          - �������� Redo LSN
  [OUT] aToRedoLSN            - �����Ϸ� Redo LSN
*/
IDE_RC smmTBSMediaRecovery::makeMediaRecoveryDBFList( sctTableSpaceNode * aTBSNode,
                                                      smiRecoverType      aRecoveryType,
                                                      UInt              * aFailureChkptImgCount,
                                                      smLSN             * aFromRedoLSN,
                                                      smLSN             * aToRedoLSN )
{
    UInt                sWhichDB;
    UInt                sFileNum;
    smmTBSNode       *  sTBSNode;
    smmChkptImageHdr    sChkptImageHdr;
    smmDatabaseFile   * sDatabaseFile;
    idBool              sIsMediaFailure;
    SChar               sMsgBuf[ SM_MAX_FILE_NAME ];
    smLSN               sFromRedoLSN;
    smLSN               sToRedoLSN;

    IDE_DASSERT( aTBSNode              != NULL );
    IDE_DASSERT( aFailureChkptImgCount != NULL );
    IDE_DASSERT( aFromRedoLSN       != NULL );
    IDE_DASSERT( aToRedoLSN         != NULL );

    idlOS::memset( &sChkptImageHdr, 0x00, ID_SIZEOF(smmChkptImageHdr) );

    sTBSNode = (smmTBSNode*)aTBSNode;

    // StableDB ��ȣ�� ��´�.
    sWhichDB = smmManager::getCurrentDB( sTBSNode );

    // �α׾�Ŀ�� ����� Stable�� ����Ÿ���ϵ��� ��������� �����Ѵ�.
    // LstCreatedDBFile�� RESTART�� �Ϸ�����Ŀ� ����Ǹ�,
    // �α׾�Ŀ�� �ʱ�ȭ�ÿ��� ����� �ȴ�.

    for( sFileNum = 0 ;
         sFileNum <= sTBSNode->mLstCreatedDBFile ;
         sFileNum ++ )
    {
        /* ------------------------------------------------
         * [1] ����Ÿ������ �����ϴ��� �˻�
         * ----------------------------------------------*/
        // Loganchor�� ����� Checkpoint Image�� �ݵ�� �����ؾ��Ѵ�.
        // ������ �����Ѵٸ� OPEN�� �����ϴ�
        if ( smmManager::openAndGetDBFile( sTBSNode,
                                           sWhichDB,
                                           sFileNum,
                                           &sDatabaseFile )
             != IDE_SUCCESS )
        {
            idlOS::snprintf( sMsgBuf, SM_MAX_FILE_NAME,
                             "  [SM-WARNING] CANNOT IDENTIFY DATAFILE\n\
               [%s-<DBF ID:%"ID_UINT32_FMT">] DATAFILE NOT FOUND\n",
                             sTBSNode->mHeader.mName,
                             sFileNum );
            IDE_CALLBACK_SEND_MSG( sMsgBuf );

            IDE_RAISE( err_file_not_found );
        }

        /* ------------------------------------------------
         * [2] ����Ÿ���ϰ� ���ϳ��� ���̳ʸ������� �˻�
         * ----------------------------------------------*/
        IDE_TEST_RAISE( sDatabaseFile->checkValidationDBFHdr(
                                        &sChkptImageHdr,
                                        &sIsMediaFailure ) != IDE_SUCCESS,
                        err_data_file_header_invalid );

        if ( sIsMediaFailure == ID_TRUE )
        {
            /*
               �̵�� ������ �����ϴ� ���

               ��ġ���� �ʴ� ���� ����(COMPLETE) ������
               �����ؾ� �Ѵ�.
            */
            IDE_TEST_RAISE( ( aRecoveryType == SMI_RECOVER_UNTILTIME ) ||
                            ( aRecoveryType == SMI_RECOVER_UNTILCANCEL ),
                            err_incomplete_media_recovery );
        }
        else
        {
            /*
              �̵������� ���� ����Ÿ����

              �ҿ�������(INCOMPLETE) �̵�� �����ÿ���
              ������� ������ ������� �����ϹǷ� REDO LSN��
              �α׾�Ŀ�� ��ġ���� �ʴ� ���� �������� �ʴ´�.

              �׷��ٰ� ���������ÿ� ��� ����Ÿ������
              ������ �־�� �Ѵٴ� ���� �ƴϴ�.
            */
        }

        if ( ( aRecoveryType == SMI_RECOVER_COMPLETE ) &&
             ( sIsMediaFailure != ID_TRUE ) )
        {
            // ���������� ������ ���� ����Ÿ���� ���
            // �̵�� ������ �ʿ����.
            continue;
        }
        else
        {
            // ������ �����ϰ����ϴ� �������� �Ǵ�
            // ������ ���� �ҿ���������
            // ��� �̵�� ������ �����Ѵ�.

            // �ҿ��� ���� :
            // ����Ÿ���� ����� oldest lsn�� �˻��� ����,
            // ����ġ�ϴ� ��� ����������Ϸ� �����Ѵ�.

            // �������� :
            // ��� ����Ÿ������ ��������� �ȴ�.

            // �̵����� ������ ����� ������ ��ȯ�Ѵ�.
            IDE_TEST( sDatabaseFile->prepareMediaRecovery(
                                            aRecoveryType,
                                            &sChkptImageHdr,
                                            &sFromRedoLSN,
                                            &sToRedoLSN )
                      != IDE_SUCCESS );

            // ��� ������� ����Ÿ���ϵ��� ���������� ������ �� �ִ�
            // �ּ� From Redo LSN, �ִ� To Redo LSN�� ���մϴ�.
            if ( smLayerCallback::isLSNGT( aFromRedoLSN,
                                           &sFromRedoLSN )
                 == ID_TRUE )
            {
                SM_GET_LSN( *aFromRedoLSN,
                            sFromRedoLSN );
            }
            else
            {
                /* nothing to do ... */
            }
           
            if ( smLayerCallback::isLSNGT( &sToRedoLSN,
                                           aToRedoLSN )
                 == ID_TRUE)
            {
                SM_GET_LSN( *aToRedoLSN,
                            sToRedoLSN );
            }
            else
            {
                /* nothing to do ... */
            }

            // �޸� ����Ÿ������ ������� ���� ������ ���
            *aFailureChkptImgCount = *aFailureChkptImgCount + 1;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_file_not_found );
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_NotExistFile ) );
    }
    IDE_EXCEPTION( err_data_file_header_invalid );
    {
        /* BUG-33353 */
        IDE_SET( ideSetErrorCode( smERR_ABORT_InvalidDatafileHeader,
                                  sDatabaseFile->getFileName(),
                                  sDatabaseFile->mSpaceID,
                                  sDatabaseFile->mFileNum,
                                  (sDatabaseFile->mChkptImageHdr).mMemRedoLSN.mFileNo,
                                  (sDatabaseFile->mChkptImageHdr).mMemRedoLSN.mOffset,
                                  sChkptImageHdr.mMemRedoLSN.mFileNo,
                                  sChkptImageHdr.mMemRedoLSN.mOffset,
                                  (sDatabaseFile->mChkptImageHdr).mMemCreateLSN.mFileNo,
                                  (sDatabaseFile->mChkptImageHdr).mMemCreateLSN.mOffset,
                                  sChkptImageHdr.mMemCreateLSN.mFileNo,
                                  sChkptImageHdr.mMemCreateLSN.mOffset,
                                  smVersionID & SM_CHECK_VERSION_MASK,
                                  (sDatabaseFile->mChkptImageHdr).mSmVersion & SM_CHECK_VERSION_MASK,
                                  sChkptImageHdr.mSmVersion & SM_CHECK_VERSION_MASK) );
    }
    IDE_EXCEPTION( err_incomplete_media_recovery );
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_NeedMediaRecovery ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

///*
//   ���̺����̽��� N��° ����Ÿ������ PageID ���� ��ȯ
//
//   [IN] aTBSNode - ���̺����̽� ���
//   [IN] aFileNum - ����Ÿ���� ��ȣ
//   [OUT] aFstPageID - ù��° Page ID
//   [OUT] aLstPageID - ������ Page ID
//
//*/
//void smmTBSMediaRecovery::getPageRangeOfNthFile( smmTBSNode * aTBSNode,
//                                                 UInt         aFileNum,
//                                                 scPageID   * aFstPageID,
//                                                 scPageID   * aLstPageID )
//{
//    scPageID  sFstPageID;
//    scPageID  sLstPageID;
//    UInt      sPageCountPerFile;
//    UInt      sFileNum;
//
//    IDE_DASSERT( aTBSNode   != NULL );
//    IDE_DASSERT( aFstPageID != NULL );
//    IDE_DASSERT( aLstPageID != NULL );
//
//    sLstPageID        = 0;
//    sFstPageID        = 0;
//    sPageCountPerFile = 0;
//
//    for ( sFileNum = 0; sFileNum <= aFileNum; sFileNum ++ )
//    {
//        sFstPageID += sPageCountPerFile;
//
//        // �� ���Ͽ� ����� �� �ִ� Page�� ��
//        sPageCountPerFile = smmManager::getPageCountPerFile( aTBSNode,
//                                                             sFileNum );
//
//        sLstPageID = sFstPageID + sPageCountPerFile - 1;
//    }
//
//    *aFstPageID = sFstPageID;
//    *aLstPageID = sLstPageID;
//
//    return;
//}

/*
  �̵������� ���� �̵����� ������ �޸� ����Ÿ���ϵ���
  ã�Ƽ� ��������� �����Ѵ�.

  [IN]  aTBSNode   - �˻��� TBS Node
  [OUT] aActionArg - Repair ����
*/
IDE_RC smmTBSMediaRecovery::doActRepairDBFHdr(
                              idvSQL             * /* aStatistics*/,
                              sctTableSpaceNode  * aSpaceNode,
                              void               * aActionArg )
{
    UInt                     sWhichDB;
    UInt                     sFileNum;
    UInt                     sNxtStableDB;
    idBool                   isOpened = ID_FALSE;
    smmTBSNode             * sTBSNode;
    smmDatabaseFile        * sDatabaseFile;
    smmDatabaseFile        * sNxtStableDatabaseFile;
    sctActRepairArgs       * sRepairArgs;
    SChar                    sNxtStableFileName[ SM_MAX_FILE_NAME ];
    SChar                  * sFileName;
    SChar                    sBuffer[SMR_MESSAGE_BUFFER_SIZE];
    idBool                   sIsCreated;

    IDE_DASSERT( aSpaceNode != NULL );
    IDE_DASSERT( aActionArg != NULL );

    sTBSNode = (smmTBSNode*)aSpaceNode;
    sRepairArgs = (sctActRepairArgs*)aActionArg;

    while ( 1 )
    {
        if ( sctTableSpaceMgr::isMemTableSpace( sTBSNode ) != ID_TRUE )
        {
            // �޸� ���̺����̽��� �ƴ� ��� üũ���� �ʴ´�.
            break;
        }

        if ( sctTableSpaceMgr::hasState( aSpaceNode,
                                         SCT_SS_UNABLE_MEDIA_RECOVERY ) 
             == ID_TRUE )
        {
            // ���̺����̽��� DROPPED �̰ų� DISCARD ������ ���
            // �̵����� ���� �ʰ� �����Ѵ�.
            break;
        }

        // StableDB ��ȣ�� ��´�.
        sWhichDB = smmManager::getCurrentDB( sTBSNode );

        // LstCreatedDBFile�� RESTART�� �Ϸ�����Ŀ� ����Ǹ�,
        // �α׾�Ŀ�� �ʱ�ȭ�ÿ��� ����� �ȴ�.
        for ( sFileNum = 0;
              sFileNum <= sTBSNode->mLstCreatedDBFile;
              sFileNum ++ )
        {
            // PRJ-1548 User Memory Tablespace
            // Loganchor�� ����� Checkpoint Image�� �ݵ�� �����ؾ��Ѵ�.

            // ������ �����Ѵٸ� OPEN�� �����ϴ�
            IDE_TEST( smmManager::getDBFile( sTBSNode,
                                             sWhichDB,
                                             sFileNum,
                                             SMM_GETDBFILEOP_NONE,
                                             &sDatabaseFile )
                      != IDE_SUCCESS );

            if ( sDatabaseFile->getIsMediaFailure() == ID_TRUE )
            {
                if ( sRepairArgs->mResetLogsLSN != NULL )
                {
                    IDE_ASSERT( ( sRepairArgs->mResetLogsLSN->mFileNo
                                  != ID_UINT_MAX ) &&
                                ( sRepairArgs->mResetLogsLSN->mOffset
                                  != ID_UINT_MAX ) );

                    // �ҿ��� �����ÿ��� ���ڷ� ���� ResetLogsLSN
                    // �����Ѵ�.
                    sDatabaseFile->setChkptImageHdr(
                                    sRepairArgs->mResetLogsLSN,
                                    NULL,   // aMemCreateLSN
                                    NULL,   // aSpaceID
                                    NULL,   // aSmVersion
                                    NULL ); // aDataFileDescSlotID
                }
                else
                {
                    // ���������ÿ��� Loganchor�� �ִ� ������
                    // �״�� ������.
                }

                // ������� ����
                IDE_TEST( sDatabaseFile->flushDBFileHdr()
                          != IDE_SUCCESS );

            }
            else
            {
                // �̵������� ���� ����
                // Nothing to do ...
            }

            //PROJ-2133 incremental backup
            //Media ������ ���� üũ����Ʈ �̹����� pingpong üũ����Ʈ
            //�̹����� �����.
            sNxtStableDB = smmManager::getNxtStableDB( sTBSNode );
            sIsCreated   = smmManager::getCreateDBFileOnDisk( sTBSNode,
                                                              sNxtStableDB,
                                                              sFileNum );

            /* CHKPT �̹����� �̹� �����Ǿ��ִ� �����϶��� �����Ѵ�. */
            if ( sIsCreated == ID_TRUE )
            {
                IDE_TEST( smmManager::getDBFile( sTBSNode,
                                                 sNxtStableDB,                     
                                                 sFileNum,              
                                                 SMM_GETDBFILEOP_NONE,          
                                                 &sNxtStableDatabaseFile )               
                          != IDE_SUCCESS );

                if ( idlOS::strncmp( sNxtStableDatabaseFile->getFileName(),  
                                     "\0",                          
                                     SMI_MAX_DATAFILE_NAME_LEN ) == 0 )
                {
                    IDE_TEST( sNxtStableDatabaseFile->setFileName( 
                                                    sDatabaseFile->getFileDir(),
                                                    sTBSNode->mHeader.mName,    
                                                    sNxtStableDB,
                                                    sDatabaseFile->getFileNum() )
                            != IDE_SUCCESS );
                }
                else 
                {
                    /* nothing to do */
                }

                idlOS::strncpy( sNxtStableFileName, 
                                sNxtStableDatabaseFile->getFileName(),
                                SM_MAX_FILE_NAME );
    
                if ( sNxtStableDatabaseFile->isOpen() == ID_TRUE ) 
                {
                    isOpened = ID_TRUE;
                    // copy target ������ open�� ���¿��� copy�ϸ� �ȵ�
                    IDE_TEST( sNxtStableDatabaseFile->close() 
                              != IDE_SUCCESS );
                }
                else 
                {
                    /* nothing to do */
                }

                if ( idf::access( sNxtStableFileName, F_OK )  == 0 )
                {
                    idf::unlink( sNxtStableFileName );
                }
                else 
                {
                    /* nothing to do */
                }

                sFileName = idlOS::strrchr( sNxtStableFileName, 
                                            IDL_FILE_SEPARATOR );
                sFileName = sFileName + 1;

                idlOS::snprintf( sBuffer,
                                 SMR_MESSAGE_BUFFER_SIZE,     
                                 "                Restoring unstable checkpoint image [%s]",
                                 sFileName );

                IDE_TEST( sDatabaseFile->copy( NULL, sNxtStableFileName )
                          != IDE_SUCCESS );

                IDE_CALLBACK_SEND_MSG(sBuffer);  

                if ( isOpened == ID_TRUE )
                {
                    IDE_TEST( sNxtStableDatabaseFile->open() != IDE_SUCCESS );
    
                    isOpened = ID_FALSE;
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
        }// for

        break;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( isOpened == ID_TRUE )
    {
        /* isOpened�� TRUE ��  FDCnt�� 0 �̾�� �Ѵ�.  
           �ƴϴ��� ũ�� ������ ������ ����׿����� Ȯ�� �ϵ��� �Ѵ�. */
        IDE_DASSERT( sNxtStableDatabaseFile->isOpen() != ID_TRUE )
        
        /* target ������ ���縦 ���� �ݾ����� �ٽ� �����ش�.*/
        (void)sNxtStableDatabaseFile->open();
        isOpened = ID_FALSE;

    }
    else
    {
        /* nothing to do */
    }

    return IDE_FAILURE;
}

/*
   ��� ���̺����̽��� ����Ÿ���Ͽ��� �Էµ� ������ ID�� ������
   Failure ����Ÿ������ ���翩�θ� ��ȯ�Ѵ�.

   [IN]  aTBSID        - ���̺����̽� ID
   [IN]  aPageID       - ������ ID
   [OUT] aExistTBS     - TBSID�� �ش��ϴ� TableSpace ���翩��
   [OUT] aIsFailureDBF - ������ ID�� �����ϴ� Failure DBF ���翩��

*/
IDE_RC smmTBSMediaRecovery::findMatchFailureDBF( scSpaceID   aTBSID,
                                                 scPageID    aPageID,
                                                 idBool    * aIsExistTBS,
                                                 idBool    * aIsFailureDBF )
{
    idBool              sIsFDBF;
    idBool              sIsETBS;
    scPageID            sFstPageID;
    scPageID            sLstPageID;
    UInt                sWhichDB;
    UInt                sFileNum;
    smmTBSNode        * sTBSNode;
    smmDatabaseFile   * sDatabaseFile;

    IDE_DASSERT( aIsExistTBS   != NULL );
    IDE_DASSERT( aIsFailureDBF != NULL );

    // ���̺����̽� ��� �˻�
    sTBSNode = (smmTBSNode*)sctTableSpaceMgr::findSpaceNodeWithoutException( aTBSID );

    if ( sTBSNode != NULL )
    {
        // �������� �����ϴ� ����Ÿ���� ��ȣ�� ��ȯ�Ѵ�.
        sFileNum = smmManager::getDbFileNo( sTBSNode, aPageID );

        // StableDB ��ȣ�� ��´�.
        sWhichDB = smmManager::getCurrentDB( sTBSNode );

        // Failure DBF��� ���� Create�� ����Ÿ���� �߿� �����Ѵ�.
        if ( sFileNum <= sTBSNode->mLstCreatedDBFile )
        {
            IDE_TEST( smmManager::getDBFile( sTBSNode,
                                             sWhichDB,
                                             sFileNum,
                                             SMM_GETDBFILEOP_NONE,
                                             &sDatabaseFile )
                      != IDE_SUCCESS );

            if ( sDatabaseFile->getIsMediaFailure() == ID_TRUE )
            {
                // ����Ÿ������ ������������ ���Ѵ�.
                sDatabaseFile->getPageRangeInFile( &sFstPageID,
                                                   &sLstPageID );

                // �������� ���Ͽ� ���ԵǴ��� Ȯ��
                IDE_ASSERT( (sFstPageID <= aPageID) &&
                            (sLstPageID >= aPageID) );

                // �������� ������ Failure ����Ÿ������ ã�� ���
                sIsFDBF = ID_TRUE;
            }
            else
            {
                sIsFDBF = ID_FALSE;
            }
        }
        else
        {
             // �������� ������ Failure ����Ÿ������ �� ã�� ���
             sIsFDBF = ID_FALSE;
        }

        sIsETBS = ID_TRUE;
    }
    else
    {
         // �������� ������ Failure ����Ÿ������ �� ã�� ���
         sIsETBS = ID_FALSE;
         sIsFDBF = ID_FALSE;
    }

    *aIsExistTBS   = sIsETBS;
    *aIsFailureDBF = sIsFDBF;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
  �̵����� �Ҵ�Ǿ��� ��ü���� �ı��ϰ�
  �޸� �����Ѵ�.

*/
IDE_RC smmTBSMediaRecovery::resetTBSNode( smmTBSNode * aTBSNode )
{
    IDE_DASSERT( aTBSNode != NULL );

    if ( sctTableSpaceMgr::hasState( & aTBSNode->mHeader,
                                     SCT_SS_NEED_PAGE_PHASE )
         == ID_TRUE )
    {
        // Load�� ��� Page Memory�ݳ�, Page System����
        IDE_TEST( smmTBSMultiPhase::finiPagePhase( aTBSNode )
                  != IDE_SUCCESS );

        // Page System�ʱ�ȭ ( Prepare/Restore�ȵ� ���� )
        IDE_TEST( smmTBSMultiPhase::initPagePhase( aTBSNode )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
   �̵����� �Ҵ��ߴ� ���̺����̽��� �ڿ���
   �����Ѵ�.
*/
IDE_RC smmTBSMediaRecovery::doActResetMediaFailureTBSNode(
                                        idvSQL            * /* aStatistics*/,
                                        sctTableSpaceNode * aTBSNode,
                                        void              * aActionArg )
{
    IDE_DASSERT( aTBSNode   != NULL );
    IDE_DASSERT( aActionArg == NULL );

    ACP_UNUSED( aActionArg );

    if ( sctTableSpaceMgr::isMemTableSpace( aTBSNode ) == ID_TRUE )
    {
        // �̵����� ����� TBS�� ���� Restore�� �Ǿ� �־�� �Ѵ�.
        if ( ( ( smmTBSNode*)aTBSNode)->mRestoreType
               != SMM_DB_RESTORE_TYPE_NOT_RESTORED_YET )
        {
            // �̵����� ����� TBS�� ResetTBS�� �����Ѵ�.
            // �׷��� ���� Assert�� �����ؾ߸� �Ѵ�.
            IDE_ASSERT( sctTableSpaceMgr::hasState( aTBSNode->mID,
                                                    SCT_SS_UNABLE_MEDIA_RECOVERY )
                        == ID_FALSE );

            IDE_TEST( resetTBSNode( (smmTBSNode *) aTBSNode )
                      != IDE_SUCCESS );
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
   �̵����� �Ҵ��ߴ� ���̺����̽����� �ڿ���
   �����Ѵ�.
*/
IDE_RC smmTBSMediaRecovery::resetMediaFailureMemTBSNodes()
{
    IDE_TEST( sctTableSpaceMgr::doAction4EachTBS( NULL, /* idvSQL* */
                                                  doActResetMediaFailureTBSNode,
                                                  NULL, /* Action Argument*/
                                                  SCT_ACT_MODE_NONE )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
