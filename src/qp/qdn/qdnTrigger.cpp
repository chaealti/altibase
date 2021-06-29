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
 * $Id: qdnTrigger.cpp 90192 2021-03-12 02:01:03Z jayce.park $
 *
 * Description :
 *
 *     [PROJ-1359] Trigger
 *
 *     Trigger ó���� ���� �Լ�
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#include <mtuProperty.h>
#include <qcg.h>
#include <qdnTrigger.h>
#include <qcmCache.h>
#include <qcmUser.h>
#include <qcmTrigger.h>
#include <qdbCommon.h>
#include <qdpPrivilege.h>
#include <qcpManager.h>
#include <qcmProc.h>
#include <qmv.h>
#include <qsv.h>
#include <qsx.h>
#include <qsxRelatedProc.h>
#include <qsvProcVar.h>
#include <qcsModule.h>
#include <qdpRole.h>

extern mtdModule    mtdBoolean;

IDE_RC
qdnTrigger::parseCreate( qcStatement * aStatement )
{
/***********************************************************************
 *
 * Description :
 *    ������ ���� Post Parsing �۾��� ������.
 *    - Syntax Validation
 *    - ������ ����� �˻�.
 *    - CREATE TRIGGER�� ���� �ΰ� Parsing �۾��� ������.
 *    - Action Body�� Post Parsing
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qdnTrigger::parseCreate"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qdnCreateTriggerParseTree * sParseTree;
    qdnTriggerRef             * sRefer;
    qdnTriggerEventTypeList   * sEventTypeList;
    qdnTriggerEventTypeList   * sEventTypeHeader;

    sParseTree = (qdnCreateTriggerParseTree *) aStatement->myPlan->parseTree;

    //---------------------------------------------------
    // Syntax Validation
    //---------------------------------------------------

    // Action Granularity�� FOR EACH ROW�� �ƴ� ��쿡��
    // RERENCING ������ ����� �� ����.

    if ( sParseTree->actionCond.actionGranularity !=
         QCM_TRIGGER_ACTION_EACH_ROW )
    {
        IDE_TEST_RAISE( sParseTree->triggerReference != NULL,
                        err_invalid_referencing );
    }
    else
    {
        // Nothing To Do
    }

    //---------------------------------------------------
    // parseTree->triggerEvent->eventTypelist�� insert or
    // delete or update.. �� ������ ����.
    //---------------------------------------------------

    sEventTypeHeader = sParseTree->triggerEvent.eventTypeList;

    for (sEventTypeList = sParseTree->triggerEvent.eventTypeList->next;
         sEventTypeList != NULL;
         sEventTypeList = sEventTypeList->next)
    {
        sEventTypeHeader->eventType |= sEventTypeList->eventType;
        if (sEventTypeList->updateColumns != NULL)
        {
            sEventTypeHeader->updateColumns = sEventTypeList->updateColumns;
        }

        else
        {
            // do nothing
        }
    }

    //---------------------------------------------------
    // ������ ����� �˻�
    //---------------------------------------------------

    // TABLE referencing ������ �������� ����.
    for ( sRefer = sParseTree->triggerReference;
          sRefer != NULL;
          sRefer = sRefer->next )
    {
        switch ( sRefer->refType )
        {
            case QCM_TRIGGER_REF_OLD_ROW:
            case QCM_TRIGGER_REF_NEW_ROW:
                // Valid Referencing Type
                break;
            default:
                IDE_RAISE( err_unsupported_trigger_referencing );
                break;
        }
    }

    //---------------------------------------------------
    // Granularity��  FOR EACH ROW�� ��� Validation�� ����
    // �ΰ� ������ ������ �־�� �Ѵ�.
    //---------------------------------------------------

    switch ( sParseTree->actionCond.actionGranularity )
    {
        case QCM_TRIGGER_ACTION_EACH_ROW:
            IDE_TEST( addGranularityInfo( sParseTree )
                      != IDE_SUCCESS );
            IDE_TEST( addActionBodyInfo( aStatement, sParseTree )
                      != IDE_SUCCESS );
            break;
        case QCM_TRIGGER_ACTION_EACH_STMT:
            sParseTree->actionBody.paraDecls = NULL;
            break;
        default:
            IDE_DASSERT(0);
            break;
    }

    aStatement->spvEnv->createProc              = &sParseTree->actionBody;
    aStatement->spvEnv->createProc->stmtText    = aStatement->myPlan->stmtText;
    aStatement->spvEnv->createProc->stmtTextLen = aStatement->myPlan->stmtTextLen;

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_invalid_referencing);
    {
        IDE_SET( ideSetErrorCode(
                     qpERR_ABORT_TRIGGER_INVALID_REFERENCING_GRANULARITY ));
    }
    IDE_EXCEPTION(err_unsupported_trigger_referencing);
    {
        IDE_SET( ideSetErrorCode(
                     qpERR_ABORT_TRIGGER_UNSUPPORTED_EVENT_TIMING ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qdnTrigger::validateCreate( qcStatement * aStatement )
{
/***********************************************************************
 *
 * Description :
 *    CREATE TRIGGER�� ���� Validation�� ������.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qdnTrigger::validateCreate"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qdnCreateTriggerParseTree * sParseTree;
    volatile UInt               sSessionUserID;

    IDE_FT_BEGIN();

    IDU_FIT_POINT_FATAL( "qdnTrigger::validateCreate::__FT__" );

    sParseTree = (qdnCreateTriggerParseTree *) aStatement->myPlan->parseTree;

    // ���� session userID ����
    sSessionUserID = QCG_GET_SESSION_USER_ID( aStatement );
    IDU_FIT_POINT_FATAL( "qdnTrigger::validateCreate::__FT__::SessionUserID" );

    // Action Body�� Validtion�� ���Ͽ� �ش� ������ ������ �־�� ��.
    aStatement->spvEnv->createProc = & sParseTree->actionBody;

    // BUG-24570
    // Trigger User�� ���� ���� ����
    IDE_TEST( setTriggerUser( aStatement, sParseTree ) != IDE_SUCCESS );

    //----------------------------------------
    // CREATE TRIGGER ������ Validation
    //----------------------------------------

    // BUG-24408
    // trigger�� �����ڷ� validation�Ѵ�.
    QCG_SET_SESSION_USER_ID( aStatement, sParseTree->triggerUserID );

    // Trigger�� ��� �������� Host ������ ������ �� ����.
    IDE_TEST( qsv::checkHostVariable( aStatement ) != IDE_SUCCESS);

    // Trigger ������ ���� ���� �˻�
    IDE_TEST( valPrivilege( aStatement, sParseTree ) != IDE_SUCCESS );

    // Trigger Name�� ���� Validation
    IDE_TEST( valTriggerName( aStatement, sParseTree ) != IDE_SUCCESS );

    // Trigger Table�� ���� Validation
    IDE_TEST( valTableName( aStatement, sParseTree ) != IDE_SUCCESS );

    // Trigger Event�� Referncing �� ���� Validation
    IDE_TEST( valEventReference( aStatement, sParseTree ) != IDE_SUCCESS );

    // Action Condition�� ���� Validation
    IDE_TEST( valActionCondition( aStatement, sParseTree ) != IDE_SUCCESS );

    // Action Body �� ���� Validation
    IDE_TEST( valActionBody( aStatement, sParseTree ) != IDE_SUCCESS );

    //PROJ-1888 INSTEAD OF TRIGGER ������� �˻�
    IDE_TEST( valInsteadOfTrigger( sParseTree ) != IDE_SUCCESS );

    // session userID�� ����
    QCG_SET_SESSION_USER_ID( aStatement, sSessionUserID );

    IDE_FT_END();

    return IDE_SUCCESS;

    IDE_EXCEPTION_SIGNAL()
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_FAULT_TOLERATED ) );
    }
    IDE_EXCEPTION_END;

    IDE_FT_EXCEPTION_BEGIN();

    // session userID�� ����
    QCG_SET_SESSION_USER_ID( aStatement, sSessionUserID );

    IDE_FT_EXCEPTION_END();

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qdnTrigger::validateReplace( qcStatement * aStatement )
{
/***********************************************************************
 *
 * Description :
 *    CREATE TRIGGER�� ���� Validation�� ������.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qdnTrigger::validateReplace"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qdnCreateTriggerParseTree * sParseTree;
    volatile UInt               sSessionUserID;

    UInt                        sTableID;
    idBool                      sExist;

    IDE_FT_BEGIN();

    IDU_FIT_POINT_FATAL( "qdnTrigger::validateReplace::__FT__" );

    sParseTree = (qdnCreateTriggerParseTree *) aStatement->myPlan->parseTree;

    // ���� session userID ����
    sSessionUserID = QCG_GET_SESSION_USER_ID( aStatement );

    IDU_FIT_POINT_FATAL( "qdnTrigger::validateReplace::__FT__::SessionUserID" );

    // Action Body�� Validtion�� ���Ͽ� �ش� ������ ������ �־�� ��.
    aStatement->spvEnv->createProc = & sParseTree->actionBody;

    // BUG-24570
    // Trigger User�� ���� ���� ����
    IDE_TEST( setTriggerUser( aStatement, sParseTree ) != IDE_SUCCESS );

    //----------------------------------------
    // CREATE TRIGGER ������ Validation
    //----------------------------------------

    // BUG-24408
    // trigger�� �����ڷ� validation�Ѵ�.
    QCG_SET_SESSION_USER_ID( aStatement, sParseTree->triggerUserID );

    // Trigger�� ��� �������� Host ������ ������ �� ����.
    IDE_TEST( qsv::checkHostVariable( aStatement ) != IDE_SUCCESS);

    // Trigger ������ ���� ���� �˻�
    IDE_TEST( valPrivilege( aStatement, sParseTree ) != IDE_SUCCESS );

    IDE_TEST(qcmTrigger::getTriggerOID( aStatement,
                                        sParseTree->triggerUserID,
                                        sParseTree->triggerNamePos,
                                        & (sParseTree->triggerOID),
                                        & sTableID,
                                        & sExist )
             != IDE_SUCCESS );
    if (sExist == ID_TRUE)
    {
        // To Fix BUG-20948
        // ���� ���̺��� TABLEID�� �����Ѵ�.
        sParseTree->orgTableID = sTableID;

        // replace trigger
        sParseTree->common.execute = qdnTrigger::executeReplace;

        // Trigger SCN ����
        IDE_TEST(getTriggerSCN( sParseTree->triggerOID,
                                &(sParseTree->triggerSCN) )
                 != IDE_SUCCESS);
    }
    else
    {
        // create trigger
        sParseTree->triggerOID = 0;
        sParseTree->common.execute = qdnTrigger::executeCreate;
    }

    // Trigger Name�� ����
    QC_STR_COPY( sParseTree->triggerName, sParseTree->triggerNamePos );

    // Trigger Table�� ���� Validation
    IDE_TEST( valTableName( aStatement, sParseTree ) != IDE_SUCCESS );

    //To Fix BUG-20948
    if ( sExist == ID_TRUE )
    {
        if ( sParseTree->tableID != sParseTree->orgTableID )
        {
            //Trigger�� ���� Table�� ����Ǿ��� ����� ó��
            IDE_TEST( valOrgTableName ( aStatement, sParseTree ) != IDE_SUCCESS );
        }
        else
        {
            //Nothing To Do.
        }
    }
    else
    {
        //Nothing To Do.
    }

    // Trigger Event�� Referncing �� ���� Validation
    IDE_TEST( valEventReference( aStatement, sParseTree ) != IDE_SUCCESS );

    // Action Condition�� ���� Validation
    IDE_TEST( valActionCondition( aStatement, sParseTree ) != IDE_SUCCESS );

    // Action Body �� ���� Validation
    IDE_TEST( valActionBody( aStatement, sParseTree ) != IDE_SUCCESS );

    //PROJ-1888 INSTEAD OF TRIGGER ������� �˻�
    IDE_TEST( valInsteadOfTrigger( sParseTree ) != IDE_SUCCESS );

    // session userID�� ����
    QCG_SET_SESSION_USER_ID( aStatement, sSessionUserID );

    IDE_FT_END();

    return IDE_SUCCESS;

    IDE_EXCEPTION_SIGNAL()
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_FAULT_TOLERATED ) );
    }
    IDE_EXCEPTION_END;

    IDE_FT_EXCEPTION_BEGIN();

    // session userID�� ����
    QCG_SET_SESSION_USER_ID( aStatement, sSessionUserID );

    IDE_FT_EXCEPTION_END();

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qdnTrigger::validateRecompile( qcStatement * aStatement )
{
/***********************************************************************
 *
 * Description :
 *    Recompile�� ���� Validation�� ������.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qdnTrigger::validateRecompile"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qdnCreateTriggerParseTree * sParseTree;
    UInt                        sSessionUserID;

    sParseTree = (qdnCreateTriggerParseTree *) aStatement->myPlan->parseTree;

    // ���� session userID ����
    sSessionUserID = QCG_GET_SESSION_USER_ID( aStatement );

    // Action Body�� Validtion�� ���Ͽ� �ش� ������ ������ �־�� ��.
    aStatement->spvEnv->createProc = & sParseTree->actionBody;

    // BUG-24570
    // Trigger User�� ���� ���� ����
    IDE_TEST( setTriggerUser( aStatement, sParseTree ) != IDE_SUCCESS );

    //----------------------------------------
    // CREATE TRIGGER ������ Validation
    //----------------------------------------

    // BUG-24408
    // trigger�� �����ڷ� validation�Ѵ�.
    QCG_SET_SESSION_USER_ID( aStatement, sParseTree->triggerUserID );

    // Trigger�� ��� �������� Host ������ ������ �� ����.
    IDE_TEST( qsv::checkHostVariable( aStatement ) != IDE_SUCCESS);

    // Trigger ������ ���� ���� �˻�
    IDE_TEST( valPrivilege( aStatement, sParseTree ) != IDE_SUCCESS );

    // Trigger Name�� ���� Validation
    // Recompile �����̹Ƿ� �ݵ�� �����ؾ� ��.
    IDE_TEST( reValTriggerName( aStatement, sParseTree ) != IDE_SUCCESS );

    // Trigger Table�� ���� Validation
    IDE_TEST( valTableName( aStatement, sParseTree ) != IDE_SUCCESS );

    // Trigger Event�� Referncing �� ���� Validation
    IDE_TEST( valEventReference( aStatement, sParseTree ) != IDE_SUCCESS );

    // Action Condition�� ���� Validation
    IDE_TEST( valActionCondition( aStatement, sParseTree ) != IDE_SUCCESS );

    // Action Body �� ���� Validation
    IDE_TEST( valActionBody( aStatement, sParseTree ) != IDE_SUCCESS );

    // session userID�� ����
    QCG_SET_SESSION_USER_ID( aStatement, sSessionUserID );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    // session userID�� ����
    QCG_SET_SESSION_USER_ID( aStatement, sSessionUserID );

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qdnTrigger::optimizeCreate( qcStatement * aStatement )
{
/***********************************************************************
 *
 * Description :
 *    CREATE TRIGGER�� ���� Optimization�� ������.
 *
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qdnTrigger::optimizeCreate"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qdnCreateTriggerParseTree * sParseTree;
    volatile UInt               sSessionUserID;

    IDE_FT_BEGIN();

    IDU_FIT_POINT_FATAL( "qdnTrigger::optimizeCreate::__FT__" );

    sParseTree = (qdnCreateTriggerParseTree *) aStatement->myPlan->parseTree;

    // ���� session userID ����
    sSessionUserID = QCG_GET_SESSION_USER_ID( aStatement );

    IDU_FIT_POINT_FATAL( "qdnTrigger::optimizeCreate::__FT__::SessionUserID" );

    //---------------------------------------------------
    // Action Body�� Optimization
    //---------------------------------------------------

    // BUG-24408
    // trigger�� �����ڷ� optimize�Ѵ�.
    QCG_SET_SESSION_USER_ID( aStatement, sParseTree->triggerUserID );

    IDE_TEST( sParseTree->actionBody.block->common.
              optimize( aStatement,
                        (qsProcStmts *) (sParseTree->actionBody.block) )
              != IDE_SUCCESS);

    // session userID�� ����
    QCG_SET_SESSION_USER_ID( aStatement, sSessionUserID );

    IDE_FT_END();

    return IDE_SUCCESS;

    IDE_EXCEPTION_SIGNAL()
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_FAULT_TOLERATED ) );
    }
    IDE_EXCEPTION_END;

    IDE_FT_EXCEPTION_BEGIN();

    // session userID�� ����
    QCG_SET_SESSION_USER_ID( aStatement, sSessionUserID );

    IDE_FT_EXCEPTION_END();

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qdnTrigger::executeCreate( qcStatement * aStatement )
{
/***********************************************************************
 *
 * Description :
 *    CREATE TRIGGER�� ���� Execution�� ������.
 *
 *    ������ PVO �������� ����� ������ Session�� �������� �����̴�.
 *    ����, Session�� ���������� ���� ������ �����ϱ� ���ؼ���
 *    �������� Memory ������ �Ҵ� �޾� ������ PVO�� �����Ͽ��� �Ѵ�.
 *
 *    �̿� ���� Recompile �۾��� ���� �ּ� �������� �����ϸ�,
 *    ���� DML�߻��� Trigger Object Cache ������ �̿��Ͽ� Recompile�ϰ� �ȴ�.
 *
 *    - Trigger Object ����(Trigger String�� ������ ����)
 *    - Trigger Object�� Trigger Object Cache ������ ����
 *      (CREATE TRIGGER ������ Server ���� ������ �����ȴ�)
 *    - ���� Meta Table�� ���� ���
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qdnTrigger::executeCreate"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    UInt                        sStage = 0;
    void                      * sTriggerHandle = NULL;
    qdnTriggerCache           * sCache = NULL;
    qdnCreateTriggerParseTree * sParseTree;
    smOID                       sTriggerOID;

    qcmPartitionInfoList      * sAllPartInfoList = NULL;
    qcmPartitionInfoList      * sOldPartInfoList = NULL;
    qcmPartitionInfoList      * sNewPartInfoList = NULL;
    qcmTableInfo              * sOldTableInfo    = NULL;
    qcmTableInfo              * sNewTableInfo    = NULL;
    void                      * sNewTableHandle  = NULL;
    smSCN                       sNewSCN          = SM_SCN_INIT;

    sParseTree = (qdnCreateTriggerParseTree *) aStatement->myPlan->parseTree;

    IDE_TEST( qcm::validateAndLockTable( aStatement,
                                         sParseTree->tableHandle,
                                         sParseTree->tableSCN,
                                         SMI_TABLE_LOCK_X )
              != IDE_SUCCESS );

    sOldTableInfo = sParseTree->tableInfo;

    if ( sOldTableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
    {
        IDE_TEST( qcmPartition::getPartitionInfoList( aStatement,
                                                      QC_SMI_STMT( aStatement ),
                                                      QC_QMX_MEM( aStatement ),
                                                      sOldTableInfo->tableID,
                                                      & sAllPartInfoList )
                  != IDE_SUCCESS );

        IDE_TEST( qcmPartition::validateAndLockPartitionInfoList( aStatement,
                                                                  sAllPartInfoList,
                                                                  SMI_TBSLV_DDL_DML, // TBS Validation �ɼ�
                                                                  SMI_TABLE_LOCK_X,
                                                                  smiGetDDLLockTimeOut((QC_SMI_STMT(aStatement))->getTrans()) )
                  != IDE_SUCCESS );

        // ���� ó���� ���Ͽ�, Lock�� ���� �Ŀ� Partition List�� �����Ѵ�.
        sOldPartInfoList = sAllPartInfoList;
    }
    else
    {
        /* Nothing to do */
    }

    //---------------------------------------
    // Trigger Object�� �����ϰ�
    // CREATE TRIGGER �� ���� ���� ��ü�� �����Ѵ�.
    //---------------------------------------

    IDE_TEST( createTriggerObject( aStatement, &sTriggerHandle )
              != IDE_SUCCESS );

    //---------------------------------------
    // Trigger Object��  Trigger Object Cache ���� ����
    //---------------------------------------

    sTriggerOID = smiGetTableId( sTriggerHandle ) ;

    // Trigger Handle��
    // Trigger Object Cache�� ���� �ʿ��� ������ �����Ѵ�.
    IDE_TEST( allocTriggerCache( sTriggerHandle,
                                 sTriggerOID,
                                 & sCache )
              != IDE_SUCCESS );

    sStage = 1;

    //---------------------------------------
    // Trigger�� ���� Meta Table�� ���� ����
    //---------------------------------------

    IDE_TEST( qcmTrigger::addMetaInfo( aStatement, sTriggerHandle )
              != IDE_SUCCESS );

    //---------------------------------------
    // Table �� Meta Cache ������ �籸����.
    //---------------------------------------

    IDE_TEST( qcm::touchTable(QC_SMI_STMT( aStatement ),
                              sParseTree->tableID,
                              SMI_TBSLV_DDL_DML )
              != IDE_SUCCESS);

    IDE_TEST(qcm::makeAndSetQcmTableInfo( QC_SMI_STMT( aStatement ),
                                          sParseTree->tableID,
                                          sParseTree->tableOID )
             != IDE_SUCCESS);

    IDE_TEST( qcm::getTableInfoByID( aStatement,
                                     sOldTableInfo->tableID,
                                     & sNewTableInfo,
                                     & sNewSCN,
                                     & sNewTableHandle )
              != IDE_SUCCESS );

    if ( sOldTableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
    {
        IDE_TEST( qcmPartition::touchPartitionInfoList( QC_SMI_STMT( aStatement ),
                                                        sOldPartInfoList )
                  != IDE_SUCCESS );

        IDE_TEST( qcmPartition::makeAndSetAndGetQcmPartitionInfoList( aStatement,
                                                                      sNewTableInfo,
                                                                      sOldPartInfoList,
                                                                      & sNewPartInfoList )
                  != IDE_SUCCESS );

        (void)qcmPartition::destroyQcmPartitionInfoList( sOldPartInfoList );
    }
    else
    {
        /* Nothing to do */
    }

    (void)qcm::destroyQcmTableInfo(sParseTree->tableInfo);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch ( sStage )
    {
        case 1:
            (void) freeTriggerCache( sCache );

        case 0:
            break;
    }

    (void)qcm::destroyQcmTableInfo( sNewTableInfo );
    (void)qcmPartition::destroyQcmPartitionInfoList( sNewPartInfoList );

    qcmPartition::restoreTempInfo( sOldTableInfo,
                                   sOldPartInfoList,
                                   NULL );

    return IDE_FAILURE;

#undef IDE_FN
}


