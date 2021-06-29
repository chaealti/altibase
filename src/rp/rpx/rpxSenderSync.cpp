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
 * $Id: rpxSenderSync.cpp 88081 2020-07-16 02:09:16Z yoonhee.kim $
 **********************************************************************/

#include <idl.h>
#include <ide.h>

#include <smi.h>
#include <smErrorCode.h>
#include <rpDef.h>
#include <rpcManager.h>
#include <rpxSender.h>
#include <rpxSync.h>
#include <rpdCatalog.h>

//===================================================================
//
// Name:          syncStart
//              
// Return Value:  IDE_SUCCESS/IDE_FAILURE
//              
// Argument:      
//              
// Called By:     rpxSender::startByFlag()
//              
// Description:   
//             
//===================================================================

IDE_RC 
rpxSender::syncStart()
{
    SInt sOld = mMeta.mReplication.mIsStarted;
    
    if( mMeta.mReplication.mItemCount != 0 )
    {
        setStatus(RP_SENDER_SYNC);

        IDE_TEST( sendSyncStart() != IDE_SUCCESS );
        ideLog::log(IDE_RP_0, "Succeeded to send information of tables.\n");

        IDE_TEST_RAISE( syncParallel() != IDE_SUCCESS, ERR_SYNC );
        ideLog::log(IDE_RP_0, "Succeeded to insert data.\n");

        mMeta.mReplication.mIsStarted = sOld;

        /* PROJ-2184 RP sync ���ɰ��� */
        IDE_TEST_RAISE( sendSyncEnd() != IDE_SUCCESS, ERR_SYNC );
        ideLog::log(IDE_RP_0, "Succeeded to rebuild indexes.\n");
    }

    if(mMeta.mReplication.mReplMode == RP_EAGER_MODE)
    {
        mCurrentType = RP_PARALLEL;
    }
    else
    {
        mCurrentType = RP_NORMAL;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_SYNC );
    {
        IDE_ERRLOG( IDE_RP_0 );
        IDE_SET( ideSetErrorCode( rpERR_ABORT_RP_SYNC_WITH_INDEX_CAUTION ) );
    }
    IDE_EXCEPTION_END;

    IDE_ERRLOG(IDE_RP_0);
    IDE_SET(ideSetErrorCode(rpERR_ABORT_RP_SENDER_SYNCSTART));
    IDE_ERRLOG(IDE_RP_0);

    mMeta.mReplication.mIsStarted = sOld;

    return IDE_FAILURE;
}

