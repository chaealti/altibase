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
 * $Id: qdr.cpp 82075 2018-01-17 06:39:52Z jina.kim $
 **********************************************************************/

#include <idl.h>
#include <ide.h>
#include <qdr.h>
#include <qcmUser.h>
#include <qcmTableSpace.h>
#include <idsPassword.h>
#include <qcuSqlSourceInfo.h>
#include <qcg.h>
#include <qdParseTree.h>
#include <qdpGrant.h>
#include <qdpPrivilege.h>
#include <smiTableSpace.h>
#include <qcmPassword.h>
#include <qc.h>
#include <qdpRole.h>

/***********************************************************************
 * VALIDATE
 **********************************************************************/

// CREATE USER
IDE_RC qdr::validateCreate(qcStatement * aStatement)
{
/***********************************************************************
 *
 * Description :
 *    CREATE USER ... �� validation  ����
 *
 * Implementation :
 *    1. CreateUse ������ �ִ��� üũ
 *    2. ���� �̸��� ����ڰ� �̹� �ִ��� üũ
 *    3. validation of DEFAULT TABLESPACE
 *    if ( DEFAULT TABLESPACE name ����� ��� )
 *    {
 *      3.1.1 TABLESPACE name ���� ���� �˻�
 *      3.1.2 DATA TABLESPACE �� �ƴ� ��� ���� �߻�
 *    }
 *    else
 *    {
 *      3.2.1 SYS_TBS_MEMORY �� DEFAULT DATA TABLESPACE �� ����
 *    }
 *    4. validation of TEMPORARY TABLESPACE
 *    if ( DEFAULT TABLESPACE name ����� ��� )
 *    {
 *      4.1.1 TABLESPACE name ���� ���� �˻�
 *      4.1.2 TEMPORARY TABLESPACE �� �ƴ� ��� ���� �߻�
 *    }
 *    else
 *    {
 *      4.2.1 SYSTEM TEMPORARY TABLESPACE(SYS_TBS_TEMP) �� TEMPORARY
 *            TABLESPACE�� ����
 *    }
 *    5. validation of ACCESS
 *    for ( each ACCESS )
 *    {
 *      5.1 TABLESPACE name ���� ���� �˻�(data tablespace, temporary
 *          tablespace ��� ��� ����)
 *    }
 *
 ***********************************************************************/

#define IDE_FN "qdr::validateCreate"
    qdUserParseTree     * sParseTree;
    smiTableSpaceAttr     sTBSAttr;
    qdUserTBSAccess     * sAccess;
    UInt                  sCurrentUserID;
    qdUserType            sUserType;
    
    sParseTree = (qdUserParseTree *)aStatement->myPlan->parseTree;

    /* PROJ-2474 SSL/TLS Support */
    if ( sParseTree->disableTCP != QD_DISABLE_TCP_NONE )
    {
        sCurrentUserID = QCG_GET_SESSION_USER_ID(aStatement);
        /* sys user�� ���� */
        IDE_TEST_RAISE ( sCurrentUserID != QC_SYS_USER_ID,
                         ERR_NOT_SYS_USER );
    }
    else
    {
        /* Nothing to do */
    }

    // check grant
    IDE_TEST( qdpRole::checkDDLCreateUserPriv( aStatement )
              != IDE_SUCCESS );

    // if same user exists, then ERR_DUPLCATED_USER
    IDE_TEST_RAISE( qcmUser::getUserIDAndType( aStatement,
                                               sParseTree->userName,
                                               &(sParseTree->userID),
                                               &sUserType ) == IDE_SUCCESS,
                    ERR_DUPLICATED_USER );

    // validation of DEFAULT TABLESPACE
    if ( QC_IS_NULL_NAME(sParseTree->dataTBSName) != ID_TRUE )
    {
        IDE_TEST( qcmTablespace::getTBSAttrByName(
                      aStatement,
                      sParseTree->dataTBSName.stmtText + sParseTree->dataTBSName.offset,
                      sParseTree->dataTBSName.size,
                      &sTBSAttr) != IDE_SUCCESS );
        sParseTree->dataTBSID = sTBSAttr.mID;

        IDE_TEST_RAISE( smiTableSpace::isDataTableSpaceType( sTBSAttr.mType ) == ID_FALSE,
                        ERR_INVALID_DATA_TBS );

        /* PROJ-2639 Altibase Disk Edition */
#ifdef ALTI_CFG_EDITION_DISK
        if ( idlOS::strMatch( sParseTree->userName.stmtText + sParseTree->userName.offset,
                              sParseTree->userName.size,
                              QC_SYSTEM_USER_NAME,
                              idlOS::strlen( QC_SYSTEM_USER_NAME ) )
             == 0 )
        {
            /* Nothing to do */
        }
        else
        {
            IDE_TEST_RAISE( smiTableSpace::isDiskTableSpaceType( sTBSAttr.mType ) == ID_FALSE,
                            ERR_NOT_SUPPORT_MEMORY_VOLATILE_TABLESPACE_IN_DISK_EDITION );
        }
#endif
    }
    else
    {
#if defined(SMALL_FOOTPRINT) && !defined(WRS_VXWORKS)
        sParseTree->dataTBSID = SMI_ID_TABLESPACE_SYSTEM_DISK_DATA;
#else
        // BUG-43737
        if ( QCU_FORCE_TABLESPACE_DEFAULT == 2 )
        {
            sParseTree->dataTBSID = SMI_ID_TABLESPACE_SYSTEM_DISK_DATA;
        }
        else
        {
            sParseTree->dataTBSID = SMI_ID_TABLESPACE_SYSTEM_MEMORY_DATA;
        }
#endif

        /* PROJ-2639 Altibase Disk Edition */
#ifdef ALTI_CFG_EDITION_DISK
        if ( idlOS::strMatch( sParseTree->userName.stmtText + sParseTree->userName.offset,
                              sParseTree->userName.size,
                              QC_SYSTEM_USER_NAME,
                              idlOS::strlen( QC_SYSTEM_USER_NAME ) )
             == 0 )
        {
            /* Nothing to do */
        }
        else
        {
            sParseTree->dataTBSID = SMI_ID_TABLESPACE_SYSTEM_DISK_DATA;
        }
#endif
    }
    // validation of TEMPORARY TABLESPACE
    if ( QC_IS_NULL_NAME(sParseTree->tempTBSName) != ID_TRUE )
    {
        IDE_TEST( qcmTablespace::getTBSAttrByName(
                      aStatement,
                      sParseTree->tempTBSName.stmtText + sParseTree->tempTBSName.offset,
                      sParseTree->tempTBSName.size,
                      &sTBSAttr) != IDE_SUCCESS );
        sParseTree->tempTBSID = sTBSAttr.mID;

        IDE_TEST_RAISE( ( sTBSAttr.mType != SMI_DISK_SYSTEM_TEMP ) &&
                        ( sTBSAttr.mType != SMI_DISK_USER_TEMP ),
                        ERR_INVALID_TEMP_TBS );
    }
    else
    {
        sParseTree->tempTBSID = SMI_ID_TABLESPACE_SYSTEM_DISK_TEMP;
    }
    // validation of ACCESS
    for ( sAccess = sParseTree->access;
          sAccess != NULL;
          sAccess=sAccess->next )
    {
        IDE_TEST( qcmTablespace::getTBSAttrByName(
                      aStatement,
                      sAccess->TBSName.stmtText + sAccess->TBSName.offset,
                      sAccess->TBSName.size,
                      &sTBSAttr) != IDE_SUCCESS );
        sAccess->dataTBSID = sTBSAttr.mID;
    }

    /* PROJ-2207 Password policy support */
    IDE_TEST( validatePasswOpts( aStatement ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_DUPLICATED_USER)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDR_DUPLICATE_USER));
    }
    IDE_EXCEPTION(ERR_INVALID_DATA_TBS)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDT_ERR_INVALID_DATA_TBS));
    }
