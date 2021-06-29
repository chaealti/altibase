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
 * $Id: qcmUser.cpp 82075 2018-01-17 06:39:52Z jina.kim $
 **********************************************************************/

#include <idl.h>
#include <ide.h>
#include <qcm.h>
#include <qcg.h>
#include <qcmUser.h>
#include <qcmProc.h>
#include <qcmView.h>
#include <qcmCache.h>
#include <qdd.h>
#include <qdpPrivilege.h>
#include <qcuProperty.h>
#include <qcmPkg.h>
#include <qdpRole.h>

const void   * gQcmUsers;
const void   * gQcmUsersIndex [ QCM_MAX_META_INDICES ];

qcmTableInfo * gQcmUsersTempInfo;

//------------------------------------------------
// SYSTEM USER�� ���� ����
//------------------------------------------------
/*
  qcUserInfo gQcmSystemUserInfo =
  { QC_SYSTEM_USER_ID,                    // User ID
  SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,  // Memory Table Space ID
  SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,  // Memory Temp Table Space ID
  ID_FALSE };                           // SYSDBA Mode
*/

IDE_RC qcmUser::getUserID(
    qcStatement     * aStatement,
    qcNamePosition    aUserName,
    UInt            * aUserID)
{
    qcuSqlSourceInfo    sqlInfo;
    UInt                sSqlCode;

    if ( QC_IS_NULL_NAME(aUserName) == ID_TRUE )
    {
        IDE_TEST( getUserID( aStatement,
                             NULL,
                             0,
                             aUserID ) != IDE_SUCCESS );
    }
    else
    {
        IDE_TEST( getUserID( aStatement,
                             aUserName.stmtText + aUserName.offset,
                             aUserName.size,
                             aUserID ) != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    // BUG-38883 print error position
    // sqlSourceInfo�� ���� error���.
    if ( ( QC_IS_NULL_NAME(aUserName) == ID_FALSE ) &&
         ( ideHasErrorPosition() == ID_FALSE ) &&
         ( aUserName.offset != 0 ) )
    {
        sqlInfo.setSourceInfo( aStatement,
                               & aUserName);

        sSqlCode = ideGetErrorCode();

        if ( sSqlCode == qpERR_ABORT_QCM_NOT_EXIST_USER )
        {
            (void)sqlInfo.initWithBeforeMessage(QC_QME_MEM(aStatement));
            IDE_SET(
                ideSetErrorCode(qpERR_ABORT_QSX_SQLTEXT_WRAPPER,
                                sqlInfo.getBeforeErrMessage(),
                                sqlInfo.getErrMessage()));
            (void)sqlInfo.fini();

            // overwrite wrapped errorcode to original error code.
            ideGetErrorMgr()->Stack.LastError = sSqlCode;
        }
        else
        {
            // Nothing to do.
        }
    }
    else
    {
        // Nothing to do.
    }

    return IDE_FAILURE;
}

IDE_RC qcmUser::getUserID(
    qcStatement     *aStatement,
    SChar           *aUserName,
    UInt             aUserNameSize,
    UInt            *aUserID)
{
/***********************************************************************
 *
 * Description :
 *    ������� ID �� ���Ѵ�.
 *
 * Implementation :
 *    �������� �־����� �ʾ��� ���
 *        ������ ���� ID �� ��ȯ�Ѵ�.
 *    �������� �־����� ���
 *        createdb ��
 *        1. SYS_USERS_ ��Ÿ ���̺��� �ڵ��� ���Ѵ�.
 *        2. 1 �� ���̺��� �����Ϳ��� ��õ� ������� ��ġ�ϴ� ������ ������
 *           USER_ID �� ��ȯ�ϰ�, �׷��� ������ ������ ��ȯ�Ѵ�.
 *        createdb �� �ƴ� ��
 *        1. ��Ÿ ĳ���κ��� USER_NAME �� �ش��ϴ� �÷��� ã�Ƽ�
 *        2. ����� ���������� keyRange �� �����.
 *        3. Ŀ�� open ,beforeFirst, readRow �� ���ؼ� userID �� ���Ѵ�.
 *        4. readRow �� ���� ������ ������ ��ȯ�Ѵ�.
 *
 ***********************************************************************/

    if ( ( aUserName == NULL ) || ( aUserNameSize == 0 ) )
    {
        *aUserID = QCG_GET_SESSION_USER_ID( aStatement );
    }
    else
    {
        IDE_TEST( getUserID( QC_STATISTICS( aStatement ),
                             QC_SMI_STMT( aStatement ),
                             aUserName,
                             aUserNameSize,
                             aUserID )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description :
 *    ������� ID �� ���Ѵ�.
 *
 * Implementation :
 *        createdb ��
 *        1. SYS_USERS_ ��Ÿ ���̺��� �ڵ��� ���Ѵ�.
 *        2. 1 �� ���̺��� �����Ϳ��� ��õ� ������� ��ġ�ϴ� ������ ������
 *           USER_ID �� ��ȯ�ϰ�, �׷��� ������ ������ ��ȯ�Ѵ�.
 *        createdb �� �ƴ� ��
 *        1. ��Ÿ ĳ���κ��� USER_NAME �� �ش��ϴ� �÷��� ã�Ƽ�
 *        2. ����� ���������� keyRange �� �����.
 *        3. Ŀ�� open ,beforeFirst, readRow �� ���ؼ� userID �� ���Ѵ�.
 *        4. readRow �� ���� ������ ������ ��ȯ�Ѵ�.
 *
 ***********************************************************************/
IDE_RC qcmUser::getUserID( idvSQL       * aStatistics,
                           smiStatement * aSmiStatement,
                           SChar        * aUserName,
                           UInt           aUserNameSize,
                           UInt         * aUserID )
{
    UInt                    sStage = 0;
    smiRange                sRange;
    const void            * sUserTable;
    const void            * sRow;
    smiTableCursor          sCursor;
    mtcColumn             * sMtcColumn;
    mtdCharType           * sUserNameStr;
    idBool                  sFoundUser = ID_FALSE;
    qtcMetaRangeColumn      sRangeColumn;
    smSCN                   sSCN;
    qcmColumn             * sColumn = NULL;
    qcNameCharBuffer        sUserNameBuffer;
    mtdCharType           * sUserName = ( mtdCharType * ) & sUserNameBuffer;
    UChar                   sSearchingUserName[ QC_MAX_OBJECT_NAME_LEN + 1 ];
    smiCursorProperties     sCursorProperty;
    scGRID                  sRid; // Disk Table�� ���� Record IDentifier
    mtcColumn             * sUserTypeCol;
    mtdCharType           * sUserTypeStr;
    UChar                 * sUserType;

    sCursor.initialize();

    //fix BUG-18627
    IDE_TEST_RAISE(aUserNameSize >  QC_MAX_OBJECT_NAME_LEN, ERR_NOT_EXIST_USER);
    idlOS::strncpy( (SChar*) sSearchingUserName , aUserName, aUserNameSize);
    sSearchingUserName [ aUserNameSize ] = '\0';

    if ( gQcmUsersIndex[QCM_USER_USERNAME_IDX_ORDER] == NULL)
    {// on createdb
        IDE_TEST(qcm::getTableHandleByName(
                     aSmiStatement,
                     QC_SYSTEM_USER_ID,
                     (UChar*)QCM_USERS,
                     idlOS::strlen((SChar*)QCM_USERS),
                     (void**)&sUserTable,
                     &sSCN)
                 != IDE_SUCCESS);

        SMI_CURSOR_PROP_INIT_FOR_META_FULL_SCAN(&sCursorProperty, aStatistics);

        IDE_TEST(sCursor.open(
                     aSmiStatement,
                     sUserTable,
                     NULL,
                     smiGetRowSCN(sUserTable),
                     NULL,
                     smiGetDefaultKeyRange(),
                     smiGetDefaultKeyRange(),
                     smiGetDefaultFilter(),
                     QCM_META_CURSOR_FLAG,
                     SMI_SELECT_CURSOR,
                     & sCursorProperty) != IDE_SUCCESS);
        sStage = 1;

        IDE_TEST(sCursor.beforeFirst() != IDE_SUCCESS);
        IDE_TEST(sCursor.readRow(&sRow, &sRid, SMI_FIND_NEXT) != IDE_SUCCESS);
        IDE_TEST_RAISE(sRow == NULL, ERR_NOT_EXIST_USER);
        IDE_TEST( smiGetTableColumns( sUserTable,
                                      QCM_USERS_USER_NAME_COL_ORDER,
                                      (const smiColumn**)&sMtcColumn )
                  != IDE_SUCCESS );

        while ( ( sRow != NULL ) && ( sFoundUser != ID_TRUE ) )
        {
            sUserNameStr = (mtdCharType*)((UChar *)sRow +
                                          sMtcColumn->column.offset);

            if ( idlOS::strMatch( (SChar*)sSearchingUserName,
                                  aUserNameSize,
                                  (SChar*)&(sUserNameStr->value),
                                  sUserNameStr->length ) == 0 )
            {
                IDE_TEST( smiGetTableColumns( sUserTable,
                                              QCM_USERS_USER_ID_COL_ORDER,
                                              (const smiColumn**)&sMtcColumn )
                          != IDE_SUCCESS );
                *aUserID = *(UInt*)((UChar*)sRow +
                                    sMtcColumn->column.offset);

                /* PROJ-1812 ROLE */
                IDE_TEST( smiGetTableColumns( sUserTable,
                                              QCM_USERS_USER_TYPE_COL_ORDER,
                                              (const smiColumn**)&sUserTypeCol )
                          != IDE_SUCCESS );
                sUserTypeStr = (mtdCharType *)( (UChar *)sRow +
                                                sUserTypeCol->column.offset );

                sUserType = sUserTypeStr->value;

                IDE_TEST_RAISE( sUserType[0] != 'U', ERR_NOT_EXIST_USER );

                sFoundUser = ID_TRUE;
            }

            IDE_TEST(sCursor.readRow(&sRow, &sRid, SMI_FIND_NEXT) != IDE_SUCCESS);
        }
        sStage = 0;
        IDE_TEST(sCursor.close() != IDE_SUCCESS);
    }
    else
    {
        IDE_TEST(qcmCache::getColumnByName(
                     gQcmUsersTempInfo,
                     (SChar*) "USER_NAME",
                     idlOS::strlen((SChar*) "USER_NAME"),
                     &sColumn) != IDE_SUCCESS);

        IDE_ERROR( sColumn != NULL );

        qtc::setVarcharValue( sUserName,
                              sColumn->basicInfo,
                              aUserName,
                              aUserNameSize );

        qcm::makeMetaRangeSingleColumn(&sRangeColumn,
                                       (const mtcColumn*)sColumn->basicInfo,
                                       (const void*)sUserName,
                                       &sRange );

        SMI_CURSOR_PROP_INIT_FOR_META_INDEX_SCAN(&sCursorProperty, aStatistics);

        IDE_TEST(sCursor.open(
                     aSmiStatement,
                     gQcmUsersTempInfo->tableHandle,
                     gQcmUsersIndex[QCM_USER_USERNAME_IDX_ORDER],
                     smiGetRowSCN(gQcmUsersTempInfo->tableHandle),
                     NULL,
                     &sRange,
                     smiGetDefaultKeyRange(),
                     smiGetDefaultFilter(),
                     QCM_META_CURSOR_FLAG,
                     SMI_SELECT_CURSOR,
                     & sCursorProperty) != IDE_SUCCESS);
        sStage = 1;

        IDE_TEST(sCursor.beforeFirst() != IDE_SUCCESS);

        IDE_TEST(sCursor.readRow(&sRow, &sRid, SMI_FIND_NEXT) != IDE_SUCCESS);

        IDE_TEST_RAISE(sRow == NULL, ERR_NOT_EXIST_USER);

        IDE_TEST( smiGetTableColumns( gQcmUsersTempInfo->tableHandle,
                                      QCM_USERS_USER_ID_COL_ORDER,
                                      (const smiColumn**)&sMtcColumn )
                  != IDE_SUCCESS );
        *aUserID = *(UInt*)((UChar*)sRow +
                            sMtcColumn->column.offset);

        /* PROJ-1812 ROLE */
        IDE_TEST( smiGetTableColumns( gQcmUsersTempInfo->tableHandle,
                                      QCM_USERS_USER_TYPE_COL_ORDER,
                                      (const smiColumn**)&sUserTypeCol )
                  != IDE_SUCCESS );
        sUserTypeStr = (mtdCharType *)( (UChar *)sRow +
                                        sUserTypeCol->column.offset );

        sUserType = sUserTypeStr->value;

        IDE_TEST_RAISE( sUserType[0] != 'U', ERR_NOT_EXIST_USER );

        sFoundUser = ID_TRUE;

        sStage = 0;
        IDE_TEST(sCursor.close() != IDE_SUCCESS);
    }
    IDE_TEST_RAISE(sFoundUser != ID_TRUE, ERR_NOT_EXIST_USER);

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_NOT_EXIST_USER);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCM_NOT_EXIST_USER));
    }
    IDE_EXCEPTION_END;

    if ( sStage == 1 )
    {
        (void)sCursor.close();
    }

    return IDE_FAILURE;
}

IDE_RC qcmUser::getUserIDAndType( qcStatement     * aStatement,
                                  qcNamePosition    aUserName,
                                  UInt            * aUserID,
                                  qdUserType      * aUserType )
{
    if ( QC_IS_NULL_NAME(aUserName) == ID_TRUE )
    {
        IDE_TEST( getUserIDAndTypeByName(
                      aStatement,
                      NULL,
                      0,
                      aUserID,
                      aUserType ) != IDE_SUCCESS );
    }
    else
    {
        IDE_TEST( getUserIDAndTypeByName(
                      aStatement,
                      aUserName.stmtText + aUserName.offset,
                      aUserName.size,
                      aUserID,
                      aUserType ) != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* PROJ-1812 ROLE */
/***********************************************************************
 *
 * Description : PROJ-1812 ROLE
 *    ������� ROLE ID, USER ID, USER TYPE �� ���Ѵ�.
 *    create user, create role �� userid �� user type�� ������� ��� üũ
 *    ���� getUserID() �� user type return �߰�.
 *
 * Implementation :
 *        createdb ��
 *        1. SYS_USERS_ ��Ÿ ���̺��� �ڵ��� ���Ѵ�.
 *        2. 1 �� ���̺��� �����Ϳ��� ��õ� ������� ��ġ�ϴ� ������ ������
 *           USER_ID �� ��ȯ�ϰ�, �׷��� ������ ������ ��ȯ�Ѵ�.
 *        createdb �� �ƴ� ��
 *        1. ��Ÿ ĳ���κ��� USER_NAME �� �ش��ϴ� �÷��� ã�Ƽ�
 *        2. ����� ���������� keyRange �� �����.
 *        3. Ŀ�� open ,beforeFirst, readRow �� ���ؼ� userID �� ���Ѵ�.
 *        4. readRow �� ���� ������ ������ ��ȯ�Ѵ�.
 *
 ***********************************************************************/
IDE_RC qcmUser::getUserIDAndTypeByName(
    qcStatement         * aStatement,
    SChar               * aUserName,
    UInt                  aUserNameSize,
    UInt                * aUserID,
    qdUserType          * aUserType )
{
    UInt                    sStage = 0;
    smiRange                sRange;
    const void            * sUserTable;
    const void            * sRow;
    smiTableCursor          sCursor;
    qtcMetaRangeColumn      sRangeColumn;
    mtcColumn             * sMtcColumn;
    mtdCharType           * sUserNameStr;
    idBool                  sFoundUser = ID_FALSE;
    smSCN                   sSCN;
    qcmColumn             * sColumn = NULL;
    qcNameCharBuffer        sUserNameBuffer;
    mtdCharType           * sUserName = ( mtdCharType * ) & sUserNameBuffer;
    UChar                   sSearchingUserName[ QC_MAX_OBJECT_NAME_LEN + 1 ];
    smiCursorProperties     sCursorProperty;
    scGRID                  sRid;
    mtcColumn             * sUserTypeCol;
    mtdCharType           * sUserTypeStr;
    UChar                 * sUserType;
 
    sCursor.initialize();

    //fix BUG-18627
    IDE_TEST_RAISE( aUserNameSize >  QC_MAX_OBJECT_NAME_LEN,ERR_NOT_EXIST_USER_OR_ROLE );
    idlOS::strncpy( (SChar*) sSearchingUserName , aUserName, aUserNameSize );
    sSearchingUserName [aUserNameSize] = '\0';

    if ( gQcmUsersIndex[QCM_USER_USERNAME_IDX_ORDER] == NULL )
    {// on createdb
        IDE_TEST( qcm::getTableHandleByName(
                      QC_SMI_STMT( aStatement ),
                      QC_SYSTEM_USER_ID,
                      (UChar*)QCM_USERS,
                      idlOS::strlen( (SChar*)QCM_USERS ),
                      (void**)&sUserTable,
                      &sSCN )
                  != IDE_SUCCESS );

        SMI_CURSOR_PROP_INIT_FOR_META_FULL_SCAN( &sCursorProperty, aStatement->mStatistics );

        IDE_TEST(sCursor.open(
                     QC_SMI_STMT( aStatement ),
                     sUserTable,
                     NULL,
                     smiGetRowSCN( sUserTable ),
                     NULL,
                     smiGetDefaultKeyRange(),
                     smiGetDefaultKeyRange(),
                     smiGetDefaultFilter(),
                     QCM_META_CURSOR_FLAG,
                     SMI_SELECT_CURSOR,
                     & sCursorProperty ) != IDE_SUCCESS );
        sStage = 1;

        IDE_TEST( sCursor.beforeFirst() != IDE_SUCCESS );
        IDE_TEST( sCursor.readRow(&sRow, &sRid, SMI_FIND_NEXT ) != IDE_SUCCESS );

        IDE_TEST_RAISE( sRow == NULL, ERR_NOT_EXIST_USER_OR_ROLE );
        
        IDE_TEST( smiGetTableColumns( sUserTable,
                                      QCM_USERS_USER_NAME_COL_ORDER,
                                      (const smiColumn**)&sMtcColumn )
                  != IDE_SUCCESS );

        while ( ( sRow != NULL ) && ( sFoundUser != ID_TRUE ) )
        {
            sUserNameStr = (mtdCharType*)( (UChar *)sRow +
                                           sMtcColumn->column.offset );

            if ( idlOS::strMatch( (SChar*)sSearchingUserName,
                                  aUserNameSize, 
                                  (SChar*)&(sUserNameStr->value ),
                                  sUserNameStr->length ) == 0 )
            {
                IDE_TEST( smiGetTableColumns( sUserTable,
                                              QCM_USERS_USER_ID_COL_ORDER,
                                              (const smiColumn**)&sMtcColumn )
                          != IDE_SUCCESS );
                *aUserID = *(UInt*)( (UChar*)sRow +
                                     sMtcColumn->column.offset );

                IDE_TEST( smiGetTableColumns( sUserTable,
                                              QCM_USERS_USER_TYPE_COL_ORDER,
                                              (const smiColumn**)&sUserTypeCol )
                          != IDE_SUCCESS );
                sUserTypeStr = (mtdCharType *)( (UChar *)sRow +
                                                sUserTypeCol->column.offset );

                sUserType = sUserTypeStr->value;
            
                if ( sUserType[0] == 'U' )
                {
                    *aUserType = QDP_USER_TYPE;
                }
                else if ( sUserType[0] == 'R' )
                {
                    *aUserType = QDP_ROLE_TYPE;
                }
                else
                {
                    IDE_RAISE( ERR_META_CRASH );
                }

                sFoundUser = ID_TRUE;
            }

            IDE_TEST( sCursor.readRow( &sRow, &sRid, SMI_FIND_NEXT ) != IDE_SUCCESS );
        }
        sStage = 0;
        IDE_TEST( sCursor.close() != IDE_SUCCESS );
    }
    else
    {
        IDE_TEST( qcmCache::getColumnByName(
                      gQcmUsersTempInfo,
                      (SChar*) "USER_NAME",
                      idlOS::strlen( (SChar*) "USER_NAME" ),
                      &sColumn )
                  != IDE_SUCCESS );

        IDE_ASSERT( sColumn != NULL );

        qtc::setVarcharValue( sUserName,
                              sColumn->basicInfo,
                              aUserName,
                              aUserNameSize );

        qcm::makeMetaRangeSingleColumn( &sRangeColumn,
                                        (const mtcColumn*)sColumn->basicInfo,
                                        (const void*)sUserName,
                                        &sRange );
            
        SMI_CURSOR_PROP_INIT_FOR_META_INDEX_SCAN( &sCursorProperty, aStatement->mStatistics );

        IDE_TEST( sCursor.open(
                      QC_SMI_STMT( aStatement ),
                      gQcmUsersTempInfo->tableHandle,
                      gQcmUsersIndex[QCM_USER_USERNAME_IDX_ORDER],
                      smiGetRowSCN( gQcmUsersTempInfo->tableHandle ),
                      NULL,
                      &sRange,
                      smiGetDefaultKeyRange(),
                      smiGetDefaultFilter(),
                      QCM_META_CURSOR_FLAG,
                      SMI_SELECT_CURSOR,
                      & sCursorProperty )
                  != IDE_SUCCESS );
        sStage = 1;

        IDE_TEST( sCursor.beforeFirst() != IDE_SUCCESS );
        IDE_TEST( sCursor.readRow(&sRow, &sRid, SMI_FIND_NEXT ) != IDE_SUCCESS );

        IDE_TEST_RAISE( sRow == NULL, ERR_NOT_EXIST_USER_OR_ROLE );
            
        IDE_TEST( smiGetTableColumns( gQcmUsersTempInfo->tableHandle,
                                      QCM_USERS_USER_ID_COL_ORDER,
                                      (const smiColumn**)&sMtcColumn )
                  != IDE_SUCCESS );
        *aUserID = *(UInt*)( (UChar*)sRow +
                             sMtcColumn->column.offset );
            
        IDE_TEST( smiGetTableColumns( gQcmUsersTempInfo->tableHandle,
                                      QCM_USERS_USER_TYPE_COL_ORDER,
                                      (const smiColumn**)&sUserTypeCol )
                  != IDE_SUCCESS );
        sUserTypeStr = (mtdCharType *)( (UChar *)sRow +
                                        sUserTypeCol->column.offset );

        sUserType = sUserTypeStr->value;
            
        if ( sUserType[0] == 'U' )
        {
            *aUserType = QDP_USER_TYPE;
        }
        else if ( sUserType[0] == 'R' )
        {
            *aUserType = QDP_ROLE_TYPE;
        }
        else
        {
            IDE_RAISE( ERR_META_CRASH );
        }
        
        sFoundUser = ID_TRUE;
            
        sStage = 0;
        IDE_TEST( sCursor.close() != IDE_SUCCESS );
    }
        
    IDE_TEST_RAISE( sFoundUser != ID_TRUE, ERR_NOT_EXIST_USER_OR_ROLE );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NOT_EXIST_USER_OR_ROLE );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QCM_NOT_EXIST_USER_OR_ROLE ) );
    }
    IDE_EXCEPTION( ERR_META_CRASH );
    {
        IDE_SET(ideSetErrorCode( qpERR_ABORT_QCM_META_CRASH ) );
    }
    IDE_EXCEPTION_END;

    if ( sStage == 1 )
    {
        (void) sCursor.close();
    }
    else
    {
        /* Nothing To Do */
    }
    
    return IDE_FAILURE;
}

