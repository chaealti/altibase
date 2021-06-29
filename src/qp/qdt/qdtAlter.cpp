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
 * $Id: qdtAlter.cpp 88963 2020-10-19 03:33:18Z jake.jang $
 **********************************************************************/

#include <idl.h>
#include <qcg.h>
#include <qcmTableSpace.h>
#include <qdtAlter.h>
#include <qdtCommon.h>
#include <qdpPrivilege.h>
#include <smiTableSpace.h>
#include <qdpRole.h>

/*
    Disk Data tablespace, Disk Temp Tablespace�� ALTER������ ����
    SM�� Tablespace Type��ȸ

    [IN] aQueryTBSType - SQL Parsing�� ������ Tablespace type
    [IN] aStorageTBSType - SM�� ����� ���� Tablespace type
 */
IDE_RC qdtAlter::isDiskTBSType(smiTableSpaceType aQueryTBSType,
                               smiTableSpaceType aStorageTBSType )
{
    // PROJ-1548 User Memory Tablespace
    //
    // Memory Tablespace�� Disk Tablespace�� ALTER������ ������ ����
    if ( ( aStorageTBSType != SMI_DISK_SYSTEM_DATA ) &&
         ( aStorageTBSType != SMI_DISK_SYSTEM_UNDO ) &&
         ( aStorageTBSType != SMI_DISK_SYSTEM_TEMP ) &&
         ( aStorageTBSType != SMI_DISK_USER_DATA )   &&
         ( aStorageTBSType != SMI_DISK_USER_TEMP ) )
    {
        // SM�� ���� Tablespace Type�� Disk Tablespace�� �ƴϸ� ����
        IDE_RAISE( ERR_INVALID_ALTER_ON_DISK_TBS );
    }

    //-----------------------------------------------------
    // To Fix BUG-10414
    // add ��� file�� ����� table space�� ���缺 �˻�
    // ( data file�� data table space��
    //   temp file�� temp table space�� add �ؾ� �� )
    // 
    // Parsing �ܰ�
    //    : � file( data file / temp file )�� add �� ���ΰ��� ����
    //      table space type ���� 
    //      < parsing �������� �����Ǵ� table space ���� >
    //        SMI_DISK_USER_DATA : data file�� add �Ϸ��� �� ��� 
    //        SMI_DISK_USER_TEMP : temp file�� add �Ϸ��� �� ���
    // Validation �ܰ�
    //    : add�� file�� ����� table space ������ ���缺 �˻� ����
    //      < qcmTablespace::getTBSAttrByName()���� ������ table space ���� >
    //        SMI_MEMORY_SYSTEM_DICTIONARY
    //        SMI_MEMORY_SYSTEM_DATA,            
    //        SMI_MEMORY_USER_DATA,
    //        SMI_DISK_SYSTEM_DATA,
    //        SMI_DISK_USER_DATA,
    //        SMI_DISK_SYSTEM_TEMP,
    //        SMI_DISK_USER_TEMP,
    //        SMI_DISK_SYSTEM_UNDO,
    //        SMI_TABLESPACE_TYPE_MAX
    
    //        SMI_MEMORY_ALL_DATA
    //        SMI_DISK_SYSTEM_DATA
    //        SMI_DISK_USER_DATA
    //        SMI_DISK_SYSTEM_TEMP
    //        SMI_DISK_USER_TEMP
    //        SMI_DISK_SYSTEM_UNDO
    //        SMI_TABLESPACE_TYPE_MAX
    //-----------------------------------------------------
    if ( aQueryTBSType == SMI_DISK_USER_DATA )
    {
        // Data File�� Add �Ϸ��� �ϴ� ���
        if ( (aStorageTBSType == SMI_DISK_SYSTEM_DATA) ||
             (aStorageTBSType == SMI_DISK_USER_DATA) ||
             (aStorageTBSType == SMI_DISK_SYSTEM_UNDO) )
        {
            // nothing to do 
        }
        else
        {
            // Data File�� add �� �� ���� TableSpace ��
            IDE_RAISE(ERR_MISMATCH_TBS_TYPE);
        }
    }
    else
    {
        // Temp File�� Add �Ϸ��� �ϴ� ���
        if ( (aStorageTBSType == SMI_DISK_SYSTEM_TEMP) ||
             (aStorageTBSType == SMI_DISK_USER_TEMP) )
        {
            // nothing to do
        }
        else
        {
            // Temp File�� add �� �� ���� TableSpace��. 
            IDE_RAISE(ERR_MISMATCH_TBS_TYPE);
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_INVALID_ALTER_ON_DISK_TBS)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDT_INVALID_ALTER_ON_DISK_TBS));
    }
    IDE_EXCEPTION(ERR_MISMATCH_TBS_TYPE)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDT_MISMATCH_TBS_TYPE));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/*
    Memory data tablespace�� ALTER������ ����
    SM�� Tablespace Type��ȸ

    [IN] aQueryTBSType - SQL Parsing�� ������ Tablespace type
    [IN] aStorageTBSType - SM�� ����� ���� Tablespace type
 */