#ifdef ALTI_CFG_EDITION_DISK
    IDE_EXCEPTION( ERR_NOT_SUPPORT_MEMORY_VOLATILE_TABLESPACE_IN_DISK_EDITION );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDB_SET_ONLY_DISK_TO_DEFAULT_TABLESPACE_IN_DISK_EDITION ) );
    }
#endif
    IDE_EXCEPTION(ERR_INVALID_TEMP_TBS)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDT_ERR_INVALID_TEMP_TBS));
    }
    IDE_EXCEPTION( ERR_NOT_SYS_USER )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDR_NOT_SYS_USER_DISABLE_TCP ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

// ALTER USER
IDE_RC qdr::validateAlter(qcStatement * aStatement)
{
/***********************************************************************
 *
 * Description :
 *    ALTER USER ... �� validation  ����
 *
 * Implementation :
 *    1. ALTER �ϰ��� �ϴ� ����ڰ� ������ ����
 *    2. AlterUser ������ �ִ��� üũ
 *    3. validation of DEFAULT TABLESPACE
 *    if ( DEFAULT TABLESPACE name ����� ��� )
 *    {
 *      3.1.1 TABLESPACE name ���� ���� �˻�
 *      3.1.2 DATA TABLESPACE �� �ƴ� ��� ���� �߻�
 *    }
 *    4. validation of TEMPORARY TABLESPACE
 *    if ( DEFAULT TABLESPACE name ����� ��� )
 *    {
 *      4.1.1 TABLESPACE name ���� ���� �˻�
 *      4.1.2 TEMPORARY TABLESPACE �� �ƴ� ��� ���� �߻�
 *    }
 *    5. validation of ACCESS
 *    for ( each ACCESS )
 *    {
 *      5.1 TABLESPACE name ���� ���� �˻�(data tablespace, temporary
 *          tablespace ��� ��� ����)
 *    }
 *
 ***********************************************************************/

#define IDE_FN "qdr::validateAlter"
    qdUserParseTree     * sParseTree;
    qcuSqlSourceInfo      sqlInfo;
    smiTableSpaceAttr     sTBSAttr;
    qdUserTBSAccess     * sAccess;
    UInt                  sCurrentUserID;

    sParseTree = (qdUserParseTree *)aStatement->myPlan->parseTree;

    // if specified user doesn't exist, then error
    if (qcmUser::getUserID(
            aStatement, sParseTree->userName, &(sParseTree->userID))
        != IDE_SUCCESS)
    {
        IDE_CLEAR();

        sqlInfo.setSourceInfo( aStatement,
                               & sParseTree->userName );
        IDE_RAISE(ERR_NOT_EXIST_USER);
    }
    else
    {
        /* Nothing to do */
    }    

    // check grant
    IDE_TEST( qdpRole::checkDDLAlterUserPriv( aStatement,
                                              sParseTree->userID )
              != IDE_SUCCESS );
    
    // check Grant : The Only SYS user can alter other user password.
    //             : A user can alter own password.
    //IDE_TEST_RAISE(QC_GET_SESSION_USER_ID(aStatement) != QC_SYS_USER_ID &&
    //               QC_GET_SESSION_USER_ID(aStatement) != sParseTree->userID,
    //    ERR_NO_GRANT);

    // validation of DEFAULT TABLESPACE
    if ( QC_IS_NULL_NAME(sParseTree->dataTBSName) != ID_TRUE )
    {
        IDE_TEST( qcmTablespace::getTBSAttrByName(
                      aStatement,
                      sParseTree->dataTBSName.stmtText + sParseTree->dataTBSName.offset,
                      sParseTree->dataTBSName.size,
                      &sTBSAttr )
                  != IDE_SUCCESS );
        sParseTree->dataTBSID = sTBSAttr.mID;

        IDE_TEST_RAISE( smiTableSpace::isDataTableSpaceType( sTBSAttr.mType ) == ID_FALSE,
                        ERR_INVALID_DATA_TBS );

        /* PROJ-2639 Altibase Disk Edition */
#ifdef ALTI_CFG_EDITION_DISK
        if ( sParseTree->userID == QC_SYSTEM_USER_ID )
        {
            /* Nothing to do */
        }
        else
        {
            IDE_TEST_RAISE( smiTableSpace::isDiskTableSpaceType( sTBSAttr.mType ) == ID_FALSE,
                            ERR_NOT_SUPPORT_MEMORY_VOLATILE_TABLESPACE_IN_DISK_EDITION );
        }
#endif

        // To Fix PR-10589
        // TABLESPACE�� ���� ���� ������ �˻��Ͽ��� ��.
        IDE_TEST( qdpRole::checkAccessTBS( aStatement,
                                           QCG_GET_SESSION_USER_ID(aStatement),
                                           sParseTree->dataTBSID )
            != IDE_SUCCESS );

    }
    // validation of TEMPORARY TABLESPACE
    if ( QC_IS_NULL_NAME(sParseTree->tempTBSName) != ID_TRUE )
    {
        IDE_TEST( qcmTablespace::getTBSAttrByName(
                      aStatement,
                      sParseTree->tempTBSName.stmtText + sParseTree->tempTBSName.offset,
                      sParseTree->tempTBSName.size,
                      &sTBSAttr) != IDE_SUCCESS );
        sParseTree->tempTBSID = sTBSAttr.mID;

        IDE_TEST_RAISE( ( sTBSAttr.mType != SMI_DISK_SYSTEM_TEMP ) &&
                        ( sTBSAttr.mType != SMI_DISK_USER_TEMP ),
                        ERR_INVALID_TEMP_TBS );

        // To Fix PR-10589
        // TABLESPACE�� ���� ���� ������ �˻��Ͽ��� ��.
        IDE_TEST( qdpRole::checkAccessTBS( aStatement,
                                           QCG_GET_SESSION_USER_ID(aStatement),
                                           sParseTree->tempTBSID )
            != IDE_SUCCESS );

    }
    // validation of ACCESS
    for ( sAccess = sParseTree->access;
          sAccess != NULL;
          sAccess=sAccess->next )
    {
        IDE_TEST( qcmTablespace::getTBSAttrByName(
                      aStatement,
                      sAccess->TBSName.stmtText + sAccess->TBSName.offset,
                      sAccess->TBSName.size,
                      &sTBSAttr) != IDE_SUCCESS );
        sAccess->dataTBSID = sTBSAttr.mID;

        // To Fix PR-10589
        // TABLESPACE�� ���� ���� ������ �˻��Ͽ��� ��.
        IDE_TEST( qdpRole::checkAccessTBS( aStatement,
                                           QCG_GET_SESSION_USER_ID(aStatement),
                                           sAccess->dataTBSID )
            != IDE_SUCCESS );
    }

    IDE_TEST( validatePasswOpts( aStatement ) != IDE_SUCCESS );

    /* PROJ-2474 SSL/TLS Support */
    if ( sParseTree->disableTCP != QD_DISABLE_TCP_NONE )
    {
        sCurrentUserID = QCG_GET_SESSION_USER_ID(aStatement);

        /* sys user�� ���� */
        IDE_TEST_RAISE ( sCurrentUserID != QC_SYS_USER_ID,
                         ERR_NOT_SYS_USER );
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_NOT_EXIST_USER)
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QDR_NOT_EXISTS_USER,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION(ERR_INVALID_DATA_TBS)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDT_ERR_INVALID_DATA_TBS));
    }