IDE_RC qcmUser::getUserPassword(
    qcStatement     * aStatement,
    UInt              aUserID,
    SChar           * aPassword)
{
/***********************************************************************
 *
 * Description :
 *    ������� ��ȣ�� ���Ѵ�.
 *
 * Implementation :
 *    1. ��Ÿ ĳ���κ��� USER_ID �� �ش��ϴ� �÷��� ã�Ƽ�
 *    2. ����� ����ID �� keyRange �� �����.
 *    3. Ŀ�� open ,beforeFirst, readRow �� ���ؼ� password �� ���Ѵ�.
 *    4. readRow �� ���� ������ ������ ��ȯ�Ѵ�.
 *
 ***********************************************************************/

    smiTableCursor          sCursor;
    qcmColumn             * sColumn = NULL;
    smiRange                sRange;
    qtcMetaRangeColumn      sRangeColumn;
    mtcColumn             * sMtcColumn;
    mtdCharType           * sPasswdStr;
    SInt                    sStage = 0;
    const void            * sRow;
    smiCursorProperties     sCursorProperty;
    scGRID                  sRid; // Disk Table�� ���� Record IDentifier
    mtcColumn             * sUserTypeColumn;
    mtdCharType           * sUserTypeStr;
    UChar                 * sUserType;

    sCursor.initialize();

    IDE_TEST(qcmCache::getColumnByName(gQcmUsersTempInfo,
                                       (SChar*) "USER_ID",
                                       idlOS::strlen((SChar*) "USER_ID"),
                                       &sColumn) != IDE_SUCCESS);

    IDE_ASSERT( sColumn != NULL );
    
    // mtdModule ����
    IDE_TEST(mtd::moduleById( &(sColumn->basicInfo->module),
                              sColumn->basicInfo->type.dataTypeId)
             != IDE_SUCCESS);

    // mtlModule ����
    IDE_TEST(mtl::moduleById( &(sColumn->basicInfo->language),
                              sColumn->basicInfo->type.languageId)
             != IDE_SUCCESS);

    qcm::makeMetaRangeSingleColumn(
        &sRangeColumn,
        (const mtcColumn*)sColumn->basicInfo,
        (const void *) & aUserID,
        &sRange);

    SMI_CURSOR_PROP_INIT_FOR_META_INDEX_SCAN(&sCursorProperty, aStatement->mStatistics);

    IDE_TEST(sCursor.open(
                 QC_SMI_STMT( aStatement ),
                 gQcmUsersTempInfo->tableHandle,
                 gQcmUsersIndex[QCM_USER_USERID_IDX_ORDER],
                 smiGetRowSCN(gQcmUsersTempInfo->tableHandle),
                 NULL,
                 &sRange,
                 smiGetDefaultKeyRange(),
                 smiGetDefaultFilter(),
                 QCM_META_CURSOR_FLAG,
                 SMI_SELECT_CURSOR,
                 & sCursorProperty) != IDE_SUCCESS);

    sStage = 1;

    IDE_TEST(sCursor.beforeFirst() != IDE_SUCCESS);

    IDE_TEST(sCursor.readRow(&sRow, &sRid, SMI_FIND_NEXT) != IDE_SUCCESS);

    IDE_TEST_RAISE( sRow == NULL, ERR_NOT_EXIST_USER );

    IDE_TEST( smiGetTableColumns( gQcmUsersTempInfo->tableHandle,
                                  QCM_USERS_USER_PASSWD_COL_ORDER,
                                  (const smiColumn**)&sMtcColumn )
              != IDE_SUCCESS );
    sPasswdStr  = (mtdCharType*)((SChar*)sRow+sMtcColumn->column.offset);

    IDE_TEST( smiGetTableColumns( gQcmUsersTempInfo->tableHandle,
                                  QCM_USERS_USER_TYPE_COL_ORDER,
                                  (const smiColumn**)&sUserTypeColumn )
              != IDE_SUCCESS );
    sUserTypeStr  = (mtdCharType*)( (SChar*)sRow + sUserTypeColumn->column.offset );

    sUserType = sUserTypeStr->value;
    
    /* PROJ-1812 ROLE */
    IDE_TEST_RAISE( sUserType[0] != 'U', ERR_NOT_EXIST_USER );
    
    idlOS::strncpy(aPassword, (SChar*)&(sPasswdStr->value),
                   sPasswdStr->length);
    aPassword[ sPasswdStr->length ] = '\0';

    sStage = 0;
    IDE_TEST(sCursor.close() != IDE_SUCCESS);

    // check grant
    IDE_TEST( qdpRole::checkDDLCreateSessionPriv( aStatement, aUserID )
              != IDE_SUCCESS );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_NOT_EXIST_USER);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCM_NOT_EXIST_USER));
    }
    IDE_EXCEPTION_END;

    if ( sStage == 1 )
    {
        (void)sCursor.close();
    }

    return IDE_FAILURE;
}

