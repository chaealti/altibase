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
 * $Id: qsv.cpp 90971 2021-06-09 00:29:36Z khkwak $
 **********************************************************************/

#include <idl.h>
#include <ide.h>
#include <idx.h>
#include <cm.h>
#include <qsv.h>
#include <qsx.h>
#include <qcmUser.h>
#include <qcuSqlSourceInfo.h>
#include <qsvProcVar.h>
#include <qsvProcType.h>
#include <qsvProcStmts.h>
#include <qsvPkgStmts.h>
#include <qcmPkg.h>
#include <qcmProc.h>
#include <qsvEnv.h>
#include <qsxRelatedProc.h>
#include <qsxPkg.h>
#include <qcpManager.h>
#include <qdpPrivilege.h>
#include <qmv.h>
#include <qso.h>
#include <qcg.h>
#include <qcmSynonym.h>
#include <qmvQuerySet.h>
#include <qcgPlan.h>
#include <qdpRole.h>
#include <qtcCache.h>
#include <qmvShardTransform.h>
#include <sdi.h>

extern mtdModule mtdInteger;

extern mtfModule qsfMCountModule;
extern mtfModule qsfMDeleteModule;
extern mtfModule qsfMExistsModule;
extern mtfModule qsfMFirstModule;
extern mtfModule qsfMLastModule;
extern mtfModule qsfMNextModule;
extern mtfModule qsfMPriorModule;

extern qcNamePosition gSysName;
qcNamePosition gDBMSStandardName = {(SChar*)"DBMS_STANDARD", 0, 13};