#ifdef ALTI_CFG_EDITION_DISK
    IDE_EXCEPTION( ERR_NOT_SUPPORT_MEMORY_VOLATILE_TABLESPACE_IN_DISK_EDITION );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDB_SET_ONLY_DISK_TO_DEFAULT_TABLESPACE_IN_DISK_EDITION ) );
    }
#endif
    IDE_EXCEPTION(ERR_INVALID_TEMP_TBS)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDT_ERR_INVALID_TEMP_TBS));
    }
    IDE_EXCEPTION( ERR_NOT_SYS_USER )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDR_NOT_SYS_USER_DISABLE_TCP ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

/***********************************************************************
 * EXECUTE
 **********************************************************************/

IDE_RC qdr::executeCreate(qcStatement * aStatement)
{
/***********************************************************************
 *
 * Description :
 *      CREATE USER ���� execution ���� �Լ�
 *
 * Implementation :
 *      1. ���ο� ����ڸ� ���� user ID�� �ο�
 *      2. DBA_USERS_ ��Ÿ ���̺� ����� ���� �Է�
 *      3. SYS_TBS_USERS_ ��Ÿ ���̺� �Է�
 *      4. default privilege �ο�
 *
 ***********************************************************************/

#define IDE_FN "qdr::executeCreate"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qdr::executeCreate"));

    qdUserParseTree     * sParseTree;
    UInt                  sUserID;
    UChar                 sUserName[ QC_MAX_OBJECT_NAME_LEN + 1 ];

    SChar                 sUserPasswd[ IDS_MAX_PASSWORD_LEN + 1 ];
    SChar                 sCryptStr[IDS_MAX_PASSWORD_BUFFER_LEN + 1];
    SChar               * sSqlStr;
    vSLong                sRowCnt;
    qdUserTBSAccess     * sAccess;

    SChar                 sPasswVerifyFunc[ QC_PASSWORD_OPT_LEN + 1 ] = {0,};
    SChar                 sPasswPolicy[2] = {0,};
    SChar                 sAccountLock[2] = {0,};
    SChar                 sDisableTCP[2]  = {0,};

    sParseTree = (qdUserParseTree *)aStatement->myPlan->parseTree;

    QC_STR_COPY( sUserName, sParseTree->userName );

    QC_STR_COPY( sUserPasswd, sParseTree->password );

    if ( sParseTree->passwLimitFlag == QD_PASSWORD_POLICY_DISABLE )
    {
        sPasswPolicy[0] = 'F';
        sPasswPolicy[1] = 0;
    }
    else
    {
        sPasswPolicy[0] = 'T';
        sPasswPolicy[1] = 0;
    }

    if ( sParseTree->accountLock == QD_ACCOUNT_UNLOCK )
    {
        sAccountLock[0] = 'N';
        sAccountLock[1] = 0;
    }
    else
    {
        sAccountLock[0] = 'L';
        sAccountLock[1] = 0;
    }

    /* PROJ-2474 SSL/TLS Support */
    if ( sParseTree->disableTCP == QD_DISABLE_TCP_TRUE )
    {
        sDisableTCP[0] = 'T';
        sDisableTCP[1] = 0;
    }
    else
    {
        sDisableTCP[0] = 'F';
        sDisableTCP[1] = 0;
    }

    // BUG-38565 password ��ȣȭ �˰��� ����
    idsPassword::crypt( sCryptStr, sUserPasswd, idlOS::strlen( sUserPasswd ), NULL );

    IDE_TEST(qcmUser::getNextUserID(aStatement, &sUserID) != IDE_SUCCESS);

    /* set password policy default options */
    if (( sUserID != QC_SYS_USER_ID) && ( sUserID !=  QC_SYSTEM_USER_ID ))
    {
        passwPolicyDefaultOpts(aStatement);
    }
    else
    {
        // Nothing To Do
    }

    if ( QC_IS_NULL_NAME(sParseTree->passwVerifyFunc) != ID_TRUE )
    {
        QC_STR_COPY( sPasswVerifyFunc, sParseTree->passwVerifyFunc );

        IDE_TEST( qcmReuseVerify::actionVerifyFunction( aStatement,
                                                        (SChar*)sUserName,
                                                        sUserPasswd,
                                                        sPasswVerifyFunc )
                  != IDE_SUCCESS );
    }
    else
    {
        /* set password policy default PASSWORD_VERIFY_FUNCTION options */
        if (( sUserID != QC_SYS_USER_ID) && ( sUserID !=  QC_SYSTEM_USER_ID ))
        {
            if ( QCU_PASSWORD_VERIFY_FUNCTION[0] != '\0' )
            {
                idlOS::snprintf( sPasswVerifyFunc,
                                 QC_PASSWORD_OPT_LEN + 1,
                                 "%s",
                                 QCU_PASSWORD_VERIFY_FUNCTION );

                IDE_TEST( qcmReuseVerify::actionVerifyFunction( aStatement,
                                                                (SChar*)sUserName,
                                                                sUserPasswd,
                                                                sPasswVerifyFunc )
                          != IDE_SUCCESS );
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
    }

    IDU_FIT_POINT( "qdr::executeCreate::alloc::sSqlStr",
                    idERR_ABORT_InsufficientMemory );

    IDE_TEST(STRUCT_ALLOC_WITH_SIZE(aStatement->qmxMem,
                                    SChar,
                                    QD_MAX_SQL_LENGTH,
                                    &sSqlStr)
             != IDE_SUCCESS);

    if ( sParseTree->passwLifeTime != 0 )
    {
        // BUG-41230 SYS_USERS_ => DBA_USERS_
        idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                         "INSERT INTO DBA_USERS_ VALUES( "
                         "INTEGER'%"ID_INT32_FMT"', "
                         "VARCHAR'%s', "
                         "VARCHAR'%s', "
                         "INTEGER'%"ID_INT32_FMT"', "
                         "INTEGER'%"ID_INT32_FMT"', "
                         "VARCHAR'%s', "                      // ACCOUNT_LOCK
                         "NULL, "                             // ACCOUNT_LOCK_DATE
                         "VARCHAR'%s', "                      // PASSWORD_LIMIT_FLAG
                         "INTEGER'%"ID_INT32_FMT"', "         // FAILED_LOGIN_ATTEMPTS
                         "INTEGER'%"ID_INT32_FMT"', "         // FAILED_LOGIN_COUNT
                         "INTEGER'%"ID_INT32_FMT"', "         // PASSWORD_LOCK_TIME
                         "SYSDATE+INTEGER'%"ID_INT32_FMT"', " // PASSWORD_EXPIRY_DATE
                         "INTEGER'%"ID_INT32_FMT"', "         // PASSWORD_LIFE_TIME
                         "INTEGER'%"ID_INT32_FMT"', "         // PASSWORD_GRACE_TIME
                         "NULL, "                             // PASSWORD_REUSE_DATE
                         "INTEGER'%"ID_INT32_FMT"', "         // PASSWORD_REUSE_TIME
                         "INTEGER'%"ID_INT32_FMT"', "         // PASSWORD_REUSE_MAX
                         "INTEGER'%"ID_INT32_FMT"', "         // PASSWORD_REUSE_COUNT
                         "VARCHAR'%s', "                      // PASSWORD_VERIFY_FUNCTION
                         "CHAR'U', "                          // USER_TYPE
                         "CHAR'%s', "                         // DISABLE_TCP
                         "SYSDATE, SYSDATE )",
                         sUserID,
                         sUserName,
                         sCryptStr,
                         (mtdIntegerType) sParseTree->dataTBSID,
                         (mtdIntegerType) sParseTree->tempTBSID,
                         sAccountLock,
                         sPasswPolicy,
                         sParseTree->failLoginAttempts,
                         sParseTree->failedCount,
                         sParseTree->passwLockTime,
                         sParseTree->passwLifeTime,
                         sParseTree->passwLifeTime,
                         sParseTree->passwGraceTime,
                         sParseTree->passwReuseTime,
                         sParseTree->passwReuseMax,
                         sParseTree->reuseCount,
                         sPasswVerifyFunc,
                         sDisableTCP );
    }
    else
    {
        // BUG-41230 SYS_USERS_ => DBA_USERS_
        idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                         "INSERT INTO DBA_USERS_ VALUES( "
                         "INTEGER'%"ID_INT32_FMT"', "
                         "VARCHAR'%s', "
                         "VARCHAR'%s', "
                         "INTEGER'%"ID_INT32_FMT"', "
                         "INTEGER'%"ID_INT32_FMT"', "
                         "CHAR'%s', "                         // ACCOUNT_LOCK
                         "NULL, "                             // ACCOUNT_LOCK_DATE
                         "CHAR'%s', "                         // PASSWORD_LIMIT_FLAG
                         "INTEGER'%"ID_INT32_FMT"', "         // FAILED_LOGIN_ATTEMPTS
                         "INTEGER'%"ID_INT32_FMT"', "         // FAILED_LOGIN_COUNT
                         "INTEGER'%"ID_INT32_FMT"', "         // PASSWORD_LOCK_TIME
                         "NULL, "                             // PASSWORD_EXPIRY_DATE
                         "INTEGER'%"ID_INT32_FMT"', "         // PASSWORD_LIFE_TIME
                         "INTEGER'%"ID_INT32_FMT"', "         // PASSWORD_GRACE_TIME
                         "NULL, "                             // PASSWORD_REUSE_DATE
                         "INTEGER'%"ID_INT32_FMT"', "         // PASSWORD_REUSE_TIME
                         "INTEGER'%"ID_INT32_FMT"', "         // PASSWORD_REUSE_MAX
                         "INTEGER'%"ID_INT32_FMT"', "         // PASSWORD_REUSE_COUNT
                         "VARCHAR'%s', "                      // PASSWORD_VERIFY_FUNCTION
                         "CHAR'U', "                          // USER_TYPE
                         "CHAR'%s', "                         // DISABLE_TCP
                         "SYSDATE, SYSDATE )",
                         sUserID,
                         sUserName,
                         sCryptStr,
                         (mtdIntegerType) sParseTree->dataTBSID,
                         (mtdIntegerType) sParseTree->tempTBSID,
                         sAccountLock,
                         sPasswPolicy,
                         sParseTree->failLoginAttempts,
                         sParseTree->failedCount,
                         sParseTree->passwLockTime,
                         sParseTree->passwLifeTime,
                         sParseTree->passwGraceTime,
                         sParseTree->passwReuseTime,
                         sParseTree->passwReuseMax,
                         sParseTree->reuseCount,
                         sPasswVerifyFunc,
                         sDisableTCP );
    }

    IDE_TEST(qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                sSqlStr,
                                & sRowCnt ) != IDE_SUCCESS);

    IDE_TEST_RAISE(sRowCnt != 1, ERR_META_CRASH);

    /* BUG-37443
     * SYS_PASSWORD_HISTORY_  �н����� ��� */
    if ( ( sParseTree->passwReuseTime != 0 ) ||
         ( sParseTree->passwReuseMax != 0 ) )
    {
        IDE_TEST( qcmReuseVerify::addPasswHistory( aStatement,
                                                   (UChar*)sCryptStr,
                                                   sUserID )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing To Do */
    }

    for ( sAccess = sParseTree->access;
          sAccess != NULL;
          sAccess=sAccess->next )
    {
        if ( ( sAccess->dataTBSID != sParseTree->dataTBSID ) &&
             ( sAccess->dataTBSID != sParseTree->tempTBSID ) )
        {
            idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                             "INSERT INTO SYS_TBS_USERS_ VALUES( "
                             "INTEGER'%"ID_INT32_FMT"', "
                             "INTEGER'%"ID_INT32_FMT"', "
                             "INTEGER'%"ID_INT32_FMT"')",
                             (mtdIntegerType) sAccess->dataTBSID,
                             sUserID,
                             (sAccess->isAccess == ID_TRUE)? 1 : 0);

            IDE_TEST(qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                        sSqlStr,
                                        & sRowCnt ) != IDE_SUCCESS);

            IDE_TEST_RAISE(sRowCnt != 1, ERR_META_CRASH);
        }
        else
        {
            /* Nothing to do */
        }
    }

    IDE_TEST(qdpGrant::grantDefaultPrivs4CreateUser(
                 aStatement,
                 QCG_GET_SESSION_USER_ID(aStatement),
                 sUserID)
             != IDE_SUCCESS);

    /* PROJ-1812 ROLE */
    IDE_TEST(qdpGrant::grantPublicRole4CreateUser(
                 aStatement,
                 QCG_GET_SESSION_USER_ID(aStatement), // GrantorID
                 sUserID ) // GranteeID
             != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_META_CRASH)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCM_META_CRASH));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qdr::executeAlter(qcStatement * aStatement)
{
/***********************************************************************
 *
 * Description :
 *    ALTER USER ���� execution ���� �Լ�
 *
 * Implementation :
 *    1. ����ڸ����� user id������ ��Ÿ���� �о��.
 *    2. PASSWORD ����
 *    if ( IDENTIFIED BY clause �� �����ϴ� ��� )
 *    {
 *      UPDATE DBA_USERS_ SET PASSWORD=? WHERE USER_ID=?
 *    }
 *    3. DEFAULT TABLESPACE ����
 *    if ( DEFAULT TABLESPACE clause �� �����ϴ� ��� )
 *    {
 *      UPDATE DBA_USERS_ SET DEFAULT_TBS_ID=? WHERE USER_ID=?
 *    }
 *    4. TEMPORARY TABLESPACE ����
 *    if ( TEMPORARY TABLESPACE clause �� �����ϴ� ��� )
 *    {
 *      UPDATE DBA_USERS_ SET TEMP_TBS_ID=? WHERE USER_ID=?
 *    }
 *    5. ACCESS ���� �Ǵ� �߰�
 *    if ( ACCESS clause �� �����ϴ� ��� )
 *    {
 *      for ( each ACCESS )
 *      {
 *        if ( tablespace �� ���� ACCESS ������ �̹� ��Ÿ�� �����ϴ� ��� )
 *        {
 *          UPDATE SYS_TBS_USERS_ SET ACCESS=? WHERE USER_ID=? AND TBS_ID=?
 *        }
 *        else
 *        {
 *          INSERT INTO SYS_TBS_USERS_ VALUES ( tbs_id, user_id, is_access )
 *        }
 *      }
 *    }
 *
 ***********************************************************************/

#define IDE_FN "qdr::executeAlter"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qdr::executeAlter"));

    qdUserParseTree     * sParseTree;
    UInt                  sUserID;
    scSpaceID             sDefaultTBSID;
    scSpaceID             sTempTBSID;
    SChar                 sAccountLock[2]={0,};

    SChar                 sUserPasswd[ QC_MAX_NAME_LEN + 1 ];
    SChar                 sCryptStr[IDS_MAX_PASSWORD_BUFFER_LEN + 1];
    SChar               * sSqlStr;
    vSLong                sRowCnt;
    idBool                sExist;
    qdUserTBSAccess     * sAccess;
    SChar                 sUserName[ QC_MAX_OBJECT_NAME_LEN + 1 ]          = {0,};
    SChar                 sPasswVerifyFunc[QC_PASSWORD_OPT_LEN + 1] = {0,};
    qciUserInfo           sUserPasswOpts;
    idBool                sIsAlterReusePasswd = ID_FALSE;
    SChar                 sDisableTCP[2]={0,};

    idlOS::memset(&sUserPasswOpts, 0, ID_SIZEOF(sUserPasswOpts));

    sParseTree = (qdUserParseTree *)aStatement->myPlan->parseTree;

    IDE_TEST(qcmUser::getUserID(aStatement, sParseTree->userName, &sUserID)
             != IDE_SUCCESS);

    sUserPasswOpts.userID = sUserID;

    IDU_FIT_POINT( "qdr::executeAlter::alloc::sSqlStr",
                    idERR_ABORT_InsufficientMemory );

    IDE_TEST(STRUCT_ALLOC_WITH_SIZE(aStatement->qmxMem,
                                    SChar,
                                    QD_MAX_SQL_LENGTH,
                                    &sSqlStr)
             != IDE_SUCCESS);

    if ( QC_IS_NULL_NAME(sParseTree->password) != ID_TRUE )
    {
        QC_STR_COPY( sUserPasswd, sParseTree->password );

        /* PROJ-2207 Password policy support
         * ���� �Ǿ��ִ� session �� alter password ���� �ϴ� session��
         * �ٸ��� ������ alter password ���� �ϴ� sesstion��
         * PASSWORD OPTION ������ SYS_USERS_ META�� ���� �����;��� */
        IDE_TEST( qcmUser::getPasswPolicyInfo( aStatement,
                                               sUserID,
                                               &sUserPasswOpts )
                  != IDE_SUCCESS );

        IDE_TEST( qcmUser::getUserPassword( aStatement,
                                            sUserID,
                                            sUserPasswOpts.userPassword )
                  != IDE_SUCCESS );

        IDE_TEST( qcmUser::getCurrentDate ( aStatement,
                                            &sUserPasswOpts )
                  != IDE_SUCCESS );

        IDE_TEST( qcmReuseVerify::checkReusePasswd( aStatement,
                                                    sUserID,
                                                    (UChar*) sUserPasswOpts.userPassword,
                                                    &sIsAlterReusePasswd )
                  != IDE_SUCCESS );

        /* SYS_PASSWORD_HISTORY_ �̹� �н����尡 ���� �ϴ� ��� ������ salt�� ��ȣȭ �Ѵ�. */
        if ( sIsAlterReusePasswd == ID_TRUE )
        {
            // BUG-38565 password ��ȣȭ �˰��� ����
            idsPassword::crypt( sCryptStr,
                                sUserPasswd, idlOS::strlen( sUserPasswd ),
                                sUserPasswOpts.userPassword );

            /* PASSWORD_REUSE_MAX, PASSWORD_REUSE_TIME */
            if ( ( sUserPasswOpts.mAccLimitOpts.mPasswReuseMax != 0 ) ||
                 ( sUserPasswOpts.mAccLimitOpts.mPasswReuseTime != 0 ) )
            {
                IDE_TEST(qcmReuseVerify::actionPasswordReuse( aStatement,
                                                              (UChar*)sCryptStr,
                                                              &sUserPasswOpts )
                         != IDE_SUCCESS );
            }
            else
            {
                /* Nothing To Do */
            }
        }
        else
        {
            // BUG-38565 password ��ȣȭ �˰��� ����
            idsPassword::crypt( sCryptStr,
                                sUserPasswd, idlOS::strlen( sUserPasswd ),
                                NULL );
        }

        /* PASSWORD_VERIFY_FUNCTION */
        if ( *sUserPasswOpts.mAccLimitOpts.mPasswVerifyFunc != '\0' )
        {
            QC_STR_COPY( sUserName, sParseTree->userName );

            IDE_TEST( qcmReuseVerify::actionVerifyFunction( aStatement,
                                                            sUserName,
                                                            sUserPasswd,
                                                            sUserPasswOpts.mAccLimitOpts.mPasswVerifyFunc )
                      != IDE_SUCCESS );

        }
        else
        {
            // Nothing To Do
        }

        if (sUserPasswOpts.mAccLimitOpts.mPasswLifeTime != 0)
        {
            /* PROJ-2207 Password policy support
             * PASSWORD_LIFE_TIME ���� �� ��� ALTER PASSWORD �� ���
             * ����� ������ ���� + PASSWORD_LIFE_TIME ���� ���� ��
             * ALTER PASSWORD ���� �ϸ� PASSWORD_LIFE_TIME �� ���� �Ǿ���. */
            idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                             "UPDATE DBA_USERS_ "
                             "SET PASSWORD = VARCHAR'%s' , "
                             "    LAST_DDL_TIME = SYSDATE , "
                             "    PASSWORD_EXPIRY_DATE = "
                             "    TO_DATE( "
                             "    TO_CHAR(SYSDATE + PASSWORD_LIFE_TIME "
                             "    , 'YYYYMMDD') "
                             "    ,'YYYYMMDD') "
                             "WHERE USER_ID = INTEGER'%"ID_INT32_FMT"' ",
                             sCryptStr,
                             sUserID );
        }
        else
        {
            idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                             "UPDATE DBA_USERS_ "
                             "SET PASSWORD = VARCHAR'%s' , "
                             "    LAST_DDL_TIME = SYSDATE  "
                             "WHERE USER_ID = INTEGER'%"ID_INT32_FMT"'",
                             sCryptStr,
                             sUserID );
        }

        IDE_TEST(qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                    sSqlStr,
                                    & sRowCnt ) != IDE_SUCCESS);

        IDE_TEST_RAISE(sRowCnt != 1, ERR_META_CRASH);
    }
    else if ( QC_IS_NULL_NAME(sParseTree->dataTBSName) != ID_TRUE )
    {
        idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                         "UPDATE DBA_USERS_ "
                         "SET DEFAULT_TBS_ID = INTEGER'%"ID_INT32_FMT"' , "
                         "    LAST_DDL_TIME = SYSDATE  "
                         "WHERE USER_ID = INTEGER'%"ID_INT32_FMT"'",
                         (mtdIntegerType) sParseTree->dataTBSID,
                         sUserID );

        IDE_TEST(qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                    sSqlStr,
                                    & sRowCnt ) != IDE_SUCCESS);

        IDE_TEST_RAISE(sRowCnt != 1, ERR_META_CRASH);
    }
    else if ( QC_IS_NULL_NAME(sParseTree->tempTBSName) != ID_TRUE )
    {
        idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                         "UPDATE DBA_USERS_ "
                         "SET TEMP_TBS_ID = INTEGER'%"ID_INT32_FMT"' , "
                         "    LAST_DDL_TIME = SYSDATE  "
                         "WHERE USER_ID = INTEGER'%"ID_INT32_FMT"'",
                         (mtdIntegerType) sParseTree->tempTBSID,
                         sUserID );

        IDE_TEST(qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                    sSqlStr,
                                    & sRowCnt ) != IDE_SUCCESS);

        IDE_TEST_RAISE(sRowCnt != 1, ERR_META_CRASH);
    }
    else if ( sParseTree->access != NULL )
    {
        IDE_TEST( qcmUser::getDefaultTBS(
                      aStatement,
                      sUserID,
                      &sDefaultTBSID,
                      &sTempTBSID) != IDE_SUCCESS );

        for ( sAccess = sParseTree->access;
              sAccess != NULL;
              sAccess=sAccess->next )
        {
            if ( ( sAccess->dataTBSID != sDefaultTBSID ) &&
                 ( sAccess->dataTBSID != sTempTBSID ) )
            {
                IDE_TEST( qcmTablespace::existAccessTBS(
                              QC_SMI_STMT( aStatement ),
                              sAccess->dataTBSID,
                              sUserID,
                              &sExist) != IDE_SUCCESS );
                if ( sExist == ID_TRUE )
                {
                    idlOS::snprintf(
                        sSqlStr, QD_MAX_SQL_LENGTH,
                        "UPDATE SYS_TBS_USERS_ "
                        "SET IS_ACCESS = INTEGER'%"ID_INT32_FMT"' "
                        "WHERE USER_ID = INTEGER'%"ID_INT32_FMT"' "
                        "AND TBS_ID = INTEGER'%"ID_INT32_FMT"'",
                        (sAccess->isAccess == ID_TRUE)? 1 : 0,
                        sUserID,
                        (mtdIntegerType) sAccess->dataTBSID );
                }
                else
                {
                    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                                     "INSERT INTO SYS_TBS_USERS_ VALUES( "
                                     "INTEGER'%"ID_INT32_FMT"', "
                                     "INTEGER'%"ID_INT32_FMT"', "
                                     "INTEGER'%"ID_INT32_FMT"')",
                                     (mtdIntegerType) sAccess->dataTBSID,
                                     sUserID,
                                     (sAccess->isAccess == ID_TRUE)? 1 : 0);
                }
                IDE_TEST(qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                            sSqlStr,
                                            & sRowCnt ) != IDE_SUCCESS);

                IDE_TEST_RAISE(sRowCnt != 1, ERR_META_CRASH);
            }
            else
            {
                /* Nothing to do */
            }
        }

        // fix BUG-14394
        idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                         "UPDATE DBA_USERS_ "
                         "SET LAST_DDL_TIME = SYSDATE  "
                         "WHERE USER_ID = INTEGER'%"ID_INT32_FMT"'",
                         sUserID );

        IDE_TEST(qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                    sSqlStr,
                                    & sRowCnt ) != IDE_SUCCESS);

        IDE_TEST_RAISE(sRowCnt != 1, ERR_META_CRASH);
    }
    else if (  sParseTree->passwLimitFlag == QD_PASSWORD_POLICY_ENABLE )
    {
        /* set password policy default options */
        if (( sUserID != QC_SYS_USER_ID) && ( sUserID !=  QC_SYSTEM_USER_ID ))
        {

            passwPolicyDefaultOpts(aStatement);
        }
        else
        {
            // Nothing To Do
        }

        if ( QC_IS_NULL_NAME(sParseTree->passwVerifyFunc) != ID_TRUE )
        {
            QC_STR_COPY( sPasswVerifyFunc, sParseTree->passwVerifyFunc );
        }
        else
        {
            /* set password policy default PASSWORD_VERIFY_FUNCTION options */
            if (( sUserID != QC_SYS_USER_ID) && ( sUserID !=  QC_SYSTEM_USER_ID ))
            {
                if ( QCU_PASSWORD_VERIFY_FUNCTION[0] != '\0' )
                {
                    idlOS::strncpy( sPasswVerifyFunc,
                                    QCU_PASSWORD_VERIFY_FUNCTION,
                                    idlOS::strlen(QCU_PASSWORD_VERIFY_FUNCTION));
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
        }

        /* ALTER USER PASSWORD OPTION�� ���� �� ��� option parameter
         * �� ���� �ǰ� ACCOUNT_STATUS �� ���� ���� �ʴ´�.
         * ACCOUNT_STATUS �� LOCK_DATE, EXPIRY_DATE ����� ������ �ִ�.
         */
        if ( sParseTree->passwLifeTime != 0 )
        {
            /* BUG-37433 alter user limit
             * PASSWORD_LIFE_TIME ���� �� ��� ALTER USER LIMITD �� ���
             * ����� ������ ���� + passwrod_life_time ���� ���� ��
             * ALTER PASSWORD ���� �ϸ� PASSWORD_LIFE_TIME �� ���� �Ǿ���. */
            idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                             "UPDATE DBA_USERS_ SET "
                             "PASSWORD_EXPIRY_DATE = "
                             "TO_DATE( "
                             "TO_CHAR(SYSDATE + INTEGER'%"ID_INT32_FMT"' "
                             ", 'YYYYMMDD') "
                             ",'YYYYMMDD'), "
                             "PASSWORD_REUSE_DATE      = NULL, "
                             "PASSWORD_LIMIT_FLAG      = CHAR'T', "
                             "FAILED_LOGIN_COUNT       = INTEGER'0', "
                             "FAILED_LOGIN_ATTEMPTS    = INTEGER'%"ID_INT32_FMT"', "
                             "PASSWORD_LIFE_TIME       = INTEGER'%"ID_INT32_FMT"', "
                             "PASSWORD_REUSE_TIME      = INTEGER'%"ID_INT32_FMT"', "
                             "PASSWORD_REUSE_MAX       = INTEGER'%"ID_INT32_FMT"', "
                             "PASSWORD_LOCK_TIME       = INTEGER'%"ID_INT32_FMT"', "
                             "PASSWORD_GRACE_TIME      = INTEGER'%"ID_INT32_FMT"', "
                             "PASSWORD_VERIFY_FUNCTION = VARCHAR'%s' "
                             "WHERE USER_ID = INTEGER'%"ID_INT32_FMT"'",
                             sParseTree->passwLifeTime,
                             sParseTree->failLoginAttempts,
                             sParseTree->passwLifeTime,
                             sParseTree->passwReuseTime,
                             sParseTree->passwReuseMax,
                             sParseTree->passwLockTime,
                             sParseTree->passwGraceTime,
                             sPasswVerifyFunc,
                             sUserID
                             );
        }
        else
        {
            // BUG-41230 SYS_USERS_ => DBA_USERS_
            idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                             "UPDATE DBA_USERS_ SET "
                             "PASSWORD_EXPIRY_DATE     = NULL, "
                             "PASSWORD_REUSE_DATE      = NULL, "
                             "PASSWORD_LIMIT_FLAG      = CHAR'T', "
                             "FAILED_LOGIN_COUNT       = INTEGER'0', "
                             "FAILED_LOGIN_ATTEMPTS    = INTEGER'%"ID_INT32_FMT"', "
                             "PASSWORD_LIFE_TIME       = INTEGER'%"ID_INT32_FMT"', "
                             "PASSWORD_REUSE_TIME      = INTEGER'%"ID_INT32_FMT"', "
                             "PASSWORD_REUSE_MAX       = INTEGER'%"ID_INT32_FMT"', "
                             "PASSWORD_LOCK_TIME       = INTEGER'%"ID_INT32_FMT"', "
                             "PASSWORD_GRACE_TIME      = INTEGER'%"ID_INT32_FMT"', "
                             "PASSWORD_VERIFY_FUNCTION = VARCHAR'%s' "
                             "WHERE USER_ID = INTEGER'%"ID_INT32_FMT"'",
                             sParseTree->failLoginAttempts,
                             sParseTree->passwLifeTime,
                             sParseTree->passwReuseTime,
                             sParseTree->passwReuseMax,
                             sParseTree->passwLockTime,
                             sParseTree->passwGraceTime,
                             sPasswVerifyFunc,
                             sUserID
                             );
        }

        IDE_TEST(qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                    sSqlStr,
                                    & sRowCnt ) != IDE_SUCCESS);

        IDE_TEST_RAISE(sRowCnt != 1, ERR_META_CRASH);

        /* BUG-27443
         * SYS_PASSWORD_HISTORY_  �н����� ��� */
        if ( ( sParseTree->passwReuseTime != 0 ) ||
             ( sParseTree->passwReuseMax != 0 ) )
        {
            IDE_TEST( qcmUser::getUserPassword( aStatement,
                                                sUserID,
                                                sUserPasswOpts.userPassword )
                      != IDE_SUCCESS );

            /* SYS_PASSWORD_HISTORY_�� �н����� ���� ���� üũ */
            IDE_TEST( qcmReuseVerify::checkReusePasswd( aStatement,
                                                        sUserID,
                                                        (UChar*)sUserPasswOpts.userPassword,
                                                        &sIsAlterReusePasswd )
                      != IDE_SUCCESS );

            if ( sIsAlterReusePasswd == ID_TRUE )
            {
                /* �н����� ���� �Ǵ� �н������� PASSWORD_DATE ���� */
                IDE_TEST( qcmReuseVerify::updatePasswdDate( aStatement,
                                                            (UChar*)sUserPasswOpts.userPassword,
                                                            sUserID )
                          != IDE_SUCCESS );
            }
            else
            {
                IDE_TEST( qcmReuseVerify::addPasswHistory( aStatement,
                                                           (UChar*)sUserPasswOpts.userPassword,
                                                           sUserID )
                          != IDE_SUCCESS );
            }
        }
        else
        {
            /* Nothing To Do */
        }
    }
    else if ( sParseTree->expLock != QD_NONE_LOCK )
    {
        if( sParseTree->accountLock == QD_ACCOUNT_LOCK )
        {
            /* LOCK : ACCOUNT_LOCK = L
             *        ACCOUNT_LOCK_DATE = SYSDATE
             *        FAILED_LOGIN_COUNT = 0 */

            sAccountLock[0] = 'L';
            sAccountLock[1] = 0;
            
            // BUG-41230 SYS_USERS_ => DBA_USERS_
            idlOS::snprintf(
                sSqlStr, QD_MAX_SQL_LENGTH,
                "UPDATE DBA_USERS_ "
                "SET ACCOUNT_LOCK = CHAR'%s', "
                "ACCOUNT_LOCK_DATE = SYSDATE, "
                "FAILED_LOGIN_COUNT = 0 "
                "WHERE USER_ID = INTEGER'%"ID_INT32_FMT"' ",
                sAccountLock, sUserID );
        }
        else
        {
            /* UNLOCK : ACCOUNT_LOCK = N
             *          ACCOUNT_LOCK_DATE = NULL
             *          FAILED_LOGIN_COUNT = 0 */

            sAccountLock[0] = 'N';
            sAccountLock[1] = 0;
            
            // BUG-41230 SYS_USERS_ => DBA_USERS_
            idlOS::snprintf(
                sSqlStr, QD_MAX_SQL_LENGTH,
                "UPDATE DBA_USERS_ "
                "SET ACCOUNT_LOCK = CHAR'%s', "
                "ACCOUNT_LOCK_DATE = NULL, "
                "FAILED_LOGIN_COUNT = 0 "
                "WHERE USER_ID = INTEGER'%"ID_INT32_FMT"' ",
                sAccountLock, sUserID );
        }

        IDE_TEST(qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                    sSqlStr,
                                    & sRowCnt ) != IDE_SUCCESS);

        IDE_TEST_RAISE(sRowCnt != 1, ERR_META_CRASH);
    }
    else if ( sParseTree->disableTCP != QD_DISABLE_TCP_NONE )
    {
        /* PROJ-2474 SSL/TLS Support */
        if ( sParseTree->disableTCP == QD_DISABLE_TCP_TRUE )
        {
            sDisableTCP[0] = 'T';
            sDisableTCP[1] = 0;
        }
        else
        {
            sDisableTCP[0] = 'F';
            sDisableTCP[1] = 0;
        }

        // BUG-41230 SYS_USERS_ => DBA_USERS_
        idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                        "UPDATE DBA_USERS_ "
                        "SET DISABLE_TCP = CHAR'%s', "
                        "    LAST_DDL_TIME = SYSDATE  "
                        "WHERE USER_ID = INTEGER'%"ID_INT32_FMT"' ",
                        sDisableTCP, sUserID );

        IDE_TEST( qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                     sSqlStr,
                                     &sRowCnt ) != IDE_SUCCESS );

        IDE_TEST_RAISE( sRowCnt != 1, ERR_META_CRASH );
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_META_CRASH)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCM_META_CRASH));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qdr::validatePasswOpts( qcStatement * aStatement )
{
/***********************************************************************
 *
 * Description : PROJ-2207 Password policy support
 *      password policy option validate ó��
 *
 * Implementation :
 *
 ***********************************************************************/
    qdUserParseTree     * sParseTree;

    sParseTree = (qdUserParseTree *)aStatement->myPlan->parseTree;

    IDE_TEST_RAISE( sParseTree->failLoginAttempts > QC_PASSWORD_OPT_MAX_COUNT_VALUE,
                    ERR_COUNT_LIMIT );

    IDE_TEST_RAISE( sParseTree->passwLifeTime > QC_PASSWORD_OPT_MAX_DATE,
                    ERR_INVALID_RESOURCE_LIMIT );


    IDE_TEST_RAISE( sParseTree->passwReuseTime > QC_PASSWORD_OPT_MAX_DATE,
                    ERR_INVALID_RESOURCE_LIMIT );


    IDE_TEST_RAISE( sParseTree->passwReuseMax > QC_PASSWORD_OPT_MAX_COUNT_VALUE,
                    ERR_COUNT_LIMIT );


    IDE_TEST_RAISE( sParseTree->passwLockTime > QC_PASSWORD_OPT_MAX_DATE,
                    ERR_INVALID_RESOURCE_LIMIT );

    IDE_TEST_RAISE( sParseTree->passwGraceTime > QC_PASSWORD_OPT_MAX_DATE,
                    ERR_INVALID_RESOURCE_LIMIT );

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_INVALID_RESOURCE_LIMIT)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDR_INVALID_RESOURCE_LIMIT));
    }
    IDE_EXCEPTION(ERR_COUNT_LIMIT)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDR_COUNT_LIMIT));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


