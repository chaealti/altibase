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
 * $Id: qcmDictionary.cpp 91010 2021-06-17 01:33:11Z hykim $
 **********************************************************************/

#include <idl.h>
#include <ide.h>
#include <qcpManager.h>
#include <qcg.h>
#include <qcmDictionary.h>
#include <qmv.h>
#include <qcmUser.h>
#include <qcuSqlSourceInfo.h>
#include <smiTableSpace.h>
#include <qdd.h>
#include <qdpDrop.h>
#include <qcmView.h>
#include <qdbComment.h>
#include <qdx.h>

const void * gQcmCompressionTables;
const void * gQcmCompressionTablesIndex[ QCM_MAX_META_INDICES ];

IDE_RC qcmDictionary::selectDicSmOID( qcTemplate     * aTemplate,
                                      void           * aTableHandle,
                                      void           * aIndexHeader,
                                      const void     * aValue,
                                      smOID          * aSmOID )
{
/***********************************************************************
 *
 * Description :
 *    Dictionary table ���� smOID �� ���Ѵ�.
 *
 * Implementation :
 *
 *    1. Dictionary table �˻��� ���� range �� �����Ѵ�.
 *    2. Ŀ���� ����.
 *       Dictionary table �� �÷��� 1������ Ư���� ���̺��̹Ƿ� 1��¥�� smiColumnList �� ����Ѵ�.
 *    3. ���ǿ� �´� row �� scGRID �� ��´�.
 *    4. scGRID ���� smOID �� ��´�.
 *
 *    PROJ-2264
 *    �� �Լ��� dictionary table �� ���� ó���̹Ƿ�
 *    fetch column list�� �������� �ʴ´�.
 *
 ***********************************************************************/

    smiTableCursor      sCursor;

    UInt                sStep = 0;
    const void        * sRow;

    mtcColumn         * sColumn;
    smiColumnList       sColumnList[QCM_MAX_META_COLUMNS];
    scGRID              sRid; // Disk Table�� ���� Record IDentifier
    smiCursorProperties sCursorProperty;
    UInt                sIndexType;

    smiRange            sRange;
    qtcMetaRangeColumn  sFirstMetaRange;

    void              * sDicTable;
    smOID               sOID;
    mtcColumn         * sFirstMtcColumn;

    // table handel
    sDicTable = aTableHandle;

    IDE_TEST( smiGetTableColumns( sDicTable,
                                  0,
                                  (const smiColumn**)&sFirstMtcColumn )
              != IDE_SUCCESS );

    // Make range
    qcm::makeMetaRangeSingleColumn( &sFirstMetaRange,
                                    sFirstMtcColumn,
                                    (const void*) aValue,
                                    &sRange );

    sCursor.initialize();

    IDE_TEST( smiGetTableColumns( sDicTable,
                                  0,
                                  (const smiColumn**)&sColumn )
              != IDE_SUCCESS );

    IDE_TEST( smiGetTableColumns( sDicTable,
                                  0,
                                  (const smiColumn**)&sColumnList[0].column )
              != IDE_SUCCESS );

    sColumnList[0].next = NULL;

    sIndexType = smiGetIndexType(aIndexHeader);
    
    SMI_CURSOR_PROP_INIT_FOR_INDEX_SCAN(&sCursorProperty, NULL, sIndexType);

    IDE_TEST( sCursor.open( QC_SMI_STMT( aTemplate->stmt ),
                            sDicTable,
                            aIndexHeader, // index
                            smiGetRowSCN( sDicTable ),
                            sColumnList, // columns
                            &sRange, // range
                            smiGetDefaultKeyRange(),
                            smiGetDefaultFilter(),
                            SMI_LOCK_READ | SMI_TRAVERSE_FORWARD | SMI_PREVIOUS_DISABLE,
                            SMI_SELECT_CURSOR,
                            &sCursorProperty)
              != IDE_SUCCESS );
    sStep = 1;

    IDE_TEST( sCursor.beforeFirst() != IDE_SUCCESS);

    IDE_TEST( sCursor.readRow( &sRow, &sRid, SMI_FIND_NEXT )
              != IDE_SUCCESS );

    if( sRow != NULL )
    {
        sOID = SMI_CHANGE_GRID_TO_OID(sRid);
    }
    else
    {
        // Data not found in Dictionary table.
        sOID = 0;
    }

    sStep = 0;
    IDE_TEST( sCursor.close() != IDE_SUCCESS );

    *aSmOID = sOID;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sStep == 1 )
    {
        (void)sCursor.close();
    }
    else
    {
        // Cursor already closed.
    }

    return IDE_FAILURE;
}