IDE_RC qdtAlter::isMemTBSType(smiTableSpaceType aTBSType)
{
    // PROJ-1548 User Memory Tablespace
    //
    // Disk Tablespace�� Memory Tablespace�� ALTER������ ������ ����
    if ( ( aTBSType != SMI_MEMORY_USER_DATA )         &&
         ( aTBSType != SMI_MEMORY_SYSTEM_DICTIONARY ) &&
         ( aTBSType != SMI_MEMORY_SYSTEM_DATA ) )
    {
        // SM�� ���� Tablespace Type�� Disk Tablespace�̸� ���� 
        IDE_RAISE( ERR_INVALID_ALTER_ON_MEM_TBS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_INVALID_ALTER_ON_MEM_TBS)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDT_INVALID_ALTER_ON_MEM_TBS));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdtAlter::validateAddFile(qcStatement * aStatement)
{
/***********************************************************************
 *
 * Description :
 *    ALTER TABLESPACE ... ADD ...FILE �� validation ����
 *
 * Implementation :
 *    1. ���� �˻� qdpPrivilege::checkDDLAlterTableSpacePriv()
 *    2. ���̺����̽� �̸� ���� ���� �˻�
 *    3. file specification validation �Լ� ȣ�� (CREATE TABLESPCE �� ����)
 *
 ***********************************************************************/

#define IDE_FN "qdtAlter::validateAddFile"

    qdAlterTBSParseTree   * sParseTree;
    UInt                    sDataFileCnt;
    ULong                   sExtentSize;
    smiTableSpaceAttr       sTBSAttr;

    qdTBSFilesSpec        * sFilesSpec;
    smiDataFileAttr       * sFileAttr;
    idBool                  sFileExist = ID_FALSE;
    SChar                   sFileName[SMI_MAX_DATAFILE_NAME_LEN + 1];

    sParseTree = (qdAlterTBSParseTree *)aStatement->myPlan->parseTree;

    // ���� �˻�
    IDE_TEST( qdpRole::checkDDLAlterTableSpacePriv(
                  aStatement,
                  QCG_GET_SESSION_USER_ID( aStatement ) )
              != IDE_SUCCESS );

    // ������ tablespace name�� �����ϴ��� �˻� 
    IDE_TEST ( qcmTablespace::getTBSAttrByName(
        aStatement,
        sParseTree->TBSAttr->mName,
        sParseTree->TBSAttr->mNameLength,
        & sTBSAttr ) != IDE_SUCCESS );

    // To Fix BUG-10414
    // add ��� file�� ����� table space�� ���缺 �˻�
    // Parse�ܰ迡���� USER_DATE Ȥ�� USER_TEMP ���� �ϳ��� ������
    // isDiskTBSType()�� USER_DATA Ȥ�� USER_TEMP�� �ϳ��� ����.
    // ���� ���� ��Ȯ�� Ÿ���� �����ϸ� �ȵ�
    IDE_TEST( isDiskTBSType(  sParseTree->TBSAttr->mType,
                              sTBSAttr.mType )
              != IDE_SUCCESS );

    // ��⼭ ���� ��Ȯ�� TBS Ÿ�� �����Ͽ��� ��
    sParseTree->TBSAttr->mID = sTBSAttr.mID;
    sParseTree->TBSAttr->mType = sTBSAttr.mType;

    // To Fix PR-10780
    // Extent Size�� ���
    sExtentSize = (ULong) sTBSAttr.mDiskAttr.mExtPageCount *
        smiGetPageSize(sParseTree->TBSAttr->mType);

    // datafile�� ũ�� ���� �� auto extend�� ���缺 �˻�  
    IDE_TEST( qdtCommon::validateFilesSpec( aStatement,
                                            sParseTree->TBSAttr->mType,
                                            sParseTree->diskFilesSpec,
                                            sExtentSize,
                                            & sDataFileCnt) != IDE_SUCCESS );

    // validate reuse
    for ( sFilesSpec = sParseTree->diskFilesSpec;
          sFilesSpec != NULL;
          sFilesSpec = sFilesSpec->next )
    {
        sFileAttr = sFilesSpec->fileAttr;

        IDE_TEST( smiTableSpace::getAbsPath( sFileAttr->mName,
                                             sFileName,
                                             SMI_TBS_DISK)
                  != IDE_SUCCESS );

        // Data File Name�� �����ϴ� �� �˻�
        IDE_TEST( qcmTablespace::existDataFileInDB( sFileName,
                                                    idlOS::strlen(sFileName),
                                                    & sFileExist )
                  != IDE_SUCCESS );

        if ( sFileExist == ID_TRUE )
        {
            //------------------------------------------
            // �����ϴ� File Name��.
            //------------------------------------------

            // REUSE option�� ���
            // �ڽ��� TBS�� �����ϴ� file�̶�� ����� �� �ִ�.
            if ( sFileAttr->mCreateMode == SMI_DATAFILE_REUSE )
            {
                // �ڽ��� TBS�� �����ϴ� File Name�� ���
                // REUSE �̹Ƿ� ����� �� �ִ�.
                IDE_TEST ( qcmTablespace::existDataFileInTBS(
                    sTBSAttr.mID,
                    sFileName,
                    idlOS::strlen(sFileName),
                    & sFileExist )
                           != IDE_SUCCESS );

                IDE_TEST_RAISE( sFileExist == ID_FALSE, ERR_EXIST_FILE );
            }
            else
            {
                // �̹� �����ϴ� File��
                IDE_RAISE( ERR_EXIST_FILE );
            }
        }
        else
        {
            // �������� �ʴ� File Name��.
            // Nothing To Do
        }
    }

    // To Fix BUG-10498
    sParseTree->fileCount = sDataFileCnt;

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_EXIST_FILE)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDT_EXIST_FILE));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qdtAlter::validateRenameOrDropFile(qcStatement * aStatement)
{
/***********************************************************************
 *
 * Description :
 *    ALTER TABLESPACE ... RENAME/DROP ...FILE �� validation ����
 *
 * Implementation :
 *    1. ���� �˻� qdpPrivilege::checkDDLAlterTableSpacePriv()
 *    2. ���̺����̽� �̸� ���� ���� �˻�
 *    3. ������ DATAFILE �� ����� ��� temporary tablespace �̸� ����
 *       ������ TEMPFILE �� ����� ��� data tablespace �̸� ����
 *    4. file �� �����ϴ��� üũ
 *
 ***********************************************************************/

#define IDE_FN "qdtAlter::validateRenameOrDropFile"

    qdAlterTBSParseTree   * sParseTree;
    smiTableSpaceAttr       sTBSAttr;

    qdTBSFileNames         *sTmpFiles;
    idBool                  sFileExist;
    SChar                   sRelName[SMI_MAX_DATAFILE_NAME_LEN + 1];
    SChar                   sFileName[SMI_MAX_DATAFILE_NAME_LEN + 1];

    sParseTree = (qdAlterTBSParseTree *)aStatement->myPlan->parseTree;

    // ���� �˻�
    IDE_TEST( qdpRole::checkDDLAlterTableSpacePriv(
                  aStatement,
                  QCG_GET_SESSION_USER_ID( aStatement ) )
              != IDE_SUCCESS );

    IDE_TEST( qcmTablespace::getTBSAttrByName(
                  aStatement,
                  sParseTree->TBSAttr->mName,
                  sParseTree->TBSAttr->mNameLength,
                  &sTBSAttr) != IDE_SUCCESS );

    // To Fix BUG-10414
    // add ��� file�� ����� table space�� ���缺 �˻�
    IDE_TEST( isDiskTBSType(  sParseTree->TBSAttr->mType,
                              sTBSAttr.mType )
              != IDE_SUCCESS );

    for ( sTmpFiles=  sParseTree->newFileNames;
          sTmpFiles != NULL;
          sTmpFiles = sTmpFiles->next )
    {
        idlOS::memcpy( sRelName,
                       sTmpFiles->fileName.stmtText + sTmpFiles->fileName.offset + 1,
                       sTmpFiles->fileName.size - 2 );
        sRelName[sTmpFiles->fileName.size - 2] = 0;

        IDE_TEST( smiTableSpace::getAbsPath(
                    sRelName,
                    sFileName,
                    SMI_TBS_DISK)
                  != IDE_SUCCESS );

        IDE_TEST( qcmTablespace::existDataFileInDB(  sFileName,
                                                     idlOS::strlen(sFileName),
                                                     & sFileExist )
                  != IDE_SUCCESS );

        IDE_TEST_RAISE( sFileExist == ID_TRUE, ERR_EXIST_FILE );
    }

    sParseTree->TBSAttr->mID = sTBSAttr.mID;
    sParseTree->TBSAttr->mType = sTBSAttr.mType;

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_EXIST_FILE)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDT_EXIST_FILE));
    }    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qdtAlter::validateModifyFile(qcStatement * aStatement)
{
/***********************************************************************
 *
 * Description :
 *    ALTER TABLESPACE ... ALTER ...FILE  �� validation ����
 *
 * Implementation :
 *    1. ���� �˻� qdpPrivilege::checkDDLAlterTableSpacePriv()
 *    2. ���̺����̽� �̸� ���� ���� �˻�
 *    3. ������ DATAFILE �� ����� ��� temporary tablespace �̸� ����
 *       ������ TEMPFILE �� ����� ��� data tablespace �̸� ����
 *    4. file �� �����ϴ��� üũ
 *    5. autoextend clause validation �� CREATE TABLESPACE ���� ����
 *
 ***********************************************************************/

#define IDE_FN "qdtAlter::validateModifyFile"

    qdAlterTBSParseTree   * sParseTree;
    smiTableSpaceAttr       sTBSAttr;
    UInt                    sDataFileCnt;
    ULong                   sExtentSize;
    SChar                   sRelName[SMI_MAX_DATAFILE_NAME_LEN+1];

    idBool                  sExist;

    sParseTree = (qdAlterTBSParseTree *)aStatement->myPlan->parseTree;

    // ���� �˻�
    IDE_TEST( qdpRole::checkDDLAlterTableSpacePriv(
                  aStatement,
                  QCG_GET_SESSION_USER_ID( aStatement ) )
              != IDE_SUCCESS );

    IDE_TEST( qcmTablespace::getTBSAttrByName(
        aStatement,
        sParseTree->TBSAttr->mName,
        sParseTree->TBSAttr->mNameLength,
        & sTBSAttr ) != IDE_SUCCESS );

    // To Fix BUG-10414
    // add ��� file�� ����� table space�� ���缺 �˻�
    // Parse�ܰ迡���� USER_DATE Ȥ�� USER_TEMP ���� �ϳ��� ������
    // isDiskTBSType()�� USER_DATA Ȥ�� USER_TEMP�� �ϳ��� ����.
    // ���� ���� ��Ȯ�� Ÿ���� �����ϸ� �ȵ�
    IDE_TEST( isDiskTBSType(  sParseTree->TBSAttr->mType,
                              sTBSAttr.mType )
              != IDE_SUCCESS );

    // ��⼭ ���� ��Ȯ�� TBS Ÿ�� �����Ͽ��� ��
    sParseTree->TBSAttr->mID = sTBSAttr.mID;
    sParseTree->TBSAttr->mType = sTBSAttr.mType;


    idlOS::memset( sRelName, 0, SMI_MAX_DATAFILE_NAME_LEN + 1 );
    idlOS::memcpy( sRelName,
                   sParseTree->diskFilesSpec->fileAttr->mName,
                   sParseTree->diskFilesSpec->fileAttr->mNameLength );

    IDE_TEST( smiTableSpace::getAbsPath(
                        sRelName,
                        sParseTree->diskFilesSpec->fileAttr->mName,
                        SMI_TBS_DISK)
              != IDE_SUCCESS );

    sParseTree->diskFilesSpec->fileAttr->mNameLength =
        idlOS::strlen(sParseTree->diskFilesSpec->fileAttr->mName);

    IDE_TEST( qcmTablespace::existDataFileInTBS(
        sTBSAttr.mID,
        sParseTree->diskFilesSpec->fileAttr->mName,
        sParseTree->diskFilesSpec->fileAttr->mNameLength,
        & sExist )
              != IDE_SUCCESS );
    IDE_TEST_RAISE( sExist == ID_FALSE, ERR_NOT_EXIST_FILE );

    // To Fix PR-10780
    // Extent Size�� ���
    sExtentSize = (ULong) sTBSAttr.mDiskAttr.mExtPageCount *
        smiGetPageSize(sParseTree->TBSAttr->mType);

    IDE_TEST( qdtCommon::validateFilesSpec( aStatement,
                                            sParseTree->TBSAttr->mType,
                                            sParseTree->diskFilesSpec,
                                            sExtentSize,
                                            &sDataFileCnt)
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_NOT_EXIST_FILE)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDT_NOT_EXIST_FILE));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qdtAlter::validateTBSOnlineOrOffline(qcStatement * aStatement)
{
/***********************************************************************
 *
 * Description :
 *    ALTER TABLESPACE ... ONLINE/OFFLINE �� execution ����
 *
 * Implementation :
 *    1. ���� �˻� qdpPrivilege::checkDDLAlterTableSpacePriv()
 *    2. ���̺����̽� �̸� ���� ���� �˻�
 *    3. Alter Online / Offline������ ���̺� �����̽����� Ȯ��
 *
 ***********************************************************************/

    qdAlterTBSParseTree   * sParseTree;
    smiTableSpaceAttr       sTBSAttr;
    qcmTableInfoList      * sTableInfoList = NULL;

    sParseTree = (qdAlterTBSParseTree *)aStatement->myPlan->parseTree;

    // ���� �˻�
    IDE_TEST( qdpRole::checkDDLAlterTableSpacePriv(
                  aStatement,
                  QCG_GET_SESSION_USER_ID( aStatement ) )
              != IDE_SUCCESS );
    
    IDE_TEST( qcmTablespace::getTBSAttrByName(
        aStatement,
        sParseTree->TBSAttr->mName,
        sParseTree->TBSAttr->mNameLength,
        &sTBSAttr) != IDE_SUCCESS );

    IDE_TEST_RAISE( smiTableSpace::isSystemTableSpace( sTBSAttr.mID )
                    == ID_TRUE,
                    ERR_CANNOT_ONOFFLINE );

    IDE_TEST_RAISE( smiTableSpace::isTempTableSpace( sTBSAttr.mID )
                    == ID_TRUE,
                    ERR_CANNOT_ONOFFLINE );

    IDE_TEST_RAISE( smiTableSpace::isVolatileTableSpace( sTBSAttr.mID )
                    == ID_TRUE,
                    ERR_CANNOT_ONOFFLINE );

    // PROJ-1567
    // TBS�� �ִ� ���̺� �Ǵ� ��Ƽ�ǿ� ����ȭ�� �ɷ����� ���
    // TBS�� Online �Ǵ� Offline���� ������ �� ����.

    // ��-��Ƽ�ǵ� ���̺�, ��Ƽ�ǵ� ���̺�
    IDE_TEST( qcmTablespace::findTableInfoListInTBS(
                aStatement,
                sTBSAttr.mID,
                ID_TRUE,
                &sTableInfoList) != IDE_SUCCESS );

    for( ;
         sTableInfoList != NULL;
         sTableInfoList = sTableInfoList->next )
    {
        // if specified tables is replicated, the error
        IDE_TEST_RAISE(sTableInfoList->tableInfo->replicationCount > 0,
                       ERR_DDL_WITH_REPLICATED_TABLE);
        //proj-1608:replicationCount�� 0�� �� recovery count�� �׻� 0�̾�� ��
        IDE_DASSERT(sTableInfoList->tableInfo->replicationRecoveryCount == 0);
    }

    sParseTree->TBSAttr->mID = sTBSAttr.mID;
    sParseTree->TBSAttr->mType = sTBSAttr.mType;

    return IDE_SUCCESS;
    
    IDE_EXCEPTION(ERR_CANNOT_ONOFFLINE)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDT_CANNOT_ONOFFLINE));
    }
    IDE_EXCEPTION(ERR_DDL_WITH_REPLICATED_TABLE)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDB_DDL_WITH_REPLICATED_TBL));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdtAlter::executeTBSOnlineOrOffline(qcStatement * aStatement)
{
/***********************************************************************
 *
 * Description :
 *    ALTER TABLESPACE ... ONLINE/OFFLINE �� execution ����
 *
 * Implementation :
 *    1. smiTableSpace::setStatus() �Լ� ȣ��
 *
 ***********************************************************************/

#define IDE_FN "qdtAlter::executeTBSOnlineOrOffline"

    qdAlterTBSParseTree   * sParseTree;

    sParseTree = (qdAlterTBSParseTree *)aStatement->myPlan->parseTree;

    /*
        �� �ȿ��� Tablespace�� X�� ȹ����
        Drop/Discard�Ǿ����� üũ�Ѵ�.
        QP������ Execute ������ �̷��� ������ ���� üũ�� �ʿ� ����.
     */
    IDE_TEST( smiTableSpace::alterStatus(
                                 aStatement->mStatistics,
                                 (QC_SMI_STMT( aStatement ))->getTrans(),
                                 sParseTree->TBSAttr->mID,
                                 sParseTree->TBSAttr->mTBSStateOnLA)
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qdtAlter::executeAddFile(qcStatement * aStatement)
{
/***********************************************************************
 *
 * Description :
 *    ALTER TABLESPACE ... ADD ...FILE �� execution ����
 *
 * Implementation :
 *    1. smiTableSpace::addDataFile() �Լ� ȣ��
 *
 ***********************************************************************/

#define IDE_FN "qdtAlter::executeAddFile"

    qdAlterTBSParseTree   * sParseTree;
    smiDataFileAttr      ** sDataFileAttr = NULL;
    qdTBSFilesSpec        * sFileSpec;
    UInt                    i;

    sParseTree = (qdAlterTBSParseTree *)aStatement->myPlan->parseTree;

    //------------------------------------------------------------------
    // To Fix BUG-10498
    // SM�� ������ add �� datafile���� attribute �������� �����Ѵ�.
    // ( Parse Tree���� �����ϴ� data file attribute ������
    //   SM���� �䱸�ϴ� data file attribute ������ ���� �ʱ� ������ )
    //
    // < Parse Tree���� �����ϴ� attribute ���� ���� >
    // ----------------------
    // | qdAlterTBSParseTree|
    // ----------------------   ------------------
    // |    fileSpec        |-->| qdTBSFilesSpec |
    // |     ...            |   ------------------   ------------------
    // ---------------------    |  fileAttr------|-->| smiDataFileAttr |
    //                          |  fileNo        |   ------------------
    //                          |  next--|       |   |  ...            |
    //                          ---------|-------    ------------------
    //                                   V
    //                          ------------------
    //                          | qdTBSFilesSpec |
    //                          ------------------    ------------------
    //                          |  fileAttr -----|--> | smiDataFileAttr |
    //                          |  fileNo        |    ------------------
    //                          |  next          |    |  ...            |
    //                          -----------------     ------------------
    //
    // < SM Interface���� �䱸�ϴ� attribute ���� ���� >
    // (smiDataFileAttr*)�� �迭
    //------------------------------------------------------------------
    
    IDU_FIT_POINT( "qdtAlter::executeAddFile::alloc::sDataFileAttr",
                    idERR_ABORT_InsufficientMemory );

    IDE_TEST( aStatement->qmxMem->alloc(
            ID_SIZEOF(smiDataFileAttr*) * sParseTree->fileCount,
            (void **) & sDataFileAttr )
        != IDE_SUCCESS);

    i = 0;
    for ( sFileSpec = sParseTree->diskFilesSpec;
          sFileSpec != NULL;
          sFileSpec = sFileSpec->next )
    {
        sDataFileAttr[i++] = sFileSpec->fileAttr;
    }
    
    IDE_TEST(
        smiTableSpace::addDataFile( aStatement->mStatistics,
                                    (QC_SMI_STMT( aStatement))->getTrans(),
                                    sParseTree->TBSAttr->mID,
                                    sDataFileAttr,
                                    sParseTree->fileCount )
        != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qdtAlter::executeRenameFile(qcStatement * aStatement)
{
/***********************************************************************
 *
 * Description :
 *    ALTER TABLESPACE ... RENAME ...FILE �� execution ����
 *
 * Implementation :
 *    1. smiTableSpace::renameDataFile() �Լ� ȣ��
 *
 ***********************************************************************/

#define IDE_FN "qdtAlter::executeRenameFile"

    qdAlterTBSParseTree   * sParseTree;
    SChar                 * sSqlStr;
    SChar                 * sOldFileName;
    SChar                 * sNewFileName;
    qdTBSFileNames        * sOldFileNames;
    qdTBSFileNames        * sNewFileNames;
    smiTableSpaceAttr       sTBSAttr;
    qdTBSFileNames         *sTmpFiles;
    idBool                  sFileExist;
    SChar                   sRelName[SMI_MAX_DATAFILE_NAME_LEN + 1];
    SChar                   sFileName[SMI_MAX_DATAFILE_NAME_LEN + 1];

    sParseTree = (qdAlterTBSParseTree *)aStatement->myPlan->parseTree;


    IDE_TEST( qcmTablespace::getTBSAttrByName(
        aStatement,
        sParseTree->TBSAttr->mName,
        sParseTree->TBSAttr->mNameLength,
        &sTBSAttr) != IDE_SUCCESS );

    // To Fix BUG-10414
    // add ��� file�� ����� table space�� ���缺 �˻�
    IDE_TEST( isDiskTBSType(  sParseTree->TBSAttr->mType,
                              sTBSAttr.mType )
              != IDE_SUCCESS );

    for ( sTmpFiles=  sParseTree->newFileNames;
          sTmpFiles != NULL;
          sTmpFiles = sTmpFiles->next )
    {
        idlOS::memcpy( sRelName,
                       sTmpFiles->fileName.stmtText + sTmpFiles->fileName.offset + 1,
                       sTmpFiles->fileName.size - 2 );
        sRelName[sTmpFiles->fileName.size - 2] = 0;

        IDE_TEST( smiTableSpace::getAbsPath(
                    sRelName,
                    sFileName,
                    SMI_TBS_DISK )
                  != IDE_SUCCESS );

        IDE_TEST( qcmTablespace::existDataFileInDB(  sFileName,
                                                     idlOS::strlen(sFileName),
                                                     & sFileExist )
                  != IDE_SUCCESS );

        IDE_TEST_RAISE( sFileExist == ID_TRUE, ERR_EXIST_FILE );
    }

    sParseTree->TBSAttr->mID = sTBSAttr.mID;
    sParseTree->TBSAttr->mType = sTBSAttr.mType;

    IDU_FIT_POINT( "qdtAlter::executeRenameFile::alloc::sOldFileName",
                    idERR_ABORT_InsufficientMemory );

    IDE_TEST(STRUCT_ALLOC_WITH_SIZE(aStatement->qmxMem,
                                    SChar,
                                    SMI_MAX_DATAFILE_NAME_LEN + 1,
                                    &sOldFileName)
             != IDE_SUCCESS);

    IDU_FIT_POINT( "qdtAlter::executeRenameFile::alloc::sNewFileName",
                    idERR_ABORT_InsufficientMemory );

    IDE_TEST(STRUCT_ALLOC_WITH_SIZE(aStatement->qmxMem,
                                    SChar,
                                    SMI_MAX_DATAFILE_NAME_LEN + 1,
                                    &sNewFileName)
             != IDE_SUCCESS);

    IDU_FIT_POINT( "qdtAlter::executeRenameFile::alloc::sSqlStr",
                    idERR_ABORT_InsufficientMemory );

    IDE_TEST(STRUCT_ALLOC_WITH_SIZE(aStatement->qmxMem,
                                    SChar,
                                    QD_MAX_SQL_LENGTH,
                                    &sSqlStr)
             != IDE_SUCCESS);

    sOldFileNames = sParseTree->oldFileNames;
    sNewFileNames = sParseTree->newFileNames;

    while (sOldFileNames != NULL)
    {
        idlOS::memcpy((void*) sOldFileName,
                      sOldFileNames->fileName.stmtText + sOldFileNames->fileName.offset + 1,
                      sOldFileNames->fileName.size - 2);
        sOldFileName[ sOldFileNames->fileName.size - 2 ] = '\0';

        idlOS::memcpy((void*) sNewFileName,
                      sNewFileNames->fileName.stmtText + sNewFileNames->fileName.offset + 1,
                      sNewFileNames->fileName.size - 2);
        sNewFileName[ sNewFileNames->fileName.size - 2 ] = '\0';
        
        IDE_TEST( smiTableSpace::renameDataFile(
                          sParseTree->TBSAttr->mID,
                          sOldFileName, sNewFileName )
                  != IDE_SUCCESS );

        sOldFileNames = sOldFileNames->next;
        sNewFileNames = sNewFileNames->next;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_EXIST_FILE)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDT_EXIST_FILE));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qdtAlter::executeModifyFileAutoExtend(qcStatement * aStatement)
{
/***********************************************************************
 *
 * Description :
 *    ALTER TABLESPACE ... ALTER ...FILE AUTOEXTEND ... �� execution ����
 *
 * Implementation :
 *    1. ���� �Ӽ��� ���� smiTableSpace �Լ� ȣ��
 *      1.1 autoextend �Ӽ� �����
 *          smiTableSpace::alterDataFileAutoExtend
 *
 ***********************************************************************/

#define IDE_FN "qdtAlter::executeModifyFileAutoExtend"

    qdAlterTBSParseTree   * sParseTree;
    SChar                 * sFileName;
    smiDataFileAttr       * sFileAttr;
    SChar                   sValidFileName[SMI_MAX_DATAFILE_NAME_LEN+1];

    sParseTree = (qdAlterTBSParseTree *)aStatement->myPlan->parseTree;
    sFileAttr  = sParseTree->diskFilesSpec->fileAttr;

    IDU_FIT_POINT( "qdtAlter::executeModifyFileAutoExtend::alloc::sFileName",
                    idERR_ABORT_InsufficientMemory );

    IDE_TEST(STRUCT_ALLOC_WITH_SIZE(aStatement->qmxMem,
                                    SChar,
                                    sFileAttr->mNameLength + 1,
                                    &sFileName)
             != IDE_SUCCESS);

    idlOS::memcpy( (void*) sFileName,
                   (void*) sFileAttr->mName,
                   sFileAttr->mNameLength );
    sFileName[ sFileAttr->mNameLength ] = '\0';

    idlOS::memset(sValidFileName, 0x00, SMI_MAX_DATAFILE_NAME_LEN+1);
    IDE_TEST( smiTableSpace::alterDataFileAutoExtend(
                  (QC_SMI_STMT( aStatement ))->getTrans(),
                  sParseTree->TBSAttr->mID,
                  sFileName,
                  sFileAttr->mIsAutoExtend,
                  sFileAttr->mNextSize,
                  sFileAttr->mMaxSize,
                  (SChar*)sValidFileName)
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qdtAlter::executeModifyFileSize(qcStatement * aStatement)
{
/***********************************************************************
 *
 * Description :
 *    ALTER TABLESPACE ... ALTER ...FILE AUTOEXTEND ... �� execution ����
 *
 * Implementation :
 *    1. ���� �Ӽ��� ���� smiTableSpace �Լ� ȣ��
 *      1.2 SIZE �Ӽ� �����
 *          smiTableSpace::resizeDataFile
 *
 ***********************************************************************/

#define IDE_FN "qdtAlter::executeModifyFileSize"

    qdAlterTBSParseTree   * sParseTree;
    SChar                 * sFileName;
    smiDataFileAttr       * sFileAttr;
    SChar                   sValidFileName[SMI_MAX_DATAFILE_NAME_LEN+1];
    ULong                   sSizeChanged;
    

    sParseTree = (qdAlterTBSParseTree *)aStatement->myPlan->parseTree;
    sFileAttr  = sParseTree->diskFilesSpec->fileAttr;

    IDU_FIT_POINT( "qdtAlter::executeModifyFileSize::alloc::sFileName",
                    idERR_ABORT_InsufficientMemory );

    IDE_TEST(STRUCT_ALLOC_WITH_SIZE(aStatement->qmxMem,
                                    SChar,
                                    sFileAttr->mNameLength + 1,
                                    &sFileName)
             != IDE_SUCCESS);

    idlOS::memcpy( (void*) sFileName,
                   (void*) sFileAttr->mName,
                   sFileAttr->mNameLength );
    sFileName[ sFileAttr->mNameLength ] = '\0';

    idlOS::memset(sValidFileName, 0x00, SMI_MAX_DATAFILE_NAME_LEN+1);
    
    IDE_TEST( smiTableSpace::resizeDataFile(
                  aStatement->mStatistics,
                  (QC_SMI_STMT( aStatement ))->getTrans(),
                  sParseTree->TBSAttr->mID,
                  sFileName,
                  sFileAttr->mInitSize,
                  &sSizeChanged,
                  (SChar*)sValidFileName)
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qdtAlter::executeModifyFileOnOffLine(qcStatement * aStatement)
{
/***********************************************************************
 *
 * Description : PROJ-1548-M3-1
 *    ALTER TABLESPACE ... ALTER ...FILE AUTOEXTEND ... �� execution ����
 *
 * Implementation :
 *    1. ���� �Ӽ��� ���� smiTableSpace �Լ� ȣ��
 *      1.3 FILE �� ONLINE/OFFLINE �Ӽ� �����
 *          smiTableSpace::alterDataFileOnLineMode
 *
 ***********************************************************************/

#define IDE_FN "qdtAlter::executeModifyFileOnOffLine"

    qdAlterTBSParseTree   * sParseTree;
    SChar                 * sFileName;
    smiDataFileAttr       * sFileAttr;
    smiTableSpaceAttr       sTBSAttr;
    SChar                   sRelName[SMI_MAX_DATAFILE_NAME_LEN+1];
    idBool                  sExist;
    
    sParseTree = (qdAlterTBSParseTree *)aStatement->myPlan->parseTree;
    sFileAttr  = sParseTree->diskFilesSpec->fileAttr;
    
    IDE_TEST( qcmTablespace::getTBSAttrByName(
                  aStatement,
                  sParseTree->TBSAttr->mName,
                  sParseTree->TBSAttr->mNameLength,
                  & sTBSAttr ) != IDE_SUCCESS );

    // To Fix BUG-10414
    // add ��� file�� ����� table space�� ���缺 �˻�
    IDE_TEST( isDiskTBSType(  sParseTree->TBSAttr->mType,
                              sTBSAttr.mType )
              != IDE_SUCCESS );
    
    idlOS::memset( sRelName, 0, SMI_MAX_DATAFILE_NAME_LEN + 1 );
    idlOS::memcpy( sRelName,
                   sParseTree->diskFilesSpec->fileAttr->mName,
                   sParseTree->diskFilesSpec->fileAttr->mNameLength );

    IDE_TEST( smiTableSpace::getAbsPath(
                        sRelName,
                        sParseTree->diskFilesSpec->fileAttr->mName,
                        SMI_TBS_DISK )
              != IDE_SUCCESS );

    sParseTree->diskFilesSpec->fileAttr->mNameLength =
        idlOS::strlen(sParseTree->diskFilesSpec->fileAttr->mName);

    IDE_TEST( qcmTablespace::existDataFileInTBS(
        sTBSAttr.mID,
        sParseTree->diskFilesSpec->fileAttr->mName,
        sParseTree->diskFilesSpec->fileAttr->mNameLength,
        & sExist )
              != IDE_SUCCESS );
    IDE_TEST_RAISE( sExist == ID_FALSE, ERR_NOT_EXIST_FILE );

    // To Fix PR-10780
    // Extent Size�� ���

    sParseTree->TBSAttr->mID = sTBSAttr.mID;
    sParseTree->TBSAttr->mType = sTBSAttr.mType;

    IDU_LIMITPOINT("qdtAlter::executeModifyFileOnOffLine::malloc");
    IDE_TEST(STRUCT_ALLOC_WITH_SIZE(aStatement->qmxMem,
                                    SChar,
                                    sFileAttr->mNameLength + 1,
                                    &sFileName)
             != IDE_SUCCESS);

    idlOS::memcpy( (void*) sFileName,
                   (void*) sFileAttr->mName,
                   sFileAttr->mNameLength );
    sFileName[ sFileAttr->mNameLength ] = '\0';
    
    IDE_TEST( smiTableSpace::alterDataFileOnLineMode(
                          sParseTree->TBSAttr->mID,
                          sFileName,
                          sFileAttr->mState )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_NOT_EXIST_FILE)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDT_NOT_EXIST_FILE));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qdtAlter::executeDropFile(qcStatement * aStatement)
{
/***********************************************************************
 *
 * Description :
 *    ALTER TABLESPACE ... DROP ...FILE �� execution ����
 *
 * Implementation :
 *    1. smiTableSpace::removeDataFile() �Լ� ȣ��
 *
 ***********************************************************************/

#define IDE_FN "qdtAlter::executeDropFile"

    qdAlterTBSParseTree   * sParseTree;
    SChar                 * sSqlStr;
    SChar                 * sInputFileName;
    qdTBSFileNames        * sFileNames;
    UInt                    sRemoveCnt;
    SChar                   sValidFileName[SMI_MAX_DATAFILE_NAME_LEN+1];

    sParseTree = (qdAlterTBSParseTree *)aStatement->myPlan->parseTree;

    IDU_FIT_POINT( "qdtAlter::executeDropFile::alloc::sInputFileName",
                    idERR_ABORT_InsufficientMemory );

    IDE_TEST(STRUCT_ALLOC_WITH_SIZE(aStatement->qmxMem,
                                    SChar,
                                    SMI_MAX_DATAFILE_NAME_LEN + 1,
                                    &sInputFileName)
             != IDE_SUCCESS);

    IDU_FIT_POINT( "qdtAlter::executeDropFile::alloc::sSqlStr",
                    idERR_ABORT_InsufficientMemory );

    IDE_TEST(STRUCT_ALLOC_WITH_SIZE(aStatement->qmxMem,
                                    SChar,
                                    QD_MAX_SQL_LENGTH,
                                    &sSqlStr)
             != IDE_SUCCESS);

    sRemoveCnt = 0;
    sFileNames = sParseTree->oldFileNames;

    while (sFileNames != NULL)
    {
        idlOS::memcpy((void*) sInputFileName,
                      sFileNames->fileName.stmtText + sFileNames->fileName.offset + 1,
                      sFileNames->fileName.size - 2);
        sInputFileName[ sFileNames->fileName.size - 2 ] = '\0';

        idlOS::memset(sValidFileName, 0x00, SMI_MAX_DATAFILE_NAME_LEN+1);
        IDE_TEST( smiTableSpace::removeDataFile(
                      (QC_SMI_STMT( aStatement ))->getTrans(),
                      sParseTree->TBSAttr->mID,
                      sInputFileName,
                      (SChar*)sValidFileName)
                  != IDE_SUCCESS );

        sRemoveCnt++;
        sFileNames = sFileNames->next;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qdtAlter::executeTBSBackup( qcStatement * aStatement )
{
/***********************************************************************
 *
 * Description : To fix BUG-11952
 *    ALTER TABLESPACE [tbs_name] [BEGIN|END] BACKUP �� execution ����
 *
 * Implementation :
 *
 *
 ***********************************************************************/
#define IDE_FN "IDE_RC qdtAlter::executeTBSBackup"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(IDE_FN));

    qdAlterTBSParseTree   * sParseTree;
    smiTableSpaceAttr       sTBSAttr;

    sParseTree = (qdAlterTBSParseTree *)aStatement->myPlan->parseTree;

    // check privileges (sysdba�� ����)
    IDE_TEST_RAISE( QCG_GET_SESSION_USER_IS_SYSDBA(aStatement) != ID_TRUE,
                    ERR_NO_GRANT_SYSDBA );
    
    IDE_TEST( qcmTablespace::getTBSAttrByName(
                  aStatement,
                  sParseTree->TBSAttr->mName,
                  sParseTree->TBSAttr->mNameLength,
                  &sTBSAttr) != IDE_SUCCESS );

    // TODO: sm interface�� ����Ͽ� �����ؾ� ��
    switch( sParseTree->backupState )
    {
        case QD_TBS_BACKUP_BEGIN:
        {
            // code �߰�
            IDE_TEST(smiBackup::beginBackupTBS( sTBSAttr.mID )
                     != IDE_SUCCESS);
            break;
            
        }
        case QD_TBS_BACKUP_END:    
        {
            // code �߰�
            IDE_TEST(smiBackup::endBackupTBS( sTBSAttr.mID )
                     != IDE_SUCCESS);
            break;
        }
        default:
            IDE_DASSERT(0);
            break;
    }
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION(ERR_NO_GRANT_SYSDBA);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDP_INSUFFICIENT_PRIVILEGES,
                                QCM_PRIV_NAME_SYSTEM_SYSDBA_STR));
    }
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qdtAlter::executeAlterMemoryTBSChkptPath( qcStatement * aStatement )
{
/***********************************************************************
 *
 * Description : PROJ-1548-M3-1
 *    ALTER TABLESPACE TBSNAME ADD/RENAME/DROP CHECKPOINT PATH ...
 *    �� ����
 *
 * ���� �� �ִ� ���� ����:
 *    - ALTER TABLESPACE TBSNAME ADD CHECKPOINT PATH '/path/to/dir';
 *    - ALTER TABLESPACE TBSNAME RENAME CHECKPOINT PATH
 *         '/path/to/old' to '/path/to/new';
 *    - ALTER TABLESPACE TBSNAME DROP CHECKPOINT PATH '/path/to/dir';
 *
 * Implementation :
 *   (1) Tablespace�̸� => Tablespace ID��ȸ, �����ϴ� Tablespace���� üũ
 *   (2) ADD/RENAME/DROP CHECKPOINT PATH ���� 
 *
 ***********************************************************************/

    qdAlterTBSParseTree   * sParseTree;
    smiTableSpaceAttr       sTBSAttr;
    qdAlterChkptPath      * sAlterCPathClause;
    scSpaceID               sSpaceID;
    SChar                 * sFromChkptPath = NULL;
    SChar                 * sToChkptPath = NULL;
    smiChkptPathAttr      * sFromChkptPathAttr;
    smiChkptPathAttr      * sToChkptPathAttr;

    sParseTree        = (qdAlterTBSParseTree *)aStatement->myPlan->parseTree;
    sAlterCPathClause = sParseTree->memAlterChkptPath;

    // tablespace name ���� ���� �˻� 
    IDE_TEST( qcmTablespace::getTBSAttrByName(
        aStatement,
        sParseTree->TBSAttr->mName,
        sParseTree->TBSAttr->mNameLength,
        & sTBSAttr ) != IDE_SUCCESS );

    // ALTER�Ϸ��� Tablespace�� Memory Tablespace���� �˻� 
    IDE_TEST( isMemTBSType( sTBSAttr.mType )
              != IDE_SUCCESS );
    
    // Tablespace type�� ���Ѵ�.
    sParseTree->TBSAttr->mID = sTBSAttr.mID;
    sParseTree->TBSAttr->mType = sTBSAttr.mType;

    // ADD/RENAME/DROP CHECKPOINT PATH ���� 
    sSpaceID = sParseTree->TBSAttr->mID ;

    sFromChkptPathAttr = sAlterCPathClause->fromChkptPathAttr;
    sToChkptPathAttr   = sAlterCPathClause->toChkptPathAttr;

    sFromChkptPath = sFromChkptPathAttr->mChkptPath; 

    // Memory TBS�� getAbsPath�� ���ڸ� IN/OUT���� ����ϱ� ����
    // ������ �ּҸ� IN/OUT���� ���� �Ѱ��ش�.
    IDE_TEST( smiTableSpace::getAbsPath( sFromChkptPath,
                                         sFromChkptPath,
                                         SMI_TBS_MEMORY )
              != IDE_SUCCESS );

    // BUG-29812
    // Memory TBS�� Checkpoint Path�� �����η� ��ȯ�Ѵ�.
    if( sToChkptPathAttr != NULL )
    {
        sToChkptPath = sToChkptPathAttr->mChkptPath; 

        // Memory TBS�� getAbsPath�� ���ڸ� IN/OUT���� ����ϱ� ����
        // ������ �ּҸ� IN/OUT���� ���� �Ѱ��ش�.
        IDE_TEST( smiTableSpace::getAbsPath( sToChkptPath,
                                             sToChkptPath,
                                             SMI_TBS_MEMORY )
                  != IDE_SUCCESS );
    }

    switch( sAlterCPathClause->alterOp )
    {
        case QD_ADD_CHECKPOINT_PATH :
            IDE_TEST( smiTableSpace::alterMemoryTBSAddChkptPath(
                          sSpaceID,
                          sFromChkptPath )
                      != IDE_SUCCESS );
            break;


        case QD_RENAME_CHECKPOINT_PATH :
            IDE_TEST( smiTableSpace::alterMemoryTBSRenameChkptPath(
                          sSpaceID,
                          sFromChkptPath,
                          sToChkptPath )
                      != IDE_SUCCESS );
            break;
            
        case QD_DROP_CHECKPOINT_PATH :
            IDE_TEST( smiTableSpace::alterMemoryTBSDropChkptPath(
                          sSpaceID,
                          sFromChkptPath )
                      != IDE_SUCCESS );
            break;
    }

    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC qdtAlter::executeAlterTBSDiscard( qcStatement * aStatement )
{
/***********************************************************************
 *
 * Description : PROJ-1548-M5-2
 *    ALTER TABLESPACE TBSNAME DISCARD �� ����
 *
 * Implementation :
 *   (1) Tablespace�̸� => Tablespace ID��ȸ, �����ϴ� Tablespace���� üũ
 *   (2) ALTER TABLESPACE TBSNAME DISCARD ����
 *
 ***********************************************************************/

    qdAlterTBSParseTree   * sParseTree;
    smiTableSpaceAttr       sTBSAttr;

    sParseTree        = (qdAlterTBSParseTree *)aStatement->myPlan->parseTree;

    // tablespace name ���� ���� �˻� 
    IDE_TEST( qcmTablespace::getTBSAttrByName(
                  aStatement,
                  sParseTree->TBSAttr->mName,
                  sParseTree->TBSAttr->mNameLength,
                  & sTBSAttr ) != IDE_SUCCESS );

    IDE_TEST_RAISE( sTBSAttr.mType != SMI_MEMORY_USER_DATA &&
                    sTBSAttr.mType != SMI_DISK_USER_DATA &&
                    sTBSAttr.mType != SMI_DISK_USER_TEMP,
                    ERR_CANNOT_DISCARD );
    
    sParseTree->TBSAttr->mID = sTBSAttr.mID;

    // ALTER TABLESPACE DISCARD ���� 
    IDE_TEST( smiTableSpace::alterDiscard( sParseTree->TBSAttr->mID )
              != IDE_SUCCESS );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_CANNOT_DISCARD)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDT_CANNOT_DISCARD));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdtAlter::validateAlterMemVolTBSAutoExtend(qcStatement * aStatement)
{
/***********************************************************************
 *
 * Description : PROJ-1548-M3-2
 *    ALTER TABLESPACE TBSNAME AUTOEXTEND ... �� validation ����
 *
 * Implementation :
 *    (1) alter tablespace ������ �ִ��� �˻�
 *    (2) ������ tablespace name ���� ���� �˻�
 *    ** ���� �� �Լ��� memory TBS�� ���ؼ��� ����Ǿ�����,
 *       volatile TBS�� ���ؼ��� ����Ǿ�� �ϱ� ������
 *       �Լ� �̸��� MemVol...�� �ٲپ���.
 *       �Ľ� �ܰ迡���� memory TBS���� volatile TBS���� �� �� ������
 *       validation �ܰ��� �� �Լ����� tbs ID�� Ÿ���� ���ͼ�
 *       validation check�� �����Ѵ�. 
 ***********************************************************************/

    qdAlterTBSParseTree   * sParseTree;
    smiTableSpaceAttr       sTBSAttr;

    sParseTree = (qdAlterTBSParseTree *)aStatement->myPlan->parseTree;

    // ���� �˻�
    IDE_TEST( qdpRole::checkDDLAlterTableSpacePriv(
                  aStatement,
                  QCG_GET_SESSION_USER_ID( aStatement ) )
              != IDE_SUCCESS );

    // ������ tablespace name ���� ���� �˻� 
    IDE_TEST( qcmTablespace::getTBSAttrByName(
                  aStatement,
                  sParseTree->TBSAttr->mName,
                  sParseTree->TBSAttr->mNameLength,
                  & sTBSAttr ) != IDE_SUCCESS );

    // BUG-32255
    // Memory system TBS verification is the first. 
    IDE_TEST_RAISE( smiTableSpace::isSystemTableSpace( sTBSAttr.mID )
                    == ID_TRUE,
                    ERR_CANNOT_ALTER_SYSTEM_TABLESPACE );

    IDE_TEST_RAISE( (sTBSAttr.mType != SMI_MEMORY_USER_DATA) &&
                    (sTBSAttr.mType != SMI_VOLATILE_USER_DATA),
                    ERR_NEITHER_VOLATILE_NOR_MEMORY );

    sParseTree->TBSAttr->mID = sTBSAttr.mID;
    sParseTree->TBSAttr->mType = sTBSAttr.mType;
    
    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_NEITHER_VOLATILE_NOR_MEMORY);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDT_INVALID_ALTER_ON_MEM_OR_VOL_TBS));
    }
    IDE_EXCEPTION(ERR_CANNOT_ALTER_SYSTEM_TABLESPACE )
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDT_CANNOT_ALTER_SYSTEM_TABLESPACE));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC qdtAlter::executeAlterMemVolTBSAutoExtend( qcStatement * aStatement )
{
/***********************************************************************
 *
 * Description : PROJ-1548-M3-2
 *    ALTER TABLESPACE TBSNAME AUTOEXTEND ... �� ����
 *
 * ���� �� �ִ� ���� ����:
 *    - ALTER TABLESPACE TBSNAME AUTOEXTEND OFF
 *    - ALTER TABLESPACE TBSNAME AUTOEXTEND ON NEXT 10M
 *    - ALTER TABLESPACE TBSNAME AUTOEXTEND ON MAXSIZE 10M/UNLIMITTED
 *    - ALTER TABLESPACE TBSNAME AUTOEXTEND ON NEXT 10M MAXSIZE 10M/UNLIMITTED
 * 
 * Implementation :
 *
 ***********************************************************************/

    qdAlterTBSParseTree   * sParseTree;
    smiDataFileAttr       * sFileAttr;

    sParseTree = (qdAlterTBSParseTree *)aStatement->myPlan->parseTree;
    sFileAttr  = sParseTree->diskFilesSpec->fileAttr;

    if (sParseTree->TBSAttr->mType == SMI_MEMORY_USER_DATA)
    {
        IDE_TEST( smiTableSpace::alterMemoryTBSAutoExtend(
                          (QC_SMI_STMT( aStatement ))->getTrans(),
                          sParseTree->TBSAttr->mID,
                          sFileAttr->mIsAutoExtend,
                          sFileAttr->mNextSize,
                          sFileAttr->mMaxSize )
                  != IDE_SUCCESS );
    }
    else
    {
        IDE_ASSERT( sParseTree->TBSAttr->mType == SMI_VOLATILE_USER_DATA);

        IDE_TEST( smiTableSpace::alterVolatileTBSAutoExtend(
                          (QC_SMI_STMT( aStatement ))->getTrans(),
                          sParseTree->TBSAttr->mID,
                          sFileAttr->mIsAutoExtend,
                          sFileAttr->mNextSize,
                          sFileAttr->mMaxSize )
                  != IDE_SUCCESS );
    }
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 *
 * Description :
 *   Tablespace�� Attribute Flag ������ ���� Validation�Լ�
 *
 * => ��� Tablespace�� ���������� ����ȴ�.
 *  
 * EX> Alter Tablespace MyTBS Alter LOG_COMPRESS ON;
 *
 * Implementation :
 *    (1) alter tablespace ������ �ִ��� �˻�
 *    (2) ������ tablespace name ���� ���� �˻�
 ***********************************************************************/
IDE_RC qdtAlter::validateAlterTBSAttrFlag(qcStatement * aStatement)
{

    qdAlterTBSParseTree   * sParseTree;
    smiTableSpaceAttr       sTBSAttr;

    sParseTree = (qdAlterTBSParseTree *)aStatement->myPlan->parseTree;

    // ���� �˻�
    IDE_TEST( qdpRole::checkDDLAlterTableSpacePriv(
                  aStatement,
                  QCG_GET_SESSION_USER_ID( aStatement ) )
              != IDE_SUCCESS );

    // ������ tablespace name ���� ���� �˻� 
    IDE_TEST( qcmTablespace::getTBSAttrByName(
                  aStatement,
                  sParseTree->TBSAttr->mName,
                  sParseTree->TBSAttr->mNameLength,
                  & sTBSAttr ) != IDE_SUCCESS );

    sParseTree->TBSAttr->mID = sTBSAttr.mID;
    sParseTree->TBSAttr->mType = sTBSAttr.mType;

    IDE_ASSERT( sParseTree->attrFlagToAlter != NULL );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 *
 * Description :
 *    Tablespace�� Attribute Flag ������ ���� �����Լ�
 *
 * => ��� Tablespace�� ���������� ����ȴ�.
 *
 * EX> Alter Tablespace MyTBS Alter LOG_COMPRESS ON;
 * 
 ***********************************************************************/
IDE_RC qdtAlter::executeAlterTBSAttrFlag( qcStatement * aStatement )
{

    qdAlterTBSParseTree   * sParseTree;

    sParseTree = (qdAlterTBSParseTree *)aStatement->myPlan->parseTree;

    IDE_ASSERT( sParseTree->attrFlagToAlter != NULL );
    
    IDE_TEST( smiTableSpace::alterTBSAttrFlag(
                          (QC_SMI_STMT( aStatement ))->getTrans(),
                          sParseTree->TBSAttr->mID,
                          sParseTree->attrFlagToAlter->attrMask,
                          sParseTree->attrFlagToAlter->attrValue )
              != IDE_SUCCESS );
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description :
 *    Tablespace Rename�� ���� validation
 *
 * EX> ALTER TABLESPACE MyTBS RENAME TO YourTBS;
 *
 *    ����� ���⼭ �ƹ��͵� �� ���� ���� ��� �˻縦 execution�ÿ� �Ѵ�.
 *    ���߿� �ٲ� ���� �����Ƿ� �Լ��� ���ܵ�.
 ***********************************************************************/
IDE_RC qdtAlter::validateAlterTBSRename( qcStatement * /* aStatement */ )
{
    return IDE_SUCCESS;
}

/***********************************************************************
 *
 * Description :
 *    Tablespace Rename�� ���� execution
 *
 * EX> ALTER TABLESPACE MyTBS RENAME TO YourTBS;
 *
 * <<CHECKLIST>>
 * 1.tablespace�����ϴ���
 * 2.system tablespace����(system tablespace�̸� rename �Ұ���)
 * 3.online����(online�� �ƴϸ� rename �Ұ���. sm���� üũ)
 * 4.�ߺ��̸�üũ(�̰� sm���� üũ�� �Ǿ�� ��)
 ***********************************************************************/
IDE_RC qdtAlter::executeAlterTBSRename( qcStatement * aStatement )
{
    qdAlterTBSParseTree   * sParseTree;
    smiTableSpaceAttr       sTBSAttr;

    sParseTree = (qdAlterTBSParseTree *)aStatement->myPlan->parseTree;

    // ���� �˻�
    IDE_TEST( qdpRole::checkDDLAlterTableSpacePriv(
                  aStatement,
                  QCG_GET_SESSION_USER_ID( aStatement ) )
              != IDE_SUCCESS );

    // tablespace�����ؾ� ��.
    IDE_TEST( qcmTablespace::getTBSAttrByName(
                  aStatement,
                  sParseTree->TBSAttr[0].mName,
                  sParseTree->TBSAttr[0].mNameLength,
                  & sTBSAttr ) != IDE_SUCCESS );

    // system tablespace���� �˻�.
    IDE_TEST_RAISE( smiTableSpace::isSystemTableSpace( sTBSAttr.mID )
                    == ID_TRUE,
                    ERR_SYS_TBS );

    /* To Fix BUG-17292 [PROJ-1548] Tablespace DDL�� Tablespace�� X�� ��´�
     * Tablespace�� Lock�� ���� �ʰ� Tablespace���� Table�� ��ȸ�ϰ� �Ǹ�
     * �� ���̿� ���ο� Table�� ���ܳ� �� �ִ�.
     * �̷��� ������ �̿��� �����ϱ� ����
     * Tablespace�� X Lock�� ���� ��� Tablespace Validation/Execution�� ���� */
    IDE_TEST( smiValidateAndLockTBS(
                  QC_SMI_STMT( aStatement ),
                  sTBSAttr.mID,
                  SMI_TBS_LOCK_EXCLUSIVE,
                  SMI_TBSLV_DDL_DML, // TBS Validation �ɼ�
                  smiGetDDLLockTimeOut((QC_SMI_STMT(aStatement))->getTrans()))
                  != IDE_SUCCESS );

    
    // sm���� �ؾ� �� ������ ���̴� ��.
    // online���� offline����(online�̾�߸� ��)
    // �ٲ� �̸��� �̹� �����ϴ���
    // => atomic�ϰ� tablespace �̸��� �ٲ��� �ϹǷ� QP���� üũ�ϱ� �����
    
    /////////////////////////////////////////////////
    // ���⿡�� sm �������̽��� ȣ���ؾ� ��.
    // Error�� �ߺ��̸����� ���� �������� offline��������
    // �ٸ� ������ ���� �������� �����ʿ�.
    /////////////////////////////////////////////////
    
    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_SYS_TBS)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDT_CANNOT_RENAME_SYS_TBS));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


