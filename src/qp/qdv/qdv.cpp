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
 * $Id: qdv.cpp 90270 2021-03-21 23:20:18Z bethy $
 **********************************************************************/

#include <idl.h>
#include <ide.h>
#include <qdv.h>
#include <qdd.h>
#include <qcm.h>
#include <qcg.h>
#include <qmv.h>
#include <qsvEnv.h>
#include <qcmUser.h>
#include <qcmProc.h>
#include <qcmView.h>
#include <qdbCreate.h>
#include <qdbCommon.h>
#include <qdpPrivilege.h>
#include <qcpManager.h>
#include <qcuSqlSourceInfo.h>
#include <qcmPkg.h>
#include <qdpRole.h>
#include <qdnTrigger.h>
#include <qmvShardTransform.h> /* TASK-7219 Shard Transformer Refactoring */

/***********************************************************************
 * PARSE
 **********************************************************************/

IDE_RC qdv::parseCreateViewAsSelect( qcStatement * aStatement )
{
    qdTableParseTree    * sParseTree;
    IDE_RC                sWithClauseParsing;

    sParseTree = (qdTableParseTree *)aStatement->myPlan->parseTree;

    QC_SHARED_TMPLATE(aStatement)->flag &= ~QC_PARSE_CREATE_VIEW_MASK;
    QC_SHARED_TMPLATE(aStatement)->flag |= QC_PARSE_CREATE_VIEW_TRUE;

    /* TASK-7219 Shard Transformer Refactoring */
    sWithClauseParsing = qmv::parseSelectInternal( sParseTree->select );
    
    QC_SHARED_TMPLATE(aStatement)->flag &= ~QC_PARSE_CREATE_VIEW_MASK;
    QC_SHARED_TMPLATE(aStatement)->flag |= QC_PARSE_CREATE_VIEW_FALSE;

    if ( sWithClauseParsing != IDE_SUCCESS )
    {
        if ( ( sParseTree->flag & QDV_OPT_FORCE_MASK ) == QDV_OPT_FORCE_FALSE )
        {
            // NO FORCE
            // A error is set in qmv::parseSelect
            IDE_RAISE( ERR_PASS );
        }
        else
        {
            // FORCE
            // set STATUS
            sParseTree->flag &= ~QDV_OPT_STATUS_MASK;
            sParseTree->flag |= QDV_OPT_STATUS_INVALID;
        }
    }
    else
    {
        /* nothing to do. */
    }

    /* TASK-7219 Shard Transformer Refactoring */
    IDE_TEST( qmvShardTransform::doTransform( sParseTree->select )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_PASS );

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * VALIDATE
 **********************************************************************/

IDE_RC qdv::validateCreate(qcStatement * aStatement)
{
/***********************************************************************
 *
 * Description :
 *    CREATE VIEW ... �� validation ����
 *
 * Implementation :
 *    1. �䰡 �����ϴ��� üũ ( replace �̸� ���� �� ����, create �̸�
 *       ������ �� ���� )
 *    2. create or replace �� �� execution �Լ��� qdv::executeRecreate �� ����
 *    2. CreateView ������ �ִ��� üũ
 *    3. SELECT statement �� ���� validation ����( validation �����
 *       ������ �־ FORCE �ɼ����� �����Ѵٸ� ��� ���� )
 *    4. select ���� sequence( currval, nextval ) �� ���Ǿ����� ����
 *    5. ���� �÷� aliases �� select �� �÷��� validation ...
 *    6. in case of INVALID VIEW => qtc::fixAfterValidationForCreateInvalidView
 *       => invalid view �� ���� �ʱ�ȭ ó��
 *    7. create or replace �� �� as select ���� ���Ǵ� ���̺�(��)�� �����ϴ�
 *       ���� �̸��� �������� ������ üũ
 *
 ***********************************************************************/

#define IDE_FN "qdv::validateCreate"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qdv::validateCreate"));

    qdTableParseTree    * sParseTree;
    IDE_RC                sTableExist;
    IDE_RC                sPrivilegeExist;
    IDE_RC                sSelectValidation;
    IDE_RC                sColumnValidation;
    idBool                sCircularViewExist = ID_FALSE;
    qsRelatedObjects    * sRelatedObject;
    UInt                  sRelatedObjectUserID;
    qcuSqlSourceInfo      sqlInfo;
    idBool                sExist = ID_FALSE;
    qsOID                 sProcID;
    volatile UInt         sSessionUserID;
    SChar                 sViewName[QC_MAX_OBJECT_NAME_LEN + 1];

    IDE_FT_BEGIN();

    IDU_FIT_POINT_FATAL( "qdv::validateCreate::__FT__" );

    // ���� session userID ����
    sSessionUserID = QCG_GET_SESSION_USER_ID( aStatement );

    IDU_FIT_POINT_FATAL( "qdv::validateCreate::__FT__::SessionUserID" );

    sParseTree = (qdTableParseTree *)aStatement->myPlan->parseTree;

    /* BUG-30059 */
    if ( qdbCommon::containDollarInName( &(sParseTree->tableName) ) == ID_TRUE )
    {
        sqlInfo.setSourceInfo(
            aStatement,
            &(sParseTree->tableName) );

        IDE_RAISE( CANT_USE_RESERVED_WORD );
    }

    //------------------------------------------------------------------
    // validation of view name
    //------------------------------------------------------------------

    if ( (sParseTree->flag & QDV_OPT_REPLACE_MASK ) == QDV_OPT_REPLACE_FALSE )
    {
        //----------------------------
        //  CREATE VIEW
        //----------------------------

        // check view exist.
        if (gQcmSynonyms == NULL)
        {
            // in createdb phase -> no synonym meta.
            // so skip check duplicated name from synonym, psm
            IDE_TEST( qdbCommon::checkDuplicatedObject(
                          aStatement,
                          sParseTree->userName,
                          sParseTree->tableName,
                          &(sParseTree->userID) )
                      != IDE_SUCCESS );
        }
        else
        {
            IDE_TEST(
                qcm::existObject(
                    aStatement,
                    ID_FALSE,
                    sParseTree->userName,
                    sParseTree->tableName,
                    QS_OBJECT_MAX,
                    &(sParseTree->userID),
                    &sProcID,
                    &sExist)
                != IDE_SUCCESS);

            IDE_TEST_RAISE(sExist == ID_TRUE, ERR_EXIST_OBJECT_NAME);
        }
    }
    else
    {
        //----------------------------
        //  CREATE OR REPLACE VIEW
        //----------------------------
        // check user existence
        if (QC_IS_NULL_NAME(sParseTree->userName) == ID_TRUE)
        {
            sParseTree->userID = QCG_GET_SESSION_USER_ID(aStatement);
        }
        else
        {
            IDE_TEST(qcmUser::getUserID( aStatement,
                                         sParseTree->userName,
                                         &(sParseTree->userID) )
                     != IDE_SUCCESS);
        }

        // check view exist.
        sTableExist = qcm::getTableInfo( aStatement,
                                         sParseTree->userID,
                                         sParseTree->tableName,
                                         &(sParseTree->tableInfo),
                                         &(sParseTree->tableSCN),
                                         &(sParseTree->tableHandle));

        if (sTableExist != IDE_SUCCESS)
        {
            IDE_CLEAR();

            // check view exist.
            if (gQcmSynonyms == NULL)
            {
                // in createdb phase -> no synonym meta.
                // so skip check duplicated name from synonym, psm
                IDE_TEST( qdbCommon::checkDuplicatedObject(
                              aStatement,
                              sParseTree->userName,
                              sParseTree->tableName,
                              &(sParseTree->userID) )
                          != IDE_SUCCESS );
            }
            else
            {
                IDE_TEST(
                    qcm::existObject(
                        aStatement,
                        ID_FALSE,
                        sParseTree->userName,
                        sParseTree->tableName,
                        QS_OBJECT_MAX,
                        &(sParseTree->userID),
                        &sProcID,
                        &sExist)
                    != IDE_SUCCESS);

                IDE_TEST_RAISE(sExist == ID_TRUE, ERR_EXIST_OBJECT_NAME);
            }
        }
        else
        {
            IDE_TEST(qcm::lockTableForDDLValidation(aStatement,
                                                    sParseTree->tableHandle,
                                                    sParseTree->tableSCN)
                     != IDE_SUCCESS);

            // PR-13725
            // CHECK OPERATABLE
            if( QCM_IS_OPERATABLE_QP_CREATE_VIEW(
                    sParseTree->tableInfo->operatableFlag )
                != ID_TRUE )
            {
                IDE_RAISE(ERR_EXIST_OBJECT_NAME);
            }

            /* PROJ-2211 Materialized View */
            IDE_TEST_RAISE( sParseTree->tableInfo->tableType == QCM_MVIEW_VIEW,
                            ERR_EXIST_OBJECT_NAME );

            // re-create
            sParseTree->common.execute = qdv::executeRecreate;
        }
    }

    sParseTree->flag &= ~QDV_OPT_STATUS_MASK;
    sParseTree->flag |= QDV_OPT_STATUS_VALID;

    //------------------------------------------------------------------
    // check grant
    //------------------------------------------------------------------
    sPrivilegeExist = qdpRole::checkDDLCreateViewPriv( aStatement );
    if ( sPrivilegeExist != IDE_SUCCESS )
    {
        if ( (sParseTree->flag & QDV_OPT_FORCE_MASK) == QDV_OPT_FORCE_FALSE )
        {
            // NO FORCE
            // A error is set in qdpPrivilege::checkDDLCreateViewPriv
            IDE_RAISE(ERR_PASS);
        }
        else
        {
            // FORCE
            // set STATUS
            sParseTree->flag &= ~QDV_OPT_STATUS_MASK;
            sParseTree->flag |= QDV_OPT_STATUS_INVALID;
        }
    }

    //------------------------------------------------------------------
    // validation of SELECT statement
    //------------------------------------------------------------------
    
    // BUG-24408
    // view�� �����ڷ� validation�Ѵ�.
    QCG_SET_SESSION_USER_ID( aStatement, sParseTree->userID );
    
    ((qmsParseTree*)(sParseTree->select->myPlan->parseTree))->querySet->lflag
        &= ~(QMV_PERFORMANCE_VIEW_CREATION_MASK);
    ((qmsParseTree*)(sParseTree->select->myPlan->parseTree))->querySet->lflag
        |= (QMV_PERFORMANCE_VIEW_CREATION_FALSE);
    ((qmsParseTree*)(sParseTree->select->myPlan->parseTree))->querySet->lflag
        &= ~(QMV_VIEW_CREATION_MASK);
    ((qmsParseTree*)(sParseTree->select->myPlan->parseTree))->querySet->lflag
        |= (QMV_VIEW_CREATION_TRUE);

    // PROJ-2204 join update, delete
    // create view�� ���Ǵ� SFWGH���� ǥ���Ѵ�.
    if ( ((qmsParseTree*)(sParseTree->select->myPlan->parseTree))->querySet->SFWGH != NULL )
    {
        ((qmsParseTree*)(sParseTree->select->myPlan->parseTree))->querySet->SFWGH->lflag
            &= ~QMV_SFWGH_UPDATABLE_VIEW_MASK;
        ((qmsParseTree*)(sParseTree->select->myPlan->parseTree))->querySet->SFWGH->lflag
            |= QMV_SFWGH_UPDATABLE_VIEW_TRUE;
    }
    else
    {
        // Nothing to do.
    }

    sSelectValidation = qmv::validateSelect(sParseTree->select );
    if (sSelectValidation != IDE_SUCCESS)
    {
        if ( (sParseTree->flag & QDV_OPT_FORCE_MASK) == QDV_OPT_FORCE_FALSE )
        {
            // NO FORCE
            // A error is set in qmv::select
            IDE_RAISE(ERR_PASS);
        }
        else
        {
            // FORCE
            // set STATUS
            sParseTree->flag &= ~QDV_OPT_STATUS_MASK;
            sParseTree->flag |= QDV_OPT_STATUS_INVALID;
        }
    }

    // session userID�� ����
    QCG_SET_SESSION_USER_ID( aStatement, sSessionUserID );
    
    //------------------------------------------------------------------
    // check SEQUENCE
    //------------------------------------------------------------------
    if ( (sParseTree->flag & QDV_OPT_STATUS_MASK) == QDV_OPT_STATUS_VALID )
    {
        // check CURRVAL
        if (sParseTree->select->myPlan->parseTree->currValSeqs != NULL)
        {
            if ( (sParseTree->flag & QDV_OPT_FORCE_MASK)
                 == QDV_OPT_FORCE_FALSE)
            {
                sqlInfo.setSourceInfo(
                    aStatement,
                    & ( sParseTree->select->myPlan->parseTree->
                        currValSeqs->sequenceNode->position ) );
                IDE_RAISE(ERR_USE_SEQUENCE_IN_VIEW);
            }
            else
            {
                // set STATUS
                sParseTree->flag &= ~QDV_OPT_STATUS_MASK;
                sParseTree->flag |= QDV_OPT_STATUS_INVALID;
            }
        }

        // check NEXTVAL
        if (sParseTree->select->myPlan->parseTree->nextValSeqs != NULL)
        {
            if ( (sParseTree->flag & QDV_OPT_FORCE_MASK)
                 == QDV_OPT_FORCE_FALSE)
            {
                sqlInfo.setSourceInfo(
                    aStatement,
                    & ( sParseTree->select->myPlan->parseTree->
                        nextValSeqs->sequenceNode->position ) );
                IDE_RAISE(ERR_USE_SEQUENCE_IN_VIEW);
            }
            else
            {
                // set STATUS
                sParseTree->flag &= ~QDV_OPT_STATUS_MASK;
                sParseTree->flag |= QDV_OPT_STATUS_INVALID;
            }
        }
    }

    //------------------------------------------------------------------
    // validation of column name and count
    //------------------------------------------------------------------
    if ( (sParseTree->flag & QDV_OPT_STATUS_MASK) == QDV_OPT_STATUS_VALID )
    {
        sColumnValidation = qdbCreate::validateTargetAndMakeColumnList(aStatement);
        
        if (sColumnValidation != IDE_SUCCESS)
        {
            if ( (sParseTree->flag & QDV_OPT_FORCE_MASK)
                 == QDV_OPT_FORCE_FALSE )
            {
                // NO FORCE
                // A error is set in qmv::select
                IDE_RAISE(ERR_PASS);
            }
            else
            {
                // FORCE
                // set STATUS
                sParseTree->flag &= ~QDV_OPT_STATUS_MASK;
                sParseTree->flag |= QDV_OPT_STATUS_INVALID;
            }
        }
        else
        {    
            sColumnValidation = qdbCommon::validateColumnListForCreate(
                aStatement,
                sParseTree->columns,
                ID_FALSE );
        
            if (sColumnValidation != IDE_SUCCESS)
            {
                if ( (sParseTree->flag & QDV_OPT_FORCE_MASK)
                     == QDV_OPT_FORCE_FALSE )
                {
                    // NO FORCE
                    // A error is set in qmv::select
                    IDE_RAISE(ERR_PASS);
                }
                else
                {
                    // FORCE
                    // set STATUS
                    sParseTree->flag &= ~QDV_OPT_STATUS_MASK;
                    sParseTree->flag |= QDV_OPT_STATUS_INVALID;
                }
            }
            else
            {
                // nothing to do 
            }
        }
    }

    // in case of INVALID VIEW
    if ( (sParseTree->flag & QDV_OPT_STATUS_MASK) != QDV_OPT_STATUS_VALID )
    {
        // clear qcTemplate
        //      for preventing qtc::fixAfterValidation() from allocating
        (void) qtc::fixAfterValidationForCreateInvalidView(
            QC_SHARED_TMPLATE(aStatement) );
    }

    //------------------------------------------
    // check circular view definition
    //------------------------------------------
    
    for ( sRelatedObject = aStatement->spvEnv->relatedObjects;
          sRelatedObject != NULL;
          sRelatedObject = sRelatedObject->next )
    {
        
        // (1) public synonym�� ���� circular view definition�˻�
        if ( sRelatedObject->objectType == QS_SYNONYM )
        {
            if ( sRelatedObject->userID == QC_PUBLIC_USER_ID )
            {
                // BUG-32964
                // ������ �̸��� ���� Public Synonym�� �����ϸ�
                // circular view definition�� �߻��Ѵ�.
                if (idlOS::strMatch(
                        sParseTree->tableName.stmtText
                        + sParseTree->tableName.offset,
                        sParseTree->tableName.size,
                        sRelatedObject->objectName.name,
                        sRelatedObject->objectName.size)   == 0)
                {
                    sqlInfo.setSourceInfo(
                        aStatement,
                        & sRelatedObject->objectNamePos );
                    IDE_RAISE(ERR_CIRCULAR_VIEW_DEF);
                }
                else
                {
                    // nothing to do 
                }
            }
            else
            {
                // nothing to do 
            }
        }
        else
        {
            // nothing to do 
        }
        
        // (2) view �Ǵ� table�� ���� circular view definition�˻�
        if ( sRelatedObject->objectType == QS_TABLE )  
        {
            IDE_TEST( qcmUser::getUserID( aStatement,
                                          sRelatedObject->userName.name,
                                          sRelatedObject->userName.size,
                                          &sRelatedObjectUserID )
                      != IDE_SUCCESS);

            if ( sParseTree->userID == sRelatedObjectUserID )
            {
                if (idlOS::strMatch(
                        sParseTree->tableName.stmtText
                        + sParseTree->tableName.offset,
                        sParseTree->tableName.size,
                        sRelatedObject->objectNamePos.stmtText
                        + sRelatedObject->objectNamePos.offset,
                        sRelatedObject->objectNamePos.size)   == 0)
                {
                    sqlInfo.setSourceInfo(
                        aStatement,
                        & sRelatedObject->objectNamePos );
                    IDE_RAISE(ERR_CIRCULAR_VIEW_DEF);
                }
            }
            // BUG-8922
            // find v1 -> v2 -> v1
            idlOS::memcpy( sViewName,
                           sParseTree->tableName.stmtText
                           + sParseTree->tableName.offset,
                           sParseTree->tableName.size );
            sViewName[sParseTree->tableName.size] = '\0';
            
            IDE_TEST( qcmView::checkCircularView(
                          aStatement,
                          sParseTree->userID,
                          sViewName,
                          sRelatedObject->tableID,
                          &sCircularViewExist )
                      != IDE_SUCCESS );

            if ( sCircularViewExist == ID_TRUE )
            {
                sqlInfo.setSourceInfo(
                    aStatement,
                    & sRelatedObject->objectNamePos );
                IDE_RAISE(ERR_CIRCULAR_VIEW_DEF);
            }
        }
    }

    //BUGBUG view�� ����� tablespace ??
    sParseTree->TBSAttr.mID = SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC;

    IDE_FT_END();

    return IDE_SUCCESS;
    
    IDE_EXCEPTION_SIGNAL()
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_FAULT_TOLERATED ) );
    }
    IDE_EXCEPTION(ERR_EXIST_OBJECT_NAME);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDB_EXIST_OBJECT_NAME));
    }

    IDE_EXCEPTION(ERR_USE_SEQUENCE_IN_VIEW)
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QMV_USE_SEQUENCE_IN_VIEW,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION(ERR_CIRCULAR_VIEW_DEF)
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QDV_CIRCULAR_VIEW,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION( CANT_USE_RESERVED_WORD );
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QDB_RESERVED_WORD_IN_OBJECT_NAME,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION(ERR_PASS);
    {
    }
    IDE_EXCEPTION_END;

    IDE_FT_EXCEPTION_BEGIN();

    // session userID�� ����
    QCG_SET_SESSION_USER_ID( aStatement, sSessionUserID );
    
    IDE_FT_EXCEPTION_END();

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qdv::validateAlter(qcStatement * aStatement)
{
/***********************************************************************
 *
 * Description :
 *    ALTER VIEW ... COMPILE �� validation ����
 *
 * Implementation :
 *    1. �䰡 �����ϴ��� üũ
 *    2. ������ �ִ��� üũ
 *
 ***********************************************************************/

#define IDE_FN "qdv::validateAlter"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qdv::validateAlter"));

    qdTableParseTree    * sParseTree;
    qcuSqlSourceInfo      sqlInfo;

    sParseTree = (qdTableParseTree *)aStatement->myPlan->parseTree;

    /* BUG-30059 */
    if ( qdbCommon::containDollarInName( &(sParseTree->tableName) ) == ID_TRUE )
    {
        sqlInfo.setSourceInfo(
            aStatement,
            &(sParseTree->tableName) );

        IDE_RAISE( CANT_USE_RESERVED_WORD );
    }

    //------------------------------------------------------------------
    // validation of view name
    //------------------------------------------------------------------
    IDE_TEST( qdbCommon::checkTableInfo( aStatement,
                                         sParseTree->userName,
                                         sParseTree->tableName,
                                         &(sParseTree->userID),
                                         &(sParseTree->tableInfo),
                                         &(sParseTree->tableHandle),
                                         &(sParseTree->tableSCN))
              != IDE_SUCCESS);

    IDE_TEST(qcm::lockTableForDDLValidation(aStatement,
                                            sParseTree->tableHandle,
                                            sParseTree->tableSCN)
             != IDE_SUCCESS);

    // PR-13725
    // CHECK OPERATABLE
    if( QCM_IS_OPERATABLE_QP_ALTER_VIEW(
            sParseTree->tableInfo->operatableFlag )
        != ID_TRUE )
    {
        sqlInfo.setSourceInfo(aStatement, &sParseTree->tableName);
        IDE_RAISE(ERR_NOT_EXIST_VIEW_NAME);
    }

    //------------------------------------------------------------------
    // check grant
    //------------------------------------------------------------------

    IDE_TEST( qdpRole::checkDDLAlterTablePriv( aStatement,
                                               sParseTree->tableInfo )
              != IDE_SUCCESS );

    return IDE_SUCCESS;
    
    IDE_EXCEPTION( CANT_USE_RESERVED_WORD );
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QDB_RESERVED_WORD_IN_OBJECT_NAME,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION( ERR_NOT_EXIST_VIEW_NAME );
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCV_NOT_EXISTS_TABLE,
                                sqlInfo.getErrMessage()));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