IDE_RC qsv::parseCreateProc(qcStatement * aStatement)
{
#define IDE_FN "qsv::parseCreateProc"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qsv::parseCreateProc"));

    qsProcParseTree    * sParseTree;
    qcuSqlSourceInfo     sqlInfo;

    sParseTree = (qsProcParseTree *)(aStatement->myPlan->parseTree);

    aStatement->spvEnv->createProc = sParseTree;
 
    /* BUG-30059 */
    if ( qdbCommon::containDollarInName( &(sParseTree->procNamePos) ) == ID_TRUE )
    {
        sqlInfo.setSourceInfo(
            aStatement,
            &(sParseTree->procNamePos) );

        IDE_RAISE( CANT_USE_RESERVED_WORD );
    }

    // validation�ÿ� �����Ǳ� ������ �ӽ÷� �����Ѵ�.
    // PVO�� �Ϸ�Ǹ� �������� ������ �����ȴ�.
    sParseTree->stmtText    = aStatement->myPlan->stmtText;
    sParseTree->stmtTextLen = aStatement->myPlan->stmtTextLen;

    IDE_TEST(checkHostVariable( aStatement ) != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION( CANT_USE_RESERVED_WORD );
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QDB_RESERVED_WORD_IN_OBJECT_NAME,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qsv::validateCreateProc(qcStatement * aStatement)
{
#define IDE_FN "qsv::validateCreateProc"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qsv::validateCreateProc"));

    qsProcParseTree    * sParseTree;
    qsLabels           * sCurrLabel;
    qcuSqlSourceInfo     sqlInfo;
    idBool               sExist = ID_FALSE;
    UInt                 sUserID = QCG_GET_SESSION_USER_ID( aStatement );

    sParseTree = (qsProcParseTree *)(aStatement->myPlan->parseTree);

    // check already existing object
    IDE_TEST( qcm::existObject( aStatement,
                                ID_FALSE,
                                sParseTree->userNamePos,
                                sParseTree->procNamePos,
                                sParseTree->objType,
                                &( sParseTree->userID ),
                                &( sParseTree->procOID ),
                                & sExist )
              != IDE_SUCCESS );

    /* 1. procOID�� ������ ���� �̸��� PSM�� �����Ƿ� ����
     * 2. procOID�� QS_EMPTY_OID�ε�, OBJECT�� �����ϸ�
     *    ���� �̸��� PSM�� �ƴ� OBJECT�� ���� (���� ��� TABLE) */
    if( sParseTree->procOID != QS_EMPTY_OID )
    {
        sqlInfo.setSourceInfo( aStatement, & sParseTree->procNamePos );
        IDE_RAISE( ERR_DUP_PROC_NAME );
    }
    else
    {
        IDE_TEST_RAISE(sExist == ID_TRUE, ERR_EXIST_OBJECT_NAME);
    }

    // check grant
    IDE_TEST( qdpRole::checkDDLCreatePSMPriv( aStatement,
                                              sParseTree->userID )
              != IDE_SUCCESS );

    // BUG-42322
    IDE_TEST( setObjectNameInfo( aStatement,
                                 sParseTree->objType,
                                 sParseTree->userID,
                                 sParseTree->procNamePos,
                                 &sParseTree->objectNameInfo )
              != IDE_SUCCESS );
 
    // validation parameter
    IDE_TEST(qsvProcVar::validateParaDef(aStatement, sParseTree->paraDecls)
             != IDE_SUCCESS);

    IDE_TEST(qsvProcVar::setParamModules(
                 aStatement,
                 sParseTree->paraDeclCount,
                 sParseTree->paraDecls,
                 (mtdModule***) & (sParseTree->paramModules),
                 &(sParseTree->paramColumns))
             != IDE_SUCCESS );

    // PR-24408
    QCG_SET_SESSION_USER_ID( aStatement, sParseTree->userID );
    
    // PROJ-1685
    if( sParseTree->procType == QS_NORMAL )
    {
        // set label name (= procedure name)
        IDE_TEST(STRUCT_ALLOC(QC_QMP_MEM(aStatement), qsLabels, &sCurrLabel)
                 != IDE_SUCCESS);

        SET_POSITION( sCurrLabel->namePos, sParseTree->procNamePos );
        sCurrLabel->id      = qsvEnv::getNextId(aStatement->spvEnv);
        sCurrLabel->stmt    = NULL;
        sCurrLabel->next    = NULL;

        sParseTree->block->common.parentLabels = sCurrLabel;

        // parsing body
        IDE_TEST( sParseTree->block->common.parse(
                      aStatement, (qsProcStmts *)(sParseTree->block))
                  != IDE_SUCCESS);
 
        // validation body
        IDE_TEST( sParseTree->block->common.
                  validate( aStatement,
                            (qsProcStmts *)(sParseTree->block),
                            NULL,
                            QS_PURPOSE_PSM )
                  != IDE_SUCCESS);
    }
    else
    {
        IDE_TEST( validateExtproc( aStatement, sParseTree ) != IDE_SUCCESS );
    }

    QCG_SET_SESSION_USER_ID( aStatement, sUserID );
   
    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_EXIST_OBJECT_NAME);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDB_EXIST_OBJECT_NAME));
    }
    IDE_EXCEPTION(ERR_DUP_PROC_NAME);
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QSV_DUPLICATED_PROC_NAME_SQLTEXT,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION_END;

    QCG_SET_SESSION_USER_ID( aStatement, sUserID );

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qsv::validateReplaceProc(qcStatement * aStatement)
{
#define IDE_FN "qsv::validateReplaceProc"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qsv::validateReplaceProc"));

    qsProcParseTree    * sParseTree;
    qsLabels           * sCurrLabel;
    idBool               sExist = ID_FALSE;
    UInt                 sUserID = QCG_GET_SESSION_USER_ID( aStatement );
    idBool               sIsUsed = ID_FALSE;
    SInt                 sObjectType = QS_OBJECT_MAX;

    sParseTree = (qsProcParseTree *)(aStatement->myPlan->parseTree);

    // check already exist object
    IDE_TEST( qcm::existObject( aStatement,
                                ID_FALSE,
                                sParseTree->userNamePos,
                                sParseTree->procNamePos,
                                sParseTree->objType,
                                &( sParseTree->userID ),
                                &( sParseTree->procOID ),
                                & sExist )
              != IDE_SUCCESS );

    /* BUG-48290 shard object�� ���� DDL ���� */
    IDE_TEST( sdi::checkShardObjectForDDL( aStatement, SDI_DDL_TYPE_PROCEDURE ) != IDE_SUCCESS );

    if( sExist == ID_TRUE )
    {
        if( sParseTree->procOID == QS_EMPTY_OID )
        {
            IDE_RAISE( ERR_EXIST_OBJECT_NAME );
        }
        else
        {
            /* BUG-39101
               ��ü�� type�� Ȯ���Ͽ� parse tree�� type�� �������� ������,
               ������ ��ü���� ���� �ٸ� type�� psm ��ü�̹Ƿ�
               �����޽����� �����Ѵ�. */
            IDE_TEST( qcmProc::getProcObjType( aStatement,
                                               sParseTree->procOID,
                                               &sObjectType )
                      != IDE_SUCCESS );

            if ( sObjectType == sParseTree->objType )
            {
                // replace old procedure
                sParseTree->common.execute = qsx::replaceProcOrFunc;

                // check grant
                IDE_TEST( qdpRole::checkDDLAlterPSMPriv(
                              aStatement,
                              sParseTree->userID )
                          != IDE_SUCCESS );
            }
            else
            {
                IDE_RAISE( ERR_EXIST_OBJECT_NAME );
            }
        }
    }
    else
    {
        // create new procedure
        sParseTree->common.execute = qsx::createProcOrFunc;

        // check grant
        IDE_TEST( qdpRole::checkDDLCreatePSMPriv( aStatement,
                                                  sParseTree->userID )
                  != IDE_SUCCESS );
    }

    // BUG-42322
    IDE_TEST( setObjectNameInfo( aStatement,
                                 sParseTree->objType,
                                 sParseTree->userID,
                                 sParseTree->procNamePos,
                                 &sParseTree->objectNameInfo )
              != IDE_SUCCESS );

    // BUG-38800 Server startup�� Function-Based Index���� ��� ���� Function��
    // recompile �� �� �־�� �Ѵ�.
    if ( ( aStatement->session->mQPSpecific.mFlag & QC_SESSION_INTERNAL_LOAD_PROC_MASK )
         != QC_SESSION_INTERNAL_LOAD_PROC_TRUE )
    {
        /* BUG-35445 Check Constraint, Function-Based Index���� ��� ���� Function�� ����/���� ���� */
        IDE_TEST( qcmProc::relIsUsedProcByConstraint(
                      aStatement,
                      &(sParseTree->procNamePos),
                      sParseTree->userID,
                      &sIsUsed )
                  != IDE_SUCCESS );
        IDE_TEST_RAISE( sIsUsed == ID_TRUE, ERR_PROC_USED_BY_CONSTRAINT );

        IDE_TEST( qcmProc::relIsUsedProcByIndex(
                      aStatement,
                      &(sParseTree->procNamePos),
                      sParseTree->userID,
                      &sIsUsed )
                  != IDE_SUCCESS );
        IDE_TEST_RAISE( sIsUsed == ID_TRUE, ERR_PROC_USED_BY_INDEX );
    }
    else
    {
        // Nothing to do.
    }

    // validation parameter
    IDE_TEST(qsvProcVar::validateParaDef(aStatement, sParseTree->paraDecls)
             != IDE_SUCCESS);

    IDE_TEST(qsvProcVar::setParamModules(
                 aStatement,
                 sParseTree->paraDeclCount,
                 sParseTree->paraDecls,
                 (mtdModule***) & sParseTree->paramModules,
                 &(sParseTree->paramColumns) )
             != IDE_SUCCESS );

    // PR-24408
    QCG_SET_SESSION_USER_ID( aStatement, sParseTree->userID );

    // PROJ-1685
    if( sParseTree->procType == QS_NORMAL )
    {
        // set label name (= procedure name)
        IDE_TEST(STRUCT_ALLOC(QC_QMP_MEM(aStatement), qsLabels, &sCurrLabel)
                 != IDE_SUCCESS);
 
        SET_POSITION( sCurrLabel->namePos, sParseTree->procNamePos );
 
        sCurrLabel->id   = qsvEnv::getNextId(aStatement->spvEnv);
        sCurrLabel->stmt = NULL;
        sCurrLabel->next = NULL;
 
        sParseTree->block->common.parentLabels = sCurrLabel;

        // parsing body
        IDE_TEST( sParseTree->block->common.parse(
                      aStatement,
                      (qsProcStmts *)(sParseTree->block))
                  != IDE_SUCCESS);
        
        // validation body
        IDE_TEST( sParseTree->block->common.validate(
                      aStatement,
                      (qsProcStmts *)(sParseTree->block),
                      NULL,
                      QS_PURPOSE_PSM )
                  != IDE_SUCCESS);
    }
    else
    {
        IDE_TEST( validateExtproc( aStatement, sParseTree ) != IDE_SUCCESS );
    }

    QCG_SET_SESSION_USER_ID( aStatement, sUserID );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_EXIST_OBJECT_NAME);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDB_EXIST_OBJECT_NAME));
    }
    IDE_EXCEPTION( ERR_PROC_USED_BY_CONSTRAINT );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QSV_PROC_USED_BY_CONSTRAINT ) );
    }
    IDE_EXCEPTION( ERR_PROC_USED_BY_INDEX );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QSV_PROC_USED_BY_INDEX ) );
    }
    IDE_EXCEPTION_END;

    QCG_SET_SESSION_USER_ID( aStatement, sUserID );
    
    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qsv::validateReplaceProcForRecompile(qcStatement * aStatement)
{
#define IDE_FN "qsv::validateReplaceProcForRecompile"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qsv::validateReplaceProcForRecompile"));

    qsProcParseTree    * sParseTree;
    qsLabels           * sCurrLabel;
    idBool               sExist = ID_FALSE;
    UInt                 sUserID = QCG_GET_SESSION_USER_ID( aStatement );
    idBool               sIsUsed = ID_FALSE;
    SInt                 sObjectType = QS_OBJECT_MAX;

    sParseTree = (qsProcParseTree *)(aStatement->myPlan->parseTree);

    // check already exist object
    IDE_TEST( qcm::existObject( aStatement,
                                ID_FALSE,
                                sParseTree->userNamePos,
                                sParseTree->procNamePos,
                                sParseTree->objType,
                                &( sParseTree->userID ),
                                &( sParseTree->procOID ),
                                & sExist )
              != IDE_SUCCESS );

    if( sExist == ID_TRUE )
    {
        if( sParseTree->procOID == QS_EMPTY_OID )
        {
            IDE_RAISE( ERR_EXIST_OBJECT_NAME );
        }
        else
        {
            /* BUG-39101
               ��ü�� type�� Ȯ���Ͽ� parse tree�� type�� �������� ������,
               ������ ��ü���� ���� �ٸ� type�� psm ��ü�̹Ƿ�
               �����޽����� �����Ѵ�. */
            IDE_TEST( qcmProc::getProcObjType( aStatement,
                                               sParseTree->procOID,
                                               &sObjectType )
                      != IDE_SUCCESS );

            if ( sObjectType == sParseTree->objType )
            {
                // replace old procedure
                sParseTree->common.execute = qsx::replaceProcOrFunc;

                // check grant
                IDE_TEST( qdpRole::checkDDLAlterPSMPriv(
                              aStatement,
                              sParseTree->userID )
                          != IDE_SUCCESS );
            }
            else
            {
                IDE_RAISE( ERR_EXIST_OBJECT_NAME );
            }
        }
    }
    else
    {
        // create new procedure
        sParseTree->common.execute = qsx::createProcOrFunc;

        // check grant
        IDE_TEST( qdpRole::checkDDLCreatePSMPriv( aStatement,
                                                  sParseTree->userID )
                  != IDE_SUCCESS );
    }

    // BUG-42322
    IDE_TEST( setObjectNameInfo( aStatement,
                                 sParseTree->objType,
                                 sParseTree->userID,
                                 sParseTree->procNamePos,
                                 &sParseTree->objectNameInfo )
              != IDE_SUCCESS );

    // BUG-38800 Server startup�� Function-Based Index���� ��� ���� Function��
    // recompile �� �� �־�� �Ѵ�.
    if ( ( aStatement->session->mQPSpecific.mFlag & QC_SESSION_INTERNAL_LOAD_PROC_MASK )
         != QC_SESSION_INTERNAL_LOAD_PROC_TRUE )
    {
        /* BUG-35445 Check Constraint, Function-Based Index���� ��� ���� Function�� ����/���� ���� */
        IDE_TEST( qcmProc::relIsUsedProcByConstraint(
                      aStatement,
                      &(sParseTree->procNamePos),
                      sParseTree->userID,
                      &sIsUsed )
                  != IDE_SUCCESS );
        IDE_TEST_RAISE( sIsUsed == ID_TRUE, ERR_PROC_USED_BY_CONSTRAINT );

        IDE_TEST( qcmProc::relIsUsedProcByIndex(
                      aStatement,
                      &(sParseTree->procNamePos),
                      sParseTree->userID,
                      &sIsUsed )
                  != IDE_SUCCESS );
        IDE_TEST_RAISE( sIsUsed == ID_TRUE, ERR_PROC_USED_BY_INDEX );
    }
    else
    {
        // Nothing to do.
    }

    // validation parameter
    IDE_TEST(qsvProcVar::validateParaDef(aStatement, sParseTree->paraDecls)
             != IDE_SUCCESS);

    IDE_TEST(qsvProcVar::setParamModules(
                 aStatement,
                 sParseTree->paraDeclCount,
                 sParseTree->paraDecls,
                 (mtdModule***) & sParseTree->paramModules,
                 &(sParseTree->paramColumns) )
             != IDE_SUCCESS );

    // PR-24408
    QCG_SET_SESSION_USER_ID( aStatement, sParseTree->userID );

    // PROJ-1685
    if( sParseTree->procType == QS_NORMAL )
    {
        // set label name (= procedure name)
        IDE_TEST(STRUCT_ALLOC(QC_QMP_MEM(aStatement), qsLabels, &sCurrLabel)
                 != IDE_SUCCESS);
 
        SET_POSITION( sCurrLabel->namePos, sParseTree->procNamePos );
 
        sCurrLabel->id   = qsvEnv::getNextId(aStatement->spvEnv);
        sCurrLabel->stmt = NULL;
        sCurrLabel->next = NULL;
 
        sParseTree->block->common.parentLabels = sCurrLabel;

        // parsing body
        IDE_TEST( sParseTree->block->common.parse(
                      aStatement,
                      (qsProcStmts *)(sParseTree->block))
                  != IDE_SUCCESS);
        
        // validation body
        IDE_TEST( sParseTree->block->common.validate(
                      aStatement,
                      (qsProcStmts *)(sParseTree->block),
                      NULL,
                      QS_PURPOSE_PSM )
                  != IDE_SUCCESS);
    }
    else
    {
        IDE_TEST( validateExtproc( aStatement, sParseTree ) != IDE_SUCCESS );
    }

    QCG_SET_SESSION_USER_ID( aStatement, sUserID );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_EXIST_OBJECT_NAME);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDB_EXIST_OBJECT_NAME));
    }
    IDE_EXCEPTION( ERR_PROC_USED_BY_CONSTRAINT );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QSV_PROC_USED_BY_CONSTRAINT ) );
    }
    IDE_EXCEPTION( ERR_PROC_USED_BY_INDEX );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QSV_PROC_USED_BY_INDEX ) );
    }
    IDE_EXCEPTION_END;

    QCG_SET_SESSION_USER_ID( aStatement, sUserID );
    
    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qsv::validateCreateFunc(qcStatement * aStatement)
{
#define IDE_FN "qsv::validateCreateFunc"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qsv::validateCreateFunc"));

    IDE_TEST( validateCreateProc( aStatement ) != IDE_SUCCESS );

    IDE_TEST( validateReturnDef( aStatement ) != IDE_SUCCESS );

    // check only in normal procedure.
    if ( ( aStatement->spvEnv->flag & QSV_ENV_RETURN_STMT_MASK )
         == QSV_ENV_RETURN_STMT_ABSENT )
    {
        IDE_RAISE( ERR_NOT_HAVE_RETURN_STMT );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_NOT_HAVE_RETURN_STMT);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QSV_PROC_NOT_HAVE_RETURN_STMT));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qsv::validateReplaceFunc(qcStatement * aStatement)
{
#define IDE_FN "qsv::validateReplaceFunc"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qsv::validateReplaceFunc"));


    IDE_TEST( validateReplaceProc( aStatement ) != IDE_SUCCESS );

    IDE_TEST( validateReturnDef( aStatement ) != IDE_SUCCESS );

    // check only in normal procedure.
    if ( ( aStatement->spvEnv->flag & QSV_ENV_RETURN_STMT_MASK )
         == QSV_ENV_RETURN_STMT_ABSENT )
    {
        IDE_RAISE( ERR_NOT_HAVE_RETURN_STMT );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_NOT_HAVE_RETURN_STMT);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QSV_PROC_NOT_HAVE_RETURN_STMT));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qsv::validateReturnDef( qcStatement * aStatement )
{
    qsProcParseTree    * sParseTree;
    mtcColumn          * sReturnTypeNodeColumn;
    qcuSqlSourceInfo     sqlInfo;
    qsVariables        * sReturnTypeVar;
    idBool               sFound;

    sParseTree = (qsProcParseTree *)(aStatement->myPlan->parseTree);

    sReturnTypeVar = sParseTree->returnTypeVar;

    // ���ռ� �˻�. returnŸ���� �ݵ�� �־�� ��.
    IDE_DASSERT( sReturnTypeVar != NULL );

    // validation return data type
    if (sReturnTypeVar->variableType == QS_COL_TYPE)
    {
        // return value���� column type�� ����ϴ� ���� �� 2����.
        // (1) table_name.column_name%TYPE
        // (2) user_name.table_name.column_name%TYPE
        if (QC_IS_NULL_NAME(sReturnTypeVar->variableTypeNode->tableName) == ID_TRUE)
        {
            sqlInfo.setSourceInfo( aStatement,
                                   & sReturnTypeVar->variableTypeNode->position );
            IDE_RAISE( ERR_NOT_SUPPORTED_DATATYPE );
        }
        else
        {
            IDE_TEST(qsvProcVar::checkAttributeColType(
                         aStatement, sReturnTypeVar)
                     != IDE_SUCCESS);

            sReturnTypeVar->variableType = QS_PRIM_TYPE;
        }
    }
    else if (sReturnTypeVar->variableType == QS_PRIM_TYPE)
    {
        // Nothing to do.
    }
    else if (sReturnTypeVar->variableType == QS_ROW_TYPE)
    {
        if (QC_IS_NULL_NAME(sReturnTypeVar->variableTypeNode->userName) == ID_TRUE)
        {
            // PROJ-1075 rowtype ���� ���.
            // (1) user_name.table_name%ROWTYPE
            // (2) table_name%ROWTYPE
            IDE_TEST(qsvProcVar::checkAttributeRowType( aStatement, sReturnTypeVar)
                     != IDE_SUCCESS);
        }
        else
        {
            sqlInfo.setSourceInfo( aStatement,
                                   & sReturnTypeVar->variableTypeNode->columnName );
            IDE_RAISE( ERR_NOT_SUPPORTED_DATATYPE );
        }
    }
    else if ( sReturnTypeVar->variableType == QS_UD_TYPE )
    {
        IDE_TEST( qsvProcVar::makeUDTVariable( aStatement,
                                               sReturnTypeVar )
                  != IDE_SUCCESS );
    }
    else
    {
        // ���ռ� �˻�. �� �̿��� Ÿ���� ����� �� �� ����.
        IDE_DASSERT(0);
    }

    IDE_TEST(STRUCT_ALLOC(QC_QMP_MEM(aStatement),
                          mtcColumn,
                          &(sParseTree->returnTypeColumn))
             != IDE_SUCCESS);

    sReturnTypeNodeColumn = QTC_STMT_COLUMN (aStatement,
                                             sReturnTypeVar->variableTypeNode);

    // to fix BUG-5773
    sReturnTypeNodeColumn->column.id     = 0;
    sReturnTypeNodeColumn->column.offset = 0;

    // sReturnTypeNodeColumn->flag = 0;
    // to fix BUG-13600 precision, scale�� �����ϱ� ����
    // column argument���� �����ؾ� ��.
    sReturnTypeNodeColumn->flag = sReturnTypeNodeColumn->flag & MTC_COLUMN_ARGUMENT_COUNT_MASK;
    sReturnTypeNodeColumn->type.languageId = sReturnTypeNodeColumn->language->id;

    /* PROJ-2586 PSM Parameters and return without precision
       �Ʒ� ���� �� �ϳ��� �����ϸ� precision�� �����ϱ� ���� �Լ� ȣ��.

       ���� 1. QCU_PSM_PARAM_AND_RETURN_WITHOUT_PRECISION_ENABLE == 1�̸鼭
       datatype�� flag�� QTC_NODE_SP_PARAM_OR_RETURN_PRECISION_ABSENT �� ��.
       ���� 2. QCU_PSM_PARAM_AND_RETURN_WITHOUT_PRECISION_ENABLE == 2 */
    /* �� �Ʒ��ʿ� �����ߴ°�?
       alloc���� �ϸ�, mtcColumn�� �����ߴ� flag ������ ����. */
    if( ((QCU_PSM_PARAM_AND_RETURN_WITHOUT_PRECISION_ENABLE == 1) &&
         (((sReturnTypeVar->variableTypeNode->lflag & QTC_NODE_SP_PARAM_OR_RETURN_PRECISION_MASK)
           == QTC_NODE_SP_PARAM_OR_RETURN_PRECISION_ABSENT))) /* ����1 */ ||
        (QCU_PSM_PARAM_AND_RETURN_WITHOUT_PRECISION_ENABLE == 2) /* ����2 */ )
    {
        IDE_TEST( setPrecisionAndScale( aStatement,
                                        sReturnTypeVar )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    /* BUG-44382 clone tuple ���ɰ��� */
    // ����� �ʱ�ȭ�� �ʿ���
    qtc::setTupleColumnFlag(
        QTC_STMT_TUPLE( aStatement, sReturnTypeVar->variableTypeNode ),
        ID_TRUE,
        ID_TRUE );

    mtc::copyColumn(sParseTree->returnTypeColumn,
                    sReturnTypeNodeColumn);

    // mtdModule ����
    sParseTree->returnTypeModule = (mtdModule *)sReturnTypeNodeColumn->module;

    // PROJ-1685
    if( sParseTree->block == NULL )
    {
        // external procedure
        sFound = ID_FALSE;
 
        switch( sParseTree->returnTypeColumn->type.dataTypeId )
        {
            case MTD_BIGINT_ID:
            case MTD_BOOLEAN_ID:
            case MTD_SMALLINT_ID:
            case MTD_INTEGER_ID:
            case MTD_REAL_ID:
            case MTD_DOUBLE_ID:
            case MTD_CHAR_ID:
            case MTD_VARCHAR_ID:
            case MTD_NCHAR_ID:
            case MTD_NVARCHAR_ID:
            case MTD_NUMERIC_ID:
            case MTD_NUMBER_ID:
            case MTD_FLOAT_ID:
            case MTD_DATE_ID:
            case MTD_INTERVAL_ID:
                sFound = ID_TRUE;
                break;
            default:
                break;
        }
 
        if ( sFound == ID_FALSE )
        {
            sqlInfo.setSourceInfo( aStatement,
                                   &sParseTree->returnTypeVar->common.name );
 
            IDE_RAISE( ERR_NOT_SUPPORTED_DATATYPE );
        }
        else
        {
            // Nothing to do.
        }
    }
    else
    {
        // normal procedure
        sFound = ID_TRUE;
        
        switch( sParseTree->returnTypeColumn->type.dataTypeId )
        {
            case MTD_LIST_ID:
                // list type�� ��ȯ�ϴ� user-defined function�� ������ �� ����.
                sFound = ID_FALSE;
                break;
            default:
                break;
        }
 
        if ( sFound == ID_FALSE )
        {
            sqlInfo.setSourceInfo( aStatement,
                                   &sParseTree->returnTypeVar->common.name );
 
            IDE_RAISE( ERR_NOT_SUPPORTED_DATATYPE );
        }
        else
        {
            // Nothing to do.
        }
    }

    /* PROJ-2452 Cache for DETERMINISTIC function */
    IDE_TEST( qtcCache::setIsCachableForFunction( sParseTree->paraDecls,
                                                  sParseTree->paramModules,
                                                  sParseTree->returnTypeModule,
                                                  &sParseTree->isDeterministic,
                                                  &sParseTree->isCachable )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_NOT_SUPPORTED_DATATYPE);
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QSV_PROC_NOT_SUPPORTED_DATATYPE_SQLTEXT,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC qsv::validateDropProc(qcStatement * aStatement)
{
#define IDE_FN "qsv::validateDropProc"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qsv::validateDropProc"));

    qsDropParseTree     * sParseTree;
    qcuSqlSourceInfo      sqlInfo;
    SInt                  sObjType;
    idBool                sIsUsed = ID_FALSE;

    sParseTree = (qsDropParseTree *)(aStatement->myPlan->parseTree);

    IDE_TEST(checkUserAndProcedure(
                 aStatement,
                 sParseTree->userNamePos,
                 sParseTree->procNamePos,
                 &(sParseTree->userID),
                 &(sParseTree->procOID) )
             != IDE_SUCCESS);

    /* BUG-48290 shard object�� ���� DDL ���� */
    IDE_TEST( sdi::checkShardObjectForDDL( aStatement, SDI_DDL_TYPE_PROCEDURE_DROP ) != IDE_SUCCESS );

    // check grant
    IDE_TEST( qdpRole::checkDDLDropPSMPriv( aStatement,
                                            sParseTree->userID )
              != IDE_SUCCESS );
    
    if (sParseTree->procOID == QS_EMPTY_OID)
    {
        sqlInfo.setSourceInfo( aStatement,
                               & sParseTree->procNamePos );
        IDE_RAISE( ERR_NOT_EXIST_PROC_NAME );
    }
    else
    {
        IDE_TEST( qcmProc::getProcObjType( aStatement,
                                           sParseTree->procOID,
                                           &sObjType )
                  != IDE_SUCCESS );

        /* BUG-39059
           ã�� ��ü�� type�� parsing �ܰ迡�� parse tree�� ���õ�
           Ÿ�԰� �������� ���� ���, �����޽����� �����Ѵ�. */
        if( sObjType != sParseTree->objectType )
        {
            sqlInfo.setSourceInfo( aStatement,
                                   & sParseTree->procNamePos );
            IDE_RAISE( ERR_NOT_EXIST_PROC_NAME );
        }
        else
        {
            // Nothing to do.
        }
    }

    /* BUG-35445 Check Constraint, Function-Based Index���� ��� ���� Function�� ����/���� ���� */
    IDE_TEST( qcmProc::relIsUsedProcByConstraint(
                  aStatement,
                  &(sParseTree->procNamePos),
                  sParseTree->userID,
                  &sIsUsed )
              != IDE_SUCCESS );
    IDE_TEST_RAISE( sIsUsed == ID_TRUE, ERR_PROC_USED_BY_CONSTRAINT );

    IDE_TEST( qcmProc::relIsUsedProcByIndex(
                  aStatement,
                  &(sParseTree->procNamePos),
                  sParseTree->userID,
                  &sIsUsed )
              != IDE_SUCCESS );
    IDE_TEST_RAISE( sIsUsed == ID_TRUE, ERR_PROC_USED_BY_INDEX );

    return IDE_SUCCESS;
    
    IDE_EXCEPTION(ERR_NOT_EXIST_PROC_NAME);
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QSV_NOT_EXIST_PROC_SQLTEXT,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION( ERR_PROC_USED_BY_CONSTRAINT );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QSV_PROC_USED_BY_CONSTRAINT ) );
    }
    IDE_EXCEPTION( ERR_PROC_USED_BY_INDEX );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QSV_PROC_USED_BY_INDEX ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qsv::validateAltProc(qcStatement * aStatement)
{
#define IDE_FN "qsv::validateAltProc"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qsv::validateAltProc"));

    qsAlterParseTree        * sParseTree;
    qcuSqlSourceInfo          sqlInfo;
    SInt                      sObjType;

    sParseTree = (qsAlterParseTree *)(aStatement->myPlan->parseTree);

    IDE_TEST(checkUserAndProcedure(
                 aStatement,
                 sParseTree->userNamePos,
                 sParseTree->procNamePos,
                 &(sParseTree->userID),
                 &(sParseTree->procOID) )
             != IDE_SUCCESS);

    // check grant
    IDE_TEST( qdpRole::checkDDLAlterPSMPriv( aStatement,
                                             sParseTree->userID )
              != IDE_SUCCESS );

    if (sParseTree->procOID == QS_EMPTY_OID)
    {
        sqlInfo.setSourceInfo( aStatement,
                               & sParseTree->procNamePos );
        IDE_RAISE( ERR_NOT_EXIST_PROC_NAME );
    }
    else
    {
        IDE_TEST( qcmProc::getProcObjType( aStatement,
                                           sParseTree->procOID,
                                           &sObjType )
                  != IDE_SUCCESS );

        /* BUG-39059
           ã�� ��ü�� type�� parsing �ܰ迡�� parse tree�� ���õ�
           Ÿ�԰� �������� ���� ���, �����޽����� �����Ѵ�. */
        if( sObjType != sParseTree->objectType )
        {
            sqlInfo.setSourceInfo( aStatement,
                                   & sParseTree->procNamePos );
            IDE_RAISE( ERR_NOT_EXIST_PROC_NAME );
        }
        else
        {
            // Nothing to do.
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_NOT_EXIST_PROC_NAME);
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QSV_NOT_EXIST_PROC_SQLTEXT,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}


IDE_RC qsv::parseExeProc(qcStatement * aStatement)
{
    qsExecParseTree     * sParseTree;
    idBool                sRecursiveCall     = ID_FALSE;
    qsxProcInfo         * sProcInfo;
    qsProcParseTree     * sProcPlanTree      = NULL;
    qcuSqlSourceInfo      sqlInfo;
    idBool                sExist             = ID_FALSE;
    idBool                sIsPkg             = ID_FALSE;
    qcmSynonymInfo        sSynonymInfo;
    // PROJ-1073 Package
    UInt                  sSubprogramID;
    qcNamePosition      * sUserName          = NULL;
    qcNamePosition      * sPkgName;
    qcNamePosition      * sProcName          = NULL;
    qsxPkgInfo          * sPkgInfo;
    idBool                sMyPkgSubprogram   = ID_FALSE;
    UInt                  sErrCode;
    // BUG-37655
    qsProcStmts         * sProcStmt;
    qsPkgStmts          * sCurrPkgStmt;
    qcNamePosition        sPos;
    qsxPkgObjectInfo    * sPkgBodyObjectInfo = NULL;
    qsOID                 sPkgBodyOID;
    // BUG-39194
    UInt                  sTmpUserID;
    // BUG-39084
    SChar               * sStmtText;
    qciStmtType           sTopStmtKind;
    // BUG-37820
    qsPkgSubprogramType   sSubprogramType;
    qsProcParseTree     * sCrtParseTree;
    UInt                  sOrgFlag;

    qsCallSpec          * sCallSpec;
    SChar                 sPkgNameStr[QC_MAX_OBJECT_NAME_LEN + 1];
    SChar                 sProcNameStr[QC_MAX_OBJECT_NAME_LEN + 1];
    SInt                  sProcNameIdx = 0;
    const SChar         * sProcNameArry[] = { "SET_SHARD_TABLE_SHARDKEY",
                                              "SET_SHARD_TABLE_SOLO",
                                              "SET_SHARD_TABLE_CLONE",                                              
                                              "UNSET_SHARD_TABLE",
                                              "UNSET_SHARD_PROCEDURE",
                                              "SET_SHARD_PROCEDURE_SHARDKEY",
                                              "SET_SHARD_PROCEDURE_SOLO",
                                              "SET_SHARD_PROCEDURE_CLONE",
                                              NULL };

    qcNamePosition        sUserNamePosition;
    qcNamePosition        sProcNamePosition;

    IDE_DASSERT( QC_SHARED_TMPLATE(aStatement) != NULL );
    IDE_DASSERT( QC_SHARED_TMPLATE(aStatement)->stmt != NULL );
    IDE_DASSERT( QC_SHARED_TMPLATE(aStatement)->stmt->myPlan != NULL );
    IDE_DASSERT( QC_SHARED_TMPLATE(aStatement)->stmt->myPlan->parseTree != NULL );

    sTopStmtKind  = QC_SHARED_TMPLATE(aStatement)->stmt->myPlan->parseTree->stmtKind;
    sParseTree    = (qsExecParseTree *)(aStatement->myPlan->parseTree);
    sCurrPkgStmt  = aStatement->spvEnv->currSubprogram;
    sCrtParseTree = aStatement->spvEnv->createProc;

    // initialize
    sParseTree->returnModule     = NULL;
    sParseTree->returnTypeColumn = NULL;
    sParseTree->pkgBodyOID       = QS_EMPTY_OID;
    sParseTree->subprogramID     = QS_INIT_SUBPROGRAM_ID;
    sParseTree->isCachable       = ID_FALSE;    // PROJ-2452

    IDU_LIST_INIT( &sParseTree->refCurList );

    if ( aStatement->spvEnv->createPkg == NULL )
    {
        // check recursive call in create procedure statement
        if ( aStatement->spvEnv->createProc != NULL )
        {
            IDE_TEST(checkRecursiveCall(aStatement,
                                        &sRecursiveCall)
                     != IDE_SUCCESS);
        }
        else
        {
            // Nothing to do.
        }
    }
    else
    {
        /* PROJ-1073 Package
           create package �� ������ package�� �����ִ� �ٸ� subprogram�� ȣ���ϴ���,
           subprogram�� �ڱ� �ڽ��� ���ȣ���ϴ��� �˻��Ѵ�. */
        IDE_TEST( checkMyPkgSubprogramCall( aStatement,
                                            &sMyPkgSubprogram,
                                            &sRecursiveCall )
                  != IDE_SUCCESS );
    }

    if (sRecursiveCall == ID_FALSE)
    {
        if( sMyPkgSubprogram == ID_FALSE )
        {
            /* PROJ-1073 Package
             *
             * procedure call�� ���
             * | USER   | TABLE    | COLUMN   | PKG |
             * |    x   | (USER)   | PSM NAME |   X |
             *
             * package�� procedure call�� ���
             * |  USER  | TABLE    | COLUMN   | PKG |
             * | (USER) | PKG NAME | PSM NAME |   X |
             *
             * ���� resolvePkg ȣ���� ���� resolvePSM�� ȣ���Ѵ�. */
            if ( QC_IS_NULL_NAME( sParseTree->callSpecNode->tableName) == ID_FALSE )
            {
                /* 1. Package�� PSM (sub program)���� Ȯ���Ѵ�. */
                // PROJ-1073 Package
                IDE_TEST( qcmSynonym::resolvePkg(
                              aStatement,
                              sParseTree->callSpecNode->userName,
                              sParseTree->callSpecNode->tableName,
                              &(sParseTree->procOID),
                              &(sParseTree->userID),
                              &sExist,
                              &sSynonymInfo )
                          != IDE_SUCCESS );

                if( sExist == ID_TRUE )
                {
                    sUserName = &(sParseTree->callSpecNode->userName);
                    sPkgName  = &(sParseTree->callSpecNode->tableName);
                    sProcName = &(sParseTree->callSpecNode->columnName);

                    sIsPkg = ID_TRUE;

                    // DBMS_SHARD package�߿� one transaction���� �����ϴ�
                    // ���ν����� ���� flag ����
                    QC_STR_COPY( sPkgNameStr, sParseTree->callSpecNode->tableName );
                    QC_STR_COPY( sProcNameStr, sParseTree->callSpecNode->columnName );

                    if ( idlOS::strMatch( sPkgNameStr,
                                          idlOS::strlen( sPkgNameStr ),
                                          "DBMS_SHARD",
                                          10 ) == 0 )
                    {
                        for( sProcNameIdx = 0; sProcNameArry[sProcNameIdx] != NULL; sProcNameIdx++ )
                        {           
                            if ( idlOS::strMatch( sProcNameStr,
                                                  idlOS::strlen( sProcNameStr ),
                                                  sProcNameArry[sProcNameIdx],
                                                  idlOS::strlen( sProcNameArry[sProcNameIdx] )) == 0 )
                            {
                                aStatement->mFlag &= ~QC_STMT_SHARD_DBMS_PKG_MASK;
                                aStatement->mFlag |= QC_STMT_SHARD_DBMS_PKG_TRUE;
                            }
                        }
                    }
                }
                else
                {
                    // user.package.proceudre�� variable�� ��
                    // user�� ���� ���� "User not found"�� �����´�.
                    // user�� ����� �� ���¿��� package�� �������� ������, sExists�� false�� �ǰ�,
                    // erroró���� ���� �ʴ´�.
                    // ����, user�� �����ϸ鼭, package�� �������� ������, erroró���� �ؾ��Ѵ�.
                    if( QC_IS_NULL_NAME( sParseTree->callSpecNode->userName ) != ID_TRUE )
                    {
                        sqlInfo.setSourceInfo( aStatement,
                                               & sParseTree->callSpecNode->tableName);
                        IDE_RAISE( ERR_NOT_EXIST_PKG_NAME );
                    }
                    else
                    {
                        // Nothing to do.
                    }
                }
            }
            else
            {
                // Nothing to do.
            }

            // BUG-46074 PSM Trigger with multiple triggering events
            // DBMS_STANDARD package�� subprogram�� �����ϱ� ���� �ڵ�
            // INSERTING���� ȣ���ص� DBMS_STANDARD.INSERTING�� ȣ���Ѵ�.
            if( (sExist == ID_FALSE) &&
                (QC_IS_NULL_NAME(sParseTree->callSpecNode->tableName) == ID_TRUE) )
            {
                if ( qcmSynonym::resolvePkg(
                         aStatement,
                         gSysName,
                         gDBMSStandardName,
                         &(sParseTree->procOID),
                         &(sParseTree->userID),
                         &sExist,
                         &sSynonymInfo)
                     == IDE_SUCCESS )
                {
                    if ( sExist == ID_TRUE )
                    {
                        sUserName = &gSysName;
                        sPkgName  = &gDBMSStandardName;
                        sProcName = &(sParseTree->callSpecNode->columnName);

                        IDE_TEST( qsxPkg::getPkgInfo( sParseTree->procOID,
                                                      &sPkgInfo )
                                  != IDE_SUCCESS );

                        sOrgFlag = aStatement->spvEnv->flag;
                        aStatement->spvEnv->flag &= ~QSV_ENV_ESTIMATE_ARGU_MASK;
                        aStatement->spvEnv->flag |= QSV_ENV_ESTIMATE_ARGU_TRUE;

                        if ( (sPkgInfo != NULL) && 
                             (qsvPkgStmts::getPkgSubprogramID( aStatement,
                                                               sParseTree->callSpecNode,
                                                              *sUserName,
                                                              *sPkgName,
                                                              *sProcName,
                                                               sPkgInfo,
                                                              &sSubprogramID )
                              == IDE_SUCCESS) )
                        {
                            if ( sSubprogramID == QS_PSM_SUBPROGRAM_ID )
                            {
                                IDE_CLEAR();
                                sUserName = NULL;
                                sPkgName  = NULL;
                                sProcName = NULL;
                                sExist = ID_FALSE;
                            }
                            else
                            {
                                sIsPkg = ID_TRUE;
                            }
                        }
                        else
                        {
                            IDE_CLEAR();
                            sUserName = NULL;
                            sPkgName  = NULL;
                            sProcName = NULL;
                            sExist = ID_FALSE;
                        }

                        aStatement->spvEnv->flag = sOrgFlag;
                    }
                }
            }

            if( sExist == ID_FALSE )
            {
                /* 2. PSM ���� Ȯ���Ѵ�. */
                if( qcmSynonym::resolvePSM(
                        aStatement,
                        sParseTree->callSpecNode->tableName,
                        sParseTree->callSpecNode->columnName,
                        &(sParseTree->procOID),
                        &(sParseTree->userID),
                        &sExist,
                        &sSynonymInfo)
                    == IDE_SUCCESS )
                {
                    if( sExist == ID_TRUE )
                    {
                        sUserName = &(sParseTree->callSpecNode->tableName);
                        sProcName = &(sParseTree->callSpecNode->columnName);
                    }
                    else
                    {
                        sqlInfo.setSourceInfo( aStatement,
                                               & sParseTree->callSpecNode->columnName);
                        IDE_RAISE( ERR_NOT_EXIST_PROC_NAME );
                    }
                }
                else
                {
                    sErrCode = ideGetErrorCode();
                    if( sErrCode == qpERR_ABORT_QCM_NOT_EXIST_USER )
                    {
                        if ( QC_IS_NULL_NAME( sParseTree->callSpecNode->tableName ) != ID_TRUE )
                        {
                            sPos.stmtText = sParseTree->callSpecNode->tableName.stmtText;
                            sPos.offset   = sParseTree->callSpecNode->tableName.offset;
                            sPos.size     = sParseTree->callSpecNode->columnName.offset + 
                                sParseTree->callSpecNode->columnName.size -
                                sParseTree->callSpecNode->tableName.offset;
                        }
                        else
                        {
                            sPos = sParseTree->callSpecNode->columnName;
                        }

                        sqlInfo.setSourceInfo( aStatement,
                                               & sPos );
                        IDE_RAISE( ERR_INVALID_IDENTIFIER );
                    }
                    else
                    {
                        IDE_RAISE( ERR_NOT_CHANGE_MSG );
                    }
                }
            }

            /* BUG-36728 Check Constraint, Function-Based Index���� Synonym�� ����� �� ����� �մϴ�. */
            if ( sSynonymInfo.isSynonymName == ID_TRUE )
            {
                sParseTree->callSpecNode->lflag &= ~QTC_NODE_SP_SYNONYM_FUNC_MASK;
                sParseTree->callSpecNode->lflag |= QTC_NODE_SP_SYNONYM_FUNC_TRUE;
            }
            else
            {
                /* Nothing to do */
            }

            /* PSM�� ȣ���ϴ� ��� */
            if( sIsPkg == ID_FALSE )
            {
                sParseTree->subprogramID = QS_PSM_SUBPROGRAM_ID;

                // synonym���� �����Ǵ� proc�� ���
                IDE_TEST( qsvProcStmts::makeProcSynonymList(
                              aStatement,
                              &sSynonymInfo,
                              *sUserName,
                              *sProcName,
                              sParseTree->procOID )
                          != IDE_SUCCESS );

                // search or make related object list
                IDE_TEST(qsvProcStmts::makeRelatedObjects(
                             aStatement,
                             sUserName,
                             sProcName,
                             & sSynonymInfo,
                             0,
                             QS_PROC )
                         != IDE_SUCCESS);

                // make procOID list
                IDE_TEST(qsxRelatedProc::prepareRelatedPlanTree(
                             aStatement,
                             sParseTree->procOID,
                             0,
                             &(aStatement->spvEnv->procPlanList))
                         != IDE_SUCCESS);

                IDE_TEST(qsxProc::getProcInfo( sParseTree->procOID,
                                               &sProcInfo)
                         != IDE_SUCCESS);

                // PROJ-2717 Internal procedure
                //   Library�� load���� ���߰ų�, function�� ����� ã�� ���Ѱ�� ����
                if ( sProcInfo->planTree->procType == QS_INTERNAL_C )
                {
                    sCallSpec = sProcInfo->planTree->expCallSpec;

                    IDE_TEST_RAISE( sCallSpec->libraryNode == NULL,
                                    ERR_LIB_NOT_FOUND );

                    IDE_TEST_RAISE( sCallSpec->libraryNode->mHandle == PDL_SHLIB_INVALID_HANDLE,
                                    ERR_LIB_NOT_FOUND );

                    IDE_TEST_RAISE( sCallSpec->libraryNode->mFunctionPtr == NULL,
                                    ERR_ENTRY_FUNCTION_NOT_FOUND );

                    IDE_TEST_RAISE( sCallSpec->functionPtr == NULL,
                                    ERR_FUNCTION_NOT_FOUND );
                }

                /* BUG-45164 */
                IDE_TEST_RAISE( sProcInfo->isValid != ID_TRUE, err_object_invalid );

                /* BUG-48443 */
                if ( ( aStatement->mFlag & QC_STMT_PACKAGE_RECOMPILE_MASK )
                     == QC_STMT_PACKAGE_RECOMPILE_TRUE )
                {
                    sProcInfo->isValid = ID_FALSE;
                }

                sProcPlanTree = sProcInfo->planTree;
                sStmtText     = sProcPlanTree->stmtText; 

                // PROJ-2646 shard analyzer enhancement
                if ( ( sdi::isShardCoordinator( aStatement ) == ID_TRUE ) ||
                     ( sdi::detectShardMetaChange( aStatement ) == ID_TRUE ) )
                {
                    // BUG-48435
                    // Synonym���� procedure�� ȣ���� ���� synonym��
                    // user name�� procedure name�� ����Ѵ�.
                    if ( sSynonymInfo.isSynonymName == ID_TRUE )
                    {
                        sUserNamePosition.stmtText = (SChar*)&sSynonymInfo.objectOwnerName;
                        sUserNamePosition.offset   = 0;
                        sUserNamePosition.size     = idlOS::strlen(sSynonymInfo.objectOwnerName);

                        sProcNamePosition.stmtText = (SChar*)&sSynonymInfo.objectName;
                        sProcNamePosition.offset   = 0;
                        sProcNamePosition.size     = idlOS::strlen(sSynonymInfo.objectName);

                        sUserName = &sUserNamePosition;
                        sProcName = &sProcNamePosition;
                    }

                    IDE_TEST( sdi::getProcedureInfoWithPlanTree(
                                  aStatement,
                                  sProcPlanTree->userID,
                                  *sUserName,
                                  *sProcName,
                                  sProcPlanTree,
                                  &(sParseTree->mShardObjInfo) )
                              != IDE_SUCCESS );

                    if ( sParseTree->mShardObjInfo != NULL )
                    {
                        aStatement->mFlag &= ~QC_STMT_SHARD_OBJ_MASK;
                        aStatement->mFlag |= QC_STMT_SHARD_OBJ_EXIST;
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

                // BUG-37655
                // pragma restrict reference
                if( aStatement->spvEnv->createPkg != NULL )
                {
                    for( sProcStmt  = sProcInfo->planTree->block->bodyStmts;
                         sProcStmt != NULL;
                         sProcStmt  = sProcStmt->next )
                    {
                        IDE_TEST( qsvPkgStmts::checkPragmaRestrictReferencesOption(
                                      aStatement,
                                      aStatement->spvEnv->currSubprogram,
                                      sProcStmt,
                                      aStatement->spvEnv->isPkgInitializeSection )
                                  != IDE_SUCCESS );
                    }
                }

                // PROJ-1075 typeset�� execute �� �� ����.
                if( sProcInfo->planTree->objType == QS_TYPESET )
                {
                    sqlInfo.setSourceInfo( aStatement,
                                           & sParseTree->callSpecNode->columnName);
                    IDE_RAISE( ERR_NOT_EXIST_PROC_NAME );
                }
                else
                {
                    // Nothing to do.
                }

                /* BUG-39770
                   package�� �����ϴ� ��ü�� related object�� �����ų�,
                   package�� �����ϴ� psm��ü�� �����ϸ�
                   parallel�� ����Ǹ� �� �ȴ�. */
                if ( sProcInfo->planTree->referToPkg == ID_TRUE )
                {
                    sParseTree->callSpecNode->lflag &= ~QTC_NODE_PKG_MEMBER_MASK;
                    sParseTree->callSpecNode->lflag |= QTC_NODE_PKG_MEMBER_EXIST;
                }
                else
                {
                    // Nothing to do.
                }

                // BUG-47971 Package global variable�� �л� ������ �����մϴ�.
                if ( sProcInfo->planTree->referToPkgVar == ID_TRUE )
                {
                    aStatement->spvEnv->flag &= ~QSV_ENV_PKG_VAR_EXIST_MASK;
                    aStatement->spvEnv->flag |= QSV_ENV_PKG_VAR_EXIST_TRUE;

                    if ( aStatement->spvEnv->createProc != NULL )
                    {
                        aStatement->spvEnv->createProc->referToPkgVar = ID_TRUE;
                    }
                }
            }
            else /* Package�� subprogram�� ȣ���ϴ� ��� */
            {
                // PROJ-1073 Package
                IDE_TEST( qsvPkgStmts::makePkgSynonymList(
                              aStatement,
                              & sSynonymInfo,
                              *sUserName,
                              *sPkgName,
                              sParseTree->procOID )
                          != IDE_SUCCESS );

                // search or make related object list
                IDE_TEST(qsvPkgStmts::makeRelatedObjects(
                             aStatement,
                             sUserName,
                             sPkgName,
                             & sSynonymInfo,
                             0,
                             QS_PKG )
                         != IDE_SUCCESS);

                // make procOID list
                IDE_TEST(qsxRelatedProc::prepareRelatedPlanTree(
                             aStatement,
                             sParseTree->procOID,
                             QS_PKG,
                             &(aStatement->spvEnv->procPlanList))
                         != IDE_SUCCESS);

                IDE_TEST( qsxPkg::getPkgInfo( sParseTree->procOID,
                                              &sPkgInfo )
                          != IDE_SUCCESS );

                /* 1) sPkgInfo�� NULL�̸� ����
                   2) sSubprogramID�� 0�� �ƴϳ� sPkgBodyInfo�� NULL�̸�
                   => qpERR_ABORT_QSV_INVALID_OR_MISSING_SUBPROGRAM
                   3) sSubprogramID�� 0�̰� sPkgBodyInfo�� NULL�̸�
                   => qpERR_ABORT_QSX_NOT_EXIST_SUBPROGRAM_OR_VARIABLE */

                // 1) spec�� package info�� �������� ����.
                IDE_TEST( sPkgInfo == NULL );

                /* BUG-45164 */
                IDE_TEST_RAISE( sPkgInfo->isValid != ID_TRUE, err_object_invalid );

                IDE_DASSERT( sPkgInfo->planTree != NULL );

                sStmtText    = sPkgInfo->planTree->stmtText;

                /* BUG-39194
                   qsExecParseTree->callSpecNode�� ������
                   package_name( �Ǵ� synonym_name ).record_name.field_name�̸� �� ã�� */
                sUserName = &(sPkgInfo->planTree->userNamePos);
                sPkgName  = &(sPkgInfo->planTree->pkgNamePos);

                if ( QC_IS_NULL_NAME(sParseTree->callSpecNode->userName) != ID_TRUE )
                {
                    if ( qcmUser::getUserID( aStatement,
                                             sParseTree->callSpecNode->userName,
                                             & sTmpUserID ) == IDE_SUCCESS )
                    {
                        IDE_DASSERT( sTmpUserID == sParseTree->userID );
                        sProcName = &(sParseTree->callSpecNode->columnName);
                    }
                    else
                    {
                        IDE_CLEAR();
                        sProcName = &(sParseTree->callSpecNode->tableName);
                    }
                }
                else
                {
                    sProcName = &(sParseTree->callSpecNode->columnName);
                }

                IDE_TEST( qsvPkgStmts::getPkgSubprogramID( aStatement,
                                                           sParseTree->callSpecNode,
                                                           *sUserName,
                                                           *sPkgName,
                                                           *sProcName,
                                                           sPkgInfo,
                                                           &sSubprogramID )
                          != IDE_SUCCESS );

                // 2) subprogram id�� �������� ����.
                IDE_TEST_RAISE( sSubprogramID == QS_PSM_SUBPROGRAM_ID,
                                ERR_NOT_EXIST_SUBPROGRAM );

                sParseTree->subprogramID  = sSubprogramID;

                /* BUG-38720
                   package body ���� ���� Ȯ�� �� body�� qsxPkgInfo�� �����´�. */
                IDE_TEST( qcmPkg::getPkgExistWithEmptyByNamePtr( aStatement,
                                                                 sParseTree->userID,
                                                                 (SChar*)( sPkgInfo->planTree->pkgNamePos.stmtText +
                                                                           sPkgInfo->planTree->pkgNamePos.offset ),
                                                                 sPkgInfo->planTree->pkgNamePos.size,
                                                                 QS_PKG_BODY,
                                                                 & sPkgBodyOID )
                          != IDE_SUCCESS);

                // BUG-41412 Variable���� ���� Ȯ���Ѵ�.
                if ( sSubprogramID == QS_PKG_VARIABLE_ID )
                {
                    /* BUG-39194
                       package variable�� �� qsExecParseTree->leftNode�� �־�� �Ѵ�. */
                    if ( sParseTree->leftNode == NULL )
                    {
                        if ( QC_IS_NULL_NAME(sParseTree->callSpecNode->pkgName) != ID_TRUE )
                        {
                            sPos = sParseTree->callSpecNode->pkgName;
                        }
                        else
                        {
                            sPos = sParseTree->callSpecNode->columnName;
                        }

                        sqlInfo.setSourceInfo( aStatement,
                                               & sPos );

                        IDE_RAISE( ERR_PKG_VARIABLE_NOT_HAVE_RETURN );
                    }
                    else
                    {
                        /* PROJ-2533
                           Member Function�� ��� spFunctionCallModle �� ��찡 �ִ�.
                           Member Function�� ������ �̸��� variable�� ���� �� ���� */
                        if ( ( sParseTree->callSpecNode->node.module == & qsfMCountModule ) ||
                             ( sParseTree->callSpecNode->node.module == & qsfMDeleteModule) ||
                             ( sParseTree->callSpecNode->node.module == & qsfMExistsModule) ||
                             ( sParseTree->callSpecNode->node.module == & qsfMFirstModule ) ||
                             ( sParseTree->callSpecNode->node.module == & qsfMLastModule  ) ||
                             ( sParseTree->callSpecNode->node.module == & qsfMNextModule  ) ||
                             ( sParseTree->callSpecNode->node.module == & qsfMPriorModule ) ||
                             ( ( sParseTree->callSpecNode->node.module == & qtc::spFunctionCallModule) &&
                               ( ( (sParseTree->callSpecNode->lflag) & QTC_NODE_SP_ARRAY_INDEX_VAR_MASK ) ==
                                 QTC_NODE_SP_ARRAY_INDEX_VAR_EXIST ) ) )
                        {
                            // BUG-41412 Member Function�� ��� columnModule�� �ٲ��� �ʴ´�.
                            // Nothing to do.
                        }
                        else
                        {
                            sParseTree->callSpecNode->node.module = & qtc::columnModule;
                        }

                        sParseTree->callSpecNode->lflag &= ~QTC_NODE_PROC_FUNCTION_MASK;
                        sParseTree->callSpecNode->lflag |= QTC_NODE_PROC_FUNCTION_FALSE;

                        sParseTree->common.validate = qsv::validateExecPkgAssign;
                        sParseTree->common.execute  = qsx::executePkgAssign;

                        // BUG-47971 Package global variable�� �л� ������ �����մϴ�.
                        aStatement->spvEnv->flag &= ~QSV_ENV_PKG_VAR_EXIST_MASK;
                        aStatement->spvEnv->flag |= QSV_ENV_PKG_VAR_EXIST_TRUE;

                        if ( aStatement->spvEnv->createProc != NULL )
                        {
                            aStatement->spvEnv->createProc->referToPkgVar = ID_TRUE;
                        }

                        IDE_CONT( IS_PACKAGE_VARIABLE );
                    }
                }
                else
                {
                    // Nothing to do.
                }

                if ( sPkgBodyOID != QS_EMPTY_OID )
                {
                    IDE_TEST( qsxPkg::getPkgObjectInfo( sPkgBodyOID,
                                                        &sPkgBodyObjectInfo )
                              != IDE_SUCCESS );

                    IDE_TEST_RAISE( sPkgBodyObjectInfo == NULL, ERR_NOT_EXIST_PKG_BODY_NAME );
                    IDE_TEST_RAISE( sPkgBodyObjectInfo->pkgInfo == NULL, ERR_NOT_EXIST_PKG_BODY_NAME );

                    sParseTree->pkgBodyOID = sPkgBodyObjectInfo->pkgInfo->pkgOID;
                }
                else
                {
                    /* BUG-39094
                       DDL�� ���� package body�� ��� �����ؾ� �Ѵ�. */
                    if ( (sTopStmtKind & QCI_STMT_MASK_MASK) != QCI_STMT_MASK_DDL )
                    {
                        sqlInfo.setSourceInfo( aStatement,
                                               & sParseTree->callSpecNode->tableName );
                        IDE_RAISE( ERR_NOT_EXIST_PKG_BODY_NAME );
                    }
                    else
                    {
                        // Nothing to do.
                    }
                }

                IDE_TEST( qsvPkgStmts::getPkgSubprogramPlanTree(
                              aStatement,
                              sPkgInfo->planTree,
                              sPkgInfo->objType,
                              sSubprogramID,
                              &sProcPlanTree )
                          != IDE_SUCCESS );

                IDE_DASSERT( sProcPlanTree != NULL );

                sProcPlanTree->procOID    = sParseTree->procOID;
                sProcPlanTree->pkgBodyOID = sParseTree->pkgBodyOID;

                // BUG-37655
                // pragma restrict reference
                if( ( aStatement->spvEnv->createPkg != NULL ) &&
                    ( sCurrPkgStmt != NULL ) )
                {
                    IDE_DASSERT( sCrtParseTree != NULL );

                    if ( sCrtParseTree->block != NULL )
                    {
                        if ( sCrtParseTree->block->isAutonomousTransBlock == ID_FALSE )
                        {
                            if( sCurrPkgStmt->flag != QS_PRAGMA_RESTRICT_REFERENCES_UNDEFINED )
                            {
                                IDE_RAISE( ERR_VIOLATES_PRAGMA );
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
                    }
                    else
                    {
                        // Nothing to do.
                    }
                }

                /* BUG-39770
                   package�� �����ϴ� ��ü�� related object�� �����ų�,
                   package�� �����ϴ� psm��ü�� �����ϸ�
                   parallel�� ����Ǹ� �� �ȴ�. */
                sParseTree->callSpecNode->lflag &= ~QTC_NODE_PKG_MEMBER_MASK;
                sParseTree->callSpecNode->lflag |= QTC_NODE_PKG_MEMBER_EXIST;

                // BUG-47971 Package global variable�� �л� ������ �����մϴ�.
                if ( sProcPlanTree->referToPkgVar == ID_TRUE )
                {
                    aStatement->spvEnv->flag &= ~QSV_ENV_PKG_VAR_EXIST_MASK;
                    aStatement->spvEnv->flag |= QSV_ENV_PKG_VAR_EXIST_TRUE;
                
                    if ( aStatement->spvEnv->createProc != NULL )
                    {
                        aStatement->spvEnv->createProc->referToPkgVar = ID_TRUE;
                    }
                }

                if ( (aStatement->spvEnv->createProc != NULL) &&
                     (aStatement->spvEnv->createPkg == NULL) )
                {
                    aStatement->spvEnv->createProc->referToPkg = ID_TRUE;
                }
                else
                {
                    // Nothing to do.
                }
            }

            /* PROJ-1090 Function-based Index */
            sParseTree->isDeterministic = sProcPlanTree->isDeterministic;

            // validation parameter
            IDE_TEST( validateArgumentsWithParser( aStatement,
                                                   sStmtText,
                                                   sParseTree->callSpecNode,
                                                   sProcPlanTree->paraDecls,
                                                   sProcPlanTree->paraDeclCount,
                                                   &sParseTree->refCurList,
                                                   sParseTree->isRootStmt )
                      != IDE_SUCCESS );
        }
        else // package body subprogram
        {
            sParseTree->paramModules = NULL;
            sParseTree->paramColumns = NULL;

            // subprogram search
            IDE_TEST( qsvPkgStmts::getMyPkgSubprogramID(
                          aStatement,
                          sParseTree->callSpecNode,
                          &sSubprogramType,
                          &sSubprogramID )
                      != IDE_SUCCESS );

            // 2) subprogram id�� �������� ����.
            IDE_TEST_RAISE( sSubprogramID == QS_PSM_SUBPROGRAM_ID,
                            ERR_NOT_EXIST_SUBPROGRAM );

            sParseTree->subprogramID  = sSubprogramID;

            IDE_TEST( qsvPkgStmts::getPkgSubprogramPlanTree(
                          aStatement,
                          aStatement->spvEnv->createPkg,
                          aStatement->spvEnv->createPkg->objType,
                          sSubprogramID,
                          &sProcPlanTree )
                      != IDE_SUCCESS );

            if( sProcPlanTree == NULL )
            {
                sqlInfo.setSourceInfo( aStatement,
                                       &sParseTree->callSpecNode->columnName );

                IDE_RAISE( ERR_NOT_DEFINE_SUBPROGRAM );
            }

            sProcPlanTree->pkgBodyOID = sParseTree->pkgBodyOID;

            if( ( aStatement->spvEnv->createPkg != NULL ) &&
                ( sCurrPkgStmt != NULL ) )
            {
                IDE_DASSERT( sCrtParseTree != NULL );

                if ( sCrtParseTree->block != NULL )
                {
                    if ( sCrtParseTree->block->isAutonomousTransBlock == ID_FALSE )
                    {
                        if( sCurrPkgStmt->flag != QS_PRAGMA_RESTRICT_REFERENCES_UNDEFINED )
                        {
                            IDE_RAISE( ERR_VIOLATES_PRAGMA );
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
                }
                else
                {
                    // Nothing to do.
                }
            }

            /* PROJ-1090 Function-based Index */
            sParseTree->isDeterministic = sProcPlanTree->isDeterministic;

            /* BUG-39770
               package�� �����ϴ� ��ü�� related object�� �����ų�,
               package�� �����ϴ� psm��ü�� �����ϸ�
               parallel�� ����Ǹ� �� �ȴ�. */
            sParseTree->callSpecNode->lflag &= ~QTC_NODE_PKG_MEMBER_MASK;
            sParseTree->callSpecNode->lflag |= QTC_NODE_PKG_MEMBER_EXIST;
        } /* sMyPkgSubprogram == ID_TRUE */
    }
    else  /* sRecursiveCall == ID_TRUE */
    {
        if( aStatement->spvEnv->createProc != NULL )
        { 
            if ( ( sParseTree->callSpecNode->lflag & QTC_NODE_SP_PARAM_DEFAULT_VALUE_MASK )
                 == QTC_NODE_SP_PARAM_DEFAULT_VALUE_FALSE ) 
            {
                sParseTree->isDeterministic = aStatement->spvEnv->createProc->isDeterministic;
            }
            else
            {
                // BUG-41228
                // parameter�� default�� �ڱ� �ڽ��� ȣ��(recursive call) �� �� ����.
                sqlInfo.setSourceInfo( aStatement,
                                       & sParseTree->callSpecNode->columnName);
                IDE_RAISE( ERR_NOT_EXIST_PROC_NAME );
            }
        }
        else
        {
            IDE_DASSERT( 0 );
        }
    }

    IDE_EXCEPTION_CONT( IS_PACKAGE_VARIABLE );

    // Shard Transformation ����
    IDE_TEST( qmvShardTransform::doTransform( aStatement )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_IDENTIFIER );
    {
        (void)sqlInfo.init( aStatement->qmeMem );
        IDE_SET(
            ideSetErrorCode( qpERR_ABORT_QSV_INVALID_IDENTIFIER,
                             sqlInfo.getErrMessage() ) );
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION( ERR_NOT_EXIST_PKG_NAME );
    {
        (void)sqlInfo.init( aStatement->qmeMem );
        IDE_SET(
            ideSetErrorCode( qpERR_ABORT_QSV_NOT_EXIST_PKG,
                             sqlInfo.getErrMessage() ) );
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION( ERR_NOT_DEFINE_SUBPROGRAM );
    {
        (void)sqlInfo.init( aStatement->qmeMem );
        IDE_SET(
            ideSetErrorCode( qpERR_ABORT_QSV_UNDEFINED_SUBPROGRAM, 
                             sqlInfo.getErrMessage() ) );
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION(ERR_NOT_EXIST_PROC_NAME);
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QSV_NOT_EXIST_PROC_SQLTEXT,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION(ERR_NOT_EXIST_SUBPROGRAM);
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QSX_NOT_EXIST_SUBPROGRAM_AND_VARIABLE,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION(ERR_NOT_EXIST_PKG_BODY_NAME);
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode( qpERR_ABORT_QSV_NOT_EXIST_PKG_BODY,
                             sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION(ERR_VIOLATES_PRAGMA)
    {
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QSV_VIOLATES_ASSOCIATED_PRAGMA) );
    }
    IDE_EXCEPTION( ERR_NOT_CHANGE_MSG )
    {
        // Do not set error code
        // reference : qtc::changeNodeFromColumnToSP();
    }
    IDE_EXCEPTION( ERR_PKG_VARIABLE_NOT_HAVE_RETURN )
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QSV_PACKAGE_VARIABLE_NOT_HAVE_RETURN,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION( err_object_invalid );   /* BUG-45164 */ 
    {
        IDE_SET( ideSetErrorCode(qpERR_ABORT_QSX_PLAN_INVALID) );
    }
    IDE_EXCEPTION( ERR_LIB_NOT_FOUND )
    {
        ideSetErrorCode( idERR_ABORT_IDX_LIBRARY_NOT_FOUND,
                         idxLocalSock::mHomePath,
                         IDX_LIB_DEFAULT_DIR,
                         sCallSpec->libraryNode->mLibPath );
    }
    IDE_EXCEPTION( ERR_ENTRY_FUNCTION_NOT_FOUND )
    {
        ideSetErrorCode( idERR_ABORT_IDX_ENTRY_FUNCTION_NOT_FOUND );
    }
    IDE_EXCEPTION( ERR_FUNCTION_NOT_FOUND )
    {
        ideSetErrorCode( idERR_ABORT_IDX_FUNCTION_NOT_FOUND );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qsv::validateExeProc(qcStatement * aStatement)
{
    qtcNode             * sProcReturnTypeNode;
    qsxProcInfo         * sProcInfo = NULL;
    qsxPkgInfo          * sPkgInfo = NULL;
    mtcColumn           * sExeReturnTypeColumn;
    mtcColumn           * sProcReturnTypeColumn;
    qsExecParseTree     * sExeParseTree;
    qsProcParseTree     * sCrtParseTree = NULL;
    qsPkgParseTree      * sCrtPkgParseTree;
    qcTemplate          * sTmplate;
    mtcColumn           * sColumn;
    mtcColumn           * sCallSpecColumn;
    idBool                sIsAllowSubq;
    qsxProcPlanList       sProcPlan;
    qcuSqlSourceInfo      sqlInfo;
    qsxPkgInfo          * sPkgBodyInfo = NULL;
    // BUG-37820
    qsPkgStmts          * sCurrStmt    = NULL;
    // fix BUG-42521
    UInt                  i = 0;
    qtcNode             * sArgument = NULL;
    qsVariableItems     * sParaDecls = NULL;
    qsVariableItems     * sProcParaDecls = NULL;

    IDU_FIT_POINT_FATAL( "qsv::validateExeProc::__FT__" );

    sExeParseTree = (qsExecParseTree *)aStatement->myPlan->parseTree;
    sCurrStmt     = aStatement->spvEnv->currSubprogram;

    if (sExeParseTree->procOID == QS_EMPTY_OID) // recursive call
    {
        if( sExeParseTree->subprogramID == QS_PSM_SUBPROGRAM_ID )
        {
            sCrtParseTree = aStatement->spvEnv->createProc;
        }
        else
        {
            // subprogram�� �ڽ��� ���ȣ������ ��
            if ( (aStatement->spvEnv->isPkgInitializeSection == ID_FALSE) &&
                 (qsvPkgStmts::checkIsRecursiveSubprogramCall( sCurrStmt,
                                                               sExeParseTree->subprogramID )
                  == ID_TRUE) )
            {
                sCrtParseTree  = aStatement->spvEnv->createProc;
            }
            else
            {
                sCrtPkgParseTree = aStatement->spvEnv->createPkg;

                IDE_TEST( qsvPkgStmts::getPkgSubprogramPlanTree(
                              aStatement,
                              sCrtPkgParseTree,
                              sCrtPkgParseTree->objType,
                              sExeParseTree->subprogramID,
                              &sCrtParseTree )
                          != IDE_SUCCESS );
            }

            if( sCrtParseTree == NULL )
            {
                sqlInfo.setSourceInfo( aStatement,
                                       &sExeParseTree->callSpecNode->columnName );

                IDE_RAISE( ERR_NOT_DEFINE_SUBPROGRAM );
            }
        }

        // in case of function
        if (sCrtParseTree->returnTypeVar != NULL)
        {
            if( sExeParseTree->subprogramID == QS_PSM_SUBPROGRAM_ID )
            {
                //fix PROJ-1596
                sTmplate = QC_SHARED_TMPLATE(aStatement->spvEnv->createProc->common.stmt);
            }
            else
            {
                sTmplate = QC_SHARED_TMPLATE(aStatement->spvEnv->createPkg->common.stmt);

                if (sExeParseTree->leftNode == NULL)
                {
                    sqlInfo.setSourceInfo(
                        aStatement,
                        & sExeParseTree->callSpecNode->columnName);
                    IDE_RAISE( ERR_FUNCTION_NOT_HAVE_RETURN );
                }
            }

            sColumn   =
                & (sTmplate->tmplate
                   .rows[sCrtParseTree->returnTypeVar->variableTypeNode->node.table]
                   .columns[sCrtParseTree->returnTypeVar->variableTypeNode->node.column]);

            sTmplate = QC_SHARED_TMPLATE(aStatement);

            // PROJ-1075
            // set return 
            // node�� �̹� ������� �ִ� ������.
            // primitive data type�� �ƴ� ��쿡 ��� �Ǵ��� üũ�� ���ƾ� ��.
            sCallSpecColumn = &( sTmplate->tmplate.rows[sExeParseTree->callSpecNode->node.table]
                                 .columns[sExeParseTree->callSpecNode->node.column]);

            mtc::copyColumn(sCallSpecColumn, sColumn);
        }

        // validation argument
        IDE_TEST( validateArgumentsWithoutParser( aStatement,
                                                  sExeParseTree->callSpecNode,
                                                  sCrtParseTree->paraDecls,
                                                  sCrtParseTree->paraDeclCount,
                                                  ID_FALSE /* no subquery is allowed */)
                  != IDE_SUCCESS );

        /* PROJ-2452 Caching for DETERMINISTIC Function */
        sExeParseTree->isCachable = sCrtParseTree->isCachable;
    }
    else    // NOT recursive call
    {
        if( sExeParseTree->subprogramID != QS_PSM_SUBPROGRAM_ID )
        {
            // get parameter declaration

            IDE_TEST( qsxPkg::getPkgInfo( sExeParseTree->procOID,
                                          &sPkgInfo )
                      != IDE_SUCCESS);

            // check grant
            IDE_TEST( qdpRole::checkDMLExecutePSMPriv( aStatement,
                                                       sPkgInfo->planTree->userID,
                                                       sPkgInfo->privilegeCount,
                                                       sPkgInfo->granteeID,
                                                       ID_FALSE,
                                                       NULL,
                                                       NULL)
                      != IDE_SUCCESS );

            /* BUG-38720
               package body ���� ���� Ȯ�� �� body�� qsxPkgInfo�� �����´�. */
            if ( sExeParseTree->pkgBodyOID != QS_EMPTY_OID )
            {
                IDE_TEST( qsxPkg::getPkgInfo( sExeParseTree->pkgBodyOID,
                                              &sPkgBodyInfo)
                          != IDE_SUCCESS );

                if ( sPkgBodyInfo->isValid == ID_TRUE )
                {
                    sProcPlan.pkgBodyOID         = sPkgBodyInfo->pkgOID;
                    sProcPlan.pkgBodyModifyCount = sPkgBodyInfo->modifyCount;
                }
                else
                {
                    sProcPlan.pkgBodyOID         = QS_EMPTY_OID;
                    sProcPlan.pkgBodyModifyCount = 0;
                }
            }
            else
            {
                /* BUG-39094
                   package body�� ��� ��ü�� �����Ǿ�� �Ѵ�. */
                sProcPlan.pkgBodyOID         = QS_EMPTY_OID;
                sProcPlan.pkgBodyModifyCount = 0;
            }

            IDE_TEST( qsvPkgStmts::getPkgSubprogramPlanTree(
                          aStatement,
                          sPkgInfo->planTree,
                          sPkgInfo->objType,
                          sExeParseTree->subprogramID,
                          &sCrtParseTree )
                      != IDE_SUCCESS );

            IDE_DASSERT( sCrtParseTree != NULL );

            // BUG-36973
            sProcPlan.objectID           = sPkgInfo->pkgOID;
            sProcPlan.modifyCount        = sPkgInfo->modifyCount;
            sProcPlan.objectType         = sPkgInfo->objType;
            sProcPlan.planTree           = (qsProcParseTree *) sCrtParseTree;
            sProcPlan.next = NULL;

            IDE_TEST( qcgPlan::registerPlanPkg( aStatement, &sProcPlan ) != IDE_SUCCESS );

            // no subquery is allowed on
            // 1. execute proc
            // 2. execute ? := func
            // 3. procedure invocation in a procedure or a function.
            // and stmtKind of these cases are set to QCI_STMT_MASK_SP mask.
            if ( (aStatement->spvEnv->flag & QSV_ENV_SUBQUERY_ON_ARGU_ALLOW_MASK)
                 == QSV_ENV_SUBQUERY_ON_ARGU_ALLOW_TRUE )
            {
                sIsAllowSubq = ID_TRUE;
            }
            else
            {
                sIsAllowSubq = ID_FALSE;
            }

            // validation parameter
            IDE_TEST( validateArgumentsAfterParser(
                          aStatement,
                          sExeParseTree->callSpecNode,
                          sCrtParseTree->paraDecls,
                          sIsAllowSubq )
                      != IDE_SUCCESS );

            // set parameter module array pointer.
            sExeParseTree->paraDeclCount = sCrtParseTree->paraDeclCount;

            // fix BUG-42521
            sProcParaDecls = sCrtParseTree->paraDecls;

            IDE_TEST( qsvProcVar::copyParamModules(
                          aStatement,
                          sExeParseTree->paraDeclCount,
                          sCrtParseTree->paramColumns,
                          (mtdModule***) & (sExeParseTree->paramModules),
                          &(sExeParseTree->paramColumns) )
                      != IDE_SUCCESS );

            if ( sCrtParseTree->returnTypeVar != NULL )
            {
                sProcReturnTypeNode = sCrtParseTree->returnTypeVar->variableTypeNode;

                if (sExeParseTree->leftNode == NULL)
                {
                    sqlInfo.setSourceInfo(
                        aStatement,
                        & sExeParseTree->callSpecNode->columnName);
                    IDE_RAISE( ERR_FUNCTION_NOT_HAVE_RETURN );
                }

                sExeReturnTypeColumn =
                    QTC_STMT_COLUMN( aStatement, sExeParseTree->callSpecNode );

                sProcReturnTypeColumn =
                    QTC_TMPL_COLUMN( sPkgInfo->tmplate,  sProcReturnTypeNode );

                //fix BUG-17410
                IDE_TEST( qsvProcType::copyColumn( aStatement,
                                                   sProcReturnTypeColumn,
                                                   sExeReturnTypeColumn,
                                                   (mtdModule **)&sExeReturnTypeColumn->module )
                          != IDE_SUCCESS );
            }
            else
            {
                if (sExeParseTree->leftNode != NULL)
                {
                    sqlInfo.setSourceInfo(
                        aStatement,
                        & sExeParseTree->callSpecNode->columnName);
                    IDE_RAISE( ERR_PROCEDURE_HAVE_RETURN );
                }
            }

            /* PROJ-2452 Caching for DETERMINISTIC Function */
            sExeParseTree->isCachable = sCrtParseTree->isCachable;
        }
        else
        {
            // get parameter declaration
            IDE_TEST( qsxProc::getProcInfo( sExeParseTree->procOID,
                                            &sProcInfo )
                      != IDE_SUCCESS);

            // check grant
            IDE_TEST( qdpRole::checkDMLExecutePSMPriv( aStatement,
                                                       sProcInfo->planTree->userID,
                                                       sProcInfo->privilegeCount,
                                                       sProcInfo->granteeID,
                                                       ID_FALSE,
                                                       NULL,
                                                       NULL)
                      != IDE_SUCCESS );

            sProcPlan.objectID           = sProcInfo->procOID;
            sProcPlan.modifyCount        = sProcInfo->modifyCount;
            sProcPlan.objectType         = sProcInfo->planTree->objType; 
            sProcPlan.planTree           = sProcInfo->planTree;
            sProcPlan.pkgBodyOID         = QS_EMPTY_OID;
            sProcPlan.pkgBodyModifyCount = 0;
            sProcPlan.next = NULL;

            // environment�� ���
            IDE_TEST( qcgPlan::registerPlanProc(
                          aStatement,
                          & sProcPlan )
                      != IDE_SUCCESS );

            // no subquery is allowed on
            // 1. execute proc
            // 2. execute ? := func
            // 3. procedure invocation in a procedure or a function.
            // and stmtKind of these cases are set to QCI_STMT_MASK_SP mask.
            if ( (aStatement->spvEnv->flag & QSV_ENV_SUBQUERY_ON_ARGU_ALLOW_MASK)
                 == QSV_ENV_SUBQUERY_ON_ARGU_ALLOW_TRUE )
            {
                sIsAllowSubq = ID_TRUE;
            }
            else
            {
                sIsAllowSubq = ID_FALSE;
            }

            // validation parameter
            IDE_TEST( validateArgumentsAfterParser(
                          aStatement,
                          sExeParseTree->callSpecNode,
                          sProcInfo->planTree->paraDecls,
                          sIsAllowSubq )
                      != IDE_SUCCESS);

            // set parameter module array pointer.
            sExeParseTree->paraDeclCount = sProcInfo->planTree->paraDeclCount;

            // fix BUG-42521
            sProcParaDecls = sProcInfo->planTree->paraDecls;

            IDE_TEST( qsvProcVar::copyParamModules(
                          aStatement,
                          sExeParseTree->paraDeclCount,
                          sProcInfo->planTree->paramColumns,
                          (mtdModule***) & (sExeParseTree->paramModules),
                          &(sExeParseTree->paramColumns) )
                      != IDE_SUCCESS );

            if ( sProcInfo->planTree->returnTypeVar != NULL )
            {
                sProcReturnTypeNode = sProcInfo->planTree->returnTypeVar->variableTypeNode;

                if (sExeParseTree->leftNode == NULL)
                {
                    sqlInfo.setSourceInfo(
                        aStatement,
                        & sExeParseTree->callSpecNode->columnName);
                    IDE_RAISE( ERR_FUNCTION_NOT_HAVE_RETURN );
                }

                sExeReturnTypeColumn =
                    QTC_STMT_COLUMN( aStatement, sExeParseTree->callSpecNode );

                sProcReturnTypeColumn =
                    QTC_TMPL_COLUMN( sProcInfo->tmplate,  sProcReturnTypeNode );

                //fix BUG-17410
                IDE_TEST( qsvProcType::copyColumn( aStatement,
                                                   sProcReturnTypeColumn,
                                                   sExeReturnTypeColumn,
                                                   (mtdModule **)&sExeReturnTypeColumn->module )
                          != IDE_SUCCESS );
            }
            else
            {
                if (sExeParseTree->leftNode != NULL)
                {
                    sqlInfo.setSourceInfo(
                        aStatement,
                        & sExeParseTree->callSpecNode->columnName);
                    IDE_RAISE( ERR_PROCEDURE_HAVE_RETURN );
                }
            }

            /* PROJ-2452 Caching for DETERMINISTIC Function */
            sExeParseTree->isCachable = sProcInfo->planTree->isCachable;
        }
    }

    // fix BUG-42521
    if ( aStatement->myPlan->sBindParam != NULL )
    {
        for ( i = 0, sParaDecls = sProcParaDecls, sArgument = (qtcNode *)sExeParseTree->callSpecNode->node.arguments;
              ( i < sExeParseTree->paraDeclCount ) && ( sArgument != NULL ) && ( sParaDecls != NULL );
              i++, sParaDecls = sParaDecls->next, sArgument = (qtcNode *)sArgument->node.next )
        {
            if ( ( (sArgument->node.lflag & MTC_NODE_BIND_MASK) == MTC_NODE_BIND_EXIST ) &&
                 ( sArgument->node.module == &(qtc::valueModule) ) )
            {
                switch ( ((qsVariables *)sParaDecls)->inOutType )
                {
                    case QS_IN:
                        aStatement->myPlan->sBindParam[sArgument->node.column].param.inoutType = CMP_DB_PARAM_INPUT; 
                        break;
                    case QS_OUT:
                        aStatement->myPlan->sBindParam[sArgument->node.column].param.inoutType = CMP_DB_PARAM_OUTPUT; 
                        break;
                    case QS_INOUT:
                        aStatement->myPlan->sBindParam[sArgument->node.column].param.inoutType = CMP_DB_PARAM_INPUT_OUTPUT; 
                        break;
                    default:
                        break;
                }
            }
            else
            {
                // Nothing to do.
            }
        }
    }
    else
    {
        // Nothing to do.
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NOT_DEFINE_SUBPROGRAM );
    {
        (void)sqlInfo.init( aStatement->qmeMem );
        IDE_SET(
            ideSetErrorCode( qpERR_ABORT_QSV_UNDEFINED_SUBPROGRAM,
                             sqlInfo.getErrMessage() ) );
        (void)sqlInfo.fini();
    }

    IDE_EXCEPTION(ERR_FUNCTION_NOT_HAVE_RETURN);
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QSV_PROC_NOT_HAVE_RETURN_VALUE_SQLTEXT,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION(ERR_PROCEDURE_HAVE_RETURN);
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QSV_PROC_HAVE_RETURN_VALUE_SQLTEXT,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qsv::validateExeFunc(qcStatement * aStatement)
{
    qsExecParseTree     * sParseTree;
    qsxProcInfo         * sProcInfo;
    qsProcParseTree     * sCrtParseTree    = NULL;
    qcuSqlSourceInfo      sqlInfo;
    qsPkgParseTree      * sCrtPkgParseTree = NULL;
    qsxPkgInfo          * sPkgInfo         = NULL;
    qsVariables         * sReturnTypeVar   = NULL;

    IDU_FIT_POINT_FATAL( "qsv::validateExeFunc::__FT__" );

    sParseTree = (qsExecParseTree *)(aStatement->myPlan->parseTree);

    IDE_TEST( validateExeProc( aStatement ) != IDE_SUCCESS );

    if (sParseTree->procOID != QS_EMPTY_OID)
    {
        if( sParseTree->subprogramID == QS_PSM_SUBPROGRAM_ID )
        {
            IDE_TEST( qsxProc::getProcInfo( sParseTree->procOID,
                                            & sProcInfo )
                      != IDE_SUCCESS );

            sCrtParseTree = sProcInfo->planTree;
        }
        else
        {
            /* BUG-39094
               package body�� ��� ��ü�� �����Ǿ�� �Ѵ�. */
            IDE_TEST( qsxPkg::getPkgInfo( sParseTree->procOID,
                                          &sPkgInfo )
                      != IDE_SUCCESS );

            IDE_TEST( qsvPkgStmts::getPkgSubprogramPlanTree(
                          aStatement,
                          sPkgInfo->planTree,
                          sPkgInfo->objType,
                          sParseTree->subprogramID,
                          &sCrtParseTree )
                      != IDE_SUCCESS );

            IDE_DASSERT( sCrtParseTree != NULL );
        }

        sReturnTypeVar = sCrtParseTree->returnTypeVar;

        // if called fuction is procedure, then error.
        if ( sReturnTypeVar == NULL )
        {
            sqlInfo.setSourceInfo( aStatement,
                                   & sParseTree->callSpecNode->columnName);
            IDE_RAISE( ERR_FUNCTION_NOT_HAVE_RETURN );
        }

        // copy information
        IDE_TEST( STRUCT_ALLOC( QC_QMP_MEM( aStatement ),
                                mtcColumn,
                                &( sParseTree->returnTypeColumn ) )
                  != IDE_SUCCESS );

        IDE_TEST( qsvProcType::copyColumn( aStatement,
                                           sCrtParseTree->returnTypeColumn,
                                           sParseTree->returnTypeColumn,
                                           (mtdModule **)&sParseTree->returnModule )
                  != IDE_SUCCESS );

        /* function�� return���� column���� ���� ��, return char(1)���� return char�� �����Ǿ����� ���� */
        if ( ( sReturnTypeVar->variableTypeNode->lflag & QTC_NODE_SP_PARAM_OR_RETURN_PRECISION_MASK)
             == QTC_NODE_SP_PARAM_OR_RETURN_PRECISION_ABSENT )
        {
            sParseTree->callSpecNode->lflag &= ~QTC_NODE_SP_PARAM_OR_RETURN_PRECISION_MASK;
            sParseTree->callSpecNode->lflag |= QTC_NODE_SP_PARAM_OR_RETURN_PRECISION_ABSENT;
        }
        else
        {
            // Nothing to do.
        }

        // BUG-32392
        // sParseTree->returnTypeColumn�� ������ ����Ǿ����Ƿ� qtc::fixAfterValidation����
        // �ݵ�� �ش� column�� ������ �������־�� �Ѵ�.
        QC_SHARED_TMPLATE(aStatement)->tmplate.rows[sParseTree->callSpecNode->node.table].lflag
            &= ~MTC_TUPLE_ROW_SKIP_MASK;
        QC_SHARED_TMPLATE(aStatement)->tmplate.rows[sParseTree->callSpecNode->node.table].lflag
            |= MTC_TUPLE_ROW_SKIP_FALSE;

        /* PROJ-2452 Caching for DETERMINISTIC Function */
        sParseTree->isCachable = sCrtParseTree->isCachable;
    }
    else    // recursive call
    {
        if( sParseTree->subprogramID != QS_PSM_SUBPROGRAM_ID )
        {
            // subprogram�� �ڽ��� ���ȣ������ ��
            if ( (aStatement->spvEnv->isPkgInitializeSection == ID_FALSE) &&
                 (qsvPkgStmts::checkIsRecursiveSubprogramCall(
                     aStatement->spvEnv->currSubprogram,
                     sParseTree->subprogramID )
                  == ID_TRUE) )
            {
                if ( aStatement->spvEnv->createProc->returnTypeVar == NULL )
                {
                    sqlInfo.setSourceInfo( aStatement,
                                           & sParseTree->callSpecNode->columnName);
                    IDE_RAISE( ERR_FUNCTION_NOT_HAVE_RETURN );
                }

                /* PROJ-2452 Caching for DETERMINISTIC Function */
                sParseTree->isCachable = aStatement->spvEnv->createProc->isCachable;
            }
            else
            {
                sCrtPkgParseTree = aStatement->spvEnv->createPkg;

                IDE_TEST( qsvPkgStmts::getPkgSubprogramPlanTree(
                              aStatement,
                              sCrtPkgParseTree,
                              sCrtPkgParseTree->objType,
                              sParseTree->subprogramID,
                              &sCrtParseTree )
                          != IDE_SUCCESS );

                sReturnTypeVar = sCrtParseTree->returnTypeVar;

                if ( sReturnTypeVar == NULL )
                {
                    sqlInfo.setSourceInfo( aStatement,
                                           & sParseTree->callSpecNode->columnName);
                    IDE_RAISE( ERR_FUNCTION_NOT_HAVE_RETURN );
                }

                // copy information
                IDE_TEST( STRUCT_ALLOC( QC_QMP_MEM( aStatement ),
                                        mtcColumn,
                                        &( sParseTree->returnTypeColumn ) )
                          != IDE_SUCCESS );

                IDE_TEST_RAISE( sCrtParseTree->returnTypeColumn == NULL , err_return_value_invalid );

                IDE_TEST( qsvProcType::copyColumn( aStatement,
                                                   sCrtParseTree->returnTypeColumn,
                                                   sParseTree->returnTypeColumn,
                                                   (mtdModule **)&sParseTree->returnModule )
                          != IDE_SUCCESS );

                /* function�� return���� column���� ���� ��, return char(1)���� return char�� �����Ǿ����� ���� */
                if ( (sReturnTypeVar->variableTypeNode->lflag & QTC_NODE_SP_PARAM_OR_RETURN_PRECISION_MASK)
                     == QTC_NODE_SP_PARAM_OR_RETURN_PRECISION_ABSENT )
                {
                    sParseTree->callSpecNode->lflag &= ~QTC_NODE_SP_PARAM_OR_RETURN_PRECISION_MASK;
                    sParseTree->callSpecNode->lflag |= QTC_NODE_SP_PARAM_OR_RETURN_PRECISION_ABSENT;
                }
                else
                {
                    // Nothing to do.
                }

                // BUG-32392
                // sParseTree->returnTypeColumn�� ������ ����Ǿ����Ƿ� qtc::fixAfterValidation����
                // �ݵ�� �ش� column�� ������ �������־�� �Ѵ�.
                QC_SHARED_TMPLATE(aStatement)->tmplate.rows[sParseTree->callSpecNode->node.table].lflag
                    &= ~MTC_TUPLE_ROW_SKIP_MASK;
                QC_SHARED_TMPLATE(aStatement)->tmplate.rows[sParseTree->callSpecNode->node.table].lflag
                    |= MTC_TUPLE_ROW_SKIP_FALSE;

                /* PROJ-2452 Caching for DETERMINISTIC Function */
                sParseTree->isCachable = sCrtParseTree->isCachable;
            }
        }
        else
        {
            if ( aStatement->spvEnv->createProc->returnTypeVar == NULL )
            {
                sqlInfo.setSourceInfo( aStatement,
                                       & sParseTree->callSpecNode->columnName);
                IDE_RAISE( ERR_FUNCTION_NOT_HAVE_RETURN );
            }

            /* PROJ-2452 Caching for DETERMINISTIC Function */
            sParseTree->isCachable = aStatement->spvEnv->createProc->isCachable;
        }
    }

    // fix BUG-42521
    if ( aStatement->myPlan->sBindParam != NULL )
    {
        if ( ( (sParseTree->leftNode->node.lflag & MTC_NODE_BIND_MASK) == MTC_NODE_BIND_EXIST ) &&
             ( sParseTree->leftNode->node.module == &(qtc::valueModule) ) )
        {
            aStatement->myPlan->sBindParam[sParseTree->leftNode->node.column].param.inoutType = CMP_DB_PARAM_OUTPUT; 
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

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_FUNCTION_NOT_HAVE_RETURN);
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QSV_PROC_NOT_HAVE_RETURN_VALUE_SQLTEXT,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION(err_return_value_invalid)
    {
        IDE_SET(ideSetErrorCode( qpERR_ABORT_QSV_SUBPROGRAM_INVALID ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC qsv::checkHostVariable( qcStatement * aStatement )
{

    UShort  sBindCount = 0;
    UShort  sBindTuple = 0;

    IDU_FIT_POINT_FATAL( "qsv::checkHostVariable::__FT__" );

    sBindTuple = QC_SHARED_TMPLATE(aStatement)->tmplate.variableRow;

    // check host variable
    if( sBindTuple == ID_USHORT_MAX )
    {
        // Nothing to do.
    }
    else
    {
        sBindCount = QC_SHARED_TMPLATE(aStatement)->tmplate.rows[sBindTuple].columnCount;
    }

    if( sBindCount > 0 )
    {
        IDE_RAISE( ERR_NOT_ALLOWED_HOST_VAR );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_NOT_ALLOWED_HOST_VAR);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCV_NOT_ALLOWED_HOSTVAR));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qsv::checkUserAndProcedure(
    qcStatement     * aStatement,
    qcNamePosition    aUserName,
    qcNamePosition    aProcName,
    UInt            * aUserID,
    qsOID           * aProcOID)
{
#define IDE_FN "qsv::checkUserAndProcedure"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qsv::checkUserAndProcedure"));

    // check user name
    if (QC_IS_NULL_NAME(aUserName) == ID_TRUE)
    {
        *aUserID = QCG_GET_SESSION_USER_ID(aStatement);
    }
    else
    {
        IDE_TEST(qcmUser::getUserID(aStatement, aUserName, aUserID)
                 != IDE_SUCCESS);
    }

    // check procedure name
    IDE_TEST(qcmProc::getProcExistWithEmptyByName(
                 aStatement,
                 *aUserID,
                 aProcName,
                 aProcOID)
             != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

// PROJ-1073 Package
IDE_RC qsv::checkUserAndPkg( 
    qcStatement    * aStatement,
    qcNamePosition   aUserName,
    qcNamePosition   aPkgName,
    UInt           * aUserID,
    qsOID          * aSpecOID,
    qsOID          * aBodyOID)
{
    // check user name
    if (QC_IS_NULL_NAME(aUserName) == ID_TRUE)
    {
        *aUserID = QCG_GET_SESSION_USER_ID(aStatement);
    }
    else
    {
        IDE_TEST(qcmUser::getUserID(aStatement, aUserName, aUserID)
                 != IDE_SUCCESS);
    }

    // check package name
    IDE_TEST( qcmPkg::getPkgExistWithEmptyByName(
                  aStatement,
                  *aUserID,
                  aPkgName,
                  QS_PKG,
                  aSpecOID )
              != IDE_SUCCESS );

    // check package body
    IDE_TEST( qcmPkg::getPkgExistWithEmptyByName(
                  aStatement,
                  *aUserID,
                  aPkgName,
                  QS_PKG_BODY,
                  aBodyOID)
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qsv::checkRecursiveCall( qcStatement * aStatement,
                                idBool      * aRecursiveCall )
{
    qsExecParseTree     * sExeParseTree;
    qsProcParseTree     * sCrtParseTree;

    IDU_FIT_POINT_FATAL( "qsv::checkRecursiveCall::__FT__" );

    sExeParseTree = (qsExecParseTree *)aStatement->myPlan->parseTree;

    *aRecursiveCall = ID_FALSE;

    if ( aStatement->spvEnv->createProc != NULL )
    {
        sCrtParseTree = aStatement->spvEnv->createProc;

        // check user name
        if ( QC_IS_NULL_NAME(sExeParseTree->callSpecNode->tableName) == ID_TRUE )
        {
            sExeParseTree->userID = QCG_GET_SESSION_USER_ID(aStatement);
        }
        else
        {
            if ( qcmUser::getUserID( aStatement,
                                     sExeParseTree->callSpecNode->tableName,
                                     & (sExeParseTree->userID) )
                 != IDE_SUCCESS )
            {
                IDE_CLEAR();
                IDE_CONT( NOT_RECURSIVECALL );
            }
            else
            {
                // Nothing to do.
            }
        }

        // check procedure name
        if ( sExeParseTree->userID == sCrtParseTree->userID )
        {
            if ( QC_IS_NAME_MATCHED( sExeParseTree->callSpecNode->columnName, sCrtParseTree->procNamePos ) )
            {
                *aRecursiveCall         = ID_TRUE;
                sExeParseTree->procOID  = QS_EMPTY_OID ;
            }
        }
    }

    IDE_EXCEPTION_CONT( NOT_RECURSIVECALL );

    return IDE_SUCCESS;
}

IDE_RC qsv::validateArgumentsWithParser(
    qcStatement     * aStatement,
    SChar           * aStmtText,
    qtcNode         * aCallSpec,
    qsVariableItems * aParaDecls,
    UInt              aParaDeclCount,
    iduList         * aRefCurList,
    idBool            aIsRootStmt )
{
    qtcNode             * sPrevArgu;
    qtcNode             * sCurrArgu;
    qtcNode             * sNewArgu;
    qtcNode             * sNewVar[2];
    mtcColumn           * sNewColumn;
    qsVariableItems     * sParaDecl;
    qsVariables         * sParaVar;
    UChar               * sDefaultValueStr;
    qdDefaultParseTree  * sDefaultParseTree;
    qcStatement         * sDefaultStatement;
    qcuSqlSourceInfo      sqlInfo;
    iduListNode         * sNode;
    // BUG-37235
    iduVarMemString       sSqlBuffer;
    SInt                  sSize = 0;
    SInt                  i     = 0;
    // BUG-41243
    qsvArgPassInfo      * sArgPassArray = NULL;
    idBool                sAlreadyAdded = ID_FALSE;

    IDU_FIT_POINT_FATAL( "qsv::validateArgumentsWithParser::__FT__" );

    // BUG-41243 Name-based Argument Passing
    //           + Too many Argument Validation
    IDE_TEST( validateNamedArguments( aStatement,
                                      aCallSpec,
                                      aParaDecls,
                                      aParaDeclCount,
                                      & sArgPassArray )
              != IDE_SUCCESS );

    // set DEFAULT of parameter
    sPrevArgu = NULL;
    sCurrArgu = (qtcNode *)(aCallSpec->node.arguments);

    for (sParaDecl = aParaDecls, i = 0;
         sParaDecl != NULL;
         sParaDecl = sParaDecl->next, i++)
    {
        sParaVar = (qsVariables*)sParaDecl;
        sAlreadyAdded = ID_FALSE;

        if( aIsRootStmt == ID_TRUE )
        {
            // ref cursor type�̰� out/inout�� �����
            // �� ��� resultset�� ��.
            if( (sParaVar->variableType == QS_REF_CURSOR_TYPE) &&
                ( (sParaVar->inOutType == QS_OUT) ||
                  (sParaVar->inOutType == QS_INOUT) ) )
            {
                // ref cursor type�̹Ƿ� �ݵ�� typeInfo�� �־����.
                IDE_DASSERT( sParaVar->typeInfo != NULL );

                IDE_TEST( qtc::createColumn(
                              aStatement,
                              (mtdModule*)sParaVar->typeInfo->typeModule,
                              &sNewColumn,
                              0, NULL, NULL, 1 )
                          != IDE_SUCCESS );

                // ������ ref cursor proc var�� �����.
                IDE_TEST( qtc::makeInternalProcVariable( aStatement,
                                                         sNewVar,
                                                         NULL,
                                                         sNewColumn,
                                                         QTC_PROC_VAR_OP_NEXT_COLUMN )
                          != IDE_SUCCESS );

                sNewArgu = sNewVar[0];

                sNewArgu->lflag &= ~QTC_NODE_OUTBINDING_MASK;
                sNewArgu->lflag |= QTC_NODE_OUTBINDING_ENABLE;

                IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(iduListNode),
                                                         (void**)&sNode )
                          != IDE_SUCCESS );

                IDU_LIST_INIT_OBJ( sNode, (void*)sNewArgu );

                IDU_LIST_ADD_LAST( aRefCurList, sNode );

                // connect argument
                if (sPrevArgu == NULL)
                {
                    aCallSpec->node.arguments = (mtcNode *)sNewArgu;
                }
                else
                {
                    sPrevArgu->node.next = (mtcNode *)sNewArgu;
                }

                sNewArgu->node.next = (mtcNode*)sCurrArgu;

                sCurrArgu = sNewArgu;

                // BUG-41243
                if ( sArgPassArray[i].mType != QSV_ARG_NOT_PASSED )
                {
                    // PassArray�� Type�� ���� ���� refCursor�� ����� �� ����.
                    // ���� refCursor�� �����Ǿ�� PassArray�� ������ ��찡 �����Ƿ�
                    // �� ���, ���� Type�� ������ �ٽ� �е��� �ؾ� �Ѵ�.
                    i--;
                }
                else
                {
                    // PassArray�� �������� �ʾƵ� �ȴ�.
                    // Nothing to do.
                }
                
                // Argument�� ���������Ƿ�, Default Argument �κ��� ����ģ��.
                sAlreadyAdded = ID_TRUE;
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

        // BUG-41243 ������ Default Parameter�� ���ؼ��� ó���� �� ����.
        // ArgPassArray�� ���� Default Parameter�� ó���Ѵ�.
        if ( ( sArgPassArray[i].mType == QSV_ARG_NOT_PASSED ) && 
             ( sAlreadyAdded == ID_FALSE ) )
        {
            if (sParaVar->defaultValueNode == NULL)
            {
                sqlInfo.setSourceInfo( aStatement,
                                       & aCallSpec->position );
                IDE_RAISE( ERR_INSUFFICIENT_ARGUMENT );
            }

            // make argument node
            IDE_TEST( STRUCT_ALLOC( QC_QMP_MEM( aStatement ), qcStatement, &sDefaultStatement )
                      != IDE_SUCCESS );

            QC_SET_STATEMENT(sDefaultStatement, aStatement, NULL);

            /****************************************************************
             * BUG-37235
             * �Ʒ� ������ ���� package ���� ������ default value�� ���� ��,
             * ���� �� ������ ã�� ���ؼ� default statement ���� ��
             * package �̸��� �߰��Ѵ�.
             *
             * ex)
             * create or replace package pkg1 as
             * v1 integer;
             * procedure proc1(p1 integer default v1);
             * end;
             * /
             * create or replackage package body pkg1 as
             * procedure proc1(p1 integer default v1) as 
             * begin
             * println('test');
             * end;
             * end;
             * /
             *
             * exec pkg1.proc1;
             ****************************************************************/

            // default statement�� ������ �ӽ� buffer ����
            IDE_TEST( iduVarMemStringInitialize( &sSqlBuffer, QC_QME_MEM(aStatement), QSV_SQL_BUFFER_SIZE )
                      != IDE_SUCCESS );

            IDE_TEST( iduVarMemStringAppendLength( &sSqlBuffer, "DEFAULT ", 8 )
                      != IDE_SUCCESS );

            /* BUG-42037 */
            if ( ((sParaVar->defaultValueNode->lflag & QTC_NODE_PKG_VARIABLE_MASK)
                  == QTC_NODE_PKG_VARIABLE_TRUE) &&
                 (QC_IS_NULL_NAME( sParaVar->defaultValueNode->userName ) == ID_TRUE) &&
                 (QC_IS_NAME_MATCHED( sParaVar->defaultValueNode->tableName,
                                      aCallSpec->tableName )
                  == ID_FALSE) )
            {
                IDE_TEST( iduVarMemStringAppendLength( &sSqlBuffer,
                                                       aStatement->myPlan->stmtText +
                                                       aCallSpec->tableName.offset,
                                                       aCallSpec->tableName.size )
                          != IDE_SUCCESS );

                IDE_TEST( iduVarMemStringAppendLength( &sSqlBuffer,
                                                       ".", 1 )
                          != IDE_SUCCESS );

                IDE_TEST( iduVarMemStringAppendLength( &sSqlBuffer,
                                                       aStmtText +
                                                       sParaVar->defaultValueNode->position.offset,
                                                       sParaVar->defaultValueNode->position.size )
                          != IDE_SUCCESS );
            }
            else
            {
                IDE_TEST( iduVarMemStringAppendLength( &sSqlBuffer,
                                                       aStmtText +
                                                       sParaVar->defaultValueNode->position.offset,
                                                       sParaVar->defaultValueNode->position.size )
                          != IDE_SUCCESS );
            }

            sSize = iduVarMemStringGetLength( &sSqlBuffer );

            IDE_TEST( QC_QMP_MEM( aStatement )->alloc( sSize + 1,
                                                       (void**)&sDefaultValueStr)
                      != IDE_SUCCESS);

            IDE_TEST( iduVarMemStringExtractString( &sSqlBuffer, (SChar *)sDefaultValueStr, sSize )
                      != IDE_SUCCESS );

            IDE_TEST( iduVarMemStringFinalize( &sSqlBuffer )
                      != IDE_SUCCESS );

            sDefaultValueStr[sSize] = '\0';

            sDefaultStatement->myPlan->stmtText = (SChar*)sDefaultValueStr;
            sDefaultStatement->myPlan->stmtTextLen =
                idlOS::strlen((SChar*)sDefaultValueStr);

            IDE_TEST( qcpManager::parseIt( sDefaultStatement ) != IDE_SUCCESS );

            sDefaultParseTree =
                (qdDefaultParseTree*) sDefaultStatement->myPlan->parseTree;

            // set default expression
            sCurrArgu = sDefaultParseTree->defaultValue;

            // connect argument
            if (sPrevArgu == NULL)
            {
                sCurrArgu->node.next      = (mtcNode *)aCallSpec->node.arguments;
                aCallSpec->node.arguments = (mtcNode *)sCurrArgu;
            }
            else
            {
                sCurrArgu->node.next = sPrevArgu->node.next;
                sPrevArgu->node.next = (mtcNode *)sCurrArgu;
            }

            // mtcNode�� MTC_NODE_ARGUMENT_COUNT ���� ������Ų��.
            aCallSpec->node.lflag++;
        }

        // next argument
        sPrevArgu = sCurrArgu;
        sCurrArgu = (qtcNode *)(sCurrArgu->node.next);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_INSUFFICIENT_ARGUMENT);
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode( qpERR_ABORT_QSV_INSUFFICIENT_ARGUEMNT_SQLTEXT,
                             sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qsv::validateArgumentsAfterParser(
    qcStatement     * aStatement,
    qtcNode         * aCallSpec,
    qsVariableItems * aParaDecls,
    idBool            aAllowSubquery )
{
    qtcNode             * sCurrArgu;
    qsVariableItems     * sParaDecl;
    qsVariables         * sParaVar;
    idBool                sFind;
    qsVariables         * sArrayVar = NULL;
    qcuSqlSourceInfo      sqlInfo;
    UShort                sTable;
    UShort                sColumn;

    IDU_FIT_POINT_FATAL( "qsv::validateArgumentsAfterParser::__FT__" );

    for (sParaDecl = aParaDecls,
             sCurrArgu = (qtcNode *)(aCallSpec->node.arguments);
         sParaDecl != NULL;
         sParaDecl = sParaDecl->next,
             sCurrArgu = (qtcNode *)(sCurrArgu->node.next))
    {
        sParaVar = (qsVariables*)sParaDecl;

        if (sParaVar->inOutType == QS_IN)
        {
            if ( ( aStatement->spvEnv->flag & QSV_ENV_ESTIMATE_ARGU_MASK )
                 == QSV_ENV_ESTIMATE_ARGU_FALSE )
            {
                IDE_TEST( qtc::estimate(
                              sCurrArgu,
                              QC_SHARED_TMPLATE( aStatement ),
                              aStatement,
                              NULL,
                              NULL,
                              NULL )
                          != IDE_SUCCESS );
            }

            if ( aAllowSubquery != ID_TRUE )
            {
                IDE_TEST_RAISE( qsv::checkNoSubquery(
                                    aStatement,
                                    sCurrArgu,
                                    & sqlInfo ) != IDE_SUCCESS,
                                ERR_SUBQ_NOT_ALLOWED_ON_ARGUMENT );
            }
        }
        else // QS_OUT or QS_INOUT
        {
            if ( ( aStatement->spvEnv->flag & QSV_ENV_ESTIMATE_ARGU_MASK )
                 == QSV_ENV_ESTIMATE_ARGU_FALSE )
            {
                IDE_TEST( qtc::estimate(
                              sCurrArgu,
                              QC_SHARED_TMPLATE( aStatement ),
                              aStatement,
                              NULL,
                              NULL,
                              NULL )
                          != IDE_SUCCESS );
            }

            if (sCurrArgu->node.module == &(qtc::columnModule))
            {
                if ( aStatement->spvEnv->createProc != NULL)
                {
                    /* BUG-38644
                       estimate���� indirect calculate�� ����
                       ������ �ִ� table/column ���� ���� */
                    sTable  = sCurrArgu->node.table;
                    sColumn = sCurrArgu->node.column;

                    // search in variable or parameter list
                    IDE_TEST( qsvProcVar::searchVarAndPara(
                                  aStatement,
                                  sCurrArgu,
                                  ID_TRUE, // for OUTPUT
                                  & sFind,
                                  & sArrayVar )
                              != IDE_SUCCESS );

                    if ( sFind == ID_FALSE )
                    {
                        IDE_TEST( qsvProcVar::searchVariableFromPkg(
                                      aStatement,
                                      sCurrArgu,
                                      & sFind,
                                      & sArrayVar )
                                  != IDE_SUCCESS );
                    }
                    else
                    {
                        // Nothing to do.
                    }

                    sCurrArgu->node.table  = sTable;
                    sCurrArgu->node.column = sColumn;
                }
                else
                {
                    // PROJ-1386 Dynamic-SQL
                    // ref cursor�� ���� internal procedure variable��
                    // �������.
                    if( ( sCurrArgu->lflag & QTC_NODE_INTERNAL_PROC_VAR_MASK )
                        == QTC_NODE_INTERNAL_PROC_VAR_EXIST )
                    {
                        sFind = ID_TRUE;
                    }
                    else
                    {
                        /* - BUG-38644
                           estimate���� indirect calculate�� ���� ������ �ִ� table/column ���� ����
                           - BUG-42311 argument�� package ������ �� ã�� �� �־�� �Ѵ�. */
                        sTable  = sCurrArgu->node.table;
                        sColumn = sCurrArgu->node.column;

                        /* BUG-44941
                           package�� initialize section���� fclose�Լ��� argument�� ������ package �����̸� ã�� �� �մϴ�. */
                        // search in variable or parameter list
                        IDE_TEST(qsvProcVar::searchVarAndPara(
                                     aStatement,
                                     sCurrArgu,
                                     ID_TRUE, // for OUTPUT
                                     &sFind,
                                     &sArrayVar)
                                 != IDE_SUCCESS);

                        if ( sFind != ID_TRUE )
                        {
                            /* search a package variable */
                            IDE_TEST( qsvProcVar::searchVariableFromPkg(
                                          aStatement,
                                          sCurrArgu,
                                          &sFind,
                                          &sArrayVar )
                                      != IDE_SUCCESS );
                        }
                        else
                        {
                            // Nohting to do.
                        }

                        sCurrArgu->node.table  = sTable;
                        sCurrArgu->node.column = sColumn;
                    }
                }

                if (sFind == ID_TRUE)
                {
                    if ( ( sCurrArgu->lflag & QTC_NODE_OUTBINDING_MASK )
                         == QTC_NODE_OUTBINDING_DISABLE )
                    {
                        sqlInfo.setSourceInfo( aStatement,
                                               & sCurrArgu->position );
                        IDE_RAISE( ERR_INVALID_OUT_ARGUMENT );
                    }
                    else
                    {
                        // Nothing to do.
                    }

                    // out parameter�� ��� lvalue_enable�� ����.
                    if( sParaVar->inOutType == QS_OUT )
                    {
                        sCurrArgu->lflag |= QTC_NODE_LVALUE_ENABLE;
                    }
                    else
                    {
                        // Nothing to do.
                    }
                }
                else
                {
                    sqlInfo.setSourceInfo( aStatement,
                                           & sCurrArgu->position );
                    IDE_RAISE( ERR_NOT_EXIST_VARIABLE );
                }
            }
            else if (sCurrArgu->node.module == &(qtc::valueModule))
            {
                // constant value or host variable
                if ( ( sCurrArgu->node.lflag & MTC_NODE_BIND_MASK )
                     == MTC_NODE_BIND_ABSENT ) // constant
                {
                    sqlInfo.setSourceInfo( aStatement,
                                           & sCurrArgu->position );
                    IDE_RAISE( ERR_INVALID_OUT_ARGUMENT );
                }
            }
            else
            {
                sqlInfo.setSourceInfo( aStatement,
                                       & sCurrArgu->position );
                IDE_RAISE( ERR_INVALID_OUT_ARGUMENT );
            }
        }
    }

    if (sParaDecl != NULL)
    {
        sqlInfo.setSourceInfo( aStatement,
                               & aCallSpec->position );
        IDE_RAISE( ERR_INSUFFICIENT_ARGUMENT );
    }

    if (sCurrArgu != NULL)
    {
        sqlInfo.setSourceInfo( aStatement,
                               & aCallSpec->position );
        IDE_RAISE( ERR_TOO_MANY_ARGUMENT );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_SUBQ_NOT_ALLOWED_ON_ARGUMENT);
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode( qpERR_ABORT_QSV_NOT_ALLOWED_SUBQUERY_SQLTEXT,
                             sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION(ERR_INSUFFICIENT_ARGUMENT);
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode( qpERR_ABORT_QSV_INSUFFICIENT_ARGUEMNT_SQLTEXT,
                             sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION(ERR_TOO_MANY_ARGUMENT);
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode( qpERR_ABORT_QSV_TOO_MANY_ARGUEMNT_SQLTEXT,
                             sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION(ERR_INVALID_OUT_ARGUMENT);
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QSV_INVALID_OUT_ARGUEMNT_SQLTEXT,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION(ERR_NOT_EXIST_VARIABLE)
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QSV_NOT_EXIST_VARIABLE_NAME_SQLTEXT,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qsv::validateArgumentsWithoutParser(
    qcStatement     * aStatement,
    qtcNode         * aCallSpec,
    qsVariableItems * aParaDecls,
    UInt              aParaDeclCount,
    idBool            aAllowSubquery )
{
    qtcNode             * sPrevArgu;
    qtcNode             * sCurrArgu;
    qsVariableItems     * sParaDecl;
    qsVariables         * sParaVar;
    idBool                sFind;
    qsVariables         * sArrayVar = NULL;
    qcuSqlSourceInfo      sqlInfo;
    UShort                sTable;
    UShort                sColumn;
    UInt                  i = 0;
    // BUG-41243
    qsvArgPassInfo      * sArgPassArray = NULL;

    // BUG-41243 Name-based Argument Passing
    //           + Too many Argument Validation
    IDE_TEST( validateNamedArguments( aStatement,
                                      aCallSpec,
                                      aParaDecls,
                                      aParaDeclCount,
                                      & sArgPassArray )
              != IDE_SUCCESS );

    // set DEFAULT of parameter
    sPrevArgu = NULL;
    sCurrArgu = (qtcNode *)(aCallSpec->node.arguments);

    for (sParaDecl = aParaDecls, i = 0;
         sParaDecl != NULL;
         sParaDecl = sParaDecl->next, i++)
    {
        sParaVar = (qsVariables*)sParaDecl;

        // BUG-41243 ������ Default Parameter�� ���ؼ��� ó���� �� ����.
        // ArgPassArray�� ���� Default Parameter�� ó���Ѵ�.
        if ( sArgPassArray[i].mType == QSV_ARG_NOT_PASSED )
        {
            if (sParaVar->defaultValueNode == NULL)
            {
                sqlInfo.setSourceInfo( aStatement,
                                       & aCallSpec->position );
                IDE_RAISE( ERR_INSUFFICIENT_ARGUMENT );
            }

            // make argument node
            IDE_TEST(STRUCT_ALLOC(QC_QMP_MEM(aStatement), qtcNode, &sCurrArgu)
                     != IDE_SUCCESS);

            // set default expression
            idlOS::memcpy(sCurrArgu,
                          sParaVar->defaultValueNode,
                          ID_SIZEOF(qtcNode));
            sCurrArgu->node.next = NULL;

            // connect argument
            if (sPrevArgu == NULL)
            {
                sCurrArgu->node.next      = (mtcNode *)aCallSpec->node.arguments;
                aCallSpec->node.arguments = (mtcNode *)sCurrArgu;
            }
            else
            {
                sCurrArgu->node.next = sPrevArgu->node.next;
                sPrevArgu->node.next = (mtcNode *)sCurrArgu;
            }

            // mtcNode�� MTC_NODE_ARGUMENT_COUNT ���� ������Ų��.
            aCallSpec->node.lflag++;
        }

        // next argument
        sPrevArgu = sCurrArgu;
        sCurrArgu = (qtcNode *)(sCurrArgu->node.next);
    }

    for (sParaDecl = aParaDecls,
             sCurrArgu = (qtcNode *)(aCallSpec->node.arguments);
         sParaDecl != NULL;
         sParaDecl = sParaDecl->next,
             sCurrArgu = (qtcNode *)(sCurrArgu->node.next))
    {
        sParaVar = (qsVariables*)sParaDecl;

        if (sParaVar->inOutType == QS_IN)
        {
            if ( ( aStatement->spvEnv->flag & QSV_ENV_ESTIMATE_ARGU_MASK )
                 == QSV_ENV_ESTIMATE_ARGU_FALSE )
            {
                IDE_TEST(qtc::estimate(
                             sCurrArgu,
                             QC_SHARED_TMPLATE(aStatement),
                             aStatement,
                             NULL,
                             NULL,
                             NULL )
                         != IDE_SUCCESS);
            }

            if ( aAllowSubquery != ID_TRUE )
            {
                IDE_TEST_RAISE( qsv::checkNoSubquery(
                                    aStatement,
                                    sCurrArgu,
                                    & sqlInfo ) != IDE_SUCCESS,
                                ERR_SUBQ_NOT_ALLOWED_ON_ARGUMENT );
            }
        }
        else // QS_OUT or QS_INOUT
        {
            if( ((qsExecParseTree *)(aStatement->myPlan->parseTree))->subprogramID
                != QS_PSM_SUBPROGRAM_ID )
            {
                if ( ( aStatement->spvEnv->flag & QSV_ENV_ESTIMATE_ARGU_MASK )
                     == QSV_ENV_ESTIMATE_ARGU_FALSE )
                {
                    IDE_TEST(qtc::estimate(
                                 sCurrArgu,
                                 QC_SHARED_TMPLATE(aStatement),
                                 aStatement,
                                 NULL,
                                 NULL,
                                 NULL )
                             != IDE_SUCCESS);
                }
            }
            else
            {
                // Nothing to do.
            }

            if (sCurrArgu->node.module == &(qtc::columnModule))
            {
                if ( aStatement->spvEnv->createProc != NULL)
                {
                    /* BUG-38644
                       estimate���� indirect calculate�� ����
                       ������ �ִ� table/column ���� ���� */
                    sTable  = sCurrArgu->node.table;
                    sColumn = sCurrArgu->node.column;

                    // search in variable or parameter list
                    IDE_TEST(qsvProcVar::searchVarAndPara(
                                 aStatement,
                                 sCurrArgu,
                                 ID_TRUE, // for OUTPUT
                                 &sFind,
                                 &sArrayVar)
                             != IDE_SUCCESS);

                    if ( sFind == ID_FALSE )
                    {
                        IDE_TEST( qsvProcVar::searchVariableFromPkg(
                                      aStatement,
                                      sCurrArgu,
                                      &sFind,
                                      &sArrayVar )
                                  != IDE_SUCCESS )
                            }
                    else
                    {
                        // Nothing to do.
                    }

                    sCurrArgu->node.table  = sTable;
                    sCurrArgu->node.column = sColumn;
                }
                else
                {
                    sFind = ID_FALSE;
                }

                if (sFind == ID_TRUE)
                {
                    if ( ( sCurrArgu->lflag & QTC_NODE_OUTBINDING_MASK )
                         == QTC_NODE_OUTBINDING_DISABLE )
                    {
                        sqlInfo.setSourceInfo( aStatement,
                                               & sCurrArgu->position );
                        IDE_RAISE( ERR_INVALID_OUT_ARGUMENT );
                    }

                    /*
                      if (sRecordVar != NULL)
                      {
                      sqlInfo.setSourceInfo( aStatement,
                      & sCurrArgu->position );
                      IDE_RAISE(ERR_INVALID_OUT_ARGUMENT);
                      }
                    */
                }
                else
                {
                    sqlInfo.setSourceInfo( aStatement,
                                           & sCurrArgu->position );
                    IDE_RAISE( ERR_NOT_EXIST_VARIABLE );
                }
            }
            else if (sCurrArgu->node.module == &(qtc::valueModule))
            {
                // constant value or host variable
                if ( ( sCurrArgu->node.lflag & MTC_NODE_BIND_MASK )
                     == MTC_NODE_BIND_ABSENT ) // constant
                {
                    sqlInfo.setSourceInfo( aStatement,
                                           & sCurrArgu->position );
                    IDE_RAISE( ERR_INVALID_OUT_ARGUMENT );
                }
            }
            else
            {
                sqlInfo.setSourceInfo( aStatement,
                                       & sCurrArgu->position );
                IDE_RAISE( ERR_INVALID_OUT_ARGUMENT );
            }
        }
    }

    if (sParaDecl != NULL)
    {
        sqlInfo.setSourceInfo( aStatement,
                               & aCallSpec->position );
        IDE_RAISE( ERR_INSUFFICIENT_ARGUMENT );
    }

    if (sCurrArgu != NULL)
    {
        sqlInfo.setSourceInfo( aStatement,
                               & aCallSpec->position );
        IDE_RAISE( ERR_TOO_MANY_ARGUMENT );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_SUBQ_NOT_ALLOWED_ON_ARGUMENT);
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode( qpERR_ABORT_QSV_NOT_ALLOWED_SUBQUERY_SQLTEXT,
                             sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION(ERR_INSUFFICIENT_ARGUMENT);
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode( qpERR_ABORT_QSV_INSUFFICIENT_ARGUEMNT_SQLTEXT,
                             sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION(ERR_TOO_MANY_ARGUMENT);
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode( qpERR_ABORT_QSV_TOO_MANY_ARGUEMNT_SQLTEXT,
                             sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION(ERR_INVALID_OUT_ARGUMENT);
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QSV_INVALID_OUT_ARGUEMNT_SQLTEXT,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION(ERR_NOT_EXIST_VARIABLE)
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QSV_NOT_EXIST_VARIABLE_NAME_SQLTEXT,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qsv::checkNoSubquery(
    qcStatement       * aStatement,
    qtcNode           * aNode,
    qcuSqlSourceInfo  * aSourceInfo)
{
    IDU_FIT_POINT_FATAL( "qsv::checkNoSubquery::__FT__" );

    if ( ( aNode->lflag & QTC_NODE_SUBQUERY_MASK )
         == QTC_NODE_SUBQUERY_EXIST )
    {
        aSourceInfo->setSourceInfo( aStatement,
                                    & aNode->position );
        IDE_RAISE( ERR_PASS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_PASS);
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

////////////////////////////////////////////////////////
// To Fix BUG-11921(A3) 11920(A4)
////////////////////////////////////////////////////////
//
// aNode�� ������ ����� call spec ����̴�.
// call spec ���� ��� �ڽ��� ����� ���ϰ��� ����Ű��
// arguments�� ����� ���ڵ��� ����Ų��.
//
// �� �Լ��� ȣ��ǰ� ���� aNode->subquery��
// qcStatement�� �����Ǵµ�, �� �ӿ� ��� ������ ����
// qsExecParseTree �� �����ȴ�.
//
// �׸��� aNode->arguments�� ����ڰ� �������� ����
// �⺻ ���ڵ��� �������ش�.
// �̴� qtc::estimateInternal����
// ����� argument�鿡 ���ؼ� recursive�ϰ�
// qtc::estimateInternal �� ȣ���Ҷ� ���δ�.
IDE_RC qsv::createExecParseTreeOnCallSpecNode(
    qtcNode     * aCallSpecNode,
    mtcCallBack * aCallBack )

{
    qtcCallBackInfo     * sCallBackInfo; /* BUG-45994 - �����Ϸ� ����ȭ ȸ�� */
    qcStatement         * sStatement;    /* BUG-45994 */
    qsExecParseTree     * sParseTree = NULL;
    volatile UInt         sStage;
    qcNamePosition        sPosition;
    qcuSqlSourceInfo      sqlInfo;

    IDE_FT_BEGIN();

    IDU_FIT_POINT_FATAL( "qsv::createExecParseTreeOnCallSpecNode::__FT__" );

    sCallBackInfo = (qtcCallBackInfo*)((mtcCallBack*)aCallBack)->info;
    sStage        = 0; /* BUG-45994 - �����Ϸ� ����ȭ ȸ�� */

    if( sCallBackInfo->statement == NULL )
    {
        // PROJ-1502 PARTITIONED DISK TABLE
        // Nothing to do.
        // �̹� �������.
    }
    else
    {
        // make execute function parse tree
        IDE_TEST(STRUCT_ALLOC(QC_QMP_MEM(sCallBackInfo->statement),
                              qsExecParseTree,
                              &sParseTree)
                 != IDE_SUCCESS);

        IDE_TEST_RAISE(sParseTree == NULL, ERR_MEMORY_SHORTAGE);

        SET_EMPTY_POSITION(sPosition);
        QC_SET_INIT_PARSE_TREE(sParseTree, sPosition);

        IDE_TEST(STRUCT_ALLOC(QC_QMP_MEM(sCallBackInfo->statement),
                              qtcNode,
                              &(sParseTree->leftNode))
                 != IDE_SUCCESS);

        // left �ʱ�ȭ
        QTC_NODE_INIT( sParseTree->leftNode );

        sParseTree->isRootStmt = ID_FALSE;
        sParseTree->mShardObjInfo = NULL;

        sParseTree->callSpecNode = aCallSpecNode;
        sParseTree->paramModules = NULL;
        sParseTree->common.parse    = qsv::parseExeProc;
        sParseTree->common.validate = qsv::validateExeFunc;
        sParseTree->common.optimize = qso::optimizeNone;
        sParseTree->common.execute  = qsx::executeProcOrFunc;

        sParseTree->isCachable = ID_FALSE;  // PROJ-2452

        // make execute function qcStatement
        IDE_TEST(STRUCT_ALLOC(QC_QMP_MEM(sCallBackInfo->statement),
                              qcStatement,
                              &sStatement)
                 != IDE_SUCCESS);

        QC_SET_STATEMENT(sStatement, sCallBackInfo->statement, sParseTree);
        sStatement->myPlan->parseTree->stmt     = sStatement;
        sStatement->myPlan->parseTree->stmtKind = QCI_STMT_EXEC_FUNC;

        // set member of qcStatement
        sStatement->myPlan->planEnv = sCallBackInfo->statement->myPlan->planEnv;
        sStatement->spvEnv = sCallBackInfo->statement->spvEnv;
        sStatement->myPlan->sBindParam = sCallBackInfo->statement->myPlan->sBindParam;
        sStage = 1;

        /* BUG-45994 */
        IDU_FIT_POINT_FATAL( "qsv::createExecParseTreeOnCallSpecNode::__FT__::STAGE1" );

        // for accessing procOID
        aCallSpecNode->subquery = sStatement;

        IDE_TEST_RAISE( ( aCallSpecNode->node.lflag & MTC_NODE_QUANTIFIER_MASK ) ==
                        MTC_NODE_QUANTIFIER_TRUE,
                        ERR_NOT_AGGREGATION );

        sStatement->spvEnv->flag &= ~QSV_ENV_ESTIMATE_ARGU_MASK;
        sStatement->spvEnv->flag |= QSV_ENV_ESTIMATE_ARGU_TRUE;

        if( ( qci::getStartupPhase() != QCI_STARTUP_SERVICE ) &&
            ( qcg::isInitializedMetaCaches() != ID_TRUE ) )
        {
            sqlInfo.setSourceInfo( sStatement,
                                   & aCallSpecNode->columnName );
            IDE_RAISE( err_invalid_psm_not_in_service_phase );
        }

        IDE_TEST(sStatement->myPlan->parseTree->parse( sStatement ) != IDE_SUCCESS);

        // BUG-43061
        // Prevent abnormal server shutdown when validates a node like 'pkg.res2.c1.c1';
        if ( (sStatement->myPlan->parseTree->validate == qsv::validateExecPkgAssign) &&
             (sParseTree->callSpecNode->node.module == &qtc::columnModule) )
        {
            sqlInfo.setSourceInfo( sStatement,
                                   & aCallSpecNode->position );
            IDE_RAISE( ERR_INVALID_IDENTIFIER );
        }
        else
        {
            /* Nothing to do */
        }

        IDE_TEST(sStatement->myPlan->parseTree->validate( sStatement ) != IDE_SUCCESS);

        sStatement->spvEnv->flag &= ~QSV_ENV_ESTIMATE_ARGU_MASK;
        sStatement->spvEnv->flag |= QSV_ENV_ESTIMATE_ARGU_FALSE;

        /* PROJ-1090 Function-based Index */
        if ( sParseTree->isDeterministic == ID_TRUE )
        {
            aCallSpecNode->lflag &= ~QTC_NODE_PROC_FUNC_DETERMINISTIC_MASK;
            aCallSpecNode->lflag |= QTC_NODE_PROC_FUNC_DETERMINISTIC_TRUE;
        }
        else
        {
            aCallSpecNode->lflag &= ~QTC_NODE_PROC_FUNC_DETERMINISTIC_MASK;
            aCallSpecNode->lflag |= QTC_NODE_PROC_FUNC_DETERMINISTIC_FALSE;

            /* PROJ-2462 Result Cache */
            if ( sCallBackInfo->querySet != NULL )
            {
                sCallBackInfo->querySet->lflag &= ~QMV_QUERYSET_RESULT_CACHE_INVALID_MASK;
                sCallBackInfo->querySet->lflag |= QMV_QUERYSET_RESULT_CACHE_INVALID_TRUE;
            }
            else
            {
                /* Nothing to do */
            }
        }
    }

    IDE_FT_END();

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_invalid_psm_not_in_service_phase)
    {
        (void)sqlInfo.init(sStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_PSM_NOT_IN_SERVICE_PHASE,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION( ERR_INVALID_IDENTIFIER );
    {
        (void)sqlInfo.init( sStatement->qmeMem );
        IDE_SET(
            ideSetErrorCode( qpERR_ABORT_QSV_INVALID_IDENTIFIER,
                             sqlInfo.getErrMessage() ) );
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION_SIGNAL()
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_FAULT_TOLERATED ) );
    }
    IDE_EXCEPTION( ERR_MEMORY_SHORTAGE );
    IDE_SET(ideSetErrorCode(mtERR_FATAL_MEMORY_SHORTAGE));
    IDE_EXCEPTION( ERR_NOT_AGGREGATION );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_NOT_AGGREGATION));

    IDE_EXCEPTION_END;

    IDE_FT_EXCEPTION_BEGIN();

    switch (sStage )
    {
        // sStatement->parseTree->validate may have
        // latched and added some procedures to sStatement->spvEnv,
        // which need to be unlocked in MM layer.
        case 1:

            // BUG-41758 replace pkg result worng
            // validate �������� ������ �߻������� �ٽ� �������ش�.
            sStatement->spvEnv->flag &= ~QSV_ENV_ESTIMATE_ARGU_MASK;
            sStatement->spvEnv->flag |= QSV_ENV_ESTIMATE_ARGU_FALSE;

            sCallBackInfo->statement->myPlan->planEnv =
                sStatement->myPlan->planEnv;
            sCallBackInfo->statement->spvEnv = sStatement->spvEnv;
            sCallBackInfo->statement->myPlan->sBindParam = sStatement->myPlan->sBindParam;
    }

    IDE_FT_EXCEPTION_END();

    return IDE_FAILURE;
}

UShort
qsv::getResultSetCount( qcStatement * aStatement )
{
    qsExecParseTree * sParseTree;
    UShort            sResultSetCount = 0;
    iduListNode     * sIterator = NULL;

    sParseTree = (qsExecParseTree*)aStatement->myPlan->parseTree;

    IDU_LIST_ITERATE( &sParseTree->refCurList, sIterator )
    {
        sResultSetCount++;
    }

    return sResultSetCount;
}

// PROJ-1685
/* param property rule
   INDICATOR    LENGTH    MAXLEN
   ------------------------------------------
   IN           O            O         X
   IN OUT/OUT   O            O         O
   RETURN       O            X         X
*/
IDE_RC qsv::validateExtproc( qcStatement * aStatement, qsProcParseTree * aParseTree )
{
    UInt                 sUserID;
    qcuSqlSourceInfo     sqlInfo;
    qcmSynonymInfo       sSynonymInfo;
    qcmLibraryInfo       sLibraryInfo;
    qsProcParseTree    * sParseTree;
    idBool               sExist = ID_FALSE;
    qsCallSpec         * sCallSpec;
    qsCallSpecParam    * sExpParam;
    qsCallSpecParam    * sExpParam2;
    qsVariableItems    * sProcParam;
    idBool               sFound;
    mtcColumn          * sColumn;
    SInt                 sFileSpecSize;
    UInt                 i;

    IDU_FIT_POINT_FATAL( "qsv::validateExtproc::__FT__" );

    sParseTree = aParseTree;

    sCallSpec = sParseTree->expCallSpec;

    IDE_TEST( qcmSynonym::resolveLibrary( aStatement,
                                          sCallSpec->userNamePos,
                                          sCallSpec->libraryNamePos,
                                          & sLibraryInfo,
                                          & sUserID,
                                          & sExist,
                                          & sSynonymInfo )
              != IDE_SUCCESS );

    if( sExist == ID_FALSE )
    {
        sqlInfo.setSourceInfo( aStatement,
                               &sCallSpec->libraryNamePos );
        IDE_RAISE( ERR_NOT_EXIST_LIBRARY_NAME );
    }


    sFileSpecSize = idlOS::strlen( sLibraryInfo.fileSpec );

    IDE_TEST( QC_QMP_MEM( aStatement)->alloc( sFileSpecSize + 1,
                                              (void**)&sCallSpec->fileSpec )
              != IDE_SUCCESS );

    idlOS::memcpy( sCallSpec->fileSpec,
                   sLibraryInfo.fileSpec,
                   sFileSpecSize );

    sCallSpec->fileSpec[sFileSpecSize] = '\0';

    IDE_TEST( qdpRole::checkDMLExecuteLibraryPriv( aStatement,
                                                   sLibraryInfo.libraryID,
                                                   sLibraryInfo.userID )
              != IDE_SUCCESS );

    IDE_TEST( qsvProcStmts::makeRelatedObjects(
                  aStatement,
                  & sCallSpec->userNamePos,
                  & sCallSpec->libraryNamePos,
                  & sSynonymInfo,
                  sLibraryInfo.libraryID,
                  QS_LIBRARY )
              != IDE_SUCCESS );

    // argument name of external procedure name has unsupported datatype name
    sProcParam = sParseTree->paraDecls;

    for( i = 0; i < sParseTree->paraDeclCount; i++ )
    {
        sFound = ID_FALSE;

        sColumn = &sParseTree->paramColumns[i];

        switch( sColumn->type.dataTypeId )
        {
            case MTD_BIGINT_ID:
            case MTD_BOOLEAN_ID:
            case MTD_SMALLINT_ID:
            case MTD_INTEGER_ID:
            case MTD_REAL_ID:
            case MTD_DOUBLE_ID:
            case MTD_CHAR_ID:
            case MTD_VARCHAR_ID:
            case MTD_NCHAR_ID:
            case MTD_NVARCHAR_ID:
            case MTD_NUMERIC_ID:
            case MTD_NUMBER_ID:
            case MTD_FLOAT_ID:
            case MTD_DATE_ID:
            case MTD_INTERVAL_ID:
                sFound = ID_TRUE;
                break;
            case MTD_BLOB_ID:
            case MTD_CLOB_ID:
            case MTD_BYTE_ID:       // PROJ-2717 Internal procedure
            case MTD_VARBYTE_ID:    // PROJ-2717 Internal procedure
                // BUG-39814 IN mode LOB Parameter in Extproc
                if ( ((qsVariables*)sProcParam)->inOutType == QS_IN )
                {
                    sFound = ID_TRUE;
                }
                else
                {
                    // Nothing to do.
                }
                break;
            default:
                break;
        }

        if( sFound == ID_FALSE )
        {
            sqlInfo.setSourceInfo( aStatement,
                                   &sProcParam->name );

            IDE_RAISE( ERR_NOT_SUPPORTED_DATATYPE );
        }

        sProcParam = sProcParam->next;
    }

    if( sCallSpec->param != NULL )
    {
        // copy in/out/in out param type
        for( sProcParam = sParseTree->paraDecls;
             sProcParam != NULL; 
             sProcParam = sProcParam->next )
        {
            for( sExpParam = sCallSpec->param;
                 sExpParam != NULL;
                 sExpParam = sExpParam->next )
            {
                if ( QC_IS_NAME_MATCHED( sProcParam->name, sExpParam->paramNamePos ) )
                {
                    sExpParam->inOutType = ((qsVariables *)sProcParam)->inOutType;
                }
            }
        }

        // RETURN, for actual function return, must be last in the parameters clause 
        for( sExpParam = sCallSpec->param;
             sExpParam != NULL;
             sExpParam = sExpParam->next )
        {
            if ( QC_IS_STR_CASELESS_MATCHED( sExpParam->paramNamePos, "RETURN" ) &&
                 ( QC_IS_NULL_NAME( sExpParam->paramPropertyPos ) == ID_TRUE ) )
            {
                if( sExpParam->next != NULL )
                {
                    sqlInfo.setSourceInfo( aStatement,
                                           &sExpParam->paramNamePos );

                    IDE_RAISE( ERR_RETURN_MUST_BE_IN_THE_LAST );
                }
            }
        }

        // Incorrect Usage of RETURN in parameters clause
        for( sExpParam = sCallSpec->param;
             sExpParam != NULL;
             sExpParam = sExpParam->next )
        {
            if ( QC_IS_STR_CASELESS_MATCHED( sExpParam->paramNamePos, "RETURN" ) )
            {
                if( sParseTree->returnTypeVar == NULL )
                {
                    sqlInfo.setSourceInfo( aStatement,
                                           &sExpParam->paramNamePos );

                    IDE_RAISE( ERR_INCORRECT_USAGE_OF_RETURN );
                }
            }
        }

        // Incorrect Usage of MAXLEN/LENGTH in parameters clause
        for( sExpParam = sCallSpec->param;
             sExpParam != NULL;
             sExpParam = sExpParam->next )
        {
            if( QC_IS_NULL_NAME( sExpParam->paramPropertyPos ) != ID_TRUE )
            { 
                if ( QC_IS_STR_CASELESS_MATCHED( sExpParam->paramPropertyPos, "MAXLEN" ) )
                {
                    if( sExpParam->inOutType == QS_IN )
                    {
                        sqlInfo.setSourceInfo( aStatement,
                                               &sExpParam->paramPropertyPos );

                        IDE_RAISE( ERR_INCORRECT_USAGE_OF_MAXLEN );
                    }
                }
            }
        }

        for( sExpParam = sCallSpec->param;
             sExpParam != NULL;
             sExpParam = sExpParam->next )
        {
            if ( QC_IS_STR_CASELESS_MATCHED( sExpParam->paramNamePos, "RETURN" ) )
            {
                if( QC_IS_NULL_NAME( sExpParam->paramPropertyPos ) != ID_TRUE )
                { 
                    if ( QC_IS_STR_CASELESS_MATCHED( sExpParam->paramPropertyPos, "MAXLEN" ) )
                    {
                        sqlInfo.setSourceInfo( aStatement,
                                               &sExpParam->paramPropertyPos );

                        IDE_RAISE( ERR_INCORRECT_USAGE_OF_MAXLEN );
                    }

                    if ( QC_IS_STR_CASELESS_MATCHED( sExpParam->paramPropertyPos, "LENGTH" ) )
                    {
                        sqlInfo.setSourceInfo( aStatement,
                                               &sExpParam->paramPropertyPos );

                        IDE_RAISE( ERR_INCORRECT_USAGE_OF_LENGTH );
                    }

                }
            }
        }

        // Formal parameter missing in the parameters
        for( sProcParam = sParseTree->paraDecls; 
             sProcParam != NULL; 
             sProcParam = sProcParam->next )
        {
            sFound = ID_FALSE;

            for( sExpParam = sCallSpec->param;
                 sExpParam != NULL;
                 sExpParam = sExpParam->next )
            {
                if ( QC_IS_NAME_MATCHED( sProcParam->name, sExpParam->paramNamePos ) &&
                     ( QC_IS_NULL_NAME( sExpParam->paramPropertyPos ) == ID_TRUE ) )
                {
                    sFound = ID_TRUE;
                    break;
                }
            }

            if( sFound == ID_FALSE )
            {
                sqlInfo.setSourceInfo( aStatement,
                                       &sProcParam->name );

                IDE_RAISE( ERR_FORMAL_PARAMETER_MISSING );
            }
        }

        // external parameter name not found in formal parameter list 
        for( sExpParam = sCallSpec->param;
             sExpParam != NULL;
             sExpParam = sExpParam->next )
        {
            sFound = ID_FALSE;

            if ( QC_IS_STR_CASELESS_MATCHED( sExpParam->paramNamePos, "RETURN" ) )
            {
                sFound = ID_TRUE;
                continue; // visit next parameter pos.
            }

            for( sProcParam = sParseTree->paraDecls;
                 sProcParam != NULL; 
                 sProcParam = sProcParam->next )
            {
                if ( QC_IS_NAME_MATCHED( sExpParam->paramNamePos, sProcParam->name ) )
                {
                    sFound = ID_TRUE;
                    break;
                }
            }

            if( sFound == ID_FALSE )
            {
                sqlInfo.setSourceInfo( aStatement,
                                       &sExpParam->paramNamePos );

                IDE_RAISE( ERR_EXTERNAL_PARAMETER_NAME_NOT_FOUND );
            }
        }


        // Multiple declarations in foreign function formal parameter list
        for( sExpParam = sCallSpec->param;
             sExpParam != NULL;
             sExpParam = sExpParam->next )
        {
            sFound = ID_FALSE;

            if ( QC_IS_STR_CASELESS_MATCHED( sExpParam->paramNamePos, "RETURN" ) )
            {
                continue;
            }

            for( sExpParam2 = sExpParam->next;
                 sExpParam2 != NULL;
                 sExpParam2 = sExpParam2->next )
            {
                if ( QC_IS_NAME_MATCHED( sExpParam->paramNamePos, sExpParam2->paramNamePos ) )
                {
                    if( QC_IS_NULL_NAME( sExpParam->paramPropertyPos ) == ID_TRUE &&
                        QC_IS_NULL_NAME( sExpParam2->paramPropertyPos ) == ID_TRUE )
                    {
                        sFound = ID_TRUE;
                        break;
                    }
                    else 
                    {
                        if( QC_IS_NULL_NAME( sExpParam->paramPropertyPos ) != ID_TRUE &&
                            QC_IS_NULL_NAME( sExpParam2->paramPropertyPos ) != ID_TRUE )
                        {
                            if ( QC_IS_NAME_MATCHED( sExpParam->paramPropertyPos, sExpParam2->paramPropertyPos ) )
                            {
                                sFound = ID_TRUE;
                                break;
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
                    }
                }
            }

            if( sFound == ID_TRUE )
            {
                sqlInfo.setSourceInfo( aStatement,
                                       &sExpParam->paramNamePos );

                IDE_RAISE( ERR_MULTIPLE_DECLARATIONS );
            }
        }

        // overwrite MAXLEN parameter
        for( sExpParam = sCallSpec->param;
             sExpParam != NULL;
             sExpParam = sExpParam->next )
        {
            if( QC_IS_NULL_NAME( sExpParam->paramPropertyPos ) == ID_TRUE )
            { 
                // Nothing to do.
            }
            else
            { 
                if ( QC_IS_STR_CASELESS_MATCHED( sExpParam->paramPropertyPos, "MAXLEN" ) )
                {
                    sExpParam->inOutType = QS_IN;
                }
                else
                {
                    // Nothing to do.
                }
            }
        }
    }
    else
    {
        if( sParseTree->paraDeclCount > 0 )
        {
            IDE_TEST( QC_QMP_MEM( aStatement)->alloc( ID_SIZEOF( qsCallSpecParam ) * sParseTree->paraDeclCount,
                                                      (void**)&sCallSpec->param )
                      != IDE_SUCCESS );

            for( i = 0, 
                     sProcParam = sParseTree->paraDecls;
                 sProcParam != NULL; 
                 sProcParam = sProcParam->next )
            {
                sExpParam = &sCallSpec->param[i];

                SET_POSITION( sExpParam->paramNamePos, sProcParam->name );

                SET_EMPTY_POSITION( sExpParam->paramPropertyPos );

                sExpParam->inOutType = ((qsVariables *)sProcParam)->inOutType;

                if( sProcParam->next != NULL )
                {
                    sExpParam->next = &sCallSpec->param[i + 1];
                }
                else
                {
                    sExpParam->next = NULL;
                }

                i++;
            }
        }
    }

    sCallSpec->paramCount = 0;

    // copy table, column
    for( sExpParam = sCallSpec->param;
         sExpParam != NULL;
         sExpParam = sExpParam->next )
    {
        if ( QC_IS_STR_CASELESS_MATCHED( sExpParam->paramNamePos, "RETURN" ) )
        {
            IDE_DASSERT( sParseTree->returnTypeVar != NULL );

            sExpParam->table = sParseTree->returnTypeVar->common.table;
            sExpParam->column = sParseTree->returnTypeVar->common.column;
        }
        else
        {
            for( sProcParam = sParseTree->paraDecls;
                 sProcParam != NULL; 
                 sProcParam = sProcParam->next )
            {
                if ( QC_IS_NAME_MATCHED( sExpParam->paramNamePos, sProcParam->name ) )
                {
                    sExpParam->table = sProcParam->table;
                    sExpParam->column = sProcParam->column;

                    break;
                }
            }
        }

        sCallSpec->paramCount++;
    }

    aStatement->spvEnv->flag &= ~QSV_ENV_RETURN_STMT_MASK;
    aStatement->spvEnv->flag |= QSV_ENV_RETURN_STMT_EXIST;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NOT_EXIST_LIBRARY_NAME )
    {
        (void)sqlInfo.init( aStatement->qmeMem );
        IDE_SET(
            ideSetErrorCode( qpERR_ABORT_QSV_NOT_EXIST_LIBRARY_SQLTEXT,
                             sqlInfo.getErrMessage() ) );
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION( ERR_NOT_SUPPORTED_DATATYPE )
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode( qpERR_ABORT_QSV_PROC_NOT_SUPPORTED_DATATYPE_SQLTEXT,
                             sqlInfo.getErrMessage() ) );
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION( ERR_RETURN_MUST_BE_IN_THE_LAST )
    {
        (void)sqlInfo.init( aStatement->qmeMem );
        IDE_SET(
            ideSetErrorCode( qpERR_ABORT_QSV_RETURN_MUST_BE_IN_THE_LAST,
                             sqlInfo.getErrMessage() ) );
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION( ERR_INCORRECT_USAGE_OF_RETURN )
    {
        (void)sqlInfo.init( aStatement->qmeMem );
        IDE_SET(
            ideSetErrorCode( qpERR_ABORT_QSV_INCORRECT_USAGE_OF_XXX,
                             "RETURN", sqlInfo.getErrMessage() ) );
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION( ERR_INCORRECT_USAGE_OF_MAXLEN )
    {
        (void)sqlInfo.init( aStatement->qmeMem );
        IDE_SET(
            ideSetErrorCode( qpERR_ABORT_QSV_INCORRECT_USAGE_OF_XXX,
                             "MAXLEN", sqlInfo.getErrMessage() ) );
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION( ERR_INCORRECT_USAGE_OF_LENGTH )
    {
        (void)sqlInfo.init( aStatement->qmeMem );
        IDE_SET(
            ideSetErrorCode( qpERR_ABORT_QSV_INCORRECT_USAGE_OF_XXX,
                             "LENGTH", sqlInfo.getErrMessage() ) );
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION( ERR_FORMAL_PARAMETER_MISSING )
    {
        (void)sqlInfo.init( aStatement->qmeMem );
        IDE_SET(
            ideSetErrorCode( qpERR_ABORT_QSV_FORMAL_PARAMETER_MISSING,
                             sqlInfo.getErrMessage() ) );
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION( ERR_MULTIPLE_DECLARATIONS )
    {
        (void)sqlInfo.init( aStatement->qmeMem );
        IDE_SET(
            ideSetErrorCode( qpERR_ABORT_QSV_MULTIPLE_DECLARATIONS,
                             sqlInfo.getErrMessage() ) );
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION( ERR_EXTERNAL_PARAMETER_NAME_NOT_FOUND )
    {
        (void)sqlInfo.init( aStatement->qmeMem );
        IDE_SET(
            ideSetErrorCode( qpERR_ABORT_QSV_EXTERNAL_PARAMETER_NAME_NOT_FOUND,
                             sqlInfo.getErrMessage() ) );
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

// PROJ-1073 Package
IDE_RC qsv::parseCreatePkg( qcStatement * aStatement )
{
    qsPkgParseTree    * sParseTree;
    qcuSqlSourceInfo    sqlInfo;

    sParseTree = ( qsPkgParseTree * )( aStatement->myPlan-> parseTree );

    aStatement->spvEnv->createPkg = sParseTree;

    if( qdbCommon::containDollarInName( &( sParseTree->pkgNamePos ) ) == ID_TRUE )
    {
        sqlInfo.setSourceInfo(
            aStatement,
            &( sParseTree->pkgNamePos ) );
        IDE_RAISE( CANT_USE_RESERVED_WORD );
    }

    sParseTree->stmtText = aStatement->myPlan->stmtText;
    sParseTree->stmtTextLen = aStatement->myPlan->stmtTextLen;

    IDE_TEST( checkHostVariable( aStatement ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( CANT_USE_RESERVED_WORD );
    {
        (void)sqlInfo.init( aStatement->qmeMem );
        IDE_SET(
            ideSetErrorCode( qpERR_ABORT_QDB_RESERVED_WORD_IN_OBJECT_NAME,
                             sqlInfo.getErrMessage() ) );
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qsv::validateCreatePkg( qcStatement * aStatement )
{
    /* 1. ��ü�� �̸� �ߺ� Ȯ��
     * 2. ���� Ȯ��
     * 3. parsing block
     * 4. validate block          */

    qsPkgParseTree     * sParseTree;
    qcuSqlSourceInfo     sqlInfo;
    idBool               sExist = ID_FALSE;
    UInt                 sUserID = QCG_GET_SESSION_USER_ID( aStatement );

    sParseTree = (qsPkgParseTree *)(aStatement->myPlan->parseTree);

    // 1. check already existitng object
    IDE_TEST( qcm::existObject( aStatement,
                                ID_FALSE,
                                sParseTree->userNamePos,
                                sParseTree->pkgNamePos,
                                sParseTree->objType,
                                &(sParseTree->userID),
                                &(sParseTree->pkgOID),
                                &sExist )
              != IDE_SUCCESS );
    if( sParseTree->pkgOID != QS_EMPTY_OID )
    {
        sqlInfo.setSourceInfo( aStatement, & sParseTree->pkgNamePos );
        IDE_RAISE( ERR_DUP_PKG_NAME );
    }
    else
    {
        IDE_TEST_RAISE(sExist == ID_TRUE, ERR_EXIST_OBJECT_NAME);
    }

    // 2. check grant
    IDE_TEST( qdpRole::checkDDLCreatePSMPriv( aStatement,
                                              sParseTree->userID )
              != IDE_SUCCESS );

    // BUG-42322
    IDE_TEST( setObjectNameInfo( aStatement,
                                 sParseTree->objType,
                                 sParseTree->userID,
                                 sParseTree->pkgNamePos,
                                 &sParseTree->objectNameInfo )
              != IDE_SUCCESS );

    QCG_SET_SESSION_USER_ID( aStatement, sParseTree->userID );

    // 3. parsing block
    IDE_TEST( sParseTree->block->common.parse(
                  aStatement,
                  (qsProcStmts *)(sParseTree->block) )
              != IDE_SUCCESS );

    // 4. validation block
    IDE_TEST( sParseTree->block->common.validate(
                  aStatement,
                  (qsProcStmts *)(sParseTree->block),
                  NULL,
                  QS_PURPOSE_PKG )
              != IDE_SUCCESS );

    QCG_SET_SESSION_USER_ID( aStatement, sUserID );

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_EXIST_OBJECT_NAME);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDB_EXIST_OBJECT_NAME));
    }
    IDE_EXCEPTION(ERR_DUP_PKG_NAME);
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QSV_DUPLICATE_PKG,
                            sqlInfo.getErrMessage()));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION_END;

    QCG_SET_SESSION_USER_ID( aStatement, sUserID );

    return IDE_FAILURE;
}

IDE_RC qsv::validateReplacePkg( qcStatement * aStatement )
{
    /* 1. ��ü�� �̸� �ߺ� Ȯ��
     * 2. ���� Ȯ��
     * 3. parsing block
     * 4. validate block          */

    qsPkgParseTree     * sParseTree;
    qcuSqlSourceInfo     sqlInfo;
    idBool               sExist = ID_FALSE;
    UInt                 sUserID = QCG_GET_SESSION_USER_ID( aStatement );
    idBool               sIsUsed = ID_FALSE;

    sParseTree = ( qsPkgParseTree * )( aStatement->myPlan->parseTree );

    // 1. check already existitng object
    IDE_TEST( qcm::existObject( aStatement,
                                ID_FALSE,
                                sParseTree->userNamePos,
                                sParseTree->pkgNamePos,
                                sParseTree->objType,
                                &( sParseTree->userID ),
                                &( sParseTree->pkgOID ),
                                &sExist )
              != IDE_SUCCESS );
    if( sExist == ID_TRUE )
    {
        // ��ü�� �̸��� ���� �ٸ� Ÿ���� ��ü
        if( sParseTree->pkgOID == QS_EMPTY_OID )
        {
            IDE_TEST_RAISE( sExist == ID_TRUE, ERR_EXIST_OBJECT_NAME );
        }
        else
        {
            // replace
            sParseTree->common.execute = qsx::replacePkgOrPkgBody;

            // 2. check grant
            IDE_TEST( qdpRole::checkDDLCreatePSMPriv( aStatement,
                                                      sParseTree->userID )
                      != IDE_SUCCESS );
        }
    }
    else
    {
        // create new procedure
        sParseTree->common.execute = qsx::createPkg;

        // 2. check grant
        IDE_TEST( qdpRole::checkDDLCreatePSMPriv( aStatement,
                                                  sParseTree->userID )
                  != IDE_SUCCESS );
    }

    // BUG-42322
    IDE_TEST( setObjectNameInfo( aStatement,
                                 sParseTree->objType,
                                 sParseTree->userID,
                                 sParseTree->pkgNamePos,
                                 &sParseTree->objectNameInfo )
              != IDE_SUCCESS );

    // BUG-38800 Server startup�� Function-Based Index���� ��� ���� Function��
    // recompile �� �� �־�� �Ѵ�.
    if ( ( aStatement->session->mQPSpecific.mFlag & QC_SESSION_INTERNAL_LOAD_PROC_MASK )
         != QC_SESSION_INTERNAL_LOAD_PROC_TRUE )
    {
        /* BUG-35445 Check Constraint, Function-Based Index���� ��� ���� Function�� ����/���� ���� */
        IDE_TEST( qcmProc::relIsUsedProcByConstraint(
                      aStatement,
                      &(sParseTree->pkgNamePos),
                      sParseTree->userID,
                      &sIsUsed )
                  != IDE_SUCCESS );
        IDE_TEST_RAISE( sIsUsed == ID_TRUE, ERR_PROC_USED_BY_CONSTRAINT );

        IDE_TEST( qcmProc::relIsUsedProcByIndex(
                      aStatement,
                      &(sParseTree->pkgNamePos),
                      sParseTree->userID,
                      &sIsUsed )
                  != IDE_SUCCESS );
        IDE_TEST_RAISE( sIsUsed == ID_TRUE, ERR_PROC_USED_BY_INDEX );
    }
    else
    {
        // Nothing to do.
    }

    QCG_SET_SESSION_USER_ID( aStatement, sParseTree->userID );

    // 3. parsing block
    IDE_TEST( sParseTree->block->common.parse(
                  aStatement,
                  ( qsProcStmts * )( sParseTree->block ) )
              != IDE_SUCCESS );

    // 4. validation block
    IDE_TEST( sParseTree->block->common.validate(
                  aStatement,
                  ( qsProcStmts * )( sParseTree->block ),
                  NULL,
                  QS_PURPOSE_PKG )
              != IDE_SUCCESS );

    QCG_SET_SESSION_USER_ID( aStatement, sUserID );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_EXIST_OBJECT_NAME );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDB_EXIST_OBJECT_NAME ) );
    }
    IDE_EXCEPTION( ERR_PROC_USED_BY_CONSTRAINT );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QSV_PROC_USED_BY_CONSTRAINT ) );
    }
    IDE_EXCEPTION( ERR_PROC_USED_BY_INDEX );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QSV_PROC_USED_BY_INDEX ) );
    }
    IDE_EXCEPTION_END;

    QCG_SET_SESSION_USER_ID( aStatement, sUserID );

    return IDE_FAILURE;
}

IDE_RC qsv::validateCreatePkgBody( qcStatement * aStatement )
{
    /* 1. package spec�� ���翩�� Ȯ��
     * 2. package body�� �ߺ����� Ȯ��
     * 3. ���� üũ
     * 4. parsing block
     * 5. validation block             */

    qsPkgParseTree     * sParseTree;
    qcuSqlSourceInfo     sqlInfo;
    idBool               sExist       = ID_FALSE;
    qsOID                sPkgSpecOID;
    qsxPkgInfo         * sPkgSpecInfo;
    UInt                 sUserID      = QCG_GET_SESSION_USER_ID( aStatement );
    SInt                 sStage       = 0;

    sParseTree   = (qsPkgParseTree *)(aStatement->myPlan->parseTree);

    // package body�� package spec�� �־�߸� ���� �� �ִ�.
    // SYS_PACKAGES_���� ������ �̸��� ����  row�� ã�� ���ϸ� ���� �� ����.
    // ����, existObject���� package spec�� �ִ��� ã�´�.
    IDE_TEST( qcm::existObject( aStatement,
                                ID_FALSE,
                                sParseTree->userNamePos,
                                sParseTree->pkgNamePos,
                                QS_PKG,
                                &(sParseTree->userID),
                                &sPkgSpecOID,
                                &sExist )
              != IDE_SUCCESS );

    // ��ü�� �����Ѵ�.
    if( sExist == ID_TRUE )
    {
        // spec�� OID�� �ʱⰪ�� �����ϸ� package��
        // �������� �ʱ� ������ body ������ �Ұ����ϸ�,
        // spec�� ���ٴ� error�� ǥ���Ѵ�.
        if ( sPkgSpecOID == QS_EMPTY_OID )
        {
            sqlInfo.setSourceInfo( aStatement, &(sParseTree->pkgNamePos) );
            IDE_RAISE( ERR_NOT_EXIST_PACKAGE_NAME );
        }
        else
        {
            /* BUG-38844
               create or replace package body�� ��, spec�� ���� �����ϱ� ������
               spec�� ����Ǵ� ���� ���� ���� S latch�� ��Ƶд�.
               1) package body�� ���������� ������ ���
               => qci::hardPrepare PVO �Ŀ� aStatement->spvEnv->procPlanList�� ���� unlatch ����
               2) package spec�� procPlanList�� �ޱ� ������ excpetion �߻��� ���
               => �� �Լ��� exception ó�� �� package spec�� ���ؼ� unlatch
               3) package spec�� procPlanList�� �޸� ���� exception �߻�
               => qci::hardPrepare�� exception ó�� �� procPlanList�� �޸� ��ü�� ���� unlatch */
            IDE_TEST( qsxPkg::latchS( sPkgSpecOID )
                      != IDE_SUCCESS );
            sStage = 1;

            IDE_TEST( qsxPkg::getPkgInfo( sPkgSpecOID,
                                          & sPkgSpecInfo )
                      != IDE_SUCCESS );

            /* BUG-38348 Package spec�� invalid ������ ���� �ִ�. */
            if ( sPkgSpecInfo->isValid == ID_TRUE )
            {
                /* BUG-38844
                   package spec�� spvEnv->procPlanList�� �޾Ƶд�. */
                IDE_TEST( qsvPkgStmts::makeRelatedPkgSpecToPkgBody( aStatement,
                                                                    sPkgSpecInfo )
                          != IDE_SUCCESS );

                /* BUG-41847
                   package spec�� related object�� body�� related object�� ��� */
                IDE_TEST( qsvPkgStmts::inheritRelatedObjectFromPkgSpec( aStatement,
                                                                        sPkgSpecInfo )
                          != IDE_SUCCESS );
                sStage = 0;

                // spec�� OID�� �ʱⰪ�� �ƴϸ�, spec�� �����ϴ� ���̸�,
                // body�� ������ �� �ִ�.
                IDE_TEST( qcmPkg::getPkgExistWithEmptyByName( aStatement,
                                                              sParseTree->userID,
                                                              sParseTree->pkgNamePos,
                                                              sParseTree->objType,
                                                              &( sParseTree->pkgOID ) )
                          != IDE_SUCCESS );

                // sParseTree->pkgOID�� �ʱⰪ�� ���, spec ������ qsvEnv�� �����Ѵ�
                if( sParseTree->pkgOID == QS_EMPTY_OID)
                {
                    aStatement->spvEnv->pkgPlanTree = sPkgSpecInfo->planTree;
                }
                else
                {
                    // sParseTree->pkgOID�� �ʱⰪ�� �ƴ� ���,
                    // ������ body�� �����ϴٴ� �ǹ��̹Ƿ� body �ߺ� error�� ǥ���Ѵ�.
                    sqlInfo.setSourceInfo( aStatement, &(sParseTree->pkgNamePos) );
                    IDE_RAISE( ERR_DUP_PKG_NAME );
                }
            }
            else
            {
                sqlInfo.setSourceInfo( aStatement, &(sParseTree->pkgNamePos) );
                IDE_RAISE( ERR_INVALID_PACKAGE_SPEC );
            }
        }
    }
    else
    {
        // ��ü�� �������� �ʴ´�.
        sqlInfo.setSourceInfo( aStatement, &(sParseTree->pkgNamePos) );
        IDE_RAISE( ERR_NOT_EXIST_PACKAGE_NAME );
    }

    // 2. check grant
    IDE_TEST( qdpRole::checkDDLCreatePSMPriv( aStatement,
                                              sParseTree->userID )
              != IDE_SUCCESS );

    // BUG-42322
    IDE_TEST( setObjectNameInfo( aStatement,
                                 sParseTree->objType,
                                 sParseTree->userID,
                                 sParseTree->pkgNamePos,
                                 &sParseTree->objectNameInfo )
              != IDE_SUCCESS );

    /* BUG-45306 PSM AUTHID 
       package body�� authid�� package spec�� authid�� �����ϰ� �����ؾ� �Ѵ�. */
    sParseTree->isDefiner = aStatement->spvEnv->pkgPlanTree->isDefiner; 

    QCG_SET_SESSION_USER_ID( aStatement, sParseTree->userID );

    // 3. parsing block
    IDE_TEST( sParseTree->block->common.parse(
                  aStatement,
                  (qsProcStmts *)(sParseTree->block) )
              != IDE_SUCCESS );

    // 4. validation block
    IDE_TEST( sParseTree->block->common.validate(
                  aStatement,
                  (qsProcStmts *)(sParseTree->block),
                  NULL,
                  QS_PURPOSE_PKG )
              != IDE_SUCCESS );

    QCG_SET_SESSION_USER_ID( aStatement, sUserID );

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_NOT_EXIST_PACKAGE_NAME);
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QSV_NOT_EXIST_PACKAGE_SPECIFICATION,
                            sqlInfo.getErrMessage()));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION(ERR_DUP_PKG_NAME);
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QSV_DUPLICATE_PKG,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION(ERR_INVALID_PACKAGE_SPEC);
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QSV_INVALID_PACKAGE_SPECIFICATION,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION_END;

    QCG_SET_SESSION_USER_ID( aStatement, sUserID );

    /* BUG-38844
       1) sStage == 1
       => aStatement->spvEnv->procPlanList�� �ޱ� �����̹Ƿ�, �� �Լ����� unlatch
       2) sStage == 0
       => procPlanList�� �޸� ���̹Ƿ�, qci::hardPrepare���� exception ó�� ��
       procPlanList�� �޸� ��ü��� �Բ� unlatch */
    if ( sStage == 1 )
    {
        sStage = 0;
        (void) qsxPkg::unlatch ( sPkgSpecOID );
    }
    else
    {
        // Nothing to do.
    }

    return IDE_FAILURE;
}

IDE_RC qsv::validateReplacePkgBody( qcStatement * aStatement )
{
    /* 1. package spec�� ���翩�� Ȯ��
     * 2. package body�� �ߺ����� Ȯ��
     * 3. package spec�� ���� ����
     * 4. ���� üũ
     * 5. parsing block
     * 6. validation block             */

    qsPkgParseTree     * sParseTree;
    qcuSqlSourceInfo     sqlInfo;
    idBool               sExist       = ID_FALSE;
    qsOID                sPkgSpecOID  = QS_EMPTY_OID;
    qsxPkgInfo         * sPkgSpecInfo;
    UInt                 sUserID      = QCG_GET_SESSION_USER_ID( aStatement );
    SInt                 sStage       = 0;

    sParseTree   = (qsPkgParseTree *)(aStatement->myPlan->parseTree);

    // package body�� package spec�� �־�߸� ���� �� �ִ�.
    // SYS_PACKAGES_���� ������ �̸��� ����  row�� ã�� ���ϸ� ���� �� ����.
    // 1. package spec�� ���� ���� Ȯ��
    IDE_TEST( qcm::existObject( aStatement,
                                ID_FALSE,
                                sParseTree->userNamePos,
                                sParseTree->pkgNamePos,
                                QS_PKG,
                                &(sParseTree->userID),
                                &sPkgSpecOID,
                                &sExist )
              != IDE_SUCCESS );

    // ��ü�� �����Ѵ�.
    if( sExist == ID_TRUE )
    {
        // spec�� OID�� �ʱⰪ�� �����ϸ� package��
        // �������� �ʱ� ������ body ������ �Ұ����ϸ�,
        // spec�� ���ٴ� error�� ǥ���Ѵ�.
        if( sPkgSpecOID == QS_EMPTY_OID )
        {
            sqlInfo.setSourceInfo( aStatement, &(sParseTree->pkgNamePos) );
            IDE_RAISE( ERR_NOT_EXIST_PACKAGE_NAME );
        }
        else
        {
            /* BUG-38348
               package spec�� rebuild �ϴ� �����ϸ�,
               package spec�� ������, invalid �����̴�.
               �� ���� body ���� invalid�� �Ǿ�� �Ѵ�.*/
            /* BUG-38844
               create or replace package body�� ��, spec�� ���� �����ϱ� ������
               spec�� ����Ǵ� ���� ���� ���� S latch�� ��Ƶд�.
               1) package body�� ���������� ������ ���
               => qci::hardPrepare PVO �Ŀ� aStatement->spvEnv->procPlanList�� ���� unlatch ����
               2) package spec�� procPlanList�� �ޱ� ������ excpetion �߻��� ���
               => �� �Լ��� exception ó�� �� package spec�� ���ؼ� unlatch
               3) package spec�� procPlanList�� �޸� ���� exception �߻�
               => qci::hardPrepare�� exception ó�� �� procPlanList�� �޸� ��ü�� ���� unlatch */
            IDE_TEST( qsxPkg::latchS( sPkgSpecOID )
                      != IDE_SUCCESS );
            sStage = 1;

            IDE_TEST( qsxPkg::getPkgInfo( sPkgSpecOID,
                                          & sPkgSpecInfo )
                      != IDE_SUCCESS );

            if ( sPkgSpecInfo->isValid == ID_TRUE )
            {
                /*BUG-38844
                  package spec�� spvEnv->procPlanList�� �޾Ƶд�. */
                IDE_TEST( qsvPkgStmts::makeRelatedPkgSpecToPkgBody( aStatement,
                                                                    sPkgSpecInfo )
                          != IDE_SUCCESS );

                /* BUG-41847
                   package spec�� related object�� body�� related object�� ��� */
                IDE_TEST( qsvPkgStmts::inheritRelatedObjectFromPkgSpec( aStatement,
                                                                        sPkgSpecInfo )
                          != IDE_SUCCESS );
                sStage = 0;

                // spec�� OID�� �ʱⰪ�� �ƴϸ�, spec�� �����ϴ� ���̸�,
                // body�� ������ �� �ִ�.
                // 2. package body�� �ߺ����� Ȯ��
                IDE_TEST( qcmPkg::getPkgExistWithEmptyByName( aStatement,
                                                              sParseTree->userID,
                                                              sParseTree->pkgNamePos,
                                                              sParseTree->objType,
                                                              &( sParseTree->pkgOID ) )
                          != IDE_SUCCESS );

                if( sParseTree->pkgOID != QS_EMPTY_OID )
                {
                    // replace
                    sParseTree->common.execute = qsx::replacePkgOrPkgBody;
                }
                else
                {
                    // create new package body
                    sParseTree->common.execute = qsx::createPkgBody;
                }

                // 3. spec�� ���� ���� ����
                aStatement->spvEnv->pkgPlanTree = sPkgSpecInfo->planTree;

                // 4. check grant
                IDE_TEST( qdpRole::checkDDLCreatePSMPriv(
                              aStatement,
                              sParseTree->userID )
                          != IDE_SUCCESS );
            }
            else
            {
                sqlInfo.setSourceInfo( aStatement, &(sParseTree->pkgNamePos) );
                IDE_RAISE( ERR_INVALID_PACKAGE_SPEC );
            }
        }
    }
    else
    {
        // ��ü�� �������� �ʴ´�.
        sqlInfo.setSourceInfo( aStatement, &(sParseTree->pkgNamePos) );
        IDE_RAISE( ERR_NOT_EXIST_PACKAGE_NAME );
    }

    // BUG-42322
    IDE_TEST( setObjectNameInfo( aStatement,
                                 sParseTree->objType,
                                 sParseTree->userID,
                                 sParseTree->pkgNamePos,
                                 &sParseTree->objectNameInfo )
              != IDE_SUCCESS );

    /* BUG-45306 PSM AUTHID 
       package body�� authid�� package spec�� authid�� �����ϰ� �����ؾ� �Ѵ�. */
    sParseTree->isDefiner = aStatement->spvEnv->pkgPlanTree->isDefiner; 

    QCG_SET_SESSION_USER_ID( aStatement, sParseTree->userID );

    // 5. parsing block
    IDE_TEST( sParseTree->block->common.parse(
                  aStatement,
                  (qsProcStmts *)(sParseTree->block) )
              != IDE_SUCCESS );

    // 6. validation block
    IDE_TEST( sParseTree->block->common.validate(
                  aStatement,
                  (qsProcStmts *)(sParseTree->block),
                  NULL,
                  QS_PURPOSE_PKG )
              != IDE_SUCCESS );

    QCG_SET_SESSION_USER_ID( aStatement, sUserID );

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_NOT_EXIST_PACKAGE_NAME);
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QSV_NOT_EXIST_PACKAGE_SPECIFICATION,
                            sqlInfo.getErrMessage()));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION(ERR_INVALID_PACKAGE_SPEC);
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QSV_INVALID_PACKAGE_SPECIFICATION,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION_END;

    QCG_SET_SESSION_USER_ID( aStatement, sUserID );

    /* BUG-38844
       1) sStage == 1
       => aStatement->spvEnv->procPlanList�� �ޱ� �����̹Ƿ�, �� �Լ����� unlatch
       2) sStage == 0
       => procPlanList�� �޸� ���̹Ƿ�, qci::hardPrepare���� exception ó�� ��
       procPlanList�� �޸� ��ü��� �Բ� unlatch */
    if ( sStage == 1 )
    {
        sStage = 0;
        (void) qsxPkg::unlatch ( sPkgSpecOID );
    }
    else
    {
        // Nothing to do.
    }

    return IDE_FAILURE;
}

IDE_RC qsv::validateAltPkg( qcStatement * aStatement )
{
    qsAlterPkgParseTree * sParseTree;
    qcuSqlSourceInfo      sqlInfo;

    sParseTree = ( qsAlterPkgParseTree * )( aStatement->myPlan->parseTree );

    // 1. check userID and pkgOID
    IDE_TEST( checkUserAndPkg(
                  aStatement,
                  sParseTree->userNamePos,
                  sParseTree->pkgNamePos,
                  &( sParseTree->userID ),
                  &( sParseTree->specOID ),
                  &( sParseTree->bodyOID ) )
              != IDE_SUCCESS );

    if( sParseTree->specOID == QS_EMPTY_OID )
    {
        sqlInfo.setSourceInfo( aStatement,
                               & sParseTree->pkgNamePos );
        IDE_RAISE( ERR_NOT_EXIST_PKG_NAME );
    }
    else
    {
        // Nothing to do.
    }

    if( sParseTree->bodyOID == QS_EMPTY_OID )
    {
        if( sParseTree->option == QS_PKG_BODY_ONLY )
        {
            sqlInfo.setSourceInfo( aStatement,
                                   & sParseTree->pkgNamePos );
            IDE_RAISE( ERR_NOT_EXIST_PKG_BODY_NAME );
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

    // 2. check grant
    IDE_TEST( qdpRole::checkDDLAlterPSMPriv( aStatement,
                                             sParseTree->userID )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NOT_EXIST_PKG_NAME );
    {
        (void)sqlInfo.init( aStatement->qmeMem );
        IDE_SET(
            ideSetErrorCode( qpERR_ABORT_QSV_NOT_EXIST_PKG,
                             sqlInfo.getErrMessage() ) );
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION(ERR_NOT_EXIST_PKG_BODY_NAME);
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode( qpERR_ABORT_QSV_NOT_EXIST_PKG_BODY,
                             sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qsv::validateDropPkg( qcStatement * aStatement )
{
    qsAlterPkgParseTree * sParseTree;
    qcuSqlSourceInfo      sqlInfo;
    idBool                sIsUsed = ID_FALSE;

    sParseTree = ( qsAlterPkgParseTree * )( aStatement->myPlan->parseTree );

    // 1. check userID and pkgOID
    IDE_TEST( checkUserAndPkg(
                  aStatement,
                  sParseTree->userNamePos,
                  sParseTree->pkgNamePos,
                  &( sParseTree->userID ),
                  &( sParseTree->specOID ),
                  &( sParseTree->bodyOID ) )
              != IDE_SUCCESS );

    if( sParseTree->specOID == QS_EMPTY_OID )
    {
        sqlInfo.setSourceInfo( aStatement,
                               & sParseTree->pkgNamePos );
        IDE_RAISE( ERR_NOT_EXIST_PKG_NAME );
    }
    else
    {
        if( sParseTree->option == QS_PKG_BODY_ONLY )
        {
            if( sParseTree->bodyOID== QS_EMPTY_OID )
            {
                sqlInfo.setSourceInfo( aStatement,
                                       & sParseTree->pkgNamePos );
                IDE_RAISE( ERR_NOT_EXIST_PKG_NAME );
            }
        }
    }

    // 2. check grant
    IDE_TEST( qdpRole::checkDDLDropPSMPriv( aStatement,
                                            sParseTree->userID )
              != IDE_SUCCESS );

    /* BUG-35445 Check Constraint, Function-Based Index���� ��� ���� Function�� ����/���� ���� */
    IDE_TEST( qcmProc::relIsUsedProcByConstraint(
                  aStatement,
                  &(sParseTree->pkgNamePos),
                  sParseTree->userID,
                  &sIsUsed )
              != IDE_SUCCESS );
    IDE_TEST_RAISE( sIsUsed == ID_TRUE, ERR_PROC_USED_BY_CONSTRAINT );

    IDE_TEST( qcmProc::relIsUsedProcByIndex(
                  aStatement,
                  &(sParseTree->pkgNamePos),
                  sParseTree->userID,
                  &sIsUsed )
              != IDE_SUCCESS );
    IDE_TEST_RAISE( sIsUsed == ID_TRUE, ERR_PROC_USED_BY_INDEX );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NOT_EXIST_PKG_NAME );
    {
        (void)sqlInfo.init( aStatement->qmeMem );
        IDE_SET(
            ideSetErrorCode( qpERR_ABORT_QSV_NOT_EXIST_PKG,
                             sqlInfo.getErrMessage() ) );
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION( ERR_PROC_USED_BY_CONSTRAINT );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QSV_PROC_USED_BY_CONSTRAINT ) );
    }
    IDE_EXCEPTION( ERR_PROC_USED_BY_INDEX );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QSV_PROC_USED_BY_INDEX ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* PROJ-1073 Package */
IDE_RC qsv::parseExecPkgAssign ( qcStatement * aQcStmt )
{
    qsExecParseTree * sParseTree;
    idBool            sExist = ID_FALSE;
    qcmSynonymInfo    sSynonymInfo;
    qcuSqlSourceInfo  sqlInfo;
    qsxPkgInfo      * sPkgInfo;
    idBool            sIsVariable = ID_FALSE;
    qsVariables     * sVariable   = NULL;

    IDE_FT_BEGIN();

    IDU_FIT_POINT_FATAL( "qsv::parseExecPkgAssign::__FT__" );

    sParseTree = (qsExecParseTree*)aQcStmt->myPlan->parseTree;

    /* BUG-45994 */
    IDU_FIT_POINT_FATAL( "qsv::parseExecPkgAssign::__FT__::STAGE1" );

    IDE_TEST( qcmSynonym::resolvePkg(
                  aQcStmt,
                  sParseTree->leftNode->userName,
                  sParseTree->leftNode->tableName,
                  &(sParseTree->procOID),
                  &(sParseTree->userID),
                  &sExist,
                  &sSynonymInfo)
              != IDE_SUCCESS );

    if( sExist == ID_TRUE )
    {
        // initialize section
        // synonym���� �����Ǵ� proc�� ���
        IDE_TEST( qsvPkgStmts::makePkgSynonymList(
                      aQcStmt,
                      & sSynonymInfo,
                      sParseTree->leftNode->userName,
                      sParseTree->leftNode->tableName,
                      sParseTree->procOID )
                  != IDE_SUCCESS );

        IDE_TEST( qsxRelatedProc::prepareRelatedPlanTree( aQcStmt,
                                                          sParseTree->procOID,
                                                          QS_PKG,
                                                          &(aQcStmt->spvEnv->procPlanList) )
                  != IDE_SUCCESS );

        IDE_TEST( qsxPkg::getPkgInfo( sParseTree->procOID,
                                      &sPkgInfo )
                  != IDE_SUCCESS );

        // 1) spec�� package info�� �������� ����.
        IDE_TEST( sPkgInfo == NULL );

        /* BUG-45164 */
        IDE_TEST_RAISE( sPkgInfo->isValid != ID_TRUE, err_object_invalid );

        /* BUG-38720
           package body ���� ���� Ȯ�� �� body�� qsxPkgInfo�� �����´�. */
        IDE_TEST( qcmPkg::getPkgExistWithEmptyByName(
                      aQcStmt,
                      sParseTree->userID,
                      sParseTree->leftNode->tableName,
                      QS_PKG_BODY,
                      &(sParseTree->pkgBodyOID) )
                  != IDE_SUCCESS );

        IDE_TEST( qdpRole::checkDMLExecutePSMPriv( aQcStmt,
                                                   sPkgInfo->planTree->userID,
                                                   sPkgInfo->privilegeCount,
                                                   sPkgInfo->granteeID,
                                                   ID_FALSE,
                                                   NULL,
                                                   NULL )
                  != IDE_SUCCESS );

        // PROJ-2533
        // exec pkg1.func(10) := 1 �� ���� subprogram�� left node�� �� �� ����.
        IDE_TEST( qsvProcVar::searchPkgVariable( aQcStmt,
                                                 sPkgInfo,
                                                 sParseTree->leftNode,
                                                 &sIsVariable,
                                                 &sVariable )
                  != IDE_SUCCESS );

        IDE_TEST_RAISE( sIsVariable == ID_FALSE,
                        ERR_NOT_EXIST_SUBPROGRAM );

        IDU_LIST_INIT( &sParseTree->refCurList );
    }
    else
    {
        // left node�� ã�� ���� ��� invalid identifier error�� �߻��Ѵ�.
        sqlInfo.setSourceInfo( aQcStmt,
                               & sParseTree->leftNode->position);
        IDE_RAISE( ERR_INVALID_IDENTIFIER );
    }

    IDE_FT_END();

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NOT_EXIST_SUBPROGRAM )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QSX_NOT_EXIST_SUBPROGRAM_AND_VARIABLE ) );
    }
    IDE_EXCEPTION( ERR_INVALID_IDENTIFIER );
    {
        (void)sqlInfo.init( aQcStmt->qmeMem );
        IDE_SET(
            ideSetErrorCode( qpERR_ABORT_QSV_INVALID_IDENTIFIER,
                             sqlInfo.getErrMessage() ) );
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION( err_object_invalid );   /* BUG-45164 */
    {
        IDE_SET( ideSetErrorCode(qpERR_ABORT_QSX_PLAN_INVALID) );
    }
    IDE_EXCEPTION_SIGNAL()
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_FAULT_TOLERATED ) );
    }
    IDE_EXCEPTION_END;

    IDE_FT_EXCEPTION_BEGIN();

    // BUG-38883 print error position
    if ( ideHasErrorPosition() == ID_FALSE )
    {
        sqlInfo.setSourceInfo( aQcStmt,
                               &sParseTree->leftNode->position);

        // set original error code.
        qsxEnv::setErrorCode( QC_QSX_ENV(aQcStmt) );

        (void)sqlInfo.initWithBeforeMessage(
            QC_QME_MEM(aQcStmt) );

        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QSX_SQLTEXT_WRAPPER,
                            sqlInfo.getBeforeErrMessage(),
                            sqlInfo.getErrMessage()));
        (void)sqlInfo.fini();

        // set sophisticated error message.
        qsxEnv::setErrorMessage( QC_QSX_ENV(aQcStmt) );
    }
    else
    {
        // Nothing to do.
    }

    IDE_FT_EXCEPTION_END();

    return IDE_FAILURE;
}

/* PROJ-1073 Package */
IDE_RC qsv::validateExecPkgAssign ( qcStatement * aQcStmt )
{
    qsExecParseTree * sParseTree;
    qsxPkgInfo      * sPkgInfo     = NULL;
    qsxPkgInfo      * sPkgBodyInfo = NULL;
    qsxProcPlanList   sPlan;

    sParseTree = (qsExecParseTree*)aQcStmt->myPlan->parseTree;

    sParseTree->leftNode->lflag |= QTC_NODE_LVALUE_ENABLE;

    // exec pkg1.v1 := 100;
    IDE_TEST( qtc::estimate(
                  sParseTree->leftNode,
                  QC_SHARED_TMPLATE( aQcStmt ),
                  aQcStmt,
                  NULL,
                  NULL,
                  NULL )
              != IDE_SUCCESS );

    // BUG-42790
    // left�� ���� ������  column ����̰ų�, host ����������.
    IDE_ERROR_RAISE( ( ( sParseTree->leftNode->node.module ==
                         &qtc::columnModule ) ||
                       ( sParseTree->leftNode->node.module ==
                         &qtc::valueModule ) ),
                     ERR_UNEXPECTED_MODULE_ERROR );

    IDE_TEST( qtc::estimate(
                  sParseTree->callSpecNode,
                  QC_SHARED_TMPLATE( aQcStmt ),
                  aQcStmt,
                  NULL,
                  NULL,
                  NULL )
              != IDE_SUCCESS );

    // get parameter declaration
    IDE_TEST( qsxPkg::getPkgInfo( sParseTree->procOID,
                                  &sPkgInfo )
              != IDE_SUCCESS);

    // check grant
    IDE_TEST( qdpRole::checkDMLExecutePSMPriv( aQcStmt,
                                               sPkgInfo->planTree->userID,
                                               sPkgInfo->privilegeCount,
                                               sPkgInfo->granteeID,
                                               ID_FALSE,
                                               NULL,
                                               NULL)
              != IDE_SUCCESS );

    if ( sParseTree->pkgBodyOID != QS_EMPTY_OID )
    {
        IDE_TEST( qsxPkg::getPkgInfo( sParseTree->pkgBodyOID,
                                      &sPkgBodyInfo )
                  != IDE_SUCCESS );

        sPlan.pkgBodyOID         = sPkgBodyInfo->pkgOID;
        sPlan.pkgBodyModifyCount = sPkgBodyInfo->modifyCount;
    }
    else
    {
        sPlan.pkgBodyOID         = QS_EMPTY_OID;
        sPlan.pkgBodyModifyCount = 0;
    }

    // BUG-36973
    sPlan.objectID     = sPkgInfo->pkgOID;
    sPlan.modifyCount  = sPkgInfo->modifyCount;
    sPlan.objectType   = sPkgInfo->objType;
    sPlan.planTree     = (qsPkgParseTree *)sPkgInfo->planTree;
    sPlan.next = NULL;

    IDE_TEST( qcgPlan::registerPlanPkg( aQcStmt, &sPlan ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_UNEXPECTED_MODULE_ERROR )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qsv::validateExecPkgAssign",
                                  "The module is unexpected" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qsv::checkMyPkgSubprogramCall( qcStatement * aStatement,
                                      idBool      * aMyPkgSubprogram,
                                      idBool      * aRecursiveCall )
{
/*******************************************************************************
 *
 * Description :
 *    create package �� subprogram�� ���� ���ȣ������ �Ǵ�
 *    ������ package�� �����ϴ� subprogram�� ȣ���ϴ� �˻�
 *
 * Implementation :
 *    1. ���ȣ������ �˻�
 *      a. qsExecParseTree->callSpecNode->columnName��
 *         aStatement->spvEnv->createProc->procNamePos�� �����ϸ� ���ȣ��
 *    2. ���ȣ���� �ƴϸ�, ������ package ���� subprogram ȣ���ϴ��� �˻�
 *      a. public subprogram���� �˻�
 *         ( ��, package spec�� ����Ǿ� �ִ� subprogram �˻� )
 *        => public subprogram�� ȣ���ϴ� ���,
 *           qsProcStmtSql->usingSubprograms�� �޾Ƴ��´�.
 *      b. a�� ���� ���, private subprogram�� �����ϴ� �˻�
 *         ( package body�� ���ǵ� subprogram )
 ******************************************************************************/

    qsExecParseTree  * sExeParseTree        = NULL;
    qsPkgParseTree   * sPkgParseTree        = NULL;
    qsPkgStmts       * sCurrStmt            = NULL;
    UInt               sSubprogramID        = QS_PSM_SUBPROGRAM_ID;
    qsPkgSubprogramType sSubprogramType     = QS_NONE_TYPE;
    idBool             sIsInitializeSection = ID_FALSE;
    idBool             sIsPossible          = ID_FALSE;
    idBool             sExist               = ID_FALSE;
    idBool             sIsRecursiveCall     = ID_FALSE;
    UInt               sErrCode;

    *aRecursiveCall      = ID_FALSE;
    *aMyPkgSubprogram    = ID_FALSE;
    sExeParseTree        = (qsExecParseTree *)(aStatement->myPlan->parseTree);
    sPkgParseTree        = aStatement->spvEnv->createPkg;
    sCurrStmt            = aStatement->spvEnv->currSubprogram;
    sIsInitializeSection = aStatement->spvEnv->isPkgInitializeSection;

    IDE_DASSERT( QC_IS_NULL_NAME( sExeParseTree->callSpecNode->columnName) != ID_TRUE );

    if ( QC_IS_NULL_NAME( sExeParseTree->callSpecNode->userName ) != ID_TRUE )
    {
        IDE_TEST( qcmUser::getUserID(aStatement,
                                     sExeParseTree->callSpecNode->userName,
                                     & ( sExeParseTree->userID ) )
                  != IDE_SUCCESS );
    }
    else /* sExeParseTree->callSpecNode->userName is NULL */
    {
        sExeParseTree->userID = QCG_GET_SESSION_USER_ID( aStatement );
    }

    if ( sExeParseTree->userID == sPkgParseTree->userID )
    {
        if ( QC_IS_NULL_NAME( sExeParseTree->callSpecNode->tableName ) != ID_TRUE )
        {
            if ( QC_IS_NAME_MATCHED( sExeParseTree->callSpecNode->tableName, sPkgParseTree->pkgNamePos ) )
            {
                sIsPossible = ID_TRUE;
            }
            else
            {
                sIsPossible = ID_FALSE;
            }
        }
        else /* sExeParseTree->callSpecNode->tableName is NULL */
        {
            sIsPossible = ID_TRUE;
        }
    }
    else
    {
        sIsPossible = ID_FALSE;
    }

    IDE_TEST_CONT( sIsPossible == ID_FALSE , IS_NOT_SAME_PACKAGE_OR_IS_RECURSIVECALL );

    // BUG-41262 PSM overloading
    if ( qsvPkgStmts::getMyPkgSubprogramID(
             aStatement,
             sExeParseTree->callSpecNode,
             &sSubprogramType,
             &sSubprogramID ) == IDE_SUCCESS )
    {
        IDE_TEST_CONT( sSubprogramID == QS_PSM_SUBPROGRAM_ID,
                       IS_NOT_SAME_PACKAGE_OR_IS_RECURSIVECALL );

        /* BUG-41847 package�� subprogram�� default value�� ��� �����ϰ� ���� */
        /* package spec�� ����� varialbe�� default�� �� �� �ִ� subprogram
           - �ٸ� package subprogram */
        IDE_TEST_CONT( (sPkgParseTree->objType == QS_PKG) &&
                       (sIsInitializeSection == ID_FALSE) &&
                       (sCurrStmt == NULL),
                       IS_NOT_SAME_PACKAGE_OR_IS_RECURSIVECALL );

        /* package body�� ����� varialbe�� default�� �� �� �ִ� subprogram
           - �ٸ� package subprogram
           - ���� package�� public subprogram */
        IDE_TEST_CONT( (sPkgParseTree->objType == QS_PKG_BODY) &&
                       (sIsInitializeSection == ID_FALSE) &&
                       (sCurrStmt == NULL) &&
                       (sSubprogramType == QS_PRIVATE_SUBPROGRAM),
                       IS_NOT_SAME_PACKAGE_OR_IS_RECURSIVECALL );

        // RecursiveCall
        if ( (sIsInitializeSection != ID_TRUE) &&
             (qsvPkgStmts::checkIsRecursiveSubprogramCall(
                 sCurrStmt,
                 sSubprogramID )
              == ID_TRUE ) )
        {
            sIsRecursiveCall = ID_TRUE;
            sExeParseTree->subprogramID
                = ( (qsPkgSubprograms *) sCurrStmt )->subprogramID;
        }
        else
        {
            if ( sSubprogramType == QS_PUBLIC_SUBPROGRAM )
            {
                // BUG-37333
                IDE_TEST( qsvPkgStmts::makeUsingSubprogramList( aStatement,
                                                                sExeParseTree->callSpecNode )
                          != IDE_SUCCESS );
            }
            else
            {
                IDE_DASSERT( sSubprogramType != QS_NONE_TYPE );
            }
        }

        sExist                  = ID_TRUE;
        sExeParseTree->procOID  = QS_EMPTY_OID ;
    }
    else
    {
        sErrCode = ideGetErrorCode();

        IDE_CLEAR();

        if ( sErrCode == qpERR_ABORT_QSV_TOO_MANY_MATCH_THIS_CALL )
        {
            sExist = ID_TRUE;
        }
        else
        {
            // Nothing to do.
        }
    }

    /* BUG-37235, BUG-41847
       default value�� subprogram name�� ������� ���, ���� �� default value�� ã�� �� �Ѵ�.
       ����, default statement�� ���� ��, package name�� ��� �� ��� �Ѵ�. */
    if ( (sExist == ID_TRUE) &&
         ((sExeParseTree->callSpecNode->lflag & QTC_NODE_SP_PARAM_DEFAULT_VALUE_MASK)
          == QTC_NODE_SP_PARAM_DEFAULT_VALUE_TRUE) )
    {
        sExeParseTree->callSpecNode->lflag &= ~QTC_NODE_PKG_VARIABLE_MASK;
        sExeParseTree->callSpecNode->lflag |= QTC_NODE_PKG_VARIABLE_TRUE;
    }
    else
    {
        // Nothing to do.
    }

    IDE_EXCEPTION_CONT( IS_NOT_SAME_PACKAGE_OR_IS_RECURSIVECALL );

    *aMyPkgSubprogram = sExist;
    *aRecursiveCall   = sIsRecursiveCall;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qsv::validateNamedArguments( qcStatement     * aStatement,
                                    qtcNode         * aCallSpec,
                                    qsVariableItems * aParaDecls,
                                    UInt              aParaDeclCnt,
                                    qsvArgPassInfo ** aArgPassArray )
{
/****************************************************************************
 * 
 * Description : BUG-41243 Name-based Argument Passing
 *               Name-based Argument �� ������ �ϰ�, ������ ���ġ�Ѵ�.
 *
 *    Argument Passing���� 3������ �ִ�.
 *
 *    1. Positional Argument Passing (���� ���)
 *       - Parameter ������� Argument ���� ����
 *       - ������ �ٲ�� ����� �ǵ��� �ٸ��� �۵�
 *         (e.g.) exec PROC1( 1, 'aa', :a );
 *
 *    2. Name-based Argument Passing
 *       - Parameter �̸��� ����� Argument ���� ����
 *       - ������ �ٲ� �������
 *         (e.g.) exec PROC1( P2=>'aa', P3=>:a, P1=>1 );
 *
 *    3. Mixed Argument Passing
 *       - �� ����� ȥ����
 *       - �ݵ�� Positional���� �Ϻ� ������ �� Name-based�� �������� ����
 *         (e.g.) exec PROC1( 1, P3=>:a, P2=>'aa' );
 *
 *
 *    �߻��� �� �ִ� ������ ������ ����.
 *
 *    - Positional > Name-based ������ ���� �����ϴ� ���
 *      >> exec PROC1( 1, P3=>:a, 'aa' ); -- �ٽ� Positional Passing
 *                                ^  ^
 *    - �߸��� Parameter �̸� ����
 *      >> exec PROC1( P2=>'aa', P3=>:a, P100=>1 ); -- P100�� ����
 *                                       ^  ^
 *    - �̹� Argument�� ������ Parameter�� �ߺ� �����ϴ� ���
 *      >> exec PROC1( 1, P1=>3, :a ); -- �̹� P1�� Argument 1�� ����
 *                        ^^
 *
 * Implementation :
 *
 *  - ���ǵ� Parameter�� ���� ���θ� ������ Array�� �Ҵ� (RefArray)
 *
 *  - CallSpecNode�� ���󰡸鼭,
 *    - ������ ���(Positional)�� ������� RefArray �� ����
 *    - ����� ���(Named)�� ParaDecls�� Ž���� �ش� ��ġ RefArray �� ����
 *
 *  - CallSpecNode���� Named ������ List�� Tmp List�� ����
 *  - ������ �̹� �� ParaDecls�� ���󰡸鼭,
 *    - Tmp List�� �˸��� qtcNode �߰�
 *    - �ش� qtcNode ���� Node�� CallSpecNode�� �̾� ����
 *      (�ش� qtcNode�� Name Node�̹Ƿ�, ���� Node�� Argument Node)
 *
 * Note :
 *
 *   - ��� ������ ��, �������� ���� ParaDecls�� �߰ߵ� �� ������
 *     Argument Validation ������ �۾����� �ɷ��� ���̴�.
 *   - RefArray��, Default Parameter ó�� �� ���� ����ؾ� �ϹǷ� ��ȯ�Ѵ�.
 *    
 ****************************************************************************/

    qsvArgPassInfo  * sArgPassArray   = NULL;
    qtcNode         * sNewArgs        = NULL;
    qtcNode         * sCurrArg        = NULL;
    qsVariableItems * sParaDecl       = NULL;
    idBool            sIsNameFound    = ID_FALSE;
    UInt              sArgCount       = 0;
    UInt              i;
    qcuSqlSourceInfo  sqlInfo;

    IDU_FIT_POINT_FATAL( "qsv::validateNamedArguments::__FT__" );

    // ���ǵ� Parameter�� ������ �ǳʶڴ�.
    IDE_TEST_CONT( aParaDeclCnt == 0, NO_NAMED_ARGUMENT );

    /****************************************************************
     * Argument Passing ���θ� ������ �迭 �Ҵ�
     ***************************************************************/

    IDE_TEST( QC_QME_MEM(aStatement)->cralloc(
                  ID_SIZEOF(qsvArgPassInfo) * aParaDeclCnt,
                  (void**)& sArgPassArray )
              != IDE_SUCCESS );
    
    /****************************************************************
     * Positional Argument�� RefArray ����
     ***************************************************************/
    
    for ( i = 0, sCurrArg = (qtcNode *)(aCallSpec->node.arguments);
          sCurrArg != NULL;
          i++, sCurrArg = (qtcNode *)(sCurrArg->node.next) )
    {
        if ( ( sCurrArg->lflag & QTC_NODE_SP_PARAM_NAME_MASK )
             == QTC_NODE_SP_PARAM_NAME_FALSE )
        {
            // Argument ������ �������.
            sArgCount++;

            // Argument ������ aParaDeclCnt�� �Ѵ� ��� ����
            IDE_TEST_RAISE( sArgCount > aParaDeclCnt, TOO_MANY_ARGUMENT );

            // PassInfo�� PassType�� POSITIONAL�� ����
            sArgPassArray[i].mType = QSV_ARG_PASS_POSITIONAL;
            // PassInfo�� PassArg�� sCurrArg �״�� ����
            sArgPassArray[i].mArg  = sCurrArg;
        }
        else
        {
            // Named Argument�� �����Ѵٴ� �� ����ϰ�, ���� �ܰ�� �Ѿ��.
            sIsNameFound = ID_TRUE;
            break;
        }
    }

    // Name-based Argument�� ������ �ǳʶڴ�.
    IDE_TEST_CONT( sIsNameFound == ID_FALSE, NO_NAMED_ARGUMENT );

    /****************************************************************
     * Name-based Argument�� RefArray ����
     ***************************************************************/

    while ( sCurrArg != NULL )
    {
        // Named Argument�� ���;� �ϴµ�
        // �ٽ� Positional Argument�� ������ ����.
        // (e.g.) exec proc1( 1, P2=>3, 3 );
        IDE_TEST_RAISE( ( sCurrArg->lflag & QTC_NODE_SP_PARAM_NAME_MASK )
                        == QTC_NODE_SP_PARAM_NAME_FALSE,
                        ARGUMENT_DOUBLE_MIXED_REFERNECE );

        // Argument ������ �������.
        sArgCount++;

        // Argument ������ aParaDeclCnt�� �Ѵ� ��� ����
        IDE_TEST_RAISE( sArgCount > aParaDeclCnt, TOO_MANY_ARGUMENT );

        // Named Argument�� Name�� ������ ParaDecls�� �˻��Ѵ�.
        for ( i = 0, sParaDecl = aParaDecls;
              sParaDecl != NULL;
              i++, sParaDecl = sParaDecl->next )
        {
            if ( QC_IS_NAME_MATCHED( sParaDecl->name, sCurrArg->position ) )
            {
                // i���� �׻� Parameter �������� ����� �Ѵ�.
                IDE_DASSERT( i < aParaDeclCnt );

                // Named Argument�� ����Ű�� �ִ� Parameter��
                // �̹� �ٸ� Argument�� ����Ű�� �ִ� ��� ����.
                IDE_TEST_RAISE( sArgPassArray[i].mType != QSV_ARG_NOT_PASSED,
                                ARGUMENT_DUPLICATE_REFERENCE );

                // PassInfo�� PassType�� NAMED�� ����
                sArgPassArray[i].mType = QSV_ARG_PASS_NAMED;
                // PassInfo�� PassArg�� sCurrArg ���� Node�� ����
                sArgPassArray[i].mArg  = (qtcNode *)(sCurrArg->node.next);

                break;
            }
            else
            {
                // Nothing to do.
            }
        }

        // ��ġ�ϴ� Parameter Name�� ������ ���� ��� ����.
        IDE_TEST_RAISE( sParaDecl == NULL, ARGUMENT_WRONG_REFERENCE );

        // ���� Node�� �������� ���� �� ����.
        IDE_DASSERT( sCurrArg->node.next != NULL );

        // �������� Argument�� �̵� = ���� Argument�� Name Node�� �̵�
        sCurrArg = (qtcNode *)(sCurrArg->node.next->next);
    }

    /****************************************************************
     * Name-based Argument�� ��ġ ������
     ***************************************************************/
    
    i        = 0;
    sNewArgs = NULL;
    sCurrArg = NULL;
    
    while ( i < aParaDeclCnt )
    {
        if ( sArgPassArray[i].mArg == NULL )
        {
            IDE_DASSERT( sArgPassArray[i].mType == QSV_ARG_NOT_PASSED );

            // Nothing to do.
        }
        else
        {
            if ( sNewArgs == NULL )
            {
                sNewArgs = sArgPassArray[i].mArg;
                sCurrArg = sNewArgs;
            }
            else
            {
                sCurrArg->node.next = (mtcNode *)sArgPassArray[i].mArg;
                sCurrArg            = (qtcNode *)sCurrArg->node.next;

                // next Node�� NULL�� �ʱ�ȭ�ؾ� �Ѵ�.
                sCurrArg->node.next = NULL;
            }
        }

        i++;
    }

    // �翬���� ����Ʈ�� ����
    aCallSpec->node.arguments = (mtcNode *)sNewArgs;

    IDE_EXCEPTION_CONT( NO_NAMED_ARGUMENT );

    // ParaRefArr ��ȯ
    if ( aArgPassArray != NULL )
    {
        *aArgPassArray = sArgPassArray;
    }
    else
    {
        // Nothing to do.
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ARGUMENT_DOUBLE_MIXED_REFERNECE )
    {
        (void)sqlInfo.setSourceInfo( aStatement, & sCurrArg->position );
        (void)sqlInfo.init( aStatement->qmeMem );
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QSV_NORMAL_PARAM_PASSING_AFTER_NAMED_NOTATION,
                                  sqlInfo.getErrMessage() ) );
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION( ARGUMENT_DUPLICATE_REFERENCE )
    {
        (void)sqlInfo.setSourceInfo( aStatement, & sCurrArg->position );
        (void)sqlInfo.init( aStatement->qmeMem );
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QSV_DUPLICATE_PARAM_NAME_NOTATION,
                                  sqlInfo.getErrMessage() ) );
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION( ARGUMENT_WRONG_REFERENCE )
    {
        (void)sqlInfo.setSourceInfo( aStatement, & sCurrArg->position );
        (void)sqlInfo.init( aStatement->qmeMem );
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QSV_WRONG_PARAM_NAME_NOTATION,
                                  sqlInfo.getErrMessage() ) );
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION( TOO_MANY_ARGUMENT )
    {
        (void)sqlInfo.setSourceInfo( aStatement, & aCallSpec->position );
        (void)sqlInfo.init( aStatement->qmeMem );
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QSV_TOO_MANY_ARGUEMNT_SQLTEXT,
                                  sqlInfo.getErrMessage() ) );
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC qsv::setPrecisionAndScale( qcStatement * aStatement,
                                  qsVariables * aParamAndReturn )
{
/*****************************************************************************
 * Description :
 *     PROJ-2586 PSM Parameters and return without precision
 *
 *     �Ʒ��� type�� ���ؼ� Precision, Scale �� Size�� default ������ ����
 *         - CHAR( M )
 *         - VARCHAR( M )
 *         - NCHAR( M )
 *         - NVARCHAR( M )
 *         - BIT( M )
 *         - VARBIT( M )
 *         - BYTE( M )
 *         - VARBYTE( M )
 *         - NIBBLE( M )
 *         - FLOAT[ ( P ) ]
 *         - NUMBER[ ( P [ , S ] ) ]
 *         - NUMERIC[ ( P [ , S ] ) ]
 *         - DECIMAL[ ( P [ , S ] ) ]
 *             ��NUMERIC�� ����
 ****************************************************************************/

    qtcNode   * sNode        = NULL;
    mtcColumn * sColumn      = NULL;
    idBool      sNeedSetting = ID_TRUE;
    SInt        sPrecision   = 0;

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aParamAndReturn != NULL );

    sNode = aParamAndReturn->variableTypeNode;
    sColumn = QTC_TMPL_COLUMN( QC_SHARED_TMPLATE( aStatement ), sNode );

    IDE_DASSERT( sColumn != NULL );
    IDE_DASSERT( sColumn->module != NULL );

    switch ( sColumn->module->id )
    {
        case MTD_CHAR_ID :
            sPrecision = QCU_PSM_CHAR_DEFAULT_PRECISION; 
            break;
        case MTD_VARCHAR_ID :
            sPrecision = QCU_PSM_VARCHAR_DEFAULT_PRECISION; 
            break;
        case MTD_NCHAR_ID :
            if ( mtl::mNationalCharSet->id == MTL_UTF16_ID )
            {
                sPrecision = QCU_PSM_NCHAR_UTF16_DEFAULT_PRECISION; 
            }
            else /* UTF8 */
            {
                /* ������ �ڵ�
                   mtdEstimate���� UTF16 �Ǵ� UTF8�� �ƴϸ� ���� �߻���. */
                IDE_DASSERT( mtl::mNationalCharSet->id == MTL_UTF8_ID );

                sPrecision = QCU_PSM_NCHAR_UTF8_DEFAULT_PRECISION; 
            }
            break;
        case MTD_NVARCHAR_ID :
            if ( mtl::mNationalCharSet->id == MTL_UTF16_ID )
            {
                sPrecision = QCU_PSM_NVARCHAR_UTF16_DEFAULT_PRECISION; 
            }
            else /* UTF8 */
            {
                /* ������ �ڵ�
                   mtdEstimate���� UTF16 �Ǵ� UTF8�� �ƴϸ� ���� �߻���. */
                IDE_DASSERT( mtl::mNationalCharSet->id == MTL_UTF8_ID );

                sPrecision = QCU_PSM_NVARCHAR_UTF8_DEFAULT_PRECISION; 
            }
            break;
        case MTD_BIT_ID :
            sPrecision = QS_BIT_PRECISION_DEFAULT; 
            break;
        case MTD_VARBIT_ID :
            sPrecision = QS_VARBIT_PRECISION_DEFAULT; 
            break;
        case MTD_BYTE_ID :
            sPrecision = QS_BYTE_PRECISION_DEFAULT; 
            break;
        case MTD_VARBYTE_ID :
            sPrecision = QS_VARBYTE_PRECISION_DEFAULT; 
            break;
        case MTD_NIBBLE_ID :
            sPrecision = MTD_NIBBLE_PRECISION_MAXIMUM; 
            break;
        case MTD_FLOAT_ID :
        case MTD_NUMBER_ID :
            sPrecision = MTD_FLOAT_PRECISION_MAXIMUM; 
            break;
        case MTD_NUMERIC_ID :
            // DECIMAL type�� NUMERIC type�� ����
            sPrecision = MTD_NUMERIC_PRECISION_MAXIMUM; 
            break;
        default :
            /* ECHAR �Ǵ� EVARCHAR datatype�� ���,
               �ļ����� mtcColumn ���� �� ���� �߻��Ѵ�. */
            IDE_DASSERT( sColumn->module->id != MTD_ECHAR_ID );
            IDE_DASSERT( sColumn->module->id != MTD_EVARCHAR_ID );

            sNeedSetting = ID_FALSE;
    }

    if ( sNeedSetting == ID_TRUE )
    {
        IDE_TEST( mtc::initializeColumn( sColumn,
                                         sColumn->module,
                                         1,
                                         sPrecision,
                                         0 )
                  != IDE_SUCCESS );

        sColumn->flag &= ~MTC_COLUMN_SP_SET_PRECISION_MASK;
        sColumn->flag |= MTC_COLUMN_SP_SET_PRECISION_TRUE;
    }
    else
    {
        // Nothing to do.
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

// BUG-42322
IDE_RC qsv::setObjectNameInfo( qcStatement        * aStatement,
                               qsObjectType         aObjectType,
                               UInt                 aUserID,
                               qcNamePosition       aObjectNamePos,
                               qsObjectNameInfo   * aObjectInfo )
{
    SChar           sBuffer[QC_MAX_OBJECT_NAME_LEN * 2 + 2];
    SInt            sObjectTypeLen = 0;
    SInt            sNameLen       = 0;

    IDE_DASSERT( aObjectNamePos.stmtText != NULL );
    IDE_DASSERT( aObjectInfo != NULL );

    switch( aObjectType )
    {
        case QS_PROC:
            sObjectTypeLen = idlOS::strlen("procedure");
            IDE_TEST( QC_QMP_MEM(aStatement)->alloc( sObjectTypeLen + 1,
                                                     (void**)&aObjectInfo->objectType )
                      != IDE_SUCCESS);
            idlOS::memcpy( aObjectInfo->objectType , "procedure" , sObjectTypeLen );
            aObjectInfo->objectType[sObjectTypeLen] = '\0';
            break;
        case QS_FUNC:
            sObjectTypeLen = idlOS::strlen("function");
            IDE_TEST( QC_QMP_MEM(aStatement)->alloc( sObjectTypeLen + 1,
                                                     (void**)&aObjectInfo->objectType )
                      != IDE_SUCCESS);
            idlOS::memcpy( aObjectInfo->objectType , "function" , sObjectTypeLen );
            aObjectInfo->objectType[sObjectTypeLen] = '\0';
            break;
        case QS_TYPESET:
            sObjectTypeLen = idlOS::strlen("typeset");
            IDE_TEST( QC_QMP_MEM(aStatement)->alloc( sObjectTypeLen + 1,
                                                     (void**)&aObjectInfo->objectType )
                      != IDE_SUCCESS);
            idlOS::memcpy( aObjectInfo->objectType , "typeset" , sObjectTypeLen );
            aObjectInfo->objectType[sObjectTypeLen] = '\0';
            break;
        case QS_PKG:
            sObjectTypeLen = idlOS::strlen("package");
            IDE_TEST( QC_QMP_MEM(aStatement)->alloc( sObjectTypeLen + 1,
                                                     (void**)&aObjectInfo->objectType )
                      != IDE_SUCCESS);
            idlOS::memcpy( aObjectInfo->objectType , "package" , sObjectTypeLen );
            aObjectInfo->objectType[sObjectTypeLen] = '\0';
            break;
        case QS_PKG_BODY:
            sObjectTypeLen = idlOS::strlen("package body");
            IDE_TEST( QC_QMP_MEM(aStatement)->alloc( sObjectTypeLen + 1,
                                                     (void**)&aObjectInfo->objectType )
                      != IDE_SUCCESS);
            idlOS::memcpy( aObjectInfo->objectType , "package body" , sObjectTypeLen );
            aObjectInfo->objectType[sObjectTypeLen] = '\0';
            break;
        case QS_TRIGGER:
            sObjectTypeLen = idlOS::strlen("trigger");
            IDE_TEST( QC_QMP_MEM(aStatement)->alloc( sObjectTypeLen + 1,
                                                     (void**)&aObjectInfo->objectType )
                      != IDE_SUCCESS);
            idlOS::memcpy( aObjectInfo->objectType , "trigger" , sObjectTypeLen );
            aObjectInfo->objectType[sObjectTypeLen] = '\0';
            break;
        default:
            IDE_DASSERT(0);
    }

    idlOS::memset( sBuffer, 0, ID_SIZEOF(sBuffer) );

    IDE_TEST( qcmUser::getUserName( aStatement,
                                    aUserID,
                                    sBuffer )
              != IDE_SUCCESS );

    // USER_NAME
    sNameLen = idlOS::strlen( sBuffer );
    IDE_TEST( QC_QMP_MEM(aStatement)->alloc( sNameLen + 1,
                                             (void**)&aObjectInfo->userName )
              != IDE_SUCCESS);

    idlOS::memcpy( aObjectInfo->userName, sBuffer, sNameLen );
    aObjectInfo->userName[sNameLen] = '\0';

    idlOS::memcpy( sBuffer + idlOS::strlen(sBuffer), ".", 1 );

    QC_STR_COPY( sBuffer + idlOS::strlen(sBuffer), aObjectNamePos );

    sNameLen = idlOS::strlen( sBuffer );

    // USER_NAME.OBJECT_NAME
    IDE_TEST( QC_QMP_MEM(aStatement)->alloc( sNameLen + 1,
                                             (void**)&aObjectInfo->name )
              != IDE_SUCCESS);

    idlOS::memcpy( aObjectInfo->name, sBuffer, sNameLen );
    aObjectInfo->name[sNameLen] = '\0';

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qsv::parseAB(qcStatement * /*aStatement*/ )
{
    return IDE_SUCCESS;

    //IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qsv::validateAB(qcStatement * aStatement )
{
    qsProcParseTree    * sParseTree;

    sParseTree = (qsProcParseTree *)(aStatement->myPlan->parseTree);

    aStatement->spvEnv->createProc = sParseTree;

    sParseTree->userID = QCG_GET_SESSION_USER_ID( aStatement );

    IDE_TEST( setObjectNameInfo( aStatement,
                                 sParseTree->objType,
                                 sParseTree->userID,
                                 sParseTree->procNamePos,
                                 &sParseTree->objectNameInfo )
              != IDE_SUCCESS );

    sParseTree->stmtText    = aStatement->myPlan->stmtText;
    sParseTree->stmtTextLen = aStatement->myPlan->stmtTextLen;

    if ( sParseTree->block->common.parentLabels != NULL )
    {
        sParseTree->block->common.parentLabels->id = qsvEnv::getNextId(aStatement->spvEnv);
    }

    // parsing body
    IDE_TEST( sParseTree->block->common.parse(
                  aStatement, (qsProcStmts *)(sParseTree->block))
              != IDE_SUCCESS);

    // validation body
    IDE_TEST( sParseTree->block->common.
              validate( aStatement,
                        (qsProcStmts *)(sParseTree->block),
                        NULL,
                        QS_PURPOSE_PSM )
              != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

