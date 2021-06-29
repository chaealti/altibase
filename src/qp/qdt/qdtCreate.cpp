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
 * $Id: qdtCreate.cpp 82075 2018-01-17 06:39:52Z jina.kim $
 **********************************************************************/

#include <idl.h>
#include <smiTableSpace.h>
#include <qcm.h>
#include <qcmTableSpace.h>
#include <qdtCommon.h>
#include <qdtCreate.h>
#include <qdpPrivilege.h>
#include <qcg.h>
#include <qdpRole.h>

IDE_RC qdtCreate::validateDiskDataTBS(qcStatement * aStatement)
{
/***********************************************************************
 *
 * Description :
 *    CREATE TABLESPACE ... �� validation ����
 *
 * Implementation :
 *    1. ���� �˻�
 *       qdbPrivilege::checkDDLCreateTableSpacePriv()
 *    2. ����� ���̺����̽����� �����ͺ��̽� ���� �̹� �����ϴ���
 *       ��Ÿ �˻�
 *    3. file specification validation �Լ� ȣ��
 *    4. extent size validation
 *
 ***********************************************************************/

#define IDE_FN "qdtCreate::validateDataTBS"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qdCreateTBSParseTree   * sParseTree;
    smiTableSpaceAttr        sTBSAttr;

    qdTBSFilesSpec        * sFilesSpec;
    smiDataFileAttr       * sFileAttr;
    idBool                  sFileExist;
    SChar                   sFileName[SMI_MAX_DATAFILE_NAME_LEN + 1];
    UInt                    i;
    
    sParseTree = (qdCreateTBSParseTree *)aStatement->myPlan->parseTree;

    // ���� �˻�
    IDE_TEST( qdpRole::checkDDLCreateTableSpacePriv(
                  aStatement,
                  QCG_GET_SESSION_USER_ID( aStatement ) )
              != IDE_SUCCESS );
 
    // ������ tablespace name�� �����ϴ��� �˻�
    IDE_TEST_RAISE( qcmTablespace::getTBSAttrByName(
                        aStatement,
                        sParseTree->TBSAttr->mName,
                        sParseTree->TBSAttr->mNameLength,
                        & sTBSAttr) == IDE_SUCCESS, ERR_DUP_TBS_NAME );

    // PROJ-1579 NCHAR
    // TBS �̸����� ASCII �̿��� ���ڰ� �� �� ����.
    // CONTROL �ܰ迡�� DB CHARSET�� �𸣱� ������
    // ASCII �̿��� ���ڸ� ó���� �� ���� �����̴�.
    for( i = 0; i < sParseTree->TBSAttr->mNameLength; i++ )
    {
        IDE_TEST_RAISE( IDN_IS_ASCII(sParseTree->TBSAttr->mName[i]) == 0, 
                        ERR_NON_ASCII_TBS_NAME );
    }

    // extent size �˻� �� ����
    if ( sParseTree->extentSize == ID_UINT_MAX )
    {
        sParseTree->extentSize =
            smiGetPageSize(sParseTree->TBSAttr->mType) * 8;
    }
    else
    {
        IDE_TEST_RAISE( ( sParseTree->extentSize ) <
                        ( smiGetPageSize(sParseTree->TBSAttr->mType) * 2 ),
                        ERR_INVALID_EXTENTSIZE );
    }

    IDE_TEST( qdtCommon::validateFilesSpec(
                  aStatement,
                  sParseTree->TBSAttr->mType,
                  sParseTree->diskFilesSpec,
                  sParseTree->extentSize,
                  &(sParseTree->fileCount)) != IDE_SUCCESS );

    // validate reuse
    for ( sFilesSpec = sParseTree->diskFilesSpec;
          sFilesSpec != NULL;
          sFilesSpec = sFilesSpec->next )
    {
        sFileAttr = sFilesSpec->fileAttr;
        if ( sFileAttr->mCreateMode == SMI_DATAFILE_REUSE )
        {
            IDE_TEST( smiTableSpace::getAbsPath(
                          sFileAttr->mName,
                          sFileName,
                          SMI_TBS_DISK )
                      != IDE_SUCCESS );

            IDE_TEST(
                qcmTablespace::existDataFileInDB( sFileName,
                                                  idlOS::strlen(sFileName),
                                                  & sFileExist )
                != IDE_SUCCESS );

            IDE_TEST_RAISE( sFileExist == ID_TRUE, ERR_EXIST_FILE );
        }
    }

    QC_SHARED_TMPLATE(aStatement)->smiStatementFlag = SMI_STATEMENT_ALL_CURSOR;

    // Tablespace�� Attribute Flag ���
    IDE_TEST( calculateTBSAttrFlag( aStatement,
                                    sParseTree ) != IDE_SUCCESS );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_DUP_TBS_NAME);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDT_DUPLICATE_TBS_NAME));
    }
    IDE_EXCEPTION(ERR_EXIST_FILE);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDT_EXIST_FILE));
    }
    IDE_EXCEPTION(ERR_INVALID_EXTENTSIZE);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDT_INVALID_EXTENTSIZE));
    }
    IDE_EXCEPTION(ERR_NON_ASCII_TBS_NAME);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDT_NON_ASCII_TBS_NAME));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qdtCreate::validateDiskTemporaryTBS(qcStatement * aStatement)
{
/***********************************************************************
 *
 * Description :
 *    CREATE TEMPORARY TABLESPACE ... �� validation ����
 *
 * Implementation :
 *    1. ���� �˻�
 *       qdbPrivilege::checkDDLCreateTableSpacePriv()
 *    2. ����� ���̺����̽����� �����ͺ��̽� ���� �̹� �����ϴ���
 *       ��Ÿ �˻�
 *    3. file specification validation �Լ� ȣ��
 *    4. extent size validation
 *
 ***********************************************************************/

#define IDE_FN "qdtCreate::validateTemporaryTBS"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qdCreateTBSParseTree   * sParseTree;
    smiTableSpaceAttr        sTBSAttr;

    qdTBSFilesSpec        * sFilesSpec;
    smiDataFileAttr       * sFileAttr;
    idBool                  sFileExist;
    SChar                   sFileName[SMI_MAX_DATAFILE_NAME_LEN + 1];
    UInt                    i;
    
    sParseTree = (qdCreateTBSParseTree *)aStatement->myPlan->parseTree;

    // ���� �˻�
    IDE_TEST( qdpRole::checkDDLCreateTableSpacePriv(
                  aStatement,
                  QCG_GET_SESSION_USER_ID( aStatement ) )
              != IDE_SUCCESS );

    IDE_TEST_RAISE( qcmTablespace::getTBSAttrByName(
                        aStatement,
                        sParseTree->TBSAttr->mName,
                        sParseTree->TBSAttr->mNameLength,
                        &sTBSAttr) == IDE_SUCCESS, ERR_DUP_TBS_NAME );

    // PROJ-1579 NCHAR
    // TBS �̸����� ASCII �̿��� ���ڰ� �� �� ����.
    // CONTROL �ܰ迡�� DB CHARSET�� �𸣱� ������
    // ASCII �̿��� ���ڸ� ó���� �� ���� �����̴�.
    for( i = 0; i < sParseTree->TBSAttr->mNameLength; i++ )
    {
        IDE_TEST_RAISE( IDN_IS_ASCII(sParseTree->TBSAttr->mName[i]) == 0,
                        ERR_NON_ASCII_TBS_NAME );
    }

    sParseTree->TBSAttr->mType = SMI_DISK_USER_TEMP;

    if ( sParseTree->extentSize == ID_UINT_MAX )
    {
        sParseTree->extentSize = smiGetPageSize(sParseTree->TBSAttr->mType) * 8;
    }
    else
    {
        IDE_TEST_RAISE( ( sParseTree->extentSize ) <
                        ( smiGetPageSize(sParseTree->TBSAttr->mType) * 2 ),
                        ERR_INVALID_EXTENTSIZE );
    }

    IDE_TEST( qdtCommon::validateFilesSpec(
                  aStatement,
                  sParseTree->TBSAttr->mType,
                  sParseTree->diskFilesSpec,
                  sParseTree->extentSize,
                  &(sParseTree->fileCount)) != IDE_SUCCESS );

    // validate reuse
    for ( sFilesSpec = sParseTree->diskFilesSpec;
          sFilesSpec != NULL;
          sFilesSpec = sFilesSpec->next )
    {
        sFileAttr = sFilesSpec->fileAttr;
        if ( sFileAttr->mCreateMode == SMI_DATAFILE_REUSE )
        {
            IDE_TEST( smiTableSpace::getAbsPath(
                          sFileAttr->mName,
                          sFileName,
                          SMI_TBS_DISK )
                      != IDE_SUCCESS );

            IDE_TEST(
                qcmTablespace::existDataFileInDB( sFileName,
                                                  idlOS::strlen(sFileName),
                                                  & sFileExist )
                != IDE_SUCCESS );
            IDE_TEST_RAISE( sFileExist == ID_TRUE, ERR_EXIST_FILE );
        }
    }
    
    QC_SHARED_TMPLATE(aStatement)->smiStatementFlag = SMI_STATEMENT_ALL_CURSOR;

    // Tablespace�� Attribute Flag ���
    IDE_TEST( calculateTBSAttrFlag( aStatement,
                                    sParseTree ) != IDE_SUCCESS );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_DUP_TBS_NAME);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDT_DUPLICATE_TBS_NAME));
    }
    IDE_EXCEPTION(ERR_EXIST_FILE);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDT_EXIST_FILE));
    }
    IDE_EXCEPTION(ERR_INVALID_EXTENTSIZE);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDT_INVALID_EXTENTSIZE));
    }
    IDE_EXCEPTION(ERR_NON_ASCII_TBS_NAME);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDT_NON_ASCII_TBS_NAME));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qdtCreate::validateMemoryTBS(qcStatement * aStatement)
{
/***********************************************************************
 *
 * Description :
 *    CREATE TABLESPACE ...memory clause�� validation ����
 *
 * Implementation :
 *    (1) create tablespace ������ �ִ��� �˻�
 *    (2) tablespace name �ߺ� �˻�
 *    (3) checkpoint path �ߺ� �˻�
 *    (4) auto extend ���� ����
 *    (5) attribute validation �ǽ�
 *
 ***********************************************************************/

    qdCreateTBSParseTree  * sParseTree;
    smiTableSpaceAttr       sTBSAttr;
    smiChkptPathAttrList  * sCurChkptPath;
    smiChkptPathAttrList  * sCompareChkptPath;
    UInt                    sTBSNameLen = 0;
    UInt                    i;
    
    sParseTree = (qdCreateTBSParseTree *)aStatement->myPlan->parseTree;

    /* PROJ-2639 Altibase Disk Edition */
#ifdef ALTI_CFG_EDITION_DISK
    if ( QCG_GET_SESSION_USER_ID(aStatement) == QC_SYSTEM_USER_ID )
    {
        /* Nothing to do */
    }
    else
    {
        IDE_RAISE( ERR_NOT_SUPPORT_MEMORY_TABLESPACE_IN_DISK_EDITION );
    }
#endif

    // ���� �˻�
    IDE_TEST( qdpRole::checkDDLCreateTableSpacePriv(
                  aStatement,
                  QCG_GET_SESSION_USER_ID( aStatement ) )
              != IDE_SUCCESS );

    sTBSNameLen = idlOS::strlen( sParseTree->memTBSName );

    // tablespace name �ߺ� �˻�
    // ������ tablespace name�� �����ϴ��� �˻�
    IDE_TEST_RAISE( qcmTablespace::getTBSAttrByName(
                        aStatement,
                        sParseTree->memTBSName,
                        sTBSNameLen,
                        & sTBSAttr) == IDE_SUCCESS, ERR_DUP_TBS_NAME );

    // PROJ-1579 NCHAR
    // TBS �̸����� ASCII �̿��� ���ڰ� �� �� ����.
    // CONTROL �ܰ迡�� DB CHARSET�� �𸣱� ������
    // ASCII �̿��� ���ڸ� ó���� �� ���� �����̴�.
    for( i = 0; i < sTBSNameLen; i++ )
    {
        IDE_TEST_RAISE( IDN_IS_ASCII(sParseTree->memTBSName[i]) == 0,
                        ERR_NON_ASCII_TBS_NAME );
    }

    for ( sCurChkptPath = sParseTree->memChkptPathList;
          sCurChkptPath != NULL;
          sCurChkptPath = sCurChkptPath->mNext )
    {
        // BUG-29812
        // Memory TBS�� Checkpoint Path�� �����η� ��ȯ�Ѵ�.
        //
        // Memory TBS�� getAbsPath�� ���ڸ� IN/OUT���� ����ϱ� ����
        // ������ �ּҸ� IN/OUT���� ���� �Ѱ��ش�.
        IDE_TEST( smiTableSpace::getAbsPath(
                    sCurChkptPath->mCPathAttr.mChkptPath,
                    sCurChkptPath->mCPathAttr.mChkptPath,
                    SMI_TBS_MEMORY )
                  != IDE_SUCCESS );
    }

    // BUGBUG-1548-M2 SM���� QP�� �Ѱ��� Checkpoint Path����ϴ��� üũ.
    // checkpoint path ���� ����
    for ( sCurChkptPath = sParseTree->memChkptPathList;
          sCurChkptPath != NULL;
          sCurChkptPath = sCurChkptPath->mNext )
    {

        for ( sCompareChkptPath = sCurChkptPath->mNext;
              sCompareChkptPath != NULL;
              sCompareChkptPath = sCompareChkptPath->mNext )
        {
            if ( idlOS::strMatch( sCurChkptPath->mCPathAttr.mChkptPath,
                                  idlOS::strlen( sCurChkptPath->mCPathAttr.mChkptPath ),
                                  sCompareChkptPath->mCPathAttr.mChkptPath,
                                  idlOS::strlen( sCompareChkptPath->mCPathAttr.mChkptPath ) ) == 0 )
            {
                // �ߺ� ���� ��ȯ
                IDE_RAISE( ERR_DUP_CHECKPOINT_PATH );
            }
        }
    }
    
    QC_SHARED_TMPLATE(aStatement)->smiStatementFlag = SMI_STATEMENT_ALL_CURSOR;

    // Tablespace�� Attribute Flag ���
    IDE_TEST( calculateTBSAttrFlag( aStatement,
                                    sParseTree ) != IDE_SUCCESS );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_DUP_TBS_NAME);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDT_DUPLICATE_TBS_NAME));
    }
    IDE_EXCEPTION(ERR_DUP_CHECKPOINT_PATH);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDT_DUPLICATE_CHECKPOINT_PATH));
    }
    IDE_EXCEPTION(ERR_NON_ASCII_TBS_NAME);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDT_NON_ASCII_TBS_NAME));
    }
