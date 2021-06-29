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
 * $Id: qdbComment.cpp 35065 2009-08-25 07:52:03Z junokun $
 **********************************************************************/

#include <idl.h>
#include <ide.h>
#include <qdbAlter.h>
#include <qtc.h>
#include <qcg.h>
#include <qcmCache.h>
#include <qcuSqlSourceInfo.h>
#include <qdpPrivilege.h>
#include <qdbComment.h>
#include <qcmSynonym.h>
#include <qdpRole.h>

const void * gQcmComments;
const void * gQcmCommentsIndex[ QCM_MAX_META_INDICES ];

IDE_RC qdbComment::validateCommentTable(qcStatement * aStatement)
{
/***********************************************************************
 *
 * Description :
 *    COMMENT ON TABLE ... IS ... �� validation ����
 *
 * Implementation :
 *    1. ���̺� ���� ���� üũ (User Name ���� ����)
 *    2. ���̺��� �����ڰ� SYSTEM_ �̸� comment �Ұ���
 *    3. ������ �ִ��� üũ(ALTER object Ȥ�� ALTER ANY TABLE privilege)
 *    4. �ּ� ���� �˻�
 *
 ***********************************************************************/
    qdCommentParseTree  * sParseTree  = NULL;
    idBool                sExist      = ID_FALSE;

    qcmSynonymInfo        sSynonymInfo;
    qcuSqlSourceInfo      sqlInfo;
    UInt                  sObjectType;
    void                * sObjectHandle = NULL;

    UInt                  sTableType;
    UInt                  sSequenceID;

    // Parameter ����
    IDE_DASSERT( aStatement     != NULL );

    sParseTree = (qdCommentParseTree *)aStatement->myPlan->parseTree;

    IDE_TEST(qcmSynonym::resolveObject(aStatement,
                                       sParseTree->userName,
                                       sParseTree->tableName,
                                       &sSynonymInfo,
                                       &sExist,
                                       &sParseTree->userID,
                                       &sObjectType,
                                       &sObjectHandle,
                                       &sSequenceID)
            != IDE_SUCCESS);

    if( sExist == ID_FALSE )
    {
        sqlInfo.setSourceInfo( aStatement, & sParseTree->tableName );
        IDE_RAISE(ERR_NOT_EXIST_OBJECT);
    }
    else
    {
        switch( sObjectType )
        {
            case QCM_OBJECT_TYPE_TABLE:
                // found the table
                sTableType = smiGetTableFlag(sObjectHandle) & SMI_TABLE_TYPE_MASK;
                
                // comment�� ����ڰ� ������ table���� �����ϴ�.
                if( ( sTableType != SMI_TABLE_MEMORY ) &&
                    ( sTableType != SMI_TABLE_DISK ) &&
                    ( sTableType != SMI_TABLE_VOLATILE ) )
                {
                    sqlInfo.setSourceInfo( aStatement, & sParseTree->tableName );
                    IDE_RAISE(ERR_NOT_SUPPORT_OBJ);
                }
                break;
            case QCM_OBJECT_TYPE_SEQUENCE:
                sqlInfo.setSourceInfo( aStatement, & sParseTree->tableName );
                IDE_RAISE(ERR_NOT_SUPPORT_SEQ);
                break;
            case QCM_OBJECT_TYPE_PSM:
                sqlInfo.setSourceInfo( aStatement, & sParseTree->tableName );
                IDE_RAISE(ERR_NOT_SUPPORT_PSM);
                break;
            default:
                sqlInfo.setSourceInfo( aStatement, & sParseTree->tableName );
                IDE_RAISE(ERR_NOT_SUPPORT_OBJ);
                break;
        }
    }

    sParseTree->tableHandle = sObjectHandle;

    IDE_TEST( smiGetTableTempInfo( sParseTree->tableHandle,
                                   (void**)&sParseTree->tableInfo )
              != IDE_SUCCESS );
    
    sParseTree->tableSCN    = smiGetRowSCN( sParseTree->tableHandle );

    // Validation ���� IS Lock
    IDE_TEST( qcm::lockTableForDDLValidation(aStatement,
                                             sParseTree->tableHandle,
                                             sParseTree->tableSCN)
              != IDE_SUCCESS );

    // Operatable Flag �˻�
    if (QCM_IS_OPERATABLE_QP_COMMENT(
            sParseTree->tableInfo->operatableFlag )
        != ID_TRUE)
    {
        sqlInfo.setSourceInfo( aStatement, & sParseTree->tableName );
        IDE_RAISE(ERR_NOT_SUPPORT_OBJ);
    }
    else
    {
        // Nothing to do
    }

    // Alter Privilege �˻�
    IDE_TEST( qdpRole::checkDDLAlterTablePriv( aStatement,
                                               sParseTree->tableInfo )
              != IDE_SUCCESS );
    
    // Literal ���� �˻� (4k)
    IDE_TEST_RAISE( sParseTree->comment.size > QC_MAX_COMMENT_LITERAL_LEN,
                    ERR_COMMENT_TOO_LONG );

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_NOT_EXIST_OBJECT);
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDP_NOT_EXISTS_OBJECT,
                                  sqlInfo.getErrMessage() ) );
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION(ERR_NOT_SUPPORT_SEQ);
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDB_COMMENT_NOT_SUPPORT_SEQUENCE,
                                  sqlInfo.getErrMessage() ) );
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION(ERR_NOT_SUPPORT_PSM);
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDB_COMMENT_NOT_SUPPORT_PSM,
                                  sqlInfo.getErrMessage() ) );
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION(ERR_NOT_SUPPORT_OBJ);
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDB_COMMENT_NOT_SUPPORT_OBJ,
                                  sqlInfo.getErrMessage() ) );
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION(ERR_COMMENT_TOO_LONG)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDB_COMMENT_TOO_LONG));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdbComment::validateCommentColumn(qcStatement * aStatement)
{
/***********************************************************************
 *
 * Description :
 *    COMMENT ON COLUMN ... IS ... �� validation ����
 *
 * Implementation :
 *    1. ���̺� ���� ���� üũ (User Name ���� ����)
 *    2. ���̺��� �����ڰ� SYSTEM_ �̸� comment �Ұ���
 *    3. ������ �ִ��� üũ(ALTER object Ȥ�� ALTER ANY TABLE privilege)
 *    4. �÷� ���� ���� üũ
 *    5. �ּ� ���� �˻�
 *
 ***********************************************************************/
    qdCommentParseTree  * sParseTree;
    idBool                sExist      = ID_FALSE;
    qcmColumn           * sColumn;

    qcmSynonymInfo        sSynonymInfo;
    qcuSqlSourceInfo      sqlInfo;
    UInt                  sObjectType;
    void                * sObjectHandle = NULL;

    UInt                  sTableType;
    UInt                  sSequenceID;

    // Parameter ����
    IDE_DASSERT( aStatement != NULL );

    sParseTree = (qdCommentParseTree *)aStatement->myPlan->parseTree;

    IDE_TEST(qcmSynonym::resolveObject(aStatement,
                                       sParseTree->userName,
                                       sParseTree->tableName,
                                       &sSynonymInfo,
                                       &sExist,
                                       &sParseTree->userID,
                                       &sObjectType,
                                       &sObjectHandle,
                                       &sSequenceID)
            != IDE_SUCCESS);

    if( sExist == ID_FALSE )
    {
        sqlInfo.setSourceInfo( aStatement, & sParseTree->tableName );
        IDE_RAISE(ERR_NOT_EXIST_OBJECT);
    }
    else
    {
        switch( sObjectType )
        {
            case QCM_OBJECT_TYPE_TABLE:
                // found the table
                sTableType = smiGetTableFlag(sObjectHandle) & SMI_TABLE_TYPE_MASK;

                // comment�� ����ڰ� ������ table���� �����ϴ�.
                if( ( sTableType != SMI_TABLE_MEMORY ) &&
                    ( sTableType != SMI_TABLE_DISK ) &&
                    ( sTableType != SMI_TABLE_VOLATILE ) )
                {
                    sqlInfo.setSourceInfo( aStatement, & sParseTree->tableName );
                    IDE_RAISE(ERR_NOT_SUPPORT_OBJ);
                }
                break;
            case QCM_OBJECT_TYPE_SEQUENCE:
                sqlInfo.setSourceInfo( aStatement, & sParseTree->tableName );
                IDE_RAISE(ERR_NOT_SUPPORT_SEQ);
                break;
            case QCM_OBJECT_TYPE_PSM:
                sqlInfo.setSourceInfo( aStatement, & sParseTree->tableName );
                IDE_RAISE(ERR_NOT_SUPPORT_PSM);
                break;
            default:
                sqlInfo.setSourceInfo( aStatement, & sParseTree->tableName );
                IDE_RAISE(ERR_NOT_SUPPORT_OBJ);
                break;
        }
    }

    sParseTree->tableHandle = sObjectHandle;

    IDE_TEST( smiGetTableTempInfo( sParseTree->tableHandle,
                                   (void**)&sParseTree->tableInfo )
              != IDE_SUCCESS );

    sParseTree->tableSCN    = smiGetRowSCN( sParseTree->tableHandle );

    // Validation ���� IS Lock
    IDE_TEST( qcm::lockTableForDDLValidation(aStatement,
                                             sParseTree->tableHandle,
                                             sParseTree->tableSCN)
              != IDE_SUCCESS );

    // Operatable Flag �˻�
    if (QCM_IS_OPERATABLE_QP_COMMENT(
            sParseTree->tableInfo->operatableFlag )
        != ID_TRUE)
    {
        sqlInfo.setSourceInfo( aStatement, & sParseTree->tableName );
        IDE_RAISE(ERR_NOT_SUPPORT_OBJ);
    }
    else
    {
        // Nothing to do
    }

    // Alter Privilege �˻�
    IDE_TEST( qdpRole::checkDDLAlterTablePriv( aStatement,
                                               sParseTree->tableInfo )
              != IDE_SUCCESS );

    // Column ���� ���� �˻�
    IDE_TEST( qcmCache::getColumn( aStatement,
                                   sParseTree->tableInfo,
                                   sParseTree->columnName,
                                   &sColumn )
              != IDE_SUCCESS );

    // Literal ���� �˻� (4000)
    IDE_TEST_RAISE( sParseTree->comment.size > QC_MAX_COMMENT_LITERAL_LEN,
                    ERR_COMMENT_TOO_LONG );

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_NOT_EXIST_OBJECT);
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDP_NOT_EXISTS_OBJECT,
                                  sqlInfo.getErrMessage() ) );
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION(ERR_NOT_SUPPORT_SEQ);
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDB_COMMENT_NOT_SUPPORT_SEQUENCE,
                                  sqlInfo.getErrMessage() ) );
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION(ERR_NOT_SUPPORT_PSM);
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDB_COMMENT_NOT_SUPPORT_PSM,
                                  sqlInfo.getErrMessage() ) );
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION(ERR_NOT_SUPPORT_OBJ);
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDB_COMMENT_NOT_SUPPORT_OBJ,
                                  sqlInfo.getErrMessage() ) );
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION(ERR_COMMENT_TOO_LONG)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDB_COMMENT_TOO_LONG));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdbComment::executeCommentTable(qcStatement * aStatement)
{
/***********************************************************************
 *
 * Description :
 *    COMMENT ON TABLE ... IS ... ����
 *
 * Implementation :
 *    1. ���̺� Lock ȹ�� (IS)
 *    2. Comment�� �̹� �����ϴ��� üũ
 *    3.   -> �����ϸ� Update, �׷��� ������ Insert
 *    4. ��Ÿ�� �ݿ�
 *
 ***********************************************************************/
    qdCommentParseTree  * sParseTree   = NULL;
    SChar               * sSqlStr      = NULL;
    SChar               * sCommentStr  = NULL;
    idBool                sExist       = ID_FALSE;
    vSLong                sRowCnt      = 0;    

    // Parameter ����
    IDE_DASSERT( aStatement     != NULL );

    sParseTree = (qdCommentParseTree *)aStatement->myPlan->parseTree;

    // Table�� Lock ȹ�� (IS)
    IDE_TEST( qcm::validateAndLockTable( aStatement,
                                         sParseTree->tableHandle,
                                         sParseTree->tableSCN,
                                         SMI_TABLE_LOCK_IS )
              != IDE_SUCCESS );    

    // Sql Query ���� �Ҵ�
    IDU_LIMITPOINT("qdbComment::executeCommentTable::malloc1");
    IDE_TEST( STRUCT_ALLOC_WITH_SIZE( aStatement->qmxMem,
                                      SChar,
                                      QD_MAX_SQL_LENGTH,
                                      &sSqlStr )
              != IDE_SUCCESS );

    // Comment ���� �Ҵ�
    IDU_LIMITPOINT("qdbComment::executeCommentTable::malloc2");
    IDE_TEST( STRUCT_ALLOC_WITH_SIZE( aStatement->qmxMem,
                                      SChar,
                                      sParseTree->comment.size+1,
                                      &sCommentStr )
              != IDE_SUCCESS );

    // Comment ���� ����
    QC_STR_COPY( sCommentStr, sParseTree->comment );

    // Comment�� �̹� �����ϴ��� üũ
    IDE_TEST( existCommentTable( aStatement,
                                 sParseTree->tableInfo->tableOwnerName,
                                 sParseTree->tableInfo->name,
                                 &sExist)
              != IDE_SUCCESS );

    if (sExist == ID_TRUE)
    {
        // �̹� �����ϸ� Update
        idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                         "UPDATE SYS_COMMENTS_ "
                         "SET COMMENTS = VARCHAR'%s' "
                         "WHERE USER_NAME = VARCHAR'%s' "
                         "AND TABLE_NAME = VARCHAR'%s' "
                         "AND COLUMN_NAME IS NULL",
                         sCommentStr,
                         sParseTree->tableInfo->tableOwnerName,
                         sParseTree->tableInfo->name );
    }
    else
    {
        // ���� �������� ������ Insert
        idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                         "INSERT INTO SYS_COMMENTS_ "
                         "( USER_NAME, TABLE_NAME, COLUMN_NAME, COMMENTS ) "
                         "VALUES ( VARCHAR'%s', VARCHAR'%s', NULL, VARCHAR'%s' )",
                         sParseTree->tableInfo->tableOwnerName,
                         sParseTree->tableInfo->name,
                         sCommentStr );
    }

    // ��Ÿ�� �ݿ�
    IDE_TEST( qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                 sSqlStr,
                                 &sRowCnt )
              != IDE_SUCCESS );

    IDE_TEST_RAISE( sRowCnt != 1, ERR_META_CRASH );

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_META_CRASH);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDB_META_CRASH));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC qdbComment::executeCommentColumn(qcStatement * aStatement)
{
/***********************************************************************
 *
 * Description :
 *    COMMENT ON COLUMN ... IS ... ����
 *
 * Implementation :
 *    1. ���̺� Lock ȹ�� (IS)
 *    2. Comment�� �̹� �����ϴ��� üũ
 *    3.   -> �����ϸ� Update, �׷��� ������ Insert
 *    4. ��Ÿ�� �ݿ�
 *
 ***********************************************************************/
    qdCommentParseTree  * sParseTree   = NULL;
    SChar               * sSqlStr      = NULL;
    SChar               * sCommentStr  = NULL;
    SChar                 sColumnStr[ QC_MAX_OBJECT_NAME_LEN + 1 ];
    idBool                sExist       = ID_FALSE;
    vSLong                sRowCnt      = 0;

    // Parameter ����
    IDE_DASSERT( aStatement     != NULL );

    sParseTree = (qdCommentParseTree *)aStatement->myPlan->parseTree;

    // Table�� Lock ȹ�� (IS)
    IDE_TEST( qcm::validateAndLockTable( aStatement,
                                         sParseTree->tableHandle,
                                         sParseTree->tableSCN,
                                         SMI_TABLE_LOCK_IS )
              != IDE_SUCCESS );
    
    // Sql Query ���� �Ҵ�
    IDU_LIMITPOINT("qdbComment::executeCommentColumn::malloc1");
    IDE_TEST( STRUCT_ALLOC_WITH_SIZE( aStatement->qmxMem,
                                      SChar,
                                      QD_MAX_SQL_LENGTH,
                                      &sSqlStr )
              != IDE_SUCCESS );

    // Comment ���� �Ҵ�
    IDU_LIMITPOINT("qdbComment::executeCommentColumn::malloc2");
    IDE_TEST( STRUCT_ALLOC_WITH_SIZE( aStatement->qmxMem,
                                      SChar,
                                      sParseTree->comment.size+1,
                                      &sCommentStr )
              != IDE_SUCCESS );

    // Comment ���� ����
    QC_STR_COPY( sCommentStr, sParseTree->comment );

    // Column ���� ����
    QC_STR_COPY( sColumnStr, sParseTree->columnName );

    // Comment�� �̹� �����ϴ��� üũ
    IDE_TEST( existCommentColumn( aStatement,
                                  sParseTree->tableInfo->tableOwnerName,
                                  sParseTree->tableInfo->name,
                                  sColumnStr,
                                  &sExist)
              != IDE_SUCCESS );

    if (sExist == ID_TRUE)
    {
        // �̹� �����ϸ� Update
        idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                         "UPDATE SYS_COMMENTS_ "
                         "SET COMMENTS = VARCHAR'%s' "
                         "WHERE USER_NAME = VARCHAR'%s' "
                         "AND TABLE_NAME = VARCHAR'%s' "
                         "AND COLUMN_NAME = VARCHAR'%s'",
                         sCommentStr,
                         sParseTree->tableInfo->tableOwnerName,
                         sParseTree->tableInfo->name,
                         sColumnStr );
    }
    else
    {
        // ���� �������� ������ Insert
        idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                         "INSERT INTO SYS_COMMENTS_ "
                         "( USER_NAME, TABLE_NAME, COLUMN_NAME, COMMENTS ) "
                         "VALUES ( VARCHAR'%s', VARCHAR'%s', VARCHAR'%s', VARCHAR'%s' )",
                         sParseTree->tableInfo->tableOwnerName,
                         sParseTree->tableInfo->name,
                         sColumnStr,
                         sCommentStr );
    }

    // ��Ÿ�� �ݿ�
    IDE_TEST( qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                 sSqlStr,
                                 &sRowCnt )
              != IDE_SUCCESS );

    IDE_TEST_RAISE( sRowCnt != 1, ERR_META_CRASH );

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_META_CRASH);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDB_META_CRASH));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdbComment::updateCommentTable( qcStatement  * aStatement,
                                       SChar        * aUserName,
                                       SChar        * aOldTableName,
                                       SChar        * aNewTableName)
{
/***********************************************************************
 *
 * Description :
 *    Comment ������ �ƴ� �ٸ� ������ ���� Table(View)�� �̸��� �ٲ���
 *    Comment ��Ÿ�� �ش� Object �̸��� ����
 *
 * Implementation :
 *    1. Comment�� Table �̸��� ����
 *
 ***********************************************************************/
    vSLong              sRowCnt        = 0;
    SChar             * sSqlStr        = NULL;

    // Parameter ����
    IDE_DASSERT( aStatement     != NULL );
    IDE_DASSERT( aUserName      != NULL );
    IDE_DASSERT( aOldTableName  != NULL );
    IDE_DASSERT( aNewTableName  != NULL );
    
    IDE_DASSERT( *aUserName     != '\0' );
    IDE_DASSERT( *aOldTableName != '\0' );
    IDE_DASSERT( *aNewTableName != '\0' );

    // ��Ÿ ����
    IDU_LIMITPOINT("qdbComment::updateCommentTable::malloc");
    IDE_TEST( STRUCT_ALLOC_WITH_SIZE( aStatement->qmxMem,
                                      SChar,
                                      QD_MAX_SQL_LENGTH,
                                      &sSqlStr )
              != IDE_SUCCESS );

    // Table Comment�� Column Comment ��� �����ϴ� ����
    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                     "UPDATE SYS_COMMENTS_ "
                     "SET TABLE_NAME = VARCHAR'%s' "
                     "WHERE USER_NAME = VARCHAR'%s' "
                     "AND TABLE_NAME = VARCHAR'%s' ",
                     aNewTableName,
                     aUserName,
                     aOldTableName );

    IDE_TEST( qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                 sSqlStr,
                                 &sRowCnt )
              != IDE_SUCCESS );

    // Table Comment, Column Comment ���� �ô�
    // 0���ϼ��� �ְ� (Comment �� ���� �޸��� ��)
    // ���� ���ϼ��� �ִ�. (Table Comment�� Column Comment�� ���� ��)
    // �׷��� sRowCnt�� �˻����� �ʴ´�.

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdbComment::updateCommentColumn( qcStatement  * aStatement,
                                        SChar        * aUserName,
                                        SChar        * aTableName,
                                        SChar        * aOldColumnName,
                                        SChar        * aNewColumnName)
{
/***********************************************************************
 *
 * Description :
 *    Comment ������ �ƴ� �ٸ� ������ ���� Column�� �̸��� �ٲ���
 *    Comment ��Ÿ�� �ش� Object �̸��� ����
 *
 * Implementation :
 *    1. Comment�� Column �̸��� ����
 *
 ***********************************************************************/
    vSLong              sRowCnt        = 0;
    SChar             * sSqlStr        = NULL;

    // Parameter ����
    IDE_DASSERT( aStatement      != NULL );
    IDE_DASSERT( aUserName       != NULL );
    IDE_DASSERT( aTableName      != NULL );
    IDE_DASSERT( aOldColumnName  != NULL );
    IDE_DASSERT( aNewColumnName  != NULL );
    
    IDE_DASSERT( *aUserName      != '\0' );
    IDE_DASSERT( *aTableName     != '\0' );
    IDE_DASSERT( *aNewColumnName != '\0' );
    IDE_DASSERT( *aNewColumnName != '\0' );

    // ��Ÿ ����
    IDU_LIMITPOINT("qdbComment::updateCommentColumn::malloc");
    IDE_TEST( STRUCT_ALLOC_WITH_SIZE( aStatement->qmxMem,
                                      SChar,
                                      QD_MAX_SQL_LENGTH,
                                      &sSqlStr )
              != IDE_SUCCESS );

    // Column Comment �����ϴ� ����
    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                     "UPDATE SYS_COMMENTS_ "
                     "SET COLUMN_NAME = VARCHAR'%s' "
                     "WHERE USER_NAME = VARCHAR'%s' "
                     "AND TABLE_NAME = VARCHAR'%s' "
                     "AND COLUMN_NAME = VARCHAR'%s' ",
                     aNewColumnName,
                     aUserName,
                     aTableName,
                     aOldColumnName );

    IDE_TEST( qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                 sSqlStr,
                                 &sRowCnt )
              != IDE_SUCCESS );

    // ������ Column Comment�� 1�Ǻ��� ���ٸ� ��Ÿ ��������
    IDE_TEST_RAISE( sRowCnt > 1, ERR_META_CRASH );

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_META_CRASH);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDB_META_CRASH));
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdbComment::deleteCommentTable( qcStatement  * aStatement,
                                       SChar        * aUserName,
                                       SChar        * aTableName)
{
/***********************************************************************
 *
 * Description :
 *    Table�� ���� �� ��� �����ϴ� Comment�� ����
 *
 * Implementation :
 *    1. �ش� Object�� ���ϴ� Table Comment, Column Comment ����
 *
 ***********************************************************************/
    vSLong              sRowCnt        = 0;
    SChar             * sSqlStr        = NULL;
    
    // Parameter ����
    IDE_DASSERT( aStatement     != NULL );
    IDE_DASSERT( aUserName      != NULL );
    IDE_DASSERT( aTableName     != NULL );
    
    IDE_DASSERT( *aUserName     != '\0' );
    IDE_DASSERT( *aTableName    != '\0' );

    // ��Ÿ ����
    IDU_LIMITPOINT("qdbComment::deleteCommentTable::malloc");
    IDE_TEST( STRUCT_ALLOC_WITH_SIZE( aStatement->qmxMem,
                                      SChar,
                                      QD_MAX_SQL_LENGTH,
                                      &sSqlStr )
              != IDE_SUCCESS );

    // Table Comment, Column Comment �����ϴ� ����
    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                     "DELETE FROM SYS_COMMENTS_ "
                     "WHERE USER_NAME = VARCHAR'%s' "
                     "AND TABLE_NAME = VARCHAR'%s' ",
                     aUserName,
                     aTableName );

    IDE_TEST( qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                 sSqlStr,
                                 &sRowCnt )
              != IDE_SUCCESS );

    // Table Comment, Column Comment ���� �ô�
    // 0���ϼ��� �ְ� (Comment �� ���� �޸��� ��)
    // ���� ���ϼ��� �ִ�. (Table Comment�� Column Comment�� ���� ��)
    // �׷��� sRowCnt�� �˻����� �ʴ´�.

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC qdbComment::deleteCommentColumn( qcStatement  * aStatement,
                                        SChar        * aUserName,
                                        SChar        * aTableName,
                                        SChar        * aColumnName)
{
/***********************************************************************
 *
 * Description :
 *    Column�� ���� �� ��� �����ϴ� Comment�� ����
 *
 * Implementation :
 *    1. �ش� Column�� ���ϴ� Comment ����
 *
 ***********************************************************************/
    vSLong              sRowCnt        = 0;
    SChar             * sSqlStr        = NULL;

    // Parameter ����
    IDE_DASSERT( aStatement     != NULL );
    IDE_DASSERT( aUserName      != NULL );
    IDE_DASSERT( aTableName     != NULL );
    IDE_DASSERT( aColumnName    != NULL );
    
    IDE_DASSERT( *aUserName     != '\0' );
    IDE_DASSERT( *aTableName    != '\0' );
    IDE_DASSERT( *aColumnName   != '\0' );

    // ��Ÿ ����
    IDU_LIMITPOINT("qdbComment::deleteCommentColumn::malloc");
    IDE_TEST( STRUCT_ALLOC_WITH_SIZE( aStatement->qmxMem,
                                      SChar,
                                      QD_MAX_SQL_LENGTH,
                                      &sSqlStr )
              != IDE_SUCCESS );

    // Column Comment �����ϴ� ����
    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                     "DELETE FROM SYS_COMMENTS_ "
                     "WHERE USER_NAME = VARCHAR'%s' "
                     "AND TABLE_NAME = VARCHAR'%s' "
                     "AND COLUMN_NAME = VARCHAR'%s' ",
                     aUserName,
                     aTableName,
                     aColumnName );

    IDE_TEST( qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                 sSqlStr,
                                 &sRowCnt )
              != IDE_SUCCESS );

    // ������ Column Comment�� 1�Ǻ��� ���ٸ� ��Ÿ ��������
    IDE_TEST_RAISE( sRowCnt > 1, ERR_META_CRASH );

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_META_CRASH);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDB_META_CRASH));
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdbComment::existCommentTable( qcStatement    * aStatement,
                                      SChar          * aUserName,
                                      SChar          * aTableName,
                                      idBool         * aExist)
{
/***********************************************************************
 *
 * Description : Check Table Comment is exists
 *
 * Implementation :
 *     1. set search condition user name
 *     2. set search condition table name
 *     3. set search condition column name (set NULL)
 *     4. select comment from SYS_COMMENTS_ meta
 *
 ***********************************************************************/
    vSLong              sRowCount;

    smiRange            sRange;
    qtcMetaRangeColumn  sFirstMetaRange;
    qtcMetaRangeColumn  sSecondMetaRange;
    qtcMetaRangeColumn  sThirdMetaRange;

    qcNameCharBuffer    sUserNameBuffer;
    qcNameCharBuffer    sTableNameBuffer;
    mtdCharType       * sUserName;
    mtdCharType       * sTableName;
    mtcColumn         * sFirstMtcColumn;
    mtcColumn         * sSceondMtcColumn;
    mtcColumn         * sThirdMtcColumn;

    IDE_DASSERT( aStatement  != NULL );
    IDE_DASSERT( aUserName   != NULL );
    IDE_DASSERT( aTableName  != NULL );

    IDE_DASSERT( *aUserName  != '\0' );
    IDE_DASSERT( *aTableName != '\0' );

    // �ʱ�ȭ
    
    sUserName   = (mtdCharType *) &sUserNameBuffer;
    sTableName  = (mtdCharType *) &sTableNameBuffer;
    
    qtc::setVarcharValue( sUserName,
                          NULL,
                          aUserName,
                          idlOS::strlen(aUserName) );
    qtc::setVarcharValue( sTableName,
                          NULL,
                          aTableName,
                          idlOS::strlen(aTableName) );

    // �˻� ����
    qtc::initializeMetaRange( &sRange,
                              MTD_COMPARE_MTDVAL_MTDVAL );  // Meta is memory table

    IDE_TEST( smiGetTableColumns( gQcmComments,
                                  QCM_COMMENTS_USER_NAME_COL_ORDER,
                                  (const smiColumn**)&sFirstMtcColumn )
              != IDE_SUCCESS );

    qtc::setMetaRangeColumn( &sFirstMetaRange,
                             sFirstMtcColumn,
                             (const void*)sUserName,
                             SMI_COLUMN_ORDER_ASCENDING,
                             0 );  // First column of Index

    qtc::addMetaRangeColumn( &sRange,
                             &sFirstMetaRange );

    IDE_TEST( smiGetTableColumns( gQcmComments,
                                  QCM_COMMENTS_TABLE_NAME_COL_ORDER,
                                  (const smiColumn**)&sSceondMtcColumn )
              != IDE_SUCCESS );

    qtc::setMetaRangeColumn( &sSecondMetaRange,
                             sSceondMtcColumn,
                             (const void*)sTableName,
                             SMI_COLUMN_ORDER_ASCENDING,
                             1 );  // Second column of Index

    qtc::addMetaRangeColumn( &sRange,
                             &sSecondMetaRange );

    IDE_TEST( smiGetTableColumns( gQcmComments,
                                  QCM_COMMENTS_COLUMN_NAME_COL_ORDER,
                                  (const smiColumn**)&sThirdMtcColumn )
              != IDE_SUCCESS );

    qtc::setMetaRangeIsNullColumn(
        &sThirdMetaRange,
        sThirdMtcColumn,
        2 );  // Third column of Index

    qtc::addMetaRangeColumn( &sRange,
                             &sThirdMetaRange );

    qtc::fixMetaRange( &sRange );

    // ��ȸ
    IDE_TEST( qcm::selectCount( QC_SMI_STMT(aStatement),
                                gQcmComments,
                                &sRowCount,
                                smiGetDefaultFilter(),
                                &sRange,
                                gQcmCommentsIndex[QCM_COMMENTS_IDX1_ORDER]
                              )
              != IDE_SUCCESS );

    if (sRowCount == 0)
    {
        *aExist = ID_FALSE;
    }
    else
    {
        *aExist = ID_TRUE;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdbComment::existCommentColumn( qcStatement    * aStatement,
                                       SChar          * aUserName,
                                       SChar          * aTableName,
                                       SChar          * aColumnName,
                                       idBool         * aExist)
{
/***********************************************************************
 *
 * Description : Check Table Comment is exists
 *
 * Implementation :
 *     1. set search condition user name
 *     2. set search condition table name
 *     3. set search condition column name
 *     4. select comment from SYS_COMMENTS_ meta
 *
 ***********************************************************************/
    vSLong              sRowCount;

    smiRange            sRange;
    qtcMetaRangeColumn  sFirstMetaRange;
    qtcMetaRangeColumn  sSecondMetaRange;
    qtcMetaRangeColumn  sThirdMetaRange;

    qcNameCharBuffer    sUserNameBuffer;
    qcNameCharBuffer    sTableNameBuffer;
    qcNameCharBuffer    sColumnNameBuffer;
    mtdCharType       * sUserName;
    mtdCharType       * sTableName;
    mtdCharType       * sColumnName;
    mtcColumn         * sFirstMtcColumn;
    mtcColumn         * sSceondMtcColumn;
    mtcColumn         * sThirdMtcColumn;

    IDE_DASSERT( aStatement   != NULL );
    IDE_DASSERT( aUserName    != NULL );
    IDE_DASSERT( aTableName   != NULL );
    IDE_DASSERT( aColumnName  != NULL );
    
    IDE_DASSERT( *aUserName   != '\0' );
    IDE_DASSERT( *aTableName  != '\0' );
    IDE_DASSERT( *aColumnName != '\0' );

    // �ʱ�ȭ
    
    sUserName   = (mtdCharType *) &sUserNameBuffer;
    sTableName  = (mtdCharType *) &sTableNameBuffer;
    sColumnName = (mtdCharType *) &sColumnNameBuffer;
    
    qtc::setVarcharValue( sUserName,
                          NULL,
                          aUserName,
                          idlOS::strlen(aUserName) );
    qtc::setVarcharValue( sTableName,
                          NULL,
                          aTableName,
                          idlOS::strlen(aTableName) );
    qtc::setVarcharValue( sColumnName,
                          NULL,
                          aColumnName,
                          idlOS::strlen(aColumnName) );

    // �˻� ����
    qtc::initializeMetaRange( &sRange,
                              MTD_COMPARE_MTDVAL_MTDVAL );  // Meta is memory table

    IDE_TEST( smiGetTableColumns( gQcmComments,
                                  QCM_COMMENTS_USER_NAME_COL_ORDER,
                                  (const smiColumn**)&sFirstMtcColumn )
              != IDE_SUCCESS );

    qtc::setMetaRangeColumn( &sFirstMetaRange,
                             sFirstMtcColumn,
                             (const void*)sUserName,
                             SMI_COLUMN_ORDER_ASCENDING,
                             0 );  // First column of Index

    qtc::addMetaRangeColumn( &sRange,
                             &sFirstMetaRange );

    IDE_TEST( smiGetTableColumns( gQcmComments,
                                  QCM_COMMENTS_TABLE_NAME_COL_ORDER,
                                  (const smiColumn**)&sSceondMtcColumn )
              != IDE_SUCCESS );

    qtc::setMetaRangeColumn( &sSecondMetaRange,
                             sSceondMtcColumn,
                             (const void*)sTableName,
                             SMI_COLUMN_ORDER_ASCENDING,
                             1 );  // Second column of Index

    qtc::addMetaRangeColumn( &sRange,
                             &sSecondMetaRange );

    IDE_TEST( smiGetTableColumns( gQcmComments,
                                  QCM_COMMENTS_COLUMN_NAME_COL_ORDER,
                                  (const smiColumn**)&sThirdMtcColumn )
              != IDE_SUCCESS );

    qtc::setMetaRangeColumn( &sThirdMetaRange,
                             sThirdMtcColumn,
                             (const void*)sColumnName,
                             SMI_COLUMN_ORDER_ASCENDING,
                             2 );  // Third column of Index
    
    qtc::addMetaRangeColumn( &sRange,
                             &sThirdMetaRange );

    qtc::fixMetaRange( &sRange );

    // ��ȸ
    IDE_TEST( qcm::selectCount( QC_SMI_STMT(aStatement),
                                gQcmComments,
                                &sRowCount,
                                smiGetDefaultFilter(),
                                &sRange,
                                gQcmCommentsIndex[QCM_COMMENTS_IDX1_ORDER]
                              )
              != IDE_SUCCESS );

    if (sRowCount == 0)
    {
        *aExist = ID_FALSE;
    }
    else
    {
        *aExist = ID_TRUE;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdbComment::copyCommentsMeta( qcStatement  * aStatement,
                                     qcmTableInfo * aSourceTableInfo,
                                     qcmTableInfo * aTargetTableInfo )
{
/***********************************************************************
 *
 * Description :
 *      Comment�� �����Ѵ�. (Meta Table)
 *          - SYS_COMMENTS_
 *              - TABLE_NAME�� Target Table Name���� �����Ѵ�.
 *              - Hidden Column�̸� Function-based Index�� Column�̹Ƿ�, Hidden Column Name�� �����Ѵ�.
 *                  - Hidden Column Name�� Prefix�� ���δ�.
 *                      - Hidden Column Name = Index Name + $ + IDX + Number
 *              - �������� �״�� �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    qcmColumn * sSourceColumn = NULL;
    qcmColumn * sTargetColumn = NULL;
    SChar     * sSqlStr       = NULL;
    vSLong      sRowCnt       = ID_vLONG(0);

    IDU_FIT_POINT( "qdbComment::copyCommentsMeta::STRUCT_ALLOC_WITH_SIZE::sSqlStr",
                   idERR_ABORT_InsufficientMemory );

    IDE_TEST( STRUCT_ALLOC_WITH_SIZE( QC_QMX_MEM( aStatement ),
                                      SChar,
                                      QD_MAX_SQL_LENGTH,
                                      & sSqlStr )
              != IDE_SUCCESS );

    /* Table Comment */
    idlOS::snprintf( sSqlStr,
                     QD_MAX_SQL_LENGTH,
                     "INSERT INTO SYS_COMMENTS_ SELECT "
                     "USER_NAME, "                      // USER_NAME
                     "VARCHAR'%s', "                    // TABLE_NAME
                     "NULL, "                           // COLUMN_NAME
                     "COMMENTS "                        // COMMENTS
                     "FROM SYS_COMMENTS_ WHERE "
                     "USER_NAME = VARCHAR'%s' AND "     // USER_NAME
                     "TABLE_NAME = VARCHAR'%s' AND "    // TABLE_NAME
                     "COLUMN_NAME IS NULL ",
                     aTargetTableInfo->name,
                     aSourceTableInfo->tableOwnerName,
                     aSourceTableInfo->name );

    IDE_TEST( qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                 sSqlStr,
                                 & sRowCnt )
              != IDE_SUCCESS );

    /* Column Comment */
    for ( sSourceColumn = aSourceTableInfo->columns, sTargetColumn = aTargetTableInfo->columns;
          ( sSourceColumn != NULL ) && ( sTargetColumn != NULL );
          sSourceColumn = sSourceColumn->next, sTargetColumn = sTargetColumn->next )
    {
        // Target Column�� Hidden Column Prefix�� �̹� �����ߴ�.
        idlOS::snprintf( sSqlStr,
                         QD_MAX_SQL_LENGTH,
                         "INSERT INTO SYS_COMMENTS_ SELECT "
                         "USER_NAME, "                      // USER_NAME
                         "VARCHAR'%s', "                    // TABLE_NAME
                         "VARCHAR'%s', "                    // COLUMN_NAME
                         "COMMENTS "                        // COMMENTS
                         "FROM SYS_COMMENTS_ WHERE "
                         "USER_NAME = VARCHAR'%s' AND "     // USER_NAME
                         "TABLE_NAME = VARCHAR'%s' AND "    // TABLE_NAME
                         "COLUMN_NAME = VARCHAR'%s' ",      // COLUMN_NAME
                         aTargetTableInfo->name,
                         sTargetColumn->name,
                         aSourceTableInfo->tableOwnerName,
                         aSourceTableInfo->name,
                         sSourceColumn->name );

        IDE_TEST( qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                     sSqlStr,
                                     & sRowCnt )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