/***********************************************************************
 * EXECUTE
 **********************************************************************/

IDE_RC qdv::executeCreate(qcStatement * aStatement)
{
/***********************************************************************
 *
 * Description :
 *    CREATE VIEW ... �� execution ����
 *
 * Implementation :
 *    1. �� status �� INVALID �̸� pseudo integer column �� �����
 *    2. View ID �ο�
 *    3. create smiTable => qdbCommon::createTableOnSM
 *    3. insert into META tables
 *    3. ��Ÿ ĳ�� ����ü ����
 *
 ***********************************************************************/

#define IDE_FN "qdv::executeCreate"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qdv::executeCreate"));

    qdTableParseTree    * sParseTree;
    UInt                  sViewID;
    smOID                 sTableOID;
    qcmColumn           * sColumn;
    qcmTableInfo        * sNewTableInfo   = NULL;
    void                * sNewTableHandle = NULL;
    smSCN                 sNewSCN         = SM_SCN_INIT;

    sParseTree = (qdTableParseTree *)aStatement->myPlan->parseTree;

    if ( (sParseTree->flag & QDV_OPT_STATUS_MASK) == QDV_OPT_STATUS_VALID )
    {
        // Nothing to do.
    }
    else
    {
        // make a pseudo integer column
        IDE_TEST(makeOneIntegerQcmColumn( aStatement, &sColumn )
                 != IDE_SUCCESS);

        sParseTree->columns = sColumn;
    }

    // get VIEW_ID
    IDE_TEST( qcm::getNextTableID( aStatement, &sViewID ) != IDE_SUCCESS );

    // create smiTable
    IDE_TEST( qdbCommon::createTableOnSM( aStatement,
                                          sParseTree->columns,
                                          sParseTree->userID,
                                          sViewID,
                                          sParseTree->maxrows,
                                          sParseTree->TBSAttr.mID,
                                          sParseTree->segAttr,
                                          sParseTree->segStoAttr,
                                          0, /* Table Flag Mask */ 
                                          0, /* Table Flag Value */
                                          1, /* Parallel Degree */
                                          &sTableOID,
                                          aStatement->myPlan->stmtText,
                                          aStatement->myPlan->stmtTextLen )
              != IDE_SUCCESS );

    // insert into META tables
    IDE_TEST( insertViewSpecIntoMeta( aStatement,
                                      sViewID,
                                      sTableOID )
              != IDE_SUCCESS );

    // make META caching structure
    IDE_TEST( qcm::makeAndSetQcmTableInfo( QC_SMI_STMT( aStatement ),
                                           sViewID,
                                           sTableOID ) != IDE_SUCCESS );

    IDE_TEST( qcm::getTableInfoByID( aStatement,
                                     sViewID,
                                     & sNewTableInfo,
                                     & sNewSCN,
                                     & sNewTableHandle )
              != IDE_SUCCESS );

    // BUG-11266
    IDE_TEST(qcmView::recompileAndSetValidViewOfRelated(
                 aStatement,
                 sParseTree->userID,
                 (SChar *) (sParseTree->tableName.stmtText +
                            sParseTree->tableName.offset),
                 sParseTree->tableName.size,
                 QS_TABLE)
             != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    (void)qcm::destroyQcmTableInfo( sNewTableInfo );

    return IDE_FAILURE;

#undef IDE_FN
}


IDE_RC qdv::executeRecreate(qcStatement * aStatement)
{
/***********************************************************************
 *
 * Description :
 *    CREATE or REPLIACE VIEW ... �� execution ����,
 *    VIEW_ID �� ������� �ʴ´�.
 *
 * Implementation :
 *    1. �� status �� INVALID �̸� pseudo integer column �� �����
 *    2. create new smiTable => qdbCommon::createTableOnSM
 *    3. ������ ĳ���� ��Ÿ ����ü qcmTableInfo ���� �α�
 *    4. META tables ���� ���� ���� ����
 *    5. META tables �� ���� ������ ���� �Է�
 *    6. related PSM �� invalid ���·� ����
 *    7. related VIEW �� invalid ���·� ����
 *    8. ��Ÿ ĳ�� ����ü ���� ( qcmTableInfo )
 *    9. ���� �� ���� => smiTable::dropTable
 *    10. ���� ĳ�� ����ü ����
 *
 ***********************************************************************/

#define IDE_FN "qdv::executeRecreate"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qdv::executeRecreate"));

    // VIEW_ID and TABLE_OID are NOT changed.
    // The others are changed.

    qdTableParseTree    * sParseTree;
    qcmColumn           * sColumn;
    qcmTableInfo        * sOldTableInfo;
    qcmTableInfo        * sNewTableInfo = NULL;
    UInt                  sViewID;
    smOID                 sNewTableOID;
    smSCN                 sSCN;
    void                * sTableHandle;

    sParseTree = (qdTableParseTree *)aStatement->myPlan->parseTree;

    // BUG-16771
    IDE_TEST( qcm::validateAndLockTable( aStatement,
                                         sParseTree->tableHandle,
                                         sParseTree->tableSCN,
                                         SMI_TABLE_LOCK_X )
              != IDE_SUCCESS);

    // BUG-30741 validate�������� sParseTree�� ���س��� tableInfo��
    // ��ȿ���� ������ �����Ƿ� �ٽ� �����´�.
    IDE_TEST( smiGetTableTempInfo( sParseTree->tableHandle,
                                   (void**)&sParseTree->tableInfo )
              != IDE_SUCCESS );

    sViewID = sParseTree->tableInfo->tableID;

    if ( (sParseTree->flag & QDV_OPT_STATUS_MASK) == QDV_OPT_STATUS_VALID )
    {
        sColumn = sParseTree->columns;
    }
    else
    {
        // make a pseudo integer column
        IDE_TEST(makeOneIntegerQcmColumn( aStatement, &sColumn )
                 != IDE_SUCCESS);

        sParseTree->columns = sColumn;
    }

    // create new smiTable
    IDE_TEST( qdbCommon::createTableOnSM( aStatement,
                                          sParseTree->columns,
                                          sParseTree->userID,
                                          sViewID,
                                          sParseTree->maxrows,
                                          sParseTree->TBSAttr.mID,
                                          sParseTree->segAttr,
                                          sParseTree->segStoAttr,
                                          0, /* Table Flag Mask */ 
                                          0, /* Table Flag Value */
                                          1, /* Parallel Degree */
                                          &sNewTableOID,
                                          aStatement->myPlan->stmtText,
                                          aStatement->myPlan->stmtTextLen )
              != IDE_SUCCESS );

    // get old qcmTableInfo
    IDE_TEST(qcm::getTableInfoByID( aStatement,
                                    sViewID,
                                    &sOldTableInfo,
                                    &sSCN,
                                    &sTableHandle )
             != IDE_SUCCESS);

    IDE_TEST(qcm::validateAndLockTable(aStatement,
                                       sTableHandle,
                                       sSCN,
                                       SMI_TABLE_LOCK_X)
             != IDE_SUCCESS);

    // delete from META tables
    IDE_TEST( qdd::deleteViewFromMeta( aStatement, sViewID ) != IDE_SUCCESS );

    // insert into META tables
    IDE_TEST( insertViewSpecIntoMeta( aStatement,
                                      sViewID,
                                      sNewTableOID )
              != IDE_SUCCESS );

    // related PSM
    // BUG-46416 Set status of PSM to invalid using new transaction.
    IDE_TEST( qcmProc::relSetInvalidProcOfRelated(
                  aStatement,
                  sParseTree->userID,
                  sParseTree->tableInfo->name,
                  idlOS::strlen((SChar*)sParseTree->tableInfo->name),
                  QS_TABLE,
                  ID_TRUE )
              != IDE_SUCCESS);

    // PROJ-1073 Package
    // BUG-46416 Set status of PSM to invalid using new transaction.
    IDE_TEST( qcmPkg::relSetInvalidPkgOfRelated(
                  aStatement,
                  sParseTree->userID,
                  sParseTree->tableInfo->name,
                  idlOS::strlen((SChar*)sParseTree->tableInfo->name),
                  QS_TABLE,
                  ID_TRUE )
              != IDE_SUCCESS);

    // related VIEW
    IDE_TEST( qcmView::setInvalidViewOfRelated(
                  aStatement,
                  sParseTree->userID,
                  sParseTree->tableInfo->name,
                  idlOS::strlen((SChar*)sParseTree->tableInfo->name),
                  QS_TABLE )
              != IDE_SUCCESS);

    // BUG-47801
    IDE_TEST( qdnTrigger::setInvalidTriggerCache4Table( sOldTableInfo )
              != IDE_SUCCESS );

    // make META caching structure ( qcmTableInfo )
    IDE_TEST( qcm::makeAndSetQcmTableInfo( QC_SMI_STMT( aStatement ),
                                           sViewID,
                                           sNewTableOID ) != IDE_SUCCESS );

    // get old qcmTableInfo
    IDE_TEST( qcm::getTableInfoByID( aStatement,
                                     sViewID,
                                     &sNewTableInfo,
                                     &sSCN,
                                     &sTableHandle )
              != IDE_SUCCESS);

    // BUG-11266
    IDE_TEST( qcmView::recompileAndSetValidViewOfRelated(
                  aStatement,
                  sParseTree->userID,
                  sNewTableInfo->name,
                  idlOS::strlen((SChar*)sNewTableInfo->name),
                  QS_TABLE)
              != IDE_SUCCESS );

    // drop old smiTable
    IDE_TEST( smiTable::dropTable( QC_SMI_STMT( aStatement ),
                                   sParseTree->tableInfo->tableHandle,
                                   SMI_TBSLV_DDL_DML )
        != IDE_SUCCESS);

    // free old qcmTableInfo
    (void)qcm::destroyQcmTableInfo( sOldTableInfo );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    (void)qcm::destroyQcmTableInfo( sNewTableInfo );

    return IDE_FAILURE;

#undef IDE_FN
}


IDE_RC qdv::executeAlter(qcStatement * aStatement)
{
/***********************************************************************
 *
 * Description :
 *    ALTER VIEW ... COMPILE �� execution ����
 *
 * Implementation :
 *    1. ������ �� �������� statement ������ ã�Ƽ� �Ľ��Ѵ�.
 *    2. 1 ���� �Ľ��� select ���� validation, optimization �� �����Ѵ�
 *    3. ���� status �� valid �̸� execution �� �����Ѵ�.
 *       4. create new smiTable => qdbCommon::createTableOnSM
 *       5. ������ ĳ���� ��Ÿ ����ü qcmTableInfo ���� �α�
 *       6. META tables ���� ���� ���� ����
 *       7. META tables �� ���� ������ ���� �Է�
 *       8. ��Ÿ ĳ�� ����ü ���� ( qcmTableInfo )
 *       9. ���� �� ���� => smiTable::dropTable
 *       10. ���� ĳ�� ����ü ����
 *
 ***********************************************************************/

#define IDE_FN "qdv::executeAlter"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qdv::executeAlter"));

    qcStatement         * sCreateStatement;
    qdTableParseTree    * sAlterParseTree;
    qdTableParseTree    * sCreateParseTree;
    qcmTableInfo        * sOldTableInfo = NULL;
    UInt                  sViewID;
    smOID                 sNewTableOID;
    smSCN                 sSCN;
    void                * sTableHandle;
    qcmTableInfo        * sNewTableInfo   = NULL;
    void                * sNewTableHandle = NULL;
    smSCN                 sNewSCN         = SM_SCN_INIT;

    // ALTER VIEW : executing only VIEW RECOMPILE
    // The status of Related PSMs are NOT changed.

    sAlterParseTree = (qdTableParseTree *)aStatement->myPlan->parseTree;

    IDE_TEST( qcm::validateAndLockTable(aStatement,
                                        sAlterParseTree->tableHandle,
                                        sAlterParseTree->tableSCN,
                                        SMI_TABLE_LOCK_X)
              != IDE_SUCCESS );

    // BUG-30741 validate�������� sAlterParseTree�� ���س��� tableInfo��
    // ��ȿ���� ������ �����Ƿ� �ٽ� �����´�.
    IDE_TEST( smiGetTableTempInfo( sAlterParseTree->tableHandle,
                                   (void**)&sAlterParseTree->tableInfo )
              != IDE_SUCCESS );

    sViewID = sAlterParseTree->tableInfo->tableID;

    //---------------------------------------------------------------
    // get the string of CREATE OR REPLACE VIEW statement and parsing
    //---------------------------------------------------------------
    IDE_TEST( makeParseTreeForAlter( aStatement, sAlterParseTree->tableInfo )
              != IDE_SUCCESS );

    sCreateStatement = sAlterParseTree->select;

    IDE_TEST( qtc::fixAfterParsing( QC_SHARED_TMPLATE(sCreateStatement) )
              != IDE_SUCCESS );

    sCreateParseTree = (qdTableParseTree *)(sCreateStatement->myPlan->parseTree);

    sCreateParseTree->flag &= ~QDV_OPT_REPLACE_MASK;
    sCreateParseTree->flag |= QDV_OPT_REPLACE_TRUE;

    //---------------------------------------------------------------
    // validation
    //---------------------------------------------------------------
    IDE_TEST( sCreateStatement->myPlan->parseTree->validate( sCreateStatement )
              != IDE_SUCCESS );

    IDE_TEST(qtc::fixAfterValidation( QC_QMP_MEM(sCreateStatement),
                                      QC_SHARED_TMPLATE(sCreateStatement) )
             != IDE_SUCCESS);

    //---------------------------------------------------------------
    // optimization
    //---------------------------------------------------------------
    IDE_TEST( sCreateStatement->myPlan->parseTree->optimize( sCreateStatement )
              != IDE_SUCCESS );

    IDE_TEST(qtc::fixAfterOptimization( sCreateStatement )
             != IDE_SUCCESS);
    
    //---------------------------------------------------------------
    // execution
    //---------------------------------------------------------------
    if ( ( sCreateParseTree->flag & QDV_OPT_STATUS_MASK )
         == QDV_OPT_STATUS_VALID )
    {
        // create new smiTable
        IDE_TEST( qdbCommon::createTableOnSM( aStatement,
                                              sCreateParseTree->columns,
                                              sCreateParseTree->userID,
                                              sViewID,
                                              sCreateParseTree->maxrows,
                                              sCreateParseTree->TBSAttr.mID,
                                              sCreateParseTree->segAttr,
                                              sCreateParseTree->segStoAttr,
                                              0, /* Table Flag Mask */ 
                                              0, /* Table Flag Value */
                                              1, /* Parallel Degree */
                                              &sNewTableOID,
                                              sCreateStatement->myPlan->stmtText,
                                              sCreateStatement->myPlan->stmtTextLen )
                  != IDE_SUCCESS );

        // get old qcmTableInfo
        IDE_TEST( qcm::getTableInfoByID( aStatement,
                                         sViewID,
                                         &sOldTableInfo,
                                         &sSCN,
                                         &sTableHandle)
                  != IDE_SUCCESS);

        IDE_TEST(qcm::validateAndLockTable(aStatement,
                                           sTableHandle,
                                           sSCN,
                                           SMI_TABLE_LOCK_X)
                 != IDE_SUCCESS);

        // delete from META tables
        IDE_TEST( qdd::deleteViewFromMeta( sCreateStatement, sViewID )
                  != IDE_SUCCESS );

        // insert into META tables
        IDE_TEST( insertViewSpecIntoMeta( sCreateStatement,
                                          sViewID,
                                          sNewTableOID )
                  != IDE_SUCCESS );

        // make META caching structure ( qcmTableInfo )
        IDE_TEST( qcm::makeAndSetQcmTableInfo( QC_SMI_STMT( aStatement ),
                                               sViewID,
                                               sNewTableOID )
                  != IDE_SUCCESS );

        IDE_TEST( qcm::getTableInfoByID( aStatement,
                                         sViewID,
                                         & sNewTableInfo,
                                         & sNewSCN,
                                         & sNewTableHandle )
                  != IDE_SUCCESS );

        // drop old smiTable
        IDE_TEST( smiTable::dropTable( QC_SMI_STMT( aStatement ),
                                       sAlterParseTree->tableInfo->tableHandle,
                                       SMI_TBSLV_DDL_DML )
                  != IDE_SUCCESS);

        // free old qcmTableInfo
        (void)qcm::destroyQcmTableInfo( sOldTableInfo );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    (void)qcm::destroyQcmTableInfo( sNewTableInfo );

    return IDE_FAILURE;

#undef IDE_FN
}


IDE_RC qdv::insertViewSpecIntoMeta(
    qcStatement * aStatement,
    UInt          aViewID,
    smOID         aTableOID)
{
/***********************************************************************
 *
 * Description :
 *    ���� ��Ÿ ������ ��Ÿ ���̺� �Է��Ѵ�.
 *
 * Implementation :
 *    1. ���� status �� ���صд�
 *    2. SYS_TABLES_ �� �Է�
 *    3. SYS_COLUMNS_ �� �Է�
 *    4. SYS_VIEWS_ �� status �Է�
 *    5. SYS_VIEW_PARSE_ �� statement text �Է�
 *    6. SYS_VIEW_RELATED_ �� ���� ������Ʈ �Է�
 *
 ***********************************************************************/

#define IDE_FN "qdv::insertViewSpecIntoMeta"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qdv::insertViewSpecIntoMeta"));

    qdTableParseTree    * sParseTree;
    qcmColumn           * sColumn;
    UInt                  sColumnCount;
    UInt                  sStatus;
    UInt                  sWithReadOnly;
    qsRelatedObjects    * sRelatedObject;
    qsRelatedObjects    * sNewRelatedObject;
    smiSegAttr            sSegmentAttr;
    smiSegStorageAttr     sSegmentStoAttr;
    qsOID                 sPkgBodyOID;

    sParseTree = (qdTableParseTree *)aStatement->myPlan->parseTree;

    // get column count
    for (sColumnCount = 0,
             sColumn = sParseTree->columns;
         sColumn != NULL;
         sColumn = sColumn->next)
    {
        sColumnCount++;
    }

    // get status
    if ( (sParseTree->flag & QDV_OPT_STATUS_MASK) == QDV_OPT_STATUS_VALID)
    {
        // valid
        sStatus = 0;
    }
    else
    {
        // invalid
        sStatus = 1;
    }

    /* BUG-36350 Updatable Join DML WITH READ ONLY
     * get WITH READ ONLY option */
    if (( sParseTree->flag & QDV_OPT_WITH_READ_ONLY_MASK) ==
        QDV_OPT_WITH_READ_ONLY_TRUE )
    {
        /* WITH READ ONLY */
        sWithReadOnly = 0;
    }
    else
    {
        /* NON WITH READ ONLY */
        sWithReadOnly = 1;
    }

    // Memory Table �� ������� �ʴ� �Ӽ�������, �����Ͽ� �����Ѵ�.
    sSegmentAttr.mPctFree =
                  QD_MEMORY_TABLE_DEFAULT_PCTFREE;  // PCTFREE
    sSegmentAttr.mPctUsed =
                  QD_MEMORY_TABLE_DEFAULT_PCTUSED;  // PCTUSED
    sSegmentAttr.mInitTrans =
                  QD_MEMORY_TABLE_DEFAULT_INITRANS;  // initial ttl size
    sSegmentAttr.mMaxTrans =
                  QD_MEMORY_TABLE_DEFAULT_MAXTRANS;  // maximum ttl size
    sSegmentStoAttr.mInitExtCnt =
                  QD_MEMORY_SEGMENT_DEFAULT_STORAGE_INITEXTENTS;  // initextents
    sSegmentStoAttr.mNextExtCnt =
                  QD_MEMORY_SEGMENT_DEFAULT_STORAGE_NEXTEXTENTS;  // nextextents
    sSegmentStoAttr.mMinExtCnt =
                  QD_MEMORY_SEGMENT_DEFAULT_STORAGE_MINEXTENTS;  // minextents
    sSegmentStoAttr.mMaxExtCnt =
                  QD_MEMORY_SEGMENT_DEFAULT_STORAGE_MAXEXTENTS;  // maxextents

    // insert SYS_TABLES_
    IDE_TEST(
        qdbCommon::insertTableSpecIntoMeta(
            aStatement,
            ID_FALSE,
            sParseTree->flag,
            sParseTree->tableName,
            sParseTree->userID,
            aTableOID,
            aViewID,
            sColumnCount,
            sParseTree->maxrows,
            QCM_ACCESS_OPTION_READ_WRITE, /* PROJ-2359 Table/Partition Access Option */
            SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
            sSegmentAttr,
            sSegmentStoAttr,
            QCM_TEMPORARY_ON_COMMIT_NONE,
            1,                           // PROJ-1071
            QCM_SHARD_FLAG_TABLE_NONE )  // TASK-7307
        != IDE_SUCCESS);

    // insert SYS_COLUMNS_
    if ( (sParseTree->flag & QDV_OPT_STATUS_MASK) == QDV_OPT_STATUS_VALID)
    {
        IDE_TEST( qdbCommon::insertColumnSpecIntoMeta( aStatement,
                                                       sParseTree->userID,
                                                       aViewID,
                                                       sParseTree->columns,
                                                       ID_FALSE /* is queue */)
                  != IDE_SUCCESS);
    }

    // insert SYS_VIEWS_
    IDE_TEST( insertIntoViewsMeta( aStatement,
                                   sParseTree->userID,
                                   aViewID,
                                   sStatus,
                                   sWithReadOnly )
              != IDE_SUCCESS);

    // insert SYS_VIEW_PARSE_
    IDE_TEST( insertIntoViewParseMeta( aStatement,
                                       sParseTree->ncharList,
                                       sParseTree->userID,
                                       aViewID )
              != IDE_SUCCESS);

    // insert SYS_VIEW_RELATED_
    if ( (sParseTree->flag & QDV_OPT_STATUS_MASK) == QDV_OPT_STATUS_VALID)
    {
        for ( sRelatedObject = aStatement->spvEnv->relatedObjects;
              sRelatedObject != NULL;
              sRelatedObject = sRelatedObject->next )
        {
            IDE_TEST( insertIntoViewRelatedMeta( aStatement,
                                                 sParseTree->userID,
                                                 aViewID,
                                                 sRelatedObject )
                      != IDE_SUCCESS);

            // BUG-36975
            if( sRelatedObject->objectType == QS_PKG )
            {
                if( qcmPkg::getPkgExistWithEmptyByNamePtr( aStatement,
                                                           sRelatedObject->userID,
                                                           (SChar*)(sRelatedObject->objectNamePos.stmtText +
                                                                    sRelatedObject->objectNamePos.offset),
                                                           sRelatedObject->objectNamePos.size,
                                                           QS_PKG_BODY,
                                                           &sPkgBodyOID )
                    == IDE_SUCCESS )
                { 
                    if( sPkgBodyOID != QS_EMPTY_OID ) 
                    {

                        IDE_TEST( qcmPkg::makeRelatedObjectNodeForInsertMeta(
                                aStatement,
                                sRelatedObject->userID,
                                sRelatedObject->objectID,
                                sRelatedObject->objectNamePos,
                                (SInt)QS_PKG_BODY,
                                &sNewRelatedObject )
                            != IDE_SUCCESS );

                        IDE_TEST( insertIntoViewRelatedMeta( aStatement,
                                                             sParseTree->userID,
                                                             aViewID,
                                                             sNewRelatedObject )
                                  != IDE_SUCCESS);

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
                // package�� spec�� body�� �з��ȴ�.
            }
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qdv::insertIntoViewsMeta(
    qcStatement * aStatement,
    UInt          aUserID,
    UInt          aViewID,
    UInt          aStatus,
    UInt          aWithReadOnly )
{
/***********************************************************************
 *
 * Description :
 *      insertViewSpecIntoMeta �κ��� ȣ��, SYS_VIEWS_ �� �Է�
 *
 * Implementation :
 *      1. SYS_VIEWS_ ��Ÿ ���̺��� ������ �Է�
 *
 ***********************************************************************/

#define IDE_FN "qdv::insertIntoViewsMeta"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qdv::insertIntoViewsMeta"));

    SChar     * sSqlStr;
    vSLong      sRowCnt;
    SChar     * sTrueFalse[2] = {(SChar*)"Y", (SChar*)"N"};
    SChar     * sIsWithReadOnly;

    /* BUG-36350 Updatable Join DML WITH READ ONLY */
    if ( aWithReadOnly == 0 )
    {
        sIsWithReadOnly = sTrueFalse[0];
    }
    else
    {
        sIsWithReadOnly = sTrueFalse[1];
    }
    
    IDU_FIT_POINT( "qdv::insertIntoViewsMeta::alloc::sSqlStr",
                    idERR_ABORT_InsufficientMemory );

    IDE_TEST(STRUCT_ALLOC_WITH_SIZE(aStatement->qmxMem,
                                    SChar,
                                    QD_MAX_SQL_LENGTH,
                                    &sSqlStr)
             != IDE_SUCCESS);

    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                     "INSERT INTO SYS_VIEWS_ VALUES( "
                     "INTEGER'%"ID_INT32_FMT"', "
                     "INTEGER'%"ID_INT32_FMT"', "
                     "INTEGER'%"ID_INT32_FMT"', "
                     "'%s' ) ",
                     aUserID,
                     aViewID,
                     aStatus,
                     sIsWithReadOnly );

    IDE_TEST(qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                sSqlStr,
                                & sRowCnt ) != IDE_SUCCESS);

    IDE_TEST_RAISE(sRowCnt != 1, ERR_META_CRASH);

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_META_CRASH)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCM_META_CRASH));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qdv::insertIntoViewParseMeta(
    qcStatement   * aStatement,
    qcNamePosList * aNcharList,
    UInt            aUserID,
    UInt            aViewID )
{
/***********************************************************************
 *
 * Description :
 *    SYS_VIEW_PARSE_ �� statement text �Է�
 *
 * Implementation :
 *    1. text �� ���� ����(100) �� �ڸ� ����
 *    2. ��ȣ�� �ο��Ͽ��� SYS_VIEW_PARSE_ �� �Է�
 *
 ***********************************************************************/

#define IDE_FN "qdv::insertIntoViewParseMeta"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qdv::insertIntoViewParseMeta"));

    SInt            sCurrPos = 0;
    SInt            sCurrLen = 0;
    SInt            sSeqNo   = 0;
    UInt            sAddSize = 0;
    SChar         * sStmtBuffer = NULL;
    UInt            sStmtBufferLen = 0;
    qcNamePosList * sNcharList = NULL;
    qcNamePosList * sTempNamePosList = NULL;
    qcNamePosition  sNamePos;
    UInt            sBufferSize = 0;
    SChar         * sIndex;
    SChar         * sStartIndex;
    SChar         * sPrevIndex;

    const mtlModule * sModule;
    
    sNcharList  = aNcharList;
    
    // PROJ-1579 NCHAR
    // ��Ÿ���̺� �����ϱ� ���� ��Ʈ���� �����ϱ� ����
    // N Ÿ���� �ִ� ��� U Ÿ������ ��ȯ�Ѵ�.
    if( sNcharList != NULL )
    {
        for( sTempNamePosList = sNcharList;
             sTempNamePosList != NULL;
             sTempNamePosList = sTempNamePosList->next )
        {
            sNamePos = sTempNamePosList->namePos;

            // U Ÿ������ ��ȯ�ϸ鼭 �þ�� ������ ���
            // N'��' => U'\C548' ���� ��ȯ�ȴٸ�
            // '��'�� ĳ���� ���� KSC5601�̶�� �������� ��,
            // single-quote���� ���ڴ� 2 byte -> 5byte�� ����ȴ�.
            // ��, 1.5�谡 �þ�� ���̴�.
            //(��ü ����� �ƴ϶� �����ϴ� ����� ����ϴ� ����)
            // ������, � �������� ĳ���� ���� ������ �𸣹Ƿ�
            // * 2�� ����� ��´�.
            sAddSize += (sNamePos.size - 3) * 2;
        }

        sBufferSize = aStatement->myPlan->stmtTextLen + sAddSize;

        IDU_FIT_POINT( "qdv::insertIntoViewParseMeta::alloc::sStmtBuffer",
                        idERR_ABORT_InsufficientMemory );

        IDE_TEST(STRUCT_ALLOC_WITH_SIZE(aStatement->qmxMem,
                                        SChar,
                                        sBufferSize,
                                        & sStmtBuffer)
                 != IDE_SUCCESS);

        IDE_TEST( qcmProc::convertToUTypeString( aStatement,
                                                 sNcharList,
                                                 sStmtBuffer,
                                                 sBufferSize )
                  != IDE_SUCCESS );

        sStmtBufferLen = idlOS::strlen( sStmtBuffer );
    }
    else
    {
        sStmtBufferLen = aStatement->myPlan->stmtTextLen;
        sStmtBuffer    = aStatement->myPlan->stmtText;
    }

    // BUG-44978
    sModule     = mtl::mDBCharSet;
    sStartIndex = sStmtBuffer;
    sIndex      = sStartIndex;

    while (1)
    {
        sPrevIndex = sIndex;
        
        (void)sModule->nextCharPtr( (UChar**) &sIndex,
                                    (UChar*) ( sStmtBuffer +
                                               sStmtBufferLen ) );
        
        if (( sStmtBuffer + sStmtBufferLen ) <= sIndex )
        {
            // ������ �� ���.
            // ����� �� �� break.
            sSeqNo++;

            sCurrPos = sStartIndex - sStmtBuffer;
            sCurrLen = sIndex - sStartIndex;

            // insert one record into SYS_VIEW_PARSE_
            IDE_TEST( insertIntoViewParseMetaOneRecord( aStatement,
                                                        sStmtBuffer,
                                                        aUserID,
                                                        aViewID,
                                                        sSeqNo,
                                                        sCurrPos,
                                                        sCurrLen )
                      != IDE_SUCCESS );

            break;
        }
        else
        {
            if ( sIndex - sStartIndex >= QCM_MAX_PROC_LEN )
            {
                // ���� ������ �� ����, �дٺ��� 100����Ʈ �Ǵ� �ʰ��� ����
                // �Ǿ��� �� �߶� ���
                sCurrPos = sStartIndex - sStmtBuffer;
                
                if ( sIndex - sStartIndex == QCM_MAX_PROC_LEN )
                {
                    // �� �������� ���
                    sCurrLen = QCM_MAX_PROC_LEN;
                    sStartIndex = sIndex;
                }
                else
                {
                    // �������� ��� �� ���� ĳ���� ��ġ���� ���
                    sCurrLen = sPrevIndex - sStartIndex;
                    sStartIndex = sPrevIndex;
                }

                sSeqNo++;
                
                // insert one record into SYS_VIEW_PARSE_
                IDE_TEST( insertIntoViewParseMetaOneRecord( aStatement,
                                                            sStmtBuffer,
                                                            aUserID,
                                                            aViewID,
                                                            sSeqNo,
                                                            sCurrPos,
                                                            sCurrLen )
                          != IDE_SUCCESS );
            }
            else
            {
                // Nothing to do.
            }
        }
    } // end of while

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}


IDE_RC qdv::insertIntoViewParseMetaOneRecord(
    qcStatement * aStatement,
    SChar       * aStmtBuffer,
    UInt          aUserID,
    UInt          aViewID,
    UInt          aSeqNo,
    UInt          aOffset,
    UInt          aLength )
{
/***********************************************************************
 *
 * Description :
 *      SYS_VIEW_PARSE_ �� �Է�
 *
 * Implementation :
 *      1. SYS_VIEW_PARSE_ ��Ÿ ���̺� view ���������� �Է�
 *
 ***********************************************************************/

#define IDE_FN "qdv::insertIntoViewParseMetaOneRecord"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qdv::insertIntoViewParseMetaOneRecord"));

    SChar                 sParseStr[ QCM_MAX_PROC_LEN * 2 + 2 ];
    SChar               * sSqlStr;
    vSLong                sRowCnt;

    if( aStmtBuffer != NULL )
    {
        qcmProc::prsCopyStrDupAppo( sParseStr,
                                    aStmtBuffer + aOffset,
                                    aLength );
    }
    else
    {
        qcmProc::prsCopyStrDupAppo( sParseStr,
                                    aStatement->myPlan->stmtText + aOffset,
                                    aLength );
    }

    IDU_FIT_POINT( "qdv::insertIntoViewParseMetaOneRecord::alloc::sSqlStr",
                    idERR_ABORT_InsufficientMemory );

    IDE_TEST(STRUCT_ALLOC_WITH_SIZE(aStatement->qmxMem,
                                    SChar,
                                    QD_MAX_SQL_LENGTH,
                                    &sSqlStr)
             != IDE_SUCCESS);

    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                     "INSERT INTO SYS_VIEW_PARSE_ VALUES ( "
                     QCM_SQL_UINT32_FMT", "
                     QCM_SQL_UINT32_FMT", "
                     QCM_SQL_UINT32_FMT", "
                     QCM_SQL_CHAR_FMT") ",
                     aUserID,
                     aViewID,
                     aSeqNo,
                     sParseStr );

    IDE_TEST(qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                sSqlStr,
                                & sRowCnt ) != IDE_SUCCESS);

    IDE_TEST_RAISE(sRowCnt != 1, ERR_META_CRASH);

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_META_CRASH)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCM_META_CRASH));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qdv::insertIntoViewRelatedMeta(
    qcStatement         * aStatement,
    UInt                  aUserID,
    UInt                  aViewID,
    qsRelatedObjects    * aRelatedObjList )
{
/***********************************************************************
 *
 * Description :
 *      SYS_VIEW_RELATED_ �� �Է�
 *
 * Implementation :
 *      1. SYS_VIEW_RELATED_ ��Ÿ ���̺� view ������ ���õ�
 *         ������Ʈ �Է�
 *
 ***********************************************************************/

#define IDE_FN "qdv::insertIntoViewRelatedMeta"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qdv::insertIntoViewRelatedMeta"));

    UInt                sRelUserID ;
    SChar               sRelObjectName[ QC_MAX_OBJECT_NAME_LEN + 1 ];
    SChar             * sSqlStr;
    vSLong              sRowCnt;

    // BUG-25587
    // public synonym�� ����Ѵ�.
    if ( ( aRelatedObjList->objectType == QS_SYNONYM ) &&
         ( aRelatedObjList->userName.size == 0 ) )
    {
        sRelUserID = QC_PUBLIC_USER_ID;
    }
    else
    {
        IDE_TEST( qcmUser::getUserID( aStatement,
                                      aRelatedObjList->userName.name,
                                      aRelatedObjList->userName.size,
                                      & sRelUserID )
                  != IDE_SUCCESS );
    }

    idlOS::memcpy(sRelObjectName,
                  aRelatedObjList-> objectName.name,
                  aRelatedObjList-> objectName.size);
    sRelObjectName[ aRelatedObjList->objectName.size ] = '\0';

    IDU_FIT_POINT( "qdv::insertIntoViewRelatedMeta::alloc::sSqlStr",
                    idERR_ABORT_InsufficientMemory );

    IDE_TEST(STRUCT_ALLOC_WITH_SIZE(aStatement->qmxMem,
                                    SChar,
                                    QD_MAX_SQL_LENGTH,
                                    &sSqlStr)
             != IDE_SUCCESS);

    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                     "INSERT INTO SYS_VIEW_RELATED_ VALUES ( "
                     QCM_SQL_UINT32_FMT", "
                     QCM_SQL_UINT32_FMT", "
                     QCM_SQL_INT32_FMT", "
                     QCM_SQL_CHAR_FMT", "
                     QCM_SQL_INT32_FMT") ",
                     aUserID,
                     aViewID,
                     sRelUserID,
                     sRelObjectName,
                     aRelatedObjList->objectType);

    IDE_TEST(qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                sSqlStr,
                                & sRowCnt ) != IDE_SUCCESS);

    IDE_TEST_RAISE(sRowCnt != 1, ERR_META_CRASH);

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_META_CRASH)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCM_META_CRASH));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}