#ifdef ALTI_CFG_EDITION_DISK
    IDE_EXCEPTION( ERR_NOT_SUPPORT_MEMORY_TABLESPACE_IN_DISK_EDITION );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDT_CANNOT_USE_USER_MEMORY_TABLESPACE_IN_DISK_EDITION,
                                  sParseTree->memTBSName ) );
    }
#endif
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdtCreate::validateVolatileTBS(qcStatement * aStatement)
{
/***********************************************************************
 *
 * Description :
 *    CREATE TABLESPACE ...volatile clause�� validation ����
 *
 * Implementation :
 *    (1) create tablespace ������ �ִ��� �˻�
 *    (2) tablespace name �ߺ� �˻�
 *    (3) auto extend ���� ����
 *    (4) attribute validation �ǽ�
 *
 ***********************************************************************/
    qdCreateTBSParseTree  * sParseTree;
    smiTableSpaceAttr       sTBSAttr;
    UInt                    sTBSNameLen = 0;
    UInt                    i;
    
    sParseTree = (qdCreateTBSParseTree *)aStatement->myPlan->parseTree;

    // ���� �˻�
    IDE_TEST( qdpRole::checkDDLCreateTableSpacePriv(
                  aStatement,
                  QCG_GET_SESSION_USER_ID( aStatement ) )
              != IDE_SUCCESS );

    sTBSNameLen = idlOS::strlen( sParseTree->memTBSName );

    // tablespace name �ߺ� �˻�
    // ������ tablespace name�� �����ϴ��� �˻�
    IDE_TEST_RAISE( qcmTablespace::getTBSAttrByName(
                        aStatement,
                        sParseTree->memTBSName,
                        sTBSNameLen,
                        & sTBSAttr) == IDE_SUCCESS, ERR_DUP_TBS_NAME );

    // PROJ-1579 NCHAR
    // TBS �̸����� ASCII �̿��� ���ڰ� �� �� ����.
    // CONTROL �ܰ迡�� DB CHARSET�� �𸣱� ������
    // ASCII �̿��� ���ڸ� ó���� �� ���� �����̴�.
    for( i = 0; i < sTBSNameLen; i++ )
    {
        IDE_TEST_RAISE( IDN_IS_ASCII(sParseTree->memTBSName[i]) == 0,
                        ERR_NON_ASCII_TBS_NAME );
    }

    QC_SHARED_TMPLATE(aStatement)->smiStatementFlag = SMI_STATEMENT_ALL_CURSOR;

    // UNCOMPRESSED/COMPRESSED LOGGING ���� ���� ���� 
    IDE_TEST( checkError4CreateVolatileTBS( sParseTree )
              != IDE_SUCCESS );
    
    // Tablespace�� Attribute Flag ���
    IDE_TEST( calculateTBSAttrFlag( aStatement,
                                    sParseTree ) != IDE_SUCCESS );

    // Volatile Tablespace�� ���
    // Log ���� ���� �ʵ��� �⺻�� ���� 
    sParseTree->attrFlag &= ~SMI_TBS_ATTR_LOG_COMPRESS_MASK;
    sParseTree->attrFlag |= SMI_TBS_ATTR_LOG_COMPRESS_FALSE;
    
    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_DUP_TBS_NAME);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDT_DUPLICATE_TBS_NAME));
    }
    IDE_EXCEPTION(ERR_NON_ASCII_TBS_NAME);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDT_NON_ASCII_TBS_NAME));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/* Volatile Tablespace������ �����ϴ� ����ó��

   => Volatile Tablespace�� ��� Log Compression�������� �ʴ´�.
      Create Volatile Tablespace������ COMPRESSED LOGGING
      ���� ����� ��� ����

   [IN] aAttrFlagList - Tablespace Attribute Flag�� List
*/
IDE_RC qdtCreate::checkError4CreateVolatileTBS(
                      qdCreateTBSParseTree  * aCreateTBS )
{
    IDE_DASSERT( aCreateTBS != NULL );

    qdTBSAttrFlagList * sAttrFlagList = aCreateTBS->attrFlagList;
    
    for ( ; sAttrFlagList != NULL ; sAttrFlagList = sAttrFlagList->next )
    {
        if ( (sAttrFlagList->attrMask & SMI_TBS_ATTR_LOG_COMPRESS_MASK) != 0 )
        {
            if ( (sAttrFlagList->attrValue & SMI_TBS_ATTR_LOG_COMPRESS_MASK )
                 == SMI_TBS_ATTR_LOG_COMPRESS_TRUE )
            {
                IDE_RAISE( err_volatile_tbs_log_compress );
            }
        }
    }

    return IDE_SUCCESS;
    
    IDE_EXCEPTION( err_volatile_tbs_log_compress );
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDT_UNABLE_TO_COMPRESS_VOLATILE_TBS_LOG));
    }
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}


