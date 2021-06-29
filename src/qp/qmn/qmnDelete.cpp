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
 * $Id: qmnDelete.cpp 55241 2012-08-27 09:13:19Z linkedlist $
 *
 * Description :
 *     DETE(DEleTE) Node
 *
 *     ������ �𵨿��� delete�� �����ϴ� Plan Node �̴�.
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#include <idl.h>
#include <ide.h>
#include <qcg.h>
#include <qmnDelete.h>
#include <qdnTrigger.h>
#include <qdnForeignKey.h>
#include <qmx.h>
#include <qdtCommon.h>

IDE_RC
qmnDETE::init( qcTemplate * aTemplate,
               qmnPlan    * aPlan )
{
/***********************************************************************
 *
 * Description :
 *    DETE ����� �ʱ�ȭ
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnDETE::init"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmnDETE::init"));

    qmncDETE * sCodePlan = (qmncDETE*) aPlan;
    qmndDETE * sDataPlan =
        (qmndDETE*) (aTemplate->tmplate.data + aPlan->offset);

    sDataPlan->flag = & aTemplate->planFlag[sCodePlan->planID];
    sDataPlan->doIt = qmnDETE::doItDefault;

    //------------------------------------------------
    // ���� �ʱ�ȭ ���� ���� �Ǵ�
    //------------------------------------------------

    if ( ( *sDataPlan->flag & QMND_DETE_INIT_DONE_MASK )
         == QMND_DETE_INIT_DONE_FALSE )
    {
        if ( ( sCodePlan->flag & QMNC_DETE_MULTIPLE_TABLE_MASK )
             == QMNC_DETE_MULTIPLE_TABLE_FALSE )
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

            IDE_TEST( allocReturnRow( aTemplate, sCodePlan, sDataPlan )
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

            IDE_TEST( allocTriggerRowMultiTable( aTemplate, sCodePlan, sDataPlan )
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

        *sDataPlan->flag &= ~QMND_DETE_INIT_DONE_MASK;
        *sDataPlan->flag |= QMND_DETE_INIT_DONE_TRUE;
    }
    else
    {
        //------------------------------------------------
        // Child Plan�� �ʱ�ȭ
        //------------------------------------------------
        IDE_TEST( aPlan->left->init( aTemplate,
                                     aPlan->left ) != IDE_SUCCESS);
    }

    //------------------------------------------------
    // ���� Data �� �ʱ�ȭ
    //------------------------------------------------

    // Limit ���� ������ �ʱ�ȭ
    sDataPlan->limitCurrent = 1;

    //------------------------------------------------
    // ���� �Լ� ����
    //------------------------------------------------
    if ( ( sCodePlan->flag & QMNC_DETE_MULTIPLE_TABLE_MASK )
         == QMNC_DETE_MULTIPLE_TABLE_FALSE )
    {
        sDataPlan->doIt = qmnDETE::doItFirst;
    }
    else
    {
        sDataPlan->doIt = qmnDETE::doItFirstMultiTable;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnDETE::doIt( qcTemplate * aTemplate,
               qmnPlan    * aPlan,
               qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    DETE �� ���� ����� �����Ѵ�.
 *
 * Implementation :
 *    ������ �Լ� �����͸� �����Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "qmnDETE::doIt"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmndDETE * sDataPlan =
        (qmndDETE*) (aTemplate->tmplate.data + aPlan->offset);

    return sDataPlan->doIt( aTemplate, aPlan, aFlag );

#undef IDE_FN
}

IDE_RC 
qmnDETE::padNull( qcTemplate * aTemplate,
                  qmnPlan    * aPlan )
{
/***********************************************************************
 *
 * Description :
 *    DETE ���� ������ null row�� ������ ������,
 *    Child�� ���Ͽ� padNull()�� ȣ���Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnDETE::padNull"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmnDETE::padNull"));

    qmncDETE * sCodePlan = (qmncDETE*) aPlan;
    // qmndDETE * sDataPlan = 
    //     (qmndDETE*) (aTemplate->tmplate.data + aPlan->offset);

    if ( (aTemplate->planFlag[sCodePlan->planID] & QMND_DETE_INIT_DONE_MASK)
         == QMND_DETE_INIT_DONE_FALSE )
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
qmnDETE::printPlan( qcTemplate   * aTemplate,
                    qmnPlan      * aPlan,
                    ULong          aDepth,
                    iduVarString * aString,
                    qmnDisplay     aMode )
{
/***********************************************************************
 *
 * Description :
 *    DETE ����� ���� ������ ����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnDETE::printPlan"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmnDETE::printPlan"));

    qmncDETE * sCodePlan = (qmncDETE*) aPlan;
    qmndDETE * sDataPlan =
        (qmndDETE*) (aTemplate->tmplate.data + aPlan->offset);

    ULong               i;
    qmmDelMultiTables * sTmp;
    qmsNamePosition     sTableOwnerName;     // Table Owner Name
    qmsNamePosition     sTableName;          // Table Name
    qmsNamePosition     sAliasName;          // Alias Name
    
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
    // DETE Target ������ ���
    //------------------------------------------------------
    if ( ( sCodePlan->flag & QMNC_DETE_MULTIPLE_TABLE_MASK )
         == QMNC_DETE_MULTIPLE_TABLE_FALSE )
    {
        // DETE ������ ���
        if ( sCodePlan->tableRef->tableType == QCM_VIEW )
        {
            iduVarStringAppendFormat( aString,
                                      "DELETE ( VIEW: " );
        }
        else
        {
            iduVarStringAppendFormat( aString,
                                      "DELETE ( TABLE: " );
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
                                  "DELETE ( " );

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
        }
    }

    //----------------------------
    // New line ���
    //----------------------------
    iduVarStringAppend( aString, " )\n" );

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
qmnDETE::firstInit( qcTemplate * aTemplate,
                    qmncDETE   * aCodePlan,
                    qmndDETE   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *    DETE node�� Data ������ ����� ���� �ʱ�ȭ�� ����
 *
 * Implementation :
 *    - Data ������ �ֿ� ����� ���� �ʱ�ȭ�� ����
 *
 ***********************************************************************/

#define IDE_FN "qmnDETE::firstInit"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmnDETE::firstInit"));

    ULong    sCount;

    //--------------------------------
    // ���ռ� �˻�
    //--------------------------------

    //--------------------------------
    // DETE ���� ������ �ʱ�ȭ
    //--------------------------------

    // Tuple Set������ �ʱ�ȭ
    aDataPlan->deleteTuple = & aTemplate->tmplate.rows[aCodePlan->tableRef->table];
    aDataPlan->deleteCursor = NULL;
    aDataPlan->deleteTupleID = ID_USHORT_MAX;

    /* PROJ-2464 hybrid partitioned table ���� */
    aDataPlan->deletePartInfo = NULL;

    // index table cursor �ʱ�ȭ
    aDataPlan->indexDeleteCursor = NULL;
    aDataPlan->indexDeleteTuple = NULL;

    // where column list �ʱ�ȭ
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
        IDE_TEST( allocCursorInfo( aTemplate, aCodePlan, aDataPlan )
                  != IDE_SUCCESS );
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
        IDE_ASSERT( (aCodePlan->flag & QMNC_DETE_LIMIT_MASK)
                    == QMNC_DETE_LIMIT_TRUE );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnDETE::allocCursorInfo( qcTemplate * aTemplate,
                          qmncDETE   * aCodePlan,
                          qmndDETE   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnDETE::allocCursorInfo"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmnDETE::allocCursorInfo"));

    qmnCursorInfo     * sCursorInfo;
    qmsPartitionRef   * sPartitionRef;
    UInt                sPartitionCount;
    UInt                i = 0;

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
    sCursorInfo->updateColumnList    = NULL;
    sCursorInfo->cursorType          = SMI_DELETE_CURSOR;
    sCursorInfo->isRowMovementUpdate = ID_FALSE;
    sCursorInfo->inplaceUpdate       = ID_FALSE;
    sCursorInfo->lockMode            = SMI_LOCK_WRITE;

    /* PROJ-2464 hybrid partitioned table ���� */
    sCursorInfo->stmtRetryColLst     = aCodePlan->whereColumnList;
    sCursorInfo->rowRetryColLst      = NULL;

    // cursorInfo ����
    aDataPlan->deleteTuple->cursorInfo = sCursorInfo;
    
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

        for ( sPartitionRef = aCodePlan->tableRef->partitionRef, i = 0;
              sPartitionRef != NULL;
              sPartitionRef = sPartitionRef->next, sCursorInfo++, i++ )
        {
            // cursorInfo �ʱ�ȭ
            sCursorInfo->cursor              = NULL;
            sCursorInfo->selectedIndex       = NULL;
            sCursorInfo->selectedIndexTuple  = NULL;
            /* PROJ-2359 Table/Partition Access Option */
            sCursorInfo->accessOption        = QCM_ACCESS_OPTION_READ_WRITE;
            sCursorInfo->updateColumnList    = NULL;
            sCursorInfo->cursorType          = SMI_DELETE_CURSOR;
            sCursorInfo->isRowMovementUpdate = ID_FALSE;
            sCursorInfo->inplaceUpdate       = ID_FALSE;
            sCursorInfo->lockMode            = SMI_LOCK_WRITE;

            /* PROJ-2464 hybrid partitioned table ���� */
            sCursorInfo->stmtRetryColLst     = aCodePlan->wherePartColumnList[i];
            sCursorInfo->rowRetryColLst      = NULL;

            // cursorInfo ����
            aTemplate->tmplate.rows[sPartitionRef->table].cursorInfo = sCursorInfo;
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
qmnDETE::allocTriggerRow( qcTemplate * aTemplate,
                          qmncDETE   * aCodePlan,
                          qmndDETE   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnDETE::allocTriggerRow"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmnDETE::allocTriggerRow"));

    UInt sMaxRowOffset = 0;

    //---------------------------------
    // Trigger�� ���� ������ ����
    //---------------------------------

    if ( aCodePlan->tableRef->tableInfo->triggerCount > 0 )
    {
        if ( aCodePlan->insteadOfTrigger == ID_TRUE )
        {
            IDE_TEST( aTemplate->stmt->qmxMem->alloc(
                    ID_SIZEOF(smiValue) *
                    aCodePlan->tableRef->tableInfo->columnCount,
                    (void**) & aDataPlan->oldRow )
                != IDE_SUCCESS);
        }
        else
        {
            sMaxRowOffset = qmx::getMaxRowOffset( &(aTemplate->tmplate),
                                                  aCodePlan->tableRef );

            if ( sMaxRowOffset > 0 )
            {
                // Old Row Referencing�� ���� ���� �Ҵ�
                IDE_TEST( aTemplate->stmt->qmxMem->alloc(
                        sMaxRowOffset,
                        (void**) & aDataPlan->oldRow )
                    != IDE_SUCCESS);
            }
            else
            {
                aDataPlan->oldRow = NULL;
            }
        }

        aDataPlan->columnsForRow = aCodePlan->tableRef->tableInfo->columns;
        
        aDataPlan->needTriggerRow = ID_FALSE;
        aDataPlan->existTrigger = ID_TRUE;
    }
    else
    {
        aDataPlan->oldRow = NULL;
        
        aDataPlan->needTriggerRow = ID_FALSE;
        aDataPlan->existTrigger = ID_FALSE;
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}
    