IDE_RC qcmUser::getUserName(
    qcStatement     * aStatement,
    UInt              aUserID,
    SChar           * aUserName)
{
/***********************************************************************
 *
 * Description :
 *    ������� �̸��� ���Ѵ�.
 *
 * Implementation :
 *    1. ��Ÿ ĳ���κ��� USER_ID �� �ش��ϴ� �÷��� ã�Ƽ�
 *    2. ����� ����ID �� keyRange �� �����.
 *    3. Ŀ�� open ,beforeFirst, readRow �� ���ؼ� user name �� ���Ѵ�.
 *    4. readRow �� ���� ������ ������ ��ȯ�Ѵ�.
 *
 ***********************************************************************/

    smiTableCursor          sCursor;
    qcmColumn             * sColumn = NULL;
    smiRange                sRange;
    qtcMetaRangeColumn      sRangeColumn;
    mtcColumn             * sMtcColumn;
    mtdCharType           * sUserNameStr;
    SInt                    sStage = 0;
    const void            * sRow;
    smiCursorProperties     sCursorProperty;
    scGRID                  sRid; // Disk Table�� ���� Record IDentifier

    // BUG-24540
    // SYSTEM_ USER�� ���, ��ųʸ� ���̺��� �˻����� �ʰ� �ٷ� ����
    if( aUserID == QC_SYSTEM_USER_ID )
    {
        idlOS::strcpy(aUserName,
                      QC_SYSTEM_USER_NAME);
        
        IDE_CONT( NORMAL_EXIT );
    }
    else
    {
        // nothing to do
    }

    // BUG-24540
    // SYS USER�� ���, ��ųʸ� ���̺��� �˻����� �ʰ� �ٷ� ����
    if( aUserID == QC_SYS_USER_ID )
    {
        idlOS::strcpy(aUserName,
                      QC_SYS_USER_NAME);
        
        IDE_CONT( NORMAL_EXIT );
    }
    else
    {
        // nothing to do
    }    

    sCursor.initialize();

    IDE_TEST(qcmCache::getColumnByName(gQcmUsersTempInfo,
                                       (SChar*) "USER_ID",
                                       idlOS::strlen((SChar*) "USER_ID"),
                                       &sColumn) != IDE_SUCCESS);

    IDE_ASSERT( sColumn != NULL );
    
    // mtdModule ����
    IDE_TEST(mtd::moduleById( &(sColumn->basicInfo->module),
                              sColumn->basicInfo->type.dataTypeId)
             != IDE_SUCCESS);

    // mtlModule ����
    IDE_TEST(mtl::moduleById( &(sColumn->basicInfo->language),
                              sColumn->basicInfo->type.languageId)
             != IDE_SUCCESS);

    qcm::makeMetaRangeSingleColumn(
        &sRangeColumn,
        (const mtcColumn*)sColumn->basicInfo,
        (const void *) & aUserID,
        &sRange);

    SMI_CURSOR_PROP_INIT_FOR_META_INDEX_SCAN( &sCursorProperty, aStatement->mStatistics );

    IDE_TEST(sCursor.open(
                 QC_SMI_STMT( aStatement ),
                 gQcmUsersTempInfo->tableHandle,
                 gQcmUsersIndex[QCM_USER_USERID_IDX_ORDER],
                 smiGetRowSCN(gQcmUsersTempInfo->tableHandle),
                 NULL,
                 &sRange,
                 smiGetDefaultKeyRange(),
                 smiGetDefaultFilter(),
                 QCM_META_CURSOR_FLAG,
                 SMI_SELECT_CURSOR,
                 & sCursorProperty) != IDE_SUCCESS);
    sStage = 1;

    IDE_TEST(sCursor.beforeFirst() != IDE_SUCCESS);

    IDE_TEST(sCursor.readRow(&sRow, &sRid, SMI_FIND_NEXT) != IDE_SUCCESS);

    IDE_TEST_RAISE( sRow == NULL, ERR_NOT_EXIST_USER );

    IDE_TEST( smiGetTableColumns( gQcmUsersTempInfo->tableHandle,
                                  QCM_USERS_USER_NAME_COL_ORDER,
                                  (const smiColumn**)&sMtcColumn )
              != IDE_SUCCESS );
    sUserNameStr = (mtdCharType*)((SChar*)sRow+sMtcColumn->column.offset);
    sStage = 0;
    IDE_TEST(sCursor.close() != IDE_SUCCESS);

    idlOS::strncpy(aUserName,
                   (SChar*)&(sUserNameStr->value),
                   sUserNameStr->length);
    aUserName[sUserNameStr->length] = '\0';

    IDE_EXCEPTION_CONT( NORMAL_EXIT );    
    
    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_NOT_EXIST_USER);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCM_NOT_EXIST_USER));
    }
    IDE_EXCEPTION_END;

    if ( sStage == 1 )
    {
        (void)sCursor.close();
    }

    return IDE_FAILURE;
}

IDE_RC qcmUser::getNextUserID(qcStatement *aStatement,
                              UInt        *aUserID)
{
/***********************************************************************
 *
 * Description :
 *    ���ο� ����� ������ user ID �� �ο��Ѵ�
 *
 * Implementation :
 *    1. USER �� �ο��ϴ� ��Ÿ �������� �ڵ��� ã�´�.
 *    2. 1�� �ڵ�� �������� nextval �� ã�Ƽ� ��ȯ�Ѵ�.
 *
 ***********************************************************************/

    void   * sSequenceHandle;
    idBool   sExist;
    idBool   sFirst;
    SLong    sSeqVal;
    SLong    sSeqValFirst;

    sExist = ID_TRUE;
    sFirst = ID_TRUE;

    IDE_TEST(qcm::getSequenceHandleByName(
                 QC_SMI_STMT( aStatement ),
                 QC_SYSTEM_USER_ID,
                 (UChar*)gDBSequenceName[QCM_DB_SEQUENCE_USERID],
                 idlOS::strlen(gDBSequenceName[QCM_DB_SEQUENCE_USERID]),
                 &sSequenceHandle)
             != IDE_SUCCESS);

    while( sExist == ID_TRUE )
    {
        IDE_TEST(smiTable::readSequence( QC_SMI_STMT( aStatement ),
                                         sSequenceHandle,
                                         SMI_SEQUENCE_NEXT,
                                         &sSeqVal,
                                         NULL )
                 != IDE_SUCCESS );

        // sSeqVal�� ��� SLong������, sequence�� ������ ��
        // max�� integer max�� �ȳѵ��� �Ͽ��� ������
        // ���⼭ overflowüũ�� ���� �ʴ´�.
        IDE_TEST( searchUserID( QC_SMI_STMT( aStatement ),
                                (SInt)sSeqVal,
                                &sExist )
                  != IDE_SUCCESS );

        if( sFirst == ID_TRUE )
        {
            sSeqValFirst = sSeqVal;
            sFirst = ID_FALSE;
        }
        else
        {
            // ã��ã�� �ѹ��� �� ���.
            // �̴� object�� �� �� ���� �ǹ���.
            IDE_TEST_RAISE( sSeqVal == sSeqValFirst, ERR_OBJECTS_OVERFLOW );
        }
    }

    *aUserID = sSeqVal;
    
    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_OBJECTS_OVERFLOW);
    {    
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCM_MAXIMUM_OBJECT_NUMBER_EXCEED,
                                "users",
                                QCM_META_SEQ_MAXVALUE) );
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qcmUser::searchUserID( smiStatement * aSmiStmt,
                              SInt           aUserID,
                              idBool       * aExist )
{
/***********************************************************************
 *
 * Description : user id�� �����ϴ��� �˻�.
 *
 * Implementation :
 *
 ***********************************************************************/
    
    UInt                sStage = 0;
    smiRange            sRange;
    smiTableCursor      sCursor;
    const void        * sRow;
    UInt                sFlag = QCM_META_CURSOR_FLAG;
    mtcColumn         * sQcmUsersIndexColumn;
    qtcMetaRangeColumn  sRangeColumn;
    scGRID              sRid; // Disk Table�� ���� Record IDentifier
    smiCursorProperties sCursorProperty;

    *aExist = ID_FALSE;

    if( gQcmUsersIndex[QCM_USER_USERID_IDX_ORDER] == NULL )
    {
        // createdb�ϴ� �����.
        // �̶��� �˻� �� �ʿ䰡 ����
    }
    else
    {
        sCursor.initialize();

        IDE_TEST( smiGetTableColumns( gQcmUsers,
                                      QCM_USERS_USER_ID_COL_ORDER,
                                      (const smiColumn**)&sQcmUsersIndexColumn )
                  != IDE_SUCCESS );

        qcm::makeMetaRangeSingleColumn(
            &sRangeColumn,
            (const mtcColumn *) sQcmUsersIndexColumn,
            (const void *) &aUserID,
            &sRange);

        SMI_CURSOR_PROP_INIT_FOR_META_INDEX_SCAN(&sCursorProperty, NULL);

        IDE_TEST(sCursor.open(
                     aSmiStmt,
                     gQcmUsers,
                     gQcmUsersIndex[QCM_USER_USERID_IDX_ORDER],
                     smiGetRowSCN(gQcmUsers),
                     NULL,
                     &sRange,
                     smiGetDefaultKeyRange(),
                     smiGetDefaultFilter(),
                     sFlag,
                     SMI_SELECT_CURSOR,
                     &sCursorProperty) != IDE_SUCCESS);
        sStage = 1;

        IDE_TEST(sCursor.beforeFirst() != IDE_SUCCESS);

        IDE_TEST(sCursor.readRow(&sRow, &sRid, SMI_FIND_NEXT) != IDE_SUCCESS);

        if ( sRow != NULL )
        {
            *aExist = ID_TRUE;
        }
        else
        {
            // Nothing to do.
        }
        
        sStage = 0;
        IDE_TEST(sCursor.close() != IDE_SUCCESS);
    }

    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    
    if ( sStage == 1 )
    {
        (void)sCursor.close();
    }

    return IDE_FAILURE;
}