IDE_RC
qdnTrigger::executeReplace( qcStatement * aStatement )
{
/***********************************************************************
 *
 * Description :
 *    CREATE TRIGGER�� ���� Execution�� ������.
 *
 *    ������ PVO �������� ����� ������ Session�� �������� �����̴�.
 *    ����, Session�� ���������� ���� ������ �����ϱ� ���ؼ���
 *    �������� Memory ������ �Ҵ� �޾� ������ PVO�� �����Ͽ��� �Ѵ�.
 *
 *    �̿� ���� Recompile �۾��� ���� �ּ� �������� �����ϸ�,
 *    ���� DML�߻��� Trigger Object Cache ������ �̿��Ͽ� Recompile�ϰ� �ȴ�.
 *
 *    - Trigger Object ����(Trigger String�� ������ ����)
 *    - Trigger Object�� Trigger Object Cache ������ ����
 *      (CREATE TRIGGER ������ Server ���� ������ �����ȴ�)
 *    - ���� Meta Table�� ���� ���
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qdnTrigger::executeReplace"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    UInt                        sStage = 0;
    void                      * sNewTriggerHandle = NULL;
    void                      * sOldTriggerHandle;
    qdnTriggerCache           * sOldTriggerCache;
    qdnTriggerCache           * sNewTriggerCache = NULL;
    qdnCreateTriggerParseTree * sParseTree;
    smOID                       sTriggerOID;

    qcmPartitionInfoList      * sAllPartInfoList    = NULL;
    qcmPartitionInfoList      * sOldPartInfoList    = NULL;
    qcmPartitionInfoList      * sNewPartInfoList    = NULL;
    qcmTableInfo              * sOldTableInfo       = NULL;
    qcmTableInfo              * sNewTableInfo       = NULL;
    qcmPartitionInfoList      * sOrgOldPartInfoList = NULL;
    qcmPartitionInfoList      * sOrgNewPartInfoList = NULL;
    qcmTableInfo              * sOrgOldTableInfo    = NULL;
    qcmTableInfo              * sOrgNewTableInfo    = NULL;
    void                      * sNewTableHandle     = NULL;
    smSCN                       sNewSCN             = SM_SCN_INIT;

    sParseTree = (qdnCreateTriggerParseTree *) aStatement->myPlan->parseTree;

    //---------------------------------------
    // TASK-2176
    // Table�� ���� Lock�� Ȯ���Ѵ�.
    //---------------------------------------

    IDE_TEST( qcm::validateAndLockTable(aStatement,
                                        sParseTree->tableHandle,
                                        sParseTree->tableSCN,
                                        SMI_TABLE_LOCK_X)
              != IDE_SUCCESS );

    sOldTableInfo = sParseTree->tableInfo;

    if ( sOldTableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
    {
        IDE_TEST( qcmPartition::getPartitionInfoList( aStatement,
                                                      QC_SMI_STMT( aStatement ),
                                                      QC_QMX_MEM( aStatement ),
                                                      sOldTableInfo->tableID,
                                                      & sAllPartInfoList )
                  != IDE_SUCCESS );

        IDE_TEST( qcmPartition::validateAndLockPartitionInfoList( aStatement,
                                                                  sAllPartInfoList,
                                                                  SMI_TBSLV_DDL_DML, // TBS Validation �ɼ�
                                                                  SMI_TABLE_LOCK_X,
                                                                  smiGetDDLLockTimeOut((QC_SMI_STMT(aStatement))->getTrans()) )
                  != IDE_SUCCESS );

        // ���� ó���� ���Ͽ�, Lock�� ���� �Ŀ� Partition List�� �����Ѵ�.
        sOldPartInfoList = sAllPartInfoList;
    }
    else
    {
        /* Nothing to do */
    }

    //------------------------------------------------------
    // BUG-20948
    // Comment : Replace�� Trigger�� TABLE ID�� ������ ���Ѵ�.
    //           TABLE ID�� �ٸ� ���, Trigger�� Meta Data�Ӹ��̳���
    //           ���� TABLE�� ����� Trigger ���� ���� �����ؾ� �Ѵ�.
    //------------------------------------------------------
    if ( sParseTree->orgTableID != sParseTree->tableID )
    {
        //Validate & Lock
        IDE_TEST( qcm::validateAndLockTable(aStatement,
                                            sParseTree->orgTableHandle,
                                            sParseTree->orgTableSCN,
                                            SMI_TABLE_LOCK_X)
                  != IDE_SUCCESS);

        sOrgOldTableInfo = sParseTree->orgTableInfo;

        if ( sOrgOldTableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
        {
            IDE_TEST( qcmPartition::getPartitionInfoList( aStatement,
                                                          QC_SMI_STMT( aStatement ),
                                                          QC_QMX_MEM( aStatement ),
                                                          sOrgOldTableInfo->tableID,
                                                          & sAllPartInfoList )
                      != IDE_SUCCESS );

            IDE_TEST( qcmPartition::validateAndLockPartitionInfoList( aStatement,
                                                                      sAllPartInfoList,
                                                                      SMI_TBSLV_DDL_DML, // TBS Validation �ɼ�
                                                                      SMI_TABLE_LOCK_X,
                                                                      smiGetDDLLockTimeOut((QC_SMI_STMT(aStatement))->getTrans()) )
                      != IDE_SUCCESS );

            // ���� ó���� ���Ͽ�, Lock�� ���� �Ŀ� Partition List�� �����Ѵ�.
            sOrgOldPartInfoList = sAllPartInfoList;
        }
        else
        {
            /* Nothing to do */
        }
    }
    else
    {
        //Nothing To Do.
    }

    //---------------------------------------
    // Trigger Object�� �����ϰ�
    // CREATE TRIGGER �� ���� ���� ��ü�� �����Ѵ�.
    //---------------------------------------

    IDE_TEST( createTriggerObject( aStatement, &sNewTriggerHandle )
              != IDE_SUCCESS );


    //---------------------------------------
    // Trigger Object��  Trigger Object Cache ���� ����
    //---------------------------------------

    sTriggerOID = smiGetTableId( sNewTriggerHandle );

    // Trigger Handle��
    // Trigger Object Cache�� ���� �ʿ��� ������ �����Ѵ�.
    IDE_TEST( allocTriggerCache( sNewTriggerHandle,
                                 sTriggerOID,
                                 & sNewTriggerCache )
              != IDE_SUCCESS );

    sStage = 1;

    //---------------------------------------
    // Ʈ���� SCN �˻� fix BUG-20479
    //---------------------------------------

    sOldTriggerHandle = (void *)smiGetTable( sParseTree->triggerOID );
    IDE_TEST_RAISE(smiValidateObjects(
                       sOldTriggerHandle,
                       sParseTree->triggerSCN)
                   != IDE_SUCCESS, ERR_TRIGGER_MODIFIED);

    //---------------------------------------
    // ���� Ʈ���� ���� ȹ�� : old trigger ĳ�� ���� ȹ��
    //---------------------------------------
    IDE_TEST( smiObject::getObjectTempInfo( sOldTriggerHandle,
                                            (void**)&sOldTriggerCache )
              != IDE_SUCCESS );

    //---------------------------------------
    // ���� Ʈ���� ��Ÿ���� ����.
    //---------------------------------------

    IDE_TEST( qcmTrigger::removeMetaInfo( aStatement, sParseTree->triggerOID )
              != IDE_SUCCESS );

    //---------------------------------------
    // NewTrigger�� ���� Meta Table�� ���� ����
    //---------------------------------------

    IDE_TEST( qcmTrigger::addMetaInfo( aStatement, sNewTriggerHandle )
              != IDE_SUCCESS );

    //---------------------------------------
    // Table �� Meta Cache ������ �籸����.
    //---------------------------------------

    IDE_TEST( qcm::touchTable(QC_SMI_STMT( aStatement ),
                              sParseTree->tableID,
                              SMI_TBSLV_DDL_DML )
              != IDE_SUCCESS);

    IDE_TEST(qcm::makeAndSetQcmTableInfo( QC_SMI_STMT( aStatement ),
                                          sParseTree->tableID,
                                          sParseTree->tableOID )
             != IDE_SUCCESS);

    IDE_TEST( qcm::getTableInfoByID( aStatement,
                                     sOldTableInfo->tableID,
                                     & sNewTableInfo,
                                     & sNewSCN,
                                     & sNewTableHandle )
              != IDE_SUCCESS );

    if ( sOldTableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
    {
        IDE_TEST( qcmPartition::touchPartitionInfoList( QC_SMI_STMT( aStatement ),
                                                        sOldPartInfoList )
                  != IDE_SUCCESS );

        IDE_TEST( qcmPartition::makeAndSetAndGetQcmPartitionInfoList( aStatement,
                                                                      sNewTableInfo,
                                                                      sOldPartInfoList,
                                                                      & sNewPartInfoList )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    //To Fix BUG-20948
    if ( sParseTree->orgTableID != sParseTree->tableID )
    {
        IDE_TEST( qcm::touchTable(QC_SMI_STMT( aStatement ),
                                  sParseTree->orgTableID,
                                  SMI_TBSLV_DDL_DML )
                  != IDE_SUCCESS);

        IDE_TEST(qcm::makeAndSetQcmTableInfo( QC_SMI_STMT( aStatement ),
                                              sParseTree->orgTableID,
                                              sParseTree->orgTableOID )
                 != IDE_SUCCESS);

        IDE_TEST( qcm::getTableInfoByID( aStatement,
                                         sOrgOldTableInfo->tableID,
                                         & sOrgNewTableInfo,
                                         & sNewSCN,
                                         & sNewTableHandle )
                  != IDE_SUCCESS );

        if ( sOrgOldTableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
        {
            IDE_TEST( qcmPartition::touchPartitionInfoList( QC_SMI_STMT( aStatement ),
                                                            sOrgOldPartInfoList )
                      != IDE_SUCCESS );

            IDE_TEST( qcmPartition::makeAndSetAndGetQcmPartitionInfoList( aStatement,
                                                                          sOrgNewTableInfo,
                                                                          sOrgOldPartInfoList,
                                                                          & sOrgNewPartInfoList )
                      != IDE_SUCCESS );
        }
        else
        {
            /* Nothing to do */
        }
    }

    //---------------------------------------
    // ���� Ʈ���� object ����
    //---------------------------------------

    IDE_TEST( dropTriggerObject( aStatement, sOldTriggerHandle )
              != IDE_SUCCESS );

    IDE_TEST(freeTriggerCache(sOldTriggerCache) != IDE_SUCCESS);

    if ( sOldTableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
    {
        (void)qcmPartition::destroyQcmPartitionInfoList( sOldPartInfoList );
    }
    else
    {
        /* Nothing to do */
    }

    (void)qcm::destroyQcmTableInfo(sParseTree->tableInfo);

    //To Fix BUG-20948
    if ( sParseTree->orgTableID != sParseTree->tableID )
    {
        if ( sOrgOldTableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
        {
            (void)qcmPartition::destroyQcmPartitionInfoList( sOrgOldPartInfoList );
        }
        else
        {
            /* Nothing to do */
        }

        //Free Old Table Infomation
        (void)qcm::destroyQcmTableInfo( sParseTree->orgTableInfo );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_TRIGGER_MODIFIED );
    {
        IDE_SET(ideSetErrorCode( qpERR_REBUILD_TRIGGER_INVALID ) );
    }
    IDE_EXCEPTION_END;

    switch ( sStage )
    {
        case 1:
            (void) freeTriggerCache( sNewTriggerCache );

        case 0:
            break;
    }

    (void)qcm::destroyQcmTableInfo( sNewTableInfo );
    (void)qcmPartition::destroyQcmPartitionInfoList( sNewPartInfoList );

    (void)qcm::destroyQcmTableInfo( sOrgNewTableInfo );
    (void)qcmPartition::destroyQcmPartitionInfoList( sOrgNewPartInfoList );

    qcmPartition::restoreTempInfo( sOldTableInfo,
                                   sOldPartInfoList,
                                   NULL );

    qcmPartition::restoreTempInfo( sOrgOldTableInfo,
                                   sOrgOldPartInfoList,
                                   NULL );

    return IDE_FAILURE;

#undef IDE_FN
}



IDE_RC
qdnTrigger::parseAlter( qcStatement * /* aStatement */ )
{
/***********************************************************************
 *
 * Description :
 *    ALTER TRIGGER�� ���� �ΰ� Parsing �۾��� ������.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qdnTrigger::parseAlter"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    // Nothing To Do

    return IDE_SUCCESS;

#undef IDE_FN
}

IDE_RC
qdnTrigger::validateAlter( qcStatement * aStatement )
{
/***********************************************************************
 *
 * Description :
 *    ALTER TRIGGER�� ���� Validation.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qdnTrigger::validateAlter"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qcuSqlSourceInfo  sqlInfo;

    idBool     sExist;

    qdnAlterTriggerParseTree * sParseTree;

    sParseTree = (qdnAlterTriggerParseTree *) aStatement->myPlan->parseTree;

    //----------------------------------------
    // Trigger�� User ���� ȹ��
    //----------------------------------------

    // Trigger User ������ ȹ��
    if ( QC_IS_NULL_NAME( sParseTree->triggerUserPos ) == ID_TRUE)
    {
        // User ID ȹ��
        sParseTree->triggerUserID = QCG_GET_SESSION_USER_ID(aStatement);
    }
    else
    {
        // User ID ȹ��
        IDE_TEST( qcmUser::getUserID( aStatement,
                                      sParseTree->triggerUserPos,
                                      & sParseTree->triggerUserID )
                  != IDE_SUCCESS);
    }

    //----------------------------------------
    // Trigger�� ���� ���� �˻�
    //----------------------------------------

    // �����ϴ� Trigger���� �˻�
    IDE_TEST( qcmTrigger::getTriggerOID( aStatement,
                                         sParseTree->triggerUserID,
                                         sParseTree->triggerNamePos,
                                         & sParseTree->triggerOID,
                                         & sParseTree->tableID,
                                         & sExist )
              != IDE_SUCCESS );

    if ( sExist == ID_FALSE )
    {
        sqlInfo.setSourceInfo( aStatement,
                               & sParseTree->triggerUserPos,
                               & sParseTree->triggerNamePos );

        IDE_RAISE( err_non_exist_trigger_name );
    }
    else
    {
        // �ش� Trigger�� ������.
    }

    //----------------------------------------
    // Alter Trigger ���� �˻�
    //----------------------------------------

    IDE_TEST( qdpRole::checkDDLAlterTriggerPriv( aStatement,
                                                 sParseTree->triggerUserID )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_non_exist_trigger_name);
    {
        (void) sqlInfo.init( aStatement->qmeMem );
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_TRIGGER_NOT_EXIST,
                            sqlInfo.getErrMessage() ));
        (void) sqlInfo.fini();
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qdnTrigger::optimizeAlter( qcStatement * /* aStatement */ )
{
/***********************************************************************
 *
 * Description :
 *    ALTER TRIGGER�� ���� Optimization.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qdnTrigger::optimizeAlter"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    // Nothing To Do

    return IDE_SUCCESS;

#undef IDE_FN
}

IDE_RC
qdnTrigger::executeAlter( qcStatement * aStatement )
{
/***********************************************************************
 *
 * Description :
 *    ALTER TRIGGER�� ����.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qdnTrigger::executeAlter"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    UInt              i;
    smSCN             sSCN;

    qcmTableInfo    * sTableInfo;
    qcmTriggerInfo  * sTriggerInfo;

    SChar           * sBuffer;
    vSLong             sRowCnt;
    void            * sTableHandle;

    qdnAlterTriggerParseTree * sParseTree;

    //----------------------------------------
    // �⺻ ������ ȹ��
    //----------------------------------------

    sParseTree = (qdnAlterTriggerParseTree *) aStatement->myPlan->parseTree;

    IDE_TEST(qcm::getTableInfoByID( aStatement,
                                    sParseTree->tableID,
                                    & sTableInfo,
                                    & sSCN,
                                    & sTableHandle )
             != IDE_SUCCESS);

    IDE_TEST(qcm::validateAndLockTable(aStatement,
                                       sTableHandle,
                                       sSCN,
                                       SMI_TABLE_LOCK_X)
             != IDE_SUCCESS);

    for ( i = 0; i < sTableInfo->triggerCount; i++ )
    {
        if ( sTableInfo->triggerInfo[i].triggerOID ==
             sParseTree->triggerOID )
        {
            break;
        }
    }

    // �ݵ�� �ش� Trigger ������ �����ؾ� ��.
    IDE_DASSERT( i < sTableInfo->triggerCount );

    sTriggerInfo = & sTableInfo->triggerInfo[i];

    //----------------------------------------
    // ALTER TRIGGER�� ����
    //----------------------------------------

    switch ( sParseTree->option )
    {
        //--------------------------------------------
        // ENABLE, DISABLE :
        //    Table Lock�� ��� �����ϱ� ������
        //    ������ Meta Cache�� �籸������ �ʰ� ���� �����Ų��.
        //--------------------------------------------
        case QDN_TRIGGER_ALTER_ENABLE:

            IDU_LIMITPOINT("qdnTrigger::executeAlter::malloc1");
            IDE_TEST( aStatement->qmxMem->alloc( QD_MAX_SQL_LENGTH,
                                                 (void**) & sBuffer )
                      != IDE_SUCCESS );

            //---------------------------------------
            // SYS_TRIGGERS_ ���� ���� ����
            // UPDATE SYS_TRIGGERS_ SET IS_ENABLE = QCM_TRIGGER_ENABLE
            //        WHERE TRIGGER_OID = triggerOID;
            //---------------------------------------

            idlOS::snprintf(
                sBuffer,
                QD_MAX_SQL_LENGTH,
                "UPDATE SYS_TRIGGERS_ SET "
                "  IS_ENABLE = "QCM_SQL_INT32_FMT             // 0. IS_ENABLE
                ", LAST_DDL_TIME = SYSDATE "
                "WHERE TRIGGER_OID = "QCM_SQL_BIGINT_FMT,     // 1. TRIGGER_OID
                QCM_TRIGGER_ENABLE,                           // 0.
                QCM_OID_TO_BIGINT( sParseTree->triggerOID ) );// 1.

            IDE_TEST( qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                         sBuffer,
                                         & sRowCnt )
                      != IDE_SUCCESS );

            IDE_DASSERT( sRowCnt == 1 );

            //----------------------------------
            // Meta Cache�� ENABLE ����
            //----------------------------------

            sTriggerInfo->enable = QCM_TRIGGER_ENABLE;

            break;

        case QDN_TRIGGER_ALTER_DISABLE:

            IDU_LIMITPOINT("qdnTrigger::executeAlter::malloc2");
            IDE_TEST( aStatement->qmxMem->alloc( QD_MAX_SQL_LENGTH,
                                                 (void**) & sBuffer )
                      != IDE_SUCCESS );

            //---------------------------------------
            // SYS_TRIGGERS_ ���� ���� ����
            // UPDATE SYS_TRIGGERS_ SET IS_ENABLE = QCM_TRIGGER_DISABLE
            //        WHERE TRIGGER_OID = triggerOID;
            //---------------------------------------

            idlOS::snprintf(
                sBuffer, QD_MAX_SQL_LENGTH,
                "UPDATE SYS_TRIGGERS_ SET "
                "  IS_ENABLE = "QCM_SQL_INT32_FMT              // 0. IS_ENABLE
                ", LAST_DDL_TIME = SYSDATE "
                "WHERE TRIGGER_OID = "QCM_SQL_BIGINT_FMT,     // 1. TRIGGER_OID
                QCM_TRIGGER_DISABLE,                          // 0.
                QCM_OID_TO_BIGINT( sParseTree->triggerOID ) );// 1.

            IDE_TEST( qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                         sBuffer,
                                         & sRowCnt )
                      != IDE_SUCCESS );

            IDE_DASSERT( sRowCnt == 1 );

            //----------------------------------
            // Meta Cache�� DISABLE ����
            //----------------------------------

            sTriggerInfo->enable = QCM_TRIGGER_DISABLE;

            break;

        case QDN_TRIGGER_ALTER_COMPILE:

            //----------------------------------
            // Recompile�� �����Ѵ�.
            //----------------------------------

            IDE_TEST( recompileTrigger( aStatement, sTriggerInfo )
                      != IDE_SUCCESS );
            break;
        default:
            IDE_DASSERT(0);
            break;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qdnTrigger::parseDrop( qcStatement * /* aStatement */ )
{
/***********************************************************************
 *
 * Description :
 *    DROP TRIGGER�� ���� �ΰ� Parsing �۾��� ������.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qdnTrigger::parseDrop"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    // Nothing To Do

    return IDE_SUCCESS;

#undef IDE_FN
}

IDE_RC
qdnTrigger::validateDrop( qcStatement * aStatement )
{
/***********************************************************************
 *
 * Description :
 *    DROP TRIGGER�� ���� Validation.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qdnTrigger::validateDrop"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qcuSqlSourceInfo  sqlInfo;

    idBool                sExist;
    
    qdnDropTriggerParseTree * sParseTree;

    sParseTree = (qdnDropTriggerParseTree *) aStatement->myPlan->parseTree;
    
    //----------------------------------------
    // Trigger�� User ���� ȹ��
    //----------------------------------------

    // Trigger User ������ ȹ��
    if ( QC_IS_NULL_NAME( sParseTree->triggerUserPos ) == ID_TRUE)
    {
        // User ID ȹ��
        sParseTree->triggerUserID = QCG_GET_SESSION_USER_ID(aStatement);
    }
    else
    {
        // User ID ȹ��
        IDE_TEST( qcmUser::getUserID( aStatement,
                                      sParseTree->triggerUserPos,
                                      & sParseTree->triggerUserID )
                  != IDE_SUCCESS);
    }

    //----------------------------------------
    // Trigger�� ���� ���� �˻�
    //----------------------------------------

    // �����ϴ� Trigger���� �˻�
    IDE_TEST( qcmTrigger::getTriggerOID( aStatement,
                                         sParseTree->triggerUserID,
                                         sParseTree->triggerNamePos,
                                         & sParseTree->triggerOID,
                                         & sParseTree->tableID,
                                         & sExist )
              != IDE_SUCCESS );

    if ( sExist == ID_FALSE )
    {
        sqlInfo.setSourceInfo( aStatement,
                               & sParseTree->triggerUserPos,
                               & sParseTree->triggerNamePos );

        IDE_RAISE( err_non_exist_trigger_name );
    }
    else
    {
        // �ش� Trigger�� ������.
    }

    IDE_TEST( qcm::getTableInfoByID( aStatement,
                                     sParseTree->tableID,
                                     & sParseTree->tableInfo,
                                     & sParseTree->tableSCN,
                                     & sParseTree->tableHandle)
              != IDE_SUCCESS);

    IDE_TEST(qcm::lockTableForDDLValidation(aStatement,
                                            sParseTree->tableHandle,
                                            sParseTree->tableSCN)
             != IDE_SUCCESS);

    //----------------------------------------
    // Trigger ���� ���� �˻�
    //----------------------------------------

    IDE_TEST( qdpRole::checkDDLDropTriggerPriv( aStatement,
                                                sParseTree->triggerUserID )
              != IDE_SUCCESS );

    //----------------------------------------
    // Replication �˻�
    //----------------------------------------
    // PROJ-1567
    // IDE_TEST_RAISE( sParseTree->tableInfo->replicationCount > 0,
    //                 ERR_DDL_WITH_REPLICATED_TABLE );

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_non_exist_trigger_name);
    {
        (void) sqlInfo.init( aStatement->qmeMem );
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_TRIGGER_NOT_EXIST,
                            sqlInfo.getErrMessage() ));
        (void) sqlInfo.fini();
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qdnTrigger::optimizeDrop( qcStatement * /* aStatement */ )
{
/***********************************************************************
 *
 * Description :
 *    DROP TRIGGER�� ���� Optimization.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qdnTrigger::optimizeDrop"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    // Nothing To Do

    return IDE_SUCCESS;

#undef IDE_FN
}

IDE_RC
qdnTrigger::executeDrop( qcStatement * aStatement )
{
/***********************************************************************
 *
 * Description :
 *    DROP TRIGGER�� ����.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qdnTrigger::executeDrop"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    void            * sTriggerHandle;
    qdnTriggerCache * sTriggerCache;
    qcmTableInfo    * sOldTableInfo = NULL;
    qcmTableInfo    * sNewTableInfo = NULL;
    smOID             sTableOID;
    void            * sTableHandle;
    smSCN             sSCN;

    qdnDropTriggerParseTree * sParseTree;

    qcmPartitionInfoList    * sAllPartInfoList = NULL;
    qcmPartitionInfoList    * sOldPartInfoList = NULL;
    qcmPartitionInfoList    * sNewPartInfoList = NULL;

    sParseTree = (qdnDropTriggerParseTree *) aStatement->myPlan->parseTree;

    //---------------------------------------
    // To Fix PR-11552
    // ���õ� PSM�� Invalid ���� ������ �־�� ��.
    // To Fix PR-12102
    // ���� TableInfo�� ���� XLatch�� ��ƾ� ��
    //
    // DROP TRIGGER(DDL) ---->Table->TRIGGER
    // DML ------------------>
    // ���� ���� �����ǹǷ� DROP TRIGGER���� �ٷ� TRIGGER�� �����Ͽ�
    // �����ϸ� �ȵ�
    //---------------------------------------

    //---------------------------------------
    // TASK-2176 Table�� ���� Lock�� ȹ���Ѵ�.
    //---------------------------------------

    IDE_TEST( qcm::validateAndLockTable(aStatement,
                                        sParseTree->tableHandle,
                                        sParseTree->tableSCN,
                                        SMI_TABLE_LOCK_X)
              != IDE_SUCCESS);

    sOldTableInfo = sParseTree->tableInfo;
    sTableOID     = smiGetTableId(sOldTableInfo->tableHandle);

    if ( sOldTableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
    {
        IDE_TEST( qcmPartition::getPartitionInfoList( aStatement,
                                                      QC_SMI_STMT( aStatement ),
                                                      QC_QMX_MEM( aStatement ),
                                                      sOldTableInfo->tableID,
                                                      & sAllPartInfoList )
                  != IDE_SUCCESS );

        IDE_TEST( qcmPartition::validateAndLockPartitionInfoList( aStatement,
                                                                  sAllPartInfoList,
                                                                  SMI_TBSLV_DDL_DML, // TBS Validation �ɼ�
                                                                  SMI_TABLE_LOCK_X,
                                                                  smiGetDDLLockTimeOut((QC_SMI_STMT(aStatement))->getTrans()) )
                  != IDE_SUCCESS );

        // ���� ó���� ���Ͽ�, Lock�� ���� �Ŀ� Partition List�� �����Ѵ�.
        sOldPartInfoList = sAllPartInfoList;
    }
    else
    {
        /* Nothing to do */
    }

    //---------------------------------------
    // Trigger Object ����
    //---------------------------------------

    sTriggerHandle = (void *)smiGetTable( sParseTree->triggerOID );

    IDE_TEST( dropTriggerObject( aStatement, sTriggerHandle ) != IDE_SUCCESS );

    //---------------------------------------
    // Trigger�� ���� Meta Table�� ���� ����
    //---------------------------------------

    IDE_TEST( qcmTrigger::removeMetaInfo( aStatement, sParseTree->triggerOID )
              != IDE_SUCCESS );

    //---------------------------------------
    // Table �� Meta Cache ������ �籸����.
    //---------------------------------------

    IDE_TEST( qcm::touchTable( QC_SMI_STMT( aStatement ),
                               sParseTree->tableID,
                               SMI_TBSLV_DDL_DML )
              != IDE_SUCCESS);

    IDE_TEST( qcm::makeAndSetQcmTableInfo( QC_SMI_STMT( aStatement ),
                                           sParseTree->tableID,
                                           sTableOID )
              != IDE_SUCCESS);

    IDE_TEST( qcm::getTableInfoByID(aStatement,
                                    sParseTree->tableID,
                                    &sNewTableInfo,
                                    &sSCN,
                                    &sTableHandle)
              != IDE_SUCCESS);

    if ( sOldTableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
    {
        IDE_TEST( qcmPartition::touchPartitionInfoList( QC_SMI_STMT( aStatement ),
                                                        sOldPartInfoList )
                  != IDE_SUCCESS );

        IDE_TEST( qcmPartition::makeAndSetAndGetQcmPartitionInfoList( aStatement,
                                                                      sNewTableInfo,
                                                                      sOldPartInfoList,
                                                                      & sNewPartInfoList )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    // To fix BUG-12102(BUG-12034�� ���� ������)
    // SM�� ���̻� ������ ������ ���� �� QP���� �����ϰ�
    // �ִ� �޸𸮵��� �����ؾ� �Ѵ�.
    IDE_TEST( smiObject::getObjectTempInfo( sTriggerHandle,
                                            (void**)&sTriggerCache )
              != IDE_SUCCESS );;

    IDE_TEST( freeTriggerCache( sTriggerCache ) != IDE_SUCCESS );

    if ( sOldTableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
    {
        (void)qcmPartition::destroyQcmPartitionInfoList( sOldPartInfoList );
    }
    else
    {
        /* Nothing to do */
    }

    (void)qcm::destroyQcmTableInfo(sOldTableInfo);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    (void)qcm::destroyQcmTableInfo( sNewTableInfo );
    (void)qcmPartition::destroyQcmPartitionInfoList( sNewPartInfoList );

    qcmPartition::restoreTempInfo( sOldTableInfo,
                                   sOldPartInfoList,
                                   NULL );

    return IDE_FAILURE;

#undef IDE_FN
}


IDE_RC
qdnTrigger::needTriggerRow( qcStatement         * aStatement,
                            qcmTableInfo        * aTableInfo,
                            qcmTriggerEventTime   aEventTime,
                            UInt                  aEventType,
                            smiColumnList       * aUptColumn,
                            idBool              * aIsNeed )
{
/***********************************************************************
 *
 * Description :
 *    ���õ� Trigger�� �� Trigger�� �����ϱ� ���Ͽ�
 *    OLD ROW, NEW ROW�� ���� ���� �����ϴ� ���� �˻��ϰ�,
 *    �̸� �����ؾ� �ϴ� ���� �Ǵ���.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qdnTrigger::needTriggerRow"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    UInt i;

    idBool sIsNeedTriggerRow = ID_FALSE;
    idBool sIsNeedAction     = ID_FALSE;
    idBool sIsRebuild        = ID_FALSE;

    qcmTriggerInfo * sTriggerInfo;

    //-------------------------------------------
    // ������ Trigger Event�� Row Granularity�� �����ϴ� ���� �˻�
    //-------------------------------------------

    sTriggerInfo = aTableInfo->triggerInfo;

    for ( i = 0; i < aTableInfo->triggerCount; i++ )
    {
        // PROJ-2219 Row-level before update trigger
        // sIsRebuild�� ���� checkCondition()���� ��������
        // �� �Լ������� ������� �ʴ´�.
        IDE_TEST( checkCondition( aStatement,
                                  & sTriggerInfo[i],
                                  QCM_TRIGGER_ACTION_EACH_ROW,
                                  aEventTime,
                                  aEventType,
                                  aUptColumn,
                                  & sIsNeedAction,
                                  & sIsRebuild )
                  != IDE_SUCCESS ) ;

        if ( ( sIsNeedAction == ID_TRUE ) &&
             ( sTriggerInfo[i].refRowCnt > 0 ) )

        {
            sIsNeedTriggerRow = ID_TRUE;
            break;
        }
        else
        {
            // continue
        }
    }

    *aIsNeed = sIsNeedTriggerRow;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
#undef IDE_FN
}

IDE_RC
qdnTrigger::fireTrigger( qcStatement         * aStatement,
                         iduMemory           * aNewValueMem,
                         qcmTableInfo        * aTableInfo,
                         qcmTriggerGranularity aGranularity,
                         qcmTriggerEventTime   aEventTime,
                         UInt                  aEventType,
                         smiColumnList       * aUptColumn,
                         smiTableCursor      * aTableCursor, /* for LOB */
                         scGRID                aGRID,
                         void                * aOldRow,
                         qcmColumn           * aOldRowColumns,
                         void                * aNewRow,
                         qcmColumn           * aNewRowColumns )
{
/***********************************************************************
 *
 * Description :
 *    �ش� �Է� ���ǿ� �����ϴ� Trigger�� ȹ���ϰ�,
 *    Trigger ������ ������ ��� �ش� Trigger Action�� �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qdnTrigger::fireTrigger"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    UInt i;
    idBool sNeedAction = ID_FALSE;
    idBool sIsRebuild  = ID_FALSE;
    qmmUptParseTree * sUptParseTree;
    qmmMultiTables  * sTmp;

    // BUG-46074 Multiple trigger event
    UInt            sOrgTriggerEventType  = aStatement->spxEnv->mTriggerEventType;
    qcmColumn     * sOrgTriggerUptColList = aStatement->spxEnv->mTriggerUptColList;

    if ( (aEventType == QCM_TRIGGER_EVENT_UPDATE) &&
         (aStatement->myPlan->parseTree->stmtKind == QCI_STMT_UPDATE) )
    {
        sUptParseTree = ((qmmUptParseTree*)aStatement->myPlan->parseTree);

        if ( sUptParseTree->mTableList == NULL )
        {
            aStatement->spxEnv->mTriggerUptColList = sUptParseTree->updateColumns;
        }
        else
        {
            aStatement->spxEnv->mTriggerUptColList = NULL;
            for ( sTmp = sUptParseTree->mTableList; sTmp != NULL; sTmp = sTmp->mNext )
            {
                if ( sTmp->mTableRef->tableInfo == aTableInfo )
                {
                    aStatement->spxEnv->mTriggerUptColList = sTmp->mColumns;
                    break;
                }
                else
                {
                    /* Nothing to do */
                }
            }
        }
    }
    else
    {
        aStatement->spxEnv->mTriggerUptColList = NULL;
    }

    aStatement->spxEnv->mTriggerEventType = aEventType;

    //-------------------------------------------
    // Trigger ���� �˻�
    //-------------------------------------------

    for ( i = 0;  i < aTableInfo->triggerCount; i++ )
    {
        sNeedAction = ID_FALSE;

        // PROJ-2219 Row-level before update trigger
        // sIsRebuild�� ���� checkCondition()���� ��������
        // �� �Լ������� ������� �ʴ´�.
        IDE_TEST( checkCondition( aStatement,
                                  & aTableInfo->triggerInfo[i],
                                  aGranularity,
                                  aEventTime,
                                  aEventType,
                                  aUptColumn,
                                  & sNeedAction,
                                  & sIsRebuild )
                  != IDE_SUCCESS );

        if ( sNeedAction == ID_TRUE )
        {
            // Trigger ������ �����ϴ� ���� Trigger�� �����Ѵ�.
            IDE_TEST( fireTriggerAction( aStatement,
                                         aNewValueMem,
                                         aTableInfo,
                                         & aTableInfo->triggerInfo[i],
                                         aTableCursor,
                                         aGRID,
                                         aOldRow,
                                         aOldRowColumns,
                                         aNewRow,
                                         aNewRowColumns )
                      != IDE_SUCCESS );
        }
        else
        {
            // Trigger ���� ������ ���յ��� �ʴ� �����.
            // Nothing To Do
        }
    }

    // BUG-46074 Multiple trigger event
    aStatement->spxEnv->mTriggerEventType  = sOrgTriggerEventType;
    aStatement->spxEnv->mTriggerUptColList = sOrgTriggerUptColList;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    // BUG-46074 Multiple trigger event
    aStatement->spxEnv->mTriggerEventType  = sOrgTriggerEventType;
    aStatement->spxEnv->mTriggerUptColList = sOrgTriggerUptColList;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qdnTrigger::loadAllTrigger( smiStatement * aSmiStmt )
{
/***********************************************************************
 *
 * Description :
 *    Server ���� �� �����ϸ�, ��� Trigger Object�� ���Ͽ�
 *    Trigger Object Cache�� �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qdnTrigger::loadAllTrigger"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    mtcColumn           *sTriggerOIDCol;
    mtcColumn           *sUserIDCol;

    smiTableCursor        sCursor;
    smiCursorProperties   sCursorProperty;
    idBool                sIsCursorOpen = ID_FALSE;

    smOID                 sTriggerOID;
    void                * sTriggerHandle;
    qdnTriggerCache     * sTriggerCache = NULL;

    scGRID                sRID;
    void                * sRow;

    //-------------------------------------------
    // ���ռ� �˻�
    //-------------------------------------------

    IDE_DASSERT( aSmiStmt != NULL );

    //-------------------------------------------
    // ��� Trigger Record�� �д´�.
    //-------------------------------------------

    // Cursor�� �ʱ�ȭ
    sCursor.initialize();

    // Cursor Property �ʱ�ȭ
    SMI_CURSOR_PROP_INIT_FOR_META_FULL_SCAN( &sCursorProperty, NULL );

    // INDEX (USER_ID, TRIGGER_NAME)�� �̿��� Cursor Open
    IDE_TEST(
        sCursor.open( aSmiStmt,  // smiStatement
                      gQcmTriggers,                  // Table Handle
                      NULL,                          // Index Handle
                      smiGetRowSCN( gQcmTriggers ),  // Table SCN
                      NULL,                          // Update Column����
                      smiGetDefaultKeyRange(),       // Key Range
                      smiGetDefaultKeyRange(),       // Key Filter
                      smiGetDefaultFilter(),         // Filter
                      QCM_META_CURSOR_FLAG,
                      SMI_SELECT_CURSOR,
                      & sCursorProperty )            // Cursor Property
        != IDE_SUCCESS );

    sIsCursorOpen = ID_TRUE;

    //-------------------------------------------
    // Trigger ID�� ȹ��
    //-------------------------------------------

    // Data�� Read��.
    IDE_TEST( sCursor.beforeFirst() != IDE_SUCCESS );
    IDE_TEST( sCursor.readRow( (const void **) & sRow, & sRID, SMI_FIND_NEXT )
              != IDE_SUCCESS );

    IDE_TEST( smiGetTableColumns( gQcmTriggers,
                                  QCM_TRIGGERS_TRIGGER_OID,
                                  (const smiColumn**)&sTriggerOIDCol )
              != IDE_SUCCESS );
    IDE_TEST( smiGetTableColumns( gQcmTriggers,
                                  QCM_TRIGGERS_USER_ID,
                                  (const smiColumn**)&sUserIDCol )
              != IDE_SUCCESS );


    while ( sRow != NULL )
    {
        //--------------------------------------------
        // �ϳ��� Trigger Object ������ ����.
        //--------------------------------------------

        // Trigger OID�� ����
        // To Fix PR-10648
        // smOID �� ȹ�� ����� �߸���.
        // *aTriggerOID = *(smOID *)
        //    ((UChar*)sRow+sColumn[QCM_TRIGGERS_TRIGGER_OID].column.offset );


        sTriggerOID = (smOID) *(ULong *)
            ( (UChar*)sRow + sTriggerOIDCol->column.offset );

        //--------------------------------------------
        // Trigger Object Cache�� ���� ���� ����
        //--------------------------------------------

        // Trigger Handle�� ȹ��
        sTriggerHandle = (void *)smiGetTable( sTriggerOID );

        // Trigger Handle�� Trigger Cache�� ���� ���� �Ҵ�
        IDE_TEST( allocTriggerCache( sTriggerHandle,
                                     sTriggerOID,
                                     & sTriggerCache )
                  != IDE_SUCCESS );

        IDE_TEST( sCursor.readRow( (const void **) & sRow,
                                   & sRID,
                                   SMI_FIND_NEXT )
                  != IDE_SUCCESS );

    }

    sIsCursorOpen = ID_FALSE;
    IDE_TEST( sCursor.close() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sIsCursorOpen == ID_TRUE )
    {
        (void) sCursor.close();
    }

    return IDE_FAILURE;

#undef IDE_FN
}


IDE_RC
qdnTrigger::freeTriggerCaches4DropTable( qcmTableInfo * aTableInfo )
{
    UInt                  i;
    qcmTriggerInfo      * sTriggerInfo;
    qdnTriggerCache     * sTriggerCache;

    IDE_DASSERT( aTableInfo != NULL );

    for ( i = 0, sTriggerInfo = aTableInfo->triggerInfo;
          i < aTableInfo->triggerCount;
          i++ )
    {
        IDE_TEST( smiObject::getObjectTempInfo( sTriggerInfo[i].triggerHandle,
                                                (void**)& sTriggerCache )
                  != IDE_SUCCESS );

        // Trigger Object Cache ����
        IDE_TEST( freeTriggerCache( sTriggerCache )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}


/* To Fix BUG-12034
   ���� ���⼭ freeTriggerCache���� �ϰԵǸ�,
   �� �Լ��� ȣ���ϴ� qdd::executeDropTable ��
   ������ ���, �޸𸮸� �����ǰ�, ��Ÿ������ �״�� ����ְԵǴ� ������ �߻�.
   *
   */
IDE_RC
qdnTrigger::dropTrigger4DropTable( qcStatement  * aStatement,
                                   qcmTableInfo * aTableInfo )
{
/***********************************************************************
 *
 * Description :
 *    �ش� Table ID�� ������ Trigger�� �����Ѵ�.
 *       - ���� Trigger Object�� ������ ��,
 *       - ���� Meta Table���� ������ �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qdnTrigger::dropTrigger4DropTable"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    UInt                  i;
    qcmTriggerInfo *      sTriggerInfo;

    SChar               * sBuffer;
    vSLong                 sRowCnt;

    //-------------------------------------------
    // ���ռ� �˻�
    //-------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aTableInfo != NULL );

    //-------------------------------------------
    // Trigger Object �� Object Cache�� ����
    //-------------------------------------------

    for ( i = 0, sTriggerInfo = aTableInfo->triggerInfo;
          i < aTableInfo->triggerCount;
          i++ )
    {
        // Trigger Object ����
        IDE_TEST( dropTriggerObject( aStatement,
                                     sTriggerInfo[i].triggerHandle )
                  != IDE_SUCCESS );
    }

    if ( aTableInfo->triggerCount > 0 )
    {
        //---------------------------------------
        // String�� ���� ���� ȹ��
        //---------------------------------------

        IDU_LIMITPOINT("qdnTrigger::dropTrigger4DropTable::malloc");
        IDE_TEST( aStatement->qmxMem->alloc( QD_MAX_SQL_LENGTH,
                                             (void**) & sBuffer )
                  != IDE_SUCCESS );

        //---------------------------------------
        // SYS_TRIGGERS_ ���� ���� ����
        // DELETE FROM SYS_TRIGGERS_ WHERE TABLE_ID = aTableID;
        //---------------------------------------

        idlOS::snprintf( sBuffer, QD_MAX_SQL_LENGTH,
                         "DELETE FROM SYS_TRIGGERS_ "
                         "WHERE TABLE_ID = "QCM_SQL_INT32_FMT,  // 0. TABLE_ID
                         aTableInfo->tableID );                 // 0.

        IDE_TEST( qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                     sBuffer,
                                     & sRowCnt )
                  != IDE_SUCCESS );

        //---------------------------------------
        // SYS_TRIGGER_STRINGS_ ���� ���� ����
        // DELETE FROM SYS_TRIGGER_STRINGS_ WHERE TABLE_ID = aTableID;
        //---------------------------------------

        idlOS::snprintf( sBuffer, QD_MAX_SQL_LENGTH,
                         "DELETE FROM SYS_TRIGGER_STRINGS_ "
                         "WHERE TABLE_ID = "QCM_SQL_INT32_FMT,  // 0. TABLE_ID
                         aTableInfo->tableID );                 // 0.

        IDE_TEST( qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                     sBuffer,
                                     & sRowCnt )
                  != IDE_SUCCESS );

        IDE_DASSERT( sRowCnt > 0 );

        //---------------------------------------
        // SYS_TRIGGER_UPDATE_COLUMNS_ ���� ���� ����
        //---------------------------------------

        idlOS::snprintf( sBuffer, QD_MAX_SQL_LENGTH,
                         "DELETE FROM SYS_TRIGGER_UPDATE_COLUMNS_ "
                         "WHERE TABLE_ID = "QCM_SQL_INT32_FMT,  // 0. TABLE_ID
                         aTableInfo->tableID );                // 0.

        IDE_TEST( qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                     sBuffer,
                                     & sRowCnt )
                  != IDE_SUCCESS );

        //---------------------------------------
        // SYS_TRIGGER_DML_TABLES �� ���� ����
        //---------------------------------------

        idlOS::snprintf( sBuffer, QD_MAX_SQL_LENGTH,
                         "DELETE FROM SYS_TRIGGER_DML_TABLES_ "
                         "WHERE TABLE_ID = "QCM_SQL_INT32_FMT, // 0. TABLE_ID
                         aTableInfo->tableID );                // 0.

        IDE_TEST( qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                     sBuffer,
                                     & sRowCnt )
                  != IDE_SUCCESS );
    }
    else
    {
        // Table�� �����ϴ� Trigger�� ����
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qdnTrigger::addGranularityInfo( qdnCreateTriggerParseTree * aParseTree )
{
/***********************************************************************
 *
 * Description :
 *    Trigger�� Action Granularity�� FOR EACH ROW�� ���
 *    REFERENCING ���� �� WHEN ������ ������ �� �ִ�.
 *
 *    Ex) CREATE TRIGGER ...
 *        AFTER UPDATE ON t1
 *        REFERENCING OLD ROW old_row, NEW_ROW new_row
 *        FOR EACH ROW WHEN ( old_row.i1 > new_row.i1 )
 *        ...;
 *
 *        ���� ���� �������� WHEN ���� ó���ϱ� ���ؼ���
 *        new_row ����, old_row �������� ���� ������ �� �־�� �Ѵ�.
 *
 * Implementation :
 *        // PROJ-1502 PARTITIONED DISK TABLE
 *        �� �Լ��� ȣ���� ���� �ݵ�� row granularity�� �־�� �ϴ� �����̴�.
 ***********************************************************************/

#define IDE_FN "qdnTrigger::addGranularityInfo"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    //----------------------------------------
    // ���ռ� �˻�
    //----------------------------------------

    IDE_DASSERT( aParseTree != NULL );

    IDE_DASSERT( aParseTree->actionCond.actionGranularity
                 == QCM_TRIGGER_ACTION_EACH_ROW );

    if ( ( aParseTree->triggerReference != NULL ) &&
         ( aParseTree->actionCond.whenCondition != NULL ) )
    {
        // PROJ-1502 PARTITIONED DISK TABLE
        // Nothing to do.
    }
    else
    {
        // reference�� ���µ� whenCondition�� �ִ� ���.
        // ����.
        IDE_TEST_RAISE( aParseTree->actionCond.whenCondition != NULL,
                        err_when_condition );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_when_condition);
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_TRIGGER_WHEN_REFERENCING ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}


IDE_RC
qdnTrigger::addActionBodyInfo( qcStatement               * aStatement,
                               qdnCreateTriggerParseTree * aParseTree )
{
/***********************************************************************
 *
 * Description :
 *     FOR EACH ROW granularity���� Action Body�� ���Ͽ�
 *     REFERENCING row �� Current Row�� ������ �߰��� �־�� �Ѵ�.
 *     �̴� PSM Body�� Action Body���� REFERENCING �� Subject Table�� ����
 *     ���� ���θ� �Ǵ��� �� ���� �����̴�.
 *
 *     �̸� ���� ������ ���� �������� Action Body�� Variable ������ �����Ѵ�.
 *     Ex)  CREATE TRIGGER ...
 *          ON t1 REFERENCING OLD old_row, NEW new_row
 *          FOR EACH ROW
 *          AS BEGIN
 *             INSERT INTO log_table VALUES ( old_row.i1, new_row.i2 );
 *          END;
 *
 *          ==>
 *
 *          ...
 *          AS
 *             old_row t1%ROWTYPE;
 *             new_row t1%ROWTYPE;
 *          BEGIN
 *             INSERT INTO log_table VALUES ( old_row.i2, new_row.i3 );
 *          END;
 *
 *          ��, ���� ���� ���������� ROWTYPE�� Variable�� �߰��Ͽ�
 *          BEGIN ... END ������ Body������ Validation �� Execution��
 *          �����ϰ� �ȴ�.
 *
 * Implementation :
 *
 *     REFERENCING ������ �����ϴ� row�� ���� ROWTYPE Variable ����
 *
 ***********************************************************************/

#define IDE_FN "qdnTrigger::addActionBodyInfo"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qdnTriggerRef * sRef;
    qsVariables   * sCurVar;
    qtcNode       * sTypeNode[2];

    //----------------------------------------
    // ���ռ� �˻�
    //----------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aParseTree != NULL );

    IDE_DASSERT( aParseTree->actionCond.actionGranularity
                 == QCM_TRIGGER_ACTION_EACH_ROW );

    //----------------------------------------
    // REFERENCING ������ �����ϴ� row�� ���� ROWTYPE Variable ����
    //----------------------------------------
    aParseTree->actionBody.paraDecls = NULL;

    for ( sRef = aParseTree->triggerReference;
          sRef != NULL;
          sRef = sRef->next )
    {
        // REFERENCING Row�� ���� ROWTYPE Variable�� ����
        // Ex) ... ON user_name.table_name REFERENCING OLD ROW AS old_row
        //    => old_row  user_name.table_name%ROWTYPE;
        // ���� ����

        //---------------------------------------------------
        // Type �κ��� ���� ����
        //---------------------------------------------------

        // old_row user_name.[table_name%ROWTYPE] �� ���� Node ����
        IDE_TEST( qtc::makeProcVariable( aStatement,
                                         sTypeNode,
                                         & aParseTree->tableNamePos,
                                         NULL,
                                         QTC_PROC_VAR_OP_NONE )
                  != IDE_SUCCESS );

        // old_row [user_name].table_name%ROWTYPE ���� user_name ������ ����
        idlOS::memcpy( & sTypeNode[0]->tableName,
                       & aParseTree->userNamePos,
                       ID_SIZEOF(qcNamePosition) );

        //---------------------------------------------------
        // ROWTYPE Variable ������ �ϼ�
        //---------------------------------------------------

        IDU_LIMITPOINT("qdnTrigger::addActionBodyInfo::malloc");
        IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(qsVariables),
                                                 (void**) & sCurVar )
                  != IDE_SUCCESS );

        sCurVar->variableType     = QS_ROW_TYPE;
        sCurVar->variableTypeNode = sTypeNode[0];
        sCurVar->defaultValueNode = NULL;

        // To fix BUG-12622
        // BEFORE TRIGGER���� new row�� ������ �����ϴ�.
        if( sRef->refType == QCM_TRIGGER_REF_NEW_ROW )
        {
            sCurVar->common.itemType = QS_TRIGGER_NEW_VARIABLE;

            if( aParseTree->triggerEvent.eventTime == QCM_TRIGGER_BEFORE )
            {
                sCurVar->inOutType = QS_INOUT;
            }
            else
            {
                sCurVar->inOutType = QS_IN;
            }
        }
        else
        {
            sCurVar->inOutType = QS_IN;
            sCurVar->common.itemType = QS_TRIGGER_OLD_VARIABLE;
        }

        sCurVar->nocopyType      = QS_COPY;
        sCurVar->typeInfo        = NULL;

        sCurVar->common.table    = sCurVar->variableTypeNode->node.table;
        sCurVar->common.column   = sCurVar->variableTypeNode->node.column;

        // [old_row] user_name.table_name%ROWTYPE
        idlOS::memcpy( & sCurVar->common.name,
                       & sRef->aliasNamePos,
                       ID_SIZEOF( qcNamePosition ) );

        // REFERENCING Row �� Action Body���� Variable�� ����
        //sRef->rowVarTableID = sCurVar->common.table;
        //sRef->rowVarColumnID = sCurVar->common.column;

        // PROJ-1075 table, column���� ã��������.
        sRef->rowVar = sCurVar;

        //---------------------------------------------------
        // Variable�� ����
        // PROJ-1502 PARTITIONED DISK TABLE
        // parameter�� �����Ѵ�.
        //---------------------------------------------------

        sCurVar->common.next = aParseTree->actionBody.paraDecls;
        aParseTree->actionBody.paraDecls =
            (qsVariableItems*) sCurVar;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qdnTrigger::setTriggerUser( qcStatement               * aStatement,
                            qdnCreateTriggerParseTree * aParseTree )
{
/***********************************************************************
 *
 * Description : BUG-24570
 *    CREATE TRIGGER�� User�� ���� ���� ����
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qdnTrigger::setTriggerUser"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    SChar   sUserName[QC_MAX_OBJECT_NAME_LEN + 1];

    //---------------------------------------
    // ���ռ� �˻�
    //---------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aParseTree != NULL );

    //---------------------------------------
    // Trigger User ������ ȹ��
    //---------------------------------------

    if ( QC_IS_NULL_NAME( aParseTree->triggerUserPos ) == ID_TRUE)
    {
        // User ID ȹ��
        if ( aParseTree->isRecompile == ID_FALSE )
        {
            aParseTree->triggerUserID = QCG_GET_SESSION_USER_ID(aStatement);
        }
        else
        {
            // �̹� ��������

            // Nothing to do.
        }

        // User Name ����
        IDE_TEST( qcmUser::getUserName( aStatement,
                                        aParseTree->triggerUserID,
                                        sUserName )
                  != IDE_SUCCESS );
        // BUG-25419 [CodeSonar] Format String ��� Warning
        idlOS::snprintf( aParseTree->triggerUserName, QC_MAX_OBJECT_NAME_LEN + 1, "%s", sUserName );
        aParseTree->triggerUserName[QC_MAX_OBJECT_NAME_LEN] = '\0';

    }
    else
    {
        // User ID ȹ��
        IDE_TEST( qcmUser::getUserID( aStatement,
                                      aParseTree->triggerUserPos,
                                      & aParseTree->triggerUserID )
                  != IDE_SUCCESS);

        // User Name ����
        QC_STR_COPY( aParseTree->triggerUserName, aParseTree->triggerUserPos );
    }

    // BUG-42322
    IDE_TEST( qsv::setObjectNameInfo( aStatement,
                                      aParseTree->actionBody.objType,
                                      aParseTree->triggerUserID,
                                      aParseTree->triggerNamePos,
                                      &aParseTree->actionBody.objectNameInfo )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qdnTrigger::valPrivilege( qcStatement               * aStatement,
                          qdnCreateTriggerParseTree * aParseTree )
{
/***********************************************************************
 *
 * Description :
 *    CREATE TRIGGER�� ���� �˻�
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qdnTrigger::valPrivilege"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    //---------------------------------------
    // ���ռ� �˻�
    //---------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aParseTree != NULL );

    //---------------------------------------
    // Trigger ���� ���� �˻�
    //---------------------------------------

    // BUG-24570
    // create, replace, alter�ÿ��� session user�� trigger ���� ���� �˻�
    if ( aParseTree->isRecompile == ID_FALSE )
    {
        // check grant
        IDE_TEST( qdpRole::checkDDLCreateTriggerPriv( aStatement,
                                                      aParseTree->triggerUserID )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qdnTrigger::valTriggerName( qcStatement               * aStatement,
                            qdnCreateTriggerParseTree * aParseTree )
{
/***********************************************************************
 *
 * Description :
 *    CREATE TRIGGER�� ���� Trigger Name�� ��ȿ�� �˻�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qdnTrigger::valTriggerName"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qcuSqlSourceInfo     sqlInfo;

    UInt                 sTableID;
    idBool               sExist;

    //---------------------------------------
    // ���ռ� �˻�
    //---------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aParseTree != NULL );

    // �̹� �����ϴ� Trigger���� �˻�
    IDE_TEST( qcmTrigger::getTriggerOID( aStatement,
                                         aParseTree->triggerUserID,
                                         aParseTree->triggerNamePos,
                                         & (aParseTree->triggerOID),
                                         & sTableID,
                                         & sExist )
              != IDE_SUCCESS );

    if ( sExist == ID_TRUE )
    {
        sqlInfo.setSourceInfo( aStatement,
                               & aParseTree->triggerUserPos,
                               & aParseTree->triggerNamePos );
        IDE_RAISE( err_duplicate_trigger_name );
    }
    else
    {
        // �ش� Trigger�� �������� ����
    }
    aParseTree->triggerOID = 0;

    // Trigger Name�� ����
    QC_STR_COPY( aParseTree->triggerName, aParseTree->triggerNamePos );

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_duplicate_trigger_name)
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET( ideSetErrorCode( qpERR_ABORT_TRIGGER_DUPLICATED_NAME,
                                  sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qdnTrigger::reValTriggerName( qcStatement               * aStatement,
                              qdnCreateTriggerParseTree * aParseTree )
{
/***********************************************************************
 *
 * Description :
 *    Recompile�� ���� Trigger Name�� ��ȿ�� �˻�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qdnTrigger::reValTriggerName"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    UInt                 sTableID;
    idBool               sExist;

    //---------------------------------------
    // ���ռ� �˻�
    //---------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aParseTree != NULL );

    // �̹� �����ϴ� Trigger���� �˻�
    IDE_TEST( qcmTrigger::getTriggerOID( aStatement,
                                         aParseTree->triggerUserID,
                                         aParseTree->triggerNamePos,
                                         & (aParseTree->triggerOID),
                                         & sTableID,
                                         & sExist )
              != IDE_SUCCESS );

    IDE_DASSERT( sExist == ID_TRUE );

    // Trigger Name�� ����
    QC_STR_COPY( aParseTree->triggerName, aParseTree->triggerNamePos );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}


IDE_RC
qdnTrigger::valTableName( qcStatement               * aStatement,
                          qdnCreateTriggerParseTree * aParseTree )
{
/***********************************************************************
 *
 * Description :
 *    CREATE TRIGGER�� ���� Table Name�� ��ȿ�� �˻�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qdnTrigger::valTableName"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qcuSqlSourceInfo     sqlInfo;
    
    //---------------------------------------
    // ���ռ� �˻�
    //---------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aParseTree != NULL );

    // BUG-24408
    // triggerUserName�� ����� ��� on���� tableUserName�� ����ؾ� �Ѵ�.
    if ( ( QC_IS_NULL_NAME( aParseTree->triggerUserPos ) != ID_TRUE ) &&
         ( QC_IS_NULL_NAME( aParseTree->userNamePos ) == ID_TRUE ) )
    {
        sqlInfo.setSourceInfo( aStatement,
                               & aParseTree->userNamePos,
                               & aParseTree->tableNamePos );
        IDE_RAISE( err_invalid_table_owner_name );
    }
    else
    {
        // Nothing to do.
    }

    // Table�� �����Ͽ��� �Ѵ�.
    // Table ���� ȹ�� �� Table�� ���� IS Lock ȹ��
    IDE_TEST( qdbCommon::checkTableInfo( aStatement,
                                         aParseTree->userNamePos,
                                         aParseTree->tableNamePos,
                                         & aParseTree->tableUserID,
                                         & aParseTree->tableInfo,
                                         & aParseTree->tableHandle,
                                         & aParseTree->tableSCN)
              != IDE_SUCCESS);
    
    IDE_TEST(qcm::lockTableForDDLValidation(aStatement,
                                            aParseTree->tableHandle,
                                            aParseTree->tableSCN)
             != IDE_SUCCESS);
    
    aParseTree->tableID = aParseTree->tableInfo->tableID;
    aParseTree->tableOID = aParseTree->tableInfo->tableOID;

    if (aParseTree->isRecompile != ID_TRUE)
    {
        // To Fix PR-10618
        // Table Owner�� ���� Validation
        IDE_TEST( qdpRole::checkDDLCreateTriggerTablePriv(
                      aStatement,
                      aParseTree->tableInfo->tableOwnerID )
                  != IDE_SUCCESS );
    }
    else
    {
        /*
         * BUG-34422: create trigger privilege should not be checked
         *            when recompling trigger
         */
    }

    // To Fix PR-10708
    // �Ϲ� Table�� ��쿡�� Trigger�� ������ �� ����.
    /* PROJ-1888 INSTEAD OF TRIGGER (view �� Ʈ���Ÿ� ������ �� ���� ) */
    if ( ( aParseTree->tableInfo->tableType != QCM_USER_TABLE )  &&
         ( aParseTree->tableInfo->tableType != QCM_VIEW )        &&
         ( aParseTree->tableInfo->tableType != QCM_MVIEW_TABLE ) )
    {
        sqlInfo.setSourceInfo( aStatement,
                               & aParseTree->userNamePos,
                               & aParseTree->tableNamePos );
        IDE_RAISE(err_invalid_object);
    }

    // PROJ-1567
    /* if( aParseTree->isRecompile == ID_FALSE )
    {
        // To fix BUG-14584
        // CREATE or REPLACE�� ���.
        // Replication�� �ɷ� �ִ� ���̺� ���ؼ��� DDL�� �߻���ų �� ����
        IDE_TEST_RAISE( aParseTree->tableInfo->replicationCount > 0,
                        ERR_DDL_WITH_REPLICATED_TABLE );
    }
    else
    {
        // recompile�� ���.
        // Nothing to do.
    } */

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_invalid_object);
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(  qpERR_ABORT_TRIGGER_INVALID_TABLE_NAME,
                              sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION(err_invalid_table_owner_name);
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode( qpERR_ABORT_TRIGGER_INVALID_TABLE_OWNER_NAME,
                             sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

//To FIX BUG-20948
//--------------------------------------------------------------
// Trigger Replace �� Target Table�� ����Ǿ��� ���, Original Table
// ���� ȹ�� �� Lock
//--------------------------------------------------------------
IDE_RC
qdnTrigger::valOrgTableName( qcStatement               * aStatement,
                             qdnCreateTriggerParseTree * aParseTree )
{
/***********************************************************************
 *
 * BUG-20948
 * Original Table�� ���Ͽ� ó���Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "qdnTrigger::valTableName"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    //---------------------------------------
    // ���ռ� �˻�
    //---------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aParseTree != NULL );

    // Table ���� ȹ�� �� Table�� ���� IS Lock ȹ��

    IDE_TEST( qcm::getTableHandleByID(  QC_SMI_STMT( aStatement ),
                                        aParseTree->orgTableID,
                                        &aParseTree->orgTableHandle,
                                        &aParseTree->orgTableSCN )
              != IDE_SUCCESS);

    IDE_TEST( smiGetTableTempInfo( aParseTree->orgTableHandle,
                                   (void**)&aParseTree->orgTableInfo )
              != IDE_SUCCESS );

    IDE_TEST(qcm::lockTableForDDLValidation(aStatement,
                                            aParseTree->orgTableHandle,
                                            aParseTree->orgTableSCN)
             != IDE_SUCCESS);

    aParseTree->orgTableOID = aParseTree->orgTableInfo->tableOID;

    // To Fix PR-10618
    // Table Owner�� ���� Validation
    IDE_TEST( qdpRole::checkDDLCreateTriggerTablePriv(
                  aStatement,
                  aParseTree->orgTableInfo->tableOwnerID )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qdnTrigger::valEventReference( qcStatement               * aStatement,
                               qdnCreateTriggerParseTree * aParseTree )
{
/***********************************************************************
 *
 * Description :
 *    CREATE TRIGGER�� ���� Trigger Event �� ��ȿ�� �˻�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qdnTrigger::valTriggerEvent"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qcmColumn          * sColumn;
    qcmColumn          * sColumnInfo;

    //---------------------------------------
    // ���ռ� �˻�
    //---------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aParseTree != NULL );

    //----------------------------------------------
    // Trigger Event �� ���� Validation
    //----------------------------------------------

    // Post-Parsing �ܰ迡�� �̹� ������ ����.
    // - BEFORE Trigger Event�� �������� ����.

    // UPDATE OF i1, i2 ON t1�� ���� Column�� �����ϴ� ��쿡 ���� Validation
    for ( sColumn = aParseTree->triggerEvent.eventTypeList->updateColumns;
          sColumn != NULL;
          sColumn = sColumn->next )
    {
        // �ش� Column�� �����ϴ� ���� �˻�
        IDE_TEST(qcmCache::getColumn( aStatement,
                                      aParseTree->tableInfo,
                                      sColumn->namePos,
                                      & sColumnInfo) != IDE_SUCCESS);

        sColumn->basicInfo = sColumnInfo->basicInfo;
    }

    //----------------------------------------------
    // Trigger Referencing �� ���� Validation
    //----------------------------------------

    // Post-Parsing �ܰ迡�� �̹� ������ ����.
    // - REFERENCING�� FOR EACH ROW Granularity������ ��� ������.
    // - TABLE REFERENCING�� �������� ����.

    // DML EVENT�� ���յ��� �ʴ� REFERENCING�� �˻��Ѵ�.
    if (aParseTree->triggerEvent.eventTypeList->eventType
        == QCM_TRIGGER_EVENT_NONE)
    {
        IDE_DASSERT(0);
        IDE_RAISE( err_invalid_event_referencing );
    }

    if( aParseTree->actionBody.paraDecls != NULL )
    {
        // PROJ-1502 PARTITIONED DISK TABLE
        // old row, new row�� ���� parameter�̹Ƿ�,
        // parameter�� validation�� ���� ������ �ؾ� �Ѵ�.
        IDE_TEST( qsvProcVar::validateParaDef(
                      aStatement,
                      aParseTree->actionBody.paraDecls )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_invalid_event_referencing)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_TRIGGER_INVALID_REFERENCING));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qdnTrigger::valActionCondition( qcStatement               * aStatement,
                                qdnCreateTriggerParseTree * aParseTree )
{
/***********************************************************************
 *
 * Description :
 *    CREATE TRIGGER�� ���� Action Condition�� ���� Validation.
 *
 *    Row Action Granularity �� ��� ������ �ΰ� ������ ���Ͽ� Validation�Ѵ�.
 *    WHEN Condition�� ��ȿ�� �˻�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qdnTrigger::valActionCondition"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));


    qcuSqlSourceInfo      sqlInfo;
    mtcColumn           * sColumn;
    qtcNode             * sWhenCondition;

    //---------------------------------------
    // ���ռ� �˻�
    //---------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aParseTree != NULL );

    // fix BUG-32837
    aStatement->spvEnv->allParaDecls = aParseTree->actionBody.paraDecls;

    //----------------------------------------
    // Row Granularity ������ Validation
    //----------------------------------------

    if ( ( aParseTree->triggerReference != NULL ) &&
         ( aParseTree->actionCond.whenCondition != NULL ) )
    {
        sWhenCondition = aParseTree->actionCond.whenCondition;

        // when condition�� üũ.
        // estimate�� �� ����, �ݵ�� booleanŸ���� ���ϵǾ�� �Ѵ�.
        // subquery�� �����ؼ��� �ȵȴ�.
        IDE_TEST( qtc::estimate( sWhenCondition,
                                 QC_SHARED_TMPLATE(aStatement),
                                 aStatement,
                                 NULL,
                                 NULL,
                                 NULL )
                  != IDE_SUCCESS );

        sColumn = &(QC_SHARED_TMPLATE(aStatement)->tmplate.
                                        rows[sWhenCondition->node.table].
                                        columns[sWhenCondition->node.column]);

        if ( sColumn->module != &mtdBoolean )
        {
            sqlInfo.setSourceInfo( aStatement,
                                   & sWhenCondition->position );
            IDE_RAISE(ERR_INSUFFICIENT_ARGUEMNT);
        }
        else
        {
            // Nothing to do.
        }

        IDE_TEST_RAISE( qsv::checkNoSubquery( aStatement,
                                              sWhenCondition,
                                              & sqlInfo )
                        != IDE_SUCCESS,
                        err_when_subquery );

        // BUG-38137 Trigger�� when condition���� PSM�� ȣ���� �� ����.
        if ( checkNoSpFunctionCall( sWhenCondition )
            != IDE_SUCCESS )
        {
            sqlInfo.setSourceInfo( aStatement,
                                   & sWhenCondition->position );
            IDE_RAISE( err_invalid_psm_in_trigger );
        }
        else
        {
            // Nothing to do.
        }
    }
    else
    {
        // Statement Granularity trigger��.
        // Nothing to do.
    }

    // fix BUG-32837
    aStatement->spvEnv->allParaDecls = NULL;

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_when_subquery);
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(ideSetErrorCode(qpERR_ABORT_TRIGGER_WHEN_SUBQUERY,
                                sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION(ERR_INSUFFICIENT_ARGUEMNT);
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode( qpERR_ABORT_QSV_INSUFFICIENT_ARGUEMNT_SQLTEXT,
                             sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION(err_invalid_psm_in_trigger);
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode( qpERR_ABORT_TRIGGER_PSM,
                             sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}


IDE_RC
qdnTrigger::valActionBody( qcStatement               * aStatement,
                           qdnCreateTriggerParseTree * aParseTree )
{
/***********************************************************************
 *
 * Description :
 *    CREATE TRIGGER�� ���� Action Body�� ���� Validation.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qdnTrigger::valActionBody"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    //---------------------------------------
    // ���ռ� �˻�
    //---------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aParseTree != NULL );

    //----------------------------------------
    // Action Body ������ ���� Validation��
    // TRIGGER �� ���� Mode�� �����Ѵ�.
    //----------------------------------------

    // row, column Ÿ���� ������ ����Ʈ�� �߰��ϱ� ���ؼ���
    // aStatement->myPlan->parseTree�� qsProcParseTree�� �Ǿ�� �Ѵ�.
    aStatement->myPlan->parseTree = (qcParseTree *)&aParseTree->actionBody;

    // stmtKind�� �����ϴ� ������ ���̺� ���� IS lock�� ��� �����̴�.
    aStatement->myPlan->parseTree->stmtKind = QCI_STMT_MASK_DML;

    //----------------------------------------
    // Action Body�� Post Parsing
    // DML�� ��� post parsing�� validation�� �����Ѵ�.
    //----------------------------------------

    IDE_TEST( aParseTree->actionBody.block->common.
              parse( aStatement,
                     (qsProcStmts *) (aParseTree->actionBody.block) )
              != IDE_SUCCESS);

    //----------------------------------------
    // Action Body�� Validation
    //----------------------------------------

    IDE_TEST( aParseTree->actionBody.block->common.
              validate( aStatement,
                        (qsProcStmts *) (aParseTree->actionBody.block),
                        NULL,
                        QS_PURPOSE_TRIGGER )
              != IDE_SUCCESS);

    // validation ���Ŀ��� ���� parseTree�� �����Ѵ�.
    aStatement->myPlan->parseTree = (qcParseTree *)aParseTree;

    //----------------------------------------
    // Trigger Cycle Detection
    //----------------------------------------

    IDE_TEST( checkCycle( aStatement, aParseTree ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    // validation ���Ŀ��� ���� parseTree�� �����Ѵ�.
    aStatement->myPlan->parseTree = (qcParseTree *)aParseTree;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qdnTrigger::valInsteadOfTrigger( qdnCreateTriggerParseTree * aParseTree )
{
/***********************************************************************
 *
 * Description :
 *    INSTEAD OF TRIGGER�� CREATE, REPLACE �� ���� ����� Validation.
 *
 * Implementation :
 *
 ***********************************************************************/

    //---------------------------------------
    // ���ռ� �˻�
    //---------------------------------------

    IDE_DASSERT( aParseTree != NULL );

    // INSTEAD OF �� �ݵ�� view
    // INSTEAD OF �� �Ϲ� ���̺� �ɼ� ����.
    IDE_TEST_RAISE(
          ( aParseTree->triggerEvent.eventTime ==
            QCM_TRIGGER_INSTEAD_OF ) &&
          ( aParseTree->tableInfo->tableType !=
            QCM_VIEW ),
        ERR_INSTEAD_OF_NOT_VIEW );

    //VIEW ���̺� INSTEAD OF�ܿ� �ٸ� Ʈ���Ÿ� �����Ҽ� ����.
    IDE_TEST_RAISE(
          ( aParseTree->triggerEvent.eventTime !=
            QCM_TRIGGER_INSTEAD_OF ) &&
          ( aParseTree->tableInfo->tableType ==
            QCM_VIEW ),
        ERR_BEFORE_AFTER_VIEW );

    // not support INSTEAD OF ~ FOR EACH STATEMENT
    IDE_TEST_RAISE(
          ( aParseTree->triggerEvent.eventTime ==
            QCM_TRIGGER_INSTEAD_OF ) &&
          ( aParseTree->actionCond.actionGranularity ==
            QCM_TRIGGER_ACTION_EACH_STMT ),
        ERR_UNSUPPORTED_INSTEAD_OF_EACH_STMT );

    // not support INSTEAD OF ~ WHEN condition
    IDE_TEST_RAISE(
          ( aParseTree->triggerEvent.eventTime ==
            QCM_TRIGGER_INSTEAD_OF ) &&
          ( aParseTree->actionCond.whenCondition !=
            NULL ),
        ERR_UNSUPPORTED_INSTEAD_OF_WHEN_CONDITION );

    // not support INSTEAD OF UPDATE OF
    IDE_TEST_RAISE(
          ( aParseTree->triggerEvent.eventTime ==
            QCM_TRIGGER_INSTEAD_OF ) &&
          ((aParseTree->triggerEvent.eventTypeList->eventType &
            QCM_TRIGGER_EVENT_UPDATE) != 0) &&
          ( aParseTree->triggerEvent.eventTypeList->updateColumns !=
            NULL ),
        ERR_UNSUPPORTED_INSTEAD_OF_UPDATE_OF );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INSTEAD_OF_NOT_VIEW );
    {
        IDE_SET( ideSetErrorCode(
                     qpERR_ABORT_TRIGGER_INSTEAD_OF_NOT_VIEW ) );
    }
    IDE_EXCEPTION( ERR_BEFORE_AFTER_VIEW );
    {
        IDE_SET( ideSetErrorCode(
                     qpERR_ABORT_TRIGGER_BEFORE_AFTER_VIEW ) );
    }
    IDE_EXCEPTION( ERR_UNSUPPORTED_INSTEAD_OF_EACH_STMT );
    {
        IDE_SET( ideSetErrorCode(
                     qpERR_ABORT_TRIGGER_UNSUPPORTED_INSTEAD_OF_EACH_STMT ) );
    }
    IDE_EXCEPTION( ERR_UNSUPPORTED_INSTEAD_OF_WHEN_CONDITION );
    {
        IDE_SET( ideSetErrorCode(
                     qpERR_ABORT_TRIGGER_UNSUPPORTED_INSTEAD_OF_WHEN_COND ) );
    }
    IDE_EXCEPTION(ERR_UNSUPPORTED_INSTEAD_OF_UPDATE_OF);
    {
        IDE_SET( ideSetErrorCode(
                     qpERR_ABORT_TRIGGER_UNSUPPORTED_INSTEAD_OF_COLUMN_LIST ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qdnTrigger::checkCycle( qcStatement               * aStatement,
                        qdnCreateTriggerParseTree * aParseTree )
{
/***********************************************************************
 *
 * Description :
 *    Trigger ���� �� Cycle�� ���� �˻縦 �����Ѵ�.
 *
 *    DML --> [T1] --> [Tr1]--DML--> [T2] --> [TrA] --> [T1]
 *             ^                     [T3]               ^^^^
 *             |                                          |
 *             |                                          |
 *             +------------------------------------------+
 *
 *    ���� �׸��� ���� Cycle�� ���� �ܰ迡 ���� ������ �� �ִ�.
 *    �ܼ��� Cache ������ �̿��Ͽ� Cycle�� Detect�� ���,
 *    IPR ������ ���� ������ ����� �� �����Ƿ� ���ü� ���
 *    ��Ȯ�ϰ� ����Ͽ��� �Ѵ�.
 *
 *    Meta Cache���� ������ ������ ���ü� ��� �ʿ�� �ϰ� �ǹǷ�,
 *    Action Body�� �����ϴ� Table ������ ����ϰ� �ִ�
 *    SYS_TRIGGER_DML_TABLES_ �� �̿��Ͽ� �˻��Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qdnTrigger::checkCycle"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qsModifiedTable     * sCheckTable;
    qsModifiedTable     * sSameTable;

    //---------------------------------------
    // ���ռ� �˻�
    //---------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aParseTree != NULL );

    //---------------------------------------
    // Action Body�� DML�� �����ϴ� Table��
    // �ڽ��� Table�� �����ϴ� Table�� �����ϴ� ���� �˻�
    //---------------------------------------

    for ( sCheckTable = aStatement->spvEnv->modifiedTableList;
          sCheckTable != NULL;
          sCheckTable = sCheckTable->next )
    {
        IDE_TEST_RAISE( aParseTree->tableID == sCheckTable->tableID,
                        err_cycle_detected );
    }

    //---------------------------------------
    // Action Body�� DML�� �����Ǵ� Table�κ��� �߻��Ǵ�
    // Cycle�� �����ϴ� �� �˻�
    // �̹� �˻��� Table�� �ٽ� �˻����� �ʴ´�.
    //---------------------------------------

    for ( sCheckTable = aStatement->spvEnv->modifiedTableList;
          sCheckTable != NULL;
          sCheckTable = sCheckTable->next )
    {
        for ( sSameTable = aStatement->spvEnv->modifiedTableList;
              (sSameTable != NULL) && (sSameTable != sCheckTable);
              sSameTable = sSameTable->next )
        {
            if ( sSameTable->tableID == sCheckTable->tableID )
            {
                // ������ Table�� �̹� �˻�Ǿ���.
                break;
            }
            else
            {
                // Nothing To Do
            }
        }

        if ( sSameTable == sCheckTable )
        {
            // ���� �˻簡 ���� ���� Table�� ���
            // ���� Table�κ��� Cycle�� �����ϴ� ���� �˻��Ѵ�.
            IDE_TEST( qcmTrigger::checkTriggerCycle ( aStatement,
                                                      sCheckTable->tableID,
                                                      aParseTree->tableID )
                      != IDE_SUCCESS );
        }
        else
        {
            // �̹� �˻簡 �Ϸ�� Table�� ���
            // �ߺ� �˻��� �ʿ䰡 ����.
            // Nothing To Do
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_cycle_detected);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_TRIGGER_CYCLE_DETECTED));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qdnTrigger::createTriggerObject( qcStatement     * aStatement,
                                 void           ** aTriggerHandle )
{
/***********************************************************************
 *
 * Description :
 *    Trigger ���� ������ �����ϴ� Object�� �����ϰ�
 *    �ش� Handle�� ��´�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qdnTrigger::createTriggerObject"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    //---------------------------------------
    // ���ռ� �˻�
    //---------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aTriggerHandle != NULL );

    //---------------------------------------
    // Trigger Object�� ����
    //---------------------------------------

    IDE_TEST( smiObject::createObject( QC_SMI_STMT( aStatement ),
                                       aStatement->myPlan->stmtText,
                                       aStatement->myPlan->stmtTextLen,
                                       NULL,
                                       SMI_OBJECT_TRIGGER,
                                       (const void **) aTriggerHandle )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qdnTrigger::allocTriggerCache( void             * aTriggerHandle,
                                      smOID              aTriggerOID,
                                      qdnTriggerCache ** aCache )
{
/***********************************************************************
 *
 * Description :
 *
 *    Trigger Object�� ���� Cache ������ �Ҵ�޴´�.
 *
 *     [Trigger Handle] -- info ----> CREATE TRIGGER String
 *                      |
 *                      -- tempInfo ----> Trigger Object Cache
 *                                        ^^^^^^^^^^^^^^^^^^^^
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qdnTrigger::allocTriggerCache"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qdnTriggerCache * sTriggerCache;
    SChar             sLatchName[IDU_MUTEX_NAME_LEN];
    UInt              sStage = 0;

    //---------------------------------------
    // Permanant Memory ������ �Ҵ�޴´�.
    //---------------------------------------

    IDU_LIMITPOINT("qdnTrigger::allocTriggerCache::malloc1");
    IDE_TEST(iduMemMgr::malloc( IDU_MEM_QSX,
                                ID_SIZEOF(qdnTriggerCache),
                                (void**) & sTriggerCache )
             != IDE_SUCCESS);

    sStage = 1;

    //---------------------------------------
    // Statement Text�� �����ϱ� ���� ���� �Ҵ�
    // remote ���� ������ stmt text, len�� ��� �´�.
    //---------------------------------------

    smiObject::getObjectInfoSize( aTriggerHandle,
                                  & sTriggerCache->stmtTextLen );

    IDU_LIMITPOINT("qdnTrigger::allocTriggerCache::malloc2");
    IDE_TEST( iduMemMgr::malloc( IDU_MEM_QSX,
                                 sTriggerCache->stmtTextLen+1,
                                 (void**) & sTriggerCache->stmtText )
              != IDE_SUCCESS );

    smiObject::getObjectInfo( aTriggerHandle,
                              (void**) & sTriggerCache->stmtText );

    IDE_DASSERT(sTriggerCache->stmtText != NULL);

    // BUG-40483 valgrind warning
    // string null termination
    sTriggerCache->stmtText[sTriggerCache->stmtTextLen] = 0;

    sStage = 2;

    //---------------------------------------
    // Trigger Cache������ �ʱ�ȭ�Ѵ�.
    // remote ���� smiGetTableId�� triggerOID ���� �� ����.
    // �̹� triggerInfo�� ������ triggerOID�� ���� �Ѵ�.
    //---------------------------------------

    // bug-29598
    idlOS::snprintf( sLatchName,
                     IDU_MUTEX_NAME_LEN,
                     "TRIGGER_%"ID_vULONG_FMT"_OBJECT_LATCH",
                     aTriggerOID );

    // Latch ������ �ʱ�ȭ
    IDE_TEST( sTriggerCache->latch.initialize( sLatchName )
              != IDE_SUCCESS );
    sStage = 3;

    // Valid ������ �ʱ�ȭ
    sTriggerCache->isValid = ID_FALSE;

    //---------------------------------------
    // Trigger Statement�� �ʱ�ȭ
    //---------------------------------------

    // ���� Stack Count�� ���� ������ Template ���翡 ���Ͽ�
    // Session �����κ��� �����ȴ�.
    IDE_TEST( qcg::allocStatement( & sTriggerCache->triggerStatement,
                                   NULL,
                                   NULL,
                                   NULL )
              != IDE_SUCCESS );
    sStage = 4;

    /* BUG-44563
       trigger ���� �� server stop�ϸ� ������ �����ϴ� ��찡 �߻��մϴ�. */
    IDE_TEST( qcg::freeSession( & sTriggerCache->triggerStatement ) != IDE_SUCCESS );

    //--------------------------------------
    // Handle�� Trigger Object Cache ������ ����
    // triggerInfo->handle ������ �����̱� ������ remote�� ����
    // handle�� �������� �ʰ� hash table���� ���ͼ� ����Ѵ�.
    //--------------------------------------

    (void)smiObject::setObjectTempInfo( aTriggerHandle, (void*) sTriggerCache );

    *aCache = sTriggerCache;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch ( sStage )
    {
        case 4:
            (void) qcg::freeStatement( & sTriggerCache->triggerStatement );
        case 3:
            (void) sTriggerCache->latch.destroy();
        case 2:
            (void) iduMemMgr::free( sTriggerCache->stmtText );
            sTriggerCache->stmtText = NULL;
        case 1:
            (void) iduMemMgr::free( sTriggerCache );
            sTriggerCache = NULL;
        case 0:
            // Nothing To Do
            break;
    }

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qdnTrigger::dropTriggerObject( qcStatement     * aStatement,
                               void            * aTriggerHandle )
{
/***********************************************************************
 *
 * Description :
 *    Trigger Object�� �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qdnTrigger::dropTriggerObject"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    //---------------------------------------
    // ���ռ� �˻�
    //---------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aTriggerHandle != NULL );

    //---------------------------------------
    // Trigger Object�� ����
    //---------------------------------------

    IDE_TEST( smiObject::dropObject( QC_SMI_STMT( aStatement ),
                                     aTriggerHandle )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qdnTrigger::freeTriggerCache( qdnTriggerCache * aCache )
{
/***********************************************************************
 *
 * Description :
 *    Trigger Handle�� ������  Trigger Object Cache�� �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qdnTrigger::freeTriggerCache"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    //---------------------------------------
    // Trigger Cache�� Statement ������ ����
    //---------------------------------------

    if ( qcg::freeStatement( & aCache->triggerStatement ) != IDE_SUCCESS )
    {
        IDE_ERRLOG( IDE_QP_1 );
    }
    else
    {
        // Nothing to do.
    }

    //---------------------------------------
    // Trigger Cache�� Text ������ ����
    //---------------------------------------

    // To Fix BUG-12034
    // �ϳ��� qdnTriggerCache�� ���� freeTriggerCache�� �ι��� ȣ��Ǹ� ����!
    IDE_DASSERT( aCache->stmtText != NULL );

    if ( iduMemMgr::free( aCache->stmtText ) != IDE_SUCCESS )
    {
        IDE_ERRLOG( IDE_QP_1 );
    }
    else
    {
        // Nothing to do.
    }

    aCache->stmtText = NULL;

    //---------------------------------------
    // Latch ����
    //---------------------------------------

    if ( aCache->latch.destroy() != IDE_SUCCESS )
    {
        IDE_ERRLOG( IDE_QP_1 );
    }
    else
    {
        // Nothing to do.
    }

    //---------------------------------------
    // Trigger Cache�� ����
    //---------------------------------------

    if ( iduMemMgr::free( aCache ) != IDE_SUCCESS )
    {
        IDE_ERRLOG( IDE_QP_1 );
    }
    else
    {
        // Nothing to do.
    }

    return IDE_SUCCESS;

#undef IDE_FN
}

void qdnTrigger::freeTriggerCacheArray( qdnTriggerCache ** aTriggerCaches,
                                        UInt               aTriggerCount )
{
    UInt i = 0;

    if ( aTriggerCaches != NULL )
    {
        for ( i = 0; i < aTriggerCount; i++ )
        {
            if ( aTriggerCaches[i] != NULL )
            {
                (void)freeTriggerCache( aTriggerCaches[i] );
                aTriggerCaches[i] = NULL;
            }
            else
            {
                /* Nothing to do */
            }
        }
    }
    else
    {
        /* Nothing to do */
    }

    return;
}

void qdnTrigger::restoreTempInfo( qdnTriggerCache ** aTriggerCaches,
                                  qcmTableInfo     * aTableInfo )
{
    UInt i = 0;

    if ( ( aTriggerCaches != NULL ) && ( aTableInfo != NULL ) )
    {
        for ( i = 0; i < aTableInfo->triggerCount; i++ )
        {
            if ( aTriggerCaches[i] != NULL )
            {
                (void)smiObject::setObjectTempInfo( aTableInfo->triggerInfo[i].triggerHandle,
                                                    aTriggerCaches[i] );
            }
            else
            {
                /* Nothing to do */
            }
        }
    }
    else
    {
        /* Nothing to do */
    }

    return;
}

IDE_RC
qdnTrigger::checkCondition( qcStatement         * aStatement,
                            qcmTriggerInfo      * aTriggerInfo,
                            qcmTriggerGranularity aGranularity,
                            qcmTriggerEventTime   aEventTime,
                            UInt                  aEventType,
                            smiColumnList       * aUptColumn,
                            idBool              * aNeedAction,
                            idBool              * aIsRecompiled )
{
/***********************************************************************
 *
 * Description :
 *    �ش� Trigger�� �����ؾ� �ϴ� Trigger���� �˻��Ѵ�.
 *
 * Implementation :
 *
 *    Trigger ������ �˻�
 *       - ENABLE ����
 *       - ������ Granularity ����(ROW/STMT)
 *       - ������ Event Time ����(BEFORE/AFTER)
 *       - ������ Event Type ����(INSERT, DELETE, UPDATE)
 *           : UPDATE�� ��� OF�� �˻絵 �ʿ�
 *
 *       - WHEN Condition�� ���� ���
 *           : ���� ����ÿ� �˻��Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "qdnTrigger::checkCondition"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    idBool                       sNeedAction;
    smiColumnList              * sColumn;
    qcStatement                * sTriggerStatement;
    qdnCreateTriggerParseTree  * sTriggerParseTree;
    qdnTriggerCache            * sTriggerCache;
    qdnTriggerEventTypeList    * sEventTypeList;
    qcmColumn                  * sUptColumn;
    UInt                         sStage = 0;

    //---------------------------------------
    // ���ռ� �˻�
    //---------------------------------------

    IDE_DASSERT( aStatement    != NULL );
    IDE_DASSERT( aTriggerInfo  != NULL );
    IDE_DASSERT( aNeedAction   != NULL );
    IDE_DASSERT( aIsRecompiled != NULL );

    //---------------------------------------
    // Event ������ �˻�
    //---------------------------------------

    sNeedAction = ID_FALSE;

    /* BUG-44819 runDMLforDDL, runDMLforInternal �� �����ϴ� SQL �� ��� 
     *           trigger �� �����ϸ� �ȵ˴ϴ� */
    if ( ( aStatement->session->mMmSession != NULL ) &&
         ( aTriggerInfo->enable == QCM_TRIGGER_ENABLE ) &&
         ( aTriggerInfo->granularity == aGranularity ) &&
         ( aTriggerInfo->eventTime == aEventTime ) &&
         ( (aTriggerInfo->eventType & aEventType) != 0 ) )
    {
        // ��� Event ������ �����ϴ� ���
        if ( aEventType == QCM_TRIGGER_EVENT_UPDATE )
        {
            //-----------------------------------------
            // UPDATE Event�� ��� OF ������ �˻��ؾ� ��.
            //-----------------------------------------

            // BUG-16543
            // aTriggerInfo->uptColumnID�� ��Ȯ���� �����Ƿ� �������� �ʰ�
            // triggerStatement�� �����Ѵ�.
            if ( aTriggerInfo->uptCount == 0 )
            {
                sNeedAction = ID_TRUE;
            }
            else
            {
                sNeedAction = ID_FALSE;

                IDE_TEST( smiObject::getObjectTempInfo( aTriggerInfo->triggerHandle,
                                                        (void**)&sTriggerCache )
                          != IDE_SUCCESS );

                while ( 1 )
                {
                    IDE_TEST( sTriggerCache->latch.lockRead (
                                  NULL, /* idvSQL* */
                                  NULL /* idvWeArgs* */ )
                              != IDE_SUCCESS );
                    sStage = 1;

                    if ( sTriggerCache->isValid != ID_TRUE )
                    {
                        sStage = 0;
                        IDE_TEST( sTriggerCache->latch.unlock() != IDE_SUCCESS );

                        while ( recompileTrigger( aStatement,
                                                  aTriggerInfo )
                                != IDE_SUCCESS )
                        {
                            // rebuild error��� �ٽ� recompile�� �õ��Ѵ�.
                            IDE_TEST( ideIsRebuild() != IDE_SUCCESS );
                        }

                        // PROJ-2219 Row-level before update trigger
                        // qmnUPTE::firstInit()���� trigger�� �˻��Ͽ�,
                        // invalid �����̸� compile �ϰ�, update DML�� rebuild �Ѵ�.
                        *aIsRecompiled = ID_TRUE;

                        continue;
                    }
                    else
                    {
                        sTriggerStatement = & sTriggerCache->triggerStatement;
                        sTriggerParseTree = (qdnCreateTriggerParseTree *)
                            sTriggerStatement->myPlan->parseTree;
                        sEventTypeList    =
                            sTriggerParseTree->triggerEvent.eventTypeList;

                        for ( sColumn = aUptColumn;
                              sColumn != NULL;
                              sColumn = sColumn->next )
                        {
                            for ( sUptColumn = sEventTypeList->updateColumns;
                                  sUptColumn != NULL;
                                  sUptColumn = sUptColumn->next )
                            {
                                if ( sUptColumn->basicInfo->column.id ==
                                     sColumn->column->id )
                                {
                                    sNeedAction = ID_TRUE;
                                    break;
                                }
                                else
                                {
                                    // Nothing to do.
                                }
                            }

                            if ( sNeedAction == ID_TRUE )
                            {
                                break;
                            }
                            else
                            {
                                // Nothing to do.
                            }
                        } //for ( sColumn = aUptColumn;
                    }

                    sStage = 0;
                    IDE_TEST( sTriggerCache->latch.unlock() != IDE_SUCCESS );

                    break;
                } // while
            } // else
        }
        else
        {
            sNeedAction = ID_TRUE;
        }
    }
    else
    {
        // Event ������ �������� �ʴ� ���
        sNeedAction = ID_FALSE;
    }

    *aNeedAction = sNeedAction;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch ( sStage )
    {
        case 1:
            (void) sTriggerCache->latch.unlock();
        default:
            break;
    }

    return IDE_FAILURE;

#undef IDE_FN
}


/***********************************************************************
 *
 *   [Trigger ������ ���ü� ����]
 *
 *    ���) t1, t2 : table, r1, r2 : trigger
 *
 *    - Trigger�� ����(r1)�� Subject Table(t1)�� ���� DDL
 *
 *
 *                DDL  |
 *                    (X)
 *                     |
 *                     V
 *     DML --(IX)-->  [t1] --(LS)--> [r1] --(IX)--> [t2]
 *
 *     ���� �׸����� ������ Subject Table�� ���� DDL�� X lock��
 *     DML�� IX lock�� ���Ͽ� ����ȴ�.
 *
 *    - Trigger�� ����(r1)�� Action Table(t2) �� ���� DDL
 *
 *      PSM�� ������ ������� ó���� ��� deadlock�� �߻��� �� �ִ�.
 *
 *
 *                                              DDL  |
 *                                                  (X)
 *                                                   |
 *                                                   V
 *                                   [  ] <--(LX)-- [  ]
 *     DML --(IX)-->  [t1] --(LS)--> [r1]           [t2]
 *                                   [  ] --(IX)--> [  ]
 *
 *     ���� �׸����� ������ DML�κ��� �����ϴ� [r1] --> [t2]���� IX��
 *     DDL�κ��� �����ϴ� [t2] --> [r1] ������ (LX) �� ���� DDL�� X��
 *     DML�� (LS) �� release�� ��ٸ��� �Ǿ� deadlock�� ������ �ȴ�.
 *     �̷��� ������ t2�� ������ r1�� �ݿ��Ϸ��� �õ��� ���� �߻��ϸ�,
 *     PSM������ �̷��� ������ �ذ��ϱ� ���� �ѹ� invalid �Ǹ�,
 *     ��������� recompile�ϱ� �������� �׻� recompile�ϰ� �Ǵ� ������
 *     �����Ѵ�.
 *
 *     �̷��� ������ �ذ��ϱ� ���ؼ��� ���� �׸��� ����
 *     Lock�� Latch�� ������ ���� �ٸ� ��찡 ���� �����Ͽ��� �ذ��Ͽ��� �Ѵ�.
 *
 *
 *                                              DDL  |
 *                                                  (X)
 *                                                   |
 *                                                   V
 *     DML --(IX)-->  [t1] --(LS)--> [r1] --(IX)--> [t2]
 *
 *     ��, Invalid�� ���δ� Trigger�� �����ϴ� ���������� �Ǵ��ϰ�,
 *     Trigger�� fire�ϴ� Session���� ���ü� ��� ����ϸ� �ȴ�.
 *
 *                            [DML]
 *                             |
 *                            (LS)
 *                             |
 *                             V
 *           [DML]  --(LS)--> [r1] --------(IX)------> [t2]
 *                             ^             |
 *                             |             |
 *                            (LX)           |
 *                             |             |
 *                             +-------------+
 *
 ***********************************************************************/


IDE_RC
qdnTrigger::fireTriggerAction( qcStatement           * aStatement,
                               iduMemory             * aNewValueMem,
                               qcmTableInfo          * aTableInfo,
                               qcmTriggerInfo        * aTriggerInfo,
                               smiTableCursor        * aTableCursor,
                               scGRID                  aGRID,
                               void                  * aOldRow,
                               qcmColumn             * aOldRowColumns,
                               void                  * aNewRow,
                               qcmColumn             * aNewRowColumns )
{
/***********************************************************************
 *
 * Description :
 *    Trigger Action�� ���� Validation �� ����
 *    Recompile �� Trigger Action�� �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qdnTrigger::fireTriggerAction"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qdnTriggerCache * sCache;
    UInt              sErrorCode;
    UInt              sStage = 0;

    //---------------------------------------
    // ���ռ� �˻�
    //---------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aTriggerInfo != NULL );

    //---------------------------------------
    // Trigger Cache������ ȹ��
    //---------------------------------------

    IDE_TEST( smiObject::getObjectTempInfo( aTriggerInfo->triggerHandle,
                                            (void**)&sCache )
              != IDE_SUCCESS );

    while ( 1 )
    {
        // S Latch ȹ��
        IDE_TEST( sCache->latch.lockRead (
                      NULL, /* idvSQL * */
                      NULL /* idvWeArgs* */ ) != IDE_SUCCESS );
        sStage = 1;

        if ( sCache->isValid != ID_TRUE )
        {
            //----------------------------------------
            // Valid���� �ʴٸ� Recompile �Ѵ�.
            //----------------------------------------

            // Release Latch
            sStage = 0;
            IDE_TEST( sCache->latch.unlock() != IDE_SUCCESS );

            while ( recompileTrigger( aStatement,
                                      aTriggerInfo )
                    != IDE_SUCCESS )
            {
                // rebuild error��� �ٽ� recompile�� �õ��Ѵ�.
                IDE_TEST( ideIsRebuild() != IDE_SUCCESS );
            }

            continue;
        }
        else
        {
            //----------------------------------------
            // Valid�ϴٸ� Trigger Action�� �����Ѵ�.
            //----------------------------------------

            if ( runTriggerAction( aStatement,
                                   aNewValueMem,
                                   aTableInfo,
                                   aTriggerInfo,
                                   aTableCursor,
                                   aGRID,
                                   aOldRow,
                                   aOldRowColumns,
                                   aNewRow,
                                   aNewRowColumns )
                 != IDE_SUCCESS )
            {
                sErrorCode = ideGetErrorCode();
                switch( sErrorCode & E_ACTION_MASK )
                {
                    case E_ACTION_RETRY:
                    case E_ACTION_REBUILD:

                        //------------------------------
                        // Rebuild �ؾ� ��.
                        //------------------------------

                        // Release Latch
                        sStage = 0;
                        IDE_TEST( sCache->latch.unlock() != IDE_SUCCESS );

                        // X Latch ȹ��
                        IDE_TEST( sCache->latch.lockWrite (
                                      NULL, /* idvSQL* */
                                      NULL /* idvWeArgs* */ )
                                  != IDE_SUCCESS );
                        sStage = 1;

                        sCache->isValid = ID_FALSE;

                        sStage = 0;
                        IDE_TEST( sCache->latch.unlock() != IDE_SUCCESS );

                        break;
                    default:

                        //------------------------------
                        // �ٸ� Error�� ������.
                        //------------------------------

                        // Release Latch
                        sStage = 0;
                        IDE_TEST( sCache->latch.unlock() != IDE_SUCCESS );

                        break;
                }

                IDE_TEST( 1 );
            }
            else
            {
                // Release Latch
                sStage = 0;
                IDE_TEST( sCache->latch.unlock() != IDE_SUCCESS );

                break;
            }
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch ( sStage )
    {
        case 1:
            (void) sCache->latch.unlock();
        default:
            break;
    }

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qdnTrigger::runTriggerAction( qcStatement           * aStatement,
                              iduMemory             * aNewValueMem,
                              qcmTableInfo          * aTableInfo,
                              qcmTriggerInfo        * aTriggerInfo,
                              smiTableCursor        * aTableCursor,
                              scGRID                  aGRID,
                              void                  * aOldRow,
                              qcmColumn             * aOldRowColumns,
                              void                  * aNewRow,
                              qcmColumn             * aNewRowColumns )
{
/***********************************************************************
 *
 * Description :
 *    Trigger Action�� �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qdnTrigger::runTriggerAction"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    idBool                      sJudge;

    qdnTriggerCache           * sCache;
    qcStatement               * sTriggerStatement;
    qdnCreateTriggerParseTree * sTriggerParseTree;

    qcTemplate                * sClonedTriggerTemplate;

    SInt                        sOrgRemain;
    mtcStack                  * sOrgStack;
    UInt                        sStage = 0;

    qtcNode                sDummyNode;
    iduMemoryStatus        sMemStatus;

    qsxProcPlanList      * sOriProcPlanList;
    idBool                 sLatched = ID_FALSE;

    //---------------------------------------
    // ���ռ� �˻�
    //---------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aTriggerInfo != NULL );

    // procPlanList ���
    sOriProcPlanList = aStatement->spvEnv->procPlanList;

    //---------------------------------------
    // Trigger Cache������ ȹ��
    //---------------------------------------

    IDE_TEST( smiObject::getObjectTempInfo( aTriggerInfo->triggerHandle,
                                            (void**)&sCache )
              != IDE_SUCCESS );;

    sTriggerStatement = & sCache->triggerStatement;
    sTriggerParseTree =
        (qdnCreateTriggerParseTree * ) sTriggerStatement->myPlan->parseTree;

    //---------------------------------------
    // Trigger �� ����
    // Action Body�� ������ ���Ͽ� EXEC proc1�� ������ routine�� �����.
    //---------------------------------------

    // To Fix PR-11368
    // �޸� ������ ���� ��ġ ����
    IDE_TEST( aStatement->qmtMem->getStatus( & sMemStatus )
              != IDE_SUCCESS );

    sStage = 1;

    // Ʈ������ 0�� template�� �����Ѵ�.
    IDU_LIMITPOINT("qdnTrigger::runTriggerAction::malloc");
    IDE_TEST( aStatement->qmtMem->alloc( ID_SIZEOF(qcTemplate),
                                         (void **)&sClonedTriggerTemplate )
              != IDE_SUCCESS );

    IDE_TEST( qcg::cloneAndInitTemplate( aStatement->qmtMem,
                                         QC_SHARED_TMPLATE(sTriggerStatement),
                                         sClonedTriggerTemplate )
              != IDE_SUCCESS );

    // PROJ-2002 Column Security
    // trigger ����� statement�� �ʿ��ϴ�.
    sClonedTriggerTemplate->stmt = aStatement;

    //---------------------------------------
    // REREFERENCING ROW�� ����
    //---------------------------------------

    IDE_TEST( setReferencingRow( sClonedTriggerTemplate,
                                 aTableInfo,
                                 sTriggerParseTree,
                                 aTableCursor,
                                 aGRID,
                                 aOldRow,
                                 aOldRowColumns,
                                 aNewRow,
                                 aNewRowColumns )
              != IDE_SUCCESS );

    //---------------------------------------
    // WHEN ������ �˻�
    //---------------------------------------

    sJudge = ID_TRUE;

    if ( sTriggerParseTree->actionCond.whenCondition != NULL )
    {
        sOrgRemain = QC_PRIVATE_TMPLATE(aStatement)->tmplate.stackRemain;
        sOrgStack = QC_PRIVATE_TMPLATE(aStatement)->tmplate.stack;

        sStage = 2;

        sClonedTriggerTemplate->tmplate.stackRemain =
            QC_PRIVATE_TMPLATE(aStatement)->tmplate.stackRemain;
        sClonedTriggerTemplate->tmplate.stack =
            QC_PRIVATE_TMPLATE(aStatement)->tmplate.stack;

        IDE_TEST( qtc::judge( & sJudge,
                              sTriggerParseTree->actionCond.whenCondition,
                              sClonedTriggerTemplate )
                  != IDE_SUCCESS );

        sStage = 1;

        QC_PRIVATE_TMPLATE(aStatement)->tmplate.stackRemain = sOrgRemain;
        QC_PRIVATE_TMPLATE(aStatement)->tmplate.stack = sOrgStack;
    }
    else
    {
        // Nothing To Do
    }

    //---------------------------------------
    // Trigger Action Body�� ����
    //---------------------------------------

    if ( sJudge == ID_TRUE )
    {
        // WHEN ������ �����ϴ� ���

        // Dummy Node �ʱ�ȭ
        idlOS::memset( & sDummyNode, 0x00, ID_SIZEOF( qtcNode ) );
        sDummyNode.node.arguments = NULL;

        // BUG-20797
        // Trigger���� ����� PSM�� s_latch ȹ��
        if( sLatched == ID_FALSE )
        {
            IDE_TEST_RAISE( qsxRelatedProc::latchObjects( sTriggerStatement->spvEnv->procPlanList )
                            != IDE_SUCCESS, invalid_procedure );
            sLatched = ID_TRUE;

            IDE_TEST_RAISE( qsxRelatedProc::checkObjects( sTriggerStatement->spvEnv->procPlanList )
                            != IDE_SUCCESS, invalid_procedure );
        }
        else
        {
            // Nothing to do.
        }

        // Trigger�� ����� PSM�� procPlanList���� �˻��� �� �ֵ��� ���
        aStatement->spvEnv->procPlanList = sTriggerStatement->spvEnv->procPlanList;

        // Ʈ���� ���� �ÿ��� statement�� ���� ����尡 �̷�� ����.
        aStatement->spvEnv->flag &= ~QSV_ENV_TRIGGER_MASK;
        aStatement->spvEnv->flag |= QSV_ENV_TRIGGER_TRUE;

        // Trigger�� ����
        IDE_TEST( qsx::callProcWithNode ( aStatement,
                                          & sTriggerParseTree->actionBody,
                                          & sDummyNode,
                                          NULL,
                                          sClonedTriggerTemplate )
                  != IDE_SUCCESS );

        // Trigger���� ����� PSM�� s_latch ����
        if( sLatched == ID_TRUE )
        {
            sLatched = ID_FALSE;
            IDE_TEST( qsxRelatedProc::unlatchObjects( sTriggerStatement->spvEnv->procPlanList )
                      != IDE_SUCCESS );
        }
        else
        {
            // Nothing to do.
        }
    }
    else
    {
        // Nohting To Do
    }

    sStage = 0;

    //---------------------------------------
    // REREFERENCING ROW�� output����
    //---------------------------------------
    // To fix BUG-12622
    // before trigger���� new row�� ������ ���
    // valuelist�� ����� �κ��� �����Ѵ�.
    IDE_TEST( makeValueListFromReferencingRow(
                  sClonedTriggerTemplate,
                  aNewValueMem,
                  aTableInfo,
                  sTriggerParseTree,
                  aNewRow,
                  aNewRowColumns )
              != IDE_SUCCESS );

    IDE_TEST( aStatement->qmtMem->setStatus( & sMemStatus )
              != IDE_SUCCESS );

    // procPlanList ����
    aStatement->spvEnv->procPlanList = sOriProcPlanList;

    return IDE_SUCCESS;

    IDE_EXCEPTION( invalid_procedure );
    {
        IDE_SET(ideSetErrorCode(qpERR_REBUILD_QCI_PROC_INVALID));
    }
    IDE_EXCEPTION_END;

    switch ( sStage )
    {
        case 2:
            QC_PRIVATE_TMPLATE(aStatement)->tmplate.stackRemain = sOrgRemain;
            QC_PRIVATE_TMPLATE(aStatement)->tmplate.stack = sOrgStack;
        case 1:
            (void)aStatement->qmtMem->setStatus( & sMemStatus );
        default:
            break;
    }

    if( sLatched == ID_TRUE )
    {
        (void) qsxRelatedProc::unlatchObjects( sTriggerStatement->spvEnv->procPlanList );
    }
    else
    {
        // Nothing to do.
    }

    // procPlanList ����
    aStatement->spvEnv->procPlanList = sOriProcPlanList;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qdnTrigger::setReferencingRow( qcTemplate                * aClonedTemplate,
                               qcmTableInfo              * aTableInfo,
                               qdnCreateTriggerParseTree * aParseTree,
                               smiTableCursor            * aTableCursor,
                               scGRID                      aGRID,
                               void                      * aOldRow,
                               qcmColumn                 * aOldRowColumns,
                               void                      * aNewRow,
                               qcmColumn                 * aNewRowColumns )
{
/***********************************************************************
 *
 * Description :
 *
 *    WHEN Condition �� Trigger Action Body�� �����ϱ� ����
 *    REFERENCING ROW ������ Trigger�� Template�� �����
 *    Cloned Template �� �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qdnTrigger::setReferencingRow"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qdnTriggerRef             * sRef;
    qsVariables               * sVariable;

    //---------------------------------------
    // ���ռ� �˻�
    //---------------------------------------

    IDE_DASSERT( aClonedTemplate != NULL );
    IDE_DASSERT( aParseTree != NULL );

    //---------------------------------------
    // REREFERENCING ROW�� ����
    //---------------------------------------

    for ( sRef = aParseTree->triggerReference;
          sRef != NULL;
          sRef = sRef->next )
    {

        //---------------------------------------
        // WHEN Conditition �� Action Body�� ���� REREFERENCING ROW�� ����
        //---------------------------------------

        for ( sVariable =
                  (qsVariables*) aParseTree->actionBody.paraDecls;
              sVariable != NULL;
              sVariable = (qsVariables*) sVariable->common.next )
        {
            if ( sRef->rowVar == sVariable )
            {
                break;
            }
        }

        // �ݵ�� �ش� Variable �� �����ؾ� ��.
        IDE_TEST_RAISE( sVariable == NULL, ERR_REF_ROW_NOT_FOUND );

        switch ( sRef->refType )
        {
            case QCM_TRIGGER_REF_OLD_ROW:
                if ( aParseTree->triggerEvent.eventTime == QCM_TRIGGER_INSTEAD_OF )
                {
                    if ( (aOldRow != NULL) && (aOldRowColumns != NULL) )
                    {
                        // trigger rowtype�� table row�� ����
                        // instead of delete OLD_ROW smivalue
                        IDE_TEST( copyRowFromValueList(
                                      aClonedTemplate,
                                      aTableInfo,
                                      sVariable,
                                      aOldRow,
                                      aOldRowColumns )
                                  != IDE_SUCCESS );
                    }
                    else
                    {
                        // Nothing to do.
                    }
                }
                else
                {
                    if ( (aOldRow != NULL) && (aOldRowColumns != NULL) )
                    {
                        // trigger rowtype�� table row�� ����
                        IDE_TEST( copyRowFromTableRow( aClonedTemplate,
                                                       aTableInfo,
                                                       sVariable,
                                                       aTableCursor,
                                                       aOldRow,
                                                       aGRID,
                                                       aOldRowColumns, 
                                                       QCM_TRIGGER_REF_OLD_ROW /* reftype */ )
                                  != IDE_SUCCESS );
                    }
                    else
                    {
                        // Nothing to do.
                    }
                }
                break;

            case QCM_TRIGGER_REF_NEW_ROW:
                if ( (aNewRow != NULL) && (aNewRowColumns != NULL) )
                {
                    // To fix BUG-12622
                    // before trigger�� valuelist�κ���
                    // trigger row�� �����Ѵ�.
                    if( ( aParseTree->triggerEvent.eventTime ==
                          QCM_TRIGGER_BEFORE ) ||
                        ( aParseTree->triggerEvent.eventTime ==
                        QCM_TRIGGER_INSTEAD_OF ) ) //PROJ-1888 INSTEAD OF TRIGGER
                    {

                        // Insert�� ��
                        // aNewRow�� row���°� �ƴ� smivalue������
                        IDE_TEST( copyRowFromValueList(
                                      aClonedTemplate,
                                      aTableInfo,
                                      sVariable,
                                      aNewRow,
                                      aNewRowColumns )
                                  != IDE_SUCCESS );
                    }
                    else
                    {
                        // PROJ-1075
                        // trigger rowtype�� table row�� ����
                        IDE_TEST( copyRowFromTableRow( aClonedTemplate,
                                                       aTableInfo,
                                                       sVariable,
                                                       aTableCursor,
                                                       aNewRow,
                                                       aGRID,
                                                       aNewRowColumns,
                                                       QCM_TRIGGER_REF_NEW_ROW /* reftype */ )
                                  != IDE_SUCCESS );
                    }
                }
                else
                {
                    // Nothing to do.
                }
                break;

            default:
                IDE_DASSERT( 0 );
                break;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_REF_ROW_NOT_FOUND )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qdnTrigger::setReferencingRow",
                                  "Referencing row not found" ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qdnTrigger::makeValueListFromReferencingRow(
    qcTemplate                * aClonedTemplate,
    iduMemory                 * aNewValueMem,
    qcmTableInfo              * aTableInfo,
    qdnCreateTriggerParseTree * aParseTree,
    void                      * aNewRow,
    qcmColumn                 * aNewRowColumns )
{
/***********************************************************************
 *
 * Description : To fix BUG-12622
 *    Before Trigger�� ���� �ٲ� new row�� value list�� �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/
    qdnTriggerRef             * sRef;
    qsVariables               * sVariable;

    //---------------------------------------
    // ���ռ� �˻�
    //---------------------------------------

    IDE_DASSERT( aClonedTemplate != NULL );
    IDE_DASSERT( aNewValueMem != NULL );
    IDE_DASSERT( aParseTree != NULL );

    //---------------------------------------
    // REREFERENCING ROW�� ����
    //---------------------------------------

    for ( sRef = aParseTree->triggerReference;
          sRef != NULL;
          sRef = sRef->next )
    {
        for ( sVariable =
                  (qsVariables*) aParseTree->actionBody.paraDecls;
              sVariable != NULL;
              sVariable = (qsVariables*) sVariable->common.next )
        {
            if ( sRef->rowVar == sVariable )
            {
                break;
            }
        }

        IDE_TEST_RAISE( sVariable == NULL, ERR_REF_ROW_NOT_FOUND );

        switch ( sRef->refType )
        {
            case QCM_TRIGGER_REF_OLD_ROW:
                break;

            case QCM_TRIGGER_REF_NEW_ROW:
                // BUG-46074
                // Before delete trigger���� new row�� reference �� �� �ֵ��� ������.
                // �� ��� param init �������� null row�� �ʱ�ȭ ��.
                if( (aParseTree->triggerEvent.eventTime == QCM_TRIGGER_BEFORE) &&
                    (aClonedTemplate->stmt->spxEnv->mTriggerEventType != QCM_TRIGGER_EVENT_DELETE) )
                {
                    // Insert�� ��
                    IDE_TEST( copyValueListFromRow(
                                  aClonedTemplate,
                                  aNewValueMem,
                                  aTableInfo,
                                  sVariable,
                                  aNewRow,    // valuelist
                                  aNewRowColumns )
                              != IDE_SUCCESS );
                }
                else
                {
                    // Nothing to do.
                }

                break;
            default:
                IDE_DASSERT( 0 );
                break;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_REF_ROW_NOT_FOUND )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qdnTrigger::makeValueListFromReferencingRow",
                                  "Referencing row not found" ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qdnTrigger::checkObjects( qcStatement  * aStatement,
                          idBool       * aInvalidProc )
{
/***********************************************************************
 *
 * Description : BUG-20797
 *    Trigger Statement�� ���� PSM�� ������ �˻��Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qdnTrigger::checkObjects"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    idBool             sInvalidProc = ID_FALSE;

    //---------------------------------------
    // ���ռ� �˻�
    //---------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aInvalidProc != NULL );

    //---------------------------------------
    // Trigger���� ����� PSM�� ������ �˻�
    //---------------------------------------

    if ( aStatement->spvEnv->procPlanList != NULL )
    {
        if ( qsxRelatedProc::latchObjects(
                 aStatement->spvEnv->procPlanList )
             == IDE_SUCCESS )
        {
            if ( qsxRelatedProc::checkObjects( aStatement->spvEnv->procPlanList )
                 != IDE_SUCCESS )
            {
                sInvalidProc = ID_TRUE;
            }
            else
            {
                // Nothing to do.
            }

            (void) qsxRelatedProc::unlatchObjects( aStatement->spvEnv->procPlanList );
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

    *aInvalidProc = sInvalidProc;

    return IDE_SUCCESS;

#undef IDE_FN
}

IDE_RC
qdnTrigger::recompileTrigger( qcStatement     * aStatement,
                              qcmTriggerInfo  * aTriggerInfo )
{
/***********************************************************************
 *
 * Description :
 *    ������ PVO ������ ���Ͽ� Trigger Cache ������ �����Ѵ�.
 *
 * Implementation :
 *    X Latch�� ���� �Ŀ� validation �˻縦 ����
 *    Recompile ���θ� �����Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "qdnTrigger::recompileTrigger"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    SChar                        sErrorMsg[ QC_MAX_OBJECT_NAME_LEN * 2 + 2 ];

    qdnTriggerCache            * sCache;
    qcStatement                * sTriggerStatement;
    smiStatement                 sSmiStmt;
    smiStatement               * sSmiStmtOrg;
    qdnCreateTriggerParseTree  * sTriggerParseTree;
    UInt                         sStage = 0;
    idBool                       sAllocFlag = ID_FALSE;
    idBool                       sInvalidProc;
    qcuSqlSourceInfo             sSqlInfo;

    //---------------------------------------
    // ���ռ� �˻�
    //---------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aTriggerInfo != NULL );

    //---------------------------------------
    // Trigger Cache������ ȹ��
    //---------------------------------------

    IDE_TEST( smiObject::getObjectTempInfo( aTriggerInfo->triggerHandle,
                                            (void**)&sCache )
              != IDE_SUCCESS );

    sTriggerStatement = & sCache->triggerStatement;

    // X Latch ȹ��
    IDE_TEST( sCache->latch.lockWrite (
                  NULL, /* idvSQL * */
                  NULL /* idvWeArgs* */ )
              != IDE_SUCCESS );
    sStage = 1;

    IDE_TEST( checkObjects( aStatement, & sInvalidProc )
              != IDE_SUCCESS );

    // fix BUG-18679
    // ALTER TRIGGER COMPILE �ÿ��� ������ recompile�Ѵ�.
    if( (aStatement->myPlan->parseTree->stmtKind != QCI_STMT_SCHEMA_DDL) &&
        (sCache->isValid == ID_TRUE) &&
        (sInvalidProc == ID_FALSE) )
    {
        IDE_CONT( already_valid );
    }
    else
    {
        // Nothing to do
    }

    //---------------------------------------
    // Trigger Cache Statement�� �ʱ�ȭ
    //---------------------------------------

    // Trigger Statement�� ���� ���� �ʱ�ȭ
    IDE_TEST( qcg::freeStatement( sTriggerStatement )
              != IDE_SUCCESS );

    // ���� Stack Count�� ���� ������ Template ���翡 ���Ͽ�
    // Session �����κ��� �����ȴ�.
    IDE_TEST( qcg::allocStatement( sTriggerStatement,
                                   aStatement->session,
                                   NULL,
                                   NULL )
              != IDE_SUCCESS );

    //fix BUG-18158
    sAllocFlag = ID_TRUE;

    // PVO ������ �߻��ϰ� �ǹǷ� ���ο� smiStatement�� �����Ѵ�.
    // Parent�� ������ smiStatement�� ����Ѵ�(by sjkim)
    IDE_TEST( sSmiStmt.begin( aStatement->mStatistics,
                              QC_SMI_STMT( aStatement ),
                              SMI_STATEMENT_NORMAL |
                              SMI_STATEMENT_MEMORY_CURSOR )
              != IDE_SUCCESS );

    qcg::getSmiStmt( sTriggerStatement, & sSmiStmtOrg );
    qcg::setSmiStmt( sTriggerStatement, & sSmiStmt );
    sStage = 2;

    // Statement Text������ ����
    sTriggerStatement->myPlan->stmtTextLen = sCache->stmtTextLen;
    sTriggerStatement->myPlan->stmtText = sCache->stmtText;

    //---------------------------------------
    // Trigger Cache Statement�� PVO ����
    //---------------------------------------

    // Parsing
    IDE_TEST( qcpManager::parseIt( sTriggerStatement ) != IDE_SUCCESS );

    // To Fix PR-10645
    // Parsing�� ��ġ�� DDL�� �����Ǿ� ON���� Table���� ȹ��� Table X Lock��
    // ȹ���ϰ� �ȴ�.  �� ��, �ٸ� DML�� ���� ���̸� Table X Lock�� ȸ������
    // ���Ͽ� Recompile�� �����ϰ� �ȴ�.
    // �׷���, Recompile ������ DML�� ALTER TRIGGER .. COMPILE
    // ������ ���� �ÿ��� ����ǰ� Meta Table �� Meta Cache�� �����Ű��
    // �۾��� �ƴϴ�.  ����, DDL�� �����Ǿ� �ִ� ������ DML��
    // ������� �־�� �Ѵ�.  Table�� ���Ͽ� IS Lock ���� �õ��ϰ� �Ѵ�.
    sTriggerStatement->myPlan->parseTree->stmtKind = QCI_STMT_MASK_DML;
    sTriggerStatement->myPlan->parseTree->validate = qdnTrigger::validateRecompile;

    sTriggerParseTree =
        (qdnCreateTriggerParseTree * ) sTriggerStatement->myPlan->parseTree;

    // To Fix BUG-10899
    // Ʈ���� �׼� �ٵ�� ���ν��� �����ƾ�� Ÿ�µ�,
    // �� �ӿ��� ������ ���� �� QC_NAME_POSITION���� ������ ��ġ��
    // ��Ȯ�� ������ qcuSqlSourceInfo��ƾ�� Ÿ�µ�, �̶� �� �ʵ���� ���δ�.
    sTriggerParseTree->actionBody.stmtText    = sCache->stmtText;
    sTriggerParseTree->actionBody.stmtTextLen = sCache->stmtTextLen;

    // To fix BUG-14584
    // recompile�� replication�� ������� �����ϱ� ���� isRecompile�� TRUE��.
    sTriggerParseTree->isRecompile = ID_TRUE;
    sTriggerParseTree->triggerUserID = aTriggerInfo->userID;

    // BUG-42989 Create trigger with enable/disable option.
    // ������ enable �ɼ��� �����Ѵ�.
    sTriggerParseTree->actionCond.isEnable = aTriggerInfo->enable;

    (void)qdnTrigger::allocProcInfo( sTriggerStatement,
                                     aTriggerInfo->userID );

    // Post-Parsing
    IDE_TEST_RAISE(
        sTriggerStatement->myPlan->parseTree->parse( sTriggerStatement )
        != IDE_SUCCESS, err_trigger_recompile );
    IDE_TEST( qcg::fixAfterParsing( sTriggerStatement ) != IDE_SUCCESS );
    sStage = 3;

    // Validation
    IDE_TEST_RAISE(
        sTriggerStatement->myPlan->parseTree->validate( sTriggerStatement )
        != IDE_SUCCESS, err_trigger_recompile  );
    IDE_TEST( qcg::fixAfterValidation( sTriggerStatement )
              != IDE_SUCCESS );

    // Optimization
    IDE_TEST_RAISE(
        sTriggerStatement->myPlan->parseTree->optimize( sTriggerStatement )
        != IDE_SUCCESS, err_trigger_recompile );
    IDE_TEST( qcg::fixAfterOptimization( sTriggerStatement )
              != IDE_SUCCESS );

    // BUG-42322
    sTriggerParseTree->actionBody.procOID = aTriggerInfo->triggerOID;

    // PROJ-2219 Row-level before update trigger
    // Row-level before update trigger����
    // new row�� ���� �����ϴ� column list�� �����Ѵ�.
    IDE_TEST( qdnTrigger::makeRefColumnList( sTriggerStatement )
              != IDE_SUCCESS );

    // BUG-16543
    // Meta Table�� ������ �����Ѵ�.
    IDE_TEST( qcmTrigger::removeMetaInfo( sTriggerStatement,
                                          aTriggerInfo->triggerOID )
              != IDE_SUCCESS );
    
    IDE_TEST( qcmTrigger::addMetaInfo( sTriggerStatement,
                                       aTriggerInfo->triggerHandle )
              != IDE_SUCCESS );

    sCache->isValid = ID_TRUE;

    IDE_TEST( qsxRelatedProc::unlatchObjects( sTriggerStatement->spvEnv->procPlanList )
              != IDE_SUCCESS );
    sTriggerStatement->spvEnv->latched = ID_FALSE;

    sStage = 1;
    qcg::setSmiStmt( sTriggerStatement, sSmiStmtOrg );

    IDE_TEST( sSmiStmt.end(SMI_STATEMENT_RESULT_SUCCESS) != IDE_SUCCESS);

    // fix BUG-18679
    IDE_EXCEPTION_CONT( already_valid );

    sStage = 0;
    IDE_TEST( sCache->latch.unlock() != IDE_SUCCESS );

    //fix BUG-18158
    if( sAllocFlag == ID_TRUE )
    {
        sTriggerStatement->session = NULL;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_trigger_recompile );
    {
        if ( ideIsRebuild() != IDE_SUCCESS )
        {
            (void)sSqlInfo.initWithBeforeMessage(
                QC_QMX_MEM( aStatement ) );

            idlOS::snprintf( sErrorMsg, ID_SIZEOF(sErrorMsg),
                             "%s.%s",
                             aTriggerInfo->userName,
                             aTriggerInfo->triggerName );

            IDE_SET(ideSetErrorCode( qpERR_ABORT_TRIGGER_RECOMPILE,
                                     sErrorMsg,
                                     "\n\n",
                                     sSqlInfo.getBeforeErrMessage() ));

            (void)sSqlInfo.fini();
        }
        else
        {
            // rebuild error�� �ö���� ���
            // error pass
        }
    }
    IDE_EXCEPTION_END;

    sCache->isValid = ID_FALSE;

    switch( sStage )
    {
        case 3 :
            if ( qsxRelatedProc::unlatchObjects( sTriggerStatement->spvEnv->procPlanList )
                 == IDE_SUCCESS )
            {
                sTriggerStatement->spvEnv->latched = ID_FALSE;
            }
            else
            {
                IDE_ERRLOG(IDE_QP_1);
            }

        case 2 :
            qcg::setSmiStmt( sTriggerStatement, sSmiStmtOrg );

            if (sSmiStmt.end(SMI_STATEMENT_RESULT_FAILURE) != IDE_SUCCESS)
            {
                IDE_CALLBACK_FATAL("Trigger recompile smiStmt.end() failed");
            }

        case 1:
            // Release Latch
            (void) sCache->latch.unlock();
    }

    //fix BUG-18158
    if( sAllocFlag == ID_TRUE )
    {
        sTriggerStatement->session = NULL;
    }

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qdnTrigger::setInvalidTriggerCache4Table( qcmTableInfo * aTableInfo )
{
/***********************************************************************
 *
 * Description :
 *    BUG-16543
 *    qdbAlter::executeDropCol���� column_id�� ����� �� �����Ƿ�
 *    triggerCache�� invalid�� �����Ͽ� recompile�� �� �ְ� �Ѵ�.
 *
 * Implementation :
 *    X Latch�� ���� �Ŀ� invalid�� �����Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "qdnTrigger::setInvalidTriggerCache4Table"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qdnTriggerCache            * sCache;
    UInt                         sStage = 0;
    UInt                         i;

    for ( i = 0; i < aTableInfo->triggerCount; i++ )
    {
        IDE_TEST( smiObject::getObjectTempInfo( aTableInfo->triggerInfo[i].triggerHandle,
                                                (void**)&sCache )
                  != IDE_SUCCESS );

        // X Latch ȹ��
        IDE_TEST( sCache->latch.lockWrite (
                            NULL, /* idvSQL * */
                            NULL /* idvWeArgs* */ )
                  != IDE_SUCCESS );
        sStage = 1;

        sCache->isValid = ID_FALSE;

        sStage = 0;
        IDE_TEST( sCache->latch.unlock()
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch( sStage )
    {
        case 1:
            (void) sCache->latch.unlock();
    }

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qdnTrigger::copyRowFromTableRow( qcTemplate       * aTemplate,
                                        qcmTableInfo     * aTableInfo,
                                        qsVariables      * aVariable,
                                        smiTableCursor   * aTableCursor,
                                        void             * aTableRow,
                                        scGRID             aGRID,
                                        qcmColumn        * aTableRowColumns,
                                        qcmTriggerRefType  aRefType )
{
/***********************************************************************
 *
 * Description : PROJ-1075 trigger�� tableRow�� psmȣȯ ������
 *               rowtype ������ ����.
 *
 * Implementation :
 *             (1) psmȣȯ rowtype�� �ش� column�� ���� �÷�����
 *                 ���� �÷��̹Ƿ� �����÷��� ��ȸ�ϸ�
 *                 �� �÷����� mtc::value�� �̿��Ͽ������͸� ������
 *             (2) trigger tableRowtype�� ���������� ��ȸ
 *             (3) memcpy�� actual size��ŭ ����.
 *
 ***********************************************************************/
#define IDE_FN "qdnTrigger::copyRowFromTableRow"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(IDE_FN));

    UShort           i;
    UShort           sTable;
    UInt             sSrcActualSize;
    mtcTuple       * sDstTuple;
    void           * sDstValue;
    void           * sSrcValue;
    mtcColumn      * sSrcColumn;
    mtcColumn      * sDstColumn;
    UInt             sColumnOrder;

    sTable = aVariable->variableTypeNode->node.table;

    sDstTuple = &aTemplate->tmplate.rows[sTable];

    // sDstTuple�� rowtype������ tuple�̹Ƿ�,
    // ���� row�� �÷� ������ 1�� ���� �Ѵ�.
    // �� ���� �÷��� rowtype���� ��ü�̱� �����̴�.
    for ( i = 0; i < sDstTuple->columnCount - 1; i++ )
    {
        IDE_ASSERT( aTableRowColumns != NULL );

        sSrcColumn = aTableRowColumns[i].basicInfo;
        sDstColumn = &sDstTuple->columns[i+1];

        sSrcValue = (void*)mtc::value( sSrcColumn,
                                       aTableRow,
                                       MTD_OFFSET_USE );
        sDstValue = (void*)mtc::value( sDstColumn,
                                       sDstTuple->row,
                                       MTD_OFFSET_USE );

        // PROJ-2002 Column Security
        if ( (sSrcColumn->module->flag & MTD_ENCRYPT_TYPE_MASK)
             == MTD_ENCRYPT_TYPE_TRUE )
        {
            // ���� Ÿ���� ����ϴ� ���̺��� trigger �����
            // referencing row�� Ÿ���� ���� Ÿ���̹Ƿ�
            // decrypt�� �ʿ��ϴ�.

            sColumnOrder = sSrcColumn->column.id & SMI_COLUMN_ID_MASK;

            // decrypt & copy
            IDE_TEST( qcsModule::decryptColumn(
                          aTemplate->stmt,
                          aTableInfo,
                          sColumnOrder,
                          sSrcColumn,   // encrypt column
                          sSrcValue,
                          sDstColumn,   // plain column
                          sDstValue )
                      != IDE_SUCCESS );
        }
        else
        {
            /* PROJ-1530 PSM/Trigger���� LOB ����Ÿ Ÿ�� ���� */
            if ( (sSrcColumn->module->id == MTD_BLOB_LOCATOR_ID) ||
                 (sSrcColumn->module->id == MTD_CLOB_LOCATOR_ID) ||
                 (sSrcColumn->module->id == MTD_BLOB_ID) ||
                 (sSrcColumn->module->id == MTD_CLOB_ID) )
            {
                IDE_TEST( convertXlobToXlobValue( & aTemplate->tmplate,
                                                  aTableCursor,
                                                  aTableRow,
                                                  aGRID,
                                                  sSrcColumn,
                                                  sSrcValue,
                                                  sDstColumn,
                                                  sDstValue,
                                                  aRefType )
                          != IDE_SUCCESS );
            }
            else
            {
                // ���ռ� �˻�. column����� �����ؾ� �ȴ�.
                IDE_DASSERT( sSrcColumn->column.size == sDstColumn->column.size );

                sSrcActualSize = sSrcColumn->module->actualSize(
                    sSrcColumn,
                    sSrcValue );

                idlOS::memcpy( sDstValue, sSrcValue, sSrcActualSize );
            }
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qdnTrigger::copyRowFromValueList( qcTemplate   * aTemplate,
                                         qcmTableInfo * aTableInfo,
                                         qsVariables  * aVariable,
                                         void         * aValueList,
                                         qcmColumn    * aTableRowColumns )
{
/***********************************************************************
 *
 * Description : To fix BUG-12622
 *               Row-level before insert/update trigger ���� ��
 *               new row�� �����ϱ� ���� ȣ���ϴ� �Լ��̴�.
 *               dml������ value list�� rowtype������ ����
 *
 * Implementation :
 *
 ***********************************************************************/

    UShort      i;
    UShort      sTable;
    UInt        sSrcActualSize;
    mtcTuple  * sDstTuple;
    void      * sDstValue;
    void      * sSrcValue;
    qcmColumn * sSrcQcmColumn;
    mtcColumn * sSrcColumn;
    mtcColumn * sDstColumn;
    smiValue  * sValueList;
    UInt        sColumnOrder;

    sTable = aVariable->variableTypeNode->node.table;

    sDstTuple = &aTemplate->tmplate.rows[sTable];

    sValueList = (smiValue*)aValueList;

    sSrcQcmColumn = aTableRowColumns;

    // sDstTuple�� rowtype������ tuple�̰�,
    // �� ���� �÷��� rowtype���� ��ü�̹Ƿ� index�� 1���� �����Ѵ�.
    // sSrcColumn�� table�� column ������� ���ĵǾ� �ִ�.
    for ( i = 1; i < sDstTuple->columnCount; i++ )
    {
        if ( sSrcQcmColumn == NULL )
        {
            break;
        }
        else
        {
            sSrcColumn = sSrcQcmColumn->basicInfo;
            sDstColumn = &sDstTuple->columns[i];
        }

        if ( sSrcColumn->column.id !=
             sDstColumn->column.id )
        {
            continue;
        }
        else
        {
            // Nothing to do.
        }

        // PROJ-2002 Column Security
        if ( (sSrcColumn->module->flag & MTD_ENCRYPT_TYPE_MASK)
             == MTD_ENCRYPT_TYPE_TRUE )
        {
            // ���� Ÿ���� ����ϴ� ���̺��� trigger �����
            // referencing row�� Ÿ���� ���� Ÿ���̹Ƿ�
            // decrypt�� �ʿ��ϴ�.

            // variable null�� ������ �����.
            if( sValueList->length == 0 )
            {
                sSrcValue = sSrcColumn->module->staticNull;
            }
            else
            {
                sSrcValue = (void*)sValueList->value;
            }

            sDstValue = (void*)mtc::value( sDstColumn,
                                           sDstTuple->row,
                                           MTD_OFFSET_USE );

            // ���� Ÿ���� MTD_DATA_STORE_MTDVALUE_TRUE �̹Ƿ�
            // memory�� disk�� �������� �ʴ´�.

            sColumnOrder = sSrcColumn->column.id & SMI_COLUMN_ID_MASK;

            // decrypt & copy
            IDE_TEST( qcsModule::decryptColumn(
                          aTemplate->stmt,
                          aTableInfo,
                          sColumnOrder,
                          sSrcColumn,   // encrypt column
                          sSrcValue,
                          sDstColumn,   // plain column
                          sDstValue )
                      != IDE_SUCCESS );
        }
        else
        {
            /* PROJ-1530 PSM/Trigger���� LOB ����Ÿ Ÿ�� ���� */
            IDE_TEST_RAISE( (sSrcColumn->module->id == MTD_BLOB_LOCATOR_ID) ||
                            (sSrcColumn->module->id == MTD_CLOB_LOCATOR_ID) ||
                            (sSrcColumn->module->id == MTD_BLOB_ID) ||
                            (sSrcColumn->module->id == MTD_CLOB_ID),
                            ERR_NOT_SUPPORTED_DATATYPE );

            // ���ռ� �˻�. column����� �����ؾ� �ȴ�.
            IDE_DASSERT( sSrcColumn->column.size == sDstColumn->column.size );

            if( ( sSrcColumn->column.flag & SMI_COLUMN_STORAGE_MASK )
                == SMI_COLUMN_STORAGE_MEMORY )
            {
                //---------------------------
                // MEMORY COLUMN
                //---------------------------

                // variable null�� ������ �����.
                if( sValueList->length == 0 )
                {
                    sSrcValue = sSrcColumn->module->staticNull;
                }
                else
                {
                    sSrcValue = (void*)sValueList->value;
                }

                sDstValue = (void*)mtc::value( sDstColumn,
                                               sDstTuple->row,
                                               MTD_OFFSET_USE );

                sSrcActualSize = sSrcColumn->module->actualSize(
                    sSrcColumn,
                    sSrcValue );

                idlOS::memcpy( sDstValue, sSrcValue, sSrcActualSize );
            }
            else
            {
                //---------------------------
                // DISK COLUMN
                //---------------------------

                IDE_ASSERT(sDstColumn->module->storedValue2MtdValue[MTD_COLUMN_COPY_FUNC_NORMAL](
                    sDstColumn->column.size,
                    (UChar*)sDstTuple->row + sDstColumn->column.offset,
                    0,
                    sValueList->length,
                    sValueList->value ) == IDE_SUCCESS);
            }
        }

        sSrcQcmColumn = sSrcQcmColumn->next;
        sValueList++;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NOT_SUPPORTED_DATATYPE );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QSV_PROC_NOT_SUPPORTED_DATATYPE_SQLTEXT,
                                  "LOB columns are not supported." ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdnTrigger::copyValueListFromRow( qcTemplate   * aTemplate,
                                         iduMemory    * aNewValueMem,
                                         qcmTableInfo * aTableInfo,
                                         qsVariables  * aVariable,
                                         void         * aValueList,
                                         qcmColumn    * aTableRowColumns )
{
/***********************************************************************
 *
 * Description : To fix BUG-12622
 *               Row-level before insert/update trigger ���� ��
 *               new row�� value list�� �����ϱ� ���� ȣ���ϴ� �Լ��̴�.
 *               row�κ��� value list�� �����.
 *               �ٲ� �÷�(l-value �Ӽ��� column)�� �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    UShort         i;
    UShort         sTable;
    UInt           sSrcActualSize;
    mtcTuple     * sSrcTuple;
    void         * sSrcValue;
    void         * sDstValue;
    UInt           sSrcStoringSize;
    void         * sCopiedSrcValue;
    qcmColumn    * sDstQcmColumn;
    mtcColumn    * sSrcColumn;
    mtcColumn    * sDstColumn;
    smiValue     * sValueList;
    qtcNode      * sNode;
    UInt           sColumnOrder;
    UInt           sStoringSize = 0;
    void         * sStoringValue = NULL;

    sTable = aVariable->variableTypeNode->node.table;

    sSrcTuple = &aTemplate->tmplate.rows[sTable];

    sValueList = (smiValue*)aValueList;

    IDE_ERROR( aTableRowColumns != NULL );

    sDstQcmColumn = aTableRowColumns;

    // sSrcTuple�� rowtype������ tuple�̰�,
    // �� ���� �÷��� rowtype���� ��ü�̹Ƿ� index�� 1���� �����Ѵ�.
    // sDstColumn�� table�� column ������� ���ĵǾ� �ִ�.
    for ( i = 1,
             sNode = (qtcNode*)aVariable->variableTypeNode->node.arguments;
          (i < sSrcTuple->columnCount) || (sNode != NULL);
          i++,
             sNode = (qtcNode*)sNode->node.next )
    {
        if ( sDstQcmColumn == NULL )
        {
            break;
        }
        else
        {
            sDstColumn = sDstQcmColumn->basicInfo;
            sSrcColumn = &sSrcTuple->columns[i];
        }

        if ( sDstColumn->column.id !=
             sSrcColumn->column.id )
        {
            continue;
        }
        else
        {
            // Nothing to do.
        }

        // QTC_NODE_LVALUE_ENABLE flag�� set�Ǿ��ִ� ���
        // ����� �÷����� �� �� ����.
        if ( ( sNode->lflag & QTC_NODE_LVALUE_MASK ) ==
             QTC_NODE_LVALUE_ENABLE )
        {
            sSrcValue = (void*)mtc::value( sSrcColumn,
                                           sSrcTuple->row,
                                           MTD_OFFSET_USE );

            // PROJ-2002 Column Security
            if ( (sDstColumn->module->flag & MTD_ENCRYPT_TYPE_MASK)
                 == MTD_ENCRYPT_TYPE_TRUE )
            {
                // ���� Ÿ���� ����ϴ� ���̺��� trigger �����
                // referencing row�� Ÿ���� ���� Ÿ���̹Ƿ�
                // encrypt�� �ʿ��ϴ�.

                sColumnOrder = sDstColumn->column.id & SMI_COLUMN_ID_MASK;

                IDU_LIMITPOINT("qdnTrigger::copyValueListFromRow::malloc1");
                IDE_TEST( aNewValueMem->alloc(
                              sDstColumn->column.size,
                              (void**)&sDstValue )
                          != IDE_SUCCESS );

                // encrypt & copy
                IDE_TEST( qcsModule::encryptColumn(
                              aTemplate->stmt,
                              aTableInfo,
                              sColumnOrder,
                              sSrcColumn,   // plain column
                              sSrcValue,
                              sDstColumn,   // encrypt column
                              sDstValue )
                          != IDE_SUCCESS );

                IDE_TEST( qdbCommon::mtdValue2StoringValue( sDstColumn,
                                                            sDstColumn,
                                                            sDstValue,
                                                            &sStoringValue )
                          != IDE_SUCCESS );
                sValueList->value = sStoringValue;

                IDE_TEST( qdbCommon::storingSize( sDstColumn,
                                                  sDstColumn,
                                                  sDstValue,
                                                  &sStoringSize )
                          != IDE_SUCCESS );
                sValueList->length = sStoringSize;
            }
            else
            {
                sSrcActualSize = sSrcColumn->module->actualSize(
                    sSrcColumn,
                    sSrcValue );

                IDE_TEST( qdbCommon::storingSize( sSrcColumn,
                                                  sSrcColumn,
                                                  sSrcValue,
                                                  &sStoringSize )
                          != IDE_SUCCESS );
                sSrcStoringSize = sStoringSize;

                if( sSrcStoringSize == 0 )
                {
                    // variable, lob, disk column�� ���� ó��
                    sValueList->value = NULL;
                    sValueList->length = 0;
                }
                else
                {
                    // PROJ-1705
                    // QP���� smiValue->value��
                    // mtdDataType value�� source�� �ؼ�
                    // qdbCommon::mtdValue2StoringValue()�Լ��� ����ؼ� �����Ѵ�.
                    // �̷��� ������ smiValue->value��
                    // qdbCommon::storingValue2mtdValue()�Լ��� ����ؼ�
                    // �ٽ� mtdDataType value�� �����ϴ� ��찡 �ֱ⶧���̴�.
                    IDU_LIMITPOINT("qdnTrigger::copyValueListFromRow::malloc2");
                    IDE_TEST( aNewValueMem->alloc(
                                  sSrcActualSize,
                                  (void**)&sCopiedSrcValue )
                              != IDE_SUCCESS );

                    idlOS::memcpy( (void*)sCopiedSrcValue,
                                   sSrcValue,
                                   sSrcActualSize );

                    IDE_TEST( qdbCommon::mtdValue2StoringValue( sSrcColumn,
                                  sSrcColumn,
                                  sCopiedSrcValue,
                                  &sStoringValue)
                              != IDE_SUCCESS );
                    sValueList->value = sStoringValue;

                    sValueList->length = sSrcStoringSize;
                }
            }
        }
        else
        {
            // Nothing to do.
        }

        sDstQcmColumn = sDstQcmColumn->next;
        sValueList++;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qdnTrigger::allocProcInfo( qcStatement * aStatement,
                           UInt          aTriggerUserID )
{
    qdnCreateTriggerParseTree * sParseTree;

    sParseTree = (qdnCreateTriggerParseTree *) aStatement->myPlan->parseTree;

    idlOS::memset( &sParseTree->procInfo,
                   0,
                   ID_SIZEOF(qsxProcInfo) );

    sParseTree->procInfo.qmsMem  = QC_QMP_MEM(aStatement);
    sParseTree->procInfo.tmplate = QC_SHARED_TMPLATE(aStatement);

    // BUG-24408
    // trigger�� �����ڷ� body�� compile�ؾ� �Ѵ�.
    sParseTree->actionBody.userID = aTriggerUserID;

    sParseTree->actionBody.procInfo = &sParseTree->procInfo;

    aStatement->spvEnv->createProc = &sParseTree->actionBody;

    // validation �ÿ� �����Ǳ� ������ �ӽ÷� �����Ѵ�.
    aStatement->spvEnv->createProc->stmtText = aStatement->myPlan->stmtText;
    aStatement->spvEnv->createProc->stmtTextLen = aStatement->myPlan->stmtTextLen;

    return IDE_SUCCESS;
}

IDE_RC
qdnTrigger::getTriggerSCN( smOID    aTriggerOID,
                           smSCN  * aTriggerSCN)
{
    void   * sTableHandle;

    sTableHandle = (void *)smiGetTable( aTriggerOID );
    *aTriggerSCN = smiGetRowSCN(sTableHandle);

    return IDE_SUCCESS;
}

IDE_RC
qdnTrigger::executeRenameTable( qcStatement  * aStatement,
                                qcmTableInfo * aTableInfo )
{
/***********************************************************************
 *
 * Description : table �̸��� ����Ǿ��� �� ����� �̸��� table��
 *               �ùٸ��� ������ �� �ֵ��� trigger������ �����Ѵ�.
 *
 * Implementation :
 *   1. ���� trigger�� parsing�Ͽ� ����� �̸��� table�� �����ϵ���
 *      trigger ������ �����Ѵ�.
 *   2. ����� trigger ������ object info�� ����
 *   3. ���ο� trigger�� cache�� ����
 *   4. SYS_TRIGGER_STRINGS_�� ���� trigger ���� �� �� trigger ���
 *   5. ���� trigger�� cache�� ����
 *
 ***********************************************************************/

    qdnCreateTriggerParseTree * sParseTree;
    qdnTriggerCache          ** sOldCaches = NULL;
    qdnTriggerCache          ** sNewCaches = NULL;
    SChar                     * sSqlStmtBuffer;
    SChar                     * sTriggerStmtBuffer;
    UInt                        sTriggerStmtBufferSize;
    vSLong                      sRowCnt;
    UInt                        sSrcOffset = 0;
    UInt                        sDstOffset = 0;
    UInt                        i;

    // SYS_TRIGGER_STRINGS_ �� ���� ���� ����
    SChar         sSubString[QCM_TRIGGER_SUBSTRING_LEN * 2 + 2];
    SInt          sCurrPos;
    SInt          sCurrLen;
    SInt          sSeqNo;
    SInt          sSubStringCnt;
    // BUG-42992
    UInt          sNewTableNameLen = 0;
    UInt          sTriggerStmtBufAllocSize = 0;

    if( aTableInfo->triggerCount > 0 )
    {
        IDE_TEST( STRUCT_ALLOC_WITH_SIZE( aStatement->qmxMem,
                                          SChar,
                                          QD_MAX_SQL_LENGTH,
                                          &sSqlStmtBuffer)
                  != IDE_SUCCESS );

        IDE_TEST( STRUCT_ALLOC_WITH_SIZE( aStatement->qmxMem,
                                          SChar,
                                          QD_MAX_SQL_LENGTH,
                                          &sTriggerStmtBuffer)
                  != IDE_SUCCESS );

        sTriggerStmtBufAllocSize = QD_MAX_SQL_LENGTH;

        IDE_TEST( STRUCT_CRALLOC_WITH_SIZE( aStatement->qmxMem,
                                            qdnTriggerCache*,
                                            ID_SIZEOF(qdnTriggerCache*) * aTableInfo->triggerCount,
                                            &sOldCaches)
                  != IDE_SUCCESS );

        IDE_TEST( STRUCT_CRALLOC_WITH_SIZE( aStatement->qmxMem,
                                            qdnTriggerCache*,
                                            ID_SIZEOF(qdnTriggerCache*) * aTableInfo->triggerCount,
                                            &sNewCaches)
                  != IDE_SUCCESS );

        IDE_DASSERT(sOldCaches[0] == NULL);
        IDE_DASSERT(sNewCaches[0] == NULL);

        for ( i = 0;  i < aTableInfo->triggerCount; i++ )
        {
            IDE_TEST( smiObject::getObjectTempInfo( aTableInfo->triggerInfo[i].triggerHandle,
                                                    (void**)&sOldCaches[i] )
                      != IDE_SUCCESS );

            // 1. ���� trigger�� parsing�Ͽ� ����� �̸��� table�� �����ϵ��� trigger ������ �����Ѵ�.

            sOldCaches[i]->triggerStatement.myPlan->stmtTextLen = sOldCaches[i]->stmtTextLen;
            sOldCaches[i]->triggerStatement.myPlan->stmtText = sOldCaches[i]->stmtText;

            // parsing
            IDE_TEST( qcpManager::parseIt( &sOldCaches[i]->triggerStatement ) != IDE_SUCCESS );

            sParseTree = (qdnCreateTriggerParseTree *)sOldCaches[i]->triggerStatement.myPlan->parseTree;

            sNewTableNameLen = idlOS::strlen(aTableInfo->name);

            // BUG-42992
            if ( sOldCaches[i]->stmtTextLen + sNewTableNameLen + 1 > sTriggerStmtBufAllocSize )
            {
                // Ʈ���� ���� �� ���� + �����ϴ� ���̺� �̸��� ���� + null
                IDE_TEST( STRUCT_ALLOC_WITH_SIZE( aStatement->qmxMem,
                                                  SChar,
                                                  sOldCaches[i]->stmtTextLen + sNewTableNameLen + 1,
                                                  &sTriggerStmtBuffer)
                          != IDE_SUCCESS );

                sTriggerStmtBufAllocSize = sOldCaches[i]->stmtTextLen + sNewTableNameLen + 1;
            }
            else
            {
                // Nothing to do.
            }

            // trigger statement�� table �̸� ������ ����
            idlOS::memcpy(sTriggerStmtBuffer, sOldCaches[i]->stmtText, sParseTree->tableNamePos.offset);
            sDstOffset = sSrcOffset = sParseTree->tableNamePos.offset;
            // ����� table �̸� ����
            idlOS::strncpy(sTriggerStmtBuffer + sDstOffset, aTableInfo->name, QC_MAX_OBJECT_NAME_LEN);
            sDstOffset += sNewTableNameLen;
            sSrcOffset += sParseTree->tableNamePos.size;
            // table �̸� �� �κ� ����
            idlOS::memcpy(sTriggerStmtBuffer + sDstOffset,
                          sOldCaches[i]->stmtText + sSrcOffset,
                          sOldCaches[i]->stmtTextLen - sSrcOffset);
            sDstOffset += sOldCaches[i]->stmtTextLen - sSrcOffset;

            sTriggerStmtBufferSize = sDstOffset;

            // 2. ����� trigger ������ object info�� ����
            IDE_TEST( smiObject::setObjectInfo( QC_SMI_STMT( aStatement ),
                                                aTableInfo->triggerInfo[i].triggerHandle,
                                                sTriggerStmtBuffer,
                                                sTriggerStmtBufferSize )
                  != IDE_SUCCESS );

            // 3. ���ο� trigger�� cache�� ����
            IDE_TEST( allocTriggerCache( aTableInfo->triggerInfo[i].triggerHandle,
                                         aTableInfo->triggerInfo[i].triggerOID,
                                         &sNewCaches[i] )
                  != IDE_SUCCESS );

            // 4. SYS_TRIGGER_STRINGS_�� ���� trigger ���� �� �� trigger ���

            idlOS::snprintf( sSqlStmtBuffer, QD_MAX_SQL_LENGTH,
                             "DELETE SYS_TRIGGER_STRINGS_ WHERE TRIGGER_OID="QCM_SQL_BIGINT_FMT,
                             QCM_OID_TO_BIGINT( aTableInfo->triggerInfo[i].triggerOID ));

            IDE_TEST( qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                         sSqlStmtBuffer,
                                         & sRowCnt )
                      != IDE_SUCCESS );

            sSubStringCnt = (sTriggerStmtBufferSize / QCM_TRIGGER_SUBSTRING_LEN ) +
                            (((sTriggerStmtBufferSize % QCM_TRIGGER_SUBSTRING_LEN) == 0) ? 0 : 1);

            // qcmTrigger::addMetaInfo���� �Ϻ� �ڵ� ����
            for ( sSeqNo = 0, sCurrLen = 0, sCurrPos = 0;
                  sSeqNo < sSubStringCnt;
                  sSeqNo++ )
            {
                if ( (sTriggerStmtBufferSize - sCurrPos) >=
                     QCM_TRIGGER_SUBSTRING_LEN )
                {
                    sCurrLen = QCM_TRIGGER_SUBSTRING_LEN;
                }
                else
                {
                    sCurrLen = sTriggerStmtBufferSize - sCurrPos;
                }

                // ['] �� ���� ���ڸ� ó���ϱ� ���Ͽ� [\'] �� ����
                // String���� ��ȯ�Ͽ� �����Ѵ�.
                IDE_TEST(
                    qcmProc::prsCopyStrDupAppo ( sSubString,
                                                 sTriggerStmtBuffer + sCurrPos,
                                                 sCurrLen )
                    != IDE_SUCCESS );

                sCurrPos += sCurrLen;

                idlOS::snprintf( sSqlStmtBuffer, QD_MAX_SQL_LENGTH,
                                 "INSERT INTO SYS_TRIGGER_STRINGS_ VALUES ("
                                 QCM_SQL_UINT32_FMT", "      // 0. TABLE_ID
                                 QCM_SQL_BIGINT_FMT", "      // 1. TRIGGER_OID
                                 QCM_SQL_INT32_FMT", "       // 2. SEQ_NO
                                 QCM_SQL_CHAR_FMT") ",       // 3. SUBSTRING
                                 aTableInfo->tableID,               // 0.
                                 QCM_OID_TO_BIGINT( aTableInfo->triggerInfo[i].triggerOID ),  // 1.
                                 sSeqNo,                            // 2.
                                 sSubString );                      // 3.

                IDE_TEST( qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                             sSqlStmtBuffer,
                                             & sRowCnt )
                          != IDE_SUCCESS );

                IDE_DASSERT( sRowCnt == 1 );
            }

            /* BUG-45516 RENAME DDL���� Trigger String�� ������ ��, SYS_TRIGGERS_�� SUBSTRING_CNT�� STRING_LENGTH�� �������� �ʽ��ϴ�.
             *  - SYS_TRIGGERS_���� �ش��ϴ� TRIGGER_OID�� �˻��ϰ�, SUBSTRING_CNT, STRING_LENGTH�� �����Ѵ�.
             */
            idlOS::snprintf( sSqlStmtBuffer, QD_MAX_SQL_LENGTH,
                             "UPDATE SYS_TRIGGERS_ SET"
                             " SUBSTRING_CNT = "QCM_SQL_UINT32_FMT","
                             " STRING_LENGTH = " QCM_SQL_UINT32_FMT" "
                             "WHERE TRIGGER_OID = "QCM_SQL_UINT32_FMT" ",
                             sSubStringCnt,
                             sTriggerStmtBufferSize,
                             QCM_OID_TO_BIGINT( aTableInfo->triggerInfo[i].triggerOID ) );

            IDE_TEST( qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                         sSqlStmtBuffer,
                                         & sRowCnt )
                      != IDE_SUCCESS );

            IDE_DASSERT( sRowCnt == 1 );
        }


        // 5. ���� trigger�� cache�� ����
        for ( i = 0;  i < aTableInfo->triggerCount; i++ )
        {
            IDE_TEST(freeTriggerCache(sOldCaches[i]) != IDE_SUCCESS);
        }
    }
    else
    {
        // Nothing to do.
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    // error �߻� �� trigger cache ����
    for ( i = 0;  i < aTableInfo->triggerCount; i++ )
    {
        if( sOldCaches != NULL )
        {
            if( sOldCaches[i] != NULL )
            {
                smiObject::setObjectTempInfo(
                    aTableInfo->triggerInfo[i].triggerHandle,
                    sOldCaches[i]);
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

        if( sNewCaches != NULL )
        {
            if( sNewCaches[i] != NULL )
            {
                freeTriggerCache(sNewCaches[i]);
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

    return IDE_FAILURE;
}

IDE_RC qdnTrigger::executeCopyTable( qcStatement       * aStatement,
                                     qcmTableInfo      * aSourceTableInfo,
                                     qcmTableInfo      * aTargetTableInfo,
                                     qcNamePosition      aNamesPrefix,
                                     qdnTriggerCache *** aTargetTriggerCache )
{
/***********************************************************************
 * Description :
 *      Target Table�� Trigger�� �����Ѵ�.
 *          - Source Table�� Trigger ������ ����Ͽ�, Target Table�� Trigger�� �����Ѵ�. (SM)
 *              - TRIGGER_NAME�� ����ڰ� ������ Prefix�� �ٿ��� TRIGGER_NAME�� �����Ѵ�.
 *              - Trigger Strings�� ������ TRIGGER_NAME�� Target Table Name�� �����Ѵ�.
 *          - Meta Table�� Target Table�� Trigger ������ �߰��Ѵ�. (Meta Table)
 *              - SYS_TRIGGERS_
 *                  - ������ TRIGGER_NAME�� ����Ѵ�.
 *                  - SM���� ���� Trigger OID�� SYS_TRIGGERS_�� �ʿ��ϴ�.
 *                  - ������ Trigger String���� SUBSTRING_CNT, STRING_LENGTH�� ������ �Ѵ�.
 *                  - LAST_DDL_TIME�� �ʱ�ȭ�Ѵ�. (SYSDATE)
 *              - SYS_TRIGGER_STRINGS_
 *                  - ������ Trigger Strings�� �߶� �ִ´�.
 *              - SYS_TRIGGER_UPDATE_COLUMNS_
 *                  - ���ο� TABLE_ID�� �̿��Ͽ� COLUMN_ID�� ������ �Ѵ�.
 *              - SYS_TRIGGER_DML_TABLES_
 *
 * Implementation :
 *
 ***********************************************************************/

    qdnCreateTriggerParseTree  * sParseTree               = NULL;
    qdnTriggerCache            * sSrcCache                = NULL;
    qdnTriggerCache           ** sTarCache                = NULL;
    void                       * sTriggerHandle           = NULL;
    smOID                        sTriggerOID              = SMI_NULL_OID;
    SChar                      * sTriggerStmtBuffer       = NULL;
    UInt                         sTriggerStmtBufAllocSize = 0;
    UInt                         sTargetTableNameLen      = 0;
    UInt                         sTargetTriggerNameLen    = 0;
    UInt                         sSrcOffset               = 0;
    UInt                         sTarOffset               = 0;
    UInt                         i                        = 0;

    SChar                        sTargetTriggerName[QC_MAX_OBJECT_NAME_LEN + 1];
    qcStatement                  sStatement;
    idBool                       sStatementAlloced        = ID_FALSE;

    *aTargetTriggerCache = NULL;

    if ( aSourceTableInfo->triggerCount > 0 )
    {
        IDE_TEST( qcg::allocStatement( & sStatement,
                                       NULL,
                                       NULL,
                                       NULL )
                  != IDE_SUCCESS );
        sStatementAlloced = ID_TRUE;

        IDU_FIT_POINT( "qdnTrigger::executeCopyTable::STRUCT_ALLOC_WITH_SIZE::sTriggerStmtBuffer",
                       idERR_ABORT_InsufficientMemory );

        IDE_TEST( STRUCT_ALLOC_WITH_SIZE( QC_QMX_MEM( aStatement ),
                                          SChar,
                                          QD_MAX_SQL_LENGTH,
                                          & sTriggerStmtBuffer )
                  != IDE_SUCCESS );

        sTriggerStmtBufAllocSize = QD_MAX_SQL_LENGTH;

        IDU_FIT_POINT( "qdnTrigger::executeCopyTable::STRUCT_CRALLOC_WITH_SIZE::sTarCache",
                       idERR_ABORT_InsufficientMemory );

        IDE_TEST( STRUCT_CRALLOC_WITH_SIZE( QC_QMX_MEM( aStatement ),
                                            qdnTriggerCache *,
                                            ID_SIZEOF(qdnTriggerCache *) * aSourceTableInfo->triggerCount,
                                            & sTarCache )
                  != IDE_SUCCESS );

        for ( i = 0; i < aSourceTableInfo->triggerCount; i++ )
        {
            IDE_TEST_RAISE( ( idlOS::strlen( aSourceTableInfo->triggerInfo[i].triggerName ) + aNamesPrefix.size ) >
                            QC_MAX_OBJECT_NAME_LEN,
                            ERR_TOO_LONG_OBJECT_NAME );

            // TRIGGER_NAME�� ����ڰ� ������ Prefix�� �ٿ��� TRIGGER_NAME�� �����Ѵ�.
            QC_STR_COPY( sTargetTriggerName, aNamesPrefix );
            idlOS::strncat( sTargetTriggerName, aSourceTableInfo->triggerInfo[i].triggerName, QC_MAX_OBJECT_NAME_LEN );

            sTargetTriggerNameLen = idlOS::strlen( sTargetTriggerName );

            IDE_TEST( smiObject::getObjectTempInfo( aSourceTableInfo->triggerInfo[i].triggerHandle,
                                                    (void **) & sSrcCache )
                      != IDE_SUCCESS );

            // Parsing Trigger String
            qcg::setSmiStmt( & sStatement, QC_SMI_STMT( aStatement ) );
            qsxEnv::initialize( sStatement.spxEnv, NULL );

            sStatement.myPlan->stmtText            = sSrcCache->stmtText;
            sStatement.myPlan->stmtTextLen         = sSrcCache->stmtTextLen;
            sStatement.myPlan->encryptedText       = NULL;  /* PROJ-2550 PSM Encryption */
            sStatement.myPlan->encryptedTextLen    = 0;     /* PROJ-2550 PSM Encryption */
            sStatement.myPlan->parseTree           = NULL;
            sStatement.myPlan->plan                = NULL;
            sStatement.myPlan->graph               = NULL;
            sStatement.myPlan->scanDecisionFactors = NULL;

            IDE_TEST( qcpManager::parseIt( & sStatement ) != IDE_SUCCESS );
            sParseTree = (qdnCreateTriggerParseTree *)sStatement.myPlan->parseTree;

            sTargetTableNameLen = idlOS::strlen( aTargetTableInfo->name );

            if ( ( sSrcCache->stmtTextLen + sTargetTriggerNameLen + sTargetTableNameLen + 1 ) >
                 sTriggerStmtBufAllocSize )
            {
                // ���� �� Trigger ���� + ���� �� Trigger Name ���� + ���� �� Table Name ���� + '\0'
                sTriggerStmtBufAllocSize = sSrcCache->stmtTextLen + sTargetTriggerNameLen + sTargetTableNameLen + 1;

                IDU_FIT_POINT( "qdnTrigger::executeCopyTable::STRUCT_ALLOC_WITH_SIZE::sTriggerStmtBuffer-2",
                               idERR_ABORT_InsufficientMemory );

                IDE_TEST( STRUCT_ALLOC_WITH_SIZE( QC_QMX_MEM( aStatement ),
                                                  SChar,
                                                  sTriggerStmtBufAllocSize,
                                                  & sTriggerStmtBuffer )
                          != IDE_SUCCESS );
            }
            else
            {
                /* Nothing to do */
            }

            // Trigger Statement�� Trigger Name ������ ����
            idlOS::memcpy( sTriggerStmtBuffer,
                           sSrcCache->stmtText,
                           sParseTree->triggerNamePos.offset );
            sTarOffset = sSrcOffset = sParseTree->triggerNamePos.offset;

            // ���� �� Trigger Name ����
            idlOS::strncpy( sTriggerStmtBuffer + sTarOffset,
                            sTargetTriggerName,
                            QC_MAX_OBJECT_NAME_LEN );
            sTarOffset += sTargetTriggerNameLen;
            sSrcOffset += sParseTree->triggerNamePos.size;

            // Trigger Statement�� Table Name ������ ����
            idlOS::memcpy( sTriggerStmtBuffer + sTarOffset,
                           sSrcCache->stmtText + sSrcOffset,
                           sParseTree->tableNamePos.offset - sSrcOffset );
            sTarOffset += sParseTree->tableNamePos.offset - sSrcOffset;
            sSrcOffset = sParseTree->tableNamePos.offset;

            // ���� �� Table Name ����
            idlOS::strncpy( sTriggerStmtBuffer + sTarOffset,
                            aTargetTableInfo->name,
                            QC_MAX_OBJECT_NAME_LEN );
            sTarOffset += sTargetTableNameLen;
            sSrcOffset += sParseTree->tableNamePos.size;

            // Table Name �� �κ� ����
            idlOS::memcpy( sTriggerStmtBuffer + sTarOffset,
                           sSrcCache->stmtText + sSrcOffset,
                           sSrcCache->stmtTextLen - sSrcOffset );
            sTarOffset += sSrcCache->stmtTextLen - sSrcOffset;

            sTriggerStmtBuffer[sTarOffset] = '\0';

            /* Source Table�� Trigger ������ ����Ͽ�, Target Table�� Trigger�� �����Ѵ�. (SM) */
            sStatement.myPlan->stmtText    = sTriggerStmtBuffer;
            sStatement.myPlan->stmtTextLen = sTarOffset;

            IDE_TEST( createTriggerObject( & sStatement,
                                           & sTriggerHandle )
                      != IDE_SUCCESS );

            sTriggerOID = smiGetTableId( sTriggerHandle ) ;

            IDE_TEST( allocTriggerCache( sTriggerHandle,
                                         sTriggerOID,
                                         & sTarCache[i] )
                      != IDE_SUCCESS );

            /* Meta Table�� Target Table�� Trigger ������ �߰��Ѵ�. (Meta Table) */
            IDE_TEST( qcmTrigger::copyTriggerMetaInfo( aStatement,
                                                       aTargetTableInfo->tableID,
                                                       sTriggerOID,
                                                       sTargetTriggerName,
                                                       aSourceTableInfo->tableID,
                                                       aSourceTableInfo->triggerInfo[i].triggerOID,
                                                       sTriggerStmtBuffer,
                                                       sTarOffset )
                      != IDE_SUCCESS );
        }

        sStatementAlloced = ID_FALSE;
        IDE_TEST( qcg::freeStatement( & sStatement ) != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    *aTargetTriggerCache = sTarCache;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_TOO_LONG_OBJECT_NAME )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDB_USING_TARGET_NAMES_PREFIX_IS_TOO_LONG ) );
    }
    IDE_EXCEPTION_END;

    if ( sStatementAlloced == ID_TRUE )
    {
        (void)qcg::freeStatement( & sStatement );
    }
    else
    {
        /* Nothing to do */
    }

    freeTriggerCacheArray( sTarCache,
                           aSourceTableInfo->triggerCount );

    return IDE_FAILURE;
}

IDE_RC qdnTrigger::executeSwapTable( qcStatement       * aStatement,
                                     qcmTableInfo      * aSourceTableInfo,
                                     qcmTableInfo      * aTargetTableInfo,
                                     qcNamePosition      aNamesPrefix,
                                     qdnTriggerCache *** aOldSourceTriggerCache,
                                     qdnTriggerCache *** aOldTargetTriggerCache,
                                     qdnTriggerCache *** aNewSourceTriggerCache,
                                     qdnTriggerCache *** aNewTargetTriggerCache )
{
/***********************************************************************
 * Description :
 *      ������ Trigger Name�� ��ȯ�� Table Name�� Trigger Strings�� �ݿ��ϰ� Trigger�� ������Ѵ�.
 *          - Prefix�� ������ ���, Source�� TRIGGER_NAME�� Prefix�� ���̰�,
 *            Target�� TRIGGER_NAME���� Prefix�� �����Ѵ�. (Meta Table)
 *              - SYS_TRIGGERS_
 *                  - TRIGGER_NAME�� �����Ѵ�.
 *                  - ������ Trigger String���� SUBSTRING_CNT, STRING_LENGTH�� ������ �Ѵ�.
 *                  - LAST_DDL_TIME�� �����Ѵ�. (SYSDATE)
 *          - Trigger Strings�� ������ Trigger Name�� ��ȯ�� Table Name�� �����Ѵ�. (SM, Meta Table, Meta Cache)
 *              - Trigger Object�� Trigger ���� ������ �����Ѵ�. (SM)
 *                  - Call : smiObject::setObjectInfo()
 *              - New Trigger Cache�� �����ϰ� SM�� ����Ѵ�. (Meta Cache)
 *                  - Call : qdnTrigger::allocTriggerCache()
 *              - Trigger Strings�� �����ϴ� Meta Table�� �����Ѵ�. (Meta Table)
 *                  - SYS_TRIGGER_STRINGS_
 *                      - DELETE & INSERT�� ó���Ѵ�.
 *          - Trigger�� ���۽�Ű�� Column �������� ���� ������ ����.
 *              - SYS_TRIGGER_UPDATE_COLUMNS_
 *          - �ٸ� Trigger�� Cycle Check�� ����ϴ� ������ �����Ѵ�. (Meta Table)
 *              - SYS_TRIGGER_DML_TABLES_
 *                  - DML_TABLE_ID = Table ID �̸�, (TABLE_ID�� �����ϰ�) DML_TABLE_ID�� Peer�� Table ID�� ��ü�Ѵ�.
 *          - ���� Call : qdnTrigger::executeRenameTable()
 *
 * Implementation :
 *
 ***********************************************************************/

    qdnTriggerCache            * sTriggerCache            = NULL;
    qdnTriggerCache           ** sOldSrcCache             = NULL;
    qdnTriggerCache           ** sOldTarCache             = NULL;
    qdnTriggerCache           ** sNewSrcCache             = NULL;
    qdnTriggerCache           ** sNewTarCache             = NULL;
    SChar                      * sTriggerStmtBuffer       = NULL;
    UInt                         sTriggerStmtBufAllocSize = 0;
    SChar                      * sPeerTableNameStr        = NULL;
    UInt                         sPeerTableNameLen        = 0;
    UInt                         sNewTriggerNameLen       = 0;
    UInt                         i                        = 0;

    SChar                        sNewTriggerName[QC_MAX_OBJECT_NAME_LEN + 1];

    *aOldSourceTriggerCache = NULL;
    *aOldTargetTriggerCache = NULL;
    *aNewSourceTriggerCache = NULL;
    *aNewTargetTriggerCache = NULL;

    IDU_FIT_POINT( "qdnTrigger::executeSwapTable::STRUCT_ALLOC_WITH_SIZE::sTriggerStmtBuffer",
                   idERR_ABORT_InsufficientMemory );

    IDE_TEST( STRUCT_ALLOC_WITH_SIZE( QC_QMX_MEM( aStatement ),
                                      SChar,
                                      QD_MAX_SQL_LENGTH,
                                      & sTriggerStmtBuffer )
              != IDE_SUCCESS );

    sTriggerStmtBufAllocSize = QD_MAX_SQL_LENGTH;

    if ( aSourceTableInfo->triggerCount > 0 )
    {
        IDU_FIT_POINT( "qdnTrigger::executeSwapTable::STRUCT_CRALLOC_WITH_SIZE::sOldSrcCache",
                       idERR_ABORT_InsufficientMemory );

        IDE_TEST( STRUCT_CRALLOC_WITH_SIZE( QC_QMX_MEM( aStatement ),
                                            qdnTriggerCache *,
                                            ID_SIZEOF(qdnTriggerCache *) * aSourceTableInfo->triggerCount,
                                            & sOldSrcCache )
                  != IDE_SUCCESS );

        IDU_FIT_POINT( "qdnTrigger::executeSwapTable::STRUCT_CRALLOC_WITH_SIZE::sNewSrcCache",
                       idERR_ABORT_InsufficientMemory );

        IDE_TEST( STRUCT_CRALLOC_WITH_SIZE( QC_QMX_MEM( aStatement ),
                                            qdnTriggerCache *,
                                            ID_SIZEOF(qdnTriggerCache *) * aSourceTableInfo->triggerCount,
                                            & sNewSrcCache )
                  != IDE_SUCCESS );

        sPeerTableNameStr = aTargetTableInfo->name;
        sPeerTableNameLen = idlOS::strlen( sPeerTableNameStr );

        for ( i = 0; i < aSourceTableInfo->triggerCount; i++ )
        {
            // TRIGGER_NAME�� ����ڰ� ������ Prefix�� �ٿ��� TRIGGER_NAME�� �����Ѵ�.
            if ( QC_IS_NULL_NAME( aNamesPrefix ) != ID_TRUE )
            {
                IDE_TEST_RAISE( ( idlOS::strlen( aSourceTableInfo->triggerInfo[i].triggerName ) + aNamesPrefix.size ) >
                                QC_MAX_OBJECT_NAME_LEN,
                                ERR_NAMES_PREFIX_IS_TOO_LONG );

                QC_STR_COPY( sNewTriggerName, aNamesPrefix );
                idlOS::strncat( sNewTriggerName, aSourceTableInfo->triggerInfo[i].triggerName, QC_MAX_OBJECT_NAME_LEN );
            }
            else
            {
                idlOS::strncpy( sNewTriggerName,
                                aSourceTableInfo->triggerInfo[i].triggerName,
                                QC_MAX_OBJECT_NAME_LEN + 1 );
            }

            sNewTriggerNameLen = idlOS::strlen( sNewTriggerName );

            IDE_TEST( smiObject::getObjectTempInfo( aSourceTableInfo->triggerInfo[i].triggerHandle,
                                                    (void **) & sTriggerCache )
                      != IDE_SUCCESS );
            sOldSrcCache[i] = sTriggerCache;

            if ( ( sTriggerCache->stmtTextLen + sNewTriggerNameLen + sPeerTableNameLen + 1 ) >
                 sTriggerStmtBufAllocSize )
            {
                // ���� �� Trigger ���� + ���� �� Trigger Name ���� + ���� �� Table Name ���� + '\0'
                sTriggerStmtBufAllocSize = sTriggerCache->stmtTextLen + sNewTriggerNameLen + sPeerTableNameLen + 1;

                IDU_FIT_POINT( "qdnTrigger::executeSwapTable::STRUCT_ALLOC_WITH_SIZE::sTriggerStmtBuffer-2",
                               idERR_ABORT_InsufficientMemory );

                IDE_TEST( STRUCT_ALLOC_WITH_SIZE( QC_QMX_MEM( aStatement ),
                                                  SChar,
                                                  sTriggerStmtBufAllocSize,
                                                  & sTriggerStmtBuffer )
                          != IDE_SUCCESS );
            }
            else
            {
                /* Nothing to do */
            }

            IDE_TEST( makeNewTriggerStringForSwap( aStatement,
                                                   sTriggerCache,
                                                   sTriggerStmtBuffer,
                                                   aSourceTableInfo->triggerInfo[i].triggerHandle,
                                                   aSourceTableInfo->triggerInfo[i].triggerOID,
                                                   aSourceTableInfo->tableID,
                                                   sNewTriggerName,
                                                   sNewTriggerNameLen,
                                                   sPeerTableNameStr,
                                                   sPeerTableNameLen,
                                                   & sNewSrcCache[i] )
                      != IDE_SUCCESS );
        }
    }
    else
    {
        /* Nothing to do */
    }

    if ( aTargetTableInfo->triggerCount > 0 )
    {
        IDU_FIT_POINT( "qdnTrigger::executeSwapTable::STRUCT_CRALLOC_WITH_SIZE::sOldTarCache",
                       idERR_ABORT_InsufficientMemory );

        IDE_TEST( STRUCT_CRALLOC_WITH_SIZE( QC_QMX_MEM( aStatement ),
                                            qdnTriggerCache *,
                                            ID_SIZEOF(qdnTriggerCache *) * aTargetTableInfo->triggerCount,
                                            & sOldTarCache )
                  != IDE_SUCCESS );

        IDU_FIT_POINT( "qdnTrigger::executeSwapTable::STRUCT_CRALLOC_WITH_SIZE::sNewTarCache",
                       idERR_ABORT_InsufficientMemory );

        IDE_TEST( STRUCT_CRALLOC_WITH_SIZE( QC_QMX_MEM( aStatement ),
                                            qdnTriggerCache *,
                                            ID_SIZEOF(qdnTriggerCache *) * aTargetTableInfo->triggerCount,
                                            & sNewTarCache )
                  != IDE_SUCCESS );

        sPeerTableNameStr = aSourceTableInfo->name;
        sPeerTableNameLen = idlOS::strlen( sPeerTableNameStr );

        for ( i = 0; i < aTargetTableInfo->triggerCount; i++ )
        {
            // TRIGGER_NAME���� ����ڰ� ������ Prefix�� ������ TRIGGER_NAME�� �����Ѵ�.
            if ( QC_IS_NULL_NAME( aNamesPrefix ) != ID_TRUE )
            {
                IDE_TEST_RAISE( idlOS::strlen( aTargetTableInfo->triggerInfo[i].triggerName ) <=
                                (UInt)aNamesPrefix.size,
                                ERR_NAMES_PREFIX_IS_TOO_LONG );

                IDE_TEST_RAISE( idlOS::strMatch( aTargetTableInfo->triggerInfo[i].triggerName,
                                                 aNamesPrefix.size,
                                                 aNamesPrefix.stmtText + aNamesPrefix.offset,
                                                 aNamesPrefix.size ) != 0,
                                ERR_NAMES_PREFIX_IS_TOO_LONG );

                idlOS::strncpy( sNewTriggerName,
                                aTargetTableInfo->triggerInfo[i].triggerName + aNamesPrefix.size,
                                QC_MAX_OBJECT_NAME_LEN + 1 );
            }
            else
            {
                idlOS::strncpy( sNewTriggerName,
                                aTargetTableInfo->triggerInfo[i].triggerName,
                                QC_MAX_OBJECT_NAME_LEN + 1 );
            }

            sNewTriggerNameLen = idlOS::strlen( sNewTriggerName );

            IDE_TEST( smiObject::getObjectTempInfo( aTargetTableInfo->triggerInfo[i].triggerHandle,
                                                    (void **) & sTriggerCache )
                      != IDE_SUCCESS );
            sOldTarCache[i] = sTriggerCache;

            if ( ( sTriggerCache->stmtTextLen + sNewTriggerNameLen + sPeerTableNameLen + 1 ) >
                 sTriggerStmtBufAllocSize )
            {
                // ���� �� Trigger ���� + ���� �� Trigger Name ���� + ���� �� Table Name ���� + '\0'
                sTriggerStmtBufAllocSize = sTriggerCache->stmtTextLen + sNewTriggerNameLen + sPeerTableNameLen + 1;

                IDU_FIT_POINT( "qdnTrigger::executeSwapTable::STRUCT_ALLOC_WITH_SIZE::sTriggerStmtBuffer-3",
                               idERR_ABORT_InsufficientMemory );

                IDE_TEST( STRUCT_ALLOC_WITH_SIZE( QC_QMX_MEM( aStatement ),
                                                  SChar,
                                                  sTriggerStmtBufAllocSize,
                                                  & sTriggerStmtBuffer )
                          != IDE_SUCCESS );
            }
            else
            {
                /* Nothing to do */
            }

            IDE_TEST( makeNewTriggerStringForSwap( aStatement,
                                                   sTriggerCache,
                                                   sTriggerStmtBuffer,
                                                   aTargetTableInfo->triggerInfo[i].triggerHandle,
                                                   aTargetTableInfo->triggerInfo[i].triggerOID,
                                                   aTargetTableInfo->tableID,
                                                   sNewTriggerName,
                                                   sNewTriggerNameLen,
                                                   sPeerTableNameStr,
                                                   sPeerTableNameLen,
                                                   & sNewTarCache[i] )
                      != IDE_SUCCESS );
        }
    }
    else
    {
        /* Nothing to do */
    }

    /* SYS_TRIGGERS_�� TRIGGER_NAME�� �����Ѵ�. */
    IDE_TEST( qcmTrigger::renameTriggerMetaInfoForSwap( aStatement,
                                                        aSourceTableInfo,
                                                        aTargetTableInfo,
                                                        aNamesPrefix )
              != IDE_SUCCESS );

    /* �ٸ� Trigger�� Cycle Check�� ����ϴ� ������ �����Ѵ�. (Meta Table) */
    IDE_TEST( qcmTrigger::swapTriggerDMLTablesMetaInfo( aStatement,
                                                        aTargetTableInfo->tableID,
                                                        aSourceTableInfo->tableID )
              != IDE_SUCCESS );

    *aOldSourceTriggerCache = sOldSrcCache;
    *aOldTargetTriggerCache = sOldTarCache;
    *aNewSourceTriggerCache = sNewTarCache;
    *aNewTargetTriggerCache = sNewSrcCache;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NAMES_PREFIX_IS_TOO_LONG )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDB_USING_TARGET_NAMES_PREFIX_IS_TOO_LONG ) );
    }
    IDE_EXCEPTION_END;

    freeTriggerCacheArray( sNewSrcCache,
                           aSourceTableInfo->triggerCount );

    freeTriggerCacheArray( sNewTarCache,
                           aTargetTableInfo->triggerCount );

    restoreTempInfo( sOldSrcCache,
                     aSourceTableInfo );

    restoreTempInfo( sOldTarCache,
                     aTargetTableInfo );

    return IDE_FAILURE;
}

IDE_RC qdnTrigger::makeNewTriggerStringForSwap( qcStatement      * aStatement,
                                                qdnTriggerCache  * aTriggerCache,
                                                SChar            * aTriggerStmtBuffer,
                                                void             * aTriggerHandle,
                                                smOID              aTriggerOID,
                                                UInt               aTableID,
                                                SChar            * aNewTriggerName,
                                                UInt               aNewTriggerNameLen,
                                                SChar            * aPeerTableNameStr,
                                                UInt               aPeerTableNameLen,
                                                qdnTriggerCache ** aNewTriggerCache )
{
    qdnCreateTriggerParseTree  * sParseTree               = NULL;
    UInt                         sOldOffset               = 0;
    UInt                         sNewOffset               = 0;

    qcStatement                  sStatement;
    idBool                       sStatementAlloced        = ID_FALSE;

    IDE_TEST( qcg::allocStatement( & sStatement,
                                   NULL,
                                   NULL,
                                   NULL )
              != IDE_SUCCESS );
    sStatementAlloced = ID_TRUE;

    // Parsing Trigger String
    qcg::setSmiStmt( & sStatement, QC_SMI_STMT( aStatement ) );
    qsxEnv::initialize( sStatement.spxEnv, NULL );

    sStatement.myPlan->stmtText            = aTriggerCache->stmtText;
    sStatement.myPlan->stmtTextLen         = aTriggerCache->stmtTextLen;
    sStatement.myPlan->encryptedText       = NULL;  /* PROJ-2550 PSM Encryption */
    sStatement.myPlan->encryptedTextLen    = 0;     /* PROJ-2550 PSM Encryption */
    sStatement.myPlan->parseTree           = NULL;
    sStatement.myPlan->plan                = NULL;
    sStatement.myPlan->graph               = NULL;
    sStatement.myPlan->scanDecisionFactors = NULL;

    IDE_TEST( qcpManager::parseIt( & sStatement ) != IDE_SUCCESS );
    sParseTree = (qdnCreateTriggerParseTree *)sStatement.myPlan->parseTree;

    // Trigger Statement�� Trigger Name ������ ����
    idlOS::memcpy( aTriggerStmtBuffer,
                   aTriggerCache->stmtText,
                   sParseTree->triggerNamePos.offset );
    sNewOffset = sOldOffset = sParseTree->triggerNamePos.offset;

    // ���� �� Trigger Name ����
    idlOS::strncpy( aTriggerStmtBuffer + sNewOffset,
                    aNewTriggerName,
                    QC_MAX_OBJECT_NAME_LEN );
    sNewOffset += aNewTriggerNameLen;
    sOldOffset += sParseTree->triggerNamePos.size;

    // Trigger Statement�� Table Name ������ ����
    idlOS::memcpy( aTriggerStmtBuffer + sNewOffset,
                   aTriggerCache->stmtText + sOldOffset,
                   sParseTree->tableNamePos.offset - sOldOffset );
    sNewOffset += sParseTree->tableNamePos.offset - sOldOffset;
    sOldOffset = sParseTree->tableNamePos.offset;

    // ���� �� Table Name ����
    idlOS::strncpy( aTriggerStmtBuffer + sNewOffset,
                    aPeerTableNameStr,
                    QC_MAX_OBJECT_NAME_LEN );
    sNewOffset += aPeerTableNameLen;
    sOldOffset += sParseTree->tableNamePos.size;

    // Table Name �� �κ� ����
    idlOS::memcpy( aTriggerStmtBuffer + sNewOffset,
                   aTriggerCache->stmtText + sOldOffset,
                   aTriggerCache->stmtTextLen - sOldOffset );
    sNewOffset += aTriggerCache->stmtTextLen - sOldOffset;

    aTriggerStmtBuffer[sNewOffset] = '\0';

    /* Trigger Object�� Trigger ���� ������ �����Ѵ�. (SM) */
    IDE_TEST( smiObject::setObjectInfo( QC_SMI_STMT( aStatement ),
                                        aTriggerHandle,
                                        aTriggerStmtBuffer,
                                        sNewOffset )
              != IDE_SUCCESS );

    /* New Trigger Cache�� �����ϰ� SM�� ����Ѵ�. (Meta Cache) */
    IDE_TEST( allocTriggerCache( aTriggerHandle,
                                 aTriggerOID,
                                 aNewTriggerCache )
              != IDE_SUCCESS );

    /* Trigger Strings�� �����ϴ� Meta Table�� �����Ѵ�. (Meta Table) */
    IDE_TEST( qcmTrigger::updateTriggerStringsMetaInfo( aStatement,
                                                        aTableID,
                                                        aTriggerOID,
                                                        sNewOffset )
              != IDE_SUCCESS );

    IDE_TEST( qcmTrigger::removeTriggerStringsMetaInfo( aStatement,
                                                        aTriggerOID )
              != IDE_SUCCESS );

    IDE_TEST( qcmTrigger::insertTriggerStringsMetaInfo( aStatement,
                                                        aTableID,
                                                        aTriggerOID,
                                                        aTriggerStmtBuffer,
                                                        sNewOffset )
              != IDE_SUCCESS );

    sStatementAlloced = ID_FALSE;
    IDE_TEST( qcg::freeStatement( & sStatement ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sStatementAlloced == ID_TRUE )
    {
        (void)qcg::freeStatement( & sStatement );
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_FAILURE;
}

IDE_RC qdnTrigger::convertXlobToXlobValue( mtcTemplate       * aTemplate,
                                           smiTableCursor    * aTableCursor,
                                           void              * aTableRow,
                                           scGRID              aGRID,
                                           mtcColumn         * aSrcLobColumn,
                                           void              * aSrcValue,
                                           mtcColumn         * aDestLobColumn,
                                           void              * aDestValue,
                                           qcmTriggerRefType   aRefType )
{
    smLobLocator         sLocator;
    idBool               sOpened = ID_FALSE;
    UInt                 sInfo = 0;

    mtdLobType         * sLobValue;
    UInt                 sLobLength;
    UInt                 sReadLength;
    idBool               sIsNull;

    IDE_TEST_RAISE( !( (aDestLobColumn->module->id == MTD_BLOB_ID) ||
                       (aDestLobColumn->module->id == MTD_CLOB_ID) ),
                    ERR_CONVERT );

    if ( (aSrcLobColumn->module->id == MTD_BLOB_LOCATOR_ID) ||
         (aSrcLobColumn->module->id == MTD_CLOB_LOCATOR_ID) )
    {
        sLocator = *(smLobLocator *)aSrcValue;
    }
    else if ( (aSrcLobColumn->module->id == MTD_BLOB_ID) ||
              (aSrcLobColumn->module->id == MTD_CLOB_ID) )
    {
        if ( (aSrcLobColumn->column.flag & SMI_COLUMN_STORAGE_MASK)
             == SMI_COLUMN_STORAGE_MEMORY )
        {
            IDE_TEST( mtc::openLobCursorWithRow( aTableCursor,
                                                 aTableRow,
                                                 & aSrcLobColumn->column,
                                                 sInfo,
                                                 SMI_LOB_READ_MODE,
                                                 & sLocator )
                      != IDE_SUCCESS );

            sOpened = ID_TRUE;
        }
        else
        {
            // BUG-16318
            IDE_TEST_RAISE( SC_GRID_IS_NULL(aGRID), ERR_NOT_APPLICABLE );
            //fix BUG-19687

            if( aRefType == QCM_TRIGGER_REF_OLD_ROW )
            {
                /* BUG-43961 trigger�� ���� lob�� ������ ��� old reference�� ����
                   old ���� ���������� �������� ���ؼ��� READ mode�� �����ؾ� �Ѵ�. */ 
                IDE_TEST( mtc::openLobCursorWithGRID( aTableCursor,
                                                      aGRID,
                                                      & aSrcLobColumn->column,
                                                      sInfo,
                                                      SMI_LOB_READ_MODE,
                                                      & sLocator )
                          != IDE_SUCCESS );
            }
            else
            {
                /* BUG-43838 trigger�� ���� lob�� ������ ��� lob cursor�� 
                   READ_WRITE mode�� �����ؾ� ���������� ���� ������ �� �ִ�. */
                IDE_TEST( mtc::openLobCursorWithGRID( aTableCursor,
                                                      aGRID,
                                                      & aSrcLobColumn->column,
                                                      sInfo,
                                                      SMI_LOB_READ_WRITE_MODE,
                                                      & sLocator )
                          != IDE_SUCCESS );

            }

            sOpened = ID_TRUE;
        }
    }
    else
    {
        IDE_RAISE( ERR_CONVERT );
    }

    IDE_TEST( mtc::getLobLengthLocator( sLocator,
                                        & sIsNull,
                                        & sLobLength,
                                        mtc::getStatistics(aTemplate) ) != IDE_SUCCESS );

    if ( sIsNull == ID_TRUE )
    {
        aDestLobColumn->module->null( aDestLobColumn, aDestValue );
    }
    else
    {
        IDE_TEST_RAISE( (UInt)aDestLobColumn->precision < sLobLength, ERR_CONVERT );

        sLobValue = (mtdLobType *)aDestValue;

        IDE_TEST( mtc::readLob( NULL,               /* idvSQL* */
                                sLocator,
                                0,
                                sLobLength,
                                sLobValue->value,
                                & sReadLength )
                  != IDE_SUCCESS );

        sLobValue->length = (SLong)sReadLength;
    }

    if ( sOpened == ID_TRUE )
    {
        sOpened = ID_FALSE;
        IDE_TEST( aTemplate->closeLobLocator( sLocator )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NOT_APPLICABLE );
    {
        IDE_SET( ideSetErrorCode( mtERR_ABORT_NOT_APPLICABLE ) );
    }
    IDE_EXCEPTION( ERR_CONVERT )
    {
        IDE_SET( ideSetErrorCode( mtERR_ABORT_CONVERSION_DISABLE ) );
    }
    IDE_EXCEPTION_END;

    if ( sOpened == ID_TRUE )
    {
        (void) aTemplate->closeLobLocator( sLocator );
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_FAILURE;
}

// PROJ-2219 Row-level before update trigger
// Row-level before update trigger���� new row�� ����
// �����ϴ� column list�� �����Ѵ�.
IDE_RC qdnTrigger::makeRefColumnList( qcStatement * aQcStmt )
{
    qdnCreateTriggerParseTree * sParseTree;
    qdnTriggerRef             * sRef;
    qtcNode                   * sNode;
    UChar                     * sRefColumnList  = NULL;
    UInt                        sRefColumnCount = 0;
    UInt                        sTableColumnCount;
    UInt                        i;

    sParseTree = (qdnCreateTriggerParseTree *)(aQcStmt->myPlan->parseTree);

    IDE_ERROR( sParseTree != NULL );
    IDE_ERROR( sParseTree->refColumnCount == 0 );
    IDE_ERROR( sParseTree->refColumnList  == NULL );

    sTableColumnCount = sParseTree->tableInfo->columnCount;

    // row-level before update trigger
    if ( ( sParseTree->actionCond.actionGranularity == QCM_TRIGGER_ACTION_EACH_ROW ) &&
         ( sParseTree->triggerEvent.eventTime == QCM_TRIGGER_BEFORE ) &&
         ( ( sParseTree->triggerEvent.eventTypeList->eventType & QCM_TRIGGER_EVENT_UPDATE ) != 0 ) )
    {
        for ( sRef = sParseTree->triggerReference;
              sRef != NULL;
              sRef = sRef->next )
        {
            if ( sRef->refType == QCM_TRIGGER_REF_NEW_ROW )
            {
                // sRefColumnList�� ������ NULL�� �����̴�.
                // NEW ROW�� �� ���� �����ϱ� �����̴�.
                IDE_ERROR( sRefColumnList == NULL );

                IDU_FIT_POINT( "qdnTrigger::makeRefColumnList::alloc::sRefColumnList",
                                idERR_ABORT_InsufficientMemory );

                IDE_TEST( QC_QMP_MEM( aQcStmt )->alloc(
                        ID_SIZEOF(UChar) * sTableColumnCount,
                        (void**)&sRefColumnList )
                    != IDE_SUCCESS);

                sNode = ((qtcNode *) sRef->rowVar->variableTypeNode->node.arguments);

                for ( i = 0; i < sTableColumnCount; i++ )
                {
                    IDE_ERROR( sNode != NULL );

                    if ( (sNode->lflag & QTC_NODE_VALIDATE_MASK) == QTC_NODE_VALIDATE_TRUE )
                    {
                        sRefColumnList[i] = QDN_REF_COLUMN_TRUE;
                        sRefColumnCount++;
                    }
                    else
                    {
                        sRefColumnList[i] = QDN_REF_COLUMN_FALSE;
                    }

                    sNode = (qtcNode *)sNode->node.next;
                }

                sParseTree->refColumnList  = sRefColumnList;
                sParseTree->refColumnCount = sRefColumnCount;
                break;
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

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

// PROJ-2219 Row-level before update trigger
// Invalid �� trigger�� ������ compile�ϰ�, DML�� rebuild �Ѵ�.
IDE_RC qdnTrigger::verifyTriggers( qcStatement   * aQcStmt,
                                   qcmTableInfo  * aTableInfo,
                                   smiColumnList * aUptColumn,
                                   idBool        * aIsNeedRebuild )
{
    qcmTriggerInfo  * sTriggerInfo;
    qdnTriggerCache * sTriggerCache;
    idBool            sNeedAction;
    UInt              i;
    UInt              sStage = 0;

    IDE_ERROR( aIsNeedRebuild != NULL );

    *aIsNeedRebuild = ID_FALSE;

    for ( i = 0; i < aTableInfo->triggerCount; i++ )
    {
        sTriggerInfo = &aTableInfo->triggerInfo[i];

        IDE_TEST( checkCondition( aQcStmt,
                                  & aTableInfo->triggerInfo[i],
                                  QCM_TRIGGER_ACTION_EACH_ROW,
                                  QCM_TRIGGER_BEFORE,
                                  QCM_TRIGGER_EVENT_UPDATE,
                                  aUptColumn,
                                  & sNeedAction,
                                  aIsNeedRebuild )
                  != IDE_SUCCESS );

        if ( sNeedAction == ID_TRUE )
        {
            IDE_TEST( smiObject::getObjectTempInfo( sTriggerInfo->triggerHandle,
                                                    (void**)&sTriggerCache)
                      != IDE_SUCCESS );;

            while ( 1 )
            {
                // S Latch ȹ��
                IDE_TEST( sTriggerCache->latch.lockRead (
                              NULL, /* idvSQL * */
                              NULL /* idvWeArgs* */ )
                          != IDE_SUCCESS );
                sStage = 1;

                if ( sTriggerCache->isValid != ID_TRUE )
                {
                    // Valid���� ������ Recompile �Ѵ�.

                    // Release Latch
                    sStage = 0;
                    IDE_TEST( sTriggerCache->latch.unlock() != IDE_SUCCESS );

                    while ( recompileTrigger( aQcStmt,
                                              sTriggerInfo )
                            != IDE_SUCCESS )
                    {
                        // rebuild error��� �ٽ� recompile�� �õ��Ѵ�.
                        IDE_TEST( ideIsRebuild() != IDE_SUCCESS );
                    }

                    *aIsNeedRebuild = ID_TRUE;

                    continue;
                }
                else
                {
                    // Release Latch
                    sStage = 0;
                    IDE_TEST( sTriggerCache->latch.unlock() != IDE_SUCCESS );

                    break;
                }
            }
        }
        else
        {
            // Nothing to do.
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch ( sStage )
    {
        case 1:
            (void) sTriggerCache->latch.unlock();
            break;
        default:
            break;
    }

    return IDE_FAILURE;
}

// BUG-38137 Trigger�� when condition���� PSM�� ȣ���� �� ����.
IDE_RC qdnTrigger::checkNoSpFunctionCall( qtcNode * aNode )
{
    qtcNode * sNode;

    for ( sNode = aNode; sNode != NULL; sNode = (qtcNode*)sNode->node.next )
    {
        IDE_TEST( ( sNode->lflag & QTC_NODE_PROC_FUNCTION_MASK )
                  == QTC_NODE_PROC_FUNCTION_TRUE );

        if ( sNode->node.arguments != NULL )
        {
            IDE_TEST( checkNoSpFunctionCall( (qtcNode*)(sNode->node.arguments ) )
                      != IDE_SUCCESS );

        }
        else
        {
            // Nothing to do.
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
