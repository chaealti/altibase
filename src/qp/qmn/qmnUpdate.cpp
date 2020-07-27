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
 *     관계형 모델에서 update를 수행하는 Plan Node 이다.
 *
 * 용어 설명 :
 *
 * 약어 :
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
 *    UPTE 노드의 초기화
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnUPTE::init"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmnUPTE::init"));

    qmncUPTE * sCodePlan = (qmncUPTE*) aPlan;
    qmndUPTE * sDataPlan =
        (qmndUPTE*) (aTemplate->tmplate.data + aPlan->offset);

    sDataPlan->flag = & aTemplate->planFlag[sCodePlan->planID];
    sDataPlan->doIt = qmnUPTE::doItDefault;

    //------------------------------------------------
    // 최초 초기화 수행 여부 판단
    //------------------------------------------------

    if ( ( *sDataPlan->flag & QMND_UPTE_INIT_DONE_MASK )
         == QMND_UPTE_INIT_DONE_FALSE )
    {
        // 최초 초기화 수행
        IDE_TEST( firstInit(aTemplate, sCodePlan, sDataPlan) != IDE_SUCCESS );

        //------------------------------------------------
        // Child Plan의 초기화
        //------------------------------------------------

        IDE_TEST( aPlan->left->init( aTemplate,
                                     aPlan->left ) != IDE_SUCCESS);

        //---------------------------------
        // trigger row를 생성
        //---------------------------------

        // child의 offset을 이용하므로 firstInit이 끝나야 offset을 이용할 수 있다.
        IDE_TEST( allocTriggerRow(aTemplate, sCodePlan, sDataPlan)
                  != IDE_SUCCESS );

        //---------------------------------
        // returnInto row를 생성
        //---------------------------------

        IDE_TEST( allocReturnRow(aTemplate, sCodePlan, sDataPlan)
                  != IDE_SUCCESS );

        //---------------------------------
        // index table cursor를 생성
        //---------------------------------

        IDE_TEST( allocIndexTableCursor(aTemplate, sCodePlan, sDataPlan)
                  != IDE_SUCCESS );

        //---------------------------------
        // 초기화 완료를 표기
        //---------------------------------

        *sDataPlan->flag &= ~QMND_UPTE_INIT_DONE_MASK;
        *sDataPlan->flag |= QMND_UPTE_INIT_DONE_TRUE;
    }
    else
    {
        //------------------------------------------------
        // Child Plan의 초기화
        //------------------------------------------------

        IDE_TEST( aPlan->left->init( aTemplate,
                                     aPlan->left ) != IDE_SUCCESS);

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

    //------------------------------------------------
    // 가변 Data 의 초기화
    //------------------------------------------------

    // Limit 시작 개수의 초기화
    sDataPlan->limitCurrent = 1;

    // update rowGRID 초기화
    sDataPlan->rowGRID = SC_NULL_GRID;
    
    //------------------------------------------------
    // 수행 함수 결정
    //------------------------------------------------

    sDataPlan->doIt = qmnUPTE::doItFirst;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnUPTE::doIt( qcTemplate * aTemplate,
               qmnPlan    * aPlan,
               qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    UPTE 의 고유 기능을 수행한다.
 *
 * Implementation :
 *    지정된 함수 포인터를 수행한다.
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
 *    UPTE 노드는 별도의 null row를 가지지 않으며,
 *    Child에 대하여 padNull()을 호출한다.
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
        // 초기화되지 않은 경우 초기화 수행
        IDE_TEST( aPlan->init( aTemplate, aPlan ) != IDE_SUCCESS );
    }
    else
    {
        // Nothing To Do
    }

    // Child Plan에 대하여 Null Padding수행
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
 *    UPTE 노드의 수행 정보를 출력한다.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnUPTE::printPlan"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmnUPTE::printPlan"));

    qmncUPTE * sCodePlan = (qmncUPTE*) aPlan;
    qmndUPTE * sDataPlan =
        (qmndUPTE*) (aTemplate->tmplate.data + aPlan->offset);

    ULong      i;
    qmmValueNode * sValue;

    sDataPlan->flag = & aTemplate->planFlag[sCodePlan->planID];

    //------------------------------------------------------
    // 시작 정보의 출력
    //------------------------------------------------------

    for ( i = 0; i < aDepth; i++ )
    {
        iduVarStringAppend( aString,
                            " " );
    }

    //------------------------------------------------------
    // UPTE Target 정보의 출력
    //------------------------------------------------------

    // UPTE 정보의 출력
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
    // Table Name 출력
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
    // Alias Name 출력
    //----------------------------

    if ( sCodePlan->aliasName.name != NULL &&
         sCodePlan->aliasName.size > 0  &&
         sCodePlan->aliasName.name != sCodePlan->tableName.name )
    {
        // Table 이름 정보와 Alias 이름 정보가 다를 경우
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
        // Alias 이름 정보가 없거나 Table 이름 정보가 동일한 경우
        // Nothing To Do
    }

    //----------------------------
    // New line 출력
    //----------------------------
    iduVarStringAppend( aString, " )\n" );

    //------------------------------------------------------
    // BUG-38343 Set 내부의 Subquery 정보 출력
    //------------------------------------------------------

    for ( sValue = sCodePlan->values;
          sValue != NULL;
          sValue = sValue->next)
    {
        IDE_TEST( qmn::printSubqueryPlan( aTemplate,
                                          sValue->value,
                                          aDepth,
                                          aString,
                                          aMode ) != IDE_SUCCESS );
    }

    //------------------------------------------------------
    // Child Plan 정보의 출력
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
 *    UPTE node의 Data 영역의 멤버에 대한 초기화를 수행
 *
 * Implementation :
 *    - Data 영역의 주요 멤버에 대한 초기화를 수행
 *
 ***********************************************************************/