IDE_RC qcmUser::getDefaultTBS(
    qcStatement     * aStatement,
    UInt              aUserID,
    scSpaceID       * aTBSID,
    scSpaceID       * aTempTBSID)
{
/***********************************************************************
 *
 * Description :
 *    ������� default tablespace �� ���Ѵ�.
 *
 * Implementation :
 *    1. ��Ÿ ĳ���κ��� USER_ID �� �ش��ϴ� �÷��� ã�Ƽ�
 *    2. ����� ����ID �� keyRange �� �����.
 *    3. Ŀ�� open ,beforeFirst, readRow �� ���ؼ� DEFAULT_TBS_ID �� ���Ѵ�.
 *    4. readRow �� ���� ������ ������ ��ȯ�Ѵ�.
 *
 ***********************************************************************/

    smiTableCursor          sCursor;
    qcmColumn             * sColumn = NULL;
    smiRange                sRange;
    qtcMetaRangeColumn      sRangeColumn;
    mtcColumn             * sMtcColumn;
    SInt                    sStage = 0;
    const void            * sRow;
    smiCursorProperties     sCursorProperty;
    scGRID                  sRid; // Disk Table�� ���� Record IDentifier

    sCursor.initialize();

    if ( aUserID == QC_SYSTEM_USER_ID )
    {// on createdb
        *aTBSID = SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC;
        if ( aTempTBSID != NULL )
        {
            *aTempTBSID = SMI_ID_TABLESPACE_SYSTEM_DISK_TEMP;
        }
    }
    else
    {
        IDE_TEST(qcmCache::getColumnByName(gQcmUsersTempInfo,
                                           (SChar*) "USER_ID",
                                           idlOS::strlen((SChar*) "USER_ID"),
                                           &sColumn) != IDE_SUCCESS);

        IDE_ASSERT( sColumn != NULL );
        
        // mtdModule ����
        IDE_TEST(mtd::moduleById( &(sColumn->basicInfo->module),
                                  sColumn->basicInfo->type.dataTypeId)
                 != IDE_SUCCESS);

        // mtlModule ����
        IDE_TEST(mtl::moduleById( &(sColumn->basicInfo->language),
                                  sColumn->basicInfo->type.languageId)
                 != IDE_SUCCESS);


        qcm::makeMetaRangeSingleColumn(
            &sRangeColumn,
            (const mtcColumn*)sColumn->basicInfo,
            (const void *) & aUserID,
            &sRange);

        SMI_CURSOR_PROP_INIT_FOR_META_INDEX_SCAN( &sCursorProperty, aStatement->mStatistics );
    
        IDE_TEST(sCursor.open(
                     QC_SMI_STMT( aStatement ),
                     gQcmUsersTempInfo->tableHandle,
                     gQcmUsersIndex[QCM_USER_USERID_IDX_ORDER],
                     smiGetRowSCN(gQcmUsersTempInfo->tableHandle),
                     NULL,
                     &sRange,
                     smiGetDefaultKeyRange(),
                     smiGetDefaultFilter(),
                     QCM_META_CURSOR_FLAG,
                     SMI_SELECT_CURSOR,
                     & sCursorProperty) != IDE_SUCCESS);
        sStage = 1;

        IDE_TEST(sCursor.beforeFirst() != IDE_SUCCESS);

        IDE_TEST(sCursor.readRow(&sRow, &sRid, SMI_FIND_NEXT) != IDE_SUCCESS);
        IDE_TEST_RAISE(sRow == NULL, ERR_NOT_EXIST_USER);

        IDE_TEST( smiGetTableColumns( gQcmUsersTempInfo->tableHandle,
                                      QCM_USERS_DEFAULT_TBS_ID_COL_ORDER,
                                      (const smiColumn**)&sMtcColumn )
                  != IDE_SUCCESS );

        *aTBSID = qcm::getSpaceID(sRow,
                                  sMtcColumn->column.offset);

        if ( aTempTBSID != NULL )
        {
            IDE_TEST( smiGetTableColumns( gQcmUsersTempInfo->tableHandle,
                                          QCM_USERS_TEMP_TBS_ID_COL_ORDER,
                                          (const smiColumn**)&sMtcColumn )
                      != IDE_SUCCESS );

            *aTempTBSID= qcm::getSpaceID( sRow, sMtcColumn->column.offset );
        }

        sStage = 0;
        IDE_TEST(sCursor.close() != IDE_SUCCESS);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_NOT_EXIST_USER);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCM_NOT_EXIST_USER));
    }
    IDE_EXCEPTION_END;

    if ( sStage == 1 )
    {
        (void)sCursor.close();
    }

    return IDE_FAILURE;
}

IDE_RC qcmUser::getCurrentDate( qcStatement     * aStatement,
                                qciUserInfo     * aResult )
{
/***********************************************************************
 *
 * Description : PROJ-2207 Password policy support
 *        select query �� ���� ���ڷ� ���� ���� ������ �ϼ��� �����´�.
 *
 * Implementation :
 *
 ***********************************************************************/
    SChar         sBuffer[QD_MAX_SQL_LENGTH] = {0,};
    idBool        sRecordExist;
    mtdBigintType sResult;

    /* BUG-365618 datediff���� DEFAULT_DATE_FORMATE�� ���ƾ� �ȴ�.. */
    idlOS::snprintf( sBuffer, QD_MAX_SQL_LENGTH,
                     "SELECT DATEDIFF( TO_DATE( '%s','DD-MON-RRRR'),SYSDATE,'DAY' ) "
                     "FROM "
                     "system_.sys_dummy_ ",
                     QD_PASSWORD_POLICY_START_DATE );
    
    IDE_TEST(qcg::runSelectOneRowforDDL( QC_SMI_STMT( aStatement ),
                                         sBuffer,
                                         &sResult,
                                         &sRecordExist,
                                         ID_FALSE )
             != IDE_SUCCESS );
    
    IDE_TEST_RAISE( sRecordExist == ID_FALSE,
                    ERR_NO_DATA_FOUND );
    
    aResult->mAccLimitOpts.mCurrentDate = (UInt) sResult;

    return IDE_SUCCESS;
    
    IDE_EXCEPTION(ERR_NO_DATA_FOUND);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QSX_NO_DATA_FOUND));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qcmUser::getPasswPolicyInfo( qcStatement     * aStatement,
                                    UInt              aUserID,
                                    qciUserInfo     * aResult )
{
/***********************************************************************
 *
 * Description : PROJ-2207 Password policy support
 *    SYS_USERS_ ��Ÿ�� PASSWORD POLICY ������ �����´�.
 *
 * Implementation :
 *    1. ��Ÿ ĳ���κ��� USER_ID �� �ش��ϴ� �÷��� ã�Ƽ�
 *    2. ����� ����ID �� keyRange �� �����.
 *    3. Ŀ�� open ,beforeFirst, readRow �� ���ؼ� password �� ���Ѵ�.
 *    4. readRow �� ���� ������ ������ ��ȯ�Ѵ�.
 *
 ***********************************************************************/
    smiTableCursor          sCursor;
    qcmColumn             * sColumn = NULL;
    smiRange                sRange;
    qtcMetaRangeColumn      sRangeColumn;
    mtcColumn             * sLockDateCol;
    mtcColumn             * sPasswExpiryDateCol;    
    mtdDateType           * sLockDateStr;
    mtdDateType           * sPasswExpiryDateStr;    
    SInt                    sStage = 0;
    const void            * sRow;
    smiCursorProperties     sCursorProperty;
    scGRID                  sRid; // Disk Table�� ���� Record IDentifier
    SInt                    sIsNull = 0;
    mtcColumn             * sPasswReuseDateCol;
    mtdDateType           * sPasswReuseDateStr;
    mtdDateType             sStartDate;
    mtdBigintType           sResult;
    mtcColumn             * sAccountLockCol;
    mtcColumn             * sPasswLimitFlagCol;
    mtdCharType           * sAccountLockStr;
    mtdCharType           * sPasswLimitFlagStr;
    mtcColumn             * sFailedCntCol;
    mtcColumn             * sReuseCntCol;
    UInt                    sFailedCntStr = 0;
    UInt                    sReuseCntStr = 0;
    mtcColumn             * sFailedLoginCol;
    mtcColumn             * sPasswLifeTimeCol;
    mtcColumn             * sPasswReuseTimeCol;
    mtcColumn             * sPasswReuseMaxCol;
    mtcColumn             * sPasswLockTimeCol;
    mtcColumn             * sPasswGraceTimeCol;
    mtcColumn             * sPasswVerifyFuncCol;
    mtdCharType           * sPasswVerifyFuncStr;
    UChar                 * sPasswPolicy;
    UChar                 * sAccountLock;

    sCursor.initialize();
   
    IDE_TEST(qcmCache::getColumnByName(gQcmUsersTempInfo,
                                       (SChar*) "USER_ID",
                                       idlOS::strlen((SChar*) "USER_ID"),
                                       &sColumn) != IDE_SUCCESS);

    IDE_ASSERT( sColumn != NULL );

    // Get Start Date
    IDE_TEST(  mtdDateInterface::makeDate( &sStartDate,
                                           QD_PASSWORD_POLICY_START_DATE_YEAR,
                                           QD_PASSWORD_POLICY_START_DATE_MON,
                                           QD_PASSWORD_POLICY_START_DATE_DAY,
                                           0,
                                           0,
                                           0,
                                           0 )
               != IDE_SUCCESS );
    
    // mtdModule ����
    IDE_TEST(mtd::moduleById( &(sColumn->basicInfo->module),
                              sColumn->basicInfo->type.dataTypeId)
             != IDE_SUCCESS);

    // mtlModule ����
    IDE_TEST(mtl::moduleById( &(sColumn->basicInfo->language),
                              sColumn->basicInfo->type.languageId)
             != IDE_SUCCESS);

    qcm::makeMetaRangeSingleColumn(
        &sRangeColumn,
        (const mtcColumn*)sColumn->basicInfo,
        (const void *) & aUserID,
        &sRange);

    SMI_CURSOR_PROP_INIT_FOR_META_INDEX_SCAN( &sCursorProperty, aStatement->mStatistics );
 
    IDE_TEST(sCursor.open(
                 QC_SMI_STMT( aStatement ),
                 gQcmUsersTempInfo->tableHandle,
                 gQcmUsersIndex[QCM_USER_USERID_IDX_ORDER],
                 smiGetRowSCN(gQcmUsersTempInfo->tableHandle),
                 NULL,
                 &sRange,
                 smiGetDefaultKeyRange(),
                 smiGetDefaultFilter(),
                 QCM_META_CURSOR_FLAG,
                 SMI_SELECT_CURSOR,
                 & sCursorProperty) != IDE_SUCCESS);

    sStage = 1;
    IDE_TEST(sCursor.beforeFirst() != IDE_SUCCESS);

    IDE_TEST(sCursor.readRow(&sRow, &sRid, SMI_FIND_NEXT) != IDE_SUCCESS);

    IDE_TEST_RAISE ( sRow == NULL, ERR_NOT_EXIST_USER );
    /* ACCOUNT_LOCK, PASSWORD_LIMIT_FLAG */
    IDE_TEST( smiGetTableColumns( gQcmUsersTempInfo->tableHandle,
                                  QCM_USERS_ACCOUNT_LOCK,
                                  (const smiColumn**)&sAccountLockCol )
              != IDE_SUCCESS );

    IDE_TEST( smiGetTableColumns( gQcmUsersTempInfo->tableHandle,
                                  QCM_USERS_PASSWORD_LIMIT_FLAG,
                                  (const smiColumn**)&sPasswLimitFlagCol )
              != IDE_SUCCESS );


    sAccountLockStr = (mtdCharType*)((UChar*)sRow +
                                     sAccountLockCol->column.offset);
    sPasswLimitFlagStr = (mtdCharType*)((UChar*)sRow +
                                        sPasswLimitFlagCol->column.offset);

    sAccountLock = sAccountLockStr->value;
    sPasswPolicy = sPasswLimitFlagStr->value;
    
    if ( idlOS::strMatch( (SChar*)sAccountLock, 1, "L", 1 ) == 0 )
    {
        aResult->mAccLimitOpts.mAccountLock = QD_ACCOUNT_LOCK;
    }
    else
    {
        // Nothing To Do
    }

    if ( idlOS::strMatch( (SChar*)sPasswPolicy, 1, "T", 1 ) == 0 )
    {
        aResult->mAccLimitOpts.mPasswLimitFlag =  QD_PASSWORD_POLICY_ENABLE;
    }
    else
    {
        // Nothing To Do
    }

    /* ACCOUNT_LOCK_DATE, PASSWORD_EXPIRY_DATE */
    IDE_TEST( smiGetTableColumns( gQcmUsersTempInfo->tableHandle,
                                  QCM_USERS_ACCOUNT_LOCK_DATE,
                                  (const smiColumn**)&sLockDateCol )
              != IDE_SUCCESS );

    IDE_TEST( smiGetTableColumns( gQcmUsersTempInfo->tableHandle,
                                  QCM_USERS_PASSWORD_EXPIRY_DATE,
                                  (const smiColumn**)&sPasswExpiryDateCol )
              != IDE_SUCCESS );

    IDE_TEST( smiGetTableColumns( gQcmUsersTempInfo->tableHandle,
                                  QCM_USERS_PASSWORD_REUSE_DATE,
                                  (const smiColumn**)&sPasswReuseDateCol )
              != IDE_SUCCESS );

    sLockDateStr        = (mtdDateType*)((SChar*)sRow + sLockDateCol->column.offset);
    sPasswExpiryDateStr = (mtdDateType*)((SChar*)sRow + sPasswExpiryDateCol->column.offset);
    sPasswReuseDateStr  = (mtdDateType*)((SChar*)sRow + sPasswReuseDateCol->column.offset);

    sIsNull = MTD_DATE_IS_NULL( sLockDateStr );

    if ( sIsNull == 0 )
    {
        IDE_TEST( mtdDateInterface::dateDiff( &sResult,
                                              &sStartDate,
                                              sLockDateStr,
                                              MTD_DATE_DIFF_DAY )
                  != IDE_SUCCESS );

        aResult->mAccLimitOpts.mLockDate = (UInt)sResult;
    }
    else
    {
        // Nothing To Do
    }

    sIsNull = MTD_DATE_IS_NULL( sPasswExpiryDateStr );

    if ( sIsNull == 0 )
    {
        IDE_TEST( mtdDateInterface::dateDiff( &sResult,
                                              &sStartDate,
                                              sPasswExpiryDateStr,
                                              MTD_DATE_DIFF_DAY )
                  != IDE_SUCCESS );

        aResult->mAccLimitOpts.mPasswExpiryDate = (UInt)sResult;
    }
    else
    {
        // Nothing To Do
    }

    sIsNull = MTD_DATE_IS_NULL( sPasswReuseDateStr );

    if ( sIsNull == 0 )
    {
        IDE_TEST( mtdDateInterface::dateDiff( &sResult,
                                              &sStartDate,
                                              sPasswReuseDateStr,
                                              MTD_DATE_DIFF_DAY )
                  != IDE_SUCCESS );

        aResult->mAccLimitOpts.mPasswReuseDate = (UInt)sResult;
    }
    else
    {
        // Nothing To Do
    }
    
    /* FAILED_LOGIN_COUNT */
    IDE_TEST( smiGetTableColumns( gQcmUsersTempInfo->tableHandle,
                                  QCM_USERS_FAILED_LOGIN_COUNT,
                                  (const smiColumn**)&sFailedCntCol )
              != IDE_SUCCESS );

    sFailedCntStr = *(UInt*)((UChar*)sRow +
                             sFailedCntCol->column.offset);

    aResult->mAccLimitOpts.mFailedCount = sFailedCntStr;

    /* FAILED_LOGIN_ATTEMPTS, PASSWORD_LIFE_TIME,
     * PASSWORD_REUSE_TIME, PASSWORD_REUSE_MAX,
     * PASSWORD_LOCK_TIME, PASSWORD_GRACE_TIME,
     * PASSWORD_VERIFY_FUNCTION */
    IDE_TEST( smiGetTableColumns( gQcmUsersTempInfo->tableHandle,
                                  QCM_USERS_FAILED_LOGIN_ATTEMPTS,
                                  (const smiColumn**)&sFailedLoginCol )
              != IDE_SUCCESS );

    IDE_TEST( smiGetTableColumns( gQcmUsersTempInfo->tableHandle,
                                  QCM_USERS_PASSWORD_LIFE_TIME,
                                  (const smiColumn**)&sPasswLifeTimeCol )
              != IDE_SUCCESS );

    IDE_TEST( smiGetTableColumns( gQcmUsersTempInfo->tableHandle,
                                  QCM_USERS_PASSWORD_REUSE_TIME,
                                  (const smiColumn**)&sPasswReuseTimeCol )
              != IDE_SUCCESS );

    IDE_TEST( smiGetTableColumns( gQcmUsersTempInfo->tableHandle,
                                  QCM_USERS_PASSWORD_REUSE_MAX,
                                  (const smiColumn**)&sPasswReuseMaxCol )
              != IDE_SUCCESS );

    IDE_TEST( smiGetTableColumns( gQcmUsersTempInfo->tableHandle,
                                  QCM_USERS_PASSWORD_LOCK_TIME,
                                  (const smiColumn**)&sPasswLockTimeCol )
              != IDE_SUCCESS );

    IDE_TEST( smiGetTableColumns( gQcmUsersTempInfo->tableHandle,
                                  QCM_USERS_PASSWORD_GRACE_TIME,
                                  (const smiColumn**)&sPasswGraceTimeCol )
              != IDE_SUCCESS );

    IDE_TEST( smiGetTableColumns( gQcmUsersTempInfo->tableHandle,
                                  QCM_USERS_PASSWORD_VERIFY_FUNCTION,
                                  (const smiColumn**)&sPasswVerifyFuncCol )
              != IDE_SUCCESS );

    aResult->mAccLimitOpts.mFailedLoginAttempts = *(UInt*)((UChar*)sRow +
                                                           sFailedLoginCol->column.offset);
    aResult->mAccLimitOpts.mPasswLifeTime       = *(UInt*)((UChar*)sRow +
                                                           sPasswLifeTimeCol->column.offset);
    aResult->mAccLimitOpts.mPasswReuseTime      = *(UInt*)((UChar*)sRow +
                                                           sPasswReuseTimeCol->column.offset);
    aResult->mAccLimitOpts.mPasswReuseMax       = *(UInt*)((UChar*)sRow +
                                                           sPasswReuseMaxCol->column.offset);
    aResult->mAccLimitOpts.mPasswLockTime       = *(UInt*)((UChar*)sRow +
                                                           sPasswLockTimeCol->column.offset);
    aResult->mAccLimitOpts.mPasswGraceTime      = *(UInt*)((UChar*)sRow +
                                                           sPasswGraceTimeCol->column.offset);
    sPasswVerifyFuncStr = (mtdCharType*)((SChar*)sRow +
                                         sPasswVerifyFuncCol->column.offset);

    idlOS::strncpy( aResult->mAccLimitOpts.mPasswVerifyFunc,
                    (SChar*)&(sPasswVerifyFuncStr->value),
                    sPasswVerifyFuncStr->length );
    aResult->mAccLimitOpts.mPasswVerifyFunc[sPasswVerifyFuncStr->length ] = '\0';

    /* PASSWORD_REUSE_COUNT */
    IDE_TEST( smiGetTableColumns( gQcmUsersTempInfo->tableHandle,
                                  QCM_USERS_PASSWORD_REUSE_COUNT,
                                  (const smiColumn**)&sReuseCntCol )
              != IDE_SUCCESS );

    sReuseCntStr = *(UInt*)((UChar*)sRow +
                             sReuseCntCol->column.offset);

    aResult->mAccLimitOpts.mReuseCount = sReuseCntStr;

    sStage = 0;
    IDE_TEST(sCursor.close() != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_NOT_EXIST_USER);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCM_NOT_EXIST_USER));
    }
    IDE_EXCEPTION_END;

    if ( sStage == 1 )
    {
        (void)sCursor.close();
    }

    return IDE_FAILURE;
}

