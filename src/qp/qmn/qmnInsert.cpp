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
 * $Id: qmnInsert.cpp 55241 2012-08-27 09:13:19Z linkedlist $
 *
 * Description :
 *     INST(Insert) Node
 *
 *     ������ �𵨿��� insert�� �����ϴ� Plan Node �̴�.
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#include <idl.h>
#include <ide.h>
#include <qcg.h>
#include <qmnInsert.h>
#include <qdnTrigger.h>
#include <qdnForeignKey.h>
#include <qdbCommon.h>
#include <qmsDefaultExpr.h>
#include <qmx.h>
#include <qcuTemporaryObj.h>

IDE_RC
qmnINST::init( qcTemplate * aTemplate,
               qmnPlan    * aPlan )
{
/***********************************************************************
 *
 * Description :
 *    INST ����� �ʱ�ȭ
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnINST::init"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmnINST::init"));

    qmncINST * sCodePlan = (qmncINST*) aPlan;
    qmndINST * sDataPlan =
        (qmndINST*) (aTemplate->tmplate.data + aPlan->offset);

    sDataPlan->flag = & aTemplate->planFlag[sCodePlan->planID];
    sDataPlan->doIt = qmnINST::doItDefault;
    
    //------------------------------------------------
    // ���� �ʱ�ȭ ���� ���� �Ǵ�
    //------------------------------------------------

    if ( ( *sDataPlan->flag & QMND_INST_INIT_DONE_MASK )
         == QMND_INST_INIT_DONE_FALSE )
    {
        // ���� �ʱ�ȭ ����
        IDE_TEST( firstInit(aTemplate, sCodePlan, sDataPlan) != IDE_SUCCESS );
        
        //---------------------------------
        // �ʱ�ȭ �ϷḦ ǥ��
        //---------------------------------
        
        *sDataPlan->flag &= ~QMND_INST_INIT_DONE_MASK;
        *sDataPlan->flag |= QMND_INST_INIT_DONE_TRUE;
    }
    else
    {
        // Nothing to do.
    }
        
    //------------------------------------------------
    // Child Plan�� �ʱ�ȭ
    //------------------------------------------------

    if ( aPlan->left != NULL )
    {
        IDE_TEST( aPlan->left->init( aTemplate,
                                     aPlan->left ) != IDE_SUCCESS);
    }
    else
    {
        // Nothing to do.
    }
    
    //------------------------------------------------
    // ���� Data �� �ʱ�ȭ
    //------------------------------------------------

    // update rowGRID �ʱ�ȭ
    sDataPlan->rowGRID = SC_NULL_GRID;
    
    //---------------------------------
    // trigger row�� ����
    //---------------------------------
        
    IDE_TEST( allocTriggerRow( sCodePlan, sDataPlan )
              != IDE_SUCCESS );
        
    //---------------------------------
    // returnInto row�� ����
    //---------------------------------

    IDE_TEST( allocReturnRow( aTemplate, sCodePlan, sDataPlan )
              != IDE_SUCCESS );
    
    //---------------------------------
    // index table cursor�� ����
    //---------------------------------
    
    IDE_TEST( allocIndexTableCursor(aTemplate, sCodePlan, sDataPlan)
              != IDE_SUCCESS );
    
    //------------------------------------------------
    // ���� �Լ� ����
    //------------------------------------------------

    if ( sCodePlan->isInsertSelect == ID_TRUE )
    {
        sDataPlan->doIt = qmnINST::doItFirst;
    }
    else
    {
        /* BUG-47625 Insert execution ������� */
        if ( ( sCodePlan->flag & QMNC_INST_COMPLEX_MASK )
             == QMNC_INST_COMPLEX_TRUE )
        {
            sDataPlan->doIt = qmnINST::doItFirstMultiRows;
        }
        else
        {
            sDataPlan->doIt = qmnINST::doItOneRow;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnINST::doIt( qcTemplate * aTemplate,
               qmnPlan    * aPlan,
               qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    INST �� ���� ����� �����Ѵ�.
 *
 * Implementation :
 *    ������ �Լ� �����͸� �����Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "qmnINST::doIt"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmndINST * sDataPlan =
        (qmndINST*) (aTemplate->tmplate.data + aPlan->offset);

    return sDataPlan->doIt( aTemplate, aPlan, aFlag );

#undef IDE_FN
}

IDE_RC 
qmnINST::padNull( qcTemplate * aTemplate,
                  qmnPlan    * aPlan )
{
/***********************************************************************
 *
 * Description :
 *    INST ���� ������ null row�� ������ ������,
 *    Child�� ���Ͽ� padNull()�� ȣ���Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnINST::padNull"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmnINST::padNull"));

    qmncINST * sCodePlan = (qmncINST*) aPlan;
    // qmndINST * sDataPlan = 
    //     (qmndINST*) (aTemplate->tmplate.data + aPlan->offset);

    if ( (aTemplate->planFlag[sCodePlan->planID] & QMND_INST_INIT_DONE_MASK)
         == QMND_INST_INIT_DONE_FALSE )
    {
        // �ʱ�ȭ���� ���� ��� �ʱ�ȭ ����
        IDE_TEST( aPlan->init( aTemplate, aPlan ) != IDE_SUCCESS );
    }
    else
    {
        // Nothing To Do
    }

    // Child Plan�� ���Ͽ� Null Padding����
    if ( aPlan->left != NULL )
    {
        IDE_TEST( aPlan->left->padNull( aTemplate, aPlan->left )
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
qmnINST::printPlan( qcTemplate   * aTemplate,
                    qmnPlan      * aPlan,
                    ULong          aDepth,
                    iduVarString * aString,
                    qmnDisplay     aMode )
{
/***********************************************************************
 *
 * Description :
 *    INST ����� ���� ������ ����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnINST::printPlan"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmnINST::printPlan"));

    qmncINST * sCodePlan = (qmncINST*) aPlan;
    qmndINST * sDataPlan =
        (qmndINST*) (aTemplate->tmplate.data + aPlan->offset);

    ULong      i;
    qmmValueNode * sValue;
    qmmMultiRows * sMultiRows;

    sDataPlan->flag = & aTemplate->planFlag[sCodePlan->planID];

    //------------------------------------------------------
    // ���� ������ ���
    //------------------------------------------------------

    for ( i = 0; i < aDepth; i++ )
    {
        iduVarStringAppend( aString,
                            " " );
    }

    //------------------------------------------------------
    // INST Target ������ ���
    //------------------------------------------------------

    // INST ������ ���
    if ( sCodePlan->tableRef->tableType == QCM_VIEW )
    {
        iduVarStringAppendFormat( aString,
                                  "INSERT ( VIEW: " );
    }
    else
    {
        iduVarStringAppendFormat( aString,
                                  "INSERT ( TABLE: " );
    }

    if ( ( sCodePlan->tableOwnerName.name != NULL ) &&
         ( sCodePlan->tableOwnerName.size > 0 ) )
    {
        iduVarStringAppendLength( aString,
                                  sCodePlan->tableOwnerName.name,
                                  sCodePlan->tableOwnerName.size );
        iduVarStringAppend( aString, "." );
    }
    else
    {
        // Nothing to do.
    }

    //----------------------------
    // Table Name ���
    //----------------------------

    if ( ( sCodePlan->tableName.size <= QC_MAX_OBJECT_NAME_LEN ) &&
         ( sCodePlan->tableName.name != NULL ) &&
         ( sCodePlan->tableName.size > 0 ) )
    {
        iduVarStringAppendLength( aString,
                                  sCodePlan->tableName.name,
                                  sCodePlan->tableName.size );
    }
    else
    {
        // Nothing to do.
    }

    //----------------------------
    // Alias Name ���
    //----------------------------
    
    if ( sCodePlan->aliasName.name != NULL &&
         sCodePlan->aliasName.size > 0  &&
         sCodePlan->aliasName.name != sCodePlan->tableName.name )
    {
        // Table �̸� ������ Alias �̸� ������ �ٸ� ���
        // (alias name)
        iduVarStringAppend( aString, " " );
        
        if ( sCodePlan->aliasName.size <= QC_MAX_OBJECT_NAME_LEN )
        {
            iduVarStringAppendLength( aString,
                                      sCodePlan->aliasName.name,
                                      sCodePlan->aliasName.size );
        }
        else
        {
            // Nothing to do.
        }
    }
    else
    {
        // Alias �̸� ������ ���ų� Table �̸� ������ ������ ���
        // Nothing To Do
    }

    //----------------------------
    // New line ���
    //----------------------------
    iduVarStringAppend( aString, " )\n" );

    //------------------------------------------------------
    // BUG-38343 VALUES ������ Subquery ���� ���
    //------------------------------------------------------

    for ( sMultiRows = sCodePlan->rows;
          sMultiRows != NULL;
          sMultiRows = sMultiRows->next )
    {
        for ( sValue = sMultiRows->values;
              sValue != NULL;
              sValue = sValue->next)
        {
            IDE_TEST( qmn::printSubqueryPlan( aTemplate,
                                              sValue->value,
                                              aDepth,
                                              aString,
                                              aMode ) != IDE_SUCCESS );
        }
    }

    //------------------------------------------------------
    // Child Plan ������ ���
    //------------------------------------------------------

    if ( aPlan->left != NULL )
    {
        IDE_TEST( aPlan->left->printPlan( aTemplate,
                                          aPlan->left,
                                          aDepth + 1,
                                          aString,
                                          aMode ) != IDE_SUCCESS );
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
qmnINST::firstInit( qcTemplate * aTemplate,
                    qmncINST   * aCodePlan,
                    qmndINST   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *    INST node�� Data ������ ����� ���� �ʱ�ȭ�� ����
 *
 * Implementation :
 *    - Data ������ �ֿ� ����� ���� �ʱ�ȭ�� ����
 *
 ***********************************************************************/

#define IDE_FN "qmnINST::firstInit"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmnINST::firstInit"));

    smiValue    * sInsertedRow;
    UShort        sTableID;

    //---------------------------------
    // �⺻ ����
    //---------------------------------

    aDataPlan->parallelDegree = 0;
    aDataPlan->rows           = aCodePlan->rows;

    //---------------------------------
    // insert cursor manager �ʱ�ȭ
    //---------------------------------
    if ( aCodePlan->isInsertSelect == ID_TRUE )
    {
        IDE_TEST( aDataPlan->insertCursorMgr.initialize(
                      aTemplate->stmt->qmxMem,
                      aCodePlan->tableRef,
                      ID_TRUE,
                      ID_FALSE )
                  != IDE_SUCCESS );
    }
    else
    {
        if ( ( ( aCodePlan->flag & QMNC_INST_COMPLEX_MASK )
             == QMNC_INST_COMPLEX_TRUE ) ||
             ( ( *aDataPlan->flag & QMND_INST_ATOMIC_MASK )
               == QMND_INST_ATOMIC_TRUE ) )
        {
            IDE_TEST( aDataPlan->insertCursorMgr.initialize(
                          aTemplate->stmt->qmxMem,
                          aCodePlan->tableRef,
                          ID_TRUE,
                          ID_FALSE )
                      != IDE_SUCCESS );
        }
        else
        {
            /* BUG-47840 Partition Insert Execution memory reduce */
            IDE_TEST( aDataPlan->insertCursorMgr.initialize(
                          aTemplate->stmt->qmxMem,
                          aCodePlan->tableRef,
                          ID_FALSE,
                          ID_FALSE )
                      != IDE_SUCCESS );
        }
    }
    //---------------------------------
    // direct-path insert �ʱ�ȭ
    //---------------------------------
    
    if ( ( aCodePlan->hints != NULL ) &&
         ( aCodePlan->isAppend == ID_TRUE ) )
    {
        if ( qci::mSessionCallback.mIsParallelDml(
                 aTemplate->stmt->session->mMmSession )
             == ID_TRUE )
        {
            // PROJ-1665
            // Session�� Parallel Mode�� ���,
            // Parallel Degree�� ������

            if ( aCodePlan->hints->parallelHint != NULL )
            {
                aDataPlan->parallelDegree =
                    aCodePlan->hints->parallelHint->parallelDegree;

                if ( aDataPlan->parallelDegree == 0 )
                {
                    // Parallel Hint�� �־����� ���� ���,
                    // Table�� Parallel Degree�� ����
                    aDataPlan->parallelDegree = 
                        aCodePlan->tableRef->tableInfo->parallelDegree;
                }
                else
                {
                    // Parallel Hint�� ������
                }
            }
            else
            {
                // Parallel Hint�� �־����� ���� ���,
                // Table�� Parallel Degree�� ����
                aDataPlan->parallelDegree = 
                    aCodePlan->tableRef->tableInfo->parallelDegree;
            }
        }
        else
        {
            // Session�� Parallel DML�� enable���� �ʾ���
        }
    }
    else
    {
        // hint ����
    }

    //---------------------------------
    // lob info �ʱ�ȭ
    //---------------------------------

    if ( aCodePlan->tableRef->tableInfo->lobColumnCount > 0 )
    {
        // PROJ-1362
        IDE_TEST( qmx::initializeLobInfo(
                      aTemplate->stmt,
                      & aDataPlan->lobInfo,
                      (UShort)aCodePlan->tableRef->tableInfo->lobColumnCount )
                  != IDE_SUCCESS );
        
        /* BUG-30351
         * insert into select���� �� Row Insert �� �ش� Lob Cursor�� �ٷ� ���������� �մϴ�.
         */
        if ( aCodePlan->isInsertSelect == ID_TRUE )
        {
            qmx::setImmediateCloseLobInfo( aDataPlan->lobInfo, ID_TRUE );
        }
        else
        {
            // Nothing to do.
        }
    }
    else
    {
        aDataPlan->lobInfo = NULL;
    }
    
    //------------------------------------------
    // INSERT�� ���� Default ROW ����
    //------------------------------------------

    if ( ( aCodePlan->isInsertSelect == ID_TRUE ) &&
         ( aCodePlan->isMultiInsertSelect == ID_FALSE ) )
    {
        sInsertedRow = aTemplate->insOrUptRow[aCodePlan->valueIdx];

        if ( aDataPlan->rows != NULL )
        {
            // set DEFAULT value
            IDE_TEST( qmx::makeSmiValueWithValue( aCodePlan->columnsForValues,
                                                  aDataPlan->rows->values,
                                                  aTemplate,
                                                  aCodePlan->tableRef->tableInfo,
                                                  sInsertedRow,
                                                  aDataPlan->lobInfo )
                      != IDE_SUCCESS);
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

    //------------------------------------------
    // Default Expr�� Row Buffer ����
    //------------------------------------------

    if ( aCodePlan->defaultExprColumns != NULL )
    {
        sTableID = aCodePlan->defaultExprTableRef->table;

        IDE_TEST( qmc::setRowSize( aTemplate->stmt->qmxMem,
                                   &(aTemplate->tmplate),
                                   sTableID )
                  != IDE_SUCCESS );
        
        if ( (aTemplate->tmplate.rows[sTableID].lflag & MTC_TUPLE_STORAGE_MASK)
             == MTC_TUPLE_STORAGE_MEMORY )
        {
            IDE_TEST( aTemplate->stmt->qmxMem->alloc(
                          aTemplate->tmplate.rows[sTableID].rowOffset,
                          &(aTemplate->tmplate.rows[sTableID].row) )
                      != IDE_SUCCESS );
        }
        else
        {
            /* Disk Table�� ���, qmc::setRowSize()���� �̹� �Ҵ� */
        }

        aDataPlan->defaultExprRowBuffer = aTemplate->tmplate.rows[sTableID].row;
    }
    else
    {
        aDataPlan->defaultExprRowBuffer = NULL;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnINST::allocTriggerRow( qmncINST   * aCodePlan,
                          qmndINST   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnINST::allocTriggerRow"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmnINST::allocTriggerRow"));

    //---------------------------------
    // Trigger�� ���� ������ ����
    //---------------------------------

    if ( ( aCodePlan->tableRef->tableInfo->triggerCount > 0 ) &&
         ( aCodePlan->disableTrigger == ID_FALSE ) )
    {
        aDataPlan->columnsForRow = aCodePlan->tableRef->tableInfo->columns;
        
        aDataPlan->needTriggerRow = ID_FALSE;
        aDataPlan->existTrigger = ID_TRUE;
    }
    else
    {
        aDataPlan->needTriggerRow = ID_FALSE;
        aDataPlan->existTrigger = ID_FALSE;
    }
    
    return IDE_SUCCESS;

#undef IDE_FN
}

IDE_RC
qmnINST::allocReturnRow( qcTemplate * aTemplate,
                         qmncINST   * aCodePlan,
                         qmndINST   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnINST::allocReturnRow"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmnINST::allocReturnRow"));

    //---------------------------------
    // return into�� ���� ������ ����
    //---------------------------------

    if ( ( aCodePlan->returnInto != NULL ) &&
         ( aCodePlan->insteadOfTrigger == ID_TRUE ) )
    {
        // insert �����̹Ƿ� view�� ���� plan�� ���� rowOffset��
        // �����Ǿ����� �ʴ�.
        IDE_TEST( qmc::setRowSize( aTemplate->stmt->qmxMem,
                                   & aTemplate->tmplate,
                                   aCodePlan->tableRef->table )
                  != IDE_SUCCESS );
        
        aDataPlan->viewTuple = & aTemplate->tmplate.rows[aCodePlan->tableRef->table];

        // ���ռ� �˻�
        IDE_DASSERT( aDataPlan->viewTuple->rowOffset > 0 );
            
        // New Row Referencing�� ���� ���� �Ҵ�
        IDE_TEST( aTemplate->stmt->qmxMem->cralloc(
                aDataPlan->viewTuple->rowOffset,
                (void**) & aDataPlan->returnRow )
            != IDE_SUCCESS);
    }
    else
    {
        aDataPlan->viewTuple = NULL;
        aDataPlan->returnRow = NULL;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}
    
IDE_RC
qmnINST::allocIndexTableCursor( qcTemplate * aTemplate,
                                qmncINST   * aCodePlan,
                                qmndINST   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnINST::allocIndexTableCursor"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmnINST::allocIndexTableCursor"));

    //---------------------------------
    // index table ó���� ���� ����
    //---------------------------------

    if ( aCodePlan->tableRef->indexTableRef != NULL )
    {
        IDE_TEST( qmsIndexTable::initializeIndexTableCursors4Insert(
                      aTemplate->stmt,
                      aCodePlan->tableRef->indexTableRef,
                      aCodePlan->tableRef->indexTableCount,
                      & (aDataPlan->indexTableCursorInfo) )
                  != IDE_SUCCESS );
        
        *aDataPlan->flag &= ~QMND_INST_INDEX_CURSOR_MASK;
        *aDataPlan->flag |= QMND_INST_INDEX_CURSOR_INITED;
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
qmnINST::doItDefault( qcTemplate * /* aTemplate */,
                      qmnPlan    * /* aPlan */,
                      qmcRowFlag * /* aFlag */ )
{
/***********************************************************************
 *
 * Description :
 *    �� �Լ��� ����Ǹ� �ȵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnINST::doItDefault"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    IDE_DASSERT( 0 );

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnINST::doItFirst( qcTemplate * aTemplate,
                    qmnPlan    * aPlan,
                    qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    INST�� ���� ���� �Լ�
 *
 * Implementation :
 *    - Table�� IX Lock�� �Ǵ�.
 *    - Session Event Check (������ ���� Detect)
 *    - Cursor Open
 *    - insert one record
 *
 ***********************************************************************/

#define IDE_FN "qmnINST::doItFirst"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncINST * sCodePlan = (qmncINST*) aPlan;
    qmndINST * sDataPlan =
        (qmndINST*) (aTemplate->tmplate.data + aPlan->offset);

    //-----------------------------------
    // open insert cursor
    //-----------------------------------
    
    if ( sCodePlan->insteadOfTrigger == ID_TRUE )
    {
        // instead of trigger�� cursor�� �ʿ����.
        // Nothing to do.
    }
    else
    {
        IDE_TEST( openCursor( aTemplate, aPlan ) != IDE_SUCCESS );
    }
    
    //-----------------------------------
    // Child Plan�� ������
    //-----------------------------------

    // doIt left child
    IDE_TEST( aPlan->left->doIt( aTemplate, aPlan->left, aFlag )
              != IDE_SUCCESS );
    
    //-----------------------------------
    // Insert�� ������
    //-----------------------------------
    
    if ( ( *aFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST )
    {
        // check trigger
        IDE_TEST( checkTrigger( aTemplate, aPlan ) != IDE_SUCCESS );

        if ( sCodePlan->insteadOfTrigger == ID_TRUE )
        {
            IDE_TEST( fireInsteadOfTrigger( aTemplate, aPlan ) != IDE_SUCCESS );
        }
        else
        {
            /* PROJ-2359 Table/Partition Access Option */
            IDE_TEST( qmx::checkAccessOption( sCodePlan->tableRef->tableInfo,
                                              ID_TRUE, /* aIsInsertion */
                                              QCG_CHECK_SHARD_DML_CONSISTENCY( aTemplate->stmt ) )
                      != IDE_SUCCESS );

            // insert one record
            IDE_TEST( insertOneRow( aTemplate, aPlan ) != IDE_SUCCESS );
        }
        
        sDataPlan->doIt = qmnINST::doItNext;
    }
    else
    {
        // nothing to do
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sDataPlan->lobInfo != NULL )
    {
        (void)qmx::finalizeLobInfo( aTemplate->stmt, sDataPlan->lobInfo );
    }

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnINST::doItNext( qcTemplate * aTemplate,
                   qmnPlan    * aPlan,
                   qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    INST�� ���� ���� �Լ�
 *    ���� Record�� �����Ѵ�.
 *
 * Implementation :
 *    - insert one record
 *
 ***********************************************************************/

#define IDE_FN "qmnINST::doItNext"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncINST * sCodePlan = (qmncINST*) aPlan;
    qmndINST * sDataPlan =
        (qmndINST*) (aTemplate->tmplate.data + aPlan->offset);

    //-----------------------------------
    // Child Plan�� ������
    //-----------------------------------

    // doIt left child
    IDE_TEST( aPlan->left->doIt( aTemplate, aPlan->left, aFlag )
              != IDE_SUCCESS );
    
    //-----------------------------------
    // Insert�� ������
    //-----------------------------------
    
    if ( ( *aFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST )
    {
        if ( sCodePlan->insteadOfTrigger == ID_TRUE )
        {
            IDE_TEST( fireInsteadOfTrigger( aTemplate, aPlan ) != IDE_SUCCESS );
        }
        else
        {
            // insert one record
            IDE_TEST( insertOneRow( aTemplate, aPlan ) != IDE_SUCCESS );
        }
    }
    else
    {
        // record�� ���� ���
        // ���� ������ ���� ���� ���� �Լ��� ������.
        sDataPlan->doIt = qmnINST::doItFirst;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sDataPlan->lobInfo != NULL )
    {
        (void)qmx::finalizeLobInfo( aTemplate->stmt, sDataPlan->lobInfo );
    }

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qmnINST::doItFirstMultiRows( qcTemplate * aTemplate,
                                    qmnPlan    * aPlan,
                                    qmcRowFlag * aFlag )
{
    qmncINST * sCodePlan = (qmncINST*) aPlan;
    qmndINST * sDataPlan =
        (qmndINST*) (aTemplate->tmplate.data + aPlan->offset);

    //-----------------------------------
    // Insert�� ������
    //-----------------------------------
    // check trigger
    IDE_TEST( checkTrigger( aTemplate, aPlan ) != IDE_SUCCESS );

    if ( sCodePlan->insteadOfTrigger == ID_TRUE )
    {
        IDE_TEST( fireInsteadOfTrigger( aTemplate, aPlan ) != IDE_SUCCESS );
    }
    else
    {
        /* PROJ-2359 Table/Partition Access Option */
        IDE_TEST( qmx::checkAccessOption( sCodePlan->tableRef->tableInfo,
                                          ID_TRUE, /* aIsInsertion */
                                          QCG_CHECK_SHARD_DML_CONSISTENCY( aTemplate->stmt ) )
                  != IDE_SUCCESS );

        // insert one record
        IDE_TEST( insertOnce( aTemplate, aPlan ) != IDE_SUCCESS );
    }

    if ( sDataPlan->rows->next != NULL )
    {
        *aFlag &= ~QMC_ROW_DATA_MASK;
        *aFlag |= QMC_ROW_DATA_EXIST;

        sDataPlan->doIt = qmnINST::doItNextMultiRows;
    }
    else
    {
        *aFlag &= ~QMC_ROW_DATA_MASK;
        *aFlag |= QMC_ROW_DATA_NONE;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sDataPlan->lobInfo != NULL )
    {
        (void)qmx::finalizeLobInfo( aTemplate->stmt, sDataPlan->lobInfo );
    }

    return IDE_FAILURE;
}

IDE_RC qmnINST::doItNextMultiRows( qcTemplate * aTemplate,
                                   qmnPlan    * aPlan,
                                   qmcRowFlag * aFlag )
{
    qmncINST * sCodePlan = (qmncINST*) aPlan;
    qmndINST * sDataPlan =
        (qmndINST*) (aTemplate->tmplate.data + aPlan->offset);

    IDE_TEST_RAISE( sDataPlan->rows->next == NULL, ERR_UNEXPECTED );

    //-----------------------------------
    // �ٸ� Row�� ����
    //-----------------------------------
    sDataPlan->rows = sDataPlan->rows->next;

    //-----------------------------------
    // Insert�� ������
    //-----------------------------------
    if ( sCodePlan->insteadOfTrigger == ID_TRUE )
    {
        IDE_TEST( fireInsteadOfTrigger( aTemplate, aPlan ) != IDE_SUCCESS );
    }
    else
    {
        // insert one record
        IDE_TEST( insertOnce( aTemplate, aPlan ) != IDE_SUCCESS );
    }

    if ( sDataPlan->rows->next == NULL )
    {
        *aFlag &= ~QMC_ROW_DATA_MASK;
        *aFlag |= QMC_ROW_DATA_NONE;
        // ���� ������ ���� ���� ���� �Լ��� ������.
        sDataPlan->doIt = qmnINST::doItFirstMultiRows;
    }
    else
    {
        *aFlag &= ~QMC_ROW_DATA_MASK;
        *aFlag |= QMC_ROW_DATA_EXIST;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_UNEXPECTED )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmnInsert::doItNextMultiRows",
                                  "Invalid Next rows" ));
    }
    IDE_EXCEPTION_END;

    if ( sDataPlan->lobInfo != NULL )
    {
        (void)qmx::finalizeLobInfo( aTemplate->stmt, sDataPlan->lobInfo );
    }

    return IDE_FAILURE;
}

IDE_RC
qmnINST::checkTrigger( qcTemplate * aTemplate,
                       qmnPlan    * aPlan )
{
/***********************************************************************
 *
 * Description :
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnINST::checkTrigger"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncINST * sCodePlan = (qmncINST*) aPlan;
    qmndINST * sDataPlan =
        (qmndINST*) (aTemplate->tmplate.data + aPlan->offset);
    idBool     sNeedTriggerRow;

    if ( sDataPlan->existTrigger == ID_TRUE )
    {
        if ( sCodePlan->insteadOfTrigger == ID_TRUE )
        {
            IDE_TEST( qdnTrigger::needTriggerRow(
                          aTemplate->stmt,
                          sCodePlan->tableRef->tableInfo,
                          QCM_TRIGGER_INSTEAD_OF,
                          QCM_TRIGGER_EVENT_INSERT,
                          NULL,
                          & sNeedTriggerRow )
                      != IDE_SUCCESS );
        }
        else
        {
            // Trigger�� ���� Referencing Row�� �ʿ������� �˻�
            IDE_TEST( qdnTrigger::needTriggerRow(
                          aTemplate->stmt,
                          sCodePlan->tableRef->tableInfo,
                          QCM_TRIGGER_BEFORE,
                          QCM_TRIGGER_EVENT_INSERT,
                          NULL,
                          & sNeedTriggerRow )
                      != IDE_SUCCESS );
        
            if ( sNeedTriggerRow == ID_FALSE )
            {
                IDE_TEST( qdnTrigger::needTriggerRow(
                              aTemplate->stmt,
                              sCodePlan->tableRef->tableInfo,
                              QCM_TRIGGER_AFTER,
                              QCM_TRIGGER_EVENT_INSERT,
                              NULL,
                              & sNeedTriggerRow )
                          != IDE_SUCCESS );
            }
            else
            {
                // Nothing to do.
            }
        }
        
        sDataPlan->needTriggerRow = sNeedTriggerRow;
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
qmnINST::openCursor( qcTemplate * aTemplate,
                     qmnPlan    * aPlan )
{
/***********************************************************************
 *
 * Description :
 *
 * Implementation :
 *     ���� scan�� open�� cursor�� ��´�.
 *
 ***********************************************************************/

#define IDE_FN "qmnINST::openCursor"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncINST * sCodePlan = (qmncINST*) aPlan;
    qmndINST * sDataPlan =
        (qmndINST*) (aTemplate->tmplate.data + aPlan->offset);

    smiCursorProperties   sCursorProperty;
    qcmTableInfo        * sDiskInfo        = NULL;
    UShort                sTupleID         = 0;
    idBool                sNeedAllFetchColumn;
    smiFetchColumnList  * sFetchColumnList = NULL;
    UInt                  sFlag            = 0;

    if ( ( *sDataPlan->flag & QMND_INST_CURSOR_MASK )
         == QMND_INST_CURSOR_CLOSED )
    {
        // INSERT �� ���� Cursor ����
        SMI_CURSOR_PROP_INIT_FOR_FULL_SCAN( &sCursorProperty, aTemplate->stmt->mStatistics );

        // BUG-43063 insert nowait
        sCursorProperty.mLockWaitMicroSec = sCodePlan->lockWaitMicroSec;
        
        sCursorProperty.mHintParallelDegree = sDataPlan->parallelDegree;

        if ( sCodePlan->tableRef->tableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
        {
            if ( sCodePlan->tableRef->partitionSummary->diskPartitionRef != NULL )
            {
                /* BUG-47614 partition table insert�� optimize ���� ���� */
                if ( ( sCodePlan->tableRef->partitionSummary->isHybridPartitionedTable == ID_TRUE ) ||
                     ( sCodePlan->parentConstraints != NULL ) )
                {
                    sDiskInfo = sCodePlan->tableRef->partitionSummary->diskPartitionRef->partitionInfo;
                    sTupleID = sCodePlan->tableRef->partitionSummary->diskPartitionRef->table;
                }
                else
                {
                    sDiskInfo = sCodePlan->tableRef->tableInfo;
                    sTupleID = sCodePlan->tableRef->table;
                }
            }
            else
            {
                /* Nothing to do */
            }
        }
        else
        {
            if ( QCM_TABLE_TYPE_IS_DISK( sCodePlan->tableRef->tableInfo->tableFlag ) == ID_TRUE )
            {
                sDiskInfo = sCodePlan->tableRef->tableInfo;
                sTupleID = sCodePlan->tableRef->table;
            }
            else
            {
                /* Nothing to do */
            }
        }

        if ( sDiskInfo != NULL )
        {
            // return into���� ������ all fetch
            if ( sCodePlan->returnInto != NULL )
            {
                sNeedAllFetchColumn = ID_TRUE;
            }
            else
            {
                sNeedAllFetchColumn = sDataPlan->needTriggerRow;
            }
            
            // PROJ-1705
            // ����Ű üũ�� ���� �о�� �� ��ġ�÷�����Ʈ ����
            IDE_TEST( qdbCommon::makeFetchColumnList4TupleID(
                          aTemplate,
                          sTupleID,
                          sNeedAllFetchColumn,  // aIsNeedAllFetchColumn
                          NULL,                 // index
                          ID_TRUE,              // allocSmiColumnListEx
                          & sFetchColumnList )
                      != IDE_SUCCESS );
        
            /* PROJ-1107 Check Constraint ���� */
            if ( (sDataPlan->needTriggerRow == ID_FALSE) &&
                 (sCodePlan->checkConstrList != NULL) )
            {
                IDE_TEST( qdbCommon::addCheckConstrListToFetchColumnList(
                              aTemplate->stmt->qmxMem,
                              sCodePlan->checkConstrList,
                              sDiskInfo->columns,
                              & sFetchColumnList )
                          != IDE_SUCCESS );
            }
            else
            {
                /* Nothing to do */
            }
        
            sCursorProperty.mFetchColumnList = sFetchColumnList;
        }
        else
        {
            // Nothing to do.
        }

        // direct-path insert
        if ( sDataPlan->isAppend == ID_TRUE )
        {
            sFlag = SMI_INSERT_METHOD_APPEND;
        }
        else
        {
            // Nothing to do.
        }
        
        IDE_TEST( sDataPlan->insertCursorMgr.openCursor(
                      aTemplate->stmt,
                      sFlag | SMI_LOCK_WRITE | SMI_TRAVERSE_FORWARD | SMI_PREVIOUS_DISABLE,
                      & sCursorProperty )
                  != IDE_SUCCESS );
        
        *sDataPlan->flag &= ~QMND_INST_CURSOR_MASK;
        *sDataPlan->flag |= QMND_INST_CURSOR_OPEN;
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
qmnINST::closeCursor( qcTemplate * aTemplate,
                      qmnPlan    * aPlan )
{
/***********************************************************************
 *
 * Description :
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnINST::closeCursor"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmndINST * sDataPlan =
        (qmndINST*) (aTemplate->tmplate.data + aPlan->offset);

    if ( ( *sDataPlan->flag & QMND_INST_CURSOR_MASK )
         == QMND_INST_CURSOR_OPEN )
    {
        *sDataPlan->flag &= ~QMND_INST_CURSOR_MASK;
        *sDataPlan->flag |= QMND_INST_CURSOR_CLOSED;
        
        IDE_TEST( sDataPlan->insertCursorMgr.closeCursor()
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    if ( ( *sDataPlan->flag & QMND_INST_INDEX_CURSOR_MASK )
         == QMND_INST_INDEX_CURSOR_INITED )
    {
        IDE_TEST( qmsIndexTable::closeIndexTableCursors(
                      & (sDataPlan->indexTableCursorInfo) )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }
        
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    if ( ( *sDataPlan->flag & QMND_INST_INDEX_CURSOR_MASK )
         == QMND_INST_INDEX_CURSOR_INITED )
    {
        qmsIndexTable::finalizeIndexTableCursors(
            & (sDataPlan->indexTableCursorInfo) );
    }
        
    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnINST::insertOneRow( qcTemplate * aTemplate,
                       qmnPlan    * aPlan )
{
/***********************************************************************
 *
 * Description :
 *    INST �� ���� ����� �����Ѵ�.
 *
 * Implementation :
 *    - insert one record ����
 *    - trigger each row ����
 *
 ***********************************************************************/

    qmncINST * sCodePlan = (qmncINST*) aPlan;
    qmndINST * sDataPlan =
        (qmndINST*) (aTemplate->tmplate.data + aPlan->offset);

    iduMemoryStatus     sQmxMemStatus;
    qcmTableInfo      * sTableForInsert;
    smiValue          * sInsertedRow;
    smiValue          * sInsertedRowForPartition = NULL;
    smiValue            sValuesForPartition[QC_MAX_COLUMN_COUNT];
    smiTableCursor    * sCursor = NULL;
    void              * sRow;
    qcmTableInfo      * sSelectedPartitionInfo = NULL;
    UInt                sInsertedRowValueCount = 0;

    sTableForInsert = sCodePlan->tableRef->tableInfo;
    sInsertedRow = aTemplate->insOrUptRow[sCodePlan->valueIdx];
    sInsertedRowValueCount = aTemplate->insOrUptRowValueCount[sCodePlan->valueIdx];

    // Memory ������ ���Ͽ� ���� ��ġ ���
    IDE_TEST_RAISE( aTemplate->stmt->qmxMem->getStatus( &sQmxMemStatus ) != IDE_SUCCESS, ERR_MEM_OP );

    //-----------------------------------
    // clear lob info
    //-----------------------------------

    if ( sDataPlan->lobInfo != NULL )
    {
        (void) qmx::initLobInfo( sDataPlan->lobInfo );
    }
    else
    {
        // Nothing to do.
    }

    //-----------------------------------
    // set next sequence
    //-----------------------------------

    // Sequence Value ȹ��
    if ( sCodePlan->nextValSeqs != NULL )
    {
        IDE_TEST( qmx::readSequenceNextVals(
                      aTemplate->stmt,
                      sCodePlan->nextValSeqs )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    //-----------------------------------
    // make insert value
    //-----------------------------------

    if ( ( sCodePlan->isInsertSelect == ID_TRUE ) &&
         ( sCodePlan->isMultiInsertSelect == ID_FALSE ) )
    {
        // stack�� ���� �̿�
        IDE_TEST( qmx::makeSmiValueWithResult( sCodePlan->columns,
                                               aTemplate,
                                               sTableForInsert,
                                               sInsertedRow,
                                               sDataPlan->lobInfo )
                  != IDE_SUCCESS );
    }
    else
    {
        // values�� ���� �̿�
        IDE_TEST( qmx::makeSmiValueWithValue( aTemplate,
                                              sTableForInsert,
                                              sCodePlan->canonizedTuple,
                                              sDataPlan->rows->values,
                                              sCodePlan->queueMsgIDSeq,
                                              sInsertedRow,
                                              sDataPlan->lobInfo )
                  != IDE_SUCCESS );
    }

    //-----------------------------------
    // insert before trigger
    //-----------------------------------
    
    if ( sDataPlan->existTrigger == ID_TRUE )
    {
        // PROJ-1359 Trigger
        // ROW GRANULARITY TRIGGER�� ����
        IDE_TEST( qdnTrigger::fireTrigger(
                      aTemplate->stmt,
                      aTemplate->stmt->qmxMem,
                      sTableForInsert,
                      QCM_TRIGGER_ACTION_EACH_ROW,
                      QCM_TRIGGER_BEFORE,
                      QCM_TRIGGER_EVENT_INSERT,
                      NULL,                      // UPDATE Column
                      NULL,                      /* Table Cursor */
                      SC_NULL_GRID,              /* Row GRID */
                      NULL,                      // OLD ROW
                      NULL,                      // OLD ROW Column
                      sInsertedRow,              // NEW ROW(value list)
                      sTableForInsert->columns ) // NEW ROW Column
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    //-----------------------------------
    // check null
    //-----------------------------------
    if ( ( sCodePlan->flag & QMNC_INST_EXIST_NOTNULL_COLUMN_MASK )
         == QMNC_INST_EXIST_NOTNULL_COLUMN_TRUE )
    {
        IDE_TEST( qmx::checkNotNullColumnForInsert(
                      sTableForInsert->columns,
                      sInsertedRow,
                      sDataPlan->lobInfo,
                      ID_TRUE )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    //------------------------------------------
    // INSERT�� ���� Default ROW ����
    //------------------------------------------
    // PROJ-2264 Dictionary table
    // Default ���� ����ϴ� column �� makeSmiValueWithResult ���� ���ŵ��� �ʴ´�.
    // ������ compression column �� makeSmiValueForCompress ���� �Ź�
    // smiValue �� ���� �о� dictionary table �� OID �� ġȯ�ϹǷ�
    // smiValue �� �ٽ� default value �� ����Ű���� �ؾ��Ѵ�.
    if ( ( sCodePlan->isInsertSelect == ID_TRUE ) &&
         ( sCodePlan->columnsForValues != NULL ) &&
         ( sDataPlan->rows != NULL ) &&
         ( ( sCodePlan->compressedTuple != UINT_MAX ) ||
           ( sCodePlan->nextValSeqs != NULL ) ) )
    {
        sInsertedRow = aTemplate->insOrUptRow[sCodePlan->valueIdx];

        // set DEFAULT value
        IDE_TEST( qmx::makeSmiValueWithValue( sCodePlan->columnsForValues,
                                              sDataPlan->rows->values,
                                              aTemplate,
                                              sTableForInsert,
                                              sInsertedRow,
                                              sDataPlan->lobInfo )
                  != IDE_SUCCESS);
    }
    else
    {
        // Nothing To Do
    }

    //-----------------------------------
    // Default Expr
    //-----------------------------------

    if ( sCodePlan->defaultExprColumns != NULL )
    {
        IDE_TEST( qmsDefaultExpr::setRowBufferFromSmiValueArray(
                      &(aTemplate->tmplate),
                      sCodePlan->defaultExprTableRef,
                      sTableForInsert->columns,
                      sDataPlan->defaultExprRowBuffer,
                      sInsertedRow,
                      QCM_TABLE_TYPE_IS_DISK( sTableForInsert->tableFlag ) )
                  != IDE_SUCCESS );

        IDE_TEST( qmsDefaultExpr::calculateDefaultExpression(
                      aTemplate,
                      sCodePlan->defaultExprTableRef,
                      NULL,
                      sCodePlan->defaultExprColumns,
                      sDataPlan->defaultExprRowBuffer,
                      sInsertedRow,
                      sTableForInsert->columns )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    //-----------------------------------
    // insert one row
    //-----------------------------------
    
    IDE_TEST( sDataPlan->insertCursorMgr.partitionFilteringWithRow(
                  sInsertedRow,
                  sDataPlan->lobInfo,
                  &sSelectedPartitionInfo )
              != IDE_SUCCESS );
    
    /* PROJ-2359 Table/Partition Access Option */
    if ( sSelectedPartitionInfo != NULL )
    {
        IDE_TEST( qmx::checkAccessOption( sSelectedPartitionInfo,
                                          ID_TRUE, /* aIsInsertion */
                                          QCG_CHECK_SHARD_DML_CONSISTENCY( aTemplate->stmt ) )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    // PROJ-2264 Dictionary table
    if( sCodePlan->compressedTuple != UINT_MAX )
    {
        IDE_TEST( qmx::makeSmiValueForCompress( aTemplate,
                                                sCodePlan->compressedTuple,
                                                sInsertedRow )
                  != IDE_SUCCESS );
    }

    IDE_TEST( sDataPlan->insertCursorMgr.getCursor( &sCursor )
              != IDE_SUCCESS );

    if ( sSelectedPartitionInfo != NULL )
    {
        if ( QCM_TABLE_TYPE_IS_DISK( sTableForInsert->tableFlag ) !=
             QCM_TABLE_TYPE_IS_DISK( sSelectedPartitionInfo->tableFlag ) )
        {
            /* PROJ-2464 hybrid partitioned table ����
             * Partitioned Table�� �������� ���� smiValue Array�� Table Partition�� �°� ��ȯ�Ѵ�.
             */
            IDE_TEST( qmx::makeSmiValueWithSmiValue( sTableForInsert,
                                                     sSelectedPartitionInfo,
                                                     sTableForInsert->columns,
                                                     sInsertedRowValueCount,
                                                     sInsertedRow,
                                                     sValuesForPartition )
                      != IDE_SUCCESS );

            sInsertedRowForPartition = sValuesForPartition;
        }
        else
        {
            sInsertedRowForPartition = sInsertedRow;
        }
    }
    else
    {
        sInsertedRowForPartition = sInsertedRow;
    }

    IDE_TEST( sCursor->insertRow( sInsertedRowForPartition,
                                  & sRow,
                                  & sDataPlan->rowGRID )
              != IDE_SUCCESS);

    // insert index table
    IDE_TEST( insertIndexTableCursor( aTemplate,
                                      sDataPlan,
                                      sInsertedRow,
                                      sDataPlan->rowGRID )
              != IDE_SUCCESS );
    
    //------------------------------------------
    // INSERT�� ������ Lob �÷��� ó��
    //------------------------------------------
    
    IDE_TEST( qmx::copyAndOutBindLobInfo( aTemplate->stmt,
                                          sDataPlan->lobInfo,
                                          sCursor,
                                          sRow,
                                          sDataPlan->rowGRID )
              != IDE_SUCCESS );
    
    //-----------------------------------
    // insert after trigger
    //-----------------------------------
    
    if ( ( sDataPlan->existTrigger == ID_TRUE ) ||
         ( sCodePlan->checkConstrList != NULL ) ||
         ( sCodePlan->returnInto != NULL ) )
    {
        IDE_TEST( qmx::fireTriggerInsertRowGranularity(
                      aTemplate->stmt,
                      sCodePlan->tableRef,
                      & sDataPlan->insertCursorMgr,
                      sDataPlan->rowGRID,
                      sCodePlan->returnInto,
                      sCodePlan->checkConstrList,
                      aTemplate->numRows,
                      sDataPlan->needTriggerRow )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    // Memory ������ ���� Memory �̵�
    IDE_TEST_RAISE( aTemplate->stmt->qmxMem->setStatus( &sQmxMemStatus ) != IDE_SUCCESS, ERR_MEM_OP ); 

    if ( ( *sDataPlan->flag & QMND_INST_INSERT_MASK )
         == QMND_INST_INSERT_FALSE )
    {
        *sDataPlan->flag &= ~QMND_INST_INSERT_MASK;
        *sDataPlan->flag |= QMND_INST_INSERT_TRUE;
    }
    else
    {
        // Nothing to do.
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_MEM_OP )
    {
        ideLog::log( IDE_ERR_0,
                     "Unexpected errors may have occurred:"
                     " qmnINST::insertOneRow"
                     " memory error" );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmnINST::insertOnce( qcTemplate * aTemplate,
                     qmnPlan    * aPlan )
{
/***********************************************************************
 *
 * Description :
 *    insertOnce�� insertOneRow�� cursor�� open�ϴ� ������ �ٸ���.
 *    insertOnce������ makeSmiValue���Ŀ� cursor�� open�Ѵ�.
 *    ���� �������� t1 insert cursor�� ���� ���� subquery�� ����� �� ����.
 *
 *    ex)
 *    insert into t1 values ( select max(i1) from t1 );
 *    
 *
 * Implementation :
 *    - cursor open
 *    - insert one record ����
 *    - trigger each row ����
 *
 ***********************************************************************/

    qmncINST * sCodePlan = (qmncINST*) aPlan;
    qmndINST * sDataPlan =
        (qmndINST*) (aTemplate->tmplate.data + aPlan->offset);

    qcmTableInfo      * sTableForInsert;
    smiValue          * sInsertedRow;
    smiValue          * sInsertedRowForPartition = NULL;
    smiValue            sValuesForPartition[QC_MAX_COLUMN_COUNT];
    smiTableCursor    * sCursor = NULL;
    void              * sRow;
    qcmTableInfo      * sSelectedPartitionInfo = NULL;
    UInt                sInsertedRowValueCount = 0;

    sTableForInsert = sCodePlan->tableRef->tableInfo;
    sInsertedRow = aTemplate->insOrUptRow[sCodePlan->valueIdx];
    sInsertedRowValueCount = aTemplate->insOrUptRowValueCount[sCodePlan->valueIdx];

    //-----------------------------------
    // clear lob info
    //-----------------------------------

    if ( sDataPlan->lobInfo != NULL )
    {
        (void) qmx::initLobInfo( sDataPlan->lobInfo );
    }
    else
    {
        // Nothing to do.
    }

    //-----------------------------------
    // set next sequence
    //-----------------------------------

    // Sequence Value ȹ��
    if ( sCodePlan->nextValSeqs != NULL )
    {
        IDE_TEST( qmx::readSequenceNextVals(
                      aTemplate->stmt,
                      sCodePlan->nextValSeqs )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    //-----------------------------------
    // make insert value
    //-----------------------------------

    // values�� ���� �̿�
    IDE_TEST( qmx::makeSmiValueWithValue( aTemplate,
                                          sTableForInsert,
                                          sCodePlan->canonizedTuple,
                                          sDataPlan->rows->values,
                                          sCodePlan->queueMsgIDSeq,
                                          sInsertedRow,
                                          sDataPlan->lobInfo )
              != IDE_SUCCESS );

    //-----------------------------------
    // insert before trigger
    //-----------------------------------
    
    if ( sDataPlan->existTrigger == ID_TRUE )
    {
        // PROJ-1359 Trigger
        // ROW GRANULARITY TRIGGER�� ����
        IDE_TEST( qdnTrigger::fireTrigger(
                      aTemplate->stmt,
                      aTemplate->stmt->qmxMem,
                      sTableForInsert,
                      QCM_TRIGGER_ACTION_EACH_ROW,
                      QCM_TRIGGER_BEFORE,
                      QCM_TRIGGER_EVENT_INSERT,
                      NULL,                      // UPDATE Column
                      NULL,                      /* Table Cursor */
                      SC_NULL_GRID,              /* Row GRID */
                      NULL,                      // OLD ROW
                      NULL,                      // OLD ROW Column
                      sInsertedRow,              // NEW ROW(value list)
                      sTableForInsert->columns ) // NEW ROW Column
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    //-----------------------------------
    // check null
    //-----------------------------------
    if ( ( sCodePlan->flag & QMNC_INST_EXIST_NOTNULL_COLUMN_MASK )
         == QMNC_INST_EXIST_NOTNULL_COLUMN_TRUE )
    {
        IDE_TEST( qmx::checkNotNullColumnForInsert(
                      sTableForInsert->columns,
                      sInsertedRow,
                      sDataPlan->lobInfo,
                      ID_TRUE )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    //-----------------------------------
    // Default Expr
    //-----------------------------------

    if ( sCodePlan->defaultExprColumns != NULL )
    {
        IDE_TEST( qmsDefaultExpr::setRowBufferFromSmiValueArray(
                      &(aTemplate->tmplate),
                      sCodePlan->defaultExprTableRef,
                      sTableForInsert->columns,
                      sDataPlan->defaultExprRowBuffer,
                      sInsertedRow,
                      QCM_TABLE_TYPE_IS_DISK( sTableForInsert->tableFlag ) )
                  != IDE_SUCCESS );

        IDE_TEST( qmsDefaultExpr::calculateDefaultExpression(
                      aTemplate,
                      sCodePlan->defaultExprTableRef,
                      NULL,
                      sCodePlan->defaultExprColumns,
                      sDataPlan->defaultExprRowBuffer,
                      sInsertedRow,
                      sTableForInsert->columns )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    //-----------------------------------
    // open cursor
    //-----------------------------------
    
    IDE_TEST( openCursor( aTemplate, aPlan ) != IDE_SUCCESS );
    
    //-----------------------------------
    // insert one row
    //-----------------------------------
    
    IDE_TEST( sDataPlan->insertCursorMgr.partitionFilteringWithRow(
                  sInsertedRow,
                  sDataPlan->lobInfo,
                  &sSelectedPartitionInfo )
              != IDE_SUCCESS );

    /* PROJ-2359 Table/Partition Access Option */
    if ( sSelectedPartitionInfo != NULL )
    {
        IDE_TEST( qmx::checkAccessOption( sSelectedPartitionInfo,
                                          ID_TRUE, /* aIsInsertion */
                                          QCG_CHECK_SHARD_DML_CONSISTENCY( aTemplate->stmt ) )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    // PROJ-2264 Dictionary table
    if( sCodePlan->compressedTuple != UINT_MAX )
    {
        IDE_TEST( qmx::makeSmiValueForCompress( aTemplate,
                                                sCodePlan->compressedTuple,
                                                sInsertedRow )
                  != IDE_SUCCESS );
    }
    
    IDE_TEST( sDataPlan->insertCursorMgr.getCursor( &sCursor )
              != IDE_SUCCESS );

    if ( sSelectedPartitionInfo != NULL )
    {
        if ( QCM_TABLE_TYPE_IS_DISK( sTableForInsert->tableFlag ) !=
             QCM_TABLE_TYPE_IS_DISK( sSelectedPartitionInfo->tableFlag ) )
        {
            /* PROJ-2464 hybrid partitioned table ����
             * Partitioned Table�� �������� ���� smiValue Array�� Table Partition�� �°� ��ȯ�Ѵ�.
             */
            IDE_TEST( qmx::makeSmiValueWithSmiValue( sTableForInsert,
                                                     sSelectedPartitionInfo,
                                                     sTableForInsert->columns,
                                                     sInsertedRowValueCount,
                                                     sInsertedRow,
                                                     sValuesForPartition )
                      != IDE_SUCCESS );

            sInsertedRowForPartition = sValuesForPartition;
        }
        else
        {
            sInsertedRowForPartition = sInsertedRow;
        }
    }
    else
    {
        sInsertedRowForPartition = sInsertedRow;
    }

    IDE_TEST( sCursor->insertRow( sInsertedRowForPartition,
                                  & sRow,
                                  & sDataPlan->rowGRID )
              != IDE_SUCCESS);
    
    // insert index table
    IDE_TEST( insertIndexTableCursor( aTemplate,
                                      sDataPlan,
                                      sInsertedRow,
                                      sDataPlan->rowGRID )
              != IDE_SUCCESS );
    
    //------------------------------------------
    // INSERT�� ������ Lob �÷��� ó��
    //------------------------------------------
    
    IDE_TEST( qmx::copyAndOutBindLobInfo( aTemplate->stmt,
                                          sDataPlan->lobInfo,
                                          sCursor,
                                          sRow,
                                          sDataPlan->rowGRID )
              != IDE_SUCCESS );
    
    //-----------------------------------
    // insert after trigger
    //-----------------------------------
    
    if ( ( sDataPlan->existTrigger == ID_TRUE ) ||
         ( sCodePlan->checkConstrList != NULL ) ||
         ( sCodePlan->returnInto != NULL ) )
    {
        IDE_TEST( qmx::fireTriggerInsertRowGranularity(
                      aTemplate->stmt,
                      sCodePlan->tableRef,
                      & sDataPlan->insertCursorMgr,
                      sDataPlan->rowGRID,
                      sCodePlan->returnInto,
                      sCodePlan->checkConstrList,
                      aTemplate->numRows,
                      sDataPlan->needTriggerRow )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }
    
    if ( ( *sDataPlan->flag & QMND_INST_INSERT_MASK )
         == QMND_INST_INSERT_FALSE )
    {
        *sDataPlan->flag &= ~QMND_INST_INSERT_MASK;
        *sDataPlan->flag |= QMND_INST_INSERT_TRUE;
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
qmnINST::fireInsteadOfTrigger( qcTemplate * aTemplate,
                               qmnPlan    * aPlan )
{
/***********************************************************************
 *
 * Description :
 *    INST �� ���� ����� �����Ѵ�.
 *
 * Implementation :
 *    - trigger each row ����
 *
 ***********************************************************************/

#define IDE_FN "qmnINST::fireInsteadOfTrigger"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncINST * sCodePlan = (qmncINST*) aPlan;
    qmndINST * sDataPlan =
        (qmndINST*) (aTemplate->tmplate.data + aPlan->offset);

    iduMemoryStatus   sQmxMemStatus;
    qcmTableInfo    * sTableForInsert;
    smiValue        * sInsertedRow;
    void            * sOrgRow;

    sTableForInsert = sCodePlan->tableRef->tableInfo;
    sInsertedRow = aTemplate->insOrUptRow[sCodePlan->valueIdx];
    
    // Memory ������ ���Ͽ� ���� ��ġ ���
    IDE_TEST_RAISE( aTemplate->stmt->qmxMem->getStatus( &sQmxMemStatus ) != IDE_SUCCESS, ERR_MEM_OP );
    
    if ( ( sDataPlan->needTriggerRow == ID_TRUE ) ||
         ( sCodePlan->returnInto != NULL ) )
    {
        //-----------------------------------
        // set next sequence
        //-----------------------------------
        
        // Sequence Value ȹ��
        if ( sCodePlan->nextValSeqs != NULL )
        {
            IDE_TEST( qmx::readSequenceNextVals(
                          aTemplate->stmt,
                          sCodePlan->nextValSeqs )
                      != IDE_SUCCESS );
        }
        else
        {
            // Nothing to do.
        }

        if ( ( sCodePlan->isInsertSelect == ID_TRUE ) &&
             ( sCodePlan->isMultiInsertSelect == ID_FALSE ) )
        {
            // stack�� ���� �̿�
            
            // insert�� select ���̿� �����ϴ� ��尡 ���� stack�� �ٷ� �д´�.
            IDE_TEST( qmx::makeSmiValueWithStack( sDataPlan->columnsForRow,
                                                  aTemplate,
                                                  aTemplate->tmplate.stack,
                                                  sTableForInsert,
                                                  sInsertedRow,
                                                  NULL )
                      != IDE_SUCCESS );
        }
        else
        {
            // values�� ���� �̿�
            IDE_TEST( qmx::makeSmiValueWithValue( aTemplate,
                                                  sTableForInsert,
                                                  sCodePlan->canonizedTuple,
                                                  sDataPlan->rows->values,
                                                  sCodePlan->queueMsgIDSeq,
                                                  sInsertedRow,
                                                  NULL )
                      != IDE_SUCCESS );
        }
    }
    else
    {
        // Nothing to do.
    }

    if ( sDataPlan->existTrigger == ID_TRUE )
    {
        // instead of trigger
        IDE_TEST( qdnTrigger::fireTrigger(
                      aTemplate->stmt,
                      aTemplate->stmt->qmxMem,
                      sTableForInsert,
                      QCM_TRIGGER_ACTION_EACH_ROW,
                      QCM_TRIGGER_INSTEAD_OF,
                      QCM_TRIGGER_EVENT_INSERT,
                      NULL,                        // UPDATE Column
                      NULL,                        /* Table Cursor */
                      SC_NULL_GRID,                /* Row GRID */
                      NULL,                        // OLD ROW
                      NULL,                        // OLD ROW Column
                      sInsertedRow,                // NEW ROW
                      sDataPlan->columnsForRow )   // NEW ROW Column
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }
    
    /* PROJ-1584 DML Return Clause */
    if ( sCodePlan->returnInto != NULL )
    {
        IDE_TEST( qmx::makeRowWithSmiValue( sDataPlan->viewTuple->columns,
                                            sDataPlan->viewTuple->columnCount,
                                            sInsertedRow,
                                            sDataPlan->returnRow )
                  != IDE_SUCCESS );

        sOrgRow = sDataPlan->viewTuple->row;
        sDataPlan->viewTuple->row = sDataPlan->returnRow;

        IDE_TEST( qmx::copyReturnToInto( aTemplate,
                                         sCodePlan->returnInto,
                                         aTemplate->numRows )
                  != IDE_SUCCESS );
        
        sDataPlan->viewTuple->row = sOrgRow;
    }
    else
    {
        // nothing do do
    }
    
    // Memory ������ ���� Memory �̵�
    IDE_TEST_RAISE( aTemplate->stmt->qmxMem->setStatus( &sQmxMemStatus ) != IDE_SUCCESS, ERR_MEM_OP );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_MEM_OP )
    {
        ideLog::log( IDE_ERR_0,
                     "Unexpected errors may have occurred:"
                     " qmnINST::fireInsteadOfTrigger"
                     " memory error" );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnINST::checkInsertRef( qcTemplate * aTemplate,
                         qmnPlan    * aPlan )
{
/***********************************************************************
 *
 * Description :
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnINST::checkInsertRef"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncINST            * sCodePlan;
    qmndINST            * sDataPlan;
    qmcInsertPartCursor * sCursorIter;
    void                * sRow = NULL;
    UInt                  i;
    
    sCodePlan = (qmncINST*) aPlan;
    sDataPlan = (qmndINST*) ( aTemplate->tmplate.data + aPlan->offset );
    
    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aTemplate != NULL );
    
    //------------------------------------------
    // parent constraint �˻�
    //------------------------------------------

    if ( sCodePlan->parentConstraints != NULL )
    {
        for ( i = 0; i < sDataPlan->insertCursorMgr.mCursorIndexCount; i++ )
        {
            sCursorIter = sDataPlan->insertCursorMgr.mCursorIndex[i];
            
            if( sCursorIter->partitionRef == NULL )
            {
                IDE_TEST( checkInsertChildRefOnScan(
                              aTemplate,
                              sCodePlan,
                              sCodePlan->tableRef->tableInfo,
                              sCodePlan->tableRef->table,
                              sCursorIter,
                              & sRow )
                          != IDE_SUCCESS );
            }
            else
            {
                IDE_TEST( checkInsertChildRefOnScan(
                              aTemplate,
                              sCodePlan,
                              sCursorIter->partitionRef->partitionInfo,
                              sCursorIter->partitionRef->table,  // table tuple ���
                              sCursorIter,
                              & sRow )
                          != IDE_SUCCESS );
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

#undef IDE_FN
}

IDE_RC qmnINST::checkInsertChildRefOnScan( qcTemplate           * aTemplate,
                                           qmncINST             * aCodePlan,
                                           qcmTableInfo         * aTableInfo,
                                           UShort                 aTable,
                                           qmcInsertPartCursor  * aCursorIter,
                                           void                ** aRow )
{
/***********************************************************************
 *
 * Description :
 *    INSERT ���� ���� �� Parent Table�� ���� Referencing ���� ������ �˻�
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnINST::checkInsertChildRefOnScan"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    UInt                sTableType;
    UInt                sRowSize = 0;
    void              * sRow;
    scGRID              sRid;
    iduMemoryStatus     sQmxMemStatus;
    mtcTuple          * sTuple;
    
    //----------------------------
    // Record ���� Ȯ��
    //----------------------------

    sTuple = &(aTemplate->tmplate.rows[aTable]);
    sTableType = aTableInfo->tableFlag & SMI_TABLE_TYPE_MASK;
    sRow = *aRow;

    if ( ( sTableType == SMI_TABLE_DISK ) &&
         ( sRow == NULL ) )
    {
        // Disk Table�� ���
        // Record Read�� ���� ������ �Ҵ��Ѵ�.
        IDE_TEST( qdbCommon::getDiskRowSize( aTableInfo,
                                             & sRowSize )
                  != IDE_SUCCESS );

        IDE_TEST( aTemplate->stmt->qmxMem->cralloc( sRowSize,
                                                    (void **) & sRow )
                  != IDE_SUCCESS);

        *aRow = sRow;
    }
    else
    {
        // Memory Table�� ���
        // Nothing to do.
    }

    //------------------------------------------
    // INSERT�� �ο� �˻��� ����,
    // ���ſ����� ����� ù��° row ���� ��ġ�� cursor ��ġ ����
    //------------------------------------------

    IDE_TEST( aCursorIter->cursor.beforeFirstModified( SMI_FIND_MODIFIED_NEW )
              != IDE_SUCCESS );

    IDE_TEST( aCursorIter->cursor.readNewRow( (const void **) & sRow,
                                              & sRid )
              != IDE_SUCCESS);
    
    //----------------------------
    // �ݺ� �˻�
    //----------------------------

    while ( sRow != NULL )
    {
        //------------------------------
        // ���� ���� �˻�
        //------------------------------

        // Memory ������ ���Ͽ� ���� ��ġ ���
        IDE_TEST_RAISE( aTemplate->stmt->qmxMem->getStatus( &sQmxMemStatus ) != IDE_SUCCESS, ERR_MEM_OP );
        
        IDE_TEST( qdnForeignKey::checkParentRef( aTemplate->stmt,
                                                 NULL,
                                                 aCodePlan->parentConstraints,
                                                 sTuple,
                                                 sRow,
                                                 0 )
                  != IDE_SUCCESS);

        // Memory ������ ���� Memory �̵�
        IDE_TEST_RAISE( aTemplate->stmt->qmxMem->setStatus( &sQmxMemStatus ) != IDE_SUCCESS, ERR_MEM_OP );

        IDE_TEST( aCursorIter->cursor.readNewRow( (const void **) & sRow,
                                                  & sRid )
                  != IDE_SUCCESS);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_MEM_OP )
    {
        ideLog::log( IDE_ERR_0,
                     "Unexpected errors may have occurred:"
                     " qmnINST::checkInsertChildRefOnScan"
                     " memory error" );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnINST::insertIndexTableCursor( qcTemplate     * aTemplate,
                                 qmndINST       * aDataPlan,
                                 smiValue       * aInsertValue,
                                 scGRID           aRowGRID )
{
/***********************************************************************
 *
 * Description :
 *    INSERT ���� ���� �� index table�� ���� insert ����
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnINST::insertIndexTableCursor"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    smOID   sPartOID;
    
    // insert index table
    if ( ( *aDataPlan->flag & QMND_INST_INDEX_CURSOR_MASK )
         == QMND_INST_INDEX_CURSOR_INITED )
    {
        IDE_TEST( aDataPlan->insertCursorMgr.getSelectedPartitionOID(
                      & sPartOID )
                  != IDE_SUCCESS );

        IDE_TEST( qmsIndexTable::insertIndexTableCursors(
                      aTemplate->stmt,
                      & (aDataPlan->indexTableCursorInfo),
                      aInsertValue,
                      sPartOID,
                      aRowGRID )
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

IDE_RC qmnINST::getLastInsertedRowGRID( qcTemplate * aTemplate,
                                        qmnPlan    * aPlan,
                                        scGRID     * aRowGRID )
{
/***********************************************************************
 *
 * Description : BUG-38129
 *     ������ insert row�� GRID�� ��ȯ�Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    qmndINST * sDataPlan =
        (qmndINST*) (aTemplate->tmplate.data + aPlan->offset);

    *aRowGRID = sDataPlan->rowGRID;
    
    return IDE_SUCCESS;
}

/**
 * BUG-47625 Insert execution �������
 */
IDE_RC qmnINST::doItOneRow( qcTemplate * aTemplate,
                            qmnPlan    * aPlan,
                            qmcRowFlag * aFlag )
{
    qmncINST            * sCodePlan = (qmncINST*) aPlan;
    qmndINST            * sDataPlan = (qmndINST*) (aTemplate->tmplate.data + aPlan->offset);
    qcmTableInfo        * sTableForInsert;
    qcmTableInfo        * sTableInfo;
    smiValue            * sInsertedRow;
    smiTableCursor      * sCursor = NULL;
    void                * sRow;
    qcmTableInfo        * sSelectedPartitionInfo = NULL;
    smiCursorProperties   sCursorProperty;
    qcmTableInfo        * sDiskInfo        = NULL;
    UShort                sTupleID         = 0;
    smiFetchColumnList  * sFetchColumnList = NULL;
    mtcStack            * sStack;
    UInt                  sColumnCount;
    mtcColumn           * sColumns;
    UInt                  sIterator;
    qmmValueNode        * sValueNode;
    void                * sValue;
    SInt                  sColumnOrder;
    mtcEncryptInfo        sEncryptInfo;
    UInt                  sStoringSize = 0;
    void                * sStoringValue;

    sTableForInsert = sCodePlan->tableRef->tableInfo;
    sTableInfo = sTableForInsert;
    sInsertedRow = aTemplate->insOrUptRow[sCodePlan->valueIdx];

    /* TASK-7307 DML Data Consistency in Shard */
    IDE_TEST_RAISE( ( QCG_CHECK_SHARD_DML_CONSISTENCY( aTemplate->stmt ) == ID_TRUE ) &&
                    ( sTableInfo->mIsUsable == ID_FALSE ),
                    ERR_TABLE_PARTITION_UNUSABLE );

    /* PROJ-2359 Table/Partition Access Option */
    IDE_TEST_RAISE( sTableInfo->accessOption == QCM_ACCESS_OPTION_READ_ONLY,
                    ERR_TABLE_PARTITION_ACCESS_DENIED );

    //-----------------------------------
    // set next sequence
    //-----------------------------------

    // Sequence Value ȹ��
    if ( sCodePlan->nextValSeqs != NULL )
    {
        IDE_TEST( qmx::readSequenceNextVals( aTemplate->stmt,
                                             sCodePlan->nextValSeqs )
                  != IDE_SUCCESS );
    }

    /* qmx::makeSimeValueWithValue Start */
    sStack         = aTemplate->tmplate.stack;
    sColumnCount   = aTemplate->tmplate.rows[sCodePlan->canonizedTuple].columnCount;
    sColumns       = aTemplate->tmplate.rows[sCodePlan->canonizedTuple].columns;

    for ( sIterator = 0, sValueNode = sDataPlan->rows->values;
          sIterator < sColumnCount;
          sIterator++, sValueNode = sValueNode->next, sColumns++ )
    {
        sColumnOrder = sColumns->column.id & SMI_COLUMN_ID_MASK;

        if ( sValueNode->value != NULL )
        {
            IDE_TEST( qtc::calculate( sValueNode->value, aTemplate )
                      != IDE_SUCCESS );

            sValue = (void*)((UChar*) aTemplate->tmplate.rows[sCodePlan->canonizedTuple].row + sColumns->column.offset);
            sColumnOrder = sColumns->column.id & SMI_COLUMN_ID_MASK;

            IDE_TEST_RAISE( sColumns->module->canonize( sColumns,
                                                        &sValue,
                                                        &sEncryptInfo,
                                                        sStack->column,
                                                        sStack->value,
                                                        NULL,
                                                        &aTemplate->tmplate )
                            != IDE_SUCCESS, ERR_INVALID_CANONIZE );

            IDE_TEST( qdbCommon::mtdValue2StoringValue( sTableForInsert->columns[sColumnOrder].basicInfo,
                                                        sColumns,
                                                        sValue,
                                                        &sStoringValue )
                      != IDE_SUCCESS );
            sInsertedRow[sIterator].value  = sStoringValue;

            IDE_TEST( qdbCommon::storingSize( sTableForInsert->columns[sColumnOrder].basicInfo,
                                              sColumns,
                                              sValue,
                                              &sStoringSize )
                      != IDE_SUCCESS );
            sInsertedRow[sIterator].length = sStoringSize;
        }
    } /* qmx::makeSimeValueWithValue END */

    //-----------------------------------
    // check null
    //-----------------------------------
    if ( ( sCodePlan->flag & QMNC_INST_EXIST_NOTNULL_COLUMN_MASK )
         == QMNC_INST_EXIST_NOTNULL_COLUMN_TRUE )
    {
        IDE_TEST( qmx::checkNotNullColumnForInsert( sTableForInsert->columns,
                                                    sInsertedRow,
                                                    sDataPlan->lobInfo,
                                                    ID_TRUE )
                  != IDE_SUCCESS );
    }

    /* Open Cursor START  INSERT �� ���� Cursor ���� */
    if ( ( *sDataPlan->flag & QMND_INST_CURSOR_MASK )
         == QMND_INST_CURSOR_CLOSED )
    {
        SMI_CURSOR_PROP_INIT_FOR_FULL_SCAN( &sCursorProperty, aTemplate->stmt->mStatistics );

        // BUG-43063 insert nowait
        sCursorProperty.mLockWaitMicroSec = sCodePlan->lockWaitMicroSec;
        sCursorProperty.mHintParallelDegree = sDataPlan->parallelDegree;

        if ( sCodePlan->tableRef->tableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
        {
            if ( sCodePlan->tableRef->partitionSummary->diskPartitionRef != NULL )
            {
                /* BUG-47614 partition table insert�� optimize ���� ���� */
                if ( sCodePlan->parentConstraints != NULL )
                {
                    sDiskInfo = sCodePlan->tableRef->partitionSummary->diskPartitionRef->partitionInfo;
                    sTupleID = sCodePlan->tableRef->partitionSummary->diskPartitionRef->table;
                }
                else
                {
                    sDiskInfo = sCodePlan->tableRef->tableInfo;
                    sTupleID = sCodePlan->tableRef->table;
                }
            }
        }
        else
        {
            if ( QCM_TABLE_TYPE_IS_DISK( sCodePlan->tableRef->tableInfo->tableFlag ) == ID_TRUE )
            {
                sDiskInfo = sCodePlan->tableRef->tableInfo;
                sTupleID = sCodePlan->tableRef->table;
            }
        }

        if ( sDiskInfo != NULL )
        {
            // PROJ-1705
            // ����Ű üũ�� ���� �о�� �� ��ġ�÷�����Ʈ ����
            IDE_TEST( qdbCommon::makeFetchColumnList4TupleID(
                          aTemplate,
                          sTupleID,
                          ID_FALSE,  // aIsNeedAllFetchColumn
                          NULL,                 // index
                          ID_TRUE,              // allocSmiColumnListEx
                          & sFetchColumnList )
                      != IDE_SUCCESS );

            sCursorProperty.mFetchColumnList = sFetchColumnList;
        }

        IDE_TEST( sDataPlan->insertCursorMgr.openCursor(
                      aTemplate->stmt,
                      SMI_LOCK_WRITE | SMI_TRAVERSE_FORWARD | SMI_PREVIOUS_DISABLE,
                      & sCursorProperty )
                  != IDE_SUCCESS );

        *sDataPlan->flag &= ~QMND_INST_CURSOR_MASK;
        *sDataPlan->flag |= QMND_INST_CURSOR_OPEN;
    }
     /* Open Cursor END  */

    //-----------------------------------
    // insert one row
    //-----------------------------------
    IDE_TEST( sDataPlan->insertCursorMgr.partitionFilteringWithRow(
                  sInsertedRow,
                  sDataPlan->lobInfo,
                  &sSelectedPartitionInfo )
              != IDE_SUCCESS );

    /* PROJ-2359 Table/Partition Access Option */
    if ( sSelectedPartitionInfo != NULL )
    {
        sTableInfo = sSelectedPartitionInfo;
        IDE_TEST_RAISE( sSelectedPartitionInfo->accessOption == QCM_ACCESS_OPTION_READ_ONLY,
                        ERR_TABLE_PARTITION_ACCESS_DENIED );
    }

    IDE_TEST( sDataPlan->insertCursorMgr.getCursor( &sCursor )
              != IDE_SUCCESS );

    IDE_TEST( sCursor->insertRow( sInsertedRow,
                                  & sRow,
                                  & sDataPlan->rowGRID )
              != IDE_SUCCESS);

    if ( ( *sDataPlan->flag & QMND_INST_INDEX_CURSOR_MASK )
         == QMND_INST_INDEX_CURSOR_INITED )
    {
        // insert index table
        IDE_TEST( insertIndexTableCursor( aTemplate,
                                          sDataPlan,
                                          sInsertedRow,
                                          sDataPlan->rowGRID )
                  != IDE_SUCCESS );
    }

    if ( ( *sDataPlan->flag & QMND_INST_INSERT_MASK )
         == QMND_INST_INSERT_FALSE )
    {
        *sDataPlan->flag &= ~QMND_INST_INSERT_MASK;
        *sDataPlan->flag |= QMND_INST_INSERT_TRUE;
    }
    else
    {
        // Nothing to do.
    }

    *aFlag &= ~QMC_ROW_DATA_MASK;
    *aFlag |= QMC_ROW_DATA_NONE;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_TABLE_PARTITION_ACCESS_DENIED );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMX_TABLE_PARTITION_ACCESS_DENIED,
                                  sTableInfo->name ) );
    }
    IDE_EXCEPTION( ERR_TABLE_PARTITION_UNUSABLE );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMX_TABLE_PARTITION_UNUSABLE,
                                  sTableInfo->name ) );
    }
    IDE_EXCEPTION( ERR_INVALID_CANONIZE );
    {
        if ( ideGetErrorCode() == mtERR_ABORT_INVALID_LENGTH )
        {
            IDE_CLEAR();
            IDE_SET( ideSetErrorCode( mtERR_ABORT_INVALID_LENGTH_COLUMN,
                                      sTableForInsert->columns[sColumnOrder].name ) );
        }
        else
        {
            // nothing to do
        }
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

