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
 * $Id: qcuTemporaryObj.cpp 88963 2020-10-19 03:33:18Z jake.jang $
 **********************************************************************/

#include <idl.h>
#include <ide.h>
#include <smDef.h>
#include <smi.h>
#include <qcg.h>
#include <qdbCommon.h>
#include <qcuTemporaryObj.h>
#include <smiTableSpace.h>

idBool                gIsInitSessionTempObjMgr = ID_FALSE;
qcuSessionTempObjMgr  gSessionTempObjMgr;

extern "C" SInt
compareTempTableID( const void* aElem1, const void* aElem2 )
{
/***********************************************************************
 *
 * Description :
 *    compare temporary table id
 *
 * Implementation :
 *
 ***********************************************************************/

    IDE_ASSERT( (qcTempTable*)aElem1 != NULL );
    IDE_ASSERT( (qcTempTable*)aElem2 != NULL );

    if ( ((qcTempTable*)aElem1)->baseTableID > ((qcTempTable*)aElem2)->baseTableID )
    {
        return 1;
    }
    else if ( ((qcTempTable*)aElem1)->baseTableID < ((qcTempTable*)aElem2)->baseTableID )
    {
        return -1;
    }
    else
    {
        return 0;
    }
}

extern "C" SInt
compareSessionTempTableID( const void* aElem1, const void* aElem2 )
{
/***********************************************************************
 *
 * Description :
 *    compare session temporary table id
 *
 * Implementation :
 *
 ***********************************************************************/

    IDE_ASSERT( (qcuSessionTempObj*)aElem1 != NULL );
    IDE_ASSERT( (qcuSessionTempObj*)aElem2 != NULL );

    if ( ((qcuSessionTempObj*)aElem1)->baseTableID >
         ((qcuSessionTempObj*)aElem2)->baseTableID )
    {
        return 1;
    }
    else if ( ((qcuSessionTempObj*)aElem1)->baseTableID <
              ((qcuSessionTempObj*)aElem2)->baseTableID )
    {
        return -1;
    }
    else
    {
        return 0;
    }
}

extern "C" SInt
compareTempIndexID( const void* aElem1, const void* aElem2 )
{
/***********************************************************************
 *
 * Description :
 *    compare temporary table id
 *
 * Implementation :
 *
 ***********************************************************************/

    IDE_ASSERT( (qcTempIndex*)aElem1 != NULL );
    IDE_ASSERT( (qcTempIndex*)aElem2 != NULL );

    if ( ((qcTempIndex*)aElem1)->baseIndexID > ((qcTempIndex*)aElem2)->baseIndexID )
    {
        return 1;
    }
    else if ( ((qcTempIndex*)aElem1)->baseIndexID < ((qcTempIndex*)aElem2)->baseIndexID )
    {
        return -1;
    }
    else
    {
        return 0;
    }
}