IDE_RC qcmUser::updateFailedCount( smiStatement * aStatement,
                                   qciUserInfo  * aUserInfo )
{

/***********************************************************************
 *
 * Description : PROJ-2207 Password policy support
 *      SYS_USERS_ ��Ÿ�� FAILED_LOGIN_ATTEMPTS ���� ����.
 *
 *      CONNECT FAILED : +1, CONNECT SUCCESS : 0
 *
 * Implementation :
 *      aUserID �������� �ش� ����ڸ�.
 *
 ***********************************************************************/

    SChar          sBuffer[QD_MAX_SQL_LENGTH];
    vSLong         sRowCnt;

    if ( aUserInfo->mAccLimitOpts.mFailedLoginAttempts != 0 )
    {        
        if (( aUserInfo->mAccLimitOpts.mAccLockStatus == QCI_ACCOUNT_OPEN ) ||
            ( aUserInfo->mAccLimitOpts.mAccLockStatus == QCI_ACCOUNT_LOCKED_TO_OPEN ) ||
            ( aUserInfo->mAccLimitOpts.mAccLockStatus == QCI_ACCOUNT_OPEN_TO_LOCKED ))
        {
            /* BUG-44303 Password Policy�� ������ ���, �ش� User�� Log-In�� ��, �׻� Meta Table�� �����մϴ�. */
            idlOS::snprintf( sBuffer, QD_MAX_SQL_LENGTH,
                             "UPDATE DBA_USERS_ "
                             "SET "
                             "FAILED_LOGIN_COUNT = %"ID_UINT32_FMT" "
                             "WHERE USER_ID = %"ID_UINT32_FMT" "
                             "  AND FAILED_LOGIN_COUNT != %"ID_UINT32_FMT" ",
                             aUserInfo->mAccLimitOpts.mUserFailedCnt,
                             aUserInfo->userID,
                             aUserInfo->mAccLimitOpts.mUserFailedCnt );
    
            IDE_TEST(qcg::runDMLforDDL( aStatement,
                                        sBuffer,
                                        &sRowCnt )
                     != IDE_SUCCESS );

            IDE_TEST_RAISE( sRowCnt > 1, ERR_META_CRASH );
        }
        else
        {
            // Nothing To Do
        }
    }
    else
    {
        // Nothing To Do
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_META_CRASH);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCM_META_CRASH));
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qcmUser::updateLockDate( smiStatement * aStatement,
                                qciUserInfo  * aUserInfo )
{

/***********************************************************************
 *
 * Description : PROJ-2207 Password policy support
 *      SYS_USERS_ ��Ÿ�� LOCK_DATE ���� ����.
 *
 * Implementation :
 *    1. ACCOUNT_STATUS : LOCKED
 *         - LOCK_DATE�� �� ������ ������ ����
 *         - LOCK_DATE�� FAILED_LOGIN_ATTEMPTS, PASSWORD_LOCK_TIME ����
 *           ���� �����ȴ�.
 *    2. ACCOUNT_STATUS : LOCKED -> OPEN
 *         - LOCK_DATE�� ���� NULL�� ����.
 *         - �α��� �Ǿ��� ���� PASSWORD_LOCK_TIME ���� ���� ���� ����
 *
 ***********************************************************************/

    SChar          sBuffer[QD_MAX_SQL_LENGTH];
    vSLong         sRowCnt;
    
    if ( aUserInfo->mAccLimitOpts.mAccLockStatus == QCI_ACCOUNT_LOCKED_TO_OPEN )
    {
        // BUG-41230 SYS_USERS_ => DBA_USERS_
        /* PASSWORD_LOCK_TIME
         * ACCOUNT_LOCK = N
         * ACCOUNT_LOCK_DATE = NULL */
        /* BUG-44303 Password Policy�� ������ ���, �ش� User�� Log-In�� ��, �׻� Meta Table�� �����մϴ�. */
        idlOS::snprintf( sBuffer, QD_MAX_SQL_LENGTH,
                         "UPDATE DBA_USERS_ "
                         "SET "
                         "ACCOUNT_LOCK = 'N', "
                         "ACCOUNT_LOCK_DATE = NULL "
                         "WHERE USER_ID = %"ID_UINT32_FMT" "
                         "  AND ( ACCOUNT_LOCK != 'N' OR ACCOUNT_LOCK_DATE IS NOT NULL ) ",
                         aUserInfo->userID );
        
        IDE_TEST(qcg::runDMLforDDL( aStatement,
                                    sBuffer,
                                    &sRowCnt )
                 != IDE_SUCCESS );
            
        IDE_TEST_RAISE( sRowCnt > 1, ERR_META_CRASH );
    }
    else
    {
        // Nothing To Do
    }

    if ( aUserInfo->mAccLimitOpts.mAccLockStatus == QCI_ACCOUNT_OPEN_TO_LOCKED )
    {
        // BUG-41230 SYS_USERS_ => DBA_USERS_
        /* FAILED_LOGIN_ATTEMPTS
         * ACCOUNT_LOCK = L
         * ACCOUNT_LOCK_DATE = 20010101 + ���� �ϼ� */
        /* BUG-44303 Password Policy�� ������ ���, �ش� User�� Log-In�� ��, �׻� Meta Table�� �����մϴ�. */
        idlOS::snprintf( sBuffer, QD_MAX_SQL_LENGTH,
                         "UPDATE DBA_USERS_ "
                         "SET "
                         "ACCOUNT_LOCK = 'L', "
                         "ACCOUNT_LOCK_DATE = "
                         "TO_DATE( '%s','%s' ) + %"ID_UINT32_FMT" "
                         "WHERE USER_ID = %"ID_UINT32_FMT" "
                         "  AND ( ACCOUNT_LOCK != 'L' OR "
                         "        ACCOUNT_LOCK_DATE != TO_DATE( '%s', '%s' ) + %"ID_UINT32_FMT" ) ",
                         QD_PASSWORD_POLICY_START_DATE,
                         QD_PASSWORD_POLICY_START_DATE_FORMAT,
                         aUserInfo->mAccLimitOpts.mCurrentDate,
                         aUserInfo->userID,
                         QD_PASSWORD_POLICY_START_DATE,
                         QD_PASSWORD_POLICY_START_DATE_FORMAT,
                         aUserInfo->mAccLimitOpts.mCurrentDate );

        IDE_TEST(qcg::runDMLforDDL( aStatement,
                                    sBuffer,
                                    &sRowCnt )
                 != IDE_SUCCESS );
            
        IDE_TEST_RAISE( sRowCnt > 1, ERR_META_CRASH );
    }
    else
    {
        // Nothing To Do
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_META_CRASH);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCM_META_CRASH));
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qcmUser::findTableListByUserID( qcStatement * aStatement,
                                       UInt          aUserID,
                                       qcmTableInfoList **aTableInfoList)
{
/***********************************************************************
 *
 * Description :
 *    executeDropUserCascade �κ��� ȣ��, drop ������ ���̺���
 *    ����Ʈ�� �����ش�.
 *
 * Implementation :
 *    1. SYS_TABLES_ ���̺��� USER_ID, TABLE_OID,TABLE_ID �÷��� ���� �д�.
 *    2. ��õ� userID �� keyRange �� �����.
 *    3. �� row �� �����鼭 tableOID, tableID �� ���� ����, ĳ����
 *       qcmTableInfo �� ���̺� �ڵ��� ���Ѵ�.
 *    4. drop �ؾ��� qcmTableInfo �� list �� �����Ͽ� ��ȯ�Ѵ�.
 *
 ***********************************************************************/

    smiRange              sRange;
    mtcColumn           * sColumn;
    mtcColumn           * sTableOidCol;
    mtcColumn           * sTableIdCol;
    mtcColumn           * sTableTypeCol;
    mtcColumn           * sHiddenCol;
    qtcMetaRangeColumn    sRangeColumn;
    SInt                  sStage = 0;
    const void          * sRow;
    smiTableCursor        sCursor;
    UInt                  sTableID;
    qcmTableInfo        * sTableInfo;
    smSCN                 sSCN;
    void                * sTableHandle;
    qcmTableInfoList    * sTableInfoList     = NULL;
    qcmTableInfoList    * sFirstTableInfoList = NULL;
    smiCursorProperties   sCursorProperty;
    scGRID                sRid; // Disk Table�� ���� Record IDentifier

    mtcColumn           * sTableNameCol;
    UChar               * sTableType;
    mtdCharType         * sTableTypeStr;
    UChar               * sHidden;
    mtdCharType         * sHiddenStr;

    sCursor.initialize();

    //-----------------------------------
    // search SYS_TABLES_
    //-----------------------------------
    IDE_TEST( smiGetTableColumns( gQcmTables,
                                  QCM_TABLES_USER_ID_COL_ORDER,
                                  (const smiColumn**)&sColumn )
              != IDE_SUCCESS );
    IDE_TEST( smiGetTableColumns( gQcmTables,
                                  QCM_TABLES_TABLE_OID_COL_ORDER,
                                  (const smiColumn**)&sTableOidCol )
              != IDE_SUCCESS );
    IDE_TEST( smiGetTableColumns( gQcmTables,
                                  QCM_TABLES_TABLE_ID_COL_ORDER,
                                  (const smiColumn**)&sTableIdCol )
              != IDE_SUCCESS );
    IDE_TEST( smiGetTableColumns( gQcmTables,
                                  QCM_TABLES_TABLE_NAME_COL_ORDER,
                                  (const smiColumn**)&sTableNameCol )
              != IDE_SUCCESS );
    IDE_TEST( smiGetTableColumns( gQcmTables,
                                  QCM_TABLES_TABLE_TYPE_COL_ORDER,
                                  (const smiColumn**)&sTableTypeCol )
              != IDE_SUCCESS );
    IDE_TEST( smiGetTableColumns( gQcmTables,
                                  QCM_TABLES_HIDDEN_COL_ORDER,
                                  (const smiColumn**)&sHiddenCol )
              != IDE_SUCCESS );

    (void)qcm::makeMetaRangeSingleColumn(&sRangeColumn,
                                         (const mtcColumn*)sColumn,
                                         (const void *) &(aUserID),
                                         &sRange);

    SMI_CURSOR_PROP_INIT_FOR_META_INDEX_SCAN(&sCursorProperty, aStatement->mStatistics);

    IDE_TEST(sCursor.open(
                 QC_SMI_STMT( aStatement ),
                 gQcmTables,
                 gQcmTablesIndex[QCM_TABLES_USERID_IDX_ORDER],
                 smiGetRowSCN(gQcmTables),
                 NULL,
                 &sRange,
                 smiGetDefaultKeyRange(),
                 smiGetDefaultFilter(),
                 QCM_META_CURSOR_FLAG,
                 SMI_SELECT_CURSOR,
                 & sCursorProperty) != IDE_SUCCESS);
    sStage = 1;
    IDE_TEST(sCursor.beforeFirst() != IDE_SUCCESS);

    IDE_TEST(sCursor.readRow(&sRow, &sRid, SMI_FIND_NEXT) != IDE_SUCCESS);
    if (sRow == NULL)
    {
        *aTableInfoList = NULL;
    }
    while (sRow != NULL)
    {
        sTableID = *(UInt *) ((UChar*)sRow +
                              sTableIdCol->column.offset);

        sTableTypeStr = (mtdCharType*)
            ((UChar*) sRow + sTableTypeCol->column.offset);
        sTableType = sTableTypeStr->value;

        sHiddenStr = (mtdCharType*)
            ((UChar*) sRow + sHiddenCol->column.offset);
        sHidden = sHiddenStr->value;

        // PROJ-1624 Global Non-partitioned Index
        // hidden table�� hidden table�� ������ ��ü�� ����ó���Ѵ�.
        // if we want to get the table is sequence or not
        /* BUG-35460 Add TABLE_TYPE G in SYS_TABLES_ */
        if ( ( ( idlOS::strMatch( (SChar*)sTableType, sTableTypeStr->length, "T", 1 ) == 0 ) ||
               ( idlOS::strMatch( (SChar*)sTableType, sTableTypeStr->length, "V", 1 ) == 0 ) ||
               ( idlOS::strMatch( (SChar*)sTableType, sTableTypeStr->length, "M", 1 ) == 0 ) ||
               ( idlOS::strMatch( (SChar*)sTableType, sTableTypeStr->length, "A", 1 ) == 0 ) ||
               ( idlOS::strMatch( (SChar*)sTableType, sTableTypeStr->length, "Q", 1 ) == 0 ) ||
               ( idlOS::strMatch( (SChar*)sTableType, sTableTypeStr->length, "G", 1 ) == 0 ) ||
               ( idlOS::strMatch( (SChar*)sTableType, sTableTypeStr->length, "D", 1 ) == 0 ) ||
               ( idlOS::strMatch( (SChar*)sTableType, sTableTypeStr->length, "R", 1 ) == 0 ) ) /* PROJ-2441 flashback */
             &&
             ( idlOS::strMatch( (SChar*)sHidden, sHiddenStr->length, "N", 1 ) == 0 ) )
        {
            IDE_TEST(qcm::getTableInfoByID(aStatement,
                                           sTableID,
                                           &sTableInfo,
                                           &sSCN,
                                           &sTableHandle) != IDE_SUCCESS);

            IDE_TEST(qcm::validateAndLockTable(aStatement,
                                               sTableHandle,
                                               sSCN,
                                               SMI_TABLE_LOCK_X)
                     != IDE_SUCCESS);

            IDE_TEST_RAISE(sTableInfo->replicationCount > 0,
                           ERR_DDL_WITH_REPLICATED_TABLE);

            //proj-1608:replicationCount�� 0�� �� recovery count�� �׻� 0�̾�� ��
            IDE_DASSERT(sTableInfo->replicationRecoveryCount == 0);

            IDU_LIMITPOINT("qcmUser::findTableListByUserID::malloc");
            IDE_TEST(aStatement->qmxMem->alloc(ID_SIZEOF(qcmTableInfoList),
                                               (void**)&(sTableInfoList))
                     != IDE_SUCCESS);

            sTableInfoList->tableInfo = sTableInfo;
            sTableInfoList->childInfo = NULL;

            if (sFirstTableInfoList == NULL)
            {
                sTableInfoList->next = NULL;
                sFirstTableInfoList = sTableInfoList;
            }
            else
            {
                sTableInfoList->next = sFirstTableInfoList;
                sFirstTableInfoList = sTableInfoList;
            }
        }
        else
        {
            // Nothing to do.
        }

        IDE_TEST(sCursor.readRow(&sRow, &sRid, SMI_FIND_NEXT) != IDE_SUCCESS);
    }
    sStage = 0;

    IDE_TEST(sCursor.close() != IDE_SUCCESS);

    *aTableInfoList = sFirstTableInfoList;

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_DDL_WITH_REPLICATED_TABLE)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDD_DDL_WITH_REPLICATED_TBL));
    }
    IDE_EXCEPTION_END;

    switch(sStage)
    {
        case 1:
            sCursor.close();
            break;
    }

    return IDE_FAILURE;
}

