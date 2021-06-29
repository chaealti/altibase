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
 * $Id: qmnUpdate.cpp 55241 2012-08-27 09:13:19Z linkedlist $
 *
 * Description :
 *     UPTE(UPdaTE) Node
 *
 *     ������ �𵨿��� update�� �����ϴ� Plan Node �̴�.
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#include <idl.h>
#include <ide.h>
#include <qcg.h>
#include <qmnUpdate.h>
#include <qmoPartition.h>
#include <qdnTrigger.h>
#include <qdnForeignKey.h>
#include <qdbCommon.h>
#include <qdnCheck.h>
#include <qmsDefaultExpr.h>
#include <qmx.h>
#include <qcuTemporaryObj.h>
#include <qdtCommon.h>

IDE_RC
qmnUPTE::init( qcTemplate * aTemplate,
               qmnPlan    * aPlan )
{
/***********************************************************************
 *
 * Description :
 *    UPTE ����� �ʱ�ȭ
 *
 * Implementation :
 *
 ***********************************************************************/

    UInt       i = 0;
    qmncUPTE * sCodePlan = (qmncUPTE*) aPlan;
    qmndUPTE * sDataPlan =
        (qmndUPTE*) (aTemplate->tmplate.data + aPlan->offset);

    sDataPlan->flag = & aTemplate->planFlag[sCodePlan->planID];
    sDataPlan->doIt = qmnUPTE::doItDefault;

    //------------------------------------------------
    // ���� �ʱ�ȭ ���� ���� �Ǵ�
    //------------------------------------------------

    if ( ( *sDataPlan->flag & QMND_UPTE_INIT_DONE_MASK )
         == QMND_UPTE_INIT_DONE_FALSE )
    {
        if ( ( sCodePlan->flag & QMNC_UPTE_MULTIPLE_TABLE_MASK )
             == QMNC_UPTE_MULTIPLE_TABLE_FALSE )
        {
            // ���� �ʱ�ȭ ����
            IDE_TEST( firstInit(aTemplate, sCodePlan, sDataPlan) != IDE_SUCCESS );

            //------------------------------------------------
            // Child Plan�� �ʱ�ȭ
            //------------------------------------------------

            IDE_TEST( aPlan->left->init( aTemplate,
                                         aPlan->left ) != IDE_SUCCESS);

            //---------------------------------
            // trigger row�� ����
            //---------------------------------

            // child�� offset�� �̿��ϹǷ� firstInit�� ������ offset�� �̿��� �� �ִ�.
            IDE_TEST( allocTriggerRow(aTemplate, sCodePlan, sDataPlan)
                      != IDE_SUCCESS );

            //---------------------------------
            // returnInto row�� ����
            //---------------------------------

            IDE_TEST( allocReturnRow(aTemplate, sCodePlan, sDataPlan)
                      != IDE_SUCCESS );

            //---------------------------------
            // index table cursor�� ����
            //---------------------------------

            IDE_TEST( allocIndexTableCursor(aTemplate, sCodePlan, sDataPlan)
                      != IDE_SUCCESS );
        }
        else
        {
            // ���� �ʱ�ȭ ����
            IDE_TEST( firstInitMultiTable(aTemplate, sCodePlan, sDataPlan) != IDE_SUCCESS );

            //------------------------------------------------
            // Child Plan�� �ʱ�ȭ
            //------------------------------------------------
            IDE_TEST( aPlan->left->init( aTemplate,
                                         aPlan->left ) != IDE_SUCCESS);

            //---------------------------------
            // trigger row�� ����
            //---------------------------------

            // child�� offset�� �̿��ϹǷ� firstInit�� ������ offset�� �̿��� �� �ִ�.
            IDE_TEST( allocTriggerRowMultiTable(aTemplate, sCodePlan, sDataPlan)
                      != IDE_SUCCESS );

            //---------------------------------
            // index table cursor�� ����
            //---------------------------------

            IDE_TEST( allocIndexTableCursorMultiTable(aTemplate, sCodePlan, sDataPlan)
                      != IDE_SUCCESS );
        }

        //---------------------------------
        // �ʱ�ȭ �ϷḦ ǥ��
        //---------------------------------

        *sDataPlan->flag &= ~QMND_UPTE_INIT_DONE_MASK;
        *sDataPlan->flag |= QMND_UPTE_INIT_DONE_TRUE;
    }
    else
    {
        //------------------------------------------------
        // Child Plan�� �ʱ�ȭ
        //------------------------------------------------

        IDE_TEST( aPlan->left->init( aTemplate,
                                     aPlan->left ) != IDE_SUCCESS);

        if ( ( sCodePlan->flag & QMNC_UPTE_MULTIPLE_TABLE_MASK )
             == QMNC_UPTE_MULTIPLE_TABLE_FALSE )
        {
            //-----------------------------------
            // init lob info
            //-----------------------------------

            if ( sDataPlan->lobInfo != NULL )
            {
                (void) qmx::initLobInfo( sDataPlan->lobInfo );
            }
            else
            {
                // Nothing to do.
            }
        }
        else
        {
            //-----------------------------------
            // init lob info
            //-----------------------------------
            for ( i = 0; i < sCodePlan->mMultiTableCount; i++ )
            {
                (void)qmx::initLobInfo( sDataPlan->mTableArray[i].mLobInfo );
            }
        }
    }

    //------------------------------------------------
    // ���� Data �� �ʱ�ȭ
    //------------------------------------------------

    // Limit ���� ������ �ʱ�ȭ
    sDataPlan->limitCurrent = 1;

    // update rowGRID �ʱ�ȭ
    sDataPlan->rowGRID = SC_NULL_GRID;

    //------------------------------------------------
    // ���� �Լ� ����
    //------------------------------------------------
    if ( ( sCodePlan->flag & QMNC_UPTE_MULTIPLE_TABLE_MASK )
         == QMNC_UPTE_MULTIPLE_TABLE_FALSE )
    {
        sDataPlan->doIt = qmnUPTE::doItFirst;
    }
    else
    {
        sDataPlan->doIt = qmnUPTE::doItFirstMultiTable;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmnUPTE::doIt( qcTemplate * aTemplate,
               qmnPlan    * aPlan,
               qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    UPTE �� ���� ����� �����Ѵ�.
 *
 * Implementation :
 *    ������ �Լ� �����͸� �����Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "qmnUPTE::doIt"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmndUPTE * sDataPlan =
        (qmndUPTE*) (aTemplate->tmplate.data + aPlan->offset);

    return sDataPlan->doIt( aTemplate, aPlan, aFlag );

#undef IDE_FN
}

IDE_RC
qmnUPTE::padNull( qcTemplate * aTemplate,
                  qmnPlan    * aPlan )
{
/***********************************************************************
 *
 * Description :
 *    UPTE ���� ������ null row�� ������ ������,
 *    Child�� ���Ͽ� padNull()�� ȣ���Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnUPTE::padNull"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmnUPTE::padNull"));

    qmncUPTE * sCodePlan = (qmncUPTE*) aPlan;
    // qmndUPTE * sDataPlan =
    //     (qmndUPTE*) (aTemplate->tmplate.data + aPlan->offset);

    if ( (aTemplate->planFlag[sCodePlan->planID] & QMND_UPTE_INIT_DONE_MASK)
         == QMND_UPTE_INIT_DONE_FALSE )
    {
        // �ʱ�ȭ���� ���� ��� �ʱ�ȭ ����
        IDE_TEST( aPlan->init( aTemplate, aPlan ) != IDE_SUCCESS );
    }
    else
    {
        // Nothing To Do
    }

    // Child Plan�� ���Ͽ� Null Padding����
    IDE_TEST( aPlan->left->padNull( aTemplate, aPlan->left )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnUPTE::printPlan( qcTemplate   * aTemplate,
                    qmnPlan      * aPlan,
                    ULong          aDepth,
                    iduVarString * aString,
                    qmnDisplay     aMode )
{
/***********************************************************************
 *
 * Description :
 *    UPTE ����� ���� ������ ����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnUPTE::printPlan"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmnUPTE::printPlan"));

    qmncUPTE * sCodePlan = (qmncUPTE*) aPlan;
    qmndUPTE * sDataPlan =
        (qmndUPTE*) (aTemplate->tmplate.data + aPlan->offset);

    ULong             i;
    qmmValueNode    * sValue;
    qmmMultiTables  * sTmp;
    qmsNamePosition   sTableOwnerName;     // Table Owner Name
    qmsNamePosition   sTableName;          // Table Name
    qmsNamePosition   sAliasName;          // Alias Name

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
    // UPTE Target ������ ���
    //------------------------------------------------------
    if ( ( sCodePlan->flag & QMNC_UPTE_MULTIPLE_TABLE_MASK )
         == QMNC_UPTE_MULTIPLE_TABLE_FALSE )
    {
        // UPTE ������ ���
        if ( sCodePlan->tableRef->tableType == QCM_VIEW )
        {
            iduVarStringAppendFormat( aString,
                                      "UPDATE ( VIEW: " );
        }
        else
        {
            iduVarStringAppendFormat( aString,
                                      "UPDATE ( TABLE: " );
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

        if ( ( sCodePlan->aliasName.name != NULL ) &&
             ( sCodePlan->aliasName.size > 0  ) &&
             ( sCodePlan->aliasName.name != sCodePlan->tableName.name ) )
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
    }
    else
    {
        iduVarStringAppendFormat( aString,
                                  "UPDATE ( " );
        for ( sTmp = sCodePlan->mTableList; sTmp != NULL; sTmp = sTmp->mNext )
        {
            if ( sTmp->mTableRef->tableType == QCM_VIEW )
            {
                iduVarStringAppendFormat( aString,
                                          "VIEW: " );
            }
            else
            {
                iduVarStringAppendFormat( aString,
                                          "TABLE: " );
            }

            qmn::setDisplayInfo( sTmp->mTableRef, &sTableOwnerName, &sTableName, &sAliasName );

            if ( ( sTableOwnerName.name != NULL ) &&
                 ( sTableOwnerName.size > 0 ) )
            {
                iduVarStringAppendLength( aString,
                                          sTableOwnerName.name,
                                          sTableOwnerName.size );
                iduVarStringAppend( aString, "." );
            }
            else
            {
                // Nothing to do.
            }

            if ( ( sTableName.size <= QC_MAX_OBJECT_NAME_LEN ) &&
                 ( sTableName.name != NULL ) &&
                 ( sTableName.size > 0 ) )
            {
                iduVarStringAppendLength( aString,
                                          sTableName.name,
                                          sTableName.size );
            }
            else
            {
                // Nothing to do.
            }

            //----------------------------
            // Alias Name ���
            //----------------------------
            if ( ( sAliasName.name != NULL ) &&
                 ( sAliasName.size > 0  ) &&
                 ( sAliasName.name != sTableName.name ) )
            {
                // Table �̸� ������ Alias �̸� ������ �ٸ� ���
                // (alias name)
                iduVarStringAppend( aString, " " );

                if ( sAliasName.size <= QC_MAX_OBJECT_NAME_LEN )
                {
                    iduVarStringAppendLength( aString,
                                              sAliasName.name,
                                              sAliasName.size );
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
            iduVarStringAppend( aString, " " );
        }
    }
    //----------------------------
    // New line ���
    //----------------------------
    iduVarStringAppend( aString, " )\n" );

    //------------------------------------------------------
    // BUG-38343 Set ������ Subquery ���� ���
    //------------------------------------------------------

    for ( sValue = sCodePlan->values;
          sValue != NULL;
          sValue = sValue->next)
    {
        if ( sValue->value != NULL )
        {
            IDE_TEST( qmn::printSubqueryPlan( aTemplate,
                                              sValue->value,
                                              aDepth,
                                              aString,
                                              aMode ) != IDE_SUCCESS );
        }
        else
        {
            /* Noting to do */
        }
    }

    //------------------------------------------------------
    // Child Plan ������ ���
    //------------------------------------------------------

    IDE_TEST( aPlan->left->printPlan( aTemplate,
                                      aPlan->left,
                                      aDepth + 1,
                                      aString,
                                      aMode ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnUPTE::firstInit( qcTemplate * aTemplate,
                    qmncUPTE   * aCodePlan,
                    qmndUPTE   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *    UPTE node�� Data ������ ����� ���� �ʱ�ȭ�� ����
 *
 * Implementation :
 *    - Data ������ �ֿ� ����� ���� �ʱ�ȭ�� ����
 *
 ***********************************************************************/

#define IDE_FN "qmnUPTE::firstInit"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmnUPTE::firstInit"));

    ULong          sCount;
    UShort         sTableID;
    qcmTableInfo * sTableInfo;
    idBool         sIsNeedRebuild = ID_FALSE;

    //--------------------------------
    // ���ռ� �˻�
    //--------------------------------

    //--------------------------------
    // UPTE ���� ������ �ʱ�ȭ
    //--------------------------------

    aDataPlan->insertLobInfo = NULL;
    aDataPlan->insertValues  = NULL;
    aDataPlan->checkValues   = NULL;

    // Tuple Set������ �ʱ�ȭ
    aDataPlan->updateTuple     = & aTemplate->tmplate.rows[aCodePlan->tableRef->table];
    aDataPlan->updateCursor    = NULL;
    aDataPlan->updateTupleID   = ID_USHORT_MAX;

    /* PROJ-2464 hybrid partitioned table ���� */
    aDataPlan->updatePartInfo = NULL;

    // index table cursor �ʱ�ȭ
    aDataPlan->indexUpdateCursor = NULL;
    aDataPlan->indexUpdateTuple = NULL;

    // set, where column list �ʱ�ȭ
    smiInitDMLRetryInfo( &(aDataPlan->retryInfo) );

    /* PROJ-2359 Table/Partition Access Option */
    aDataPlan->accessOption = QCM_ACCESS_OPTION_READ_WRITE;

    //--------------------------------
    // cursorInfo ����
    //--------------------------------

    if ( aCodePlan->insteadOfTrigger == ID_TRUE )
    {
        // instead of trigger�� cursor�� �ʿ����.
        // Nothing to do.
    }
    else
    {
        sTableInfo = aCodePlan->tableRef->tableInfo;

        // PROJ-2219 Row-level before update trigger
        // Invalid �� trigger�� ������ compile�ϰ�, DML�� rebuild �Ѵ�.
        if ( sTableInfo->triggerCount > 0 )
        {
            IDE_TEST( qdnTrigger::verifyTriggers( aTemplate->stmt,
                                                  sTableInfo,
                                                  aCodePlan->updateColumnList,
                                                  &sIsNeedRebuild )
                      != IDE_SUCCESS );

            IDE_TEST_RAISE( sIsNeedRebuild == ID_TRUE,
                            trigger_invalid );
        }
        else
        {
            // Nothing to do.
        }

        IDE_TEST( allocCursorInfo( aTemplate, aCodePlan, aDataPlan )
                  != IDE_SUCCESS );
    }

    //--------------------------------
    // partition ���� ������ �ʱ�ȭ
    //--------------------------------

    if ( aCodePlan->tableRef->tableInfo->lobColumnCount > 0 )
    {
        // PROJ-1362
        IDE_TEST( qmx::initializeLobInfo( aTemplate->stmt,
                                          & aDataPlan->lobInfo,
                                          (UShort)aCodePlan->tableRef->tableInfo->lobColumnCount )
                  != IDE_SUCCESS );
    }
    else
    {
        aDataPlan->lobInfo = NULL;
    }

    switch ( aCodePlan->updateType )
    {
        case QMO_UPDATE_ROWMOVEMENT:
        {
            // insert cursor manager �ʱ�ȭ
            IDE_TEST( aDataPlan->insertCursorMgr.initialize(
                          aTemplate->stmt->qmxMem,
                          aCodePlan->insertTableRef,
                          ID_TRUE,
                          ID_FALSE )
                      != IDE_SUCCESS );

            // lob info �ʱ�ȭ
            if ( aCodePlan->tableRef->tableInfo->lobColumnCount > 0 )
            {
                // PROJ-1362
                IDE_TEST( qmx::initializeLobInfo(
                              aTemplate->stmt,
                              & aDataPlan->insertLobInfo,
                              (UShort)aCodePlan->tableRef->tableInfo->lobColumnCount )
                          != IDE_SUCCESS );
            }
            else
            {
                aDataPlan->insertLobInfo = NULL;
            }

            // insert smiValues �ʱ�ȭ
            IDE_TEST( aTemplate->stmt->qmxMem->alloc(
                          aCodePlan->tableRef->tableInfo->columnCount
                          * ID_SIZEOF(smiValue),
                          (void**)& aDataPlan->insertValues )
                      != IDE_SUCCESS );

            break;
        }

        case QMO_UPDATE_CHECK_ROWMOVEMENT:
        {
            // check smiValues �ʱ�ȭ
            IDE_TEST( aTemplate->stmt->qmxMem->alloc(
                          aCodePlan->tableRef->tableInfo->columnCount
                          * ID_SIZEOF(smiValue),
                          (void**)& aDataPlan->checkValues )
                      != IDE_SUCCESS );

            break;
        }

        default:
            break;
    }

    //--------------------------------
    // Limitation ���� ������ �ʱ�ȭ
    //--------------------------------

    if( aCodePlan->limit != NULL )
    {
        IDE_TEST( qmsLimitI::getStartValue(
                      aTemplate,
                      aCodePlan->limit,
                      &aDataPlan->limitStart )
                  != IDE_SUCCESS );

        IDE_TEST( qmsLimitI::getCountValue(
                      aTemplate,
                      aCodePlan->limit,
                      &sCount )
                  != IDE_SUCCESS );

        aDataPlan->limitEnd = aDataPlan->limitStart + sCount;
    }
    else
    {
        aDataPlan->limitStart = 1;
        aDataPlan->limitEnd   = 0;
    }

    // ���ռ� �˻�
    if ( aDataPlan->limitEnd > 0 )
    {
        IDE_ASSERT( (aCodePlan->flag & QMNC_UPTE_LIMIT_MASK)
                    == QMNC_UPTE_LIMIT_TRUE );
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

    IDE_EXCEPTION( trigger_invalid )
    {
        IDE_SET( ideSetErrorCode( qpERR_REBUILD_TRIGGER_INVALID ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnUPTE::allocCursorInfo( qcTemplate * aTemplate,
                          qmncUPTE   * aCodePlan,
                          qmndUPTE   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnUPTE::allocCursorInfo"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmnUPTE::allocCursorInfo"));

    qmnCursorInfo     * sCursorInfo;
    qmsPartitionRef   * sPartitionRef;
    UInt                sPartitionCount;
    UInt                sIndexUpdateColumnCount;
    UInt                i;
    idBool              sIsInplaceUpdate = ID_FALSE;

    //--------------------------------
    // cursorInfo ����
    //--------------------------------

    IDE_TEST( aTemplate->stmt->qmxMem->alloc( ID_SIZEOF(qmnCursorInfo),
                                              (void**)& sCursorInfo )
              != IDE_SUCCESS );

    // cursorInfo �ʱ�ȭ
    sCursorInfo->cursor              = NULL;
    sCursorInfo->selectedIndex       = NULL;
    sCursorInfo->selectedIndexTuple  = NULL;
    sCursorInfo->accessOption        = QCM_ACCESS_OPTION_READ_WRITE; /* PROJ-2359 Table/Partition Access Option */
    sCursorInfo->updateColumnList    = aCodePlan->updateColumnList;
    sCursorInfo->cursorType          = aCodePlan->cursorType;
    sCursorInfo->isRowMovementUpdate = aCodePlan->isRowMovementUpdate;

    /* PROJ-2626 Snapshot Export */
    if ( aTemplate->stmt->mInplaceUpdateDisableFlag == ID_FALSE )
    {
        sIsInplaceUpdate = aCodePlan->inplaceUpdate;
    }
    else
    {
        /* Nothing td do */
    }

    sCursorInfo->inplaceUpdate = sIsInplaceUpdate;

    sCursorInfo->lockMode            = SMI_LOCK_WRITE;

    sCursorInfo->stmtRetryColLst     = aCodePlan->whereColumnList;
    sCursorInfo->rowRetryColLst      = aCodePlan->setColumnList;

    // cursorInfo ����
    aDataPlan->updateTuple->cursorInfo = sCursorInfo;

    //--------------------------------
    // partition cursorInfo ����
    //--------------------------------

    if ( aCodePlan->tableRef->partitionRef != NULL )
    {
        sPartitionCount = 0;
        for ( sPartitionRef = aCodePlan->tableRef->partitionRef;
              sPartitionRef != NULL;
              sPartitionRef = sPartitionRef->next )
        {
            sPartitionCount++;
        }

        // cursorInfo ����
        IDE_TEST( aTemplate->stmt->qmxMem->alloc(
                      sPartitionCount * ID_SIZEOF(qmnCursorInfo),
                      (void**)& sCursorInfo )
                  != IDE_SUCCESS );

        for ( sPartitionRef = aCodePlan->tableRef->partitionRef,
                  i = 0;
              sPartitionRef != NULL;
              sPartitionRef = sPartitionRef->next,
                  i++, sCursorInfo++ )
        {
            // cursorInfo �ʱ�ȭ
            sCursorInfo->cursor              = NULL;
            sCursorInfo->selectedIndex       = NULL;
            sCursorInfo->selectedIndexTuple  = NULL;
            /* PROJ-2359 Table/Partition Access Option */
            sCursorInfo->accessOption        = QCM_ACCESS_OPTION_READ_WRITE;
            sCursorInfo->updateColumnList    = aCodePlan->updatePartColumnList[i];
            sCursorInfo->cursorType          = aCodePlan->cursorType;
            sCursorInfo->isRowMovementUpdate = aCodePlan->isRowMovementUpdate;
            sCursorInfo->inplaceUpdate       = sIsInplaceUpdate;
            sCursorInfo->lockMode            = SMI_LOCK_WRITE;

            /* PROJ-2464 hybrid partitioned table ���� */
            sCursorInfo->stmtRetryColLst     = aCodePlan->wherePartColumnList[i];
            sCursorInfo->rowRetryColLst      = aCodePlan->setPartColumnList[i];

            // cursorInfo ����
            aTemplate->tmplate.rows[sPartitionRef->table].cursorInfo = sCursorInfo;
        }

        // PROJ-1624 non-partitioned index
        // partitioned table�� ��� index table cursor�� update column list�� �����Ѵ�.
        if ( aCodePlan->tableRef->selectedIndexTable != NULL )
        {
            IDE_DASSERT( aCodePlan->tableRef->indexTableRef != NULL );

            sCursorInfo = (qmnCursorInfo*) aDataPlan->updateTuple->cursorInfo;

            IDE_TEST( qmsIndexTable::makeUpdateSmiColumnList(
                          aCodePlan->updateColumnCount,
                          aCodePlan->updateColumnIDs,
                          aCodePlan->tableRef->selectedIndexTable,
                          sCursorInfo->isRowMovementUpdate,
                          & sIndexUpdateColumnCount,
                          aDataPlan->indexUpdateColumnList )
                      != IDE_SUCCESS );

            if ( sIndexUpdateColumnCount > 0 )
            {
                // update�� �÷��� �ִ� ���
                sCursorInfo->updateColumnList = aDataPlan->indexUpdateColumnList;
                // index table�� �׻� update, composite ����
                sCursorInfo->cursorType       = SMI_UPDATE_CURSOR;

                // update�ؾ���
                *aDataPlan->flag &= ~QMND_UPTE_SELECTED_INDEX_CURSOR_MASK;
                *aDataPlan->flag |= QMND_UPTE_SELECTED_INDEX_CURSOR_TRUE;
            }
            else
            {
                // update�� �÷��� ���� ���
                sCursorInfo->updateColumnList = NULL;
                sCursorInfo->cursorType       = SMI_SELECT_CURSOR;
                sCursorInfo->inplaceUpdate    = ID_FALSE;
                sCursorInfo->lockMode         = SMI_LOCK_READ;
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

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnUPTE::allocTriggerRow( qcTemplate * aTemplate,
                          qmncUPTE   * aCodePlan,
                          qmndUPTE   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnUPTE::allocTriggerRow"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmnUPTE::allocTriggerRow"));

    UInt sMaxRowOffsetForUpdate = 0;
    UInt sMaxRowOffsetForInsert = 0;

    //---------------------------------
    // Trigger�� ���� ������ ����
    //---------------------------------

    if ( aCodePlan->tableRef->tableInfo->triggerCount > 0 )
    {
        if ( aCodePlan->insteadOfTrigger == ID_TRUE )
        {
            // instead of trigger������ smiValues�� ����Ѵ�.

            // alloc sOldRow
            IDE_TEST( aTemplate->stmt->qmxMem->alloc(
                    ID_SIZEOF(smiValue) *
                    aCodePlan->tableRef->tableInfo->columnCount,
                    (void**) & aDataPlan->oldRow )
                != IDE_SUCCESS);

            // alloc sNewRow
            IDE_TEST( aTemplate->stmt->qmxMem->alloc(
                    ID_SIZEOF(smiValue) *
                    aCodePlan->tableRef->tableInfo->columnCount,
                    (void**) & aDataPlan->newRow )
                != IDE_SUCCESS);
        }
        else
        {
            sMaxRowOffsetForUpdate = qmx::getMaxRowOffset( &(aTemplate->tmplate),
                                                           aCodePlan->tableRef );
            if ( aCodePlan->updateType == QMO_UPDATE_ROWMOVEMENT )
            {
                sMaxRowOffsetForInsert = qmx::getMaxRowOffset( &(aTemplate->tmplate),
                                                               aCodePlan->insertTableRef );
            }
            else
            {
                sMaxRowOffsetForInsert = 0;
            }

            if ( sMaxRowOffsetForUpdate > 0 )
            {
                // Old Row Referencing�� ���� ���� �Ҵ�
                IDE_TEST( aTemplate->stmt->qmxMem->alloc(
                            sMaxRowOffsetForUpdate,
                            (void**) & aDataPlan->oldRow )
                          != IDE_SUCCESS);
            }
            else
            {
                aDataPlan->oldRow = NULL;
            }

            sMaxRowOffsetForInsert = IDL_MAX( sMaxRowOffsetForUpdate, sMaxRowOffsetForInsert );
            if ( sMaxRowOffsetForInsert > 0)
            {
                // New Row Referencing�� ���� ���� �Ҵ�
                IDE_TEST( aTemplate->stmt->qmxMem->cralloc(
                            sMaxRowOffsetForInsert,
                            (void**) & aDataPlan->newRow )
                          != IDE_SUCCESS);
            }
            else
            {
                aDataPlan->newRow = NULL;
            }
        }

        aDataPlan->columnsForRow = aCodePlan->tableRef->tableInfo->columns;

        aDataPlan->needTriggerRow = ID_FALSE;
        aDataPlan->existTrigger = ID_TRUE;
    }
    else
    {
        // check constraint�� return into������ trigger row�� ����Ѵ�.
        if ( ( aCodePlan->checkConstrList != NULL ) ||
             ( aCodePlan->returnInto != NULL ) )
        {
            sMaxRowOffsetForUpdate = qmx::getMaxRowOffset( &(aTemplate->tmplate),
                                                           aCodePlan->tableRef );
            if ( aCodePlan->updateType == QMO_UPDATE_ROWMOVEMENT )
            {
                sMaxRowOffsetForInsert = qmx::getMaxRowOffset( &(aTemplate->tmplate),
                                                               aCodePlan->insertTableRef );
            }
            else
            {
                sMaxRowOffsetForInsert = 0;
            }

            if ( sMaxRowOffsetForUpdate > 0 )
            {
                // Old Row Referencing�� ���� ���� �Ҵ�
                IDE_TEST( aTemplate->stmt->qmxMem->alloc(
                            sMaxRowOffsetForUpdate,
                            (void**) & aDataPlan->oldRow )
                          != IDE_SUCCESS);
            }
            else
            {
                aDataPlan->oldRow = NULL;
            }

            sMaxRowOffsetForInsert = IDL_MAX( sMaxRowOffsetForUpdate, sMaxRowOffsetForInsert );
            if ( sMaxRowOffsetForInsert > 0)
            {
                // New Row Referencing�� ���� ���� �Ҵ�
                IDE_TEST( aTemplate->stmt->qmxMem->cralloc(
                            sMaxRowOffsetForInsert,
                            (void**) & aDataPlan->newRow )
                          != IDE_SUCCESS);
            }
            else
            {
                aDataPlan->newRow = NULL;
            }
        }
        else
        {
            aDataPlan->oldRow = NULL;
            aDataPlan->newRow = NULL;
        }

        aDataPlan->needTriggerRow = ID_FALSE;
        aDataPlan->existTrigger = ID_FALSE;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnUPTE::allocReturnRow( qcTemplate * aTemplate,
                         qmncUPTE   * aCodePlan,
                         qmndUPTE   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnUPTE::allocReturnRow"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmnUPTE::allocReturnRow"));

    UInt sMaxRowOffset = 0;

    //---------------------------------
    // return into�� ���� ������ ����
    //---------------------------------

    if ( ( aCodePlan->returnInto != NULL ) &&
         ( aCodePlan->insteadOfTrigger == ID_TRUE ) )
    {
        sMaxRowOffset = qmx::getMaxRowOffset( &(aTemplate->tmplate),
                                              aCodePlan->tableRef );

        if ( sMaxRowOffset > 0 )
        {
            // New Row Referencing�� ���� ���� �Ҵ�
            IDE_TEST( aTemplate->stmt->qmxMem->cralloc(
                        sMaxRowOffset,
                        (void**) & aDataPlan->returnRow )
                      != IDE_SUCCESS);
        }
        else
        {
            aDataPlan->returnRow = NULL;
        }
    }
    else
    {
        aDataPlan->returnRow = NULL;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnUPTE::allocIndexTableCursor( qcTemplate * aTemplate,
                                qmncUPTE   * aCodePlan,
                                qmndUPTE   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnUPTE::allocIndexTableCursor"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmnUPTE::allocIndexTableCursor"));

    //---------------------------------
    // index table ó���� ���� ����
    //---------------------------------

    if ( aCodePlan->tableRef->indexTableRef != NULL )
    {
        IDE_TEST( qmsIndexTable::initializeIndexTableCursors(
                      aTemplate->stmt,
                      aCodePlan->tableRef->indexTableRef,
                      aCodePlan->tableRef->indexTableCount,
                      aCodePlan->tableRef->selectedIndexTable,
                      & (aDataPlan->indexTableCursorInfo) )
                  != IDE_SUCCESS );

        *aDataPlan->flag &= ~QMND_UPTE_INDEX_CURSOR_MASK;
        *aDataPlan->flag |= QMND_UPTE_INDEX_CURSOR_INITED;
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
qmnUPTE::doItDefault( qcTemplate * /* aTemplate */,
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

#define IDE_FN "qmnUPTE::doItDefault"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    IDE_DASSERT( 0 );

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnUPTE::doItFirst( qcTemplate * aTemplate,
                    qmnPlan    * aPlan,
                    qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    UPTE�� ���� ���� �Լ�
 *
 * Implementation :
 *    - Table�� IX Lock�� �Ǵ�.
 *    - Session Event Check (������ ���� Detect)
 *    - Cursor Open
 *    - update one record
 *
 ***********************************************************************/

#define IDE_FN "qmnUPTE::doItFirst"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncUPTE * sCodePlan = (qmncUPTE*) aPlan;
    qmndUPTE * sDataPlan =
        (qmndUPTE*) (aTemplate->tmplate.data + aPlan->offset);

    /* PROJ-2359 Table/Partition Access Option */
    idBool     sIsTableCursorChanged;

    //-----------------------------------
    // Child Plan�� ������
    //-----------------------------------

    /* TASK-7307 DML Data Consistency in Shard */
    // To fix PR-3921
    if ( sDataPlan->limitCurrent == sDataPlan->limitEnd )
    {
        // �־��� Limit ���ǿ� �ٴٸ� ���
        *aFlag = QMC_ROW_DATA_NONE;
    }
    else if ( ( QCG_CHECK_SHARD_DML_CONSISTENCY( aTemplate->stmt ) == ID_TRUE ) &&
              ( sCodePlan->tableRef->tableInfo->mIsUsable == ID_FALSE ) )
    {
        // �����Ѵ�
        *aFlag = QMC_ROW_DATA_NONE;
    }
    else
    {
        // doIt left child
        IDE_TEST( aPlan->left->doIt( aTemplate, aPlan->left, aFlag )
                  != IDE_SUCCESS );
    }

    if ( ( *aFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST )
    {
        // Limit Start ó��
        for ( ;
              sDataPlan->limitCurrent < sDataPlan->limitStart;
              sDataPlan->limitCurrent++ )
        {
            // Limitation ������ ���� �ʴ´�.
            // ���� Update���� Child�� �����ϱ⸸ �Ѵ�.
            IDE_TEST( aPlan->left->doIt( aTemplate, aPlan->left, aFlag )
                      != IDE_SUCCESS );

            if ( ( *aFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_NONE )
            {
                break;
            }
            else
            {
                // Nothing To Do
            }
        }

        if ( sDataPlan->limitStart <= sDataPlan->limitEnd )
        {
            if ( ( sDataPlan->limitCurrent >= sDataPlan->limitStart ) &&
                 ( sDataPlan->limitCurrent < sDataPlan->limitEnd ) )
            {
                // Limit�� ����
                sDataPlan->limitCurrent++;
            }
            else
            {
                // Limitation ������ ��� ���
                *aFlag = QMC_ROW_DATA_NONE;
            }
        }
        else
        {
            // Nothing To Do
        }

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
                // get cursor
                IDE_TEST( getCursor( aTemplate, aPlan, &sIsTableCursorChanged ) != IDE_SUCCESS );

                /* PROJ-2359 Table/Partition Access Option */
                IDE_TEST( qmx::checkAccessOption( sCodePlan->tableRef->tableInfo,
                                                  ID_FALSE /* aIsInsertion */ )
                          != IDE_SUCCESS );

                if ( sCodePlan->tableRef->partitionRef != NULL )
                {
                    IDE_TEST( qmx::checkAccessOptionForExistentRecord(
                                        sDataPlan->accessOption,
                                        sDataPlan->updateTuple->tableHandle )
                              != IDE_SUCCESS );
                }
                else
                {
                    /* Nothing to do */
                }

                switch ( sCodePlan->updateType )
                {
                    case QMO_UPDATE_NORMAL:
                    {
                        // update one record
                        IDE_TEST( updateOneRow( aTemplate, aPlan ) != IDE_SUCCESS );

                        break;
                    }

                    case QMO_UPDATE_ROWMOVEMENT:
                    {
                        // open insert cursor
                        IDE_TEST( openInsertCursor( aTemplate, aPlan ) != IDE_SUCCESS );

                        // update one record
                        IDE_TEST( updateOneRowForRowmovement( aTemplate, aPlan )
                                  != IDE_SUCCESS );

                        break;
                    }

                    case QMO_UPDATE_CHECK_ROWMOVEMENT:
                    {
                        // update one record
                        IDE_TEST( updateOneRowForCheckRowmovement( aTemplate, aPlan )
                                  != IDE_SUCCESS );

                        break;
                    }

                    case QMO_UPDATE_NO_ROWMOVEMENT:
                    {
                        // update one record
                        IDE_TEST( updateOneRow( aTemplate, aPlan ) != IDE_SUCCESS );

                        break;
                    }

                    default:
                        IDE_DASSERT(0);
                        break;
                }
            }

            sDataPlan->doIt = qmnUPTE::doItNext;
        }
        else
        {
            // Nothing To Do
        }
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
qmnUPTE::doItNext( qcTemplate * aTemplate,
                   qmnPlan    * aPlan,
                   qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    UPTE�� ���� ���� �Լ�
 *    ���� Record�� �����Ѵ�.
 *
 * Implementation :
 *    - update one record
 *
 ***********************************************************************/

#define IDE_FN "qmnUPTE::doItNext"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncUPTE * sCodePlan = (qmncUPTE*) aPlan;
    qmndUPTE * sDataPlan =
        (qmndUPTE*) (aTemplate->tmplate.data + aPlan->offset);

    /* PROJ-2359 Table/Partition Access Option */
    idBool     sIsTableCursorChanged;
    
    //-----------------------------------
    // Child Plan�� ������
    //-----------------------------------

    // To fix PR-3921
    if ( sDataPlan->limitCurrent == sDataPlan->limitEnd )
    {
        // �־��� Limit ���ǿ� �ٴٸ� ���
        *aFlag = QMC_ROW_DATA_NONE;
    }
    else if ( ( QCG_CHECK_SHARD_DML_CONSISTENCY( aTemplate->stmt ) == ID_TRUE ) &&
              ( sCodePlan->tableRef->tableInfo->mIsUsable == ID_FALSE ) )
    {
        // �����Ѵ�
        *aFlag = QMC_ROW_DATA_NONE;
    }
    else
    {
        // doIt left child
        IDE_TEST( aPlan->left->doIt( aTemplate, aPlan->left, aFlag )
                  != IDE_SUCCESS );
    }

    if ( ( *aFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST )
    {
        if ( sDataPlan->limitStart <= sDataPlan->limitEnd )
        {
            if ( ( sDataPlan->limitCurrent >= sDataPlan->limitStart ) &&
                 ( sDataPlan->limitCurrent < sDataPlan->limitEnd ) )
            {
                // Limit�� ����
                sDataPlan->limitCurrent++;
            }
            else
            {
                // Limitation ������ ��� ���
                *aFlag = QMC_ROW_DATA_NONE;
            }
        }
        else
        {
            // Nothing To Do
        }

        if ( ( *aFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST )
        {
            if ( sCodePlan->insteadOfTrigger == ID_TRUE )
            {
                IDE_TEST( fireInsteadOfTrigger( aTemplate, aPlan ) != IDE_SUCCESS );
            }
            else
            {
                switch ( sCodePlan->updateType )
                {
                    case QMO_UPDATE_NORMAL:
                    {
                        // update one record
                        IDE_TEST( updateOneRow( aTemplate, aPlan ) != IDE_SUCCESS );

                        break;
                    }

                    case QMO_UPDATE_ROWMOVEMENT:
                    {
                        // get cursor
                        IDE_TEST( getCursor( aTemplate, aPlan, &sIsTableCursorChanged ) != IDE_SUCCESS );

                        /* PROJ-2359 Table/Partition Access Option */
                        if ( sIsTableCursorChanged == ID_TRUE )
                        {
                            IDE_TEST( qmx::checkAccessOptionForExistentRecord(
                                                sDataPlan->accessOption,
                                                sDataPlan->updateTuple->tableHandle )
                                      != IDE_SUCCESS );
                        }
                        else
                        {
                            /* Nothing to do */
                        }

                        // update one record
                        IDE_TEST( updateOneRowForRowmovement( aTemplate, aPlan )
                                  != IDE_SUCCESS );

                        break;
                    }

                    case QMO_UPDATE_CHECK_ROWMOVEMENT:
                    {
                        // get cursor
                        IDE_TEST( getCursor( aTemplate, aPlan, &sIsTableCursorChanged ) != IDE_SUCCESS );

                        /* PROJ-2359 Table/Partition Access Option */
                        if ( sIsTableCursorChanged == ID_TRUE )
                        {
                            IDE_TEST( qmx::checkAccessOptionForExistentRecord(
                                                sDataPlan->accessOption,
                                                sDataPlan->updateTuple->tableHandle )
                                      != IDE_SUCCESS );
                        }
                        else
                        {
                            /* Nothing to do */
                        }

                        // update one record
                        IDE_TEST( updateOneRowForCheckRowmovement( aTemplate, aPlan )
                                  != IDE_SUCCESS );

                        break;
                    }

                    case QMO_UPDATE_NO_ROWMOVEMENT:
                    {
                        // get cursor
                        IDE_TEST( getCursor( aTemplate, aPlan, &sIsTableCursorChanged ) != IDE_SUCCESS );

                        /* PROJ-2359 Table/Partition Access Option */
                        if ( sIsTableCursorChanged == ID_TRUE )
                        {
                            IDE_TEST( qmx::checkAccessOptionForExistentRecord(
                                                sDataPlan->accessOption,
                                                sDataPlan->updateTuple->tableHandle )
                                      != IDE_SUCCESS );
                        }
                        else
                        {
                            /* Nothing to do */
                        }

                        // update one record
                        IDE_TEST( updateOneRow( aTemplate, aPlan ) != IDE_SUCCESS );

                        break;
                    }

                    default:
                        IDE_DASSERT(0);
                        break;
                }
            }
        }
        else
        {
            // record�� ���� ���
            // ���� ������ ���� ���� ���� �Լ��� ������.
            sDataPlan->doIt = qmnUPTE::doItFirst;
        }
    }
    else
    {
        // record�� ���� ���
        // ���� ������ ���� ���� ���� �Լ��� ������.
        sDataPlan->doIt = qmnUPTE::doItFirst;
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
qmnUPTE::checkTrigger( qcTemplate * aTemplate,
                       qmnPlan    * aPlan )
{
/***********************************************************************
 *
 * Description :
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnUPTE::checkTrigger"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncUPTE * sCodePlan = (qmncUPTE*) aPlan;
    qmndUPTE * sDataPlan =
        (qmndUPTE*) (aTemplate->tmplate.data + aPlan->offset);
    idBool     sNeedTriggerRow;

    if ( sDataPlan->existTrigger == ID_TRUE )
    {
        if ( sCodePlan->insteadOfTrigger == ID_TRUE )
        {
            IDE_TEST( qdnTrigger::needTriggerRow(
                          aTemplate->stmt,
                          sCodePlan->tableRef->tableInfo,
                          QCM_TRIGGER_INSTEAD_OF,
                          QCM_TRIGGER_EVENT_UPDATE,
                          sCodePlan->updateColumnList,
                          & sNeedTriggerRow )
                      != IDE_SUCCESS );
        }
        else
        {
            // Trigger�� ���� Referencing Row�� �ʿ������� �˻�
            // PROJ-2219 Row-level before update trigger
            IDE_TEST( qdnTrigger::needTriggerRow(
                          aTemplate->stmt,
                          sCodePlan->tableRef->tableInfo,
                          QCM_TRIGGER_BEFORE,
                          QCM_TRIGGER_EVENT_UPDATE,
                          sCodePlan->updateColumnList,
                          & sNeedTriggerRow )
                      != IDE_SUCCESS );

            if ( sNeedTriggerRow == ID_FALSE )
            {
                IDE_TEST( qdnTrigger::needTriggerRow(
                              aTemplate->stmt,
                              sCodePlan->tableRef->tableInfo,
                              QCM_TRIGGER_AFTER,
                              QCM_TRIGGER_EVENT_UPDATE,
                              sCodePlan->updateColumnList,
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
qmnUPTE::getCursor( qcTemplate * aTemplate,
                    qmnPlan    * aPlan,
                    idBool     * aIsTableCursorChanged ) /* PROJ-2359 Table/Partition Access Option */
{
/***********************************************************************
 *
 * Description :
 *
 * Implementation :
 *     ���� scan�� open�� cursor�� ��´�.
 *
 ***********************************************************************/

#define IDE_FN "qmnUPTE::getCursor"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncUPTE * sCodePlan = (qmncUPTE*) aPlan;
    qmndUPTE * sDataPlan =
        (qmndUPTE*) (aTemplate->tmplate.data + aPlan->offset);

    qmnCursorInfo   * sCursorInfo = NULL;

    /* PROJ-2359 Table/Partition Access Option */
    *aIsTableCursorChanged = ID_FALSE;

    if ( sCodePlan->tableRef->partitionRef == NULL )
    {
        if ( sDataPlan->updateTupleID != sCodePlan->tableRef->table )
        {
            sDataPlan->updateTupleID = sCodePlan->tableRef->table;

            // cursor�� ��´�.
            sCursorInfo = (qmnCursorInfo*)
                aTemplate->tmplate.rows[sDataPlan->updateTupleID].cursorInfo;

            IDE_TEST_RAISE( sCursorInfo == NULL, ERR_NOT_FOUND );

            sDataPlan->updateCursor    = sCursorInfo->cursor;

            /* PROJ-2464 hybrid partitioned table ���� */
            sDataPlan->updatePartInfo = sCodePlan->tableRef->tableInfo;

            sDataPlan->retryInfo.mIsWithoutRetry  = sCodePlan->withoutRetry;
            sDataPlan->retryInfo.mStmtRetryColLst = sCursorInfo->stmtRetryColLst;
            sDataPlan->retryInfo.mRowRetryColLst  = sCursorInfo->rowRetryColLst;

            /* PROJ-2359 Table/Partition Access Option */
            sDataPlan->accessOption = sCursorInfo->accessOption;
            *aIsTableCursorChanged  = ID_TRUE;
        }
        else
        {
            /* Nothing to do */
        }
    }
    else
    {
        if ( sDataPlan->updateTupleID != sDataPlan->updateTuple->partitionTupleID )
        {
            sDataPlan->updateTupleID = sDataPlan->updateTuple->partitionTupleID;

            // partition�� cursor�� ��´�.
            sCursorInfo = (qmnCursorInfo*)
                aTemplate->tmplate.rows[sDataPlan->updateTupleID].cursorInfo;

            /* BUG-42440 BUG-39399 has invalid erorr message */
            if ( ( sDataPlan->updateTuple->lflag & MTC_TUPLE_PARTITIONED_TABLE_MASK )
                 == MTC_TUPLE_PARTITIONED_TABLE_TRUE )
            {
                IDE_TEST_RAISE( sCursorInfo == NULL, ERR_MODIFY_UNABLE_RECORD );
            }
            else
            {
                /* Nothing to do */
            }
            IDE_TEST_RAISE( sCursorInfo == NULL, ERR_NOT_FOUND );

            sDataPlan->updateCursor    = sCursorInfo->cursor;

            /* PROJ-2464 hybrid partitioned table ���� */
            IDE_TEST( smiGetTableTempInfo( sDataPlan->updateTuple->tableHandle,
                                           (void **)&(sDataPlan->updatePartInfo) )
                      != IDE_SUCCESS );

            sDataPlan->retryInfo.mIsWithoutRetry  = sCodePlan->withoutRetry;
            sDataPlan->retryInfo.mStmtRetryColLst = sCursorInfo->stmtRetryColLst;
            sDataPlan->retryInfo.mRowRetryColLst  = sCursorInfo->rowRetryColLst;

            /* PROJ-2359 Table/Partition Access Option */
            sDataPlan->accessOption = sCursorInfo->accessOption;
            *aIsTableCursorChanged  = ID_TRUE;
        }
        else
        {
            /* Nothing to do */
        }

        // index table cursor�� ��´�.
        if ( sDataPlan->indexUpdateTuple == NULL )
        {
            sCursorInfo = (qmnCursorInfo*)
                aTemplate->tmplate.rows[sCodePlan->tableRef->table].cursorInfo;

            IDE_TEST_RAISE( sCursorInfo == NULL, ERR_NOT_FOUND );

            sDataPlan->indexUpdateCursor    = sCursorInfo->cursor;

            sDataPlan->indexUpdateTuple = sCursorInfo->selectedIndexTuple;
        }
        else
        {
            // Nothing to do.
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NOT_FOUND )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmnUPTE::getCursor",
                                  "cursor not found" ));
    }
    IDE_EXCEPTION( ERR_MODIFY_UNABLE_RECORD );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMN_MODIFY_UNABLE_RECORD ) ) ;
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnUPTE::openInsertCursor( qcTemplate * aTemplate,
                           qmnPlan    * aPlan )
{
/***********************************************************************
 *
 * Description :
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnUPTE::openInsertCursor"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncUPTE * sCodePlan = (qmncUPTE*) aPlan;
    qmndUPTE * sDataPlan =
        (qmndUPTE*) (aTemplate->tmplate.data + aPlan->offset);

    smiCursorProperties   sCursorProperty;
    UShort                sTupleID         = 0;
    idBool                sIsDiskChecked   = ID_FALSE;
    smiFetchColumnList  * sFetchColumnList = NULL;

    if ( ( ( *sDataPlan->flag & QMND_UPTE_CURSOR_MASK )
           == QMND_UPTE_CURSOR_CLOSED )
         &&
         ( sCodePlan->insertTableRef != NULL ) )
    {
        // INSERT �� ���� Cursor ����
        SMI_CURSOR_PROP_INIT_FOR_FULL_SCAN( &sCursorProperty, aTemplate->stmt->mStatistics );

        if ( sCodePlan->insertTableRef->tableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
        {
            if ( sCodePlan->insertTableRef->partitionSummary->diskPartitionRef != NULL )
            {
                sTupleID = sCodePlan->insertTableRef->partitionSummary->diskPartitionRef->table;
                sIsDiskChecked = ID_TRUE;
            }
            else
            {
                /* Nothing to do */
            }
        }
        else
        {
            if ( QCM_TABLE_TYPE_IS_DISK( sCodePlan->insertTableRef->tableInfo->tableFlag ) == ID_TRUE )
            {
                sTupleID = sCodePlan->insertTableRef->table;
                sIsDiskChecked = ID_TRUE;
            }
            else
            {
                /* Nothing to do */
            }
        }

        if ( sIsDiskChecked == ID_TRUE )
        {
            // PROJ-1705
            // ����Ű üũ�� ���� �о�� �� ��ġ�÷�����Ʈ ����
            IDE_TEST( qdbCommon::makeFetchColumnList4TupleID(
                          aTemplate,
                          sTupleID,
                          sDataPlan->needTriggerRow,  // aIsNeedAllFetchColumn
                          NULL,             // index
                          ID_TRUE,          // allocSmiColumnListEx
                          & sFetchColumnList )
                      != IDE_SUCCESS );

            sCursorProperty.mFetchColumnList = sFetchColumnList;
        }
        else
        {
            /* Nothing to do */
        }

        IDE_TEST( sDataPlan->insertCursorMgr.openCursor(
                      aTemplate->stmt,
                      SMI_LOCK_WRITE | SMI_TRAVERSE_FORWARD | SMI_PREVIOUS_DISABLE,
                      & sCursorProperty )
                  != IDE_SUCCESS );

        *sDataPlan->flag &= ~QMND_UPTE_CURSOR_MASK;
        *sDataPlan->flag |= QMND_UPTE_CURSOR_OPEN;
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
qmnUPTE::closeCursor( qcTemplate * aTemplate,
                      qmnPlan    * aPlan )
{
/***********************************************************************
 *
 * Description :
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnUPTE::closeCursor"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmndUPTE * sDataPlan =
        (qmndUPTE*) (aTemplate->tmplate.data + aPlan->offset);

    if ( ( *sDataPlan->flag & QMND_UPTE_CURSOR_MASK )
         == QMND_UPTE_CURSOR_OPEN )
    {
        *sDataPlan->flag &= ~QMND_UPTE_CURSOR_MASK;
        *sDataPlan->flag |= QMND_UPTE_CURSOR_CLOSED;

        IDE_TEST( sDataPlan->insertCursorMgr.closeCursor()
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    if ( ( *sDataPlan->flag & QMND_UPTE_INDEX_CURSOR_MASK )
         == QMND_UPTE_INDEX_CURSOR_INITED )
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

    if ( ( *sDataPlan->flag & QMND_UPTE_INDEX_CURSOR_MASK )
         == QMND_UPTE_INDEX_CURSOR_INITED )
    {
        qmsIndexTable::finalizeIndexTableCursors(
            & (sDataPlan->indexTableCursorInfo) );
    }

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnUPTE::updateOneRow( qcTemplate * aTemplate,
                       qmnPlan    * aPlan )
{
/***********************************************************************
 *
 * Description :
 *    UPTE �� ���� ����� �����Ѵ�.
 *
 * Implementation :
 *    - update one record ����
 *    - trigger each row ����
 *
 ***********************************************************************/

#define IDE_FN "qmnUPTE::updateOneRow"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncUPTE * sCodePlan = (qmncUPTE*) aPlan;
    qmndUPTE * sDataPlan =
        (qmndUPTE*) (aTemplate->tmplate.data + aPlan->offset);

    void              * sOrgRow;

    smiValue          * sSmiValues;
    smiValue          * sSmiValuesForPartition = NULL;
    smiValue            sValuesForPartition[QC_MAX_COLUMN_COUNT];
    UInt                sSmiValuesValueCount   = 0;
    void              * sNewRow                = NULL;
    void              * sRow;

    // PROJ-1784 DML Without Retry
    smiValue            sWhereSmiValues[QC_MAX_COLUMN_COUNT];
    smiValue            sSetSmiValues[QC_MAX_COLUMN_COUNT];
    idBool              sIsDiskTableOrPartition = ID_FALSE;

    /* PROJ-2464 hybrid partitioned table ����
     * Memory�� ���, newRow�� ���ο� �ּҸ� �Ҵ��Ѵ�. ����, newRow�� ���� �������� �ʴ´�.
     */
    sNewRow = sDataPlan->newRow;

    if ( ( sCodePlan->flag & QMNC_UPTE_VIEW_KEY_PRESERVED_MASK )
         == QMNC_UPTE_VIEW_KEY_PRESERVED_FALSE )
    {
        /* BUG-39399 remove search key preserved table */
        if ( ( sCodePlan->flag & QMNC_UPTE_VIEW_MASK )
             == QMNC_UPTE_VIEW_TRUE )
        {
            IDE_TEST( checkDuplicateUpdate( sCodePlan,
                                            sDataPlan )
                      != IDE_SUCCESS );
        }
        else
        {
            /* Nothing To Do */
        }
    }
    
    //-----------------------------------
    // clear lob
    //-----------------------------------

    // PROJ-1362
    if ( sDataPlan->lobInfo != NULL )
    {
        (void) qmx::clearLobInfo( sDataPlan->lobInfo );
    }
    else
    {
        // Nothing to do.
    }

    //-----------------------------------
    // copy old row
    //-----------------------------------

    if ( sDataPlan->needTriggerRow == ID_TRUE )
    {
        // OLD ROW REFERENCING�� ���� ����
        idlOS::memcpy( sDataPlan->oldRow,
                       sDataPlan->updateTuple->row,
                       sDataPlan->updateTuple->rowOffset );
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
    // make update smiValues
    //-----------------------------------

    sSmiValues = aTemplate->insOrUptRow[ sCodePlan->valueIdx ];
    sSmiValuesValueCount = aTemplate->insOrUptRowValueCount[sCodePlan->valueIdx];

    // subquery�� ������ smi value ���� ����
    IDE_TEST( qmx::makeSmiValueForSubquery( aTemplate,
                                            sCodePlan->tableRef->tableInfo,
                                            sCodePlan->columns,
                                            sCodePlan->subqueries,
                                            sCodePlan->canonizedTuple,
                                            sSmiValues,
                                            sCodePlan->isNull,
                                            sDataPlan->lobInfo )
              != IDE_SUCCESS );

    // ������ smi value ���� ����
    IDE_TEST( qmx::makeSmiValueForUpdate( aTemplate,
                                          sCodePlan->tableRef->tableInfo,
                                          sCodePlan->columns,
                                          sCodePlan->values,
                                          sCodePlan->canonizedTuple,
                                          sSmiValues,
                                          sCodePlan->isNull,
                                          sDataPlan->lobInfo )
              != IDE_SUCCESS );

    //-----------------------------------
    // Default Expr
    //-----------------------------------

    if ( sCodePlan->defaultExprColumns != NULL )
    {
        qmsDefaultExpr::setRowBufferFromBaseColumn(
            &(aTemplate->tmplate),
            sCodePlan->tableRef->table,
            sCodePlan->defaultExprTableRef->table,
            sCodePlan->defaultExprBaseColumns,
            sDataPlan->defaultExprRowBuffer );

        IDE_TEST( qmsDefaultExpr::setRowBufferFromSmiValueArray(
                      &(aTemplate->tmplate),
                      sCodePlan->defaultExprTableRef,
                      sCodePlan->columns,
                      sDataPlan->defaultExprRowBuffer,
                      sSmiValues,
                      QCM_TABLE_TYPE_IS_DISK( sCodePlan->tableRef->tableInfo->tableFlag ) )
                  != IDE_SUCCESS );

        IDE_TEST( qmsDefaultExpr::calculateDefaultExpression(
                      aTemplate,
                      sCodePlan->defaultExprTableRef,
                      sCodePlan->columns,
                      sCodePlan->defaultExprColumns,
                      sDataPlan->defaultExprRowBuffer,
                      sSmiValues,
                      sCodePlan->tableRef->tableInfo->columns )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    //-----------------------------------
    // PROJ-2334 PMT
    // set update trigger memory variable column info
    //-----------------------------------
    if ( ( sDataPlan->existTrigger == ID_TRUE ) &&
         ( sCodePlan->tableRef->tableInfo->tablePartitionType == QCM_PARTITIONED_TABLE ) )
    {
        sDataPlan->columnsForRow = sDataPlan->updatePartInfo->columns;
    }
    else
    {
        // Nothing To Do
    }

    // PROJ-2219 Row-level before update trigger
    if ( sDataPlan->existTrigger == ID_TRUE )
    {
        // ���� trigger���� lob column�� �ִ� table�� �������� �����Ƿ�
        // Table cursor�� NULL�� �ѱ��.
        IDE_TEST( qdnTrigger::fireTrigger(
                      aTemplate->stmt,
                      aTemplate->stmt->qmxMem,
                      sCodePlan->tableRef->tableInfo,
                      QCM_TRIGGER_ACTION_EACH_ROW,
                      QCM_TRIGGER_BEFORE,
                      QCM_TRIGGER_EVENT_UPDATE,
                      sCodePlan->updateColumnList, // UPDATE Column
                      NULL,                        // Table Cursor
                      SC_NULL_GRID,                // Row GRID
                      sDataPlan->oldRow,           // OLD ROW
                      sDataPlan->columnsForRow,    // OLD ROW Column
                      sSmiValues,                  // NEW ROW(value list)
                      sCodePlan->columns )         // NEW ROW Column
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    //-----------------------------------
    // update one row
    //-----------------------------------

    if ( sDataPlan->retryInfo.mIsWithoutRetry == ID_TRUE )
    {
        if ( sCodePlan->tableRef->tableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
        {
            sIsDiskTableOrPartition = QCM_TABLE_TYPE_IS_DISK( sDataPlan->updatePartInfo->tableFlag );
        }
        else
        {
            sIsDiskTableOrPartition = QCM_TABLE_TYPE_IS_DISK( sCodePlan->tableRef->tableInfo->tableFlag );
        }

        if ( sIsDiskTableOrPartition == ID_TRUE )
        {
            IDE_TEST( qmx::setChkSmiValueList( sDataPlan->updateTuple->row,
                                               sDataPlan->retryInfo.mStmtRetryColLst,
                                               sWhereSmiValues,
                                               & (sDataPlan->retryInfo.mStmtRetryValLst) )
                      != IDE_SUCCESS );

            IDE_TEST( qmx::setChkSmiValueList( sDataPlan->updateTuple->row,
                                               sDataPlan->retryInfo.mRowRetryColLst,
                                               sSetSmiValues,
                                               & (sDataPlan->retryInfo.mRowRetryValLst) )
                      != IDE_SUCCESS );
        }
        else
        {
            /* Nothing to do */
        }
    }
    else
    {
        // Nothing to do.
    }

    // PROJ-2264 Dictionary table
    if( sCodePlan->compressedTuple != UINT_MAX )
    {
        IDE_TEST( qmx::makeSmiValueForCompress( aTemplate,
                                                sCodePlan->compressedTuple,
                                                sSmiValues )
                  != IDE_SUCCESS );
    }
    
    sDataPlan->retryInfo.mIsRowRetry = ID_FALSE;

    if ( QCM_TABLE_TYPE_IS_DISK( sCodePlan->tableRef->tableInfo->tableFlag ) !=
         QCM_TABLE_TYPE_IS_DISK( sDataPlan->updatePartInfo->tableFlag ) )
    {
        /* PROJ-2464 hybrid partitioned table ����
         * Partitioned Table�� �������� ���� smiValue Array�� Table Partition�� �°� ��ȯ�Ѵ�.
         */
        IDE_TEST( qmx::makeSmiValueWithSmiValue( sCodePlan->tableRef->tableInfo,
                                                 sDataPlan->updatePartInfo,
                                                 sCodePlan->columns,
                                                 sSmiValuesValueCount,
                                                 sSmiValues,
                                                 sValuesForPartition )
                  != IDE_SUCCESS );

        sSmiValuesForPartition = sValuesForPartition;
    }
    else
    {
        sSmiValuesForPartition = sSmiValues;
    }
    
    if ( ( sCodePlan->flag & QMNC_UPTE_VIEW_KEY_PRESERVED_MASK )
         == QMNC_UPTE_VIEW_KEY_PRESERVED_TRUE )
    {
        // PROJ-2204 join update, delete
        // tuple ������ cursor�� �����ؾ��Ѵ�.
        if ( ( sCodePlan->flag & QMNC_UPTE_VIEW_MASK )
             == QMNC_UPTE_VIEW_TRUE )
        {
            IDE_TEST( sDataPlan->updateCursor->setRowPosition(
                          sDataPlan->updateTuple->row,
                          sDataPlan->updateTuple->rid )
                      != IDE_SUCCESS );
        }
        else
        {
            // Nothing to do.
        }
    }

    while( sDataPlan->updateCursor->updateRow( sSmiValuesForPartition,
                                               &( sDataPlan->retryInfo ),
                                               & sRow,
                                               & sDataPlan->rowGRID )
           != IDE_SUCCESS )
    {
        IDE_TEST( ideGetErrorCode() != smERR_RETRY_Row_Retry );

        IDE_TEST( sDataPlan->updateCursor->getLastRow(
                      (const void**) &(sDataPlan->updateTuple->row),
                      & sDataPlan->updateTuple->rid )
                  != IDE_SUCCESS );

        if ( sDataPlan->needTriggerRow == ID_TRUE )
        {
            // OLD ROW REFERENCING�� ���� ����
            idlOS::memcpy( sDataPlan->oldRow,
                           sDataPlan->updateTuple->row,
                           sDataPlan->updateTuple->rowOffset );
        }
        else
        {
            // Nothing to do.
        }

        // PROJ-1362
        if ( sDataPlan->lobInfo != NULL )
        {
            (void) qmx::clearLobInfo( sDataPlan->lobInfo );
        }
        else
        {
            // Nothing to do.
        }

        // ������ smi value ���� ����
        IDE_TEST( qmx::makeSmiValueForUpdate( aTemplate,
                                              sCodePlan->tableRef->tableInfo,
                                              sCodePlan->columns,
                                              sCodePlan->values,
                                              sCodePlan->canonizedTuple,
                                              sSmiValues,
                                              sCodePlan->isNull,
                                              sDataPlan->lobInfo )
                  != IDE_SUCCESS );

        //-----------------------------------
        // Default Expr
        //-----------------------------------

        if ( sCodePlan->defaultExprColumns != NULL )
        {
            qmsDefaultExpr::setRowBufferFromBaseColumn(
                &(aTemplate->tmplate),
                sCodePlan->tableRef->table,
                sCodePlan->defaultExprTableRef->table,
                sCodePlan->defaultExprBaseColumns,
                sDataPlan->defaultExprRowBuffer );

            IDE_TEST( qmsDefaultExpr::setRowBufferFromSmiValueArray(
                          &(aTemplate->tmplate),
                          sCodePlan->defaultExprTableRef,
                          sCodePlan->columns,
                          sDataPlan->defaultExprRowBuffer,
                          sSmiValues,
                          QCM_TABLE_TYPE_IS_DISK( sCodePlan->tableRef->tableInfo->tableFlag ) )
                      != IDE_SUCCESS );

            IDE_TEST( qmsDefaultExpr::calculateDefaultExpression(
                          aTemplate,
                          sCodePlan->defaultExprTableRef,
                          sCodePlan->columns,
                          sCodePlan->defaultExprColumns,
                          sDataPlan->defaultExprRowBuffer,
                          sSmiValues,
                          sCodePlan->tableRef->tableInfo->columns )
                      != IDE_SUCCESS );
        }
        else
        {
            /* Nothing to do */
        }

        if ( sIsDiskTableOrPartition == ID_TRUE )
        {
            IDE_TEST( qmx::setChkSmiValueList( sDataPlan->updateTuple->row,
                                               sDataPlan->retryInfo.mRowRetryColLst,
                                               sSetSmiValues,
                                               & (sDataPlan->retryInfo.mRowRetryValLst) )
                      != IDE_SUCCESS );
        }
        else
        {
            // Nothing to do.
        }

        sDataPlan->retryInfo.mIsRowRetry = ID_TRUE;

        if ( QCM_TABLE_TYPE_IS_DISK( sCodePlan->tableRef->tableInfo->tableFlag ) !=
             QCM_TABLE_TYPE_IS_DISK( sDataPlan->updatePartInfo->tableFlag ) )
        {
            /* PROJ-2464 hybrid partitioned table ����
             * Partitioned Table�� �������� ���� smiValue Array�� Table Partition�� �°� ��ȯ�Ѵ�.
             */
            IDE_TEST( qmx::makeSmiValueWithSmiValue( sCodePlan->tableRef->tableInfo,
                                                     sDataPlan->updatePartInfo,
                                                     sCodePlan->columns,
                                                     sSmiValuesValueCount,
                                                     sSmiValues,
                                                     sValuesForPartition )
                      != IDE_SUCCESS );

            sSmiValuesForPartition = sValuesForPartition;
        }
        else
        {
            sSmiValuesForPartition = sSmiValues;
        }
    }

    // update index table
    IDE_TEST( updateIndexTableCursor( aTemplate,
                                      sCodePlan,
                                      sDataPlan,
                                      sSmiValues )
              != IDE_SUCCESS );

    /* PROJ-2464 hybrid partitioned table ����
     *  Disk Partition�� ���, Disk Type�� Lob Column�� �ʿ��ϴ�.
     *  Memory/Volatile Partition�� ���, �ش� Partition�� Lob Column�� �ʿ��ϴ�.
     */
    if ( ( QCM_TABLE_TYPE_IS_DISK( sCodePlan->tableRef->tableInfo->tableFlag ) !=
           QCM_TABLE_TYPE_IS_DISK( sDataPlan->updatePartInfo->tableFlag ) ) ||
         ( ( sCodePlan->tableRef->tableInfo->tablePartitionType == QCM_PARTITIONED_TABLE ) &&
           ( QCM_TABLE_TYPE_IS_DISK( sDataPlan->updatePartInfo->tableFlag ) != ID_TRUE ) ) )
    {
        IDE_TEST( qmx::changeLobColumnInfo( sDataPlan->lobInfo,
                                            sDataPlan->updatePartInfo->columns )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    // Lob�÷� ó��
    IDE_TEST( qmx::copyAndOutBindLobInfo( aTemplate->stmt,
                                          sDataPlan->lobInfo,
                                          sDataPlan->updateCursor,
                                          sRow,
                                          sDataPlan->rowGRID )
              != IDE_SUCCESS );

    //-----------------------------------
    // check constraint
    //-----------------------------------

    if ( ( sDataPlan->needTriggerRow == ID_TRUE ) ||
         ( sCodePlan->checkConstrList != NULL ) ||
         ( sCodePlan->returnInto != NULL ) )
    {
        // NEW ROW�� ȹ��
        IDE_TEST( sDataPlan->updateCursor->getLastModifiedRow(
                      & sNewRow,
                      sDataPlan->updateTuple->rowOffset )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    /* PROJ-1107 Check Constraint ���� */
    if ( sCodePlan->checkConstrList != NULL )
    {
        sOrgRow = sDataPlan->updateTuple->row;
        sDataPlan->updateTuple->row = sNewRow;

        IDE_TEST( qdnCheck::verifyCheckConstraintList(
                      aTemplate,
                      sCodePlan->checkConstrList )
                  != IDE_SUCCESS );

        sDataPlan->updateTuple->row = sOrgRow;
    }
    else
    {
        // Nothing to do.
    }

    //-----------------------------------
    // update after trigger
    //-----------------------------------

    if ( sDataPlan->existTrigger == ID_TRUE )
    {
        // PROJ-1359 Trigger
        // ROW GRANULARITY TRIGGER�� ����
        IDE_TEST( qdnTrigger::fireTrigger( aTemplate->stmt,
                                           aTemplate->stmt->qmxMem,
                                           sCodePlan->tableRef->tableInfo,
                                           QCM_TRIGGER_ACTION_EACH_ROW,
                                           QCM_TRIGGER_AFTER,
                                           QCM_TRIGGER_EVENT_UPDATE,
                                           sCodePlan->updateColumnList,
                                           sDataPlan->updateCursor,         /* Table Cursor */
                                           sDataPlan->updateTuple->rid,     /* Row GRID */
                                           sDataPlan->oldRow,
                                           sDataPlan->columnsForRow,
                                           sNewRow,
                                           sDataPlan->columnsForRow )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    //-----------------------------------
    // return into
    //-----------------------------------

    /* PROJ-1584 DML Return Clause */
    if ( sCodePlan->returnInto != NULL )
    {
        sOrgRow = sDataPlan->updateTuple->row;
        sDataPlan->updateTuple->row = sNewRow;

        IDE_TEST( qmx::copyReturnToInto( aTemplate,
                                         sCodePlan->returnInto,
                                         aTemplate->numRows )
                  != IDE_SUCCESS );

        sDataPlan->updateTuple->row = sOrgRow;
    }
    else
    {
        // nothing do do
    }

    if ( ( *sDataPlan->flag & QMND_UPTE_UPDATE_MASK )
         == QMND_UPTE_UPDATE_FALSE )
    {
        *sDataPlan->flag &= ~QMND_UPTE_UPDATE_MASK;
        *sDataPlan->flag |= QMND_UPTE_UPDATE_TRUE;
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
qmnUPTE::updateOneRowForRowmovement( qcTemplate * aTemplate,
                                     qmnPlan    * aPlan )
{
/***********************************************************************
 *
 * Description :
 *    UPTE �� ���� ����� �����Ѵ�.
 *
 * Implementation :
 *    - update one record ����
 *    - trigger each row ����
 *
 ***********************************************************************/

#define IDE_FN "qmnUPTE::updateOneRowForRowmovement"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncUPTE * sCodePlan = (qmncUPTE*) aPlan;
    qmndUPTE * sDataPlan =
        (qmndUPTE*) (aTemplate->tmplate.data + aPlan->offset);

    smiTableCursor    * sInsertCursor = NULL;
    void              * sOrgRow;

    UShort              sPartitionTupleID       = 0;
    mtcTuple          * sSelectedPartitionTuple = NULL;
    mtcTuple            sCopyTuple;
    idBool              sNeedToRecoverTuple     = ID_FALSE;

    qmsPartitionRef   * sSelectedPartitionRef;
    qcmTableInfo      * sSelectedPartitionInfo = NULL;
    qmnCursorInfo     * sCursorInfo;
    smiTableCursor    * sCursor                = NULL;
    smiValue          * sSmiValues;
    smiValue          * sSmiValuesForPartition = NULL;
    smiValue            sValuesForPartition[QC_MAX_COLUMN_COUNT];
    UInt                sSmiValuesValueCount   = 0;
    void              * sNewRow                = NULL;
    void              * sRow                   = NULL;
    smOID               sPartOID;
    qcmColumn         * sColumnsForNewRow      = NULL;
    
    /* PROJ-2464 hybrid partitioned table ����
     * Memory�� ���, newRow�� ���ο� �ּҸ� �Ҵ��Ѵ�. ����, newRow�� ���� �������� �ʴ´�.
     */
    sNewRow = sDataPlan->newRow;

    if ( ( sCodePlan->flag & QMNC_UPTE_VIEW_KEY_PRESERVED_MASK )
         == QMNC_UPTE_VIEW_KEY_PRESERVED_FALSE )
    {
        /* BUG-39399 remove search key preserved table */
        if ( ( sCodePlan->flag & QMNC_UPTE_VIEW_MASK )
             == QMNC_UPTE_VIEW_TRUE )
        {
            IDE_TEST( checkDuplicateUpdate( sCodePlan,
                                            sDataPlan )
                      != IDE_SUCCESS );
        }
        else
        {
            /* Nothing To Do */
        }
    }
    
    //-----------------------------------
    // clear lob
    //-----------------------------------

    // PROJ-1362
    if ( sDataPlan->lobInfo != NULL )
    {
        (void) qmx::clearLobInfo( sDataPlan->lobInfo );
    }
    else
    {
        // Nothing to do.
    }

    // PROJ-1362
    if ( sDataPlan->insertLobInfo != NULL )
    {
        (void) qmx::initLobInfo( sDataPlan->insertLobInfo );
    }
    else
    {
        // Nothing to do.
    }

    //-----------------------------------
    // copy old row
    //-----------------------------------

    if ( sDataPlan->needTriggerRow == ID_TRUE )
    {
        // OLD ROW REFERENCING�� ���� ����
        idlOS::memcpy( sDataPlan->oldRow,
                       sDataPlan->updateTuple->row,
                       sDataPlan->updateTuple->rowOffset );
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

    //-----------------------------------
    // make update smiValues
    //-----------------------------------

    sSmiValues = aTemplate->insOrUptRow[ sCodePlan->valueIdx ];
    sSmiValuesValueCount = aTemplate->insOrUptRowValueCount[sCodePlan->valueIdx];

    // subquery�� ������ smi value ���� ����
    IDE_TEST( qmx::makeSmiValueForSubquery( aTemplate,
                                            sCodePlan->tableRef->tableInfo,
                                            sCodePlan->columns,
                                            sCodePlan->subqueries,
                                            sCodePlan->canonizedTuple,
                                            sSmiValues,
                                            sCodePlan->isNull,
                                            sDataPlan->lobInfo )
              != IDE_SUCCESS );

    // ������ smi value ���� ����
    IDE_TEST( qmx::makeSmiValueForUpdate( aTemplate,
                                          sCodePlan->tableRef->tableInfo,
                                          sCodePlan->columns,
                                          sCodePlan->values,
                                          sCodePlan->canonizedTuple,
                                          sSmiValues,
                                          sCodePlan->isNull,
                                          sDataPlan->lobInfo )
              != IDE_SUCCESS );

    /* PROJ-1090 Function-based Index */
    if ( sCodePlan->defaultExprColumns != NULL )
    {
        qmsDefaultExpr::setRowBufferFromBaseColumn(
            &(aTemplate->tmplate),
            sCodePlan->tableRef->table,
            sCodePlan->defaultExprTableRef->table,
            sCodePlan->defaultExprBaseColumns,
            sDataPlan->defaultExprRowBuffer );

        IDE_TEST( qmsDefaultExpr::setRowBufferFromSmiValueArray(
                      &(aTemplate->tmplate),
                      sCodePlan->defaultExprTableRef,
                      sCodePlan->columns,
                      sDataPlan->defaultExprRowBuffer,
                      sSmiValues,
                      QCM_TABLE_TYPE_IS_DISK( sCodePlan->tableRef->tableInfo->tableFlag ) )
                  != IDE_SUCCESS );

        IDE_TEST( qmsDefaultExpr::calculateDefaultExpression(
                      aTemplate,
                      sCodePlan->defaultExprTableRef,
                      sCodePlan->columns,
                      sCodePlan->defaultExprColumns,
                      sDataPlan->defaultExprRowBuffer,
                      sSmiValues,
                      sCodePlan->tableRef->tableInfo->columns )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    // PROJ-2219 Row-level before update trigger
    if ( sDataPlan->existTrigger == ID_TRUE )
    {
        // ���� trigger���� lob column�� �ִ� table�� �������� �����Ƿ�
        // Table cursor�� NULL�� �ѱ��.
        IDE_TEST( qdnTrigger::fireTrigger(
                      aTemplate->stmt,
                      aTemplate->stmt->qmxMem,
                      sCodePlan->tableRef->tableInfo,
                      QCM_TRIGGER_ACTION_EACH_ROW,
                      QCM_TRIGGER_BEFORE,
                      QCM_TRIGGER_EVENT_UPDATE,
                      sCodePlan->updateColumnList,        // UPDATE Column
                      NULL,                               // Table Cursor
                      SC_NULL_GRID,                       // Row GRID
                      sDataPlan->oldRow,                  // OLD ROW
                      sDataPlan->updatePartInfo->columns, // OLD ROW Column
                      sSmiValues,                         // NEW ROW(value list)
                      sCodePlan->columns )                // NEW ROW Column
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    // row movement�� smi value ���� ����
    IDE_TEST( qmx::makeSmiValueForRowMovement(
                  sCodePlan->tableRef->tableInfo,
                  sCodePlan->updateColumnList,
                  sSmiValues,
                  sDataPlan->updateTuple,
                  sDataPlan->updateCursor,
                  sDataPlan->lobInfo,
                  sDataPlan->insertValues,
                  sDataPlan->insertLobInfo )
              != IDE_SUCCESS );

    //-----------------------------------
    // update one row
    //-----------------------------------

    if ( qmoPartition::partitionFilteringWithRow(
             sCodePlan->tableRef,
             sDataPlan->insertValues,
             &sSelectedPartitionRef )
         != IDE_SUCCESS )
    {
        IDE_CLEAR();

        //-----------------------------------
        // tableRef�� ���� partition�� ���
        // insert row -> update row
        //-----------------------------------

        /* PROJ-1090 Function-based Index */
        if ( sCodePlan->defaultExprColumns != NULL )
        {
            IDE_TEST( qmsDefaultExpr::setRowBufferFromSmiValueArray(
                          &(aTemplate->tmplate),
                          sCodePlan->defaultExprTableRef,
                          sCodePlan->tableRef->tableInfo->columns,
                          sDataPlan->defaultExprRowBuffer,
                          sDataPlan->insertValues,
                          QCM_TABLE_TYPE_IS_DISK( sCodePlan->tableRef->tableInfo->tableFlag ) )
                      != IDE_SUCCESS );

            IDE_TEST( qmsDefaultExpr::calculateDefaultExpression(
                          aTemplate,
                          sCodePlan->defaultExprTableRef,
                          NULL,
                          sCodePlan->defaultExprColumns,
                          sDataPlan->defaultExprRowBuffer,
                          sDataPlan->insertValues,
                          sCodePlan->tableRef->tableInfo->columns )
                      != IDE_SUCCESS );
        }
        else
        {
            /* Nothing to do */
        }

        IDE_TEST( sDataPlan->insertCursorMgr.partitionFilteringWithRow(
                      sDataPlan->insertValues,
                      sDataPlan->insertLobInfo,
                      &sSelectedPartitionInfo )
                  != IDE_SUCCESS );

        /* PROJ-2359 Table/Partition Access Option */
        IDE_TEST( qmx::checkAccessOption( sSelectedPartitionInfo,
                                          ID_TRUE, /* aIsInsertion */
                                          QCG_CHECK_SHARD_DML_CONSISTENCY( aTemplate->stmt ) )
                  != IDE_SUCCESS );

        // insert row
        IDE_TEST( sDataPlan->insertCursorMgr.getCursor( &sCursor )
                  != IDE_SUCCESS );

        if ( QCM_TABLE_TYPE_IS_DISK( sCodePlan->tableRef->tableInfo->tableFlag ) !=
             QCM_TABLE_TYPE_IS_DISK( sSelectedPartitionInfo->tableFlag ) )
        {
            /* PROJ-2464 hybrid partitioned table ����
             * Partitioned Table�� �������� ���� smiValue Array�� Table Partition�� �°� ��ȯ�Ѵ�.
             */
            IDE_TEST( qmx::makeSmiValueWithSmiValue( sCodePlan->tableRef->tableInfo,
                                                     sSelectedPartitionInfo,
                                                     sCodePlan->tableRef->tableInfo->columns,
                                                     sCodePlan->tableRef->tableInfo->columnCount,
                                                     sDataPlan->insertValues,
                                                     sValuesForPartition )
                      != IDE_SUCCESS );

            sSmiValuesForPartition = sValuesForPartition;
        }
        else
        {
            sSmiValuesForPartition = sDataPlan->insertValues;
        }

        IDE_TEST( sCursor->insertRow( sSmiValuesForPartition,
                                      & sRow,
                                      & sDataPlan->rowGRID )
                  != IDE_SUCCESS );

        IDE_TEST( sDataPlan->insertCursorMgr.getSelectedPartitionOID(
                      & sPartOID )
                  != IDE_SUCCESS );

        // update index table
        IDE_TEST( updateIndexTableCursorForRowMovement( aTemplate,
                                                        sCodePlan,
                                                        sDataPlan,
                                                        sPartOID,
                                                        sDataPlan->rowGRID,
                                                        sSmiValues )
                  != IDE_SUCCESS );

        IDE_TEST( qmx::copyAndOutBindLobInfo( aTemplate->stmt,
                                              sDataPlan->insertLobInfo,
                                              sCursor,
                                              sRow,
                                              sDataPlan->rowGRID )
                  != IDE_SUCCESS );

    if ( ( sCodePlan->flag & QMNC_UPTE_VIEW_KEY_PRESERVED_MASK )
         == QMNC_UPTE_VIEW_KEY_PRESERVED_TRUE )
        {
            // PROJ-2204 join update, delete
            // tuple ������ cursor�� �����ؾ��Ѵ�.
            if ( ( sCodePlan->flag & QMNC_UPTE_VIEW_MASK )
                 == QMNC_UPTE_VIEW_TRUE )
            {
                IDE_TEST( sDataPlan->updateCursor->setRowPosition(
                              sDataPlan->updateTuple->row,
                              sDataPlan->updateTuple->rid )
                          != IDE_SUCCESS );
            }
            else
            {
                // Nothing to do.
            }
        }
        
        // delete row
        IDE_TEST( sDataPlan->updateCursor->deleteRow()
                  != IDE_SUCCESS );

        /* PROJ-2464 hybrid partitioned table ���� */
        IDE_TEST( sDataPlan->insertCursorMgr.getSelectedPartitionTupleID( &sPartitionTupleID )
                  != IDE_SUCCESS );
        sSelectedPartitionTuple = &(aTemplate->tmplate.rows[sPartitionTupleID]);

        if ( ( sDataPlan->needTriggerRow == ID_TRUE ) ||
             ( sCodePlan->checkConstrList != NULL ) ||
             ( sCodePlan->returnInto != NULL ) )
        {
            // NEW ROW�� ȹ��
            IDE_TEST( sCursor->getLastModifiedRow(
                          & sNewRow,
                          sSelectedPartitionTuple->rowOffset )
                      != IDE_SUCCESS );

            IDE_TEST( sDataPlan->insertCursorMgr.setColumnsForNewRow()
                      != IDE_SUCCESS );

            IDE_TEST( sDataPlan->insertCursorMgr.getColumnsForNewRow(
                          &sColumnsForNewRow )
                      != IDE_SUCCESS );
        }
        else
        {
            // Nothing To Do
        }
    }
    else
    {
        /* PROJ-2464 hybrid partitioned table ���� */
        sSelectedPartitionTuple = &(aTemplate->tmplate.rows[sSelectedPartitionRef->table]);

        if ( sSelectedPartitionRef->table == sDataPlan->updateTupleID )
        {
            //-----------------------------------
            // tableRef�� �ְ� select�� partition�� ���
            //-----------------------------------

            if ( QCM_TABLE_TYPE_IS_DISK( sCodePlan->tableRef->tableInfo->tableFlag ) !=
                 QCM_TABLE_TYPE_IS_DISK( sDataPlan->updatePartInfo->tableFlag ) )
            {
                /* PROJ-2464 hybrid partitioned table ����
                 * Partitioned Table�� �������� ���� smiValue Array�� Table Partition�� �°� ��ȯ�Ѵ�.
                 */
                IDE_TEST( qmx::makeSmiValueWithSmiValue( sCodePlan->tableRef->tableInfo,
                                                         sDataPlan->updatePartInfo,
                                                         sCodePlan->columns,
                                                         sSmiValuesValueCount,
                                                         sSmiValues,
                                                         sValuesForPartition )
                          != IDE_SUCCESS );

                sSmiValuesForPartition = sValuesForPartition;
            }
            else
            {
                sSmiValuesForPartition = sSmiValues;
            }

            if ( ( sCodePlan->flag & QMNC_UPTE_VIEW_KEY_PRESERVED_MASK )
                 == QMNC_UPTE_VIEW_KEY_PRESERVED_TRUE )
            {
                // PROJ-2204 join update, delete
                // tuple ������ cursor�� �����ؾ��Ѵ�.
                if ( ( sCodePlan->flag & QMNC_UPTE_VIEW_MASK )
                     == QMNC_UPTE_VIEW_TRUE )
                {
                    IDE_TEST( sDataPlan->updateCursor->setRowPosition(
                                  sDataPlan->updateTuple->row,
                                  sDataPlan->updateTuple->rid )
                              != IDE_SUCCESS );
                }
                else
                {
                    // Nothing to do.
                }
            }
            
            IDE_TEST( sDataPlan->updateCursor->updateRow( sSmiValuesForPartition,
                                                          NULL,
                                                          & sRow,
                                                          & sDataPlan->rowGRID )
                      != IDE_SUCCESS );

            sPartOID = sSelectedPartitionRef->partitionOID;

            // update index table
            IDE_TEST( updateIndexTableCursorForRowMovement( aTemplate,
                                                            sCodePlan,
                                                            sDataPlan,
                                                            sPartOID,
                                                            sDataPlan->rowGRID,
                                                            sSmiValues )
                      != IDE_SUCCESS );

            /* PROJ-2464 hybrid partitioned table ����
             *  Disk Partition�� ���, Disk Type�� Lob Column�� �ʿ��ϴ�.
             *  Memory/Volatile Partition�� ���, �ش� Partition�� Lob Column�� �ʿ��ϴ�.
             */
            if ( ( QCM_TABLE_TYPE_IS_DISK( sCodePlan->tableRef->tableInfo->tableFlag ) !=
                   QCM_TABLE_TYPE_IS_DISK( sDataPlan->updatePartInfo->tableFlag ) ) ||
                 ( QCM_TABLE_TYPE_IS_DISK( sDataPlan->updatePartInfo->tableFlag ) != ID_TRUE ) )
            {
                IDE_TEST( qmx::changeLobColumnInfo( sDataPlan->lobInfo,
                                                    sDataPlan->updatePartInfo->columns )
                          != IDE_SUCCESS );
            }
            else
            {
                /* Nothing to do */
            }

            // Lob�÷� ó��
            IDE_TEST( qmx::copyAndOutBindLobInfo( aTemplate->stmt,
                                                  sDataPlan->lobInfo,
                                                  sDataPlan->updateCursor,
                                                  sRow,
                                                  sDataPlan->rowGRID )
                      != IDE_SUCCESS );

            if ( ( sDataPlan->needTriggerRow == ID_TRUE ) ||
                 ( sCodePlan->checkConstrList != NULL ) ||
                 ( sCodePlan->returnInto != NULL ) )
            {
                // NEW ROW�� ȹ��
                IDE_TEST( sDataPlan->updateCursor->getLastModifiedRow(
                              & sNewRow,
                              sSelectedPartitionTuple->rowOffset )
                          != IDE_SUCCESS );

                sColumnsForNewRow = sSelectedPartitionRef->partitionInfo->columns;
            }
            else
            {
                // Nothing to do.
            }
        }
        else
        {
            //-----------------------------------
            // tableRef�� �ִ� �ٸ� partition�� ���
            // insert row -> update row
            //-----------------------------------

            /* PROJ-2359 Table/Partition Access Option */
            IDE_TEST( qmx::checkAccessOption( sSelectedPartitionRef->partitionInfo,
                                              ID_TRUE, /* aIsInsertion */
                                              QCG_CHECK_SHARD_DML_CONSISTENCY( aTemplate->stmt ) )
                      != IDE_SUCCESS );

            /* PROJ-1090 Function-based Index */
            if ( sCodePlan->defaultExprColumns != NULL )
            {
                IDE_TEST( qmsDefaultExpr::setRowBufferFromSmiValueArray(
                              &(aTemplate->tmplate),
                              sCodePlan->defaultExprTableRef,
                              sCodePlan->tableRef->tableInfo->columns,
                              sDataPlan->defaultExprRowBuffer,
                              sDataPlan->insertValues,
                              QCM_TABLE_TYPE_IS_DISK( sCodePlan->tableRef->tableInfo->tableFlag ) )
                          != IDE_SUCCESS );

                IDE_TEST( qmsDefaultExpr::calculateDefaultExpression(
                              aTemplate,
                              sCodePlan->defaultExprTableRef,
                              NULL,
                              sCodePlan->defaultExprColumns,
                              sDataPlan->defaultExprRowBuffer,
                              sDataPlan->insertValues,
                              sCodePlan->tableRef->tableInfo->columns )
                          != IDE_SUCCESS );
            }
            else
            {
                /* Nothing to do */
            }

            // partition�� cursor�� ��´�.
            sCursorInfo = (qmnCursorInfo*)
                aTemplate->tmplate.rows[sSelectedPartitionRef->table].cursorInfo;

            IDE_TEST_RAISE( sCursorInfo == NULL, ERR_NOT_FOUND );

            // insert row
            sInsertCursor = sCursorInfo->cursor;

            if ( QCM_TABLE_TYPE_IS_DISK( sCodePlan->tableRef->tableInfo->tableFlag ) !=
                 QCM_TABLE_TYPE_IS_DISK( sSelectedPartitionRef->partitionInfo->tableFlag ) )
            {
                /* PROJ-2464 hybrid partitioned table ����
                 * Partitioned Table�� �������� ���� smiValue Array�� Table Partition�� �°� ��ȯ�Ѵ�.
                 */
                IDE_TEST( qmx::makeSmiValueWithSmiValue( sCodePlan->tableRef->tableInfo,
                                                         sSelectedPartitionRef->partitionInfo,
                                                         sCodePlan->tableRef->tableInfo->columns,
                                                         sCodePlan->tableRef->tableInfo->columnCount,
                                                         sDataPlan->insertValues,
                                                         sValuesForPartition )
                          != IDE_SUCCESS );

                sSmiValuesForPartition = sValuesForPartition;
            }
            else
            {
                sSmiValuesForPartition = sDataPlan->insertValues;
            }

            IDE_TEST( sInsertCursor->insertRow( sSmiValuesForPartition,
                                                & sRow,
                                                & sDataPlan->rowGRID )
                      != IDE_SUCCESS );

            /* PROJ-2334 PMT
             * ���õ� ��Ƽ���� �÷����� LOB Column���� ���� */
            /* PROJ-2464 hybrid partitioned table ����
             *  Disk Partition�� ���, Disk Type�� Lob Column�� �ʿ��ϴ�.
             *  Memory/Volatile Partition�� ���, �ش� Partition�� Lob Column�� �ʿ��ϴ�.
             */
            if ( ( QCM_TABLE_TYPE_IS_DISK( sCodePlan->tableRef->tableInfo->tableFlag ) !=
                   QCM_TABLE_TYPE_IS_DISK( sSelectedPartitionRef->partitionInfo->tableFlag ) ) ||
                 ( QCM_TABLE_TYPE_IS_DISK( sSelectedPartitionRef->partitionInfo->tableFlag ) != ID_TRUE ) )
            {
                IDE_TEST( qmx::changeLobColumnInfo( sDataPlan->insertLobInfo,
                                                    sSelectedPartitionRef->partitionInfo->columns )
                          != IDE_SUCCESS );
            }
            else
            {
                /* Nothing to do */
            }

            sPartOID = sSelectedPartitionRef->partitionOID;

            // update index table
            IDE_TEST( updateIndexTableCursorForRowMovement( aTemplate,
                                                            sCodePlan,
                                                            sDataPlan,
                                                            sPartOID,
                                                            sDataPlan->rowGRID,
                                                            sSmiValues )
                      != IDE_SUCCESS );

            IDE_TEST( qmx::copyAndOutBindLobInfo( aTemplate->stmt,
                                                  sDataPlan->insertLobInfo,
                                                  sInsertCursor,
                                                  sRow,
                                                  sDataPlan->rowGRID )
                      != IDE_SUCCESS );
            
            if ( ( sCodePlan->flag & QMNC_UPTE_VIEW_KEY_PRESERVED_MASK )
                 == QMNC_UPTE_VIEW_KEY_PRESERVED_TRUE )
            {
                // PROJ-2204 join update, delete
                // tuple ������ cursor�� �����ؾ��Ѵ�.
                if ( ( sCodePlan->flag & QMNC_UPTE_VIEW_MASK )
                     == QMNC_UPTE_VIEW_TRUE )
                {
                    IDE_TEST( sDataPlan->updateCursor->setRowPosition(
                                  sDataPlan->updateTuple->row,
                                  sDataPlan->updateTuple->rid )
                              != IDE_SUCCESS );
                }
                else
                {
                    // Nothing to do.
                }
            }
            
            // delete row
            IDE_TEST( sDataPlan->updateCursor->deleteRow()
                      != IDE_SUCCESS );

            if ( ( sDataPlan->needTriggerRow == ID_TRUE ) ||
                 ( sCodePlan->checkConstrList != NULL ) ||
                 ( sCodePlan->returnInto != NULL ) )
            {
                // NEW ROW�� ȹ��
                IDE_TEST( sInsertCursor->getModifiedRow(
                              & sNewRow,
                              sSelectedPartitionTuple->rowOffset,
                              sRow,
                              sDataPlan->rowGRID )
                          != IDE_SUCCESS );

                sColumnsForNewRow = sSelectedPartitionRef->partitionInfo->columns;
            }
            else
            {
                // Nothing to do.
            }
        }
    }

    /* PROJ-2464 hybrid partitioned table ���� */
    if ( ( sCodePlan->tableRef->partitionSummary->isHybridPartitionedTable == ID_TRUE ) ||
         ( sCodePlan->insertTableRef->partitionSummary->isHybridPartitionedTable == ID_TRUE ) )
    {
        qmx::copyMtcTupleForPartitionDML( &sCopyTuple, sDataPlan->updateTuple );
        sNeedToRecoverTuple = ID_TRUE;

        qmx::adjustMtcTupleForPartitionDML( sDataPlan->updateTuple, sSelectedPartitionTuple );
    }
    else
    {
        /* Nothing to do */
    }

    //-----------------------------------
    // check constraint
    //-----------------------------------

    /* PROJ-1107 Check Constraint ���� */
    if ( sCodePlan->checkConstrList != NULL )
    {
        sOrgRow = sDataPlan->updateTuple->row;
        sDataPlan->updateTuple->row = sNewRow;

        IDE_TEST( qdnCheck::verifyCheckConstraintList(
                      aTemplate,
                      sCodePlan->checkConstrList )
                  != IDE_SUCCESS );

        sDataPlan->updateTuple->row = sOrgRow;
    }
    else
    {
        // Nothing to do.
    }

    //-----------------------------------
    // update after trigger
    //-----------------------------------

    if ( sDataPlan->existTrigger == ID_TRUE )
    {
        // PROJ-1359 Trigger
        // ROW GRANULARITY TRIGGER�� ����
        IDE_TEST( qdnTrigger::fireTrigger( aTemplate->stmt,
                                           aTemplate->stmt->qmxMem,
                                           sCodePlan->tableRef->tableInfo,
                                           QCM_TRIGGER_ACTION_EACH_ROW,
                                           QCM_TRIGGER_AFTER,
                                           QCM_TRIGGER_EVENT_UPDATE,
                                           sCodePlan->updateColumnList,
                                           sDataPlan->updateCursor,         /* Table Cursor */
                                           sDataPlan->updateTuple->rid,     /* Row GRID */
                                           sDataPlan->oldRow,
                                           sDataPlan->updatePartInfo->columns,
                                           sNewRow,
                                           sColumnsForNewRow )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    //-----------------------------------
    // return into
    //-----------------------------------

    /* PROJ-1584 DML Return Clause */
    if ( sCodePlan->returnInto != NULL )
    {
        sOrgRow = sDataPlan->updateTuple->row;
        sDataPlan->updateTuple->row = sNewRow;

        IDE_TEST( qmx::copyReturnToInto( aTemplate,
                                         sCodePlan->returnInto,
                                         aTemplate->numRows )
                  != IDE_SUCCESS );

        sDataPlan->updateTuple->row = sOrgRow;
    }
    else
    {
        // nothing do do
    }

    if ( ( *sDataPlan->flag & QMND_UPTE_UPDATE_MASK )
         == QMND_UPTE_UPDATE_FALSE )
    {
        *sDataPlan->flag &= ~QMND_UPTE_UPDATE_MASK;
        *sDataPlan->flag |= QMND_UPTE_UPDATE_TRUE;
    }
    else
    {
        // Nothing to do.
    }

    /* PROJ-2464 hybrid partitioned table ���� */
    if ( sNeedToRecoverTuple == ID_TRUE )
    {
        sNeedToRecoverTuple = ID_FALSE;
        qmx::copyMtcTupleForPartitionDML( sDataPlan->updateTuple, &sCopyTuple );
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NOT_FOUND )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmnUPTE::updateOneRowForRowmovement",
                                  "cursor not found" ));
    }
    IDE_EXCEPTION_END;

    /* PROJ-2464 hybrid partitioned table ���� */
    if ( sNeedToRecoverTuple == ID_TRUE )
    {
        qmx::copyMtcTupleForPartitionDML( sDataPlan->updateTuple, &sCopyTuple );
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnUPTE::updateOneRowForCheckRowmovement( qcTemplate * aTemplate,
                                          qmnPlan    * aPlan )
{
/***********************************************************************
 *
 * Description :
 *    UPTE �� ���� ����� �����Ѵ�.
 *
 * Implementation :
 *    - update one record ����
 *    - trigger each row ����
 *
 ***********************************************************************/

#define IDE_FN "qmnUPTE::updateOneRowForCheckRowmovement"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncUPTE * sCodePlan = (qmncUPTE*) aPlan;
    qmndUPTE * sDataPlan =
        (qmndUPTE*) (aTemplate->tmplate.data + aPlan->offset);

    void              * sOrgRow;
    qmsPartitionRef   * sSelectedPartitionRef;
    smiValue          * sSmiValues;
    smiValue          * sSmiValuesForPartition = NULL;
    smiValue            sValuesForPartition[QC_MAX_COLUMN_COUNT];
    UInt                sSmiValuesValueCount   = 0;
    void              * sNewRow                = NULL;
    void              * sRow;

    /* PROJ-2464 hybrid partitioned table ����
     * Memory�� ���, newRow�� ���ο� �ּҸ� �Ҵ��Ѵ�. ����, newRow�� ���� �������� �ʴ´�.
     */
    sNewRow = sDataPlan->newRow;

    if ( ( sCodePlan->flag & QMNC_UPTE_VIEW_KEY_PRESERVED_MASK )
         == QMNC_UPTE_VIEW_KEY_PRESERVED_FALSE )
    {
        /* BUG-39399 remove search key preserved table */
        if ( ( sCodePlan->flag & QMNC_UPTE_VIEW_MASK )
             == QMNC_UPTE_VIEW_TRUE )
        {
            IDE_TEST( checkDuplicateUpdate( sCodePlan,
                                            sDataPlan )
                      != IDE_SUCCESS );
        }
        else
        {
            /* Nothing To Do */
        }
    }
    
    //-----------------------------------
    // clear lob
    //-----------------------------------

    // PROJ-1362
    if ( sDataPlan->lobInfo != NULL )
    {
        (void) qmx::clearLobInfo( sDataPlan->lobInfo );
    }
    else
    {
        // Nothing to do.
    }

    //-----------------------------------
    // copy old row
    //-----------------------------------

    if ( sDataPlan->needTriggerRow == ID_TRUE )
    {
        // OLD ROW REFERENCING�� ���� ����
        idlOS::memcpy( sDataPlan->oldRow,
                       sDataPlan->updateTuple->row,
                       sDataPlan->updateTuple->rowOffset );
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

    //-----------------------------------
    // make update smiValues
    //-----------------------------------

    sSmiValues = aTemplate->insOrUptRow[ sCodePlan->valueIdx ];
    sSmiValuesValueCount = aTemplate->insOrUptRowValueCount[sCodePlan->valueIdx];

    // subquery�� ������ smi value ���� ����
    IDE_TEST( qmx::makeSmiValueForSubquery( aTemplate,
                                            sCodePlan->tableRef->tableInfo,
                                            sCodePlan->columns,
                                            sCodePlan->subqueries,
                                            sCodePlan->canonizedTuple,
                                            sSmiValues,
                                            sCodePlan->isNull,
                                            sDataPlan->lobInfo )
              != IDE_SUCCESS );

    // ������ smi value ���� ����
    IDE_TEST( qmx::makeSmiValueForUpdate( aTemplate,
                                          sCodePlan->tableRef->tableInfo,
                                          sCodePlan->columns,
                                          sCodePlan->values,
                                          sCodePlan->canonizedTuple,
                                          sSmiValues,
                                          sCodePlan->isNull,
                                          sDataPlan->lobInfo )
              != IDE_SUCCESS );

    //-----------------------------------
    // Default Expr
    //-----------------------------------

    if ( sCodePlan->defaultExprColumns != NULL )
    {
        qmsDefaultExpr::setRowBufferFromBaseColumn(
            &(aTemplate->tmplate),
            sCodePlan->tableRef->table,
            sCodePlan->defaultExprTableRef->table,
            sCodePlan->defaultExprBaseColumns,
            sDataPlan->defaultExprRowBuffer );

        IDE_TEST( qmsDefaultExpr::setRowBufferFromSmiValueArray(
                      &(aTemplate->tmplate),
                      sCodePlan->defaultExprTableRef,
                      sCodePlan->columns,
                      sDataPlan->defaultExprRowBuffer,
                      sSmiValues,
                      QCM_TABLE_TYPE_IS_DISK( sCodePlan->tableRef->tableInfo->tableFlag ) )
                  != IDE_SUCCESS );

        IDE_TEST( qmsDefaultExpr::calculateDefaultExpression(
                      aTemplate,
                      sCodePlan->defaultExprTableRef,
                      sCodePlan->columns,
                      sCodePlan->defaultExprColumns,
                      sDataPlan->defaultExprRowBuffer,
                      sSmiValues,
                      sCodePlan->tableRef->tableInfo->columns )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    // PROJ-2219 Row-level before update trigger
    if ( sDataPlan->existTrigger == ID_TRUE )
    {
        // ���� trigger���� lob column�� �ִ� table�� �������� �����Ƿ�
        // Table cursor�� NULL�� �ѱ��.
        IDE_TEST( qdnTrigger::fireTrigger(
                      aTemplate->stmt,
                      aTemplate->stmt->qmxMem,
                      sCodePlan->tableRef->tableInfo,
                      QCM_TRIGGER_ACTION_EACH_ROW,
                      QCM_TRIGGER_BEFORE,
                      QCM_TRIGGER_EVENT_UPDATE,
                      sCodePlan->updateColumnList,        // UPDATE Column
                      NULL,                               // Table Cursor
                      SC_NULL_GRID,                       // Row GRID
                      sDataPlan->oldRow,                  // OLD ROW
                      sDataPlan->updatePartInfo->columns, // OLD ROW Column
                      sSmiValues,                         // NEW ROW(value list)
                      sCodePlan->columns )                // NEW ROW Column
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    IDE_TEST( qmx::makeSmiValueForChkRowMovement(
                  sCodePlan->updateColumnList,
                  sSmiValues,
                  sCodePlan->tableRef->tableInfo->partKeyColumns,
                  sDataPlan->updateTuple,
                  sDataPlan->checkValues )
              != IDE_SUCCESS );

    IDE_TEST_RAISE( qmoPartition::partitionFilteringWithRow(
                        sCodePlan->tableRef,
                        sDataPlan->checkValues,
                        &sSelectedPartitionRef )
                    != IDE_SUCCESS,
                    ERR_NO_ROW_MOVEMENT );

    IDE_TEST_RAISE( sSelectedPartitionRef->table != sDataPlan->updateTupleID,
                    ERR_NO_ROW_MOVEMENT );

    //-----------------------------------
    // update one row
    //-----------------------------------

    if ( QCM_TABLE_TYPE_IS_DISK( sCodePlan->tableRef->tableInfo->tableFlag ) !=
         QCM_TABLE_TYPE_IS_DISK( sDataPlan->updatePartInfo->tableFlag ) )
    {
        /* PROJ-2464 hybrid partitioned table ����
         * Partitioned Table�� �������� ���� smiValue Array�� Table Partition�� �°� ��ȯ�Ѵ�.
         */
        IDE_TEST( qmx::makeSmiValueWithSmiValue( sCodePlan->tableRef->tableInfo,
                                                 sDataPlan->updatePartInfo,
                                                 sCodePlan->columns,
                                                 sSmiValuesValueCount,
                                                 sSmiValues,
                                                 sValuesForPartition )
                  != IDE_SUCCESS );

        sSmiValuesForPartition = sValuesForPartition;
    }
    else
    {
        sSmiValuesForPartition = sSmiValues;
    }

    if ( ( sCodePlan->flag & QMNC_UPTE_VIEW_KEY_PRESERVED_MASK )
         == QMNC_UPTE_VIEW_KEY_PRESERVED_TRUE )
    {
        // PROJ-2204 join update, delete
        // tuple ������ cursor�� �����ؾ��Ѵ�.
        if ( ( sCodePlan->flag & QMNC_UPTE_VIEW_MASK )
             == QMNC_UPTE_VIEW_TRUE )
        {
            IDE_TEST( sDataPlan->updateCursor->setRowPosition(
                          sDataPlan->updateTuple->row,
                          sDataPlan->updateTuple->rid )
                      != IDE_SUCCESS );
        }
        else
        {
            // Nothing to do.
        }
    }
    
    IDE_TEST( sDataPlan->updateCursor->updateRow( sSmiValuesForPartition,
                                                  NULL,
                                                  & sRow,
                                                  & sDataPlan->rowGRID )
              != IDE_SUCCESS );

    // update index table
    IDE_TEST( updateIndexTableCursor( aTemplate,
                                      sCodePlan,
                                      sDataPlan,
                                      sSmiValues )
              != IDE_SUCCESS );

    /* PROJ-2464 hybrid partitioned table ����
     *  Disk Partition�� ���, Disk Type�� Lob Column�� �ʿ��ϴ�.
     *  Memory/Volatile Partition�� ���, �ش� Partition�� Lob Column�� �ʿ��ϴ�.
     */
    if ( ( QCM_TABLE_TYPE_IS_DISK( sCodePlan->tableRef->tableInfo->tableFlag ) !=
           QCM_TABLE_TYPE_IS_DISK( sDataPlan->updatePartInfo->tableFlag ) ) ||
         ( QCM_TABLE_TYPE_IS_DISK( sDataPlan->updatePartInfo->tableFlag ) != ID_TRUE ) )
    {
        IDE_TEST( qmx::changeLobColumnInfo( sDataPlan->lobInfo,
                                            sDataPlan->updatePartInfo->columns )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    // Lob�÷� ó��
    IDE_TEST( qmx::copyAndOutBindLobInfo( aTemplate->stmt,
                                          sDataPlan->lobInfo,
                                          sDataPlan->updateCursor,
                                          sRow,
                                          sDataPlan->rowGRID )
              != IDE_SUCCESS );

    if ( ( sDataPlan->needTriggerRow == ID_TRUE ) ||
         ( sCodePlan->checkConstrList != NULL ) ||
         ( sCodePlan->returnInto != NULL ) )
    {
        // NEW ROW�� ȹ��
        IDE_TEST( sDataPlan->updateCursor->getLastModifiedRow(
                      & sNewRow,
                      sDataPlan->updateTuple->rowOffset )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    //-----------------------------------
    // check constraint
    //-----------------------------------

    /* PROJ-1107 Check Constraint ���� */
    if ( sCodePlan->checkConstrList != NULL )
    {
        sOrgRow = sDataPlan->updateTuple->row;
        sDataPlan->updateTuple->row = sNewRow;

        IDE_TEST( qdnCheck::verifyCheckConstraintList(
                      aTemplate,
                      sCodePlan->checkConstrList )
                  != IDE_SUCCESS );

        sDataPlan->updateTuple->row = sOrgRow;
    }
    else
    {
        // Nothing to do.
    }

    //-----------------------------------
    // update after trigger
    //-----------------------------------

    if ( sDataPlan->existTrigger == ID_TRUE )
    {
        // PROJ-1359 Trigger
        // ROW GRANULARITY TRIGGER�� ����
        IDE_TEST( qdnTrigger::fireTrigger( aTemplate->stmt,
                                           aTemplate->stmt->qmxMem,
                                           sCodePlan->tableRef->tableInfo,
                                           QCM_TRIGGER_ACTION_EACH_ROW,
                                           QCM_TRIGGER_AFTER,
                                           QCM_TRIGGER_EVENT_UPDATE,
                                           sCodePlan->updateColumnList,
                                           sDataPlan->updateCursor,         /* Table Cursor */
                                           sDataPlan->updateTuple->rid,     /* Row GRID */
                                           sDataPlan->oldRow,
                                           sDataPlan->updatePartInfo->columns,
                                           sNewRow,
                                           sDataPlan->updatePartInfo->columns )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    //-----------------------------------
    // return into
    //-----------------------------------

    /* PROJ-1584 DML Return Clause */
    if ( sCodePlan->returnInto != NULL )
    {
        sOrgRow = sDataPlan->updateTuple->row;
        sDataPlan->updateTuple->row = sNewRow;

        IDE_TEST( qmx::copyReturnToInto( aTemplate,
                                         sCodePlan->returnInto,
                                         aTemplate->numRows )
                  != IDE_SUCCESS );

        sDataPlan->updateTuple->row = sOrgRow;
    }
    else
    {
        // nothing do do
    }

    if ( ( *sDataPlan->flag & QMND_UPTE_UPDATE_MASK )
         == QMND_UPTE_UPDATE_FALSE )
    {
        *sDataPlan->flag &= ~QMND_UPTE_UPDATE_MASK;
        *sDataPlan->flag |= QMND_UPTE_UPDATE_TRUE;
    }
    else
    {
        // Nothing to do.
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NO_ROW_MOVEMENT )
    {
        IDE_SET( ideSetErrorCode(qpERR_ABORT_QMV_INVALID_PARTITION_KEY_INSERT) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnUPTE::fireInsteadOfTrigger( qcTemplate * aTemplate,
                               qmnPlan    * aPlan )
{
/***********************************************************************
 *
 * Description :
 *    UPTE �� ���� ����� �����Ѵ�.
 *
 * Implementation :
 *    - trigger each row ����
 *
 ***********************************************************************/

#define IDE_FN "qmnUPTE::fireInsteadOfTrigger"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncUPTE * sCodePlan = (qmncUPTE*) aPlan;
    qmndUPTE * sDataPlan =
        (qmndUPTE*) (aTemplate->tmplate.data + aPlan->offset);

    qcmTableInfo * sTableInfo = NULL;
    qcmColumn    * sQcmColumn = NULL;
    mtcColumn    * sColumn    = NULL;
    smiValue     * sSmiValues = NULL;
    mtcStack     * sStack     = NULL;
    SInt           sRemain    = 0;
    void         * sOrgRow    = NULL;
    UShort         i          = 0;

    sTableInfo = sCodePlan->tableRef->tableInfo;

    if ( ( sDataPlan->needTriggerRow == ID_TRUE ) ||
         ( sCodePlan->returnInto != NULL ) )
    {
        //-----------------------------------
        // get Old Row
        //-----------------------------------

        sStack = aTemplate->tmplate.stack;
        sRemain = aTemplate->tmplate.stackRemain;

        IDE_TEST_RAISE( sRemain < sDataPlan->updateTuple->columnCount,
                        ERR_STACK_OVERFLOW );

        // UPDATE�� VIEW ���̿� FILT ���� �ٸ� ���鿡 ���� stack�� ����Ǿ��� �� �����Ƿ�
        // stack�� view tuple�� �÷����� �缳���Ѵ�.
        for ( i = 0, sColumn = sDataPlan->updateTuple->columns;
              i < sDataPlan->updateTuple->columnCount;
              i++, sColumn++, sStack++ )
        {
            sStack->column = sColumn;
            sStack->value  =
                (void*)((SChar*)sDataPlan->updateTuple->row + sColumn->column.offset);
        }

        /* PROJ-2464 hybrid partitioned table ���� */
        if ( sTableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
        {
            if ( sDataPlan->updatePartInfo != NULL )
            {
                if ( sDataPlan->updateTuple->tableHandle != sDataPlan->updatePartInfo->tableHandle )
                {
                    IDE_TEST( smiGetTableTempInfo( sDataPlan->updateTuple->tableHandle,
                                                   (void **)&(sDataPlan->updatePartInfo) )
                              != IDE_SUCCESS );
                }
                else
                {
                    /* Nothing to do */
                }
            }
            else
            {
                IDE_TEST( smiGetTableTempInfo( sDataPlan->updateTuple->tableHandle,
                                               (void **)&(sDataPlan->updatePartInfo) )
                          != IDE_SUCCESS );
            }

            // ��� Partition�� ��Ȯ�ϹǷ�, �۾� ������ Partition���� �����Ѵ�.
            sTableInfo = sDataPlan->updatePartInfo;
            sDataPlan->columnsForRow = sDataPlan->updatePartInfo->columns;
        }
        else
        {
            /* Nothing to do */
        }

        IDE_TEST( qmx::makeSmiValueWithStack( sDataPlan->columnsForRow,
                                              aTemplate,
                                              aTemplate->tmplate.stack,
                                              sTableInfo,
                                              (smiValue*) sDataPlan->oldRow,
                                              NULL )
                  != IDE_SUCCESS );

        //-----------------------------------
        // get New Row
        //-----------------------------------

        // Sequence Value ȹ��
        if ( sCodePlan->nextValSeqs != NULL )
        {
            IDE_TEST( qmx::readSequenceNextVals(
                          aTemplate->stmt,
                          sCodePlan->nextValSeqs )
                      != IDE_SUCCESS );
        }

        sSmiValues = aTemplate->insOrUptRow[ sCodePlan->valueIdx ];

        // subquery�� ������ smi value ���� ����
        IDE_TEST( qmx::makeSmiValueForSubquery( aTemplate,
                                                sTableInfo,
                                                sCodePlan->columns,
                                                sCodePlan->subqueries,
                                                sCodePlan->canonizedTuple,
                                                sSmiValues,
                                                sCodePlan->isNull,
                                                sDataPlan->lobInfo )
                  != IDE_SUCCESS );

        // ������ smi value ���� ����
        IDE_TEST( qmx::makeSmiValueForUpdate( aTemplate,
                                              sTableInfo,
                                              sCodePlan->columns,
                                              sCodePlan->values,
                                              sCodePlan->canonizedTuple,
                                              sSmiValues,
                                              sCodePlan->isNull,
                                              sDataPlan->lobInfo )
                  != IDE_SUCCESS );

        // old smiValues�� new smiValues�� ����
        idlOS::memcpy( sDataPlan->newRow,
                       sDataPlan->oldRow,
                       ID_SIZEOF(smiValue) * sDataPlan->updateTuple->columnCount );

        // update smiValues�� ������ ��ġ�� ����
        for ( sQcmColumn = sCodePlan->columns;
              sQcmColumn != NULL;
              sQcmColumn = sQcmColumn->next, sSmiValues++ )
        {
            for ( i = 0; i < sTableInfo->columnCount; i++ )
            {
                if ( sQcmColumn->basicInfo->column.id ==
                     sDataPlan->columnsForRow[i].basicInfo->column.id )
                {
                    *((smiValue*)sDataPlan->newRow + i) = *sSmiValues;
                    break;
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

    if ( sDataPlan->existTrigger == ID_TRUE )
    {
        // PROJ-1359 Trigger
        // ROW GRANULARITY TRIGGER�� ����
        IDE_TEST( qdnTrigger::fireTrigger( aTemplate->stmt,
                                           aTemplate->stmt->qmxMem,
                                           sCodePlan->tableRef->tableInfo,
                                           QCM_TRIGGER_ACTION_EACH_ROW,
                                           QCM_TRIGGER_INSTEAD_OF,
                                           QCM_TRIGGER_EVENT_UPDATE,
                                           NULL,               // UPDATE Column
                                           NULL,               /* Table Cursor */
                                           SC_NULL_GRID,       /* Row GRID */
                                           sDataPlan->oldRow,
                                           sDataPlan->columnsForRow,
                                           sDataPlan->newRow,
                                           sDataPlan->columnsForRow )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    /* PROJ-1584 DML Return Clause */
    if ( sCodePlan->returnInto != NULL )
    {
        IDE_TEST( qmx::makeRowWithSmiValue( sDataPlan->updateTuple->columns,
                                            sDataPlan->updateTuple->columnCount,
                                            (smiValue*) sDataPlan->newRow,
                                            sDataPlan->returnRow )
                  != IDE_SUCCESS );

        sOrgRow = sDataPlan->updateTuple->row;
        sDataPlan->updateTuple->row = sDataPlan->returnRow;

        IDE_TEST( qmx::copyReturnToInto( aTemplate,
                                         sCodePlan->returnInto,
                                         aTemplate->numRows )
                  != IDE_SUCCESS );

        sDataPlan->updateTuple->row = sOrgRow;
    }
    else
    {
        // nothing do do
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_STACK_OVERFLOW );
    {
        IDE_SET(ideSetErrorCode(mtERR_ABORT_STACK_OVERFLOW));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnUPTE::checkUpdateParentRef( qcTemplate * aTemplate,
                               qmnPlan    * aPlan )
{
/***********************************************************************
 *
 * Description :
 *
 * Implementation :
 *     Foreign Key Referencing�� ����
 *     Master Table�� �����ϴ� �� �˻�
 *
 *     To Fix PR-10592
 *     Cursor�� �ùٸ� ����� ���ؼ��� Master�� ���� �˻縦 ������ �Ŀ�
 *     Child Table�� ���� �˻縦 �����Ͽ��� �Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "qmnUPTE::checkUpdateParentRef"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncUPTE            * sCodePlan;
    qmndUPTE            * sDataPlan;
    iduMemoryStatus       sQmxMemStatus;
    void                * sOrgRow;
    void                * sSearchRow;
    qmsPartitionRef     * sPartitionRef;
    qmcInsertPartCursor * sInsertPartCursor;
    UInt                  i;

    sCodePlan = (qmncUPTE*) aPlan;
    sDataPlan = (qmndUPTE*) ( aTemplate->tmplate.data + aPlan->offset );

    //------------------------------------------
    // UPDATE�� �ο� �˻��� ����,
    // ���ſ����� ����� ù��° row ���� ��ġ�� cursor ��ġ ����
    //------------------------------------------

    if ( ( sCodePlan->parentConstraints != NULL ) &&
         ( sDataPlan->updateCursor != NULL ) )
    {
        if ( sCodePlan->tableRef->partitionRef == NULL )
        {
            IDE_TEST( checkUpdateParentRefOnScan( aTemplate,
                                                  sCodePlan,
                                                  sDataPlan->updateTuple )
                      != IDE_SUCCESS );
        }
        else
        {
            for ( sPartitionRef = sCodePlan->tableRef->partitionRef;
                  sPartitionRef != NULL;
                  sPartitionRef = sPartitionRef->next )
            {
                IDE_TEST( checkUpdateParentRefOnScan(
                              aTemplate,
                              sCodePlan,
                              & aTemplate->tmplate.rows[sPartitionRef->table] )
                      != IDE_SUCCESS );
            }
        }
    }
    else
    {
        // Nothing to do.
    }

    //------------------------------------------
    // INSERT�� �ο� �˻��� ����,
    // ���ſ����� ����� ù��° row ���� ��ġ�� cursor ��ġ ����
    //------------------------------------------

    if ( ( sCodePlan->parentConstraints != NULL ) &&
         ( sCodePlan->updateType == QMO_UPDATE_ROWMOVEMENT ) )
    {
        for ( i = 0; i < sDataPlan->insertCursorMgr.mCursorIndexCount; i++ )
        {
            sInsertPartCursor = sDataPlan->insertCursorMgr.mCursorIndex[i];

            sOrgRow = sSearchRow = sDataPlan->updateTuple->row;

            //------------------------------------------
            // ���� ��ü�� ���� ���� Ȯ�� �� �÷� ���� ����
            //------------------------------------------

            IDE_TEST( sInsertPartCursor->cursor.beforeFirstModified( SMI_FIND_MODIFIED_NEW )
                      != IDE_SUCCESS );
            
            //------------------------------------------
            // ����� Row�� �ݺ������� �о� Referecing �˻縦 ��
            //------------------------------------------
            
            IDE_TEST( sInsertPartCursor->cursor.readNewRow( (const void **) & sSearchRow,
                                                            & sDataPlan->updateTuple->rid )
                      != IDE_SUCCESS);

            sDataPlan->updateTuple->row = ( sSearchRow == NULL ) ? sOrgRow : sSearchRow;

            while ( sSearchRow != NULL )
            {
                // Memory ������ ���Ͽ� ���� ��ġ ���
                IDE_TEST_RAISE( aTemplate->stmt->qmxMem->getStatus(&sQmxMemStatus) != IDE_SUCCESS, ERR_MEM_OP );

                //------------------------------------------
                // Master Table�� ���� Referencing �˻�
                //------------------------------------------

                IDE_TEST( qdnForeignKey::checkParentRef(
                              aTemplate->stmt,
                              sCodePlan->updateColumnIDs,
                              sCodePlan->parentConstraints,
                              sDataPlan->updateTuple,
                              sDataPlan->updateTuple->row,
                              sCodePlan->updateColumnCount )
                          != IDE_SUCCESS );

                // Memory ������ ���� Memory �̵�
                IDE_TEST_RAISE( aTemplate->stmt->qmxMem->setStatus(&sQmxMemStatus) != IDE_SUCCESS, ERR_MEM_OP );

                sOrgRow = sSearchRow = sDataPlan->updateTuple->row;

                IDE_TEST( sInsertPartCursor->cursor.readNewRow(
                              (const void **) &sSearchRow,
                              & sDataPlan->updateTuple->rid )
                          != IDE_SUCCESS);

                sDataPlan->updateTuple->row =
                    ( sSearchRow == NULL ) ? sOrgRow : sSearchRow;
            }
        }
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
                     " qmnUPTE::checkUpdateParentRef"
                     " memory error" );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnUPTE::checkUpdateParentRefOnScan( qcTemplate   * aTemplate,
                                     qmncUPTE     * aCodePlan,
                                     mtcTuple     * aUpdateTuple )
{
/***********************************************************************
 *
 * Description :
 *
 * Implementation :
 *     Foreign Key Referencing�� ����
 *     Master Table�� �����ϴ� �� �˻�
 *
 *     To Fix PR-10592
 *     Cursor�� �ùٸ� ����� ���ؼ��� Master�� ���� �˻縦 ������ �Ŀ�
 *     Child Table�� ���� �˻縦 �����Ͽ��� �Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "qmnUPTE::checkUpdateParentRefOnScan"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    iduMemoryStatus   sQmxMemStatus;
    void            * sOrgRow;
    void            * sSearchRow;
    smiTableCursor  * sUpdateCursor;
    qmnCursorInfo   * sCursorInfo;

    //------------------------------------------
    // UPDATE�� �ο� �˻��� ����,
    // ���ſ����� ����� ù��° row ���� ��ġ�� cursor ��ġ ����
    //------------------------------------------

    sCursorInfo = (qmnCursorInfo*) aUpdateTuple->cursorInfo;

    IDE_TEST_RAISE( sCursorInfo == NULL, ERR_NOT_FOUND );

    sUpdateCursor = sCursorInfo->cursor;

    // BUG-37147 sUpdateCursor �� null �� ��찡 �߻���
    // PROJ-1624 non-partitioned index
    // index table scan���� open���� ���� partition�� �����Ѵ�.
    if ( sUpdateCursor != NULL )
    {
        sOrgRow = sSearchRow = aUpdateTuple->row;

        IDE_TEST( sUpdateCursor->beforeFirstModified( SMI_FIND_MODIFIED_NEW )
                  != IDE_SUCCESS );
        
        IDE_TEST( sUpdateCursor->readNewRow( (const void**) & sSearchRow,
                                             & aUpdateTuple->rid )
                  != IDE_SUCCESS );

        //------------------------------------------
        // Referencing �˻縦 ���� ������ Row���� �˻�
        //------------------------------------------

        aUpdateTuple->row = ( sSearchRow == NULL ) ? sOrgRow : sSearchRow;

        while( sSearchRow != NULL )
        {
            // Memory ������ ���Ͽ� ���� ��ġ ���
            IDE_TEST_RAISE( aTemplate->stmt->qmxMem->getStatus(&sQmxMemStatus) != IDE_SUCCESS, ERR_MEM_OP );

            //------------------------------------------
            // Child Table�� ���� Referencing �˻�
            //------------------------------------------

            IDE_TEST( qdnForeignKey::checkParentRef(
                        aTemplate->stmt,
                        aCodePlan->updateColumnIDs,
                        aCodePlan->parentConstraints,
                        aUpdateTuple,
                        aUpdateTuple->row,
                        aCodePlan->updateColumnCount )
                    != IDE_SUCCESS );

            // Memory ������ ���� Memory �̵�
            IDE_TEST_RAISE( aTemplate->stmt->qmxMem->setStatus(&sQmxMemStatus) != IDE_SUCCESS, ERR_MEM_OP );

            sOrgRow = sSearchRow = aUpdateTuple->row;

            IDE_TEST( sUpdateCursor->readNewRow( (const void**) & sSearchRow,
                                                 & aUpdateTuple->rid )
                      != IDE_SUCCESS );

            aUpdateTuple->row =
                (sSearchRow == NULL) ? sOrgRow : sSearchRow;
        }
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
                     " qmnUPTE::checkUpdateParentRefOnScan"
                     " memory error" );
    }
    IDE_EXCEPTION( ERR_NOT_FOUND )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmnUPTE::checkUpdateParentRefOnScan",
                                  "cursor not found" ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnUPTE::checkUpdateChildRef( qcTemplate * aTemplate,
                              qmnPlan    * aPlan )
{
/***********************************************************************
 *
 * Description :
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnUPTE::checkUpdateChildRef"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncUPTE        * sCodePlan;
    qmndUPTE        * sDataPlan;
    qmsPartitionRef * sPartitionRef;
    smiStatement      sSmiStmt;
    smiStatement    * sSmiStmtOrg;
    UInt              sStage = 0;

    sCodePlan = (qmncUPTE*) aPlan;
    sDataPlan = (qmndUPTE*) ( aTemplate->tmplate.data + aPlan->offset );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aTemplate != NULL );

    //------------------------------------------
    // child constraint �˻�
    //------------------------------------------

    if ( sCodePlan->childConstraints != NULL )
    {
        // BUG-17940 parent key�� �����ϰ� child key�� ã����
        // parent row�� lock�� ���� ���� view�� ��������
        // ���ο� smiStmt�� �̿��Ѵ�.
        // Update cascade �ɼǿ� ����ؼ� normal�� �Ѵ�.
        // child table�� Ÿ���� ���� �� �� ���� ������ ALL CURSOR�� �Ѵ�.
        qcg::getSmiStmt( aTemplate->stmt, & sSmiStmtOrg );

        IDE_TEST( sSmiStmt.begin( aTemplate->stmt->mStatistics,
                                  QC_SMI_STMT( aTemplate->stmt ),
                                  SMI_STATEMENT_NORMAL |
                                  SMI_STATEMENT_SELF_TRUE |
                                  SMI_STATEMENT_ALL_CURSOR )
                  != IDE_SUCCESS );
        qcg::setSmiStmt( aTemplate->stmt, & sSmiStmt );

        sStage = 1;

        if ( sDataPlan->updateCursor != NULL )
        {
            if ( sCodePlan->tableRef->partitionRef == NULL )
            {
                IDE_TEST( checkUpdateChildRefOnScan( aTemplate,
                                                     sCodePlan,
                                                     sCodePlan->tableRef->tableInfo,
                                                     sDataPlan->updateTuple )
                          != IDE_SUCCESS );
            }
            else
            {
                for ( sPartitionRef = sCodePlan->tableRef->partitionRef;
                      sPartitionRef != NULL;
                      sPartitionRef = sPartitionRef->next )
                {
                    IDE_TEST( checkUpdateChildRefOnScan(
                                  aTemplate,
                                  sCodePlan,
                                  sPartitionRef->partitionInfo,
                                  & aTemplate->tmplate.rows[sPartitionRef->table] )
                              != IDE_SUCCESS );
                }
            }
        }
        else
        {
            // Nothing to do.
        }

        sStage = 0;

        qcg::setSmiStmt( aTemplate->stmt, sSmiStmtOrg );
        IDE_TEST( sSmiStmt.end(SMI_STATEMENT_RESULT_SUCCESS) != IDE_SUCCESS);
    }
    else
    {
        // Nothing to do.
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sStage == 1 )
    {
        qcg::setSmiStmt( aTemplate->stmt, sSmiStmtOrg );

        if (sSmiStmt.end(SMI_STATEMENT_RESULT_FAILURE) != IDE_SUCCESS)
        {
            IDE_CALLBACK_FATAL("Check Child Key On Update smiStmt.end() failed");
        }
    }

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnUPTE::checkUpdateChildRefOnScan( qcTemplate     * aTemplate,
                                    qmncUPTE       * aCodePlan,
                                    qcmTableInfo   * aTableInfo,
                                    mtcTuple       * aUpdateTuple )
{
/***********************************************************************
 *
 * Description :
 *    UPDATE ���� ���� �� Child Table�� ���� Referencing ���� ������ �˻�
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnUPTE::checkUpdateChildRefOnScan"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    iduMemoryStatus   sQmxMemStatus;
    void            * sOrgRow;
    void            * sSearchRow;
    smiTableCursor  * sUpdateCursor;
    qmnCursorInfo   * sCursorInfo;

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aTemplate != NULL );
    IDE_DASSERT( aCodePlan->childConstraints != NULL );

    //------------------------------------------
    // UPDATE�� �ο� �˻��� ����,
    // ���ſ����� ����� ù��° row ���� ��ġ�� cursor ��ġ ����
    //------------------------------------------

    sCursorInfo = (qmnCursorInfo*) aUpdateTuple->cursorInfo;

    IDE_TEST_RAISE( sCursorInfo == NULL, ERR_NOT_FOUND );

    sUpdateCursor = sCursorInfo->cursor;

    // PROJ-1624 non-partitioned index
    // index table scan���� open���� ���� partition�� �����Ѵ�.
    if ( sUpdateCursor != NULL )
    {
        IDE_TEST( sUpdateCursor->beforeFirstModified( SMI_FIND_MODIFIED_OLD )
                  != IDE_SUCCESS );

        //------------------------------------------
        // Referencing �˻縦 ���� ������ Row���� �˻�
        //------------------------------------------

        sOrgRow = sSearchRow = aUpdateTuple->row;
        IDE_TEST(
            sUpdateCursor->readOldRow( (const void**) & sSearchRow,
                                       & aUpdateTuple->rid )
            != IDE_SUCCESS );

        aUpdateTuple->row = ( sSearchRow == NULL ) ? sOrgRow : sSearchRow;

        while( sSearchRow != NULL )
        {
            // Memory ������ ���Ͽ� ���� ��ġ ���
            IDE_TEST_RAISE( aTemplate->stmt->qmxMem->getStatus(&sQmxMemStatus) != IDE_SUCCESS, ERR_MEM_OP );

            //------------------------------------------
            // Child Table�� ���� Referencing �˻�
            //------------------------------------------

            IDE_TEST( qdnForeignKey::checkChildRefOnUpdate(
                          aTemplate->stmt,
                          aCodePlan->tableRef,
                          aTableInfo,
                          aCodePlan->updateColumnIDs,
                          aCodePlan->childConstraints,
                          aTableInfo->tableID,
                          aUpdateTuple,
                          aUpdateTuple->row,
                          aCodePlan->updateColumnCount )
                      != IDE_SUCCESS );

            // Memory ������ ���� Memory �̵�
            IDE_TEST_RAISE( aTemplate->stmt->qmxMem->setStatus(&sQmxMemStatus) != IDE_SUCCESS, ERR_MEM_OP );

            sOrgRow = sSearchRow = aUpdateTuple->row;

            IDE_TEST(
                sUpdateCursor->readOldRow( (const void**) & sSearchRow,
                                           & aUpdateTuple->rid )
                != IDE_SUCCESS );

            aUpdateTuple->row =
                (sSearchRow == NULL) ? sOrgRow : sSearchRow;
        }
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
                     " qmnUPTE::checkUpdateChildRefOnScan"
                     " memory error" );
    }
    IDE_EXCEPTION( ERR_NOT_FOUND )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmnUPTE::checkUpdateChildRefOnScan",
                                  "cursor not found" ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnUPTE::updateIndexTableCursor( qcTemplate     * aTemplate,
                                 qmncUPTE       * aCodePlan,
                                 qmndUPTE       * aDataPlan,
                                 smiValue       * aUpdateValue )
{
/***********************************************************************
 *
 * Description :
 *    UPDATE ���� ���� �� index table�� ���� update ����
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnUPTE::updateIndexTableCursor"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    void      * sRow;
    scGRID      sRowGRID;
    UInt        sIndexUpdateValueCount;

    // update index table
    if ( ( *aDataPlan->flag & QMND_UPTE_INDEX_CURSOR_MASK )
         == QMND_UPTE_INDEX_CURSOR_INITED )
    {
        // selected index table
        if ( ( *aDataPlan->flag & QMND_UPTE_SELECTED_INDEX_CURSOR_MASK )
             == QMND_UPTE_SELECTED_INDEX_CURSOR_TRUE )
        {
            IDE_TEST( qmsIndexTable::makeUpdateSmiValue(
                          aCodePlan->updateColumnCount,
                          aCodePlan->updateColumnIDs,
                          aUpdateValue,
                          aCodePlan->tableRef->selectedIndexTable,
                          ID_FALSE,
                          NULL,
                          NULL,
                          & sIndexUpdateValueCount,
                          aDataPlan->indexUpdateValue )
                      != IDE_SUCCESS );

            // PROJ-2204 join update, delete
            // tuple ������ cursor�� �����ؾ��Ѵ�.
            if ( ( aCodePlan->flag & QMNC_UPTE_VIEW_MASK )
                 == QMNC_UPTE_VIEW_TRUE )
            {
                IDE_TEST( aDataPlan->indexUpdateCursor->setRowPosition(
                              aDataPlan->indexUpdateTuple->row,
                              aDataPlan->indexUpdateTuple->rid )
                          != IDE_SUCCESS );
            }
            else
            {
                // Nothing to do.
            }

            IDE_TEST( aDataPlan->indexUpdateCursor->updateRow(
                          aDataPlan->indexUpdateValue,
                          & sRow,
                          & sRowGRID )
                      != IDE_SUCCESS );
        }
        else
        {
            // Nothing to do.
        }

        // �ٸ� index table�� update
        IDE_TEST( qmsIndexTable::updateIndexTableCursors(
                      aTemplate->stmt,
                      & (aDataPlan->indexTableCursorInfo),
                      aCodePlan->updateColumnCount,
                      aCodePlan->updateColumnIDs,
                      aUpdateValue,
                      ID_FALSE,
                      NULL,
                      NULL,
                      aDataPlan->updateTuple->rid )
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
qmnUPTE::updateIndexTableCursorForRowMovement( qcTemplate     * aTemplate,
                                               qmncUPTE       * aCodePlan,
                                               qmndUPTE       * aDataPlan,
                                               smOID            aPartOID,
                                               scGRID           aRowGRID,
                                               smiValue       * aUpdateValue )
{
/***********************************************************************
 *
 * Description :
 *    UPDATE ���� ���� �� index table�� ���� update ����
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnUPTE::updateIndexTableCursorForRowMovement"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    smOID       sPartOID = aPartOID;
    scGRID      sRowGRID = aRowGRID;
    void      * sRow;
    scGRID      sGRID;
    UInt        sIndexUpdateValueCount;

    // update index table
    if ( ( *aDataPlan->flag & QMND_UPTE_INDEX_CURSOR_MASK )
         == QMND_UPTE_INDEX_CURSOR_INITED )
    {
        // selected index table
        if ( ( *aDataPlan->flag & QMND_UPTE_SELECTED_INDEX_CURSOR_MASK )
             == QMND_UPTE_SELECTED_INDEX_CURSOR_TRUE )
        {
            IDE_TEST( qmsIndexTable::makeUpdateSmiValue(
                          aCodePlan->updateColumnCount,
                          aCodePlan->updateColumnIDs,
                          aUpdateValue,
                          aCodePlan->tableRef->selectedIndexTable,
                          ID_TRUE,
                          & sPartOID,
                          & sRowGRID,
                          & sIndexUpdateValueCount,
                          aDataPlan->indexUpdateValue )
                      != IDE_SUCCESS );

            // PROJ-2204 join update, delete
            // tuple ������ cursor�� �����ؾ��Ѵ�.
            if ( ( aCodePlan->flag & QMNC_UPTE_VIEW_MASK )
                 == QMNC_UPTE_VIEW_TRUE )
            {
                IDE_TEST( aDataPlan->indexUpdateCursor->setRowPosition(
                              aDataPlan->indexUpdateTuple->row,
                              aDataPlan->indexUpdateTuple->rid )
                          != IDE_SUCCESS );
            }
            else
            {
                // Nothing to do.
            }

            IDE_TEST( aDataPlan->indexUpdateCursor->updateRow(
                          aDataPlan->indexUpdateValue,
                          & sRow,
                          & sGRID )
                      != IDE_SUCCESS );
        }
        else
        {
            // Nothing to do.
        }

        // �ٸ� index table�� update
        IDE_TEST( qmsIndexTable::updateIndexTableCursors(
                      aTemplate->stmt,
                      & (aDataPlan->indexTableCursorInfo),
                      aCodePlan->updateColumnCount,
                      aCodePlan->updateColumnIDs,
                      aUpdateValue,
                      ID_TRUE,
                      & sPartOID,
                      & sRowGRID,
                      aDataPlan->updateTuple->rid )
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

IDE_RC qmnUPTE::getLastUpdatedRowGRID( qcTemplate * aTemplate,
                                       qmnPlan    * aPlan,
                                       scGRID     * aRowGRID )
{
/***********************************************************************
 *
 * Description : BUG-38129
 *     ������ update row�� GRID�� ��ȯ�Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    qmndUPTE * sDataPlan =
        (qmndUPTE*) (aTemplate->tmplate.data + aPlan->offset);

    *aRowGRID = sDataPlan->rowGRID;

    return IDE_SUCCESS;
}

IDE_RC qmnUPTE::checkDuplicateUpdate( qmncUPTE   * aCodePlan,
                                      qmndUPTE   * aDataPlan )
{
/***********************************************************************
 *
 * Description : BUG-39399 remove search key preserved table 
 *       join view update�� �ߺ� update���� üũ
 * Implementation :
 *    1. join�� ��� null���� üũ.
 *    2. cursor ����
 *    3. update �ߺ� üũ
 ***********************************************************************/
    
    scGRID            sNullRID;
    void            * sNullRow     = NULL;
    UInt              sTableType;
    void            * sTableHandle = NULL;
    idBool            sIsDupUpdate = ID_FALSE;
    
    /* PROJ-2464 hybrid partitioned table ���� */
    if ( aCodePlan->tableRef->partitionRef == NULL )
    {
        sTableType   = aCodePlan->tableRef->tableInfo->tableFlag & SMI_TABLE_TYPE_MASK;
        sTableHandle = aCodePlan->tableRef->tableHandle;
    }
    else
    {
        sTableType   = aDataPlan->updatePartInfo->tableFlag & SMI_TABLE_TYPE_MASK;
        sTableHandle = aDataPlan->updateTuple->tableHandle;
    }

    /* check null */
    if ( sTableType == SMI_TABLE_DISK )
    {
        SMI_MAKE_VIRTUAL_NULL_GRID( sNullRID );
            
        IDE_TEST_RAISE( SC_GRID_IS_EQUAL( sNullRID,
                                          aDataPlan->updateTuple->rid ),
                        ERR_MODIFY_UNABLE_RECORD );
    }
    else
    {
        IDE_TEST( smiGetTableNullRow( sTableHandle,
                                      (void **) & sNullRow,
                                      & sNullRID )
                  != IDE_SUCCESS );        

        IDE_TEST_RAISE( sNullRow == aDataPlan->updateTuple->row,
                        ERR_MODIFY_UNABLE_RECORD );
    }

    // PROJ-2204 join update, delete
    // tuple ������ cursor�� �����ؾ��Ѵ�.
    IDE_TEST( aDataPlan->updateCursor->setRowPosition( aDataPlan->updateTuple->row,
                                                       aDataPlan->updateTuple->rid )
              != IDE_SUCCESS );
        
    /* �ߺ� update���� üũ */
    IDE_TEST( aDataPlan->updateCursor->isUpdatedRowBySameStmt( &sIsDupUpdate )
              != IDE_SUCCESS );

    IDE_TEST_RAISE( sIsDupUpdate == ID_TRUE, ERR_MODIFY_UNABLE_RECORD );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_MODIFY_UNABLE_RECORD );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMN_MODIFY_UNABLE_RECORD ) ) ;
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmnUPTE::firstInitMultiTable( qcTemplate * aTemplate,
                                     qmncUPTE   * aCodePlan,
                                     qmndUPTE   * aDataPlan )
{
    UShort            sTableID;
    idBool            sIsNeedRebuild = ID_FALSE;
    qcmTableInfo    * sTableInfo = NULL;
    qmmMultiTables  * sTmp = NULL;
    qmnCursorInfo   * sCursorInfo = NULL;
    qmsPartitionRef * sPartitionRef = NULL;
    UInt              sPartitionCount;
    UInt              sIndexUpdateColumnCount;
    idBool            sIsInplaceUpdate = ID_FALSE;
    qmndMultiTables * sDataTable;
    UInt              i;
    UInt              j;

    //--------------------------------
    // ���ռ� �˻�
    //--------------------------------

    //--------------------------------
    // UPTE ���� ������ �ʱ�ȭ
    //--------------------------------
    aDataPlan->insertLobInfo = NULL;
    aDataPlan->insertValues  = NULL;
    aDataPlan->checkValues   = NULL;

    // Tuple Set������ �ʱ�ȭ
    aDataPlan->updateTuple     = NULL;
    aDataPlan->updateCursor    = NULL;
    aDataPlan->updateTupleID   = ID_USHORT_MAX;

    /* PROJ-2464 hybrid partitioned table ���� */
    aDataPlan->updatePartInfo = NULL;

    // index table cursor �ʱ�ȭ
    aDataPlan->indexUpdateCursor = NULL;
    aDataPlan->indexUpdateTuple = NULL;

    // set, where column list �ʱ�ȭ
    smiInitDMLRetryInfo( &(aDataPlan->retryInfo) );

    /* PROJ-2359 Table/Partition Access Option */
    aDataPlan->accessOption = QCM_ACCESS_OPTION_READ_WRITE;

    IDU_FIT_POINT("qmnUpdate::firstInitMultiTable::alloc::mTableArray",
                  idERR_ABORT_InsufficientMemory);
    IDE_TEST( aTemplate->stmt->qmxMem->alloc( ID_SIZEOF(qmndMultiTables) * aCodePlan->mMultiTableCount,
                                              (void**)&aDataPlan->mTableArray)
              != IDE_SUCCESS );
    //--------------------------------
    // cursorInfo ����
    //--------------------------------

    for ( sTmp = aCodePlan->mTableList, j = 0;
          sTmp != NULL;
          sTmp = sTmp->mNext, j++ )
    {
        QMND_UPDATE_MULTI_TABLES( &aDataPlan->mTableArray[j] );

        sDataTable = &aDataPlan->mTableArray[j];

        if ( sTmp->mInsteadOfTrigger == ID_TRUE )
        {
            // instead of trigger�� cursor�� �ʿ����.
            // Nothing to do.
        }
        else
        {
            sTableInfo = sTmp->mTableRef->tableInfo;

            // PROJ-2219 Row-level before update trigger
            // Invalid �� trigger�� ������ compile�ϰ�, DML�� rebuild �Ѵ�.
            if ( sTableInfo->triggerCount > 0 )
            {
                IDE_TEST( qdnTrigger::verifyTriggers( aTemplate->stmt,
                                                      sTableInfo,
                                                      sTmp->mUptColumnList,
                                                      &sIsNeedRebuild )
                          != IDE_SUCCESS );

                IDE_TEST_RAISE( sIsNeedRebuild == ID_TRUE,
                                trigger_invalid );
            }
            else
            {
                // Nothing to do.
            }
        }
        IDU_FIT_POINT("qmnUpdate::firstInitMultiTable::alloc::sCursorInfo",
                      idERR_ABORT_InsufficientMemory);
        IDE_TEST( aTemplate->stmt->qmxMem->alloc( ID_SIZEOF(qmnCursorInfo),
                                                  (void**)& sCursorInfo )
                  != IDE_SUCCESS );

        // cursorInfo �ʱ�ȭ
        sCursorInfo->cursor              = NULL;
        sCursorInfo->selectedIndex       = NULL;
        sCursorInfo->selectedIndexTuple  = NULL;
        sCursorInfo->accessOption        = QCM_ACCESS_OPTION_READ_WRITE; /* PROJ-2359 Table/Partition Access Option */
        sCursorInfo->updateColumnList    = sTmp->mUptColumnList;
        sCursorInfo->cursorType          = sTmp->mCursorType;
        if ( sTmp->mUpdateType == QMO_UPDATE_ROWMOVEMENT )
        {
            sCursorInfo->isRowMovementUpdate = ID_TRUE;
        }
        else
        {
            sCursorInfo->isRowMovementUpdate = ID_FALSE;
        }

        /* PROJ-2626 Snapshot Export */
        if ( aTemplate->stmt->mInplaceUpdateDisableFlag == ID_FALSE )
        {
            sIsInplaceUpdate = sTmp->mInplaceUpdate;
        }
        else
        {
            /* Nothing td do */
        }
        sCursorInfo->inplaceUpdate = sIsInplaceUpdate;
        sCursorInfo->lockMode            = SMI_LOCK_WRITE;
        sCursorInfo->stmtRetryColLst     = sTmp->mWhereColumnList;
        sCursorInfo->rowRetryColLst      = sTmp->mSetColumnList;

        // cursorInfo ����
        aTemplate->tmplate.rows[sTmp->mTableRef->table].cursorInfo = sCursorInfo;

        //--------------------------------
        // partition cursorInfo ����
        //--------------------------------
        if ( sTmp->mTableRef->partitionRef != NULL )
        {
            sPartitionCount = 0;
            for ( sPartitionRef = sTmp->mTableRef->partitionRef;
                  sPartitionRef != NULL;
                  sPartitionRef = sPartitionRef->next )
            {
                sPartitionCount++;
            }

            IDU_FIT_POINT("qmnUpdate::firstInitMultiTable::alloc::sCursorInfo2",
                          idERR_ABORT_InsufficientMemory);
            // cursorInfo ����
            IDE_TEST( aTemplate->stmt->qmxMem->alloc(
                          sPartitionCount * ID_SIZEOF(qmnCursorInfo),
                          (void**)& sCursorInfo )
                      != IDE_SUCCESS );
            for ( sPartitionRef = sTmp->mTableRef->partitionRef, i = 0;
                  sPartitionRef != NULL;
                  sPartitionRef = sPartitionRef->next, i++, sCursorInfo++ )
            {
                // cursorInfo �ʱ�ȭ
                sCursorInfo->cursor              = NULL;
                sCursorInfo->selectedIndex       = NULL;
                sCursorInfo->selectedIndexTuple  = NULL;
                /* PROJ-2359 Table/Partition Access Option */
                sCursorInfo->accessOption        = QCM_ACCESS_OPTION_READ_WRITE;
                sCursorInfo->updateColumnList    = sTmp->mPartColumnList[i];
                sCursorInfo->cursorType          = sTmp->mCursorType;
                sCursorInfo->inplaceUpdate       = sIsInplaceUpdate;
                sCursorInfo->lockMode            = SMI_LOCK_WRITE;
                if ( sTmp->mUpdateType == QMO_UPDATE_ROWMOVEMENT )
                {
                    sCursorInfo->isRowMovementUpdate = ID_TRUE;
                }
                else
                {
                    sCursorInfo->isRowMovementUpdate = ID_FALSE;
                }

                /* PROJ-2464 hybrid partitioned table ���� */
                sCursorInfo->stmtRetryColLst     = sTmp->mWherePartColumnList[i];
                sCursorInfo->rowRetryColLst      = sTmp->mSetPartColumnList[i];

                // cursorInfo ����
                aTemplate->tmplate.rows[sPartitionRef->table].cursorInfo = sCursorInfo;
            }
            // PROJ-1624 non-partitioned index
            // partitioned table�� ��� index table cursor�� update column list�� �����Ѵ�.
            if ( sTmp->mTableRef->selectedIndexTable != NULL )
            {
                IDE_TEST_RAISE( sTmp->mTableRef->indexTableRef == NULL, UNEXPECTED );
                sCursorInfo = (qmnCursorInfo *)aTemplate->tmplate.rows[sTmp->mTableRef->table].cursorInfo;

                IDE_TEST( qmsIndexTable::makeUpdateSmiColumnList(
                              sTmp->mColumnCount,
                              sTmp->mColumnIDs,
                              sTmp->mTableRef->selectedIndexTable,
                              sCursorInfo->isRowMovementUpdate,
                              &sIndexUpdateColumnCount,
                              sDataTable->mIndexUpdateColumnList )
                          != IDE_SUCCESS );

                if ( sIndexUpdateColumnCount > 0 )
                {
                    IDU_FIT_POINT("qmnUpdate::firstInitMultiTable::alloc::updateColumnList",
                                  idERR_ABORT_InsufficientMemory);
                    IDE_TEST( aTemplate->stmt->qmxMem->alloc(
                                  sIndexUpdateColumnCount * ID_SIZEOF(smiColumnList),
                                  (void**)& sCursorInfo->updateColumnList )
                              != IDE_SUCCESS );
                    idlOS::memcpy( sCursorInfo->updateColumnList,
                                   sDataTable->mIndexUpdateColumnList,
                                   sIndexUpdateColumnCount * ID_SIZEOF(smiColumnList) );
                    sCursorInfo->cursorType = SMI_UPDATE_CURSOR;

                    // update�ؾ���
                    sDataTable->mFlag &= ~QMND_UPTE_SELECTED_INDEX_CURSOR_MASK;
                    sDataTable->mFlag |= QMND_UPTE_SELECTED_INDEX_CURSOR_TRUE;
                }
                else
                {
                    // update�� �÷��� ���� ���
                    sCursorInfo->updateColumnList = NULL;
                    sCursorInfo->cursorType       = SMI_SELECT_CURSOR;
                    sCursorInfo->inplaceUpdate    = ID_FALSE;
                    sCursorInfo->lockMode         = SMI_LOCK_READ;
                }

                sDataTable->mIndexUpdateCount = sIndexUpdateColumnCount;
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

        if ( sTmp->mTableRef->tableInfo->lobColumnCount > 0 )
        {
            // PROJ-1362
            IDE_TEST( qmx::initializeLobInfo( aTemplate->stmt,
                                              &sDataTable->mLobInfo,
                                              (UShort)sTmp->mTableRef->tableInfo->lobColumnCount )
                      != IDE_SUCCESS );
        }
        else
        {
            /* Nothing to do */
        }

        switch ( sTmp->mUpdateType )
        {
            case QMO_UPDATE_ROWMOVEMENT:
            {
                IDE_TEST( sDataTable->mInsertCursorMgr.initialize(
                              aTemplate->stmt->qmxMem,
                              sTmp->mInsertTableRef,
                              ID_TRUE,
                              ID_FALSE )
                          != IDE_SUCCESS );
                // lob info �ʱ�ȭ
                if ( sTmp->mTableRef->tableInfo->lobColumnCount > 0 )
                {
                    // PROJ-1362
                    IDE_TEST( qmx::initializeLobInfo(
                                  aTemplate->stmt,
                                  & sDataTable->mInsertLobInfo,
                                  (UShort)sTmp->mTableRef->tableInfo->lobColumnCount )
                              != IDE_SUCCESS );
                }
                else
                {
                    sDataTable->mInsertLobInfo = NULL;
                }
                IDU_FIT_POINT("qmnUpdate::firstInitMultiTable::alloc::mInsertValues",
                              idERR_ABORT_InsufficientMemory);
                // insert smiValues �ʱ�ȭ
                IDE_TEST( aTemplate->stmt->qmxMem->alloc(
                              sTmp->mTableRef->tableInfo->columnCount
                              * ID_SIZEOF(smiValue),
                              (void**)& sDataTable->mInsertValues )
                          != IDE_SUCCESS );
                break;
            }
            case QMO_UPDATE_CHECK_ROWMOVEMENT:
            {
                IDU_FIT_POINT("qmnUpdate::firstInitMultiTable::alloc::mCheckValues",
                              idERR_ABORT_InsufficientMemory);
                // check smiValues �ʱ�ȭ
                IDE_TEST( aTemplate->stmt->qmxMem->alloc(
                              sTmp->mTableRef->tableInfo->columnCount
                              * ID_SIZEOF(smiValue),
                              (void**)&sDataTable->mCheckValues )
                          != IDE_SUCCESS );
                break;
            }
            default:
                break;
        }

        if ( sTmp->mDefaultColumns != NULL )
        {
            sTableID = sTmp->mDefaultTableRef->table;

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
            sDataTable->mDefaultExprRowBuffer = aTemplate->tmplate.rows[sTableID].row;
        }
        else
        {
            sDataTable->mDefaultExprRowBuffer = NULL;
        }
    }

    aDataPlan->limitStart = 1;
    aDataPlan->limitEnd   = 0;
    aDataPlan->needTriggerRow  = ID_FALSE;
    aDataPlan->existTrigger = ID_FALSE;
    aDataPlan->returnRow = NULL;

    return IDE_SUCCESS;

    IDE_EXCEPTION( trigger_invalid )
    {
        IDE_SET( ideSetErrorCode( qpERR_REBUILD_TRIGGER_INVALID ) );
    }
    IDE_EXCEPTION( UNEXPECTED )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmnUpdate::firstInitMultiTable",
                                  "indexTableRef is NULL" ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmnUPTE::allocIndexTableCursorMultiTable( qcTemplate * aTemplate,
                                                 qmncUPTE   * aCodePlan,
                                                 qmndUPTE   * aDataPlan )
{
    qmmMultiTables  * sTmp = NULL;
    UInt              i;

    for ( sTmp = aCodePlan->mTableList, i = 0;
          sTmp != NULL;
          sTmp = sTmp->mNext, i++ )
    {
        if ( sTmp->mTableRef->indexTableRef != NULL )
        {
            IDE_TEST( qmsIndexTable::initializeIndexTableCursors(
                          aTemplate->stmt,
                          sTmp->mTableRef->indexTableRef,
                          sTmp->mTableRef->indexTableCount,
                          sTmp->mTableRef->selectedIndexTable,
                          & (aDataPlan->mTableArray[i].mIndexTableCursorInfo) )
                      != IDE_SUCCESS );

            aDataPlan->mTableArray[i].mFlag &= ~QMND_UPTE_INDEX_CURSOR_MASK;
            aDataPlan->mTableArray[i].mFlag |= QMND_UPTE_INDEX_CURSOR_INITED;
        }
        else
        {
            /* Nothing to do */
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmnUPTE::doItFirstMultiTable( qcTemplate * aTemplate,
                                     qmnPlan    * aPlan,
                                     qmcRowFlag * aFlag )
{
    qmncUPTE        * sCodePlan = (qmncUPTE*) aPlan;
    qmndUPTE        * sDataPlan = (qmndUPTE*) (aTemplate->tmplate.data + aPlan->offset);
    idBool            sIsSkip = ID_FALSE;
    idBool            sSkipExist = ID_FALSE;
    ULong             sUpdateCount = 0;
    qmmMultiTables  * sTmp = NULL;
    UInt              i = 0;

    //-----------------------------------
    // Child Plan�� ������
    //-----------------------------------
    // doIt left child
    IDE_TEST( aPlan->left->doIt( aTemplate, aPlan->left, aFlag )
              != IDE_SUCCESS );

    if ( ( *aFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST )
    {
        for ( sTmp = sCodePlan->mTableList, i = 0;
              sTmp != NULL;
              sTmp = sTmp->mNext, i++ )
        {
            // check trigger
            IDE_TEST( checkTriggerMultiUpdate( aTemplate,
                                               sDataPlan,
                                               sTmp,
                                               i )
                      != IDE_SUCCESS );

            sDataPlan->updateTuple = &aTemplate->tmplate.rows[sTmp->mTableRef->table];
            if ( sTmp->mInsteadOfTrigger == ID_TRUE )
            {
                IDE_TEST( fireInsteadOfTriggerMultiUpdate( aTemplate,
                                                           sCodePlan,
                                                           sDataPlan,
                                                           sTmp,
                                                           i )
                          != IDE_SUCCESS );
            }
            else
            {
                sIsSkip = ID_FALSE;

                /* PROJ-2359 Table/Partition Access Option */
                IDE_TEST( qmx::checkAccessOption( sTmp->mTableRef->tableInfo,
                                                  ID_FALSE /* aIsInsertion */ )
                          != IDE_SUCCESS );

                if ( sTmp->mTableRef->partitionRef != NULL )
                {
                    IDE_TEST( qmx::checkAccessOptionForExistentRecord(
                                        sDataPlan->accessOption,
                                        sDataPlan->updateTuple->tableHandle )
                              != IDE_SUCCESS );
                }
                else
                {
                    /* Nothing to do */
                }

                // get cursor
                IDE_TEST( getCursorMultiTable( aTemplate,
                                               sCodePlan,
                                               sDataPlan,
                                               sTmp,
                                               &sIsSkip )
                          != IDE_SUCCESS );

                if ( sIsSkip == ID_FALSE )
                {
                    IDE_TEST( checkSkipMultiTable( aTemplate,
                                                   sDataPlan,
                                                   sTmp,
                                                   &sIsSkip )
                              != IDE_SUCCESS );
                }
                else
                {
                    /* Nothing to do */
                }

                if ( sIsSkip == ID_TRUE )
                {
                    sSkipExist = ID_TRUE;
                    continue;
                }
                else
                {
                    /* Nothing to do */
                }

                switch ( sTmp->mUpdateType )
                {
                    case QMO_UPDATE_NORMAL:
                    {
                        // update one record
                        IDE_TEST( updateOneRowMultiTable( aTemplate,
                                                          sCodePlan,
                                                          sDataPlan,
                                                          sTmp,
                                                          i )
                                  != IDE_SUCCESS );
                        sUpdateCount++;
                        break;
                    }
                    case QMO_UPDATE_ROWMOVEMENT:
                    {
                        // open insert cursor
                        IDE_TEST( openInsertCursorMultiTable( aTemplate,
                                                              sDataPlan,
                                                              sTmp,
                                                              i )
                                  != IDE_SUCCESS );

                        // update one record
                        IDE_TEST( updateOneRowForRowmovementMultiTable( aTemplate,
                                                                        sCodePlan,
                                                                        sDataPlan,
                                                                        sTmp,
                                                                        i )
                                  != IDE_SUCCESS );
                        sUpdateCount++;
                        break;
                    }
                    case QMO_UPDATE_CHECK_ROWMOVEMENT:
                    {
                        // update one record
                        IDE_TEST( updateOneRowForCheckRowmovementMultiTable( aTemplate,
                                                                             sCodePlan,
                                                                             sDataPlan,
                                                                             sTmp,
                                                                             i )
                                  != IDE_SUCCESS );
                        sUpdateCount++;
                        break;
                    }
                    case QMO_UPDATE_NO_ROWMOVEMENT:
                    {
                        // update one record
                        IDE_TEST( updateOneRowMultiTable( aTemplate,
                                                          sCodePlan,
                                                          sDataPlan,
                                                          sTmp,
                                                          i )
                                  != IDE_SUCCESS );
                        sUpdateCount++;
                        break;
                    }
                    default:
                        IDE_RAISE( UNEXPECTED );
                        break;
                }
            }
        }

        /**
         * Multiple Update�̱� ������ do it �� 1���� row�� �ƴ϶� �������� row��
         * ������Ʈ �� ���� �ְ� �ƹ��͵� ���� ���� �ִ�.
         *
         * nuwRows�� �⺻������ 1���� �����Ǿ��� ������ skip �� ���ٸ� update
         * row -1 ��ŭ ������Ű�� skip�� �ִٸ� update row �� ���� ����
         * 1�ΰ�� �׸��� �������� ���� numRow�� ��ȭ��������Ѵ�.
         */
        if ( sSkipExist == ID_TRUE )
        {
            if ( sUpdateCount < 1 )
            {
                aTemplate->numRows--;
            }
            else if ( sUpdateCount == 1 )
            {
                /* Nothing to do */
            }
            else
            {
                aTemplate->numRows += ( sUpdateCount - 1 );
            }
        }
        else
        {
            aTemplate->numRows += ( sUpdateCount - 1 );
        }

        sDataPlan->doIt = doItNextMultiTable;
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( UNEXPECTED )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmnUPTE::doItFirstMultiTable",
                                  "Not support updateType" ));
    }
    IDE_EXCEPTION_END;

    for ( i = 0; i < sCodePlan->mMultiTableCount; i++ )
    {
        if ( sDataPlan->mTableArray[i].mLobInfo != NULL )
        {
            (void)qmx::finalizeLobInfo( aTemplate->stmt, sDataPlan->mTableArray[i].mLobInfo );
        }
        else
        {
            /* Nothing to do */
        }
    }
    return IDE_FAILURE;
}

IDE_RC qmnUPTE::doItNextMultiTable( qcTemplate * aTemplate,
                                    qmnPlan    * aPlan,
                                    qmcRowFlag * aFlag )
{
    qmncUPTE        * sCodePlan = (qmncUPTE*) aPlan;
    qmndUPTE        * sDataPlan = (qmndUPTE*) (aTemplate->tmplate.data + aPlan->offset);
    idBool            sIsSkip = ID_FALSE;
    idBool            sSkipExist = ID_FALSE;
    qmmMultiTables  * sTmp = NULL;
    UInt              i = 0;
    UInt              sUpdateCount = 0;

    //-----------------------------------
    // Child Plan�� ������
    //-----------------------------------

    // doIt left child
    IDE_TEST( aPlan->left->doIt( aTemplate, aPlan->left, aFlag )
              != IDE_SUCCESS );

    if ( ( *aFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST )
    {
        for ( sTmp = sCodePlan->mTableList, i = 0;
              sTmp != NULL;
              sTmp = sTmp->mNext, i++ )
        {
            sDataPlan->updateTuple = &aTemplate->tmplate.rows[sTmp->mTableRef->table];

            if ( sTmp->mInsteadOfTrigger == ID_TRUE )
            {
                IDE_TEST( fireInsteadOfTriggerMultiUpdate( aTemplate,
                                                           sCodePlan,
                                                           sDataPlan,
                                                           sTmp,
                                                           i )
                          != IDE_SUCCESS );
            }
            else
            {
                sIsSkip = ID_FALSE;
                // get cursor
                IDE_TEST( getCursorMultiTable( aTemplate,
                                               sCodePlan,
                                               sDataPlan,
                                               sTmp,
                                               &sIsSkip )
                          != IDE_SUCCESS );

                if ( sIsSkip == ID_FALSE )
                {
                    IDE_TEST( checkSkipMultiTable( aTemplate,
                                                   sDataPlan,
                                                   sTmp,
                                                   &sIsSkip )
                              != IDE_SUCCESS );
                }
                else
                {
                    /* Nothing to do */
                }

                if ( sIsSkip == ID_TRUE )
                {
                    sSkipExist = ID_TRUE;
                    continue;
                }
                else
                {
                    /* Nothing to do */
                }

                switch ( sTmp->mUpdateType )
                {
                    case QMO_UPDATE_NORMAL:
                    {
                        // update one record
                        IDE_TEST( updateOneRowMultiTable( aTemplate,
                                                          sCodePlan,
                                                          sDataPlan,
                                                          sTmp,
                                                          i )
                                  != IDE_SUCCESS );
                        sUpdateCount++;
                        break;
                    }
                    case QMO_UPDATE_ROWMOVEMENT:
                    {
                        IDE_TEST( qmx::checkAccessOptionForExistentRecord(
                                            sDataPlan->accessOption,
                                            sDataPlan->updateTuple->tableHandle )
                                  != IDE_SUCCESS );

                        // open insert cursor
                        IDE_TEST( openInsertCursorMultiTable( aTemplate,
                                                              sDataPlan,
                                                              sTmp,
                                                              i )
                                  != IDE_SUCCESS );

                        // update one record
                        IDE_TEST( updateOneRowForRowmovementMultiTable( aTemplate,
                                                                        sCodePlan,
                                                                        sDataPlan,
                                                                        sTmp,
                                                                        i )
                                  != IDE_SUCCESS );
                        sUpdateCount++;
                        break;
                    }
                    case QMO_UPDATE_CHECK_ROWMOVEMENT:
                    {
                        IDE_TEST( qmx::checkAccessOptionForExistentRecord(
                                            sDataPlan->accessOption,
                                            sDataPlan->updateTuple->tableHandle )
                                  != IDE_SUCCESS );

                        // update one record
                        IDE_TEST( updateOneRowForCheckRowmovementMultiTable( aTemplate,
                                                                             sCodePlan,
                                                                             sDataPlan,
                                                                             sTmp,
                                                                             i )
                                  != IDE_SUCCESS );
                        sUpdateCount++;
                        break;
                    }
                    case QMO_UPDATE_NO_ROWMOVEMENT:
                    {
                        IDE_TEST( qmx::checkAccessOptionForExistentRecord(
                                            sDataPlan->accessOption,
                                            sDataPlan->updateTuple->tableHandle )
                                  != IDE_SUCCESS );

                        // update one record
                        IDE_TEST( updateOneRowMultiTable( aTemplate,
                                                          sCodePlan,
                                                          sDataPlan,
                                                          sTmp,
                                                          i )
                                  != IDE_SUCCESS );
                        sUpdateCount++;
                        break;
                    }
                    default:
                        IDE_RAISE( UNEXPECTED );
                        break;
                }
            }
        }

        /**
         * Multiple Update�̱� ������ do it �� 1���� row�� �ƴ϶� �������� row��
         * ������Ʈ �� ���� �ְ� �ƹ��͵� ���� ���� �ִ�.
         *
         * nuwRows�� �⺻������ 1���� �����Ǿ��� ������ skip �� ���ٸ� update
         * row -1 ��ŭ ������Ű�� skip�� �ִٸ� update row �� ���� ����
         * 1�ΰ�� �׸��� �������� ���� numRow�� ��ȭ��������Ѵ�.
         */
        if ( sSkipExist == ID_TRUE )
        {
            if ( sUpdateCount < 1 )
            {
                aTemplate->numRows--;
            }
            else if ( sUpdateCount == 1 )
            {
                /* Nothing to do */
            }
            else
            {
                aTemplate->numRows += ( sUpdateCount - 1 );
            }
        }
        else
        {
            aTemplate->numRows += ( sUpdateCount - 1 );
        }
    }
    else
    {
        // record�� ���� ���
        // ���� ������ ���� ���� ���� �Լ��� ������.
        sDataPlan->doIt = qmnUPTE::doItFirstMultiTable;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( UNEXPECTED )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmnUPTE::doItNextMultiTable",
                                  "Not support updateType" ));
    }
    IDE_EXCEPTION_END;

    for ( i = 0; i < sCodePlan->mMultiTableCount; i++ )
    {
        if ( sDataPlan->mTableArray[i].mLobInfo != NULL )
        {
            (void)qmx::finalizeLobInfo( aTemplate->stmt, sDataPlan->mTableArray[i].mLobInfo );
        }
        else
        {
            /* Nothing to do */
        }
    }
    return IDE_FAILURE;
}

IDE_RC qmnUPTE::getCursorMultiTable( qcTemplate     * aTemplate,
                                     qmncUPTE       * aCodePlan,
                                     qmndUPTE       * aDataPlan,
                                     qmmMultiTables * aTable,
                                     idBool         * aIsSkip )
{
    qmnCursorInfo * sCursorInfo = NULL;
    UShort          sTupleID = 0;
    ULong           sFlag = 0;

    if ( aTable->mTableRef->partitionRef == NULL )
    {
        if ( aDataPlan->updateTupleID != aTable->mTableRef->table )
        {
            aDataPlan->updateTupleID = aTable->mTableRef->table;

            sCursorInfo = (qmnCursorInfo *)aTemplate->tmplate.rows[aDataPlan->updateTupleID].cursorInfo;

            IDE_TEST_RAISE( sCursorInfo == NULL, ERR_NOT_FOUND );

            aDataPlan->updateCursor = sCursorInfo->cursor;

            aDataPlan->updatePartInfo             = aTable->mTableRef->tableInfo;
            aDataPlan->retryInfo.mIsWithoutRetry  = aCodePlan->withoutRetry;
            aDataPlan->retryInfo.mStmtRetryColLst = sCursorInfo->stmtRetryColLst;
            aDataPlan->retryInfo.mRowRetryColLst  = sCursorInfo->rowRetryColLst;

            /* PROJ-2359 Table/Partition Access Option */
            aDataPlan->accessOption = sCursorInfo->accessOption;
        }
        else
        {
            /* Nothing to do */
        }
    }
    else
    {
        if ( aDataPlan->updateTupleID != aDataPlan->updateTuple->partitionTupleID )
        {
            sTupleID = aDataPlan->updateTupleID;
            aDataPlan->updateTupleID = aDataPlan->updateTuple->partitionTupleID;

            // partition�� cursor�� ��´�.
            sCursorInfo = (qmnCursorInfo*)
                aTemplate->tmplate.rows[aDataPlan->updateTupleID].cursorInfo;

            sFlag = aTemplate->tmplate.rows[aDataPlan->updateTupleID].lflag;

            if ( ( sFlag & MTC_TUPLE_PARTITIONED_TABLE_MASK )
                 == MTC_TUPLE_PARTITIONED_TABLE_TRUE )
            {
                *aIsSkip = ID_TRUE;
                aDataPlan->updateTupleID = sTupleID;
                IDE_RAISE( normal_exit );
            }
            else
            {
                /* Nothing to do */
            }

            IDE_TEST_RAISE( sCursorInfo == NULL, ERR_NOT_FOUND );

            aDataPlan->updateCursor    = sCursorInfo->cursor;

            /* PROJ-2464 hybrid partitioned table ���� */
            IDE_TEST( smiGetTableTempInfo( aDataPlan->updateTuple->tableHandle,
                                           (void **)&(aDataPlan->updatePartInfo) )
                      != IDE_SUCCESS );

            aDataPlan->retryInfo.mIsWithoutRetry  = aCodePlan->withoutRetry;
            aDataPlan->retryInfo.mStmtRetryColLst = sCursorInfo->stmtRetryColLst;
            aDataPlan->retryInfo.mRowRetryColLst  = sCursorInfo->rowRetryColLst;

            /* PROJ-2359 Table/Partition Access Option */
            aDataPlan->accessOption = sCursorInfo->accessOption;
        }
        else
        {
            /* Nothing to do */
        }

        sCursorInfo = (qmnCursorInfo*)
            aTemplate->tmplate.rows[aTable->mTableRef->table].cursorInfo;

        IDE_TEST_RAISE( sCursorInfo == NULL, ERR_NOT_FOUND );

        aDataPlan->indexUpdateCursor = sCursorInfo->cursor;
        aDataPlan->indexUpdateTuple = sCursorInfo->selectedIndexTuple;
    }

    IDE_EXCEPTION_CONT( normal_exit );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NOT_FOUND )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmnUPTE::getCursorMultiTable",
                                  "cursor not found" ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

    // Update One Record
IDE_RC qmnUPTE::updateOneRowMultiTable( qcTemplate     * aTemplate,
                                        qmncUPTE       * aCodePlan,
                                        qmndUPTE       * aDataPlan,
                                        qmmMultiTables * aTable,
                                        UInt             aIndex )
{
    void              * sOrgRow;

    smiValue          * sSmiValues;
    smiValue          * sSmiValuesForPartition = NULL;
    smiValue            sValuesForPartition[QC_MAX_COLUMN_COUNT];
    void              * sRow;
    qmndMultiTables   * sDataTable;
    // PROJ-1784 DML Without Retry
    smiValue            sWhereSmiValues[QC_MAX_COLUMN_COUNT];
    smiValue            sSetSmiValues[QC_MAX_COLUMN_COUNT];
    idBool              sIsDiskTableOrPartition = ID_FALSE;

    /* PROJ-2464 hybrid partitioned table ����
     * Memory�� ���, newRow�� ���ο� �ּҸ� �Ҵ��Ѵ�. ����, newRow�� ���� �������� �ʴ´�.
     */
    sDataTable = &aDataPlan->mTableArray[aIndex];

    //-----------------------------------
    // clear lob
    //-----------------------------------
    // PROJ-1362
    if ( sDataTable->mLobInfo != NULL )
    {
        (void) qmx::clearLobInfo( sDataTable->mLobInfo );
    }
    else
    {
        // Nothing to do.
    }

    //-----------------------------------
    // copy old row
    //-----------------------------------

    if ( sDataTable->mNeedTriggerRow == ID_TRUE )
    {
        // OLD ROW REFERENCING�� ���� ����
        idlOS::memcpy( sDataTable->mOldRow,
                       aDataPlan->updateTuple->row,
                       aDataPlan->updateTuple->rowOffset );
    }
    else
    {
        // Nothing to do.
    }

    //-----------------------------------
    // set next sequence
    //-----------------------------------

    // Sequence Value ȹ��
    if ( aCodePlan->nextValSeqs != NULL )
    {
        IDE_TEST( qmx::readSequenceNextVals( aTemplate->stmt, aCodePlan->nextValSeqs )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    //-----------------------------------
    // make update smiValues
    //-----------------------------------

    sSmiValues = aTemplate->insOrUptRow[ aCodePlan->valueIdx ];

    // subquery�� ������ smi value ���� ����
    IDE_TEST( qmx::makeSmiValueForSubqueryMultiTable( aTemplate,
                                                      aTable->mTableRef->tableInfo,
                                                      aTable->mColumns,
                                                      aTable->mValues,
                                                      aTable->mValuesPos,
                                                      aCodePlan->subqueries,
                                                      aTable->mCanonizedTuple,
                                                      sSmiValues,
                                                      aTable->mIsNull,
                                                      sDataTable->mLobInfo )
              != IDE_SUCCESS );

    // ������ smi value ���� ����
    IDE_TEST( qmx::makeSmiValueForUpdate( aTemplate,
                                          aTable->mTableRef->tableInfo,
                                          aTable->mColumns,
                                          aTable->mValues,
                                          aTable->mCanonizedTuple,
                                          sSmiValues,
                                          aTable->mIsNull,
                                          sDataTable->mLobInfo )
              != IDE_SUCCESS );

    //-----------------------------------
    // Default Expr
    //-----------------------------------
    if ( aTable->mDefaultColumns != NULL )
    {
        qmsDefaultExpr::setRowBufferFromBaseColumn(
            &(aTemplate->tmplate),
            aTable->mTableRef->table,
            aTable->mDefaultTableRef->table,
            aTable->mDefaultBaseColumns,
            sDataTable->mDefaultExprRowBuffer );

        IDE_TEST( qmsDefaultExpr::setRowBufferFromSmiValueArray(
                      &(aTemplate->tmplate),
                      aTable->mDefaultTableRef,
                      aTable->mColumns,
                      sDataTable->mDefaultExprRowBuffer,
                      sSmiValues,
                      QCM_TABLE_TYPE_IS_DISK( aTable->mTableRef->tableInfo->tableFlag ) )
                  != IDE_SUCCESS );

        IDE_TEST( qmsDefaultExpr::calculateDefaultExpression(
                      aTemplate,
                      aTable->mDefaultTableRef,
                      aTable->mColumns,
                      aTable->mDefaultColumns,
                      sDataTable->mDefaultExprRowBuffer,
                      sSmiValues,
                      aTable->mTableRef->tableInfo->columns )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    //-----------------------------------
    // PROJ-2334 PMT
    // set update trigger memory variable column info
    //-----------------------------------
    if ( ( sDataTable->mExistTrigger == ID_TRUE ) &&
         ( aTable->mTableRef->tableInfo->tablePartitionType == QCM_PARTITIONED_TABLE ) )
    {
        sDataTable->mColumnsForRow = aDataPlan->updatePartInfo->columns;
    }
    else
    {
        // Nothing To Do
    }

    // PROJ-2219 Row-level before update trigger
    if ( sDataTable->mExistTrigger == ID_TRUE )
    {
        // ���� trigger���� lob column�� �ִ� table�� �������� �����Ƿ�
        // Table cursor�� NULL�� �ѱ��.
        IDE_TEST( qdnTrigger::fireTrigger(
                      aTemplate->stmt,
                      aTemplate->stmt->qmxMem,
                      aTable->mTableRef->tableInfo,
                      QCM_TRIGGER_ACTION_EACH_ROW,
                      QCM_TRIGGER_BEFORE,
                      QCM_TRIGGER_EVENT_UPDATE,
                      aTable->mUptColumnList, // UPDATE Column
                      NULL,                        // Table Cursor
                      SC_NULL_GRID,                // Row GRID
                      sDataTable->mOldRow,           // OLD ROW
                      sDataTable->mColumnsForRow,    // OLD ROW Column
                      sSmiValues,                  // NEW ROW(value list)
                      aTable->mColumns )           // NEW ROW Column
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    //-----------------------------------
    // update one row
    //-----------------------------------
    if ( aDataPlan->retryInfo.mIsWithoutRetry == ID_TRUE )
    {
        if ( aTable->mTableRef->tableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
        {
            sIsDiskTableOrPartition = QCM_TABLE_TYPE_IS_DISK( aDataPlan->updatePartInfo->tableFlag );
        }
        else
        {
            sIsDiskTableOrPartition = QCM_TABLE_TYPE_IS_DISK( aTable->mTableRef->tableInfo->tableFlag );
        }

        if ( sIsDiskTableOrPartition == ID_TRUE )
        {
            IDE_TEST( qmx::setChkSmiValueList( aDataPlan->updateTuple->row,
                                               aDataPlan->retryInfo.mStmtRetryColLst,
                                               sWhereSmiValues,
                                               & (aDataPlan->retryInfo.mStmtRetryValLst) )
                      != IDE_SUCCESS );

            IDE_TEST( qmx::setChkSmiValueList( aDataPlan->updateTuple->row,
                                               aDataPlan->retryInfo.mRowRetryColLst,
                                               sSetSmiValues,
                                               & (aDataPlan->retryInfo.mRowRetryValLst) )
                      != IDE_SUCCESS );
        }
        else
        {
            /* Nothing to do */
        }
    }
    else
    {
        // Nothing to do.
    }

    aDataPlan->retryInfo.mIsRowRetry = ID_FALSE;

    if ( QCM_TABLE_TYPE_IS_DISK( aTable->mTableRef->tableInfo->tableFlag ) !=
         QCM_TABLE_TYPE_IS_DISK( aDataPlan->updatePartInfo->tableFlag ) )
    {
        /* PROJ-2464 hybrid partitioned table ����
         * Partitioned Table�� �������� ���� smiValue Array�� Table Partition�� �°� ��ȯ�Ѵ�.
         */
        IDE_TEST( qmx::makeSmiValueWithSmiValue( aTable->mTableRef->tableInfo,
                                                 aDataPlan->updatePartInfo,
                                                 aTable->mColumns,
                                                 aTable->mColumnCount,
                                                 sSmiValues,
                                                 sValuesForPartition )
                  != IDE_SUCCESS );

        sSmiValuesForPartition = sValuesForPartition;
    }
    else
    {
        sSmiValuesForPartition = sSmiValues;
    }

    while ( aDataPlan->updateCursor->updateRow( sSmiValuesForPartition,
                                                &( aDataPlan->retryInfo ),
                                                & sRow,
                                                & aDataPlan->rowGRID )
           != IDE_SUCCESS )
    {
        IDE_TEST( ideGetErrorCode() != smERR_RETRY_Row_Retry );

        IDE_TEST( aDataPlan->updateCursor->getLastRow(
                      (const void**) &(aDataPlan->updateTuple->row),
                      & aDataPlan->updateTuple->rid )
                  != IDE_SUCCESS );

        if ( sDataTable->mNeedTriggerRow == ID_TRUE )
        {
            // OLD ROW REFERENCING�� ���� ����
            idlOS::memcpy( sDataTable->mOldRow,
                           aDataPlan->updateTuple->row,
                           aDataPlan->updateTuple->rowOffset );
        }
        else
        {
            // Nothing to do.
        }

        if ( sDataTable->mLobInfo != NULL )
        {
            (void) qmx::clearLobInfo( sDataTable->mLobInfo );
        }
        else
        {
            // Nothing to do.
        }

        // ������ smi value ���� ����
        IDE_TEST( qmx::makeSmiValueForUpdate( aTemplate,
                                              aTable->mTableRef->tableInfo,
                                              aTable->mColumns,
                                              aTable->mValues,
                                              aTable->mCanonizedTuple,
                                              sSmiValues,
                                              aTable->mIsNull,
                                              sDataTable->mLobInfo )
                  != IDE_SUCCESS );

        //-----------------------------------
        // Default Expr
        //-----------------------------------

        if ( aTable->mDefaultColumns != NULL )
        {
            qmsDefaultExpr::setRowBufferFromBaseColumn(
                &(aTemplate->tmplate),
                aTable->mTableRef->table,
                aTable->mDefaultTableRef->table,
                aTable->mDefaultBaseColumns,
                sDataTable->mDefaultExprRowBuffer );

            IDE_TEST( qmsDefaultExpr::setRowBufferFromSmiValueArray(
                          &(aTemplate->tmplate),
                          aTable->mDefaultTableRef,
                          aTable->mColumns,
                          sDataTable->mDefaultExprRowBuffer,
                          sSmiValues,
                          QCM_TABLE_TYPE_IS_DISK( aTable->mTableRef->tableInfo->tableFlag ) )
                      != IDE_SUCCESS );

            IDE_TEST( qmsDefaultExpr::calculateDefaultExpression(
                          aTemplate,
                          aTable->mDefaultTableRef,
                          aTable->mColumns,
                          aTable->mDefaultColumns,
                          sDataTable->mDefaultExprRowBuffer,
                          sSmiValues,
                          aTable->mTableRef->tableInfo->columns )
                      != IDE_SUCCESS );
        }
        else
        {
            /* Nothing to do */
        }

        if ( sIsDiskTableOrPartition == ID_TRUE )
        {
            IDE_TEST( qmx::setChkSmiValueList( aDataPlan->updateTuple->row,
                                               aDataPlan->retryInfo.mRowRetryColLst,
                                               sSetSmiValues,
                                               & (aDataPlan->retryInfo.mRowRetryValLst) )
                      != IDE_SUCCESS );
        }
        else
        {
            // Nothing to do.
        }

        aDataPlan->retryInfo.mIsRowRetry = ID_TRUE;

        if ( QCM_TABLE_TYPE_IS_DISK( aTable->mTableRef->tableInfo->tableFlag ) !=
             QCM_TABLE_TYPE_IS_DISK( aDataPlan->updatePartInfo->tableFlag ) )
        {
            /* PROJ-2464 hybrid partitioned table ����
             * Partitioned Table�� �������� ���� smiValue Array�� Table Partition�� �°� ��ȯ�Ѵ�.
             */
            IDE_TEST( qmx::makeSmiValueWithSmiValue( aTable->mTableRef->tableInfo,
                                                     aDataPlan->updatePartInfo,
                                                     aTable->mColumns,
                                                     aTable->mColumnCount,
                                                     sSmiValues,
                                                     sValuesForPartition )
                      != IDE_SUCCESS );

            sSmiValuesForPartition = sValuesForPartition;
        }
        else
        {
            sSmiValuesForPartition = sSmiValues;
        }
    }

    // update index table
    IDE_TEST( updateIndexTableCursorMultiTable( aTemplate,
                                                aDataPlan,
                                                aTable,
                                                aIndex,
                                                sSmiValues )
              != IDE_SUCCESS );

    /* PROJ-2464 hybrid partitioned table ����
     *  Disk Partition�� ���, Disk Type�� Lob Column�� �ʿ��ϴ�.
     *  Memory/Volatile Partition�� ���, �ش� Partition�� Lob Column�� �ʿ��ϴ�.
     */
    if ( ( QCM_TABLE_TYPE_IS_DISK( aTable->mTableRef->tableInfo->tableFlag ) !=
           QCM_TABLE_TYPE_IS_DISK( aDataPlan->updatePartInfo->tableFlag ) ) ||
         ( ( aTable->mTableRef->tableInfo->tablePartitionType == QCM_PARTITIONED_TABLE ) &&
           ( QCM_TABLE_TYPE_IS_DISK( aDataPlan->updatePartInfo->tableFlag ) != ID_TRUE ) ) )
    {
        IDE_TEST( qmx::changeLobColumnInfo( sDataTable->mLobInfo,
                                            aDataPlan->updatePartInfo->columns )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    // Lob�÷� ó��
    IDE_TEST( qmx::copyAndOutBindLobInfo( aTemplate->stmt,
                                          sDataTable->mLobInfo,
                                          aDataPlan->updateCursor,
                                          sRow,
                                          aDataPlan->rowGRID )
              != IDE_SUCCESS );

    //-----------------------------------
    // check constraint
    //-----------------------------------
    if ( ( sDataTable->mNeedTriggerRow == ID_TRUE ) ||
         ( aTable->mCheckConstrList != NULL ) )
    {
        // NEW ROW�� ȹ��
        IDE_TEST( aDataPlan->updateCursor->getLastModifiedRow(
                      &sDataTable->mNewRow,
                      aDataPlan->updateTuple->rowOffset )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    /* PROJ-1107 Check Constraint ���� */
    if ( aTable->mCheckConstrList != NULL )
    {
        sOrgRow = aDataPlan->updateTuple->row;
        aDataPlan->updateTuple->row = sDataTable->mNewRow;

        IDE_TEST( qdnCheck::verifyCheckConstraintList(
                      aTemplate,
                      aTable->mCheckConstrList )
                  != IDE_SUCCESS );

        aDataPlan->updateTuple->row = sOrgRow;
    }
    else
    {
        // Nothing to do.
    }

    //-----------------------------------
    // update after trigger
    //-----------------------------------
    if ( sDataTable->mExistTrigger == ID_TRUE )
    {
        // PROJ-1359 Trigger
        // ROW GRANULARITY TRIGGER�� ����
        IDE_TEST( qdnTrigger::fireTrigger( aTemplate->stmt,
                                           aTemplate->stmt->qmxMem,
                                           aTable->mTableRef->tableInfo,
                                           QCM_TRIGGER_ACTION_EACH_ROW,
                                           QCM_TRIGGER_AFTER,
                                           QCM_TRIGGER_EVENT_UPDATE,
                                           aTable->mUptColumnList,
                                           aDataPlan->updateCursor,         /* Table Cursor */
                                           aDataPlan->updateTuple->rid,     /* Row GRID */
                                           sDataTable->mOldRow,
                                           sDataTable->mColumnsForRow,
                                           sDataTable->mNewRow,
                                           sDataTable->mColumnsForRow )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    if ( ( *aDataPlan->flag & QMND_UPTE_UPDATE_MASK )
         == QMND_UPTE_UPDATE_FALSE )
    {
        *aDataPlan->flag &= ~QMND_UPTE_UPDATE_MASK;
        *aDataPlan->flag |= QMND_UPTE_UPDATE_TRUE;
    }
    else
    {
        // Nothing to do.
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmnUPTE::checkSkipMultiTable( qcTemplate     * aTemplate,
                                     qmndUPTE       * aDataPlan,
                                     qmmMultiTables * aTable,
                                     idBool         * aIsSkip )
{
    scGRID   sNullRID;
    void   * sNullRow = NULL;
    UInt     sTableType;
    void   * sTableHandle = NULL;
    idBool   sIsSkip = ID_FALSE;

    if ( aTable->mTableRef->partitionRef == NULL )
    {
        sTableType   = aTable->mTableRef->tableInfo->tableFlag & SMI_TABLE_TYPE_MASK;
        sTableHandle = aTable->mTableRef->tableHandle;
    }
    else
    {
        sTableType   = aDataPlan->updatePartInfo->tableFlag & SMI_TABLE_TYPE_MASK;
        sTableHandle = aDataPlan->updateTuple->tableHandle;
    }

    if ( aDataPlan->updateTuple->row == NULL )
    {
        sIsSkip = ID_TRUE;
        IDE_CONT( normal_exit );
    }
    else
    {
        /* Nothing to do */
    }

    /* check null */
    if ( sTableType == SMI_TABLE_DISK )
    {
        SMI_MAKE_VIRTUAL_NULL_GRID( sNullRID );

        if ( SC_GRID_IS_EQUAL( sNullRID, aDataPlan->updateTuple->rid )
             == ID_TRUE )
        {
            sIsSkip = ID_TRUE;
            IDE_CONT( normal_exit );
        }
        else
        {
            /* Nothing to do */
        }
    }
    else
    {
        IDE_TEST( smiGetTableNullRow( sTableHandle,
                                      (void **) &sNullRow,
                                      & sNullRID )
                  != IDE_SUCCESS );

        if ( sNullRow == aDataPlan->updateTuple->row )
        {
            sIsSkip = ID_TRUE;
            IDE_CONT( normal_exit );
        }
        else
        {
            /* Nothing to do */
        }
    }

    /* Left Outer join�� right pad null �� view�� ��쿡�� �ڽ��� view tuple��
     * ���� row�� ���� pad null�� �����Ѵ�.
     * right view�� ���� ���̺��� ������Ʈ�� ��� right pad null�ÿ��� skip �ؾ� �Ѵ�.
     */
    if ( aTable->mViewID > -1 )
    {
        if ( ( aTemplate->tmplate.rows[aTable->mViewID].lflag & MTC_TUPLE_VIEW_PADNULL_MASK )
             == MTC_TUPLE_VIEW_PADNULL_TRUE )
        {
            sIsSkip = ID_TRUE;
            IDE_CONT( normal_exit );
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

    // PROJ-2204 join update, delete
    // tuple ������ cursor�� �����ؾ��Ѵ�.
    IDE_TEST( aDataPlan->updateCursor->setRowPosition( aDataPlan->updateTuple->row,
                                                       aDataPlan->updateTuple->rid )
              != IDE_SUCCESS );

    /* �ߺ� update���� üũ */
    IDE_TEST( aDataPlan->updateCursor->isUpdatedRowBySameStmt( &sIsSkip )
              != IDE_SUCCESS );

    IDE_EXCEPTION_CONT( normal_exit );

    *aIsSkip = sIsSkip;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmnUPTE::updateIndexTableCursorMultiTable( qcTemplate     * aTemplate,
                                                  qmndUPTE       * aDataPlan,
                                                  qmmMultiTables * aTable,
                                                  UInt             aIndex,
                                                  smiValue       * aUpdateValue )
{
    void            * sRow;
    scGRID            sRowGRID;
    UInt              sIndexUpdateValueCount;
    qmndMultiTables * sDataTable;

    sDataTable = &aDataPlan->mTableArray[aIndex];

    // update index table
    if ( ( sDataTable->mFlag & QMND_UPTE_INDEX_CURSOR_MASK )
         == QMND_UPTE_INDEX_CURSOR_INITED )
    {
        if ( ( sDataTable->mFlag & QMND_UPTE_SELECTED_INDEX_CURSOR_MASK )
             == QMND_UPTE_SELECTED_INDEX_CURSOR_TRUE )
        {
            IDE_TEST( qmsIndexTable::makeUpdateSmiValue(
                          aTable->mColumnCount,
                          aTable->mColumnIDs,
                          aUpdateValue,
                          aTable->mTableRef->selectedIndexTable,
                          ID_FALSE,
                          NULL,
                          NULL,
                          &sIndexUpdateValueCount,
                          aDataPlan->indexUpdateValue )
                      != IDE_SUCCESS );

            // PROJ-2204 join update, delete
            // tuple ������ cursor�� �����ؾ��Ѵ�.
            if ( aTable->mTableRef->view != NULL )
            {
                IDE_TEST( aDataPlan->indexUpdateCursor->setRowPosition(
                              aDataPlan->indexUpdateTuple->row,
                              aDataPlan->indexUpdateTuple->rid )
                          != IDE_SUCCESS );
            }
            else
            {
                // Nothing to do.
            }

            IDE_TEST( aDataPlan->indexUpdateCursor->updateRow(
                          aDataPlan->indexUpdateValue,
                          & sRow,
                          & sRowGRID )
                      != IDE_SUCCESS );
        }
        else
        {
            /* Nothing to do */
        }
        // �ٸ� index table�� update
        IDE_TEST( qmsIndexTable::updateIndexTableCursors(
                      aTemplate->stmt,
                      & (sDataTable->mIndexTableCursorInfo),
                      aTable->mColumnCount,
                      aTable->mColumnIDs,
                      aUpdateValue,
                      ID_FALSE,
                      NULL,
                      NULL,
                      aDataPlan->updateTuple->rid )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmnUPTE::closeCursorMultiTable( qcTemplate * aTemplate,
                                       qmnPlan    * aPlan )
{
    qmncUPTE * sCodePlan = (qmncUPTE*) aPlan;
    qmndUPTE * sDataPlan =
        (qmndUPTE*) (aTemplate->tmplate.data + aPlan->offset);
    UInt        i;

    for ( i = 0; i < sCodePlan->mMultiTableCount; i++ )
    {
        if ( ( sDataPlan->mTableArray[i].mFlag & QMND_UPTE_CURSOR_MASK )
             == QMND_UPTE_CURSOR_OPEN )
        {
            sDataPlan->mTableArray[i].mFlag &= ~QMND_UPTE_CURSOR_MASK;
            sDataPlan->mTableArray[i].mFlag |= QMND_UPTE_CURSOR_CLOSED;

            IDE_TEST( sDataPlan->mTableArray[i].mInsertCursorMgr.closeCursor()
                      != IDE_SUCCESS );
        }
        else
        {
            // Nothing to do.
        }

        if ( ( sDataPlan->mTableArray[i].mFlag & QMND_UPTE_INDEX_CURSOR_MASK )
             == QMND_UPTE_INDEX_CURSOR_INITED )
        {
            sDataPlan->mTableArray[i].mFlag &= ~QMND_UPTE_INDEX_CURSOR_MASK;
            sDataPlan->mTableArray[i].mFlag |= QMND_UPTE_INDEX_CURSOR_NONE;

            IDE_TEST( qmsIndexTable::closeIndexTableCursors(
                          & (sDataPlan->mTableArray[i].mIndexTableCursorInfo) )
                      != IDE_SUCCESS );
        }
        else
        {
            /* Nothing to do */
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    for ( i = 0; i < sCodePlan->mMultiTableCount; i++ )
    {
        if ( ( sDataPlan->mTableArray[i].mFlag & QMND_UPTE_CURSOR_MASK )
             == QMND_UPTE_CURSOR_OPEN )
        {
            sDataPlan->mTableArray[i].mFlag &= ~QMND_UPTE_CURSOR_MASK;
            sDataPlan->mTableArray[i].mFlag |= QMND_UPTE_CURSOR_CLOSED;

            ( void )sDataPlan->mTableArray[i].mInsertCursorMgr.closeCursor();
        }
        else
        {
            // Nothing to do.
        }

        if ( ( sDataPlan->mTableArray[i].mFlag & QMND_UPTE_INDEX_CURSOR_MASK )
             == QMND_UPTE_INDEX_CURSOR_INITED )
        {
            sDataPlan->mTableArray[i].mFlag &= ~QMND_UPTE_INDEX_CURSOR_MASK;
            sDataPlan->mTableArray[i].mFlag |= QMND_UPTE_INDEX_CURSOR_NONE;

            qmsIndexTable::finalizeIndexTableCursors(
                &(sDataPlan->mTableArray[i].mIndexTableCursorInfo));
        }
        else
        {
            /* Nothing to do */
        }
    }
    return IDE_FAILURE;
}

IDE_RC qmnUPTE::allocTriggerRowMultiTable( qcTemplate * aTemplate,
                                           qmncUPTE   * aCodePlan,
                                           qmndUPTE   * aDataPlan )
{
    UInt sMaxRowOffsetForUpdate = 0;
    UInt sMaxRowOffsetForInsert = 0;
    UInt i                      = 0;
    qmmMultiTables * sTmp       = NULL;

    for ( sTmp = aCodePlan->mTableList, i = 0; sTmp != NULL; sTmp = sTmp->mNext, i++ )
    {
        //---------------------------------
        // Trigger�� ���� ������ ����
        //---------------------------------

        if ( sTmp->mTableRef->tableInfo->triggerCount > 0 )
        {
            if ( sTmp->mInsteadOfTrigger == ID_TRUE )
            {
                // instead of trigger������ smiValues�� ����Ѵ�.

                IDU_FIT_POINT("qmnUpdate::allocTriggerRowMultiTable::alloc::mOldRow",
                              idERR_ABORT_InsufficientMemory);
                // alloc sOldRow
                IDE_TEST( aTemplate->stmt->qmxMem->alloc(
                        ID_SIZEOF(smiValue) *
                        sTmp->mTableRef->tableInfo->columnCount,
                        (void**) & aDataPlan->mTableArray[i].mOldRow )
                    != IDE_SUCCESS);

                // alloc sNewRow
                IDU_FIT_POINT("qmnUpdate::allocTriggerRowMultiTable::alloc::mNewRow",
                              idERR_ABORT_InsufficientMemory);
                IDE_TEST( aTemplate->stmt->qmxMem->alloc(
                        ID_SIZEOF(smiValue) *
                        sTmp->mTableRef->tableInfo->columnCount,
                        (void**) & aDataPlan->mTableArray[i].mNewRow )
                    != IDE_SUCCESS);
            }
            else
            {
                sMaxRowOffsetForUpdate = qmx::getMaxRowOffset( &(aTemplate->tmplate),
                                                               sTmp->mTableRef );
                if ( sTmp->mUpdateType == QMO_UPDATE_ROWMOVEMENT )
                {
                    sMaxRowOffsetForInsert = qmx::getMaxRowOffset( &(aTemplate->tmplate),
                                                                   sTmp->mInsertTableRef );
                }
                else
                {
                    sMaxRowOffsetForInsert = 0;
                }

                IDU_FIT_POINT("qmnUpdate::allocTriggerRowMultiTable::alloc::mOldRow2",
                              idERR_ABORT_InsufficientMemory);

                if ( sMaxRowOffsetForUpdate > 0 )
                {
                    // Old Row Referencing�� ���� ���� �Ҵ�
                    IDE_TEST( aTemplate->stmt->qmxMem->alloc(
                                sMaxRowOffsetForUpdate,
                                (void**) & aDataPlan->mTableArray[i].mOldRow )
                              != IDE_SUCCESS);
                }
                else
                {
                    aDataPlan->mTableArray[i].mOldRow = NULL;
                }

                IDU_FIT_POINT("qmnUpdate::allocTriggerRowMultiTable::alloc::mNewRow2",
                              idERR_ABORT_InsufficientMemory);

                sMaxRowOffsetForInsert = IDL_MAX( sMaxRowOffsetForUpdate, sMaxRowOffsetForInsert);
                if ( sMaxRowOffsetForInsert > 0 )
                {
                    // New Row Referencing�� ���� ���� �Ҵ�
                    IDE_TEST( aTemplate->stmt->qmxMem->cralloc(
                                sMaxRowOffsetForInsert,
                                (void**) & aDataPlan->mTableArray[i].mNewRow )
                              != IDE_SUCCESS);
                }
                else
                {
                    aDataPlan->mTableArray[i].mNewRow = NULL;
                }
            }

            aDataPlan->mTableArray[i].mColumnsForRow = sTmp->mTableRef->tableInfo->columns;

            aDataPlan->mTableArray[i].mNeedTriggerRow = ID_FALSE;
            aDataPlan->mTableArray[i].mExistTrigger = ID_TRUE;
        }
        else
        {
            // check constraint�� return into������ trigger row�� ����Ѵ�.
            if ( sTmp->mCheckConstrList != NULL )
            {
                sMaxRowOffsetForUpdate = qmx::getMaxRowOffset( &(aTemplate->tmplate),
                                                               sTmp->mTableRef );

                if ( sTmp->mUpdateType == QMO_UPDATE_ROWMOVEMENT )
                {
                    sMaxRowOffsetForInsert = qmx::getMaxRowOffset( &(aTemplate->tmplate),
                                                                   sTmp->mInsertTableRef );
                }
                else
                {
                    sMaxRowOffsetForInsert = 0;
                }

                IDU_FIT_POINT("qmnUpdate::allocTriggerRowMultiTable::alloc::mOldRow3",
                              idERR_ABORT_InsufficientMemory);

                if ( sMaxRowOffsetForUpdate > 0 )
                {
                    // Old Row Referencing�� ���� ���� �Ҵ�
                    IDE_TEST( aTemplate->stmt->qmxMem->alloc(
                                sMaxRowOffsetForUpdate,
                                (void**) & aDataPlan->mTableArray[i].mOldRow )
                              != IDE_SUCCESS);
                }
                else
                {
                    aDataPlan->mTableArray[i].mOldRow = NULL;
                }

                IDU_FIT_POINT("qmnUpdate::allocTriggerRowMultiTable::alloc::mNewRow3",
                              idERR_ABORT_InsufficientMemory);

                sMaxRowOffsetForInsert = IDL_MAX( sMaxRowOffsetForUpdate, sMaxRowOffsetForInsert);
                if ( sMaxRowOffsetForInsert > 0 )
                {
                    // New Row Referencing�� ���� ���� �Ҵ�
                    IDE_TEST( aTemplate->stmt->qmxMem->cralloc(
                                sMaxRowOffsetForInsert,
                                (void**) & aDataPlan->mTableArray[i].mNewRow )
                              != IDE_SUCCESS);
                }
                else
                {
                    aDataPlan->mTableArray[i].mNewRow = NULL;
                }
            }
            else
            {
                aDataPlan->mTableArray[i].mOldRow = NULL;
                aDataPlan->mTableArray[i].mNewRow = NULL;
            }

            aDataPlan->mTableArray[i].mNeedTriggerRow = ID_FALSE;
            aDataPlan->mTableArray[i].mExistTrigger = ID_FALSE;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmnUPTE::checkTriggerMultiUpdate( qcTemplate     * aTemplate,
                                         qmndUPTE       * aDataPlan,
                                         qmmMultiTables * aTable,
                                         UInt             aIndex )
{

    idBool     sNeedTriggerRow;

    if ( aDataPlan->mTableArray[aIndex].mExistTrigger == ID_TRUE )
    {
        if ( aTable->mInsteadOfTrigger == ID_TRUE )
        {
            IDE_TEST( qdnTrigger::needTriggerRow( aTemplate->stmt,
                                                  aTable->mTableRef->tableInfo,
                                                  QCM_TRIGGER_INSTEAD_OF,
                                                  QCM_TRIGGER_EVENT_UPDATE,
                                                  aTable->mUptColumnList,
                                                  &sNeedTriggerRow )
                      != IDE_SUCCESS );
        }
        else
        {
            // Trigger�� ���� Referencing Row�� �ʿ������� �˻�
            // PROJ-2219 Row-level before update trigger
            IDE_TEST( qdnTrigger::needTriggerRow( aTemplate->stmt,
                                                  aTable->mTableRef->tableInfo,
                                                  QCM_TRIGGER_BEFORE,
                                                  QCM_TRIGGER_EVENT_UPDATE,
                                                  aTable->mUptColumnList,
                                                  &sNeedTriggerRow )
                      != IDE_SUCCESS );

            if ( sNeedTriggerRow == ID_FALSE )
            {
                IDE_TEST( qdnTrigger::needTriggerRow( aTemplate->stmt,
                                                      aTable->mTableRef->tableInfo,
                                                      QCM_TRIGGER_AFTER,
                                                      QCM_TRIGGER_EVENT_UPDATE,
                                                      aTable->mUptColumnList,
                                                      &sNeedTriggerRow )
                          != IDE_SUCCESS );
            }
            else
            {
                // Nothing to do.
            }
        }
        aDataPlan->mTableArray[aIndex].mNeedTriggerRow = sNeedTriggerRow;
    }
    else
    {
        // Nothing to do.
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmnUPTE::fireInsteadOfTriggerMultiUpdate( qcTemplate     * aTemplate,
                                                 qmncUPTE       * aCodePlan,
                                                 qmndUPTE       * aDataPlan,
                                                 qmmMultiTables * aTable,
                                                 UInt             aIndex )
{
    qcmTableInfo * sTableInfo = NULL;
    qcmColumn    * sQcmColumn = NULL;
    mtcColumn    * sColumn    = NULL;
    smiValue     * sSmiValues = NULL;
    mtcStack     * sStack     = NULL;
    SInt           sRemain    = 0;
    UShort         i          = 0;
    qmndMultiTables * sDataTable = NULL;

    sTableInfo = aTable->mTableRef->tableInfo;
    sDataTable = &aDataPlan->mTableArray[aIndex];

    if ( sDataTable->mNeedTriggerRow == ID_TRUE )
    {
        //-----------------------------------
        // get Old Row
        //-----------------------------------

        sStack = aTemplate->tmplate.stack;
        sRemain = aTemplate->tmplate.stackRemain;

        IDE_TEST_RAISE( sRemain < aDataPlan->updateTuple->columnCount,
                        ERR_STACK_OVERFLOW );

        // UPDATE�� VIEW ���̿� FILT ���� �ٸ� ���鿡 ���� stack�� ����Ǿ��� �� �����Ƿ�
        // stack�� view tuple�� �÷����� �缳���Ѵ�.
        for ( i = 0, sColumn = aDataPlan->updateTuple->columns;
              i < aDataPlan->updateTuple->columnCount;
              i++, sColumn++, sStack++ )
        {
            sStack->column = sColumn;
            sStack->value  =
                (void*)((SChar*)aDataPlan->updateTuple->row + sColumn->column.offset);
        }

        /* PROJ-2464 hybrid partitioned table ���� */
        if ( sTableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
        {
            if ( aDataPlan->updatePartInfo != NULL )
            {
                if ( aDataPlan->updateTuple->tableHandle != aDataPlan->updatePartInfo->tableHandle )
                {
                    IDE_TEST( smiGetTableTempInfo( aDataPlan->updateTuple->tableHandle,
                                                   (void **)&(aDataPlan->updatePartInfo) )
                              != IDE_SUCCESS );
                }
                else
                {
                    /* Nothing to do */
                }
            }
            else
            {
                IDE_TEST( smiGetTableTempInfo( aDataPlan->updateTuple->tableHandle,
                                               (void **)&(aDataPlan->updatePartInfo) )
                          != IDE_SUCCESS );
            }

            // ��� Partition�� ��Ȯ�ϹǷ�, �۾� ������ Partition���� �����Ѵ�.
            sTableInfo = aDataPlan->updatePartInfo;
            sDataTable->mColumnsForRow = aDataPlan->updatePartInfo->columns;
        }
        else
        {
            /* Nothing to do */
        }

        IDE_TEST( qmx::makeSmiValueWithStack( sDataTable->mColumnsForRow,
                                              aTemplate,
                                              aTemplate->tmplate.stack,
                                              sTableInfo,
                                              (smiValue*) sDataTable->mOldRow,
                                              NULL )
                  != IDE_SUCCESS );

        //-----------------------------------
        // get New Row
        //-----------------------------------

        // Sequence Value ȹ��
        if ( aCodePlan->nextValSeqs != NULL )
        {
            IDE_TEST( qmx::readSequenceNextVals( aTemplate->stmt,
                                                 aCodePlan->nextValSeqs )
                      != IDE_SUCCESS );
        }
        else
        {
            /* Nothing to do */
        }

        sSmiValues = aTemplate->insOrUptRow[ aCodePlan->valueIdx ];

        // subquery�� ������ smi value ���� ����
        IDE_TEST( qmx::makeSmiValueForSubqueryMultiTable( aTemplate,
                                                          sTableInfo,
                                                          aTable->mColumns,
                                                          aTable->mValues,
                                                          aTable->mValuesPos,
                                                          aCodePlan->subqueries,
                                                          aTable->mCanonizedTuple,
                                                          sSmiValues,
                                                          aTable->mIsNull,
                                                          sDataTable->mLobInfo )
                  != IDE_SUCCESS );

        // ������ smi value ���� ����
        IDE_TEST( qmx::makeSmiValueForUpdate( aTemplate,
                                              aTable->mTableRef->tableInfo,
                                              aTable->mColumns,
                                              aTable->mValues,
                                              aTable->mCanonizedTuple,
                                              sSmiValues,
                                              aTable->mIsNull,
                                              sDataTable->mLobInfo )
                  != IDE_SUCCESS );

        // old smiValues�� new smiValues�� ����
        idlOS::memcpy( sDataTable->mNewRow,
                       sDataTable->mOldRow,
                       ID_SIZEOF(smiValue) * aDataPlan->updateTuple->columnCount );

        // update smiValues�� ������ ��ġ�� ����
        for ( sQcmColumn = aTable->mColumns;
              sQcmColumn != NULL;
              sQcmColumn = sQcmColumn->next, sSmiValues++ )
        {
            for ( i = 0; i < sTableInfo->columnCount; i++ )
            {
                if ( sQcmColumn->basicInfo->column.id ==
                     sDataTable->mColumnsForRow[i].basicInfo->column.id )
                {
                    *((smiValue*)sDataTable->mNewRow + i ) = *sSmiValues;
                    break;
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

    if ( sDataTable->mExistTrigger == ID_TRUE )
    {
        // PROJ-1359 Trigger
        // ROW GRANULARITY TRIGGER�� ����
        IDE_TEST( qdnTrigger::fireTrigger( aTemplate->stmt,
                                           aTemplate->stmt->qmxMem,
                                           sTableInfo,
                                           QCM_TRIGGER_ACTION_EACH_ROW,
                                           QCM_TRIGGER_INSTEAD_OF,
                                           QCM_TRIGGER_EVENT_UPDATE,
                                           NULL,               // UPDATE Column
                                           NULL,               /* Table Cursor */
                                           SC_NULL_GRID,       /* Row GRID */
                                           sDataTable->mOldRow,
                                           sDataTable->mColumnsForRow,
                                           sDataTable->mNewRow,
                                           sDataTable->mColumnsForRow )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_STACK_OVERFLOW );
    {
        IDE_SET(ideSetErrorCode(mtERR_ABORT_STACK_OVERFLOW));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmnUPTE::updateOneRowForCheckRowmovementMultiTable( qcTemplate     * aTemplate,
                                                           qmncUPTE       * aCodePlan,
                                                           qmndUPTE       * aDataPlan,
                                                           qmmMultiTables * aTable,
                                                           UInt             aIndex )
{
    void              * sOrgRow;
    qmsPartitionRef   * sSelectedPartitionRef;
    smiValue          * sSmiValues;
    smiValue          * sSmiValuesForPartition = NULL;
    smiValue            sValuesForPartition[QC_MAX_COLUMN_COUNT];
    void              * sRow;
    qmndMultiTables   * sDataTable = NULL;

    sDataTable = &aDataPlan->mTableArray[aIndex];

    //-----------------------------------
    // clear lob
    //-----------------------------------
    // PROJ-1362
    if ( sDataTable->mLobInfo != NULL )
    {
        (void) qmx::clearLobInfo( sDataTable->mLobInfo );
    }
    else
    {
        // Nothing to do.
    }

    //-----------------------------------
    // copy old row
    //-----------------------------------
    if ( sDataTable->mNeedTriggerRow == ID_TRUE )
    {
        // OLD ROW REFERENCING�� ���� ����
        idlOS::memcpy( sDataTable->mOldRow,
                       aDataPlan->updateTuple->row,
                       aDataPlan->updateTuple->rowOffset );
    }
    else
    {
        // Nothing to do.
    }

    //-----------------------------------
    // set next sequence
    //-----------------------------------
    // Sequence Value ȹ��
    if ( aCodePlan->nextValSeqs != NULL )
    {
        IDE_TEST( qmx::readSequenceNextVals( aTemplate->stmt, aCodePlan->nextValSeqs )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    //-----------------------------------
    // make update smiValues
    //-----------------------------------
    sSmiValues = aTemplate->insOrUptRow[ aCodePlan->valueIdx ];

    // subquery�� ������ smi value ���� ����
    IDE_TEST( qmx::makeSmiValueForSubqueryMultiTable( aTemplate,
                                                      aTable->mTableRef->tableInfo,
                                                      aTable->mColumns,
                                                      aTable->mValues,
                                                      aTable->mValuesPos,
                                                      aCodePlan->subqueries,
                                                      aTable->mCanonizedTuple,
                                                      sSmiValues,
                                                      aTable->mIsNull,
                                                      sDataTable->mLobInfo )
              != IDE_SUCCESS );

    // ������ smi value ���� ����
    IDE_TEST( qmx::makeSmiValueForUpdate( aTemplate,
                                          aTable->mTableRef->tableInfo,
                                          aTable->mColumns,
                                          aTable->mValues,
                                          aTable->mCanonizedTuple,
                                          sSmiValues,
                                          aTable->mIsNull,
                                          sDataTable->mLobInfo )
              != IDE_SUCCESS );

    //-----------------------------------
    // Default Expr
    //-----------------------------------
    if ( aTable->mDefaultColumns != NULL )
    {
        qmsDefaultExpr::setRowBufferFromBaseColumn(
            &(aTemplate->tmplate),
            aTable->mTableRef->table,
            aTable->mDefaultTableRef->table,
            aTable->mDefaultBaseColumns,
            sDataTable->mDefaultExprRowBuffer );

        IDE_TEST( qmsDefaultExpr::setRowBufferFromSmiValueArray(
                      &(aTemplate->tmplate),
                      aTable->mDefaultTableRef,
                      aTable->mColumns,
                      sDataTable->mDefaultExprRowBuffer,
                      sSmiValues,
                      QCM_TABLE_TYPE_IS_DISK( aTable->mTableRef->tableInfo->tableFlag ) )
                  != IDE_SUCCESS );

        IDE_TEST( qmsDefaultExpr::calculateDefaultExpression(
                      aTemplate,
                      aTable->mDefaultTableRef,
                      aTable->mColumns,
                      aTable->mDefaultColumns,
                      sDataTable->mDefaultExprRowBuffer,
                      sSmiValues,
                      aTable->mTableRef->tableInfo->columns )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    // PROJ-2219 Row-level before update trigger
    if ( sDataTable->mExistTrigger == ID_TRUE )
    {
        // ���� trigger���� lob column�� �ִ� table�� �������� �����Ƿ�
        // Table cursor�� NULL�� �ѱ��.
        IDE_TEST( qdnTrigger::fireTrigger(
                      aTemplate->stmt,
                      aTemplate->stmt->qmxMem,
                      aTable->mTableRef->tableInfo,
                      QCM_TRIGGER_ACTION_EACH_ROW,
                      QCM_TRIGGER_BEFORE,
                      QCM_TRIGGER_EVENT_UPDATE,
                      aTable->mUptColumnList, // UPDATE Column
                      NULL,                        // Table Cursor
                      SC_NULL_GRID,                // Row GRID
                      sDataTable->mOldRow,           // OLD ROW
                      sDataTable->mColumnsForRow,    // OLD ROW Column
                      sSmiValues,                  // NEW ROW(value list)
                      aTable->mColumns )           // NEW ROW Column
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    IDE_TEST( qmx::makeSmiValueForChkRowMovement(
                  aTable->mUptColumnList,
                  sSmiValues,
                  aTable->mTableRef->tableInfo->partKeyColumns,
                  aDataPlan->updateTuple,
                  sDataTable->mCheckValues )
              != IDE_SUCCESS );

    IDE_TEST_RAISE( qmoPartition::partitionFilteringWithRow(
                        aTable->mTableRef,
                        sDataTable->mCheckValues,
                        &sSelectedPartitionRef )
                    != IDE_SUCCESS,
                    ERR_NO_ROW_MOVEMENT );

    IDE_TEST_RAISE( sSelectedPartitionRef->table != aDataPlan->updateTupleID,
                    ERR_NO_ROW_MOVEMENT );

    aDataPlan->retryInfo.mIsRowRetry = ID_FALSE;

    if ( QCM_TABLE_TYPE_IS_DISK( aTable->mTableRef->tableInfo->tableFlag ) !=
         QCM_TABLE_TYPE_IS_DISK( aDataPlan->updatePartInfo->tableFlag ) )
    {
        /* PROJ-2464 hybrid partitioned table ����
         * Partitioned Table�� �������� ���� smiValue Array�� Table Partition�� �°� ��ȯ�Ѵ�.
         */
        IDE_TEST( qmx::makeSmiValueWithSmiValue( aTable->mTableRef->tableInfo,
                                                 aDataPlan->updatePartInfo,
                                                 aTable->mColumns,
                                                 aTable->mColumnCount,
                                                 sSmiValues,
                                                 sValuesForPartition )
                  != IDE_SUCCESS );

        sSmiValuesForPartition = sValuesForPartition;
    }
    else
    {
        sSmiValuesForPartition = sSmiValues;
    }

    IDE_TEST ( aDataPlan->updateCursor->updateRow( sSmiValuesForPartition,
                                                   NULL,
                                                   &sRow,
                                                   & aDataPlan->rowGRID )
               != IDE_SUCCESS );

    // update index table
    IDE_TEST( updateIndexTableCursorMultiTable( aTemplate,
                                                aDataPlan,
                                                aTable,
                                                aIndex,
                                                sSmiValues )
              != IDE_SUCCESS );

    /* PROJ-2464 hybrid partitioned table ����
     *  Disk Partition�� ���, Disk Type�� Lob Column�� �ʿ��ϴ�.
     *  Memory/Volatile Partition�� ���, �ش� Partition�� Lob Column�� �ʿ��ϴ�.
     */
    if ( ( QCM_TABLE_TYPE_IS_DISK( aTable->mTableRef->tableInfo->tableFlag ) !=
           QCM_TABLE_TYPE_IS_DISK( aDataPlan->updatePartInfo->tableFlag ) ) ||
         ( ( aTable->mTableRef->tableInfo->tablePartitionType == QCM_PARTITIONED_TABLE ) &&
           ( QCM_TABLE_TYPE_IS_DISK( aDataPlan->updatePartInfo->tableFlag ) != ID_TRUE ) ) )
    {
        IDE_TEST( qmx::changeLobColumnInfo( sDataTable->mLobInfo,
                                            aDataPlan->updatePartInfo->columns )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    // Lob�÷� ó��
    IDE_TEST( qmx::copyAndOutBindLobInfo( aTemplate->stmt,
                                          sDataTable->mLobInfo,
                                          aDataPlan->updateCursor,
                                          sRow,
                                          aDataPlan->rowGRID )
              != IDE_SUCCESS );

    //-----------------------------------
    // check constraint
    //-----------------------------------
    if ( ( sDataTable->mNeedTriggerRow == ID_TRUE ) ||
         ( aTable->mCheckConstrList != NULL ) )
    {
        // NEW ROW�� ȹ��
        IDE_TEST( aDataPlan->updateCursor->getLastModifiedRow(
                      &sDataTable->mNewRow,
                      aDataPlan->updateTuple->rowOffset )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    /* PROJ-1107 Check Constraint ���� */
    if ( aTable->mCheckConstrList != NULL )
    {
        sOrgRow = aDataPlan->updateTuple->row;
        aDataPlan->updateTuple->row = sDataTable->mNewRow;

        IDE_TEST( qdnCheck::verifyCheckConstraintList(
                      aTemplate,
                      aTable->mCheckConstrList )
                  != IDE_SUCCESS );

        aDataPlan->updateTuple->row = sOrgRow;
    }
    else
    {
        // Nothing to do.
    }

    //-----------------------------------
    // update after trigger
    //-----------------------------------
    if ( sDataTable->mExistTrigger == ID_TRUE )
    {
        // PROJ-1359 Trigger
        // ROW GRANULARITY TRIGGER�� ����
        IDE_TEST( qdnTrigger::fireTrigger( aTemplate->stmt,
                                           aTemplate->stmt->qmxMem,
                                           aTable->mTableRef->tableInfo,
                                           QCM_TRIGGER_ACTION_EACH_ROW,
                                           QCM_TRIGGER_AFTER,
                                           QCM_TRIGGER_EVENT_UPDATE,
                                           aTable->mUptColumnList,
                                           aDataPlan->updateCursor,         /* Table Cursor */
                                           aDataPlan->updateTuple->rid,     /* Row GRID */
                                           sDataTable->mOldRow,
                                           sDataTable->mColumnsForRow,
                                           sDataTable->mNewRow,
                                           sDataTable->mColumnsForRow )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    if ( ( *aDataPlan->flag & QMND_UPTE_UPDATE_MASK )
         == QMND_UPTE_UPDATE_FALSE )
    {
        *aDataPlan->flag &= ~QMND_UPTE_UPDATE_MASK;
        *aDataPlan->flag |= QMND_UPTE_UPDATE_TRUE;
    }
    else
    {
        // Nothing to do.
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NO_ROW_MOVEMENT )
    {
        IDE_SET( ideSetErrorCode(qpERR_ABORT_QMV_INVALID_PARTITION_KEY_INSERT) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmnUPTE::openInsertCursorMultiTable( qcTemplate     * aTemplate,
                                            qmndUPTE       * aDataPlan,
                                            qmmMultiTables * aTable,
                                            UInt             aIndex )
{
    smiCursorProperties   sCursorProperty;
    UShort                sTupleID         = 0;
    idBool                sIsDiskChecked   = ID_FALSE;
    smiFetchColumnList  * sFetchColumnList = NULL;
    qmndMultiTables     * sDataTable;

    sDataTable = &aDataPlan->mTableArray[aIndex];

    if ( ( ( sDataTable->mFlag & QMND_UPTE_CURSOR_MASK )
           == QMND_UPTE_CURSOR_CLOSED ) &&
         ( aTable->mInsertTableRef != NULL ) )
    {
        // INSERT �� ���� Cursor ����
        SMI_CURSOR_PROP_INIT_FOR_FULL_SCAN( &sCursorProperty, aTemplate->stmt->mStatistics );

        if ( aTable->mInsertTableRef->tableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
        {
            if ( aTable->mInsertTableRef->partitionSummary->diskPartitionRef != NULL )
            {
                sTupleID = aTable->mInsertTableRef->partitionSummary->diskPartitionRef->table;
                sIsDiskChecked = ID_TRUE;
            }
            else
            {
                /* Nothing to do */
            }
        }
        else
        {
            if ( QCM_TABLE_TYPE_IS_DISK( aTable->mInsertTableRef->tableInfo->tableFlag ) == ID_TRUE )
            {
                sTupleID = aTable->mInsertTableRef->table;
                sIsDiskChecked = ID_TRUE;
            }
            else
            {
                /* Nothing to do */
            }
        }

        if ( sIsDiskChecked == ID_TRUE )
        {
            // PROJ-1705
            // ����Ű üũ�� ���� �о�� �� ��ġ�÷�����Ʈ ����
            IDE_TEST( qdbCommon::makeFetchColumnList4TupleID(
                          aTemplate,
                          sTupleID,
                          sDataTable->mNeedTriggerRow,  // aIsNeedAllFetchColumn
                          NULL,             // index
                          ID_TRUE,          // allocSmiColumnListEx
                          & sFetchColumnList )
                      != IDE_SUCCESS );

            sCursorProperty.mFetchColumnList = sFetchColumnList;
        }
        else
        {
            /* Nothing to do */
        }

        IDE_TEST( sDataTable->mInsertCursorMgr.openCursor(
                      aTemplate->stmt,
                      SMI_LOCK_WRITE | SMI_TRAVERSE_FORWARD | SMI_PREVIOUS_DISABLE,
                      & sCursorProperty )
                  != IDE_SUCCESS );

        sDataTable->mFlag &= ~QMND_UPTE_CURSOR_MASK;
        sDataTable->mFlag |= QMND_UPTE_CURSOR_OPEN;
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmnUPTE::updateIndexTableCursorRowMoveMultiTable( qcTemplate      * aTemplate,
                                                         qmndUPTE        * aDataPlan,
                                                         qmmMultiTables  * aTable,
                                                         UInt              aIndex,
                                                         smOID             aPartOID,
                                                         scGRID            aRowGRID,
                                                         smiValue        * aUpdateValue )
{
    smOID             sPartOID = aPartOID;
    scGRID            sRowGRID = aRowGRID;
    void            * sRow;
    scGRID            sGRID;
    UInt              sIndexUpdateValueCount;
    qmndMultiTables * sDataTable;

    sDataTable = &aDataPlan->mTableArray[aIndex];

    // update index table
    if ( ( sDataTable->mFlag & QMND_UPTE_INDEX_CURSOR_MASK )
         == QMND_UPTE_INDEX_CURSOR_INITED )
    {
        if ( ( sDataTable->mFlag & QMND_UPTE_SELECTED_INDEX_CURSOR_MASK )
             == QMND_UPTE_SELECTED_INDEX_CURSOR_TRUE )
        {
            IDE_TEST( qmsIndexTable::makeUpdateSmiValue(
                          aTable->mColumnCount,
                          aTable->mColumnIDs,
                          aUpdateValue,
                          aTable->mTableRef->selectedIndexTable,
                          ID_TRUE,
                          & sPartOID,
                          & sRowGRID,
                          & sIndexUpdateValueCount,
                          aDataPlan->indexUpdateValue )
                      != IDE_SUCCESS );

            // PROJ-2204 join update, delete
            // tuple ������ cursor�� �����ؾ��Ѵ�.
            if ( aTable->mTableRef->view != NULL )
            {
                IDE_TEST( aDataPlan->indexUpdateCursor->setRowPosition(
                              aDataPlan->indexUpdateTuple->row,
                              aDataPlan->indexUpdateTuple->rid )
                          != IDE_SUCCESS );
            }
            else
            {
                // Nothing to do.
            }
            IDE_TEST( aDataPlan->indexUpdateCursor->updateRow(
                          aDataPlan->indexUpdateValue,
                          & sRow,
                          & sGRID )
                      != IDE_SUCCESS );
        }
        else
        {
            /* Nothing to do */
        }
        // �ٸ� index table�� update
        IDE_TEST( qmsIndexTable::updateIndexTableCursors(
                      aTemplate->stmt,
                      & (sDataTable->mIndexTableCursorInfo),
                      aTable->mColumnCount,
                      aTable->mColumnIDs,
                      aUpdateValue,
                      ID_TRUE,
                      & sPartOID,
                      & sRowGRID,
                      aDataPlan->updateTuple->rid )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmnUPTE::updateOneRowForRowmovementMultiTable( qcTemplate     * aTemplate,
                                                      qmncUPTE       * aCodePlan,
                                                      qmndUPTE       * aDataPlan,
                                                      qmmMultiTables * aTable,
                                                      UInt             aIndex )
{
    smiTableCursor    * sInsertCursor = NULL;
    void              * sOrgRow;

    UShort              sPartitionTupleID       = 0;
    mtcTuple          * sSelectedPartitionTuple = NULL;
    mtcTuple            sCopyTuple;
    idBool              sNeedToRecoverTuple     = ID_FALSE;

    qmsPartitionRef   * sSelectedPartitionRef;
    qcmTableInfo      * sSelectedPartitionInfo = NULL;
    qmnCursorInfo     * sCursorInfo;
    smiTableCursor    * sCursor                = NULL;
    smiValue          * sSmiValues;
    smiValue          * sSmiValuesForPartition = NULL;
    smiValue            sValuesForPartition[QC_MAX_COLUMN_COUNT];
    void              * sRow                   = NULL;
    smOID               sPartOID;
    qcmColumn         * sColumnsForNewRow      = NULL;
    qmndMultiTables   * sDataTable;

    sDataTable = &aDataPlan->mTableArray[aIndex];

    //-----------------------------------
    // clear lob
    //-----------------------------------
    // PROJ-1362
    if ( sDataTable->mLobInfo != NULL )
    {
        (void) qmx::clearLobInfo( sDataTable->mLobInfo );
    }
    else
    {
        // Nothing to do.
    }

    if ( sDataTable->mInsertLobInfo != NULL )
    {
        (void) qmx::initLobInfo( sDataTable->mInsertLobInfo );
    }
    else
    {
        // Nothing to do.
    }

    //-----------------------------------
    // copy old row
    //-----------------------------------
    if ( sDataTable->mNeedTriggerRow == ID_TRUE )
    {
        // OLD ROW REFERENCING�� ���� ����
        idlOS::memcpy( sDataTable->mOldRow,
                       aDataPlan->updateTuple->row,
                       aDataPlan->updateTuple->rowOffset );
    }
    else
    {
        // Nothing to do.
    }

    //-----------------------------------
    // set next sequence
    //-----------------------------------
    // Sequence Value ȹ��
    if ( aCodePlan->nextValSeqs != NULL )
    {
        IDE_TEST( qmx::readSequenceNextVals( aTemplate->stmt, aCodePlan->nextValSeqs )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    //-----------------------------------
    // make update smiValues
    //-----------------------------------
    sSmiValues = aTemplate->insOrUptRow[ aCodePlan->valueIdx ];

    // subquery�� ������ smi value ���� ����
    IDE_TEST( qmx::makeSmiValueForSubqueryMultiTable( aTemplate,
                                                      aTable->mTableRef->tableInfo,
                                                      aTable->mColumns,
                                                      aTable->mValues,
                                                      aTable->mValuesPos,
                                                      aCodePlan->subqueries,
                                                      aTable->mCanonizedTuple,
                                                      sSmiValues,
                                                      aTable->mIsNull,
                                                      sDataTable->mLobInfo )
              != IDE_SUCCESS );

    // ������ smi value ���� ����
    IDE_TEST( qmx::makeSmiValueForUpdate( aTemplate,
                                          aTable->mTableRef->tableInfo,
                                          aTable->mColumns,
                                          aTable->mValues,
                                          aTable->mCanonizedTuple,
                                          sSmiValues,
                                          aTable->mIsNull,
                                          sDataTable->mLobInfo )
              != IDE_SUCCESS );

    //-----------------------------------
    // Default Expr
    //-----------------------------------
    if ( aTable->mDefaultColumns != NULL )
    {
        qmsDefaultExpr::setRowBufferFromBaseColumn(
            &(aTemplate->tmplate),
            aTable->mTableRef->table,
            aTable->mDefaultTableRef->table,
            aTable->mDefaultBaseColumns,
            sDataTable->mDefaultExprRowBuffer );

        IDE_TEST( qmsDefaultExpr::setRowBufferFromSmiValueArray(
                      &(aTemplate->tmplate),
                      aTable->mDefaultTableRef,
                      aTable->mColumns,
                      sDataTable->mDefaultExprRowBuffer,
                      sSmiValues,
                      QCM_TABLE_TYPE_IS_DISK( aTable->mTableRef->tableInfo->tableFlag ) )
                  != IDE_SUCCESS );

        IDE_TEST( qmsDefaultExpr::calculateDefaultExpression(
                      aTemplate,
                      aTable->mDefaultTableRef,
                      aTable->mColumns,
                      aTable->mDefaultColumns,
                      sDataTable->mDefaultExprRowBuffer,
                      sSmiValues,
                      aTable->mTableRef->tableInfo->columns )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    // PROJ-2219 Row-level before update trigger
    if ( sDataTable->mExistTrigger == ID_TRUE )
    {
        // ���� trigger���� lob column�� �ִ� table�� �������� �����Ƿ�
        // Table cursor�� NULL�� �ѱ��.
        IDE_TEST( qdnTrigger::fireTrigger(
                      aTemplate->stmt,
                      aTemplate->stmt->qmxMem,
                      aTable->mTableRef->tableInfo,
                      QCM_TRIGGER_ACTION_EACH_ROW,
                      QCM_TRIGGER_BEFORE,
                      QCM_TRIGGER_EVENT_UPDATE,
                      aTable->mUptColumnList, // UPDATE Column
                      NULL,                        // Table Cursor
                      SC_NULL_GRID,                // Row GRID
                      sDataTable->mOldRow,           // OLD ROW
                      sDataTable->mColumnsForRow,    // OLD ROW Column
                      sSmiValues,                  // NEW ROW(value list)
                      aTable->mColumns )           // NEW ROW Column
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    // row movement�� smi value ���� ����
    IDE_TEST( qmx::makeSmiValueForRowMovement(
                  aTable->mTableRef->tableInfo,
                  aTable->mUptColumnList,
                  sSmiValues,
                  aDataPlan->updateTuple,
                  aDataPlan->updateCursor,
                  sDataTable->mLobInfo,
                  sDataTable->mInsertValues,
                  sDataTable->mInsertLobInfo )
              != IDE_SUCCESS );

    if ( qmoPartition::partitionFilteringWithRow(
                        aTable->mTableRef,
                        sDataTable->mInsertValues,
                        &sSelectedPartitionRef )
          != IDE_SUCCESS )
    {
        IDE_CLEAR();

        //-----------------------------------
        // tableRef�� ���� partition�� ���
        // insert row -> update row
        //-----------------------------------

        /* PROJ-1090 Function-based Index */
        if ( aTable->mDefaultColumns != NULL )
        {
            IDE_TEST( qmsDefaultExpr::setRowBufferFromSmiValueArray(
                          &(aTemplate->tmplate),
                          aTable->mDefaultTableRef,
                          aTable->mTableRef->tableInfo->columns,
                          sDataTable->mDefaultExprRowBuffer,
                          sDataTable->mInsertValues,
                          QCM_TABLE_TYPE_IS_DISK( aTable->mTableRef->tableInfo->tableFlag ) )
                      != IDE_SUCCESS );

            IDE_TEST( qmsDefaultExpr::calculateDefaultExpression(
                          aTemplate,
                          aTable->mDefaultTableRef,
                          NULL,
                          aTable->mDefaultColumns,
                          sDataTable->mDefaultExprRowBuffer,
                          sDataTable->mInsertValues,
                          aTable->mTableRef->tableInfo->columns )
                      != IDE_SUCCESS );
        }
        else
        {
            /* Nothing to do */
        }

        IDE_TEST( sDataTable->mInsertCursorMgr.partitionFilteringWithRow(
                      sDataTable->mInsertValues,
                      sDataTable->mInsertLobInfo,
                      &sSelectedPartitionInfo )
                  != IDE_SUCCESS );

        /* PROJ-2359 Table/Partition Access Option */
        IDE_TEST( qmx::checkAccessOption( sSelectedPartitionInfo,
                                          ID_TRUE, /* aIsInsertion */
                                          QCG_CHECK_SHARD_DML_CONSISTENCY( aTemplate->stmt ) )
                  != IDE_SUCCESS );

        // insert row
        IDE_TEST( sDataTable->mInsertCursorMgr.getCursor( &sCursor )
                  != IDE_SUCCESS );

        if ( QCM_TABLE_TYPE_IS_DISK( aTable->mTableRef->tableInfo->tableFlag ) !=
             QCM_TABLE_TYPE_IS_DISK( sSelectedPartitionInfo->tableFlag ) )
        {
            /* PROJ-2464 hybrid partitioned table ����
             * Partitioned Table�� �������� ���� smiValue Array�� Table Partition�� �°� ��ȯ�Ѵ�.
             */
            IDE_TEST( qmx::makeSmiValueWithSmiValue( aTable->mTableRef->tableInfo,
                                                     sSelectedPartitionInfo,
                                                     aTable->mTableRef->tableInfo->columns,
                                                     aTable->mTableRef->tableInfo->columnCount,
                                                     sDataTable->mInsertValues,
                                                     sValuesForPartition )
                      != IDE_SUCCESS );

            sSmiValuesForPartition = sValuesForPartition;
        }
        else
        {
            sSmiValuesForPartition = sDataTable->mInsertValues;
        }

        IDE_TEST( sCursor->insertRow( sSmiValuesForPartition,
                                      & sRow,
                                      & aDataPlan->rowGRID )
                  != IDE_SUCCESS );

        IDE_TEST( sDataTable->mInsertCursorMgr.getSelectedPartitionOID(
                      & sPartOID )
                  != IDE_SUCCESS );

        IDE_TEST( updateIndexTableCursorRowMoveMultiTable( aTemplate,
                                                           aDataPlan,
                                                           aTable,
                                                           aIndex,
                                                           sPartOID,
                                                           aDataPlan->rowGRID,
                                                           sSmiValues )
                  != IDE_SUCCESS );

        IDE_TEST( qmx::copyAndOutBindLobInfo( aTemplate->stmt,
                                              sDataTable->mInsertLobInfo,
                                              sCursor,
                                              sRow,
                                              aDataPlan->rowGRID )
                  != IDE_SUCCESS );
        // PROJ-2204 join update, delete
        // tuple ������ cursor�� �����ؾ��Ѵ�.
        if ( aTable->mTableRef->view != NULL )
        {
            IDE_TEST( aDataPlan->indexUpdateCursor->setRowPosition(
                          aDataPlan->indexUpdateTuple->row,
                          aDataPlan->indexUpdateTuple->rid )
                      != IDE_SUCCESS );
        }
        else
        {
            // Nothing to do.
        }

        // delete row
        IDE_TEST( aDataPlan->updateCursor->deleteRow()
                  != IDE_SUCCESS );

        /* PROJ-2464 hybrid partitioned table ���� */
        IDE_TEST( sDataTable->mInsertCursorMgr.getSelectedPartitionTupleID( &sPartitionTupleID )
                  != IDE_SUCCESS );
        sSelectedPartitionTuple = &(aTemplate->tmplate.rows[sPartitionTupleID]);

        if ( ( sDataTable->mNeedTriggerRow == ID_TRUE ) ||
             ( aTable->mCheckConstrList != NULL ) )
        {
            // NEW ROW�� ȹ��
            IDE_TEST( sCursor->getLastModifiedRow(
                          & sDataTable->mNewRow,
                          sSelectedPartitionTuple->rowOffset )
                      != IDE_SUCCESS );

            IDE_TEST( sDataTable->mInsertCursorMgr.setColumnsForNewRow()
                      != IDE_SUCCESS );

            IDE_TEST( sDataTable->mInsertCursorMgr.getColumnsForNewRow(
                          &sColumnsForNewRow )
                      != IDE_SUCCESS );
        }
        else
        {
            /* Nothing to do */
        }
    }
    else
    {
        /* PROJ-2464 hybrid partitioned table ���� */
        sSelectedPartitionTuple = &(aTemplate->tmplate.rows[sSelectedPartitionRef->table]);

        if ( sSelectedPartitionRef->table == aDataPlan->updateTupleID )
        {
            //-----------------------------------
            // tableRef�� �ְ� select�� partition�� ���
            //-----------------------------------

            if ( QCM_TABLE_TYPE_IS_DISK( aTable->mTableRef->tableInfo->tableFlag ) !=
                 QCM_TABLE_TYPE_IS_DISK( aDataPlan->updatePartInfo->tableFlag ) )
            {
                /* PROJ-2464 hybrid partitioned table ����
                 * Partitioned Table�� �������� ���� smiValue Array�� Table Partition�� �°� ��ȯ�Ѵ�.
                 */
                IDE_TEST( qmx::makeSmiValueWithSmiValue( aTable->mTableRef->tableInfo,
                                                         aDataPlan->updatePartInfo,
                                                         aTable->mColumns,
                                                         aTable->mColumnCount,
                                                         sSmiValues,
                                                         sValuesForPartition )
                          != IDE_SUCCESS );
                sSmiValuesForPartition = sValuesForPartition;
            }
            else
            {
                sSmiValuesForPartition = sSmiValues;
            }

            if ( aTable->mTableRef->view != NULL )
            {
                IDE_TEST( aDataPlan->indexUpdateCursor->setRowPosition(
                              aDataPlan->indexUpdateTuple->row,
                              aDataPlan->indexUpdateTuple->rid )
                          != IDE_SUCCESS );
            }
            else
            {
                // Nothing to do.
            }
            IDE_TEST( aDataPlan->updateCursor->updateRow( sSmiValuesForPartition,
                                                          NULL,
                                                          & sRow,
                                                          & aDataPlan->rowGRID )
                      != IDE_SUCCESS );

            sPartOID = sSelectedPartitionRef->partitionOID;

            IDE_TEST( updateIndexTableCursorRowMoveMultiTable( aTemplate,
                                                               aDataPlan,
                                                               aTable,
                                                               aIndex,
                                                               sPartOID,
                                                               aDataPlan->rowGRID,
                                                               sSmiValues )
                      != IDE_SUCCESS );

            /* PROJ-2464 hybrid partitioned table ����
             *  Disk Partition�� ���, Disk Type�� Lob Column�� �ʿ��ϴ�.
             *  Memory/Volatile Partition�� ���, �ش� Partition�� Lob Column�� �ʿ��ϴ�.
             */
            if ( ( QCM_TABLE_TYPE_IS_DISK( aTable->mTableRef->tableInfo->tableFlag ) !=
                   QCM_TABLE_TYPE_IS_DISK( aDataPlan->updatePartInfo->tableFlag ) ) ||
                 ( QCM_TABLE_TYPE_IS_DISK( aDataPlan->updatePartInfo->tableFlag ) != ID_TRUE ) )
            {
                IDE_TEST( qmx::changeLobColumnInfo( sDataTable->mLobInfo,
                                                    aDataPlan->updatePartInfo->columns )
                          != IDE_SUCCESS );
            }
            else
            {
                /* Nothing to do */
            }

            // Lob�÷� ó��
            IDE_TEST( qmx::copyAndOutBindLobInfo( aTemplate->stmt,
                                                  sDataTable->mLobInfo,
                                                  aDataPlan->updateCursor,
                                                  sRow,
                                                  aDataPlan->rowGRID )
                      != IDE_SUCCESS );

            if ( ( sDataTable->mNeedTriggerRow == ID_TRUE ) ||
                 ( aTable->mCheckConstrList != NULL ) )
            {
                // NEW ROW�� ȹ��
                IDE_TEST( aDataPlan->updateCursor->getLastModifiedRow(
                              & sDataTable->mNewRow,
                              sSelectedPartitionTuple->rowOffset )
                          != IDE_SUCCESS );

                sColumnsForNewRow = sSelectedPartitionRef->partitionInfo->columns;
            }
            else
            {
                // Nothing to do.
            }
        }
        else
        {
            //-----------------------------------
            // tableRef�� �ִ� �ٸ� partition�� ���
            // insert row -> update row
            //-----------------------------------

            /* PROJ-2359 Table/Partition Access Option */
            IDE_TEST( qmx::checkAccessOption( sSelectedPartitionRef->partitionInfo,
                                              ID_TRUE, /* aIsInsertion */
                                              QCG_CHECK_SHARD_DML_CONSISTENCY( aTemplate->stmt ) )
                      != IDE_SUCCESS );

            /* PROJ-1090 Function-based Index */
            if ( aTable->mDefaultColumns != NULL )
            {
                IDE_TEST( qmsDefaultExpr::setRowBufferFromSmiValueArray(
                              &(aTemplate->tmplate),
                              aTable->mDefaultTableRef,
                              aTable->mTableRef->tableInfo->columns,
                              sDataTable->mDefaultExprRowBuffer,
                              sDataTable->mInsertValues,
                              QCM_TABLE_TYPE_IS_DISK( aTable->mTableRef->tableInfo->tableFlag ) )
                          != IDE_SUCCESS );

                IDE_TEST( qmsDefaultExpr::calculateDefaultExpression(
                              aTemplate,
                              aTable->mDefaultTableRef,
                              NULL,
                              aTable->mDefaultColumns,
                              sDataTable->mDefaultExprRowBuffer,
                              sDataTable->mInsertValues,
                              aTable->mTableRef->tableInfo->columns )
                          != IDE_SUCCESS );
            }
            else
            {
                /* Nothing to do */
            }

            // partition�� cursor�� ��´�.
            sCursorInfo = (qmnCursorInfo*)
                aTemplate->tmplate.rows[sSelectedPartitionRef->table].cursorInfo;

            IDE_TEST_RAISE( sCursorInfo == NULL, ERR_NOT_FOUND );

            // insert row
            sInsertCursor = sCursorInfo->cursor;

            if ( QCM_TABLE_TYPE_IS_DISK( aTable->mTableRef->tableInfo->tableFlag ) !=
                 QCM_TABLE_TYPE_IS_DISK( sSelectedPartitionRef->partitionInfo->tableFlag ) )
            {
                IDE_TEST( qmx::makeSmiValueWithSmiValue( aTable->mTableRef->tableInfo,
                                                         sSelectedPartitionRef->partitionInfo,
                                                         aTable->mTableRef->tableInfo->columns,
                                                         aTable->mTableRef->tableInfo->columnCount,
                                                         sDataTable->mInsertValues,
                                                         sValuesForPartition )
                          != IDE_SUCCESS );

                sSmiValuesForPartition = sValuesForPartition;
            }
            else
            {
                sSmiValuesForPartition = sDataTable->mInsertValues;
            }

            IDE_TEST( sInsertCursor->insertRow( sSmiValuesForPartition,
                                                & sRow,
                                                & aDataPlan->rowGRID )
                      != IDE_SUCCESS );
            /* PROJ-2464 hybrid partitioned table ����
             *  Disk Partition�� ���, Disk Type�� Lob Column�� �ʿ��ϴ�.
             *  Memory/Volatile Partition�� ���, �ش� Partition�� Lob Column�� �ʿ��ϴ�.
             */
            if ( ( QCM_TABLE_TYPE_IS_DISK( aTable->mTableRef->tableInfo->tableFlag ) !=
                   QCM_TABLE_TYPE_IS_DISK( sSelectedPartitionRef->partitionInfo->tableFlag ) ) ||
                 ( QCM_TABLE_TYPE_IS_DISK( sSelectedPartitionRef->partitionInfo->tableFlag ) != ID_TRUE ) )
            {
                IDE_TEST( qmx::changeLobColumnInfo( sDataTable->mInsertLobInfo,
                                                    sSelectedPartitionRef->partitionInfo->columns )
                          != IDE_SUCCESS );
            }
            else
            {
                /* Nothing to do */
            }

            sPartOID = sSelectedPartitionRef->partitionOID;

            IDE_TEST( updateIndexTableCursorRowMoveMultiTable( aTemplate,
                                                               aDataPlan,
                                                               aTable,
                                                               aIndex,
                                                               sPartOID,
                                                               aDataPlan->rowGRID,
                                                               sSmiValues )
                      != IDE_SUCCESS );

            IDE_TEST( qmx::copyAndOutBindLobInfo( aTemplate->stmt,
                                                  sDataTable->mInsertLobInfo,
                                                  sInsertCursor,
                                                  sRow,
                                                  aDataPlan->rowGRID )
                      != IDE_SUCCESS );
            // PROJ-2204 join update, delete
            // tuple ������ cursor�� �����ؾ��Ѵ�.
            if ( aTable->mTableRef->view != NULL )
            {
                IDE_TEST( aDataPlan->indexUpdateCursor->setRowPosition(
                              aDataPlan->indexUpdateTuple->row,
                              aDataPlan->indexUpdateTuple->rid )
                          != IDE_SUCCESS );
            }
            else
            {
                // Nothing to do.
            }

            // delete row
            IDE_TEST( aDataPlan->updateCursor->deleteRow()
                      != IDE_SUCCESS );

            if ( ( sDataTable->mNeedTriggerRow == ID_TRUE ) ||
                 ( aTable->mCheckConstrList != NULL ) )
            {
                // NEW ROW�� ȹ��
                IDE_TEST( sInsertCursor->getModifiedRow(
                              & sDataTable->mNewRow,
                              sSelectedPartitionTuple->rowOffset,
                              sRow,
                              aDataPlan->rowGRID )
                          != IDE_SUCCESS );

                sColumnsForNewRow = sSelectedPartitionRef->partitionInfo->columns;
            }
            else
            {
                /* Nothing to do */
            }
        }
    }

    /* PROJ-2464 hybrid partitioned table ���� */
    if ( ( aTable->mTableRef->partitionSummary->isHybridPartitionedTable == ID_TRUE ) ||
         ( aTable->mInsertTableRef->partitionSummary->isHybridPartitionedTable == ID_TRUE ) )
    {
        qmx::copyMtcTupleForPartitionDML( &sCopyTuple, aDataPlan->updateTuple );
        sNeedToRecoverTuple = ID_TRUE;

        qmx::adjustMtcTupleForPartitionDML( aDataPlan->updateTuple, sSelectedPartitionTuple );
    }
    else
    {
        /* Nothing to do */
    }

    //-----------------------------------
    // check constraint
    //-----------------------------------

    /* PROJ-1107 Check Constraint ���� */
    if ( aTable->mCheckConstrList != NULL )
    {
        sOrgRow = aDataPlan->updateTuple->row;
        aDataPlan->updateTuple->row = sDataTable->mNewRow;

        IDE_TEST( qdnCheck::verifyCheckConstraintList(
                      aTemplate,
                      aTable->mCheckConstrList )
                  != IDE_SUCCESS );

        aDataPlan->updateTuple->row = sOrgRow;
    }
    else
    {
        // Nothing to do.
    }

    //-----------------------------------
    // update after trigger
    //-----------------------------------
    if ( sDataTable->mExistTrigger == ID_TRUE )
    {
        // PROJ-1359 Trigger
        // ROW GRANULARITY TRIGGER�� ����
        IDE_TEST( qdnTrigger::fireTrigger( aTemplate->stmt,
                                           aTemplate->stmt->qmxMem,
                                           aTable->mTableRef->tableInfo,
                                           QCM_TRIGGER_ACTION_EACH_ROW,
                                           QCM_TRIGGER_AFTER,
                                           QCM_TRIGGER_EVENT_UPDATE,
                                           aTable->mUptColumnList,
                                           aDataPlan->updateCursor,         /* Table Cursor */
                                           aDataPlan->updateTuple->rid,     /* Row GRID */
                                           sDataTable->mOldRow,
                                           aDataPlan->updatePartInfo->columns,
                                           sDataTable->mNewRow,
                                           sColumnsForNewRow )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    if ( ( *aDataPlan->flag & QMND_UPTE_UPDATE_MASK )
         == QMND_UPTE_UPDATE_FALSE )
    {
        *aDataPlan->flag &= ~QMND_UPTE_UPDATE_MASK;
        *aDataPlan->flag |= QMND_UPTE_UPDATE_TRUE;
    }
    else
    {
        // Nothing to do.
    }

    /* PROJ-2464 hybrid partitioned table ���� */
    if ( sNeedToRecoverTuple == ID_TRUE )
    {
        sNeedToRecoverTuple = ID_FALSE;
        qmx::copyMtcTupleForPartitionDML( aDataPlan->updateTuple, &sCopyTuple );
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NOT_FOUND )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmnUPTE::updateOneRowForRowmovementMultiTable",
                                  "cursor not found" ));
    }
    IDE_EXCEPTION_END;

    /* PROJ-2464 hybrid partitioned table ���� */
    if ( sNeedToRecoverTuple == ID_TRUE )
    {
        qmx::copyMtcTupleForPartitionDML( aDataPlan->updateTuple, &sCopyTuple );
    }
    else
    {
        /* Nothing to do */
    }
    return IDE_FAILURE;
}

IDE_RC qmnUPTE::checkUpdateParentRefMultiTable( qcTemplate     * aTemplate,
                                                qmnPlan        * aPlan,
                                                qmmMultiTables * aTable,
                                                UInt             aIndex )
{
    qmndUPTE            * sDataPlan;
    iduMemoryStatus       sQmxMemStatus;
    void                * sOrgRow;
    void                * sSearchRow;
    qmsPartitionRef     * sPartitionRef;
    qmcInsertPartCursor * sInsertPartCursor;
    qmndMultiTables     * sDataTable;
    UInt                  i;

    sDataPlan = (qmndUPTE*) ( aTemplate->tmplate.data + aPlan->offset );
    sDataPlan->updateTuple = &aTemplate->tmplate.rows[aTable->mTableRef->table];
    sDataTable = &sDataPlan->mTableArray[aIndex];

    //------------------------------------------
    // UPDATE�� �ο� �˻��� ����,
    // ���ſ����� ����� ù��° row ���� ��ġ�� cursor ��ġ ����
    //------------------------------------------
    if ( aTable->mParentConst != NULL )
    {
        if ( aTable->mTableRef->partitionRef == NULL )
        {
            IDE_TEST( checkUpdateParentRefOnScanMultiTable(
                        aTemplate,
                        aTable,
                        &aTemplate->tmplate.rows[aTable->mTableRef->table] )
                      != IDE_SUCCESS );
        }
        else
        {
            for ( sPartitionRef = aTable->mTableRef->partitionRef;
                  sPartitionRef != NULL;
                  sPartitionRef = sPartitionRef->next )
            {
                IDE_TEST( checkUpdateParentRefOnScanMultiTable(
                              aTemplate,
                              aTable,
                              &aTemplate->tmplate.rows[sPartitionRef->table] )
                          != IDE_SUCCESS );
            }
        }
    }
    else
    {
        // Nothing to do.
    }

    //------------------------------------------
    // INSERT�� �ο� �˻��� ����,
    // ���ſ����� ����� ù��° row ���� ��ġ�� cursor ��ġ ����
    //------------------------------------------
    if ( ( aTable->mParentConst != NULL ) &&
         ( aTable->mUpdateType == QMO_UPDATE_ROWMOVEMENT ) )
    {
        for ( i = 0; i < sDataTable->mInsertCursorMgr.mCursorIndexCount; i++ )
        {
            sInsertPartCursor = sDataTable->mInsertCursorMgr.mCursorIndex[i];

            sOrgRow = sSearchRow = sDataPlan->updateTuple->row;

            //------------------------------------------
            // ���� ��ü�� ���� ���� Ȯ�� �� �÷� ���� ����
            //------------------------------------------
            IDE_TEST( sInsertPartCursor->cursor.beforeFirstModified( SMI_FIND_MODIFIED_NEW )
                      != IDE_SUCCESS );

            //------------------------------------------
            // ����� Row�� �ݺ������� �о� Referecing �˻縦 ��
            //------------------------------------------
            IDE_TEST( sInsertPartCursor->cursor.readNewRow( (const void **) & sSearchRow,
                                                            & sDataPlan->updateTuple->rid )
                      != IDE_SUCCESS);

            sDataPlan->updateTuple->row = ( sSearchRow == NULL ) ? sOrgRow : sSearchRow;

            while ( sSearchRow != NULL )
            {
                // Memory ������ ���Ͽ� ���� ��ġ ���
                IDE_TEST_RAISE( aTemplate->stmt->qmxMem->getStatus(&sQmxMemStatus) != IDE_SUCCESS, ERR_MEM_OP );

                //------------------------------------------
                // Master Table�� ���� Referencing �˻�
                //------------------------------------------
                IDE_TEST( qdnForeignKey::checkParentRef(
                              aTemplate->stmt,
                              aTable->mColumnIDs,
                              aTable->mParentConst,
                              sDataPlan->updateTuple,
                              sDataPlan->updateTuple->row,
                              aTable->mColumnCount )
                          != IDE_SUCCESS );

                // Memory ������ ���� Memory �̵�
                IDE_TEST_RAISE( aTemplate->stmt->qmxMem->setStatus(&sQmxMemStatus) != IDE_SUCCESS, ERR_MEM_OP );

                sOrgRow = sSearchRow = sDataPlan->updateTuple->row;

                IDE_TEST( sInsertPartCursor->cursor.readNewRow(
                              (const void **) &sSearchRow,
                              & sDataPlan->updateTuple->rid )
                          != IDE_SUCCESS);

                sDataPlan->updateTuple->row = ( sSearchRow == NULL ) ? sOrgRow : sSearchRow;
            }
        }
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
                     "qmnUPTE::checkUpdateParentRefMultiTable"
                     "memory error" );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmnUPTE::checkUpdateChildRefMultiTable( qcTemplate     * aTemplate,
                                               qmnPlan        * /*aPlan*/,
                                               qmmMultiTables * aTable )
{
    qmsPartitionRef * sPartitionRef;
    smiStatement      sSmiStmt;
    smiStatement    * sSmiStmtOrg;
    UInt              sStage = 0;

    if ( aTable->mChildConst != NULL )
    {
        // BUG-17940 parent key�� �����ϰ� child key�� ã����
        // parent row�� lock�� ���� ���� view�� ��������
        // ���ο� smiStmt�� �̿��Ѵ�.
        // Update cascade �ɼǿ� ����ؼ� normal�� �Ѵ�.
        // child table�� Ÿ���� ���� �� �� ���� ������ ALL CURSOR�� �Ѵ�.
        qcg::getSmiStmt( aTemplate->stmt, & sSmiStmtOrg );

        IDE_TEST( sSmiStmt.begin( aTemplate->stmt->mStatistics,
                                  QC_SMI_STMT( aTemplate->stmt ),
                                  SMI_STATEMENT_NORMAL |
                                  SMI_STATEMENT_SELF_TRUE |
                                  SMI_STATEMENT_ALL_CURSOR )
                  != IDE_SUCCESS );
        qcg::setSmiStmt( aTemplate->stmt, & sSmiStmt );

        sStage = 1;

        if ( aTable->mTableRef->partitionRef == NULL )
        {
            IDE_TEST( checkUpdateChildRefOnScanMultiTable(
                        aTemplate,
                        aTable,
                        aTable->mTableRef->tableInfo,
                        &aTemplate->tmplate.rows[aTable->mTableRef->table] )
                      != IDE_SUCCESS );
        }
        else
        {
            for ( sPartitionRef = aTable->mTableRef->partitionRef;
                  sPartitionRef != NULL;
                  sPartitionRef = sPartitionRef->next )
            {
                IDE_TEST( checkUpdateChildRefOnScanMultiTable(
                              aTemplate,
                              aTable,
                              sPartitionRef->partitionInfo,
                              &aTemplate->tmplate.rows[sPartitionRef->table] )
                          != IDE_SUCCESS );
            }
        }

        sStage = 0;

        qcg::setSmiStmt( aTemplate->stmt, sSmiStmtOrg );
        IDE_TEST( sSmiStmt.end(SMI_STATEMENT_RESULT_SUCCESS) != IDE_SUCCESS);
    }
    else
    {
        // Nothing to do.
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sStage == 1 )
    {
        qcg::setSmiStmt( aTemplate->stmt, sSmiStmtOrg );

        if (sSmiStmt.end(SMI_STATEMENT_RESULT_FAILURE) != IDE_SUCCESS)
        {
            IDE_CALLBACK_FATAL("Check Child Key On Update smiStmt.end() failed");
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
    return IDE_FAILURE;
}

IDE_RC qmnUPTE::checkUpdateParentRefOnScanMultiTable( qcTemplate     * aTemplate,
                                                      qmmMultiTables * aTable,
                                                      mtcTuple       * aUpdateTuple )
{
    iduMemoryStatus   sQmxMemStatus;
    void            * sOrgRow;
    void            * sSearchRow;
    smiTableCursor  * sUpdateCursor;
    qmnCursorInfo   * sCursorInfo;

    //------------------------------------------
    // UPDATE�� �ο� �˻��� ����,
    // ���ſ����� ����� ù��° row ���� ��ġ�� cursor ��ġ ����
    //------------------------------------------

    sCursorInfo = (qmnCursorInfo*) aUpdateTuple->cursorInfo;

    IDE_TEST_RAISE( sCursorInfo == NULL, ERR_NOT_FOUND );

    sUpdateCursor = sCursorInfo->cursor;

    // BUG-37147 sUpdateCursor �� null �� ��찡 �߻���
    // PROJ-1624 non-partitioned index
    // index table scan���� open���� ���� partition�� �����Ѵ�.
    if ( sUpdateCursor != NULL )
    {
        sOrgRow = sSearchRow = aUpdateTuple->row;

        IDE_TEST( sUpdateCursor->beforeFirstModified( SMI_FIND_MODIFIED_NEW )
                  != IDE_SUCCESS );

        IDE_TEST( sUpdateCursor->readNewRow( (const void**) & sSearchRow,
                                             & aUpdateTuple->rid )
                  != IDE_SUCCESS );

        //------------------------------------------
        // Referencing �˻縦 ���� ������ Row���� �˻�
        //------------------------------------------

        aUpdateTuple->row = ( sSearchRow == NULL ) ? sOrgRow : sSearchRow;

        while ( sSearchRow != NULL )
        {
            // Memory ������ ���Ͽ� ���� ��ġ ���
            IDE_TEST_RAISE( aTemplate->stmt->qmxMem->getStatus(&sQmxMemStatus) != IDE_SUCCESS, ERR_MEM_OP );

            //------------------------------------------
            // Child Table�� ���� Referencing �˻�
            //------------------------------------------
            IDE_TEST( qdnForeignKey::checkParentRef(
                        aTemplate->stmt,
                        aTable->mColumnIDs,
                        aTable->mParentConst,
                        aUpdateTuple,
                        aUpdateTuple->row,
                        aTable->mColumnCount )
                    != IDE_SUCCESS );

            // Memory ������ ���� Memory �̵�
            IDE_TEST_RAISE( aTemplate->stmt->qmxMem->setStatus(&sQmxMemStatus) != IDE_SUCCESS, ERR_MEM_OP );

            sOrgRow = sSearchRow = aUpdateTuple->row;

            IDE_TEST( sUpdateCursor->readNewRow( (const void**) & sSearchRow,
                                                 & aUpdateTuple->rid )
                      != IDE_SUCCESS );

            aUpdateTuple->row = ( sSearchRow == NULL ) ? sOrgRow : sSearchRow;
        }
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
                     "qmnUPTE::checkUpdateParentRefOnScanMultiTable"
                     "memory error" );
    }
    IDE_EXCEPTION( ERR_NOT_FOUND )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmnUPTE::checkUpdateParentRefOnScanMultiTable",
                                  "cursor not found" ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmnUPTE::checkUpdateChildRefOnScanMultiTable( qcTemplate     * aTemplate,
                                                     qmmMultiTables * aTable,
                                                     qcmTableInfo   * aTableInfo,
                                                     mtcTuple       * aUpdateTuple )
{
    iduMemoryStatus   sQmxMemStatus;
    void            * sOrgRow;
    void            * sSearchRow;
    smiTableCursor  * sUpdateCursor;
    qmnCursorInfo   * sCursorInfo;

    //------------------------------------------
    // UPDATE�� �ο� �˻��� ����,
    // ���ſ����� ����� ù��° row ���� ��ġ�� cursor ��ġ ����
    //------------------------------------------
    sCursorInfo = (qmnCursorInfo*) aUpdateTuple->cursorInfo;

    IDE_TEST_RAISE( sCursorInfo == NULL, ERR_NOT_FOUND );

    sUpdateCursor = sCursorInfo->cursor;

    // PROJ-1624 non-partitioned index
    // index table scan���� open���� ���� partition�� �����Ѵ�.
    if ( sUpdateCursor != NULL )
    {
        IDE_TEST( sUpdateCursor->beforeFirstModified( SMI_FIND_MODIFIED_OLD )
                  != IDE_SUCCESS );

        //------------------------------------------
        // Referencing �˻縦 ���� ������ Row���� �˻�
        //------------------------------------------

        sOrgRow = sSearchRow = aUpdateTuple->row;
        IDE_TEST( sUpdateCursor->readOldRow( (const void**) & sSearchRow,
                                             &aUpdateTuple->rid )
                  != IDE_SUCCESS );

        aUpdateTuple->row = ( sSearchRow == NULL ) ? sOrgRow : sSearchRow;

        while ( sSearchRow != NULL )
        {
            // Memory ������ ���Ͽ� ���� ��ġ ���
            IDE_TEST_RAISE( aTemplate->stmt->qmxMem->getStatus(&sQmxMemStatus) != IDE_SUCCESS, ERR_MEM_OP );

            //------------------------------------------
            // Child Table�� ���� Referencing �˻�
            //------------------------------------------
            IDE_TEST( qdnForeignKey::checkChildRefOnUpdate(
                          aTemplate->stmt,
                          aTable->mTableRef,
                          aTableInfo,
                          aTable->mColumnIDs,
                          aTable->mChildConst,
                          aTableInfo->tableID,
                          aUpdateTuple,
                          aUpdateTuple->row,
                          aTable->mColumnCount )
                      != IDE_SUCCESS );

            // Memory ������ ���� Memory �̵�
            IDE_TEST_RAISE( aTemplate->stmt->qmxMem->setStatus(&sQmxMemStatus) != IDE_SUCCESS, ERR_MEM_OP );

            sOrgRow = sSearchRow = aUpdateTuple->row;

            IDE_TEST( sUpdateCursor->readOldRow( (const void**) & sSearchRow,
                                                 &aUpdateTuple->rid )
                      != IDE_SUCCESS );

            aUpdateTuple->row = ( sSearchRow == NULL ) ? sOrgRow : sSearchRow;
        }
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
                     "qmnUPTE::checkUpdateChildRefOnScanMultiTable"
                     "memory error" );
    }
    IDE_EXCEPTION( ERR_NOT_FOUND )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmnUPTE::checkUpdateChildRefOnScanMultiTable",
                                  "cursor not found" ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