IDE_RC qdv::makeParseTreeForViewInSelect(
    qcStatement * aStatement,
    qmsTableRef * aTableRef )
{
/***********************************************************************
 *
 * Description :
 *    �� ���� �������� select �κ��� �Ľ�
 *
 * Implementation :
 *    1. �� ������(stmt text)�� ���� ���ϱ�
 *    2. stmt text �� ���Ѵ�
 *    3. qcStatement �� �Ҵ�
 *    4. 2 ���� ���� text ���� select �κи� �����Ͽ� 3 �� qcStatment ��
 *       stmtText �� ī���Ѵ�
 *    5. �Ľ�
 *    6. set parse tree
 *
 ***********************************************************************/

#define IDE_FN "qdv::makeParseTreeForViewInSelect"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qdv::makeParseTreeForViewInSelect"));

    qcStatement * sStatement;
    SChar       * sStmtText;
    UInt          sStmtTextLen;
    qdTableParseTree    * sCreateViewParseTree;

    smiObject::getObjectInfoSize( aTableRef->tableInfo->tableHandle,
                                  &sStmtTextLen );
    
    IDU_FIT_POINT( "qdv::makeParseTreeForViewInSelect::alloc::sStmtText",
                    idERR_ABORT_InsufficientMemory );

    IDE_TEST(QC_QMP_MEM(aStatement)->alloc( sStmtTextLen+1, (void **)(&sStmtText) )
             != IDE_SUCCESS);

    // get stmt text
    smiObject::getObjectInfo( aTableRef->tableInfo->tableHandle,
                              (void **)(&sStmtText) );

    IDE_TEST_RAISE( sStmtText == NULL, ERR_VIEW_NOT_FOUND );

    // fix BUG-18808
    sStmtText[sStmtTextLen] = '\0';

    // alloc qcStatement for view
    IDU_FIT_POINT( "qdv::makeParseTreeForViewInSelect::alloc::sStatement",
                    idERR_ABORT_InsufficientMemory );

    IDE_TEST(STRUCT_ALLOC(QC_QMP_MEM(aStatement), qcStatement, &sStatement)
             != IDE_SUCCESS);

    // set meber of qcStatement
    idlOS::memcpy( sStatement, aStatement, ID_SIZEOF(qcStatement) );

    // myPlan�� �缳���Ѵ�.
    sStatement->myPlan = & sStatement->privatePlan;
    sStatement->myPlan->planEnv = NULL;

    IDU_FIT_POINT( "qdv::makeParseTreeForViewInSelect::alloc::stmtText",
                    idERR_ABORT_InsufficientMemory );

    IDE_TEST(QC_QMP_MEM(aStatement)->alloc( sStmtTextLen+1,
                                            (void **)(&(sStatement->myPlan->stmtText)) )
             != IDE_SUCCESS);

    idlOS::memcpy(sStatement->myPlan->stmtText, sStmtText, sStmtTextLen);
    // fix BUG-18808
    sStatement->myPlan->stmtText[sStmtTextLen] = '\0';
    sStatement->myPlan->stmtTextLen = sStmtTextLen;

    sStatement->myPlan->parseTree   = NULL;
    sStatement->myPlan->plan        = NULL;

    // parsing view
    IDE_TEST(qcpManager::parseIt( sStatement ) != IDE_SUCCESS);

    sCreateViewParseTree = (qdTableParseTree *)(sStatement->myPlan->parseTree);

    // set parse tree
    aTableRef->view = sCreateViewParseTree->select;

    // planEnv�� �缳���Ѵ�.
    aTableRef->view->myPlan->planEnv = aStatement->myPlan->planEnv;
    
    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_VIEW_NOT_FOUND )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qdv::makeParseTreeForViewInSelect",
                                  "View not found" ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qdv::makeParseTreeForAlter(
    qcStatement     * aStatement,
    qcmTableInfo    * aTableInfo )
{
/***********************************************************************
 *
 * Description :
 *    recompile �� �� �������� ���ؼ� �Ľ�
 *
 * Implementation :
 *    1. �� ������(stmt text)�� ���� ���ϱ�
 *    2. stmt text �� ���Ѵ�
 *    3. qcStatement �� �Ҵ�
 *    4. 2 ���� ���� text �� 3 �� qcStatment �� stmtText �� �ο��Ѵ�
 *    5. �Ľ�
 *    6. set CREATE VIEW statement pointer
 *
 ***********************************************************************/

#define IDE_FN "qdv::makeParseTreeForAlter"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qdv::makeParseTreeForAlter"));

    qcStatement         * sStatement;
    qdTableParseTree    * sAlterParseTree;
    SChar               * sStmtText;
    UInt                  sStmtTextLen;

    sAlterParseTree = (qdTableParseTree *)aStatement->myPlan->parseTree;

    // get the size of stmt text
    smiObject::getObjectInfoSize( aTableInfo->tableHandle,
                                  &sStmtTextLen );

    IDU_FIT_POINT( "qdv::makeParseTreeForAlter::alloc::sStmtText",
                    idERR_ABORT_InsufficientMemory );

    IDE_TEST(QC_QMP_MEM(aStatement)->alloc( sStmtTextLen + 1, (void **)(&sStmtText) )
             != IDE_SUCCESS);

    // get stmt text
    smiObject::getObjectInfo( aTableInfo->tableHandle,
                              (void **)(&sStmtText) );

    // BUG-42581 valgrind warning
    // string null termination
    sStmtText[sStmtTextLen] = '\0';

    // alloc qcStatement for view
    IDU_FIT_POINT( "qdv::makeParseTreeForAlter::alloc::sStatement",
                    idERR_ABORT_InsufficientMemory );

    IDE_TEST(STRUCT_ALLOC(QC_QMP_MEM(aStatement), qcStatement, &sStatement)
             != IDE_SUCCESS);

    // set meber of qcStatement
    idlOS::memcpy( sStatement, aStatement, ID_SIZEOF(qcStatement) );

    // myPlan�� �缳���Ѵ�.
    sStatement->myPlan = & sStatement->privatePlan;
    sStatement->myPlan->planEnv = NULL;

    // template�� �缳���Ѵ�.
    QC_SHARED_TMPLATE(sStatement) = QC_PRIVATE_TMPLATE(sStatement);
    QC_PRIVATE_TMPLATE(sStatement) = NULL;    
    
    sStatement->myPlan->stmtText    = sStmtText;
    sStatement->myPlan->stmtTextLen = sStmtTextLen;
    sStatement->myPlan->parseTree   = NULL;
    sStatement->myPlan->plan        = NULL;

    // parsing view
    IDE_TEST(qcpManager::parseIt( sStatement ) != IDE_SUCCESS);

    // set CREATE VIEW statement pointer
    sAlterParseTree->select = sStatement;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}


