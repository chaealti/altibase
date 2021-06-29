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
 * $Id: rpsSmExecutor.cpp 88285 2020-08-05 05:49:37Z bethy $
 **********************************************************************/

#include <idl.h>
#include <ide.h>

#include <smErrorCode.h>
#include <qci.h>

#include <qtcDef.h>

#include <rp.h>
#include <rpDef.h>
#include <rpuProperty.h>
#include <rpxReceiver.h>
#include <rpsSmExecutor.h>
#include <rpdCatalog.h>

extern mtdModule mtdVarchar;

extern "C" int
compareColumnID(const void* aElem1, const void* aElem2)
{
/***********************************************************************
 *
 * Description :
 *    compare columnID
 *
 * Implementation :
 *
 ***********************************************************************/

    IDE_DASSERT((smiFetchColumnList*)aElem1 != NULL);
    IDE_DASSERT((smiFetchColumnList*)aElem2 != NULL);

    //--------------------------------
    // compare columnID
    //--------------------------------

    if(((smiFetchColumnList*)aElem1)->column->id >
       ((smiFetchColumnList*)aElem2)->column->id)
    {
        return 1;
    }
    else if(((smiFetchColumnList*)aElem1)->column->id <
            ((smiFetchColumnList*)aElem2)->column->id)
    {
        return -1;
    }
    else
    {
        // �ε������� ������ �÷��� ������ �� ����.
        IDE_ASSERT(0);
    }
}

rpsSmExecutor::rpsSmExecutor()
{
}

IDE_RC rpsSmExecutor::initialize( idvSQL  * aOpStatistics,
                                  rpdMeta * aMeta,
                                  idBool    aIsConflictWhenNotEnoughSpace )
{
    UInt               i;
    UInt               sRowSize      = 0;
    UInt               sMaxRowSize   = 0;
    SInt               sItemCount;
    const void       * sTable;
    qciIndexTableRef * sIndexTable;

    mIsConflictWhenNotEnoughSpace = aIsConflictWhenNotEnoughSpace;
    mCursorOpenFlag = SMI_INSERT_METHOD_NORMAL;

    for(i = 0; i < QCI_MAX_COLUMN_COUNT - 1; i++)
    {
        mUpdateColList[i].next = & mUpdateColList[i+1];
    }
    mUpdateColList[ QCI_MAX_COLUMN_COUNT -1 ].next = NULL;
    mRealRow = NULL;
    mOpStatistics = aOpStatistics;
    mDeleteRowCount = 0;

    /*
     * PROJ-1705
     * ��ũ ���̺��� ���, �ش� ���̺��� ���ڵ� �ִ� ������ ��ŭ��
     * �Ҵ� �޴´�. �޸𸮸� �̸� �޾Ƴ�������,
     * �ݺ����� Memory �Ҵ�/���� �۾��� overhead�� ���̷��� �õ�
     */
    for(sItemCount = 0; sItemCount < aMeta->mReplication.mItemCount; sItemCount++)
    {
        sTable = smiGetTable( (smOID)aMeta->mItems[sItemCount].mItem.mTableOID );

        if((SMI_MISC_TABLE_HEADER(sTable)->mFlag & SMI_TABLE_TYPE_MASK)
           == SMI_TABLE_DISK)
        {
            IDE_TEST(qciMisc::getDiskRowSize((void *)sTable,
                                             &sRowSize)
                     != IDE_SUCCESS);

            /* ���� ū Row Size�� �����Ѵ�. */
            if(sMaxRowSize < sRowSize)
            {
                sMaxRowSize = sRowSize;
            }
        }

        /*
         * PROJ-1624 non-partitioned index
         * global index table�� �ִ� ��� mRealRow�� �����ϹǷ�
         * global index table�� record size�� �Բ� ����Ѵ�.
         */
        for ( sIndexTable = aMeta->mItems[sItemCount].mIndexTableRef;
              sIndexTable != NULL;
              sIndexTable = sIndexTable->next )
        {
            // index table�� �׻� disk table�̴�.
            IDE_DASSERT( (SMI_MISC_TABLE_HEADER(sIndexTable->tableHandle)->mFlag
                          & SMI_TABLE_TYPE_MASK)
                         == SMI_TABLE_DISK );
            
            IDE_TEST(qciMisc::getDiskRowSize((void *)sIndexTable->tableHandle,
                                             &sRowSize)
                     != IDE_SUCCESS);

            /* ���� ū Row Size�� �����Ѵ�. */
            if(sMaxRowSize < sRowSize)
            {
                sMaxRowSize = sRowSize;
            }
        }
    }

    if(sMaxRowSize > 0)
    {
        sMaxRowSize = idlOS::align(sMaxRowSize, 8);
        IDE_TEST_RAISE(iduMemMgr::malloc(IDU_MEM_RP_RPS,
                                         sMaxRowSize,
                                         (void **)&mRealRow,
                                         IDU_MEM_IMMEDIATE)
                       != IDE_SUCCESS, ERR_MEMORY_ALLOC_REAL_ROW);
        mRealRowSize = sMaxRowSize;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_MEMORY_ALLOC_REAL_ROW);
    {
        IDE_ERRLOG(IDE_RP_0);
        IDE_SET(ideSetErrorCode(rpERR_ABORT_MEMORY_ALLOC,
                                "rpsSmExecutor::initialize",
                                "mRealRow"));
    }
    IDE_EXCEPTION_END;
    IDE_PUSH();

    if(mRealRow != NULL)
    {
        (void)iduMemMgr::free(mRealRow);
        mRealRow = NULL;
    }

    IDE_POP();
    return IDE_FAILURE;
}

void rpsSmExecutor::destroy()
{
    if(mRealRow != NULL)
    {
        (void)iduMemMgr::free(mRealRow);
        mRealRow = NULL;
    }

    return;
}

/* [ INPUT ]
 * ( insert columns )
 * aXLog->mColCnt
 * aXLog->mACols
 */
IDE_RC rpsSmExecutor::executeInsert( smiTrans         * aTrans,
                                     rpdXLog          * aXLog,
                                     rpdMetaItem      * aMetaItem,
                                     qciIndexTableRef * aIndexTableRef,
                                     rpApplyFailType  * aFailType )
{
    smiTableCursor       sCursor;
    const void          *sTable;
    UInt                 sStep = 0;
    smiStatement         sSmiStmt;
    smiCursorProperties  sProperty;
    void                *sDummyRow;
    scGRID               sRowGRID;
    smiValue             sConvertCols[QCI_MAX_COLUMN_COUNT]; // PROJ-1705
    smOID                sCompressColOIDs[QCI_MAX_COLUMN_COUNT]; // PROJ-2397
    smiValue             sFinalColsValues[QCI_MAX_COLUMN_COUNT];
    smiValue            *sACols = NULL;
    idBool               sInserted = ID_FALSE;

    RP_CREATE_FLAG_VARIABLE(IDV_OPTM_INDEX_RP_R_INSERT_ROW);

    RP_OPTIMIZE_TIME_BEGIN(mOpStatistics, IDV_OPTM_INDEX_RP_R_INSERT_ROW);

    SC_MAKE_NULL_GRID( sRowGRID );

    *aFailType = RP_APPLY_FAIL_NONE;

    IDE_CLEAR();

    /* smiTableCursor::insertRow()���� ������� ���� �κ��� �ʱ�ȭ���� �ʴ´�.
     * idlOS::memset(sConvertCols, 0, ID_SIZEOF(smiValue) * QCI_MAX_COLUMN_COUNT);
     */

    SMI_CURSOR_PROP_INIT(&sProperty, NULL, NULL);

    sTable = smiGetTable( aMetaItem->mItem.mTableOID );

    /*
     * PROJ-1705
     *
     * Disk table�� insert value�� mtdValue�� �ƴ� value�� ���¸� ������.
     * sender���� ������ XLog�� after image���´� �ϰ������� mtdValue��
     * ���¸� �����Ƿ�, sm������ insert row �۾��ÿ� standby ������ ���̺���
     * Disk�� ���, value���� ��ȯ�۾��� �ʿ��ϴ�.
     * �� ��ġ�� ������, insert row���� conflict�߻���, �ļ� �۾�����
     * insert compare�ϸ� mtdValue�� ����ϰ� �ǹǷ�, XLog�� after image�� �����ϱ� ���ؼ��̴�.
     */
    if((SMI_MISC_TABLE_HEADER(sTable)->mFlag & SMI_TABLE_TYPE_MASK) == SMI_TABLE_DISK)
    {
        IDE_TEST(convertToNonMtdValue(aXLog, sConvertCols, sTable)
                 != IDE_SUCCESS);
    }
    else
    {
        /* BUG-30119 variable �÷��� fixed column�� ����ȭ�� ���, insert�� �����Ⱚ�� ���...*/
        IDE_TEST( convertXlogToSmiValue( aXLog, 
                                         sConvertCols, 
                                         sTable )
                  != IDE_SUCCESS );
    }


    // insert retry
retryInsert:

    IDE_TEST(sSmiStmt.begin( aTrans->getStatistics(),
                             aTrans->getStatement(),
                             SMI_STATEMENT_NORMAL | SMI_STATEMENT_ALL_CURSOR)
             != IDE_SUCCESS);

    sStep = 1;

    IDE_TEST( convertValueToOID( aXLog,
                                 sConvertCols,
                                 sFinalColsValues,
                                 sTable,
                                 &sSmiStmt,
                                 sCompressColOIDs )
              != IDE_SUCCESS );

    sACols = sFinalColsValues;

    sCursor.initialize();

    if ( cursorOpen( &sCursor,
                     &sSmiStmt,
                     sTable,
                     NULL,                    // index
                     smiGetRowSCN(sTable),
                     NULL,                    // columns
                     smiGetDefaultKeyRange(), // range
                     smiGetDefaultKeyRange(), // callback
                     smiGetDefaultFilter(),
                     SMI_LOCK_WRITE | SMI_TRAVERSE_FORWARD | SMI_PREVIOUS_DISABLE,
                     SMI_INSERT_CURSOR,
                     &sProperty )
         == IDE_SUCCESS )
    {
        /* do nothing */ 
    }
    else
    {
        IDE_TEST_RAISE( ideIsRebuild() != IDE_SUCCESS, ERR_CONFLICT );
        
        IDE_ERRLOG( IDE_RP_0 );

        IDE_CLEAR();
        
        sStep = 0;
        IDE_TEST( sSmiStmt.end(SMI_STATEMENT_RESULT_FAILURE) != IDE_SUCCESS );

        goto retryInsert;
    }
    sStep = 2;

    IDU_FIT_POINT( "1.TASK-2004@rpsSmExecutor::executeInsert" );

    IDU_FIT_POINT_RAISE( "rpsSmExecutor::executeInsert::insertRow::corrupted",
                         ERR_CONFLICT,
                         smERR_ABORT_PageCorrupted, 0, 0 );

    //BUG-22484 : sCursor.insertRow���� Retry������ �߻� �Ҽ� ����
    //���� insertRow : N, close Y --> Disk ����� Y , N  Memory N , Y
    if(sCursor.insertRow(sACols,     // PROJ-1705
                         &sDummyRow,
                         &sRowGRID)
       != IDE_SUCCESS)
    {
        IDE_TEST_RAISE(ideIsRetry() != IDE_SUCCESS, ERR_CONFLICT);

        IDE_CLEAR();

        sStep = 1;
        IDE_TEST(cursorClose(&sCursor) != IDE_SUCCESS);

        sStep = 0;
        IDE_TEST(sSmiStmt.end(SMI_STATEMENT_RESULT_FAILURE) != IDE_SUCCESS);

        // retry.
        RP_DBG_PRINTLINE();
        goto retryInsert;
    }
    else
    {
        sInserted = ID_TRUE;
    }

    sStep = 1;
    if(cursorClose(&sCursor) != IDE_SUCCESS)
    {
        IDE_TEST_RAISE(ideIsRetry() != IDE_SUCCESS, ERR_CONFLICT);

        IDE_CLEAR();

        sStep = 0;
        IDE_TEST(sSmiStmt.end(SMI_STATEMENT_RESULT_FAILURE) != IDE_SUCCESS);

        // retry.
        RP_DBG_PRINTLINE();
        goto retryInsert;
    }

    // PROJ-1624 non-partitioned index
    // insertRow�� �����ߴٸ� index table���� �ݵ�� insert�Ǿ�� �Ѵ�.
    if ( ( aIndexTableRef != NULL ) &&
         ( sInserted == ID_TRUE ) )
    {
        if( qciMisc::insertIndexTable4OneRow(
                            & sSmiStmt,
                            aIndexTableRef,
                            sACols,
                            aMetaItem->mItem.mTableOID,
                            sRowGRID )
            != IDE_SUCCESS )
        {
             IDE_TEST_RAISE( ( ideIsRetry() != IDE_SUCCESS ) || ( ideIsRebuild() != IDE_SUCCESS ),
                             ERR_INSERT_GLOBAL_INDEX_TABLE );

             IDE_CLEAR();

             sStep = 0;
             IDE_TEST(sSmiStmt.end(SMI_STATEMENT_RESULT_FAILURE) != IDE_SUCCESS);

             // retry.
             RP_DBG_PRINTLINE();
             goto retryInsert;

         }
         else
         {
             /*do nothing: insert to global index success*/
         }
    }
    else
    {
        // Nothing to do.
    }

    if ( aMetaItem->mQueueMsgIDSeq != NULL )
    {
        IDE_TEST_RAISE( correctQMsgIDSeq( &sSmiStmt, aXLog, aMetaItem ) != IDE_SUCCESS, ERR_CORRECT_Q );
    }
    else
    {
        /*do nothing*/
    }
    IDE_TEST(sSmiStmt.end(SMI_STATEMENT_RESULT_SUCCESS) != IDE_SUCCESS);
    sStep = 0;

    RP_OPTIMIZE_TIME_END(mOpStatistics, IDV_OPTM_INDEX_RP_R_INSERT_ROW);

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_CORRECT_Q );
    {
        IDE_ERRLOG( IDE_RP_0 );
    }
    IDE_EXCEPTION( ERR_INSERT_GLOBAL_INDEX_TABLE );
    {
        if ( RP_IS_CORRUPTED_PAGE() != ID_TRUE )
        {
            IDE_ERRLOG( IDE_RP_0 );
            IDE_SET( ideSetErrorCode( rpERR_ABORT_INSERT_GLOBAL_INDEX_TABLE ) );

            IDE_ERRLOG( IDE_RP_0 );

            *aFailType = RP_APPLY_FAIL_BY_CONFLICT;
        }
        else
        {
            *aFailType = RP_APPLY_FAIL_BY_CORRUPTED_PAGE;
        }
    }
    IDE_EXCEPTION(ERR_CONFLICT);
    {
        /* BUG-27573 If the receiver get not enough free space,
         *           then the receiver stop.
         * BUG-31851 The transaction needs rollback in eager mode.
         */
        if ( ( ideGetErrorCode() == smERR_ABORT_NOT_ENOUGH_SPACE ) &&
             ( mIsConflictWhenNotEnoughSpace == ID_FALSE ) )
        {
            /* Nothing to do */
        }
        else
        {
            /* ���� Primary Key�� ���� Row�� �̹� ����
             * Lock Timeout
             * Unique Violation
             * Eager Mode�� ��, Not Enough Space
             */
            if ( ( ideGetErrorCode() == smERR_ABORT_smnUniqueViolationInReplTrans ) ||
                 ( ideGetErrorCode() == smERR_ABORT_smcExceedLockTimeWait ) )
            {
                /* conflict resolution tx�� ���� lock�� ��� �����ߴٸ�, ������ ������ �۾��� �Ѵ�.
                   1. conflict resolution tx�� ���� ���, �� �۾��� conflict resolution tx�� �ְ� ��õ�
                   2. conflict resolution tx�� ������, �ٸ� tx�� conflict resolutionó������ ���̹Ƿ� ����ó�� */
                *aFailType = RP_APPLY_FAIL_BY_CONFLICT_RESOLUTION_TX;
            }
            else
            {
                if ( RP_IS_CORRUPTED_PAGE() != ID_TRUE )
                {
                    *aFailType = RP_APPLY_FAIL_BY_CONFLICT;
                }
                else
                {
                    *aFailType = RP_APPLY_FAIL_BY_CORRUPTED_PAGE;
                }
            }
        }
    }
    IDE_EXCEPTION_END;

    RP_OPTIMIZE_TIME_END(mOpStatistics, IDV_OPTM_INDEX_RP_R_INSERT_ROW);

    if ( *aFailType == RP_APPLY_FAIL_NONE )
    {
        *aFailType = RP_APPLY_FAIL_INTERNAL;
    }
    else
    {
        /* Nothing to do */
    }

    IDE_PUSH();

    switch (sStep)
    {
        case 2 :
            if(cursorClose(&sCursor) != IDE_SUCCESS)
            {
                IDE_ERRLOG(IDE_RP_0);
            }
        case 1 :
            if(sSmiStmt.end(SMI_STATEMENT_RESULT_FAILURE) != IDE_SUCCESS)
            {
                IDE_ERRLOG(IDE_RP_0);
            }
        default :
            break;
    }

    IDE_POP();

    return IDE_FAILURE;
}
/*
 * the sequence value of queue table msgid must be identical with msgid of xlog.
 */