IDE_RC qcmUser::findIndexListByUserID( qcStatement * aStatement,
                                       UInt          aUserID,
                                       qcmIndexInfoList **aIndexInfoList)
{
/***********************************************************************
 *
 * Description :
 *    executeDropUserCascade �κ��� ȣ��,
 *    drop ������ �ε����� ����Ʈ�� ���� �ش�.
 *
 * Implementation :
 *
 *
 ***********************************************************************/

    SInt                    sStage = 0;
    UInt                    sTableID;
    qcmTableInfo          * sTableInfo;
    smSCN                   sSCN;
    smiRange                sRange;
    mtcColumn             * sUserID;
    mtcColumn             * sIndexIDCol;
    mtcColumn             * sTableIDCol;
    mtcColumn             * sIsPartitionedCol;
    mtcColumn             * sIndexTableIDCol;
    mtdCharType           * sIsPartitionedStr;
    UInt                    sIndexTableID;
    qtcMetaRangeColumn      sRangeColumn;
    const void            * sRow;
    smiTableCursor          sCursor;
    smiCursorProperties     sCursorProperty;
    scGRID                  sRid; // Disk Table�� ���� Record IDentifier
    qcmIndexInfoList      * sFirstIndexInfoList = NULL;
    qcmIndexInfoList      * sIndexInfoList;
    void                  * sTableHandle;

    sCursor.initialize();

    //-----------------------------------
    // search SYS_INDICES_
    //-----------------------------------
    IDE_TEST( smiGetTableColumns( gQcmIndices,
                                  QCM_INDICES_USER_ID_COL_ORDER,
                                  (const smiColumn**)&sUserID )
              != IDE_SUCCESS );
    IDE_TEST( smiGetTableColumns( gQcmIndices,
                                  QCM_INDICES_INDEX_ID_COL_ORDER,
                                  (const smiColumn**)&sIndexIDCol )
              != IDE_SUCCESS );
    IDE_TEST( smiGetTableColumns( gQcmIndices,
                                  QCM_INDICES_TABLE_ID_COL_ORDER,
                                  (const smiColumn**)&sTableIDCol )
              != IDE_SUCCESS );
    IDE_TEST( smiGetTableColumns( gQcmIndices,
                                  QCM_INDICES_IS_PARTITIONED_COL_ORDER,
                                  (const smiColumn**)&sIsPartitionedCol )
              != IDE_SUCCESS );
    IDE_TEST( smiGetTableColumns( gQcmIndices,
                                  QCM_INDICES_INDEX_TABLE_ID_COL_ORDER,
                                  (const smiColumn**)&sIndexTableIDCol )
              != IDE_SUCCESS );
    
    (void)qcm::makeMetaRangeSingleColumn( &sRangeColumn,
                                          (const mtcColumn*)sUserID,
                                          (const void *) &(aUserID),
                                          &sRange );

    SMI_CURSOR_PROP_INIT_FOR_META_INDEX_SCAN( &sCursorProperty, aStatement->mStatistics );
    
    IDE_TEST(sCursor.open(
                 QC_SMI_STMT( aStatement ),
                 gQcmIndices,
                 gQcmIndicesIndex[QCM_INDICES_USERID_INDEXNAME_IDX_ORDER],
                 smiGetRowSCN(gQcmIndices),
                 NULL,
                 &sRange,
                 smiGetDefaultKeyRange(),
                 smiGetDefaultFilter(),
                 QCM_META_CURSOR_FLAG,
                 SMI_SELECT_CURSOR,
                 & sCursorProperty) != IDE_SUCCESS);

    sStage = 1;
    IDE_TEST(sCursor.beforeFirst() != IDE_SUCCESS);

    IDE_TEST(sCursor.readRow(&sRow, &sRid, SMI_FIND_NEXT) != IDE_SUCCESS);

    while (sRow != NULL)
    {
        sTableID = *(UInt *) ((UChar*)sRow +
                              sTableIDCol->column.offset);
        sIsPartitionedStr = (mtdCharType *) ((UChar *)sRow +
                                             sIsPartitionedCol->column.offset);
        sIndexTableID = *(UInt*) ((UChar*)sRow +
                                  sIndexTableIDCol->column.offset);
        
        IDE_TEST(qcm::getTableInfoByID(aStatement,
                                       sTableID,
                                       &sTableInfo,
                                       &sSCN,
                                       &sTableHandle) != IDE_SUCCESS);

        IDE_TEST(qcm::validateAndLockTable(aStatement,
                                           sTableHandle,
                                           sSCN,
                                           SMI_TABLE_LOCK_X)
                 != IDE_SUCCESS);

        if (sTableInfo->tableOwnerID != aUserID)
        {
            // BUG-16565
            IDE_TEST_RAISE(sTableInfo->replicationCount > 0,
                           ERR_DDL_WITH_REPLICATED_TABLE);

            //proj-1608:replicationCount�� 0�� �� recovery count�� �׻� 0�̾�� ��
            IDE_DASSERT(sTableInfo->replicationRecoveryCount == 0);

            IDU_LIMITPOINT("qcmUser::findIndexListByUserID::malloc");
            IDE_TEST(aStatement->qmxMem->cralloc(ID_SIZEOF(qcmIndexInfoList),
                                                 (void**)&(sIndexInfoList))
                     != IDE_SUCCESS);

            sIndexInfoList->indexID = *(UInt *) ((UChar*)sRow +
                                                 sIndexIDCol->column.offset);
            sIndexInfoList->tableInfo = sTableInfo;
            sIndexInfoList->childInfo = NULL;
            
            sIndexInfoList->indexTable.tableID = sIndexTableID;
            
            if ( idlOS::strMatch( (SChar *)&(sIsPartitionedStr->value),
                                  sIsPartitionedStr->length,
                                  "T",
                                  1 ) == 0 )
            {
                sIndexInfoList->isPartitionedIndex = ID_TRUE;
            }
            else
            {
                sIndexInfoList->isPartitionedIndex = ID_FALSE;
            }

            if ( sTableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
            {
                if ( sIndexInfoList->isPartitionedIndex == ID_TRUE )
                {
                    // partitioned index�� index table�� ���� �� ����.
                    IDE_TEST_RAISE( sIndexInfoList->indexTable.tableID != 0,
                                    ERR_META_CRASH );
                }
                else
                {
                    // non-partitioned index��� index table�� �־���Ѵ�.
                    IDE_TEST_RAISE( sIndexInfoList->indexTable.tableID == 0,
                                    ERR_META_CRASH );
                }
            }
            else
            {
                // non-partitioned table�� partitioned index�� ���� �� ����.
                IDE_TEST_RAISE( sIndexInfoList->isPartitionedIndex == ID_TRUE,
                                ERR_META_CRASH );

                // non-partitioned table�� index table�� ���� �� ����.
                IDE_TEST_RAISE( sIndexInfoList->indexTable.tableID != 0,
                                ERR_META_CRASH );
            }

            if (sFirstIndexInfoList == NULL)
            {
                sIndexInfoList->next = NULL;
                sFirstIndexInfoList  = sIndexInfoList;
            }
            else
            {
                sIndexInfoList->next = sFirstIndexInfoList;
                sFirstIndexInfoList  = sIndexInfoList;
            }
        }

        IDE_TEST(sCursor.readRow(&sRow, &sRid, SMI_FIND_NEXT) != IDE_SUCCESS);
    }

    sStage = 0;

    IDE_TEST(sCursor.close() != IDE_SUCCESS);

    *aIndexInfoList = sFirstIndexInfoList;

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_DDL_WITH_REPLICATED_TABLE)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDD_DDL_WITH_REPLICATED_TBL));
    }
    IDE_EXCEPTION(ERR_META_CRASH);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCM_META_CRASH));
    }
    IDE_EXCEPTION_END;

    switch(sStage)
    {
        case 1:
            sCursor.close();
            break;
    }

    return IDE_FAILURE;
}