IDE_RC qdv::makeOneIntegerQcmColumn(
    qcStatement     * aStatement,
    qcmColumn      ** aColumn )
{
#define IDE_FN "qdv::makeOneIntegerQcmColumn"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qdv::makeOneIntegerQcmColumn"));

    qcmColumn       * sColumn;

    IDU_FIT_POINT( "qdv::makeOneIntegerQcmColumn::alloc::sColumn",
                    idERR_ABORT_InsufficientMemory );

    IDE_TEST(STRUCT_ALLOC(QC_QMP_MEM(aStatement), qcmColumn, &sColumn)
             != IDE_SUCCESS);

    // BUG-16233
    IDU_FIT_POINT( "qdv::makeOneIntegerQcmColumn::alloc::basicInfo",
                    idERR_ABORT_InsufficientMemory );

    IDE_TEST(STRUCT_CRALLOC(QC_QMP_MEM(aStatement), mtcColumn, &(sColumn->basicInfo))
             != IDE_SUCCESS);

    // sColumn�� �ʱ�ȭ
    // : dataType�� integer, language��  session�� language�� ����
    IDE_TEST( mtc::initializeColumn(
                  sColumn->basicInfo,
                  MTD_INTEGER_ID,
                  0,
                  0,
                  0 )
              != IDE_SUCCESS );

    // set offset of first column
    sColumn->basicInfo->column.offset = smiGetRowHeaderSize(SMI_TABLE_MEMORY);

    sColumn->flag            = 0;
    idlOS::memset(sColumn->name, 0x00, QC_MAX_OBJECT_NAME_LEN + 1 );
    sColumn->name[0]         = 'X';
    SET_EMPTY_POSITION(sColumn->namePos);
    sColumn->defaultValue    = NULL;
    sColumn->next            = NULL;
    sColumn->defaultValueStr = NULL;

    *aColumn = sColumn;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}