IDE_RC rpsSmExecutor::correctQMsgIDSeq( smiStatement     * aSmiStmt,
                                        rpdXLog          * aXLog,
                                        rpdMetaItem      * aMetaItem )
{
    SLong          sQMsgIDSeqValue;
    UInt           i;
    SLong          sXLogValue = 0;
    rpdColumn     *sColumn = NULL;
    idBool         sIsMSGIDFound = ID_FALSE;

    IDE_DASSERT( aMetaItem->mQueueMsgIDSeq != NULL );

    IDE_TEST( smiTable::readSequence( aSmiStmt,
                                      aMetaItem->mQueueMsgIDSeq,
                                      SMI_SEQUENCE_NEXT,
                                      &sQMsgIDSeqValue,
                                      NULL )
              != IDE_SUCCESS );

    for( i = 0; i < aXLog->mColCnt; i ++ )
    {
        sColumn = aMetaItem->getRpdColumn( aXLog->mCIDs[i] );
        if ( idlOS::strncmp( sColumn->mColumnName, "MSGID", 5 ) == 0 )
        {
            sXLogValue = *((SLong*)aXLog->mACols[i].value);

            if ( sXLogValue != sQMsgIDSeqValue )
            {
                IDE_TEST( smiTable::setSequence( aSmiStmt,
                                                 aMetaItem->mQueueMsgIDSeq,
                                                 sXLogValue )
                          != IDE_SUCCESS );

                IDE_TEST( smiTable::readSequence( aSmiStmt,
                                                  aMetaItem->mQueueMsgIDSeq,
                                                  SMI_SEQUENCE_CURR,
                                                  &sQMsgIDSeqValue,
                                                  NULL )
                          != IDE_SUCCESS );
            }
            else
            {
                /*do nothing*/
            }
            IDE_TEST_RAISE( sQMsgIDSeqValue != sXLogValue , ERR_INTERNAL );
            sIsMSGIDFound = ID_TRUE;
            break;
        }
        else
        {
            /*do nothing*/
        }
    }

    IDE_TEST_RAISE( sIsMSGIDFound != ID_TRUE , ERR_INTERNAL_NOTFOUND );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INTERNAL );
    {
        IDE_SET( ideSetErrorCode( rpERR_ABORT_RP_INTERNAL_ARG, "Queue MsgID Sequence Value is not equal." ) );
    }
    IDE_EXCEPTION( ERR_INTERNAL_NOTFOUND );
    {
        IDE_SET( ideSetErrorCode( rpERR_ABORT_RP_INTERNAL_ARG, "Queue MsgID Sequence Value was not found." ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
/*
 * �޸����̺� �Ǵ� ���ڵ尡 �̹� ����ִ� ���̺� ���ؼ��� normal insert�� �Ѵ�.
 * Lob�÷��� �ִ°��, Lob�� cursor open�� ���� �ϹǷ�  direct path insert�� �浹�� ���� ������
 * normal insert�� �Ѵ�. (�浹���� : dpath insert�߿��� select �� DML�� ������ �� ����.)
 */
IDE_RC rpsSmExecutor::setCursorOpenFlag( smiTrans   * aTrans,
                                         const void * aTable,
                                         UInt       * aFlag )
{
    const smiColumn * sColumn       = NULL;
    smSCN             sSCN = SM_SCN_INIT;
    SLong             sRecordNumber = 0;
    UInt              i             = 0;
    UInt              sTableFlag    = 0;
    idBool            sIsLobExist   = ID_FALSE;

    *aFlag = SMI_LOCK_WRITE | SMI_TRAVERSE_FORWARD | SMI_PREVIOUS_DISABLE;

    sTableFlag = smiGetTableFlag( aTable );

    /*
     * BUG-34948
     * set APPEND flag on DISK table only
     */
    if ( ( ( sTableFlag & SMI_TABLE_TYPE_MASK ) == SMI_TABLE_DISK ) &&
         ( RPU_REPLICATION_SYNC_APPLY_METHOD == 1 ) )
    {
        sSCN = smiGetRowSCN( aTable );

        IDE_TEST_RAISE( smiValidateAndLockObjects(
                aTrans,
                aTable,
                sSCN,
                SMI_TBSLV_DDL_DML,
                SMI_TABLE_LOCK_SIX,
                (ULong)RPU_REPLICATION_SYNC_LOCK_TIMEOUT * 1000000,
                ID_FALSE ) /* Exp/Imp Lock���� */
            != IDE_SUCCESS, ERR_LOCK_TABLE );

        IDE_TEST( smiStatistics::getTableStatNumRow( (void *)aTable,
                                                     ID_TRUE,
                                                     aTrans,
                                                     &sRecordNumber )
                  != IDE_SUCCESS );

        for ( i = 0; i < smiGetTableColumnCount( aTable ); i++ )
        {
            sColumn = rpdCatalog::rpdGetTableColumns( aTable, i );

            if ( (sColumn->flag & SMI_COLUMN_TYPE_MASK) == SMI_COLUMN_TYPE_LOB )
            {
                sIsLobExist = ID_TRUE;
                break;
            }
            else
            {
                /* do nothing */
            }
        }

        if ( ( sRecordNumber == 0 ) && ( sIsLobExist == ID_FALSE ) )
        {
            /* direct path insert */
            *aFlag |= SMI_INSERT_METHOD_APPEND;
            mCursorOpenFlag = SMI_INSERT_METHOD_APPEND;

            // APPEND Flag�� ������, Cursor Open���� SIX Lock�� ��´�.
        }
        else
        {
            *aFlag |= SMI_INSERT_METHOD_NORMAL;
            mCursorOpenFlag = SMI_INSERT_METHOD_NORMAL;
        }
    }
    else
    {
        *aFlag |= SMI_INSERT_METHOD_NORMAL;
        mCursorOpenFlag = SMI_INSERT_METHOD_NORMAL;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_LOCK_TABLE );
    {
        IDE_SET( ideSetErrorCode( rpERR_ABORT_LOCK_TABLE ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
 *
 */
IDE_RC rpsSmExecutor::stmtBeginAndCursorOpen( smiTrans       * aTrans,
                                              smiStatement   * aSmiStmt,
                                              smiTableCursor * aCursor,
                                              rpdMetaItem    * aMetaItem,
                                              idBool         * aIsBegunSyncStmt,
                                              idBool         * aIsOpenedCursor )
{
    const void        * sTable;
    UInt                sFlag;
    smiCursorProperties sProperty;
    SChar               sErrorBuffer[256];

    SMI_CURSOR_PROP_INIT( &sProperty, mOpStatistics, NULL );

    sProperty.mLockWaitMicroSec   = ID_ULONG_MAX;
    sProperty.mFirstReadRecordPos = 0;
    sProperty.mReadRecordCount    = ID_ULONG_MAX;
    aTrans->setStatistics( mOpStatistics );
    sProperty.mStatistics         = aTrans->getStatistics();
    sProperty.mIsUndoLogging      = ID_TRUE;

    sTable = smiGetTable( aMetaItem->mItem.mTableOID );

    IDE_TEST_RAISE( setCursorOpenFlag( aTrans, sTable, &sFlag )
                    != IDE_SUCCESS, ERR_SET_CURSOR_OPEN );

    IDE_TEST_RAISE( aSmiStmt->begin( aTrans->getStatistics(),
                                     aTrans->getStatement(),
                                     SMI_STATEMENT_NORMAL | SMI_STATEMENT_ALL_CURSOR )
                    != IDE_SUCCESS, ERR_STMT_BEGIN );
    *aIsBegunSyncStmt = ID_TRUE;

    aCursor->initialize();

    IDE_TEST_RAISE( cursorOpen( aCursor,
                                aSmiStmt,
                                sTable,
                                NULL,                    // index
                                smiGetRowSCN(sTable),
                                NULL,                    // columns
                                smiGetDefaultKeyRange(), // range
                                smiGetDefaultKeyRange(), // callback
                                smiGetDefaultFilter(),
                                sFlag,
                                SMI_INSERT_CURSOR,
                                &sProperty )
                    != IDE_SUCCESS, ERR_CURSOR_OPEN );
    *aIsOpenedCursor = ID_TRUE;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_SET_CURSOR_OPEN );
    {
        IDE_ERRLOG(IDE_RP_0);

        idlOS::snprintf( sErrorBuffer, 256,
                         "[rpsSmExecutor::stmtBeginAndCursorOpen] failed set cursor open flag "
                         "[TABLEID: %"ID_vULONG_FMT"]", aMetaItem->mItem.mTableOID );
        IDE_SET( ideSetErrorCode( rpERR_ABORT_RP_INTERNAL_ARG, sErrorBuffer ) );
    }
    IDE_EXCEPTION( ERR_CURSOR_OPEN );
    {
        IDE_ERRLOG(IDE_RP_0);

        idlOS::snprintf( sErrorBuffer, 256,
                         "[rpsSmExecutor::stmtBeginAndCursorOpen] failed cursor open "
                         "[TABLEID: %"ID_vULONG_FMT"]", aMetaItem->mItem.mTableOID );
        IDE_SET( ideSetErrorCode( rpERR_ABORT_RP_INTERNAL_ARG, sErrorBuffer ) );
    }
    IDE_EXCEPTION( ERR_STMT_BEGIN );
    {
        IDE_ERRLOG(IDE_RP_0);

        idlOS::snprintf( sErrorBuffer, 256,
                         "[rpsSmExecutor::stmtBeginAndCursorOpen] failed statement begin "
                         "[TABLEID: %"ID_vULONG_FMT"]", aMetaItem->mItem.mTableOID );
        IDE_SET( ideSetErrorCode( rpERR_ABORT_RP_INTERNAL_ARG, sErrorBuffer ) );
    }
    IDE_EXCEPTION_END;

    IDE_PUSH();

    if ( *aIsBegunSyncStmt == ID_TRUE )
    {
        if ( aSmiStmt->end( SMI_STATEMENT_RESULT_FAILURE ) != IDE_SUCCESS )
        {
            IDE_ERRLOG( IDE_RP_0 );
        }
        else
        {
            /* nothing to do */
        }
            
        *aIsBegunSyncStmt = ID_FALSE;
    }

    IDE_POP();

    return IDE_FAILURE;
}

/*
 *
 */
IDE_RC rpsSmExecutor::stmtEndAndCursorClose( smiStatement   * aSmiStmt,
                                             smiTableCursor * aCursor,
                                             idBool         * aIsBegunSyncStmt,
                                             idBool         * aIsOpenedCursor,
                                             ULong          * aSyncTupleSuccessCount,
                                             SInt             aStmtEndFlag )
{
    if ( *aIsOpenedCursor == ID_TRUE )
    {
        *aIsOpenedCursor = ID_FALSE;
        IDE_TEST_RAISE( cursorClose( aCursor ) != IDE_SUCCESS, ERR_CURSOR_CLOSE );
    }
    else
    {
        /* nothing to do */
    }

    if ( *aIsBegunSyncStmt == ID_TRUE )
    {
        *aIsBegunSyncStmt = ID_FALSE;
        IDE_TEST_RAISE( aSmiStmt->end( aStmtEndFlag ) != IDE_SUCCESS, ERR_STMT_END );
    }
    else
    {
        /* nothing to do */
    }

    if ( aSyncTupleSuccessCount != NULL )
    {
        *aSyncTupleSuccessCount = 0;
    }
    else
    {
        /* nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_CURSOR_CLOSE );
    {
        IDE_ERRLOG(IDE_RP_0);
        IDE_SET(ideSetErrorCode( rpERR_ABORT_RP_INTERNAL_ARG,
                                 "[rpsSmExecutor::stmtEndAndCursorClose] failed cursor close" ) );
    }
    IDE_EXCEPTION( ERR_STMT_END );
    {
        IDE_ERRLOG(IDE_RP_0);
        IDE_SET(ideSetErrorCode( rpERR_ABORT_RP_INTERNAL_ARG,
                                 "[rpsSmExecutor::stmtEndAndCursorClose] failed statement end" ) );
    }
    IDE_EXCEPTION_END;

    IDE_PUSH();

    /* BUGBUG cursor close�� �����ߴµ� statement end�ϴ°� ������? */
    if ( *aIsBegunSyncStmt == ID_TRUE )
    {
        if ( aSmiStmt->end( SMI_STATEMENT_RESULT_FAILURE ) != IDE_SUCCESS )
        {
            IDE_ERRLOG( IDE_RP_0 );
        }
        else
        {
            /* nothing to do */
        }

        *aIsBegunSyncStmt = ID_FALSE;
    }

    IDE_POP();

    return IDE_FAILURE;
}
/*
 *
 */
IDE_RC rpsSmExecutor::executeSyncInsert( rpdXLog          * aXLog,
                                         smiTrans         * aSmiTrans,
                                         smiStatement     * aSmiStmt,
                                         smiTableCursor   * aCursor,
                                         rpdMetaItem      * aMetaItem,
                                         ULong              aSyncTupleSuccessCount,
                                         idBool           * aIsBegunSyncStmt,
                                         idBool           * aIsOpenedSyncCursor,
                                         rpApplyFailType  * aFailType )
{
    const void * sTable;
    void       * sDummyRow;
    scGRID       sRowGRID;
    smiValue     sConvertCols[QCI_MAX_COLUMN_COUNT]; // PROJ-1705
    smOID        sCompressColOIDs[QCI_MAX_COLUMN_COUNT]; // PROJ-2397
    smiValue     sFinalColsValues[QCI_MAX_COLUMN_COUNT];
    smiValue   * sACols = NULL;
    idBool       sInserted = ID_FALSE;

    RP_CREATE_FLAG_VARIABLE( IDV_OPTM_INDEX_RP_R_INSERT_ROW );

    RP_OPTIMIZE_TIME_BEGIN( mOpStatistics, IDV_OPTM_INDEX_RP_R_INSERT_ROW );

    SC_MAKE_NULL_GRID( sRowGRID );

    *aFailType = RP_APPLY_FAIL_NONE;

    IDE_CLEAR();
    
    if ( *aIsBegunSyncStmt == ID_FALSE )
    {
        IDE_TEST( stmtBeginAndCursorOpen( aSmiTrans,
                                          aSmiStmt,
                                          aCursor,
                                          aMetaItem,
                                          aIsBegunSyncStmt,
                                          aIsOpenedSyncCursor )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }
    
    /* smiTableCursor::insertRow()���� ������� ���� �κ��� �ʱ�ȭ���� �ʴ´�.
     *  idlOS::memset(sConvertCols, 0, ID_SIZEOF(smiValue) * QCI_MAX_COLUMN_COUNT);
     */

    sTable = smiGetTable( aMetaItem->mItem.mTableOID );

    /*
     * PROJ-1705
     *
     * Disk table�� insert value�� mtdValue�� �ƴ� value�� ���¸� ������.
     * sender���� ������ XLog�� after image���´� �ϰ������� mtdValue��
     * ���¸� �����Ƿ�, sm������ insert row �۾��ÿ� standby ������ ���̺���
     * Disk�� ���, value���� ��ȯ�۾��� �ʿ��ϴ�.
     * �� ��ġ�� ������, insert row���� conflict�߻���, �ļ� �۾�����
     * insert compare�ϸ� mtdValue�� ����ϰ� �ǹǷ�, XLog�� after image�� �����ϱ� ���ؼ��̴�.
     */
    if ( ( SMI_MISC_TABLE_HEADER( sTable )->mFlag & SMI_TABLE_TYPE_MASK ) == SMI_TABLE_DISK )
    {
        IDE_TEST( convertToNonMtdValue( aXLog, sConvertCols, sTable )
                  != IDE_SUCCESS );
    }
    else
    {
        /* BUG-30119 variable �÷��� fixed column�� ����ȭ�� ���, insert�� �����Ⱚ�� ���...*/
        IDE_TEST( convertXlogToSmiValue( aXLog, 
                                         sConvertCols, 
                                         sTable )
                  != IDE_SUCCESS );
    }

    // insert retry
  retryInsert:

    IDE_TEST( convertValueToOID( aXLog,
                                 sConvertCols,
                                 sFinalColsValues,
                                 sTable,
                                 aSmiStmt,
                                 sCompressColOIDs )
              != IDE_SUCCESS );

    sACols = sFinalColsValues;

    IDU_FIT_POINT_RAISE( "rpsSmExecutor::executeSyncInsert::insertRow::corrupted",
                          ERR_CONFLICT,
                          smERR_ABORT_PageCorrupted, 0, 0 );

    //BUG-22484 : sCursor.insertRow���� Retry������ �߻� �Ҽ� ����
    //���� insertRow : N, close Y --> Disk ����� Y , N  Memory N , Y
    if ( aCursor->insertRow( sACols,     // PROJ-1705
                             &sDummyRow,
                             &sRowGRID )
         != IDE_SUCCESS)
    {

        /* BUG-46910 insertRow ������ ��� stmtEnd stmtBegin�� ���� ���־�
         * ������ insert ����Ÿ���� rollback�ǹǷ� sync�� fail ó�� �ؾ��Ѵ�.
         */
        if ( aSyncTupleSuccessCount == 0 )
        {
            /* insertRow �����ϸ�, sm���� cursor�� init�ع��� �ٽ� ����� �� ���� �ȴ�.
             * sync�� ���, ��ó�� �ѹ� statement begin�� cursor open �� sync�� ���������� ����ϹǷ�,
             * init�� �ǰ��� statement begin�� cursor open�� �ٽ� �� �־���Ѵ�.
             */
            IDE_TEST( stmtEndAndCursorClose( aSmiStmt,
                                             aCursor,
                                             aIsBegunSyncStmt,
                                             aIsOpenedSyncCursor,
                                             NULL, // ���� �Լ����� SynTupleSuccessCount�� üũ�ؾ� �ϹǷ� �ʱ�ȭ ��Ű�� �ʴ´�
                                             SMI_STATEMENT_RESULT_FAILURE )
                      != IDE_SUCCESS );

            IDE_TEST( stmtBeginAndCursorOpen( aSmiTrans,
                                              aSmiStmt,
                                              aCursor,
                                              aMetaItem,
                                              aIsBegunSyncStmt,
                                              aIsOpenedSyncCursor )
                      != IDE_SUCCESS );

            IDE_TEST_RAISE( ideIsRetry() != IDE_SUCCESS, ERR_CONFLICT );

            IDE_CLEAR();

            // retry.
            RP_DBG_PRINTLINE();
            goto retryInsert;
        }
        else
        {
            IDE_TEST_RAISE( ideIsRetry() != IDE_SUCCESS, ERR_CONFLICT );
            IDE_TEST( ID_TRUE );
        }
    }
    else
    {
        sInserted = ID_TRUE;
    }

    // PROJ-1624 non-partitioned index
    // insertRow�� �����ߴٸ� index table���� �ݵ�� insert�Ǿ�� �Ѵ�.
    if ( ( aMetaItem->mIndexTableRef != NULL ) &&
         ( sInserted == ID_TRUE ) )
    {
        IDE_TEST_RAISE( qciMisc::insertIndexTable4OneRow(
                            aSmiStmt,
                            aMetaItem->mIndexTableRef,
                            sACols,
                            aMetaItem->mItem.mTableOID,
                            sRowGRID )
                        != IDE_SUCCESS, ERR_INSERT_GLOBAL_INDEX_TABLE );
    }
    else
    {
        // Nothing to do.
    }
    
    RP_OPTIMIZE_TIME_END( mOpStatistics, IDV_OPTM_INDEX_RP_R_INSERT_ROW );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INSERT_GLOBAL_INDEX_TABLE );
    {
        if ( RP_IS_CORRUPTED_PAGE() != ID_TRUE )
        {
            IDE_ERRLOG( IDE_RP_0 );
            IDE_SET( ideSetErrorCode( rpERR_ABORT_INSERT_GLOBAL_INDEX_TABLE ) );

            IDE_ERRLOG( IDE_RP_0 );

            *aFailType = RP_APPLY_FAIL_BY_CONFLICT; 
        }
        else
        {
            *aFailType = RP_APPLY_FAIL_BY_CORRUPTED_PAGE; 
        }
    }
    IDE_EXCEPTION( ERR_CONFLICT );
    {
        /* BUG-27573 If the receiver get not enough free space,
         *           then the receiver stop.
         * BUG-31851 The transaction needs rollback in eager mode.
         */
        if ( ( ideGetErrorCode() == smERR_ABORT_NOT_ENOUGH_SPACE ) ||
             ( ideGetErrorCode() == smERR_ABORT_INCONSISTENT_INDEX ) )
        {
            /* Nothing to do */
        }
        else
        {
            /* ���� Primary Key�� ���� Row�� �̹� ����
             * Lock Timeout
             * Unique Violation
             * Eager Mode�� ��, Not Enough Space
             */
            if ( ( ideGetErrorCode() == smERR_ABORT_smnUniqueViolationInReplTrans ) ||
                 ( ideGetErrorCode() == smERR_ABORT_smcExceedLockTimeWait ) )
            {
                /* conflict resolution tx�� ���� lock�� ��� �����ߴٸ�, ������ ������ �۾��� �Ѵ�.
                   1. conflict resolution tx�� ���� ���, �� �۾��� conflict resolution tx�� �ְ� ��õ�
                   2. conflict resolution tx�� ������, �ٸ� tx�� conflict resolutionó������ ���̹Ƿ� ����ó�� */
                *aFailType = RP_APPLY_FAIL_BY_CONFLICT_RESOLUTION_TX;
            }
            else
            {
                *aFailType = RP_APPLY_FAIL_BY_CONFLICT;
            }

        }
    }
    IDE_EXCEPTION_END;
    RP_OPTIMIZE_TIME_END( mOpStatistics, IDV_OPTM_INDEX_RP_R_INSERT_ROW );
    
    IDE_PUSH();
    if ( *aIsBegunSyncStmt == ID_TRUE )
    {
        (void)stmtEndAndCursorClose( aSmiStmt,
                                     aCursor,
                                     aIsBegunSyncStmt,
                                     aIsOpenedSyncCursor,
                                     NULL, // ���� �Լ����� SynTupleSuccessCount�� üũ�ؾ� �ϹǷ� �ʱ�ȭ ��Ű�� �ʴ´�
                                     SMI_STATEMENT_RESULT_FAILURE );
    }
    else
    {
        /* Nothing to do */
    }
    IDE_POP();
    
    if ( *aFailType == RP_APPLY_FAIL_NONE )
    {
        *aFailType = RP_APPLY_FAIL_INTERNAL;
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_FAILURE;
}

/* [ INPUT ]
 * ( primary key columns )
 * aXLog->mPKColCnt
 * aXLog->mPKCIDs
 * aXLog->mPKCols
 * ( update columns )
 * aXLog->mColCnt
 * aXLog->mCIDs
 * aXLog->mBCols   -> for compare image
 * aXLog->mACols
 */
IDE_RC rpsSmExecutor::executeUpdate(smiTrans         * aTrans,
                                    rpdXLog          * aXLog,
                                    rpdMetaItem      * aMetaItem,
                                    qciIndexTableRef * aIndexTableRef,
                                    smiRange         * aKeyRange,
                                    rpApplyFailType  * aFailType,
                                    idBool             aCompareBeforeImage,
                                    mtcColumn        * aTsFlag)
{
    const void          *sRow;
    scGRID               sRid;
    scGRID               sRowGRID;
    smiTableCursor       sCursor;
    const void          *sTable;
    const void          *sIndex;
    UInt                 sCount;
    UInt                 sStep = 0;
    smiStatement         sSmiStmt;
    smiCursorProperties  sProperty;
    smiValue             sConvertCols[QCI_MAX_COLUMN_COUNT];
    smOID                sCompressColOIDs[QCI_MAX_COLUMN_COUNT]; // PROJ-2397
    smiValue             sFinalColsValues[QCI_MAX_COLUMN_COUNT];
    smiValue            *sACols = NULL;
    idBool               sUpdated = ID_FALSE;
    idBool               sIsConflict = ID_FALSE;

    RP_CREATE_FLAG_VARIABLE(IDV_OPTM_INDEX_RP_R_UPDATE_ROW);

    RP_OPTIMIZE_TIME_BEGIN(mOpStatistics, IDV_OPTM_INDEX_RP_R_UPDATE_ROW);

    SC_MAKE_NULL_GRID( sRowGRID );

    IDE_CLEAR();

    *aFailType = RP_APPLY_FAIL_NONE;

    /* smiTableCursor::updateRow()���� ������� ���� �κ��� �ʱ�ȭ���� �ʴ´�.
     *  idlOS::memset(sConvertCols, 0, ID_SIZEOF(smiValue) * QCI_MAX_COLUMN_COUNT);
     */

    sTable = smiGetTable( aMetaItem->mItem.mTableOID );

    // get primary key index
    sIndex = smiGetTablePrimaryKeyIndex( sTable );

    SMI_CURSOR_PROP_INIT(&sProperty, NULL, sIndex); //PROJ-1705

    IDE_TEST( makeUpdateColumnList( aXLog, aMetaItem->mItem.mTableOID ) != IDE_SUCCESS );

    /* PROJ-1705 Fetch Column List ���� */
    if((SMI_MISC_TABLE_HEADER(sTable)->mFlag & SMI_TABLE_TYPE_MASK) == SMI_TABLE_DISK)
    {
        // PROJ-1705 Fetch Column List ����
        IDE_TEST( makeFetchColumnList( aXLog, aMetaItem->mItem.mTableOID, sIndex )
                  != IDE_SUCCESS );

        // PROJ-1705 smiCursorProperties�� Fetch Column List ���� ����
        sProperty.mFetchColumnList = mFetchColumnList;
    }
    else
    {
        // memory table�� fetch column list�� �ʿ����� �ʴ�.
        sProperty.mFetchColumnList = NULL;
    }

    /*
     * PROJ-1705
     *
     * Disk table�� insert value�� mtdValue�� �ƴ� value�� ���¸� ������.
     * sender���� ������ XLog�� after image���´� �ϰ������� mtdValue��
     * ���¸� �����Ƿ�, sm������ update row �۾��ÿ� standby ������ ���̺���
     * Disk�� ���, value���� ��ȯ�۾��� �ʿ��ϴ�.
     */
    if((SMI_MISC_TABLE_HEADER(sTable)->mFlag & SMI_TABLE_TYPE_MASK) == SMI_TABLE_DISK)
    {
        IDE_TEST(convertToNonMtdValue(aXLog, sConvertCols, sTable)
                 != IDE_SUCCESS);
    }
    else
    {
        /* BUG-30119 variable �÷��� fixed column�� ����ȭ�� ���, insert�� �����Ⱚ�� ���...*/
        IDE_TEST( convertXlogToSmiValue( aXLog, 
                                         sConvertCols, 
                                         sTable )
                  != IDE_SUCCESS );
    }

    // update retry
retryUpdate:

    IDE_TEST(sSmiStmt.begin( aTrans->getStatistics(),
                             aTrans->getStatement(),
                             SMI_STATEMENT_NORMAL | SMI_STATEMENT_ALL_CURSOR )
             != IDE_SUCCESS);

    IDE_TEST( convertValueToOID( aXLog,
                                 sConvertCols,
                                 sFinalColsValues,
                                 sTable,
                                 &sSmiStmt,
                                 sCompressColOIDs )
              != IDE_SUCCESS );   

    sACols = sFinalColsValues;

    sStep = 1;

    sCursor.initialize();

    if ( cursorOpen( &sCursor,
                     &sSmiStmt,
                     sTable,
                     sIndex,
                     smiGetRowSCN(sTable),
                     mUpdateColList,
                     aKeyRange,
                     smiGetDefaultKeyRange(),
                     smiGetDefaultFilter(),
                     SMI_LOCK_WRITE | SMI_TRAVERSE_FORWARD | SMI_PREVIOUS_DISABLE,
                     SMI_UPDATE_CURSOR,
                     &sProperty )
         == IDE_SUCCESS )
    {
        /* do nothing */
    }
    else
    {
        IDE_TEST_RAISE( ideIsRebuild() != IDE_SUCCESS, ERR_CONFLICT );

        IDE_ERRLOG( IDE_RP_0 );

        IDE_CLEAR();

        sStep=0;
        IDE_TEST( sSmiStmt.end(SMI_STATEMENT_RESULT_FAILURE) != IDE_SUCCESS );

        goto retryUpdate;
    }
    sStep = 2;

    IDE_TEST(sCursor.beforeFirst() != IDE_SUCCESS);

    sCount = 0;

    if((SMI_MISC_TABLE_HEADER(sTable)->mFlag & SMI_TABLE_TYPE_MASK) == SMI_TABLE_DISK)
    {
        sRow = (const void*)mRealRow;
    }

    IDE_TEST_RAISE( sCursor.readRow(&sRow, &sRowGRID, SMI_FIND_NEXT)
                    != IDE_SUCCESS, ERR_READROW ) ;

    IDE_TEST_RAISE(sRow == NULL, ERR_NO_ROW_IS_FOUND);

    while(sRow != NULL)
    {
        sCount++;
        IDE_TEST_RAISE(sCount > 1, ERR_PK_MODIFIED_MORE_THAN_ONE_ROW);

        if(aTsFlag != NULL)
        {
            /* Timestamp �÷����� ������ �ͺ��� ������, Conflict�̴�. */
            IDE_TEST( compareImageTS( aXLog,
                                      aMetaItem->mItem.mTableOID,
                                      sRow,
                                      &sIsConflict,
                                      aTsFlag )
                      != IDE_SUCCESS );

            /* BUG-31770 Timestamp Conflict ���� ���� */
            IDE_TEST_RAISE( sIsConflict == ID_TRUE, ERR_TIMESTAMP_CONFLICT );
        }
        else
        {
            if ( aCompareBeforeImage == ID_TRUE )
            {
                /* Before Image�� ������ �÷����� �ٸ���, Conflict�̴�. */
                IDE_TEST( compareUpdateImage( aXLog,
                                              aMetaItem->mItem.mTableOID,
                                              sRow,
                                              &sIsConflict )
                          != IDE_SUCCESS );

                IDE_TEST_RAISE( sIsConflict == ID_TRUE, ERR_IMAGE_MISMATCH );
            }
        }

        IDU_FIT_POINT( "1.TASK-2004@rpsSmExecutor::executeUpdate" );

        if(sCursor.updateRow(sACols) != IDE_SUCCESS)
        {
            IDE_TEST_RAISE(ideIsRetry() != IDE_SUCCESS, ERR_CONFLICT);

            IDE_CLEAR();

            sStep = 1;
            IDE_TEST(cursorClose(&sCursor) != IDE_SUCCESS);

            sStep = 0;
            IDE_TEST(sSmiStmt.end(SMI_STATEMENT_RESULT_FAILURE) != IDE_SUCCESS);

            // retry.
            RP_DBG_PRINTLINE();
            goto retryUpdate;
        }
        else
        {
            sUpdated = ID_TRUE;
        }
        
        IDE_TEST_RAISE( sCursor.readRow(&sRow, &sRid, SMI_FIND_NEXT)
                        != IDE_SUCCESS, ERR_READROW ) ;
    }

    sStep = 1;
    if(cursorClose(&sCursor) != IDE_SUCCESS)
    {
        IDE_TEST_RAISE(ideIsRetry() != IDE_SUCCESS, ERR_CONFLICT);

        IDE_CLEAR();

        sStep = 0;
        IDE_TEST(sSmiStmt.end(SMI_STATEMENT_RESULT_FAILURE) != IDE_SUCCESS);

        // retry.
        RP_DBG_PRINTLINE();
        goto retryUpdate;
    }

    // PROJ-1624 non-partitioned index
    // updateRow�� �����ߴٸ� index table���� �ݵ�� update�Ǿ�� �Ѵ�.
    if ( ( aIndexTableRef != NULL ) &&
         ( sUpdated == ID_TRUE ) )
    {
        IDE_TEST_RAISE( qciMisc::updateIndexTable4OneRow(
                            & sSmiStmt,
                            aIndexTableRef,
                            aXLog->mColCnt,
                            mUpdateColID,
                            sACols,
                            (void*) mRealRow,
                            sRowGRID )
                        != IDE_SUCCESS, ERR_UPDATE_GLOBAL_INDEX_TABLE );
    }
    else
    {
        // Nothing to do.
    }
    
    IDE_TEST(sSmiStmt.end( SMI_STATEMENT_RESULT_SUCCESS) != IDE_SUCCESS);
    sStep = 0;

    RP_OPTIMIZE_TIME_END(mOpStatistics, IDV_OPTM_INDEX_RP_R_UPDATE_ROW);

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_UPDATE_GLOBAL_INDEX_TABLE );
    {
        if ( RP_IS_CORRUPTED_PAGE() != ID_TRUE )
        {
            IDE_ERRLOG( IDE_RP_0 );
            IDE_SET( ideSetErrorCode( rpERR_ABORT_UPDATE_GLOBAL_INDEX_TABLE ) );

            IDE_ERRLOG( IDE_RP_0 );

            *aFailType = RP_APPLY_FAIL_BY_CONFLICT;
        }
        else
        {
            *aFailType = RP_APPLY_FAIL_BY_CORRUPTED_PAGE;
        }
    }
    IDE_EXCEPTION(ERR_TIMESTAMP_CONFLICT)
    {
        IDE_SET( ideSetErrorCode( rpERR_IGNORE_TIMESTAMP_UPDATE_CONFLICT ) );
        *aFailType = RP_APPLY_FAIL_BY_CONFLICT;
    }
    IDE_EXCEPTION(ERR_IMAGE_MISMATCH)
    {
        IDE_SET(ideSetErrorCode(rpERR_ABORT_UPDATE_CONFLICT));
        *aFailType = RP_APPLY_FAIL_BY_CONFLICT;
    }
    IDE_EXCEPTION(ERR_NO_ROW_IS_FOUND);
    {
        IDE_SET(ideSetErrorCode(rpERR_ABORT_NOT_FOUND_RECORD, "executeUpdate()"));
        *aFailType = RP_APPLY_FAIL_BY_CONFLICT;
    }
    IDE_EXCEPTION(ERR_PK_MODIFIED_MORE_THAN_ONE_ROW);
    {
        IDE_SET(ideSetErrorCode( rpERR_ABORT_RP_INTERNAL_ARG,
                                 "[rpsSmExecutor::executeUpdate] ERR_PK_MODIFIED_MORE_THAN_ONE_ROW"));
    }
    IDE_EXCEPTION(ERR_CONFLICT);
    {
        /* BUG-27573 If the receiver get not enough free space,
         *           then the receiver stop.
         * BUG-31851 The transaction needs rollback in eager mode.
         */
        if ( ( ideGetErrorCode() == smERR_ABORT_NOT_ENOUGH_SPACE ) &&
             ( mIsConflictWhenNotEnoughSpace == ID_FALSE ) )
        {
            /* Nothing to do */
        }
        else
        {
            /* Lock Timeout
             * Unique Violation
             * Eager Mode�� ��, Not Enough Space
             */
            if ( ( ideGetErrorCode() == smERR_ABORT_smnUniqueViolationInReplTrans ) ||
                 ( ideGetErrorCode() == smERR_ABORT_smcExceedLockTimeWait ) )
            {
                /* conflict resolution tx�� ���� lock�� ��� �����ߴٸ�, ������ ������ �۾��� �Ѵ�.
                   1. conflict resolution tx�� ���� ���, �� �۾��� conflict resolution tx�� �ְ� ��õ�
                   2. conflict resolution tx�� ������, �ٸ� tx�� conflict resolutionó������ ���̹Ƿ� ����ó�� */
                *aFailType = RP_APPLY_FAIL_BY_CONFLICT_RESOLUTION_TX;
            }
            else
            {
                if ( RP_IS_CORRUPTED_PAGE() != ID_TRUE )
                {
                    *aFailType = RP_APPLY_FAIL_BY_CONFLICT;
                }
                else
                {
                    *aFailType = RP_APPLY_FAIL_BY_CORRUPTED_PAGE;
                }

            }
        }
    }
    IDE_EXCEPTION( ERR_READROW );
    {
        if ( RP_IS_CORRUPTED_PAGE() == ID_TRUE )
        {
            *aFailType = RP_APPLY_FAIL_BY_CORRUPTED_PAGE;
        }
    }
    IDE_EXCEPTION_END;

    RP_OPTIMIZE_TIME_END(mOpStatistics, IDV_OPTM_INDEX_RP_R_UPDATE_ROW);

    if ( *aFailType == RP_APPLY_FAIL_NONE )
    {
        *aFailType = RP_APPLY_FAIL_INTERNAL;
    }
    else
    {
        /* Nothing to do */
    }

    IDE_PUSH();

    switch (sStep)
    {
        case 2 :
            if(cursorClose(&sCursor) != IDE_SUCCESS)
            {
                IDE_ERRLOG(IDE_RP_0);
            }
        case 1 :
            if(sSmiStmt.end(SMI_STATEMENT_RESULT_FAILURE) != IDE_SUCCESS)
            {
                IDE_ERRLOG(IDE_RP_0);
            }
        default :
            break;
    }

    IDE_POP();
    return IDE_FAILURE;
}

/* [ INPUT ]
 * ( primary key columns )
 * aXLog->mPKColCnt
 * aXLog->mPKCIDs
 * aXLog->mPKCols
 */
IDE_RC rpsSmExecutor::executeDelete(smiTrans         * aTrans,
                                    rpdXLog          * aXLog,
                                    rpdMetaItem      * aMetaItem,
                                    qciIndexTableRef * aIndexTableRef,
                                    smiRange         * aKeyRange,
                                    rpApplyFailType  * aFailType,
                                    idBool             aCheckRowExistence)
{
    const void*         sRow;
    scGRID              sRid;
    scGRID              sRowGRID;
    smiTableCursor      sCursor;
    const void*         sTable;
    const void*         sIndex;
    UInt                sCount;
    UInt                sStep = 0;
    smiStatement        sSmiStmt;
    smiCursorProperties sProperty;
    idBool              sDeleted = ID_FALSE;

    RP_CREATE_FLAG_VARIABLE(IDV_OPTM_INDEX_RP_R_DELETE_ROW);

    RP_OPTIMIZE_TIME_BEGIN(mOpStatistics, IDV_OPTM_INDEX_RP_R_DELETE_ROW);

    SC_MAKE_NULL_GRID( sRowGRID );

    IDE_CLEAR();

    *aFailType = RP_APPLY_FAIL_NONE;
    mDeleteRowCount = 0;

    sTable = smiGetTable( aMetaItem->mItem.mTableOID );

    // get primary key index
    sIndex = smiGetTablePrimaryKeyIndex( sTable );

    SMI_CURSOR_PROP_INIT(&sProperty, NULL, sIndex);

    /* PROJ-1705 Fetch Column List ���� */
    if((SMI_MISC_TABLE_HEADER(sTable)->mFlag & SMI_TABLE_TYPE_MASK) == SMI_TABLE_DISK)
    {
        // PROJ-1705 Fetch Column List ����
        IDE_TEST( makeFetchColumnList( aXLog, aMetaItem->mItem.mTableOID, sIndex )
                  != IDE_SUCCESS );

        // PROJ-1705 smiCursorProperties�� Fetch Column List ���� ����
        sProperty.mFetchColumnList = mFetchColumnList;
    }
    else
    {
        // memory table�� fetch column list�� �ʿ����� �ʴ�.
        sProperty.mFetchColumnList = NULL;
    }

    // delete retry
retryDelete:

    IDE_TEST( sSmiStmt.begin( aTrans->getStatistics(),
                              aTrans->getStatement(),
                              SMI_STATEMENT_NORMAL | SMI_STATEMENT_ALL_CURSOR )
              != IDE_SUCCESS );
    sStep = 1;

    sCursor.initialize();

    if ( cursorOpen( &sCursor,
                     &sSmiStmt,
                     sTable,
                     sIndex,
                     smiGetRowSCN(sTable),
                     NULL,  // smiColumnList
                     aKeyRange,
                     smiGetDefaultKeyRange(),
                     smiGetDefaultFilter(),
                     SMI_LOCK_WRITE | SMI_TRAVERSE_FORWARD | SMI_PREVIOUS_DISABLE,
                     SMI_DELETE_CURSOR,
                     &sProperty )
         == IDE_SUCCESS )
    {
        /* do nothing */
    }
    else
    {
        IDE_TEST_RAISE( ideIsRebuild() != IDE_SUCCESS, ERR_CONFLICT );
        
        IDE_ERRLOG( IDE_RP_0 );

        IDE_CLEAR();
        
        sStep = 0;
        IDE_TEST( sSmiStmt.end(SMI_STATEMENT_RESULT_FAILURE) != IDE_SUCCESS );

        goto retryDelete;

    }
    sStep = 2;

    IDE_TEST(sCursor.beforeFirst() != IDE_SUCCESS);

    sCount = 0;

    if((SMI_MISC_TABLE_HEADER(sTable)->mFlag & SMI_TABLE_TYPE_MASK) == SMI_TABLE_DISK)
    {
        sRow = (const void*)mRealRow;
    }

    IDU_FIT_POINT_RAISE( "rpsSmExecutor::executeDelete::readRow::corrupted",
                         ERR_READROW,
                         smERR_ABORT_PageCorrupted, 0, 0 );

    IDE_TEST_RAISE( sCursor.readRow(&sRow, &sRowGRID, SMI_FIND_NEXT) 
                    != IDE_SUCCESS, ERR_READROW ) ;

    if(sRow == NULL)
    {
        IDE_TEST_RAISE( aCheckRowExistence == ID_TRUE, ERR_NO_ROW_IS_FOUND);
    }
    else
    {
        IDU_FIT_POINT( "1.TASK-2004@rpsSmExecutor::executeDelete" );

        while(sRow != NULL)
        {
            sCount++;

            if(sCursor.deleteRow() != IDE_SUCCESS)
            {
                IDE_TEST_RAISE(ideIsRetry() != IDE_SUCCESS, ERR_CONFLICT)

                IDE_CLEAR();

                sStep = 1;
                IDE_TEST(cursorClose(&sCursor) != IDE_SUCCESS);

                sStep = 0;
                IDE_TEST(sSmiStmt.end(SMI_STATEMENT_RESULT_FAILURE)
                         != IDE_SUCCESS);

                // retry.
                RP_DBG_PRINTLINE();
                goto retryDelete;
            }
            else
            {
                sDeleted = ID_TRUE;
            }
            
            IDE_TEST_RAISE( sCursor.readRow(&sRow, &sRid, SMI_FIND_NEXT)
                            != IDE_SUCCESS, ERR_READROW ) ;
        }
        
        mDeleteRowCount = sCount;
        
        IDE_TEST_RAISE( sCount > 1, ERR_PK_MODIFIED_MORE_THAN_ONE_ROW );
    }

    sStep = 1;
    IDE_TEST(cursorClose(&sCursor) != IDE_SUCCESS);

    // PROJ-1624 non-partitioned index
    // deleteRow�� �����ߴٸ� index table���� �ݵ�� delete�Ǿ�� �Ѵ�.
    if ( ( aIndexTableRef != NULL ) &&
         ( sDeleted == ID_TRUE ) )
    {
        IDE_TEST_RAISE( qciMisc::deleteIndexTable4OneRow(
                      & sSmiStmt,
                      aIndexTableRef,
                      (void*) mRealRow,
                      sRowGRID )
                  != IDE_SUCCESS, ERR_DELETE_GLOBAL_INDEX_TABLE );
    }
    else
    {
        // Nothing to do.
    }
    
    IDE_TEST(sSmiStmt.end(SMI_STATEMENT_RESULT_SUCCESS) != IDE_SUCCESS);
    sStep = 0;

    RP_OPTIMIZE_TIME_END(mOpStatistics, IDV_OPTM_INDEX_RP_R_DELETE_ROW);

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_DELETE_GLOBAL_INDEX_TABLE );
    {
        if ( RP_IS_CORRUPTED_PAGE() != ID_TRUE )
        {
            IDE_ERRLOG( IDE_RP_0 );
            IDE_SET( ideSetErrorCode( rpERR_ABORT_DELETE_GLOBAL_INDEX_TABLE ) );

            IDE_ERRLOG( IDE_RP_0 );

            *aFailType = RP_APPLY_FAIL_BY_CONFLICT;
        }
        else
        {
            *aFailType = RP_APPLY_FAIL_BY_CORRUPTED_PAGE;
        }
    }
    IDE_EXCEPTION(ERR_NO_ROW_IS_FOUND);
    {
        IDE_SET(ideSetErrorCode(rpERR_ABORT_NOT_FOUND_RECORD, "executeDelete()"));

        *aFailType = RP_APPLY_FAIL_BY_CONFLICT;
    }
    IDE_EXCEPTION(ERR_PK_MODIFIED_MORE_THAN_ONE_ROW);
    {
        IDE_SET(ideSetErrorCode( rpERR_ABORT_RP_INTERNAL_ARG,
                                 "[rpsSmExecutor::executeDelete] ERR_PK_MODIFIED_MORE_THAN_ONE_ROW"));

        *aFailType = RP_APPLY_FAIL_BY_CONFLICT;
    }
    IDE_EXCEPTION(ERR_CONFLICT);
    {
        /* BUG-27573 If the receiver get not enough free space,
         *           then the receiver stop.
         * BUG-31851 The transaction needs rollback in eager mode.
         */
        if ( ( ideGetErrorCode() == smERR_ABORT_NOT_ENOUGH_SPACE ) &&
             ( mIsConflictWhenNotEnoughSpace == ID_FALSE ) )
        {
            /* Nothing to do */
        }
        else
        {
            /* Lock Timeout
             * Eager Mode�� ��, Not Enough Space
             */
            if ( ( ideGetErrorCode() == smERR_ABORT_smnUniqueViolationInReplTrans ) ||
                 ( ideGetErrorCode() == smERR_ABORT_smcExceedLockTimeWait ) )
            {
                /* conflict resolution tx�� ���� lock�� ��� �����ߴٸ�, ������ ������ �۾��� �Ѵ�.
                   1. conflict resolution tx�� ���� ���, �� �۾��� conflict resolution tx�� �ְ� ��õ�
                   2. conflict resolution tx�� ������, �ٸ� tx�� conflict resolutionó������ ���̹Ƿ� ����ó�� */
                *aFailType = RP_APPLY_FAIL_BY_CONFLICT_RESOLUTION_TX;
            }
            else
            {
                if ( RP_IS_CORRUPTED_PAGE() != ID_TRUE )
                {
                    *aFailType = RP_APPLY_FAIL_BY_CONFLICT;
                }
                else
                {
                    *aFailType = RP_APPLY_FAIL_BY_CORRUPTED_PAGE;
                }
            }
        }
    }
    IDE_EXCEPTION( ERR_READROW );
    {
        if ( RP_IS_CORRUPTED_PAGE() == ID_TRUE )
        {
            *aFailType = RP_APPLY_FAIL_BY_CORRUPTED_PAGE;
        }
    }

    IDE_EXCEPTION_END;

    RP_OPTIMIZE_TIME_END(mOpStatistics, IDV_OPTM_INDEX_RP_R_DELETE_ROW);

    if ( *aFailType == RP_APPLY_FAIL_NONE )
    {
        *aFailType = RP_APPLY_FAIL_INTERNAL;
    }
    else
    {
        /* Nothing to do */
    }

    IDE_PUSH();

    switch (sStep)
    {
        case 2 :
            if(cursorClose(&sCursor) != IDE_SUCCESS)
            {
                IDE_ERRLOG(IDE_RP_0);
            }
        case 1 :
            if(sSmiStmt.end(SMI_STATEMENT_RESULT_FAILURE) != IDE_SUCCESS)
            {
                IDE_ERRLOG(IDE_RP_0);
            }
        default :
            break;
    }

    IDE_POP();
    return IDE_FAILURE;
}

IDE_RC rpsSmExecutor::makeFetchColumnList(rpdXLog            * aXLog,
                                          const smOID          aTableOID,
                                          const void         * aIndexHandle)
{
/*******************************************************************************
 *
 * Description : sm���κ��� ���ڵ���ġ�� ���簡 �ʿ��� �÷���������
 *
 * Implementation :  PROJ-1705
 *
 *   PROJ-1705 �������� sm���� ���ڵ� ��ġ�����
 *   ���� ���ڵ������ ��ġ���� �÷������� ��ġ�� ��ġ����� �����.
 *   sm���� �÷������� ��ġ�� �̷���� �� �ֵ���
 *   Ŀ�� ���½�,
 *   ��ġ�� �÷������� �����ؼ� �� ������ smiCursorProperties�� �Ѱ��ش�.
 *   
 *   INSERT(Timestamp Conflict Resolution), LOB Cursor Open / DELETE / UPDATE ��
 *   PK�ε��� �÷��� ���� ��ġ�÷�����Ʈ ����
 *
 ******************************************************************************/

    UInt                    i;
    UInt                    sPKColIdx;
    UInt                    sUpdateColIdx;
    UInt                  * sKeyCols;
    UInt                    sCID;
    idBool                  sDupColumn;
    const void            * sTable;
    const smiColumn       * sSmiColumn;
    smiFetchColumnList    * sFetchColumnList;
    smiFetchColumnList    * sFetchColumn;
    UInt                    sFetchColumnCnt;
    SChar                   sErrorBuffer[256];
    UInt                    sFunctionIdx;

    sTable           = smiGetTable( aTableOID );
    sFetchColumnList = mFetchColumnList;

    // LOB Cursor Open / DELETE / UPDATE �ÿ� ���´�.
    if(aXLog->mPKColCnt > 0)
    {
        IDE_TEST_RAISE(aIndexHandle == NULL, ERR_NULL_KEY_INDEX);
        sKeyCols = (UInt*)smiGetIndexColumns(aIndexHandle);
        IDE_TEST_RAISE(sKeyCols == NULL, ERR_NOT_FOUND_KEY_COLUMNS);
        for(i = 0; i < aXLog->mPKColCnt; i++)
        {
            sCID = sKeyCols[i] % SMI_COLUMN_ID_MAXIMUM;
            sSmiColumn = rpdCatalog::rpdGetTableColumns(sTable, sCID);

            IDU_FIT_POINT_RAISE( "rpsSmExecutor::makeFetchColumnList::Erratic::rpERR_NOT_FOUND_COLUMN",
                                 ERR_NOT_FOUND_COLUMN );

            IDE_TEST_RAISE(sSmiColumn == NULL, ERR_NOT_FOUND_COLUMN);

            sFetchColumn = sFetchColumnList + i;
            sFetchColumn->column    = sSmiColumn;
            sFetchColumn->columnSeq = SMI_GET_COLUMN_SEQ( sSmiColumn );

            /* PROJ-2429 Dictionary based data compress for on-disk DB */
            if ( (sFetchColumn->column->flag & SMI_COLUMN_COMPRESSION_MASK) 
                 != SMI_COLUMN_COMPRESSION_TRUE )
            {
                sFunctionIdx = MTD_COLUMN_COPY_FUNC_NORMAL;
            }
            else
            {
                sFunctionIdx = MTD_COLUMN_COPY_FUNC_COMPRESSION;
            }

            sFetchColumn->copyDiskColumn
                = (void*)(((mtcColumn*)sSmiColumn)->module->storedValue2MtdValue[sFunctionIdx]);
        }
        // update�� update column�� ���� fetch column list ����
        if ( aXLog->mType == RP_X_UPDATE )
        {
            IDE_DASSERT( aXLog->mColCnt > 0 );

            for(sUpdateColIdx = 0; sUpdateColIdx < aXLog->mColCnt; sUpdateColIdx++)
            {
                sDupColumn = ID_FALSE;

                for(sPKColIdx = 0; sPKColIdx < aXLog->mPKColCnt; sPKColIdx++)
                {
                    sCID = sKeyCols[sPKColIdx] % SMI_COLUMN_ID_MAXIMUM;
                    sSmiColumn = rpdCatalog::rpdGetTableColumns(sTable, sCID);

                    IDE_TEST_RAISE(sSmiColumn == NULL, ERR_NOT_FOUND_COLUMN);

                    if(mCol[sUpdateColIdx].column.id == sSmiColumn->id)
                    {
                        sDupColumn = ID_TRUE;
                        break;
                    }
                }

                // BUG-29020
                // update column�� pk column�� �ߺ��Ǵ� ��� fetch column list���� �����Ѵ�.
                if(sDupColumn == ID_FALSE)
                {
                    sFetchColumn = sFetchColumnList + i;
                    sFetchColumn->column    = (smiColumn*)(&mCol[sUpdateColIdx].column);
                    sFetchColumn->columnSeq = SMI_GET_COLUMN_SEQ( sFetchColumn->column );

                    /* PROJ-2429 Dictionary based data compress for on-disk DB */
                    if ( (sFetchColumn->column->flag & SMI_COLUMN_COMPRESSION_MASK) 
                         != SMI_COLUMN_COMPRESSION_TRUE )
                    {
                        sFunctionIdx = MTD_COLUMN_COPY_FUNC_NORMAL;
                    }
                    else
                    {
                        sFunctionIdx = MTD_COLUMN_COPY_FUNC_COMPRESSION;
                    }

                    sFetchColumn->copyDiskColumn
                        = (void*)(mCol[sUpdateColIdx].module->storedValue2MtdValue[sFunctionIdx]);

                    i++;
                }
                else
                {
                    // Nothing to do.
                }
            }
        }
        else
        {
            /* Nothing to do */
        }

        // i�� fetch column count�̴�.
        sFetchColumnCnt = i;
        
        // �ε��� �÷������� fetch column ����
        if(sFetchColumnCnt > 1)
        {
            idlOS::qsort(sFetchColumnList,
                         sFetchColumnCnt,
                         ID_SIZEOF(smiFetchColumnList),
                         compareColumnID);
        }
        else
        {
            // Nothing To Do
        }
        
        // ���̺�����÷�������� fetch column list �籸��
        for(i = 0; i < sFetchColumnCnt - 1; i++)
        {
            sFetchColumnList[i].next = &sFetchColumnList[i+1];
        }
        sFetchColumnList[i].next = NULL;
    }
    // INSERT �ÿ� ���´�.
    // compare Insert image�ÿ� readRow�� ���� fetch column list�� ����� ���Ͽ�.
    else
    {
        for ( i = 0; i < aXLog->mColCnt; i++ )
        {
            sCID = aXLog->mCIDs[i];
            sSmiColumn = rpdCatalog::rpdGetTableColumns(sTable, sCID);

            IDE_TEST_RAISE(sSmiColumn == NULL, ERR_NOT_FOUND_COLUMN);

            sFetchColumn = sFetchColumnList + i;
            sFetchColumn->column    = sSmiColumn;
            sFetchColumn->columnSeq = SMI_GET_COLUMN_SEQ( sSmiColumn );

            /* PROJ-2429 Dictionary based data compress for on-disk DB */
            if ( (sFetchColumn->column->flag & SMI_COLUMN_COMPRESSION_MASK) 
                 != SMI_COLUMN_COMPRESSION_TRUE )
            {
                sFunctionIdx = MTD_COLUMN_COPY_FUNC_NORMAL;
            }
            else
            {
                sFunctionIdx = MTD_COLUMN_COPY_FUNC_COMPRESSION;
            }

            sFetchColumn->copyDiskColumn
                = (void*)(((mtcColumn*)sSmiColumn)->module->storedValue2MtdValue[sFunctionIdx]);

            if(i == (aXLog->mColCnt - 1))
            {
                sFetchColumn->next = NULL;
            }
            else
            {
                sFetchColumn->next = sFetchColumn + 1;
            }
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_NOT_FOUND_COLUMN);
    {
        idlOS::snprintf(sErrorBuffer, 256,
                        "[rpsSmExecutor::makeFetchColumnList] Column not found [CID: %"ID_UINT32_FMT"] "
                        "at [Remote SN: %"ID_UINT64_FMT", TID: %"ID_UINT32_FMT", TABLEID: %"ID_vULONG_FMT"]",
                        sCID, aXLog->mSN, aXLog->mTID, (vULong)aTableOID);

        IDE_SET(ideSetErrorCode(rpERR_ABORT_RP_INTERNAL_ARG, sErrorBuffer));
    }
    IDE_EXCEPTION(ERR_NOT_FOUND_KEY_COLUMNS);
    {
        idlOS::snprintf(sErrorBuffer, 256,
                        "[rpsSmExecutor::makeFetchColumnList] Key columns not found "
                        "at [Remote SN: %"ID_UINT64_FMT", TID: %"ID_UINT32_FMT", TABLEID: %"ID_vULONG_FMT"]",
                        aXLog->mSN, aXLog->mTID, (vULong)aTableOID);

        IDE_SET(ideSetErrorCode(rpERR_ABORT_RP_INTERNAL_ARG, sErrorBuffer));
    }
    IDE_EXCEPTION(ERR_NULL_KEY_INDEX);
    {
        idlOS::snprintf(sErrorBuffer, 256,
                        "[rpsSmExecutor::makeFetchColumnList] Key index is NULL "
                        "at [Remote SN: %"ID_UINT64_FMT", TID: %"ID_UINT32_FMT", TABLEID: %"ID_vULONG_FMT"]",
                        aXLog->mSN, aXLog->mTID, (vULong)aTableOID);

        IDE_SET(ideSetErrorCode(rpERR_ABORT_RP_INTERNAL_ARG, sErrorBuffer));
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC rpsSmExecutor::makeUpdateColumnList(rpdXLog *aXLog, smOID aTableOID)
{
    UInt              i;
    UInt              sCID;
    const smiColumn  *sSmiColumn;
    const void       *sTable;
    SChar             sErrorBuffer[256];

    IDE_TEST_RAISE(aXLog->mColCnt <= 0, err_update_column_count_le_zero);
    IDE_TEST_RAISE(aXLog->mColCnt > QCI_MAX_COLUMN_COUNT,
                   err_update_column_count_out_of_bounds);

    sTable = smiGetTable( aTableOID );

    for(i = 0; i < aXLog->mColCnt; i ++)
    {
        sCID = aXLog->mCIDs[i];
        IDE_TEST_RAISE(sCID >= QCI_MAX_COLUMN_COUNT,
                       err_out_of_column_id_bound);

        sSmiColumn = rpdCatalog::rpdGetTableColumns(sTable, sCID);
        IDE_TEST_RAISE(sSmiColumn == NULL, ERR_NOT_FOUND_COLUMN);
        IDE_TEST_RAISE((sSmiColumn->id & SMI_COLUMN_ID_MASK) != sCID,
                       err_cid_not_matched);

        idlOS::memcpy(&mCol[i], sSmiColumn, ID_SIZEOF(mtcColumn));

        mUpdateColList[i].column = &mCol[i].column;
        mUpdateColID[i] = mCol[i].column.id;
    }

    for(i = 0; i < (aXLog->mColCnt - 1); i ++)
    {
        mUpdateColList[i].next = &mUpdateColList[i + 1];
    }
    mUpdateColList[i].next = NULL;
    
    IDU_FIT_POINT_RAISE( "1.TASK-2004@rpsSmExecutor::makeUpdateColumnList", 
                          err_update_column_count_le_zero );

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_NOT_FOUND_COLUMN);
    {
        idlOS::snprintf(sErrorBuffer, 256,
                        "[rpsSmExecutor::makeUpdateColumnList] Column not found [CID: %"ID_UINT32_FMT"] "
                        "at [Remote SN: %"ID_UINT64_FMT", TID: %"ID_UINT32_FMT", TABLEID: %"ID_vULONG_FMT"]",
                        sCID, aXLog->mSN, aXLog->mTID, (vULong)aTableOID);

        IDE_SET(ideSetErrorCode(rpERR_ABORT_RP_INTERNAL_ARG, sErrorBuffer));
    }
    IDE_EXCEPTION(err_update_column_count_out_of_bounds);
    {
        idlOS::snprintf( sErrorBuffer, 256,
                         "[rpsSmExecutor::makeUpdateColumnList] Column Count is out of bound "
                         "(%"ID_UINT32_FMT") at [Remote SN: %"ID_UINT64_FMT", TID: %"ID_UINT32_FMT"]",
                         aXLog->mColCnt, aXLog->mSN, aXLog->mTID );

        IDE_SET(ideSetErrorCode(rpERR_ABORT_RP_INTERNAL_ARG, sErrorBuffer));
    }
    IDE_EXCEPTION(err_out_of_column_id_bound);
    {
        idlOS::snprintf( sErrorBuffer, 256,
                         "[rpsSmExecutor::makeUpdateColumnList] Too Large Column ID [%"ID_UINT32_FMT"] "
                         "at [Remote SN: %"ID_UINT64_FMT", TID: %"ID_UINT32_FMT"]",
                         sCID, aXLog->mSN, aXLog->mTID );

        IDE_SET(ideSetErrorCode(rpERR_ABORT_RP_INTERNAL_ARG, sErrorBuffer));
    }
    IDE_EXCEPTION(err_cid_not_matched);
    {
        idlOS::snprintf( sErrorBuffer, 256,
                         "[rpsSmExecutor::makeUpdateColumnList] Mismatch between Column IDs "
                         "[XLog: %"ID_UINT32_FMT", Catalog: %"ID_UINT32_FMT"] "
                         "at [Remote SN: %"ID_UINT64_FMT", TID: %"ID_UINT32_FMT"]",
                         sCID, sSmiColumn->id & SMI_COLUMN_ID_MASK, aXLog->mSN, aXLog->mTID );

        IDE_SET(ideSetErrorCode(rpERR_ABORT_RP_INTERNAL_ARG, sErrorBuffer));
    }
    IDE_EXCEPTION(err_update_column_count_le_zero);
    {
        idlOS::snprintf( sErrorBuffer, 256,
                         "[rpsSmExecutor::makeUpdateColumnList] Update Column Count is LE 0(zero) at "
                         "[Remote SN: %"ID_UINT64_FMT", TID: %"ID_UINT32_FMT"]",
                         aXLog->mSN, aXLog->mTID );

        IDE_SET(ideSetErrorCode(rpERR_ABORT_RP_INTERNAL_ARG, sErrorBuffer));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC rpsSmExecutor::compareUpdateImage(rpdXLog    * aXLog,
                                         smOID        /* aTableOID */,
                                         const void * aRow,
                                         idBool     * aIsConflict)
{
    UInt                i;
    UInt                sCID;
    idBool              sIsMemAlloc = ID_FALSE;
    const smiColumn   * sSmiColumn = NULL;
    mtcColumn         * sMtcColumn;
    const void        * sBeforeValue;
    const void        * sFieldValue;
    UInt                sLength;
    const mtdModule   * sMtd = NULL;
    SChar               sErrorBuffer[256];
    idBool              sIsSamePhysicalImage = ID_FALSE;

    RP_CREATE_FLAG_VARIABLE(IDV_OPTM_INDEX_RP_R_COMPARE_IMAGE);

    RP_OPTIMIZE_TIME_BEGIN(mOpStatistics, IDV_OPTM_INDEX_RP_R_COMPARE_IMAGE);

    *aIsConflict = ID_FALSE;
    mDeleteRowCount = 0;

    IDE_TEST_RAISE(aXLog->mColCnt <= 0, err_update_column_count_le_zero);

    IDE_TEST_RAISE(aXLog->mColCnt > QCI_MAX_COLUMN_COUNT,
                   err_update_column_count_out_of_bounds);

    for(i = 0; i < aXLog->mColCnt; i++)
    {
        sSmiColumn = NULL;
        sCID = aXLog->mCIDs[i];
        IDE_TEST_RAISE( sCID >= QCI_MAX_COLUMN_COUNT,
                        err_out_of_column_id_bound );
        sSmiColumn = &mCol[i].column;

        IDE_TEST_RAISE((sSmiColumn->id & SMI_COLUMN_ID_MASK) != sCID,
                       err_cid_not_matched );

        sMtcColumn = (mtcColumn *)&mCol[i];

        if ( ( sSmiColumn->flag & SMI_COLUMN_COMPRESSION_MASK )
             == SMI_COLUMN_COMPRESSION_FALSE )
        {
            if ( (sSmiColumn->flag & SMI_COLUMN_TYPE_MASK) == SMI_COLUMN_TYPE_FIXED )
            {
                sFieldValue = &((SChar*)aRow)[sSmiColumn->offset];
            }
            else if ( (sSmiColumn->flag & SMI_COLUMN_TYPE_MASK) == SMI_COLUMN_TYPE_VARIABLE ) 
            {
                sFieldValue = smiGetVarColumn( aRow,
                                               sSmiColumn,
                                               &sLength );
            }
            else if ( (sSmiColumn->flag & SMI_COLUMN_TYPE_MASK) == SMI_COLUMN_TYPE_VARIABLE_LARGE )
            {
                // To fix BUG-24356
                // geometry�� ���ؼ��� buffer�Ҵ�
                if ( ( sMtcColumn->type.dataTypeId == MTD_GEOMETRY_ID ) &&
                     ( ( sSmiColumn->flag & SMI_COLUMN_STORAGE_MASK )
                     == SMI_COLUMN_STORAGE_MEMORY ) )
                {
                    sLength = smiGetVarColumnLength(aRow, sSmiColumn);
                    IDE_TEST_RAISE(iduMemMgr::malloc(IDU_MEM_RP_RPS,
                                                 sLength + smiGetVarColumnBufHeadSize(sSmiColumn),
                                                 (void **)&sSmiColumn->value,
                                                 IDU_MEM_IMMEDIATE)
                                   != IDE_SUCCESS, ERR_MEMORY_ALLOC_VALUE);
                    sIsMemAlloc = ID_TRUE;

                    *(sdRID *)(sSmiColumn->value) = SD_NULL_RID;
                }
                sFieldValue = smiGetVarColumn( aRow,
                                               sSmiColumn,
                                               &sLength );
            }
            else if((sSmiColumn->flag & SMI_COLUMN_TYPE_MASK) == SMI_COLUMN_TYPE_LOB)
            {
                // LOB Column�� ���� Conflict üũ�� ���� �ʴ´�.
                continue;
            }
            else
            {
               IDE_RAISE(err_column_unknown_type);
            }
        }
        else
        {
            /* Compressed Column */
            sFieldValue = mtc::getCompressionColumn( aRow,
                                                     sSmiColumn,
                                                     ID_TRUE, /* aUseColumnOffset */
                                                     &sLength );
        }

        sBeforeValue = aXLog->mBCols[i].value;
        IDU_FIT_POINT_RAISE( "rpsSmExecutor::compareUpdateImage::Erratic::rpERR_ABORT_GET_MODULE",
                             ERR_GET_MODULE );
        IDE_TEST_RAISE(mtd::moduleById(&sMtd, sMtcColumn->type.dataTypeId)
                       != IDE_SUCCESS, ERR_GET_MODULE);
        if(sBeforeValue == NULL)
        {
            sBeforeValue = sMtd->staticNull;
        }

        if(sFieldValue == NULL)
        {
            sFieldValue = sMtd->staticNull;
        }

        if((sBeforeValue != NULL) && (sFieldValue != NULL))
        {
            /* To Fix BUG-16531
             * ���������� ������ Image���� �˻��Ѵ�.
             */
            IDE_TEST( mtc::isSamePhysicalImage( sMtcColumn,
                                                sBeforeValue,
                                                MTD_OFFSET_USELESS,
                                                sFieldValue,
                                                MTD_OFFSET_USELESS,
                                                &sIsSamePhysicalImage )
                      != IDE_SUCCESS );

            if ( sIsSamePhysicalImage != ID_TRUE )
            {
                *aIsConflict = ID_TRUE;
                if(sIsMemAlloc == ID_TRUE)
                {
                    (void)iduMemMgr::free(sSmiColumn->value);
                    sIsMemAlloc = ID_FALSE;
                }
                break;
            }
        }
        else
        {
            IDE_RAISE(ERR_VALUE_NULL);
        }

        if(sIsMemAlloc == ID_TRUE)
        {
            (void)iduMemMgr::free(sSmiColumn->value);
            sIsMemAlloc = ID_FALSE;
        }
    }

    RP_OPTIMIZE_TIME_END(mOpStatistics, IDV_OPTM_INDEX_RP_R_COMPARE_IMAGE);

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_VALUE_NULL);
    {
        idlOS::snprintf( sErrorBuffer, 256,
                         "[rpsSmExecutor::compareImage] Value is NULL "
                         "[Before: %"ID_xPOINTER_FMT", Field: %"ID_xPOINTER_FMT"]",
                         sBeforeValue, sFieldValue );
        IDE_SET(ideSetErrorCode(rpERR_ABORT_RP_INTERNAL_ARG, sErrorBuffer));
    }
    IDE_EXCEPTION(err_update_column_count_out_of_bounds);
    {
        idlOS::snprintf( sErrorBuffer, 256,
                         "[rpsSmExecutor::compareImage] Column Count is out of bound "
                         "(%"ID_UINT32_FMT") at [Remote SN: %"ID_UINT64_FMT", TID: %"ID_UINT32_FMT"]",
                         aXLog->mColCnt, aXLog->mSN, aXLog->mTID );

        IDE_SET(ideSetErrorCode(rpERR_ABORT_RP_INTERNAL_ARG, sErrorBuffer));
    }
    IDE_EXCEPTION(err_out_of_column_id_bound);
    {
        idlOS::snprintf( sErrorBuffer, 256,
                         "[rpsSmExecutor::compareImage] Too Large Column ID [%"ID_UINT32_FMT"] "
                         "at [Remote SN: %"ID_UINT64_FMT", TID: %"ID_UINT32_FMT"]",
                         sCID, aXLog->mSN, aXLog->mTID );

        IDE_SET(ideSetErrorCode(rpERR_ABORT_RP_INTERNAL_ARG, sErrorBuffer));
    }
    IDE_EXCEPTION(err_cid_not_matched);
    {
        idlOS::snprintf( sErrorBuffer, 256,
                         "[rpsSmExecutor::compareImage] Mismatch between Column IDs "
                         "[XLog: %"ID_UINT32_FMT", Catalog: %"ID_UINT32_FMT"] "
                         "at [Remote SN: %"ID_UINT64_FMT", TID: %"ID_UINT32_FMT"]",
                         sCID, sSmiColumn->id & SMI_COLUMN_ID_MASK, aXLog->mSN, aXLog->mTID );

        IDE_SET(ideSetErrorCode(rpERR_ABORT_RP_INTERNAL_ARG, sErrorBuffer));
    }
    IDE_EXCEPTION(err_update_column_count_le_zero);
    {
        idlOS::snprintf( sErrorBuffer, 256,
                         "[rpsSmExecutor::compareImage] Update Column Count is LE 0(zero) at "
                         "[Remote SN: %"ID_UINT64_FMT", TID: %"ID_UINT32_FMT"]",
                         aXLog->mSN, aXLog->mTID );

        IDE_SET(ideSetErrorCode(rpERR_ABORT_RP_INTERNAL_ARG, sErrorBuffer));
    }
    IDE_EXCEPTION(ERR_GET_MODULE);
    {
        IDE_SET(ideSetErrorCode(rpERR_ABORT_GET_MODULE));
    }
    IDE_EXCEPTION(err_column_unknown_type)
    {
        idlOS::snprintf( sErrorBuffer, 256,
                         "[rpsSmExecutor::compareImageTS] Update Column Type is unknown at "
                         "[Remote SN: %"ID_UINT64_FMT", TID: %"ID_UINT32_FMT"]",
                         aXLog->mSN, aXLog->mTID );
        IDE_SET(ideSetErrorCode(rpERR_ABORT_RP_INTERNAL_ARG, sErrorBuffer));
    }
    IDE_EXCEPTION(ERR_MEMORY_ALLOC_VALUE);
    {
        IDE_ERRLOG(IDE_RP_0);
        IDE_SET(ideSetErrorCode(rpERR_ABORT_MEMORY_ALLOC,
                                "rpsSmExecutor::compareImage",
                                "sSmiColumn->value"));
    }
    
    IDE_EXCEPTION_END;
    RP_OPTIMIZE_TIME_END(mOpStatistics, IDV_OPTM_INDEX_RP_R_COMPARE_IMAGE);

    IDE_PUSH();

    if(sSmiColumn != NULL)
    {
        if(sIsMemAlloc == ID_TRUE)
        {
            (void)iduMemMgr::free(sSmiColumn->value);
        }
    }

    IDE_POP();
    return IDE_FAILURE;
}

IDE_RC rpsSmExecutor::compareImageTS(rpdXLog     *aXLog,
                                     smOID        /* aTableOID */,
                                     const void  *aRow,
                                     idBool      *aIsConflict,
                                     mtcColumn   *aTsFlag)
{
    UInt               i;
    UInt               sCID;
    const smiColumn   *sSmiColumn = NULL;
    mtcColumn          sMtcColumn;
    const void        *sAfterValue;
    const void        *sFieldValue;
    SInt               sFlag = 0;
    UInt               sLength;
    const mtdModule   *sMtd = NULL;
    SChar              sErrorBuffer[256];
    mtdValueInfo       sValueInfo1;
    mtdValueInfo       sValueInfo2;

    RP_CREATE_FLAG_VARIABLE(IDV_OPTM_INDEX_RP_R_COMPARE_IMAGE);

    RP_OPTIMIZE_TIME_BEGIN(mOpStatistics, IDV_OPTM_INDEX_RP_R_COMPARE_IMAGE);

    *aIsConflict = ID_FALSE;

    IDE_TEST_RAISE(aXLog->mColCnt <= 0, err_update_column_count_le_zero);

    IDE_TEST_RAISE(aXLog->mColCnt > QCI_MAX_COLUMN_COUNT,
                   err_update_column_count_out_of_bounds);

    idlOS::memcpy(&sMtcColumn, aTsFlag, ID_SIZEOF(mtcColumn));

    for(i = 0; i < aXLog->mColCnt; i ++)
    {
        sSmiColumn = NULL;
        sCID = aXLog->mCIDs[i];

        if(sCID != (aTsFlag->column.id & SMI_COLUMN_ID_MASK))
        {
            continue;
        }

        IDE_TEST_RAISE(sCID >= QCI_MAX_COLUMN_COUNT,
                       err_out_of_column_id_bound);

        sSmiColumn = &sMtcColumn.column;
        IDE_TEST_RAISE((sSmiColumn->id & SMI_COLUMN_ID_MASK) != sCID,
                       err_cid_not_matched);

        if((sSmiColumn->flag & SMI_COLUMN_TYPE_MASK) == SMI_COLUMN_TYPE_FIXED)
        {
            sFieldValue = &((SChar*)aRow)[sSmiColumn->offset];
        }
        else if ( ( (sSmiColumn->flag & SMI_COLUMN_TYPE_MASK) == SMI_COLUMN_TYPE_VARIABLE ) ||
                  ( (sSmiColumn->flag & SMI_COLUMN_TYPE_MASK) == SMI_COLUMN_TYPE_VARIABLE_LARGE ) )
        {
            sFieldValue = smiGetVarColumn( aRow,
                                          sSmiColumn,
                                          &sLength);
        }
        else
        {
            IDE_RAISE(err_column_unknown_type);
        }

        sAfterValue = aXLog->mACols[i].value;

        IDU_FIT_POINT_RAISE( "rpsSmExecutor::compareImageTS::Erratic::rpERR_ABORT_GET_MODULE",
                             ERR_GET_MODULE );
        IDE_TEST_RAISE(mtd::moduleById(&sMtd, sMtcColumn.type.dataTypeId)
                       != IDE_SUCCESS, ERR_GET_MODULE);

        if(sAfterValue == NULL)
        {
            sAfterValue = sMtd->staticNull;
        }

        if(sFieldValue == NULL)
        {
            sFieldValue = sMtd->staticNull;
        }

        sValueInfo1.column = (const mtcColumn*)&sMtcColumn;
        sValueInfo1.value  = sAfterValue;
        sValueInfo1.flag   = MTD_OFFSET_USELESS;

        sValueInfo2.column = (const mtcColumn*)&sMtcColumn;
        sValueInfo2.value  = sFieldValue;
        sValueInfo2.flag   = MTD_OFFSET_USELESS;

        sFlag = sMtd->logicalCompare[MTD_COMPARE_ASCENDING](&sValueInfo1, &sValueInfo2);

        if(sFlag < 0)
        {
            *aIsConflict = ID_TRUE;
        }
        else
        {
            // nothing to do
        }

        break;
    }

    RP_OPTIMIZE_TIME_END(mOpStatistics, IDV_OPTM_INDEX_RP_R_COMPARE_IMAGE);

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_update_column_count_out_of_bounds);
    {
        idlOS::snprintf( sErrorBuffer, 256,
                         "[rpsSmExecutor::compareImageTS] Column Count is out of bound "
                         "(%"ID_UINT32_FMT") at [Remote SN: %"ID_UINT64_FMT", TID: %"ID_UINT32_FMT"]",
                         aXLog->mColCnt, aXLog->mSN, aXLog->mTID );

        IDE_SET(ideSetErrorCode(rpERR_ABORT_RP_INTERNAL_ARG, sErrorBuffer));
    }
    IDE_EXCEPTION(err_out_of_column_id_bound);
    {
        idlOS::snprintf( sErrorBuffer, 256,
                         "[rpsSmExecutor::compareImageTS] Too Large Column ID [%"ID_UINT32_FMT"] "
                         "at [Remote SN: %"ID_UINT64_FMT", TID: %"ID_UINT32_FMT"]",
                         sCID, aXLog->mSN, aXLog->mTID );

        IDE_SET(ideSetErrorCode(rpERR_ABORT_RP_INTERNAL_ARG, sErrorBuffer));
    }
    IDE_EXCEPTION(err_cid_not_matched);
    {
        idlOS::snprintf( sErrorBuffer, 256,
                         "[rpsSmExecutor::compareImageTS] Mismatch between Column IDs "
                         "[XLog: %"ID_UINT32_FMT", Catalog: %"ID_UINT32_FMT"] "
                         "at [Remote SN: %"ID_UINT64_FMT", TID: %"ID_UINT32_FMT"]",
                         sCID, sSmiColumn->id & SMI_COLUMN_ID_MASK, aXLog->mSN, aXLog->mTID );

        IDE_SET(ideSetErrorCode(rpERR_ABORT_RP_INTERNAL_ARG, sErrorBuffer));
    }
    IDE_EXCEPTION(err_update_column_count_le_zero);
    {
        idlOS::snprintf( sErrorBuffer, 256,
                         "[rpsSmExecutor::compareImageTS] Update Column Count is LE 0(zero) at "
                         "[Remote SN: %"ID_UINT64_FMT", TID: %"ID_UINT32_FMT"]",
                         aXLog->mSN, aXLog->mTID );

        IDE_SET(ideSetErrorCode(rpERR_ABORT_RP_INTERNAL_ARG, sErrorBuffer));
    }
    IDE_EXCEPTION(err_column_unknown_type)
    {
        idlOS::snprintf( sErrorBuffer, 256,
                         "[rpsSmExecutor::compareImageTS] Update Column Type is unknown at "
                         "[Remote SN: %"ID_UINT64_FMT", TID: %"ID_UINT32_FMT"]",
                         aXLog->mSN, aXLog->mTID );

        IDE_SET(ideSetErrorCode(rpERR_ABORT_RP_INTERNAL_ARG, sErrorBuffer));
    }
    IDE_EXCEPTION(ERR_GET_MODULE);
    {
        IDE_SET(ideSetErrorCode(rpERR_ABORT_GET_MODULE));
    }
    IDE_EXCEPTION_END;
    RP_OPTIMIZE_TIME_END(mOpStatistics, IDV_OPTM_INDEX_RP_R_COMPARE_IMAGE);

    return IDE_FAILURE;
}

IDE_RC rpsSmExecutor::compareInsertImage( smiTrans         * aTrans,
                                          rpdXLog          * aXLog,
                                          rpdMetaItem      * aMetaItem,
                                          smiRange         * aKeyRange,
                                          idBool           * aIsConflict,
                                          mtcColumn        * aTsFlag,
                                          rpApplyFailType  * aFailType )
{
    const void*         sRow;
    scGRID              sRid;
    smiTableCursor      sCursor;
    const void*         sTable;
    const void*         sIndex;
    UInt                sCount;
    UInt                sStep = 0;
    smiStatement        sSmiStmt;
    smiCursorProperties sProperty;

    *aIsConflict = ID_FALSE;

    sTable = smiGetTable( aMetaItem->mItem.mTableOID );

    // get primary key index
    sIndex = smiGetTablePrimaryKeyIndex( sTable );

    SMI_CURSOR_PROP_INIT(&sProperty, NULL, sIndex); // PROJ-1705

    /* PROJ-1705 Fetch Column List ���� */
    if((SMI_MISC_TABLE_HEADER(sTable)->mFlag & SMI_TABLE_TYPE_MASK) == SMI_TABLE_DISK)
    {
        // PROJ-1705 Fetch Column List ����
        IDE_TEST( makeFetchColumnList( aXLog, aMetaItem->mItem.mTableOID, NULL )
                  != IDE_SUCCESS );

        // PROJ-1705 smiCursorProperties�� Fetch Column List ���� ����
        sProperty.mFetchColumnList = mFetchColumnList;
    }
    else
    {
        // memory table�� fetch column list�� �ʿ����� �ʴ�.
        sProperty.mFetchColumnList = NULL;
    }

    IDE_TEST( sSmiStmt.begin( aTrans->getStatistics(),
                              aTrans->getStatement(),
                              SMI_STATEMENT_NORMAL | SMI_STATEMENT_ALL_CURSOR )
              != IDE_SUCCESS );
    sStep = 1;

    sCursor.initialize();

    IDE_TEST(cursorOpen(&sCursor,
                        &sSmiStmt,
                        sTable,
                        sIndex,
                        smiGetRowSCN(sTable),
                        NULL,
                        aKeyRange,
                        smiGetDefaultKeyRange(),
                        smiGetDefaultFilter(),
                        SMI_LOCK_WRITE | SMI_TRAVERSE_FORWARD | SMI_PREVIOUS_DISABLE,
                        SMI_SELECT_CURSOR,
                        &sProperty)
             != IDE_SUCCESS);
    sStep = 2;

    IDE_TEST(sCursor.beforeFirst() != IDE_SUCCESS);

    sCount = 0;

    if((SMI_MISC_TABLE_HEADER(sTable)->mFlag & SMI_TABLE_TYPE_MASK) == SMI_TABLE_DISK)
    {
        sRow = (const void*)mRealRow;
    }

    IDU_FIT_POINT( "rpsSmExecutor::compareInsertImage:readRow::corrupted",
                   smERR_ABORT_PageCorrupted, 0, 0 );
   
    IDE_TEST( sCursor.readRow(&sRow, &sRid, SMI_FIND_NEXT) != IDE_SUCCESS );
    /* Row�� ������ Insert�� �� �����Ƿ�, Conflict�� �ƴϴ�. */

    while(sRow != NULL)
    {
        sCount++;
        IDE_TEST_RAISE(sCount > 1, ERR_PK_MODIFIED_MORE_THAN_ONE_ROW);

        /* Timestamp �÷����� ������ �ͺ��� ������, Conflict�̴�. */
        IDE_TEST( compareImageTS( aXLog,
                                  aMetaItem->mItem.mTableOID,
                                  sRow,
                                  aIsConflict,
                                  aTsFlag )
                  != IDE_SUCCESS );

        IDE_TEST( sCursor.readRow(&sRow, &sRid, SMI_FIND_NEXT) != IDE_SUCCESS );
    }

    sStep = 1;
    IDE_TEST(cursorClose(&sCursor) != IDE_SUCCESS);

    IDE_TEST(sSmiStmt.end( SMI_STATEMENT_RESULT_SUCCESS) != IDE_SUCCESS);
    sStep = 0;

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_PK_MODIFIED_MORE_THAN_ONE_ROW);
    {
        IDE_SET(ideSetErrorCode( rpERR_ABORT_RP_INTERNAL_ARG,
                                 "[rpsSmExecutor::compareInsertImage] ERR_PK_MODIFIED_MORE_THAN_ONE_ROW"));
    }
    IDE_EXCEPTION_END;

    if ( RP_IS_CORRUPTED_PAGE() == ID_TRUE )
    {
        *aFailType = RP_APPLY_FAIL_BY_CORRUPTED_PAGE;
    }
    else
    {
        /* BUG-47053 Disk corrupt ��å ���濡 ���� ����ó�� �߰�
         * �� �Լ��� �̹� conflic�� �߻� �� ȣ��Ǵ� �Լ��̴�.
         * �������� aFailType�� �������־���, ���⼭ �߻��ϴ� corrupt�� ��쿡�� �������ָ� �ȴ�. */
    }

    IDE_PUSH();

    switch (sStep)
    {
        case 2 :
            if(cursorClose(&sCursor) != IDE_SUCCESS)
            {
                IDE_ERRLOG(IDE_RP_0);
            }
        case 1 :
            if(sSmiStmt.end(SMI_STATEMENT_RESULT_FAILURE) != IDE_SUCCESS)
            {
                IDE_ERRLOG(IDE_RP_0);
            }
        default :
            break;
    }

    IDE_POP();
    return IDE_FAILURE;
}

IDE_RC rpsSmExecutor::openLOBCursor( smiTrans         * aTrans,
                                     rpdXLog          * aXLog,
                                     rpdMetaItem      * aMetaItem,
                                     smiRange         * aKeyRange,
                                     rpdTransTbl      * aTransTbl,
                                     idBool           * aIsLOBOperationException,
                                     rpApplyFailType  * aFailType )
{
    const void         *sRow;
    scGRID              sGRID;
    smiTableCursor      sCursor;
    const void*         sTable;
    const void*         sIndex;
    UInt                sCount;
    UInt                sStep = 0;
    smiStatement        sSmiStmt;
    smiCursorProperties sProperty;
    smLobLocator        sLocalLobLocator = SMI_NULL_LOCATOR;
    mtcColumn           sMtcColumn;
    const smiColumn    *sSmiColumn;
    SChar               sErrorBuffer[256];

    RP_CREATE_FLAG_VARIABLE(IDV_OPTM_INDEX_RP_R_OPEN_LOB_CURSOR);

    RP_OPTIMIZE_TIME_BEGIN(mOpStatistics, IDV_OPTM_INDEX_RP_R_OPEN_LOB_CURSOR);

    *aIsLOBOperationException = ID_FALSE;
   *aFailType = RP_APPLY_FAIL_NONE;

    sTable = smiGetTable( aMetaItem->mItem.mTableOID );

    sSmiColumn = rpdCatalog::rpdGetTableColumns(sTable, aXLog->mLobPtr->mLobColumnID);
    IDE_TEST_RAISE(sSmiColumn == NULL, ERR_NOT_FOUND_COLUMN);

    idlOS::memcpy(&sMtcColumn, sSmiColumn, ID_SIZEOF(mtcColumn));

    // get primary key index
    sIndex = smiGetTablePrimaryKeyIndex( sTable );

    SMI_CURSOR_PROP_INIT(&sProperty, NULL, sIndex); //PROJ-1705

    /* PROJ-1705 Fetch Column List ���� */
    if((SMI_MISC_TABLE_HEADER(sTable)->mFlag & SMI_TABLE_TYPE_MASK) == SMI_TABLE_DISK)
    {
        // PROJ-1705 Fetch Column List ����
        IDE_TEST_RAISE( makeFetchColumnList( aXLog, aMetaItem->mItem.mTableOID, sIndex )
                        != IDE_SUCCESS, ERR_MAKE_FETCH_COLUMN_LIST );

        // PROJ-1705 smiCursorProperties�� Fetch Column List ���� ����
        sProperty.mFetchColumnList = mFetchColumnList;
    }
    else
    {
        // memory table�� fetch column list�� �ʿ����� �ʴ�.
        sProperty.mFetchColumnList = NULL;
    }
    sProperty.mLockRowBuffer = (UChar*)mRealRow;
    sProperty.mLockRowBufferSize = mRealRowSize;


    IDE_TEST_RAISE( sSmiStmt.begin( aTrans->getStatistics(),
                                    aTrans->getStatement(),
                                    SMI_STATEMENT_NORMAL |
                                    SMI_STATEMENT_ALL_CURSOR )
                    != IDE_SUCCESS, ERR_STMT_BEGIN );
    sStep = 1;

    sCursor.initialize();

    IDE_TEST_RAISE( cursorOpen( &sCursor,
                                &sSmiStmt,
                                sTable,
                                sIndex,
                                smiGetRowSCN(sTable),
                                NULL,
                                aKeyRange,
                                smiGetDefaultKeyRange(),
                                smiGetDefaultFilter(),
                                SMI_LOCK_REPEATABLE | SMI_TRAVERSE_FORWARD | SMI_PREVIOUS_DISABLE,
                                SMI_SELECT_CURSOR,
                                &sProperty )
                    != IDE_SUCCESS, ERR_CURSOR_OPEN );
    sStep = 2;

    IDE_TEST( sCursor.beforeFirst() != IDE_SUCCESS );

    sCount = 0;

    if((SMI_MISC_TABLE_HEADER(sTable)->mFlag & SMI_TABLE_TYPE_MASK) == SMI_TABLE_DISK)
    {
        sRow = (const void*)mRealRow;
    }

    IDE_TEST(sCursor.readRow(&sRow, &sGRID, SMI_FIND_NEXT) != IDE_SUCCESS);

    IDE_TEST_RAISE(sRow == NULL, ERR_NO_ROW_IS_FOUND);

    if((SMI_MISC_TABLE_HEADER(sTable)->mFlag & SMI_TABLE_TYPE_MASK) == SMI_TABLE_DISK)
    {
        IDU_FIT_POINT_RAISE( "rpsSmExecutor::openLOBCursor::Erratic::rpERR_ABORT_OPEN_LOB_CURSOR",
                             ERR_LOB_CURSOR_OPEN ); 
        IDE_TEST_RAISE(smiLob::openLobCursorWithGRID(&sCursor,
                                                     sGRID,
                                                     &sMtcColumn.column,
                                                     0,
                                                     SMI_LOB_READ_WRITE_MODE,
                                                     &sLocalLobLocator)
                       != IDE_SUCCESS, ERR_LOB_CURSOR_OPEN );
    }
    else
    {
        IDE_TEST_RAISE(smiLob::openLobCursorWithRow(&sCursor,
                                                    (void *)sRow,
                                                    &sMtcColumn.column,
                                                    0,
                                                    SMI_LOB_READ_WRITE_MODE,
                                                    &sLocalLobLocator)
                       != IDE_SUCCESS, ERR_LOB_CURSOR_OPEN );
    }

    while(sRow != NULL)
    {
        sCount++;
        IDE_TEST_RAISE(sCount > 1, ERR_PK_MODIFIED_MORE_THAN_ONE_ROW);

        IDU_FIT_POINT( "rpsSmExecutor::openLOBCursor::readRow::corrupted",
                       smERR_ABORT_PageCorrupted, 0, 0 );

        IDE_TEST(sCursor.readRow(&sRow, &sGRID, SMI_FIND_NEXT) != IDE_SUCCESS);
    }

    sStep = 1;
    IDE_TEST(cursorClose(&sCursor) != IDE_SUCCESS);

    IDE_TEST(sSmiStmt.end(SMI_STATEMENT_RESULT_SUCCESS) != IDE_SUCCESS);
    sStep = 0;

    /* Remote LOB Locator�� ���� ���� Local LOB Locator�� Ʈ����� Table�� ���� */
    IDE_TEST(aTransTbl->insertLocator(aXLog->mTID,
                                      aXLog->mLobPtr->mLobLocator,
                                      sLocalLobLocator)
             != IDE_SUCCESS);

    RP_OPTIMIZE_TIME_END(mOpStatistics, IDV_OPTM_INDEX_RP_R_OPEN_LOB_CURSOR);

    return IDE_SUCCESS;
    IDE_EXCEPTION(ERR_NOT_FOUND_COLUMN);
    {
        IDE_SET(ideSetErrorCode( rpERR_ABORT_RP_INTERNAL_ARG,
                                 "[rpsSmExecutor::openLOBCursor] Column not found" ) );
    }
    IDE_EXCEPTION( ERR_MAKE_FETCH_COLUMN_LIST );
    {
        IDE_ERRLOG(IDE_RP_0);
        IDE_SET(ideSetErrorCode( rpERR_ABORT_RP_INTERNAL_ARG,
                                 "[rpsSmExecutor::openLOBCursor] failed making fetch column list" ) );
    }
    IDE_EXCEPTION( ERR_STMT_BEGIN );
    {
        IDE_ERRLOG(IDE_RP_0);
        IDE_SET(ideSetErrorCode( rpERR_ABORT_RP_INTERNAL_ARG,
                                 "[rpsSmExecutor::openLOBCursor] failed statement begin" ) );
    }
    IDE_EXCEPTION(ERR_NO_ROW_IS_FOUND);
    {
        IDE_SET(ideSetErrorCode(rpERR_ABORT_NOT_FOUND_RECORD, "openLOBCursor()"));
    }
    IDE_EXCEPTION( ERR_CURSOR_OPEN );
    {
        IDE_ERRLOG(IDE_RP_0);
        IDE_SET(ideSetErrorCode( rpERR_ABORT_RP_INTERNAL_ARG,
                                 "[rpsSmExecutor::openLOBCursor] failed cursor open" ) );
    }
    IDE_EXCEPTION(ERR_PK_MODIFIED_MORE_THAN_ONE_ROW);
    {
        IDE_SET(ideSetErrorCode( rpERR_ABORT_RP_INTERNAL_ARG,
                                 "[rpsSmExecutor::openLOBCursor] ERR_PK_MODIFIED_MORE_THAN_ONE_ROW"));

    }
    IDE_EXCEPTION(ERR_LOB_CURSOR_OPEN);
    {
        /* BUG-32423 If the receiver get not enough free space,
         *           then the receiver stop. (LOB)
         * BUG-31851 The transaction needs rollback in eager mode.
         */
        if ( ( ideGetErrorCode() == smERR_ABORT_NOT_ENOUGH_SPACE ) &&
             ( mIsConflictWhenNotEnoughSpace == ID_FALSE ) )
        {
            /* Nothing to do */
        }
        else
        {
            *aIsLOBOperationException = ID_TRUE;
        }

        IDE_ERRLOG(IDE_RP_0);
        IDE_SET(ideSetErrorCode(rpERR_ABORT_OPEN_LOB_CURSOR));
    }
    IDE_EXCEPTION_END;

    if ( ( ideGetErrorCode() == smERR_ABORT_smnUniqueViolationInReplTrans ) ||
         ( ideGetErrorCode() == smERR_ABORT_smcExceedLockTimeWait ) )
    {            
        *aFailType = RP_APPLY_FAIL_BY_CONFLICT;
    }
    else
    {
        if ( RP_IS_CORRUPTED_PAGE() == ID_TRUE )
        {
            *aFailType = RP_APPLY_FAIL_BY_CORRUPTED_PAGE;
        }
    }
    
    if ( *aFailType == RP_APPLY_FAIL_NONE )
    {
        *aFailType = RP_APPLY_FAIL_INTERNAL;
    }
    else
    {
        /* Nothing to do */
    }

    RP_OPTIMIZE_TIME_END(mOpStatistics, IDV_OPTM_INDEX_RP_R_OPEN_LOB_CURSOR);

    IDE_ERRLOG( IDE_RP_0 );
    idlOS::snprintf( sErrorBuffer, 256,
                     "[rpsSmExecutor::openLOBCursor] "
                     "[CID: %"ID_UINT32_FMT"] "
                     "at [Remote SN: %"ID_UINT64_FMT", TID: %"ID_UINT32_FMT", TABLEID: %"ID_vULONG_FMT"]",
                     aXLog->mLobPtr->mLobColumnID, aXLog->mSN, aXLog->mTID, (vULong)aMetaItem->mItem.mTableOID );
    IDE_SET( ideSetErrorCode( rpERR_ABORT_RP_INTERNAL_ARG, sErrorBuffer ) );

    IDE_PUSH();

    switch (sStep)
    {
        case 2 :
            if(cursorClose(&sCursor) != IDE_SUCCESS)
            {
                IDE_ERRLOG(IDE_RP_0);
            }
        case 1 :
            if(sSmiStmt.end(SMI_STATEMENT_RESULT_FAILURE) != IDE_SUCCESS)
            {
                IDE_ERRLOG(IDE_RP_0);
            }
        default :
            break;
    }

    IDE_POP();
    return IDE_FAILURE;
}

IDE_RC rpsSmExecutor::closeLOBCursor(rpdXLog     *aXLog,
                                     rpdTransTbl *aTransTbl,
                                     idBool      *aIsLOBOperationException)
{
    smLobLocator sLocalLobLocator = SMI_NULL_LOCATOR;
    idBool       sIsFound;
    SChar        sErrorBuffer[256];

    RP_CREATE_FLAG_VARIABLE(IDV_OPTM_INDEX_RP_R_CLOSE_LOB_CURSOR);

    RP_OPTIMIZE_TIME_BEGIN(mOpStatistics, IDV_OPTM_INDEX_RP_R_CLOSE_LOB_CURSOR);

    *aIsLOBOperationException = ID_FALSE;

    IDE_TEST_RAISE( aTransTbl->searchLocator( aXLog->mTID,
                                              aXLog->mLobPtr->mLobLocator,
                                              &sLocalLobLocator,
                                              &sIsFound )
                    != IDE_SUCCESS, ERR_SEARCH_LOBLOCATOR );

    /* PROJ-1442 Replication Online �� DDL ���
     * �ش� LOB Column�� Replication ����� �ƴϸ�,
     * LOB Cursor�� Open���� �����Ƿ� Locator�� ã�� �� ����.
     */
    if(sIsFound == ID_TRUE)
    {
        aTransTbl->removeLocator(aXLog->mTID, aXLog->mLobPtr->mLobLocator);

        IDU_FIT_POINT_RAISE( "rpsSmExecutor::closeLOBCursor::Erratic::rpERR_ABORT_CLOSE_LOB_CURSOR",
                             ERR_LOB_CURSOR_CLOSE );
        IDE_TEST_RAISE(smiLob::closeLobCursor(NULL, /* idvSQL* */
                                              sLocalLobLocator)
                       != IDE_SUCCESS, ERR_LOB_CURSOR_CLOSE);
    }

    RP_OPTIMIZE_TIME_END(mOpStatistics, IDV_OPTM_INDEX_RP_R_CLOSE_LOB_CURSOR);

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_LOB_CURSOR_CLOSE);
    {
        /* BUG-32423 If the receiver get not enough free space,
         *           then the receiver stop. (LOB)
         * BUG-31851 The transaction needs rollback in eager mode.
         */
        if ( ( ideGetErrorCode() == smERR_ABORT_NOT_ENOUGH_SPACE ) &&
             ( mIsConflictWhenNotEnoughSpace == ID_FALSE ) )
        {
            /* Nothing to do */
        }
        else
        {
            *aIsLOBOperationException = ID_TRUE;
        }

        IDE_ERRLOG(IDE_RP_0);
        IDE_SET(ideSetErrorCode(rpERR_ABORT_CLOSE_LOB_CURSOR));
    }
    IDE_EXCEPTION( ERR_SEARCH_LOBLOCATOR );
    {
        IDE_ERRLOG( IDE_RP_0 );
        IDE_SET( ideSetErrorCode( rpERR_ABORT_RP_INTERNAL_ARG,
                                  "[rpsSmExecutor::closeLOBCursor] failed searchLocator" ) );
    }
    IDE_EXCEPTION_END;

    RP_OPTIMIZE_TIME_END(mOpStatistics, IDV_OPTM_INDEX_RP_R_CLOSE_LOB_CURSOR);

    IDE_ERRLOG( IDE_RP_0 );
    idlOS::snprintf( sErrorBuffer, 256,
                     "[rpsSmExecutor::closeLOBCursor] "
                     "[CID: %"ID_UINT32_FMT"] "
                     "at [Remote SN: %"ID_UINT64_FMT", TID: %"ID_UINT32_FMT"]",
                     aXLog->mLobPtr->mLobColumnID, aXLog->mSN, aXLog->mTID );
    IDE_SET( ideSetErrorCode( rpERR_ABORT_RP_INTERNAL_ARG, sErrorBuffer ) );

    return IDE_FAILURE;
}

IDE_RC rpsSmExecutor::prepareLOBWrite(rpdXLog     *aXLog,
                                      rpdTransTbl *aTransTbl,
                                      idBool      *aIsLOBOperationException)
{
    smLobLocator sLocalLobLocator = SMI_NULL_LOCATOR;
    idBool       sIsFound;
    SChar        sErrorBuffer[256];

    RP_CREATE_FLAG_VARIABLE(IDV_OPTM_INDEX_RP_R_PREPARE_LOB_WRITE);

    RP_OPTIMIZE_TIME_BEGIN(mOpStatistics, IDV_OPTM_INDEX_RP_R_PREPARE_LOB_WRITE);

    *aIsLOBOperationException = ID_FALSE;

    IDE_TEST_RAISE( aTransTbl->searchLocator( aXLog->mTID,
                                              aXLog->mLobPtr->mLobLocator,
                                              &sLocalLobLocator,
                                              &sIsFound )
                    != IDE_SUCCESS, ERR_SEARCH_LOBLOCATOR );

    /* PROJ-1442 Replication Online �� DDL ���
     * �ش� LOB Column�� Replication ����� �ƴϸ�,
     * LOB Cursor�� Open���� �����Ƿ� Locator�� ã�� �� ����.
     */
    if(sIsFound == ID_TRUE)
    {
        IDE_TEST_RAISE(smiLob::prepare4Write(NULL, /* idvSQL* */
                                             sLocalLobLocator,
                                             aXLog->mLobPtr->mLobOffset,
                                             aXLog->mLobPtr->mLobNewSize)
                       != IDE_SUCCESS, ERR_LOB_PREPARE4WRITE);
    }

    RP_OPTIMIZE_TIME_END(mOpStatistics, IDV_OPTM_INDEX_RP_R_PREPARE_LOB_WRITE);

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_LOB_PREPARE4WRITE);
    {
        /* BUG-32423 If the receiver get not enough free space,
         *           then the receiver stop. (LOB)
         * BUG-31851 The transaction needs rollback in eager mode.
         */
        if ( ( ideGetErrorCode() == smERR_ABORT_NOT_ENOUGH_SPACE ) &&
             ( mIsConflictWhenNotEnoughSpace == ID_FALSE ) )
        {
            /* Nothing to do */
        }
        else
        {
            *aIsLOBOperationException = ID_TRUE;
        }

        IDE_ERRLOG(IDE_RP_0);
        IDE_SET(ideSetErrorCode(rpERR_ABORT_LOB_PREPARE_WRITE));
    }
    IDE_EXCEPTION( ERR_SEARCH_LOBLOCATOR );
    {
        IDE_ERRLOG( IDE_RP_0 );
        IDE_SET( ideSetErrorCode( rpERR_ABORT_RP_INTERNAL_ARG,
                                  "[rpsSmExecutor::prepareLOBWrite] failed searchLocator" ) );
    }
    IDE_EXCEPTION_END;

    RP_OPTIMIZE_TIME_END(mOpStatistics, IDV_OPTM_INDEX_RP_R_PREPARE_LOB_WRITE);

    IDE_ERRLOG( IDE_RP_0 );
    idlOS::snprintf( sErrorBuffer, 256,
                     "[rpsSmExecutor::prepareLOBWrite] "
                     "[CID: %"ID_UINT32_FMT"] "
                     "at [Remote SN: %"ID_UINT64_FMT", TID: %"ID_UINT32_FMT"]",
                     aXLog->mLobPtr->mLobColumnID, aXLog->mSN, aXLog->mTID );
    IDE_SET( ideSetErrorCode( rpERR_ABORT_RP_INTERNAL_ARG, sErrorBuffer ) );

    return IDE_FAILURE;
}

IDE_RC rpsSmExecutor::finishLOBWrite(rpdXLog     *aXLog,
                                     rpdTransTbl *aTransTbl,
                                     idBool      *aIsLOBOperationException)
{
    smLobLocator sLocalLobLocator = SMI_NULL_LOCATOR;
    idBool       sIsFound;
    SChar        sErrorBuffer[256];

    RP_CREATE_FLAG_VARIABLE(IDV_OPTM_INDEX_RP_R_FINISH_LOB_WRITE);

    RP_OPTIMIZE_TIME_BEGIN(mOpStatistics, IDV_OPTM_INDEX_RP_R_FINISH_LOB_WRITE); 

    *aIsLOBOperationException = ID_FALSE;

    IDE_TEST_RAISE( aTransTbl->searchLocator( aXLog->mTID,
                                              aXLog->mLobPtr->mLobLocator,
                                              &sLocalLobLocator,
                                              &sIsFound )
                    != IDE_SUCCESS, ERR_SEARCH_LOBLOCATOR );

    /* PROJ-1442 Replication Online �� DDL ���
     * �ش� LOB Column�� Replication ����� �ƴϸ�,
     * LOB Cursor�� Open���� �����Ƿ� Locator�� ã�� �� ����.
     */
    if(sIsFound == ID_TRUE)
    {
        IDE_TEST_RAISE(smiLob::finishWrite(NULL, /* idvSQL* */
                                           sLocalLobLocator)
                       != IDE_SUCCESS, ERR_LOB_FINISH_WRITE);
    }

    RP_OPTIMIZE_TIME_END(mOpStatistics, IDV_OPTM_INDEX_RP_R_FINISH_LOB_WRITE);

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_LOB_FINISH_WRITE);
    {
        /* BUG-32423 If the receiver get not enough free space,
         *           then the receiver stop. (LOB)
         * BUG-31851 The transaction needs rollback in eager mode.
         */
        if ( ( ideGetErrorCode() == smERR_ABORT_NOT_ENOUGH_SPACE ) &&
             ( mIsConflictWhenNotEnoughSpace == ID_FALSE ) )
        {
            /* Nothing to do */
        }
        else
        {
            *aIsLOBOperationException = ID_TRUE;
        }

        IDE_ERRLOG(IDE_RP_0);
        IDE_SET(ideSetErrorCode(rpERR_ABORT_LOB_FINISH_WRITE));
    }
    IDE_EXCEPTION( ERR_SEARCH_LOBLOCATOR );
    {
        IDE_ERRLOG( IDE_RP_0 );
        IDE_SET( ideSetErrorCode( rpERR_ABORT_RP_INTERNAL_ARG,
                                  "[rpsSmExecutor::finishLOBWrite] failed searchLocator" ) );
    }
    IDE_EXCEPTION_END;

    RP_OPTIMIZE_TIME_END(mOpStatistics, IDV_OPTM_INDEX_RP_R_FINISH_LOB_WRITE);

    IDE_ERRLOG( IDE_RP_0 );
    idlOS::snprintf( sErrorBuffer, 256,
                     "[rpsSmExecutor::finishLOBWrite] "
                     "[CID: %"ID_UINT32_FMT"] "
                     "at [Remote SN: %"ID_UINT64_FMT", TID: %"ID_UINT32_FMT"]",
                     aXLog->mLobPtr->mLobColumnID, aXLog->mSN, aXLog->mTID );
    IDE_SET( ideSetErrorCode( rpERR_ABORT_RP_INTERNAL_ARG, sErrorBuffer ) );

    return IDE_FAILURE;
}

IDE_RC rpsSmExecutor::trimLOB(rpdXLog     *aXLog,
                              rpdTransTbl *aTransTbl,
                              idBool      *aIsLOBOperationException)
{
    smLobLocator sLocalLobLocator = SMI_NULL_LOCATOR;
    idBool       sIsFound;
    SChar        sErrorBuffer[256];

    RP_CREATE_FLAG_VARIABLE(IDV_OPTM_INDEX_RP_R_TRIM_LOB);

    RP_OPTIMIZE_TIME_BEGIN(mOpStatistics, IDV_OPTM_INDEX_RP_R_TRIM_LOB);

    *aIsLOBOperationException = ID_FALSE;

    IDE_TEST_RAISE( aTransTbl->searchLocator( aXLog->mTID,
                                              aXLog->mLobPtr->mLobLocator,
                                              &sLocalLobLocator,
                                              &sIsFound )
                    != IDE_SUCCESS, ERR_SEARCH_LOBLOCATOR );

    /* PROJ-1442 Replication Online �� DDL ���
     * �ش� LOB Column�� Replication ����� �ƴϸ�,
     * LOB Cursor�� Open���� �����Ƿ� Locator�� ã�� �� ����.
     */
    if(sIsFound == ID_TRUE)
    {
        IDE_TEST_RAISE(smiLob::trim(NULL, /* idvSQL* */
                                    sLocalLobLocator,
                                    aXLog->mLobPtr->mLobOffset)
                       != IDE_SUCCESS, ERR_LOB_TRIM);
    }

    RP_OPTIMIZE_TIME_END(mOpStatistics, IDV_OPTM_INDEX_RP_R_TRIM_LOB);

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_LOB_TRIM);
    {
        /* BUG-32423 If the receiver get not enough free space,
         *           then the receiver stop. (LOB)
         * BUG-31851 The transaction needs rollback in eager mode.
         */
        if ( ( ideGetErrorCode() == smERR_ABORT_NOT_ENOUGH_SPACE ) &&
             ( mIsConflictWhenNotEnoughSpace == ID_FALSE ) )
        {
            /* Nothing to do */
        }
        else
        {
            *aIsLOBOperationException = ID_TRUE;
        }

        IDE_ERRLOG(IDE_RP_0);
        IDE_SET(ideSetErrorCode(rpERR_ABORT_LOB_TRIM));
    }
    IDE_EXCEPTION( ERR_SEARCH_LOBLOCATOR );
    {
        IDE_ERRLOG( IDE_RP_0 );
        IDE_SET( ideSetErrorCode( rpERR_ABORT_RP_INTERNAL_ARG,
                                  "[rpsSmExecutor::trimLOB] failed searchLocator" ) );
    }
    IDE_EXCEPTION_END;

    RP_OPTIMIZE_TIME_END(mOpStatistics, IDV_OPTM_INDEX_RP_R_TRIM_LOB);

    IDE_ERRLOG( IDE_RP_0 );
    idlOS::snprintf( sErrorBuffer, 256,
                     "[rpsSmExecutor::trimLOB] "
                     "[CID: %"ID_UINT32_FMT"] "
                     "at [Remote SN: %"ID_UINT64_FMT", TID: %"ID_UINT32_FMT"]",
                     aXLog->mLobPtr->mLobColumnID, aXLog->mSN, aXLog->mTID );
    IDE_SET( ideSetErrorCode( rpERR_ABORT_RP_INTERNAL_ARG, sErrorBuffer ) );

    return IDE_FAILURE;
}

IDE_RC rpsSmExecutor::writeLOBPiece(rpdXLog     *aXLog,
                                    rpdTransTbl *aTransTbl,
                                    idBool      *aIsLOBOperationException)
{
    smLobLocator sLocalLobLocator = SMI_NULL_LOCATOR;
    idBool       sIsFound;
    SChar        sErrorBuffer[256];

    RP_CREATE_FLAG_VARIABLE(IDV_OPTM_INDEX_RP_R_WRITE_LOB_PIECE);

    RP_OPTIMIZE_TIME_BEGIN(mOpStatistics, IDV_OPTM_INDEX_RP_R_WRITE_LOB_PIECE);

    *aIsLOBOperationException = ID_FALSE;

    IDE_TEST_RAISE( aTransTbl->searchLocator(aXLog->mTID,
                                             aXLog->mLobPtr->mLobLocator,
                                             &sLocalLobLocator,
                                             &sIsFound )
                    != IDE_SUCCESS, ERR_SEARCH_LOBLOCATOR );

    /* PROJ-1442 Replication Online �� DDL ���
     * �ش� LOB Column�� Replication ����� �ƴϸ�,
     * LOB Cursor�� Open���� �����Ƿ� Locator�� ã�� �� ����.
     */
    if(sIsFound == ID_TRUE)
    {
        IDE_TEST_RAISE(smiLob::write(NULL, /* idvSQL* */
                                     sLocalLobLocator,
                                     aXLog->mLobPtr->mLobPieceLen,
                                     aXLog->mLobPtr->mLobPiece)
                       != IDE_SUCCESS, ERR_LOB_WRITE);
    }

    RP_OPTIMIZE_TIME_END(mOpStatistics, IDV_OPTM_INDEX_RP_R_WRITE_LOB_PIECE);

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_LOB_WRITE);
    {
        /* BUG-32423 If the receiver get not enough free space,
         *           then the receiver stop. (LOB)
         * BUG-31851 The transaction needs rollback in eager mode.
         */
        if ( ( ideGetErrorCode() == smERR_ABORT_NOT_ENOUGH_SPACE ) &&
             ( mIsConflictWhenNotEnoughSpace == ID_FALSE ) )
        {
            /* Nothing to do */
        }
        else
        {
            *aIsLOBOperationException = ID_TRUE;
        }

        IDE_ERRLOG(IDE_RP_0);
        IDE_SET(ideSetErrorCode(rpERR_ABORT_LOB_PARTIAL_WRITE));
    }
    IDE_EXCEPTION( ERR_SEARCH_LOBLOCATOR );
    {
        IDE_ERRLOG( IDE_RP_0 );
        IDE_SET( ideSetErrorCode( rpERR_ABORT_RP_INTERNAL_ARG,
                                  "[rpsSmExecutor::writeLOBPiece] failed searchLocator" ) );
    }
    IDE_EXCEPTION_END;

    RP_OPTIMIZE_TIME_END(mOpStatistics, IDV_OPTM_INDEX_RP_R_WRITE_LOB_PIECE);

    IDE_ERRLOG( IDE_RP_0 );
    idlOS::snprintf( sErrorBuffer, 256,
                     "[rpsSmExecutor::writeLOBPiece] "
                     "[CID: %"ID_UINT32_FMT"] "
                     "at [Remote SN: %"ID_UINT64_FMT", TID: %"ID_UINT32_FMT"]",
                     aXLog->mLobPtr->mLobColumnID, aXLog->mSN, aXLog->mTID );
    IDE_SET( ideSetErrorCode( rpERR_ABORT_RP_INTERNAL_ARG, sErrorBuffer ) );

    return IDE_FAILURE;
}

/*
 * PROJ-1705
 *
 * mtdValue���� value�� ��ȯ�Ѵ�.
 * insert�� after image�� update�� after image�� ��ȯ��Ų��.
 */
IDE_RC rpsSmExecutor::convertToNonMtdValue(rpdXLog    * aXLog,
                                           smiValue   * aACols,
                                           const void * aTable)
{
    UInt              i;
    UInt              sCID;
    const mtcColumn * sColumn;
    SChar             sErrorBuffer[256];
    UInt              sStoringSize = 0;
    void            * sStoringValue;

    for(i = 0; i < aXLog->mColCnt; i++)
    {
        sCID = aXLog->mCIDs[i];
        IDE_TEST_RAISE(sCID >= QCI_MAX_COLUMN_COUNT, ERR_INVALID_COLUMN_ID);

        sColumn = (mtcColumn*)rpdCatalog::rpdGetTableColumns(aTable, sCID);
        IDE_TEST_RAISE(sColumn == NULL, ERR_NOT_FOUND_COLUMN);
        IDE_TEST_RAISE((((smiColumn *)sColumn)->id & SMI_COLUMN_ID_MASK) != sCID,
                       ERR_CID_MISMATCH);

        if( (sColumn->column.flag & SMI_COLUMN_TYPE_MASK) == SMI_COLUMN_TYPE_LOB )
        {
            aACols[i].value  = aXLog->mACols[i].value;
            aACols[i].length = aXLog->mACols[i].length;

            if( aACols[i].length > 0 )
            {
                aACols[i].value = (SChar*)(aACols[i].value) + RP_LOB_MTD_HEADER_SIZE;
                aACols[i].length -= RP_LOB_MTD_HEADER_SIZE;
            }
            else
            {
                IDE_ASSERT( aACols[i].value  == NULL );
                IDE_ASSERT( aACols[i].length == 0 );
            }
        }
        else
        {
            if( ((sColumn->module->flag & MTD_DATA_STORE_MTDVALUE_MASK) == MTD_DATA_STORE_MTDVALUE_TRUE) ||
                (aXLog->mACols[i].value == NULL) ) // peer server�� memory table�̸�, null�� �´�.
            {
                aACols[i].value  = aXLog->mACols[i].value;
                aACols[i].length = aXLog->mACols[i].length;
            }
            else
            {
                IDE_TEST( qciMisc::storingSize( (mtcColumn*)sColumn,
                                                (mtcColumn*)sColumn,
                                                (void*)aXLog->mACols[i].value,
                                                &sStoringSize )
                          != IDE_SUCCESS );
                aACols[i].length = sStoringSize;
            
                IDE_TEST( qciMisc::mtdValue2StoringValue( (mtcColumn*)sColumn,
                                                          (mtcColumn*)sColumn,
                                                          (void*)aXLog->mACols[i].value,
                                                          &sStoringValue )
                          != IDE_SUCCESS );
                aACols[i].value = sStoringValue;
            }
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_NOT_FOUND_COLUMN);
    {
        idlOS::snprintf(sErrorBuffer, 256,
                        "[rpsSmExecutor::convertToNonMtdValue] Column not found [CID: %"ID_UINT32_FMT"] "
                        "at [Remote SN: %"ID_UINT64_FMT", TID: %"ID_UINT32_FMT", TABLEID: %"ID_vULONG_FMT"]",
                        sCID, aXLog->mSN, aXLog->mTID, (vULong)smiGetTableId(aTable));

        IDE_SET(ideSetErrorCode(rpERR_ABORT_RP_INTERNAL_ARG, sErrorBuffer));
    }
    IDE_EXCEPTION(ERR_INVALID_COLUMN_ID);
    {
        idlOS::snprintf(sErrorBuffer, 256,
                        "[rpsSmExecutor::convertToNonMtdValue] Too Large Column ID [%"ID_UINT32_FMT"] "
                        "at [Remote SN: %"ID_UINT64_FMT", TID: %"ID_UINT32_FMT"]",
                        sCID, aXLog->mSN, aXLog->mTID);

        IDE_SET(ideSetErrorCode(rpERR_ABORT_RP_INTERNAL_ARG, sErrorBuffer));
    }
    IDE_EXCEPTION(ERR_CID_MISMATCH);
    {
        idlOS::snprintf(sErrorBuffer, 256,
                        "[rpsSmExecutor::convertToNonMtdValue] Mismatch between Column IDs "
                        "[XLog: %"ID_UINT32_FMT", Catalog: %"ID_UINT32_FMT"] "
                        "at [Remote SN: %"ID_UINT64_FMT", TID: %"ID_UINT32_FMT"]",
                        sCID, ((smiColumn *)sColumn)->id & SMI_COLUMN_ID_MASK, aXLog->mSN, aXLog->mTID);

        IDE_SET(ideSetErrorCode(rpERR_ABORT_RP_INTERNAL_ARG, sErrorBuffer));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
 * BUG-30119
 * �޸� ���̺��� fixed�÷��� variable�÷��� null�÷� �α��� ���°� �ٸ���.
 * ���� �÷��� ��� Active���������� variable ������ Ÿ���̰�, Standby ������ fixed������ Ÿ���� ���
 * �� �÷��� value�� null�̸� XLog���� length=0, value=null�� �ȴ�.
 * ������ standby���� fixed�̹Ƿ� length�� 0�� �Ǿ�� �ȵǹǷ�, �̿� �´� length�� nullValue�� �־��ش�.
 */
IDE_RC rpsSmExecutor::convertXlogToSmiValue( rpdXLog        * aXLog,
                                             smiValue       * aACols,
                                             const void     * aTable )
{
    UInt              i;
    UInt              sCID;
    const mtcColumn * sColumn;
    SChar             sErrorBuffer[RP_MAX_MSG_LEN];

    IDE_TEST_CONT((SMI_MISC_TABLE_HEADER(aTable)->mFlag & SMI_TABLE_TYPE_MASK)
                   != SMI_TABLE_MEMORY, NORMAL_EXIT);

    for ( i = 0; i < aXLog->mColCnt; i++ )
    {
        sCID = aXLog->mCIDs[i];
        IDE_TEST_RAISE(sCID >= QCI_MAX_COLUMN_COUNT, ERR_INVALID_COLUMN_ID);

        sColumn = (mtcColumn*)rpdCatalog::rpdGetTableColumns(aTable, sCID);
        IDE_TEST_RAISE(sColumn == NULL, ERR_NOT_FOUND_COLUMN);
        
        aACols[i].value  = aXLog->mACols[i].value;
        aACols[i].length = aXLog->mACols[i].length;

        if( (sColumn->column.flag & SMI_COLUMN_TYPE_MASK) == SMI_COLUMN_TYPE_LOB )
        {
            if( aACols[i].length > 0 )
            {
                aACols[i].value = (SChar*)(aACols[i].value) + RP_LOB_MTD_HEADER_SIZE;
                aACols[i].length -= RP_LOB_MTD_HEADER_SIZE;
            }
            else
            {
                IDE_ASSERT( aACols[i].value  == NULL );
                IDE_ASSERT( aACols[i].length == 0 );
            }
        }
        else
        {
            /* Nothing to do */
        }

        if ( ( (sColumn->column.flag & SMI_COLUMN_TYPE_MASK) == SMI_COLUMN_TYPE_FIXED) &&
             ( aXLog->mACols[i].length == 0 ) )
        {
            IDE_ASSERT(aXLog->mACols[i].value == NULL);

            aACols[i].value = sColumn->module->staticNull;
            aACols[i].length = sColumn->module->nullValueSize();
        }
    }

    RP_LABEL(NORMAL_EXIT);

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_NOT_FOUND_COLUMN);
    {
        idlOS::snprintf(sErrorBuffer, RP_MAX_MSG_LEN,
                        "[rpsSmExecutor::convertXlogToSmiValue] Column not found [CID: %"ID_UINT32_FMT"] "
                        "at [Remote SN: %"ID_UINT64_FMT", TID: %"ID_UINT32_FMT", TABLEID: %"ID_vULONG_FMT"]",
                        sCID, aXLog->mSN, aXLog->mTID, (vULong)smiGetTableId(aTable));
        IDE_SET(ideSetErrorCode(rpERR_ABORT_RP_INTERNAL_ARG, sErrorBuffer));
    }

    IDE_EXCEPTION(ERR_INVALID_COLUMN_ID);
    {
        idlOS::snprintf(sErrorBuffer, RP_MAX_MSG_LEN,
                        "[rpsSmExecutor::convertXlogToSmiValue] Too Large Column ID [%"ID_UINT32_FMT"] "
                        "at [Remote SN: %"ID_UINT64_FMT", TID: %"ID_UINT32_FMT"]",
                        sCID, aXLog->mSN, aXLog->mTID);

        IDE_SET(ideSetErrorCode(rpERR_ABORT_RP_INTERNAL_ARG, sErrorBuffer));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC rpsSmExecutor::convertValueToOID( rpdXLog        * aXLog,
                                         smiValue       * aACols,
                                         smiValue       * aAConvertedCols,
                                         const void     * aTable,
                                         smiStatement   * aStmt,
                                         smOID          * aCompressColOIDs )
{
    UInt              i;
    UInt              sCID;
    const mtcColumn * sColumn;
    SChar             sErrorBuffer[RP_MAX_MSG_LEN];
    const smiColumn * sSmiColumn; 
    void            * sTableHandle;
    void            * sIndexHeader;
    UInt              sDicIDX = 0;

    for ( i = 0; i < aXLog->mColCnt; i++ )
    {
        sCID = aXLog->mCIDs[i];
        IDE_TEST_RAISE(sCID >= QCI_MAX_COLUMN_COUNT, ERR_INVALID_COLUMN_ID);

        sColumn = (mtcColumn*)rpdCatalog::rpdGetTableColumns(aTable, sCID);
        IDE_TEST_RAISE(sColumn == NULL, ERR_NOT_FOUND_COLUMN);

        if ( (sColumn->column.flag & SMI_COLUMN_COMPRESSION_MASK)
             == SMI_COLUMN_COMPRESSION_TRUE )
        {
            sSmiColumn = rpdCatalog::rpdGetTableColumns(aTable, sCID);
            sTableHandle = (void *)smiGetTable( sSmiColumn->mDictionaryTableOID );
            sIndexHeader = (void *)smiGetTableIndexes( sTableHandle, 0 );

            IDE_TEST( qciMisc::makeDictValueForCompress( aStmt,
                                                         sTableHandle,
                                                         sIndexHeader,
                                                         &(aACols[i]),
                                                         &(aCompressColOIDs[sDicIDX]))
                      != IDE_SUCCESS );

            aAConvertedCols[i].value  = &(aCompressColOIDs[sDicIDX]);
            aAConvertedCols[i].length = ID_SIZEOF(smOID);

            sDicIDX++;
        }
        else
        {
            aAConvertedCols[i].value  = aACols[i].value;
            aAConvertedCols[i].length = aACols[i].length;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_NOT_FOUND_COLUMN);
    {
        idlOS::snprintf(sErrorBuffer, RP_MAX_MSG_LEN,
                        "[rpsSmExecutor::convertValueToOID] Column not found [CID: %"ID_UINT32_FMT"] "
                        "at [Remote SN: %"ID_UINT64_FMT", TID: %"ID_UINT32_FMT", TABLEID: %"ID_vULONG_FMT"]",
                        sCID, aXLog->mSN, aXLog->mTID, (vULong)smiGetTableId(aTable));
        IDE_SET(ideSetErrorCode(rpERR_ABORT_RP_INTERNAL_ARG, sErrorBuffer));
    }
    IDE_EXCEPTION(ERR_INVALID_COLUMN_ID);
    {
        idlOS::snprintf(sErrorBuffer, RP_MAX_MSG_LEN,
                        "[rpsSmExecutor::convertValueToOID] Too Large Column ID [%"ID_UINT32_FMT"] "
                        "at [Remote SN: %"ID_UINT64_FMT", TID: %"ID_UINT32_FMT"]",
                        sCID, aXLog->mSN, aXLog->mTID);

        IDE_SET(ideSetErrorCode(rpERR_ABORT_RP_INTERNAL_ARG, sErrorBuffer));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC rpsSmExecutor::cursorOpen( smiTableCursor      * aCursor,
                                  smiStatement        * aStatement,
                                  const void          * aTable,
                                  const void          * aIndex,
                                  smSCN                 aSCN,
                                  const smiColumnList * aColumns,
                                  const smiRange      * aKeyRange,
                                  const smiRange      * aKeyFilter,
                                  const smiCallBack   * aRowFilter,
                                  UInt                  aFlag,
                                  smiCursorType         aCursorType,
                                  smiCursorProperties * aProperties)
{
    RP_CREATE_FLAG_VARIABLE(IDV_OPTM_INDEX_RP_R_TABLE_CURSOR_OPEN);

    IDE_ASSERT(aCursor != NULL);

    RP_OPTIMIZE_TIME_BEGIN(mOpStatistics, IDV_OPTM_INDEX_RP_R_TABLE_CURSOR_OPEN);

    IDE_TEST(aCursor->open(aStatement,
                           aTable,
                           aIndex,      // index
                           aSCN,
                           aColumns,    // columns
                           aKeyRange,   // range
                           aKeyFilter,  // callback
                           aRowFilter,
                           aFlag,
                           aCursorType,
                           aProperties)
             != IDE_SUCCESS);

    RP_OPTIMIZE_TIME_END(mOpStatistics, IDV_OPTM_INDEX_RP_R_TABLE_CURSOR_OPEN);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    RP_OPTIMIZE_TIME_END(mOpStatistics, IDV_OPTM_INDEX_RP_R_TABLE_CURSOR_OPEN);

    return IDE_FAILURE;
}

IDE_RC rpsSmExecutor::cursorClose(smiTableCursor * aCursor)
{
    RP_CREATE_FLAG_VARIABLE(IDV_OPTM_INDEX_RP_R_TABLE_CURSOR_CLOSE);

    IDE_ASSERT(aCursor != NULL);

    RP_OPTIMIZE_TIME_BEGIN(mOpStatistics, IDV_OPTM_INDEX_RP_R_TABLE_CURSOR_CLOSE);

    IDE_TEST(aCursor->close() != IDE_SUCCESS);

    RP_OPTIMIZE_TIME_END(mOpStatistics, IDV_OPTM_INDEX_RP_R_TABLE_CURSOR_CLOSE);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    RP_OPTIMIZE_TIME_END(mOpStatistics, IDV_OPTM_INDEX_RP_R_TABLE_CURSOR_CLOSE);

    return IDE_FAILURE;
}