IDE_RC qcuTemporaryObj::initializeStatic( )
{
/***********************************************************************
 *
 * Description :
 *   SessionTempObjMgr�� MUTEX �ʱ�ȭ,
 *
 * Implementation :
 *
 ***********************************************************************/

    if( gIsInitSessionTempObjMgr == ID_FALSE )
    {
        IDE_TEST( gSessionTempObjMgr.mutex.initialize(
                      (SChar*) "QCU_TEMPORARY_OBJECT_MUTEX",
                      IDU_MUTEX_KIND_NATIVE,
                      IDV_WAIT_INDEX_NULL )
                  != IDE_SUCCESS);

        gIsInitSessionTempObjMgr = ID_TRUE;
    }
        
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

void qcuTemporaryObj::finilizeStatic()
{
/***********************************************************************
 *
 * Description :
 *   mutex�� session temp table info�� �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    SInt i;

    if( gSessionTempObjMgr.tableAllocCount > 0 )
    {
        if( ( gSessionTempObjMgr.tableCount > 0 ) &&
            ( gSessionTempObjMgr.tables     != NULL ) &&
            ( gSessionTempObjMgr.tableCount <=
              gSessionTempObjMgr.tableAllocCount ) )
        {
            for ( i = 0; i < gSessionTempObjMgr.tableCount; i++ )
            {
                if( gSessionTempObjMgr.tables[i].sessionTableCount != 0 )
                {
                    ideLog::log( IDE_QP_0,
                                 "Warning : Session Temporary Table Exist\n"
                                 "Base Table ID       : %u\n"
                                 "Session Table Count : %d\n",
                                 gSessionTempObjMgr.tables[i].baseTableID,
                                 gSessionTempObjMgr.tables[i].sessionTableCount );
                }
            }
        }
        else
        {
            ideLog::log( IDE_QP_0,
                         "Warning : Invalid Session Temporary Object Value\n"
                         "Table Count      : %d\n"
                         "Table Array Size : %d\n"
                         "Table Array Ptr  : %"ID_xPOINTER_FMT"\n",
                         gSessionTempObjMgr.tableCount,
                         gSessionTempObjMgr.tableAllocCount,
                         gSessionTempObjMgr.tables );
        }

        (void) iduMemMgr::free( gSessionTempObjMgr.tables );
    }
    else
    {
        if( ( gSessionTempObjMgr.tableCount != 0 ) ||
            ( gSessionTempObjMgr.tables != NULL ) )
        {
            ideLog::log( IDE_QP_0,
                         "Warning : Invalid Session Temporary Object Value\n"
                         "Table Count      : %d\n"
                         "Table Array Size : %d\n"
                         "Table Array Ptr  : %"ID_xPOINTER_FMT"\n",
                         gSessionTempObjMgr.tableCount,
                         gSessionTempObjMgr.tableAllocCount,
                         gSessionTempObjMgr.tables );
        }
    }

    if( gIsInitSessionTempObjMgr == ID_TRUE )
    {
        (void)gSessionTempObjMgr.mutex.destroy();
        gIsInitSessionTempObjMgr = ID_FALSE;
    }
    else
    {
        if( gSessionTempObjMgr.tableAllocCount != 0 )
        {
            ideLog::log( IDE_QP_0,
                         "Warning : Invalid Session Temporary Object Value\n"
                         "Table Array Size : %d, "
                         "Temporary Object Mutex Flag : %u\n",
                         gSessionTempObjMgr.tableAllocCount,
                         gIsInitSessionTempObjMgr );
        }
    }
}

void qcuTemporaryObj::initTemporaryObj( qcTemporaryObjInfo * aTemporaryObj )
{
/***********************************************************************
 *
 * Description :
 *   temporary obj�� �ʱ�ȭ
 *
 * Implementation :
 *
 ***********************************************************************/

    IDE_ASSERT( aTemporaryObj != NULL );

    initTempTables( & aTemporaryObj->transTempTable );
    initTempTables( & aTemporaryObj->sessionTempTable );
}

void qcuTemporaryObj::finalizeTemporaryObj( idvSQL             * aStatistics,
                                            qcTemporaryObjInfo * aTemporaryObj )
{
/***********************************************************************
 *
 * Description :
 *   temporary obj�� ����
 *
 * Implementation :
 *
 ***********************************************************************/

    dropAllTempTables( aStatistics,
                       aTemporaryObj,
                       QCM_TEMPORARY_ON_COMMIT_DELETE_ROWS );

    dropAllTempTables( aStatistics,
                       aTemporaryObj,
                       QCM_TEMPORARY_ON_COMMIT_PRESERVE_ROWS );
}

void qcuTemporaryObj::dropAllTempTables( idvSQL             * aStatistics,
                                         qcTemporaryObjInfo * aTemporaryObj,
                                         qcmTemporaryType     aTemporaryType )
{
/***********************************************************************
 *
 * Description :
 *   session temporary obj�� ����
 *
 * Implementation :
 *
 ***********************************************************************/

    SInt           i;
    qcTempTables * sTempTables;

    IDE_ASSERT( aTemporaryObj != NULL );

    sTempTables = getTempTables( aTemporaryObj,
                                 aTemporaryType );

    IDE_ASSERT( sTempTables->tableCount <= sTempTables->tableAllocCount );

    if( sTempTables->tableAllocCount > 0 )
    {
        IDU_FIT_POINT( "1.PROJ-1407@qcuTemporaryObj::dropAllTempTables" );

        // drop table

        for ( i = 0; i < sTempTables->tableCount; i++ )
        {
            if( dropTempTable( aStatistics, &(sTempTables->tables[i]), ID_FALSE ) != IDE_SUCCESS )
            {
                ideLog::log( IDE_QP_0,
                             "Warning : Drop Temporary Table [TableID-%u]",
                             sTempTables->tables[i].baseTableID );
            }

            decSessionTableCount( sTempTables->tables[i].baseTableID );
        }

        // free temp obj

        for ( i = 0; i < sTempTables->tableCount; i++ )
        {
            if ( sTempTables->tables[i].indexCount > 0 )
            {
                IDE_ASSERT( sTempTables->tables[i].indices != NULL );
                (void) iduMemMgr::free( sTempTables->tables[i].indices );
            }
        }

        (void) iduMemMgr::free( sTempTables->tables );
    }

    initTempTables( sTempTables );
    
#ifdef ALTIBASE_FIT_CHECK
    IDE_EXCEPTION_END;
#endif
}

void qcuTemporaryObj::initTempTables( qcTempTables * aTempTables )
{
/***********************************************************************
 *
 * Description :
 *   temporary obj�� ����
 *
 * Implementation :
 *
 ***********************************************************************/

    IDE_ASSERT( aTempTables != NULL );

    aTempTables->tables          = NULL;
    aTempTables->tableCount      = 0;
    aTempTables->tableAllocCount = 0;
}


IDE_RC qcuTemporaryObj::truncateTempTable( qcStatement     * aStatement,
                                           UInt              aTableID,
                                           qcmTemporaryType  aTemporaryType )
{
/***********************************************************************
 *
 * Description :
 *   ������ temporary table�� drop�Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/
    qcTempTable        * sTempTable;
    qcTempTables       * sTempTables;
    ULong                sCopySize;
    qcTemporaryObjInfo * sTemporaryObj;

    IDE_ASSERT( aStatement != NULL );

    sTemporaryObj = getTemporaryObj( aStatement );

    sTempTable = getTempTable( sTemporaryObj,
                               aTableID,
                               aTemporaryType );

    if( sTempTable != NULL )
    {
        if( dropTempTable( aStatement->mStatistics, sTempTable, ID_TRUE ) != IDE_SUCCESS )
        {
            ideLog::log( IDE_QP_0,
                         "Warning : Truncate Temporary Table [TableID-%u]",
                         sTempTable->baseTableID );
        }

        decSessionTableCount( aTableID );

        // reset Temp tables
        sTempTables = getTempTables( sTemporaryObj,
                                     aTemporaryType );

        // end offset
        sCopySize  = (ULong)(sTempTables->tables + sTempTables->tableCount);
        // copy size  =  end offset - table end offset
        sCopySize -= (ULong)(sTempTable + 1);

        if( sCopySize > 0 )
        {
            // BUG-36381 memcpy�� ������ ����� ���� ���� �Դϴ�.
            // �����ϰ� memmove�� �����մϴ�.
            idlOS::memmove( (void*)sTempTable,
                            (void*)(sTempTable + 1),
                            sCopySize );
        }

        IDE_ASSERT( sTempTables->tableCount > 0 );
        sTempTables->tableCount--;
    }
    else
    {
        /* session temp table �� �������� ����ü��
         * truncate temporary table�� ȣ��� ��� */
    }
    
    return IDE_SUCCESS;
}

