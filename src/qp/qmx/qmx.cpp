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
 * $Id: qmx.cpp 90311 2021-03-24 09:46:45Z ahra.cho $
 **********************************************************************/

#include <cm.h>
#include <idl.h>
#include <ide.h>
#include <mtdTypes.h>
#include <smi.h>
#include <qci.h>
#include <qcg.h>
#include <qmx.h>
#include <qmo.h>
#include <qmn.h>
#include <qcuSqlSourceInfo.h>
#include <qcuProperty.h>
#include <qdn.h>
#include <qdnForeignKey.h>
#include <qdbCommon.h>
#include <qdnTrigger.h>
#include <qmmParseTree.h>
#include <qcm.h>
#include <qmcInsertCursor.h>
#include <qsxUtil.h>
#include <qcsModule.h>
#include <qdnCheck.h>
#include <smiMisc.h>
#include <smDef.h>
#include <qcmDictionary.h>
#include <qds.h>
#include <qcuTemporaryObj.h>

extern mtdModule mtdDouble;

// PROJ-1518
IDE_RC qmx::atomicExecuteInsertBefore( qcStatement  * aStatement )
{
/***********************************************************************
 *
 * Description :
 *    Atomic Insert �� ������ �غ���
 *
 * Implementation :
 *    1. SMI_TABLE_LOCK_IX lock �� ��´� => smiValidateAndLockObjects
 *    2. result row count �ʱ�ȭ
 *    3. Before Each Statement Trigger ����
 *    4. set SYSDATE
 *    5. Cursor Open
 *
 ***********************************************************************/

    qmmInsParseTree    * sParseTree;
    qcmTableInfo       * sTableForInsert;
    UInt               * sINSTflag;
    UInt                 sPlanID;

    // PROJ-1566
    smiTableLockMode     sLockMode;

    sParseTree = (qmmInsParseTree*) aStatement->myPlan->parseTree;

    IDE_ASSERT( sParseTree != NULL );

    //------------------------------------------
    // �ش� Table�� lock�� ȹ����.
    //------------------------------------------

    if ( ((qmncINST*)aStatement->myPlan->plan)->isAppend == ID_TRUE )
    {
        sLockMode = SMI_TABLE_LOCK_SIX;
    }
    else
    {
        sLockMode = SMI_TABLE_LOCK_IX;
    }

    IDE_TEST( lockTableForDML( aStatement,
                               sParseTree->insertTableRef,
                               sLockMode )
              != IDE_SUCCESS );

    sTableForInsert = sParseTree->insertTableRef->tableInfo;

    //------------------------------------------
    // STATEMENT GRANULARITY TRIGGER�� ����
    //------------------------------------------
    // before trigger
    IDE_TEST( qdnTrigger::fireTrigger( aStatement,
                                       aStatement->qmxMem,
                                       sTableForInsert,
                                       QCM_TRIGGER_ACTION_EACH_STMT,
                                       QCM_TRIGGER_BEFORE,
                                       QCM_TRIGGER_EVENT_INSERT,
                                       NULL,  // UPDATE Column
                                       NULL,            /* Table Cursor */
                                       SC_NULL_GRID,    /* Row GRID */
                                       NULL,  // OLD ROW
                                       NULL,  // OLD ROW Column
                                       NULL,  // NEW ROW
                                       NULL ) // NEW ROW Column
              != IDE_SUCCESS );

    //------------------------------------------
    // INSERT�� ó���ϱ� ���� �⺻ �� ȹ��
    //------------------------------------------

    // set SYSDATE
    IDE_TEST( qtc::setDatePseudoColumn( QC_PRIVATE_TMPLATE( aStatement ) ) != IDE_SUCCESS );

    // initialize result row count
    QC_PRIVATE_TMPLATE(aStatement)->numRows = 0;

    //------------------------------------------
    // INSERT Plan �ʱ�ȭ
    //------------------------------------------

    // BUG-45288 atomic insert �� direct path ����. normal insert �Ұ���.
    ((qmndINST*) (QC_PRIVATE_TMPLATE(aStatement)->tmplate.data +
                  aStatement->myPlan->plan->offset))->isAppend =
        ((qmncINST*)aStatement->myPlan->plan)->isAppend;
  
    sPlanID = ((qmncINST*)(aStatement->myPlan->plan))->planID;
    sINSTflag = &(QC_PRIVATE_TMPLATE(aStatement)->planFlag[sPlanID]);

    *sINSTflag &= ~QMND_INST_ATOMIC_MASK;
    *sINSTflag |= QMND_INST_ATOMIC_TRUE;

    IDE_TEST( qmnINST::init( QC_PRIVATE_TMPLATE(aStatement),
                             aStatement->myPlan->plan)
             != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmx::atomicExecuteInsert( qcStatement  * aStatement )
{
/***********************************************************************
 *
 * Description :
 *    Atomic Insert �� ������
 *
 * Implementation :
 *    1. �������� ����ϸ� �� ������ ã�Ƴ��´�
 *    2. Before Trigger
 *    3. �Է��� ������ smiValue �� ����� => makeSmiValueWithValue
 *    4. smiCursorTable::insertRow �� �����ؼ� �Է��Ѵ�.
 *    5. result row count ����
 *    6. After Trigger
 *
 ***********************************************************************/
    
    qmcRowFlag           sFlag = QMC_ROW_INITIALIZE;

    //------------------------------------------
    // INSERT�� ó���ϱ� ���� �⺻ �� ȹ��
    //------------------------------------------

    // check session cache SEQUENCE.CURRVAL
    if (aStatement->myPlan->parseTree->currValSeqs != NULL)
    {
        IDE_TEST(findSessionSeqCaches(aStatement,
                                      aStatement->myPlan->parseTree) != IDE_SUCCESS);
    }
    else
    {
        // Nothing to do
    }

    // get SEQUENCE.NEXTVAL
    if (aStatement->myPlan->parseTree->nextValSeqs != NULL)
    {
        IDE_TEST(addSessionSeqCaches(aStatement,
                                     aStatement->myPlan->parseTree) != IDE_SUCCESS);
    }
    else
    {
        // Nothing to do
    }

    //------------------------------------------
    // INSERT�� ����
    //------------------------------------------

    // �̸� ������Ų��. doIt�� ������ �� �ִ�.
    QC_PRIVATE_TMPLATE(aStatement)->numRows++;
    
    // insert�� plan�� �����Ѵ�.
    IDE_TEST( qmnINST::doIt( QC_PRIVATE_TMPLATE(aStatement),
                             aStatement->myPlan->plan,
                             &sFlag )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmx::atomicExecuteInsertAfter( qcStatement   * aStatement )
{
/***********************************************************************
 *
 * Description :
 *    Atomic Insert �� ������ ������ ��ó��
 *
 * Implementation :
 *    1. Cursor Close
 *    2. �����ϴ� Ű�� ������, parent ���̺��� �÷��� ���� �����ϴ��� �˻�
 *    3. Before Each Statement Trigger ����
 *
 ***********************************************************************/
    
    qmmInsParseTree     * sParseTree;
    qcmTableInfo        * sTableForInsert;

    sParseTree = (qmmInsParseTree*) aStatement->myPlan->parseTree;
    sTableForInsert = sParseTree->insertTableRef->tableInfo;
    
    //------------------------------------------
    // INSERT cursor close
    //------------------------------------------

    IDE_TEST( qmnINST::closeCursor( QC_PRIVATE_TMPLATE(aStatement),
                                    aStatement->myPlan->plan )
              != IDE_SUCCESS );
    
    /* PROJ-1071 */
    IDE_TEST(qcg::joinThread(aStatement) != IDE_SUCCESS);
    
    IDE_TEST( QC_PRIVATE_TMPLATE(aStatement)->cursorMgr->closeAllCursor(aStatement->mStatistics)
              != IDE_SUCCESS );

    IDE_TEST(qcg::finishAndReleaseThread(aStatement) != IDE_SUCCESS);

    //------------------------------------------
    // Foreign Key Reference �˻�
    //------------------------------------------
    
    if ( QC_PRIVATE_TMPLATE(aStatement)->numRows == 0 )
    {
        // Nothing to do.
    }
    else
    {
        if ( sParseTree->parentConstraints != NULL )
        {
            IDE_TEST( qmnINST::checkInsertRef(
                      QC_PRIVATE_TMPLATE(aStatement),
                      aStatement->myPlan->plan )
                  != IDE_SUCCESS );
        }
        else
        {
            // Nothing To Do
        }
    }

    //------------------------------------------
    // PROJ-1359 Trigger
    // STATEMENT GRANULARITY TRIGGER�� ����
    //------------------------------------------
    IDE_TEST( qdnTrigger::fireTrigger( aStatement,
                                       aStatement->qmxMem,
                                       sTableForInsert,
                                       QCM_TRIGGER_ACTION_EACH_STMT,
                                       QCM_TRIGGER_AFTER,
                                       QCM_TRIGGER_EVENT_INSERT,
                                       NULL,  // UPDATE Column
                                       NULL,            /* Table Cursor */
                                       SC_NULL_GRID,    /* Row GRID */
                                       NULL,  // OLD ROW
                                       NULL,  // OLD ROW Column
                                       NULL,  // NEW ROW
                                       NULL ) // NEW ROW Column
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmx::atomicExecuteFinalize( qcStatement  * aStatement )
{
/***********************************************************************
 *
 * Description :
 *    Atomic Insert�� �߰��� ���ߴ� ���
 *
 * Implementation :
 *    1. Cursor Close
 *
 ***********************************************************************/
    
    //------------------------------------------
    // INSERT cursor close
    //------------------------------------------

    IDE_TEST( qmnINST::closeCursor( QC_PRIVATE_TMPLATE(aStatement),
                                    aStatement->myPlan->plan )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmx::executeInsertValues(qcStatement *aStatement)
{
/***********************************************************************
 *
 * Description :
 *    INSERT INTO ... �� execution ����
 *
 * Implementation :
 *    1. SMI_TABLE_LOCK_IX lock �� ��´� => smiValidateAndLockObjects
 *    2. result row count �ʱ�ȭ
 *    3. set SYSDATE
 *    4. �������� ����ϸ� �� ������ ã�Ƴ��´�
 *    6. �Է��� ������ smiValue �� ����� => makeSmiValueWithValue
 *    7. smiCursorTable::insertRow �� �����ؼ� �Է��Ѵ�.
 *    8. �����ϴ� Ű�� ������, parent ���̺��� �÷��� ���� �����ϴ��� �˻�
 *    9. result row count ����
 *
 ***********************************************************************/

#define IDE_FN "qmx::executeInsertValues"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmmInsParseTree    * sParseTree;
    qmsTableRef        * sTableRef;
    qcmTableInfo       * sTableForInsert;
    qmcRowFlag           sFlag = QMC_ROW_INITIALIZE;
    idBool               sInitialized = ID_FALSE;
    scGRID               sRowGRID;

    sParseTree = (qmmInsParseTree*) aStatement->myPlan->parseTree;
    sTableRef  = sParseTree->insertTableRef;

    //------------------------------------------
    // �ش� Table�� IX lock�� ȹ����.
    //------------------------------------------

    // PROJ-1502 PARTITIONED DISK TABLE
    IDE_TEST( lockTableForDML( aStatement,
                               sTableRef,
                               SMI_TABLE_LOCK_IX )
              != IDE_SUCCESS );

    sTableForInsert = sTableRef->tableInfo;

    //------------------------------------------
    // STATEMENT GRANULARITY TRIGGER�� ����
    //------------------------------------------
    // To fix BUG-12622
    // before trigger
    IDE_TEST( qdnTrigger::fireTrigger( aStatement,
                                       aStatement->qmxMem,
                                       sTableForInsert,
                                       QCM_TRIGGER_ACTION_EACH_STMT,
                                       QCM_TRIGGER_BEFORE,
                                       QCM_TRIGGER_EVENT_INSERT,
                                       NULL,  // UPDATE Column
                                       NULL,            /* Table Cursor */
                                       SC_NULL_GRID,    /* Row GRID */
                                       NULL,  // OLD ROW
                                       NULL,  // OLD ROW Column
                                       NULL,  // NEW ROW
                                       NULL ) // NEW ROW Column
              != IDE_SUCCESS );

    //------------------------------------------
    // INSERT�� ó���ϱ� ���� �⺻ �� ȹ��
    //------------------------------------------

    // initialize result row count
    QC_PRIVATE_TMPLATE(aStatement)->numRows = 0;

    // set SYSDATE
    IDE_TEST( qtc::setDatePseudoColumn( QC_PRIVATE_TMPLATE( aStatement ) ) != IDE_SUCCESS );

    // check session cache SEQUENCE.CURRVAL
    if (aStatement->myPlan->parseTree->currValSeqs != NULL)
    {
        IDE_TEST(findSessionSeqCaches(aStatement,
                                      aStatement->myPlan->parseTree) != IDE_SUCCESS);
    }
    else
    {
        // Nothing to do
    }

    // get SEQUENCE.NEXTVAL
    if (aStatement->myPlan->parseTree->nextValSeqs != NULL)
    {
        IDE_TEST(addSessionSeqCaches(aStatement,
                                     aStatement->myPlan->parseTree) != IDE_SUCCESS);
    }
    else
    {
        // Nothing to do
    }

    //------------------------------------------
    // INSERT�� ���� plan tree �ʱ�ȭ
    //------------------------------------------
    // BUG-45288 atomic insert �� direct path ����. normal insert �Ұ���.
    ((qmndINST*) (QC_PRIVATE_TMPLATE(aStatement)->tmplate.data +
                  aStatement->myPlan->plan->offset))->isAppend = ID_FALSE;

    IDE_TEST( qmnINST::init( QC_PRIVATE_TMPLATE(aStatement),
                             aStatement->myPlan->plan)
             != IDE_SUCCESS);
    sInitialized = ID_TRUE;

    //------------------------------------------
    // INSERT�� ����
    //------------------------------------------

    do
    {
        IDE_TEST( iduCheckSessionEvent( aStatement->mStatistics )
                  != IDE_SUCCESS );

        // �̸� ������Ų��. doIt�� ������ �� �ִ�.
        QC_PRIVATE_TMPLATE(aStatement)->numRows++;

        // insert�� plan�� �����Ѵ�.
        IDE_TEST( qmnINST::doIt( QC_PRIVATE_TMPLATE(aStatement),
                                 aStatement->myPlan->plan,
                                 &sFlag )
                  != IDE_SUCCESS );
    } while ( ( sFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST );

    IDE_TEST( qmnINST::getLastInsertedRowGRID( QC_PRIVATE_TMPLATE(aStatement),
                                               aStatement->myPlan->plan,
                                               & sRowGRID )
              != IDE_SUCCESS );

    //------------------------------------------
    // INSERT cursor close
    //------------------------------------------

    sInitialized = ID_FALSE;
    IDE_TEST( qmnINST::closeCursor( QC_PRIVATE_TMPLATE(aStatement),
                                    aStatement->myPlan->plan )
              != IDE_SUCCESS );

    /* PROJ-1071 */
    IDE_TEST(qcg::joinThread(aStatement) != IDE_SUCCESS);

    IDE_TEST( QC_PRIVATE_TMPLATE(aStatement)->cursorMgr->closeAllCursor(aStatement->mStatistics)
              != IDE_SUCCESS );

    IDE_TEST(qcg::finishAndReleaseThread(aStatement) != IDE_SUCCESS);

    //------------------------------------------
    // Foreign Key Reference �˻�
    //------------------------------------------

    if ( QC_PRIVATE_TMPLATE(aStatement)->numRows == 0 )
    {
        // Nothing to do.
    }
    else
    {
        if ( sParseTree->parentConstraints != NULL )
        {
            IDE_TEST( qmnINST::checkInsertRef(
                      QC_PRIVATE_TMPLATE(aStatement),
                      aStatement->myPlan->plan )
                  != IDE_SUCCESS );
        }
        else
        {
            // Nothing To Do
        }
    }

    //------------------------------------------
    // PROJ-1359 Trigger
    // STATEMENT GRANULARITY TRIGGER�� ����
    //------------------------------------------
    IDE_TEST( qdnTrigger::fireTrigger( aStatement,
                                       aStatement->qmxMem,
                                       sTableForInsert,
                                       QCM_TRIGGER_ACTION_EACH_STMT,
                                       QCM_TRIGGER_AFTER,
                                       QCM_TRIGGER_EVENT_INSERT,
                                       NULL,  // UPDATE Column
                                       NULL,            /* Table Cursor */
                                       SC_NULL_GRID,    /* Row GRID */
                                       NULL,  // OLD ROW
                                       NULL,  // OLD ROW Column
                                       NULL,  // NEW ROW
                                       NULL ) // NEW ROW Column
              != IDE_SUCCESS );

    // BUG-38129
    qcg::setLastModifiedRowGRID( aStatement, sRowGRID );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    QC_PRIVATE_TMPLATE(aStatement)->numRows = 0;

    if ( sInitialized == ID_TRUE )
    {
        (void) qmnINST::closeCursor( QC_PRIVATE_TMPLATE(aStatement),
                                     aStatement->myPlan->plan );
    }

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qmx::executeInsertSelect(qcStatement *aStatement)
{
/***********************************************************************
 *
 * Description :
 *    INSERT INTO ... SELECT ... �� execution ����
 *
 * Implementation :
 *    1. SMI_TABLE_LOCK_IX lock �� ��´� => smiValidateAndLockObjects
 *    2. result row count �ʱ�ȭ
 *    3. set SYSDATE
 *    4. �������� ����ϸ� �� ������ ã�Ƴ��´�
 *    6. set DEFAULT value ???
 *    7. select plan �� ��´�
 *    8. insert �� ���� Ŀ���� �����Ѵ�
 *    9. qmnPROJ::doIt
 *    10. select �� ������ smiValue �� �����
 *    11. smiCursorTable::insertRow �� �����ؼ� ���̺� �Է��Ѵ�
 *    12. �����ϴ� Ű�� ������, parent ���̺��� �÷��� ���� �����ϴ��� �˻�
 *    13. select �� ���� ���� ������ 9,10,11,12 �� �ݺ��Ѵ�
 *
 ***********************************************************************/

#define IDE_FN "qmx::executeInsertSelect"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmmInsParseTree   * sParseTree;
    qmsTableRef       * sTableRef;
    qcmTableInfo      * sTableForInsert;
    qmcRowFlag          sRowFlag = QMC_ROW_INITIALIZE;
    idBool              sInitialized = ID_FALSE;
    scGRID              sRowGRID;

    // PROJ-1566
    smiTableLockMode    sLockMode;
    idBool              sIsOld;

    //------------------------------------------
    // �⺻ ���� ����
    //------------------------------------------

    sParseTree = (qmmInsParseTree *) aStatement->myPlan->parseTree;
    sTableRef  = sParseTree->insertTableRef;
    
    //------------------------------------------
    // �ش� Table�� IX lock�� ȹ����.
    //------------------------------------------
    
    if ( ((qmncINST*)aStatement->myPlan->plan)->isAppend == ID_TRUE )
    {
        sLockMode = SMI_TABLE_LOCK_SIX;
    }
    else
    {
        sLockMode = SMI_TABLE_LOCK_IX;
    }

    //---------------------------------------------------
    // PROJ-1502 PARTITIONED DISK TABLE
    //---------------------------------------------------

    IDE_TEST( lockTableForDML( aStatement,
                               sTableRef,
                               sLockMode )
              != IDE_SUCCESS );
    
    //fix BUG-14080
    IDE_TEST( checkPlanTreeOld( aStatement, & sIsOld ) != IDE_SUCCESS );

    IDE_TEST_RAISE( sIsOld == ID_TRUE, err_plan_too_old );

    sTableForInsert = sTableRef->tableInfo;

    //------------------------------------------
    // STATEMENT GRANULARITY TRIGGER�� ����
    //------------------------------------------
    // To fix BUG-12622
    // before trigger
    IDE_TEST( qdnTrigger::fireTrigger( aStatement,
                                       aStatement->qmxMem,
                                       sTableForInsert,
                                       QCM_TRIGGER_ACTION_EACH_STMT,
                                       QCM_TRIGGER_BEFORE,
                                       QCM_TRIGGER_EVENT_INSERT,
                                       NULL,  // UPDATE Column
                                       NULL,            /* Table Cursor */
                                       SC_NULL_GRID,    /* Row GRID */
                                       NULL,  // OLD ROW
                                       NULL,  // OLD ROW Column
                                       NULL,  // NEW ROW
                                       NULL ) // NEW ROW Column
              != IDE_SUCCESS );

    //------------------------------------------
    // INSERT�� ó���ϱ� ���� �⺻ �� ȹ��
    //------------------------------------------

    // initialize result row count
    QC_PRIVATE_TMPLATE(aStatement)->numRows = 0;

    // set SYSDATE
    IDE_TEST( qtc::setDatePseudoColumn( QC_PRIVATE_TMPLATE( aStatement ) ) != IDE_SUCCESS );

    // check session cache SEQUENCE.CURRVAL
    if (aStatement->myPlan->parseTree->currValSeqs != NULL)
    {
        IDE_TEST(findSessionSeqCaches(aStatement,
                                      aStatement->myPlan->parseTree)
                 != IDE_SUCCESS);
    }
    else
    {
        // Nothing to do
    }

    // get SEQUENCE.NEXTVAL
    if (aStatement->myPlan->parseTree->nextValSeqs != NULL)
    {
        IDE_TEST(addSessionSeqCaches(aStatement,
                                     aStatement->myPlan->parseTree)
                 != IDE_SUCCESS);
    }
    else
    {
        // Nothing to do
    }

    // check session cache SEQUENCE.CURRVAL
    if (sParseTree->select->myPlan->parseTree->currValSeqs != NULL)
    {
        IDE_TEST(findSessionSeqCaches(aStatement,
                                      sParseTree->select->myPlan->parseTree)
                 != IDE_SUCCESS);
    }
    else
    {
        // Nothing to do
    }
    
    // get SEQUENCE.NEXTVAL
    if (sParseTree->select->myPlan->parseTree->nextValSeqs != NULL)
    {
        IDE_TEST(addSessionSeqCaches(aStatement,
                                     sParseTree->select->myPlan->parseTree)
                 != IDE_SUCCESS);
    }
    else
    {
        // Nothing to do
    }
    
    //------------------------------------------
    // INSERT Plan �ʱ�ȭ
    //------------------------------------------

    // PROJ-1446 Host variable�� ������ ���� ����ȭ
    IDE_TEST( qmo::optimizeForHost( aStatement ) != IDE_SUCCESS );

    // BUG-45288 atomic insert �� direct path ����. normal insert �Ұ���.
    ((qmndINST*) (QC_PRIVATE_TMPLATE(aStatement)->tmplate.data +
                  aStatement->myPlan->plan->offset))->isAppend =
        ((qmncINST*)aStatement->myPlan->plan)->isAppend;
    
    IDE_TEST( qmnINST::init( QC_PRIVATE_TMPLATE(aStatement),
                             aStatement->myPlan->plan)
             != IDE_SUCCESS);
    sInitialized = ID_TRUE;

    //------------------------------------------
    // INSERT�� ����
    //------------------------------------------

    do
    {
        // BUG-22287 insert �ð��� ���� �ɸ��� ��쿡 session
        // out�Ǿ insert�� ���� �� ���� ��ٸ��� ������ �ִ�.
        // session out�� Ȯ���Ѵ�.
        IDE_TEST( iduCheckSessionEvent( aStatement->mStatistics )
                  != IDE_SUCCESS );
        
        // �̸� ������Ų��. doIt�� ������ �� �ִ�.
        QC_PRIVATE_TMPLATE(aStatement)->numRows++;
        
        // update�� plan�� �����Ѵ�.
        IDE_TEST( qmnINST::doIt( QC_PRIVATE_TMPLATE(aStatement),
                                 aStatement->myPlan->plan,
                                 &sRowFlag )
                  != IDE_SUCCESS );
        
    } while ( ( sRowFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST );

    IDE_TEST( qmnINST::getLastInsertedRowGRID( QC_PRIVATE_TMPLATE(aStatement),
                                               aStatement->myPlan->plan,
                                               &sRowGRID )
              != IDE_SUCCESS );
    
    // DATA_NONE�� ���� ���ش�.
    QC_PRIVATE_TMPLATE(aStatement)->numRows--;
    
    //------------------------------------------
    // INSERT cursor close
    //------------------------------------------

    sInitialized = ID_FALSE;
    IDE_TEST( qmnINST::closeCursor( QC_PRIVATE_TMPLATE(aStatement),
                                    aStatement->myPlan->plan )
              != IDE_SUCCESS );
    
    /* PROJ-1071 */
    IDE_TEST(qcg::joinThread(aStatement) != IDE_SUCCESS);
    
    IDE_TEST( QC_PRIVATE_TMPLATE(aStatement)->cursorMgr->closeAllCursor(aStatement->mStatistics)
              != IDE_SUCCESS );

    IDE_TEST(qcg::finishAndReleaseThread(aStatement) != IDE_SUCCESS);
    
    //------------------------------------------
    // Foreign Key Reference �˻�
    //------------------------------------------

    //------------------------------------------
    // self reference�� ���� �ùٸ� ����� ����� ���ؼ���,
    // insert�� ��� ����ǰ� ���Ŀ�,
    // parent table�� ���� �˻縦 �����ؾ� ��.
    // ��) CREATE TABLE T1 ( I1 INTEGER, I2 INTEGER );
    //     CREATE TABLE T2 ( I1 INTEGER, I2 INTEGER );
    //     CREATE INDEX T2_I1 ON T2( I1 );
    //     ALTER TABLE T1 ADD CONSTRAINT T1_PK PRIMARY KEY (I1);
    //     ALTER TABLE T1 ADD CONSTRAINT
    //            T1_I2_FK FOREIGN KEY(I2) REFERENCES T1(I1);
    //
    //     insert into t2 values ( 1, 1 );
    //     insert into t2 values ( 2, 1 );
    //     insert into t2 values ( 3, 2 );
    //     insert into t2 values ( 4, 3 );
    //     insert into t2 values ( 5, 4 );
    //     insert into t2 values ( 6, 1 );
    //     insert into t2 values ( 7, 7 );
    //
    //     select * from t1 order by i1;
    //     select * from t2 order by i1;
    //
    //     insert into t1 select * from t2;
    //     : insert success�ؾ� ��.
    //      ( �� ������ �ϳ��� ���ڵ尡 �μ�Ʈ �ɶ�����
    //        parent���̺��� �����ϴ� �������� �ڵ��ϸ�,
    //        parent record is not found��� ������ �߻��ϴ� ������ ����. )
    //------------------------------------------

    // BUG-25180 insert�� row�� ������ ������ �˻��ؾ� ��
    if( QC_PRIVATE_TMPLATE(aStatement)->numRows == 0 )
    {
        // Nothing to do.
    }
    else
    {
        if ( sParseTree->parentConstraints != NULL )
        {
            IDE_TEST( qmnINST::checkInsertRef(
                      QC_PRIVATE_TMPLATE(aStatement),
                      aStatement->myPlan->plan )
                  != IDE_SUCCESS );
        }
        else
        {
            // Nothing To Do
        }
    }

    //------------------------------------------
    // STATEMENT GRANULARITY TRIGGER�� ����
    //------------------------------------------
    IDE_TEST( qdnTrigger::fireTrigger( aStatement,
                                       aStatement->qmxMem,
                                       sTableForInsert,
                                       QCM_TRIGGER_ACTION_EACH_STMT,
                                       QCM_TRIGGER_AFTER,
                                       QCM_TRIGGER_EVENT_INSERT,
                                       NULL,  // UPDATE Column
                                       NULL,            /* Table Cursor */
                                       SC_NULL_GRID,    /* Row GRID */
                                       NULL,  // OLD ROW
                                       NULL,  // OLD ROW Column
                                       NULL,  // NEW ROW
                                       NULL ) // NEW ROW Column
              != IDE_SUCCESS );

    // BUG-38129
    qcg::setLastModifiedRowGRID( aStatement, sRowGRID );
    
    return IDE_SUCCESS;

    //fix BUG-14080
    IDE_EXCEPTION(err_plan_too_old);
    {
        IDE_SET(ideSetErrorCode(qpERR_REBUILD_QMX_TOO_OLD_PLANTREE));
    }
    IDE_EXCEPTION_END;

    QC_PRIVATE_TMPLATE(aStatement)->numRows = 0;
    
    if ( sInitialized == ID_TRUE )
    {
        (void) qmnINST::closeCursor( QC_PRIVATE_TMPLATE(aStatement),
                                     aStatement->myPlan->plan );
    }

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qmx::executeMultiInsertSelect(qcStatement *aStatement)
{
/***********************************************************************
 *
 * Description :
 *    INSERT INTO ... SELECT ... �� execution ����
 *
 * Implementation :
 *    1. SMI_TABLE_LOCK_IX lock �� ��´� => smiValidateAndLockObjects
 *    2. result row count �ʱ�ȭ
 *    3. set SYSDATE
 *    4. �������� ����ϸ� �� ������ ã�Ƴ��´�
 *    6. set DEFAULT value ???
 *    7. select plan �� ��´�
 *    8. insert �� ���� Ŀ���� �����Ѵ�
 *    9. qmnPROJ::doIt
 *    10. select �� ������ smiValue �� �����
 *    11. smiCursorTable::insertRow �� �����ؼ� ���̺� �Է��Ѵ�
 *    12. �����ϴ� Ű�� ������, parent ���̺��� �÷��� ���� �����ϴ��� �˻�
 *    13. select �� ���� ���� ������ 9,10,11,12 �� �ݺ��Ѵ�
 *
 ***********************************************************************/

#define IDE_FN "qmx::executeMultiInsertSelect"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmmInsParseTree   * sParseTree;
    qcmTableInfo      * sTableForInsert;
    qcStatement       * sSelect;
    qmcRowFlag          sRowFlag = QMC_ROW_INITIALIZE;
    idBool              sInitialized = ID_FALSE;

    // PROJ-1566
    smiTableLockMode    sLockMode;
    idBool              sIsOld;

    qmncMTIT          * sMTIT;
    qmnChildren       * sChildren;
    qmncINST          * sINST;
    UInt                sInsertCount = 0;

    //------------------------------------------
    // �⺻ ���� ����
    //------------------------------------------

    sParseTree = (qmmInsParseTree *) aStatement->myPlan->parseTree;
    sSelect    = sParseTree->select;

    sMTIT = (qmncMTIT*)aStatement->myPlan->plan;

    //------------------------------------------
    // Multi-insert
    //------------------------------------------
    
    for ( sChildren = sMTIT->plan.children;
          sChildren != NULL;
          sChildren = sChildren->next )
    {
        sINST = (qmncINST*)sChildren->childPlan;
        
        //------------------------------------------
        // �ش� Table�� IX lock�� ȹ����.
        //------------------------------------------
        
        if ( sINST->isAppend == ID_TRUE )
        {
            sLockMode = SMI_TABLE_LOCK_SIX;
        }
        else
        {
            sLockMode = SMI_TABLE_LOCK_IX;
        }

        //---------------------------------------------------
        // PROJ-1502 PARTITIONED DISK TABLE
        //---------------------------------------------------

        IDE_TEST( lockTableForDML( aStatement,
                                   sINST->tableRef,
                                   sLockMode )
                  != IDE_SUCCESS );

        sInsertCount++;
    }

    //fix BUG-14080
    IDE_TEST( checkPlanTreeOld( aStatement, & sIsOld ) != IDE_SUCCESS );

    IDE_TEST_RAISE( sIsOld == ID_TRUE, err_plan_too_old );

    for ( sChildren = sMTIT->plan.children;
          sChildren != NULL;
          sChildren = sChildren->next )
    {
        sINST = (qmncINST*)sChildren->childPlan;
        sTableForInsert = sINST->tableRef->tableInfo;

        //------------------------------------------
        // STATEMENT GRANULARITY TRIGGER�� ����
        //------------------------------------------
        // To fix BUG-12622
        // before trigger
        IDE_TEST( qdnTrigger::fireTrigger( aStatement,
                                           aStatement->qmxMem,
                                           sTableForInsert,
                                           QCM_TRIGGER_ACTION_EACH_STMT,
                                           QCM_TRIGGER_BEFORE,
                                           QCM_TRIGGER_EVENT_INSERT,
                                           NULL,  // UPDATE Column
                                           NULL,            /* Table Cursor */
                                           SC_NULL_GRID,    /* Row GRID */
                                           NULL,  // OLD ROW
                                           NULL,  // OLD ROW Column
                                           NULL,  // NEW ROW
                                           NULL ) // NEW ROW Column
                  != IDE_SUCCESS );
    }

    //------------------------------------------
    // INSERT�� ó���ϱ� ���� �⺻ �� ȹ��
    //------------------------------------------

    // initialize result row count
    QC_PRIVATE_TMPLATE(aStatement)->numRows = 0;

    // set SYSDATE
    IDE_TEST( qtc::setDatePseudoColumn( QC_PRIVATE_TMPLATE( aStatement ) ) != IDE_SUCCESS );

    // check session cache SEQUENCE.CURRVAL
    if (sSelect->myPlan->parseTree->currValSeqs != NULL)
    {
        IDE_TEST(findSessionSeqCaches(aStatement,
                                      sSelect->myPlan->parseTree)
                 != IDE_SUCCESS);
    }
    else
    {
        // Nothing to do
    }

    // get SEQUENCE.NEXTVAL
    if (sSelect->myPlan->parseTree->nextValSeqs != NULL)
    {
        IDE_TEST(addSessionSeqCaches(aStatement,
                                     sSelect->myPlan->parseTree)
                 != IDE_SUCCESS);
    }
    else
    {
        // Nothing to do
    }
    
    // check session cache SEQUENCE.CURRVAL
    if (aStatement->myPlan->parseTree->currValSeqs != NULL)
    {
        IDE_TEST(findSessionSeqCaches(aStatement,
                                      aStatement->myPlan->parseTree)
                 != IDE_SUCCESS);
    }
    else
    {
        // Nothing to do
    }

    // get SEQUENCE.NEXTVAL
    if (aStatement->myPlan->parseTree->nextValSeqs != NULL)
    {
        IDE_TEST(addSessionSeqCaches(aStatement,
                                     aStatement->myPlan->parseTree)
                 != IDE_SUCCESS);
    }
    else
    {
        // Nothing to do
    }

    //------------------------------------------
    // INSERT Plan �ʱ�ȭ
    //------------------------------------------

    // PROJ-1446 Host variable�� ������ ���� ����ȭ
    IDE_TEST( qmo::optimizeForHost( aStatement ) != IDE_SUCCESS );

    IDE_TEST( qmnMTIT::init( QC_PRIVATE_TMPLATE(aStatement),
                             aStatement->myPlan->plan)
             != IDE_SUCCESS);
    sInitialized = ID_TRUE;

    //------------------------------------------
    // INSERT�� ����
    //------------------------------------------

    do
    {
        // BUG-22287 insert �ð��� ���� �ɸ��� ��쿡 session
        // out�Ǿ insert�� ���� �� ���� ��ٸ��� ������ �ִ�.
        // session out�� Ȯ���Ѵ�.
        IDE_TEST( iduCheckSessionEvent( aStatement->mStatistics )
                  != IDE_SUCCESS );
        
        // �̸� ������Ų��. doIt�� ������ �� �ִ�.
        QC_PRIVATE_TMPLATE(aStatement)->numRows += sInsertCount;
        
        // update�� plan�� �����Ѵ�.
        IDE_TEST( qmnMTIT::doIt( QC_PRIVATE_TMPLATE(aStatement),
                                 aStatement->myPlan->plan,
                                 &sRowFlag )
                  != IDE_SUCCESS );
        
    } while ( ( sRowFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST );

    // DATA_NONE�� ���� ���ش�.
    QC_PRIVATE_TMPLATE(aStatement)->numRows -= sInsertCount;
    
    //------------------------------------------
    // INSERT cursor close
    //------------------------------------------

    for ( sChildren = sMTIT->plan.children;
          sChildren != NULL;
          sChildren = sChildren->next )
    {
        IDE_TEST( qmnINST::closeCursor( QC_PRIVATE_TMPLATE(aStatement),
                                        sChildren->childPlan )
                  != IDE_SUCCESS );
    }
    sInitialized = ID_FALSE;
    
    /* PROJ-1071 */
    IDE_TEST(qcg::joinThread(aStatement) != IDE_SUCCESS);

    IDE_TEST( QC_PRIVATE_TMPLATE(aStatement)->cursorMgr->closeAllCursor(aStatement->mStatistics)
              != IDE_SUCCESS );

    IDE_TEST(qcg::finishAndReleaseThread(aStatement) != IDE_SUCCESS);

    //------------------------------------------
    // Foreign Key Reference �˻�
    //------------------------------------------

    //------------------------------------------
    // self reference�� ���� �ùٸ� ����� ����� ���ؼ���,
    // insert�� ��� ����ǰ� ���Ŀ�,
    // parent table�� ���� �˻縦 �����ؾ� ��.
    // ��) CREATE TABLE T1 ( I1 INTEGER, I2 INTEGER );
    //     CREATE TABLE T2 ( I1 INTEGER, I2 INTEGER );
    //     CREATE INDEX T2_I1 ON T2( I1 );
    //     ALTER TABLE T1 ADD CONSTRAINT T1_PK PRIMARY KEY (I1);
    //     ALTER TABLE T1 ADD CONSTRAINT
    //            T1_I2_FK FOREIGN KEY(I2) REFERENCES T1(I1);
    //
    //     insert into t2 values ( 1, 1 );
    //     insert into t2 values ( 2, 1 );
    //     insert into t2 values ( 3, 2 );
    //     insert into t2 values ( 4, 3 );
    //     insert into t2 values ( 5, 4 );
    //     insert into t2 values ( 6, 1 );
    //     insert into t2 values ( 7, 7 );
    //
    //     select * from t1 order by i1;
    //     select * from t2 order by i1;
    //
    //     insert into t1 select * from t2;
    //     : insert success�ؾ� ��.
    //      ( �� ������ �ϳ��� ���ڵ尡 �μ�Ʈ �ɶ�����
    //        parent���̺��� �����ϴ� �������� �ڵ��ϸ�,
    //        parent record is not found��� ������ �߻��ϴ� ������ ����. )
    //------------------------------------------

    // BUG-25180 insert�� row�� ������ ������ �˻��ؾ� ��
    if( QC_PRIVATE_TMPLATE(aStatement)->numRows == 0 )
    {
        // Nothing to do.
    }
    else
    {
        for ( sChildren = sMTIT->plan.children;
              ( ( sChildren != NULL ) && ( sParseTree != NULL ) );
              sChildren = sChildren->next, sParseTree = sParseTree->next )
        {
            if ( sParseTree->parentConstraints != NULL )
            {
                IDE_TEST( qmnINST::checkInsertRef(
                              QC_PRIVATE_TMPLATE(aStatement),
                              sChildren->childPlan )
                          != IDE_SUCCESS );
            }
            else
            {
                // Nothing To Do
            }
        }
    }

    //------------------------------------------
    // STATEMENT GRANULARITY TRIGGER�� ����
    //------------------------------------------
    for ( sChildren = sMTIT->plan.children;
          sChildren != NULL;
          sChildren = sChildren->next )
    {
        sINST = (qmncINST*)sChildren->childPlan;
        sTableForInsert = sINST->tableRef->tableInfo;

        IDE_TEST( qdnTrigger::fireTrigger( aStatement,
                                           aStatement->qmxMem,
                                           sTableForInsert,
                                           QCM_TRIGGER_ACTION_EACH_STMT,
                                           QCM_TRIGGER_AFTER,
                                           QCM_TRIGGER_EVENT_INSERT,
                                           NULL,  // UPDATE Column
                                           NULL,            /* Table Cursor */
                                           SC_NULL_GRID,    /* Row GRID */
                                           NULL,  // OLD ROW
                                           NULL,  // OLD ROW Column
                                           NULL,  // NEW ROW
                                           NULL ) // NEW ROW Column
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    //fix BUG-14080
    IDE_EXCEPTION(err_plan_too_old);
    {
        IDE_SET(ideSetErrorCode(qpERR_REBUILD_QMX_TOO_OLD_PLANTREE));
    }
    IDE_EXCEPTION_END;

    QC_PRIVATE_TMPLATE(aStatement)->numRows = 0;
    
    if ( sInitialized == ID_TRUE )
    {
        for ( sChildren = sMTIT->plan.children;
              sChildren != NULL;
              sChildren = sChildren->next )
        {
            (void) qmnINST::closeCursor( QC_PRIVATE_TMPLATE(aStatement),
                                         sChildren->childPlan );
        }
    }

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qmx::executeDelete(qcStatement *aStatement)
{
/***********************************************************************
 *
 * Description :
 *    DELETE FROM ... �� execution ����
 *
 * Implementation :
 *    1. SMI_TABLE_LOCK_IX lock �� ��´� => smiValidateAndLockObjects
 *    2. set SYSDATE
 *    3. qmnSCAN::init
 *    4. childConstraints �� ������,
 *       ������ ���ڵ带 ã�Ƽ�
 *       �� ���� �����ϴ� child ���̺��� �ִ��� Ȯ���ϰ�,
 *       ������ ������ ��ȯ�Ѵ�.
 *    5. childConstraints �� ������, �ش��ϴ� ���ڵ带 �����Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "qmx::executeDelete"

    qmmDelParseTree   * sParseTree;
    qmsTableRef       * sTableRef;

    idBool              sIsOld;
    qmcRowFlag          sFlag = QMC_ROW_INITIALIZE;
    idBool              sInitialized = ID_FALSE;

    //------------------------------------------
    // �⺻ ���� ȹ��
    //------------------------------------------

    sParseTree = (qmmDelParseTree *) aStatement->myPlan->parseTree;

    sTableRef = sParseTree->deleteTableRef;

    //------------------------------------------
    // �ش� Table�� IX lock�� ȹ����.
    //------------------------------------------

    IDE_TEST( lockTableForDML( aStatement,
                               sTableRef,
                               SMI_TABLE_LOCK_IX )
              != IDE_SUCCESS );

    //fix BUG-14080
    IDE_TEST( checkPlanTreeOld( aStatement, & sIsOld ) != IDE_SUCCESS );

    IDE_TEST_RAISE( sIsOld == ID_TRUE, err_plan_too_old );

    //-------------------------------------------------------
    // STATEMENT GRANULARITY TRIGGER�� ����
    //-------------------------------------------------------
    // To fix BUG-12622
    // before trigger
    IDE_TEST( fireStatementTriggerOnDeleteCascade(
                  aStatement,
                  sTableRef,
                  sParseTree->childConstraints,
                  QCM_TRIGGER_BEFORE )
              != IDE_SUCCESS );

    //------------------------------------------
    // DELETE �� ó���ϱ� ���� �⺻ �� ȹ��
    //------------------------------------------

    QC_PRIVATE_TMPLATE(aStatement)->numRows = 0;
    
    // set SYSDATE
    IDE_TEST( qtc::setDatePseudoColumn( QC_PRIVATE_TMPLATE( aStatement ) )
              != IDE_SUCCESS );

    // PROJ-1446 Host variable�� ������ ���� ����ȭ
    IDE_TEST( qmo::optimizeForHost( aStatement ) != IDE_SUCCESS );
    
    // PROJ-1502 PARTITIONED DISK TABLE
    IDE_TEST( qmnDETE::init( QC_PRIVATE_TMPLATE(aStatement),
                             aStatement->myPlan->plan )
              != IDE_SUCCESS);
    sInitialized = ID_TRUE;

    //------------------------------------------
    // DELETE�� ����
    //------------------------------------------

    do
    {
        // BUG-22287 insert �ð��� ���� �ɸ��� ��쿡 session
        // out�Ǿ insert�� ���� �� ���� ��ٸ��� ������ �ִ�.
        // session out�� Ȯ���Ѵ�.
        IDE_TEST( iduCheckSessionEvent( aStatement->mStatistics )
                  != IDE_SUCCESS );
        
        // �̸� ������Ų��. doIt�� ������ �� �ִ�.
        QC_PRIVATE_TMPLATE(aStatement)->numRows++;
        
        // delete�� plan�� �����Ѵ�.
        IDE_TEST( qmnDETE::doIt( QC_PRIVATE_TMPLATE(aStatement),
                                 aStatement->myPlan->plan,
                                 &sFlag )
                  != IDE_SUCCESS );

    } while ( ( sFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST );
    
    // DATA_NONE�� ���� ���ش�.
    QC_PRIVATE_TMPLATE(aStatement)->numRows--;
    
    //------------------------------------------
    // DELETE�� ���� ����ξ��� Cursor�� Close
    //------------------------------------------

    sInitialized = ID_FALSE;
    IDE_TEST( qmnDETE::closeCursor( QC_PRIVATE_TMPLATE(aStatement),
                                    aStatement->myPlan->plan )
              != IDE_SUCCESS );

    /* PROJ-1071 */
    IDE_TEST(qcg::joinThread(aStatement) != IDE_SUCCESS);
    
    IDE_TEST( QC_PRIVATE_TMPLATE(aStatement)->cursorMgr->closeAllCursor(aStatement->mStatistics)
              != IDE_SUCCESS );

    IDE_TEST(qcg::finishAndReleaseThread(aStatement) != IDE_SUCCESS);
    
    //------------------------------------------
    // Foreign Key Reference �˻�
    //------------------------------------------
    
    // BUG-28049
    if( ( QC_PRIVATE_TMPLATE(aStatement)->numRows > 0 )
        &&
        ( sParseTree->childConstraints != NULL ) )
    {
        // Child Table�� �����ϰ� �ִ� ���� �˻�
        IDE_TEST( qmnDETE::checkDeleteRef(
                      QC_PRIVATE_TMPLATE(aStatement),
                      aStatement->myPlan->plan )
                  != IDE_SUCCESS );
    }
    else
    {
        // (��1)
        // DELETE FROM PARENT
        // WHERE I1 IN ( SELECT I1 FROM CHILD WHERE I1 = 100 ) �� ����
        // where���� ������ in subquery keyRange�� ����Ǵ� ���ǹ��� ���,
        // child table�� ���ڵ尡 �ϳ��� ���� ���,
        // ���ǿ� �´� ���ڵ尡 �����Ƿ�
        // i1 in �� ���� keyRange�� ����� ����,
        // in subquery keyRange�� filter�ε� ó���� �� ���� ������.
        // SCAN��忡���� ���� cursor open�� ���� ���� ����̸�
        // cursor�� open���� �ʴ´�.
        // ����, ���� ���� ���� update�� �ο찡 �����Ƿ�,
        // close close�� child table�����˻�� ���� ó���� ���� �ʴ´�.
        // (��2)
        // delete from t1 where 1 != 1;
        // �� ���� ��쿡�� cursor�� open���� ����.

        // Nothing to do.
    }

    //-------------------------------------------------------
    // STATEMENT GRANULARITY TRIGGER�� ����
    //-------------------------------------------------------
    
    IDE_TEST( fireStatementTriggerOnDeleteCascade(
                  aStatement,
                  sTableRef,
                  sParseTree->childConstraints,
                  QCM_TRIGGER_AFTER )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    //fix BUG-14080
    IDE_EXCEPTION(err_plan_too_old);
    {
        IDE_SET(ideSetErrorCode(qpERR_REBUILD_QMX_TOO_OLD_PLANTREE));
    }
    IDE_EXCEPTION_END;

    QC_PRIVATE_TMPLATE(aStatement)->numRows = 0;
    
    if ( sInitialized == ID_TRUE )
    {
        (void) qmnDETE::closeCursor( QC_PRIVATE_TMPLATE(aStatement),
                                     aStatement->myPlan->plan );
    }

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qmx::executeUpdate(qcStatement *aStatement)
{
/***********************************************************************
 *
 * Description :
 *    UPDATE ... SET ... �� execution ����
 *
 * Implementation :
 *    1. SMI_TABLE_LOCK_IX lock �� ��´� => smiValidateAndLockObjects
 *    2. set SYSDATE
 *    3. �������� ����ϸ� �� ������ ã�Ƴ��´�
 *    4. qmnSCAN::init
 *    5. update �Ǵ� �÷��� ID �� ã�Ƽ� sUpdateColumnIDs �迭�� �����
 *    6. childConstraints �� ������,
 *       1. update �Ǵ� ���ڵ带 ã�Ƽ� �������� ���� �����ϴ� child ���̺���
 *          �ִ��� �����Ŀ� �� ���� ���������� Ȯ���ϰ�,
 *          ������ ������ ��ȯ�Ѵ�.
 *    7. childConstraints �� ������, �ش��ϴ� ���ڵ带 �����Ѵ�.
 *    8. ����Ǵ� �÷��� �����ϴ� parenet �� ������, �������� ���� parent ��
 *       �����ϴ��� Ȯ���ϰ�, ���� ������ ������ ��ȯ�Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "qmx::executeUpdate"

    qmmUptParseTree   * sUptParseTree;
    qmsTableRef       * sTableRef;

    idBool              sIsOld;
    qmcRowFlag          sFlag = QMC_ROW_INITIALIZE;
    idBool              sInitialized = ID_FALSE;
    scGRID              sRowGRID;
    
    //------------------------------------------
    // �⺻ ���� ����
    //------------------------------------------

    sUptParseTree = (qmmUptParseTree *) aStatement->myPlan->parseTree;

    sTableRef = sUptParseTree->updateTableRef;

    //------------------------------------------
    // �ش� Table�� ������ �´� IX lock�� ȹ����.
    //------------------------------------------

    // PROJ-1509
    // MEMORY table������
    // table�� trigger or foreign key�� �����ϸ�,
    // ���� ����/���İ��� �����ϱ� ����,
    // inplace update�� ���� �ʵ��� �ؾ� �Ѵ�.
    // table lock�� IX lock���� �⵵�� �Ѵ�.

    IDE_TEST( lockTableForUpdate(
                  aStatement,
                  sTableRef,
                  aStatement->myPlan->plan )
              != IDE_SUCCESS );

    //fix BUG-14080
    IDE_TEST( checkPlanTreeOld( aStatement, & sIsOld ) != IDE_SUCCESS );

    IDE_TEST_RAISE( sIsOld == ID_TRUE, err_plan_too_old );

    //------------------------------------------
    // STATEMENT GRANULARITY TRIGGER�� ����
    //------------------------------------------
    // To fix BUG-12622
    // before trigger
    IDE_TEST( qdnTrigger::fireTrigger( aStatement,
                                       aStatement->qmxMem,
                                       sTableRef->tableInfo,
                                       QCM_TRIGGER_ACTION_EACH_STMT,
                                       QCM_TRIGGER_BEFORE,
                                       QCM_TRIGGER_EVENT_UPDATE,
                                       sUptParseTree->uptColumnList, // UPDATE Column
                                       NULL,            /* Table Cursor */
                                       SC_NULL_GRID,    /* Row GRID */
                                       NULL,  // OLD ROW
                                       NULL,  // OLD ROW Column
                                       NULL,  // NEW ROW
                                       NULL ) // NEW ROW Column
              != IDE_SUCCESS );

    //------------------------------------------
    // UPDATE�� ���� �⺻ ���� ����
    //------------------------------------------

    QC_PRIVATE_TMPLATE(aStatement)->numRows = 0;
    
    // set SYSDATE
    IDE_TEST( qtc::setDatePseudoColumn( QC_PRIVATE_TMPLATE( aStatement ) ) != IDE_SUCCESS );

    // check session cache SEQUENCE.CURRVAL
    if (aStatement->myPlan->parseTree->currValSeqs != NULL)
    {
        IDE_TEST(findSessionSeqCaches(aStatement,
                                      aStatement->myPlan->parseTree)
                 != IDE_SUCCESS);
    }
    else
    {
        // Nothing To Do
    }

    // get SEQUENCE.NEXTVAL
    if (aStatement->myPlan->parseTree->nextValSeqs != NULL)
    {
        IDE_TEST(addSessionSeqCaches(aStatement,
                                     aStatement->myPlan->parseTree)
                 != IDE_SUCCESS);
    }
    else
    {
        // Nothing To Do
    }

    //------------------------------------------
    // UPDATE�� ���� plan tree �ʱ�ȭ
    //------------------------------------------
    
    // PROJ-1446 Host variable�� ������ ���� ����ȭ
    IDE_TEST( qmo::optimizeForHost( aStatement ) != IDE_SUCCESS );

    IDE_TEST( qmnUPTE::init( QC_PRIVATE_TMPLATE(aStatement),
                             aStatement->myPlan->plan)
             != IDE_SUCCESS);
    sInitialized = ID_TRUE;

    //------------------------------------------
    // UPDATE�� ����
    //------------------------------------------

    do
    {
        // BUG-22287 insert �ð��� ���� �ɸ��� ��쿡 session
        // out�Ǿ insert�� ���� �� ���� ��ٸ��� ������ �ִ�.
        // session out�� Ȯ���Ѵ�.
        IDE_TEST( iduCheckSessionEvent( aStatement->mStatistics )
                  != IDE_SUCCESS );
        
        // �̸� ������Ų��. doIt�� ������ �� �ִ�.
        QC_PRIVATE_TMPLATE(aStatement)->numRows++;
        
        // update�� plan�� �����Ѵ�.
        IDE_TEST( qmnUPTE::doIt( QC_PRIVATE_TMPLATE(aStatement),
                                 aStatement->myPlan->plan,
                                 &sFlag )
                  != IDE_SUCCESS );

    } while ( ( sFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST );

    // DATA_NONE�� ���� ���ش�.
    QC_PRIVATE_TMPLATE(aStatement)->numRows--;
    
    IDE_TEST( qmnUPTE::getLastUpdatedRowGRID( QC_PRIVATE_TMPLATE(aStatement),
                                              aStatement->myPlan->plan,
                                              & sRowGRID )
              != IDE_SUCCESS );
    
    //------------------------------------------
    // UPDATE cursor close
    //------------------------------------------

    sInitialized = ID_FALSE;
    IDE_TEST( qmnUPTE::closeCursor( QC_PRIVATE_TMPLATE(aStatement),
                                    aStatement->myPlan->plan )
              != IDE_SUCCESS );
    
    /* PROJ-1071 */
    IDE_TEST(qcg::joinThread(aStatement) != IDE_SUCCESS);
    
    IDE_TEST( QC_PRIVATE_TMPLATE(aStatement)->cursorMgr->closeAllCursor(aStatement->mStatistics)
              != IDE_SUCCESS );

    IDE_TEST(qcg::finishAndReleaseThread(aStatement) != IDE_SUCCESS);
    
    //------------------------------------------
    // Foreign Key Reference �˻�
    //------------------------------------------
    
    if( QC_PRIVATE_TMPLATE(aStatement)->numRows == 0 )
    {
        // (��1)
        // UPDATE PARENT SET I1 = 6
        // WHERE I1 IN ( SELECT I1 FROM CHILD WHERE I1 = 100 )
        // �� where���� ������ in subquery keyRange�� ����Ǵ� ���ǹ��� ���,
        // child table�� ���ڵ尡 �ϳ��� ���� ���,
        // ���ǿ� �´� ���ڵ尡 �����Ƿ�  i1 in �� ���� keyRange�� ����� ����,
        // in subquery keyRange�� filter�ε� ó���� �� ���� ������.
        // SCAN��忡���� ���� cursor open�� ���� ���� ����̸�
        // cursor�� open���� �ʴ´�.
        // ����, ���� ���� ���� update�� �ο찡 �����Ƿ�,
        // close close�� child table�����˻�� ���� ó���� ���� �ʴ´�.
        // (��2)
        // update t1 set i1=1 where 1 != 1;
        // �� ��쵵 cursor�� open���� ����.
    }
    else
    {
        //------------------------------------------
        // Foreign Key Referencing�� ����
        // Master Table�� �����ϴ� �� �˻�
        // To Fix PR-10592
        // Cursor�� �ùٸ� ����� ���ؼ��� Master�� ���� �˻縦 ������ �Ŀ�
        // Child Table�� ���� �˻縦 �����Ͽ��� �Ѵ�.
        //------------------------------------------

        if ( sUptParseTree->parentConstraints != NULL )
        {
            IDE_TEST( qmnUPTE::checkUpdateParentRef(
                          QC_PRIVATE_TMPLATE(aStatement),
                          aStatement->myPlan->plan )
                      != IDE_SUCCESS );
        }
        else
        {
            // Nothing To Do
        }

        //------------------------------------------
        // Child Table�� ���� Referencing �˻�
        //------------------------------------------

        if ( sUptParseTree->childConstraints != NULL )
        {
            IDE_TEST( qmnUPTE::checkUpdateChildRef(
                          QC_PRIVATE_TMPLATE(aStatement),
                          aStatement->myPlan->plan )
                      != IDE_SUCCESS );
        }
        else
        {
            // Child Table�� Referencing ������ �˻��� �ʿ䰡 ���� ���
            // Nothing To Do
        }
    }

    //------------------------------------------
    // PROJ-1359 Trigger
    // STATEMENT GRANULARITY TRIGGER�� ����
    //------------------------------------------
    IDE_TEST( qdnTrigger::fireTrigger( aStatement,
                                       aStatement->qmxMem,
                                       sTableRef->tableInfo,
                                       QCM_TRIGGER_ACTION_EACH_STMT,
                                       QCM_TRIGGER_AFTER,
                                       QCM_TRIGGER_EVENT_UPDATE,
                                       sUptParseTree->uptColumnList, // UPDATE Column
                                       NULL,            /* Table Cursor */
                                       SC_NULL_GRID,    /* Row GRID */
                                       NULL,  // OLD ROW
                                       NULL,  // OLD ROW Column
                                       NULL,  // NEW ROW
                                       NULL ) // NEW ROW Column
              != IDE_SUCCESS );

    // BUG-38129
    qcg::setLastModifiedRowGRID( aStatement, sRowGRID );
    
    return IDE_SUCCESS;

    //fix BUG-14080
    IDE_EXCEPTION(err_plan_too_old);
    {
        IDE_SET(ideSetErrorCode(qpERR_REBUILD_QMX_TOO_OLD_PLANTREE));
    }
    IDE_EXCEPTION_END;

    QC_PRIVATE_TMPLATE(aStatement)->numRows = 0;

    if ( sInitialized == ID_TRUE )
    {
        (void) qmnUPTE::closeCursor( QC_PRIVATE_TMPLATE(aStatement),
                                     aStatement->myPlan->plan );
    }
    
    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qmx::executeLockTable(qcStatement *aStatement)
{
    qmmLockParseTree     * sParseTree;
    qcmPartitionInfoList * sPartInfoList  = NULL;
    idBool                 sIsReadOnly    = ID_FALSE;
    smiTableLockMode       sTableLockMode = SMI_TABLE_NLOCK;

    sParseTree = (qmmLockParseTree*) aStatement->myPlan->parseTree;

    /* BUG-42853 LOCK TABLE�� UNTIL NEXT DDL ��� �߰� */
    if ( sParseTree->untilNextDDL == ID_TRUE )
    {
        IDE_TEST( ( QC_SMI_STMT(aStatement) )->getTrans()->isReadOnly( &sIsReadOnly )
                  != IDE_SUCCESS );

        /* �����͸� �����ϴ� DML�� �Բ� ����� �� ����. */
        IDE_TEST_RAISE( sIsReadOnly != ID_TRUE,
                        ERR_CANNOT_LOCK_TABLE_UNTIL_NEXT_DDL_WITH_DML );

        /* �� Transaction�� �� ���� ������ �� �ִ�. */
        IDE_TEST_RAISE( QCG_GET_SESSION_LOCK_TABLE_UNTIL_NEXT_DDL( aStatement ) == ID_TRUE,
                        ERR_CANNOT_LOCK_TABLE_UNTIL_NEXT_DDL_MORE_THAN_ONCE );
    }
    else
    {
        /* Nothing to do */
    }

    /* BUG-46273 Lock Partition */
    if ( QC_IS_NULL_NAME( sParseTree->partitionName ) == ID_FALSE )
    {
        switch ( sParseTree->tableLockMode )
        {
            case SMI_TABLE_LOCK_S :
                /* fall through */
            case SMI_TABLE_LOCK_IS :
                sTableLockMode = SMI_TABLE_LOCK_IS;
                break;

            case SMI_TABLE_LOCK_X :
                /* fall through */
            case SMI_TABLE_LOCK_IX :
                /* fall through */
            case SMI_TABLE_LOCK_SIX :
                sTableLockMode = SMI_TABLE_LOCK_IX;
                break;

            default :
                break;
        }

        IDE_TEST( smiValidateAndLockObjects( (QC_SMI_STMT(aStatement))->getTrans(),
                                             sParseTree->tableHandle,
                                             sParseTree->tableSCN,
                                             SMI_TBSLV_DDL_DML, // TBS Validation �ɼ�
                                             sTableLockMode,
                                             sParseTree->lockWaitMicroSec,
                                             ID_TRUE ) // BUG-28752 ����� Lock�� ������ Lock�� �����մϴ�.
                  != IDE_SUCCESS );

        IDE_TEST( smiValidateAndLockObjects( (QC_SMI_STMT(aStatement))->getTrans(),
                                             sParseTree->partitionHandle,
                                             sParseTree->partitionSCN,
                                             SMI_TBSLV_DDL_DML, // TBS Validation �ɼ�
                                             sParseTree->tableLockMode,
                                             sParseTree->lockWaitMicroSec,
                                             ID_TRUE ) // BUG-28752 ����� Lock�� ������ Lock�� �����մϴ�.
                  != IDE_SUCCESS );
    }
    else
    {
        // To fix PR-4159
        IDE_TEST( smiValidateAndLockObjects( (QC_SMI_STMT(aStatement))->getTrans(),
                                             sParseTree->tableHandle,
                                             sParseTree->tableSCN,
                                             SMI_TBSLV_DDL_DML, // TBS Validation �ɼ�
                                             sParseTree->tableLockMode,
                                             sParseTree->lockWaitMicroSec,
                                             ID_TRUE ) // BUG-28752 ����� Lock�� ������ Lock�� �����մϴ�.
                  != IDE_SUCCESS );

        // PROJ-1502 PARTITIONED DISK TABLE
        if ( sParseTree->tableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
        {
            IDE_TEST( qcmPartition::getPartitionInfoList(
                          aStatement,
                          QC_SMI_STMT( aStatement ),
                          aStatement->qmxMem,
                          sParseTree->tableInfo->tableID,
                          & sPartInfoList )
                      != IDE_SUCCESS );

            // ��Ƽ�ǵ� ���̺�� ���� ������ LOCK�� ��´�.
            for ( ;
                  sPartInfoList != NULL;
                  sPartInfoList = sPartInfoList->next )
            {
                IDE_TEST( smiValidateAndLockObjects( (QC_SMI_STMT(aStatement))->getTrans(),
                                                     sPartInfoList->partHandle,
                                                     sPartInfoList->partSCN,
                                                     SMI_TBSLV_DDL_DML, // TBS Validation �ɼ�
                                                     sParseTree->tableLockMode,
                                                     sParseTree->lockWaitMicroSec,
                                                     ID_TRUE ) // BUG-28752 ����� Lock�� ������ Lock�� �����մϴ�.
                          != IDE_SUCCESS );
            }
        }
        else
        {
            // Nothing to do
        }
    }

    /* BUG-42853 LOCK TABLE�� UNTIL NEXT DDL ��� �߰� */
    if ( sParseTree->untilNextDDL == ID_TRUE )
    {
        /* Session�� ���� ���� �����Ѵ�. */
        QCG_SET_SESSION_LOCK_TABLE_UNTIL_NEXT_DDL( aStatement,
                                                   ID_TRUE );
        QCG_SET_SESSION_TABLE_ID_OF_LOCK_TABLE_UNTIL_NEXT_DDL( aStatement,
                                                               sParseTree->tableInfo->tableID );
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_CANNOT_LOCK_TABLE_UNTIL_NEXT_DDL_WITH_DML )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMX_CANNOT_LOCK_TABLE_UNTIL_NEXT_DDL_WITH_DML ) );
    }
    IDE_EXCEPTION( ERR_CANNOT_LOCK_TABLE_UNTIL_NEXT_DDL_MORE_THAN_ONCE );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMX_CANNOT_LOCK_TABLE_UNTIL_NEXT_DDL_MORE_THAN_ONCE ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmx::executeSelect( qcStatement * aStatement )
{
/***********************************************************************
 *
 * Description :
 *    SELECT ������ ������.
 *
 * Implementation :
 *    PROJ-1350
 *    �ش� Plan Tree�� ��ȿ������ �˻�
 *
 ***********************************************************************/
    idBool   sIsOld;

    // PROJ-1350 Plan Tree �� ������ ����� ��� ������ ���������� �˻�
    IDE_TEST( checkPlanTreeOld( aStatement, & sIsOld ) != IDE_SUCCESS );

    IDE_TEST_RAISE( sIsOld == ID_TRUE, err_plan_too_old );

    // set SYSDATE
    IDE_TEST( qtc::setDatePseudoColumn( QC_PRIVATE_TMPLATE( aStatement ) ) != IDE_SUCCESS );

    // check session cache SEQUENCE.CURRVAL
    if (aStatement->myPlan->parseTree->currValSeqs != NULL)
    {
        IDE_TEST(findSessionSeqCaches(aStatement,
                                      aStatement->myPlan->parseTree)
                 != IDE_SUCCESS);
    }

    // PROJ-1446 Host variable�� ������ ���� ����ȭ
    IDE_TEST( qmo::optimizeForHost( aStatement ) != IDE_SUCCESS );

    IDE_TEST(aStatement->myPlan->plan->init( QC_PRIVATE_TMPLATE(aStatement),
                                             aStatement->myPlan->plan)
             != IDE_SUCCESS);

    return IDE_SUCCESS;

    // PROJ-1350
    // REBUILD error�� �����Ͽ� �������� Plan Tree�� recompile�ϵ��� �Ѵ�.
    IDE_EXCEPTION(err_plan_too_old);
    {
        IDE_SET(ideSetErrorCode(qpERR_REBUILD_QMX_TOO_OLD_PLANTREE));
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmx::makeSmiValueWithValue(qcmColumn     * aColumn,
                                  qmmValueNode  * aValue,
                                  qcTemplate    * aTmplate,
                                  qcmTableInfo  * aTableInfo, /* Criterion for smiValue */
                                  smiValue      * aInsertedRow,
                                  qmxLobInfo    * aLobInfo )
{
/***********************************************************************
 *
 * Description :
 *    smiValue�� �����Ѵ�.
 *
 * Implementation :
 *    PROJ-1362
 *    lob�÷��� �����Ҷ� ������ ��
 *    1. Xlob�� lob-value�� lob-locator �� ���� Ÿ���� �ִ�.
 *    2. calculate�� ��ġ�� lob-locator�� open�ȴ�.
 *       �� �Լ������� locator�� lobInfo�� �ݵ�� �����ؾ� �ϰ�
 *       close�� ȣ���� �Լ��� å������.
 *    3. Xlob�� null�϶� smiValue�� {0,NULL}�̾�� �Ѵ�.
 *       mtdXlobNull�� {0,""}�̱� ������ NULL�� �����ؾ� �Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "qmx::makeSmiValueWithValue"

    qcmColumn         * sColumn           = NULL;
    qmmValueNode      * sValNode          = NULL;
    mtcColumn         * sValueColumn      = NULL;
    UInt                sArguCount        = 0;
    mtvConvert        * sConvert          = NULL;
    SInt                sColumnOrder      = 0;
    void              * sCanonizedValue   = NULL;
    void              * sValue            = NULL;
    mtcStack          * sStack            = NULL;
    SInt                sBindId;
    idBool              sIsOutBind;
    mtcColumn         * sMtcColumn;
    mtcEncryptInfo      sEncryptInfo;
    UInt                sStoringSize      = 0;
    void              * sStoringValue;
    SLong               sLobLen;
    idBool              sIsNullLob;

    sStack = aTmplate->tmplate.stack;

    for (sColumn = aColumn, sValNode = aValue;
         sColumn != NULL;
         sColumn = sColumn->next, sValNode = sValNode->next )
    {
        sColumnOrder = sColumn->basicInfo->column.id & SMI_COLUMN_ID_MASK;

        if (sValNode->timestamp == ID_TRUE)
        {
            // set gettimeofday()
            IDE_TEST(aTmplate->stmt->qmxMem->alloc(
                         MTD_BYTE_TYPE_STRUCT_SIZE(QC_BYTE_PRECISION_FOR_TIMESTAMP),
                         &sValue)
                     != IDE_SUCCESS);
            ((mtdByteType*)sValue)->length = QC_BYTE_PRECISION_FOR_TIMESTAMP;

            IDE_TEST( setTimeStamp( ((mtdByteType*)sValue)->value ) != IDE_SUCCESS );

            IDE_TEST( qdbCommon::mtdValue2StoringValue(
                          aTableInfo->columns[sColumnOrder].basicInfo,
                          sColumn->basicInfo,
                          sValue,
                          &sStoringValue )
                      !=IDE_SUCCESS );
            aInsertedRow[sColumnOrder].value  = sStoringValue;

            IDE_TEST( qdbCommon::storingSize( aTableInfo->columns[sColumnOrder].basicInfo,
                                              sColumn->basicInfo,
                                              sValue,
                                              &sStoringSize )
                      != IDE_SUCCESS );
            aInsertedRow[sColumnOrder].length = sStoringSize;
        }
        else
        {
            if (sValNode->value != NULL)
            {
                IDE_TEST(qtc::calculate(sValNode->value, aTmplate) != IDE_SUCCESS);

                sValueColumn = sStack->column;
                sValue = sStack->value;

                if ( (sValueColumn->module->id == MTD_BLOB_LOCATOR_ID) ||
                     (sValueColumn->module->id == MTD_CLOB_LOCATOR_ID) )
                {
                    sIsOutBind = ID_FALSE;

                    if ( (sValNode->value->node.lflag & MTC_NODE_BIND_MASK)
                         == MTC_NODE_BIND_EXIST )
                    {
                        IDE_DASSERT( sValNode->value->node.table ==
                                     aTmplate->tmplate.variableRow );

                        sBindId = sValNode->value->node.column;

                        if ( aTmplate->stmt->pBindParam[sBindId].param.inoutType
                             == CMP_DB_PARAM_OUTPUT )
                        {
                            sIsOutBind = ID_TRUE;
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

                    if ( sIsOutBind == ID_TRUE )
                    {
                        // bind�����̴�.
                        IDE_TEST( addLobInfoForOutBind(
                                      aLobInfo,
                                      & aTableInfo->columns[sColumnOrder].basicInfo->column,
                                      sValNode->value->node.column)
                                  != IDE_SUCCESS );

                        // NOT NULL Constraint
                        // Not Null constraint�� ���� ��� locator�� outbind�� ����� �� ����.
                        IDE_TEST_RAISE(
                            ( aTableInfo->columns[sColumnOrder].basicInfo->flag &
                              MTC_COLUMN_NOTNULL_MASK )
                            == MTC_COLUMN_NOTNULL_TRUE,
                            ERR_NOT_ALLOW_NULL );
                    }
                    else
                    {
                        IDE_TEST( addLobInfoForCopy(
                                      aLobInfo,
                                      & aTableInfo->columns[sColumnOrder].basicInfo->column,
                                      *(smLobLocator*)sValue,
                                      sColumnOrder)
                                  != IDE_SUCCESS );

                        // NOT NULL Constraint
                        if ( ( aTableInfo->columns[sColumnOrder].basicInfo->flag &
                               MTC_COLUMN_NOTNULL_MASK )
                             == MTC_COLUMN_NOTNULL_TRUE )
                        {
                            if ( *(smLobLocator*)sValue != MTD_LOCATOR_NULL )
                            {
                                IDE_TEST( smiLob::getLength( QC_STATISTICS(aTmplate->stmt),
                                                             *(smLobLocator*)sValue,
                                                             & sLobLen,
                                                             & sIsNullLob )
                                          != IDE_SUCCESS );
                            }
                            else
                            {
                                sIsNullLob = ID_TRUE;
                            }

                            IDE_TEST_RAISE( sIsNullLob == ID_TRUE, ERR_NOT_ALLOW_NULL );
                        }
                        else
                        {
                            // Nothing to do.
                        }
                    }

                    // NULL�� �Ҵ��Ѵ�.
                    aInsertedRow[sColumnOrder].value  = NULL;
                    aInsertedRow[sColumnOrder].length = 0;
                }
                else
                {
                    // check conversion
                    /* PROJ-1361 : data module �� language module �и����� */
                    if (sColumn->basicInfo->type.dataTypeId ==
                        sValueColumn->type.dataTypeId )
                    {
                        // same type
                        sValue = sStack->value;
                    }
                    else
                    {
                        // convert
                        sArguCount =
                            sValueColumn->flag & MTC_COLUMN_ARGUMENT_COUNT_MASK;

                        IDE_TEST(
                            mtv::estimateConvert4Server(
                                aTmplate->stmt->qmxMem,
                                &sConvert,
                                sColumn->basicInfo->type, // aDestinationType
                                sValueColumn->type,       // aSourceType
                                sArguCount,               // aSourceArgument
                                sValueColumn->precision,  // aSourcePrecision
                                sValueColumn->scale,      // aSourceScale
                                &aTmplate->tmplate)       // mtcTemplate* : for passing session dateFormat
                            != IDE_SUCCESS);

                        // source value pointer
                        sConvert->stack[sConvert->count].value = sStack->value;
                        // destination value pointer
                        sValueColumn = sConvert->stack->column;
                        sValue       = sConvert->stack->value;

                        IDE_TEST(mtv::executeConvert( sConvert, &aTmplate->tmplate ) != IDE_SUCCESS);
                    }

                    // PROJ-2002 Column Security
                    // insert select, move ������ subquery�� ����ϴ� ��� ��ȣ Ÿ����
                    // �� �� ����. �� typed literal�� ����ϴ� ���� default policy��
                    // ��ȣ Ÿ���� �� �� �ִ�.
                    //
                    // insert into t1 select i1 from t2;
                    // insert into t1 select echar'a' from t2;
                    //
                    sMtcColumn = QTC_TMPL_COLUMN( aTmplate, sValNode->value );

                    IDE_TEST_RAISE( ( (sMtcColumn->module->flag & MTD_ENCRYPT_TYPE_MASK)
                                      == MTD_ENCRYPT_TYPE_TRUE ) &&
                                    ( QCS_IS_DEFAULT_POLICY( sMtcColumn ) != ID_TRUE ),
                                    ERR_INVALID_ENCRYPTED_COLUMN );

                    // PROJ-2002 Column Security
                    if ( ( sColumn->basicInfo->module->flag & MTD_ENCRYPT_TYPE_MASK )
                         == MTD_ENCRYPT_TYPE_TRUE )
                    {
                        IDE_TEST( qcsModule::getEncryptInfo( aTmplate->stmt,
                                                             aTableInfo,
                                                             sColumnOrder,
                                                             & sEncryptInfo )
                                  != IDE_SUCCESS );
                    }
                    else
                    {
                        // Nothing to do.
                    }

                    // canonize
                    if ( ( sColumn->basicInfo->module->flag & MTD_CANON_MASK )
                         == MTD_CANON_NEED )
                    {
                        sCanonizedValue = sValue;

                        IDE_TEST_RAISE( sColumn->basicInfo->module->canonize(
                                            sColumn->basicInfo,
                                            & sCanonizedValue,           // canonized value
                                            & sEncryptInfo,
                                            sValueColumn,
                                            sValue,                     // original value
                                            NULL,
                                            & aTmplate->tmplate )
                                        != IDE_SUCCESS, ERR_INVALID_CANONIZE );

                        sValue = sCanonizedValue;
                    }
                    else if ( ( sColumn->basicInfo->module->flag & MTD_CANON_MASK )
                              == MTD_CANON_NEED_WITH_ALLOCATION )
                    {
                        IDU_FIT_POINT("qmx::makeSmiValueWithValue::malloc1");
                        IDE_TEST( aTmplate->stmt->qmxMem->alloc(
                                      sColumn->basicInfo->column.size,
                                      (void**)&sCanonizedValue)
                                  != IDE_SUCCESS );

                        IDE_TEST_RAISE( sColumn->basicInfo->module->canonize(
                                            sColumn->basicInfo,
                                            & sCanonizedValue,           // canonized value
                                            & sEncryptInfo,
                                            sValueColumn,
                                            sValue,                      // original value
                                            NULL,
                                            & aTmplate->tmplate )
                                        != IDE_SUCCESS, ERR_INVALID_CANONIZE );

                        sValue = sCanonizedValue;
                    }
                    else
                    {
                        // Nothing to do.
                    }

                    if ( ( aTableInfo->columns[sColumnOrder].basicInfo->column.flag
                           & SMI_COLUMN_TYPE_MASK )
                         == SMI_COLUMN_TYPE_LOB )
                    {
                        if( sColumn->basicInfo->module->isNull( sColumn->basicInfo,
                                                                sValue )
                            != ID_TRUE )
                        {
                            // PROJ-1362
                            IDE_DASSERT( sColumn->basicInfo->module->id
                                         == aTableInfo->columns[sColumnOrder].basicInfo->module->id );
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

                    // PROJ-1705
                    IDE_TEST( qdbCommon::mtdValue2StoringValue(
                                  aTableInfo->columns[sColumnOrder].basicInfo,
                                  sColumn->basicInfo,
                                  sValue,
                                  &sStoringValue )
                              != IDE_SUCCESS );
                    aInsertedRow[sColumnOrder].value  = sStoringValue;

                    IDE_TEST( qdbCommon::storingSize( aTableInfo->columns[sColumnOrder].basicInfo,
                                                      sColumn->basicInfo,
                                                      sValue,
                                                      &sStoringSize )
                              != IDE_SUCCESS );
                    aInsertedRow[sColumnOrder].length = sStoringSize;
                }
            }
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NOT_ALLOW_NULL )
    {
        /* BUG-45680 insert ����� not null column�� ���� �����޽��� ������ column ���� ���. */
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMX_NOT_NULL_CONSTRAINT,
                                  "",
                                  "" ) );
    }
    IDE_EXCEPTION( ERR_INVALID_ENCRYPTED_COLUMN );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QTC_INVALID_ENCRYPTED_COLUMN ) );
    }
    IDE_EXCEPTION( ERR_INVALID_CANONIZE );
    {
        if ( ideGetErrorCode() == mtERR_ABORT_INVALID_LENGTH )
        {
            IDE_CLEAR();
            IDE_SET( ideSetErrorCode( mtERR_ABORT_INVALID_LENGTH_COLUMN,
                                      aTableInfo->columns[sColumnOrder].name ) );
        }
        else
        {
            // nothing to do
        }
    }
    IDE_EXCEPTION_END;
                
    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qmx::makeSmiValueWithValue( qcTemplate    * aTemplate,
                                   qcmTableInfo  * aTableInfo, /* Criterion for smiValue */
                                   UInt            aCanonizedTuple,
                                   qmmValueNode  * aValue,
                                   void          * aQueueMsgIDSeq,
                                   smiValue      * aInsertedRow,
                                   qmxLobInfo    * aLobInfo )
{
#define IDE_FN "qmx::makeSmiValueWithValue"

    mtcStack          * sStack;
    UInt                sColumnCount;
    mtcColumn         * sColumns;
    UInt                sIterator;
    qmmValueNode      * sValueNode;
    void              * sValue;
    SInt                sColumnOrder;
    SInt                sBindId;
    idBool              sIsOutBind;
    mtcColumn         * sMtcColumn;
    mtcEncryptInfo      sEncryptInfo;
    UInt                sStoringSize = 0;
    void              * sStoringValue;
    SLong               sLobLen;
    idBool              sIsNullLob;

    sStack         = aTemplate->tmplate.stack;
    sColumnCount   =
        aTemplate->tmplate.rows[aCanonizedTuple].columnCount;
    sColumns       =
        aTemplate->tmplate.rows[aCanonizedTuple].columns;

    for( sIterator = 0,
             sValueNode = aValue;
         sIterator < sColumnCount;
         sIterator++,
             sValueNode = sValueNode->next,
             sColumns++ )
    {
        sColumnOrder = sColumns->column.id & SMI_COLUMN_ID_MASK;

        if (sValueNode->timestamp == ID_TRUE)
        {
            // set gettimeofday()
            sValue = (void*)
                ((UChar*) aTemplate->tmplate.rows[aCanonizedTuple].row
                 + sColumns->column.offset);

            ((mtdByteType*)sValue)->length = QC_BYTE_PRECISION_FOR_TIMESTAMP;

            IDE_TEST( setTimeStamp( ((mtdByteType*)sValue)->value ) != IDE_SUCCESS );

            IDE_TEST( qdbCommon::mtdValue2StoringValue(
                          aTableInfo->columns[sColumnOrder].basicInfo,
                          sColumns,
                          sValue,
                          &sStoringValue )
                      != IDE_SUCCESS );
            aInsertedRow[sIterator].value  = sStoringValue;

            IDE_TEST( qdbCommon::storingSize(
                          aTableInfo->columns[sColumnOrder].basicInfo,
                          sColumns,
                          sValue,
                          &sStoringSize )
                      != IDE_SUCCESS );
            aInsertedRow[sIterator].length = sStoringSize;
        }
        // Proj-1360 Queue
        else if (sValueNode->msgID == ID_TRUE)
        {
            // queue�� messageIDĮ���� bigint type�̸�,
            // �ش� Į���� ���� ���� sequence�� �о �����Ѵ�.
            IDU_FIT_POINT("qmx::makeSmiValueWithValue::malloc3");
            IDE_TEST(aTemplate->stmt->qmxMem->alloc(
                         ID_SIZEOF(mtdBigintType),
                         &sValue)
                     != IDE_SUCCESS);

            IDE_TEST( smiTable::readSequence( QC_SMI_STMT(aTemplate->stmt),
                                              aQueueMsgIDSeq,
                                              SMI_SEQUENCE_NEXT,
                                              (mtdBigintType*)sValue,
                                              NULL)
                      != IDE_SUCCESS);

            IDE_TEST( qdbCommon::mtdValue2StoringValue(
                          aTableInfo->columns[sColumnOrder].basicInfo,
                          sColumns,
                          sValue,
                          &sStoringValue )
                      != IDE_SUCCESS );
            aInsertedRow[sIterator].value  = sStoringValue;

            IDE_TEST( qdbCommon::storingSize(
                          aTableInfo->columns[sColumnOrder].basicInfo,
                          sColumns,
                          sValue,
                          &sStoringSize )
                      != IDE_SUCCESS );
            aInsertedRow[sIterator].length = sStoringSize;
        }
        else
        {
            if (sValueNode->value != NULL)
            {
                IDE_TEST( qtc::calculate( sValueNode->value, aTemplate )
                          != IDE_SUCCESS );

                if ( (sStack->column->module->id == MTD_BLOB_LOCATOR_ID) ||
                     (sStack->column->module->id == MTD_CLOB_LOCATOR_ID) )
                {
                    sIsOutBind = ID_FALSE;

                    if ( (sValueNode->value->node.lflag & MTC_NODE_BIND_MASK)
                         == MTC_NODE_BIND_EXIST )
                    {
                        IDE_DASSERT( sValueNode->value->node.table ==
                                     aTemplate->tmplate.variableRow );

                        sBindId = sValueNode->value->node.column;

                        if ( aTemplate->stmt->pBindParam[sBindId].param.inoutType
                             == CMP_DB_PARAM_OUTPUT )
                        {
                            sIsOutBind = ID_TRUE;
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

                    if ( sIsOutBind == ID_TRUE )
                    {
                        // bind�����̴�.
                        IDE_TEST( addLobInfoForOutBind(
                                      aLobInfo,
                                      & aTableInfo->
                                      columns[sColumnOrder].basicInfo->column,
                                      sValueNode->value->node.column)
                                  != IDE_SUCCESS );

                        // NOT NULL Constraint
                        // Not Null constraint�� ���� ��� locator�� outbind�� ����� �� ����.
                        IDE_TEST_RAISE(
                            ( aTableInfo->columns[sColumnOrder].basicInfo->flag &
                              MTC_COLUMN_NOTNULL_MASK )
                            == MTC_COLUMN_NOTNULL_TRUE,
                            ERR_NOT_ALLOW_NULL );
                    }
                    else
                    {
                        IDE_TEST( addLobInfoForCopy(
                                      aLobInfo,
                                      & aTableInfo->
                                      columns[sColumnOrder].basicInfo->column,
                                      *(smLobLocator*)sStack->value,
                                      sColumnOrder)
                                  != IDE_SUCCESS );

                        // NOT NULL Constraint
                        if ( ( aTableInfo->columns[sColumnOrder].basicInfo->flag &
                               MTC_COLUMN_NOTNULL_MASK )
                             == MTC_COLUMN_NOTNULL_TRUE )
                        {
                            if ( *(smLobLocator*)sStack->value != MTD_LOCATOR_NULL )
                            {
                                IDE_TEST( smiLob::getLength( QC_STATISTICS(aTemplate->stmt),
                                                             *(smLobLocator*)sStack->value,
                                                             & sLobLen,
                                                             & sIsNullLob )
                                          != IDE_SUCCESS );
                            }
                            else
                            {
                                sIsNullLob = ID_TRUE;
                            }

                            IDE_TEST_RAISE( sIsNullLob == ID_TRUE,
                                            ERR_NOT_ALLOW_NULL );
                        }
                        else
                        {
                            // Nothing to do.
                        }
                    }

                    // NULL�� �Ҵ��Ѵ�.
                    aInsertedRow[sIterator].value  = NULL;
                    aInsertedRow[sIterator].length = 0;
                }
                else
                {
                    sValue = (void*)
                        ( (UChar*) aTemplate->tmplate.rows[
                            aCanonizedTuple].row
                          + sColumns->column.offset);

                    sColumnOrder = sColumns->column.id & SMI_COLUMN_ID_MASK;

                    // PROJ-2002 Column Security
                    // insert value ������ subquery�� ����ϴ� ���� ��ȣ Ÿ����
                    // �� �� ����. �� typed literal�� ����ϴ� ��� default policy��
                    // ��ȣ Ÿ���� �� ���� �ִ�.
                    sMtcColumn = QTC_TMPL_COLUMN( aTemplate, sValueNode->value );

                    IDE_TEST_RAISE( ( (sMtcColumn->module->flag & MTD_ENCRYPT_TYPE_MASK)
                                      == MTD_ENCRYPT_TYPE_TRUE ) &&
                                    ( QCS_IS_DEFAULT_POLICY( sMtcColumn ) != ID_TRUE ),
                                    ERR_INVALID_ENCRYPTED_COLUMN );

                    // PROJ-2002 Column Security
                    if ( ( sColumns->module->flag & MTD_ENCRYPT_TYPE_MASK )
                         == MTD_ENCRYPT_TYPE_TRUE )
                    {
                        IDE_TEST( qcsModule::getEncryptInfo( aTemplate->stmt,
                                                             aTableInfo,
                                                             sColumnOrder,
                                                             & sEncryptInfo )
                                  != IDE_SUCCESS );
                    }
                    else
                    {
                        // Nothing to do.
                    }

                    IDE_TEST_RAISE( sColumns->module->canonize( sColumns,
                                                                & sValue,
                                                                & sEncryptInfo,
                                                                sStack->column,
                                                                sStack->value,
                                                                NULL,
                                                                & aTemplate->tmplate )
                                    != IDE_SUCCESS, ERR_INVALID_CANONIZE );

                    IDE_TEST( qdbCommon::mtdValue2StoringValue(
                                  aTableInfo->columns[sColumnOrder].basicInfo,
                                  sColumns,
                                  sValue,
                                  &sStoringValue )
                              != IDE_SUCCESS );
                    aInsertedRow[sIterator].value  = sStoringValue;

                    IDE_TEST( qdbCommon::storingSize(
                                  aTableInfo->columns[sColumnOrder].basicInfo,
                                  sColumns,
                                  sValue,
                                  &sStoringSize )
                              != IDE_SUCCESS );
                    aInsertedRow[sIterator].length = sStoringSize;
                }
            }
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NOT_ALLOW_NULL );
    {
        /* BUG-45680 insert ����� not null column�� ���� �����޽��� ������ column ���� ���. */
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMX_NOT_NULL_CONSTRAINT,
                                  "",
                                  "" ) );
    }
    IDE_EXCEPTION( ERR_INVALID_ENCRYPTED_COLUMN );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QTC_INVALID_ENCRYPTED_COLUMN ) );
    }
    IDE_EXCEPTION( ERR_INVALID_CANONIZE );
    {
        if ( ideGetErrorCode() == mtERR_ABORT_INVALID_LENGTH )
        {
            IDE_CLEAR();
            IDE_SET( ideSetErrorCode( mtERR_ABORT_INVALID_LENGTH_COLUMN,
                                      aTableInfo->columns[sColumnOrder].name ) );
        }
        else
        {
            // nothing to do
        }
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qmx::makeSmiValueWithResult( qcmColumn     * aColumn,
                                    qcTemplate    * aTemplate,
                                    qcmTableInfo  * aTableInfo, /* Criterion for smiValue */
                                    smiValue      * aInsertedRow,
                                    qmxLobInfo    * aLobInfo )
{
    mtcStack          * sStack;
    sStack = aTemplate->tmplate.stack;

    IDE_TEST( makeSmiValueWithStack(aColumn,
                                    aTemplate,
                                    sStack,
                                    aTableInfo,
                                    aInsertedRow,
                                    aLobInfo )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmx::makeSmiValueWithStack( qcmColumn     * aColumn,
                                   qcTemplate    * aTemplate,
                                   mtcStack      * aStack,
                                   qcmTableInfo  * aTableInfo, /* Criterion for smiValue */
                                   smiValue      * aInsertedRow,
                                   qmxLobInfo    * aLobInfo )
{
    qcmColumn         * sColumn;
    mtcColumn         * sStoringColumn = NULL;
    mtcColumn         * sValueColumn;
    UInt                sArguCount;
    mtvConvert        * sConvert;
    SInt                sColumnOrder;
    void              * sCanonizedValue;
    void              * sValue;
    SInt                i;
    mtcStack          * sStack;
    mtcEncryptInfo      sEncryptInfo;
    UInt                sStoringSize = 0;
    void              * sStoringValue;
    SLong               sLobLen;
    idBool              sIsNullLob;

    sStack = aStack;

    for (sColumn = aColumn, i = 0;
         sColumn != NULL;
         sColumn = sColumn->next, i++, sStack++)
    {
        sValueColumn = sStack->column;
        sValue       = sStack->value;

        sColumnOrder = (sColumn->basicInfo->column.id & SMI_COLUMN_ID_MASK);

        /* PROJ-2464 hybrid partitioned table ���� */
        sStoringColumn = aTableInfo->columns[sColumnOrder].basicInfo;

        if ( ( (sValueColumn->module->id == MTD_BLOB_LOCATOR_ID) &&
               ( (sColumn->basicInfo->module->id == MTD_BLOB_LOCATOR_ID) ||
                 (sColumn->basicInfo->module->id == MTD_BLOB_ID) ) )
             ||
             ( (sValueColumn->module->id == MTD_CLOB_LOCATOR_ID) &&
               ( (sColumn->basicInfo->module->id == MTD_CLOB_LOCATOR_ID) ||
                 (sColumn->basicInfo->module->id == MTD_CLOB_ID) ) ) )
        {
            // PROJ-1362
            // select�� ����� ��ȿ���� ���� Lob Locator�� ���� �� ����.
            IDE_TEST( addLobInfoForCopy(
                          aLobInfo,
                          & sStoringColumn->column,
                          *(smLobLocator*)sValue,
                          sColumnOrder)
                      != IDE_SUCCESS );

            // NOT NULL Constraint
            if ( ( sColumn->basicInfo->flag & MTC_COLUMN_NOTNULL_MASK )
                                           == MTC_COLUMN_NOTNULL_TRUE )
            {
                if ( *(smLobLocator*)sValue != MTD_LOCATOR_NULL )
                {
                    IDE_TEST( smiLob::getLength( QC_STATISTICS(aTemplate->stmt),
                                                 *(smLobLocator*)sValue,
                                                 & sLobLen,
                                                 & sIsNullLob )
                              != IDE_SUCCESS );
                }
                else
                {
                    sIsNullLob = ID_TRUE;
                }

                IDE_TEST_RAISE( sIsNullLob == ID_TRUE,
                                ERR_NOT_ALLOW_NULL );
            }
            else
            {
                // Nothing to do.
            }

            // NULL�� �Ҵ��Ѵ�.
            aInsertedRow[sColumnOrder].value  = NULL;
            aInsertedRow[sColumnOrder].length = 0;
        }
        else
        {
            // check conversion
            /* PROJ-1361 : data module�� language module �и����� */
            if (sColumn->basicInfo->type.dataTypeId ==
                sValueColumn->type.dataTypeId )
            {
                /* Nothing to do */
            }
            else
            {
                // convert
                sArguCount = sValueColumn->flag & MTC_COLUMN_ARGUMENT_COUNT_MASK;
                
                IDE_TEST(
                    mtv::estimateConvert4Server(
                        aTemplate->stmt->qmxMem,
                        &sConvert,
                        sColumn->basicInfo->type,// aDestinationType
                        sValueColumn->type,      // aSourceType
                        sArguCount,              // aSourceArgument
                        sValueColumn->precision, // aSourcePrecision
                        sValueColumn->scale,     // aSourceScale
                        &aTemplate->tmplate)     // mtcTemplate* : for passing session dateFormat
                    != IDE_SUCCESS);
                
                // source value pointer
                sConvert->stack[sConvert->count].value =
                    sStack->value;
                
                // destination value pointer
                sValueColumn = sConvert->stack[0].column;
                sValue       = sConvert->stack[0].value;
                
                IDE_TEST(mtv::executeConvert( sConvert, &aTemplate->tmplate )
                         != IDE_SUCCESS);
            }
            
            // PROJ-2002 Column Security
            if ( ( sColumn->basicInfo->module->flag & MTD_ENCRYPT_TYPE_MASK )
                 == MTD_ENCRYPT_TYPE_TRUE )
            {
                IDE_TEST( qcsModule::getEncryptInfo( aTemplate->stmt,
                                                     aTableInfo,
                                                     sColumnOrder,
                                                     & sEncryptInfo )
                          != IDE_SUCCESS );
            }
            else
            {
                // Nothing to do.
            }

            // canonize
            if ( ( sColumn->basicInfo->module->flag & MTD_CANON_MASK )
                 == MTD_CANON_NEED )
            {
                sCanonizedValue = sValue;

                IDE_TEST_RAISE( sColumn->basicInfo->module->canonize(
                                    sColumn->basicInfo,
                                    & sCanonizedValue,           // canonized value
                                    & sEncryptInfo,
                                    sValueColumn,
                                    sValue,                     // original value
                                    NULL,
                                    & aTemplate->tmplate )
                                != IDE_SUCCESS, ERR_INVALID_CANONIZE );

                sValueColumn = sColumn->basicInfo;
                sValue       = sCanonizedValue;
            }
            else if ( ( sColumn->basicInfo->module->flag & MTD_CANON_MASK )
                      == MTD_CANON_NEED_WITH_ALLOCATION )
            {
                IDU_FIT_POINT("qmx::makeSmiValueWithResult::malloc");
                IDE_TEST(aTemplate->stmt->qmxMem->alloc(
                             sColumn->basicInfo->column.size,
                             (void**)&sCanonizedValue)
                         != IDE_SUCCESS);

                IDE_TEST_RAISE( sColumn->basicInfo->module->canonize(
                                    sColumn->basicInfo,
                                    & sCanonizedValue,           // canonized value
                                    & sEncryptInfo,
                                    sValueColumn,
                                    sValue,                     // original value
                                    NULL,
                                    & aTemplate->tmplate )
                                != IDE_SUCCESS, ERR_INVALID_CANONIZE );

                sValueColumn = sColumn->basicInfo;
                sValue       = sCanonizedValue;
            }

            if( ( sColumn->basicInfo->column.flag & SMI_COLUMN_TYPE_MASK )
                                                 == SMI_COLUMN_TYPE_LOB )
            {
                if( sColumn->basicInfo->module->isNull( sValueColumn,
                                                        sValue )
                    != ID_TRUE )
                {

                    // PROJ-1362
                    IDE_DASSERT( sStoringColumn->module->id == sColumn->basicInfo->module->id );
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

            IDE_TEST( qdbCommon::mtdValue2StoringValue(
                          sStoringColumn,
                          sValueColumn,
                          sValue,
                          &sStoringValue )
                      != IDE_SUCCESS );
            aInsertedRow[sColumnOrder].value  = sStoringValue;

            IDE_TEST( qdbCommon::storingSize( sStoringColumn,
                                              sValueColumn,
                                              sValue,
                                              &sStoringSize )
                      != IDE_SUCCESS );
            aInsertedRow[sColumnOrder].length = sStoringSize;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_NOT_ALLOW_NULL)
    {
        /* BUG-45680 insert ����� not null column�� ���� �����޽��� ������ column ���� ���. */
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMX_NOT_NULL_CONSTRAINT,
                                  "",
                                  "" ) );
    }
    IDE_EXCEPTION( ERR_INVALID_CANONIZE );
    {
        if ( ideGetErrorCode() == mtERR_ABORT_INVALID_LENGTH )
        {
            IDE_CLEAR();
            IDE_SET( ideSetErrorCode( mtERR_ABORT_INVALID_LENGTH_COLUMN,
                                      aTableInfo->columns[sColumnOrder].name ) );
        }
        else
        {
            // nothing to do
        }
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmx::makeSmiValueForUpdate( qcTemplate    * aTmplate,
                                   qcmTableInfo  * aTableInfo, /* Criterion for smiValue */
                                   qcmColumn     * aColumn,
                                   qmmValueNode  * aValue,
                                   UInt            aCanonizedTuple,
                                   smiValue      * aUpdatedRow,
                                   mtdIsNullFunc * aIsNull,
                                   qmxLobInfo    * aLobInfo )
{
#define IDE_FN "qmx::makeSmiValueForUpdate"

    mtcStack          * sStack;
    UInt                sColumnCount;
    SInt                sColumnOrder;
    mtcColumn         * sColumns;
    UInt                sIterator;
    qmmValueNode      * sValueNode;
    void              * sCanonizedValue;
    qcmColumn         * sQcmColumn = NULL;
    qcmColumn         * sMetaColumn;
    SInt                sBindId;
    idBool              sIsOutBind;
    mtcColumn         * sMtcColumn;
    mtcEncryptInfo      sEncryptInfo;
    UInt                sStoringSize = 0;
    void              * sStoringValue;
    SLong               sLobLen;
    idBool              sIsNullLob;

    sStack         = aTmplate->tmplate.stack;
    sColumnCount   =
        aTmplate->tmplate.rows[aCanonizedTuple].columnCount;
    sColumns       =
        aTmplate->tmplate.rows[aCanonizedTuple].columns;

    for( sIterator = 0,
             sValueNode = aValue,
             sQcmColumn = aColumn;
         sIterator < sColumnCount;
         sIterator++,
             sValueNode = sValueNode->next,
             sQcmColumn = sQcmColumn->next,
             sColumns++ )
    {
        sColumnOrder = (sQcmColumn->basicInfo->column.id & SMI_COLUMN_ID_MASK);

        /* PROJ-2464 hybrid partitioned table ���� */
        sMetaColumn = &(aTableInfo->columns[sColumnOrder]);

        if ( sValueNode->timestamp == ID_FALSE )
        {
            if( ( sValueNode->calculate == ID_TRUE ) && ( sValueNode->value != NULL ) )
            {
                IDE_TEST( qtc::calculate( sValueNode->value, aTmplate )
                          != IDE_SUCCESS );

                if( (sStack->column->module->id == MTD_BLOB_LOCATOR_ID) ||
                    (sStack->column->module->id == MTD_CLOB_LOCATOR_ID) )
                {
                    // PROJ-1362
                    sIsOutBind = ID_FALSE;

                    if ( (sValueNode->value->node.lflag & MTC_NODE_BIND_MASK)
                         == MTC_NODE_BIND_EXIST )
                    {
                        IDE_DASSERT( sValueNode->value->node.table ==
                                     aTmplate->tmplate.variableRow );

                        sBindId = sValueNode->value->node.column;

                        if ( aTmplate->stmt->pBindParam[sBindId].param.inoutType
                             == CMP_DB_PARAM_OUTPUT )
                        {
                            sIsOutBind = ID_TRUE;
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

                    if ( sIsOutBind == ID_TRUE )
                    {
                        // bind�����̴�.
                        IDE_TEST( addLobInfoForOutBind(
                                      aLobInfo,
                                      & sMetaColumn->basicInfo->column,
                                      sValueNode->value->node.column)
                                  != IDE_SUCCESS );

                        // NOT NULL Constraint
                        // Not Null constraint�� ���� ��� locator�� outbind�� ����� �� ����.
                        IDE_TEST_RAISE(
                            ( sMetaColumn->basicInfo->flag & MTC_COLUMN_NOTNULL_MASK )
                            == MTC_COLUMN_NOTNULL_TRUE,
                            ERR_NOT_ALLOW_NULL );
                    }
                    else
                    {
                        IDE_TEST( addLobInfoForCopy(
                                      aLobInfo,
                                      & sMetaColumn->basicInfo->column,
                                      *(smLobLocator*)sStack->value)
                                  != IDE_SUCCESS );

                        // NOT NULL Constraint
                        if ( ( sMetaColumn->basicInfo->flag & MTC_COLUMN_NOTNULL_MASK )
                             == MTC_COLUMN_NOTNULL_TRUE )
                        {
                            if ( *(smLobLocator*)sStack->value != MTD_LOCATOR_NULL )
                            {
                                IDE_TEST( smiLob::getLength( QC_STATISTICS(aTmplate->stmt),
                                                             *(smLobLocator*)sStack->value,
                                                             & sLobLen,
                                                             & sIsNullLob )
                                          != IDE_SUCCESS );
                            }
                            else
                            {
                                sIsNullLob = ID_TRUE;
                            }

                            IDE_TEST_RAISE( sIsNullLob == ID_TRUE,
                                            ERR_NOT_ALLOW_NULL );
                        }
                        else
                        {
                            // Nothing to do.
                        }
                    }

                    // NULL�� �Ҵ��Ѵ�.
                    aUpdatedRow[sIterator].value = NULL;
                    aUpdatedRow[sIterator].length = 0;
                }
                else
                {
                    sCanonizedValue = (void*)
                        ((UChar*) aTmplate->tmplate.rows[aCanonizedTuple].row
                         + sColumns->column.offset);

                    // PROJ-2002 Column Security
                    // update t1 set i1=i2 ���� ��� i2�� ��ȣȭ�Լ��� �����Ǿ����Ƿ�
                    // ��ȣ Ÿ���� �� �� ����. �� typed literal�� ����ϴ� ���
                    // default policy�� ��ȣ Ÿ���� �� ���� �ִ�.
                    sMtcColumn = QTC_TMPL_COLUMN( aTmplate, sValueNode->value );

                    IDE_TEST_RAISE( ( (sMtcColumn->module->flag & MTD_ENCRYPT_TYPE_MASK)
                                      == MTD_ENCRYPT_TYPE_TRUE ) &&
                                    ( QCS_IS_DEFAULT_POLICY( sMtcColumn ) != ID_TRUE ),
                                    ERR_INVALID_ENCRYPTED_COLUMN );

                    // PROJ-2002 Column Security
                    if ( ( sColumns->module->flag & MTD_ENCRYPT_TYPE_MASK )
                         == MTD_ENCRYPT_TYPE_TRUE )
                    {
                        IDE_TEST( qcsModule::getEncryptInfo( aTmplate->stmt,
                                                             aTableInfo,
                                                             sColumnOrder,
                                                             & sEncryptInfo )
                                  != IDE_SUCCESS );
                    }
                    else
                    {
                        // Nothing to do.
                    }

                    IDE_TEST_RAISE( sColumns->module->canonize( sColumns,
                                                                & sCanonizedValue,
                                                                & sEncryptInfo,
                                                                sStack->column,
                                                                sStack->value,
                                                                NULL,
                                                                & aTmplate->tmplate )
                                    != IDE_SUCCESS, ERR_INVALID_CANONIZE );

                    IDE_TEST( qdbCommon::mtdValue2StoringValue(
                                  sMetaColumn->basicInfo,
                                  sColumns,
                                  sCanonizedValue,
                                  &sStoringValue )
                              != IDE_SUCCESS );
                    aUpdatedRow[sIterator].value = sStoringValue;

                    IDE_TEST( qdbCommon::storingSize( sMetaColumn->basicInfo,
                                                      sColumns,
                                                      sCanonizedValue,
                                                      &sStoringSize )
                              != IDE_SUCCESS );
                    aUpdatedRow[sIterator].length = sStoringSize;

                    // BUG-16523
                    // locator�� ��� length�� �˻������Ƿ�
                    // locator�� �ƴ� ��츸 �˻��Ѵ�.
                    IDE_TEST_RAISE( aIsNull[sIterator](
                                        sColumns,
                                        sCanonizedValue ) == ID_TRUE,
                                    ERR_NOT_ALLOW_NULL );
                }
            }
            else
            {
                // Nothing to do
                // �ƴ� ��쿡�� aUpdatedRow�� ���õǾ� ����
                // makeSmiValueForUpdate()�� ȣ���ϴ� ��: qmnScan.cpp�� updateOneRow()


                // PROJ-1362
                // calculate �����ʴ� ��� isNull���
                if ( ( sMetaColumn->basicInfo->flag & MTC_COLUMN_NOTNULL_MASK )
                     == MTC_COLUMN_NOTNULL_TRUE )
                {
                    // sMetaColumn�� lob_locator_type�� �� ����.
                    // �׷��Ƿ� lob_type�� ����ϸ� �ȴ�.
                    IDE_DASSERT( sMetaColumn->basicInfo->module->id
                                 != MTD_BLOB_LOCATOR_ID );
                    IDE_DASSERT( sMetaColumn->basicInfo->module->id
                                 != MTD_CLOB_LOCATOR_ID );

                    if ( (sMetaColumn->basicInfo->column.flag & SMI_COLUMN_TYPE_MASK)
                         == SMI_COLUMN_TYPE_LOB )
                    {
                        IDE_TEST_RAISE( aUpdatedRow[sIterator].length == 0,
                                        ERR_NOT_ALLOW_NULL );
                    }
                    else
                    {
                        IDE_TEST_RAISE( sMetaColumn->basicInfo->module->isNull(
                                            sColumns,
                                            aUpdatedRow[sIterator].value ) == ID_TRUE,
                                        ERR_NOT_ALLOW_NULL );
                    }
                }
            }
        }
        else
        {
            // set gettimeofday()
            sCanonizedValue = (void*)
                ( (UChar*) aTmplate->tmplate.rows[aCanonizedTuple].row
                  + sColumns->column.offset);

            ((mtdByteType*)sCanonizedValue)->length = QC_BYTE_PRECISION_FOR_TIMESTAMP;

            IDE_TEST( setTimeStamp( ((mtdByteType*)sCanonizedValue)->value ) != IDE_SUCCESS );

            IDE_TEST( qdbCommon::mtdValue2StoringValue(
                          sMetaColumn->basicInfo,
                          sColumns,
                          sCanonizedValue,
                          &sStoringValue )
                      != IDE_SUCCESS );
            aUpdatedRow[sIterator].value = sStoringValue;

            IDE_TEST( qdbCommon::storingSize( sMetaColumn->basicInfo,
                                              sColumns,
                                              sCanonizedValue,
                                              &sStoringSize )
                      != IDE_SUCCESS );
            aUpdatedRow[sIterator].length = sStoringSize;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NOT_ALLOW_NULL );
    {
        /* BUG-45680 insert ����� not null column�� ���� �����޽��� ������ column ���� ���. */
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMX_NOT_NULL_CONSTRAINT,
                                  " : ",
                                  sMetaColumn->name ) );
    }
    IDE_EXCEPTION( ERR_INVALID_ENCRYPTED_COLUMN );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QTC_INVALID_ENCRYPTED_COLUMN ) );
    }
    IDE_EXCEPTION( ERR_INVALID_CANONIZE );
    {
        if ( ideGetErrorCode() == mtERR_ABORT_INVALID_LENGTH )
        {
            IDE_CLEAR();
            IDE_SET( ideSetErrorCode( mtERR_ABORT_INVALID_LENGTH_COLUMN,
                                      aTableInfo->columns[sColumnOrder].name ) );
        }
        else
        {
            // nothing to do
        }
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qmx::makeSmiValueForSubquery( qcTemplate    * aTemplate,
                                     qcmTableInfo  * aTableInfo, /* Criterion for smiValue */
                                     qcmColumn     * aColumn,
                                     qmmSubqueries * aSubquery,
                                     UInt            aCanonizedTuple,
                                     smiValue      * aUpdatedRow,
                                     mtdIsNullFunc * aIsNull,
                                     qmxLobInfo    * aLobInfo )
{
#define IDE_FN "qmx::makeSmiValueForSubquery"

    qmmSubqueries     * sSubquery;
    qmmValuePointer   * sValue;
    mtcStack          * sStack;
    mtcColumn         * sColumn;
    void              * sCanonizedValue;
    SInt                sColumnOrder;
    qcmColumn         * sQcmColumn = NULL;
    qcmColumn         * sMetaColumn;
    UInt                sIterator;
    mtcColumn         * sMtcColumn;
    mtcEncryptInfo      sEncryptInfo;
    UInt                sStoringSize = 0;
    void              * sStoringValue;
    SLong               sLobLen;
    idBool              sIsNullLob;

    // Value ���� ����
    for( sSubquery = aSubquery;
         sSubquery != NULL;
         sSubquery = sSubquery->next )
    {
        IDE_TEST( qtc::calculate( sSubquery->subquery, aTemplate )
                  != IDE_SUCCESS );

        for( sValue = sSubquery->valuePointer,
                 sStack = (mtcStack*)aTemplate->tmplate.stack->value;
             sValue != NULL;
             sValue = sValue->next,
                 sStack++ )
        {
            // Meta�÷��� ã�´�.
            for( sIterator = 0,
                     sQcmColumn = aColumn;
                 sIterator != sValue->valueNode->order;
                 sIterator++,
                     sQcmColumn = sQcmColumn->next )
            {
                // Nothing to do.
            }

            sColumnOrder = (sQcmColumn->basicInfo->column.id & SMI_COLUMN_ID_MASK);

            /* PROJ-2464 hybrid partitioned table ���� */
            sMetaColumn = &(aTableInfo->columns[sColumnOrder]);

            if( (sStack->column->module->id == MTD_BLOB_LOCATOR_ID) ||
                (sStack->column->module->id == MTD_CLOB_LOCATOR_ID) )
            {
                // PROJ-1362
                // subquery�� ����� ��ȿ���� ���� Lob Locator�� ���� �� ����.
                IDE_TEST( addLobInfoForCopy(
                              aLobInfo,
                              & sMetaColumn->basicInfo->column,
                              *(smLobLocator*)sStack->value)
                          != IDE_SUCCESS );

                // NOT NULL Constraint
                if ( ( sMetaColumn->basicInfo->flag & MTC_COLUMN_NOTNULL_MASK )
                     == MTC_COLUMN_NOTNULL_TRUE )
                {
                    if ( *(smLobLocator*)sStack->value != MTD_LOCATOR_NULL )
                    {
                        IDE_TEST( smiLob::getLength( QC_STATISTICS(aTemplate->stmt),
                                                     *(smLobLocator*)sStack->value,
                                                     & sLobLen,
                                                     & sIsNullLob )
                                  != IDE_SUCCESS );
                    }
                    else
                    {
                        sIsNullLob = ID_TRUE;
                    }

                    IDE_TEST_RAISE( sIsNullLob == ID_TRUE,
                                    ERR_NOT_ALLOW_NULL );
                }
                else
                {
                    // Nothing to do.
                }

                // NULL�� �Ҵ��Ѵ�.
                aUpdatedRow[sValue->valueNode->order].value  = NULL;
                aUpdatedRow[sValue->valueNode->order].length = 0;
            }
            else
            {
                sColumn =
                    &( aTemplate->tmplate.rows[aCanonizedTuple]
                       .columns[sValue->valueNode->order]);

                sCanonizedValue = (void*)
                    ( (UChar*)
                      aTemplate->tmplate.rows[aCanonizedTuple].row
                      + sColumn->column.offset);

                // PROJ-2002 Column Security
                // update�� subquery�� ����ϴ� ��� ��ȣ Ÿ���� �� �� ����.
                // �� typed literal�� ����ϴ� ��� default policy��
                // ��ȣ Ÿ���� �� ���� �ִ�.
                //
                // update t1 set i1=(select echar'a' from dual);
                //
                sMtcColumn = QTC_TMPL_COLUMN( aTemplate, sValue->valueNode->value );

                IDE_TEST_RAISE( ( (sMtcColumn->module->flag & MTD_ENCRYPT_TYPE_MASK)
                                  == MTD_ENCRYPT_TYPE_TRUE ) &&
                                ( QCS_IS_DEFAULT_POLICY( sMtcColumn ) != ID_TRUE ),
                                ERR_INVALID_ENCRYPTED_COLUMN );

                // PROJ-2002 Column Security
                if ( ( sColumn->module->flag & MTD_ENCRYPT_TYPE_MASK )
                     == MTD_ENCRYPT_TYPE_TRUE )
                {
                    IDE_TEST( qcsModule::getEncryptInfo( aTemplate->stmt,
                                                         aTableInfo,
                                                         sColumnOrder,
                                                         & sEncryptInfo )
                              != IDE_SUCCESS );
                }
                else
                {
                    // Nothing to do.
                }

                IDE_TEST_RAISE( sColumn->module->canonize( sColumn,
                                                           & sCanonizedValue,
                                                           & sEncryptInfo,
                                                           sStack->column,
                                                           sStack->value,
                                                           NULL,
                                                           & aTemplate->tmplate )
                                != IDE_SUCCESS, ERR_INVALID_CANONIZE );

                IDE_TEST( qdbCommon::mtdValue2StoringValue(
                              sMetaColumn->basicInfo,
                              sStack->column,
                              sCanonizedValue,
                              &sStoringValue )
                          != IDE_SUCCESS );
                aUpdatedRow[sValue->valueNode->order].value  = sStoringValue;

                IDE_TEST( qdbCommon::storingSize( sMetaColumn->basicInfo,
                                                  sStack->column,
                                                  sCanonizedValue,
                                                  &sStoringSize )
                          != IDE_SUCCESS );
                aUpdatedRow[sValue->valueNode->order].length = sStoringSize;

                IDE_TEST_RAISE( aIsNull[sIterator](
                                    sColumn,
                                    sCanonizedValue ) == ID_TRUE,
                                ERR_NOT_ALLOW_NULL );
            }
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NOT_ALLOW_NULL );
    {
        /* BUG-45680 insert ����� not null column�� ���� �����޽��� ������ column ���� ���. */
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMX_NOT_NULL_CONSTRAINT,
                                  "",
                                  "" ) );
    }
    IDE_EXCEPTION( ERR_INVALID_ENCRYPTED_COLUMN );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QTC_INVALID_ENCRYPTED_COLUMN ) );
    }
    IDE_EXCEPTION( ERR_INVALID_CANONIZE );
    {
        if ( ideGetErrorCode() == mtERR_ABORT_INVALID_LENGTH )
        {
            IDE_CLEAR();
            IDE_SET( ideSetErrorCode( mtERR_ABORT_INVALID_LENGTH_COLUMN,
                                      aTableInfo->columns[sColumnOrder].name ) );
        }
        else
        {
            // nothing to do
        }
    }    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qmx::findSessionSeqCaches(qcStatement * aStatement,
                                 qcParseTree * aParseTree)
{
#define IDE_FN "qmx::findSessionSeqCaches"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmx::findSessionSeqCaches"));

    qcSessionSeqCaches  * sSessionSeqCache;
    qcParseSeqCaches    * sParseSeqCache;
    qcuSqlSourceInfo      sqlInfo;

    if (aStatement->session->mQPSpecific.mCache.mSequences_ == NULL)
    {
        sqlInfo.setSourceInfo(
            aStatement,
            & aStatement->myPlan->parseTree->currValSeqs->sequenceNode->position );
        IDE_RAISE(ERR_SEQ_NOT_DEFINE_IN_SESSION);
    }
    else
    {
        for (sParseSeqCache = aParseTree->currValSeqs;
             sParseSeqCache != NULL;
             sParseSeqCache = sParseSeqCache->next)
        {
            for (sSessionSeqCache =
                     aStatement->session->mQPSpecific.mCache.mSequences_;
                 sSessionSeqCache != NULL;
                 sSessionSeqCache = sSessionSeqCache->next)
            {
                if ( sParseSeqCache->sequenceOID == sSessionSeqCache->sequenceOID )
                {
                    /* BUG-45315
                     * PROJ-2268�� ���� OID�� ������ Sequence�� ����Ǿ��� �� �ִ�. */
                    if ( smiValidatePlanHandle( smiGetTable( sSessionSeqCache->sequenceOID ),
                                                sSessionSeqCache->sequenceSCN )
                         == IDE_SUCCESS )
                    {
                        // set CURRVAL in tuple set
                        *(mtdBigintType *)
                            (QC_PRIVATE_TMPLATE(aStatement)->tmplate
                             .rows[sParseSeqCache->sequenceNode->node.table].row)
                            = sSessionSeqCache->currVal;

                        break;
                    }
                    else
                    {
                        /* nothing to do ... */
                    }
                }
                else
                {
                    /* Nothing to do */
                }
            }

            if (sSessionSeqCache == NULL)
            {
                sqlInfo.setSourceInfo(
                    aStatement,
                    & sParseSeqCache->sequenceNode->position );
                IDE_RAISE(ERR_SEQ_NOT_DEFINE_IN_SESSION);
            }
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_SEQ_NOT_DEFINE_IN_SESSION)
    {
        (void)sqlInfo.init(aStatement->qmxMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QMX_SEQ_NOT_DEFINE_IN_SESSION,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qmx::addSessionSeqCaches(qcStatement * aStatement,
                                qcParseTree * aParseTree)
{
#define IDE_FN "qmx::addSessionSeqCaches"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmx::addSessionSeqCaches"));

    qcSessionSeqCaches  * sSessionSeqCache      = NULL;
    qcParseSeqCaches    * sParseSeqCache        = NULL;
    qcSessionSeqCaches  * sNextSessionSeqCache  = NULL; // for cleanup

    for (sParseSeqCache = aParseTree->nextValSeqs;
         sParseSeqCache != NULL;
         sParseSeqCache = sParseSeqCache->next)
    {
        for (sSessionSeqCache =
                 aStatement->session->mQPSpecific.mCache.mSequences_;
             sSessionSeqCache != NULL;
             sSessionSeqCache = sSessionSeqCache->next)
        {
            if ( sParseSeqCache->sequenceOID == sSessionSeqCache->sequenceOID )
            {
                /* BUG-43515
                 * PROJ-2268�� ���� OID�� ������ SEQUENCE�� ����Ǿ��� �� �ִ�. */
                if ( smiValidatePlanHandle( smiGetTable( sSessionSeqCache->sequenceOID ),
                                            sSessionSeqCache->sequenceSCN )
                     == IDE_SUCCESS )
                {
                    // FOUND
                    break;
                }
                else
                {
                    /* nothing to do ... */
                }
            }
            else
            {
                /* Nothing to do */
            }
        }

        if (sSessionSeqCache == NULL) // NOT FOUND
        {
            // free this node in qmxSessionCache::clearSequence
            IDU_FIT_POINT("qmx::addSessionSeqCaches::malloc");
            IDE_TEST(iduMemMgr::malloc(IDU_MEM_QMSEQ,
                                       ID_SIZEOF(qcSessionSeqCaches),
                                       (void**)&sSessionSeqCache)
                     != IDE_SUCCESS);

            sSessionSeqCache->sequenceOID = sParseSeqCache->sequenceOID;
            sSessionSeqCache->sequenceSCN = smiGetRowSCN( smiGetTable( sParseSeqCache->sequenceOID ) ); /* BUG-43515 */

            // connect to sessionSeqCaches
            sSessionSeqCache->next =
                aStatement->session->mQPSpecific.mCache.mSequences_;
            *(&(aStatement->session->mQPSpecific.mCache.mSequences_))
                = sSessionSeqCache;
        }
        else
        {
            // Nothing to do.
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    while( sSessionSeqCache != NULL )
    {
        sNextSessionSeqCache = sSessionSeqCache->next;
        (void)iduMemMgr::free(sSessionSeqCache);
        sSessionSeqCache = sNextSessionSeqCache;
    }

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qmx::setValSessionSeqCaches(
    qcStatement         * aStatement,
    qcParseSeqCaches    * aParseSeqCache,
    SLong                 aNextVal)
{
#define IDE_FN "qmx::setValSessionSeqCaches"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmx::setValSessionSeqCaches"));

    qcSessionSeqCaches  * sSessionSeqCache;
    qcuSqlSourceInfo      sqlInfo;

    for (sSessionSeqCache =
             aStatement->session->mQPSpecific.mCache.mSequences_;
         sSessionSeqCache != NULL;
         sSessionSeqCache = sSessionSeqCache->next)
    {
        if ( aParseSeqCache->sequenceOID == sSessionSeqCache->sequenceOID )
        {
            // set NEXTVAL
            sSessionSeqCache->currVal = aNextVal;
            break;
        }
        else
        {
            /* Nothing to do */
        }
    }

    if (sSessionSeqCache == NULL) // NOT FOUND
    {
        sqlInfo.setSourceInfo(
            aStatement,
            & aParseSeqCache->sequenceNode->position );
        IDE_RAISE(ERR_SEQ_NOT_DEFINE_IN_SESSION);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_SEQ_NOT_DEFINE_IN_SESSION)
    {
        (void)sqlInfo.init(aStatement->qmxMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QMX_SEQ_NOT_DEFINE_IN_SESSION,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qmx::readSequenceNextVals(
    qcStatement         * aStatement,
    qcParseSeqCaches    * aNextValSeqs)
{
#define IDE_FN "qmx::readSequenceNextVals"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmx::readSequenceNextVals"));

    qcParseSeqCaches    * sParseSeqCache;
    SLong                 sCurrVal;
    SLong                 sNodeID;
    
    qdsCallBackInfo       sCallBackInfo;
    smiSeqTableCallBack   sSeqTableCallBack = {
        (void*) & sCallBackInfo,
        qds::selectCurrValTx,
        qds::updateLastValTx,
        0 // max scale of sequence, for sharded sequence
    };
    
    for (sParseSeqCache = aNextValSeqs;
         sParseSeqCache != NULL;
         sParseSeqCache = sParseSeqCache->next)
    {
        if ( sParseSeqCache->sequenceTable == ID_TRUE )
        {
            sCallBackInfo.mStatistics  = aStatement->mStatistics;
            sCallBackInfo.mTableHandle = sParseSeqCache->tableHandle;
            sCallBackInfo.mTableSCN    = sParseSeqCache->tableSCN;
        }
        else
        {
            // Nothing to do.
        }
        
        // read SEQUENCE.NEXTVAL from SM
        IDE_TEST(smiTable::readSequence( QC_SMI_STMT( aStatement ),
                                         sParseSeqCache->sequenceHandle,
                                         SMI_SEQUENCE_NEXT,
                                         &sCurrVal,
                                         &sSeqTableCallBack)
                 != IDE_SUCCESS);

        /* TASK-7217 Sharded sequence */
        if ( sSeqTableCallBack.scale > 0 )
        {
            sNodeID = qcm::getShardNodeID( sSeqTableCallBack.scale );

            IDE_TEST_RAISE(sNodeID == 0, ERR_NOT_EXIST_META);

            if ( sCurrVal >= 0 )
            {
                sCurrVal = sCurrVal + sNodeID;
            }
            else
            {
                sCurrVal = sCurrVal - sNodeID;
            }
        }

        // set NEXTVAL in session sequence cache
        IDE_TEST(setValSessionSeqCaches(aStatement, sParseSeqCache, sCurrVal)
                 != IDE_SUCCESS);

        // set NEXTVAL in tuple set
        *(mtdBigintType *)
            (QC_PRIVATE_TMPLATE(aStatement)->tmplate
             .rows[sParseSeqCache->sequenceNode->node.table].row)
            = sCurrVal;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_NOT_EXIST_META)
    {
        ideSetErrorCode(qpERR_ABORT_QDSD_SHARD_META_NOT_CREATED);
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qmx::dummySequenceNextVals(
    qcStatement         * aStatement,
    qcParseSeqCaches    * aNextValSeqs)
{
    qcParseSeqCaches    * sParseSeqCache;
    
    for (sParseSeqCache = aNextValSeqs;
         sParseSeqCache != NULL;
         sParseSeqCache = sParseSeqCache->next)
    {
        // set NEXTVAL in tuple set
        *(mtdBigintType *)
            (QC_PRIVATE_TMPLATE(aStatement)->tmplate
             .rows[sParseSeqCache->sequenceNode->node.table].row)
            = 0;
    }

    return IDE_SUCCESS;
}

IDE_RC qmx::setTimeStamp( void * aValue )
{
#define IDE_FN "qmx::setTimeStamp"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmx::setTimeStamp"));

    PDL_Time_Value     sTimevalue;
    UInt               sTvSec;     /* seconds since Jan. 1, 1970 */
    UInt               sTvUsec;    /* and microseconds */
    UInt               sSize = ID_SIZEOF(UInt);

    sTimevalue = idlOS::gettimeofday();

    sTvSec  = idlOS::hton( (UInt)sTimevalue.sec() );
    sTvUsec = idlOS::hton( (UInt)sTimevalue.usec() );

    IDE_TEST_RAISE( ( ( sSize + sSize )
                      != QC_BYTE_PRECISION_FOR_TIMESTAMP),   // defined in qc.h
                    ERR_QMX_SET_TIMESTAMP_INTERNAL);

    idlOS::memcpy( aValue,
                   &sTvSec,
                   sSize );

    idlOS::memcpy( (void *)( (SChar *)aValue + sSize ),
                   &sTvUsec,
                   sSize );

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_QMX_SET_TIMESTAMP_INTERNAL)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QMX_CANNOT_SET_TIMESTAMP));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qmx::initializeLobInfo( qcStatement  * aStatement,
                               qmxLobInfo  ** aLobInfo,
                               UShort         aSize )
{
#define IDE_FN "qmx::initializeLobInfo"

    qmxLobInfo  * sLobInfo = NULL;
    UShort        i;

    if ( aSize > 0 )
    {
        // lobInfo
        IDU_FIT_POINT("qmx::initializeLobInfo::malloc1");
        IDE_TEST( aStatement->qmxMem->alloc(
                      ID_SIZEOF(qmxLobInfo),
                      (void**) & sLobInfo )
                  != IDE_SUCCESS );

        // for copy
        // column
        IDU_FIT_POINT("qmx::initializeLobInfo::malloc2");
        IDE_TEST( aStatement->qmxMem->alloc(
                      ID_SIZEOF(smiColumn *) * aSize,
                      (void**) & sLobInfo->column )
                  != IDE_SUCCESS );

        // locator
        IDU_FIT_POINT("qmx::initializeLobInfo::malloc3");
        IDE_TEST( aStatement->qmxMem->alloc(
                      ID_SIZEOF(smLobLocator) * aSize,
                      (void**) & sLobInfo->locator )
                  != IDE_SUCCESS );

        // dstBindId
        IDE_TEST( aStatement->qmxMem->alloc(
                      ID_SIZEOF(UShort) * aSize,
                      (void**) & sLobInfo->dstBindId )
                  != IDE_SUCCESS );

        // for outbind
        // outBindId
        IDU_FIT_POINT("qmx::initializeLobInfo::malloc4");
        IDE_TEST( aStatement->qmxMem->alloc(
                      ID_SIZEOF(UShort) * aSize,
                      (void**) & sLobInfo->outBindId )
                  != IDE_SUCCESS );

        // outColumn
        IDU_FIT_POINT("qmx::initializeLobInfo::malloc5");
        IDE_TEST( aStatement->qmxMem->alloc(
                      ID_SIZEOF(smiColumn *) * aSize,
                      (void**) & sLobInfo->outColumn )
                  != IDE_SUCCESS );

        // outLocator
        IDU_FIT_POINT("qmx::initializeLobInfo::malloc6");
        IDE_TEST( aStatement->qmxMem->alloc(
                      ID_SIZEOF(smLobLocator) * aSize,
                      (void**) & sLobInfo->outLocator )
                  != IDE_SUCCESS );

        // outFirstLocator
        IDU_FIT_POINT("qmx::initializeLobInfo::malloc7");
        IDE_TEST( aStatement->qmxMem->alloc(
                      ID_SIZEOF(smLobLocator) * aSize,
                      (void**) & sLobInfo->outFirstLocator )
                  != IDE_SUCCESS );

        // initialize
        sLobInfo->size        = aSize;
        sLobInfo->count       = 0;
        sLobInfo->outCount    = 0;
        sLobInfo->outFirst    = ID_TRUE;
        sLobInfo->outCallback = ID_FALSE;
        sLobInfo->mCount4PutLob = 0;
        sLobInfo->mBindId4PutLob = NULL;
        sLobInfo->mCount4OutBindNonLob = 0;
        sLobInfo->mBindId4OutBindNonLob = NULL;
        sLobInfo->mImmediateClose = ID_FALSE;

        for ( i = 0; i < aSize; i++ )
        {
            sLobInfo->column[i] = NULL;
            sLobInfo->locator[i] = MTD_LOCATOR_NULL;
            sLobInfo->dstBindId[i] = 0;

            sLobInfo->outBindId[i] = 0;
            sLobInfo->outColumn[i] = NULL;
            sLobInfo->outLocator[i] = MTD_LOCATOR_NULL;
            sLobInfo->outFirstLocator[i] = MTD_LOCATOR_NULL;
        }
    }
    else
    {
        // Nothing to do.
    }

    *aLobInfo = sLobInfo;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

void qmx::initLobInfo( qmxLobInfo  * aLobInfo )
{
/***********************************************************************
 *
 * Description :
 *     insert, insert select, atomic insert ���� single row insert��
 *     lob info�� �����Ҷ����� outFirst�� �ʱ�ȭ�ؾ��Ѵ�.
 *
 * Implementation : lob info�� �ʱ�ȭ�Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "qmx::initLobInfo"

    UShort   i;

    if ( aLobInfo != NULL )
    {
        aLobInfo->count       = 0;
        aLobInfo->outCount    = 0;
        aLobInfo->outFirst    = ID_TRUE;
        aLobInfo->outCallback = ID_FALSE;
        aLobInfo->mImmediateClose = ID_FALSE;
        
        for ( i = 0; i < aLobInfo->size; i++ )
        {
            aLobInfo->column[i] = NULL;
            aLobInfo->locator[i] = MTD_LOCATOR_NULL;
            aLobInfo->dstBindId[i] = 0;

            aLobInfo->outBindId[i] = 0;
            aLobInfo->outColumn[i] = NULL;
            aLobInfo->outLocator[i] = MTD_LOCATOR_NULL;
            aLobInfo->outFirstLocator[i] = MTD_LOCATOR_NULL;
        }
    }
    else
    {
        // Nothing to do.
    }

#undef IDE_FN
}

void qmx::clearLobInfo( qmxLobInfo  * aLobInfo )
{
/***********************************************************************
 *
 * Description :
 *     update ���� multi row update�� lob info�� ����������
 *     outFirst�� �ʱ�ȭ�ϸ� �ȵȴ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmx::clearLobInfo"

    UShort   i;

    if ( aLobInfo != NULL )
    {
        aLobInfo->count       = 0;
        aLobInfo->outCount    = 0;
        
        for ( i = 0; i < aLobInfo->size; i++ )
        {
            aLobInfo->column[i] = NULL;
            aLobInfo->locator[i] = MTD_LOCATOR_NULL;
            aLobInfo->dstBindId[i] = 0;

            aLobInfo->outBindId[i] = 0;
            aLobInfo->outColumn[i] = NULL;
            aLobInfo->outLocator[i] = MTD_LOCATOR_NULL;
        }
    }
    else
    {
        // Nothing to do.
    }

#undef IDE_FN
}

IDE_RC qmx::addLobInfoForCopy( qmxLobInfo   * aLobInfo,
                               smiColumn    * aColumn,
                               smLobLocator   aLocator,
                               UShort         aBindId )
{
#define IDE_FN "qmx::addLobInfoForCopy"

    // BUG-38188 instead of trigger���� aLobInfo�� NULL�� �� �ִ�.
    if ( aLobInfo != NULL )
    {
        IDE_ASSERT( aLobInfo->count < aLobInfo->size );
        IDE_ASSERT( (aColumn->flag & SMI_COLUMN_TYPE_MASK) ==
                    SMI_COLUMN_TYPE_LOB );

        /* BUG-44022 CREATE AS SELECT�� TABLE ������ ���� OUTER JOIN�� LOB�� ���� ����ϸ� ������ �߻� */
        if ( ( aLocator == MTD_LOCATOR_NULL )
             ||
             ( SMI_IS_NULL_LOCATOR( aLocator ) ) )
        {
            // Nothing To Do
        }
        else
        {
            aLobInfo->column[aLobInfo->count] = aColumn;
            aLobInfo->locator[aLobInfo->count] = aLocator;
            aLobInfo->dstBindId[aLobInfo->count] = aBindId;
            aLobInfo->count++;
        }
    }
    else
    {
        // Nothing to do.
    }

    return IDE_SUCCESS;

#undef IDE_FN
}

IDE_RC qmx::addLobInfoForOutBind( qmxLobInfo   * aLobInfo,
                                  smiColumn    * aColumn,
                                  UShort         aBindId )
{
#define IDE_FN "qmx::addLobInfoForOutBind"

    IDE_ASSERT( aLobInfo != NULL );
    IDE_ASSERT( aLobInfo->outCount < aLobInfo->size );
    IDE_ASSERT( (aColumn->flag & SMI_COLUMN_TYPE_MASK) ==
                SMI_COLUMN_TYPE_LOB );

    aLobInfo->outColumn[aLobInfo->outCount] = aColumn;
    aLobInfo->outBindId[aLobInfo->outCount] = aBindId;
    aLobInfo->outCount++;

    return IDE_SUCCESS;

#undef IDE_FN
}

/* BUG-30351
 * insert into select���� �� Row Insert �� �ش� Lob Cursor�� �ٷ� ���������� �մϴ�.
 */
void qmx::setImmediateCloseLobInfo( qmxLobInfo  * aLobInfo,
                                    idBool        aImmediateClose )
{
    if(aLobInfo != NULL)
    {
        aLobInfo->mImmediateClose = aImmediateClose;
    }
    else
    {
        // Nothing to do
    }
}

IDE_RC qmx::copyAndOutBindLobInfo( qcStatement    * aStatement,
                                   qmxLobInfo     * aLobInfo,
                                   smiTableCursor * aCursor,
                                   void           * aRow,
                                   scGRID           aRowGRID )
{
#define IDE_FN "qmx::copyAndOutBindLobInfo"

    smLobLocator       sLocator = MTD_LOCATOR_NULL;
    smLobLocator       sTempLocator;
    qcTemplate       * sTemplate;
    UShort             sBindTuple;
    UShort             sBindId;
    UInt               sInfo = 0;
    UInt               i;

    if ( aLobInfo != NULL )
    {
        sInfo = MTC_LOB_LOCATOR_CLOSE_TRUE;

        // for copy
        for ( i = 0;
              i < aLobInfo->count;
              i++ )
        {
            IDE_ASSERT( aLobInfo->column    != NULL );
            IDE_ASSERT( aLobInfo->column[i] != NULL );

            if ( (aLobInfo->column[i]->flag & SMI_COLUMN_STORAGE_MASK)
                 == SMI_COLUMN_STORAGE_MEMORY )
            {
                IDE_TEST( smiLob::openLobCursorWithRow( aCursor,
                                                        aRow,
                                                        aLobInfo->column[i],
                                                        sInfo,
                                                        SMI_LOB_TABLE_CURSOR_MODE,
                                                        & sLocator )
                          != IDE_SUCCESS );
            }
            else
            {
                //fix BUG-19687
                IDE_TEST( smiLob::openLobCursorWithGRID( aCursor,
                                                         aRowGRID,
                                                         aLobInfo->column[i],
                                                         sInfo,
                                                         SMI_LOB_TABLE_CURSOR_MODE,
                                                         & sLocator )
                          != IDE_SUCCESS );
            }

            IDE_TEST( smiLob::copy( aStatement->mStatistics,
                                    sLocator,
                                    aLobInfo->locator[i] )
                      != IDE_SUCCESS );

            sTempLocator = sLocator;
            sLocator = MTD_LOCATOR_NULL;
            IDE_TEST( closeLobLocator( aStatement->mStatistics,
                                       sTempLocator )
                      != IDE_SUCCESS );

            /* BUG-30351
             * insert into select���� �� Row Insert �� �ش� Lob Cursor�� �ٷ� ���������� �մϴ�.
             */
            sTempLocator = aLobInfo->locator[i];
            aLobInfo->locator[i] = MTD_LOCATOR_NULL;

            if( aLobInfo->mImmediateClose == ID_TRUE )
            {
                if( ( sTempLocator == MTD_LOCATOR_NULL )
                    ||
                    ( SMI_IS_NULL_LOCATOR( sTempLocator )) )
                {
                    // nothing todo
                }
                else
                {
                    IDE_TEST( smiLob::closeLobCursor( aStatement->mStatistics,
                                                      sTempLocator )
                              != IDE_SUCCESS );
                }
            }
            else
            {
                IDE_TEST( closeLobLocator( aStatement->mStatistics,
                                           sTempLocator )
                        != IDE_SUCCESS );
            }
        }

        sInfo = MTC_LOB_COLUMN_NOTNULL_FALSE;

        // for outbind
        for ( i = 0;
              i < aLobInfo->outCount;
              i++ )
        {
            IDE_ASSERT( aLobInfo->outColumn    != NULL );
            IDE_ASSERT( aLobInfo->outColumn[i] != NULL );

            if ( (aLobInfo->outColumn[i]->flag & SMI_COLUMN_STORAGE_MASK)
                 == SMI_COLUMN_STORAGE_MEMORY )
            {
                IDE_TEST( smiLob::openLobCursorWithRow( aCursor,
                                                        aRow,
                                                        aLobInfo->outColumn[i],
                                                        sInfo,
                                                        SMI_LOB_TABLE_CURSOR_MODE,
                                                        & sLocator )
                          != IDE_SUCCESS );
            }
            else
            {
                IDE_TEST( smiLob::openLobCursorWithGRID( aCursor,
                                                         aRowGRID,
                                                         aLobInfo->outColumn[i],
                                                         sInfo,
                                                         SMI_LOB_TABLE_CURSOR_MODE,
                                                         & sLocator )
                          != IDE_SUCCESS );
            }

            // locator�� out-bound �Ǿ��� ��
            // getParamData�� ù��° locator�� �Ѱ��ֱ� ����
            // ù��° locator���� bind-tuple�� �����Ѵ�.
            if ( aLobInfo->outFirst == ID_TRUE )
            {
                sTemplate = QC_PRIVATE_TMPLATE(aStatement);
                sBindTuple = sTemplate->tmplate.variableRow;
                sBindId = aLobInfo->outBindId[i];

                IDE_DASSERT( sBindTuple != ID_USHORT_MAX );
                IDE_DASSERT( aLobInfo->outColumn[i]->size
                             != ID_SIZEOF(sLocator) );

                idlOS::memcpy( (SChar*) sTemplate->tmplate.rows[sBindTuple].row
                               + sTemplate->tmplate.rows[sBindTuple].
                               columns[sBindId].column.offset,
                               & sLocator,
                               ID_SIZEOF(sLocator) );

                // ù��° locator���� ���� �����Ѵ�.
                // mm���� locator-list�� hash function�� ����Ѵ�.
                aLobInfo->outFirstLocator[i] = sLocator;
            }
            else
            {
                // Nothing to do.
            }

            aLobInfo->outLocator[i] = sLocator;
            sLocator = MTD_LOCATOR_NULL;
        }

        aLobInfo->outFirst = ID_FALSE;

        if ( aLobInfo->outCount > 0 )
        {
            IDE_TEST( qci::mOutBindLobCallback(
                          aStatement->session->mMmSession,
                          aLobInfo->outLocator,
                          aLobInfo->outFirstLocator,
                          aLobInfo->outCount )
                      != IDE_SUCCESS );

            // callback�� ȣ�������� ǥ��
            aLobInfo->outCallback = ID_TRUE;
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

    IDE_EXCEPTION_END;

    (void) closeLobLocator( aStatement->mStatistics, sLocator );

    for ( i = 0;
          i < aLobInfo->outCount;
          i++ )
    {
        (void) closeLobLocator( aStatement->mStatistics, aLobInfo->outLocator[i] );
        aLobInfo->outLocator[i] = MTD_LOCATOR_NULL;
    }

    return IDE_FAILURE;

#undef IDE_FN
}

void qmx::finalizeLobInfo( qcStatement * aStatement,
                           qmxLobInfo  * aLobInfo )
{
#define IDE_FN "qmx::finalizeLobInfo"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmx::finalizeLobInfo"));

    UShort    i;

    if ( aLobInfo != NULL )
    {
        for ( i = 0;
              i < aLobInfo->count;
              i++ )
        {
            (void) closeLobLocator( aStatement->mStatistics,
                                    aLobInfo->locator[i] );
            aLobInfo->locator[i] = MTD_LOCATOR_NULL;
        }

        if ( aLobInfo->outCallback == ID_TRUE )
        {
            (void) qci::mCloseOutBindLobCallback(
                aStatement->session->mMmSession,
                aLobInfo->outFirstLocator,
                aLobInfo->outCount );

            aLobInfo->outCallback = ID_FALSE;
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

#undef IDE_FN
}

IDE_RC qmx::closeLobLocator( idvSQL       * aStatistics,
                             smLobLocator   aLocator )
{
#define IDE_FN "qmx::closeLobLocator"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(IDE_FN));

    UInt sInfo;

    if ( (aLocator == MTD_LOCATOR_NULL)
         ||
         (SMI_IS_NULL_LOCATOR(aLocator)) )
    {
        // Nothing to do.
    }
    else
    {
        IDE_TEST( smiLob::getInfo( aLocator,
                                   & sInfo )
                  != IDE_SUCCESS );

        if ( (sInfo & MTC_LOB_LOCATOR_CLOSE_MASK)
             == MTC_LOB_LOCATOR_CLOSE_TRUE )
        {
            IDE_TEST( smiLob::closeLobCursor( aStatistics,
                                              aLocator )
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

#undef IDE_FN
}

IDE_RC
qmx::fireStatementTriggerOnDeleteCascade(
    qcStatement         * aStatement,
    qmsTableRef         * aParentTableRef,
    qcmRefChildInfo     * aChildInfo,
    qcmTriggerEventTime   aTriggerEventTime )
{
/***********************************************************************
 * BUG-28049, BUG-28094
 *
 * Description : delete cascade�� ���� Statement trigger�� �����Ѵ�.
 *               plan�� ������ child constraint list�� ���Ե�
 *               ��� ���̺� IS lock�� �ο��ϰ�,
 *               �� ���̺��� �ϳ����� delete Ʈ���Ÿ� �����Ѵ�.
 *
 *               Ʈ���� ȣ�� ������ Ʈ���� ���� ������ ���� �����ϸ�,
 *               QCM_TRIGGER_BEFORE & QCM_TRIGGER_AFTER �� ������ ����.
 *               �� ���� �ݴ��� ������ statement Ʈ���Ÿ� ȣ���Ѵ�.
 *
 *               validation �������� ������ child constraint list��
 *               delete�� delete cascade�� ����Ͽ� �����Ǿ��� ������
 *               �ش� �Լ������� �̸� �����ؼ� ������ �ʿ䰡 ����.
 *
 * Implementation :
 *
 ***********************************************************************/

    qcmRefChildInfo     * sRefChildInfo;
    qmsTableRef         * sChildTableRef;
    qmsTableRef        ** sChildTableRefList;
    UInt                  sChildTableRefCount;
    UInt                  i;

    IDE_DASSERT( aParentTableRef != NULL );

    // 1. child constraint list�� �̿��Ͽ�
    // child Table�� ���� ���� ����Ʈ ���� & �ߺ�����
    sChildTableRefCount = 1;

    for ( sRefChildInfo = aChildInfo;
          sRefChildInfo != NULL;
          sRefChildInfo = sRefChildInfo->next)
    {
        sChildTableRefCount++;
    }

    IDU_FIT_POINT("qmx::fireStatementTriggerOnDeleteCascade::malloc");
    IDE_TEST( aStatement->qmxMem->alloc(
                  ID_SIZEOF(qmsTableRef *) * sChildTableRefCount,
                  (void**) & sChildTableRefList )
              != IDE_SUCCESS );

    sChildTableRefList[0] = aParentTableRef;
    sChildTableRefCount   = 1;

    for ( sRefChildInfo = aChildInfo;
          sRefChildInfo != NULL;
          sRefChildInfo = sRefChildInfo->next)
    {
        sChildTableRef = sRefChildInfo->childTableRef;

        for( i = 0; i < sChildTableRefCount; i++ )
        {
            if( sChildTableRefList[i]->tableInfo ==
                sChildTableRef->tableInfo )
            {
                break;
            }
            else
            {
                // Nothing to do.
            }
        }

        if( i == sChildTableRefCount )
        {
            sChildTableRefList[sChildTableRefCount] = sChildTableRef;
            sChildTableRefCount++;
        }
        else
        {
            // Nothing to do.
        }
    }

    for ( i = 0; i < sChildTableRefCount; i++ )
    {
        if ( aParentTableRef->tableHandle != sChildTableRefList[i]->tableHandle )
        {
            // BUG-21816
            // child�� tableInfo�� �����ϱ� ���� IS LOCK�� ��´�.
            IDE_TEST( qmx::lockTableForDML( aStatement,
                                            sChildTableRefList[i],
                                            SMI_TABLE_LOCK_IS )
                      != IDE_SUCCESS );
        }
        else
        {
            // Nothing to do.
        }
    }
    
    // 2. Ʈ���� ���� ������ ���� �����ϸ� statement Ʈ���� ȣ��
    switch ( aTriggerEventTime )
    {
        case QCM_TRIGGER_BEFORE:
            for( i = 0; i < sChildTableRefCount; i++ )
            {
                IDE_TEST(
                    qdnTrigger::fireTrigger(
                        aStatement,
                        aStatement->qmxMem,
                        sChildTableRefList[i]->tableInfo,
                        QCM_TRIGGER_ACTION_EACH_STMT,
                        aTriggerEventTime,
                        QCM_TRIGGER_EVENT_DELETE,
                        NULL,  // UPDATE Column
                        NULL,            /* Table Cursor */
                        SC_NULL_GRID,    /* Row GRID */
                        NULL,  // OLD ROW
                        NULL,  // OLD ROW Column
                        NULL,  // NEW ROW
                        NULL ) // NEW ROW Column
                    != IDE_SUCCESS );
            }
            break;

        case QCM_TRIGGER_AFTER:
            for( i = 1; i <= sChildTableRefCount; i++ )
            {
                IDE_TEST(
                    qdnTrigger::fireTrigger(
                        aStatement,
                        aStatement->qmxMem,
                        sChildTableRefList[ sChildTableRefCount - i ]->tableInfo,
                        QCM_TRIGGER_ACTION_EACH_STMT,
                        aTriggerEventTime,
                        QCM_TRIGGER_EVENT_DELETE,
                        NULL,  // UPDATE Column
                        NULL,            /* Table Cursor */
                        SC_NULL_GRID,    /* Row GRID */
                        NULL,  // OLD ROW
                        NULL,  // OLD ROW Column
                        NULL,  // NEW ROW
                        NULL ) // NEW ROW Column
                    != IDE_SUCCESS );
            }
            break;

        default:
            IDE_DASSERT( 0 );
            break;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmx::fireTriggerInsertRowGranularity( qcStatement      * aStatement,
                                      qmsTableRef      * aTableRef,
                                      qmcInsertCursor  * aInsertCursorMgr,
                                      scGRID             aInsertGRID,
                                      qmmReturnInto    * aReturnInto,
                                      qdConstraintSpec * aCheckConstrList,
                                      vSLong             aRowCnt,
                                      idBool             aNeedNewRow )
{
/***********************************************************************
 *
 * Description :
 *     INSERT ���� ���� �� Row Granularity Trigger�� ���Ͽ�
 *     Trigger�� fire�Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmx::fireTriggerInsertRowGranularity"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    UShort            sTableID;
    UShort            sPartitionTupleID   = 0;
    smiTableCursor  * sCursor             = NULL;
    qcmColumn       * sColumnsForNewRow;
    idBool            sHasCheckConstraint;
    idBool            sNeedToRecoverTuple = ID_FALSE;
    mtcTuple        * sInsertTuple;
    mtcTuple        * sPartitionTuple     = NULL;
    mtcTuple          sCopyTuple;

    //---------------------------------------
    // ���ռ� �˻�
    //---------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aInsertCursorMgr != NULL );

    //---------------------------------------
    // Trigger Object�� ����
    //---------------------------------------

    sHasCheckConstraint = (aCheckConstrList != NULL) ? ID_TRUE : ID_FALSE;

    IDE_TEST( aInsertCursorMgr->getCursor( & sCursor )
              != IDE_SUCCESS );

    //--------------------------------------
    // Return Clause ���� After Triggeró��
    // Insert �� NEW ROW�� �ʿ�.
    //--------------------------------------
    if ( ( aNeedNewRow == ID_TRUE ) ||
         ( aReturnInto != NULL ) ||
         ( sHasCheckConstraint == ID_TRUE ) )
    {
        //---------------------------------
        // NEW ROW �� �ʿ��� ���
        //---------------------------------

        sTableID = aTableRef->table;
        sInsertTuple = &(QC_PRIVATE_TMPLATE(aStatement)->tmplate.rows[sTableID]);

        /* PROJ-2464 hybrid partitioned table ���� */
        if ( aTableRef->tableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
        {
            if ( aTableRef->partitionSummary->isHybridPartitionedTable == ID_TRUE )
            {
                IDE_TEST( aInsertCursorMgr->getSelectedPartitionTupleID( &sPartitionTupleID )
                          != IDE_SUCCESS );

                sPartitionTuple = &(QC_PRIVATE_TMPLATE(aStatement)->tmplate.rows[sPartitionTupleID]);

                copyMtcTupleForPartitionDML( &sCopyTuple, sInsertTuple );
                sNeedToRecoverTuple = ID_TRUE;

                adjustMtcTupleForPartitionDML( sInsertTuple, sPartitionTuple );
            }
            else
            {
                /* Nothing to do */
            }
        }
        else
        {
            /* Nothing to do */
        }

        IDE_TEST( qmc::setRowSize( aStatement->qmxMem,
                                   & QC_PRIVATE_TMPLATE(aStatement)->tmplate,
                                   sTableID )
                  != IDE_SUCCESS );

        // NEW ROW�� ȹ��
        IDE_TEST( sCursor->getLastModifiedRow( & sInsertTuple->row,
                                               sInsertTuple->rowOffset )
                  != IDE_SUCCESS );

        IDE_TEST( aInsertCursorMgr->setColumnsForNewRow()
                  != IDE_SUCCESS );

        IDE_TEST( aInsertCursorMgr->getColumnsForNewRow(
                      &sColumnsForNewRow )
                  != IDE_SUCCESS );

        /* PROJ-1107 Check Constraint ���� */
        if ( sHasCheckConstraint == ID_TRUE )
        {
            IDE_TEST( qdnCheck::verifyCheckConstraintList(
                            QC_PRIVATE_TMPLATE(aStatement),
                            aCheckConstrList )
                      != IDE_SUCCESS );
        }
        else
        {
            /* Nothing to do */
        }

        if ( aNeedNewRow == ID_TRUE )
        {

            // PROJ-1359 Trigger
            // ROW GRANULARITY TRIGGER�� ����
            IDE_TEST( qdnTrigger::fireTrigger(
                        aStatement,
                        aStatement->qmxMem,
                        aTableRef->tableInfo,
                        QCM_TRIGGER_ACTION_EACH_ROW,
                        QCM_TRIGGER_AFTER,
                        QCM_TRIGGER_EVENT_INSERT,
                        NULL,  // UPDATE Column
                        sCursor,    /* Table Cursor */
                        aInsertGRID,/* Row GRID */
                        NULL,  // OLD ROW
                        NULL,  // OLD ROW Column
                        sInsertTuple->row,  // NEW ROW
                        sColumnsForNewRow ) // NEW ROW Column
                    != IDE_SUCCESS );
        }
        else
        {
            // Nothing to do
        }

        /* PROJ-1584 DML Return Clause */
        if ( aReturnInto != NULL )
        {
            IDE_TEST( copyReturnToInto( QC_PRIVATE_TMPLATE(aStatement),
                                        aReturnInto,
                                        aRowCnt )
                    != IDE_SUCCESS );
        }
        else
        {
            // nothing do do
        }

        /* PROJ-2464 hybrid partitioned table ���� */
        if ( sNeedToRecoverTuple == ID_TRUE )
        {
            sNeedToRecoverTuple = ID_FALSE;
            copyMtcTupleForPartitionDML( sInsertTuple, &sCopyTuple );
        }
        else
        {
            /* Nothing to do */
        }

        // insert-select�� ��� qmx memory�� �ʱ�ȭ �ǹǷ�
        // column�� ���� ���ؾ� �Ѵ�.
        IDE_TEST( aInsertCursorMgr->clearColumnsForNewRow()
                  != IDE_SUCCESS );
    }
    else
    {
        // PROJ-1359 Trigger
        // ROW GRANULARITY TRIGGER�� ����
        IDE_TEST( qdnTrigger::fireTrigger( aStatement,
                                           aStatement->qmxMem,
                                           aTableRef->tableInfo,
                                           QCM_TRIGGER_ACTION_EACH_ROW,
                                           QCM_TRIGGER_AFTER,
                                           QCM_TRIGGER_EVENT_INSERT,
                                           NULL,  // UPDATE Column
                                           NULL,            /* Table Cursor */
                                           SC_NULL_GRID,    /* Row GRID */
                                           NULL,  // OLD ROW
                                           NULL,  // OLD ROW Column
                                           NULL,  // NEW ROW
                                           NULL ) // NEW ROW Column
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    /* PROJ-2464 hybrid partitioned table ���� */
    if ( sNeedToRecoverTuple == ID_TRUE )
    {
        copyMtcTupleForPartitionDML( sInsertTuple, &sCopyTuple );
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmx::checkPlanTreeOld( qcStatement * aStatement,
                       idBool      * aIsOld )
{
/***********************************************************************
 *
 * Description :
 *    PROJ-1350
 *    �ش� Plan Tree�� �ʹ� �����Ǿ������� �˻�
 *
 * Implementation :
 *    Plan Tree�� ���� Table���� Record ������
 *    ���� Table�� ���� Record ������ ���Ͽ�
 *    ���� ���� �̻��̸� Plan Tree�� �ſ� �����Ǿ����� ������.
 *    ��, RULE based Hint�� ���� ������ ��� �������
 *    �̷��� ������ ������ �ʴ´�.
 *
 ***********************************************************************/

#define IDE_FN "qmx::checkPlanTreeOld"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    UShort            i;

    idBool            sIsOld;

    ULong             sRecordCnt;
    SDouble           sRealRecordCnt;
    SDouble           sStatRecordCnt;
    SDouble           sChangeRatio;
    qcTableMap      * sTableMap;
    qmsTableRef     * sTableRef;
    qmsPartitionRef * sPartitionRef;

    //----------------------------------------
    // ���ռ� �˻�
    //----------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aIsOld     != NULL );

    //----------------------------------------
    // �⺻ ���� ����
    //----------------------------------------

    sIsOld = ID_FALSE;
    sTableMap = QC_PRIVATE_TMPLATE(aStatement)->tableMap;

    if ( QCU_FAKE_TPCH_SCALE_FACTOR > 0 )
    {
        // TPC-H�� ���� ������ ��� ������ ������ �����
        // �˻縦 ���� �ʴ´�.
        IDE_CONT( NORMAL_EXIT );
    }
    else
    {
        // Nothing To Do
    }

    if ( ( QC_PRIVATE_TMPLATE(aStatement)->flag & QC_TMP_PLAN_KEEP_MASK )
         == QC_TMP_PLAN_KEEP_TRUE )
    {
        // ������� ���濡 ���� rebuild�� ������ �ʴ� ���
        // �˻縦 ���� �ʴ´�.
        IDE_CONT( NORMAL_EXIT );
    }
    else
    {
        // Nothing To Do
    }

    // fix BUG-33589
    if ( QCU_PLAN_REBUILD_ENABLE == 0 )
    {

        IDE_CONT( NORMAL_EXIT );
    }
    else
    {
        /* Nothing to do. */
    }

    // DNF�� ó���� ��� plan ������ ��û���� �þ �� ����
    // �̶� PLAN�� �þ���� Table Map�� �������� ����.

    // BUG-17409
    for ( i = 0; i < QC_PRIVATE_TMPLATE(aStatement)->tmplate.rowCount; i++ )
    {
        if ( sTableMap[i].from != NULL )
        {
            // FROM�� �ش��ϴ� ��ü�� ���

            if ( sTableMap[i].from->tableRef->view == NULL )
            {
                /* PROJ-1832 New database link */
                if ( sTableMap[i].from->tableRef->remoteTable != NULL )
                {
                    continue;
                }
                else
                {
                    /* nothing to do */
                }

                // BUG-47700 recursive view�� ��� rebuild Ȯ���� �ʿ� �����ϴ�.
                if ( ( sTableMap[i].from->tableRef->flag & QMS_TABLE_REF_RECURSIVE_VIEW_MASK) ==
                     QMS_TABLE_REF_RECURSIVE_VIEW_TRUE )
                {
                    continue;
                }         

                // Table �� ���
                if ( sTableMap[i].from->tableRef->statInfo->optGoleType
                     != QMO_OPT_GOAL_TYPE_RULE )
                {
                    if( ( QC_PRIVATE_TMPLATE(aStatement)->tmplate.rows[i].lflag &
                          MTC_TUPLE_PARTITION_MASK ) ==
                        MTC_TUPLE_PARTITION_FALSE )
                    {
                        sTableRef = sTableMap[i].from->tableRef;

                        //-----------------------------------------
                        // Record ���� ���� ����
                        //-----------------------------------------

                        // Plan Tree�� ���� Record ���� ���� ȹ��

                        if( ( QC_PRIVATE_TMPLATE(aStatement)->tmplate.rows[i].lflag &
                              MTC_TUPLE_PARTITIONED_TABLE_MASK ) ==
                            MTC_TUPLE_PARTITIONED_TABLE_TRUE )
                        {
                            // PROJ-1502 PARTITIONED DISK TABLE
                            // ������ partition�� ���� record count�� ������
                            // �Ǿ������� �˻��ؾ� �Ѵ�.
                            IDE_DASSERT( sTableRef->tableInfo->tablePartitionType ==
                                         QCM_PARTITIONED_TABLE );

                            for( sPartitionRef = sTableRef->partitionRef;
                                 sPartitionRef != NULL;
                                 sPartitionRef = sPartitionRef->next )
                            {
                                sStatRecordCnt = sPartitionRef->statInfo->totalRecordCnt;

                                // PROJ-2248 ��������� ��踸 ����Ѵ�.
                                IDE_TEST( smiStatistics::getTableStatNumRow(
                                              sPartitionRef->partitionHandle,
                                              ID_TRUE,
                                              NULL,
                                              (SLong*)& sRecordCnt )
                                          != IDE_SUCCESS );

                                sRealRecordCnt =
                                    ( sRecordCnt == 0 ) ? 1 : ID_ULTODB( sRecordCnt );

                                //-----------------------------------------
                                // Record ���淮�� ��
                                //-----------------------------------------
                                sChangeRatio = (sRealRecordCnt - sStatRecordCnt)
                                    / sStatRecordCnt;

                                if ( (sChangeRatio > QMC_AUTO_REBUILD_PLAN_PLUS_RATIO) ||
                                     (sChangeRatio < QMC_AUTO_REBUILD_PLAN_MINUS_RATIO) )
                                {
                                    // Record ���淮�� ���� ���� �ش� Plan Tree��
                                    // �� �̻� ��ȿ���� �ʴ�.
                                    sIsOld = ID_TRUE;
                                    break;
                                }
                                else
                                {
                                    // Continue
                                }
                            }

                            if( sIsOld == ID_TRUE )
                            {
                                break;
                            }
                            else
                            {
                                // Nothing to do.
                            }
                        }
                        else
                        {
                            if ( ( sTableRef->tableInfo->tablePartitionType != QCM_PARTITIONED_TABLE ) &&
                                 ( qcuTemporaryObj::isTemporaryTable( sTableRef->tableInfo ) == ID_FALSE ) )
                            {
                                sStatRecordCnt = sTableRef->statInfo->totalRecordCnt;

                                // PROJ-2248 ��������� ��踸 ����Ѵ�.
                                IDE_TEST( smiStatistics::getTableStatNumRow(
                                              sTableRef->tableHandle,
                                              ID_TRUE,
                                              NULL,
                                              (SLong*)& sRecordCnt )
                                          != IDE_SUCCESS );

                                sRealRecordCnt =
                                    ( sRecordCnt == 0 ) ? 1 : ID_ULTODB( sRecordCnt );

                                //-----------------------------------------
                                // Record ���淮�� ��
                                //-----------------------------------------
                                sChangeRatio = (sRealRecordCnt - sStatRecordCnt)
                                    / sStatRecordCnt;

                                if ( (sChangeRatio > QMC_AUTO_REBUILD_PLAN_PLUS_RATIO) ||
                                     (sChangeRatio < QMC_AUTO_REBUILD_PLAN_MINUS_RATIO) )
                                {
                                    // Record ���淮�� ���� ���� �ش� Plan Tree��
                                    // �� �̻� ��ȿ���� �ʴ�.
                                    sIsOld = ID_TRUE;
                                    break;
                                }
                                else
                                {
                                    // Continue
                                }
                            }
                            else
                            {
                                // Continue
                            }
                        }
                    }
                    else
                    {
                        // Nothing to do.
                        // partition�� ���� tuple�� ����� �����Ѵ�.
                    }
                }
                else
                {
                    // RULE Based Hint�κ��� ������ ��� ������ ���
                    // �ش� Record ������ ���ϸ� �ȵȴ�.
                    // Nothing To Do
                }
            }
            else
            {
                // View�� ���
                // Nothing To Do
            }
        }
        else
        {
            // Nothing To Do
        }
    }

    IDE_EXCEPTION_CONT(NORMAL_EXIT);

    *aIsOld = sIsOld;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qmx::executeMove( qcStatement * aStatement )
{
/***********************************************************************
 *
 * Description :
 *    MOVE ... FROM ... �� execution ����
 *
 * Implementation :
 *    1. insert�� ���̺��� SMI_TABLE_LOCK_IX lock �� ��´�
 *    2. delete�� ���̺��� SMI_TABLE_LOCK_IX lock �� ��´�
 *    3. insert row trigger�ʿ����� �˻�
 *    4. delete row trigger�ʿ����� �˻�
 *    5. set SYSDATE
 *    6. �������� ����ϸ� �� ������ ã�Ƴ��´�
 *    7. qmnSCAN::init
 *    8. insert�� delete �� ���� Ŀ���� �����Ѵ�
 *    9.limit ���� ���� �ʱ�ȭ
 *    10. Child�� ���� Foreign Key Reference�� �ʿ伺 �˻�
 *    11. insert �� delete�� ���� cursor open
 *    12. loop�� ���鼭 ���� ���� ����
 *    12.1. delete�� ���̺��� row�ϳ� �о��
 *    12.2. insert row�� �����Ͽ� insert�� ���̺� ����
 *    12.3. �����ϴ� Ű�� ������,
 *          parent ���̺��� �÷��� ���� �����ϴ��� �˻�
 *    12.4. insert row trigger ����
 *    12.5. delete�� ���̺��� ���ڵ� ����
 *    12.6. delete row trigger ����
 *    13. childConstraints�˻�
 *    14. insert statement trigger ����
 *    15. delete statement trigger ����
 ***********************************************************************/
#define IDE_FN "qmx::executeMove"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(IDE_FN));

    qmmMoveParseTree  * sParseTree;
    qcmTableInfo      * sTableForInsert;
    qmsTableRef       * sTargetTableRef;
    qmsTableRef       * sSourceTableRef;

    idBool              sIsOld;
    qmcRowFlag          sFlag = QMC_ROW_INITIALIZE;
    idBool              sInitialized = ID_FALSE;

    //------------------------------------------
    // �⺻ ���� ȹ�� �� �ʱ�ȭ
    //------------------------------------------
    
    sParseTree = (qmmMoveParseTree *) aStatement->myPlan->parseTree;

    sTargetTableRef = sParseTree->targetTableRef;
    sSourceTableRef = sParseTree->querySet->SFWGH->from->tableRef;

    //------------------------------------------
    // PROJ-1502 PARTITIONED DISK TABLE
    // insert, delete�� ���̺��� lock �� ��´�
    //------------------------------------------
    
    IDE_TEST( lockTableForDML( aStatement,
                               sTargetTableRef,
                               SMI_TABLE_LOCK_IX )
              != IDE_SUCCESS );

    IDE_TEST( lockTableForDML( aStatement,
                               sSourceTableRef,
                               SMI_TABLE_LOCK_IX )
              != IDE_SUCCESS );

    //fix BUG-14080
    IDE_TEST( checkPlanTreeOld( aStatement, & sIsOld ) != IDE_SUCCESS );

    IDE_TEST_RAISE( sIsOld == ID_TRUE, err_plan_too_old );

    sTableForInsert = sTargetTableRef->tableInfo;

    //------------------------------------------
    // insert statement trigger ����
    //------------------------------------------
    // To fix BUG-12622
    // before trigger
    IDE_TEST( qdnTrigger::fireTrigger( aStatement,
                                       aStatement->qmxMem,
                                       sTableForInsert,
                                       QCM_TRIGGER_ACTION_EACH_STMT,
                                       QCM_TRIGGER_BEFORE,
                                       QCM_TRIGGER_EVENT_INSERT,
                                       NULL,  // UPDATE Column
                                       NULL,            /* Table Cursor */
                                       SC_NULL_GRID,    /* Row GRID */
                                       NULL,  // OLD ROW
                                       NULL,  // OLD ROW Column
                                       NULL,  // NEW ROW
                                       NULL ) // NEW ROW Column
              != IDE_SUCCESS );

    //------------------------------------------
    // delete statement trigger
    //------------------------------------------
    // To fix BUG-12622
    // before trigger
    IDE_TEST( fireStatementTriggerOnDeleteCascade(
                  aStatement,
                  sSourceTableRef,
                  sParseTree->childConstraints,
                  QCM_TRIGGER_BEFORE )
              != IDE_SUCCESS );

    //------------------------------------------
    // MOVE�� ó���ϱ� ���� �⺻ �� ȹ��
    //------------------------------------------
    
    // To fix BUG-12327 initialize numRows to 0.
    QC_PRIVATE_TMPLATE(aStatement)->numRows = 0;

    //------------------------------------------
    // set SYSDATE
    //------------------------------------------
    
    IDE_TEST( qtc::setDatePseudoColumn( QC_PRIVATE_TMPLATE( aStatement ) )
              != IDE_SUCCESS );

    //------------------------------------------
    // �������� ����ϸ� �� ������ ã�Ƴ��´�
    //------------------------------------------
    
    // check session cache SEQUENCE.CURRVAL
    if ( aStatement->myPlan->parseTree->currValSeqs != NULL )
    {
        IDE_TEST( findSessionSeqCaches( aStatement,
                                        aStatement->myPlan->parseTree )
                  != IDE_SUCCESS);
    }
    else
    {
        // Nothing to do
    }

    // get SEQUENCE.NEXTVAL
    if ( aStatement->myPlan->parseTree->nextValSeqs != NULL )
    {
        IDE_TEST( addSessionSeqCaches( aStatement,
                                       aStatement->myPlan->parseTree )
                  != IDE_SUCCESS);
    }
    else
    {
        // Nothing to do
    }
    
    //------------------------------------------
    // MOVE�� ���� plan tree �ʱ�ȭ
    //------------------------------------------
    
    // PROJ-1446 Host variable�� ������ ���� ����ȭ
    IDE_TEST( qmo::optimizeForHost( aStatement ) != IDE_SUCCESS );

    IDE_TEST( qmnMOVE::init( QC_PRIVATE_TMPLATE(aStatement),
                             aStatement->myPlan->plan)
              != IDE_SUCCESS );
    sInitialized = ID_TRUE;

    //------------------------------------------
    // MOVE�� ����
    //------------------------------------------

    do
    {
        // BUG-22287 insert �ð��� ���� �ɸ��� ��쿡 session
        // out�Ǿ insert�� ���� �� ���� ��ٸ��� ������ �ִ�.
        // session out�� Ȯ���Ѵ�.
        IDE_TEST( iduCheckSessionEvent( aStatement->mStatistics )
                  != IDE_SUCCESS );
        
        // �̸� ������Ų��. doIt�� ������ �� �ִ�.
        QC_PRIVATE_TMPLATE(aStatement)->numRows++;
        
        // move�� plan�� �����Ѵ�.
        IDE_TEST( qmnMOVE::doIt( QC_PRIVATE_TMPLATE(aStatement),
                                 aStatement->myPlan->plan,
                                 &sFlag )
                  != IDE_SUCCESS );

    } while ( ( sFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST );

    // DATA_NONE�� ���� ���ش�.
    QC_PRIVATE_TMPLATE(aStatement)->numRows--;
    
    //------------------------------------------
    // MOVE cursor close
    //------------------------------------------

    sInitialized = ID_FALSE;
    IDE_TEST( qmnMOVE::closeCursor( QC_PRIVATE_TMPLATE(aStatement),
                                    aStatement->myPlan->plan )
              != IDE_SUCCESS );
    
    /* PROJ-1071 */
    IDE_TEST(qcg::joinThread(aStatement) != IDE_SUCCESS);
    
    IDE_TEST( QC_PRIVATE_TMPLATE(aStatement)->cursorMgr->closeAllCursor(aStatement->mStatistics)
              != IDE_SUCCESS );
    
    IDE_TEST(qcg::finishAndReleaseThread(aStatement) != IDE_SUCCESS);
    
    //------------------------------------------
    // Foreign Key Reference�� �˻�
    //------------------------------------------
    
    if ( QC_PRIVATE_TMPLATE(aStatement)->numRows == 0 )
    {
        // (��1)
        // MOVE INTO PARENT FROM CHILD
        // WHERE I1 IN ( SELECT I1 FROM CHILD WHERE I1 = 100 )
        // �� where���� ������ in subquery keyRange�� ����Ǵ� ���ǹ��� ���,
        // child table�� ���ڵ尡 �ϳ��� ���� ���,
        // ���ǿ� �´� ���ڵ尡 �����Ƿ�  i1 in �� ���� keyRange�� ����� ����,
        // in subquery keyRange�� filter�ε� ó���� �� ���� ������.
        // SCAN��忡���� ���� cursor open�� ���� ���� ����̸�
        // cursor�� open���� �ʴ´�.
        // ����, ���� ���� ���� update�� �ο찡 �����Ƿ�,
        // close close�� child table�����˻�� ���� ó���� ���� �ʴ´�.
        // (��2)
        // MOVE INTO PARENT FROM CHILD WHERE 1 != 1;
        // �̿� ���� ��쵵 cursor�� open���� �ʴ´�.
    }
    else
    {
        //------------------------------------------
        // self reference�� ���� �ùٸ� ����� ����� ���ؼ���,
        // insert�� ��� ����ǰ� ���Ŀ�,
        // parent table�� ���� �˻縦 �����ؾ� ��.
        // ��) CREATE TABLE T1 ( I1 INTEGER, I2 INTEGER );
        //     CREATE TABLE T2 ( I1 INTEGER, I2 INTEGER );
        //     CREATE INDEX T2_I1 ON T2( I1 );
        //     ALTER TABLE T1 ADD CONSTRAINT T1_PK PRIMARY KEY (I1);
        //     ALTER TABLE T1 ADD CONSTRAINT
        //            T1_I2_FK FOREIGN KEY(I2) REFERENCES T1(I1);
        //
        //     insert into t2 values ( 1, 1 );
        //     insert into t2 values ( 2, 1 );
        //     insert into t2 values ( 3, 2 );
        //     insert into t2 values ( 4, 3 );
        //     insert into t2 values ( 5, 4 );
        //     insert into t2 values ( 6, 1 );
        //     insert into t2 values ( 7, 7 );
        //
        //     select * from t1 order by i1;
        //     select * from t2 order by i1;
        //
        //     insert into t1 select * from t2;
        //     : insert success�ؾ� ��.
        //      ( �� ������ �ϳ��� ���ڵ尡 �μ�Ʈ �ɶ�����
        //        parent���̺��� �����ϴ� �������� �ڵ��ϸ�,
        //        parent record is not found��� ������ �߻��ϴ� ������ ����. )
        //------------------------------------------

        // parent reference üũ
        if ( sParseTree->parentConstraints != NULL )
        {
            IDE_TEST( qmnMOVE::checkInsertRef(
                      QC_PRIVATE_TMPLATE(aStatement),
                      aStatement->myPlan->plan )
                  != IDE_SUCCESS );
        }
        else
        {
            // Nothing To Do
        }

        // Child Table�� �����ϰ� �ִ� ���� �˻�
        if ( sParseTree->childConstraints != NULL )
        {
            IDE_TEST( qmnMOVE::checkDeleteRef(
                          QC_PRIVATE_TMPLATE(aStatement),
                          aStatement->myPlan->plan )
                      != IDE_SUCCESS );
        }
        else
        {
            // Nothing To Do
        }
    }

    //------------------------------------------
    // insert statement trigger ����
    //------------------------------------------
    
    IDE_TEST( qdnTrigger::fireTrigger( aStatement,
                                       aStatement->qmxMem,
                                       sTableForInsert,
                                       QCM_TRIGGER_ACTION_EACH_STMT,
                                       QCM_TRIGGER_AFTER,
                                       QCM_TRIGGER_EVENT_INSERT,
                                       NULL,  // UPDATE Column
                                       NULL,            /* Table Cursor */
                                       SC_NULL_GRID,    /* Row GRID */
                                       NULL,  // OLD ROW
                                       NULL,  // OLD ROW Column
                                       NULL,  // NEW ROW
                                       NULL ) // NEW ROW Column
              != IDE_SUCCESS );

    //------------------------------------------
    // delete statement trigger ����
    //------------------------------------------
    
    IDE_TEST( fireStatementTriggerOnDeleteCascade(
                  aStatement,
                  sSourceTableRef,
                  sParseTree->childConstraints,
                  QCM_TRIGGER_AFTER ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    //fix BUG-14080
    IDE_EXCEPTION(err_plan_too_old);
    {
        IDE_SET(ideSetErrorCode(qpERR_REBUILD_QMX_TOO_OLD_PLANTREE));
    }
    IDE_EXCEPTION_END;

    QC_PRIVATE_TMPLATE(aStatement)->numRows = 0;

    if ( sInitialized == ID_TRUE )
    {
        (void) qmnMOVE::closeCursor( QC_PRIVATE_TMPLATE(aStatement),
                                     aStatement->myPlan->plan );
    }
    
    return IDE_FAILURE;

#undef IDE_FN
}

// PROJ-1502 PARTITIONED DISK TABLE

IDE_RC qmx::lockTableForDML( qcStatement      * aStatement,
                             qmsTableRef      * aTableRef,
                             smiTableLockMode   aLockMode )
{
/***********************************************************************
 *
 *  Description : PROJ-1502 Partitioned Disk Table
 *                ��Ƽ���� ����� DML ������ table lock
 *  TODO1502: qcm������ �ű��?
 *  Implementation :
 *      non-partitioned table : table�� IX
 *      partitioned table : table�� IX, partition�� IX
 *                  default partition�� ���� ���(range partitioned table)
 *                      empty partition�� ������ �ʰ� validate Object Ȯ�θ� 
 *
 ***********************************************************************/

    IDE_TEST(smiValidateAndLockObjects( (QC_SMI_STMT(aStatement))->getTrans(),
                                        aTableRef->tableHandle,
                                        aTableRef->tableSCN,
                                        SMI_TBSLV_DDL_DML, // TBS Validation �ɼ�
                                        aLockMode,
                                        ID_ULONG_MAX,
                                        ID_FALSE ) // BUG-28752 ����� Lock�� ������ Lock�� �����մϴ�.
             != IDE_SUCCESS);

    // BUG-47599
    // emptyPartiton�� lock�� �ɸ� add partition op�� lock �浹�� �߻��մϴ�.
    if ( aTableRef->mEmptyPartRef != NULL )
    {
        IDE_TEST( smiValidateObjects( aTableRef->mEmptyPartRef->partitionHandle,
                                      aTableRef->mEmptyPartRef->partitionSCN )
                  != IDE_SUCCESS);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmx::lockPartitionForDML( smiStatement     * aSmiStmt,
                                 qmsPartitionRef  * aPartitionRef,
                                 smiTableLockMode   aLockMode )
{
    IDE_TEST( smiValidateAndLockObjects( aSmiStmt->getTrans(),
                                         aPartitionRef->partitionHandle,
                                         aPartitionRef->partitionSCN,
                                         SMI_TBSLV_DDL_DML, // TBS Validation �ɼ�
                                         aLockMode,
                                         ID_ULONG_MAX,
                                         ID_FALSE ) // BUG-28752 ����� Lock�� ������ Lock�� �����մϴ�.
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmx::lockTableForUpdate( qcStatement * aStatement,
                         qmsTableRef * aTableRef,
                         qmnPlan     * aPlan )
{
    qmncUPTE  * sCodePlan;
    UInt        sTableType;
    idBool      sHasMemory = ID_FALSE;

    IDE_DASSERT( aPlan->type == QMN_UPTE );

    sCodePlan = (qmncUPTE*)aPlan;

    sTableType = aTableRef->tableFlag & SMI_TABLE_TYPE_MASK;

    /* PROJ-2464 hybrid partitioned table ���� */
    if ( aTableRef->partitionSummary != NULL )
    {
        if ( ( aTableRef->partitionSummary->memoryPartitionCount +
               aTableRef->partitionSummary->volatilePartitionCount ) > 0 )
        {
            sHasMemory = ID_TRUE;
        }
        else
        {
            /* Nothing to do */
        }
    }
    else
    {
        if ( ( sTableType == SMI_TABLE_MEMORY ) || ( sTableType == SMI_TABLE_VOLATILE ) )
        {
            sHasMemory = ID_TRUE;
        }
        else
        {
            /* Nothing to do */
        }
    }

    // BUG-28757 UPDATE_IN_PLACE�� ���� VOLATILE TABLESPACE�� ���� ����� ����.
    if( ( QCU_UPDATE_IN_PLACE == 1 ) && ( sHasMemory == ID_TRUE ) )
    {
        if ( ( sCodePlan->inplaceUpdate == ID_TRUE ) &&
             ( aStatement->mInplaceUpdateDisableFlag == ID_FALSE ) )
        {
            IDE_TEST( lockTableForDML( aStatement,
                                       aTableRef,
                                       SMI_TABLE_LOCK_X )
                      != IDE_SUCCESS );
        }
        else
        {
            IDE_TEST( lockTableForDML( aStatement,
                                       aTableRef,
                                       SMI_TABLE_LOCK_IX )
                      != IDE_SUCCESS );
        }
    }
    else
    {
        IDE_TEST( lockTableForDML( aStatement,
                                   aTableRef,
                                   SMI_TABLE_LOCK_IX )
                  != IDE_SUCCESS );
    }
    
    // PROJ-1502 PARTITIONED DISK TABLE
    // row movement�� �ִ� ��� inserTableRef�� �����Ǹ�,
    // �̿� ���� lock�� ȹ���Ͽ��� �Ѵ�.
    if( sCodePlan->insertTableRef != NULL )
    {
        IDE_TEST( lockTableForDML(
                      aStatement,
                      sCodePlan->insertTableRef,
                      SMI_TABLE_LOCK_IX )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmx::changeLobColumnInfo( qmxLobInfo * aLobInfo,
                          qcmColumn  * aColumns )
{
/***********************************************************************
 *
 *  Description : PROJ-1502 PARTITIONED DISK TABLE
 *                ������ partition�� �°� lob column�� �ٲ۴�.
 *                insert, update���� ����Ѵ�.
 *
 *  Implementation : parameter�� ���� column(partition�� column)��
 *                   �˻��ؼ� ���� �÷��� ������ �װ����� ��ü.
 *
 ***********************************************************************/

    qcmColumn * sColumn;
    UShort      i;

    if( aLobInfo != NULL )
    {
        for( i = 0; i < aLobInfo->count; i++ )
        {
            for( sColumn = aColumns;
                 sColumn != NULL;
                 sColumn = sColumn->next )
            {
                IDE_ASSERT( aLobInfo->column    != NULL );
                IDE_ASSERT( aLobInfo->column[i] != NULL );

                if( aLobInfo->column[i]->id == sColumn->basicInfo->column.id )
                {
                    IDE_DASSERT( ( sColumn->basicInfo->column.flag &
                                   SMI_COLUMN_TYPE_MASK ) ==
                                 SMI_COLUMN_TYPE_LOB );

                    aLobInfo->column[i] = &sColumn->basicInfo->column;
                }
                else
                {
                    // Nothing to do.
                }
            }
        }

        for( i = 0; i < aLobInfo->outCount; i++ )
        {
            for( sColumn = aColumns;
                 sColumn != NULL;
                 sColumn = sColumn->next )
            {
                IDE_ASSERT( aLobInfo->outColumn    != NULL );
                IDE_ASSERT( aLobInfo->outColumn[i] != NULL );

                if( aLobInfo->outColumn[i]->id == sColumn->basicInfo->column.id )
                {
                    IDE_DASSERT( ( sColumn->basicInfo->column.flag &
                                   SMI_COLUMN_TYPE_MASK ) ==
                                 SMI_COLUMN_TYPE_LOB );

                    aLobInfo->outColumn[i] = &sColumn->basicInfo->column;
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
        // Nothing to do.
    }

    return IDE_SUCCESS;
}

IDE_RC
qmx::makeSmiValueForChkRowMovement( smiColumnList * aUpdateColumns,
                                    smiValue      * aUpdateValues,
                                    qcmColumn     * aPartKeyColumns, /* Criterion for smiValue */
                                    mtcTuple      * aTuple,
                                    smiValue      * aChkValues )
{
    smiColumnList   * sUptColumn;
    qcmColumn       * sPartKeyColumn;
    idBool            sFound;
    SInt              sColumnOrder;
    UInt              i;
    UInt              sStoringSize = 0;
    void            * sStoringValue;
    void            * sValue;

    for( sPartKeyColumn = aPartKeyColumns;
         sPartKeyColumn != NULL;
         sPartKeyColumn = sPartKeyColumn->next )
    {
        sColumnOrder = sPartKeyColumn->basicInfo->column.id &
            SMI_COLUMN_ID_MASK;

        sFound = ID_FALSE;

        for ( i = 0, sUptColumn = aUpdateColumns;
              sUptColumn != NULL;
              i++, sUptColumn = sUptColumn->next )
        {
            if( sPartKeyColumn->basicInfo->column.id ==
                sUptColumn->column->id )
            {
                sFound = ID_TRUE;
                break;
            }
            else
            {
                // Nothing to do.
            }
        }

        if( sFound == ID_TRUE )
        {
            // update column�� �����.
            aChkValues[sColumnOrder].value = aUpdateValues[i].value;
            aChkValues[sColumnOrder].length = aUpdateValues[i].length;
        }
        else
        {
            sValue = (void*)mtc::value( &aTuple->columns[sColumnOrder],
                                        aTuple->row,
                                        MTD_OFFSET_USE );

            // update column�� �ƴϰ� row���� ���ؾ� �ϴ� �����.
            IDE_TEST( qdbCommon::mtdValue2StoringValue(
                          sPartKeyColumn->basicInfo,
                          &aTuple->columns[sColumnOrder],
                          sValue,
                          &sStoringValue )
                      != IDE_SUCCESS );
            aChkValues[sColumnOrder].value = sStoringValue;

            IDE_TEST( qdbCommon::storingSize(
                          sPartKeyColumn->basicInfo,
                          &aTuple->columns[sColumnOrder],
                          sValue,
                          &sStoringSize )
                      != IDE_SUCCESS );
            aChkValues[sColumnOrder].length = sStoringSize;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmx::makeSmiValueForRowMovement( qcmTableInfo   * aTableInfo, /* Criterion for smiValue */
                                 smiColumnList  * aUpdateColumns,
                                 smiValue       * aUpdateValues,
                                 mtcTuple       * aTuple,
                                 smiTableCursor * aCursor,
                                 qmxLobInfo     * aUptLobInfo,
                                 smiValue       * aInsValues,
                                 qmxLobInfo     * aInsLobInfo )
{
    smiColumnList   * sUptColumn;
    UInt              sInsColumnCount;
    mtcColumn       * sInsColumns;
    idBool            sFound;
    UInt              sIterator;
    UInt              i;
    smLobLocator      sLocator       = MTD_LOCATOR_NULL;
    UInt              sInfo          = MTC_LOB_LOCATOR_CLOSE_TRUE;
    scGRID            sGRID;
    mtcColumn       * sStoringColumn = NULL;
    SInt              sColumnOrder;
    UInt              sStoringSize   = 0;
    void            * sStoringValue  = NULL;
    void            * sValue;

    sInsColumns       = aTuple->columns;
    sInsColumnCount   = aTuple->columnCount;

    for( sIterator = 0;
         sIterator < sInsColumnCount;
         sIterator++ )
    {
        sFound = ID_FALSE;

        for ( i = 0, sUptColumn = aUpdateColumns;
              sUptColumn != NULL;
              i++, sUptColumn = sUptColumn->next )
        {
            if( sInsColumns[sIterator].column.id ==
                sUptColumn->column->id )
            {
                sFound = ID_TRUE;
                break;
            }
            else
            {
                // Nothing to do.
            }
        }

        if( sFound == ID_TRUE )
        {
            // update column�� �����.
            aInsValues[sIterator].value = aUpdateValues[i].value;
            aInsValues[sIterator].length = aUpdateValues[i].length;

            if ( ( sInsColumns[sIterator].module->id == MTD_BLOB_LOCATOR_ID ) ||
                 ( sInsColumns[sIterator].module->id == MTD_CLOB_LOCATOR_ID ) )
            {
                IDE_TEST( addLobInfoFromLobInfo( sInsColumns[sIterator].column.id,
                                                 aUptLobInfo,
                                                 aInsLobInfo )
                          != IDE_SUCCESS );
            }
            else
            {
                // Nothing to do.
            }
        }
        else
        {
            // update column�� �ƴϰ� row���� ���ؾ� �ϴ� �����.
            sColumnOrder = (sInsColumns[sIterator].column.id & SMI_COLUMN_ID_MASK);

            /* PROJ-2464 hybrid partitioned table ���� */
            sStoringColumn = aTableInfo->columns[sColumnOrder].basicInfo;

            if( ( sInsColumns[sIterator].column.flag &
                  SMI_COLUMN_TYPE_MASK )
                == SMI_COLUMN_TYPE_LOB )
            {
                aInsValues[sIterator].value = NULL;
                aInsValues[sIterator].length = 0;

                if ( (sInsColumns[sIterator].column.flag & SMI_COLUMN_STORAGE_MASK)
                     == SMI_COLUMN_STORAGE_MEMORY )
                {
                    if( smiIsNullLobColumn(aTuple->row, &sInsColumns[sIterator].column)
                        == ID_TRUE )
                    {
                        sLocator = MTD_LOCATOR_NULL;
                    }
                    else
                    {
                        // empty�� lob cursor�� ������.
                        IDE_TEST( smiLob::openLobCursorWithRow(
                                      aCursor,
                                      aTuple->row,
                                      & sInsColumns[sIterator].column,
                                      sInfo,
                                      SMI_LOB_READ_MODE,
                                      & sLocator )
                                  != IDE_SUCCESS );
                    }
                }
                else
                {
                    SC_COPY_GRID( aTuple->rid,
                                  sGRID );

                    if( SMI_GRID_IS_VIRTUAL_NULL(sGRID) )
                    {
                        sLocator = MTD_LOCATOR_NULL;
                    }
                    else
                    {
                        if ( smiIsNullLobColumn(aTuple->row,
                                                &sInsColumns[sIterator].column)
                             == ID_TRUE )
                        {
                            sLocator = MTD_LOCATOR_NULL;
                        }
                        else
                        {
                            // empty�� lob cursor�� ������.
                            IDE_TEST( smiLob::openLobCursorWithGRID(
                                          aCursor,
                                          sGRID,
                                          & sInsColumns[sIterator].column,
                                          sInfo,
                                          SMI_LOB_READ_MODE,
                                          & sLocator )
                                      != IDE_SUCCESS );
                        }
                    }
                }

                if ( sLocator != MTD_LOCATOR_NULL )
                {
                    // empty�� copy ����̴�.
                    IDE_TEST( qmx::addLobInfoForCopy(
                                  aInsLobInfo,
                                  & (sStoringColumn->column),
                                  sLocator)
                              != IDE_SUCCESS );

                    sLocator = MTD_LOCATOR_NULL;
                }
                else
                {
                    // Nothing to do.
                }
            }
            else
            {
                sValue = (void*)mtc::value( &sInsColumns[sIterator],
                                            aTuple->row,
                                            MTD_OFFSET_USE );
                /* PROJ-2334 PMT */
                IDE_TEST( qdbCommon::mtdValue2StoringValue(
                              sStoringColumn,
                              &sInsColumns[sIterator],
                              sValue,
                              &sStoringValue )
                          != IDE_SUCCESS );
                aInsValues[sIterator].value = sStoringValue;

                IDE_TEST( qdbCommon::storingSize(
                              sStoringColumn,
                              &sInsColumns[sIterator],
                              sValue,
                              &sStoringSize )
                          != IDE_SUCCESS );
                aInsValues[sIterator].length = sStoringSize;
            }
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    (void) qmx::closeLobLocator( NULL, /* idvSQL* */
                                 sLocator );

    return IDE_FAILURE;
}

IDE_RC
qmx::makeRowWithSmiValue( mtcColumn   * aColumn,
                          UInt          aColumnCount,
                          smiValue    * aValues,
                          void        * aRow )
{
/***********************************************************************
 *
 *  Description :
 *
 *  Implementation :
 *      smiValue list�� table row ���·� ����
 *
 ***********************************************************************/

    smiValue  * sCurrValue;
    mtcColumn * sSrcColumn;
    void      * sSrcValue;
    UInt        sSrcActualSize;
    void      * sDestValue;
    UInt        i;

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------
    
    IDE_DASSERT( aColumn != NULL );
    IDE_DASSERT( aValues != NULL );
    IDE_DASSERT( aRow != NULL );

    //------------------------------------------
    // ����
    //------------------------------------------
    
    for ( i = 0, sSrcColumn = aColumn, sCurrValue = aValues;
          i < aColumnCount;
          i++, sSrcColumn++, sCurrValue++ )
    {
        if ( ( sSrcColumn->column.flag & SMI_COLUMN_STORAGE_MASK )
             == SMI_COLUMN_STORAGE_MEMORY )
        {
            //---------------------------
            // MEMORY COLUMN 
            //---------------------------
                
            // variable null�� ������ �����.
            if( sCurrValue->length == 0 )
            {
                sSrcValue = sSrcColumn->module->staticNull;
            }
            else
            {
                sSrcValue = (void*) sCurrValue->value;
            }
            
            sDestValue = (void*)mtc::value( sSrcColumn,
                                            aRow,
                                            MTD_OFFSET_USE );
                
            sSrcActualSize = sSrcColumn->module->actualSize(
                sSrcColumn,
                sSrcValue );
                
            idlOS::memcpy( sDestValue, sSrcValue, sSrcActualSize );
        }
        else
        {
            //---------------------------
            // DISK COLUMN
            //---------------------------

            // PROJ-2429 Dictionary based data compress for on-disk DB 
            IDE_ASSERT( sSrcColumn->module->storedValue2MtdValue[MTD_COLUMN_COPY_FUNC_NORMAL](
                            sSrcColumn->column.size,
                            (UChar*)aRow + sSrcColumn->column.offset,
                            0,
                            sCurrValue->length,
                            sCurrValue->value )
                        == IDE_SUCCESS );
        }
    }
    
    return IDE_SUCCESS;
}

IDE_RC
qmx::addLobInfoFromLobInfo( UInt         aColumnID,
                            qmxLobInfo * aSrcLobInfo,
                            qmxLobInfo * aDestLobInfo )
{
/***********************************************************************
 *
 *  Description : PROJ-1502 PARTITIONED DISK TABLE
 *
 *  Implementation : row movement�� �Ͼ�� ��쿡 ���.
 *                   update �ϴ� lobInfo���� insert�ϴ� lobInfo��
 *                   lob column�� ������ �����Ѵ�.
 *
 ***********************************************************************/
    UInt i;

    if( ( aSrcLobInfo != NULL ) && ( aDestLobInfo != NULL ) )
    {
        for( i = 0; i < aSrcLobInfo->count; i++ )
        {
            if( aSrcLobInfo->column[i]->id == aColumnID )
            {
                IDE_TEST( addLobInfoForCopy(
                              aDestLobInfo,
                              aSrcLobInfo->column[i],
                              aSrcLobInfo->locator[i] )
                          != IDE_SUCCESS );
            }
            else
            {
                // Nothing to do.
            }
        }

        for( i = 0; i < aSrcLobInfo->outCount; i++ )
        {
            if( aSrcLobInfo->outColumn[i]->id == aColumnID )
            {
                IDE_TEST( addLobInfoForOutBind(
                              aDestLobInfo,
                              aSrcLobInfo->outColumn[i],
                              aSrcLobInfo->outBindId[i] )
                          != IDE_SUCCESS );
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

IDE_RC
qmx::checkNotNullColumnForInsert( qcmColumn  * aColumn,
                                  smiValue   * aInsertedRow,
                                  qmxLobInfo * aLobInfo,
                                  idBool       aPrintColumnName )
{
/***********************************************************************
 *
 * Description : To fix BUG-12622
 *               before triggerȣ�� �� not null üũ�� �ϵ��� �Լ��� ����.
 * Implementation :
 *
 ***********************************************************************/
    qcmColumn * sColumn      = NULL;
    SInt        sColumnOrder = 0;

    for ( sColumn = aColumn;
          sColumn != NULL;
          sColumn = sColumn->next )
    {
        // not null constraint�� �ִ� ���
        if( ( sColumn->basicInfo->flag &
              MTC_COLUMN_NOTNULL_MASK )
            == MTC_COLUMN_NOTNULL_TRUE )
        {
            if( existLobInfo(aLobInfo,
                             sColumn->basicInfo->column.id )
                == ID_FALSE )
            {
                sColumnOrder = sColumn->basicInfo->column.id & SMI_COLUMN_ID_MASK;

                if( ( ( sColumn->basicInfo->column.flag & SMI_COLUMN_STORAGE_MASK )
                      == SMI_COLUMN_STORAGE_MEMORY ) &&
                    ( ( sColumn->basicInfo->column.flag & SMI_COLUMN_TYPE_MASK )
                      == SMI_COLUMN_TYPE_FIXED ) )
                {
                    //---------------------------------------------------
                    // memory fixed column�� isNull�Լ��� null �˻�
                    // memory fixed column��
                    // mtdValue�� storingValue�� �����ϴ�.
                    //---------------------------------------------------

                    IDE_TEST_RAISE(
                        sColumn->basicInfo->module->isNull(
                            sColumn->basicInfo,
                            aInsertedRow[sColumnOrder].value ) == ID_TRUE,
                        ERR_NOT_ALLOW_NULL );
                }
                else
                {
                    //---------------------------------------------------
                    // variable, lob, disk column�� length�� 0�ΰ����� �˻�
                    //---------------------------------------------------

                    IDE_TEST_RAISE(
                        aInsertedRow[sColumnOrder].length == 0,
                        ERR_NOT_ALLOW_NULL );
                }
            }
            else
            {
                // lobinfo�� ���Ե� ���� �˻����� ����.
                // Nothing to do.
            }
        }
        else
        {
            // nothing to do.
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_NOT_ALLOW_NULL)
    {
        if ( aPrintColumnName == ID_TRUE )
        {
            /* BUG-45680 insert ����� not null column�� ���� �����޽��� ������ column ���� ���. */
                IDE_SET( ideSetErrorCode( qpERR_ABORT_QMX_NOT_NULL_CONSTRAINT,
                                          " : ",
                                          sColumn->name ) );
        }
        else
        {
            /* BUG-45680 insert ����� not null column�� ���� �����޽��� ������ column ���� ���. */
                IDE_SET( ideSetErrorCode( qpERR_ABORT_QMX_NOT_NULL_CONSTRAINT,
                                          "",
                                          "" ) );
        }
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

idBool
qmx::existLobInfo( qmxLobInfo * aLobInfo,
                   UInt         aColumnID )
{
/***********************************************************************
 *
 * Description : To fix BUG-12622
 *               lobinfo�� �ش� �÷�id �� ���� �ִ��� �˻�.
 * Implementation :
 *
 ***********************************************************************/
    
    UInt   i;
    idBool sExist = ID_FALSE;

    if( aLobInfo != NULL )
    {
        for( i = 0; i < aLobInfo->count; i++ )
        {
            IDE_ASSERT( aLobInfo->column    != NULL );
            IDE_ASSERT( aLobInfo->column[i] != NULL );

            if( aLobInfo->column[i]->id == aColumnID )
            {
                sExist = ID_TRUE;
                break;
            }
            else
            {
                // Nothing to do.
            }
        }

        if( sExist == ID_FALSE )
        {
            for( i = 0; i < aLobInfo->outCount; i++ )
            {
                IDE_ASSERT( aLobInfo->outColumn    != NULL );
                IDE_ASSERT( aLobInfo->outColumn[i] != NULL );

                if( aLobInfo->outColumn[i]->id ==
                    aColumnID )
                {
                    sExist = ID_TRUE;
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
    }
    else
    {
        // Nothing to do.
    }

    return sExist;
}

IDE_RC qmx::executeMerge( qcStatement * aStatement )
{
    qmmMergeParseTree   * sParseTree;
    qcStatement           sTempStatement;
    qmsTableRef         * sTableRef;
    idBool                sIsPlanOld;
    idBool                sInitialized = ID_FALSE;
    vSLong                sMergedCount;
    qmcRowFlag            sFlag = QMC_ROW_INITIALIZE;

    //------------------------------------------
    // �⺻ ���� ����
    //------------------------------------------
    
    sParseTree = (qmmMergeParseTree *) aStatement->myPlan->parseTree;

    //------------------------------------------
    // �ش� Table�� ������ �´� IX lock�� ȹ����.
    //------------------------------------------

    // BUG-44807 on clause�� subqery�� unnesting�Ǵ� ��� selectTargetParseTree��
    // tableRef�� tableHandle������ NULL�� �Ǵ� ��찡 �־� ���� ������ parseTree 
    // ����ϵ��� ��.
    if ( sParseTree->updateParseTree != NULL )
    {
        sTableRef = sParseTree->updateParseTree->querySet->SFWGH->from->tableRef;        
    }
    else
    {
        if ( sParseTree->insertParseTree != NULL )
        {
            sTableRef = sParseTree->insertParseTree->insertTableRef;        
        }
        else
        {
            sTableRef = sParseTree->insertNoRowsParseTree->insertTableRef;        
        }
    }
    
    if ( sParseTree->updateStatement != NULL )
    {
        // update plan�� children�� QMO_MERGE_UPDATE_IDX��°�� �ִ�.
        IDE_TEST( lockTableForUpdate(
                      aStatement,
                      sTableRef,
                      aStatement->myPlan->plan->children[QMO_MERGE_UPDATE_IDX].childPlan )
                  != IDE_SUCCESS );
    }
    else
    {
        IDE_TEST( lockTableForDML( aStatement,
                                   sTableRef,
                                   SMI_TABLE_LOCK_IX )
                  != IDE_SUCCESS );
    }

    sIsPlanOld = ID_FALSE;
    IDE_TEST( checkPlanTreeOld(aStatement, &sIsPlanOld) != IDE_SUCCESS );
    
    IDE_TEST_RAISE( sIsPlanOld == ID_TRUE, err_plan_too_old );

    //------------------------------------------
    // SELCT SOURCE�� ���� �⺻ ���� ����
    //------------------------------------------

    idlOS::memcpy( & sTempStatement,
                   sParseTree->selectSourceStatement,
                   ID_SIZEOF( qcStatement ) );
    setSubStatement( aStatement, & sTempStatement );
    
    // PROJ-1446 Host variable�� ������ ���� ����ȭ
    IDE_TEST( qmo::optimizeForHost( & sTempStatement ) != IDE_SUCCESS );
    
    //------------------------------------------
    // SELCT TARGET�� ���� �⺻ ���� ����
    //------------------------------------------
    
    idlOS::memcpy( & sTempStatement,
                   sParseTree->selectTargetStatement,
                   ID_SIZEOF( qcStatement ) );
    setSubStatement( aStatement, & sTempStatement );
    
    // PROJ-1446 Host variable�� ������ ���� ����ȭ
    IDE_TEST( qmo::optimizeForHost( & sTempStatement ) != IDE_SUCCESS );
    
    //------------------------------------------
    // UPDATE�� ���� �⺻ ���� ����
    //------------------------------------------
    
    if ( sParseTree->updateStatement != NULL )
    {
        //------------------------------------------
        // STATEMENT GRANULARITY TRIGGER�� ����
        //------------------------------------------
        
        IDE_TEST( qdnTrigger::fireTrigger( aStatement,
                                           aStatement->qmxMem,
                                           sTableRef->tableInfo,
                                           QCM_TRIGGER_ACTION_EACH_STMT,
                                           QCM_TRIGGER_BEFORE,
                                           QCM_TRIGGER_EVENT_UPDATE,
                                           sParseTree->updateParseTree->uptColumnList, // UPDATE Column
                                           NULL,            /* Table Cursor */
                                           SC_NULL_GRID,    /* Row GRID */
                                           NULL,  // OLD ROW
                                           NULL,  // OLD ROW Column
                                           NULL,  // NEW ROW
                                           NULL ) // NEW ROW Column
                  != IDE_SUCCESS );

        //------------------------------------------
        // sequence ���� ����
        //------------------------------------------
        
        if ( sParseTree->updateParseTree->common.currValSeqs != NULL )
        {
            IDE_TEST( findSessionSeqCaches( aStatement,
                                            (qcParseTree*) sParseTree->updateParseTree )
                      != IDE_SUCCESS );
        }
        else
        {
            /* nothing to do */
        }

        if ( sParseTree->updateParseTree->common.nextValSeqs != NULL )
        {
            IDE_TEST( addSessionSeqCaches( aStatement,
                                           (qcParseTree*) sParseTree->updateParseTree )
                      != IDE_SUCCESS );
        }
        else
        {
            /* nothing to do */
        }
        
        idlOS::memcpy( & sTempStatement,
                       sParseTree->updateStatement,
                       ID_SIZEOF( qcStatement ) );
        setSubStatement( aStatement, & sTempStatement );
        
        // PROJ-1446 Host variable�� ������ ���� ����ȭ
        IDE_TEST( qmo::optimizeForHost( & sTempStatement ) != IDE_SUCCESS );
    }
    else
    {
        /* nothing to do */
    }

    //------------------------------------------
    // DELTE�� ���� �⺻ ���� ����
    //------------------------------------------
    
    if ( sParseTree->deleteStatement != NULL )
    {
        //------------------------------------------
        // STATEMENT GRANULARITY TRIGGER�� ����
        //------------------------------------------
        
        IDE_TEST( fireStatementTriggerOnDeleteCascade(
                      aStatement,
                      sTableRef,
                      sParseTree->deleteParseTree->childConstraints,
                      QCM_TRIGGER_BEFORE )
                  != IDE_SUCCESS );
        
        idlOS::memcpy( & sTempStatement,
                       sParseTree->deleteStatement,
                       ID_SIZEOF( qcStatement ) );
        setSubStatement( aStatement, & sTempStatement );
        
        // PROJ-1446 Host variable�� ������ ���� ����ȭ
        IDE_TEST( qmo::optimizeForHost( & sTempStatement ) != IDE_SUCCESS );
    }
    else
    {
        /* nothing to do */
    }

    //------------------------------------------
    // INSERT�� ���� �⺻ ���� ����
    //------------------------------------------
    
    if ( sParseTree->insertParseTree != NULL )
    {
        //------------------------------------------
        // STATEMENT GRANULARITY TRIGGER�� ����
        //------------------------------------------
        
        IDE_TEST( qdnTrigger::fireTrigger( aStatement,
                                           aStatement->qmxMem,
                                           sTableRef->tableInfo,
                                           QCM_TRIGGER_ACTION_EACH_STMT,
                                           QCM_TRIGGER_BEFORE,
                                           QCM_TRIGGER_EVENT_INSERT,
                                           NULL,  // UPDATE Column
                                           NULL,            /* Table Cursor */
                                           SC_NULL_GRID,    /* Row GRID */
                                           NULL,  // OLD ROW
                                           NULL,  // OLD ROW Column
                                           NULL,  // NEW ROW
                                           NULL ) // NEW ROW Column
                  != IDE_SUCCESS );

        //------------------------------------------
        // sequence ���� ����
        //------------------------------------------
        
        if ( sParseTree->insertParseTree->common.currValSeqs != NULL )
        {
            IDE_TEST( findSessionSeqCaches( aStatement,
                                            (qcParseTree*) sParseTree->insertParseTree )
                      != IDE_SUCCESS );
        }
        else
        {
            // Nothing to do
        }
        
        if ( sParseTree->insertParseTree->common.nextValSeqs != NULL )
        {
            IDE_TEST( addSessionSeqCaches( aStatement,
                                           (qcParseTree*) sParseTree->insertParseTree )
                      != IDE_SUCCESS );
        }
        else
        {
            // Nothing to do
        }
    }
    else
    {
        /* nothing to do */
    }

    //------------------------------------------
    // MERGE�� ���� �⺻ ���� ����
    //------------------------------------------

    QC_PRIVATE_TMPLATE(aStatement)->numRows = 0;
    
    IDE_TEST( qtc::setDatePseudoColumn( QC_PRIVATE_TMPLATE( aStatement ) ) != IDE_SUCCESS );

    //------------------------------------------
    // MERGE�� ���� plan tree �ʱ�ȭ
    //------------------------------------------
    
    IDE_TEST( qmnMRGE::init( QC_PRIVATE_TMPLATE(aStatement),
                             aStatement->myPlan->plan )
             != IDE_SUCCESS);
    sInitialized = ID_TRUE;

    //------------------------------------------
    // MERGE�� ����
    //------------------------------------------

    do
    {
        // BUG-22287 insert �ð��� ���� �ɸ��� ��쿡 session
        // out�Ǿ insert�� ���� �� ���� ��ٸ��� ������ �ִ�.
        // session out�� Ȯ���Ѵ�.
        IDE_TEST( iduCheckSessionEvent( aStatement->mStatistics )
                  != IDE_SUCCESS );
        
        // merge�� plan�� �����Ѵ�.
        IDE_TEST( qmnMRGE::doIt( QC_PRIVATE_TMPLATE(aStatement),
                                 aStatement->myPlan->plan,
                                 & sFlag )
                  != IDE_SUCCESS );

        // merge count�� update row count�� insert row count�� �մϴ�.
        // 1ȸ doIt�� ���� row�� merge�� �� �ִ�.
        IDE_TEST( qmnMRGE::getMergedRowCount( QC_PRIVATE_TMPLATE(aStatement),
                                              aStatement->myPlan->plan,
                                              & sMergedCount )
                  != IDE_SUCCESS );

        QC_PRIVATE_TMPLATE(aStatement)->numRows += sMergedCount;
        
    } while ( ( sFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST );

    //------------------------------------------
    // MERGE cursor close
    //------------------------------------------

    sInitialized = ID_FALSE;
    IDE_TEST( qmnMRGE::closeCursor( QC_PRIVATE_TMPLATE(aStatement),
                                    aStatement->myPlan->plan )
              != IDE_SUCCESS );
    
    /* PROJ-1071 */
    IDE_TEST(qcg::joinThread(aStatement) != IDE_SUCCESS);
    
    IDE_TEST( QC_PRIVATE_TMPLATE(aStatement)->cursorMgr->closeAllCursor(aStatement->mStatistics)
              != IDE_SUCCESS );

    IDE_TEST(qcg::finishAndReleaseThread(aStatement) != IDE_SUCCESS);
    
    //------------------------------------------
    // UPDATE STATEMENT GRANULARITY TRIGGER�� ����
    //------------------------------------------
        
    if ( sParseTree->updateStatement != NULL )
    {
        IDE_TEST( qdnTrigger::fireTrigger( aStatement,
                                           aStatement->qmxMem,
                                           sTableRef->tableInfo,
                                           QCM_TRIGGER_ACTION_EACH_STMT,
                                           QCM_TRIGGER_AFTER,
                                           QCM_TRIGGER_EVENT_UPDATE,
                                           sParseTree->updateParseTree->uptColumnList, // UPDATE Column
                                           NULL,            /* Table Cursor */
                                           SC_NULL_GRID,    /* Row GRID */
                                           NULL,  // OLD ROW
                                           NULL,  // OLD ROW Column
                                           NULL,  // NEW ROW
                                           NULL ) // NEW ROW Column
                  != IDE_SUCCESS );
    }
    else
    {
        /* nothing to do */
    }

    //------------------------------------------
    // DELETE STATEMENT GRANULARITY TRIGGER�� ����
    //------------------------------------------
    
    if ( sParseTree->deleteStatement != NULL )
    {
        IDE_TEST( fireStatementTriggerOnDeleteCascade(
                      aStatement,
                      sTableRef,
                      sParseTree->deleteParseTree->childConstraints,
                      QCM_TRIGGER_AFTER )
                  != IDE_SUCCESS );
    }
    else
    {
        /* nothing to do */
    }

    //------------------------------------------
    // INSERT STATEMENT GRANULARITY TRIGGER�� ����
    //------------------------------------------
        
    if ( sParseTree->insertParseTree != NULL )
    {
        IDE_TEST( qdnTrigger::fireTrigger( aStatement,
                                           aStatement->qmxMem,
                                           sTableRef->tableInfo,
                                           QCM_TRIGGER_ACTION_EACH_STMT,
                                           QCM_TRIGGER_AFTER,
                                           QCM_TRIGGER_EVENT_INSERT,
                                           NULL,  // UPDATE Column
                                           NULL,            /* Table Cursor */
                                           SC_NULL_GRID,    /* Row GRID */
                                           NULL,  // OLD ROW
                                           NULL,  // OLD ROW Column
                                           NULL,  // NEW ROW
                                           NULL ) // NEW ROW Column
                  != IDE_SUCCESS );
    }
    else
    {
        /* nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_plan_too_old)
    {
        IDE_SET( ideSetErrorCode(qpERR_REBUILD_QMX_TOO_OLD_PLANTREE) );
    }
    IDE_EXCEPTION_END;

    QC_PRIVATE_TMPLATE(aStatement)->numRows = 0;

    if ( sInitialized == ID_TRUE )
    {
        (void) qmnMRGE::closeCursor( QC_PRIVATE_TMPLATE(aStatement),
                                     aStatement->myPlan->plan );
    }
    
    return IDE_FAILURE;
}

IDE_RC qmx::executeShardDML( qcStatement * aStatement )
{
    qmcRowFlag  sFlag = QMC_ROW_INITIALIZE;

    // initialize result row count
    QC_PRIVATE_TMPLATE(aStatement)->numRows = 0;

    // set SYSDATE
    IDE_TEST( qtc::setDatePseudoColumn( QC_PRIVATE_TMPLATE( aStatement ) )
              != IDE_SUCCESS );

    // check session cache SEQUENCE.CURRVAL
    if (aStatement->myPlan->parseTree->currValSeqs != NULL)
    {
        IDE_TEST(findSessionSeqCaches(aStatement,
                                      aStatement->myPlan->parseTree) != IDE_SUCCESS);
    }
    else
    {
        // Nothing to do
    }

    // get SEQUENCE.NEXTVAL
    if (aStatement->myPlan->parseTree->nextValSeqs != NULL)
    {
        IDE_TEST(addSessionSeqCaches(aStatement,
                                     aStatement->myPlan->parseTree) != IDE_SUCCESS);
    }
    else
    {
        // Nothing to do
    }

    // init�� �����Ѵ�.
    IDE_TEST( qmnSDEX::init( QC_PRIVATE_TMPLATE(aStatement),
                             aStatement->myPlan->plan)
             != IDE_SUCCESS);

    // plan�� �����Ѵ�.
    IDE_TEST( qmnSDEX::doIt( QC_PRIVATE_TMPLATE(aStatement),
                             aStatement->myPlan->plan,
                             &sFlag )
              != IDE_SUCCESS );

    IDE_TEST( QC_PRIVATE_TMPLATE(aStatement)->cursorMgr->closeAllCursor(aStatement->mStatistics)
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    /* BUG-47459 */
    qmnSDEX::shardStmtPartialRollbackUsingSavepoint( QC_PRIVATE_TMPLATE(aStatement),
                                                     aStatement->myPlan->plan );

    return IDE_FAILURE;
}

IDE_RC qmx::executeShardInsert( qcStatement * aStatement )
{
    qmmInsParseTree   * sParseTree = NULL;
    qmsTableRef       * sTableRef  = NULL;
    qmcRowFlag          sRowFlag = QMC_ROW_INITIALIZE;

    IDE_DASSERT( aStatement != NULL );

    //------------------------------------------
    // �⺻ ���� ����
    //------------------------------------------

    sParseTree = (qmmInsParseTree *) aStatement->myPlan->parseTree;
    sTableRef  = sParseTree->insertTableRef;

    //------------------------------------------
    // �ش� Table�� IS lock�� ȹ����.
    //------------------------------------------

    IDE_TEST( lockTableForDML( aStatement,
                               sTableRef,
                               SMI_TABLE_LOCK_IS )
              != IDE_SUCCESS );

    //------------------------------------------
    // INSERT�� ó���ϱ� ���� �⺻ �� ȹ��
    //------------------------------------------

    // initialize result row count
    QC_PRIVATE_TMPLATE(aStatement)->numRows = 0;

    // set SYSDATE
    IDE_TEST( qtc::setDatePseudoColumn( QC_PRIVATE_TMPLATE( aStatement ) )
              != IDE_SUCCESS );

    // check session cache SEQUENCE.CURRVAL
    if (aStatement->myPlan->parseTree->currValSeqs != NULL)
    {
        IDE_TEST(findSessionSeqCaches(aStatement,
                                      aStatement->myPlan->parseTree)
                 != IDE_SUCCESS);
    }
    else
    {
        // Nothing to do
    }

    // get SEQUENCE.NEXTVAL
    if (aStatement->myPlan->parseTree->nextValSeqs != NULL)
    {
        IDE_TEST(addSessionSeqCaches(aStatement,
                                     aStatement->myPlan->parseTree)
                 != IDE_SUCCESS);
    }
    else
    {
        // Nothing to do
    }

    if (sParseTree->select != NULL)
    {
        // check session cache SEQUENCE.CURRVAL
        if (sParseTree->select->myPlan->parseTree->currValSeqs != NULL)
        {
            IDE_TEST(findSessionSeqCaches(aStatement,
                                          sParseTree->select->myPlan->parseTree)
                     != IDE_SUCCESS);
        }
        else
        {
            // Nothing to do
        }

        // get SEQUENCE.NEXTVAL
        if (sParseTree->select->myPlan->parseTree->nextValSeqs != NULL)
        {
            IDE_TEST(addSessionSeqCaches(aStatement,
                                         sParseTree->select->myPlan->parseTree)
                     != IDE_SUCCESS);
        }
        else
        {
            // Nothing to do
        }
    }
    else
    {
        // Nothing to do
    }

    //------------------------------------------
    // INSERT Plan �ʱ�ȭ
    //------------------------------------------

    IDE_TEST( qmnSDIN::init( QC_PRIVATE_TMPLATE(aStatement),
                             aStatement->myPlan->plan )
             != IDE_SUCCESS);

    //------------------------------------------
    // INSERT�� ����
    //------------------------------------------

    do
    {
        // BUG-22287 insert �ð��� ���� �ɸ��� ��쿡 session
        // out�Ǿ insert�� ���� �� ���� ��ٸ��� ������ �ִ�.
        // session out�� Ȯ���Ѵ�.
        IDE_TEST( iduCheckSessionEvent( aStatement->mStatistics )
                  != IDE_SUCCESS );

        // update�� plan�� �����Ѵ�.
        IDE_TEST( qmnSDIN::doIt( QC_PRIVATE_TMPLATE(aStatement),
                                 aStatement->myPlan->plan,
                                 &sRowFlag )
                  != IDE_SUCCESS );

    } while ( ( sRowFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST );

    /* PROJ-1071 */
    IDE_TEST(qcg::joinThread(aStatement) != IDE_SUCCESS);

    IDE_TEST(qcg::finishAndReleaseThread(aStatement) != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    qmnSDIN::shardStmtPartialRollbackUsingSavepoint( QC_PRIVATE_TMPLATE(aStatement), 
                                                     aStatement->myPlan->plan );

    QC_PRIVATE_TMPLATE(aStatement)->numRows = 0;

    return IDE_FAILURE;
}

void qmx::setSubStatement( qcStatement * aStatement,
                           qcStatement * aSubStatement )
{
    IDE_ASSERT( aStatement != NULL );
    IDE_ASSERT( aSubStatement != NULL );

    QC_PRIVATE_TMPLATE(aSubStatement)       = QC_PRIVATE_TMPLATE(aStatement);
    QC_PRIVATE_TMPLATE(aSubStatement)->stmt = aStatement;
    QC_QMX_MEM(aSubStatement)               = QC_QMX_MEM(aStatement);
    /* BUG-48125 Merge Trigger Fatal */
    QC_QMT_MEM(aSubStatement)               = QC_QMT_MEM(aStatement);
    // BUG-36766
    aSubStatement->stmtInfo                 = aStatement->stmtInfo;
    QC_PRIVATE_TMPLATE(aSubStatement)->cursorMgr
        = QC_PRIVATE_TMPLATE(aStatement)->cursorMgr;
    QC_PRIVATE_TMPLATE(aSubStatement)->tempTableMgr
        = QC_PRIVATE_TMPLATE(aStatement)->tempTableMgr;
    aSubStatement->spvEnv           = aStatement->spvEnv;
    aSubStatement->spxEnv           = aStatement->spxEnv;
    aSubStatement->sharedFlag       = aStatement->sharedFlag;
    aSubStatement->mStatistics      = aStatement->mStatistics;
    aSubStatement->stmtMutexPtr     = aStatement->stmtMutexPtr;
    aSubStatement->planTreeFlag     = aStatement->planTreeFlag;
    aSubStatement->mRandomPlanInfo  = aStatement->mRandomPlanInfo;
    aSubStatement->disableLeftStore = aStatement->disableLeftStore;

    // BUG-38932 alloc, free cost of cursor mutex is too expensive.
    // merge �������� ������ Statement �� ����Ѵ�. mutex �� �Ű��־�� �Ѵ�.
    aSubStatement->mCursorMutex.mEntry = aStatement->mCursorMutex.mEntry;

    // BUG-41227 source select statement of merge query containing function
    //           reference already freed session information
    aSubStatement->myPlan       = &(aSubStatement->privatePlan);
    aSubStatement->session      = aStatement->session;
    QC_QXC_MEM(aSubStatement)   = QC_QXC_MEM(aStatement);

    // BUG-43158 Enhance statement list caching in PSM
    aSubStatement->mStmtList    = aStatement->mStmtList;
    aSubStatement->mStmtList2   = aStatement->mStmtList2;
}

IDE_RC
qmx::copyReturnToInto( qcTemplate      * aTemplate,
                       qmmReturnInto   * aReturnInto,
                       vSLong            aRowCnt )
{
/***********************************************************************
 *
 * Description : PROJ-1584 DML Return Clause
 *         RETURN Clause (Column Value) -> INTO Clause (Column Value)��
 *         ���� �Ͽ� �ش�.
 *
 *         PREPARE iSQL             => normalReturnToInto
 *         PSM Primitive Type       => normalReturnToInto
 *         PSM Record TYpe          => recordReturnToInto
 *         PSM Record Array TYpe    => recordReturnToInto
 *         PSM Primitive Array TYpe => arrayReturnToInto
 *
 * Implementation :
 *
 ***********************************************************************/
    
    mtcColumn          * sMtcColumn;
    qcTemplate         * sDestTemplate;
    qmmReturnInto      * sDestReturnInto;

    IDE_DASSERT( aTemplate != NULL );
    IDE_DASSERT( aReturnInto->returnValue != NULL );

    // BUG-42715
    // package ������ ���� ����� ���
    if ( aReturnInto->returnIntoValue->returningInto->node.objectID != QS_EMPTY_OID )
    {
        sDestTemplate   = aTemplate;
        sDestReturnInto = aReturnInto;
    }
    else
    {
        sDestTemplate   = aTemplate->stmt->parentInfo->parentTmplate;
        sDestReturnInto = aTemplate->stmt->parentInfo->parentReturnInto;
    }

    // PSM���� DML���� return into ���� ó���ϴ� ���
    if( ( sDestTemplate   != NULL ) &&
        ( sDestReturnInto != NULL ) )
    {
        sMtcColumn = QTC_TMPL_COLUMN(
                                 sDestTemplate,
                                 sDestReturnInto->returnIntoValue->returningInto );

        // Row Type, Record Type, Associative Array ������ Record Type��
        // qsxUtil::recordReturnToInto �Լ��� ȣ���Ѵ�.
        if( ( sMtcColumn->type.dataTypeId == MTD_ROWTYPE_ID ) ||
            ( sMtcColumn->type.dataTypeId == MTD_RECORDTYPE_ID ) )
        {
            IDE_TEST( qsxUtil::recordReturnToInto( aTemplate,
                                                   sDestTemplate,
                                                   aReturnInto->returnValue,
                                                   sDestReturnInto->returnIntoValue,
                                                   aRowCnt,
                                                   aReturnInto->bulkCollect )
                      != IDE_SUCCESS);
        }
        else
        {
            // Primitive Type�� Associative Array�̸�
            // qsxUtil::arrayReturnToInto �Լ��� ȣ���Ѵ�.
            if( aReturnInto->bulkCollect == ID_TRUE )
            {
                IDE_TEST( qsxUtil::arrayReturnToInto( aTemplate,
                                                      sDestTemplate,
                                                      aReturnInto->returnValue,
                                                      sDestReturnInto->returnIntoValue,
                                                      aRowCnt )
                          != IDE_SUCCESS);
            }
            else
            {
                IDE_TEST( qsxUtil::primitiveReturnToInto( aTemplate,
                                                          sDestTemplate,
                                                          aReturnInto->returnValue,
                                                          sDestReturnInto->returnIntoValue )
                          != IDE_SUCCESS );
            }
        }
    }
    else
    {
        // �Ϲ� DML���� return into ���� ó���ϴ� ���
        IDE_TEST( normalReturnToInto( aTemplate,
                                      aReturnInto )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmx::normalReturnToInto( qcTemplate    * aTemplate,
                         qmmReturnInto * aReturnInto )
{
/***********************************************************************
 *
 * Description : PROJ-1584 DML Return Clause
 *     iSQL RETURN Clause (Column Value) -> INTO Clause (Column Value)��
 *     ���� �Ͽ� �ش�.
 *
 * Implementation :
 *      (1) Return Clause -> INTO Clause value copy.
 *      (2) ��ȣȭ �÷��� decrypt �� copy.
 *
 ***********************************************************************/

    void               * sDestValue;
    void               * sSrcValue;
    mtcColumn          * sSrcColumn;
    mtcColumn          * sDestColumn;
    qmmReturnValue     * sReturnValue;
    qmmReturnIntoValue * sReturnIntoValue;

    sReturnValue     = aReturnInto->returnValue;
    sReturnIntoValue = aReturnInto->returnIntoValue;

    for( ;
         sReturnValue    != NULL;
         sReturnValue     = sReturnValue->next,
         sReturnIntoValue = sReturnIntoValue->next )
    {
        IDE_TEST( qtc::calculate( sReturnValue->returnExpr,
                                  aTemplate )
                  != IDE_SUCCESS );

        sSrcColumn = aTemplate->tmplate.stack->column;
        sSrcValue  = aTemplate->tmplate.stack->value;

        // decrypt�Լ��� �ٿ����Ƿ� ��ȣ�÷��� ���� �� ����.
        IDE_DASSERT( (sSrcColumn->module->flag & MTD_ENCRYPT_TYPE_MASK)
                     == MTD_ENCRYPT_TYPE_FALSE );
            
        IDE_TEST( qtc::calculate( sReturnIntoValue->returningInto,
                                  aTemplate )
                  != IDE_SUCCESS );

        sDestColumn = aTemplate->tmplate.stack->column;
        sDestValue  = aTemplate->tmplate.stack->value;

        // BUG-35195
        // into���� ȣ��Ʈ ���� Ÿ���� return������ �ö���� Ÿ���� �ƴ϶�
        // ����ڰ� bind�� Ÿ������ �ʱ�ȭ�Ǿ� �����Ƿ�
        // ����� �׻� �ǽð� �������Ѵ�.
        IDE_TEST( qsxUtil::assignPrimitiveValue( aTemplate->stmt->qmxMem,
                                                 sSrcColumn,
                                                 sSrcValue,
                                                 sDestColumn,
                                                 sDestValue,
                                                 & aTemplate->tmplate )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description : PROJ-2464 hybrid partitioned table ����
 *               Partitioned Table�� Tuple�� Table Partition���� ��ü�ϱ� ���� �ʿ��� ������ �����Ѵ�.
 *               Partitioned Table�� Tuple�� �����ϱ� ���� �ʿ��� ������ �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/
void qmx::copyMtcTupleForPartitionedTable( mtcTuple * aDstTuple,
                                           mtcTuple * aSrcTuple )
{
    aDstTuple->partitionTupleID = aSrcTuple->partitionTupleID;
    aDstTuple->columns          = aSrcTuple->columns;
    aDstTuple->rowOffset        = aSrcTuple->rowOffset;
    aDstTuple->rowMaximum       = aSrcTuple->rowMaximum;
    aDstTuple->tableHandle      = aSrcTuple->tableHandle;
}

void qmx::copyMtcTupleForPartitionDML( mtcTuple * aDstTuple,
                                       mtcTuple * aSrcTuple )
{
    aDstTuple->lflag            = aSrcTuple->lflag;

    aDstTuple->partitionTupleID = aSrcTuple->partitionTupleID;
    aDstTuple->columns          = aSrcTuple->columns;
    aDstTuple->rowOffset        = aSrcTuple->rowOffset;
    aDstTuple->rowMaximum       = aSrcTuple->rowMaximum;
    aDstTuple->tableHandle      = aSrcTuple->tableHandle;
}

void qmx::adjustMtcTupleForPartitionDML( mtcTuple * aDstTuple,
                                         mtcTuple * aSrcTuple )
{
    aDstTuple->lflag &= ~MTC_TUPLE_STORAGE_MASK;
    aDstTuple->lflag |= (aSrcTuple->lflag & MTC_TUPLE_STORAGE_MASK);

    aDstTuple->partitionTupleID = aSrcTuple->partitionTupleID;
    aDstTuple->columns          = aSrcTuple->columns;
    aDstTuple->rowOffset        = aSrcTuple->rowOffset;
    aDstTuple->rowMaximum       = aSrcTuple->rowMaximum;
    aDstTuple->tableHandle      = aSrcTuple->tableHandle;
}

/***********************************************************************
 *
 * Description : PROJ-2464 hybrid partitioned table ����
 *               Max Row Offset�� ���Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/
UInt qmx::getMaxRowOffset( mtcTemplate * aTemplate,
                           qmsTableRef * aTableRef )
{
    UInt sMaxRowOffset    = 0;
    UInt sDiskRowOffset   = 0;
    UInt sMemoryRowOffset = 0;

    if ( aTableRef->tableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
    {
        if ( aTableRef->partitionSummary->diskPartitionRef != NULL )
        {
            sDiskRowOffset = qmc::getRowOffsetForTuple(
                                      aTemplate,
                                      aTableRef->partitionSummary->diskPartitionRef->table );
        }
        else
        {
            sDiskRowOffset = 0;
        }

        if ( aTableRef->partitionSummary->memoryOrVolatilePartitionRef != NULL )
        {
            sMemoryRowOffset = qmc::getRowOffsetForTuple(
                                        aTemplate,
                                        aTableRef->partitionSummary->memoryOrVolatilePartitionRef->table );
        }
        else
        {
            sMemoryRowOffset = 0;
        }

        sMaxRowOffset = IDL_MAX( sDiskRowOffset, sMemoryRowOffset );
    }
    else
    {
        sMaxRowOffset = qmc::getRowOffsetForTuple( aTemplate, aTableRef->table );
    }

    return sMaxRowOffset;
}

IDE_RC qmx::setChkSmiValueList( void                 * aRow,
                                const smiColumnList  * aColumnList,
                                smiValue             * aSmiValues,
                                const smiValue      ** aSmiValuePtr )
{
/***********************************************************************
 *
 * Description : PROJ-1784 DML Without Retry
 *               ���� �� column list�� ���߾� value list�� �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/
    const smiColumnList  * sColumnList;
    void                 * sValue;
    void                 * sStoringValue;
    UInt                   sStoringSize;
    UInt                   i;

    if( aColumnList != NULL )
    {
        // PROJ-1784 DML Without Retry
        for ( sColumnList = aColumnList, i = 0;
              sColumnList != NULL;
              sColumnList = sColumnList->next, i++ )
        {
            sValue = (void*)mtc::value( (mtcColumn*) sColumnList->column,
                                        aRow,
                                        MTD_OFFSET_USE );

            IDE_TEST( qdbCommon::mtdValue2StoringValue(
                          (mtcColumn*) sColumnList->column,
                          (mtcColumn*) sColumnList->column,
                          sValue,
                          &sStoringValue ) != IDE_SUCCESS );

            IDE_TEST( qdbCommon::storingSize(
                          (mtcColumn*) sColumnList->column,
                          (mtcColumn*) sColumnList->column,
                          sValue,
                          &sStoringSize ) != IDE_SUCCESS );

            aSmiValues[i].value  = sStoringValue;
            aSmiValues[i].length = sStoringSize;
        }

        *aSmiValuePtr = aSmiValues;
    }
    else
    {
        *aSmiValuePtr = NULL;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

// PROJ-2264 Dictionary table
IDE_RC qmx::makeSmiValueForCompress( qcTemplate   * aTemplate,
                                     UInt           aCompressedTuple,
                                     smiValue     * aInsertedRow )
{

    mtcColumn         * sColumns;
    UInt                sColumnCount;
    UInt                sIterator;
    void              * sValue;
    void              * sTableHandle;
    void              * sIndexHeader;

    smiTableCursor      sCursor;
    UInt                sState = 0;

    sColumnCount   =
        aTemplate->tmplate.rows[aCompressedTuple].columnCount;
    sColumns       =
        aTemplate->tmplate.rows[aCompressedTuple].columns;

    for( sIterator = 0;
         sIterator < sColumnCount;
         sIterator++,
             sColumns++ )
    {
        if ( (sColumns->column.flag & SMI_COLUMN_COMPRESSION_MASK)
             == SMI_COLUMN_COMPRESSION_TRUE )
        {
            sTableHandle = (void *)smiGetTable( sColumns->column.mDictionaryTableOID );
            sIndexHeader = (void *)smiGetTableIndexes( sTableHandle, 0 );

            sValue = (void*)( (UChar*) aTemplate->tmplate.rows[aCompressedTuple].row
                                      + sColumns->column.offset );

            // PROJ-2397 Dictionary Table Handle Interface Re-factoring
            // 1. Dictionary table �� ���� �����ϴ��� ����, �� row �� OID �� �����´�.
            // 2. Null OID �� ��ȯ�Ǿ����� dictionary table �� ���� ���� ���̴�.
            IDE_TEST( qcmDictionary::makeDictValueForCompress( 
						QC_SMI_STMT( aTemplate->stmt ),
						sTableHandle,
						sIndexHeader,
						&(aInsertedRow[sIterator]),
						sValue )
                      != IDE_SUCCESS );

            // 3. smiValue �� dictionary table �� OID �� ����Ű���� �Ѵ�.

            // OID �� canonize�� �ʿ����.
            // OID �� memory table �̹Ƿ� mtd value �� storing value �� �����ϴ�.
            aInsertedRow[sIterator].value  = sValue;
            aInsertedRow[sIterator].length = ID_SIZEOF(smOID);
        }
        else
        {
            // Nothing to do.
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sState != 0 )
    {
        sCursor.close();
    }

    return IDE_FAILURE;
}

IDE_RC qmx::checkAccessOption( qcmTableInfo * aTableInfo,
                               idBool         aIsInsertion,
                               idBool         aCheckDmlConsistency )
{
/***********************************************************************
 *
 *  Description : PROJ-2359 Table/Partition Access Option
 *      Access Option�� ��������, ���̺�/��Ƽ�� ������ ������ �� Ȯ���Ѵ�.
 *
 *  Implementation :
 *
 ***********************************************************************/

    /* TASK-7307 DML Data Consistency in Shard */
    if ( aIsInsertion == ID_TRUE )
    {
        IDE_TEST_RAISE( ( aCheckDmlConsistency == ID_TRUE ) &&
                        ( aTableInfo->mIsUsable == ID_FALSE ),
                        ERR_TABLE_PARTITION_UNUSABLE );
    }

    switch ( aTableInfo->accessOption )
    {
        case QCM_ACCESS_OPTION_READ_ONLY :
            IDE_RAISE( ERR_TABLE_PARTITION_ACCESS_DENIED );
            break;

        case QCM_ACCESS_OPTION_READ_WRITE :
            break;

        case QCM_ACCESS_OPTION_READ_APPEND :
            IDE_TEST_RAISE( aIsInsertion != ID_TRUE,
                            ERR_TABLE_PARTITION_ACCESS_DENIED );
            break;

        default : /* QCM_ACCESS_OPTION_NONE */
            IDE_DASSERT( 0 );
            break;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_TABLE_PARTITION_ACCESS_DENIED );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMX_TABLE_PARTITION_ACCESS_DENIED,
                                  aTableInfo->name ) );
    }
    IDE_EXCEPTION( ERR_TABLE_PARTITION_UNUSABLE );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMX_TABLE_PARTITION_UNUSABLE,
                                  aTableInfo->name ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmx::checkAccessOptionForExistentRecord( qcmAccessOption   aAccessOption,
                                                void            * aTableHandle )
{
/***********************************************************************
 *
 *  Description : PROJ-2359 Table/Partition Access Option
 *      Access Option�� ��������, ���̺�/��Ƽ�� ������ ������ �� Ȯ���Ѵ�.
 *
 *  Implementation :
 *
 ***********************************************************************/

    qcmTableInfo * sTableInfo = NULL;

    switch ( aAccessOption )
    {
        case QCM_ACCESS_OPTION_READ_ONLY :
            IDE_RAISE( ERR_TABLE_PARTITION_ACCESS_DENIED );
            break;

        case QCM_ACCESS_OPTION_READ_WRITE :
            break;

        case QCM_ACCESS_OPTION_READ_APPEND :
            IDE_RAISE( ERR_TABLE_PARTITION_ACCESS_DENIED );
            break;

        default : /* QCM_ACCESS_OPTION_NONE */
            IDE_DASSERT( 0 );
            break;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_TABLE_PARTITION_ACCESS_DENIED );
    {
        (void)smiGetTableTempInfo( aTableHandle, (void**)&sTableInfo );

        IDE_DASSERT( sTableInfo != NULL );

        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMX_TABLE_PARTITION_ACCESS_DENIED,
                                  sTableInfo->name ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description :
 *    �����ü(Memory/Disk)�� ����Ͽ� smiValue�� ��ȯ�Ѵ�.
 *    Partitioned Table�� smiValue�� Table Partition�� smiValue�� ��� �뵵�� ����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/
IDE_RC qmx::makeSmiValueWithSmiValue( qcmTableInfo * aSrcTableInfo,
                                      qcmTableInfo * aDstTableInfo,
                                      qcmColumn    * aQcmColumn,
                                      UInt           aColumnCount,
                                      smiValue     * aSrcValueArr,
                                      smiValue     * aDstValueArr )
{
    qcmColumn * sQcmColumn = NULL;
    UInt        sColumnID  = 0;
    UInt        i          = 0;

    IDE_DASSERT( QCM_TABLE_TYPE_IS_DISK( aSrcTableInfo->tableFlag ) !=
                 QCM_TABLE_TYPE_IS_DISK( aDstTableInfo->tableFlag ) );

    if ( QCM_TABLE_TYPE_IS_DISK( aDstTableInfo->tableFlag ) == ID_TRUE )
    {
        for ( i = 0, sQcmColumn = aQcmColumn;
              (i < aColumnCount) && (sQcmColumn != NULL);
              i++, sQcmColumn = sQcmColumn->next )
        {
            sColumnID = sQcmColumn->basicInfo->column.id & SMI_COLUMN_ID_MASK;

            IDE_TEST( qdbCommon::adjustSmiValueToDisk( aSrcTableInfo->columns[sColumnID].basicInfo,
                                                       &(aSrcValueArr[i]),
                                                       aDstTableInfo->columns[sColumnID].basicInfo,
                                                       &(aDstValueArr[i]) )
                      != IDE_SUCCESS );
        }
    }
    else
    {
        for ( i = 0, sQcmColumn = aQcmColumn;
              (i < aColumnCount) && (sQcmColumn != NULL);
              i++, sQcmColumn = sQcmColumn->next )
        {
            sColumnID = sQcmColumn->basicInfo->column.id & SMI_COLUMN_ID_MASK;

            IDE_TEST( qdbCommon::adjustSmiValueToMemory( aSrcTableInfo->columns[sColumnID].basicInfo,
                                                         &(aSrcValueArr[i]),
                                                         aDstTableInfo->columns[sColumnID].basicInfo,
                                                         &(aDstValueArr[i]) )
                      != IDE_SUCCESS );
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**
 * PROJ-2714 Multiple Update Delete support
 *
 * Description :
 *    UPDATE ... SET ... �� execution ����
 *
 * Implementation :
 *    1. SMI_TABLE_LOCK_IX lock �� ��´� => smiValidateAndLockObjects
 *    2. set SYSDATE
 *    3. �������� ����ϸ� �� ������ ã�Ƴ��´�
 *    4. qmnSCAN::init
 *    5. update �Ǵ� �÷��� ID �� ã�Ƽ� sUpdateColumnIDs �迭�� �����
 *    6. childConstraints �� ������,
 *       1. update �Ǵ� ���ڵ带 ã�Ƽ� �������� ���� �����ϴ� child ���̺���
 *          �ִ��� �����Ŀ� �� ���� ���������� Ȯ���ϰ�,
 *          ������ ������ ��ȯ�Ѵ�.
 *    7. childConstraints �� ������, �ش��ϴ� ���ڵ带 �����Ѵ�.
 *    8. ����Ǵ� �÷��� �����ϴ� parenet �� ������, �������� ���� parent ��
 *       �����ϴ��� Ȯ���ϰ�, ���� ������ ������ ��ȯ�Ѵ�.
 */
IDE_RC qmx::executeMultiUpdate( qcStatement * aStatement )
{
    qmmUptParseTree   * sUptParseTree;
    qmmMultiTables    * sTmp;

    idBool              sIsOld;
    qmcRowFlag          sFlag = QMC_ROW_INITIALIZE;
    idBool              sInitialized = ID_FALSE;
    scGRID              sRowGRID;
    UInt                i;

    //------------------------------------------
    // �⺻ ���� ����
    //------------------------------------------

    sUptParseTree = (qmmUptParseTree *) aStatement->myPlan->parseTree;

    //------------------------------------------
    // �ش� Table�� ������ �´� IX lock�� ȹ����.
    //------------------------------------------

    // PROJ-1509
    // MEMORY table������
    // table�� trigger or foreign key�� �����ϸ�,
    // ���� ����/���İ��� �����ϱ� ����,
    // inplace update�� ���� �ʵ��� �ؾ� �Ѵ�.
    // table lock�� IX lock���� �⵵�� �Ѵ�.

    for ( sTmp = sUptParseTree->mTableList;
          sTmp != NULL;
          sTmp = sTmp->mNext )
    {
        IDE_TEST( lockTableForUpdate( aStatement,
                                      sTmp->mTableRef,
                                      aStatement->myPlan->plan )
                  != IDE_SUCCESS );
    }

    //fix BUG-14080
    IDE_TEST( checkPlanTreeOld( aStatement, & sIsOld ) != IDE_SUCCESS );

    IDE_TEST_RAISE( sIsOld == ID_TRUE, err_plan_too_old );

    //------------------------------------------
    // STATEMENT GRANULARITY TRIGGER�� ����
    //------------------------------------------
    // To fix BUG-12622
    // before trigger
    for ( sTmp = sUptParseTree->mTableList;
          sTmp != NULL;
          sTmp = sTmp->mNext )
    {
        IDE_TEST( qdnTrigger::fireTrigger( aStatement,
                                           aStatement->qmxMem,
                                           sTmp->mTableRef->tableInfo,
                                           QCM_TRIGGER_ACTION_EACH_STMT,
                                           QCM_TRIGGER_BEFORE,
                                           QCM_TRIGGER_EVENT_UPDATE,
                                           sTmp->mUptColumnList, // UPDATE Column
                                           NULL,            /* Table Cursor */
                                           SC_NULL_GRID,    /* Row GRID */
                                           NULL,  // OLD ROW
                                           NULL,  // OLD ROW Column
                                           NULL,  // NEW ROW
                                           NULL ) // NEW ROW Column
                  != IDE_SUCCESS );
    }

    //------------------------------------------
    // UPDATE�� ���� �⺻ ���� ����
    //------------------------------------------

    QC_PRIVATE_TMPLATE(aStatement)->numRows = 0;

    // set SYSDATE
    IDE_TEST( qtc::setDatePseudoColumn( QC_PRIVATE_TMPLATE( aStatement ) ) != IDE_SUCCESS );

    // check session cache SEQUENCE.CURRVAL
    if ( aStatement->myPlan->parseTree->currValSeqs != NULL )
    {
        IDE_TEST(findSessionSeqCaches(aStatement, aStatement->myPlan->parseTree)
                 != IDE_SUCCESS);
    }
    else
    {
        // Nothing To Do
    }

    // get SEQUENCE.NEXTVAL
    if ( aStatement->myPlan->parseTree->nextValSeqs != NULL )
    {
        IDE_TEST(addSessionSeqCaches(aStatement, aStatement->myPlan->parseTree)
                 != IDE_SUCCESS);
    }
    else
    {
        // Nothing To Do
    }

    //------------------------------------------
    // UPDATE�� ���� plan tree �ʱ�ȭ
    //------------------------------------------

    // PROJ-1446 Host variable�� ������ ���� ����ȭ
    IDE_TEST( qmo::optimizeForHost( aStatement ) != IDE_SUCCESS );

    IDE_TEST( qmnUPTE::init( QC_PRIVATE_TMPLATE(aStatement),
                             aStatement->myPlan->plan)
             != IDE_SUCCESS);

    sInitialized = ID_TRUE;

    //------------------------------------------
    // UPDATE�� ����
    //------------------------------------------

    do
    {
        // BUG-22287 insert �ð��� ���� �ɸ��� ��쿡 session
        // out�Ǿ insert�� ���� �� ���� ��ٸ��� ������ �ִ�.
        // session out�� Ȯ���Ѵ�.
        IDE_TEST( iduCheckSessionEvent( aStatement->mStatistics )
                  != IDE_SUCCESS );

        // �̸� ������Ų��. doIt�� ������ �� �ִ�.
        QC_PRIVATE_TMPLATE(aStatement)->numRows++;

        // update�� plan�� �����Ѵ�.
        IDE_TEST( qmnUPTE::doIt( QC_PRIVATE_TMPLATE(aStatement),
                                 aStatement->myPlan->plan,
                                 &sFlag )
                  != IDE_SUCCESS );

    } while ( ( sFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST );

    // DATA_NONE�� ���� ���ش�.
    QC_PRIVATE_TMPLATE(aStatement)->numRows--;

    IDE_TEST( qmnUPTE::getLastUpdatedRowGRID( QC_PRIVATE_TMPLATE(aStatement),
                                              aStatement->myPlan->plan,
                                              & sRowGRID )
              != IDE_SUCCESS );

    //------------------------------------------
    // UPDATE cursor close
    //------------------------------------------

    sInitialized = ID_FALSE;
    IDE_TEST( qmnUPTE::closeCursorMultiTable( QC_PRIVATE_TMPLATE(aStatement),
                                              aStatement->myPlan->plan )
              != IDE_SUCCESS );

    /* PROJ-1071 */
    IDE_TEST(qcg::joinThread(aStatement) != IDE_SUCCESS);

    IDE_TEST( QC_PRIVATE_TMPLATE(aStatement)->cursorMgr->closeAllCursor(aStatement->mStatistics)
              != IDE_SUCCESS );

    IDE_TEST(qcg::finishAndReleaseThread(aStatement) != IDE_SUCCESS);

    //------------------------------------------
    // Foreign Key Reference �˻�
    //------------------------------------------

    for ( sTmp = sUptParseTree->mTableList, i = 0;
          sTmp != NULL;
          sTmp = sTmp->mNext, i++ )
    {
        if ( QC_PRIVATE_TMPLATE(aStatement)->numRows == 0 )
        {
            // (��1)
            // UPDATE PARENT SET I1 = 6
            // WHERE I1 IN ( SELECT I1 FROM CHILD WHERE I1 = 100 )
            // �� where���� ������ in subquery keyRange�� ����Ǵ� ���ǹ��� ���,
            // child table�� ���ڵ尡 �ϳ��� ���� ���,
            // ���ǿ� �´� ���ڵ尡 �����Ƿ�  i1 in �� ���� keyRange�� ����� ����,
            // in subquery keyRange�� filter�ε� ó���� �� ���� ������.
            // SCAN��忡���� ���� cursor open�� ���� ���� ����̸�
            // cursor�� open���� �ʴ´�.
            // ����, ���� ���� ���� update�� �ο찡 �����Ƿ�,
            // close close�� child table�����˻�� ���� ó���� ���� �ʴ´�.
            // (��2)
            // update t1 set i1=1 where 1 != 1;
            // �� ��쵵 cursor�� open���� ����.
        }
        else
        {
            //------------------------------------------
            // Foreign Key Referencing�� ����
            // Master Table�� �����ϴ� �� �˻�
            // To Fix PR-10592
            // Cursor�� �ùٸ� ����� ���ؼ��� Master�� ���� �˻縦 ������ �Ŀ�
            // Child Table�� ���� �˻縦 �����Ͽ��� �Ѵ�.
            //------------------------------------------
            if ( sTmp->mParentConst != NULL )
            {
                IDE_TEST( qmnUPTE::checkUpdateParentRefMultiTable( QC_PRIVATE_TMPLATE( aStatement ),
                                                                   aStatement->myPlan->plan,
                                                                   sTmp,
                                                                   i )
                          != IDE_SUCCESS );
            }
            else
            {
                /* Nothing to do */
            }

            //------------------------------------------
            // Child Table�� ���� Referencing �˻�
            //------------------------------------------
            if ( sTmp->mChildConst != NULL )
            {
                IDE_TEST( qmnUPTE::checkUpdateChildRefMultiTable( QC_PRIVATE_TMPLATE( aStatement ),
                                                                  aStatement->myPlan->plan,
                                                                  sTmp )
                          != IDE_SUCCESS );
            }
            else
            {
                /* Nothing to do */
            }
        }
        //------------------------------------------
        // PROJ-1359 Trigger
        // STATEMENT GRANULARITY TRIGGER�� ����
        //------------------------------------------
        IDE_TEST( qdnTrigger::fireTrigger( aStatement,
                                           aStatement->qmxMem,
                                           sTmp->mTableRef->tableInfo,
                                           QCM_TRIGGER_ACTION_EACH_STMT,
                                           QCM_TRIGGER_AFTER,
                                           QCM_TRIGGER_EVENT_UPDATE,
                                           sTmp->mUptColumnList, // UPDATE Column
                                           NULL,            /* Table Cursor */
                                           SC_NULL_GRID,    /* Row GRID */
                                           NULL,  // OLD ROW
                                           NULL,  // OLD ROW Column
                                           NULL,  // NEW ROW
                                           NULL ) // NEW ROW Column
                  != IDE_SUCCESS );
    }

    // BUG-38129
    qcg::setLastModifiedRowGRID( aStatement, sRowGRID );

    return IDE_SUCCESS;

    //fix BUG-14080
    IDE_EXCEPTION(err_plan_too_old);
    {
        IDE_SET(ideSetErrorCode(qpERR_REBUILD_QMX_TOO_OLD_PLANTREE));
    }
    IDE_EXCEPTION_END;

    QC_PRIVATE_TMPLATE(aStatement)->numRows = 0;

    if ( sInitialized == ID_TRUE )
    {
        (void) qmnUPTE::closeCursorMultiTable( QC_PRIVATE_TMPLATE(aStatement),
                                               aStatement->myPlan->plan );
    }
    return IDE_FAILURE;
}

IDE_RC qmx::executeMultiDelete( qcStatement * aStatement )
{
    qmmDelParseTree   * sParseTree;
    qmmDelMultiTables * sTmp;
    UInt                i;
    idBool              sIsOld;
    qmcRowFlag          sFlag = QMC_ROW_INITIALIZE;
    idBool              sInitialized = ID_FALSE;

    //------------------------------------------
    // �⺻ ���� ȹ��
    //------------------------------------------
    sParseTree = (qmmDelParseTree *) aStatement->myPlan->parseTree;

    for ( sTmp = sParseTree->mTableList;
          sTmp != NULL;
          sTmp = sTmp->mNext )
    {
        IDE_TEST( lockTableForDML( aStatement,
                                   sTmp->mTableRef,
                                   SMI_TABLE_LOCK_IX )
                  != IDE_SUCCESS );
    }

    //fix BUG-14080
    IDE_TEST( checkPlanTreeOld( aStatement, & sIsOld ) != IDE_SUCCESS );

    IDE_TEST_RAISE( sIsOld == ID_TRUE, err_plan_too_old );

    for ( sTmp = sParseTree->mTableList;
          sTmp != NULL;
          sTmp = sTmp->mNext )
    {
        //-------------------------------------------------------
        // STATEMENT GRANULARITY TRIGGER�� ����
        //-------------------------------------------------------
        // To fix BUG-12622
        // before trigger
        IDE_TEST( fireStatementTriggerOnDeleteCascade(
                      aStatement,
                      sTmp->mTableRef,
                      sTmp->mChildConstraints,
                      QCM_TRIGGER_BEFORE )
                  != IDE_SUCCESS );
    }

    //------------------------------------------
    // DELETE �� ó���ϱ� ���� �⺻ �� ȹ��
    //------------------------------------------

    QC_PRIVATE_TMPLATE(aStatement)->numRows = 0;

    // set SYSDATE
    IDE_TEST( qtc::setDatePseudoColumn( QC_PRIVATE_TMPLATE( aStatement ) )
              != IDE_SUCCESS );

    // PROJ-1446 Host variable�� ������ ���� ����ȭ
    IDE_TEST( qmo::optimizeForHost( aStatement ) != IDE_SUCCESS );

    // PROJ-1502 PARTITIONED DISK TABLE
    IDE_TEST( qmnDETE::init( QC_PRIVATE_TMPLATE(aStatement),
                             aStatement->myPlan->plan )
              != IDE_SUCCESS);
    sInitialized = ID_TRUE;

    //------------------------------------------
    // DELETE�� ����
    //------------------------------------------

    do
    {
        // BUG-22287 insert �ð��� ���� �ɸ��� ��쿡 session
        // out�Ǿ insert�� ���� �� ���� ��ٸ��� ������ �ִ�.
        // session out�� Ȯ���Ѵ�.
        IDE_TEST( iduCheckSessionEvent( aStatement->mStatistics )
                  != IDE_SUCCESS );

        // �̸� ������Ų��. doIt�� ������ �� �ִ�.
        QC_PRIVATE_TMPLATE(aStatement)->numRows++;

        // delete�� plan�� �����Ѵ�.
        IDE_TEST( qmnDETE::doIt( QC_PRIVATE_TMPLATE(aStatement),
                                 aStatement->myPlan->plan,
                                 &sFlag )
                  != IDE_SUCCESS );

    } while ( ( sFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST );

    // DATA_NONE�� ���� ���ش�.
    QC_PRIVATE_TMPLATE(aStatement)->numRows--;

    //------------------------------------------
    // DELETE�� ���� ����ξ��� Cursor�� Close
    //------------------------------------------

    sInitialized = ID_FALSE;
    IDE_TEST( qmnDETE::closeCursorMultiTable( QC_PRIVATE_TMPLATE(aStatement),
                                              aStatement->myPlan->plan )
              != IDE_SUCCESS );

    /* PROJ-1071 */
    IDE_TEST(qcg::joinThread(aStatement) != IDE_SUCCESS);

    IDE_TEST( QC_PRIVATE_TMPLATE(aStatement)->cursorMgr->closeAllCursor(aStatement->mStatistics)
              != IDE_SUCCESS );

    IDE_TEST(qcg::finishAndReleaseThread(aStatement) != IDE_SUCCESS);

    //------------------------------------------
    // Foreign Key Reference �˻�
    //------------------------------------------

    // BUG-28049
    if ( QC_PRIVATE_TMPLATE(aStatement)->numRows > 0 )
    {
        for ( sTmp = sParseTree->mTableList, i = 0;
              sTmp != NULL;
              sTmp = sTmp->mNext, i++ )
        {
            if ( sTmp->mChildConstraints != NULL )
            {
                // Child Table�� �����ϰ� �ִ� ���� �˻�
                IDE_TEST( qmnDETE::checkDeleteRefMultiTable(
                              QC_PRIVATE_TMPLATE(aStatement),
                              aStatement->myPlan->plan,
                              sTmp,
                              i )
                          != IDE_SUCCESS );
            }
            else
            {
                /* Nothing to do */
            }
        }
    }
    else
    {
        // (��1)
        // DELETE FROM PARENT
        // WHERE I1 IN ( SELECT I1 FROM CHILD WHERE I1 = 100 ) �� ����
        // where���� ������ in subquery keyRange�� ����Ǵ� ���ǹ��� ���,
        // child table�� ���ڵ尡 �ϳ��� ���� ���,
        // ���ǿ� �´� ���ڵ尡 �����Ƿ�
        // i1 in �� ���� keyRange�� ����� ����,
        // in subquery keyRange�� filter�ε� ó���� �� ���� ������.
        // SCAN��忡���� ���� cursor open�� ���� ���� ����̸�
        // cursor�� open���� �ʴ´�.
        // ����, ���� ���� ���� update�� �ο찡 �����Ƿ�,
        // close close�� child table�����˻�� ���� ó���� ���� �ʴ´�.
        // (��2)
        // delete from t1 where 1 != 1;
        // �� ���� ��쿡�� cursor�� open���� ����.

        // Nothing to do.
    }

    for ( sTmp = sParseTree->mTableList;
          sTmp != NULL;
          sTmp = sTmp->mNext )
    {
        //-------------------------------------------------------
        // STATEMENT GRANULARITY TRIGGER�� ����
        //-------------------------------------------------------
        IDE_TEST( fireStatementTriggerOnDeleteCascade(
                      aStatement,
                      sTmp->mTableRef,
                      sTmp->mChildConstraints,
                      QCM_TRIGGER_AFTER )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_plan_too_old);
    {
        IDE_SET(ideSetErrorCode(qpERR_REBUILD_QMX_TOO_OLD_PLANTREE));
    }
    IDE_EXCEPTION_END;

    QC_PRIVATE_TMPLATE(aStatement)->numRows = 0;

    if ( sInitialized == ID_TRUE )
    {
        (void) qmnDETE::closeCursorMultiTable( QC_PRIVATE_TMPLATE(aStatement),
                                               aStatement->myPlan->plan );
    }

    return IDE_FAILURE;
}

IDE_RC qmx::makeSmiValueForSubqueryMultiTable( qcTemplate    * aTemplate,
                                               qcmTableInfo  * aTableInfo,
                                               qcmColumn     * aColumn,
                                               qmmValueNode  * aValues,
                                               qmmValueNode ** aValuesPos,
                                               qmmSubqueries * aSubquery,
                                               UInt            aCanonizedTuple,
                                               smiValue      * aUpdatedRow,
                                               mtdIsNullFunc * aIsNull,
                                               qmxLobInfo    * aLobInfo )
{
    qmmSubqueries     * sSubquery;
    qmmValueNode      * sValue;
    qmmValuePointer   * sValuePointer;
    mtcStack          * sStack;
    mtcColumn         * sColumn;
    void              * sCanonizedValue;
    SInt                sColumnOrder;
    qcmColumn         * sQcmColumn = NULL;
    qcmColumn         * sMetaColumn;
    UInt                sIterator;
    mtcColumn         * sMtcColumn;
    mtcEncryptInfo      sEncryptInfo;
    UInt                sStoringSize = 0;
    void              * sStoringValue;
    SLong               sLobLen;
    idBool              sIsNullLob;
    qtcNode           * sNode;
    UInt                i;

    // Value ���� ����
    for ( sSubquery = aSubquery;
          sSubquery != NULL;
          sSubquery = sSubquery->next )
    {
        IDE_TEST( qtc::calculate( sSubquery->subquery, aTemplate )
                  != IDE_SUCCESS );

        for ( sValuePointer = sSubquery->valuePointer,
                sStack = (mtcStack*)aTemplate->tmplate.stack->value;
              sValuePointer != NULL;
              sValuePointer = sValuePointer->next, sStack++ )
        {
            sNode = NULL;
            for ( sValue = aValues, i = 0; sValue != NULL; sValue = sValue->next, i++ )
            {
                if ( ( aValuesPos[i] == sValuePointer->valueNode ) )
                {
                    sNode = sValuePointer->valueNode->value;
                    break;
                }
                else
                {
                    /* Nothing to do */
                }
            }

            if ( ( sNode == NULL ) || ( sValue == NULL ) )
            {
                continue;
            }
            else
            {
                /* Nothing to do */
            }

            // Meta�÷��� ã�´�.
            for ( sIterator = 0, sQcmColumn = aColumn;
                  sIterator != sValue->order;
                  sIterator++, sQcmColumn = sQcmColumn->next )
            {
                // Nothing to do.
            }

            sColumnOrder = (sQcmColumn->basicInfo->column.id & SMI_COLUMN_ID_MASK);

            /* PROJ-2464 hybrid partitioned table ���� */
            sMetaColumn = &(aTableInfo->columns[sColumnOrder]);

            if ( (sStack->column->module->id == MTD_BLOB_LOCATOR_ID) ||
                 (sStack->column->module->id == MTD_CLOB_LOCATOR_ID) )
            {
                // PROJ-1362
                // subquery�� ����� ��ȿ���� ���� Lob Locator�� ���� �� ����.
                IDE_TEST( addLobInfoForCopy(
                              aLobInfo,
                              & sMetaColumn->basicInfo->column,
                              *(smLobLocator*)sStack->value)
                          != IDE_SUCCESS );

                // NOT NULL Constraint
                if ( ( sMetaColumn->basicInfo->flag & MTC_COLUMN_NOTNULL_MASK )
                     == MTC_COLUMN_NOTNULL_TRUE )
                {
                    if ( *(smLobLocator*)sStack->value != MTD_LOCATOR_NULL )
                    {
                        IDE_TEST( smiLob::getLength( QC_STATISTICS(aTemplate->stmt),
                                                     *(smLobLocator*)sStack->value,
                                                     & sLobLen,
                                                     & sIsNullLob )
                                  != IDE_SUCCESS );
                    }
                    else
                    {
                        sIsNullLob = ID_TRUE;
                    }

                    IDE_TEST_RAISE( sIsNullLob == ID_TRUE,
                                    ERR_NOT_ALLOW_NULL );
                }
                else
                {
                    // Nothing to do.
                }

                // NULL�� �Ҵ��Ѵ�.
                aUpdatedRow[sValue->order].value  = NULL;
                aUpdatedRow[sValue->order].length = 0;
            }
            else
            {
                sColumn =
                    &( aTemplate->tmplate.rows[aCanonizedTuple]
                       .columns[sValue->order]);

                sCanonizedValue = (void*)
                    ( (UChar*)
                      aTemplate->tmplate.rows[aCanonizedTuple].row
                      + sColumn->column.offset);

                // PROJ-2002 Column Security
                // update�� subquery�� ����ϴ� ��� ��ȣ Ÿ���� �� �� ����.
                // �� typed literal�� ����ϴ� ��� default policy��
                // ��ȣ Ÿ���� �� ���� �ִ�.
                //
                // update t1 set i1=(select echar'a' from dual);
                //
                sMtcColumn = QTC_TMPL_COLUMN( aTemplate, sNode );

                IDE_TEST_RAISE( ( (sMtcColumn->module->flag & MTD_ENCRYPT_TYPE_MASK)
                                  == MTD_ENCRYPT_TYPE_TRUE ) &&
                                ( QCS_IS_DEFAULT_POLICY( sMtcColumn ) != ID_TRUE ),
                                ERR_INVALID_ENCRYPTED_COLUMN );

                // PROJ-2002 Column Security
                if ( ( sColumn->module->flag & MTD_ENCRYPT_TYPE_MASK )
                     == MTD_ENCRYPT_TYPE_TRUE )
                {
                    IDE_TEST( qcsModule::getEncryptInfo( aTemplate->stmt,
                                                         aTableInfo,
                                                         sColumnOrder,
                                                         & sEncryptInfo )
                              != IDE_SUCCESS );
                }
                else
                {
                    // Nothing to do.
                }

                IDE_TEST_RAISE( sColumn->module->canonize( sColumn,
                                                           & sCanonizedValue,
                                                           & sEncryptInfo,
                                                           sStack->column,
                                                           sStack->value,
                                                           NULL,
                                                           & aTemplate->tmplate )
                                != IDE_SUCCESS, ERR_INVALID_CANONIZE );

                IDE_TEST( qdbCommon::mtdValue2StoringValue(
                              sMetaColumn->basicInfo,
                              sStack->column,
                              sCanonizedValue,
                              &sStoringValue )
                          != IDE_SUCCESS );
                aUpdatedRow[sValue->order].value  = sStoringValue;

                IDE_TEST( qdbCommon::storingSize( sMetaColumn->basicInfo,
                                                  sStack->column,
                                                  sCanonizedValue,
                                                  &sStoringSize )
                          != IDE_SUCCESS );
                aUpdatedRow[sValue->order].length = sStoringSize;

                IDE_TEST_RAISE( aIsNull[sIterator](
                                    sColumn,
                                    sCanonizedValue ) == ID_TRUE,
                                ERR_NOT_ALLOW_NULL );
            }
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NOT_ALLOW_NULL );
    {
        /* BUG-45680 insert ����� not null column�� ���� �����޽��� ������ column ���� ���. */
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMX_NOT_NULL_CONSTRAINT,
                                  "",
                                  "" ) );
    }
    IDE_EXCEPTION( ERR_INVALID_ENCRYPTED_COLUMN );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QTC_INVALID_ENCRYPTED_COLUMN ) );
    }
    IDE_EXCEPTION( ERR_INVALID_CANONIZE );
    {
        if ( ideGetErrorCode() == mtERR_ABORT_INVALID_LENGTH )
        {
            IDE_CLEAR();
            IDE_SET( ideSetErrorCode( mtERR_ABORT_INVALID_LENGTH_COLUMN,
                                      aTableInfo->columns[sColumnOrder].name ) );
        }
        else
        {
            // nothing to do
        }
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