IDE_RC qcmUser::findSequenceListByUserID( qcStatement         * aStatement,
                                          UInt                  aUserID,
                                          qcmSequenceInfoList **aSequenceInfoList)
{
/***********************************************************************
 *
 * Description :
 *    BUG-16980
 *    executeDropUserCascade �κ��� ȣ��, drop ������ ���̺���
 *    ����Ʈ�� �����ش�.
 *
 * Implementation :
 *    1. SYS_TABLES_ ���̺��� USER_ID, TABLE_OID,TABLE_ID �÷��� ���� �д�.
 *    2. ��õ� userID �� keyRange �� �����.
 *    3. �� row �� �����鼭 tableOID, tableID �� ���� ����,
 *       qcmSequenceInfo�� �����Ѵ�.
 *    4. drop �ؾ��� qcmSequenceInfo �� list �� �����Ͽ� ��ȯ�Ѵ�.
 *
 ***********************************************************************/

    smiRange              sRange;
    mtcColumn           * sColumn;
    mtcColumn           * sTableOidCol;
    mtcColumn           * sTableIdCol;
    mtcColumn           * sTableTypeCol;
    qtcMetaRangeColumn    sRangeColumn;
    SInt                  sStage = 0;
    const void          * sRow;
    smiTableCursor        sCursor;
    void                * sSequenceHandle;
    qcmSequenceInfoList * sSequenceInfoList = NULL;
    qcmSequenceInfoList * sFirstSequenceInfoList = NULL;
    smiCursorProperties   sCursorProperty;
    scGRID                sRid; // Disk Table�� ���� Record IDentifier

    mtcColumn           * sTableNameCol;
    mtdCharType         * sTableNameStr;
    UChar               * sTableType;
    mtdCharType         * sTableTypeStr;

    sCursor.initialize();

    //-----------------------------------
    // search SYS_TABLES_
    //-----------------------------------
    IDE_TEST( smiGetTableColumns( gQcmTables,
                                  QCM_TABLES_USER_ID_COL_ORDER,
                                  (const smiColumn**)&sColumn )
              != IDE_SUCCESS );
    IDE_TEST( smiGetTableColumns( gQcmTables,
                                  QCM_TABLES_TABLE_OID_COL_ORDER,
                                  (const smiColumn**)&sTableOidCol )
              != IDE_SUCCESS );
    IDE_TEST( smiGetTableColumns( gQcmTables,
                                  QCM_TABLES_TABLE_ID_COL_ORDER,
                                  (const smiColumn**)&sTableIdCol )
              != IDE_SUCCESS );
    IDE_TEST( smiGetTableColumns( gQcmTables,
                                  QCM_TABLES_TABLE_NAME_COL_ORDER,
                                  (const smiColumn**)&sTableNameCol )
              != IDE_SUCCESS );
    IDE_TEST( smiGetTableColumns( gQcmTables,
                                  QCM_TABLES_TABLE_TYPE_COL_ORDER,
                                  (const smiColumn**)&sTableTypeCol )
              != IDE_SUCCESS );

    (void)qcm::makeMetaRangeSingleColumn(&sRangeColumn,
                                         (const mtcColumn*)sColumn,
                                         (const void *) &(aUserID),
                                         &sRange);

    SMI_CURSOR_PROP_INIT_FOR_META_INDEX_SCAN( &sCursorProperty, aStatement->mStatistics );
    
    IDE_TEST(sCursor.open(
                 QC_SMI_STMT( aStatement ),
                 gQcmTables,
                 gQcmTablesIndex[QCM_TABLES_USERID_IDX_ORDER],
                 smiGetRowSCN(gQcmTables),
                 NULL,
                 &sRange,
                 smiGetDefaultKeyRange(),
                 smiGetDefaultFilter(),
                 QCM_META_CURSOR_FLAG,
                 SMI_SELECT_CURSOR,
                 & sCursorProperty) != IDE_SUCCESS);
    sStage = 1;
    IDE_TEST(sCursor.beforeFirst() != IDE_SUCCESS);

    IDE_TEST(sCursor.readRow(&sRow, &sRid, SMI_FIND_NEXT) != IDE_SUCCESS);
    if (sRow == NULL)
    {
        *aSequenceInfoList = NULL;
    }
    while (sRow != NULL)
    {
        sTableNameStr = (mtdCharType*)
            ((UChar*)sRow + sTableNameCol->column.offset);

        sTableTypeStr = (mtdCharType*)
            ((UChar*) sRow + sTableTypeCol->column.offset);
        sTableType = sTableTypeStr->value;

        // if we want to get the table is sequence or not
        // QUEUE_SEQUENCE�� QUEUE_TABLE�� ���� �����ǹǷ� skip�Ѵ�.
        if ( idlOS::strMatch( (SChar*)sTableType, sTableTypeStr->length, "S", 1 ) == 0 )
        {
            IDU_LIMITPOINT("qcmUser::findSequenceListByUserID::malloc");
            IDE_TEST(aStatement->qmxMem->alloc(ID_SIZEOF(qcmSequenceInfoList),
                                               (void**)&(sSequenceInfoList))
                     != IDE_SUCCESS);

            IDE_TEST(qcm::getSequenceInfoByName(QC_SMI_STMT(aStatement),
                                                aUserID,
                                                sTableNameStr->value,
                                                sTableNameStr->length,
                                                & sSequenceInfoList->sequenceInfo,
                                                & sSequenceHandle )
                     != IDE_SUCCESS);

            if (sFirstSequenceInfoList == NULL)
            {
                sSequenceInfoList->next = NULL;
                sFirstSequenceInfoList = sSequenceInfoList;
            }
            else
            {
                sSequenceInfoList->next = sFirstSequenceInfoList;
                sFirstSequenceInfoList = sSequenceInfoList;
            }
        }
        else
        {
            // Nothing to do.
        }

        IDE_TEST(sCursor.readRow(&sRow, &sRid, SMI_FIND_NEXT) != IDE_SUCCESS);
    }
    sStage = 0;

    IDE_TEST(sCursor.close() != IDE_SUCCESS);

    *aSequenceInfoList = sFirstSequenceInfoList;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch(sStage)
    {
        case 1:
            sCursor.close();
            break;
    }

    return IDE_FAILURE;
}

IDE_RC qcmUser::findProcListByUserID( qcStatement     * aStatement,
                                      UInt              aUserID,
                                      qcmProcInfoList **aProcInfoList)
{
/***********************************************************************
 *
 * Description :
 *    executeDropUserCascade �κ��� ȣ��,
 *    drop ������ ���ν����� ����Ʈ�� �����ش�.
 *
 * Implementation :
 *
 *
 ***********************************************************************/

    qcmProcInfoList  * sFirstProcInfoList = NULL;
    qcmProcInfoList  * sProcInfoList      = NULL;
    qcmProcedures    * sQcmProcedures     = NULL;
    vSLong             sRowCount          = 0;
    vSLong             sRealRowCount      = 0;
    SInt               i;

    IDE_TEST( qcmProc::procSelectCountWithUserId (
                  aStatement,
                  aUserID,
                  & sRowCount ) != IDE_SUCCESS );

    if ( sRowCount > 0 )
    {
        // BUGBUG X lock on sys_procedure table before selecting.
        IDE_TEST(QC_QMX_MEM( aStatement )->
                 alloc( idlOS::align8((UInt)ID_SIZEOF(qcmProcedures)) * sRowCount,
                        (void**)&sQcmProcedures)
                 != IDE_SUCCESS);

        IDE_TEST( qcmProc::procSelectAllWithUserId (
                      aStatement,
                      aUserID,
                      sRowCount,
                      & sRealRowCount,
                      sQcmProcedures ) != IDE_SUCCESS );

        IDE_TEST_RAISE( sRowCount != sRealRowCount,
                        err_rowcount_mismatch );

        for (i = 0; i < sRealRowCount; i++)
        {
            IDE_TEST(QC_QMX_MEM( aStatement)->
                     alloc(ID_SIZEOF(qcmProcInfoList),
                           (void**)&(sProcInfoList))
                     != IDE_SUCCESS);

            sProcInfoList->procName = sQcmProcedures[i].procName;
            sProcInfoList->procOID  = sQcmProcedures[i].procOID;

            sProcInfoList->next = sFirstProcInfoList;
            sFirstProcInfoList  = sProcInfoList;
        }
    }

    *aProcInfoList = sFirstProcInfoList;

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_rowcount_mismatch);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QSX_INTERNAL_SERVER_ERROR_ARG,
                                "sRowCount != sRealRowCount"));
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qcmUser::findTriggerListByUserID( qcStatement * aStatement,
                                         UInt          aUserID,
                                         qcmTriggerInfoList **aTriggerInfoList)
{
/***********************************************************************
 *
 * Description :
 *    executeDropUserCascade �κ��� ȣ��,
 *    drop ������ Ʈ������ ����Ʈ�� �����ش�.
 *
 * Implementation :
 *
 *
 ***********************************************************************/

    SInt                    sStage = 0;
    UInt                    sTableID;
    qcmTableInfo          * sTableInfo;
    smSCN                   sSCN;
    smiRange                sRange;
    mtcColumn             * sUserIDCol;
    mtcColumn             * sTriggerOIDCol;
    mtcColumn             * sTableIDCol;
    qtcMetaRangeColumn      sRangeColumn;
    const void            * sRow;
    smiTableCursor          sCursor;
    smiCursorProperties     sCursorProperty;
    scGRID                  sRid; // Disk Table�� ���� Record IDentifier
    qcmTriggerInfoList    * sFirstTriggerInfoList = NULL;
    qcmTriggerInfoList    * sTriggerInfoList;
    void                  * sTableHandle;

    sCursor.initialize();

    //-----------------------------------
    // search SYS_TRIGGERS_
    //-----------------------------------

    IDE_TEST( smiGetTableColumns( gQcmTriggers,
                                  QCM_TRIGGERS_USER_ID,
                                  (const smiColumn**)&sUserIDCol )
              != IDE_SUCCESS );

    IDE_TEST( smiGetTableColumns( gQcmTriggers,
                                  QCM_TRIGGERS_TRIGGER_OID,
                                  (const smiColumn**)&sTriggerOIDCol )
              != IDE_SUCCESS );

    IDE_TEST( smiGetTableColumns( gQcmTriggers,
                                  QCM_TRIGGERS_TABLE_ID,
                                  (const smiColumn**)&sTableIDCol )
              != IDE_SUCCESS );


    (void)qcm::makeMetaRangeSingleColumn( &sRangeColumn,
                                          (const mtcColumn*)sUserIDCol,
                                          (const void *) &(aUserID),
                                          &sRange );

    SMI_CURSOR_PROP_INIT_FOR_META_INDEX_SCAN( &sCursorProperty, aStatement->mStatistics );
    
    IDE_TEST(sCursor.open(
                 QC_SMI_STMT( aStatement ),
                 gQcmTriggers,
                 gQcmTriggersIndex[QCM_TRIGGERS_USERID_TRIGGERNAME_INDEX],
                 smiGetRowSCN(gQcmTriggers),
                 NULL,
                 &sRange,
                 smiGetDefaultKeyRange(),
                 smiGetDefaultFilter(),
                 QCM_META_CURSOR_FLAG,
                 SMI_SELECT_CURSOR,
                 & sCursorProperty) != IDE_SUCCESS);

    sStage = 1;
    IDE_TEST(sCursor.beforeFirst() != IDE_SUCCESS);

    IDE_TEST(sCursor.readRow(&sRow, &sRid, SMI_FIND_NEXT) != IDE_SUCCESS);

    while (sRow != NULL)
    {
        sTableID = *(UInt *) ((UChar*)sRow +
                              sTableIDCol->column.offset);

        IDE_TEST(qcm::getTableInfoByID(aStatement,
                                       sTableID,
                                       &sTableInfo,
                                       &sSCN,
                                       &sTableHandle) != IDE_SUCCESS);

        IDE_TEST(qcm::validateAndLockTable(aStatement,
                                           sTableHandle,
                                           sSCN,
                                           SMI_TABLE_LOCK_X)
                 != IDE_SUCCESS);

        if (sTableInfo->tableOwnerID != aUserID)
        {
            // BUG-16565
            IDE_TEST_RAISE(sTableInfo->replicationCount > 0,
                           ERR_DDL_WITH_REPLICATED_TABLE);

            //proj-1608:replicationCount�� 0�� �� recovery count�� �׻� 0�̾�� ��
            IDE_DASSERT(sTableInfo->replicationRecoveryCount == 0);

            IDU_LIMITPOINT("qcmUser::findTriggerListByUserID::malloc");
            IDE_TEST(aStatement->qmxMem->alloc(ID_SIZEOF(qcmTriggerInfoList),
                                               (void**)&(sTriggerInfoList))
                     != IDE_SUCCESS);

            sTriggerInfoList->triggerOID = (smOID) * (ULong*)
                ( (UChar*)sRow + sTriggerOIDCol->column.offset );

            sTriggerInfoList->tableInfo = sTableInfo;

            sTriggerInfoList->next = sFirstTriggerInfoList;
            sFirstTriggerInfoList  = sTriggerInfoList;
        }

        IDE_TEST(sCursor.readRow(&sRow, &sRid, SMI_FIND_NEXT) != IDE_SUCCESS);
    }

    sStage = 0;

    IDE_TEST(sCursor.close() != IDE_SUCCESS);

    *aTriggerInfoList = sFirstTriggerInfoList;

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_DDL_WITH_REPLICATED_TABLE)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDD_DDL_WITH_REPLICATED_TBL));
    }
    IDE_EXCEPTION_END;

    switch(sStage)
    {
        case 1:
            sCursor.close();
            break;
    }

    return IDE_FAILURE;
}