IDE_RC qcuTemporaryObj::dropTempTable( idvSQL      * aStatistics,
                                       qcTempTable * aTempTable,
                                       idBool        aIsDDL )
{
/***********************************************************************
 *
 * Description :
 *   temporary table�� drop
 *
 * Implementation :
 *
 ***********************************************************************/

    smiStatement *sDummySmiStmt;
    smiStatement  sSmiStmt;
    smiTrans      sTrans;
    UInt          sSmiStmtFlag;
    UInt          sStage = 0;
    SInt          i;
    const void *  sTableHandle;

    IDE_ASSERT( aTempTable    != NULL );
    IDE_ASSERT( aTempTable->tableHandle != NULL );

    IDE_TEST( sTrans.initialize() != IDE_SUCCESS );
    sStage = 1;

    IDE_TEST( sTrans.begin( &sDummySmiStmt,
                            aStatistics,
                            (SMI_ISOLATION_CONSISTENT |
                             SMI_TRANSACTION_NORMAL |
                             SMI_TRANSACTION_REPL_DEFAULT |
                             SMI_COMMIT_WRITE_NOWAIT) )
              != IDE_SUCCESS );
    sStage = 2;

    sSmiStmtFlag = SMI_STATEMENT_NORMAL | SMI_STATEMENT_MEMORY_CURSOR;

    IDE_TEST( sSmiStmt.begin( aStatistics, sDummySmiStmt, sSmiStmtFlag )
              != IDE_SUCCESS );
    sStage = 3;

    sTableHandle = aTempTable->tableHandle;

    if ( aIsDDL == ID_FALSE )
    {
        /* - drop session temp table ���߿� tablespace�� ������� ������
         *   �Ǵ°��� �����ϱ� ���� Tablespace lock�� ��´�.*/
        /* - TBS�� X, S lock�� ���� ��� �ٸ� ���̺��� DDL���Ա���
         *   ������ �ֹǷ� IS�� IX ���� intention lock�� ��ƾ��Ѵ�.
         *   session table�� �������� �����Ƿ� table lock�� �ʿ������,
         *   tablespace�� IS,IX lock�� ��� ������ �Լ��� ���
         *   Table�� lock�� �����鼭 TBS�� intention lock�� ��� �� �Լ���
         *   ����Ͽ���.*/
        /* - TBS���� IS lock �� IX lock�� ���̴� ��ǻ� ����.
         *   Session table�� ȥ�ڸ� �����ϹǷ� X,S lock ���̰� ����.
         *   ������ drop session table�̶�� ��Ȳ�� �����ؼ�
         *   table�� X lock, tablespace IX lock�� ����Ͽ���.
         *   lock type�� ���� ������ ���̴� ����.*/
        IDE_TEST( smiValidateAndLockObjects( sSmiStmt.getTrans(),
                                             sTableHandle,
                                             smiGetRowSCN( aTempTable->tableHandle ),
                                             SMI_TBSLV_DDL_DML,
                                             SMI_TABLE_LOCK_X,
                                             ID_ULONG_MAX,
                                             ID_FALSE ) // BUG-28752 ����� Lock�� ������ Lock�� �����մϴ�.
                  != IDE_SUCCESS );
    }
    else
    {
        IDE_TEST( smiValidateAndLockObjects( sSmiStmt.getTrans(),
                                             sTableHandle,
                                             smiGetRowSCN( aTempTable->tableHandle ),
                                             SMI_TBSLV_DDL_DML,
                                             SMI_TABLE_LOCK_X,
                                             smiGetDDLLockTimeOut(sSmiStmt.getTrans()),
                                             ID_FALSE ) // BUG-28752 ����� Lock�� ������ Lock�� �����մϴ�.
                  != IDE_SUCCESS );
    }

    for( i = aTempTable->indexCount - 1 ; i >= 0 ; i-- )
    {
        /* Index�� �������� ���� */
        IDE_TEST( smiTable::dropIndex( &sSmiStmt,
                                       sTableHandle,
                                       smiGetTableIndexes( sTableHandle,
                                                           i ),
                                       SMI_TBSLV_DDL_DML )
                  != IDE_SUCCESS );
    }

    IDE_TEST( smiTable::dropTable( &sSmiStmt,
                                   sTableHandle,
                                   SMI_TBSLV_DDL_DML )
              != IDE_SUCCESS );


    aTempTable->tableHandle = NULL;

    /* index ���� free */
    if( aTempTable->indices != NULL )
    {
        (void) iduMemMgr::free( aTempTable->indices );

        aTempTable->indices    = NULL;
        aTempTable->indexCount = 0;
    }

    sStage = 2;
    IDE_TEST( sSmiStmt.end( SMI_STATEMENT_RESULT_SUCCESS )
              != IDE_SUCCESS );

    sStage = 1;
    IDE_TEST( sTrans.commit() != IDE_SUCCESS );

    sStage = 0;
    IDE_TEST( sTrans.destroy( aStatistics ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch( sStage )
    {
        case 3:
            (void) sSmiStmt.end( SMI_STATEMENT_RESULT_FAILURE );
        case 2:
            (void) sTrans.rollback();
        case 1:
            (void) sTrans.destroy( aStatistics );

            ideLog::log( IDE_QP_0,
                         "Warning : Drop Temporary Table [TableID-%u]",
                         aTempTable->baseTableID );
            break;
        default:
            break;
    }

    return IDE_FAILURE;
}

IDE_RC qcuTemporaryObj::createTempTable( qcStatement  * aStatement,
                                         qcmTableInfo * aTableInfo,
                                         qcTempTable  * aTempTable )
{
/***********************************************************************
 *
 * Description :
 *   temporary table�� create
 *
 * Implementation :
 *   qmxMemory�� ����ϱ����� statement�� �����Ѵ�.
 *   �Ŀ� sm���� temp table���� create�Լ��� �����Ǹ� �����ؾ��Ѵ�.
 *
 ***********************************************************************/

    const void       * sTableHandle;
    smiColumnList    * sSmiColumnList;
    smiValue         * sNullRow;
    UInt               sOrgTableFlag;
    UInt               sOrgTableParallelDegree;
    iduMemoryStatus    sQmxMemStatus;
    UInt               sStage = 0;
    smiTrans           sTrans;
    smiStatement       sSmiStmt;
    smiStatement     * sDummySmiStmt;
    UInt               sSmiStmtFlag = 0;
    idBool             sIsCreated   = ID_FALSE;

    IDE_ASSERT( aStatement != NULL );
    IDE_ASSERT( aTableInfo != NULL );
    IDE_ASSERT( aTempTable != NULL );

    // initialize smiStatement flag
    sSmiStmtFlag &= ~SMI_STATEMENT_MASK;
    sSmiStmtFlag |= SMI_STATEMENT_NORMAL;

    sSmiStmtFlag &= ~SMI_STATEMENT_CURSOR_MASK;
    sSmiStmtFlag |= SMI_STATEMENT_MEMORY_CURSOR;

    /* Memory ������ ���� ������ġ ��� */
    IDE_TEST( aStatement->qmxMem->getStatus( & sQmxMemStatus )
                != IDE_SUCCESS);
    sStage = 1;

    IDE_TEST( qdbCommon::makeSmiColumnList( aStatement,
                                            aTableInfo->TBSID,
                                            aTableInfo->tableID,
                                            aTableInfo->columns,
                                            & sSmiColumnList )
              != IDE_SUCCESS);

    // PROJ-1705
    // ��ũ���̺��� null row�� ������ �ʴ´�.
    if ( ( aTableInfo->tableFlag & SMI_TABLE_TYPE_MASK ) == SMI_TABLE_DISK )
    {
        sNullRow = NULL;
    }
    else
    {
        IDE_TEST( qdbCommon::makeMemoryTableNullRow( aStatement,
                                                     aTableInfo->columns,
                                                     & sNullRow )
                  != IDE_SUCCESS);
    }

    // transaction initialize
    IDE_TEST( sTrans.initialize() != IDE_SUCCESS );
    sStage = 2;

    // transaction begin
    IDE_TEST( sTrans.begin( &sDummySmiStmt,
                            aStatement->mStatistics,
                            (SMI_ISOLATION_CONSISTENT |
                             SMI_TRANSACTION_NORMAL |
                             SMI_TRANSACTION_REPL_DEFAULT |
                             SMI_COMMIT_WRITE_NOWAIT) )
              != IDE_SUCCESS );
    sStage = 3;

    IDE_TEST( sSmiStmt.begin( aStatement->mStatistics, sDummySmiStmt, sSmiStmtFlag )
              != IDE_SUCCESS );
    sStage = 4;

    /* ���� ���̺��� Flag, parallel degree
     * ���ο� ���̺� ���� Flag, parallel degree�� ������ �ȴ�. */
    sOrgTableFlag           = aTableInfo->tableFlag;
    sOrgTableParallelDegree = aTableInfo->parallelDegree;

    IDE_TEST( smiTable::createTable( aStatement->mStatistics,
                                     aTableInfo->TBSID,
                                     0, // PROJ-2446 bugbug
                                     & sSmiStmt,
                                     sSmiColumnList,
                                     ID_SIZEOF(mtcColumn),
                                     NULL,
                                     0,
                                     sNullRow,
                                     sOrgTableFlag | SMI_TABLE_PRIVATE_VOLATILE_TRUE,
                                     aTableInfo->maxrows,
                                     aTableInfo->segAttr,
                                     aTableInfo->segStoAttr,
                                     sOrgTableParallelDegree,
                                     & sTableHandle )
              != IDE_SUCCESS);

    aTempTable->baseTableID = aTableInfo->tableID;
    aTempTable->baseTableSCN = smiGetRowSCN( aTableInfo->tableHandle );
    aTempTable->tableHandle = sTableHandle;

    if( aTableInfo->indexCount > 0 )
    {
        IDE_TEST( createTempIndices(
                      aStatement->mStatistics,
                      &sSmiStmt,
                      aTableInfo,
                      aTempTable,
                      aTableInfo->TBSID )
                  != IDE_SUCCESS );
    }
    else
    {
        aTempTable->indexCount = 0;
        aTempTable->indices    = NULL;
    }

    sStage = 3;
    IDE_TEST( sSmiStmt.end( SMI_STATEMENT_RESULT_SUCCESS )
              != IDE_SUCCESS );

    sStage = 2;
    IDE_TEST( sTrans.commit() != IDE_SUCCESS );
    sIsCreated = ID_TRUE;

    sStage = 1;
    IDE_TEST( sTrans.destroy( aStatement->mStatistics ) != IDE_SUCCESS );

    // Memory ������ ���� �޸� �̵�
    sStage = 0;
    IDE_TEST( aStatement->qmxMem->setStatus( & sQmxMemStatus )
              != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch( sStage )
    {
        case 4:
            (void) sSmiStmt.end( SMI_STATEMENT_RESULT_FAILURE );
        case 3:
            (void) sTrans.rollback();
        case 2:
            (void) sTrans.destroy( aStatement->mStatistics );
        case 1:
            (void) aStatement->qmxMem->setStatus( & sQmxMemStatus );
            break;
        default:
            break;
    }

    /* commit���Ŀ� ���ܰ� �߻��ϸ� create�� table�� drop�Ѵ�.*/
    if ( sIsCreated == ID_TRUE )
    {
        if ( ( aStatement->myPlan->parseTree->stmtKind & QCI_STMT_MASK_MASK ) == QCI_STMT_MASK_DDL )
        {
            (void)dropTempTable( aStatement->mStatistics, aTempTable, ID_TRUE );
        }
        else
        {
            (void)dropTempTable( aStatement->mStatistics, aTempTable, ID_FALSE );
        }
    }

    return IDE_FAILURE;
}



IDE_RC qcuTemporaryObj::createTempIndices(
    idvSQL               * aStatistics,
    smiStatement         * aStatement,
    qcmTableInfo         * aTableInfo,
    qcTempTable          * aTempTable,
    scSpaceID              aTableTBSID )
{
/***********************************************************************
 *
 * Description :
 *    temporary table�� index�� �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    mtcColumn               sNewTableIndexColumnAtRow[QC_MAX_KEY_COLUMN_COUNT];
    mtcColumn               sNewTableIndexColumnAtKey[QC_MAX_KEY_COLUMN_COUNT];
    UInt                    i;
    UInt                    j;
    UInt                    sTableType;
    smiColumnList           sColumnListAtRow[QC_MAX_KEY_COLUMN_COUNT];
    smiColumnList           sColumnListAtKey[QC_MAX_KEY_COLUMN_COUNT];
    UInt                    sStage = 0;
    const void            * sIndexHandle;
    mtcColumn             * sTempColumn;
    UInt                    sIndexColumnID;
    const void            * sTempTableHandle;
    const qcmIndex        * sIndex;
    smiSegAttr              sSegAttr;
    smiSegStorageAttr       sSegStoAttr;
    UInt                    sFlag;

    IDE_ASSERT( aTableInfo->indexCount > 0 );

    sTempTableHandle = aTempTable->tableHandle;

    sTableType = aTableInfo->tableFlag & SMI_TABLE_TYPE_MASK;

    IDE_ASSERT( sTableType == SMI_TABLE_VOLATILE );

    IDE_TEST( iduMemMgr::malloc(
            IDU_MEM_QCI,
            ID_SIZEOF(qcTempIndex) * aTableInfo->indexCount,
            (void**) &(aTempTable->indices) )
        != IDE_SUCCESS);
    sStage = 1;

    aTempTable->indexCount = 0;

    for( j = 0; j < aTableInfo->indexCount; j++ )
    {
        sIndex = &(aTableInfo->indices[j]);

        for( i = 0 ; i < sIndex->keyColCount ; i++ )
        {
            /* aDelColList�� ���� ��� index column ID�� �ٲ���
             * �ʴ´�. */
            sIndexColumnID = sIndex->keyColumns[i].column.id;

            IDE_TEST( smiGetTableColumns(
                          sTempTableHandle,
                          (sIndexColumnID % QC_MAX_COLUMN_COUNT),
                          (const smiColumn**)&sTempColumn )
                      != IDE_SUCCESS );

            idlOS::memcpy(&(sNewTableIndexColumnAtRow[i]),
                          sTempColumn,
                          ID_SIZEOF(mtcColumn) );

            sNewTableIndexColumnAtRow[i].column.flag =
                sIndex->keyColsFlag[i];
            sColumnListAtRow[i].column = (smiColumn*)
                &sNewTableIndexColumnAtRow[i];

            idlOS::memcpy(&(sNewTableIndexColumnAtKey[i]),
                          &(sNewTableIndexColumnAtRow[i]),
                          ID_SIZEOF(mtcColumn));

            sColumnListAtKey[i].column =
                (smiColumn*) &sNewTableIndexColumnAtKey[i];

            if (i == sIndex->keyColCount - 1)
            {
                sColumnListAtRow[i].next = NULL;
                sColumnListAtKey[i].next = NULL;
            }
            else
            {
                sColumnListAtRow[i].next = &sColumnListAtRow[i+1];
                sColumnListAtKey[i].next = &sColumnListAtKey[i+1];
            }
        }

        // Memory Table �� ������� �ʴ� �Ӽ�������, �����Ͽ� �����Ѵ�.
        sSegAttr.mPctFree   = QD_MEMORY_TABLE_DEFAULT_PCTFREE;  // PCTFREE
        sSegAttr.mPctUsed   = QD_MEMORY_TABLE_DEFAULT_PCTUSED;  // PCTUSED
        sSegAttr.mInitTrans = QD_MEMORY_INDEX_DEFAULT_INITRANS; // initial ttl size
        sSegAttr.mMaxTrans  = QD_MEMORY_INDEX_DEFAULT_MAXTRANS; // maximum ttl size
        sSegStoAttr.mInitExtCnt = QD_MEMORY_SEGMENT_DEFAULT_STORAGE_INITEXTENTS;  // initextents
        sSegStoAttr.mNextExtCnt = QD_MEMORY_SEGMENT_DEFAULT_STORAGE_NEXTEXTENTS;  // nextextents
        sSegStoAttr.mMinExtCnt  = QD_MEMORY_SEGMENT_DEFAULT_STORAGE_MINEXTENTS;  // minextents
        sSegStoAttr.mMaxExtCnt  = QD_MEMORY_SEGMENT_DEFAULT_STORAGE_MAXEXTENTS;  // maxextents

        sFlag = smiTable::getIndexInfo( sIndex->indexHandle );

        IDE_TEST(smiTable::createIndex(
                     aStatistics,
                     aStatement,
                     aTableTBSID,
                     sTempTableHandle,
                     (SChar*)sIndex->name,
                     sIndex->indexId,
                     sIndex->indexTypeId,
                     sColumnListAtKey,
                     sFlag,
                     QD_INDEX_DEFAULT_PARALLEL_DEGREE,
                     SMI_INDEX_BUILD_UNCOMMITTED_ROW_DISABLE,
                     sSegAttr,
                     sSegStoAttr,
                     0, /* PROJ-2433 : temporary table���� �Ϲ� index ���. */
                     &sIndexHandle)
                 != IDE_SUCCESS);

        aTempTable->indices[j].baseIndexID = sIndex->indexId;
#ifdef DEBUG
        aTempTable->indices[j].indexID = smiGetIndexId( sIndexHandle );
#endif
        aTempTable->indexCount++ ;
    }

    IDE_ASSERT( aTempTable->indexCount == aTableInfo->indexCount );

    for( i = 0 ; i < aTempTable->indexCount ; i++ )
    {
        /* create/drop index �� �ݺ��ϸ� index handle�� ����ȴ�.
         * ��� index�� ��� ���� �� �Ŀ� handle�� �ٽ� �����´�.*/
        aTempTable->indices[i].indexHandle = smiGetTableIndexes( sTempTableHandle,
                                                                 i );

        IDE_DASSERT( aTempTable->indices[i].indexHandle ==
                     smiGetTableIndexByID( sTempTableHandle,
                                           aTempTable->indices[i].indexID ) );
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch( sStage )
    {
        case 1:
            (void) iduMemMgr::free( aTempTable->indices );
            break;
        default:
            break;
    }
    return IDE_FAILURE;
}



IDE_RC qcuTemporaryObj::createAndGetTempTable( qcStatement      * aStatement,
                                               qcmTableInfo     * aTableInfo,
                                               const void      ** aTempTableHandle )
{
/***********************************************************************
 *
 * Description :
 *   temporary table�� create�ϰ� session temporary obj�� �߰�
 *
 * Implementation :
 *   temp table�� ����ð��� �˻��Ǿ� tableHandle�� ��ü�Ǵ� �������
 *   plan�� ������ �����ʴ� ������ ������ �ǻ��� �˻������ �߻��Ѵ�.
 *   �̸� �����ϰ��� temp table�� sorting�� array�� �����Ѵ�.
 *   array�� ���� init_size��ŭ �Ҵ�ǰ� �����ϸ� �ι�� Ȯ���Ѵ�.
 *
 ***********************************************************************/

    qcTempTables       * sTempTables;
    qcTempTable        * sTempTable;
    qcTemporaryObjInfo * sTemporaryObj;

    IDE_ASSERT( aStatement       != NULL );
    IDE_ASSERT( aTableInfo       != NULL );
    IDE_ASSERT( aTempTableHandle != NULL );

    sTemporaryObj = getTemporaryObj( aStatement );

    sTempTables = getTempTables( sTemporaryObj,
                                 aTableInfo->temporaryInfo.type );

    IDE_ASSERT( sTempTables != NULL );

    /* temp table obj�� Ȯ���Ѵ�. */
    IDE_TEST( expandTempTables( sTempTables ) != IDE_SUCCESS );

    sTempTable = sTempTables->tables + sTempTables->tableCount;

    IDE_TEST( createTempTable( aStatement,
                               aTableInfo,
                               sTempTable ) != IDE_SUCCESS );

    IDE_ASSERT( sTempTables->tableCount >= 0 );
    sTempTables->tableCount++;

    IDE_TEST( incSessionTableCount( aTableInfo->tableID ) != IDE_SUCCESS);

    if ( sTempTables->tableCount > 1 )
    {
        idlOS::qsort( sTempTables->tables,
                      sTempTables->tableCount,
                      ID_SIZEOF(qcTempTable),
                      compareTempTableID );

        sTempTable = getTempTable( sTemporaryObj,
                                   aTableInfo->tableID,
                                   aTableInfo->temporaryInfo.type  );

        IDE_ASSERT( sTempTable != NULL );
    }
    else
    {
        // Nothing to do.
        IDE_ASSERT( sTempTables->tableCount == 1 );

        IDE_DASSERT( sTempTable == getTempTable( sTemporaryObj,
                                                 aTableInfo->tableID,
                                                 aTableInfo->temporaryInfo.type ));
    }

    *aTempTableHandle = sTempTable->tableHandle;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qcuTemporaryObj::expandTempTables( qcTempTables *  aTempTables )
{
/***********************************************************************
 *
 * Description :
 *   temporary table object�� ���� �� ��� Ȯ���Ѵ�.(�����Լ�)
 *
 * Implementation :
 *
 ***********************************************************************/

    IDE_ASSERT( aTempTables != NULL );
    IDE_ASSERT( aTempTables->tableCount <= aTempTables->tableAllocCount );

    if( aTempTables->tableCount == aTempTables->tableAllocCount )
    {
        IDE_TEST( expandTempTableInfo( (void**)&(aTempTables->tables),
                                       ID_SIZEOF( qcTempTable ),
                                       &(aTempTables->tableAllocCount) ) != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC qcuTemporaryObj::expandSessionTempObj()
{
/***********************************************************************
 *
 * Description :
 *   session temporary object�� ���� �� ��� Ȯ���Ѵ�.(�����Լ�)
 *
 * Implementation :
 *
 ***********************************************************************/

    IDE_ASSERT( gSessionTempObjMgr.tableCount <= gSessionTempObjMgr.tableAllocCount );

    if( gSessionTempObjMgr.tableCount == gSessionTempObjMgr.tableAllocCount )
    {
        IDE_TEST( expandTempTableInfo( (void**)&(gSessionTempObjMgr.tables),
                                       ID_SIZEOF( qcuSessionTempObj ),
                                       &(gSessionTempObjMgr.tableAllocCount) ) != IDE_SUCCESS );
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC qcuTemporaryObj::expandTempTableInfo( void   ** aTempTableInfo,
                                             UInt      aObjectSize,
                                             SInt   *  aAllocCount )
{
/***********************************************************************
 *
 * Description :
 *   temporary table object�� ������ array�� Ȯ��
 *
 * Implementation :
 *
 ***********************************************************************/

    void   * sOldTableInfo;
    void   * sNewTableInfo;
    SInt     sTableAlloCount;
    UInt     sState = 0;

    IDE_ASSERT( aTempTableInfo != NULL );
    IDE_ASSERT( aAllocCount    != NULL );

    sOldTableInfo   = *aTempTableInfo;
    sTableAlloCount = *aAllocCount;

    if ( sTableAlloCount == 0 )
    {
        IDE_TEST( iduMemMgr::malloc(
                IDU_MEM_QCI,
                aObjectSize * QCU_TEMPORARY_TABLE_INIT_SIZE,
                (void**) &sNewTableInfo )
            != IDE_SUCCESS);
        sState = 1;

        sNewTableInfo   = sNewTableInfo;
        sTableAlloCount = QCU_TEMPORARY_TABLE_INIT_SIZE;
    }
    else
    {
        IDE_TEST( iduMemMgr::malloc(
                IDU_MEM_QCI,
                aObjectSize * sTableAlloCount * 2,
                (void**) &sNewTableInfo )
            != IDE_SUCCESS);
        sState = 1;

        idlOS::memcpy( (void*)sNewTableInfo,
                       (void*)sOldTableInfo,
                       sTableAlloCount * aObjectSize );

        (void) iduMemMgr::free( sOldTableInfo );

        sNewTableInfo    = sNewTableInfo;
        sTableAlloCount *= 2;
    }

    *aTempTableInfo = sNewTableInfo;
    *aAllocCount    = sTableAlloCount;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch( sState )
    {
        case 1:
            iduMemMgr::free( sNewTableInfo );
            break;
        default:
            break;
    }

    return IDE_FAILURE;
}

void qcuTemporaryObj::getTempTableHandle( qcStatement    * aStatement,
                                          qcmTableInfo   * aTableInfo,
                                          const void    ** aTableHandle,
                                          smSCN          * aTableSCN )
{
/***********************************************************************
 *
 * Description :
 *   table id�� temporary table handle�� �˻�
 *
 * Implementation :
 *
 ***********************************************************************/

    qcTempTable   * sTempTable;
    const void    * sTableHandle;
    smSCN           sTableSCN;

    IDE_ASSERT( aStatement != NULL );
    IDE_ASSERT( aTableInfo != NULL );

    SM_INIT_SCN( &sTableSCN );

    sTempTable = getTempTable( getTemporaryObj( aStatement ),
                               aTableInfo->tableID,
                               aTableInfo->temporaryInfo.type );

    if ( sTempTable != NULL )
    {
        sTableHandle = sTempTable->tableHandle;
        sTableSCN    = sTempTable->baseTableSCN;
    }
    else
    {
        sTableHandle = NULL;
    }

    *aTableHandle = sTableHandle;
    *aTableSCN    = sTableSCN;
}

const void * qcuTemporaryObj::getTempIndexHandle( qcStatement    * aStatement,
                                                  qcmTableInfo   * aTableInfo,
                                                  UInt             aBaseIndexID )
{
/***********************************************************************
 *
 * Description :
 *   table id, index id�� temporary index handle�� �˻�
 *
 * Implementation :
 *
 ***********************************************************************/

    qcTempTable  * sTempTable;
    qcTempIndex    sTempIndex;
    qcTempIndex  * sFound;
    const void   * sIndexHandle = NULL;

    IDE_ASSERT( aStatement != NULL );

    sTempTable = getTempTable( getTemporaryObj( aStatement ),
                               aTableInfo->tableID,
                               aTableInfo->temporaryInfo.type  );

    if ( sTempTable != NULL )
    {
        if ( sTempTable->indexCount > 0 )
        {
            sTempIndex.baseIndexID = aBaseIndexID;

            sFound = (qcTempIndex*) idlOS::bsearch( & sTempIndex,
                                                    sTempTable->indices,
                                                    sTempTable->indexCount,
                                                    ID_SIZEOF(qcTempIndex),
                                                    compareTempIndexID );

            if ( sFound != NULL )
            {
                sIndexHandle = sFound->indexHandle;
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
    }

    return sIndexHandle;
}

idBool qcuTemporaryObj::isTemporaryTable( qcmTableInfo * aTableInfo )
{
/***********************************************************************
 *
 * Description :
 *   temporary table�ΰ�?
 *
 * Implementation :
 *
 ***********************************************************************/
    idBool            sIsTemporaryTable;
    qcmTemporaryType  sTemporaryType;

    IDE_ASSERT( aTableInfo != NULL );

    sTemporaryType = aTableInfo->temporaryInfo.type;

    if( sTemporaryType == QCM_TEMPORARY_ON_COMMIT_NONE )
    {
        sIsTemporaryTable = ID_FALSE;
    }
    else
    {
        IDE_ASSERT_MSG((( sTemporaryType == QCM_TEMPORARY_ON_COMMIT_DELETE_ROWS ) ||
                        ( sTemporaryType == QCM_TEMPORARY_ON_COMMIT_PRESERVE_ROWS )),
                       "Invalid Temporary Type : %"ID_UINT32_FMT"\n",
                       sTemporaryType );

        sIsTemporaryTable = ID_TRUE;
    }

    return sIsTemporaryTable;
}

qcTempTables * qcuTemporaryObj::getTempTables( qcTemporaryObjInfo  * aTemporaryObj,
                                               qcmTemporaryType      aTemporaryType )
{
/***********************************************************************
 *
 * Description :
 *   temporary type�� ���� temporary table array ��ȯ
 *
 * Implementation :
 *
 ***********************************************************************/
    qcTempTables * sTempTables;

    IDE_ASSERT( aTemporaryObj != NULL );

    switch ( aTemporaryType )
    {
        case QCM_TEMPORARY_ON_COMMIT_DELETE_ROWS:
        {
            sTempTables = & aTemporaryObj->transTempTable;
            break;
        }
        case QCM_TEMPORARY_ON_COMMIT_PRESERVE_ROWS:
        {
            sTempTables = & aTemporaryObj->sessionTempTable;
            break;
        }
        default:
        {
            ideLog::log( IDE_ERR_0,
                         "Invalid Temporary Type : %u\n",
                          aTemporaryType );

            IDE_ASSERT( 0 );
            break;
        }
    }

    return sTempTables;
}

qcTempTable * qcuTemporaryObj::getTempTable( qcTemporaryObjInfo  * aTemporaryObj,
                                             UInt                  aBaseTableID,
                                             qcmTemporaryType      aTemporaryType )
{
/***********************************************************************
 *
 * Description :
 *   table id�� temporary table�� �˻�
 *
 * Implementation :
 *   sorting�Ǿ� �־� bsearch�� ����Ѵ�.
 *   �˻������� default�� transaction temp table�� ���� �˻��ϰ�
 *   session temp table�� �˻��Ѵ�.
 *
 ***********************************************************************/

    qcTempTables * sTempTables;
    qcTempTable    sTempTable;
    qcTempTable  * sFound;

    IDE_ASSERT( aTemporaryObj != NULL );

    sTempTables = getTempTables( aTemporaryObj,
                                 aTemporaryType );

    if ( sTempTables->tableCount > 0 )
    {
        sTempTable.baseTableID = aBaseTableID;

        sFound = (qcTempTable*) idlOS::bsearch( & sTempTable,
                                                sTempTables->tables,
                                                sTempTables->tableCount,
                                                ID_SIZEOF(qcTempTable),
                                                compareTempTableID );
    }
    else
    {
        sFound = NULL;
    }

    return sFound;
}

idBool qcuTemporaryObj::existSessionTable( qcmTableInfo  * aTableInfo )
{
/***********************************************************************
 *
 * Description :
 *   Table id�� �˻��Ͽ�, �ش� temp table�� ������
 *   session temporary table�� �����ϴ��� Ȯ���Ѵ�.
 *
 *   on commit preserve rows �� ������ temp table�� Ȯ���ϰ�
 *   on commit delete rows�� �Ű澲�� �ʴ´�.
 *   Table lock���� commit�������� ��ȣ�ǰ�, on commit delete rows��
 *   commit�� session temp table�� drop�ǹǷ� on DDL���� �浹�� ��������
 *   �ʾƵ� �ȴ�.
 *
 * Implementation :
 *
 ***********************************************************************/
    idBool sExistSessionTable = ID_FALSE;

    IDE_ASSERT( aTableInfo != NULL );

    if( aTableInfo->temporaryInfo.type != QCM_TEMPORARY_ON_COMMIT_NONE )
    {
        if( getSessionTableCount( aTableInfo->tableID ) > 0 )
        {
            sExistSessionTable = ID_TRUE;
        }
    }

    return sExistSessionTable;
}

IDE_RC qcuTemporaryObj::incSessionTableCount( UInt   aBaseTableID )
{
/***********************************************************************
 *
 * Description :
 *   table id�� �˻��Ͽ�, �ش� temp table�� ������
 *   session temporary table�� ���� �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/
    qcuSessionTempObj  * sSessionTempObj;
    UInt                 sState = 0;

    lock();
    sState = 1;

    sSessionTempObj = getSessionTempObj( aBaseTableID );

    if( sSessionTempObj == NULL )
    {
        /* �ش� Table�� ������ �߰��Ѵ�. */
        IDE_TEST( allocSessionTempObj( aBaseTableID,
                                       &sSessionTempObj )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    (sSessionTempObj->sessionTableCount)++;

    sState = 0;
    unlock();

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sState == 1 )
    {
        unlock();
    }
    else
    {
        // Nothing to do.
    }

    return IDE_FAILURE;
}

IDE_RC qcuTemporaryObj::allocSessionTempObj( UInt                 aBaseTableID,
                                             qcuSessionTempObj ** aSessionTempObj )
{
/***********************************************************************
 *
 * Description :
 *   table id�� �˻��Ͽ�, �ش� temp table�� ������
 *   session temporary table�� ���� �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/
    qcuSessionTempObj  * sSessionTempObj;

    IDE_ASSERT( aSessionTempObj != NULL );
    IDE_DASSERT( getSessionTempObj( aBaseTableID ) == NULL );

    IDE_TEST( expandSessionTempObj() != IDE_SUCCESS );

    sSessionTempObj = gSessionTempObjMgr.tables + gSessionTempObjMgr.tableCount;

    sSessionTempObj->baseTableID       = aBaseTableID ;
    sSessionTempObj->sessionTableCount = 0;

    gSessionTempObjMgr.tableCount++; // ��Ī, �ּ��߰�

    if ( gSessionTempObjMgr.tableCount > 1 )
    {
        idlOS::qsort( gSessionTempObjMgr.tables,
                      gSessionTempObjMgr.tableCount,
                      ID_SIZEOF(qcuSessionTempObj),
                      compareSessionTempTableID );

        sSessionTempObj = getSessionTempObj( aBaseTableID );

        IDE_ASSERT( sSessionTempObj != NULL );
    }
    else
    {
        IDE_ASSERT( gSessionTempObjMgr.tableCount == 1 );

        IDE_DASSERT( sSessionTempObj == getSessionTempObj( aBaseTableID ));
    }

    *aSessionTempObj = sSessionTempObj;
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

void qcuTemporaryObj::decSessionTableCount( UInt   aBaseTableID )
{
/***********************************************************************
 *
 * Description :
 *   table id�� �˻��Ͽ�, �ش� temp table�� ������
 *   session temporary table�� ���� ���ҽ�Ų��.
 *
 * Implementation :
 *
 ***********************************************************************/
    qcuSessionTempObj  * sSessionTempObj;

    lock();

    sSessionTempObj = getSessionTempObj( aBaseTableID );

    IDE_ASSERT( sSessionTempObj != NULL );
    IDE_ASSERT( gSessionTempObjMgr.tableCount > 0 );

    (sSessionTempObj->sessionTableCount)--;

    unlock();
}

SInt qcuTemporaryObj::getSessionTableCount( UInt   aBaseTableID )
{
/***********************************************************************
 *
 * Description :
 *   table id�� �˻��Ͽ�, �ش� temp table�� ������
 *   session temporary table�� ���� ��ȯ�Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/
    qcuSessionTempObj * sSessionTempObj;
    SInt                sSessionTableCount = 0;

    lock();

    sSessionTempObj = getSessionTempObj( aBaseTableID );

    if( sSessionTempObj != NULL )
    {
        sSessionTableCount = sSessionTempObj->sessionTableCount;

        IDE_DASSERT( sSessionTableCount >= 0 );
    }
    else
    {
        // nothing to do
    }

    unlock();

    return sSessionTableCount;
}

qcuSessionTempObj * qcuTemporaryObj::getSessionTempObj( UInt  aBaseTableID )
{
/***********************************************************************
 *
 * Description :
 *   table id �� session temporary object�� �˻�
 *   lock�� ���� ���·� ȣ��ȴ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    qcuSessionTempObj * sTempObj;
    qcuSessionTempObj   sTableInfo;

    if ( gSessionTempObjMgr.tableCount > 0 )
    {
        sTableInfo.baseTableID = aBaseTableID;

        sTempObj = (qcuSessionTempObj*) idlOS::bsearch(
            & sTableInfo,
            gSessionTempObjMgr.tables,
            gSessionTempObjMgr.tableCount,
            ID_SIZEOF(qcuSessionTempObj),
            compareSessionTempTableID );
    }
    else
    {
        sTempObj = NULL;
    }

    return sTempObj;
}

void qcuTemporaryObj::lock()
{
    IDE_DASSERT( gIsInitSessionTempObjMgr == ID_TRUE );
    (void)gSessionTempObjMgr.mutex.lock( NULL );
}

void qcuTemporaryObj::unlock()
{
    IDE_DASSERT( gIsInitSessionTempObjMgr == ID_TRUE );
    (void)gSessionTempObjMgr.mutex.unlock();
}

