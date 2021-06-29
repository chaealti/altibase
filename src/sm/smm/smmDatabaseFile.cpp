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
 * $Id: smmDatabaseFile.cpp 85837 2019-07-14 23:44:48Z emlee $
 **********************************************************************/

#include <idl.h>
#include <ideErrorMgr.h>
#include <idErrorCode.h>
#ifndef VC_WIN32
#include <iduFileAIO.h>
#endif
#include <idu.h>
#include <smErrorCode.h>
#include <smm.h>
#include <sct.h>
#include <smmReq.h>
#include <smmDatabaseFile.h>
#include <smmTBSChkptPath.h>
#include <smriChangeTrackingMgr.h>
#include <smriBackupInfoMgr.h>

smmDatabaseFile::smmDatabaseFile()
{
}

/*
  ����Ÿ���̽�����(CHECKPOINT IMAGE) ��ü�� �ʱ�ȭ�Ѵ�.

  [IN] aSpaceID    - ����Ÿ���̽������� TBS ID
  [IN] aPingpongNo - PINGPONG ��ȣ
  [IN] aDBFileNo   - ����Ÿ���̽����� ��ȣ
  [IN] aFstPageID   - ����Ÿ���̽������� ù��° PID
  [IN] aLstPageID   - ����Ÿ���̽������� ������ PID
 */
IDE_RC smmDatabaseFile::initialize( scSpaceID   aSpaceID,
                                    UInt        aPingPongNum,
                                    UInt        aFileNum,
                                    scPageID  * aFstPageID,
                                    scPageID  * aLstPageID )
{
    smLSN   sMaxLSN;
    SChar   sMutexName[128];

    // BUG-27456 Klocwork SM (4)
    mDir = NULL;

    IDE_TEST( mFile.initialize( IDU_MEM_SM_SMM,
                                1, /* Max Open FD Count */
                                IDU_FIO_STAT_OFF,
                                IDV_WAIT_INDEX_NULL )
              != IDE_SUCCESS );


    // To Fix BUG-18434
    //        alter tablespace online/offline ���ü����� �������
    //
    // Page Buffer�� ���ٿ� ���� ���ü� ��� ���� Mutex �ʱ�ȭ
    idlOS::snprintf( sMutexName,
                     128,
                     "MEM_DBFILE_PAGEBUFFER_%"ID_UINT32_FMT"_%"ID_UINT32_FMT"_%"ID_UINT32_FMT"_MUTEX",
                     (UInt) aSpaceID,
                     (UInt) aPingPongNum,
                     (UInt) aFileNum );

    IDE_TEST(mPageBufferMutex.initialize( sMutexName,
                                          IDU_MUTEX_KIND_POSIX,
                                          IDV_WAIT_INDEX_NULL )
             != IDE_SUCCESS );

    mPageBufferMemPtr = NULL;
    mAlignedPageBuffer = NULL;

    // BUG-27456 Klocwork SM (4)
    /* smmDatabaseFile_initialize_malloc_Dir.tc */
    IDU_FIT_POINT("smmDatabaseFile::initialize::malloc::Dir");
    IDE_TEST( iduMemMgr::malloc( IDU_MEM_SM_SMM,
                                 SM_MAX_FILE_NAME,
                                 (void **)&mDir,
                                 IDU_MEM_FORCE )
              != IDE_SUCCESS );

    IDE_TEST( mFile.allocBuff4DirectIO( IDU_MEM_SM_SMM,
                                        SM_PAGE_SIZE,
                                        (void**)&mPageBufferMemPtr,
                                        (void**)&mAlignedPageBuffer )
              != IDE_SUCCESS );

    // To Fix BUG-18130
    //    Meta Page�� �Ϻθ� File Header����ü�� memcpy��
    //    Disk�� �������� Meta Page��ü�� ������ ���� �ذ�
    idlOS::memset( mAlignedPageBuffer,
                   0,
                   SM_DBFILE_METAHDR_PAGE_SIZE );

    idlOS::memset( &mChkptImageHdr,
                   0,
                   ID_SIZEOF(mChkptImageHdr) );

    // PROJ-2133 incremental backup
    mChkptImageHdr.mDataFileDescSlotID.mBlockID = SMRI_CT_INVALID_BLOCK_ID;
    mChkptImageHdr.mDataFileDescSlotID.mSlotIdx = 
                                    SMRI_CT_DATAFILE_DESC_INVALID_SLOT_IDX;

    mSpaceID     = aSpaceID;
    mPingPongNum = aPingPongNum;
    mFileNum     = aFileNum;

    SM_LSN_MAX( sMaxLSN );


    // RedoLSN�� �α׾�Ŀ�κ��� �ʱ�ȭ�Ǳ��������� �Ǵ�
    // üũ����Ʈ�� �߻��ϱ� �� �������� ID_UINT_MAX ���� ������.
    SM_GET_LSN( mChkptImageHdr.mMemRedoLSN,
                sMaxLSN )

    // CreateLSN�� �α׾�Ŀ�κ��� �ʱ�ȭ�Ǳ��������� �Ǵ�
    // ����Ÿ������ �����Ǳ� �������� ID_UINT_MAX ���� ������.
    SM_GET_LSN( mChkptImageHdr.mMemCreateLSN,
                sMaxLSN )

    ////////////////////////////////////////////////////////////
    // PRJ-1548 User Memory TableSpace ���䵵��

    // �޸����̺����̽��� �̵�� �������꿡�� ���

    if ( aFstPageID != NULL )
    {
        mFstPageID      = *aFstPageID;  // ����Ÿ������ ������ PID
    }

    if ( aLstPageID != NULL )
    {
        mLstPageID      = *aLstPageID;  // ����Ÿ������ ù��° PID
    }
    mIsMediaFailure = ID_FALSE;    // �̵������࿩��

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    // BUG-27456 Klocwork SM (4)
    if ( mDir != NULL )
    {
        IDE_ASSERT( iduMemMgr::free( mDir ) == IDE_SUCCESS );

        mDir = NULL;
    }

    return IDE_FAILURE;

}

IDE_RC smmDatabaseFile::destroy()
{

    IDE_TEST( iduMemMgr::free( mPageBufferMemPtr ) != IDE_SUCCESS );

    // BUG-27456 Klocwork SM (4)
    IDE_TEST( iduMemMgr::free( mDir ) != IDE_SUCCESS );

    mDir = NULL;

    IDE_TEST( mPageBufferMutex.destroy() != IDE_SUCCESS );

    IDE_TEST( mFile.destroy() != IDE_SUCCESS );

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;

    // BUG-27456 Klocwork SM (4)
    if ( mDir != NULL )
    {
        IDE_ASSERT( iduMemMgr::free( mDir ) == IDE_SUCCESS );

        mDir = NULL;
    }

    return IDE_FAILURE;
}

smmDatabaseFile::~smmDatabaseFile()
{
}

IDE_RC
smmDatabaseFile::syncUntilSuccess()
{

    return mFile.syncUntilSuccess( smLayerCallback::setEmergency );

}

const SChar*
smmDatabaseFile::getDir()
{

    return (SChar*)mDir;

}


void
smmDatabaseFile::setDir( SChar *aDir )
{
    // BUG-27456 Klocwork SM (4)
    IDE_ASSERT( aDir != NULL );

    idlOS::strncpy( mDir, aDir, SM_MAX_FILE_NAME - 1);

    mDir[ SM_MAX_FILE_NAME - 1 ] = '\0';
}

const SChar*
smmDatabaseFile::getFileName()
{

    return mFile.getFileName();

}


IDE_RC
smmDatabaseFile::setFileName(SChar *aFilename)
{

    return mFile.setFileName(aFilename);

}

IDE_RC
smmDatabaseFile::setFileName( SChar *aDir,
                              SChar *aTBSName,
                              UInt   aPingPongNum,
                              UInt   aFileNum )
{

    SChar sDBFileName[SM_MAX_FILE_NAME];

    IDE_DASSERT( aDir != NULL );
    IDE_DASSERT( aTBSName != NULL );

    idlOS::snprintf( sDBFileName,
                     ID_SIZEOF(sDBFileName),
                     SMM_CHKPT_IMAGE_NAME_WITH_PATH,
                     aDir,
                     aTBSName,
                     aPingPongNum,
                     aFileNum );

    return mFile.setFileName(sDBFileName);

}


/***********************************************************************
 * Description : <aCurrentDB, aDBFileNo>�� �ش��ϴ� Database������ �����ϰ�
 *               ũ��� aSize�� �Ѵ�.
 *
 * aTBSNode   : Db File�� ������� �ϴ�  Tablespace�� Node
 * aCurrentDB : MMDB�� Ping-Pong�̱� ������ �ΰ��� Database Image�� ����. ����
 *              ��� Image�� DB�� �������� ������������ �ѱ�
 *              0 or 1�̾�� ��.
 * aDBFileNo  : Datbase File No
 * aSize      : ������ ������ ũ��.(Byte), ���� aSize == 0���, DB Header��
 *              ����Ѵ�.
 * aChkptImageHdr : �޸� üũ����Ʈ�̹���(����Ÿ����)�� ��Ÿ���
 **********************************************************************/