#define IDE_FN "qmnUPTE::firstInit"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmnUPTE::firstInit"));

    ULong          sCount;
    UShort         sTableID;
    qcmTableInfo * sTableInfo;
    idBool         sIsNeedRebuild = ID_FALSE;

    //--------------------------------
    // 적합성 검사
    //--------------------------------

    //--------------------------------
    // UPTE 고유 정보의 초기화
    //--------------------------------

    aDataPlan->insertLobInfo = NULL;
    aDataPlan->insertValues  = NULL;
    aDataPlan->checkValues   = NULL;

    // Tuple Set정보의 초기화
    aDataPlan->updateTuple     = & aTemplate->tmplate.rows[aCodePlan->tableRef->table];
    aDataPlan->updateCursor    = NULL;
    aDataPlan->updateTupleID   = ID_USHORT_MAX;

    /* PROJ-2464 hybrid partitioned table 지원 */
    aDataPlan->updatePartInfo = NULL;

    // index table cursor 초기화
    aDataPlan->indexUpdateCursor = NULL;
    aDataPlan->indexUpdateTuple = NULL;

    // set, where column list 초기화
    smiInitDMLRetryInfo( &(aDataPlan->retryInfo) );

    /* PROJ-2359 Table/Partition Access Option */
    aDataPlan->accessOption = QCM_ACCESS_OPTION_READ_WRITE;

    //--------------------------------
    // cursorInfo 생성
    //--------------------------------

    if ( aCodePlan->insteadOfTrigger == ID_TRUE )
    {
        // instead of trigger는 cursor가 필요없다.
        // Nothing to do.
    }
    else
    {
        sTableInfo = aCodePlan->tableRef->tableInfo;

        // PROJ-2219 Row-level before update trigger
        // Invalid 한 trigger가 있으면 compile하고, DML을 rebuild 한다.
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
    // partition 관련 정보의 초기화
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
            // insert cursor manager 초기화
            IDE_TEST( aDataPlan->insertCursorMgr.initialize(
                          aTemplate->stmt->qmxMem,
                          aCodePlan->insertTableRef,
                          ID_FALSE )
                      != IDE_SUCCESS );

            // lob info 초기화
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

            // insert smiValues 초기화
            IDE_TEST( aTemplate->stmt->qmxMem->alloc(
                          aCodePlan->tableRef->tableInfo->columnCount
                          * ID_SIZEOF(smiValue),
                          (void**)& aDataPlan->insertValues )
                      != IDE_SUCCESS );

            break;
        }

        case QMO_UPDATE_CHECK_ROWMOVEMENT:
        {
            // check smiValues 초기화
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
    // Limitation 관련 정보의 초기화
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

    // 적합성 검사
    if ( aDataPlan->limitEnd > 0 )
    {
        IDE_ASSERT( (aCodePlan->flag & QMNC_UPTE_LIMIT_MASK)
                    == QMNC_UPTE_LIMIT_TRUE );
    }

    //------------------------------------------
    // Default Expr의 Row Buffer 구성
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
            /* Disk Table의 경우, qmc::setRowSize()에서 이미 할당 */
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
    // cursorInfo 생성
    //--------------------------------

    IDE_TEST( aTemplate->stmt->qmxMem->alloc( ID_SIZEOF(qmnCursorInfo),
                                              (void**)& sCursorInfo )
              != IDE_SUCCESS );

    // cursorInfo 초기화
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

    // cursorInfo 설정
    aDataPlan->updateTuple->cursorInfo = sCursorInfo;

    //--------------------------------
    // partition cursorInfo 생성
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

        // cursorInfo 생성
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
            // cursorInfo 초기화
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

            /* PROJ-2464 hybrid partitioned table 지원 */
            sCursorInfo->stmtRetryColLst     = aCodePlan->wherePartColumnList[i];
            sCursorInfo->rowRetryColLst      = aCodePlan->setPartColumnList[i];

            // cursorInfo 설정
            aTemplate->tmplate.rows[sPartitionRef->table].cursorInfo = sCursorInfo;
        }

        // PROJ-1624 non-partitioned index
        // partitioned table인 경우 index table cursor의 update column list를 지정한다.
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
                // update할 컬럼이 있는 경우
                sCursorInfo->updateColumnList = aDataPlan->indexUpdateColumnList;
                // index table은 항상 update, composite 없음
                sCursorInfo->cursorType       = SMI_UPDATE_CURSOR;

                // update해야함
                *aDataPlan->flag &= ~QMND_UPTE_SELECTED_INDEX_CURSOR_MASK;
                *aDataPlan->flag |= QMND_UPTE_SELECTED_INDEX_CURSOR_TRUE;
            }
            else
            {
                // update할 컬럼이 없는 경우
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
    // Trigger를 위한 공간을 마련
    //---------------------------------

    if ( aCodePlan->tableRef->tableInfo->triggerCount > 0 )
    {
        if ( aCodePlan->insteadOfTrigger == ID_TRUE )
        {
            // instead of trigger에서는 smiValues를 사용한다.

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

            // 적합성 검사
            IDE_DASSERT( sMaxRowOffsetForUpdate > 0 );

            // Old Row Referencing을 위한 공간 할당
            IDE_TEST( aTemplate->stmt->qmxMem->alloc(
                    sMaxRowOffsetForUpdate,
                    (void**) & aDataPlan->oldRow )
                != IDE_SUCCESS);

            // New Row Referencing을 위한 공간 할당
            IDE_TEST( aTemplate->stmt->qmxMem->cralloc(
                    IDL_MAX( sMaxRowOffsetForUpdate, sMaxRowOffsetForInsert ),
                    (void**) & aDataPlan->newRow )
                != IDE_SUCCESS);
        }

        aDataPlan->columnsForRow = aCodePlan->tableRef->tableInfo->columns;

        aDataPlan->needTriggerRow = ID_FALSE;
        aDataPlan->existTrigger = ID_TRUE;
    }
    else
    {
        // check constraint와 return into에서도 trigger row를 사용한다.
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

            // 적합성 검사
            IDE_DASSERT( sMaxRowOffsetForUpdate > 0 );

            // Old Row Referencing을 위한 공간 할당
            IDE_TEST( aTemplate->stmt->qmxMem->alloc(
                    sMaxRowOffsetForUpdate,
                    (void**) & aDataPlan->oldRow )
                != IDE_SUCCESS);

            // New Row Referencing을 위한 공간 할당
            IDE_TEST( aTemplate->stmt->qmxMem->cralloc(
                    IDL_MAX( sMaxRowOffsetForUpdate, sMaxRowOffsetForInsert ),
                    (void**) & aDataPlan->newRow )
                != IDE_SUCCESS);
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
    // return into를 위한 공간을 마련
    //---------------------------------

    if ( ( aCodePlan->returnInto != NULL ) &&
         ( aCodePlan->insteadOfTrigger == ID_TRUE ) )
    {
        sMaxRowOffset = qmx::getMaxRowOffset( &(aTemplate->tmplate),
                                              aCodePlan->tableRef );

        // New Row Referencing을 위한 공간 할당
        IDE_TEST( aTemplate->stmt->qmxMem->cralloc(
                sMaxRowOffset,
                (void**) & aDataPlan->returnRow )
            != IDE_SUCCESS);
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
    // index table 처리를 위한 정보
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
 *    이 함수가 수행되면 안됨.
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
 *    UPTE의 최초 수행 함수
 *
 * Implementation :
 *    - Table에 IX Lock을 건다.
 *    - Session Event Check (비정상 종료 Detect)
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
    // Child Plan을 수행함
    //-----------------------------------

    // To fix PR-3921
    if ( sDataPlan->limitCurrent == sDataPlan->limitEnd )
    {
        // 주어진 Limit 조건에 다다른 경우
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
        // Limit Start 처리
        for ( ;
              sDataPlan->limitCurrent < sDataPlan->limitStart;
              sDataPlan->limitCurrent++ )
        {
            // Limitation 범위에 들지 않는다.
            // 따라서 Update없이 Child를 수행하기만 한다.
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
                // Limit값 증가
                sDataPlan->limitCurrent++;
            }
            else
            {
                // Limitation 범위를 벗어난 경우
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
 *    UPTE의 다음 수행 함수
 *    다음 Record를 삭제한다.
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
    // Child Plan을 수행함
    //-----------------------------------

    // To fix PR-3921
    if ( sDataPlan->limitCurrent == sDataPlan->limitEnd )
    {
        // 주어진 Limit 조건에 다다른 경우
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
                // Limit값 증가
                sDataPlan->limitCurrent++;
            }
            else
            {
                // Limitation 범위를 벗어난 경우
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
            // record가 없는 경우
            // 다음 수행을 위해 최초 수행 함수로 설정함.
            sDataPlan->doIt = qmnUPTE::doItFirst;
        }
    }
    else
    {
        // record가 없는 경우
        // 다음 수행을 위해 최초 수행 함수로 설정함.
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
            // Trigger를 위한 Referencing Row가 필요한지를 검사
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
 *     하위 scan이 open한 cursor를 얻는다.
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

            // cursor를 얻는다.
            sCursorInfo = (qmnCursorInfo*)
                aTemplate->tmplate.rows[sDataPlan->updateTupleID].cursorInfo;

            IDE_TEST_RAISE( sCursorInfo == NULL, ERR_NOT_FOUND );

            sDataPlan->updateCursor    = sCursorInfo->cursor;

            /* PROJ-2464 hybrid partitioned table 지원 */
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

            // partition의 cursor를 얻는다.
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

            /* PROJ-2464 hybrid partitioned table 지원 */
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

        // index table cursor를 얻는다.
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
        // INSERT 를 위한 Cursor 구성
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
            // 포린키 체크를 위해 읽어야 할 패치컬럼리스트 생성
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
 *    UPTE 의 고유 기능을 수행한다.
 *
 * Implementation :
 *    - update one record 수행
 *    - trigger each row 수행
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

    /* PROJ-2464 hybrid partitioned table 지원
     * Memory인 경우, newRow에 새로운 주소를 할당한다. 따라서, newRow에 직접 접근하지 않는다.
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
        // OLD ROW REFERENCING을 위한 저장
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

    // Sequence Value 획득
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

    // subquery가 있을때 smi value 정보 구성
    IDE_TEST( qmx::makeSmiValueForSubquery( aTemplate,
                                            sCodePlan->tableRef->tableInfo,
                                            sCodePlan->columns,
                                            sCodePlan->subqueries,
                                            sCodePlan->canonizedTuple,
                                            sSmiValues,
                                            sCodePlan->isNull,
                                            sDataPlan->lobInfo )
              != IDE_SUCCESS );

    // 갱신할 smi value 정보 구성
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
        // 현재 trigger에서 lob column이 있는 table은 지원하지 않으므로
        // Table cursor는 NULL을 넘긴다.
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
        /* PROJ-2464 hybrid partitioned table 지원
         * Partitioned Table을 기준으로 만든 smiValue Array를 Table Partition에 맞게 변환한다.
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
        // tuple 원복시 cursor도 원복해야한다.
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
            // OLD ROW REFERENCING을 위한 저장
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

        // 갱신할 smi value 정보 구성
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
            /* PROJ-2464 hybrid partitioned table 지원
             * Partitioned Table을 기준으로 만든 smiValue Array를 Table Partition에 맞게 변환한다.
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

    /* PROJ-2464 hybrid partitioned table 지원
     *  Disk Partition인 경우, Disk Type의 Lob Column이 필요하다.
     *  Memory/Volatile Partition의 경우, 해당 Partition의 Lob Column이 필요하다.
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

    // Lob컬럼 처리
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
        // NEW ROW의 획득
        IDE_TEST( sDataPlan->updateCursor->getLastModifiedRow(
                      & sNewRow,
                      sDataPlan->updateTuple->rowOffset )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    /* PROJ-1107 Check Constraint 지원 */
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
        // ROW GRANULARITY TRIGGER의 수행
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
 *    UPTE 의 고유 기능을 수행한다.
 *
 * Implementation :
 *    - update one record 수행
 *    - trigger each row 수행
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
    
    /* PROJ-2464 hybrid partitioned table 지원
     * Memory인 경우, newRow에 새로운 주소를 할당한다. 따라서, newRow에 직접 접근하지 않는다.
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
        // OLD ROW REFERENCING을 위한 저장
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

    // Sequence Value 획득
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

    // subquery가 있을때 smi value 정보 구성
    IDE_TEST( qmx::makeSmiValueForSubquery( aTemplate,
                                            sCodePlan->tableRef->tableInfo,
                                            sCodePlan->columns,
                                            sCodePlan->subqueries,
                                            sCodePlan->canonizedTuple,
                                            sSmiValues,
                                            sCodePlan->isNull,
                                            sDataPlan->lobInfo )
              != IDE_SUCCESS );

    // 갱신할 smi value 정보 구성
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
        // 현재 trigger에서 lob column이 있는 table은 지원하지 않으므로
        // Table cursor는 NULL을 넘긴다.
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

    // row movement용 smi value 정보 구성
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
        // tableRef에 없는 partition인 경우
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
                                          ID_TRUE /* aIsInsertion */ )
                  != IDE_SUCCESS );

        // insert row
        IDE_TEST( sDataPlan->insertCursorMgr.getCursor( &sCursor )
                  != IDE_SUCCESS );

        if ( QCM_TABLE_TYPE_IS_DISK( sCodePlan->tableRef->tableInfo->tableFlag ) !=
             QCM_TABLE_TYPE_IS_DISK( sSelectedPartitionInfo->tableFlag ) )
        {
            /* PROJ-2464 hybrid partitioned table 지원
             * Partitioned Table을 기준으로 만든 smiValue Array를 Table Partition에 맞게 변환한다.
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
            // tuple 원복시 cursor도 원복해야한다.
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

        /* PROJ-2464 hybrid partitioned table 지원 */
        IDE_TEST( sDataPlan->insertCursorMgr.getSelectedPartitionTupleID( &sPartitionTupleID )
                  != IDE_SUCCESS );
        sSelectedPartitionTuple = &(aTemplate->tmplate.rows[sPartitionTupleID]);

        if ( ( sDataPlan->needTriggerRow == ID_TRUE ) ||
             ( sCodePlan->checkConstrList != NULL ) ||
             ( sCodePlan->returnInto != NULL ) )
        {
            // NEW ROW의 획득
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
        /* PROJ-2464 hybrid partitioned table 지원 */
        sSelectedPartitionTuple = &(aTemplate->tmplate.rows[sSelectedPartitionRef->table]);

        if ( sSelectedPartitionRef->table == sDataPlan->updateTupleID )
        {
            //-----------------------------------
            // tableRef에 있고 select한 partition인 경우
            //-----------------------------------

            if ( QCM_TABLE_TYPE_IS_DISK( sCodePlan->tableRef->tableInfo->tableFlag ) !=
                 QCM_TABLE_TYPE_IS_DISK( sDataPlan->updatePartInfo->tableFlag ) )
            {
                /* PROJ-2464 hybrid partitioned table 지원
                 * Partitioned Table을 기준으로 만든 smiValue Array를 Table Partition에 맞게 변환한다.
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
                // tuple 원복시 cursor도 원복해야한다.
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

            /* PROJ-2464 hybrid partitioned table 지원
             *  Disk Partition인 경우, Disk Type의 Lob Column이 필요하다.
             *  Memory/Volatile Partition의 경우, 해당 Partition의 Lob Column이 필요하다.
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

            // Lob컬럼 처리
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
                // NEW ROW의 획득
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
            // tableRef에 있는 다른 partition인 경우
            // insert row -> update row
            //-----------------------------------

            /* PROJ-2359 Table/Partition Access Option */
            IDE_TEST( qmx::checkAccessOption( sSelectedPartitionRef->partitionInfo,
                                              ID_TRUE /* aIsInsertion */ )
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

            // partition의 cursor를 얻는다.
            sCursorInfo = (qmnCursorInfo*)
                aTemplate->tmplate.rows[sSelectedPartitionRef->table].cursorInfo;

            IDE_TEST_RAISE( sCursorInfo == NULL, ERR_NOT_FOUND );

            // insert row
            sInsertCursor = sCursorInfo->cursor;

            if ( QCM_TABLE_TYPE_IS_DISK( sCodePlan->tableRef->tableInfo->tableFlag ) !=
                 QCM_TABLE_TYPE_IS_DISK( sSelectedPartitionRef->partitionInfo->tableFlag ) )
            {
                /* PROJ-2464 hybrid partitioned table 지원
                 * Partitioned Table을 기준으로 만든 smiValue Array를 Table Partition에 맞게 변환한다.
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
             * 선택된 파티션의 컬럼으로 LOB Column정보 변경 */
            /* PROJ-2464 hybrid partitioned table 지원
             *  Disk Partition인 경우, Disk Type의 Lob Column이 필요하다.
             *  Memory/Volatile Partition의 경우, 해당 Partition의 Lob Column이 필요하다.
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
                // tuple 원복시 cursor도 원복해야한다.
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
                // NEW ROW의 획득
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

    /* PROJ-2464 hybrid partitioned table 지원 */
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

    /* PROJ-1107 Check Constraint 지원 */
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
        // ROW GRANULARITY TRIGGER의 수행
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

    /* PROJ-2464 hybrid partitioned table 지원 */
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

    /* PROJ-2464 hybrid partitioned table 지원 */
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
 *    UPTE 의 고유 기능을 수행한다.
 *
 * Implementation :
 *    - update one record 수행
 *    - trigger each row 수행
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

    /* PROJ-2464 hybrid partitioned table 지원
     * Memory인 경우, newRow에 새로운 주소를 할당한다. 따라서, newRow에 직접 접근하지 않는다.
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
        // OLD ROW REFERENCING을 위한 저장
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

    // Sequence Value 획득
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

    // subquery가 있을때 smi value 정보 구성
    IDE_TEST( qmx::makeSmiValueForSubquery( aTemplate,
                                            sCodePlan->tableRef->tableInfo,
                                            sCodePlan->columns,
                                            sCodePlan->subqueries,
                                            sCodePlan->canonizedTuple,
                                            sSmiValues,
                                            sCodePlan->isNull,
                                            sDataPlan->lobInfo )
              != IDE_SUCCESS );

    // 갱신할 smi value 정보 구성
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
        // 현재 trigger에서 lob column이 있는 table은 지원하지 않으므로
        // Table cursor는 NULL을 넘긴다.
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
        /* PROJ-2464 hybrid partitioned table 지원
         * Partitioned Table을 기준으로 만든 smiValue Array를 Table Partition에 맞게 변환한다.
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
        // tuple 원복시 cursor도 원복해야한다.
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

    /* PROJ-2464 hybrid partitioned table 지원
     *  Disk Partition인 경우, Disk Type의 Lob Column이 필요하다.
     *  Memory/Volatile Partition의 경우, 해당 Partition의 Lob Column이 필요하다.
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

    // Lob컬럼 처리
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
        // NEW ROW의 획득
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

    /* PROJ-1107 Check Constraint 지원 */
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
        // ROW GRANULARITY TRIGGER의 수행
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
 *    UPTE 의 고유 기능을 수행한다.
 *
 * Implementation :
 *    - trigger each row 수행
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

        // UPDATE와 VIEW 사이에 FILT 같은 다른 노드들에 의해 stack이 변경되었을 수 있으므로
        // stack을 view tuple의 컬럼으로 재설정한다.
        for ( i = 0, sColumn = sDataPlan->updateTuple->columns;
              i < sDataPlan->updateTuple->columnCount;
              i++, sColumn++, sStack++ )
        {
            sStack->column = sColumn;
            sStack->value  =
                (void*)((SChar*)sDataPlan->updateTuple->row + sColumn->column.offset);
        }

        /* PROJ-2464 hybrid partitioned table 지원 */
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

            // 대상 Partition이 명확하므로, 작업 기준을 Partition으로 설정한다.
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

        // Sequence Value 획득
        if ( sCodePlan->nextValSeqs != NULL )
        {
            IDE_TEST( qmx::readSequenceNextVals(
                          aTemplate->stmt,
                          sCodePlan->nextValSeqs )
                      != IDE_SUCCESS );
        }

        sSmiValues = aTemplate->insOrUptRow[ sCodePlan->valueIdx ];

        // subquery가 있을때 smi value 정보 구성
        IDE_TEST( qmx::makeSmiValueForSubquery( aTemplate,
                                                sTableInfo,
                                                sCodePlan->columns,
                                                sCodePlan->subqueries,
                                                sCodePlan->canonizedTuple,
                                                sSmiValues,
                                                sCodePlan->isNull,
                                                sDataPlan->lobInfo )
                  != IDE_SUCCESS );

        // 갱신할 smi value 정보 구성
        IDE_TEST( qmx::makeSmiValueForUpdate( aTemplate,
                                              sTableInfo,
                                              sCodePlan->columns,
                                              sCodePlan->values,
                                              sCodePlan->canonizedTuple,
                                              sSmiValues,
                                              sCodePlan->isNull,
                                              sDataPlan->lobInfo )
                  != IDE_SUCCESS );

        // old smiValues를 new smiValues로 복사
        idlOS::memcpy( sDataPlan->newRow,
                       sDataPlan->oldRow,
                       ID_SIZEOF(smiValue) * sDataPlan->updateTuple->columnCount );

        // update smiValues를 적절한 위치에 복사
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
        // ROW GRANULARITY TRIGGER의 수행
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
 *     Foreign Key Referencing을 위한
 *     Master Table이 존재하는 지 검사
 *
 *     To Fix PR-10592
 *     Cursor의 올바른 사용을 위해서는 Master에 대한 검사를 수행한 후에
 *     Child Table에 대한 검사를 수행하여야 한다.
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
    // UPDATE된 로우 검색을 위해,
    // 갱신연산이 수행된 첫번째 row 이전 위치로 cursor 위치 설정
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
    // INSERT된 로우 검색을 위해,
    // 갱신연산이 수행된 첫번째 row 이전 위치로 cursor 위치 설정
    //------------------------------------------

    if ( ( sCodePlan->parentConstraints != NULL ) &&
         ( sCodePlan->updateType == QMO_UPDATE_ROWMOVEMENT ) )
    {
        for ( i = 0; i < sDataPlan->insertCursorMgr.mCursorIndexCount; i++ )
        {
            sInsertPartCursor = sDataPlan->insertCursorMgr.mCursorIndex[i];

            sOrgRow = sSearchRow = sDataPlan->updateTuple->row;

            //------------------------------------------
            // 저장 매체에 따른 공간 확보 및 컬럼 정보 구성
            //------------------------------------------

            IDE_TEST( sInsertPartCursor->cursor.beforeFirstModified( SMI_FIND_MODIFIED_NEW )
                      != IDE_SUCCESS );
            
            //------------------------------------------
            // 변경된 Row를 반복적으로 읽어 Referecing 검사를 함
            //------------------------------------------
            
            IDE_TEST( sInsertPartCursor->cursor.readNewRow( (const void **) & sSearchRow,
                                                            & sDataPlan->updateTuple->rid )
                      != IDE_SUCCESS);

            sDataPlan->updateTuple->row = ( sSearchRow == NULL ) ? sOrgRow : sSearchRow;

            while ( sSearchRow != NULL )
            {
                // Memory 재사용을 위하여 현재 위치 기록
                IDE_TEST_RAISE( aTemplate->stmt->qmxMem->getStatus(&sQmxMemStatus) != IDE_SUCCESS, ERR_MEM_OP );

                //------------------------------------------
                // Master Table에 대한 Referencing 검사
                //------------------------------------------

                IDE_TEST( qdnForeignKey::checkParentRef(
                              aTemplate->stmt,
                              sCodePlan->updateColumnIDs,
                              sCodePlan->parentConstraints,
                              sDataPlan->updateTuple,
                              sDataPlan->updateTuple->row,
                              sCodePlan->updateColumnCount )
                          != IDE_SUCCESS );

                // Memory 재사용을 위한 Memory 이동
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
 *     Foreign Key Referencing을 위한
 *     Master Table이 존재하는 지 검사
 *
 *     To Fix PR-10592
 *     Cursor의 올바른 사용을 위해서는 Master에 대한 검사를 수행한 후에
 *     Child Table에 대한 검사를 수행하여야 한다.
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
    // UPDATE된 로우 검색을 위해,
    // 갱신연산이 수행된 첫번째 row 이전 위치로 cursor 위치 설정
    //------------------------------------------

    sCursorInfo = (qmnCursorInfo*) aUpdateTuple->cursorInfo;

    IDE_TEST_RAISE( sCursorInfo == NULL, ERR_NOT_FOUND );

    sUpdateCursor = sCursorInfo->cursor;

    // BUG-37147 sUpdateCursor 가 null 인 경우가 발생함
    // PROJ-1624 non-partitioned index
    // index table scan으로 open되지 않은 partition이 존재한다.
    if ( sUpdateCursor != NULL )
    {
        sOrgRow = sSearchRow = aUpdateTuple->row;

        IDE_TEST( sUpdateCursor->beforeFirstModified( SMI_FIND_MODIFIED_NEW )
                  != IDE_SUCCESS );
        
        IDE_TEST( sUpdateCursor->readNewRow( (const void**) & sSearchRow,
                                             & aUpdateTuple->rid )
                  != IDE_SUCCESS );

        //------------------------------------------
        // Referencing 검사를 위해 삭제된 Row들을 검색
        //------------------------------------------

        aUpdateTuple->row = ( sSearchRow == NULL ) ? sOrgRow : sSearchRow;

        while( sSearchRow != NULL )
        {
            // Memory 재사용을 위하여 현재 위치 기록
            IDE_TEST_RAISE( aTemplate->stmt->qmxMem->getStatus(&sQmxMemStatus) != IDE_SUCCESS, ERR_MEM_OP );

            //------------------------------------------
            // Child Table에 대한 Referencing 검사
            //------------------------------------------

            IDE_TEST( qdnForeignKey::checkParentRef(
                        aTemplate->stmt,
                        aCodePlan->updateColumnIDs,
                        aCodePlan->parentConstraints,
                        aUpdateTuple,
                        aUpdateTuple->row,
                        aCodePlan->updateColumnCount )
                    != IDE_SUCCESS );

            // Memory 재사용을 위한 Memory 이동
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
    // 적합성 검사
    //------------------------------------------

    IDE_DASSERT( aTemplate != NULL );

    //------------------------------------------
    // child constraint 검사
    //------------------------------------------

    if ( sCodePlan->childConstraints != NULL )
    {
        // BUG-17940 parent key를 갱신하고 child key를 찾을때
        // parent row에 lock을 잡은 이후 view를 보기위해
        // 새로운 smiStmt를 이용한다.
        // Update cascade 옵션에 대비해서 normal로 한다.
        // child table의 타입을 현재 알 수 없기 때문에 ALL CURSOR로 한다.
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
 *    UPDATE 구문 수행 시 Child Table에 대한 Referencing 제약 조건을 검사
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
    // 적합성 검사
    //------------------------------------------

    IDE_DASSERT( aTemplate != NULL );
    IDE_DASSERT( aCodePlan->childConstraints != NULL );

    //------------------------------------------
    // UPDATE된 로우 검색을 위해,
    // 갱신연산이 수행된 첫번째 row 이전 위치로 cursor 위치 설정
    //------------------------------------------

    sCursorInfo = (qmnCursorInfo*) aUpdateTuple->cursorInfo;

    IDE_TEST_RAISE( sCursorInfo == NULL, ERR_NOT_FOUND );

    sUpdateCursor = sCursorInfo->cursor;

    // PROJ-1624 non-partitioned index
    // index table scan으로 open되지 않은 partition이 존재한다.
    if ( sUpdateCursor != NULL )
    {
        IDE_TEST( sUpdateCursor->beforeFirstModified( SMI_FIND_MODIFIED_OLD )
                  != IDE_SUCCESS );

        //------------------------------------------
        // Referencing 검사를 위해 삭제된 Row들을 검색
        //------------------------------------------

        sOrgRow = sSearchRow = aUpdateTuple->row;
        IDE_TEST(
            sUpdateCursor->readOldRow( (const void**) & sSearchRow,
                                       & aUpdateTuple->rid )
            != IDE_SUCCESS );

        aUpdateTuple->row = ( sSearchRow == NULL ) ? sOrgRow : sSearchRow;

        while( sSearchRow != NULL )
        {
            // Memory 재사용을 위하여 현재 위치 기록
            IDE_TEST_RAISE( aTemplate->stmt->qmxMem->getStatus(&sQmxMemStatus) != IDE_SUCCESS, ERR_MEM_OP );

            //------------------------------------------
            // Child Table에 대한 Referencing 검사
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

            // Memory 재사용을 위한 Memory 이동
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
 *    UPDATE 구문 수행 시 index table에 대한 update 수행
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
            // tuple 원복시 cursor도 원복해야한다.
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

        // 다른 index table도 update
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
 *    UPDATE 구문 수행 시 index table에 대한 update 수행
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
            // tuple 원복시 cursor도 원복해야한다.
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

        // 다른 index table도 update
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
 *     마지막 update row의 GRID를 반환한다.
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
 *       join view update시 중복 update여부 체크
 * Implementation :
 *    1. join의 결과 null인지 체크.
 *    2. cursor 원복
 *    3. update 중복 체크
 ***********************************************************************/
    
    scGRID            sNullRID;
    void            * sNullRow     = NULL;
    UInt              sTableType;
    void            * sTableHandle = NULL;
    idBool            sIsDupUpdate = ID_FALSE;
    
    /* PROJ-2464 hybrid partitioned table 지원 */
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
                                          aDataPlan->updateTuple->rid )
                        == ID_TRUE, ERR_MODIFY_UNABLE_RECORD );
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
    // tuple 원복시 cursor도 원복해야한다.
    IDE_TEST( aDataPlan->updateCursor->setRowPosition( aDataPlan->updateTuple->row,
                                                       aDataPlan->updateTuple->rid )
              != IDE_SUCCESS );
        
    /* 중복 update인지 체크 */
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