void qdr::passwPolicyDefaultOpts( qcStatement * aStatement )
{
/***********************************************************************
 *
 * Description : PROJ-2207 Password policy support
 *     altibase.properties�� ������ ������ ����
 *
 * Implementation :
 *
 ***********************************************************************/
    qdUserParseTree     * sParseTree;

    sParseTree = (qdUserParseTree *)aStatement->myPlan->parseTree;

    if (( sParseTree->failLoginAttempts == 0 ) &&
        ( QCU_FAILED_LOGIN_ATTEMPTS != 0 ))
    {
        sParseTree->failLoginAttempts = QCU_FAILED_LOGIN_ATTEMPTS;
    }
    else
    {
        // Nothing To Do
    }

    if (( sParseTree->passwLockTime == 0) &&
        ( QCU_PASSWORD_LOCK_TIME !=0 ))
    {
        sParseTree->passwLockTime = QCU_PASSWORD_LOCK_TIME;
    }
    else
    {
        // Nothing To Do
    }

    if (( sParseTree->passwLifeTime == 0 ) &&
        ( QCU_PASSWORD_LIFE_TIME != 0 ))
    {
        sParseTree->passwLifeTime = QCU_PASSWORD_LIFE_TIME;
    }
    else
    {
        // Nothing To Do
    }

    if (( sParseTree->passwGraceTime == 0 ) &&
        ( QCU_PASSWORD_GRACE_TIME != 0 ))
    {
        sParseTree->passwGraceTime = QCU_PASSWORD_GRACE_TIME;
    }
    else
    {
        // Nothing To Do
    }

    if (( sParseTree->passwReuseMax == 0 ) &&
        ( QCU_PASSWORD_REUSE_MAX != 0 ))
    {
        sParseTree->passwReuseMax = QCU_PASSWORD_REUSE_MAX;
    }
    else
    {
        // Nothing To Do
    }

    if (( sParseTree->passwReuseTime == 0 ) &&
        ( QCU_PASSWORD_REUSE_TIME != 0 ))
    {
        sParseTree->passwReuseTime = QCU_PASSWORD_REUSE_TIME;
    }
    else
    {
        // Nothing To Do
    }
}