IDE_RC qcmDictionary::recreateDictionaryTable( qcStatement           * aStatement,
                                               qcmTableInfo          * aTableInfo,
                                               qcmColumn             * aColumn,
                                               scSpaceID               aTBSID,
                                               qcmDictionaryTable   ** aDicTable )
{
    qcmTableInfo  * sDicTableInfo;
    qcmColumn     * sColumn;
    UInt            sTableFlagMask;
    UInt            sTableFlag;
    UInt            sTableParallelDegree;

    // create new dictionary tables
    for( sColumn = aColumn; sColumn != NULL; sColumn = sColumn->next )
    {
        // Max rows �� ���� ���� dictionary table �� table info �� �����´�.
        sDicTableInfo = (qcmTableInfo *)smiGetTableRuntimeInfoFromTableOID(
            sColumn->basicInfo->column.mDictionaryTableOID );

        // Table flag �� data table �� ���� ������.
        sTableFlagMask  = QDB_TABLE_ATTR_MASK_ALL;
        sTableFlag      = aTableInfo->tableFlag;
        sTableParallelDegree = 1;

        // Dictionary table flag �� �����Ѵ�.
        sTableFlagMask |= SMI_TABLE_DICTIONARY_MASK;
        sTableFlag     |= SMI_TABLE_DICTIONARY_TRUE;

        // PROJ-2429 table type�� ������ memory�� �����Ѵ�.
        sTableFlag     &= ~SMI_TABLE_TYPE_MASK;
        sTableFlag     |= SMI_TABLE_MEMORY;

        IDE_TEST( createDictionaryTable( aStatement,
                                         aTableInfo->tableOwnerID,
                                         sColumn,
                                         1, // Column count
                                         aTBSID,
                                         sDicTableInfo->maxrows,
                                         sTableFlagMask,
                                         sTableFlag,
                                         sTableParallelDegree,
                                         aDicTable )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qcmDictionary::makeDictionaryTable( qcStatement           * aStatement,
                                           scSpaceID               aTBSID,
                                           qcmColumn             * aColumns,
                                           qcmCompressionColumn  * aCompCol,
                                           qcmDictionaryTable   ** aDicTable )
{
    qdTableParseTree   * sParseTree;
    qcmColumn          * sColIter;
    qcmColumn          * sColumn = NULL;
    ULong                sMaxrows;
    UInt                 sTableFlagMask;
    UInt                 sTableFlag;
    UInt                 sTableParallelDegree;

    sParseTree = (qdTableParseTree *)aStatement->myPlan->parseTree;

    for( sColIter = aColumns;
         sColIter != NULL;
         sColIter = sColIter->next )
    {
        if ( QC_IS_NAME_MATCHED( aCompCol->namePos, sColIter->namePos ) )
        {
            sColumn = sColIter;

            // �̸��� ���� qcmColumn �� ã�Ҵ�.
            break;
        }
        else
        {
            // ã���ִ� �÷��� �ƴϴ�.
            // Nothing to do.
        }
    }

    if( sColumn != NULL )
    {
        // ���� data table �� column �̸��� �����´�.
        QC_STR_COPY( sColumn->name, sColumn->namePos );

        // MAXROW �� �����´�.
        if( aCompCol->maxrows == 0 )
        {
            if( ( sParseTree->maxrows != 0 ) &&
                ( sParseTree->maxrows != ID_ULONG_MAX ) )
            {
                // CREATE TABLE ��
                // Compress column �� MAXROW �� �������� �ʾҰ�,
                // data table �� MAXROW �� �����Ǿ��� ���� table �� MAXROW �� ������.
                sMaxrows = sParseTree->maxrows;
            }
            else
            {
                // Compress column �� data table ���� ��� MAXROW ��
                // �������� �ʾ����� 0(������)���� �����Ѵ�.
                sMaxrows = 0;
            }
        }
        else
        {
            // Compress column �� MAXROW �� �����Ǿ����� �� ������ �����Ѵ�.
            sMaxrows = aCompCol->maxrows;
        }

        // Dictionary table ����
        sTableFlagMask = SMI_TABLE_DICTIONARY_MASK;
        sTableFlag     = SMI_TABLE_DICTIONARY_TRUE;
        sTableParallelDegree = 1;

        IDE_TEST( createDictionaryTable( aStatement,
                                         sParseTree->userID,
                                         sColumn,
                                         1, // Column count
                                         aTBSID,
                                         sMaxrows,
                                         sTableFlagMask,
                                         sTableFlag,
                                         sTableParallelDegree,
                                         aDicTable )
                  != IDE_SUCCESS );
    }
    else
    {
        // Compression column �� �ش��ϴ� column �� ����.
        // ����ڰ� �߸� ����� ��� validation ���� �ɷ����Ƿ� ASSERTION ó�� �Ѵ�.
        IDE_DASSERT( 0 );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qcmDictionary::validateCompressColumn( qcStatement       * aStatement,
                                              qdTableParseTree  * aParseTree,
                                              smiTableSpaceType   aType )
{
/***********************************************************************
 *
 * Description :
 *    Dictionary based compress column ���� �Լ�
 *
 * Implementation :
 *    1. Column list�� ����� �÷����� �˻�
 *      1-1. ������ type ���� �˻�
 *      1-2. ������ size ���� �˻�
 *      1-3. smiColumn �� compression column flag ����
 *    2. ���� ���̺��� tablespace �˻�
 *    3. MEMORY PARTITIONED TABLE  �˻�.
 *
 ***********************************************************************/

    qcmCompressionColumn * sCompColumn;
    qcmColumn            * sColumn;
    qcuSqlSourceInfo       sqlInfo;

    IDE_DASSERT( aParseTree != NULL );

    if( aParseTree->compressionColumn != NULL )
    {
        // 1. column list �� ����� �÷����� �˻�
        for( sCompColumn = aParseTree->compressionColumn;
             sCompColumn != NULL;
             sCompColumn = sCompColumn->next )
        {
            for( sColumn = aParseTree->columns;
                 sColumn != NULL;
                 sColumn = sColumn->next )
            {
                if ( QC_IS_NAME_MATCHED( sCompColumn->namePos, sColumn->namePos ) )
                {
                    // 1-1. �����ϴ� type ���� �˻��Ѵ�.
                    switch( sColumn->basicInfo->type.dataTypeId )
                    {
                        case MTD_CHAR_ID :
                        case MTD_VARCHAR_ID :
                        case MTD_NCHAR_ID :
                        case MTD_NVARCHAR_ID :
                        case MTD_BYTE_ID :
                        case MTD_VARBYTE_ID :
                        case MTD_NIBBLE_ID :
                        case MTD_BIT_ID :
                        case MTD_VARBIT_ID :
                        case MTD_DATE_ID :
                            // ���� type
                            break;

                        case MTD_DOUBLE_ID :
                        case MTD_FLOAT_ID :
                        case MTD_NUMBER_ID :
                        case MTD_NUMERIC_ID :
                        case MTD_BIGINT_ID :
                        case MTD_INTEGER_ID :
                        case MTD_REAL_ID :
                        case MTD_SMALLINT_ID :
                        case MTD_ECHAR_ID :
                        case MTD_EVARCHAR_ID :
                        case MTD_BLOB_ID :
                        case MTD_BLOB_LOCATOR_ID :
                        case MTD_CLOB_ID :
                        case MTD_CLOB_LOCATOR_ID :
                        case MTD_GEOMETRY_ID :
                        case MTD_NULL_ID :
                        case MTD_LIST_ID :
                        case MTD_NONE_ID :
                        case MTS_FILETYPE_ID :
                        case MTD_BINARY_ID :
                        case MTD_INTERVAL_ID :
                        case MTD_BOOLEAN_ID :
                        default : // ����� ���� Ÿ�� �Ǵ� rowtype id
                            sqlInfo.setSourceInfo( aStatement,
                                                   &(sColumn->namePos) );
                            IDE_RAISE(ERR_NOT_SUPPORT_TYPE);
                            break;
                    }

                    // 1-2. Column �� ����ũ��(size)�� smOID ���� ������ �ȵȴ�.
                    //    �켱 compression �ϸ� ������ ���� ������ �� ����ϰԵǰ�
                    //    in-place update �ÿ� logging ������ �߻��ϰ� �ȴ�.
                    if( sColumn->basicInfo->column.size < ID_SIZEOF(smOID) )
                    {
                        sqlInfo.setSourceInfo( aStatement,
                                               &(sColumn->namePos) );
                        IDE_RAISE( ERR_NOT_SUPPORT_TYPE );
                    }

                    // 1-3. Compression column ���� ��Ÿ������ flag ����
                    sColumn->basicInfo->column.flag &= ~SMI_COLUMN_COMPRESSION_MASK;
                    sColumn->basicInfo->column.flag |= SMI_COLUMN_COMPRESSION_TRUE;

                    break;
                }
            }

            // 1. ������ �̵��ߴٴ� ���� �� ã�Ҵٴ� �ǹ��̴�.
            if( sColumn == NULL )
            {
                sqlInfo.setSourceInfo( aStatement,
                                       &(sCompColumn->namePos) );
                IDE_RAISE( ERR_NOT_EXIST_COLUMN_NAME );
            }
        }

        /* BUG-45641 disk partitioned table�� ���� �÷��� �߰��ϴٰ� �����ϴµ�, memory partitioned table ������ ���ɴϴ�. */
        // 2. ���� ���̺��� tablespace �˻�
        //   Compression �÷��� volatile ���̺��� ����� �� ����.
        IDE_TEST_RAISE( ( smiTableSpace::isMemTableSpaceType( aType ) != ID_TRUE ) &&
                        ( smiTableSpace::isDiskTableSpaceType( aType ) != ID_TRUE ),
                        ERR_NOT_SUPPORTED_TABLESPACE_TYPE );

        /* 3. PROJ-2334 PMT dictionary compress�� memory/disk tablespace�� ����
         * memory/disk partitioned table�� ���� ����� �Ǿ� ���� �ʾ� ���� ��� */
        if ( aParseTree->tableInfo != NULL )
        {
            IDE_TEST_RAISE( aParseTree->tableInfo->tablePartitionType == QCM_PARTITIONED_TABLE,
                            ERROR_UBABLE_TO_COMPRESS_PARTITION );
        }
        else
        {
            IDE_TEST_RAISE( aParseTree->partTable != NULL,
                            ERROR_UBABLE_TO_COMPRESS_PARTITION );
        }
    }
    else
    {
        // Nothing to do.
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_NOT_SUPPORTED_TABLESPACE_TYPE)
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDB_COMPRESSION_NOT_SUPPORTED_TABLESPACE ) );
    }
    IDE_EXCEPTION(ERR_NOT_EXIST_COLUMN_NAME)
    {
        (void)sqlInfo.init( aStatement->qmeMem );
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QCM_NOT_EXIST_COLUMN_NAME,
                                  sqlInfo.getErrMessage() ) );
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION(ERR_NOT_SUPPORT_TYPE)
    {
        (void)sqlInfo.init( aStatement->qmeMem );
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDB_COMPRESSION_NOT_SUPPORTED_DATATYPE,
                                  sqlInfo.getErrMessage() ) );
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION(ERROR_UBABLE_TO_COMPRESS_PARTITION)
    {
        /* BUG-45641 disk partitioned table�� ���� �÷��� �߰��ϴٰ� �����ϴµ�, memory partitioned table ������ ���ɴϴ�. */
        /* PROJ-2429 Dictionary based data compress for on-disk DB */
        if ( smiTableSpace::isDiskTableSpaceType( aType ) == ID_TRUE )
        {
            IDE_SET( ideSetErrorCode( qpERR_ABORT_QDB_UNABLE_TO_COMPRESS_DISK_PARTITION ) );
        }
        else
        {
            IDE_SET( ideSetErrorCode( qpERR_ABORT_QDB_UNABLE_TO_COMPRESS_MEM_PARTITION ) );
        }
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qcmDictionary::rebuildDictionaryTable( qcStatement        * aStatement,
                                              qcmTableInfo       * aOldTableInfo,
                                              qcmTableInfo       * aNewTableInfo,
                                              qcmDictionaryTable * aDicTable )
{
/***********************************************************************
 *
 * Description :
 *    Data table �� ���ڵ带 �о� dictionary table �� ���� �����Ѵ�.
 *    ��, data table �� �����ϴ� ������ dictionary table �� �籸���Ѵ�.
 *
 *    �Ʒ��Լ����� ȣ���.
 *    qdbAlter::executeReorganizeColumn()
 *
 * Implementation :
 *    1. Open data table cursor
 *    2. Read row from data table
 *      (for each dictionary table)
 *        2-1. Open dictionary table cursor
 *        2-2. Insert value to dictionary table
 *        2-3. Close dictionary table cursor
 *      2-4. Update OID of data table
 *    3. Close data table cursor
 *
 ***********************************************************************/

    const void            * sOldRow;
    smiValue              * sNewRow;
    smiValue              * sDataRow;
    mtcColumn             * sSrcCol         = NULL;
    mtcColumn             * sDestCol        = NULL;
    void                  * sValue;
    SInt                    sStage          = 0;
    iduMemoryStatus         sQmxMemStatus;
    scGRID                  sRowGRID;
    void                  * sInsRow;
    scGRID                  sInsGRID;
    smiFetchColumnList    * sSrcFetchColumnList;
    UInt                    sStoringSize    = 0;
    void                  * sStoringValue;
    smiTableCursor          sDstTblCursor;
    smiCursorProperties     sDstCursorProperty;
    smiTableCursor          sDataTblCursor;
    smiCursorProperties     sDataCursorProperty;
    smiCursorProperties   * sDicCursorProperty;
    qcmDictionaryTable    * sDicIter;
    qcmColumn             * sColumn         = NULL;
    qcmColumn             * sDataColumns;
    mtcColumn             * sOldDataColumns = NULL;
    UInt                    i;
    UInt                    sDicTabCount;
    void                  * sRow;
    smiColumnList         * sUpdateColList;
    smiColumnList         * sPrev           = NULL;

    void                  * sTableHandle;

    SChar                 * sNullOID        = NULL;
    scGRID                  sDummyGRID;     
    SChar                 * sNullColumn     = NULL;
    UInt                    sDiskRowSize    = 0;

    qdbAnalyzeUsage       * sAnalyzeUsage   = NULL;

    //-----------------------------------------
    // Data table �� ���� smiColumnList ����
    //-----------------------------------------

    for( sDicIter = aDicTable, sDicTabCount = 0;
         sDicIter != NULL;
         sDicIter = sDicIter->next )
    {
        sDicTabCount++;
    }

    // Data table �� fetch column list ������ ���� qcmColumn ����
    IDE_TEST( aStatement->qmxMem->cralloc( ID_SIZEOF(qcmColumn) * sDicTabCount,
                                           (void **) & sDataColumns )
              != IDE_SUCCESS );

    for ( sDicIter = aDicTable, i = 0;
          sDicIter != NULL;
          sDicIter = sDicIter->next, i++ )
    {
        idlOS::memcpy( &(sDataColumns[i]), sDicIter->dataColumn, ID_SIZEOF(qcmColumn) );
    }

    /* PROJ-2465 Tablespace Alteration for Table
     *  Old Table�� Compressed Column�� �����ϰ�, mtc::value()���� ����� offset�� New Table�� ������ �����Ѵ�.
     */
    IDU_FIT_POINT( "qcmDictionary::rebuildDictionaryTable::alloc::sOldDataColumns",
                   idERR_ABORT_InsufficientMemory );

    IDE_TEST( QC_QMX_MEM(aStatement)->alloc( ID_SIZEOF(mtcColumn) * sDicTabCount,
                                             (void **) & sOldDataColumns )
              != IDE_SUCCESS );

    for ( sDicIter = aDicTable, i = 0;
          sDicIter != NULL;
          sDicIter = sDicIter->next, i++ )
    {
        sColumn = qdbCommon::findColumnInColumnList( aOldTableInfo->columns,
                                                     sDicIter->dataColumn->basicInfo->column.id );

        IDE_TEST_RAISE( sColumn == NULL, ERR_COLUMN_NOT_FOUND );

        idlOS::memcpy( &(sOldDataColumns[i]), sColumn->basicInfo, ID_SIZEOF(mtcColumn) );
        sOldDataColumns[i].column.offset = sDataColumns[i].basicInfo->column.offset;
    }

    // Data table �� OID �� update �ϱ� ���� smiValue �迭
    IDE_TEST(aStatement->qmxMem->alloc(ID_SIZEOF(smiValue) * sDicTabCount,
                                       (void**)&sDataRow)
             != IDE_SUCCESS);

    for( i = 0; i < sDicTabCount; i++ )
    {
        IDE_TEST(aStatement->qmxMem->alloc(ID_SIZEOF(smOID) * sDicTabCount,
                                           (void**)&(sDataRow[i].value))
                 != IDE_SUCCESS);
    }

    // Fetch column list �� ���� ���� �Ҵ�
    IDE_TEST(aStatement->qmxMem->alloc(ID_SIZEOF(smiFetchColumnList) * sDicTabCount,
                                       (void**)&sSrcFetchColumnList)
             != IDE_SUCCESS);

    // Update column list �� ���� ���� �Ҵ�
    IDE_TEST(aStatement->qmxMem->alloc(ID_SIZEOF(smiColumnList) * sDicTabCount,
                                       (void**)&sUpdateColList)
                 != IDE_SUCCESS);

    /* PROJ-2465 Tablespace Alteration for Table */
    if ( QCM_TABLE_TYPE_IS_DISK( aNewTableInfo->tableFlag ) == ID_TRUE )
    {
        IDE_TEST( qdbCommon::getDiskRowSize( aNewTableInfo->tableHandle,
                                             & sDiskRowSize )
                  != IDE_SUCCESS );

        IDU_FIT_POINT( "qcmDictionary::rebuildDictionaryTable::alloc::sOldRow",
                       idERR_ABORT_InsufficientMemory );

        IDE_TEST( QC_QMX_MEM(aStatement)->alloc( sDiskRowSize,
                                                 (void **) & sOldRow )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    //-------------------------------------------
    // open data table. ( select + update )
    // cursor property ����.
    //
    // PROJ-1705
    // fetch column list ������ �����ؼ� sm���� ������.
    //-------------------------------------------

    sDataTblCursor.initialize();
    SMI_CURSOR_PROP_INIT_FOR_FULL_SCAN( &sDataCursorProperty, aStatement->mStatistics );
    sDataCursorProperty.mLockWaitMicroSec = 0;

    // Data table �� ���� fetch column list
    IDE_TEST( qdbCommon::makeFetchColumnList( QC_PRIVATE_TMPLATE(aStatement),
                                              sDicTabCount,
                                              sDataColumns,
                                              ID_FALSE,   // alloc smiColumnList
                                              & sSrcFetchColumnList )
          != IDE_SUCCESS );

    // Data table �� ���� update column list
    for( sDicIter = aDicTable, i = 0;
         sDicIter != NULL;
         sDicIter = sDicIter->next, i++ )
    {
        sUpdateColList[i].column = &(sDicIter->dataColumn->basicInfo->column);
        sUpdateColList[i].next   = NULL;

        if( sPrev == NULL )
        {
            sPrev = &(sUpdateColList[i]);
        }
        else
        {
            sPrev->next = &(sUpdateColList[i]);
            sPrev = sPrev->next;
        }
    }

    sDataCursorProperty.mFetchColumnList = sSrcFetchColumnList;

    IDE_TEST(
        sDataTblCursor.open(
            QC_SMI_STMT( aStatement ),
            aNewTableInfo->tableHandle,
            NULL,
            smiGetRowSCN( aNewTableInfo->tableHandle ),
            sUpdateColList,
            smiGetDefaultKeyRange(),
            smiGetDefaultKeyRange(),
            smiGetDefaultFilter(),
            SMI_LOCK_TABLE_EXCLUSIVE|
            SMI_TRAVERSE_FORWARD|
            SMI_PREVIOUS_DISABLE,
            SMI_UPDATE_CURSOR,
            & sDataCursorProperty )
        != IDE_SUCCESS );
    sStage = 1;

    //-------------------------------------------
    // open destination table. ( insert )
    // cursor property ����.
    //-------------------------------------------

    // smiTableCursor �� �� cursor property �� dictionary table ���� �����.
    IDE_TEST( aStatement->qmxMem->cralloc( ID_SIZEOF(smiCursorProperties) * sDicTabCount,
                                           (void **) & sDicCursorProperty )
              != IDE_SUCCESS );

    sDstTblCursor.initialize();

    SMI_CURSOR_PROP_INIT_FOR_FULL_SCAN( &sDstCursorProperty, aStatement->mStatistics );

    /* ------------------------------------------------
     * make new row values
     * ----------------------------------------------*/
    IDE_TEST(aStatement->qmxMem->alloc(ID_SIZEOF(smiValue) * sDicTabCount,
                                       (void**)&sNewRow)
             != IDE_SUCCESS);

    // move..
    IDE_TEST(sDataTblCursor.beforeFirst() != IDE_SUCCESS);
    IDE_TEST(sDataTblCursor.readRow(&sOldRow,
                                    &sRowGRID, SMI_FIND_NEXT)
             != IDE_SUCCESS);

    /* PROJ-2465 Tablespace Alteration for Table
     *  - aDicTable ����Ʈ���� ù��° dictionaryTableInfo������ sAnalyzeUsage�� �ʱ�ȭ�Ѵ�.
     *  - ������ Table�� �ٸ� Dictionary�� Row�� ������ TBS�� ����ȴ�.
     */
    IDE_TEST( qdbCommon::initializeAnalyzeUsage( aStatement,
                                                 aNewTableInfo,
                                                 aDicTable->dictionaryTableInfo,
                                                 &( sAnalyzeUsage ) )
              != IDE_SUCCESS );

    while (sOldRow != NULL)
    {
        /* PROJ-2465 Tablespace Alteration for Table */
        IDE_TEST( qdbCommon::checkAndSetAnalyzeUsage( aStatement,
                                                      sAnalyzeUsage )
                  != IDE_SUCCESS );

        // To Fix PR-11704
        // ���ڵ� �Ǽ��� ����Ͽ� �޸𸮰� �������� �ʵ��� �ؾ� ��.
        // Memory ������ ���Ͽ� ���� ��ġ ���
        IDE_TEST( aStatement->qmxMem->getStatus(&sQmxMemStatus)
                  != IDE_SUCCESS);

        //------------------------------------------
        // INSERT�� ����
        //------------------------------------------

        for( sDicIter = aDicTable, i = 0;
             sDicIter != NULL;
             sDicIter = sDicIter->next, i++ )
        {
            sSrcCol  = &(sOldDataColumns[i]);
            sDestCol = sDicIter->dictionaryTableInfo->columns->basicInfo;

            sTableHandle = sDicIter->dictionaryTableInfo->tableHandle;

            // BUGBUG
            IDE_TEST( sDstTblCursor.open(
                            QC_SMI_STMT( aStatement ),
                            sTableHandle,
                            NULL,
                            smiGetRowSCN( sTableHandle ),
                            NULL,
                            smiGetDefaultKeyRange(),
                            smiGetDefaultKeyRange(),
                            smiGetDefaultFilter(),
                            SMI_LOCK_WRITE|
                            SMI_TRAVERSE_FORWARD|
                            SMI_PREVIOUS_DISABLE,
                            SMI_INSERT_CURSOR,
                            & sDstCursorProperty )
                      != IDE_SUCCESS );

            sStage = 2;

            // Data table ���� ���� �д´�.
            sValue = (void *) mtc::value( sSrcCol,
                                          sOldRow,
                                          MTD_OFFSET_USE );

            IDE_TEST( qdbCommon::mtdValue2StoringValue(
                          sDestCol,
                          sSrcCol,
                          (void*)sValue,
                          &sStoringValue )
                      != IDE_SUCCESS );
            sNewRow[i].value = sStoringValue;

            IDE_TEST( qdbCommon::storingSize( sDestCol,
                                              sSrcCol,
                                              (void*)sValue,
                                              &sStoringSize )
                      != IDE_SUCCESS );
            sNewRow[i].length = sStoringSize;

            // Data table ���� ���� ��(sNewRow)�� dictionary table �� �ְ�,
            // �� OID �� �ٽ� data table �� update �ϱ� ���� �޴´�.(sDataRow)
            IDE_TEST( sDstTblCursor.insertRowWithIgnoreUniqueError(
                          &sDstTblCursor,
                          (smcTableHeader *)SMI_MISC_TABLE_HEADER( sTableHandle ), //(smcTableHeader*)
                          &(sNewRow[i]),
                          (smOID*)sDataRow[i].value,
                          &sRow )
                      != IDE_SUCCESS );

            sStage = 1;

            IDE_TEST( sDstTblCursor.close() != IDE_SUCCESS );

            sDataRow[i].length = ID_SIZEOF(smOID);
        }

        IDE_TEST(sDataTblCursor.updateRow(sDataRow,
                                         & sInsRow,
                                         & sInsGRID)
                 != IDE_SUCCESS);

        // To Fix PR-11704
        // Memory ������ ���� Memory �̵�
        IDE_TEST( aStatement->qmxMem->setStatus(&sQmxMemStatus)
                  != IDE_SUCCESS);

        IDE_TEST(sDataTblCursor.readRow(&sOldRow, &sRowGRID, SMI_FIND_NEXT)
                 != IDE_SUCCESS);
    }

    sStage = 0;
    IDE_TEST(sDataTblCursor.close() != IDE_SUCCESS);

    /* PROJ-2465 Tablespace Alteration for Table */
    if ( QCM_TABLE_TYPE_IS_DISK( aNewTableInfo->tableFlag ) != ID_TRUE )
    {
        //------------------------------------------
        // Update Null row
        //------------------------------------------
        (void)smiGetTableNullRow( aNewTableInfo->tableHandle, (void**)&sNullOID, &sDummyGRID );

        for ( sDicIter = aDicTable, i = 0;
              ( sDicIter != NULL ) && ( i < sDicTabCount );
              sDicIter = sDicIter->next, i++ )
        {
            sTableHandle = sDicIter->dictionaryTableInfo->tableHandle;
            sNullColumn  = sNullOID + sDicIter->dataColumn->basicInfo->column.offset;

            idlOS::memcpy( sNullColumn,
                           &(SMI_MISC_TABLE_HEADER(sTableHandle)->mNullOID),
                           ID_SIZEOF(smOID) );
        }
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_COLUMN_NOT_FOUND )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qcmDictionary::rebuildDictionaryTable",
                                  "Column Not Found" ) );
    }
    IDE_EXCEPTION_END;

    switch ( sStage )
    {
        case 2:
            // Close cursor for dictionary table( sStage > 1 ) 
            (void)sDstTblCursor.close();
            /* fall through */
        case 1:
            // Close cursor for data table (sStage >= 1)
            (void)sDataTblCursor.close();
            break;
        case 0:
        default:
            break;
    }

    return IDE_FAILURE;
}

IDE_RC qcmDictionary::removeDictionaryTable( qcStatement  * aStatement,
                                             qcmColumn    * aColumn )
{
    qcmTableInfo  * sDicTableInfo;

    IDE_DASSERT( aColumn->basicInfo->column.mDictionaryTableOID != SM_NULL_OID );
    
    // smiColumn.mDictionaryTableOID ���
    sDicTableInfo = (qcmTableInfo *)smiGetTableRuntimeInfoFromTableOID(
        aColumn->basicInfo->column.mDictionaryTableOID );

    IDE_DASSERT( sDicTableInfo != NULL );

    IDE_TEST( dropDictionaryTable( aStatement,
                                   sDicTableInfo )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qcmDictionary::removeDictionaryTableWithInfo( qcStatement  * aStatement,
                                                     qcmTableInfo * aTableInfo )
{
    IDE_TEST( dropDictionaryTable( aStatement,
                                   aTableInfo )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qcmDictionary::updateCompressionTableSpecMeta( qcStatement * aStatement,
                                                      UInt          aOldDicTableID,
                                                      UInt          aNewDicTableID )
{
/***********************************************************************
 *
 * Description :
 *      PROJ-2264
 *      executeReorganizeColumn ���κ��� ȣ��
 *      Data table-Dictionary table �� ���� ������ �����Ѵ�.
 *
 * Implementation :
 *      1. SYS_COMPRESSION_TABLES_ ��Ÿ ���̺��� ���� ���� ����
 *
 ***********************************************************************/

    SChar         * sSqlStr;
    vSLong          sRowCnt;

    IDE_TEST( STRUCT_ALLOC_WITH_SIZE( aStatement->qmxMem,
                                      SChar,
                                      QD_MAX_SQL_LENGTH,
                                      &sSqlStr )
              != IDE_SUCCESS);

    // SYS_COMPRESSION_TABLES_ �� �����Ѵ�.
    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                     "UPDATE SYS_COMPRESSION_TABLES_ SET DIC_TABLE_ID = "
                     "INTEGER'%"ID_INT32_FMT"' "
                     "WHERE DIC_TABLE_ID = "
                     "INTEGER'%"ID_INT32_FMT"'",
                     aNewDicTableID,
                     aOldDicTableID );

    IDE_TEST( qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                 sSqlStr,
                                 &sRowCnt )
              != IDE_SUCCESS );

    IDE_TEST_RAISE( sRowCnt != 1, ERR_META_CRASH );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_META_CRASH );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDB_META_CRASH ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qcmDictionary::updateColumnIDInCompressionTableSpecMeta(
                          qcStatement * aStatement,
                          UInt          aDicTableID,
                          UInt          aNewColumnID )
{
/***********************************************************************
 *
 * Description :
 *      PROJ-2264
 *      executeDropCol ���κ��� ȣ��
 *      Data table-Dictionary table �� ���� ������ �����Ѵ�.
 *
 * Implementation :
 *      1. SYS_COMPRESSION_TABLES_ ��Ÿ ���̺��� ���� ���� ����
 *
 ***********************************************************************/

    SChar         * sSqlStr;
    vSLong          sRowCnt;

    IDE_TEST( STRUCT_ALLOC_WITH_SIZE( aStatement->qmxMem,
                                      SChar,
                                      QD_MAX_SQL_LENGTH,
                                      &sSqlStr )
              != IDE_SUCCESS);

    // SYS_COMPRESSION_TABLES_ �� �����Ѵ�.
    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                     "UPDATE SYS_COMPRESSION_TABLES_ SET COLUMN_ID = "
                     "INTEGER'%"ID_INT32_FMT"' "
                     "WHERE DIC_TABLE_ID = INTEGER'%"ID_INT32_FMT"'",
                     aNewColumnID,
                     aDicTableID );

    IDE_TEST( qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                 sSqlStr,
                                 &sRowCnt )
              != IDE_SUCCESS );

    IDE_TEST_RAISE( sRowCnt != 1, ERR_META_CRASH );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_META_CRASH );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDB_META_CRASH ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qcmDictionary::dropDictionaryTable( qcStatement  * aStatement,
                                    qcmTableInfo * aTableInfo )
{
/***********************************************************************
 *
 * Description : PROJ-2253 Dictionary table
 *
 * Implementation : Dictionary table �� �����Ѵ�.
 *
 ***********************************************************************/

    qcmTableInfo  * sTableInfo;
    UInt            sTableID;

    sTableInfo = aTableInfo;
    sTableID   = aTableInfo->tableID;

    // Constraint, Index, Table �� ���õ� ��Ÿ ���̺��� ���� ���ڵ带 �����Ѵ�.
    IDE_TEST(qdd::deleteConstraintsFromMeta(aStatement, sTableID)
             != IDE_SUCCESS);

    IDE_TEST(qdd::deleteIndicesFromMeta(aStatement, sTableInfo)
             != IDE_SUCCESS);

    IDE_TEST(qdd::deleteTableFromMeta(aStatement, sTableID)
             != IDE_SUCCESS);

    // �� �ܿ� Priviledge, Related view, Comment ���� ��Ÿ ���̺��� �����ؾ� �Ѵ�.
    // ������ dictionary table �� DDL �� �����Ǿ� ��Ÿ�� ���ڵ尡 ������ �ʴ´�.
    // ���� ������ �����Ѵ�.

    // SM ���� dictionary table ����
    IDE_TEST( smiTable::dropTable( QC_SMI_STMT( aStatement ),
                                   sTableInfo->tableHandle,
                                   SMI_TBSLV_DDL_DML )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qcmDictionary::createDictionaryTable( qcStatement         * aStatement,
                                      UInt                  aUserID,
                                      qcmColumn           * aColumn,
                                      UInt                  aColumnCount,
                                      scSpaceID             aTBSID,
                                      ULong                 aMaxRows,
                                      UInt                  aInitFlagMask,
                                      UInt                  aInitFlagValue,
                                      UInt                  aParallelDegree,
                                      qcmDictionaryTable ** aDicTable )
{
/***********************************************************************
 *
 * Description : PROJ-2264 Dictionary table
 *
 * Implementation :
 *     dictionary table�� �����Ѵ�.
 *
 ***********************************************************************/

    qcmTableInfo          * sTableInfo = NULL;
    void                  * sTableHandle;
    smSCN                   sSCN;
    UInt                    sTableID;
    smOID                   sTableOID;
    qcmColumn               sColumn;
    smiColumnList           sColumnListAtKey;
    mtcColumn               sMtcColumn;
    SChar                   sObjName[ QC_MAX_OBJECT_NAME_LEN + 1 ];
    UInt                    sIndexID;
    UInt                    sIndexType;
    const void            * sIndexHandle;
    UInt                    sIndexFlag = 0;
    UInt                    sBuildFlag = 0;
    qcmDictionaryTable    * sDicIter;
    qcmDictionaryTable    * sDicInfo;
    qcNamePosition          sTableNamePos;
    SChar                   sTableName[QC_MAX_OBJECT_NAME_LEN + 1] = { 0, };
    UInt                    sDictionaryID;
    scSpaceID               sDictionaryTableTBS;
    smiSegAttr              sSegAttr;
    smiSegStorageAttr       sSegStoAttr;

    // ---------------------------------------------------------------
    // Physical Attribute for Memory Tablespace
    // ---------------------------------------------------------------
    idlOS::memset( &sSegAttr, 0x00, ID_SIZEOF(smiSegAttr) );
    idlOS::memset( &sSegStoAttr, 0x00, ID_SIZEOF(smiSegStorageAttr) );

    // ---------------------------------------------------------------
    // Column ���� ����
    // ---------------------------------------------------------------

    // qcmColumn deep copy
    idlOS::memcpy( &sColumn, aColumn, ID_SIZEOF(qcmColumn) );
    idlOS::memcpy( &sMtcColumn, aColumn->basicInfo, ID_SIZEOF(mtcColumn) );
    sColumn.basicInfo = &sMtcColumn;
    sColumn.next = NULL;

    // smiColumn ������ �����Ѵ�.
    sMtcColumn.column.offset = smiGetRowHeaderSize( SMI_TABLE_MEMORY );
    sMtcColumn.column.value = NULL;
    sMtcColumn.column.flag &= ~SMI_COLUMN_COMPRESSION_MASK;
    sMtcColumn.column.flag |= SMI_COLUMN_COMPRESSION_FALSE;

    // Recreate ��, Dictionary Table�� Column�� Dictionary Table OID�� ����� �Ѵ�.
    SMI_INIT_SCN( & sMtcColumn.column.mDictionaryTableOID );

    //PROJ-2429 Dictionary based data compress for on-disk DB
    if ( smiTableSpace::isDiskTableSpace( aTBSID ) == ID_TRUE )
    {
        //column���� ������ �޸𸮷� �����Ѵ�.
        sMtcColumn.column.flag &= ~SMI_COLUMN_STORAGE_MASK;
        sMtcColumn.column.flag |= SMI_COLUMN_STORAGE_MEMORY;

        sMtcColumn.column.flag &= ~SMI_COLUMN_COMPRESSION_TARGET_MASK;
        sMtcColumn.column.flag |= SMI_COLUMN_COMPRESSION_TARGET_DISK;

        //disk column���� �����ȴ� flag�� ���� �Ѵ�.
        sMtcColumn.column.flag &= ~SMI_COLUMN_DATA_STORE_DIVISIBLE_MASK;

        //disk table�� dictionary table�� SMI_ID_TABLESPACE_SYSTEM_MEMORY_DATA�� ���� �ȴ�.
        sDictionaryTableTBS        = SMI_ID_TABLESPACE_SYSTEM_MEMORY_DATA;
        sMtcColumn.column.colSpace = SMI_ID_TABLESPACE_SYSTEM_MEMORY_DATA;
    }
    else
    {
        //memory table�� dictionary table�� �ش� memory TBS�� ���� �ȴ�.
        sDictionaryTableTBS = aTBSID;

        sMtcColumn.column.flag &= ~SMI_COLUMN_COMPRESSION_TARGET_MASK;
        sMtcColumn.column.flag |= SMI_COLUMN_COMPRESSION_TARGET_MEMORY;
    } 

    // ---------------------------------------------------------------
    // TABLE ����
    // ---------------------------------------------------------------

    // Create table
    IDE_TEST( qcm::getNextTableID( aStatement, &sTableID ) != IDE_SUCCESS );

    IDE_TEST( qdbCommon::createTableOnSM( aStatement,
                                          &sColumn,
                                          aUserID,
                                          sTableID,
                                          aMaxRows,
                                          sDictionaryTableTBS,
                                          sSegAttr,
                                          sSegStoAttr,
                                          aInitFlagMask,
                                          aInitFlagValue,
                                          aParallelDegree,
                                          &sTableOID )
              != IDE_SUCCESS );

    // �ߺ��� �����ϵ��� dictionary �� sequencial �� ID ���� ���δ�.
    IDE_TEST( qcm::getNextDictionaryID( aStatement, &sDictionaryID ) != IDE_SUCCESS );

    // Dictionary table name �� �����.
    idlOS::snprintf( sTableName, QC_MAX_OBJECT_NAME_LEN + 1,
                     "DIC_%"ID_vULONG_FMT"_%"ID_UINT32_FMT,
                     sTableOID,
                     sDictionaryID );

    sTableNamePos.stmtText = sTableName;
    sTableNamePos.offset   = 0;
    sTableNamePos.size     = idlOS::strlen(sTableName);

    // BUG-37144 QDV_HIDDEN_DICTIONARY_TABLE_TRUE add
    IDE_TEST( qdbCommon::insertTableSpecIntoMeta( aStatement,
                                                  ID_FALSE,
                                                  QDV_HIDDEN_DICTIONARY_TABLE_TRUE,
                                                  sTableNamePos,
                                                  aUserID,
                                                  sTableOID,
                                                  sTableID,
                                                  aColumnCount,
                                                  aMaxRows,
                                                  /* PROJ-2359 Table/Partition Access Option */
                                                  QCM_ACCESS_OPTION_READ_WRITE,
                                                  sDictionaryTableTBS,
                                                  sSegAttr,
                                                  sSegStoAttr,
                                                  QCM_TEMPORARY_ON_COMMIT_NONE,
                                                  aParallelDegree,      // PROJ-1071
                                                  QCM_SHARD_FLAG_TABLE_NONE ) // TASK-7307
              != IDE_SUCCESS );

    IDE_TEST( qdbCommon::insertColumnSpecIntoMeta( aStatement,
                                                   aUserID,
                                                   sTableID,
                                                   &sColumn,
                                                   ID_FALSE )
              != IDE_SUCCESS );

    /* Table�� ���������Ƿ�, Lock�� ȹ���Ѵ�. */
    IDE_TEST( qcm::makeAndSetQcmTableInfo( QC_SMI_STMT( aStatement ),
                                           sTableID,
                                           sTableOID )
              != IDE_SUCCESS );

    IDE_TEST( qcm::getTableInfoByID( aStatement,
                                     sTableID,
                                     &sTableInfo,
                                     &sSCN,
                                     &sTableHandle )
              != IDE_SUCCESS );

    IDE_TEST( qcm::validateAndLockTable( aStatement,
                                         sTableHandle,
                                         sSCN,
                                         SMI_TABLE_LOCK_X )
              != IDE_SUCCESS );

    // ---------------------------------------------------------------
    // Unique Index ����
    // ---------------------------------------------------------------
    sColumnListAtKey.column = (smiColumn*) &sMtcColumn;
    sColumnListAtKey.next = NULL;

    IDE_TEST(qcm::getNextIndexID(aStatement, &sIndexID) != IDE_SUCCESS);

    idlOS::snprintf( sObjName , ID_SIZEOF(sObjName),
                     "%sIDX_ID_%"ID_INT32_FMT"",
                     QC_SYS_OBJ_NAME_HEADER,
                     sIndexID );

    sIndexType = mtd::getDefaultIndexTypeID( sColumn.basicInfo->module );

    sIndexFlag = SMI_INDEX_UNIQUE_ENABLE|
                 SMI_INDEX_TYPE_NORMAL|
                 SMI_INDEX_USE_ENABLE|
                 SMI_INDEX_PERSISTENT_DISABLE;

    sBuildFlag = SMI_INDEX_BUILD_DEFAULT|
                  SMI_INDEX_BUILD_UNCOMMITTED_ROW_DISABLE;

    if( smiTable::createIndex(aStatement->mStatistics,
                               QC_SMI_STMT( aStatement ),
                               sDictionaryTableTBS,
                               sTableHandle,
                               (SChar*)sObjName,
                               sIndexID,
                               sIndexType,
                               &sColumnListAtKey,
                               sIndexFlag,
                               1, // parallelDegree
                               sBuildFlag,
                               sSegAttr,
                               sSegStoAttr,
                               0, /* PROJ-2433 : dictionary table�� �Ϲ� index ��� */
                               &sIndexHandle)
         != IDE_SUCCESS)
    {
        // To fix BUG-17762
        // ���� �����ڵ忡 ���� ���� ȣȯ���� ����Ͽ� SM ������
        // QP ������ ��ȯ�Ѵ�.
        if( ideGetErrorCode() == smERR_ABORT_NOT_NULL_VIOLATION )
        {
            IDE_CLEAR();
            IDE_RAISE( ERR_HAS_NULL_VALUE );
        }
        else
        {
            IDE_TEST( ID_TRUE );
        }
    }

    IDE_TEST(qdx::insertIndexIntoMeta(aStatement,
                                      sDictionaryTableTBS,
                                      aUserID,
                                      sTableID,
                                      sIndexID,
                                      sObjName,
                                      sIndexType,
                                      ID_TRUE,  // Unique
                                      1,        // Column count
                                      ID_TRUE,
                                      ID_FALSE, // IsPartitionedIndex
                                      0,
                                      sIndexFlag)
             != IDE_SUCCESS);

    IDE_TEST( qdx::insertIndexColumnIntoMeta(
                  aStatement,
                  aUserID,
                  sIndexID,
                  sColumn.basicInfo->column.id,
                  0,                                   // Column index
                  ( ( (sColumn.basicInfo->column.flag &
                       SMI_COLUMN_ORDER_MASK) == SMI_COLUMN_ORDER_ASCENDING )
                    ? ID_TRUE : ID_FALSE ),
                  sTableID)
              != IDE_SUCCESS);

    (void)qcm::destroyQcmTableInfo( sTableInfo );
    sTableInfo = NULL;

    /* Index ������ ���� Meta Cache�� �����Ѵ�. */
    IDE_TEST( qcm::makeAndSetQcmTableInfo( QC_SMI_STMT( aStatement ),
                                           sTableID,
                                           sTableOID )
              != IDE_SUCCESS );

    IDE_TEST( qcm::getTableInfoByID( aStatement,
                                     sTableID,
                                     &sTableInfo,
                                     &sSCN,
                                     &sTableHandle )
              != IDE_SUCCESS );

    // ---------------------------------------------------------------
    // smiColumn �� Dictionary Table OID �� �����Ѵ�.
    // ---------------------------------------------------------------
    aColumn->basicInfo->column.mDictionaryTableOID = smiGetTableId( sTableHandle );

    // ---------------------------------------------------------------
    // Table ������ �Ϸ�� �Ŀ� SYS_COMPRESSION_TABLES_ �� �ֱ� ����
    // Column name �� dictionary table name �� ����Ʈ�� �����.
    // ---------------------------------------------------------------
    IDE_TEST( STRUCT_CRALLOC( aStatement->qmxMem,
                              qcmDictionaryTable,
                              (void**) &sDicInfo ) != IDE_SUCCESS );
    QCM_DICTIONARY_TABLE_INIT( sDicInfo );

    sDicInfo->dictionaryTableInfo = sTableInfo;
    sDicInfo->dataColumn          = aColumn;
    sDicInfo->next                = NULL;

    if( *aDicTable == NULL )
    {
        *aDicTable = sDicInfo;
    }
    else
    {
        // List �� ���� �ڿ� �ٿ��ֱ� ���� ���������� �̵��Ѵ�.
        for( sDicIter = *aDicTable; sDicIter->next != NULL; sDicIter = sDicIter->next );

        sDicIter->next = sDicInfo;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_HAS_NULL_VALUE);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDB_NOTNULL_HAS_NULL));
    }
    IDE_EXCEPTION_END;

    (void)qcm::destroyQcmTableInfo( sTableInfo );

    return IDE_FAILURE;
}