IDE_RC
rpxSender::syncALAStart()
{
    smiStatement * sParallelStmts     = NULL;
    smiTrans     * sSyncParallelTrans = NULL;
    SChar        * sUsername          = NULL;
    SChar        * sTablename         = NULL;
    SChar        * sPartitionname     = NULL;
    SInt           i                  = 0;
    idBool         sIsAllocSCN        = ID_FALSE;
    SInt           sOld;
    ULong          sSyncedTuples      = 0;
    
    sOld = mMeta.mReplication.mIsStarted;
    
    if( mMeta.mReplication.mItemCount != 0 )
    {
        setStatus( RP_SENDER_SYNC );

        IDE_TEST( allocSCN( &sParallelStmts, &sSyncParallelTrans )
                  != IDE_SUCCESS );
        sIsAllocSCN = ID_TRUE;

        rpdMeta::setReplFlagStartSyncApply( &(mMeta.mReplication) );

        for ( i = 0 ; i < mMeta.mReplication.mItemCount ; i++ )
        {
            sUsername  = mMeta.mItemsOrderByTableOID[i]->mItem.mLocalUsername;
            sTablename = mMeta.mItemsOrderByTableOID[i]->mItem.mLocalTablename;
            sPartitionname = mMeta.mItemsOrderByTableOID[i]->mItem.mLocalPartname;

            // Replication Table�� ������ Sync Table�� ��쿡�� ��� �����Ѵ�.
            if ( isSyncItem( mSyncInsertItems,
                             sUsername,
                             sTablename,
                             sPartitionname )
                 == ID_TRUE )
            {
                IDE_TEST( rpxSync::syncTable( &(sParallelStmts[0]),
                                              &mMessenger,
                                              mMeta.mItemsOrderByTableOID[i],
                                              &mExitFlag,
                                              1, /* mChild Count for Parallel Scan */
                                              1, /* mChild Number for Parallel Scan */
                                              &sSyncedTuples, /* V$REPSYNC.SYNC_RECORD_COUNT */
                                              ID_TRUE /* aIsALA */ )
                          != IDE_SUCCESS );
            }
            else
            {
                /* do nothing */
            }
        }
        rpdMeta::clearReplFlagStartSyncApply( &(mMeta.mReplication) );

        sIsAllocSCN = ID_FALSE;
        destroySCN( sParallelStmts, sSyncParallelTrans );
        sParallelStmts     = NULL;
        sSyncParallelTrans = NULL;

        mMeta.mReplication.mIsStarted = sOld;
    }

    if( mMeta.mReplication.mReplMode == RP_EAGER_MODE )
    {
        mCurrentType = RP_PARALLEL;
    }
    else
    {
        mCurrentType = RP_NORMAL;
    }

    mSvcThrRootStmt = NULL;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    IDE_SET( ideSetErrorCode( rpERR_ABORT_RP_SENDER_SYNCSTART ) );
    
    IDE_ERRLOG( IDE_RP_0 );

    if ( sIsAllocSCN  == ID_TRUE )
    {
        destroySCN( sParallelStmts, sSyncParallelTrans );
        sParallelStmts      = NULL;
        sSyncParallelTrans  = NULL;
    }
    else
    {
        /* do nothing */
    }

    mMeta.mReplication.mIsStarted = sOld;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : �Ķ���ͷ� �־��� User Name�� Table Name��
 *               ����ڰ� ������ Sync Item�� ���� �ִ��� Ȯ���Ѵ�.
 *
 **********************************************************************/
idBool rpxSender::isSyncItem( rpdReplSyncItem *aSyncItems,
                              const SChar  *aUsername,
                              const SChar  *aTablename,
                              const SChar  *aPartname )
{
    idBool        sResult   = ID_FALSE;
    rpdReplSyncItem *sSyncItem = NULL;

    if ( aSyncItems != NULL )
    {

        for ( sSyncItem = aSyncItems;
              sSyncItem != NULL;
              sSyncItem = sSyncItem->next )
        {

            if ( ( idlOS::strncmp( aUsername,
                                   sSyncItem->mUserName,
                                   QC_MAX_OBJECT_NAME_LEN )
                   == 0 ) &&
                 ( idlOS::strncmp( aTablename,
                                   sSyncItem->mTableName,
                                   QC_MAX_OBJECT_NAME_LEN )
                   == 0 ) &&
                 ( idlOS::strncmp( aPartname,
                                   sSyncItem->mPartitionName,
                                   QC_MAX_OBJECT_NAME_LEN )
                   == 0 ) )
            {
                sResult = ID_TRUE;
                break;
            }
            else
            {
                /* To do nothing */
            }
        }
    }
    else
    {
        // Sync Item�� ��������� �������� ���� ���, ��� Sync Item���� �����Ѵ�.
        sResult = ID_TRUE;
    }

    return sResult;
}

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
 *   �� ���̺� ��ü �÷��� ���� ��ġ�÷�����Ʈ ����
 *
 ******************************************************************************/
IDE_RC rpxSender::makeFetchColumnList(const smOID          aTableOID,
                                      smiFetchColumnList * aFetchColumnList)
{
    UInt                 i;
    UInt                 sColCount;
    const void         * sTable;
    const smiColumn    * sSmiColumn;
    smiFetchColumnList * sFetchColumnList;
    smiFetchColumnList * sFetchColumn;
    SChar                sErrorBuffer[256];
    UInt                 sFunctionIdx;

    IDE_ASSERT(aFetchColumnList != NULL);

    sTable           = smiGetTable( aTableOID );
    sColCount        = smiGetTableColumnCount(sTable);
    sFetchColumnList = aFetchColumnList;

    for(i = 0; i < sColCount; i++)
    {
        sSmiColumn = rpdCatalog::rpdGetTableColumns(sTable, i);
        IDE_TEST_RAISE(sSmiColumn == NULL, ERR_COLUMN_NOT_FOUND);

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

        if(i == (sColCount - 1))
        {
            sFetchColumn->next = NULL;
        }
        else
        {
            sFetchColumn->next = sFetchColumn + 1;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_COLUMN_NOT_FOUND);
    {
        idlOS::snprintf(sErrorBuffer, 256,
                        "[rpxSender::makeFetchColumnList] Column not found "
                        "[CID: %"ID_UINT32_FMT", Table OID: %"ID_vULONG_FMT"]",
                        i, (vULong)aTableOID);

        IDE_SET(ideSetErrorCode(rpERR_ABORT_RP_INTERNAL_ARG, sErrorBuffer));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Ư�� Column Array�� Key Range�� �����.
 *               �ش� Table�� IS Lock�� ��Ҵٰ� �����Ѵ�.
 *
 **********************************************************************/
IDE_RC rpxSender::getKeyRange(smOID               aTableOID,
                              smiValue           *aColArray,
                              smiRange           *aKeyRange,
                              qriMetaRangeColumn *aRangeColumn)
{
    UInt           i;
    UInt           sOrder;
    UInt           sFlag;
    UInt           sCompareType;
    qcmTableInfo * sItemInfo;

    IDE_ASSERT(aColArray != NULL);
    IDE_ASSERT(aKeyRange != NULL);
    IDE_ASSERT(aRangeColumn != NULL);

    sItemInfo = (qcmTableInfo *)rpdCatalog::rpdGetTableTempInfo(smiGetTable( aTableOID ));

    /* Proj-1872 DiskIndex ����ȭ
     * Range�� �����ϴ� ����� DiskIndex�� ���, Stored Ÿ���� �����
     * Range�� ���� �Ǿ�� �Ѵ�. */
    sFlag = (sItemInfo->primaryKey->keyColumns)->column.flag;
    if(((sFlag & SMI_COLUMN_STORAGE_MASK) == SMI_COLUMN_STORAGE_DISK) &&
       ((sFlag & SMI_COLUMN_USAGE_MASK)   == SMI_COLUMN_USAGE_INDEX))
    {
        sCompareType = MTD_COMPARE_STOREDVAL_MTDVAL;
    }
    else
    {
        sCompareType = MTD_COMPARE_MTDVAL_MTDVAL;
    }

    qtc::initializeMetaRange(aKeyRange, sCompareType);

    for(i = 0; i < sItemInfo->primaryKey->keyColCount; i++)
    {
        sOrder = sItemInfo->primaryKey->keyColsFlag[i] & SMI_COLUMN_ORDER_MASK;

        qtc::setMetaRangeColumn(&aRangeColumn[i],
                                &sItemInfo->primaryKey->keyColumns[i],
                                aColArray[i].value,
                                sOrder,
                                i);

        qtc::addMetaRangeColumn(aKeyRange,
                                &aRangeColumn[i]);
    }

    qtc::fixMetaRange(aKeyRange);

    return IDE_SUCCESS;
}

IDE_RC rpxSender::sendSyncStart()
{
    IDE_TEST( mMessenger.sendSyncStart() != IDE_SUCCESS );

    IDE_TEST( sendSyncTableInfo() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_SET( ideSetErrorCode( rpERR_ABORT_SEND_SYNC_START ) );

    return IDE_FAILURE;
}

IDE_RC rpxSender::sendSyncEnd()
{
    IDE_TEST_RAISE( mMessenger.sendSyncEnd() != IDE_SUCCESS, ERR_REBUILD_INDEX );

    /* receiver�� drop indices�� ��� ���������� ��ٸ��� */
    while ( ( mSenderInfo->getFlagRebuildIndex() == ID_FALSE ) &&
            ( checkInterrupt() == RP_INTR_NONE ) )
    {
        PDL_Time_Value sPDL_Time_Value;
        sPDL_Time_Value.initialize( 1, 0 );
        idlOS::sleep( sPDL_Time_Value );
    }
    IDE_TEST( ( mExitFlag == ID_TRUE ) || ( mRetryError == ID_TRUE ) );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_REBUILD_INDEX );
    {
    }
    IDE_EXCEPTION_END;

    IDE_SET( ideSetErrorCode( rpERR_ABORT_REBUILD_INDEX ) );

    return IDE_FAILURE;
}

IDE_RC rpxSender::sendSyncTableInfo()
{
    SChar * sUsername  = NULL;
    SChar * sTablename = NULL;
    SChar * sPartname = NULL;
    rpdReplSyncItem * sSyncTables;
    SInt sSyncTableNumber = 0;
    SInt i;

    if ( mSyncInsertItems == NULL ) /* sync table�� �������� ������� */
    {
        sSyncTableNumber = mMeta.mReplication.mItemCount;
    }
    else
    {
        for ( sSyncTables = mSyncInsertItems; sSyncTables != NULL; sSyncTables = sSyncTables->next)
        {
            sSyncTableNumber++;
        }
    }
    IDE_TEST_RAISE( ( sSyncTableNumber <= 0 ) || ( sSyncTableNumber > mMeta.mReplication.mItemCount ),
                    ERR_SYNC_TABLE_NUMBER );

    IDE_TEST( mMessenger.sendSyncTableNumber( sSyncTableNumber ) != IDE_SUCCESS );

    for ( i = 0; i < mMeta.mReplication.mItemCount; i++ )
    {
        sUsername  = mMeta.mItemsOrderByTableOID[i]->mItem.mLocalUsername;
        sTablename = mMeta.mItemsOrderByTableOID[i]->mItem.mLocalTablename;
        sPartname = mMeta.mItemsOrderByTableOID[i]->mItem.mLocalPartname;
        // Replication Table�� ������ Sync Table�� ��쿡�� ��� �����Ѵ�.
        if ( isSyncItem( mSyncInsertItems,
                         sUsername,
                         sTablename,
                         sPartname )
             != ID_TRUE )
        {
            continue;
        }
        else
        {
            /* Nothing to do */
        }

        IDE_TEST( mMessenger.sendSyncTableInfo( mMeta.mItemsOrderByTableOID[i] )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_SYNC_TABLE_NUMBER )
    {
        IDE_SET( ideSetErrorCode( rpERR_ABORT_INVALID_SYNC_TABLE_NUMBER ) );
        IDE_ERRLOG( IDE_RP_0 );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