IDE_RC qcmUser::getMetaUserName( smiStatement * aSmiStmt,
                                 UInt           aUserID,
                                 SChar        * aUserName )
{
/***********************************************************************
 *
 * Description : BUG-24957
 *    meta table���� user name�� �����´�.
 *
 * Implementation :
 *
 ***********************************************************************/

    smiTableCursor          sCursor;
    mtcColumn             * sQcmUsersIndexColumn;
    smiRange                sRange;
    qtcMetaRangeColumn      sRangeColumn;
    mtdCharType           * sUserNameStr;
    SInt                    sStage = 0;
    const void            * sRow;
    scGRID                  sRid; // Disk Table�� ���� Record IDentifier
    smiCursorProperties     sCursorProperty;
    mtcColumn             * sFstMtcColumn;

    sCursor.initialize();

    IDE_TEST( smiGetTableColumns( gQcmUsers,
                                  QCM_USERS_USER_ID_COL_ORDER,
                                  (const smiColumn**)&sQcmUsersIndexColumn )
              != IDE_SUCCESS );
    
    qcm::makeMetaRangeSingleColumn(
        &sRangeColumn,
        (const mtcColumn*) sQcmUsersIndexColumn,
        (const void *) & aUserID,
        &sRange);

    SMI_CURSOR_PROP_INIT_FOR_META_INDEX_SCAN(&sCursorProperty, NULL);

    IDE_TEST(sCursor.open(
                 aSmiStmt,
                 gQcmUsers,
                 gQcmUsersIndex[QCM_USER_USERID_IDX_ORDER],
                 smiGetRowSCN(gQcmUsers),
                 NULL,
                 &sRange,
                 smiGetDefaultKeyRange(),
                 smiGetDefaultFilter(),
                 QCM_META_CURSOR_FLAG,
                 SMI_SELECT_CURSOR,
                 &sCursorProperty) != IDE_SUCCESS);
    sStage = 1;

    IDE_TEST(sCursor.beforeFirst() != IDE_SUCCESS);

    IDE_TEST(sCursor.readRow(&sRow, &sRid, SMI_FIND_NEXT) != IDE_SUCCESS);

    IDE_TEST( smiGetTableColumns( gQcmUsers,
                                  QCM_USERS_USER_NAME_COL_ORDER,
                                  (const smiColumn**)&sFstMtcColumn )
              != IDE_SUCCESS );

    if ( sRow != NULL )
    {
        sUserNameStr = (mtdCharType*)
            ( (SChar*)sRow + sFstMtcColumn->column.offset );

        idlOS::strncpy(aUserName,
                       (SChar*) &(sUserNameStr->value),
                       sUserNameStr->length);
        aUserName[sUserNameStr->length] = '\0';
    }
    else
    {
        aUserName[0] = '\0';
    }

    sStage = 0;
    IDE_TEST(sCursor.close() != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sStage == 1 )
    {
        (void)sCursor.close();
    }

    return IDE_FAILURE;
}

// PROJ-1073 Package
IDE_RC qcmUser::findPkgListByUserID( qcStatement     * aStatement,
                                     UInt              aUserID,
                                     qcmPkgInfoList  **aPkgInfoList)
{
/***********************************************************************
 *
 * Description :
 *    executeDropUserCascade �κ��� ȣ��,
 *    drop ������ package�� ����Ʈ�� �����ش�.
 *
 * Implementation :
 *
 *
 ***********************************************************************/
   
    qcmPkgInfoList  * sFirstPkgInfoList = NULL;
    qcmPkgInfoList  * sPkgInfoList      = NULL;
    qcmPkgs         * sQcmPackages      = NULL;
    vSLong            sRowCount         = 0;
    vSLong            sRealRowCount     = 0;
    SInt              i;

    IDE_TEST( qcmPkg::pkgSelectCountWithUserId (
                  aStatement,
                  aUserID,
                  & sRowCount ) != IDE_SUCCESS );

    if ( sRowCount > 0 )
    {
        IDE_TEST(QC_QMX_MEM( aStatement )->
                 alloc( idlOS::align8((UInt)ID_SIZEOF(qcmPkgs)) * sRowCount,
                        (void**)&sQcmPackages)
                 != IDE_SUCCESS);

        IDE_TEST( qcmPkg::pkgSelectAllWithUserId (
                      aStatement,
                      aUserID,
                      sRowCount,
                      & sRealRowCount,
                      sQcmPackages ) != IDE_SUCCESS );

        IDE_TEST_RAISE( sRowCount != sRealRowCount,
                        err_rowcount_mismatch );

        for (i = 0; i < sRealRowCount; i++)
        {
            IDE_TEST( QC_QMX_MEM( aStatement )->
                      alloc(ID_SIZEOF(qcmPkgInfoList),
                            (void**)&(sPkgInfoList))
                     != IDE_SUCCESS);

            sPkgInfoList->pkgName = sQcmPackages[i].pkgName;
            sPkgInfoList->pkgOID  = sQcmPackages[i].pkgOID;
            sPkgInfoList->pkgType = sQcmPackages[i].pkgType;   /* BUG-39340 */

            sPkgInfoList->next = sFirstPkgInfoList;
            sFirstPkgInfoList  = sPkgInfoList;
        }
    }

    *aPkgInfoList = sFirstPkgInfoList;

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_rowcount_mismatch);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QSX_INTERNAL_SERVER_ERROR_ARG,
                                "sRowCount != sRealRowCount"));
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description : PROJ-1812 ROLE
 *    ������� ��ȣ, default tablespace �� ���Ѵ�.
 *
 * Implementation :
 *    1. ��Ÿ ĳ���κ��� USER_ID �� �ش��ϴ� �÷��� ã�Ƽ�
 *    2. ����� ����ID �� keyRange �� �����.
 *    3. Ŀ�� open ,beforeFirst, readRow �� ���ؼ� password �� ���Ѵ�.
 *    4. readRow �� ���� ������ ������ ��ȯ�Ѵ�.
 *
 ***********************************************************************/
IDE_RC qcmUser::getUserPasswordAndDefaultTBS( qcStatement     * aStatement,
                                              UInt              aUserID,
                                              SChar           * aPassword,
                                              scSpaceID       * aTBSID,
                                              scSpaceID       * aTempTBSID,
                                              qciDisableTCP   * aDisableTCP )
{
    smiTableCursor          sCursor;
    qcmColumn             * sColumn = NULL;
    smiRange                sRange;
    qtcMetaRangeColumn      sRangeColumn;
    mtcColumn             * sMtcColumn;
    mtdCharType           * sPasswdStr;
    mtcColumn             * sDefaultTBSIDColumn;
    mtcColumn             * sTempTBSIDColumn;        
    SInt                    sStage = 0;
    const void            * sRow;
    smiCursorProperties     sCursorProperty;
    scGRID                  sRid; // Disk Table�� ���� Record IDentifier
    mtcColumn             * sUserTypeColumn;
    mtdCharType           * sUserTypeStr;
    UChar                 * sUserType;
    mtcColumn             * sDisableTCPColumn = NULL;
    mtdCharType           * sDisableTCPStr    = NULL;

    sCursor.initialize();

    IDE_TEST( qcmCache::getColumnByName( gQcmUsersTempInfo,
                                         (SChar*) "USER_ID",
                                         idlOS::strlen((SChar*) "USER_ID"),
                                         &sColumn ) != IDE_SUCCESS );

    IDE_ASSERT( sColumn != NULL );
    
    // mtdModule ����
    IDE_TEST( mtd::moduleById( &( sColumn->basicInfo->module ),
                               sColumn->basicInfo->type.dataTypeId )
              != IDE_SUCCESS );

    // mtlModule ����
    IDE_TEST( mtl::moduleById( &(sColumn->basicInfo->language),
                               sColumn->basicInfo->type.languageId)
              != IDE_SUCCESS );

    qcm::makeMetaRangeSingleColumn(
        &sRangeColumn,
        (const mtcColumn*)sColumn->basicInfo,
        (const void *) & aUserID,
        &sRange );

    SMI_CURSOR_PROP_INIT_FOR_META_INDEX_SCAN( &sCursorProperty, aStatement->mStatistics );

    IDE_TEST(sCursor.open(
                 QC_SMI_STMT( aStatement ),
                 gQcmUsersTempInfo->tableHandle,
                 gQcmUsersIndex[QCM_USER_USERID_IDX_ORDER],
                 smiGetRowSCN(gQcmUsersTempInfo->tableHandle),
                 NULL,
                 &sRange,
                 smiGetDefaultKeyRange(),
                 smiGetDefaultFilter(),
                 QCM_META_CURSOR_FLAG,
                 SMI_SELECT_CURSOR,
                 & sCursorProperty )
             != IDE_SUCCESS );

    sStage = 1;

    IDE_TEST( sCursor.beforeFirst() != IDE_SUCCESS );

    IDE_TEST( sCursor.readRow( &sRow, &sRid, SMI_FIND_NEXT ) != IDE_SUCCESS );

    IDE_TEST_RAISE( sRow == NULL, ERR_NOT_EXIST_USER );

    IDE_TEST( smiGetTableColumns( gQcmUsersTempInfo->tableHandle,
                                  QCM_USERS_USER_PASSWD_COL_ORDER,
                                  (const smiColumn**)&sMtcColumn )
              != IDE_SUCCESS );
    sPasswdStr  = (mtdCharType*)( (SChar*)sRow + sMtcColumn->column.offset );

    IDE_TEST( smiGetTableColumns( gQcmUsersTempInfo->tableHandle,
                                  QCM_USERS_USER_TYPE_COL_ORDER,
                                  (const smiColumn**)&sUserTypeColumn )
              != IDE_SUCCESS );
    sUserTypeStr  = (mtdCharType*)( (SChar*)sRow + sUserTypeColumn->column.offset );

    sUserType = sUserTypeStr->value;
    
    /* PROJ-1812 ROLE */
    IDE_TEST_RAISE( sUserType[0] != 'U', ERR_NOT_EXIST_USER );
    
    idlOS::strncpy(aPassword, (SChar*)&(sPasswdStr->value),
                   sPasswdStr->length);
    aPassword[ sPasswdStr->length ] = '\0';

    /* PROJ-1812 ROLE */
    if ( aUserID == QC_SYSTEM_USER_ID )
    {// on createdb
        *aTBSID = SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC;
        
        if ( aTempTBSID != NULL )
        {
            *aTempTBSID = SMI_ID_TABLESPACE_SYSTEM_DISK_TEMP;
        }
        else
        {
            /* Nothing To Do */
        }

        /* PROJ-2474 SSL/TLS Support */
        *aDisableTCP = QCI_DISABLE_TCP_FALSE;
    }
    else
    {
        IDE_TEST( smiGetTableColumns( gQcmUsersTempInfo->tableHandle,
                                      QCM_USERS_DEFAULT_TBS_ID_COL_ORDER,
                                      (const smiColumn**)&sDefaultTBSIDColumn )
                  != IDE_SUCCESS );

        *aTBSID = qcm::getSpaceID( sRow,
                                   sDefaultTBSIDColumn->column.offset );

        if ( aTempTBSID != NULL )
        {
            IDE_TEST( smiGetTableColumns( gQcmUsersTempInfo->tableHandle,
                                          QCM_USERS_TEMP_TBS_ID_COL_ORDER,
                                          (const smiColumn**)&sTempTBSIDColumn )
                      != IDE_SUCCESS );

            *aTempTBSID= qcm::getSpaceID( sRow, sTempTBSIDColumn->column.offset );
        }
        else
        {
            /* Nothing To Do */
        }

        /* PROJ-2474 SSL/TLS Support */
        IDE_TEST( smiGetTableColumns( gQcmUsersTempInfo->tableHandle,
                                      QCM_USERS_DISABLE_TCP_COL_ORDER,
                                      (const smiColumn**)&sDisableTCPColumn )
                      != IDE_SUCCESS );
        sDisableTCPStr = (mtdCharType *)( (SChar *)sRow + sDisableTCPColumn->column.offset );

        /* PROJ-2474 TLS/SSL Support */
        if ( sDisableTCPStr->length == 0 )
        {
            *aDisableTCP = QCI_DISABLE_TCP_NULL;
        }
        else
        {
            if ( sDisableTCPStr->value[0] == 'T' )
            {
                *aDisableTCP = QCI_DISABLE_TCP_TRUE;
            }
            else if ( sDisableTCPStr->value[0] == 'F' )
            {
                *aDisableTCP = QCI_DISABLE_TCP_FALSE;
            }
            else
            {
                IDE_RAISE( ERR_META_CRASH );
            }
        }
    }
        
    sStage = 0;
    IDE_TEST(sCursor.close() != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NOT_EXIST_USER );
    {
        IDE_SET(ideSetErrorCode( qpERR_ABORT_QCM_NOT_EXIST_USER ) );
    }
    IDE_EXCEPTION( ERR_META_CRASH );
    {
        IDE_SET(ideSetErrorCode( qpERR_ABORT_QCM_META_CRASH ) );
    }
    IDE_EXCEPTION_END;

    if ( sStage == 1 )
    {
        (void)sCursor.close();
    }
    else
    {
        /* Nothing To Do */
    }

    return IDE_FAILURE;
}