IDE_RC
qcmDictionary::makeDictValueForCompress( smiStatement   * aStatement,
                                         void           * aTableHandle,
                                         void           * aIndexHeader,
                                         smiValue       * aInsertedRow,
                                         void           * aOIDValue )
{
/***********************************************************************
 *
 * Description : PROJ-2397 Compressed Column Table Replication
 
 * Implementation : Dictionary table �� �����Ѵ�.
 *                  Dictionary table �� ������ ���� �ִ� ��� �ش��ϴ� OID �� ��ȯ �޴´�.
 *
 ***********************************************************************/
   
    void              * sMtdValue = NULL;
    smSCN               sSCN;
    smiTableCursor      sCursor;
    smiCursorProperties sCursorProperty;
    UInt                sState = 0;
    void              * sDummyRow = NULL;
    smiValue            sValue4Disk;
    smiValue          * sInsertValue;
    mtcColumn         * sMtcColumn;
    UInt                sNonStoringSize;
    smOID               sOID = SM_NULL_OID;

    IDE_TEST( smiGetTableColumns( aTableHandle,
                                  0,
                                  (const smiColumn**)&sMtcColumn )
              != IDE_SUCCESS );


    /* PROJ-2429 */
    if ( (sMtcColumn->column.flag & SMI_COLUMN_COMPRESSION_TARGET_MASK) 
         == SMI_COLUMN_COMPRESSION_TARGET_DISK )
    {
        aStatement->mFlag &= ~SMI_STATEMENT_CURSOR_MASK;
        aStatement->mFlag |= SMI_STATEMENT_ALL_CURSOR;

        IDE_TEST( mtc::getNonStoringSize( &sMtcColumn->column, &sNonStoringSize ) 
                  != IDE_SUCCESS );

        if ( aInsertedRow->value != NULL )
        {
            sValue4Disk.value  = (void*)((UChar*)aInsertedRow->value - sNonStoringSize); 
            sValue4Disk.length = sMtcColumn->module->actualSize( NULL, sValue4Disk.value );

            sMtdValue = (void*)sValue4Disk.value;
        }
        else
        {
            sValue4Disk.value  = sMtcColumn->module->staticNull; 
            sValue4Disk.length = sMtcColumn->module->nullValueSize();

            sMtdValue = sMtcColumn->module->staticNull;
        }

        sInsertValue = &sValue4Disk;
    }
    else
    {
        sInsertValue = aInsertedRow;

        // BUG-36718
        // Storing value(smiValue.valu)�� index range scan �� ����ϱ� ����
        // �ٽ� mtd value �� ��ȯ�Ѵ�.
        IDE_TEST( qdbCommon::storingValue2MtdValue(
                                sMtcColumn,
                                (void*)( sInsertValue->value ),
                                &sMtdValue )
                  != IDE_SUCCESS );
    }

    // 1. Dictionary table �� ���� �����ϴ��� ����, �� row �� OID �� �����´�.
    IDE_TEST( qcmDictionary::selectDicSmOID( aStatement,
                                             aTableHandle,
                                             aIndexHeader,
                                             sMtdValue,
                                             &sOID )
    != IDE_SUCCESS );

    // 2. Null OID �� ��ȯ�Ǿ����� dictionary table �� ���� ���� ���̴�.
    // Dictionary table �� ���� �ְ� �� row �� OID �� �����´�.
    if ( sOID == SM_NULL_OID )
    {
        sSCN = smiGetRowSCN(aTableHandle);

        // Dictionary table �� insert �ϱ� ���� cursor �� ����.
        sCursor.initialize();
        SMI_CURSOR_PROP_INIT(&sCursorProperty, NULL, NULL);

        IDE_TEST( sCursor.open( aStatement,
                                aTableHandle,
                                NULL, // index
                                sSCN,
                                NULL, // columns
                                smiGetDefaultKeyRange(), // range
                                smiGetDefaultKeyRange(),
                                smiGetDefaultFilter(),
                                SMI_LOCK_WRITE | SMI_TRAVERSE_FORWARD | SMI_PREVIOUS_DISABLE,
                                SMI_INSERT_CURSOR,
                                &sCursorProperty )
                  != IDE_SUCCESS );
        sState = 1;

        // Dictionary table �� ���� �ְ� OID �� ��ȯ �޴´�.
        // ���� ���ü� ����(�ٸ� ���ǿ��� �̹� ���� ���� ���)��
        // unique error �� �߻��ϴ��� ������ ó������ �ʰ�
        // �̹� �����ϴ� row �� OID �� ��ȯ�Ѵ�.
        IDE_TEST( sCursor.insertRowWithIgnoreUniqueError(
                      &sCursor,
                      (smcTableHeader*)SMI_MISC_TABLE_HEADER( aTableHandle ),
                      sInsertValue,
                      &sOID,
                      &sDummyRow )
                  != IDE_SUCCESS );

        sState = 0;
        IDE_TEST( sCursor.close() != IDE_SUCCESS );
    }
    else
    {
        /* selectDicOID �� ������ ����̴�. */
        /* Nothing to do */
    }

    /* BUG-39508 A variable is not considered for memory align 
     * in qcmDictionary::makeDictValueForCompress function */
    idlOS::memcpy( aOIDValue, &sOID, ID_SIZEOF(smOID) );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sState != 0 )
    {
        IDE_PUSH();

        (void)sCursor.close();

        IDE_POP();
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_FAILURE;
}