IDE_RC smmDatabaseFile::createDbFile(smmTBSNode       * aTBSNode,
                                     SInt               aCurrentDB,
                                     SInt               aDBFileNo,
                                     UInt               aSize,
                                     smmChkptImageHdr * aChkptImageHdr )
{
    SChar     sDummyPage[ SM_DBFILE_METAHDR_PAGE_SIZE ];
    SChar     sDBFileName[SM_MAX_FILE_NAME];
    SChar     sCreateDir[SM_MAX_FILE_NAME];
    idBool    sIsDirectIO;

    IDE_ASSERT( aTBSNode != NULL );
    IDE_ASSERT( mFile.getCurFDCnt() == 0 );

    /* aSize�� DB File Header���� Ŀ�� �Ѵ�. */
    IDE_ASSERT( ( aSize + 1 > SD_PAGE_SIZE ) || ( aSize == 0 ) );

    if( smuProperty::getIOType() == 0 )
    {
        sIsDirectIO = ID_FALSE;
    }
    else
    {
        sIsDirectIO = ID_TRUE;
    }

    IDE_TEST( makeDBDirForCreate( aTBSNode,
                                  aDBFileNo,
                                  sCreateDir )
             != IDE_SUCCESS );

    setDir( sCreateDir ) ;

    /* DBFile �̸��� �����. */
    idlOS::snprintf( sDBFileName,
                     ID_SIZEOF(sDBFileName),
                     SMM_CHKPT_IMAGE_NAME_WITH_PATH,
                     sCreateDir,
                     aTBSNode->mTBSAttr.mName,
                     aCurrentDB,
                     aDBFileNo );

    IDE_TEST( setFileName( sDBFileName ) != IDE_SUCCESS );

    if ( mFile.exist() == ID_TRUE )
    {
        // BUG-29607 create checkpoint image ���� ���� ������ ������
        //           ���� ���� ���� ����� �ٽ� �����Ѵ�.
        // checkpoint image ������ checkpoint�� �ڵ����� �߻��ϹǷ�
        // ������ �ִٰ� ������ ��ȯ�ϸ�, ����ڰ� trc.log�� Ȯ������ ���� �� ��ð�
        // checkpoint�� ���� �� �����Ƿ�, ������ ��ȯ���� �ʰ� ���� �� ������Ѵ�.
        // ���, create tablespace�� ������ ���� �� ���� �����ؼ�
        // File�� ���� ������ �̸� �˻��Ͽ� ������� �Ǽ��� �̸� �����Ѵ�.
        //
        // BUGBUG ���� �˶� ����� �߰��Ǹ� ���ܷ� ��ȯ�Ѵ�.

        IDE_TEST( idf::unlink( sDBFileName ) != IDE_SUCCESS );
    }

    // PRJ-1149 media recovery�� ���Ͽ� memory ����Ÿ���ϵ�
    // ���� ����� �߰���.
    // ����Ÿ���̽����� ��Ÿ��� ����
    // MemCreateLSN�� AllocNewPageChunk ó���� ������
    // �̹� �Ǿ� �־�� �Ѵ�.
    setChkptImageHdr( sctTableSpaceMgr::getMemRedoLSN(),
                      NULL,     // aMemCreateLSN
                      NULL,     // spaceID
                      NULL,     // smVersion
                      NULL );   // aDataFileDescSlotID

    /* DBFile Header���� ���� DBFile�� �����Ѵ�. */
    IDE_TEST( createDBFileOnDiskAndOpen( sIsDirectIO )
              != IDE_SUCCESS );

    /* FileHeader�� Checkpont Image Header�� ����Ѵ� */
    IDE_TEST( setDBFileHeader( aChkptImageHdr ) != IDE_SUCCESS );

    IDU_FIT_POINT("BUG-46574@smmDatabaseFile::createDbFile::writeUntilSuccess");

    /* File�� ũ�⸦ ���Ѵ�. */
    if( aSize > (SM_DBFILE_METAHDR_PAGE_SIZE + 1) )
    {
        idlOS::memset( sDummyPage, 0, SM_DBFILE_METAHDR_PAGE_SIZE );

        // Set File Size
        IDE_TEST( writeUntilSuccessDIO( aSize - SM_DBFILE_METAHDR_PAGE_SIZE,
                                        (void*) sDummyPage,
                                        SM_DBFILE_METAHDR_PAGE_SIZE,
                                        smLayerCallback::setEmergency )
                  != IDE_SUCCESS );
    }

    IDE_TEST( mFile.syncUntilSuccess( smLayerCallback::setEmergency )
              != IDE_SUCCESS );

    //==============================================================
    // To Fix BUG-13924
    //==============================================================
    ideLog::log( SM_TRC_LOG_LEVEL_MEMORY,
                 SM_TRC_MEMORY_FILE_CREATE,
                 getFileName() );

    // �α׾�Ŀ�� ����Ǿ� ���������� �����Ѵ�.
    // �α׾�Ŀ�� ����Ǿ� �ִٸ� CreateDBFileOnDisk Flag�� �����Ͽ�
    // �����Ѵ�.

    IDE_TEST( addAttrToLogAnchorIfCrtFlagIsFalse( aTBSNode )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
 * Memory DBFile�� Disk�� �����ϰ� Open�Ѵ�.
 *
 * aUsedDirectIO - [IN] Direct IO�� ����ϸ� ID_TRUE, else ID_FALSE
 *
 */
IDE_RC smmDatabaseFile::createDBFileOnDiskAndOpen( idBool aUseDirectIO )
{
    IDE_TEST( mFile.createUntilSuccess( smLayerCallback::setEmergency,
                                        smLayerCallback::isLogFinished() )
              != IDE_SUCCESS );

    IDE_TEST( mFile.open( aUseDirectIO ) != IDE_SUCCESS );

    // ���⿡�� DB File Header Page�� �̸� �����θ� �ȵȴ�.
    //
    // Create Tablespace�� Undo�ÿ� ������ DB File�� Header�� ��ϵ�
    // Tablespace ID�� ����, Tablespace ID�� ��ġ�� ���������� �����Ѵ�.
    //
    // �׷��Ƿ�, DB File�� Header�� TablespaceID�� ����� ������ �Ŀ�
    // DB������ Header Page�� ����Ͽ��� �Ѵ�.

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
 * Memory DB File Header�� ��Ÿ����� ����Ѵ�.
 *
 * aChkptImageHdr - [IN] �޸� üũ����Ʈ�̹���(����Ÿ����)�� ��Ÿ���
 *
 */
IDE_RC smmDatabaseFile::setDBFileHeader( smmChkptImageHdr * aChkptImageHdr )
{
    smmChkptImageHdr * sChkptImageHdr2Write;

    if ( aChkptImageHdr != NULL )
    {
        IDE_DASSERT( checkValuesOfDBFHdr( aChkptImageHdr )
                     == IDE_SUCCESS );

        sChkptImageHdr2Write = aChkptImageHdr;
    }
    else
    {
        // CreateDB�ÿ��� Redo LSN�� Hdr�� �����Ǿ� ���� �ʴ�
        // ��찡 �ִ�.

        sChkptImageHdr2Write = & mChkptImageHdr;
    }

    // write dbf file header.
    IDE_TEST( writeUntilSuccessDIO( SM_DBFILE_METAHDR_PAGE_OFFSET,
                                    sChkptImageHdr2Write,
                                    ID_SIZEOF(smmChkptImageHdr),
                                    smLayerCallback::setEmergency )
              != IDE_SUCCESS );

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

/*
 * ������ ����� Memory DB File Header�� ��Ÿ����� ����Ѵ�.
 * dbs������ �������� �ʴ�(���� DB������ �������ʴ�) Memory DB������ �����
 * �����Ҷ� ���Ǵ� �Լ��̴�. incremental backup�� ��������� backup������
 * ��������� ���ȴ�.
 *
 * aChkptImagePath  - [IN] �޸� üũ����Ʈ�̹���(����Ÿ����)�� ���
 * aChkptImageHdr   - [IN] �޸� üũ����Ʈ�̹���(����Ÿ����)�� ��Ÿ���
 *
 */
IDE_RC smmDatabaseFile::setDBFileHeaderByPath( 
                                    SChar            * aChkptImagePath,
                                    smmChkptImageHdr * aChkptImageHdr )
{
    iduFile sFile;
    UInt    sState = 0;

    IDE_DASSERT( aChkptImagePath != NULL );
    IDE_DASSERT( aChkptImageHdr  != NULL );

    IDE_TEST( sFile.initialize( IDU_MEM_SM_SMM, 
                                1, /* Max Open FD Count */     
                                IDU_FIO_STAT_OFF,              
                                IDV_WAIT_INDEX_NULL ) != IDE_SUCCESS );
    sState = 1;

    IDE_TEST( sFile.setFileName( aChkptImagePath ) != IDE_SUCCESS );

    IDE_TEST_RAISE( sFile.open() != IDE_SUCCESS , err_file_not_found);
    sState = 2;

    // write dbf file header.
    IDE_TEST_RAISE( sFile.write( NULL, /*idvSQL*/               
                                 SM_DBFILE_METAHDR_PAGE_OFFSET, 
                                 aChkptImageHdr,
                                 SM_DBFILE_METAHDR_PAGE_SIZE )
                    != IDE_SUCCESS, err_chkptimagehdr_write_failure );

    sState = 1;
    IDE_TEST( sFile.close()   != IDE_SUCCESS );

    sState = 0;
    IDE_TEST( sFile.destroy() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_file_not_found );                      
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_NotFoundDataFileByPath,
                                  aChkptImagePath ) );
    }
    IDE_EXCEPTION( err_chkptimagehdr_write_failure );
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_Datafile_Header_Write_Failure, 
                                  aChkptImagePath ) );
    }

    IDE_EXCEPTION_END;

    switch( sState )
    {
        case 2:
            IDE_ASSERT( sFile.close()   == IDE_SUCCESS );
        case 1:
            IDE_ASSERT( sFile.destroy() == IDE_SUCCESS );
        case 0:
        default:
            break;
    }

    return IDE_FAILURE;
}