IDE_RC
qmnDETE::allocReturnRow( qcTemplate * aTemplate,
                         qmncDETE   * aCodePlan,
                         qmndDETE   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnDETE::allocReturnRow"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmnDETE::allocReturnRow"));

    UInt sMaxRowOffset;

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
qmnDETE::allocIndexTableCursor( qcTemplate * aTemplate,
                                qmncDETE   * aCodePlan,
                                qmndDETE   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnDETE::allocIndexTableCursor"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmnDETE::allocIndexTableCursor"));

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
        
        *aDataPlan->flag &= ~QMND_DETE_INDEX_CURSOR_MASK;
        *aDataPlan->flag |= QMND_DETE_INDEX_CURSOR_INITED;
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
qmnDETE::doItDefault( qcTemplate * /* aTemplate */,
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

#define IDE_FN "qmnDETE::doItDefault"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    IDE_DASSERT( 0 );

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnDETE::doItFirst( qcTemplate * aTemplate,
                    qmnPlan    * aPlan,
                    qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    DETE�� ���� ���� �Լ�
 *
 * Implementation :
 *    - Cursor Open
 *    - delete one record
 *
 ***********************************************************************/

#define IDE_FN "qmnDETE::doItFirst"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncDETE * sCodePlan = (qmncDETE*) aPlan;
    qmndDETE * sDataPlan =
        (qmndDETE*) (aTemplate->tmplate.data + aPlan->offset);

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
    /* TASK-7307 DML Data Consistency in Shard */
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
            // ���� Delete���� Child�� �����ϱ⸸ �Ѵ�.
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
                                        sDataPlan->deleteTuple->tableHandle )
                              != IDE_SUCCESS );
                }
                else
                {
                    /* Nothing to do */
                }

                // delete one record
                IDE_TEST( deleteOneRow( aTemplate, aPlan ) != IDE_SUCCESS );
            }

            sDataPlan->doIt = qmnDETE::doItNext;
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

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnDETE::doItNext( qcTemplate * aTemplate,
                   qmnPlan    * aPlan,
                   qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    DETE�� ���� ���� �Լ�
 *    ���� Record�� �����Ѵ�.
 *
 * Implementation :
 *    - delete one record
 *
 ***********************************************************************/

#define IDE_FN "qmnDETE::doItNext"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncDETE * sCodePlan = (qmncDETE*) aPlan;
    qmndDETE * sDataPlan =
        (qmndDETE*) (aTemplate->tmplate.data + aPlan->offset);

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
                if ( sCodePlan->tableRef->partitionRef != NULL )
                {
                    // get cursor
                    IDE_TEST( getCursor( aTemplate, aPlan, &sIsTableCursorChanged ) != IDE_SUCCESS );

                    /* PROJ-2359 Table/Partition Access Option */
                    if ( sIsTableCursorChanged == ID_TRUE )
                    {
                        IDE_TEST( qmx::checkAccessOptionForExistentRecord(
                                            sDataPlan->accessOption,
                                            sDataPlan->deleteTuple->tableHandle )
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
                
                // delete one record
                IDE_TEST( deleteOneRow( aTemplate, aPlan ) != IDE_SUCCESS );
            }
        }
        else
        {
            // record�� ���� ���
            // ���� ������ ���� ���� ���� �Լ��� ������.
            sDataPlan->doIt = qmnDETE::doItFirst;
        }
    }
    else
    {
        // record�� ���� ���
        // ���� ������ ���� ���� ���� �Լ��� ������.
        sDataPlan->doIt = qmnDETE::doItFirst;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnDETE::checkTrigger( qcTemplate * aTemplate,
                       qmnPlan    * aPlan )
{
/***********************************************************************
 *
 * Description :
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnDETE::checkTrigger"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncDETE * sCodePlan = (qmncDETE*) aPlan;
    qmndDETE * sDataPlan =
        (qmndDETE*) (aTemplate->tmplate.data + aPlan->offset);
    idBool     sNeedTriggerRow;

    if ( sDataPlan->existTrigger == ID_TRUE )
    {
        if ( sCodePlan->insteadOfTrigger == ID_TRUE )
        {
            IDE_TEST( qdnTrigger::needTriggerRow(
                          aTemplate->stmt,
                          sCodePlan->tableRef->tableInfo,
                          QCM_TRIGGER_INSTEAD_OF,
                          QCM_TRIGGER_EVENT_DELETE,
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
                          QCM_TRIGGER_EVENT_DELETE,
                          NULL,
                          & sNeedTriggerRow )
                      != IDE_SUCCESS );
        
            if ( sNeedTriggerRow == ID_FALSE )
            {
                IDE_TEST( qdnTrigger::needTriggerRow(
                              aTemplate->stmt,
                              sCodePlan->tableRef->tableInfo,
                              QCM_TRIGGER_AFTER,
                              QCM_TRIGGER_EVENT_DELETE,
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
qmnDETE::getCursor( qcTemplate * aTemplate,
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

#define IDE_FN "qmnDETE::getCursor"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncDETE * sCodePlan = (qmncDETE*) aPlan;
    qmndDETE * sDataPlan =
        (qmndDETE*) (aTemplate->tmplate.data + aPlan->offset);

    qmnCursorInfo * sCursorInfo = NULL;

    /* PROJ-2359 Table/Partition Access Option */
    *aIsTableCursorChanged = ID_FALSE;

    if ( sCodePlan->tableRef->partitionRef == NULL )
    {
        if ( sDataPlan->deleteTupleID != sCodePlan->tableRef->table )
        {
            sDataPlan->deleteTupleID = sCodePlan->tableRef->table;
            
            // cursor�� ��´�.
            sCursorInfo = (qmnCursorInfo*)
                aTemplate->tmplate.rows[sDataPlan->deleteTupleID].cursorInfo;
            
            IDE_TEST_RAISE( sCursorInfo == NULL, ERR_NOT_FOUND );

            sDataPlan->deleteCursor = sCursorInfo->cursor;
   
            /* PROJ-2464 hybrid partitioned table ���� */
            sDataPlan->deletePartInfo = sCodePlan->tableRef->tableInfo;

            sDataPlan->retryInfo.mIsWithoutRetry  = sCodePlan->withoutRetry;
            sDataPlan->retryInfo.mStmtRetryColLst = sCursorInfo->stmtRetryColLst;

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
        if ( sDataPlan->deleteTupleID != sDataPlan->deleteTuple->partitionTupleID )
        {
            sDataPlan->deleteTupleID = sDataPlan->deleteTuple->partitionTupleID;
            
            // partition�� cursor�� ��´�.
            sCursorInfo = (qmnCursorInfo*)
                aTemplate->tmplate.rows[sDataPlan->deleteTupleID].cursorInfo;

            /* BUG-42440 BUG-39399 has invalid erorr message */
            if ( ( sDataPlan->deleteTuple->lflag & MTC_TUPLE_PARTITIONED_TABLE_MASK )
                 == MTC_TUPLE_PARTITIONED_TABLE_TRUE )
            {
                IDE_TEST_RAISE( sCursorInfo == NULL, ERR_MODIFY_UNABLE_RECORD );
            }
            else
            {
                /* Nothing to do */
            }

            IDE_TEST_RAISE( sCursorInfo == NULL, ERR_NOT_FOUND );

            sDataPlan->deleteCursor = sCursorInfo->cursor;

            /* PROJ-2464 hybrid partitioned table ���� */
            IDE_TEST( smiGetTableTempInfo( sDataPlan->deleteTuple->tableHandle,
                                           (void **)&(sDataPlan->deletePartInfo) )
                      != IDE_SUCCESS );

            sDataPlan->retryInfo.mIsWithoutRetry  = sCodePlan->withoutRetry;
            sDataPlan->retryInfo.mStmtRetryColLst = sCursorInfo->stmtRetryColLst;

            /* PROJ-2359 Table/Partition Access Option */
            sDataPlan->accessOption = sCursorInfo->accessOption;
            *aIsTableCursorChanged  = ID_TRUE;
        }
        else
        {
            /* Nothing to do */
        }
        
        // index table cursor�� ��´�.
        if ( sDataPlan->indexDeleteCursor == NULL )
        {
            sCursorInfo = (qmnCursorInfo*)
                aTemplate->tmplate.rows[sCodePlan->tableRef->table].cursorInfo;
            
            IDE_TEST_RAISE( sCursorInfo == NULL, ERR_NOT_FOUND );

            sDataPlan->indexDeleteCursor = sCursorInfo->cursor;
            sDataPlan->indexDeleteTuple = sCursorInfo->selectedIndexTuple;
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
                                  "qmnDETE::getCursor",
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
qmnDETE::closeCursor( qcTemplate * aTemplate,
                      qmnPlan    * aPlan )
{
/***********************************************************************
 *
 * Description :
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnDETE::closeCursor"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmndDETE * sDataPlan =
        (qmndDETE*) (aTemplate->tmplate.data + aPlan->offset);

    if ( ( *sDataPlan->flag & QMND_DETE_INDEX_CURSOR_MASK )
         == QMND_DETE_INDEX_CURSOR_INITED )
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

    if ( ( *sDataPlan->flag & QMND_DETE_INDEX_CURSOR_MASK )
         == QMND_DETE_INDEX_CURSOR_INITED )
    {
        (void) qmsIndexTable::finalizeIndexTableCursors(
            & (sDataPlan->indexTableCursorInfo) );
    }
    
    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnDETE::deleteOneRow( qcTemplate * aTemplate,
                       qmnPlan    * aPlan )
{
/***********************************************************************
 *
 * Description :
 *    DETE �� ���� ����� �����Ѵ�.
 *
 * Implementation :
 *    - delete one record ����
 *    - trigger each row ����
 *
 ***********************************************************************/

#define IDE_FN "qmnDETE::deleteOneRow"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncDETE * sCodePlan = (qmncDETE*) aPlan;
    qmndDETE * sDataPlan =
        (qmndDETE*) (aTemplate->tmplate.data + aPlan->offset);
    smiValue           sWhereSmiValues[QC_MAX_COLUMN_COUNT];
    idBool             sIsDiskTableOrPartition;

    if ( ( sCodePlan->flag & QMNC_DETE_VIEW_KEY_PRESERVED_MASK )
         == QMNC_DETE_VIEW_KEY_PRESERVED_FALSE )
    {
        /* BUG-39399 remove search key preserved table */
        if ( ( sCodePlan->flag & QMNC_DETE_VIEW_MASK )
             == QMNC_DETE_VIEW_TRUE )
        {
            IDE_TEST( checkDuplicateDelete( sCodePlan,
                                            sDataPlan )
                      != IDE_SUCCESS );
        }
        else
        {
            /* Nothing To Do */
        }
    }
    
    //-----------------------------------
    // copy old row
    //-----------------------------------
    
    if ( sDataPlan->needTriggerRow == ID_TRUE )
    {
        // OLD ROW REFERENCING�� ���� ����
        idlOS::memcpy( sDataPlan->oldRow,
                       sDataPlan->deleteTuple->row,
                       sDataPlan->deleteTuple->rowOffset );
    }
    else
    {
        // Nothing to do.
    }

    //-----------------------------------
    // PROJ-2334PMT
    // set delete trigger memory variable column info
    //-----------------------------------

    if ( ( sDataPlan->existTrigger == ID_TRUE ) &&
         ( sCodePlan->tableRef->tableInfo->tablePartitionType == QCM_PARTITIONED_TABLE ) )
    {
        sDataPlan->columnsForRow = sDataPlan->deletePartInfo->columns;
    }
    else
    {
        // Nothing To Do
    }

    //-----------------------------------
    // delete before trigger
    //-----------------------------------
    
    if ( sDataPlan->existTrigger == ID_TRUE )
    {
        // PROJ-1359 Trigger
        // ROW GRANULARITY TRIGGER�� ����
        IDE_TEST( qdnTrigger::fireTrigger(
                      aTemplate->stmt,
                      aTemplate->stmt->qmxMem,
                      sCodePlan->tableRef->tableInfo,
                      QCM_TRIGGER_ACTION_EACH_ROW,
                      QCM_TRIGGER_BEFORE,
                      QCM_TRIGGER_EVENT_DELETE,
                      NULL,                        // UPDATE Column
                      sDataPlan->deleteCursor,     /* Table Cursor */
                      sDataPlan->deleteTuple->rid, /* Row GRID */
                      sDataPlan->oldRow,           // OLD ROW
                      sDataPlan->columnsForRow,    // OLD ROW Column
                      NULL,                        // NEW ROW
                      NULL )                       // NEW ROW Column
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }
        
    //-----------------------------------
    // delete one row
    //-----------------------------------

    if ( ( sCodePlan->flag & QMNC_DETE_VIEW_KEY_PRESERVED_MASK )
         == QMNC_DETE_VIEW_KEY_PRESERVED_TRUE )
    {
        // PROJ-2204 join update, delete
        // tuple ������ cursor�� �����ؾ��Ѵ�.
        if ( ( sCodePlan->flag & QMNC_DETE_VIEW_MASK )
             == QMNC_DETE_VIEW_TRUE )
        {
            IDE_TEST( sDataPlan->deleteCursor->setRowPosition(
                          sDataPlan->deleteTuple->row,
                          sDataPlan->deleteTuple->rid )
                      != IDE_SUCCESS );
        }
        else
        {
            // Nothing to do.
        }
    }
    
    if ( sDataPlan->retryInfo.mIsWithoutRetry == ID_TRUE )
    {
        if ( sCodePlan->tableRef->tableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
        {
            sIsDiskTableOrPartition = QCM_TABLE_TYPE_IS_DISK( sDataPlan->deletePartInfo->tableFlag );
        }
        else
        {
            sIsDiskTableOrPartition = QCM_TABLE_TYPE_IS_DISK( sCodePlan->tableRef->tableInfo->tableFlag );
        }

        if ( sIsDiskTableOrPartition == ID_TRUE )
        {
            IDE_TEST( qmx::setChkSmiValueList( sDataPlan->deleteTuple->row,
                                               sDataPlan->retryInfo.mStmtRetryColLst,
                                               sWhereSmiValues,
                                               & (sDataPlan->retryInfo.mStmtRetryValLst) )
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

    IDE_TEST( sDataPlan->deleteCursor->deleteRow( &(sDataPlan->retryInfo) )
              != IDE_SUCCESS );

    // delete index table
    IDE_TEST( deleteIndexTableCursor( aTemplate,
                                      sCodePlan,
                                      sDataPlan )
              != IDE_SUCCESS );
    
    //-----------------------------------
    // delete after trigger
    //-----------------------------------
    
    if ( sDataPlan->existTrigger == ID_TRUE )
    {
        // PROJ-1359 Trigger
        // ROW GRANULARITY TRIGGER�� ����
        IDE_TEST( qdnTrigger::fireTrigger(
                      aTemplate->stmt,
                      aTemplate->stmt->qmxMem,
                      sCodePlan->tableRef->tableInfo,
                      QCM_TRIGGER_ACTION_EACH_ROW,
                      QCM_TRIGGER_AFTER,
                      QCM_TRIGGER_EVENT_DELETE,
                      NULL,                        // UPDATE Column
                      sDataPlan->deleteCursor,     /* Table Cursor */
                      sDataPlan->deleteTuple->rid, /* Row GRID */
                      sDataPlan->oldRow,           // OLD ROW
                      sDataPlan->columnsForRow,    // OLD ROW Column
                      NULL,                        // NEW ROW
                      NULL )                       // NEW ROW Column
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
        IDE_TEST( qmx::copyReturnToInto( aTemplate,
                                         sCodePlan->returnInto,
                                         aTemplate->numRows )
                  != IDE_SUCCESS );
    }
    else
    {
        // nothing do do
    }

    if ( ( *sDataPlan->flag & QMND_DETE_REMOVE_MASK )
         == QMND_DETE_REMOVE_FALSE )
    {
        *sDataPlan->flag &= ~QMND_DETE_REMOVE_MASK;
        *sDataPlan->flag |= QMND_DETE_REMOVE_TRUE;
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
qmnDETE::fireInsteadOfTrigger( qcTemplate * aTemplate,
                               qmnPlan    * aPlan )
{
/***********************************************************************
 *
 * Description :
 *    DETE �� ���� ����� �����Ѵ�.
 *
 * Implementation :
 *    - trigger each row ����
 *
 ***********************************************************************/

#define IDE_FN "qmnDETE::fireInsteadOfTrigger"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncDETE * sCodePlan = (qmncDETE*) aPlan;
    qmndDETE * sDataPlan =
        (qmndDETE*) (aTemplate->tmplate.data + aPlan->offset);

    qcmTableInfo * sTableInfo = NULL;
    mtcColumn    * sColumn    = NULL;
    mtcStack     * sStack     = NULL;
    SInt           sRemain    = 0;
    void         * sOrgRow    = NULL;
    UShort         i          = 0;

    sTableInfo = sCodePlan->tableRef->tableInfo;

    if ( ( sDataPlan->needTriggerRow == ID_TRUE ) ||
         ( sCodePlan->returnInto != NULL ) )
    {
        sStack = aTemplate->tmplate.stack;
        sRemain = aTemplate->tmplate.stackRemain;

        IDE_TEST_RAISE( sRemain < sDataPlan->deleteTuple->columnCount,
                        ERR_STACK_OVERFLOW );
        
        // DELETE�� VIEW ���̿� FILT ���� �ٸ� ���鿡 ���� stack�� ����Ǿ��� �� �����Ƿ�
        // stack�� view tuple�� �÷����� �缳���Ѵ�.
        for ( i = 0, sColumn = sDataPlan->deleteTuple->columns;
              i < sDataPlan->deleteTuple->columnCount;
              i++, sColumn++, sStack++ )
        {
            sStack->column = sColumn;
            sStack->value  =
                (void*)((SChar*)sDataPlan->deleteTuple->row + sColumn->column.offset);
        }

        /* PROJ-2464 hybrid partitioned table ���� */
        if ( sTableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
        {
            if ( sDataPlan->deletePartInfo != NULL )
            {
                if ( sDataPlan->deleteTuple->tableHandle != sDataPlan->deletePartInfo->tableHandle )
                {
                    IDE_TEST( smiGetTableTempInfo( sDataPlan->deleteTuple->tableHandle,
                                                   (void **)&(sDataPlan->deletePartInfo) )
                              != IDE_SUCCESS );
                }
                else
                {
                    /* Nothing to do */
                }
            }
            else
            {
                IDE_TEST( smiGetTableTempInfo( sDataPlan->deleteTuple->tableHandle,
                                               (void **)&(sDataPlan->deletePartInfo) )
                          != IDE_SUCCESS );
            }

            sTableInfo = sDataPlan->deletePartInfo;
            sDataPlan->columnsForRow = sDataPlan->deletePartInfo->columns;
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
                      sCodePlan->tableRef->tableInfo,
                      QCM_TRIGGER_ACTION_EACH_ROW,
                      QCM_TRIGGER_INSTEAD_OF,
                      QCM_TRIGGER_EVENT_DELETE,
                      NULL,                        // UPDATE Column
                      NULL,                        /* Table Cursor */
                      SC_NULL_GRID,                /* Row GRID */
                      sDataPlan->oldRow,           // OLD ROW
                      sDataPlan->columnsForRow,    // OLD ROW Column
                      NULL,                        // NEW ROW
                      NULL )                       // NEW ROW Column
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }
    
    /* PROJ-1584 DML Return Clause */
    if ( sCodePlan->returnInto != NULL )
    {
        IDE_TEST( qmx::makeRowWithSmiValue( sDataPlan->deleteTuple->columns,
                                            sDataPlan->deleteTuple->columnCount,
                                            (smiValue*) sDataPlan->oldRow,
                                            sDataPlan->returnRow )
                  != IDE_SUCCESS );
        
        sOrgRow = sDataPlan->deleteTuple->row;
        sDataPlan->deleteTuple->row = sDataPlan->returnRow;
        
        IDE_TEST( qmx::copyReturnToInto( aTemplate,
                                         sCodePlan->returnInto,
                                         aTemplate->numRows )
                  != IDE_SUCCESS );

        sDataPlan->deleteTuple->row = sOrgRow;
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
qmnDETE::checkDeleteRef( qcTemplate * aTemplate,
                         qmnPlan    * aPlan )
{
/***********************************************************************
 *
 * Description :
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnDETE::checkDeleteRef"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncDETE        * sCodePlan;
    qmndDETE        * sDataPlan;
    qmsPartitionRef * sPartitionRef;
    smiStatement      sSmiStmt;
    smiStatement    * sSmiStmtOrg;
    UInt              sStage = 0;
    
    sCodePlan = (qmncDETE*) aPlan;
    sDataPlan = (qmndDETE*) ( aTemplate->tmplate.data + aPlan->offset );
    
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
        // Delete cascade �ɼǿ� ����ؼ� normal�� �Ѵ�.
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

        if ( sDataPlan->deleteCursor != NULL )
        {
            if ( sCodePlan->tableRef->partitionRef == NULL )
            {
                IDE_TEST( checkDeleteChildRefOnScan( aTemplate,
                                                     sCodePlan,
                                                     sCodePlan->tableRef->tableInfo,
                                                     sDataPlan->deleteTuple )
                          != IDE_SUCCESS );
            }
            else
            {
                for ( sPartitionRef = sCodePlan->tableRef->partitionRef;
                      sPartitionRef != NULL;
                      sPartitionRef = sPartitionRef->next )
                {
                    IDE_TEST( checkDeleteChildRefOnScan(
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
            IDE_CALLBACK_FATAL("Check Child Key On Delete smiStmt.end() failed");
        }
    }

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnDETE::checkDeleteChildRefOnScan( qcTemplate     * aTemplate,
                                    qmncDETE       * aCodePlan,
                                    qcmTableInfo   * aTableInfo,
                                    mtcTuple       * aDeleteTuple )
{
/***********************************************************************
 *
 * Description :
 *    DELETE ���� ���� �� Child Table�� ���� Referencing ���� ������ �˻�
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnDETE::checkDeleteChildRefOnScan"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    iduMemoryStatus   sQmxMemStatus;
    void            * sOrgRow;
    void            * sSearchRow;
    smiTableCursor  * sDeleteCursor;
    qmnCursorInfo   * sCursorInfo;

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aTemplate != NULL );
    IDE_DASSERT( aCodePlan->childConstraints != NULL );

    //------------------------------------------
    // DELETE�� �ο� �˻��� ����,
    // ���ſ����� ����� ù��° row ���� ��ġ�� cursor ��ġ ����
    //------------------------------------------

    sCursorInfo = (qmnCursorInfo*) aDeleteTuple->cursorInfo;
    
    IDE_TEST_RAISE( sCursorInfo == NULL, ERR_NOT_FOUND );
    
    sDeleteCursor = sCursorInfo->cursor;

    // PROJ-1624 non-partitioned index
    // index table scan���� open���� ���� partition�� �����Ѵ�.
    if ( sDeleteCursor != NULL )
    {
        IDE_TEST( sDeleteCursor->beforeFirstModified( SMI_FIND_MODIFIED_OLD )
                  != IDE_SUCCESS );

        //------------------------------------------
        // Referencing �˻縦 ���� ������ Row���� �˻�
        //------------------------------------------

        sOrgRow = sSearchRow = aDeleteTuple->row;
        IDE_TEST(
            sDeleteCursor->readOldRow( (const void**) & sSearchRow,
                                       & aDeleteTuple->rid )
            != IDE_SUCCESS );            
        
        aDeleteTuple->row = ( sSearchRow == NULL ) ? sOrgRow : sSearchRow;

        while( sSearchRow != NULL )
        {
            // Memory ������ ���Ͽ� ���� ��ġ ���
            IDE_TEST_RAISE( aTemplate->stmt->qmxMem->getStatus(&sQmxMemStatus) != IDE_SUCCESS, ERR_MEM_OP );
        
            //------------------------------------------
            // Child Table�� ���� Referencing �˻�
            //------------------------------------------
        
            IDE_TEST( qdnForeignKey::checkChildRefOnDelete(
                          aTemplate->stmt,
                          aCodePlan->childConstraints,
                          aTableInfo->tableID,
                          aDeleteTuple,
                          aDeleteTuple->row,
                          ID_TRUE )
                      != IDE_SUCCESS );
        
            // Memory ������ ���� Memory �̵�
            IDE_TEST_RAISE( aTemplate->stmt->qmxMem->setStatus(&sQmxMemStatus) != IDE_SUCCESS, ERR_MEM_OP );
        
            sOrgRow = sSearchRow = aDeleteTuple->row;

            IDE_TEST(
                sDeleteCursor->readOldRow( (const void**) & sSearchRow,
                                           & aDeleteTuple->rid )
                != IDE_SUCCESS );
            
            aDeleteTuple->row =
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
                     " qmnDETE::checkDeleteChildRefOnScan"
                     " memory error" );
    }
    IDE_EXCEPTION( ERR_NOT_FOUND )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmnDETE::checkDeleteChildRefOnScan",
                                  "cursor not found" ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnDETE::deleteIndexTableCursor( qcTemplate     * aTemplate,
                                 qmncDETE       * aCodePlan,
                                 qmndDETE       * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *    DELETE ���� ���� �� index table�� ���� delete ����
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnDETE::deleteIndexTableCursor"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    // update index table
    if ( ( *aDataPlan->flag & QMND_DETE_INDEX_CURSOR_MASK )
         == QMND_DETE_INDEX_CURSOR_INITED )
    {
        // selected index table
        if ( aCodePlan->tableRef->selectedIndexTable != NULL )
        {
            // PROJ-2204 join update, delete
            // tuple ������ cursor�� �����ؾ��Ѵ�.
            if ( ( aCodePlan->flag & QMNC_DETE_VIEW_MASK )
                 == QMNC_DETE_VIEW_TRUE )
            {
                IDE_TEST( aDataPlan->indexDeleteCursor->setRowPosition(
                              aDataPlan->indexDeleteTuple->row,
                              aDataPlan->indexDeleteTuple->rid )
                          != IDE_SUCCESS );
            }
            else
            {
                // Nothing to do.
            }

            IDE_TEST( aDataPlan->indexDeleteCursor->deleteRow()
                      != IDE_SUCCESS );
        }
        else
        {
            // Nothing to do.
        }
        
        // �ٸ� index table�� update
        IDE_TEST( qmsIndexTable::deleteIndexTableCursors(
                      aTemplate->stmt,
                      & (aDataPlan->indexTableCursorInfo),
                      aDataPlan->deleteTuple->rid )
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

IDE_RC qmnDETE::checkDuplicateDelete( qmncDETE   * aCodePlan,
                                      qmndDETE   * aDataPlan )
{
/***********************************************************************
 *
 * Description : BUG-39399 remove search key preserved table
 *       join view delete�� �ߺ� update���� üũ
 * Implementation :
 *    1. join�� ��� null���� üũ.
 *    2. cursor ����
 *    3. update �ߺ� üũ
 ***********************************************************************/
    
    scGRID            nullRID;
    void            * nullRow = NULL;
    UInt              sTableType;
    void            * sTableHandle = NULL;
    idBool            sIsDupDelete = ID_FALSE;
    
    /* PROJ-2464 hybrid partitioned table ���� */
    if ( aCodePlan->tableRef->partitionRef == NULL )
    {
        sTableType   = aCodePlan->tableRef->tableInfo->tableFlag & SMI_TABLE_TYPE_MASK;
        sTableHandle = aCodePlan->tableRef->tableHandle;
    }
    else
    {
        sTableType   = aDataPlan->deletePartInfo->tableFlag & SMI_TABLE_TYPE_MASK;
        sTableHandle = aDataPlan->deleteTuple->tableHandle;
    }

    /* check null */
    if ( sTableType == SMI_TABLE_DISK )
    {
        SMI_MAKE_VIRTUAL_NULL_GRID( nullRID );
            
        IDE_TEST_RAISE( SC_GRID_IS_EQUAL( nullRID,
                                          aDataPlan->deleteTuple->rid ),
                        ERR_MODIFY_UNABLE_RECORD );
    }
    else
    {
        IDE_TEST( smiGetTableNullRow( sTableHandle,
                                      (void **) & nullRow,
                                      & nullRID )
                  != IDE_SUCCESS );        

        IDE_TEST_RAISE( nullRow == aDataPlan->deleteTuple->row,
                        ERR_MODIFY_UNABLE_RECORD );
    }
        
    // PROJ-2204 join update, delete
    // tuple ������ cursor�� �����ؾ��Ѵ�.
    IDE_TEST( aDataPlan->deleteCursor->setRowPosition( aDataPlan->deleteTuple->row,
                                                       aDataPlan->deleteTuple->rid )
              != IDE_SUCCESS );
        
    /* �ߺ� delete���� üũ */
    IDE_TEST( aDataPlan->deleteCursor->isUpdatedRowBySameStmt( &sIsDupDelete )
              != IDE_SUCCESS );

    IDE_TEST_RAISE( sIsDupDelete == ID_TRUE, ERR_MODIFY_UNABLE_RECORD );

    return IDE_SUCCESS;
    
    IDE_EXCEPTION( ERR_MODIFY_UNABLE_RECORD );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMN_MODIFY_UNABLE_RECORD ) ) ;
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmnDETE::firstInitMultiTable( qcTemplate * aTemplate,
                                     qmncDETE   * aCodePlan,
                                     qmndDETE   * aDataPlan )
{
    qmmDelMultiTables * sTmp = NULL;
    qmnCursorInfo     * sCursorInfo = NULL;
    qmsPartitionRef   * sPartitionRef = NULL;
    UInt                sPartitionCount;
    UInt                i;
    UInt                j;

    // Tuple Set������ �ʱ�ȭ
    aDataPlan->deleteTuple   = NULL;
    aDataPlan->deleteCursor  = NULL;
    aDataPlan->deleteTupleID = ID_USHORT_MAX;

    /* PROJ-2464 hybrid partitioned table ���� */
    aDataPlan->deletePartInfo = NULL;

    // index table cursor �ʱ�ȭ
    aDataPlan->indexDeleteCursor = NULL;
    aDataPlan->indexDeleteTuple = NULL;

    // where column list �ʱ�ȭ
    // set, where column list �ʱ�ȭ
    smiInitDMLRetryInfo( &(aDataPlan->retryInfo) );

    /* PROJ-2359 Table/Partition Access Option */
    aDataPlan->accessOption = QCM_ACCESS_OPTION_READ_WRITE;

    IDU_FIT_POINT("qmnDelete::firstInitMultiTable::alloc::mTableArray",
                  idERR_ABORT_InsufficientMemory);
    IDE_TEST( aTemplate->stmt->qmxMem->alloc( ID_SIZEOF(qmndDelMultiTables) * aCodePlan->mMultiTableCount,
                                              (void**)&aDataPlan->mTableArray)
              != IDE_SUCCESS );

    //--------------------------------
    // cursorInfo ����
    //--------------------------------
    for ( sTmp = aCodePlan->mTableList, j = 0;
          sTmp != NULL;
          sTmp = sTmp->mNext, j++ )
    {
        QMND_DETE_MULTI_TABLES( &aDataPlan->mTableArray[j] );

        if ( sTmp->mInsteadOfTrigger == ID_TRUE )
        {
            // instead of trigger�� cursor�� �ʿ����.
            // Nothing to do.
        }
        else
        {
            //--------------------------------
            // cursorInfo ����
            //--------------------------------
            IDU_FIT_POINT("qmnDelete::firstInitMultiTable::alloc::sCursorInfo",
                          idERR_ABORT_InsufficientMemory);
            IDE_TEST( aTemplate->stmt->qmxMem->alloc( ID_SIZEOF(qmnCursorInfo),
                                                      (void**)& sCursorInfo )
                      != IDE_SUCCESS );

            // cursorInfo �ʱ�ȭ
            sCursorInfo->cursor              = NULL;
            sCursorInfo->selectedIndex       = NULL;
            sCursorInfo->selectedIndexTuple  = NULL;
            sCursorInfo->accessOption        = QCM_ACCESS_OPTION_READ_WRITE;
            sCursorInfo->updateColumnList    = NULL;
            sCursorInfo->cursorType          = SMI_DELETE_CURSOR;
            sCursorInfo->isRowMovementUpdate = ID_FALSE;
            sCursorInfo->inplaceUpdate       = ID_FALSE;
            sCursorInfo->lockMode            = SMI_LOCK_WRITE;

            /* PROJ-2464 hybrid partitioned table ���� */
            sCursorInfo->stmtRetryColLst     = sTmp->mWhereColumnList;
            sCursorInfo->rowRetryColLst      = NULL;

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

                // cursorInfo ����
                IDU_FIT_POINT("qmnDelete::firstInitMultiTable::alloc::sCursorInfo2",
                              idERR_ABORT_InsufficientMemory);
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
                    sCursorInfo->updateColumnList    = NULL;
                    sCursorInfo->cursorType          = SMI_DELETE_CURSOR;
                    sCursorInfo->isRowMovementUpdate = ID_FALSE;
                    sCursorInfo->inplaceUpdate       = ID_FALSE;
                    sCursorInfo->lockMode            = SMI_LOCK_WRITE;

                    /* PROJ-2464 hybrid partitioned table ���� */
                    sCursorInfo->stmtRetryColLst     = sTmp->mWherePartColumnList[i];
                    sCursorInfo->rowRetryColLst      = NULL;

                    // cursorInfo ����
                    aTemplate->tmplate.rows[sPartitionRef->table].cursorInfo = sCursorInfo;
                }
            }
            else
            {
                /* Nothing to do */
            }
        }
    }

    aDataPlan->limitStart = 1;
    aDataPlan->limitEnd   = 0;
    aDataPlan->returnRow = NULL;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmnDETE::doItFirstMultiTable( qcTemplate * aTemplate,
                                     qmnPlan    * aPlan,
                                     qmcRowFlag * aFlag )
{
    qmncDETE          * sCodePlan = (qmncDETE*) aPlan;
    qmndDETE          * sDataPlan = (qmndDETE*) (aTemplate->tmplate.data + aPlan->offset);
    qmmDelMultiTables * sTmp;
    idBool              sIsSkip = ID_FALSE;
    idBool              sSkipExist = ID_FALSE;
    ULong               sDeleteCount = 0;
    UInt                i;

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
            IDE_TEST( checkTriggerMultiTable( aTemplate,
                                              sDataPlan,
                                              sTmp,
                                              i )
                      != IDE_SUCCESS );

            sDataPlan->deleteTuple = &aTemplate->tmplate.rows[sTmp->mTableRef->table];

            if ( sTmp->mInsteadOfTrigger == ID_TRUE )
            {
                IDE_TEST( fireInsteadOfTriggerMultiTable( aTemplate,
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
                                        sDataPlan->deleteTuple->tableHandle )
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

                // delete one record
                IDE_TEST( deleteOneRowMultiTable( aTemplate,
                                                  sDataPlan,
                                                  sTmp,
                                                  i )
                          != IDE_SUCCESS );
                sDeleteCount++;
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
            if ( sDeleteCount < 1 )
            {
                aTemplate->numRows--;
            }
            else if ( sDeleteCount == 1 )
            {
                /* Nothing to do */
            }
            else
            {
                aTemplate->numRows += ( sDeleteCount - 1 );
            }
        }
        else
        {
            aTemplate->numRows += ( sDeleteCount - 1 );
        }

        sDataPlan->doIt = qmnDETE::doItNextMultiTable;
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmnDETE::doItNextMultiTable( qcTemplate * aTemplate,
                                    qmnPlan    * aPlan,
                                    qmcRowFlag * aFlag )
{
    qmncDETE * sCodePlan = (qmncDETE*) aPlan;
    qmndDETE * sDataPlan = (qmndDETE*) (aTemplate->tmplate.data + aPlan->offset);
    qmmDelMultiTables * sTmp;
    idBool              sIsSkip = ID_FALSE;
    idBool              sSkipExist = ID_FALSE;
    ULong               sDeleteCount = 0;
    UInt                i;

    // doIt left child
    IDE_TEST( aPlan->left->doIt( aTemplate, aPlan->left, aFlag )
              != IDE_SUCCESS );

    if ( ( *aFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST )
    {
        for ( sTmp = sCodePlan->mTableList, i = 0;
              sTmp != NULL;
              sTmp = sTmp->mNext, i++ )
        {
            sDataPlan->deleteTuple = &aTemplate->tmplate.rows[sTmp->mTableRef->table];

            if ( sTmp->mInsteadOfTrigger == ID_TRUE )
            {
                IDE_TEST( fireInsteadOfTriggerMultiTable( aTemplate,
                                                          sDataPlan,
                                                          sTmp,
                                                          i )
                          != IDE_SUCCESS );
            }
            else
            {
                sIsSkip = ID_FALSE;
                /* PROJ-2359 Table/Partition Access Option */
                if ( sTmp->mTableRef->partitionRef != NULL )
                {
                    IDE_TEST( qmx::checkAccessOptionForExistentRecord(
                                        sDataPlan->accessOption,
                                        sDataPlan->deleteTuple->tableHandle )
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

                // delete one record
                IDE_TEST( deleteOneRowMultiTable( aTemplate,
                                                  sDataPlan,
                                                  sTmp,
                                                  i )
                         != IDE_SUCCESS );
                sDeleteCount++;
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
            if ( sDeleteCount < 1 )
            {
                aTemplate->numRows--;
            }
            else if ( sDeleteCount == 1 )
            {
                /* Nothing to do */
            }
            else
            {
                aTemplate->numRows += ( sDeleteCount - 1 );
            }
        }
        else
        {
            aTemplate->numRows += ( sDeleteCount - 1 );
        }
    }
    else
    {
        // record�� ���� ���
        // ���� ������ ���� ���� ���� �Լ��� ������.
        sDataPlan->doIt = qmnDETE::doItFirst;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmnDETE::getCursorMultiTable( qcTemplate        * aTemplate,
                                     qmncDETE          * aCodePlan,
                                     qmndDETE          * aDataPlan,
                                     qmmDelMultiTables * aTable,
                                     idBool            * aIsSkip )
{
    qmnCursorInfo * sCursorInfo = NULL;
    UShort          sTupleID = 0;
    ULong           sFlag = 0;

    if ( aTable->mTableRef->partitionRef == NULL )
    {
        if ( aDataPlan->deleteTupleID != aTable->mTableRef->table )
        {
            aDataPlan->deleteTupleID = aTable->mTableRef->table;

            sCursorInfo = (qmnCursorInfo *)aTemplate->tmplate.rows[aDataPlan->deleteTupleID].cursorInfo;

            IDE_TEST_RAISE( sCursorInfo == NULL, ERR_NOT_FOUND );

            aDataPlan->deleteCursor = sCursorInfo->cursor;

            /* PROJ-2464 hybrid partitioned table ���� */
            aDataPlan->deletePartInfo = aTable->mTableRef->tableInfo;
            aDataPlan->retryInfo.mIsWithoutRetry  = aCodePlan->withoutRetry;
            aDataPlan->retryInfo.mStmtRetryColLst = sCursorInfo->stmtRetryColLst;

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
        if ( aDataPlan->deleteTupleID != aDataPlan->deleteTuple->partitionTupleID )
        {
            sTupleID = aDataPlan->deleteTupleID;
            aDataPlan->deleteTupleID = aDataPlan->deleteTuple->partitionTupleID;

            // partition�� cursor�� ��´�.
            sCursorInfo = (qmnCursorInfo*)
                aTemplate->tmplate.rows[aDataPlan->deleteTupleID].cursorInfo;

            sFlag = aTemplate->tmplate.rows[aDataPlan->deleteTupleID].lflag;

            if ( ( sFlag & MTC_TUPLE_PARTITIONED_TABLE_MASK )
                 == MTC_TUPLE_PARTITIONED_TABLE_TRUE )
            {
                *aIsSkip = ID_TRUE;
                aDataPlan->deleteTupleID = sTupleID;
                IDE_RAISE( normal_exit );
            }
            else
            {
                /* Nothing to do */
            }
            IDE_TEST_RAISE( sCursorInfo == NULL, ERR_NOT_FOUND );

            aDataPlan->deleteCursor = sCursorInfo->cursor;

            /* PROJ-2464 hybrid partitioned table ���� */
            IDE_TEST( smiGetTableTempInfo( aDataPlan->deleteTuple->tableHandle,
                                           (void **)&(aDataPlan->deletePartInfo) )
                      != IDE_SUCCESS );

            aDataPlan->retryInfo.mIsWithoutRetry  = aCodePlan->withoutRetry;
            aDataPlan->retryInfo.mStmtRetryColLst = sCursorInfo->stmtRetryColLst;

            /* PROJ-2359 Table/Partition Access Option */
            aDataPlan->accessOption = sCursorInfo->accessOption;
        }
        else
        {
            /* Nothing to do */
        }

        // index table cursor�� ��´�.
        if ( aDataPlan->indexDeleteCursor == NULL )
        {
            sCursorInfo = (qmnCursorInfo *)
                aTemplate->tmplate.rows[aTable->mTableRef->table].cursorInfo;

            IDE_TEST_RAISE( sCursorInfo == NULL, ERR_NOT_FOUND );

            aDataPlan->indexDeleteCursor = sCursorInfo->cursor;
            aDataPlan->indexDeleteTuple = sCursorInfo->selectedIndexTuple;
        }
        else
        {
            /* Nothing to do */
        }
    }

    IDE_EXCEPTION_CONT( normal_exit );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NOT_FOUND )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmnDETE::getCursorMultiTable",
                                  "cursor not found" ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmnDETE::deleteOneRowMultiTable( qcTemplate        * aTemplate,
                                        qmndDETE          * aDataPlan,
                                        qmmDelMultiTables * aTable,
                                        UInt                aIndex )
{
    smiValue             sWhereSmiValues[QC_MAX_COLUMN_COUNT];
    idBool               sIsDiskTableOrPartition = ID_FALSE;
    qmndDelMultiTables * sDataTable = NULL;

    sDataTable = &aDataPlan->mTableArray[aIndex];

    //-----------------------------------
    // copy old row
    //-----------------------------------
    if ( sDataTable->mNeedTriggerRow == ID_TRUE )
    {
        // OLD ROW REFERENCING�� ���� ����
        idlOS::memcpy( sDataTable->mOldRow,
                       aDataPlan->deleteTuple->row,
                       aDataPlan->deleteTuple->rowOffset );
    }
    else
    {
        // Nothing to do.
    }

    //-----------------------------------
    // PROJ-2334PMT
    // set delete trigger memory variable column info
    //-----------------------------------
    if ( ( sDataTable->mExistTrigger == ID_TRUE ) &&
         ( aTable->mTableRef->tableInfo->tablePartitionType == QCM_PARTITIONED_TABLE ) )
    {
        sDataTable->mColumnsForRow = aDataPlan->deletePartInfo->columns;
    }
    else
    {
        // Nothing To Do
    }

    //-----------------------------------
    // delete before trigger
    //-----------------------------------
    if ( sDataTable->mExistTrigger == ID_TRUE )
    {
        // PROJ-1359 Trigger
        // ROW GRANULARITY TRIGGER�� ����
        IDE_TEST( qdnTrigger::fireTrigger(
                      aTemplate->stmt,
                      aTemplate->stmt->qmxMem,
                      aTable->mTableRef->tableInfo,
                      QCM_TRIGGER_ACTION_EACH_ROW,
                      QCM_TRIGGER_BEFORE,
                      QCM_TRIGGER_EVENT_DELETE,
                      NULL,                        // UPDATE Column
                      aDataPlan->deleteCursor,     /* Table Cursor */
                      aDataPlan->deleteTuple->rid, /* Row GRID */
                      sDataTable->mOldRow,         // OLD ROW
                      sDataTable->mColumnsForRow,  // OLD ROW Column
                      NULL,                        // NEW ROW
                      NULL )                       // NEW ROW Column
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    //-----------------------------------
    // delete one row
    //-----------------------------------
    if ( aDataPlan->retryInfo.mIsWithoutRetry == ID_TRUE )
    {
        if ( aTable->mTableRef->tableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
        {
            sIsDiskTableOrPartition = QCM_TABLE_TYPE_IS_DISK( aDataPlan->deletePartInfo->tableFlag );
        }
        else
        {
            sIsDiskTableOrPartition = QCM_TABLE_TYPE_IS_DISK( aTable->mTableRef->tableInfo->tableFlag );
        }

        if ( sIsDiskTableOrPartition == ID_TRUE )
        {
            IDE_TEST( qmx::setChkSmiValueList( aDataPlan->deleteTuple->row,
                                               aDataPlan->retryInfo.mStmtRetryColLst,
                                               sWhereSmiValues,
                                               & (aDataPlan->retryInfo.mStmtRetryValLst) )
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

    IDE_TEST( aDataPlan->deleteCursor->deleteRow( &(aDataPlan->retryInfo) )
              != IDE_SUCCESS );

    // delete index table
    IDE_TEST( deleteIndexTableCursorMultiTable( aTemplate,
                                                aDataPlan,
                                                aTable,
                                                aIndex )
              != IDE_SUCCESS );

    //-----------------------------------
    // delete after trigger
    //-----------------------------------
    if ( sDataTable->mExistTrigger == ID_TRUE )
    {
        // PROJ-1359 Trigger
        // ROW GRANULARITY TRIGGER�� ����
        IDE_TEST( qdnTrigger::fireTrigger(
                      aTemplate->stmt,
                      aTemplate->stmt->qmxMem,
                      aTable->mTableRef->tableInfo,
                      QCM_TRIGGER_ACTION_EACH_ROW,
                      QCM_TRIGGER_AFTER,
                      QCM_TRIGGER_EVENT_DELETE,
                      NULL,                        // UPDATE Column
                      aDataPlan->deleteCursor,     /* Table Cursor */
                      aDataPlan->deleteTuple->rid, /* Row GRID */
                      sDataTable->mOldRow,           // OLD ROW
                      sDataTable->mColumnsForRow,    // OLD ROW Column
                      NULL,                        // NEW ROW
                      NULL )                       // NEW ROW Column
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    if ( ( *aDataPlan->flag & QMND_DETE_REMOVE_MASK )
         == QMND_DETE_REMOVE_FALSE )
    {
        *aDataPlan->flag &= ~QMND_DETE_REMOVE_MASK;
        *aDataPlan->flag |= QMND_DETE_REMOVE_TRUE;
    }
    else
    {
        // Nothing to do.
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmnDETE::checkSkipMultiTable( qcTemplate        * aTemplate,
                                     qmndDETE          * aDataPlan,
                                     qmmDelMultiTables * aTable,
                                     idBool            * aIsSkip )
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
        sTableType   = aDataPlan->deletePartInfo->tableFlag & SMI_TABLE_TYPE_MASK;
        sTableHandle = aDataPlan->deleteTuple->tableHandle;
    }

    if ( aDataPlan->deleteTuple->row == NULL )
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

        if ( SC_GRID_IS_EQUAL( sNullRID, aDataPlan->deleteTuple->rid )
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
                                      (void **)&sNullRow,
                                      &sNullRID )
                  != IDE_SUCCESS );

        if ( sNullRow == aDataPlan->deleteTuple->row )
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
    IDE_TEST( aDataPlan->deleteCursor->setRowPosition( aDataPlan->deleteTuple->row,
                                                       aDataPlan->deleteTuple->rid )
              != IDE_SUCCESS );

    /* �ߺ� delete���� üũ */
    IDE_TEST( aDataPlan->deleteCursor->isUpdatedRowBySameStmt( &sIsSkip )
              != IDE_SUCCESS );

    IDE_EXCEPTION_CONT( normal_exit );

    *aIsSkip = sIsSkip;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmnDETE::deleteIndexTableCursorMultiTable( qcTemplate        * aTemplate,
                                                  qmndDETE          * aDataPlan,
                                                  qmmDelMultiTables * aTable,
                                                  UInt                aIndex )
{
    qmndDelMultiTables * sDataTable = NULL;

    sDataTable = &aDataPlan->mTableArray[aIndex];

    // update index table
    if ( ( sDataTable->mFlag & QMND_DETE_INDEX_CURSOR_MASK )
         == QMND_DETE_INDEX_CURSOR_INITED )
    {
        if ( aTable->mTableRef->selectedIndexTable != NULL )
        {
            IDE_TEST( aDataPlan->indexDeleteCursor->deleteRow()
                      != IDE_SUCCESS );
        }
        else
        {
            /* Nothing to do */
        }
        // �ٸ� index table�� update
        IDE_TEST( qmsIndexTable::deleteIndexTableCursors(
                      aTemplate->stmt,
                      & (sDataTable->mIndexTableCursorInfo),
                      aDataPlan->deleteTuple->rid )
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

IDE_RC qmnDETE::allocIndexTableCursorMultiTable( qcTemplate * aTemplate,
                                                 qmncDETE   * aCodePlan,
                                                 qmndDETE   * aDataPlan )
{
    qmmDelMultiTables  * sTmp = NULL;
    qmndDelMultiTables * sDataTable = NULL;
    UInt                i;

    for ( sTmp = aCodePlan->mTableList, i = 0;
          sTmp != NULL;
          sTmp = sTmp->mNext, i++ )
    {
        if ( sTmp->mTableRef->indexTableRef != NULL )
        {
            sDataTable = &aDataPlan->mTableArray[i];
            IDE_TEST( qmsIndexTable::initializeIndexTableCursors(
                          aTemplate->stmt,
                          sTmp->mTableRef->indexTableRef,
                          sTmp->mTableRef->indexTableCount,
                          sTmp->mTableRef->selectedIndexTable,
                          & (sDataTable->mIndexTableCursorInfo) )
                      != IDE_SUCCESS );
            sDataTable->mFlag &= ~QMND_DETE_INDEX_CURSOR_MASK;
            sDataTable->mFlag |= QMND_DETE_INDEX_CURSOR_INITED;
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

IDE_RC qmnDETE::closeCursorMultiTable( qcTemplate * aTemplate,
                                       qmnPlan    * aPlan )
{
    qmncDETE * sCodePlan = (qmncDETE*) aPlan;
    qmndDETE * sDataPlan =
        (qmndDETE*) (aTemplate->tmplate.data + aPlan->offset);
    qmndDelMultiTables * sDataTable = NULL;
    UInt                 i;

    for ( i = 0; i < sCodePlan->mMultiTableCount; i++ )
    {
        sDataTable = &sDataPlan->mTableArray[i];

        if ( ( sDataTable->mFlag & QMND_DETE_INDEX_CURSOR_MASK )
             == QMND_DETE_INDEX_CURSOR_INITED )
        {
            sDataTable->mFlag &= ~QMND_DETE_INDEX_CURSOR_MASK;
            sDataTable->mFlag |= QMND_DETE_INDEX_CURSOR_NONE;

            IDE_TEST( qmsIndexTable::closeIndexTableCursors(
                          & (sDataTable->mIndexTableCursorInfo) )
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
        sDataTable = &sDataPlan->mTableArray[i];

        if ( ( sDataTable->mFlag & QMND_DETE_INDEX_CURSOR_MASK )
             == QMND_DETE_INDEX_CURSOR_INITED )
        {
            qmsIndexTable::finalizeIndexTableCursors( &(sDataTable->mIndexTableCursorInfo) );
        }
        else
        {
            /* Nothing to do */
        }
    }
    return IDE_FAILURE;
}

IDE_RC qmnDETE::allocTriggerRowMultiTable( qcTemplate * aTemplate,
                                           qmncDETE   * aCodePlan,
                                           qmndDETE   * aDataPlan )
{
    qmmDelMultiTables * sTmp = NULL;
    UInt                i;
    UInt                sMaxRowOffset = 0;

    for ( sTmp = aCodePlan->mTableList, i = 0;
          sTmp != NULL;
          sTmp = sTmp->mNext, i++ )
    {
        if ( sTmp->mTableRef->tableInfo->triggerCount > 0 )
        {
            if ( sTmp->mInsteadOfTrigger == ID_TRUE )
            {
                IDU_FIT_POINT("qmnDelete::allocTriggerRowMultiTable::alloc::mOldRow",
                              idERR_ABORT_InsufficientMemory);
                IDE_TEST( aTemplate->stmt->qmxMem->alloc(
                            ID_SIZEOF(smiValue) *
                            sTmp->mTableRef->tableInfo->columnCount,
                            (void**) & aDataPlan->mTableArray[i].mOldRow )
                          != IDE_SUCCESS);
            }
            else
            {
                sMaxRowOffset = qmx::getMaxRowOffset( &(aTemplate->tmplate),
                                                      sTmp->mTableRef );

                IDU_FIT_POINT("qmnDelete::allocTriggerRowMultiTable::alloc::mOldRow2",
                              idERR_ABORT_InsufficientMemory);
                if ( sMaxRowOffset > 0 )
                {
                    IDE_TEST( aTemplate->stmt->qmxMem->alloc(
                                sMaxRowOffset,
                                (void**) & aDataPlan->mTableArray[i].mOldRow )
                              != IDE_SUCCESS);
                }
                else
                {
                    aDataPlan->mTableArray[i].mOldRow = NULL;
                }
            }

            aDataPlan->mTableArray[i].mColumnsForRow = sTmp->mTableRef->tableInfo->columns;
            aDataPlan->mTableArray[i].mNeedTriggerRow = ID_FALSE;
            aDataPlan->mTableArray[i].mExistTrigger = ID_TRUE;
        }
        else
        {
            aDataPlan->mTableArray[i].mColumnsForRow = NULL,
            aDataPlan->mTableArray[i].mNeedTriggerRow = ID_FALSE;
            aDataPlan->mTableArray[i].mExistTrigger = ID_FALSE;
        }
    }

    aDataPlan->oldRow = NULL;
    aDataPlan->needTriggerRow = ID_FALSE;
    aDataPlan->existTrigger = ID_FALSE;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmnDETE::checkTriggerMultiTable( qcTemplate        * aTemplate,
                                        qmndDETE          * aDataPlan,
                                        qmmDelMultiTables * aTable,
                                        UInt                aIndex )
{
    qmndDelMultiTables * sDataTable = NULL;
    idBool               sNeedTriggerRow = ID_FALSE;

    sDataTable = &aDataPlan->mTableArray[aIndex];

    if ( sDataTable->mExistTrigger == ID_TRUE )
    {
        if ( aTable->mInsteadOfTrigger == ID_TRUE )
        {
            IDE_TEST( qdnTrigger::needTriggerRow(
                          aTemplate->stmt,
                          aTable->mTableRef->tableInfo,
                          QCM_TRIGGER_INSTEAD_OF,
                          QCM_TRIGGER_EVENT_DELETE,
                          NULL,
                          &sNeedTriggerRow )
                      != IDE_SUCCESS );

        }
        else
        {
            // Trigger�� ���� Referencing Row�� �ʿ������� �˻�
            IDE_TEST( qdnTrigger::needTriggerRow(
                          aTemplate->stmt,
                          aTable->mTableRef->tableInfo,
                          QCM_TRIGGER_BEFORE,
                          QCM_TRIGGER_EVENT_DELETE,
                          NULL,
                          &sNeedTriggerRow )
                      != IDE_SUCCESS );

            if ( sNeedTriggerRow == ID_FALSE )
            {
                IDE_TEST( qdnTrigger::needTriggerRow(
                              aTemplate->stmt,
                              aTable->mTableRef->tableInfo,
                              QCM_TRIGGER_AFTER,
                              QCM_TRIGGER_EVENT_DELETE,
                              NULL,
                              &sNeedTriggerRow )
                          != IDE_SUCCESS );

            }
            else
            {
                /* Nothing to do */
            }
        }

        sDataTable->mNeedTriggerRow = sNeedTriggerRow;
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmnDETE::fireInsteadOfTriggerMultiTable( qcTemplate        * aTemplate,
                                                qmndDETE          * aDataPlan,
                                                qmmDelMultiTables * aTable,
                                                UInt                aIndex )
{
    qmndDelMultiTables * sDataTable = NULL;
    qcmTableInfo       * sTableInfo = NULL;
    mtcColumn          * sColumn    = NULL;
    mtcStack           * sStack     = NULL;
    SInt                 sRemain    = 0;
    UShort               i          = 0;

    sDataTable = &aDataPlan->mTableArray[aIndex];

    sTableInfo = aTable->mTableRef->tableInfo;

    if ( sDataTable->mNeedTriggerRow == ID_TRUE )
    {
        sStack = aTemplate->tmplate.stack;
        sRemain = aTemplate->tmplate.stackRemain;

        IDE_TEST_RAISE( sRemain < aDataPlan->deleteTuple->columnCount,
                        ERR_STACK_OVERFLOW );

        // DELETE�� VIEW ���̿� FILT ���� �ٸ� ���鿡 ���� stack�� ����Ǿ��� �� �����Ƿ�
        // stack�� view tuple�� �÷����� �缳���Ѵ�.
        for ( i = 0, sColumn = aDataPlan->deleteTuple->columns;
              i < aDataPlan->deleteTuple->columnCount;
              i++, sColumn++, sStack++ )
        {
            sStack->column = sColumn;
            sStack->value  =
                (void*)((SChar*)aDataPlan->deleteTuple->row + sColumn->column.offset);
        }

        /* PROJ-2464 hybrid partitioned table ���� */
        if ( sTableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
        {
            if ( aDataPlan->deletePartInfo != NULL )
            {
                if ( aDataPlan->deleteTuple->tableHandle != aDataPlan->deletePartInfo->tableHandle )
                {
                    IDE_TEST( smiGetTableTempInfo( aDataPlan->deleteTuple->tableHandle,
                                                   (void **)&(aDataPlan->deletePartInfo) )
                              != IDE_SUCCESS );
                }
                else
                {
                    /* Nothing to do */
                }
            }
            else
            {
                IDE_TEST( smiGetTableTempInfo( aDataPlan->deleteTuple->tableHandle,
                                               (void **)&(aDataPlan->deletePartInfo) )
                          != IDE_SUCCESS );
            }

            sTableInfo = aDataPlan->deletePartInfo;
            sDataTable->mColumnsForRow = aDataPlan->deletePartInfo->columns;
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
    }
    else
    {
        /* Nothing to do */
    }

    if ( sDataTable->mExistTrigger == ID_TRUE )
    {
        // instead of trigger
        IDE_TEST( qdnTrigger::fireTrigger(
                      aTemplate->stmt,
                      aTemplate->stmt->qmxMem,
                      aTable->mTableRef->tableInfo,
                      QCM_TRIGGER_ACTION_EACH_ROW,
                      QCM_TRIGGER_INSTEAD_OF,
                      QCM_TRIGGER_EVENT_DELETE,
                      NULL,                        // UPDATE Column
                      NULL,                        /* Table Cursor */
                      SC_NULL_GRID,                /* Row GRID */
                      sDataTable->mOldRow,         // OLD ROW
                      sDataTable->mColumnsForRow,  // OLD ROW Column
                      NULL,                        // NEW ROW
                      NULL )                       // NEW ROW Column
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

IDE_RC qmnDETE::checkDeleteRefMultiTable( qcTemplate        * aTemplate,
                                          qmnPlan           * /*aPlan*/,
                                          qmmDelMultiTables * aTable,
                                          UInt                /*aIndex*/ )
{
    qmsPartitionRef    * sPartitionRef;
    smiStatement         sSmiStmt;
    smiStatement       * sSmiStmtOrg;
    UInt                 sStage = 0;

    if ( aTable->mChildConstraints != NULL )
    {
        // BUG-17940 parent key�� �����ϰ� child key�� ã����
        // parent row�� lock�� ���� ���� view�� ��������
        // ���ο� smiStmt�� �̿��Ѵ�.
        // Delete cascade �ɼǿ� ����ؼ� normal�� �Ѵ�.
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
            IDE_TEST( checkDeleteChildRefOnScanMultiTable(
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
                IDE_TEST( checkDeleteChildRefOnScanMultiTable(
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
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sStage == 1 )
    {
        qcg::setSmiStmt( aTemplate->stmt, sSmiStmtOrg );

        if ( sSmiStmt.end(SMI_STATEMENT_RESULT_FAILURE) != IDE_SUCCESS )
        {
            IDE_CALLBACK_FATAL("Check Child Key On Delete smiStmt.end() failed");
        }
    }
    return IDE_FAILURE;
}

IDE_RC qmnDETE::checkDeleteChildRefOnScanMultiTable( qcTemplate        * aTemplate,
                                                     qmmDelMultiTables * aTable,
                                                     qcmTableInfo      * aTableInfo,
                                                     mtcTuple          * aDeleteTuple )
{
    iduMemoryStatus   sQmxMemStatus;
    void            * sOrgRow;
    void            * sSearchRow;
    smiTableCursor  * sDeleteCursor;
    qmnCursorInfo   * sCursorInfo;

    sCursorInfo = (qmnCursorInfo*) aDeleteTuple->cursorInfo;
    IDE_TEST_RAISE( sCursorInfo == NULL, ERR_NOT_FOUND );
    sDeleteCursor = sCursorInfo->cursor;

    // PROJ-1624 non-partitioned index
    // index table scan���� open���� ���� partition�� �����Ѵ�.
    if ( sDeleteCursor != NULL )
    {
        IDE_TEST( sDeleteCursor->beforeFirstModified( SMI_FIND_MODIFIED_OLD )
                  != IDE_SUCCESS );

        //------------------------------------------
        // Referencing �˻縦 ���� ������ Row���� �˻�
        //------------------------------------------
        sOrgRow = sSearchRow = aDeleteTuple->row;
        IDE_TEST( sDeleteCursor->readOldRow( (const void**) & sSearchRow,
                                             &aDeleteTuple->rid )
                  != IDE_SUCCESS );
        aDeleteTuple->row = ( sSearchRow == NULL ) ? sOrgRow : sSearchRow;

        while ( sSearchRow != NULL )
        {
            // Memory ������ ���Ͽ� ���� ��ġ ���
            IDE_TEST_RAISE( aTemplate->stmt->qmxMem->getStatus(&sQmxMemStatus) != IDE_SUCCESS, ERR_MEM_OP );

            //------------------------------------------
            // Child Table�� ���� Referencing �˻�
            //------------------------------------------
            IDE_TEST( qdnForeignKey::checkChildRefOnDelete(
                          aTemplate->stmt,
                          aTable->mChildConstraints,
                          aTableInfo->tableID,
                          aDeleteTuple,
                          aDeleteTuple->row,
                          ID_TRUE )
                      != IDE_SUCCESS );
            // Memory ������ ���� Memory �̵�
            IDE_TEST_RAISE( aTemplate->stmt->qmxMem->setStatus(&sQmxMemStatus) != IDE_SUCCESS, ERR_MEM_OP );

            sOrgRow = sSearchRow = aDeleteTuple->row;

            IDE_TEST( sDeleteCursor->readOldRow( (const void**) & sSearchRow,
                                                 & aDeleteTuple->rid )
                       != IDE_SUCCESS );

            aDeleteTuple->row = (sSearchRow == NULL) ? sOrgRow : sSearchRow;
        }
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_MEM_OP )
    {
        ideLog::log( IDE_ERR_0,
                     "Unexpected errors may have occurred:"
                     " qmnDETE::checkDeleteChildRefOnScanMultiTable"
                     " memory error" );
    }
    IDE_EXCEPTION( ERR_NOT_FOUND )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmnDETE::checkDeleteChildRefOnScanMultiTable",
                                  "cursor not found" ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