// PROJ-2397 Compressed Column Table Replication
IDE_RC qcmDictionary::selectDicSmOID( smiStatement   * aSmiStatement,
                                      void           * aTableHandle,
                                      void           * aIndexHeader,
                                      const void     * aValue,
                                      smOID          * aSmOID )
{
/***********************************************************************
 *
 * Description :
 *    Dictionary table ���� smOID �� ���Ѵ�.
 *
 * Implementation :
 *
 *    1. Dictionary table �˻��� ���� range �� �����Ѵ�.
 *    2. Ŀ���� ����.
 *       Dictionary table �� �÷��� 1������ Ư���� ���̺��̹Ƿ� 1��¥�� smiColumnList �� ����Ѵ�.
 *    3. ���ǿ� �´� row �� scGRID �� ��´�.
 *    4. scGRID ���� smOID �� ��´�.
 *
 *
 ***********************************************************************/

    smiTableCursor      sCursor;

    idBool              sIsCursorOpened = ID_FALSE;
    const void        * sRow;

    smiColumnList       sColumnList[QCM_MAX_META_COLUMNS];
    scGRID              sRid; // Disk Table�� ���� Record IDentifier
    smiCursorProperties sCursorProperty;

    smiRange            sRange;
    qtcMetaRangeColumn  sFirstMetaRange;

    void              * sDicTable;
    smOID               sOID;
    mtcColumn         * sFirstMtcColumn;

    // table handle
    sDicTable = aTableHandle;

    IDE_TEST( smiGetTableColumns( sDicTable,
                                  0,
                                  (const smiColumn**)&sFirstMtcColumn )
              != IDE_SUCCESS );

    // Make range
    qcm::makeMetaRangeSingleColumn( &sFirstMetaRange,
                                    sFirstMtcColumn,
                                    (const void*) aValue,
                                    &sRange );

    sCursor.initialize();

    IDE_TEST( smiGetTableColumns( sDicTable,
                                  0,
                                  (const smiColumn**)&sColumnList[0].column )
              != IDE_SUCCESS );

    sColumnList[0].next = NULL;

    SMI_CURSOR_PROP_INIT(&sCursorProperty, NULL, aIndexHeader);

    IDE_TEST( sCursor.open( aSmiStatement,
                            sDicTable,
                            aIndexHeader, // index
                            smiGetRowSCN( sDicTable ),
                            sColumnList, // columns
                            &sRange, // range
                            smiGetDefaultKeyRange(),
                            smiGetDefaultFilter(),
                            SMI_LOCK_READ | SMI_TRAVERSE_FORWARD | SMI_PREVIOUS_DISABLE,
                            SMI_SELECT_CURSOR,
                            &sCursorProperty)
              != IDE_SUCCESS );
    sIsCursorOpened = ID_TRUE;

    IDE_TEST( sCursor.beforeFirst() != IDE_SUCCESS);

    IDE_TEST( sCursor.readRow( &sRow, &sRid, SMI_FIND_NEXT )
              != IDE_SUCCESS );

    if ( sRow != NULL )
    {
        sOID = SMI_CHANGE_GRID_TO_OID(sRid);
     }
    else
    {
        // Data not found in Dictionary table.
        sOID = SM_NULL_OID;
    }

    sIsCursorOpened = ID_FALSE;
    IDE_TEST( sCursor.close() != IDE_SUCCESS );

    *aSmOID = sOID;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sIsCursorOpened == ID_TRUE )
    {
        IDE_PUSH();

        (void)sCursor.close();

        IDE_POP();
    }
    else
    {
        // Cursor already closed.
    }

    return IDE_FAILURE;
}