/*
  üũ����Ʈ �̹��� ���Ͽ� Checkpoint Image Header�� ��ϵ� ���
  �̸� �о Tablespace ID�� ��ġ�ϴ��� ���ϰ� File�� Close�Ѵ�.

  [IN] aSpaceID - Checkpoint Image Header�� Tablespace ID�� ���� ID
  [OUT] aIsHeaderWritten - Checkpoint Image Header�� ��ϵǾ� �ִ��� ����
  [OUT] aSpaceIdMatches - Checkpoint Image Header�� Tablespace ID��
                          aSpaceID�� ��ġ�ϴ��� ����
*/
IDE_RC smmDatabaseFile::readSpaceIdAndClose( scSpaceID   aSpaceID,
                                             idBool    * aIsHeaderWritten,
                                             idBool    * aSpaceIdMatches)
{
    IDE_DASSERT( aIsHeaderWritten != NULL );
    IDE_DASSERT( aSpaceIdMatches != NULL );

    smmChkptImageHdr sChkptImageHdr;
    ULong sChkptFileSize = 0;

    *aIsHeaderWritten  = ID_FALSE;
    *aSpaceIdMatches   = ID_FALSE;

    // Checkpoint Image Header�� �о���� ���� File�� Open
    if ( isOpen() == ID_FALSE )
    {
        if ( open() != IDE_SUCCESS )
        {
            errno = 0;
            IDE_CLEAR();
        }
    }

    if ( isOpen() == ID_TRUE )
    {
        IDE_TEST ( mFile.getFileSize( & sChkptFileSize )
                   != IDE_SUCCESS );

        // To Fix BUG-18272   TC/Server/sm4/PRJ-1548/dynmem/../suites/
        //                    restart/rt_ctdt_aa.sql ���൵�� �׽��ϴ�.
        //
        // Checkpoint Image File�� �����Ǵ� �� ���
        // Checkpoint Image Header�� ������� �Ǿ� ���� ���� ���� �ִ�
        //
        // File�� ũ�Ⱑ Checkpoint Image Header���� Ŭ����
        // Checkpoint Image Header�� �д´�.
        if ( sChkptFileSize >= ID_SIZEOF(sChkptImageHdr) )
        {
            IDE_ASSERT( isOpen() == ID_TRUE );

            IDE_TEST ( readChkptImageHdr(&sChkptImageHdr) != IDE_SUCCESS );
            ideLog::log(SM_TRC_LOG_LEVEL_MRECOV,"[TBSID:%d] dbf id : %d\n",
                        aSpaceID,
                        sChkptImageHdr.mSpaceID );

            if ( sChkptImageHdr.mSpaceID == aSpaceID )
            {
                *aSpaceIdMatches = ID_TRUE;
            }
            else
            {
                ideLog::log(SM_TRC_LOG_LEVEL_MRECOV,"[TBSID:%d] DIFFID!!! \n", aSpaceID );

                *aSpaceIdMatches = ID_FALSE;
            }
            *aIsHeaderWritten = ID_TRUE;
        }

        // Checkpoint Image File�� Close
        IDE_TEST( mFile.close() != IDE_SUCCESS );
    }

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

/*
    ������ ������ ��� �����Ѵ�.
    �������� ���� ��� �ƹ��ϵ� ���� �ʴ´�.

    [IN] aSpaceID  - ������ Tablespace�� ID
    [IN] aFileName - ������ ���ϸ�
 */
IDE_RC smmDatabaseFile::removeFileIfExist( scSpaceID aSpaceID,
                                           const SChar * aFileName )
{
    if ( idf::access( aFileName, F_OK)
         == 0 ) // �������� ?
    {
        // Checkpoint Image File�� Disk���� ����
        if ( idf::unlink( aFileName ) != 0 )
        {

            ideLog::log(SM_TRC_LOG_LEVEL_MEMORY,
                        SM_TRC_MEMORY_CHECKPOINT_IMAGE_NOT_REMOVED,
                        aFileName );
            errno = 0;
            ideLog::log(SM_TRC_LOG_LEVEL_MRECOV,"[TBSID:%d] file remove failed \n", aSpaceID );
        }
        else
        {
            ideLog::log(SM_TRC_LOG_LEVEL_MRECOV,"[TBSID:%d] REMOVED \n", aSpaceID );

        }

    }
    else
    {
        ideLog::log(SM_TRC_LOG_LEVEL_MRECOV,"[TBSID:%d] NO FILE!!! \n", aSpaceID );
        // ������ ������ ���� ��� ����.
        errno = 0;
    }

    return IDE_SUCCESS;
}


/*
    Checkpoint Image File�� Close�ϰ� Disk���� �����ش�.

    ���� ������ �������� ���� ��� �����ϰ� �Ѿ��.

    [IN] aSpaceID - �����Ϸ��� Checkpoint Image�� ���� Tablespace�� ID
    [IN] aRemoveImageFiles  - Checkpoint Image File�� ������ �� �� ����
*/
IDE_RC smmDatabaseFile::closeAndRemoveDbFile(scSpaceID      aSpaceID,
                                             idBool         aRemoveImageFiles,
                                             smmTBSNode   * aTBSNode )
{
    idBool                      sIsHeaderWritten ;
    idBool                      sSpaceIdMatches ;
    idBool                      sSaved;
    UInt                        sFileNum;
    smiDataFileDescSlotID       sDataFileDescSlotID;
    smmChkptImageAttr           sChkptImageAttr;

    if ( smLayerCallback::isRestartRecoveryPhase() == ID_TRUE )
    {
        ideLog::log(SM_TRC_LOG_LEVEL_MRECOV,"[In Recovery]"  );
    }
    else
    {
        ideLog::log(SM_TRC_LOG_LEVEL_MRECOV,"[In NormalProcessing]"  );
    }

    // Checkpoint Image Header�� �о Space ID�� üũ�ϰ� File�� Close
    // (  Close�� �Ǿ�� �������� )
    IDE_TEST( readSpaceIdAndClose( aSpaceID,
                                   & sIsHeaderWritten,
                                   & sSpaceIdMatches ) != IDE_SUCCESS );

    if ( aRemoveImageFiles == ID_TRUE )
    {
        // TO Fix BUG-17143  create/drop tablespace ���� kill, restart��
        //                   The data file does not exist.�����߻�
        // Checkpoint Image�� ��ϵ� Tablespace ID��
        // DROP�Ϸ��� Tablespace�� ID�� ��ġ�ϴ� ��쿡�� ������ �����Ѵ�.

        // Checkpoint Image Header���� Tablespace ID�� ��ġ�ϰų�
        if ( ( sSpaceIdMatches == ID_TRUE ) ||
             // Checkpoint Image Header���� ��ϵ��� ���� ���
             ( sIsHeaderWritten == ID_FALSE ) )
        {
            IDE_TEST( removeFileIfExist( aSpaceID,
                                         getFileName() )
                      != IDE_SUCCESS );

        }
    }

    //PROJ-2133 incremental backup
    if ( smLayerCallback::isCTMgrEnabled() == ID_TRUE )    
    {
        IDE_ERROR( sctTableSpaceMgr::isMemTableSpace( aSpaceID ) == ID_TRUE );

        // �̹� �α׾�Ŀ�� ����Ǿ����� Ȯ���Ѵ�.
        sSaved = smmManager::isCreateDBFileAtLeastOne(
                (aTBSNode->mCrtDBFileInfo[ mFileNum ]).mCreateDBFileOnDisk ) ;

        // �α׾�Ŀ�� DBFile�� ����Ǿ����� �����Ƿ� ������ DataFileDescSlot��
        // ����.
        IDE_TEST_CONT( sSaved != ID_TRUE, 
                        already_dealloc_datafiledesc_slot );

        sFileNum = getFileNum();

        IDE_TEST( smLayerCallback::getDataFileDescSlotIDFromLogAncho4ChkptImage(
                        aTBSNode->mCrtDBFileInfo[sFileNum].mAnchorOffset,
                        &sDataFileDescSlotID )
                  != IDE_SUCCESS );

        // DataFileDescSlot�� �̹� �Ҵ����� �Ȱ�� 
        IDE_TEST_CONT( smriChangeTrackingMgr::isAllocDataFileDescSlotID(
                        &sDataFileDescSlotID ) != ID_TRUE,
                        already_dealloc_datafiledesc_slot );

        // DataFileDescSlot�� �̹� �Ҵ����� �Ǿ����� loganchor����
        // DataFileDescSlotID�� �����ִ°�� Ȯ��
        IDE_TEST_CONT( smriChangeTrackingMgr::isValidDataFileDescSlot4Mem( 
                        this, &sDataFileDescSlotID ) != ID_TRUE,
                        already_reuse_datafiledesc_slot);

        IDE_TEST( smriChangeTrackingMgr::deleteDataFileFromCTFile(
                    &sDataFileDescSlotID ) != IDE_SUCCESS );
        
        IDE_EXCEPTION_CONT( already_reuse_datafiledesc_slot );

        getChkptImageAttr( aTBSNode, &sChkptImageAttr );

        sChkptImageAttr.mDataFileDescSlotID.mBlockID = 
                                    SMRI_CT_INVALID_BLOCK_ID;
        sChkptImageAttr.mDataFileDescSlotID.mSlotIdx = 
                                    SMRI_CT_DATAFILE_DESC_INVALID_SLOT_IDX;

        IDE_TEST( smLayerCallback::updateChkptImageAttrAndFlush(
                  &(aTBSNode->mCrtDBFileInfo[sFileNum]),
                  &sChkptImageAttr )
                  != IDE_SUCCESS );

        IDE_EXCEPTION_CONT( already_dealloc_datafiledesc_slot );
    }
    else
    {
        /* nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**
   Direct I/O�� ���� Disk Sector Size�� Align�� Buffer�� ���� File Read����

   [IN] aWhere  - read�� File�� Offset
   [IN] aBuffer - read�� �����Ͱ� ����� Buffer
   [IN] aSize   - read�� ũ��
 */
IDE_RC
smmDatabaseFile::readDIO( PDL_OFF_T  aWhere,
                          void*      aBuffer,
                          size_t     aSize )
{
    IDE_ASSERT( aBuffer != NULL );
    IDE_ASSERT( aSize <= SM_PAGE_SIZE );
    IDE_ASSERT( mAlignedPageBuffer != NULL );

    UInt sSizeToRead;
    UInt sStage = 0;
    idBool  sCanDirectIO;
    UChar  *sReadBuffer;

    sCanDirectIO = mFile.canDirectIO( aWhere,
                                      aBuffer,
                                      aSize );

    if( sCanDirectIO == ID_TRUE )
    {
        sSizeToRead = aSize;
        sReadBuffer = (UChar*)aBuffer;
    }
    else
    {
        sReadBuffer = mAlignedPageBuffer;
        sSizeToRead = idlOS::align( aSize,
                                    iduProperty::getDirectIOPageSize() );
    }

    IDE_TEST( mPageBufferMutex.lock(NULL /* idvSQL* */) != IDE_SUCCESS );
    sStage = 1;

    IDE_ASSERT( sSizeToRead <= SM_PAGE_SIZE );

    IDE_TEST( mFile.read( NULL, /* idvSQL* */
                          aWhere, 
                          sReadBuffer, 
                          sSizeToRead )
              != IDE_SUCCESS );

    if( sCanDirectIO == ID_FALSE )
    {
        idlOS::memcpy( aBuffer, sReadBuffer, aSize );
    }

    sStage = 0;
    IDE_TEST( mPageBufferMutex.unlock() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();

    switch( sStage )
    {
        case 1:
            IDE_ASSERT( mPageBufferMutex.unlock() == IDE_SUCCESS );
            break;
        default :
            break;
    }

    IDE_POP();

    return IDE_FAILURE;
}

/**
    Direct I/O�� �̿��Ͽ� write�ϱ� ���� Align�� Buffer�� ������ ����

    [IN] aAlignedDst - �����Ͱ� ����� Align�� �޸� ����
    [IN] aSrc        - ���� ������ �ּ�
    [IN] aSrcSize    - ���� ������ ũ��
    [OUT] aSizeToWrite - aAlignedDst�� Direct I/O�� write�ÿ� ����� ũ��
 */
IDE_RC smmDatabaseFile::copyDataForDirectWrite( void * aAlignedDst,
                                                void * aSrc,
                                                UInt   aSrcSize,
                                                UInt * aSizeToWrite )
{
    // Direct IO Pageũ�Ⱑ DB File Header Pageũ�⺸�� Ŭ ���,
    // DB File Header�� Direct I/O�� ����ϱ� ����
    // Direct IO Pageũ�⸸ŭ Align �Ͽ� ����� ���
    // 0�� Page������ ħ���� �� �ִ�.
    //
    // �׷��Ƿ�, Direct IO Pageũ��� �׻� DB File�� Header Page����
    // �۰ų� ���ƾ� �Ѵ�.
    IDE_ASSERT( iduProperty::getDirectIOPageSize() <=
                SM_DBFILE_METAHDR_PAGE_SIZE );

    UInt sSizeToWrite = idlOS::align( aSrcSize,
                                      iduProperty::getDirectIOPageSize() );

    // Page ���� ū ũ�⸦ ����� �� ����.
    IDE_ASSERT( sSizeToWrite <= SM_PAGE_SIZE );

    if ( sSizeToWrite > aSrcSize )
    {
        // Align�� ũ�Ⱑ �� Ŭ ���, Buffer�� �켱 0���� �ʱ�ȭ
        // �ʱ�ȭ���� ���� ������ Write���� �ʵ��� �̿��� ����.
        idlOS::memsetOnMemCheck( aAlignedDst, 0, sSizeToWrite );
    }

    // Data ũ�⸸ŭ Align�� �޸� ������ ����
    idlOS::memcpy( aAlignedDst, aSrc, aSrcSize );

    *aSizeToWrite = sSizeToWrite;

    return IDE_SUCCESS;
}

///**
//   Direct I/O�� ���� Disk Sector Size�� Align�� Buffer�� ���� File Write����
//
//   [IN] aWhere  - write�� File�� Offset
//   [IN] aBuffer - write�� �����Ͱ� ����� Buffer
//   [IN] aSize   - write�� ũ��
// */
//IDE_RC
//smmDatabaseFile::writeDIO( PDL_OFF_T aWhere,
//                           void*     aBuffer,
//                           size_t    aSize )
//{
//    IDE_ASSERT( aBuffer != NULL );
//    IDE_ASSERT( aSize <= SM_PAGE_SIZE );
//    IDE_ASSERT( mAlignedPageBuffer != NULL );
//
//    UInt   sSizeToWrite;
//    UInt   sStage = 0;
//    idBool sCanDirectIO;
//    UChar *sWriteBuffer;
//
//    sCanDirectIO = mFile.canDirectIO( aWhere,
//                                      aBuffer,
//                                      aSize );
//
//    IDE_TEST( mPageBufferMutex.lock(NULL /* idvSQL* */) != IDE_SUCCESS );
//    sStage = 1;
//
//    if( sCanDirectIO == ID_FALSE)
//    {
//        sWriteBuffer = mAlignedPageBuffer;
//        IDE_TEST( copyDataForDirectWrite( mAlignedPageBuffer,
//                                          aBuffer,
//                                          aSize,
//                                          & sSizeToWrite )
//                  != IDE_SUCCESS );
//    }
//    else
//    {
//        sWriteBuffer = (UChar*)aBuffer;
//        sSizeToWrite = aSize;
//    }
//
//    IDE_TEST( mFile.write( NULL, aWhere, sWriteBuffer, sSizeToWrite )
//              != IDE_SUCCESS );
//
//    sStage = 0;
//    IDE_TEST( mPageBufferMutex.unlock() != IDE_SUCCESS );
//
//    return IDE_SUCCESS;
//
//    IDE_EXCEPTION_END;
//
//    IDE_PUSH();
//
//    switch( sStage )
//    {
//        case 1:
//            IDE_ASSERT( mPageBufferMutex.unlock() == IDE_SUCCESS );
//            break;
//        default :
//            break;
//    }
//
//    IDE_POP();
//
//    return IDE_FAILURE;
//}

/**
   Direct I/O�� ���� Disk Sector Size�� Align�� Buffer�� ���� File Write����
   ( writeUntilSuccess���� )

   [IN] aWhere  - write�� File�� Offset
   [IN] aBuffer - write�� �����Ͱ� ����� Buffer
   [IN] aSize   - write�� ũ��
 */
IDE_RC
smmDatabaseFile::writeUntilSuccessDIO( PDL_OFF_T            aWhere,
                                       void*                aBuffer,
                                       size_t               aSize,
                                       iduEmergencyFuncType aSetEmergencyFunc,
                                       idBool               aKillServer )
{
    IDE_ASSERT( aBuffer != NULL );
    IDE_ASSERT( aSize <= SM_PAGE_SIZE );
    IDE_ASSERT( mAlignedPageBuffer != NULL );

    UInt    sSizeToWrite;
    UInt    sStage = 0;
    idBool  sCanDirectIO;
    UChar  *sWriteBuffer;

    sCanDirectIO = mFile.canDirectIO( aWhere,
                                      aBuffer,
                                      aSize );

    IDE_TEST( mPageBufferMutex.lock(NULL /* idvSQL* */) != IDE_SUCCESS );
    sStage = 1;

    if( sCanDirectIO == ID_FALSE)
    {
        sWriteBuffer = mAlignedPageBuffer;
        IDE_TEST( copyDataForDirectWrite( mAlignedPageBuffer,
                                          aBuffer,
                                          aSize,
                                          & sSizeToWrite )
                  != IDE_SUCCESS );
    }
    else
    {
        sWriteBuffer = (UChar*)aBuffer;
        sSizeToWrite = aSize;
    }

    IDE_TEST( mFile.writeUntilSuccess( NULL, /* idvSQL* */
                                       aWhere,
                                       sWriteBuffer,
                                       sSizeToWrite,
                                       aSetEmergencyFunc,
                                       aKillServer )
              != IDE_SUCCESS );

    sStage = 0;
    IDE_TEST( mPageBufferMutex.unlock() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();

    switch( sStage )
    {
        case 1:
            IDE_ASSERT( mPageBufferMutex.unlock() == IDE_SUCCESS );
            break;
        default :
            break;
    }

    IDE_POP();

    return IDE_FAILURE;
}




IDE_RC
smmDatabaseFile::readPage( smmTBSNode * aTBSNode,
                           scPageID     aPageID,
                           UChar      * aPage )
{
    PDL_OFF_T         sOffset;

    //PRJ-1149, backup & media recovery.
    sOffset = ( SM_PAGE_SIZE * smmManager::getPageNoInFile( aTBSNode, aPageID ) ) 
              + SM_DBFILE_METAHDR_PAGE_SIZE;

    /* Direct IO�� IO�� ����� �� �ֱ� ������ mAlignedPageBuffer�� �̿��ؾ� �Ѵ�.*/
    IDE_TEST( mFile.read( NULL, /* idvSQL* */
                          sOffset,
                          aPage,
                          SM_PAGE_SIZE ) 
              != IDE_SUCCESS );

    if ( aPageID != smLayerCallback::getPersPageID( (void*)aPage ) )
    {
        ideLog::log(SM_TRC_LOG_LEVEL_MEMORY,
                    SM_TRC_MEMORY_INVALID_PID_IN_PAGEHEAD1 );

        ideLog::log(SM_TRC_LOG_LEVEL_MEMORY,
                    SM_TRC_MEMORY_INVALID_PID_IN_PAGEHEAD2,
                    (ULong) aPageID);

        ideLog::log( SM_TRC_LOG_LEVEL_MEMORY,
                     SM_TRC_MEMORY_INVALID_PID_IN_PAGEHEAD3,
                     (ULong)smLayerCallback::getPersPageID( (void*)aPage ) );

        IDE_ASSERT(0);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


// For dumplf only !!!
IDE_RC
smmDatabaseFile::readPageWithoutCheck( smmTBSNode * aTBSNode,
                                       scPageID     aPageID,
                                       UChar      * aPage)
{
    PDL_OFF_T         sOffset;

    //PRJ-1149, backup & media recovery.
    sOffset = ( SM_PAGE_SIZE * smmManager::getPageNoInFile( aTBSNode, aPageID ) ) 
              + SM_DBFILE_METAHDR_PAGE_SIZE;

    /* Direct IO�� IO�� ����� �� �ֱ� ������ mAlignedPageBuffer�� �̿��ؾ� �Ѵ�.*/
    IDE_TEST( mFile.read( NULL, /* idvSQL* */
                          sOffset,
                          aPage,
                          SM_PAGE_SIZE ) 
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


//IDE_RC
//smmDatabaseFile::readPage(smmTBSNode * aTBSNode, scPageID aPageID)
//{
//
//    void*            sPage;
//    PDL_OFF_T        sOffset;
//
//    IDE_ASSERT( smmManager::getPersPagePtr( aTBSNode->mTBSAttr.mID, 
//                                            aPageID,
//                                            &sPage )
//                == IDE_SUCCESS );
//
//    //PRJ-1149, backup & media recovery.
//    sOffset = ( SM_PAGE_SIZE * smmManager::getPageNoInFile(
//                    aTBSNode, aPageID ) ) + SM_DBFILE_METAHDR_PAGE_SIZE;
//
//    /* Direct IO�� IO�� ����� �� �ֱ� ������ mAlignedPageBuffer�� �̿��ؾ� �Ѵ�.*/
//    IDE_TEST( mFile.read( NULL, /* idvSQL* */
//                             sOffset,
//                             sPage,
//                             SM_PAGE_SIZE )
//              != IDE_SUCCESS );
//
//    IDE_ASSERT( aPageID == smLayerCallback::getPersPageID( sPage ) );
//
//    return IDE_SUCCESS;
//
//    IDE_EXCEPTION_END;
//
//    return IDE_FAILURE;
//}

// PRJ-1149, memory DBF�� ����Ÿ���� ����� �߰��Ǿ.
// smmManager::loadSerial���� iduFile::read(PDL_OFF_T, void*, size_t,size_t*)
// �� �ٷ� ȣ������ �ʱ� ���Ͽ� �߰��� �Լ�.
IDE_RC smmDatabaseFile::readPages(PDL_OFF_T   aWhere,
                                  void*       aBuffer,
                                  size_t      aSize,
                                  size_t *    aReadSize)

{

    IDE_TEST ( mFile.read( NULL, /* idvSQL* */
                           aWhere + SM_DBFILE_METAHDR_PAGE_SIZE,
                           aBuffer,
                           aSize,
                           aReadSize )
               != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

#ifndef VC_WIN32
/* **************************************************************************
 * Description : AIO�� �̿��� DBFile�� Page���� �о���δ�.
 *
 *   aWhere          [IN] - DBFile�κ��� �о���� ��ġ
 *   aBuffer         [IN] - Page���� ������ Buffer
 *   aSize           [IN] - �о���� ������
 *   aReadSize      [OUT] - ���� �о���� ��
 *   aAIOCount       [IN] - AIO ����
 *
 *   �о���� �κ��� AIOCount��ŭ �����Ͽ� �񵿱� Read�� �����Ѵ�.
 *   �� AIO�� ���� ��ġ�� ������ �� �� Read�� �±��.
 *   AIO�� Read �Ϸ�� ������ ����ϰ� �ȴ�.
 *
 * **************************************************************************/
IDE_RC smmDatabaseFile::readPagesAIO( PDL_OFF_T   aWhere,
                                      void*       aBuffer,
                                      size_t      aSize,
                                      size_t     *aReadSize,
                                      UInt        aAIOCount )

{

    UInt        i;
    ULong       sReadUnitSize;
    ULong       sOffset;
    ULong       sStartPos;
    iduFileAIO *sAIOUnit = NULL;
    ULong       sReadSum   = 0;
    SInt        sErrorCode = 0;
    UInt        sFreeCount = 0;
    UInt        sReadCount = 0;
    PDL_HANDLE  sFD;
    SInt        sState = 0;

    IDE_DASSERT( aSize > SMM_MIN_AIO_FILE_SIZE );
    IDE_DASSERT( (aSize % SM_PAGE_SIZE) == 0); // ������ �������� ���!

    sOffset   = 0;
    sStartPos = aWhere + SM_DBFILE_METAHDR_PAGE_SIZE;
    aAIOCount = IDL_MIN( aAIOCount, SMM_MAX_AIO_COUNT_PER_FILE );

    IDE_TEST( iduMemMgr::malloc( IDU_MEM_SM_SMM,
                                 (ULong)ID_SIZEOF(iduFileAIO) * aAIOCount,
                                 (void **)&sAIOUnit,
                                 IDU_MEM_FORCE)
              != IDE_SUCCESS );

    /* ------------------------------------------------
     * [1] Setup AIO Object
     * ----------------------------------------------*/
    IDE_TEST( mFile.allocFD( NULL /* idvSQL */, &sFD ) != IDE_SUCCESS );
    sState = 1;

    for (i = 0; i < aAIOCount; i++)
    {
        IDE_TEST( sAIOUnit[i].initialize( sFD ) != IDE_SUCCESS );
    }

    // �� AIO�� �о���� Size�� ����Ѵ�.
    if( aAIOCount > 1 )
    {
        sReadUnitSize = idlOS::align(
                (aSize + aAIOCount) / aAIOCount, SM_PAGE_SIZE);

        aAIOCount = aSize / sReadUnitSize;

        if( (aSize % sReadUnitSize) == 0 )
        {
            // nothing
        }
        else
        {
            aAIOCount++;
        }
    }
    else
    {
        sReadUnitSize = aSize;
    }
    /* ------------------------------------------------
     * Request Asynchronous Read Operation
     * ----------------------------------------------*/

    while(1)
    {
        ULong sCalcSize;

        if( (aSize - sOffset) <= sReadUnitSize )
        {
            // calc remains which is smaller than sReadUnitSize
            sCalcSize = aSize - sOffset;

            // anyway..should be pagesize align.
            IDE_ASSERT( (sCalcSize % SM_PAGE_SIZE) == 0 );
        }
        else
        {
            sCalcSize = sReadUnitSize;
        }

        // �� AIO���� �۾� �й�
        if( sReadCount < aAIOCount )
        {
            if( sAIOUnit[sReadCount].read( sStartPos + sOffset,
                                           (SChar *)aBuffer + sOffset,
                                           sCalcSize) != IDE_SUCCESS )
            {
                IDE_ASSERT( sAIOUnit[sReadCount].getErrno() == EAGAIN );
                PDL_Time_Value sPDL_Time_Value;
                sPDL_Time_Value.initialize(0, IDU_AIO_FILE_AGAIN_SLEEP_TIME);
                idlOS::sleep( sPDL_Time_Value );
            }
            else
            {
                // on success
                sOffset  += sCalcSize;
                sReadSum += sCalcSize;
                sReadCount++;
            }
        }

        // Read �Ϸ�Ǳ���� ���.
        if( sFreeCount < sReadCount )
        {
            if( sAIOUnit[sFreeCount].isFinish(&sErrorCode) == ID_TRUE )
            {
                if( sErrorCode != 0 )
                {
                    ideLog::log( SM_TRC_LOG_LEVEL_FATAL,
                                 SM_TRC_MEMORY_AIO_FATAL,
                                 sErrorCode );
                    IDE_ASSERT( sErrorCode == 0 );
                }

                IDE_ASSERT( sAIOUnit[sFreeCount].join() == IDE_SUCCESS );
                sFreeCount++;
            }
        }
        else
        {
            if( sFreeCount == aAIOCount )
            {
                break;
            }

        }
    }
    IDE_ASSERT( sReadSum == aSize );

    IDE_ASSERT( iduMemMgr::free(sAIOUnit) == IDE_SUCCESS );
    sAIOUnit = NULL;

    *aReadSize = sReadSum;

    sState = 0;
    IDE_TEST( mFile.freeFD( sFD ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    {
        IDE_PUSH();

        if( sAIOUnit != NULL )
        {
            IDE_ASSERT( iduMemMgr::free(sAIOUnit) == IDE_SUCCESS );
        }

        if( sState != 0 )
        {
            IDE_ASSERT( mFile.freeFD( sFD ) == IDE_SUCCESS );
        }

        IDE_POP();
    }

    return IDE_FAILURE;
}
#endif /* VC_WIN32 */


IDE_RC
smmDatabaseFile::writePage(smmTBSNode * aTBSNode, scPageID aPageID)
{

    void*            sPage;
    PDL_OFF_T        sOffset;
    UChar            sTornPageBuffer[ SM_PAGE_SIZE ];
    UInt             sMemDPFlushWaitOffset;
    UInt             sIBChunkID;
    smmChkptImageHdr sChkptImageHdr;

    //PRJ-1149, backup & media recovery.
    sOffset = ( SM_PAGE_SIZE * smmManager::getPageNoInFile( aTBSNode, aPageID ) ) 
              + SM_DBFILE_METAHDR_PAGE_SIZE;

    //PROJ-2133 incremental backup
    // change tracking�� �����Ѵ�. 
    if ( smLayerCallback::isCTMgrEnabled() == ID_TRUE )
    {
        getChkptImageHdr( &sChkptImageHdr );

        if( smriChangeTrackingMgr::isAllocDataFileDescSlotID( 
                    &sChkptImageHdr.mDataFileDescSlotID ) == ID_TRUE )
        {
            sIBChunkID = smriChangeTrackingMgr::calcIBChunkID4MemPage(
                                                smmManager::getPageNoInFile( aTBSNode, aPageID));

            IDE_TEST( smriChangeTrackingMgr::changeTracking( NULL, 
                                                             this, 
                                                             sIBChunkID )
                      != IDE_SUCCESS );
        }
        else
        {
            // DatabaseFile�� �����ɶ� pageID 0���� ����ϰԵȴ�.
            // �̶� DataFileDescSlot�� �Ҵ�Ǳ� ���� pageID 0����
            // ��ϵ� ��찡 �ִµ� �̰�쿡�� change tracking�� ���� �ʴ´�.
            // DatabaseFile ���� �Ĺ� �۾����� DataFileDescSlot�� �Ҵ�ȴ�.
            IDE_ASSERT( aPageID == 0 );
        }
        
    }

    IDE_ASSERT( smmManager::getPersPagePtr( aTBSNode->mTBSAttr.mID, 
                                            aPageID,
                                            &sPage )
                == IDE_SUCCESS );

    IDE_ASSERT( aPageID == smLayerCallback::getPersPageID( sPage ) );


    /* BUG-32386       [sm_recovery] If the ager remove the MMDB slot and the
     * checkpoint thread flush the page containing the slot at same time, the
     * server can misunderstand that the freed slot is the allocated slot. */
    if( smuProperty::getMemDPFlushWaitTime() > 0 )
    {
        if( ( smuProperty::getMemDPFlushWaitSID() == aTBSNode->mTBSAttr.mID ) &&
            ( smuProperty::getMemDPFlushWaitPID() == aPageID ) )
        {
            sMemDPFlushWaitOffset = smuProperty::getMemDPFlushWaitOffset();
            idlOS::memcpy( sTornPageBuffer,
                           sPage,
                           sMemDPFlushWaitOffset );
            idlOS::sleep( smuProperty::getMemDPFlushWaitTime() );
            idlOS::memcpy( sTornPageBuffer + sMemDPFlushWaitOffset,
                           ((UChar*)sPage) + sMemDPFlushWaitOffset,
                           SM_PAGE_SIZE - sMemDPFlushWaitOffset );
            sPage = (void*)sTornPageBuffer;
        }
    }

    /* Direct IO�� IO�� ����� �� �ֱ� ������ mAlignedPageBuffer�� �̿��ؾ� �Ѵ�.*/
    IDE_TEST( writeUntilSuccessDIO( sOffset,
                                    sPage,
                                    SM_PAGE_SIZE,
                                    smLayerCallback::setEmergency )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
smmDatabaseFile::writePage( smmTBSNode * aTBSNode,
                            scPageID     aPageID,
                            UChar *      aPage)
{

    PDL_OFF_T         sOffset;

    //PRJ-1149, backup & media recovery.
    sOffset = (SM_PAGE_SIZE * smmManager::getPageNoInFile(
                   aTBSNode, aPageID ) ) + SM_DBFILE_METAHDR_PAGE_SIZE;

    /* Direct IO�� IO�� ����� �� �ֱ� ������ mAlignedPageBuffer�� �̿��ؾ� �Ѵ�.*/
    IDE_TEST( writeUntilSuccessDIO( sOffset,
                                    aPage,
                                    SM_PAGE_SIZE,
                                    smLayerCallback::setEmergency )
              != IDE_SUCCESS );

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

IDE_RC smmDatabaseFile::open()
{
    return mFile.open(((smuProperty::getIOType() == 0) ?
                          ID_FALSE : ID_TRUE));
}

/*
    ���ο� Checkpoint Image������ ������ Checkpoint Path�� �����Ѵ�.

    [IN]  aTBSNode  - Checkpoint Path�� ã�� Tablespace Node
    [IN]  aDBFileNo - DB File(Checkpoint Image) ��ȣ
    [OUT] aDBDir    - File�� ������ DB Dir (Checkpoint Path)
 */
IDE_RC smmDatabaseFile::makeDBDirForCreate(smmTBSNode * aTBSNode,
                                           UInt         aDBFileNo,
                                           SChar      * aDBDir )
{
    UInt                sCPathCount ;
    smmChkptPathNode  * sCPathNode;

    IDE_DASSERT ( aTBSNode != NULL );
    IDE_DASSERT ( aDBDir != NULL );


    // BUGBUG-1548 Refactoring
    // Checkpoint Path���� �Լ����� ������ class�� ����
    // smmDatabaseFile���� ���� Layer�� �д�.

    // ��߿��� Alter Tablespace Checkpoint Path�� �Ұ����ϹǷ�
    // ���ü� ��� �Ű� �� �ʿ䰡 ����.

    IDE_TEST( smmTBSChkptPath::getChkptPathNodeCount( aTBSNode,
                                                      & sCPathCount )
              != IDE_SUCCESS );

    // fix BUG-26925 [CodeSonar::Division By Zero]
    IDE_ASSERT( sCPathCount != 0 );

    IDE_TEST( smmTBSChkptPath::getChkptPathNodeByIndex( aTBSNode,
                                                        aDBFileNo % sCPathCount,
                                                        & sCPathNode )
              != IDE_SUCCESS );

    IDE_ASSERT( sCPathNode != NULL );

#if defined(VC_WIN32)
    sctTableSpaceMgr::adjustFileSeparator( sCPathNode->mChkptPathAttr.mChkptPath );
#endif

    idlOS::snprintf(aDBDir, SM_MAX_FILE_NAME, "%s",
                    sCPathNode->mChkptPathAttr.mChkptPath );

    return IDE_SUCCESS ;

    IDE_EXCEPTION_END;

    return IDE_FAILURE ;
}

/* ------------------------------------------------
 *
 * ��� dir���� file�� ã��,
 * ���� �ִٸ� file�� �̸��� true�� �����Ѵ�.
 * false��� ���ϵ� file�� �̸��� �ǹ̾���.
 * ----------------------------------------------*/
idBool smmDatabaseFile::findDBFile(smmTBSNode * aTBSNode,
                                   UInt         aPingPongNum,
                                   UInt         aFileNum,
                                   SChar  *     aFileName,
                                   SChar **     aFileDir)
{
    idBool              sFound = ID_FALSE;
    smuList           * sListNode ;
    smmChkptPathNode  * sCPathNode;

    IDE_DASSERT( aFileName != NULL );

    for (sListNode  = SMU_LIST_GET_FIRST( & aTBSNode->mChkptPathBase );
         sListNode != & aTBSNode->mChkptPathBase ;
         sListNode  = SMU_LIST_GET_NEXT( sListNode ))
    {
        IDE_ASSERT( sListNode != NULL );

        sCPathNode = (smmChkptPathNode*) sListNode->mData;

        IDE_ASSERT( sCPathNode != NULL );

#if defined(VC_WIN32)
        sctTableSpaceMgr::adjustFileSeparator( sCPathNode->mChkptPathAttr.mChkptPath );
#endif

        // BUGBUG-1548 Tablespace Name�� NULL���ڷ� ������
        // ���ڿ��� �ƴѵ��� NULL���ڷ� ������ ���ڿ�ó�� ����ϰ� ����
        idlOS::snprintf(aFileName,
                        SM_MAX_FILE_NAME,
                        SMM_CHKPT_IMAGE_NAME_WITH_PATH,
                        sCPathNode->mChkptPathAttr.mChkptPath,
                        aTBSNode->mTBSAttr.mName,
                        aPingPongNum,
                        aFileNum);

        if( idf::access(aFileName, F_OK) == 0 )
        {
            sFound = ID_TRUE;
            *aFileDir = (SChar*)sCPathNode->mChkptPathAttr.mChkptPath;
            break;
        }
    }

    return sFound;
}

/*
 * Ư�� DB������ Disk�� �ִ��� Ȯ���Ѵ�.
 *
 * aPingPongNum [IN] - Ping/Ping DB��ȣ ( 0 or 1 )
 * aDBFileNo    [IN] - DB File�� ��ȣ ( 0���� �����ϴ� ���� )
 */
idBool smmDatabaseFile::isDBFileOnDisk( smmTBSNode * aTBSNode,
                                        UInt         aPingPongNum,
                                        UInt         aDBFileNo )
{

    IDE_DASSERT( ( aPingPongNum == 0 ) || ( aPingPongNum == 1 ) );

    SChar   sDBFileName[SM_MAX_FILE_NAME];
    SChar  *sDBFileDir;
    idBool  sFound;

    sFound = findDBFile( aTBSNode,
                         aPingPongNum,
                         aDBFileNo,
                         (SChar*)sDBFileName,
                         &sDBFileDir);
    return sFound ;
}




/*
   ����Ÿ ���� �ش� ����.
   PRJ-1149,  �־��� ����Ÿ ���� ����� ����� �̿��Ͽ�
   ����Ÿ ���� ������ �Ѵ�.
*/
IDE_RC smmDatabaseFile::flushDBFileHdr()
{
    PDL_OFF_T         sOffset;

    IDE_DASSERT( assertValuesOfDBFHdr( &mChkptImageHdr )
                 == IDE_SUCCESS );

    /* BUG-17825 Data File Header����� Process Failure��
     *           Startup�ȵ� �� ����
     *
     * ���� ���׿� �����Ͽ� Memory Checkpoint Image Header��
     * ���� Atomic Write�� ������ ����� ���� �������Ͽ���
     * Checkpoint �� ������ ���̱� ������ ���� Stable�� ���� �ϰ� �ǹǷ�
     * ������ �߻����� �ʴ´�. */

    sOffset = SM_DBFILE_METAHDR_PAGE_OFFSET;

    /* PROJ-2162 RestartRiskReduction
     * Consistency ���� ������, ������ MRDB Flush�� ���´�. */
    if( ( smrRecoveryMgr::getConsistency() == ID_TRUE ) || 
        ( smuProperty::getCrashTolerance() == 2 ) )
    {
        IDE_TEST( writeUntilSuccessDIO( sOffset,
                                        &mChkptImageHdr,
                                        ID_SIZEOF(smmChkptImageHdr),
                                        smLayerCallback::setEmergency )
                  != IDE_SUCCESS );

        IDE_TEST( syncUntilSuccess() != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
   ����Ÿ���̽� ���� �ش� ����.
   aChkptImageHdr �� NULL�� �ƴϸ� ���ڿ� �о���̰�,
   NULL�̸� mChkptImageHdr�� �о���δ�.

   [OUT] aChkptImageHdr : ����Ÿ���ϸ�Ÿ��� ������
*/
IDE_RC smmDatabaseFile::readChkptImageHdr(
                            smmChkptImageHdr * aChkptImageHdr )
{
    smmChkptImageHdr * sChkptImageHdrBuffer ;

    if ( aChkptImageHdr != NULL )
    {
        sChkptImageHdrBuffer = aChkptImageHdr;
    }
    else
    {
        sChkptImageHdrBuffer = &mChkptImageHdr;

    }

    //PRJ-1149, backup & media recovery.
    IDE_TEST( readDIO( SM_DBFILE_METAHDR_PAGE_OFFSET,
                       sChkptImageHdrBuffer,
                       ID_SIZEOF( smmChkptImageHdr ) )
                   != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
   ����Ÿ���� HEADER�� üũ����Ʈ ������ ����Ѵ�.

   [IN] aMemRedoLSN         : �̵����� �ʿ��� Memory Redo LSN
   [IN] aMemCreateLSN       : �̵����� �ʿ��� Memory Create LSN
   [IN] aSpaceID            : Checkpoint Image�� ���� Tablespace�� ID
   [IN] aSmVersion          : �̵����� �ʿ��� ���̳ʸ� Version
   [IN] aDataFileDescSlotID : change tracking�� �Ҵ�� DataFileDescSlotID 
                              PROJ-2133
*/
void smmDatabaseFile::setChkptImageHdr( 
                            smLSN                    * aMemRedoLSN,
                            smLSN                    * aMemCreateLSN,
                            scSpaceID                * aSpaceID,
                            UInt                     * aSmVersion,
                            smiDataFileDescSlotID    * aDataFileDescSlotID )
{
    if ( aMemRedoLSN != NULL )
    {
        SM_GET_LSN( mChkptImageHdr.mMemRedoLSN, *aMemRedoLSN );
    }
    else
    {
        /* nothing to do ... */
    }

    if ( aMemCreateLSN != NULL )
    {
        SM_GET_LSN( mChkptImageHdr.mMemCreateLSN, *aMemCreateLSN );
    }
    else
    {
        /* nothing to do ... */
    }

    if ( aSpaceID != NULL )
    {
        mChkptImageHdr.mSpaceID = *aSpaceID;
    }

    if ( aSmVersion != NULL )
    {
        mChkptImageHdr.mSmVersion = *aSmVersion;
    }

    if ( aDataFileDescSlotID != NULL )
    {
        mChkptImageHdr.mDataFileDescSlotID.mBlockID = 
                                        aDataFileDescSlotID->mBlockID;
        mChkptImageHdr.mDataFileDescSlotID.mSlotIdx = 
                                        aDataFileDescSlotID->mSlotIdx;
    }

    return;
}

/*
  ����Ÿ���� ������� ��ȯ

  ���������� ���ʷ� Loganchor�κ��� �ʱ�ȭ�ȴ�.

  [OUT] aChkptImageHdr - ����Ÿ������ ��Ÿ����� ��ȯ�Ѵ�.
*/
void smmDatabaseFile::getChkptImageHdr( smmChkptImageHdr * aChkptImageHdr )
{
    IDE_DASSERT( aChkptImageHdr != NULL );

    idlOS::memcpy( aChkptImageHdr,
                   &mChkptImageHdr,
                   ID_SIZEOF( smmChkptImageHdr ) );

    return;
}

/*
  ����Ÿ���� �Ӽ� ��ȯ

  �Ʒ� �Լ������� smmManager::mCreateDBFileOnDisk�� �����ϱ� ������
  ���� smmManager::mCreateDBFileOnDisk ������ �ʿ��ϴٸ�
  smmManager::setCreateDBFileOnDisk ȣ���Ŀ�
  getChkptImageAttr�� ȣ���Ͽ� ������ ���Ѿ� �Ѵ�.

  [OUT] aChkptImageAttr - ����Ÿ������ �Ӽ��� ��ȯ�Ѵ�.

*/
void smmDatabaseFile::getChkptImageAttr(
                              smmTBSNode        * aTBSNode,
                              smmChkptImageAttr * aChkptImageAttr )
{
    UInt i;
    
    IDE_DASSERT( aTBSNode        != NULL );
    IDE_DASSERT( aChkptImageAttr != NULL );

    aChkptImageAttr->mAttrType    = SMI_CHKPTIMG_ATTR;

    aChkptImageAttr->mSpaceID     = mSpaceID;
    aChkptImageAttr->mFileNum     = mFileNum;

    //PROJ-2133 incremental backup
    if ( ( smLayerCallback::isCTMgrEnabled() == ID_TRUE ) ||
         ( smLayerCallback::isCreatingCTFile() == ID_TRUE ) )
    {

        aChkptImageAttr->mDataFileDescSlotID.mBlockID = 
                                    mChkptImageHdr.mDataFileDescSlotID.mBlockID;
        aChkptImageAttr->mDataFileDescSlotID.mSlotIdx = 
                                    mChkptImageHdr.mDataFileDescSlotID.mSlotIdx;
    }
    else
    {
        aChkptImageAttr->mDataFileDescSlotID.mBlockID = 
                                    SMRI_CT_INVALID_BLOCK_ID;
        aChkptImageAttr->mDataFileDescSlotID.mSlotIdx = 
                                    SMRI_CT_DATAFILE_DESC_INVALID_SLOT_IDX;
    }

    SM_GET_LSN( aChkptImageAttr->mMemCreateLSN, mChkptImageHdr.mMemCreateLSN )

    // fix BUG-17343
    // Loganchor�� �̹� ����Ǿ� �ִٸ� ID_TRUE�� �����Ѵ�.
    for ( i = 0 ; i < SMM_PINGPONG_COUNT; i++ )
    {
        aChkptImageAttr->mCreateDBFileOnDisk[ i ]
              = smmManager::getCreateDBFileOnDisk( aTBSNode,
                                                   i,
                                                   mFileNum );
    }

    return;
}


/*
  ����Ÿ������ ��Ÿ����� �ǵ��Ͽ� �̵�����
  �ʿ����� �Ǵ��Ѵ�.

  ����Ÿ���ϰ�ü�� ������ ChkptImageHdr(== Loganchor�κ��� �ʱ�ȭ��)
  �� ���� ����Ÿ���Ϸκ��� �ǵ��� ChkptImageHdr�� ���Ͽ�
  �̵������θ� �Ǵ��Ѵ�.

  [IN ] aChkptImageHdr - ����Ÿ���Ϸκ��� �ǵ��� ��Ÿ���
  [OUT] aNeedRecovery  - �̵����� �ʿ俩�� ��ȯ
*/
IDE_RC smmDatabaseFile::checkValidationDBFHdr(
                             smmChkptImageHdr  * aChkptImageHdr,
                             idBool            * aIsMediaFailure )
{
    UInt               sDBVer;
    UInt               sCtlVer;
    UInt               sFileVer;
    idBool             sIsMediaFailure;

    IDE_ASSERT( aChkptImageHdr  != NULL );
    IDE_ASSERT( aIsMediaFailure != NULL );

    sIsMediaFailure = ID_FALSE;

    // Loganchor�κ��� �ʱ�ȭ�� ChkptImageHdr

    IDE_TEST ( readChkptImageHdr(aChkptImageHdr) != IDE_SUCCESS );

// XXX BUG-27058 replicationGiveup.sql���� ���� ������ ����� ���Ͽ�
// �ӽ÷� ���� ����� Msg�� Release������ ��� �ǰ� �մϴ�.
// #ifdef DEBUG
    ideLog::log(SM_TRC_LOG_LEVEL_MEMORY,
                SM_TRC_MRECOVERY_CHECK_DB_SID_PPID_FID,
                mSpaceID,
                mPingPongNum,
                mFileNum);

    ideLog::log(SM_TRC_LOG_LEVEL_MEMORY,
                "LogAnchor SpaceID=%"ID_UINT32_FMT", SmVersion=%"ID_UINT32_FMT", "
                "DBFileHdr SpaceID=%"ID_UINT32_FMT", SmVersion=%"ID_UINT32_FMT", ",
                mChkptImageHdr.mSpaceID,
                mChkptImageHdr.mSmVersion,
                aChkptImageHdr->mSpaceID,
                aChkptImageHdr->mSmVersion );

    ideLog::log(SM_TRC_LOG_LEVEL_MEMORY,
                SM_TRC_MRECOVERY_CHECK_MEMORY_RLSN,
                mChkptImageHdr.mMemRedoLSN.mFileNo,
                mChkptImageHdr.mMemRedoLSN.mOffset,
                aChkptImageHdr->mMemRedoLSN.mFileNo,
                aChkptImageHdr->mMemRedoLSN.mOffset);

    ideLog::log(SM_TRC_LOG_LEVEL_MEMORY,
                SM_TRC_MRECOVERY_CHECK_MEMORY_CLSN,
                mChkptImageHdr.mMemCreateLSN.mFileNo,
                mChkptImageHdr.mMemCreateLSN.mOffset,
                aChkptImageHdr->mMemCreateLSN.mFileNo,
                aChkptImageHdr->mMemCreateLSN.mOffset);
// #endif

    sDBVer   = smVersionID & SM_CHECK_VERSION_MASK;
    sCtlVer  = mChkptImageHdr.mSmVersion & SM_CHECK_VERSION_MASK;
    sFileVer = aChkptImageHdr->mSmVersion & SM_CHECK_VERSION_MASK;

    IDE_TEST_RAISE( checkValuesOfDBFHdr( &mChkptImageHdr )
                    != IDE_SUCCESS,
                    err_invalid_hdr );

    // [1] SM VERSION Ȯ��
    // ����Ÿ���ϰ� ���� ���̳ʸ��� ȣȯ���� �˻��Ѵ�.
    IDE_TEST_RAISE(sDBVer != sCtlVer, err_invalid_hdr);
    IDE_TEST_RAISE(sDBVer != sFileVer, err_invalid_hdr);

    // [3] CREATE SN Ȯ��
    // ����Ÿ������ Create SN�� Loganchor�� Create SN�� �����ؾ��Ѵ�.
    // �ٸ��� ���ϸ� ������ ������ �ٸ� ����Ÿ�����̴�.
    // CREATE LSN
    IDE_TEST_RAISE( smLayerCallback::isLSNEQ(
                        &mChkptImageHdr.mMemCreateLSN,
                        &aChkptImageHdr->mMemCreateLSN)
                    != ID_TRUE, err_invalid_hdr );

    // [4] Redo LSN Ȯ��
    if ( smLayerCallback::isLSNLTE(
             &mChkptImageHdr.mMemRedoLSN,
             &aChkptImageHdr->mMemRedoLSN ) != ID_TRUE )
    {
        // ����Ÿ ������ REDO LSN���� Loganchor��
        // Redo LSN�� �� ũ�� �̵�� ������ �ʿ��ϴ�.

        sIsMediaFailure = ID_TRUE; // �̵�� ���� �ʿ�
    }
    else
    {
        // [ ������� ]
        // ����Ÿ ������ REDO LSN���� Loganchor�� Redo LSN��
        // �� �۰ų� ����.
    }

#ifdef DEBUG
    if ( sIsMediaFailure == ID_FALSE )
    {
        ideLog::log(SM_TRC_LOG_LEVEL_MEMORY,
                    SM_TRC_MRECOVERY_CHECK_DB_SUCCESS);
    }
    else
    {
        ideLog::log(SM_TRC_LOG_LEVEL_MEMORY,
                    SM_TRC_MRECOVERY_CHECK_NEED_MEDIARECOV);
    }
#endif

    *aIsMediaFailure = sIsMediaFailure;

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_invalid_hdr );
    {
        /* BUG-33353 */
        ideLog::log( IDE_SM_0, 
                     "DBVer = %"ID_UINT32_FMT", "
                     "CtrVer = %"ID_UINT32_FMT", "
                     "FileVer = %"ID_UINT32_FMT"",
                     sDBVer,     
                     sCtlVer,    
                     sFileVer );

        IDE_SET( ideSetErrorCode( smERR_ABORT_INVALID_CIMAGE_HEADER,
                                  (UInt)mSpaceID,
                                  (UInt)mPingPongNum,
                                  (UInt)mFileNum ) );
    }
    IDE_EXCEPTION_END;

#ifdef DEBUG
    ideLog::log(SM_TRC_LOG_LEVEL_MEMORY,
                SM_TRC_MRECOVERY_CHECK_DB_FAILURE);

#endif

    return IDE_FAILURE;
}

/*
   ����Ÿ���� HEADER�� ��ȿ���� �˻��Ѵ�.

   [IN] aChkptImageHdr : smmChkptImageHdr Ÿ���� ������
*/
IDE_RC smmDatabaseFile::checkValuesOfDBFHdr(
                                   smmChkptImageHdr*  aChkptImageHdr )
{
    smLSN sCmpLSN;

    IDE_DASSERT( aChkptImageHdr != NULL );

    // REDO LSN�� CREATE LSN�� 0�̰ų� ULONG_MAX ���� ���� �� ����.
    SM_LSN_INIT(sCmpLSN);

    IDE_TEST( smLayerCallback::isLSNEQ( &aChkptImageHdr->mMemRedoLSN,
                                        &sCmpLSN ) == ID_TRUE );

    SM_LSN_MAX(sCmpLSN);

    IDE_TEST( smLayerCallback::isLSNEQ( &aChkptImageHdr->mMemRedoLSN,
                                        &sCmpLSN ) == ID_TRUE );

    IDE_TEST( smLayerCallback::isLSNEQ( &aChkptImageHdr->mMemCreateLSN,
                                        &sCmpLSN ) == ID_TRUE );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}



/*
   ����Ÿ���� HEADER�� ��ȿ���� �˻��Ѵ�. (ASSERT����)

   [IN] aChkptImageHdr : smmChkptImageHdr Ÿ���� ������
*/
IDE_RC smmDatabaseFile::assertValuesOfDBFHdr(
                          smmChkptImageHdr*  aChkptImageHdr )
{
    smLSN sCmpLSN;

    IDE_DASSERT( aChkptImageHdr != NULL );

    // REDO LSN�� CREATE LSN�� 0�̰ų� ULONG_MAX ���� ���� �� ����.
    SM_LSN_INIT(sCmpLSN);

    IDE_ASSERT( smLayerCallback::isLSNEQ(
                    &aChkptImageHdr->mMemRedoLSN,
                    &sCmpLSN ) == ID_FALSE );

// BUGBUG-1548 Memory LFG���� log�� ����� ��Ȳ������ Disk LFG(0��)�� LSN�� 0�ϼ� �ִ�.
/*
        IDE_TEST( smLayerCallback::isLSNEQ(
                         &aChkptImageHdr->mArrMemCreateLSN[ sLoop ],
                         &sCmpLSN ) == ID_TRUE );
*/

    SM_LSN_MAX(sCmpLSN);

    IDE_ASSERT( smLayerCallback::isLSNEQ(
                    &aChkptImageHdr->mMemRedoLSN,
                    &sCmpLSN ) == ID_FALSE );

    IDE_ASSERT( smLayerCallback::isLSNEQ(
                    &aChkptImageHdr->mMemCreateLSN,
                    &sCmpLSN ) == ID_FALSE );

    return IDE_SUCCESS;
}

/*
  PRJ-1548 User Memroy Tablespace ���䵵��

  ������ ����Ÿ���Ͽ� ���� �Ӽ��� �α׾�Ŀ�� �߰��Ѵ�.
*/
IDE_RC smmDatabaseFile::addAttrToLogAnchorIfCrtFlagIsFalse(
                                 smmTBSNode* aTBSNode )
{
    idBool                      sSaved;
    smmChkptImageAttr           sChkptImageAttr;
    smiDataFileDescSlotID     * sDataFileDescSlotID;
    smiDataFileDescSlotID       sDataFileDescSlotIDFromLogAnchor;

    IDE_DASSERT( aTBSNode != NULL );

    // smmManager::mCreateDBFileOnDisk[] �÷��״� TRUE�� �����ϱ� ������
    // �ݵ�� FALSE ���� �Ѵ�.
    IDE_ASSERT( smmManager::getCreateDBFileOnDisk( aTBSNode,
                                                   mPingPongNum,
                                                   mFileNum )
                == ID_FALSE );

    // �̹� �α׾�Ŀ�� ����Ǿ����� Ȯ���Ѵ�.
    sSaved = smmManager::isCreateDBFileAtLeastOne(
                (aTBSNode->mCrtDBFileInfo[ mFileNum ]).mCreateDBFileOnDisk ) ;

    // ������ �����Ǿ����Ƿ� �������θ� �����Ѵ�.
    smmManager::setCreateDBFileOnDisk( aTBSNode,
            mPingPongNum,
            mFileNum,
            ID_TRUE );

    // ����Ÿ���� ����� ���� �Ӽ��� ��´�.
    // �Ʒ� �Լ������� smmManager::mCreateDBFileOnDisk�� �����ϱ� ������
    // ���� ������ �ʿ��ϴٸ� smmManager::setCreateDBFileOnDisk ȣ���Ŀ�
    // getChkptImageAttr�� ȣ���Ͽ� ������ ���Ѿ� �Ѵ�.
    getChkptImageAttr( aTBSNode, &sChkptImageAttr );

    if ( sSaved == ID_TRUE )
    {
        //PROJ-2133 incremental backup
        //Disk datafile�������� �ٸ��� memory DatabaseFile�� ������ checkpoint��
        //���� ����Ǳ⶧���� ������ checkpoint������ lock�� �����ʰ�
        //DataFileDescSlot�� �Ҵ��Ѵ�.
        if ( ( smLayerCallback::isCTMgrEnabled() == ID_TRUE ) ||
             ( smLayerCallback::isCreatingCTFile() == ID_TRUE ) )
        {
            IDE_ASSERT( smLayerCallback::getDataFileDescSlotIDFromLogAncho4ChkptImage( 
                            aTBSNode->mCrtDBFileInfo[ mFileNum ].mAnchorOffset,
                            &sDataFileDescSlotIDFromLogAnchor )
                        == IDE_SUCCESS );
                                                    
            sChkptImageAttr.mDataFileDescSlotID.mBlockID = 
                                    sDataFileDescSlotIDFromLogAnchor.mBlockID;
            sChkptImageAttr.mDataFileDescSlotID.mSlotIdx = 
                                    sDataFileDescSlotIDFromLogAnchor.mSlotIdx;

            mChkptImageHdr.mDataFileDescSlotID.mBlockID = 
                                    sDataFileDescSlotIDFromLogAnchor.mBlockID;
            mChkptImageHdr.mDataFileDescSlotID.mSlotIdx = 
                                    sDataFileDescSlotIDFromLogAnchor.mSlotIdx;

            //DataFileDescSlotID�� ChkptImageHdr�� ����Ѵ�.
            IDE_ASSERT( setDBFileHeader( NULL ) == IDE_SUCCESS );
        }
        else
        {
            /* nothing to do */
        }


        // fix BUG-17343
        // loganchor�� Stable/Unstable Chkpt Image�� ���� ���� ������ ����
        IDE_ASSERT( smLayerCallback::updateChkptImageAttrAndFlush(
                           &(aTBSNode->mCrtDBFileInfo[ mFileNum ]),
                           &sChkptImageAttr )
                    == IDE_SUCCESS );
    }
    else
    {
        //PROJ-2133 incremental backup
        //Disk datafile�������� �ٸ��� memory DatabaseFile�� ������ checkpoint��
        //���� ����Ǳ⶧���� ������ checkpoint������ lock�� �����ʰ�
        //DataFileDescSlot�� �Ҵ��Ѵ�.
        if ( ( smLayerCallback::isCTMgrEnabled() == ID_TRUE ) ||
             ( smLayerCallback::isCreatingCTFile() == ID_TRUE ) )
        {
            IDE_ASSERT( smriChangeTrackingMgr::addDataFile2CTFile( 
                                                       aTBSNode->mHeader.mID,
                                                       mFileNum,                             
                                                       SMRI_CT_MEM_TBS,
                                                       &sDataFileDescSlotID ) 
                        == IDE_SUCCESS ); 

            sChkptImageAttr.mDataFileDescSlotID.mBlockID = 
                                        sDataFileDescSlotID->mBlockID;
            sChkptImageAttr.mDataFileDescSlotID.mSlotIdx = 
                                        sDataFileDescSlotID->mSlotIdx;

            mChkptImageHdr.mDataFileDescSlotID.mBlockID = 
                                        sDataFileDescSlotID->mBlockID;
            mChkptImageHdr.mDataFileDescSlotID.mSlotIdx = 
                                        sDataFileDescSlotID->mSlotIdx;
            
            //DataFileDescSlotID�� ChkptImageHdr�� ����Ѵ�.
            IDE_ASSERT( setDBFileHeader( NULL ) == IDE_SUCCESS );
        }
        else
        {
            /* nothing to do */
        }


        // ���� �����̱� ������ �α׾�Ŀ�� �߰��Ѵ�.
        IDE_ASSERT( smLayerCallback::addChkptImageAttrAndFlush(
                           &(aTBSNode->mCrtDBFileInfo[ mFileNum ]),
                           &(sChkptImageAttr) )
                    == IDE_SUCCESS );
    }

    return IDE_SUCCESS;
}


/*
  PRJ-1548 User Memroy Tablespace ���䵵��

  �̵����� �����ϱ� ���� ����Ÿ���Ͽ� �÷��׸� �����Ѵ�.
*/
IDE_RC smmDatabaseFile::prepareMediaRecovery(
                               smiRecoverType        aRecoveryType,
                               smmChkptImageHdr    * aChkptImageHdr,
                               smLSN               * aFromRedoLSN,
                               smLSN               * aToRedoLSN )
{
    smLSN  sInitLSN;

    IDE_DASSERT( aChkptImageHdr  != NULL );
    IDE_DASSERT( aFromRedoLSN != NULL );
    IDE_DASSERT( aToRedoLSN   != NULL );

    // �̵����� �÷��׸� ID_TRUE �����Ѵ�.
    mIsMediaFailure = ID_TRUE;

    SM_LSN_INIT( sInitLSN );

    // [1] TO REDO LSN �����ϱ�
    if ( aRecoveryType == SMI_RECOVER_COMPLETE )
    {
        // ���������� ��� ��������� REDO LSN����
        // �̵�� ������ �����Ѵ�.
        SM_GET_LSN( *aToRedoLSN, mChkptImageHdr.mMemRedoLSN );
    }
    else
    {
        // �ҿ��� �����ϰ�� �Ҽ� �ִµ�����
        // �̵�� ������ �����Ѵ�.
        IDE_ASSERT( ( aRecoveryType == SMI_RECOVER_UNTILCANCEL ) ||
                    ( aRecoveryType == SMI_RECOVER_UNTILTIME) );

        SM_LSN_MAX( *aToRedoLSN );
    }

    /*
      [2] FROM REDOLSN �����ϱ�
      ���� ��������� REDOLSN�� INITLSN�̶�� EMPTY
      ����Ÿ�����̴�.
     */
    if ( smLayerCallback::isLSNEQ( &aChkptImageHdr->mMemRedoLSN,
                                   &sInitLSN) == ID_TRUE )
    {
        // �ҿ������� �䱸�� EMPTY ������ �����Ѵٸ�
        // ���������� �����Ͽ����ϱ� ������ ����ó���Ѵ�.
        IDE_TEST_RAISE(
            (aRecoveryType == SMI_RECOVER_UNTILTIME) ||
            (aRecoveryType == SMI_RECOVER_UNTILCANCEL),
            err_incomplete_media_recovery);

        // EMPTY ����Ÿ������ ��쿡�� CREATELSN
        // ���� �̵����� �����Ѵ�.
        SM_GET_LSN( *aFromRedoLSN, mChkptImageHdr.mMemCreateLSN );
    }
    else
    {
        /* BUG-39354
         * ���� ����� REDOLSN�� CREATELSN�� �����ϸ�
         * EMPTY Memory Checkpoint Image�����̴�.
         */
        if ( smLayerCallback::isLSNEQ( &aChkptImageHdr->mMemRedoLSN,
                                       &mChkptImageHdr.mMemCreateLSN ) == ID_TRUE )
        {
            /* �ҿ������� �䱸�� EMPTY ������ �����Ѵٸ�
             ���������� �����Ͽ����ϱ� ������ ����ó���Ѵ�. */
            IDE_TEST_RAISE(
                (aRecoveryType == SMI_RECOVER_UNTILTIME) ||
                (aRecoveryType == SMI_RECOVER_UNTILCANCEL),
                err_incomplete_media_recovery);
            
            /* EMPTY ����Ÿ������ ��쿡�� CREATELSN
             * ���� �̵����� �����Ѵ�. */
            SM_GET_LSN( *aFromRedoLSN, mChkptImageHdr.mMemCreateLSN );
        }
        else
        {
            // �������� �Ǵ� �ҿ��� ����������
            // FROM REDOLSN�� ��������� REDO LSN����
            // �����Ѵ�.
            SM_GET_LSN( *aFromRedoLSN, aChkptImageHdr->mMemRedoLSN );
        }
    }

    // �̵���������
    // FROM REDOLSN < TO REDOLSN�� ������ �����ؾ��Ѵ�.
    IDE_ASSERT( smLayerCallback::isLSNGT( aToRedoLSN,
                                          aFromRedoLSN)
                == ID_TRUE );

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_incomplete_media_recovery );
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_NeedMediaRecovery ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : BUG-28523 [SM] üũ ����Ʈ ��� ������ ��λ�
 *               üũ����Ʈ �̹����� �����ϴ��� �˻��ؾ� �մϴ�.
 *               Checkpoint Path�� Drop�ϱ� ���� �ش� Path����
 *               Checkpoint Image�� �ٸ� ��ȿ�� ��η� �Ű������ Ȯ���Ѵ�.
 *
 *   aTBSNode          [IN] - Tablespace Node
 *   aCheckpointReason [IN] - Drop�Ϸ��� Checkpoint Path Node
 **********************************************************************/
IDE_RC smmDatabaseFile::checkChkptImgInDropCPath( smmTBSNode       * aTBSNode,
                                                  smmChkptPathNode * aDropPathNode )
{
    UInt               sStableDB;
    UInt               sDBFileNo;
    idBool             sFound;
    SChar              sDBFileName[SM_MAX_FILE_NAME];
    smuList          * sListNode ;
    smmChkptPathNode * sCPathNode;
    PDL_stat           sFileInfo;

    IDE_ASSERT( aTBSNode      != NULL );
    IDE_ASSERT( aDropPathNode != NULL );

    sStableDB = smmManager::getCurrentDB(aTBSNode);

    for ( sDBFileNo = 0;
          sDBFileNo <= aTBSNode->mLstCreatedDBFile;
          sDBFileNo++ )
    {
        // Drop�Ϸ��� ��θ� ������ �ٸ� ��ο���
        // Checkpoint Image File�� ã�´�.

        sFound = ID_FALSE;

        for( sListNode  = SMU_LIST_GET_FIRST( & aTBSNode->mChkptPathBase );
             sListNode != & aTBSNode->mChkptPathBase ;
             sListNode  = SMU_LIST_GET_NEXT( sListNode ) )
        {
            IDE_ASSERT( sListNode != NULL );

            sCPathNode = (smmChkptPathNode*) sListNode->mData;

            IDE_ASSERT( sCPathNode != NULL );

#if defined(VC_WIN32)
            sctTableSpaceMgr::adjustFileSeparator( sCPathNode->mChkptPathAttr.mChkptPath );
            sctTableSpaceMgr::adjustFileSeparator( aDropPathNode->mChkptPathAttr.mChkptPath );
#endif

            // Drop �� Path�̿��� ��ο� DB Img File�� ���� �Ͽ��� �Ѵ�.
            if( idlOS::strcmp( sCPathNode->mChkptPathAttr.mChkptPath,
                               aDropPathNode->mChkptPathAttr.mChkptPath ) == 0 )
            {
                continue;
            }

            idlOS::snprintf(sDBFileName,
                            ID_SIZEOF( sDBFileName ),
                            SMM_CHKPT_IMAGE_NAME_WITH_PATH,
                            sCPathNode->mChkptPathAttr.mChkptPath,
                            aTBSNode->mTBSAttr.mName,
                            sStableDB,
                            sDBFileNo);

            if( idlOS::stat( sDBFileName, &sFileInfo ) == 0 )
            {
                sFound = ID_TRUE;
                break;
            }
        }

        if( sFound == ID_FALSE )
        {
            // Chkpt Img�� ã�� ���ߴٸ� ������ �� �� �ϳ��̴�.
            // 1. Drop�Ϸ��� ��ο� Chkpt Image�� �ִ� ���
            // 2. ������ �ƹ������� ���� ���

#if defined(VC_WIN32)
            sctTableSpaceMgr::adjustFileSeparator( aDropPathNode->mChkptPathAttr.mChkptPath );
#endif

            idlOS::snprintf(sDBFileName,
                            ID_SIZEOF( sDBFileName ),
                            SMM_CHKPT_IMAGE_NAME_WITH_PATH,
                            aDropPathNode->mChkptPathAttr.mChkptPath,
                            aTBSNode->mTBSAttr.mName,
                            sStableDB,
                            sDBFileNo);

            IDE_TEST_RAISE( idlOS::stat( sDBFileName, &sFileInfo ) == 0 ,
                            err_chkpt_img_in_drop_path );

            IDE_RAISE( err_file_not_exist );
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_file_not_exist );
    {
        idlOS::snprintf(sDBFileName,
                        ID_SIZEOF( sDBFileName ),
                        SMM_CHKPT_IMAGE_NAME_TBS_PINGPONG_FILENUM,
                        aTBSNode->mTBSAttr.mName,
                        sStableDB,
                        sDBFileNo);

        IDE_SET( ideSetErrorCode( smERR_ABORT_NoExistFile,
                                  (SChar*)sDBFileName ) );
    }
    IDE_EXCEPTION( err_chkpt_img_in_drop_path );
    {
        idlOS::snprintf(sDBFileName,
                        ID_SIZEOF( sDBFileName ),
                        SMM_CHKPT_IMAGE_NAME_TBS_PINGPONG_FILENUM,
                        aTBSNode->mTBSAttr.mName,
                        sStableDB,
                        sDBFileNo);

#if defined(VC_WIN32)
        sctTableSpaceMgr::adjustFileSeparator( aDropPathNode->mChkptPathAttr.mChkptPath );
#endif

        IDE_SET( ideSetErrorCode( smERR_ABORT_DROP_CPATH_NOT_YET_MOVED_CIMG_IN_CPATH,
                                  (SChar*)sDBFileName,
                                  aDropPathNode->mChkptPathAttr.mChkptPath ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : BUG-29607 Create Memory Tablespace �� ������ �̸��� ������
 *               üũ����Ʈ ��ξȿ� �����ϴ����� ������ ������ ���ϸ�
 *               �����ؼ� Ȯ���մϴ�.
 *
 *   aTBSNode          [IN] - Tablespace Node
 **********************************************************************/
IDE_RC smmDatabaseFile::chkExistDBFileByNode( smmTBSNode * aTBSNode )
{
    UInt              sCPathIdx;
    UInt              sCPathCnt;
    smmChkptPathNode *sCPathNode;

    IDE_TEST( smmTBSChkptPath::getChkptPathNodeCount( aTBSNode,
                                                      &sCPathCnt )
              != IDE_SUCCESS );

    IDE_ASSERT( sCPathCnt != 0 );

    for( sCPathIdx = 0 ; sCPathIdx < sCPathCnt ; sCPathIdx++ )
    {
        IDE_TEST( smmTBSChkptPath::getChkptPathNodeByIndex( aTBSNode,
                                                            sCPathIdx,
                                                            &sCPathNode )
                  != IDE_SUCCESS );

        IDE_ASSERT( sCPathNode != NULL );

#if defined(VC_WIN32)
        sctTableSpaceMgr::adjustFileSeparator( sCPathNode->mChkptPathAttr.mChkptPath );
#endif

        IDE_TEST( chkExistDBFile( aTBSNode->mHeader.mName,
                                  sCPathNode->mChkptPathAttr.mChkptPath )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : BUG-29607 Create DB �� ������ �̸��� ������
 *               üũ����Ʈ ��ξȿ� �����ϴ����� ������ ������ ���ϸ�
 *               �����ؼ� Ȯ���մϴ�.
 *
 *   aTBSNode          [IN] - Tablespace Name
 **********************************************************************/
IDE_RC smmDatabaseFile::chkExistDBFileByProp( const SChar * aTBSName )
{
    UInt   sCPathIdx;

    for( sCPathIdx = 0 ; sCPathIdx < smuProperty::getDBDirCount() ; sCPathIdx++ )
    {
        IDE_TEST( chkExistDBFile( aTBSName,
                                  smuProperty::getDBDir( sCPathIdx ) )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : BUG-29607 Create Memory Tablespace �� ����� �̸��� ������
 *               üũ����Ʈ ��ξȿ� �����ϴ��� Ȯ���մϴ�.
 *
 *   aTBSName       [IN] - Tablespace Name
 *   aChkptPath     [IN] - Checkpoint Path
 **********************************************************************/
IDE_RC smmDatabaseFile::chkExistDBFile( const SChar * aTBSName,
                                        const SChar * aChkptPath )
{
    DIR           *sDir;
    SInt           sRc;
    SInt           sState = 0;
    UInt           sPingPongNum;
    UInt           sTokenLen;
    SChar*         sCurFileName = NULL;
    SChar          sChkFileName[2][SM_MAX_FILE_NAME];
    SChar          sInvalidFileName[SM_MAX_FILE_NAME];
    struct dirent *sDirEnt;
    struct dirent *sResDirEnt;

    IDE_ASSERT( aTBSName != NULL );
    IDE_ASSERT( aChkptPath  != NULL );

    /* smmDatabaseFile_chkExistDBFile_malloc_DirEnt.tc */
    IDU_FIT_POINT("smmDatabaseFile::chkExistDBFile::malloc::DirEnt");
    IDE_TEST( iduMemMgr::malloc( IDU_MEM_SM_SMM,
                                 ID_SIZEOF(struct dirent) + SM_MAX_FILE_NAME,
                                 (void**)&sDirEnt,
                                 IDU_MEM_FORCE )
              != IDE_SUCCESS );
    sState = 1;

    for( sPingPongNum = 0 ; sPingPongNum < SMM_PINGPONG_COUNT ; sPingPongNum++ )
    {
        idlOS::snprintf( (SChar*)&sChkFileName[ sPingPongNum ],
                         ID_SIZEOF( sChkFileName[ sPingPongNum ] ),
                         SMM_CHKPT_IMAGE_NAME_TBS_PINGPONG,
                         aTBSName,
                         sPingPongNum );
    }

    sTokenLen = idlOS::strlen( (SChar*)sChkFileName[0] );
    IDE_ASSERT( idlOS::strlen( (SChar*)sChkFileName[1] ) == sTokenLen );

    sDir   = idf::opendir( aChkptPath );
    sState = 2;

    IDE_TEST_RAISE( sDir == NULL, invalid_path_error );

    while (1)
    {
	/*
         * BUG-31424 In AIX, createdb can't be executed because  
         *           the readdir_r function isn't handled well.
         */
	errno=0;

        // ���ϸ��� �ϳ��� �����ͼ� ��,
        sRc = idf::readdir_r( sDir,
                              sDirEnt,
                              &sResDirEnt );

        IDE_TEST_RAISE( (sRc != 0) && (errno != 0),
                        invalid_path_error );

        if (sResDirEnt == NULL)
        {
            break;
        }

        sCurFileName = sResDirEnt->d_name;

        for( sPingPongNum = 0 ;
             sPingPongNum < SMM_PINGPONG_COUNT ;
             sPingPongNum++ )
        {
            if ( idlOS::strncmp( sCurFileName,
                                 (SChar*)&sChkFileName[ sPingPongNum ],
                                 sTokenLen ) == 0 )
            {
                break;
            }
        }

        if( sPingPongNum == SMM_PINGPONG_COUNT )
        {
            // ���ϸ��� �ƿ� ������� ������� ����
            continue;
        }

        sCurFileName += sTokenLen;

        if( smuUtility::isDigitForString( sCurFileName,
                                          idlOS::strlen( sCurFileName ) )
            == ID_FALSE )
        {
            // ���ϸ� ���ĺκ��� ������ ��찡 �ƴϸ� ����.
            // Checkpoint Image File Number Ȯ��.
            continue;
        }

        idlOS::snprintf( sInvalidFileName,
                         SM_MAX_FILE_NAME,
                         SMM_CHKPT_IMAGE_NAMES,
                         aTBSName );

        IDE_RAISE( exist_file_error );
    }

    sState = 1;
    idf::closedir(sDir);

    sState = 0;
    IDE_TEST( iduMemMgr::free( sDirEnt ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( invalid_path_error );
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_CPATH_NOT_EXIST,
                                  aChkptPath ));
    }
    IDE_EXCEPTION( exist_file_error );
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_AlreadyExistFile,
                                  sInvalidFileName ));
    }
    IDE_EXCEPTION_END;

    switch( sState )
    {
        case 2:
            idf::closedir(sDir);
        case 1:
            IDE_ASSERT( iduMemMgr::free( sDirEnt ) == IDE_SUCCESS );
            break;
        default:
            break;
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : smmDataFile�� incremetalBackup�Ѵ�.
 * PROJ-2133
 *
 **********************************************************************/
IDE_RC smmDatabaseFile::incrementalBackup( smriCTDataFileDescSlot * aDataFileDescSlot,
                                           smmDatabaseFile        * aDatabaseFile,
                                           smriBISlot             * aBackupInfo )
{
    iduFile               * sSrcFile; 
    iduFile                 sDestFile; 
    UInt                    sState = 0;
    ULong                   sBackupFileSize  = 0;
    ULong                   sBackupSize;
    ULong                   sIBChunkSizeInByte;            
    ULong                   sRestIBChunkSizeInByte;   
    idBool                  sCreateFileState = ID_FALSE;

    IDE_ASSERT( aDataFileDescSlot   != NULL );
    IDE_ASSERT( aDatabaseFile       != NULL );
    IDE_ASSERT( aBackupInfo         != NULL );

    sSrcFile = &aDatabaseFile->mFile;     

    IDE_TEST( sDestFile.initialize( IDU_MEM_SM_SMM,
                                    1,                             
                                    IDU_FIO_STAT_OFF,              
                                    IDV_WAIT_INDEX_NULL )          
              != IDE_SUCCESS );   
    sState = 1;

    IDE_TEST( sDestFile.setFileName( aBackupInfo->mBackupFileName ) 
              != IDE_SUCCESS );

    IDE_TEST( sDestFile.create()    != IDE_SUCCESS );
    sCreateFileState = ID_TRUE;

    IDE_TEST( sDestFile.open()      != IDE_SUCCESS );
    sState = 2;

    /* Incremental backup�� �����Ѵ�.*/
    IDE_TEST( smriChangeTrackingMgr::performIncrementalBackup( 
                                                   aDataFileDescSlot,
                                                   sSrcFile,
                                                   &sDestFile,
                                                   SMRI_CT_MEM_TBS,
                                                   aDatabaseFile->mSpaceID,
                                                   aDatabaseFile->mFileNum,
                                                   aBackupInfo )
              != IDE_SUCCESS );

    /* 
     * ������ ������ ũ��� ����� IBChunk�� ������ ���� backup�� ũ�Ⱑ
     * �������� Ȯ���Ѵ�
     */
    IDE_TEST( sDestFile.getFileSize( &sBackupFileSize ) != IDE_SUCCESS );

    sIBChunkSizeInByte = (ULong)SM_PAGE_SIZE * aBackupInfo->mIBChunkSize;

    sBackupSize = (ULong)aBackupInfo->mIBChunkCNT * sIBChunkSizeInByte;

    /* 
     * ������ Ȯ���̳� ��Ұ� IBChunk�� ũ�⸸ŭ �̷����� ������ �ֱ⶧����
     * ���� ����� ������ ũ��� mIBChunkCNT�� ����� �ƴҼ� �ִ�.
     * ������ ũ�Ⱑ mIBChunkCNT�� ����� �ǵ��� �������ش�.
     */
    
    if( sBackupFileSize > SM_DBFILE_METAHDR_PAGE_SIZE )
    {
        sBackupFileSize -= SM_DBFILE_METAHDR_PAGE_SIZE;

        sRestIBChunkSizeInByte = (ULong)sBackupFileSize % sIBChunkSizeInByte;

        IDE_ASSERT( sRestIBChunkSizeInByte % SM_PAGE_SIZE == 0 );

        if( sRestIBChunkSizeInByte != 0 )
        {
            sBackupFileSize += sIBChunkSizeInByte - sRestIBChunkSizeInByte;
        }
    }

    IDE_ASSERT( sBackupFileSize == sBackupSize );

    sState = 1;
    IDE_TEST( sDestFile.close() != IDE_SUCCESS );

    sState = 0;
    IDE_TEST( sDestFile.destroy() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch( sState )
    {
        case 2:
            IDE_ASSERT( sDestFile.close() == IDE_SUCCESS );
        case 1:
            IDE_ASSERT( sDestFile.destroy() == IDE_SUCCESS );
        case 0:
        default:
            break;
    }

    if( sCreateFileState == ID_TRUE )
    {
        IDE_ASSERT( idf::unlink( aBackupInfo->mBackupFileName ) 
                    == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : ������ chkptImage������ pingpong������ �������ϴ��� �Ǵ��Ѵ�.
 * PROJ-2133
 *
 **********************************************************************/
IDE_RC smmDatabaseFile::isNeedCreatePingPongFile( smriBISlot * aBISlot,
                                                  idBool     * aResult )
{
    UInt                sDBVer;
    UInt                sCtlVer;
    UInt                sFileVer;
    idBool              sResult;
    smmChkptImageHdr    sChkptImageHdr;

    IDE_DASSERT( aBISlot != NULL );
    IDE_DASSERT( aResult != NULL );

    sResult = ID_TRUE;

    IDE_TEST ( readChkptImageHdrByPath( aBISlot->mBackupFileName,
                                        &sChkptImageHdr) != IDE_SUCCESS );

    IDE_TEST_RAISE( checkValuesOfDBFHdr( &mChkptImageHdr )
                    != IDE_SUCCESS,
                    err_invalid_hdr );

    sDBVer   = smVersionID & SM_CHECK_VERSION_MASK;
    sCtlVer  = mChkptImageHdr.mSmVersion & SM_CHECK_VERSION_MASK;
    sFileVer = sChkptImageHdr.mSmVersion & SM_CHECK_VERSION_MASK;

    // [1] SM VERSION Ȯ��
    // ����Ÿ���ϰ� ���� ���̳ʸ��� ȣȯ���� �˻��Ѵ�.
    IDE_TEST_RAISE(sDBVer != sCtlVer, err_invalid_hdr);
    IDE_TEST_RAISE(sDBVer != sFileVer, err_invalid_hdr);

    // [3] CREATE SN Ȯ��
    // ����Ÿ������ Create SN�� Loganchor�� Create SN�� �����ؾ��Ѵ�.
    // �ٸ��� ���ϸ� ������ ������ �ٸ� ����Ÿ�����̴�.
    // CREATE LSN
    IDE_TEST_RAISE( smLayerCallback::isLSNEQ(
                        &mChkptImageHdr.mMemCreateLSN,
                        &sChkptImageHdr.mMemCreateLSN)
                    != ID_TRUE, err_invalid_hdr );

    // [4] Redo LSN Ȯ��
    if ( smLayerCallback::isLSNLTE(
             &mChkptImageHdr.mMemRedoLSN,
             &sChkptImageHdr.mMemRedoLSN ) != ID_TRUE )
    {
        // ����Ÿ ������ REDO LSN���� Loganchor��
        // Redo LSN�� �� ũ�� �̵�� ������ �ʿ��ϴ�.

        sResult = ID_FALSE;
    }
    else
    {
        // [ ������� ]
        // ����Ÿ ������ REDO LSN���� Loganchor�� Redo LSN��
        // �� �۰ų� ����.
    }

    *aResult = sResult;

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_invalid_hdr );
    {
        IDE_SET( ideSetErrorCode(
                    smERR_ABORT_INVALID_CIMAGE_HEADER,
                    (UInt)mSpaceID,
                    (UInt)mPingPongNum,
                    (UInt)mFileNum ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
} 

/***********************************************************************
 * Description : ������ ���� path�� �����ϴ� chkptImage�� ����� �о�´�.
 * PROJ-2133
 *
 **********************************************************************/
IDE_RC smmDatabaseFile::readChkptImageHdrByPath( SChar            * aChkptImagePath,
                                                 smmChkptImageHdr * aChkptImageHdr )
{
    iduFile sFile;
    UInt    sState = 0;

    IDE_DASSERT( aChkptImagePath != NULL );
    IDE_DASSERT( aChkptImageHdr  != NULL );

    IDE_TEST( sFile.initialize( IDU_MEM_SM_SMM, 
                                1, /* Max Open FD Count */     
                                IDU_FIO_STAT_OFF,              
                                IDV_WAIT_INDEX_NULL ) != IDE_SUCCESS );
    sState = 1;

    IDE_TEST( sFile.setFileName( aChkptImagePath ) != IDE_SUCCESS );

    IDE_TEST_RAISE( sFile.open() != IDE_SUCCESS , err_file_not_found);
    sState = 2;

    IDE_TEST( sFile.read( NULL, /*idvSQL*/               
                          SM_DBFILE_METAHDR_PAGE_OFFSET, 
                          (void*)aChkptImageHdr,
                          ID_SIZEOF( smmChkptImageHdr ) )
              != IDE_SUCCESS );

    sState = 1;
    IDE_TEST( sFile.close()   != IDE_SUCCESS );
    
    sState = 0;
    IDE_TEST( sFile.destroy() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_file_not_found );                      
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_NotFoundDataFileByPath,
                                  aChkptImagePath ) );
    }
    IDE_EXCEPTION_END;

    switch( sState )
    {
        case 2:
            IDE_ASSERT( sFile.close()   == IDE_SUCCESS );
        case 1:
            IDE_ASSERT( sFile.destroy() == IDE_SUCCESS );
        default:
            break;
    }

    return IDE_FAILURE;
}
