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
 * Copyright 1999-2000, RTBase Corporation or its subsidiaries.
 * All rights reserved.
 **********************************************************************/

/***********************************************************************
 * $Id: qmcInsertCursor.cpp 18910 2006-11-13 01:56:34Z mhjeong $
 **********************************************************************/

#include <idl.h>
#include <ide.h>
#include <qc.h>
#include <qmsParseTree.h>
#include <qmoPartition.h>
#include <qmcInsertCursor.h>
#include <qmx.h>
#include <qdbCommon.h>
#include <qcuTemporaryObj.h>

IDE_RC qmcInsertCursor::initialize( iduMemory   * aMemory,
                                    qmsTableRef * aTableRef,
                                    idBool        aAllocPartCursors,
                                    idBool        aInitPartCursors )
{
/***********************************************************************
 *
 *  Description : PROJ-1502 PARTITIONED DISK TABLE
 *                insert cursor manager �� �ʱ�ȭ
 *
 *  Implementation :
 *         1) partitioned / non-partitioned�� ����.
 *         2) partitioned��� ���� partition filtering�� ����Ͽ�
 *            partition reference�� ����.
 *
 ***********************************************************************/

    qmsPartitionRef     * sCurrRef;
    UInt                  sPartIndex;

    IDE_ASSERT( aTableRef != NULL );

    mCursorIndex      = NULL;
    mCursorIndexCount = 0;
    mCursors          = NULL;
    
    mSelectedCursor   = NULL;
    mTableRef         = aTableRef;
    mCursorSmiStmt    = NULL;    
    mCursorFlag       = 0;
    
    SMI_CURSOR_PROP_INIT_FOR_FULL_SCAN( &mCursorProperties, NULL );

    if( aTableRef->tableInfo->partitionMethod ==
        QCM_PARTITION_METHOD_NONE )
    {
        mIsPartitioned = ID_FALSE;
        
        // alloc���� �ʰ� internal cursor�� ����Ѵ�.
        mCursorIndex = & mInternalCursorIndex;
        mCursors     = & mInternalCursor;
        
        IDE_TEST( addInsertPartCursor( mCursors, NULL )
                  != IDE_SUCCESS );
        
        mSelectedCursor = mCursors;
    }
    else
    {
        mIsPartitioned = ID_TRUE;

        IDU_FIT_POINT( "qmcInsertCursor::initialize::alloc::mCursorIndex",
                        idERR_ABORT_InsufficientMemory );

        /* BUG-47840 Partition Insert Execution memory reduce */
        if ( aAllocPartCursors == ID_TRUE )
        {
            IDE_TEST( aMemory->alloc(
                      ID_SIZEOF(qmcInsertPartCursor*) * aTableRef->partitionCount,
                      (void**)& mCursorIndex )
                      != IDE_SUCCESS );

            IDU_FIT_POINT( "qmcInsertCursor::initialize::alloc::mCursors",
                            idERR_ABORT_InsufficientMemory );

            IDE_TEST( aMemory->alloc(
                      ID_SIZEOF(qmcInsertPartCursor) * aTableRef->partitionCount,
                      (void**)& mCursors )
                      != IDE_SUCCESS );

            if ( aInitPartCursors == ID_TRUE )
            {
                for( sCurrRef = aTableRef->partitionRef, sPartIndex = 0;
                     sCurrRef != NULL;
                     sCurrRef = sCurrRef->next, sPartIndex++ )
                {                    
                    IDE_TEST( addInsertPartCursor( & mCursors[sPartIndex],
                                                   sCurrRef )
                              != IDE_SUCCESS );
                }
            }
            else
            {
                // Nothing to do.
            }
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

IDE_RC qmcInsertCursor::openCursor( qcStatement         * aStatement,
                                    UInt                  aFlag,
                                    smiCursorProperties * aProperties )
{
/***********************************************************************
 *
 *  Description : cursor�� open�Ѵ�.
 *
 *  Implementation :
 *
 ***********************************************************************/

    const void          * sTableHandle        = NULL;
    smSCN                 sTableSCN;
    UInt                  sOpenedCount        = 0;
    UInt                  i;
    smSCN                 sBaseTableSCN;
    UInt                  sFlag               = 0;
    UInt                  sHintParallelDegree = 0;

    if( mIsPartitioned == ID_FALSE )
    {
        // PROJ-1407 Temporary Table
        if ( qcuTemporaryObj::isTemporaryTable( mTableRef->tableInfo ) == ID_TRUE )
        {
            qcuTemporaryObj::getTempTableHandle( aStatement,
                                                 mTableRef->tableInfo,
                                                 &sTableHandle,
                                                 &sBaseTableSCN );
            
            if ( sTableHandle == NULL )
            {
                IDE_TEST( qcuTemporaryObj::createAndGetTempTable(
                              aStatement,
                              mTableRef->tableInfo,
                              &sTableHandle ) != IDE_SUCCESS );
            }
            else
            {
                IDE_TEST_RAISE( !SM_SCN_IS_EQ( &(mTableRef->tableSCN), &sBaseTableSCN ),
                                ERR_TEMPORARY_TABLE_EXIST );
            }

            sTableSCN = smiGetRowSCN( sTableHandle );
            
            mSelectedCursor->cursor.initialize();

            IDE_TEST( mSelectedCursor->cursor.open(
                          QC_SMI_STMT(aStatement),
                          sTableHandle,
                          NULL,
                          sTableSCN,
                          NULL,
                          smiGetDefaultKeyRange(),
                          smiGetDefaultKeyRange(),
                          smiGetDefaultFilter(),
                          aFlag,
                          SMI_INSERT_CURSOR,
                          aProperties )
                      != IDE_SUCCESS );
        }
        else
        {
            // normal table
            sTableHandle = mTableRef->tableHandle;
            sTableSCN    = mTableRef->tableSCN;
            
            mSelectedCursor->cursor.initialize();
            
            IDE_TEST( mSelectedCursor->cursor.open(
                          QC_SMI_STMT(aStatement),
                          sTableHandle,
                          NULL,
                          sTableSCN,
                          NULL,
                          smiGetDefaultKeyRange(),
                          smiGetDefaultKeyRange(),
                          smiGetDefaultFilter(),
                          aFlag,
                          SMI_INSERT_CURSOR,
                          aProperties )
                      != IDE_SUCCESS );
        }
    }
    else
    {
        for ( i = 0; i < mCursorIndexCount; i++ )
        {
            mCursorIndex[i]->cursor.initialize();
        }

        /* PROJ-2464 hybrid partitioned table ���� */
        sHintParallelDegree = aProperties->mHintParallelDegree;

        for ( i = 0; i < mCursorIndexCount; i++ )
        {
            /* PROJ-2464 hybrid partitioned table ���� */
            if ( QCM_TABLE_TYPE_IS_DISK( mCursorIndex[i]->partitionRef->partitionInfo->tableFlag ) == ID_TRUE )
            {
                sFlag = aFlag;
                aProperties->mHintParallelDegree = sHintParallelDegree;
            }
            else
            {
                sFlag = ( aFlag & ~SMI_INSERT_METHOD_MASK ) | SMI_INSERT_METHOD_NORMAL;
                aProperties->mHintParallelDegree = 0;
            }

            IDE_TEST( mCursorIndex[i]->cursor.open(
                          QC_SMI_STMT(aStatement),
                          mCursorIndex[i]->partitionRef->partitionHandle,
                          NULL,
                          mCursorIndex[i]->partitionRef->partitionSCN,
                          NULL,
                          smiGetDefaultKeyRange(),
                          smiGetDefaultKeyRange(),
                          smiGetDefaultFilter(),
                          sFlag,
                          SMI_INSERT_CURSOR,
                          aProperties )
                      != IDE_SUCCESS );
            
            sOpenedCount++;
        }

        /* PROJ-2464 hybrid partitioned table ���� */
        aProperties->mHintParallelDegree = sHintParallelDegree;

        mCursorSmiStmt    = QC_SMI_STMT(aStatement);
        mCursorFlag       = aFlag;
        mCursorProperties = *aProperties;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_TEMPORARY_TABLE_EXIST )
    {
        IDE_SET(ideSetErrorCode( qpERR_ABORT_QMN_INVALID_TEMPORARY_TABLE ));
    }
    IDE_EXCEPTION_END;

    // BUG-27693 ��Ƽ�����̺� ���� insert cursor���½�, �������� ��쿡 ���� ����ó���� ����.
    if ( mIsPartitioned == ID_FALSE )
    {
        // Nothing to do.
    }
    else
    {
        for ( i = 0; i < sOpenedCount; i++ )
        {
            mCursorIndex[i]->cursor.close();
        }
    }

    return IDE_FAILURE;
}

IDE_RC qmcInsertCursor::partitionFilteringWithRow( smiValue      * aValues,
                                                   qmxLobInfo    * aLobInfo,
                                                   qcmTableInfo ** aSelectedTableInfo )
{
/***********************************************************************
 *
 *  Description : partitioned table�� ���Ͽ� partition filtering�� �Ѵ�.
 *
 *  Implementation : partition filtering���� lob������ filtering���� ����
 *                   partition�� �÷����� ����ģ��.
 *
 *
 ***********************************************************************/

    qmsPartitionRef     * sCurrRef;
    qmsPartitionRef     * sSelectedPartitionRef;
    idBool                sFound = ID_FALSE;
    UInt                  sPartIndex;
    UInt                  i;

    if( mIsPartitioned == ID_FALSE )
    {
        // �Ϲ� ���̺�.
        // Nothing to do.
    }
    else
    {
        IDE_TEST( qmoPartition::partitionFilteringWithRow(
                      mTableRef,
                      aValues,
                      &sSelectedPartitionRef )
                  != IDE_SUCCESS );

        for ( i = 0; i < mCursorIndexCount; i++ )
        {
            if ( mCursorIndex[i]->partitionRef == sSelectedPartitionRef )
            {
                mSelectedCursor = mCursorIndex[i];
                
                sFound = ID_TRUE;
                break;
            }
            else
            {
                // Nothing to do.
            }
        }

        if ( sFound == ID_FALSE )
        {
            // BUGBUG partitionRef�� partIndex�� �ִٸ� �˻��� �ʿ䰡 ����.
            for( sCurrRef = mTableRef->partitionRef, sPartIndex = 0;
                 sCurrRef != NULL;
                 sCurrRef = sCurrRef->next, sPartIndex++ )
            {
                if ( sCurrRef == sSelectedPartitionRef )
                {
                    sFound = ID_TRUE;
                    break;
                }
                else
                {
                    // Nothing to do.
                }
            }

            IDE_TEST_RAISE( sFound == ID_FALSE, ERR_NOT_FOUND );
            
            // BUG-34085 partition lock pruning
            IDE_TEST( qmx::lockPartitionForDML( mCursorSmiStmt,
                                                sCurrRef,
                                                SMI_TABLE_LOCK_IX )
                      != IDE_SUCCESS );

            /* BUG-47840 Partition Insert Execution memory reduce */
            if ( mCursors == NULL )
            {
                // alloc���� �ʰ� internal cursor�� ����Ѵ�.
                mCursorIndex = & mInternalCursorIndex;
                mCursors     = & mInternalCursor;
                IDE_TEST( openInsertPartCursor( &mCursors[0],
                                                sCurrRef )
                          != IDE_SUCCESS );

                IDE_TEST( addInsertPartCursor( &mCursors[0],
                                               sCurrRef )
                          != IDE_SUCCESS );

                mSelectedCursor = &mCursors[0];
            }
            else
            {
                IDE_TEST( openInsertPartCursor( &mCursors[sPartIndex],
                                                sCurrRef )
                          != IDE_SUCCESS );

                IDE_TEST( addInsertPartCursor( &mCursors[sPartIndex],
                                               sCurrRef )
                          != IDE_SUCCESS );

                mSelectedCursor = &mCursors[sPartIndex];
            }
        }
        else
        {
            // Nothing to do.
        }
        
        if( aSelectedTableInfo != NULL )
        {
            *aSelectedTableInfo = sSelectedPartitionRef->partitionInfo;
        }

        IDE_TEST( qmx::changeLobColumnInfo(
                      aLobInfo,
                      sSelectedPartitionRef->partitionInfo->columns )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NOT_FOUND )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmcInsertCursor::partitionFilteringWithRow",
                                  "partition not found" ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmcInsertCursor::getCursor( smiTableCursor ** aCursor )
{
/***********************************************************************
 *
 *  Description : ���� ���õ� Ŀ���� ��ȯ
 *
 *  Implementation :
 *
 ***********************************************************************/

    IDE_DASSERT( mSelectedCursor != NULL );

    *aCursor = &mSelectedCursor->cursor;
    
    return IDE_SUCCESS;
}

IDE_RC qmcInsertCursor::getSelectedPartitionOID( smOID * aPartOID )
{
/***********************************************************************
 *
 *  Description : ���� ���õ� partition�� oid�� ��ȯ
 *
 *  Implementation :
 *
 ***********************************************************************/

    IDE_DASSERT( mSelectedCursor != NULL );

    *aPartOID = mSelectedCursor->partitionRef->partitionInfo->tableOID;
    
    return IDE_SUCCESS;
}

/***********************************************************************
 *
 *  Description : ���� ���õ� Partition�� Tuple ID�� ��ȯ
 *
 *  Implementation :
 *
 ***********************************************************************/
IDE_RC qmcInsertCursor::getSelectedPartitionTupleID( UShort * aPartTupleID )
{
    IDE_DASSERT( mSelectedCursor != NULL );

    *aPartTupleID = mSelectedCursor->partitionRef->table;

    return IDE_SUCCESS;
}

IDE_RC qmcInsertCursor::closeCursor( )
{
/***********************************************************************
 *
 *  Description : ��� Ŀ���� ����.
 *
 *  Implementation :
 *
 ***********************************************************************/

    UInt  i = 0;

    for ( ; i < mCursorIndexCount; i++ )
    {
        IDE_TEST( mCursorIndex[i]->cursor.close() != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    for ( i++; i < mCursorIndexCount; i++ )
    {
        (void) mCursorIndex[i]->cursor.close();
    }

    return IDE_FAILURE;
}

IDE_RC qmcInsertCursor::setColumnsForNewRow( )
{
/***********************************************************************
 *
 *  Description : trigger�� newrow�� �ؼ��� �� �ִ� column ������ �����Ѵ�.
 *                ���� �ѹ��� �����ϰ� ���ķδ� �������� ����.
 *
 *  Implementation :
 *
 ***********************************************************************/

    qcmTableInfo      * sTableInfo;

    IDE_DASSERT( mSelectedCursor != NULL );

    if( mSelectedCursor->isSetColumnsForNewRow == ID_FALSE )
    {
        if( mIsPartitioned == ID_TRUE )
        {
            sTableInfo = mSelectedCursor->partitionRef->partitionInfo;
        }
        else
        {
            sTableInfo = mTableRef->tableInfo;
        }

        mSelectedCursor->columnsForNewRow = sTableInfo->columns;

        mSelectedCursor->isSetColumnsForNewRow = ID_TRUE;
    }
    else
    {
        // Nothing to do.
    }

    return IDE_SUCCESS;
}

IDE_RC qmcInsertCursor::getColumnsForNewRow( qcmColumn ** aColumnsForNewRow )
{
/***********************************************************************
 *
 *  Description : trigger new row�� �ؼ��ϱ� ���� �÷������� �����´�.
 *
 *  Implementation :
 *
 ***********************************************************************/

    IDE_DASSERT( mSelectedCursor != NULL );
    IDE_DASSERT( mSelectedCursor->isSetColumnsForNewRow == ID_TRUE );

    *aColumnsForNewRow = mSelectedCursor->columnsForNewRow;
    
    return IDE_SUCCESS;
}

IDE_RC qmcInsertCursor::clearColumnsForNewRow()
{
/***********************************************************************
 *
 *  Description : trigger new row�� �ؼ��ϱ� ���� �÷������� �����´�.
 *
 *  Implementation :
 *
 ***********************************************************************/
    
    IDE_DASSERT( mSelectedCursor != NULL );
    IDE_DASSERT( mSelectedCursor->isSetColumnsForNewRow == ID_TRUE );

    mSelectedCursor->isSetColumnsForNewRow = ID_FALSE;

    mSelectedCursor->columnsForNewRow = NULL;
    
    return IDE_SUCCESS;
}

IDE_RC qmcInsertCursor::openInsertPartCursor( qmcInsertPartCursor * aPartitionCursor,
                                              qmsPartitionRef     * aPartitionRef )
{
    UInt sFlag               = 0;
    UInt sHintParallelDegree = 0;

    IDE_DASSERT( aPartitionCursor != NULL );

    aPartitionCursor->cursor.initialize();

    /* PROJ-2464 hybrid partitioned table ���� */
    sHintParallelDegree = mCursorProperties.mHintParallelDegree;

    /* PROJ-2464 hybrid partitioned table ���� */
    if ( QCM_TABLE_TYPE_IS_DISK( aPartitionRef->partitionInfo->tableFlag ) == ID_TRUE )
    {
        sFlag = mCursorFlag;
        // mCursorProperties.mHintParallelDegree = sHintParallelDegree;
    }
    else
    {
        sFlag = ( mCursorFlag & ~SMI_INSERT_METHOD_MASK ) | SMI_INSERT_METHOD_NORMAL;
        mCursorProperties.mHintParallelDegree = 0;
    }

    IDE_TEST( aPartitionCursor->cursor.open(
                  mCursorSmiStmt,
                  aPartitionRef->partitionHandle,
                  NULL,
                  aPartitionRef->partitionSCN,
                  NULL,
                  smiGetDefaultKeyRange(),
                  smiGetDefaultKeyRange(),
                  smiGetDefaultFilter(),
                  sFlag,
                  SMI_INSERT_CURSOR,
                  & mCursorProperties )
              != IDE_SUCCESS );
    
    /* PROJ-2464 hybrid partitioned table ���� */
    mCursorProperties.mHintParallelDegree = sHintParallelDegree;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmcInsertCursor::addInsertPartCursor( qmcInsertPartCursor * aPartitionCursor,
                                             qmsPartitionRef     * aPartitionRef )
{
    IDE_DASSERT( aPartitionCursor != NULL );
    
    aPartitionCursor->columnsForNewRow = NULL;
    aPartitionCursor->isSetColumnsForNewRow = ID_FALSE;
    aPartitionCursor->partitionRef = aPartitionRef;

    mCursorIndex[mCursorIndexCount] = aPartitionCursor;
    mCursorIndexCount++;

    return IDE_SUCCESS;
}