/*  Tablespace�� Attribute Flag List�κ���
    32bit Flag���� ���

    [IN] qcStatement - �������� Statement
    [IN] aCreateTBS - Create Tablespace�� Parse Tree
 */
IDE_RC qdtCreate::calculateTBSAttrFlag( qcStatement          * aStatement,
                                        qdCreateTBSParseTree * aCreateTBS )
{
    
    if ( aCreateTBS->attrFlagList != NULL )
    {
        IDE_TEST( qdtCommon::validateTBSAttrFlagList( aStatement,
                                                      aCreateTBS->attrFlagList )
                  != IDE_SUCCESS );
    
        IDE_TEST( qdtCommon::getTBSAttrFlagFromList( aCreateTBS->attrFlagList,
                                                     & aCreateTBS->attrFlag )
                  != IDE_SUCCESS );
    }
    else
    {
        // Tablespace Attribute List�� �������� ���� ��� 
        // �⺻������ ����
        //
        // Tablespace Attribute���� �⺻������ ���� ���� ����
        // Bit 0�� ����ϵ��� �����ȴ�.
        //
        // �׷��Ƿ�, AttrFlag�� 0���� �����ϸ� �װ��� �ٷ� �⺻���� �ȴ�
        aCreateTBS->attrFlag = 0;
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/*
  ���� ������ ����ڿ��� Ư�� Tablespace ���� ���ٱ����� �ش�.

  [IN] aStatement - ���� �������� Statement
  [IN] aTBSID     - Tablespace ID

  To Fix PR-10589
  TABLESPACE ���� ������ �ִ� �Ϲ� ������
  Tablespace�� ������ ��� �ش� Tablespace�� ���� ���� ������
  �ڵ������� �ο��ؾ� �Ѵ�.
*/

IDE_RC qdtCreate::grantTBSAccess(qcStatement * aStatement,
                                 scSpaceID     aTBSID )
{
    SChar               * sSqlStr;
    vSLong                sRowCnt;

    if ( (QCG_GET_SESSION_USER_ID(aStatement) != QC_SYS_USER_ID)
         &&
         (QCG_GET_SESSION_USER_ID(aStatement) != QC_SYSTEM_USER_ID) )
    {
        // �Ϲ� ������ ���
        IDU_FIT_POINT( "qdtCreate::grantTBSAccess::alloc::sSqlStr",
                        idERR_ABORT_InsufficientMemory );

        IDE_TEST( STRUCT_ALLOC_WITH_SIZE( aStatement->qmxMem,
                                          SChar,
                                          QD_MAX_SQL_LENGTH,
                                          & sSqlStr )
                  != IDE_SUCCESS);
        idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                         "INSERT INTO SYS_TBS_USERS_ VALUES( "
                         "INTEGER'%"ID_INT32_FMT"', "
                         "INTEGER'%"ID_INT32_FMT"', "
                         "INTEGER'%"ID_INT32_FMT"')",
                         (mtdIntegerType) aTBSID,
                         QCG_GET_SESSION_USER_ID(aStatement),
                         1 );

        IDE_TEST(qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                    sSqlStr,
                                    & sRowCnt ) != IDE_SUCCESS);

        IDE_TEST_RAISE(sRowCnt != 1, ERR_META_CRASH);
    }
    else
    {
        // �ý��� ������ ���
        // Nothing To Do
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_META_CRASH)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCM_META_CRASH));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdtCreate::executeDiskDataTBS(qcStatement * aStatement)
{
/***********************************************************************
 *
 * Description :
 *    CREATE TABLESPACE ... �� execution ����
 *
 * Implementation :
 *    1. smiTableSpace::create() �Լ� �Ķ���� �� ���ϱ�
 *    2. ȣ�� smiTableSpace::create
 *
 ***********************************************************************/

    qdCreateTBSParseTree   * sParseTree;

    UInt                     i;
    UInt                     sDataFileCount;
    UInt                     sExtPageCnt;
    qdTBSFilesSpec         * sFilesSpec;
    smiTableSpaceAttr      * sTBSAttr;
    smiDataFileAttr       ** sDataFileAttrList;

    sParseTree = (qdCreateTBSParseTree *)aStatement->myPlan->parseTree;

    sTBSAttr = sParseTree->TBSAttr;
    sDataFileCount = sParseTree->fileCount;
    // To Fix BUG-10378
    sExtPageCnt =
        (UInt) idlOS::floor( ( ID_ULTODB(sParseTree->extentSize) /
                               ID_ULTODB(smiGetPageSize(SMI_DISK_USER_DATA)) )
                             + 0.5 ); // �ݿø�
    sExtPageCnt = ( sExtPageCnt < 1 ) ? 1 : sExtPageCnt;

    IDU_FIT_POINT( "qdtCreate::executeDiskDataTBS::alloc::sDataFileAttrList",
                    idERR_ABORT_InsufficientMemory );

    IDE_TEST( aStatement->qmxMem->alloc( ID_SIZEOF(smiDataFileAttr*) *
                                         sDataFileCount,
                                         (void**)&sDataFileAttrList)
              != IDE_SUCCESS);

    i = 0;
    for ( sFilesSpec = sParseTree->diskFilesSpec;
          sFilesSpec != NULL;
          sFilesSpec = sFilesSpec->next )
    {
        sDataFileAttrList[i++] = sFilesSpec->fileAttr;
    }

    // Disk Tablespace�� ��� Tablespace�� Attribute Flag��
    // smiTableSpaceAttr ����ü�� ���Ͽ� �ѱ��.
    sTBSAttr->mAttrFlag = sParseTree->attrFlag;

    sTBSAttr->mDiskAttr.mSegMgmtType 
              = sParseTree->segMgmtType;

    sTBSAttr->mDiskAttr.mExtMgmtType 
              = sParseTree->extMgmtType;
    
    // tablespace ���� 
    IDE_TEST( smiTableSpace::createDiskTBS(
                  aStatement->mStatistics,
                  (QC_SMI_STMT( aStatement ))->getTrans(),
                  sTBSAttr,
                  sDataFileAttrList,
                  sParseTree->fileCount,
                  sExtPageCnt,
                  sParseTree->extentSize) != IDE_SUCCESS );

    // ����ڰ� ������ tablespace���� ���ٱ����� �ο�
    IDE_TEST( grantTBSAccess( aStatement, sTBSAttr->mID ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdtCreate::executeMemoryTBS(qcStatement * aStatement)
{
/***********************************************************************
 *
 * Description : Create Memory Tablespace
 *
 * Implementation :
 *
 ***********************************************************************/
    qdCreateTBSParseTree   * sParseTree;

    scSpaceID                sCreatedTBSID;


    sParseTree = (qdCreateTBSParseTree *)aStatement->myPlan->parseTree;

    // memory tablespace ����
    IDE_TEST( smiTableSpace::createMemoryTBS(
                  (QC_SMI_STMT( aStatement ))->getTrans(),
                  sParseTree->memTBSName,
                  sParseTree->attrFlag, /* Attribute Flag */
                  sParseTree->memChkptPathList,
                  sParseTree->memSplitFileSize,
                  sParseTree->memInitSize,
                  sParseTree->memIsAutoExtend,
                  sParseTree->memNextSize,
                  sParseTree->memMaxSize,
                  sParseTree->memIsOnline,
                  & sCreatedTBSID )
              != IDE_SUCCESS );


    // ����ڰ� ������ tablespace���� ���ٱ����� �ο�
    IDE_TEST( grantTBSAccess( aStatement, sCreatedTBSID ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdtCreate::executeVolatileTBS(qcStatement * aStatement)
{
/***********************************************************************
 *
 * Description : Create Volatile Tablespace
 *
 * Implementation :
 *
 ***********************************************************************/
    qdCreateTBSParseTree   * sParseTree;

    scSpaceID                sCreatedTBSID;

    sParseTree = (qdCreateTBSParseTree *)aStatement->myPlan->parseTree;

    // memory tablespace ����
    IDE_TEST( smiTableSpace::createVolatileTBS(
                  (QC_SMI_STMT( aStatement ))->getTrans(),
                  sParseTree->memTBSName,
                  sParseTree->attrFlag, /* Attribute Flag */
                  sParseTree->memInitSize,
                  sParseTree->memIsAutoExtend,
                  sParseTree->memNextSize,
                  sParseTree->memMaxSize,
                  // BUGBUG-1548 ����ڰ� ������ ONLINE/OFFLINE���� ����
                  SMI_TBS_ONLINE,
                  & sCreatedTBSID )
              != IDE_SUCCESS );


    // ����ڰ� ������ tablespace���� ���ٱ����� �ο�
    IDE_TEST( grantTBSAccess( aStatement, sCreatedTBSID ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdtCreate::executeDiskTemporaryTBS(qcStatement * aStatement)
{
/***********************************************************************
 *
 * Description :
 *    CREATE TEMPORARY TABLESPACE ... �� execution ����
 *
 * Implementation :
 *    1. smiTableSpace::create() �Լ� �Ķ���� �� ���ϱ�
 *    2. ȣ�� smiTableSpace::create
 *
 ***********************************************************************/

    qdCreateTBSParseTree   * sParseTree;

    UInt                     i;
    UInt                     sDataFileCount;
    UInt                     sExtPageCnt;
    qdTBSFilesSpec         * sFilesSpec;
    smiTableSpaceAttr      * sTBSAttr;
    smiDataFileAttr       ** sDataFileAttrList;

    sParseTree = (qdCreateTBSParseTree *)aStatement->myPlan->parseTree;

    sTBSAttr = sParseTree->TBSAttr;
    sDataFileCount = sParseTree->fileCount;

    // To Fix BUG-10378
    sExtPageCnt = (UInt)
        idlOS::floor( ( ID_ULTODB(sParseTree->extentSize) /
                        ID_ULTODB(smiGetPageSize(SMI_DISK_USER_DATA)) )
                      + 0.5 ); // �ݿø�
    sExtPageCnt = ( sExtPageCnt < 1 ) ? 1 : sExtPageCnt;

    IDU_FIT_POINT( "qdtCreate::executeDiskTemporaryTBS::alloc::sDataFileAttrList",
                    idERR_ABORT_InsufficientMemory );

    IDE_TEST(aStatement->qmxMem->alloc( ID_SIZEOF(smiDataFileAttr*) *
                                        sDataFileCount,
                                        (void**) & sDataFileAttrList )
             != IDE_SUCCESS);

    i = 0;
    for ( sFilesSpec = sParseTree->diskFilesSpec;
          sFilesSpec != NULL;
          sFilesSpec = sFilesSpec->next )
    {
        sDataFileAttrList[i++] = sFilesSpec->fileAttr;
    }

    // Disk Tablespace�� ��� Tablespace�� Attribute Flag��
    // smiTableSpaceAttr ����ü�� ���Ͽ� �ѱ��.
    sTBSAttr->mAttrFlag = sParseTree->attrFlag;
    sTBSAttr->mDiskAttr.mSegMgmtType = sParseTree->segMgmtType;
    sTBSAttr->mDiskAttr.mExtMgmtType
        = sParseTree->extMgmtType;
    
    IDE_TEST( smiTableSpace::createDiskTBS(
                  aStatement->mStatistics,
                  (QC_SMI_STMT( aStatement ))->getTrans(),
                  sTBSAttr,
                  sDataFileAttrList,
                  sParseTree->fileCount,
                  sExtPageCnt,
                  sParseTree->extentSize) != IDE_SUCCESS );

    // ����ڰ� ������ tablespace���� ���ٱ����� �ο�
    IDE_TEST( grantTBSAccess( aStatement, sTBSAttr->mID ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
