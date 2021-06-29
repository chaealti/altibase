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
 * $Id: qdx.cpp 90824 2021-05-13 05:35:21Z minku.kang $
 **********************************************************************/

#include <idl.h>
#include <ide.h>
#include <mtd.h>
#include <smErrorCode.h>
#include <smiMisc.h>
#include <smiTableSpace.h>
#include <qdx.h>
#include <qcm.h>
#include <qcg.h>
#include <qcmCache.h>
#include <qcmUser.h>
#include <qcmView.h>
#include <qcmTableSpace.h>
#include <qcuSqlSourceInfo.h>
#include <qcuTemporaryObj.h>
#include <qdn.h>
#include <qdd.h>
#include <qcmProc.h>
#include <qdbCommon.h>
#include <qdtCommon.h>
#include <qdpPrivilege.h>
#include <qmv.h>
#include <qdpDrop.h>
#include <qdnTrigger.h>
#include <qdbComment.h>
#include <qmvQuerySet.h>
#include <qdbAlter.h>
#include <qmsDefaultExpr.h>
#include <qcpUtil.h>
#include <qcmAudit.h>
#include <qdpRole.h>
#include <sdi.h>

/***********************************************************************
 * PARSE
 **********************************************************************/
// CREATE INDEX

// Parsing in Parser
IDE_RC qdx::parse(qcStatement * aStatement)
{
/***********************************************************************
 *
 * Description :
 *    CREATE INDEX ... �� parsing ����
 *
 * Implementation :
 *    1. �����ϴ� ���̺����� üũ
 *    2. hidden column�� basicInfo ����
 *    2. hidden column���� add column list ����
 *
 ***********************************************************************/

#define IDE_FN "qdx::parse"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qdx::parse"));

    qdIndexParseTree    * sParseTree;
    qdTableParseTree    * sTableParseTree;
    qcmTableInfo        * sTableInfo;
    qcmColumn           * sColumn;
    qcmColumn           * sExprColumnList;
    qcmColumn           * sExprColumn;
    qcmColumn           * sExprColumnInfo;
    qcmColumn           * sNewColumns = NULL;
    UInt                  sTableType;
    UInt                  sFlag;
    idBool                sIsFunctionBasedIndex = ID_FALSE;
    qcuSqlSourceInfo      sqlInfo;
    qmsTableRef         * sTableRef;
    mtcTemplate         * sMtcTemplate;
    mtcColumn           * sMtcColumn;
    UInt                  sColumnFlag;
    UInt                  i;

    sParseTree = (qdIndexParseTree *)aStatement->myPlan->parseTree;
    
    // check table exist.
    if ( qdbCommon::checkTableInfo( aStatement,
                                    sParseTree->userNameOfTable,
                                    sParseTree->tableName,
                                    &(sParseTree->userIDOfTable),
                                    &(sParseTree->tableInfo),
                                    &(sParseTree->tableHandle),
                                    &(sParseTree->tableSCN))
         != IDE_SUCCESS)
    {
        sqlInfo.setSourceInfo( aStatement,
                               & sParseTree->tableName );
        IDE_RAISE(ERR_NOT_EXIST_TABLE);
    }

    IDE_TEST(qcm::lockTableForDDLValidation(aStatement,
                                            sParseTree->tableHandle,
                                            sParseTree->tableSCN)
             != IDE_SUCCESS);

    sTableInfo = sParseTree->tableInfo;
    
    sTableType = sTableInfo->tableFlag & SMI_TABLE_TYPE_MASK;

    // PROJ-2264 Dictionary table
    // Dictionary table �� ���� DDL �� ��� �����Ѵ�.
    IDE_TEST_RAISE( sTableInfo->isDictionary == ID_TRUE,
                    ERR_CANNOT_DDL_DICTIONARY_TABLE );

    /* PROJ-1090 Function-based Index
     *  Function-based Index�� ���, TableRef�� ���Ѵ�.
     */
    for ( sColumn = sParseTree->keyColumns;
          sColumn != NULL;
          sColumn = sColumn->next )
    {
        if ( (sColumn->flag & QCM_COLUMN_HIDDEN_COLUMN_MASK)
             == QCM_COLUMN_HIDDEN_COLUMN_TRUE )
        {
            sIsFunctionBasedIndex = ID_TRUE;
            break;
        }
        else
        {
            /* Nothing to do */
        }
    }
    
    if ( sIsFunctionBasedIndex == ID_TRUE )
    {
        /* check existence of table and get table META Info */
        sFlag  = 0;
        sFlag &= ~QMV_PERFORMANCE_VIEW_CREATION_MASK;
        sFlag |=  QMV_PERFORMANCE_VIEW_CREATION_FALSE;
        sFlag &= ~QMV_VIEW_CREATION_MASK;
        sFlag |=  QMV_VIEW_CREATION_FALSE;

        /* BUG-17409 */
        sParseTree->defaultExprFrom->tableRef->flag &=
            ~QMS_TABLE_REF_SCAN_FOR_NON_SELECT_MASK;
        sParseTree->defaultExprFrom->tableRef->flag |=
            QMS_TABLE_REF_SCAN_FOR_NON_SELECT_TRUE;
        
        IDE_TEST( qmvQuerySet::validateQmsTableRef(
                      aStatement,
                      NULL,
                      sParseTree->defaultExprFrom->tableRef,
                      sFlag,
                      MTC_COLUMN_NOTNULL_TRUE ) /* PR-13597 */
                  != IDE_SUCCESS );

        /* Memory Table�̸�, Variable Column�� Fixed Column���� ��ȯ�� TableRef�� �����. */
        if ( ( sTableType == SMI_TABLE_MEMORY ) ||
             ( sTableType == SMI_TABLE_VOLATILE ) )
        {
            // add tuple set
            IDE_TEST( qtc::nextTable(
                          &(sParseTree->defaultExprFrom->tableRef->table),
                          aStatement,
                          NULL,     /* Tuple ID�� ��´�. */
                          ID_FALSE, /* Memory Table */
                          MTC_COLUMN_NOTNULL_TRUE ) // PR-13597
                      != IDE_SUCCESS );

            IDE_TEST( qmvQuerySet::makeTupleForInlineView(
                          aStatement,
                          sParseTree->defaultExprFrom->tableRef,
                          sParseTree->defaultExprFrom->tableRef->table,
                          MTC_COLUMN_NOTNULL_TRUE )
                      != IDE_SUCCESS );

            // BUG-38670
            // Compressed column �� ���ԵǾ� ���� ��� add column ��
            // record ���� �� dictionary table �� record OID �� �ö�´�.
            // (fixed/variable ���)
            // �̸� ó���ϱ� ���ؼ��� ���� ���� intermediate tuple ��
            // column ������ compressed column �� ���,
            // compressed �Ӽ��� fixed/variable �Ӽ��� �����ؾ� �Ѵ�.
            //
            // Function based index ������ �߻��ϴ� �����̹Ƿ�
            // makeTupleForInlineView �� �������� �ʰ�,
            // ���⿡�� compressed, fixed/variable �Ӽ��� �����Ѵ�.
            sTableRef = sParseTree->defaultExprFrom->tableRef;
            sMtcTemplate = & QC_SHARED_TMPLATE(aStatement)->tmplate;

            for ( i = 0, sColumn = sTableRef->tableInfo->columns;
                  i <  sMtcTemplate->rows[sTableRef->table].columnCount;
                  i++, sColumn = sColumn->next)
            {
                sColumnFlag = sColumn->basicInfo->column.flag;

                if ( (sColumnFlag & SMI_COLUMN_COMPRESSION_MASK) ==
                     SMI_COLUMN_COMPRESSION_TRUE )
                {
                    sMtcColumn = &sMtcTemplate->rows[sTableRef->table].columns[i];
                    // Fixed/Variable
                    sMtcColumn->column.flag &= ~SMI_COLUMN_TYPE_MASK;
                    sMtcColumn->column.flag |= (sColumnFlag & SMI_COLUMN_TYPE_MASK);

                    // Compressed
                    sMtcColumn->column.flag &= ~SMI_COLUMN_COMPRESSION_MASK;
                    sMtcColumn->column.flag |= SMI_COLUMN_COMPRESSION_TRUE;
                }
            }
        }
        else
        {
            /* Disk Table�� Row Buffer���� Variable Column�� ����. */
        }
    }
    else
    {
        /* Nothing to do */
    }
    
    for ( sColumn = sParseTree->keyColumns;
          sColumn != NULL;
          sColumn = sColumn->next )
    {
        /* PROJ-1090 Function-based Index
         *  Function-based Index�� ���, �÷� ������ �����Ѵ�.
         */
        if ( (sColumn->flag & QCM_COLUMN_HIDDEN_COLUMN_MASK)
             == QCM_COLUMN_HIDDEN_COLUMN_TRUE )
        {
            /* Nchar List�� ���Ѵ�. */
            IDE_TEST( qdbCommon::makeNcharLiteralStrForIndex(
                          aStatement,
                          sParseTree->ncharList,
                          sColumn )
                      != IDE_SUCCESS );

            /* Default Expression�� �����ϴ� Column�� �˻��Ѵ�. */
            sExprColumnList = NULL;
            IDE_TEST( qmsDefaultExpr::makeColumnListFromExpression(
                          aStatement,
                          &sExprColumnList,
                          sColumn->defaultValue )
                      != IDE_SUCCESS );

            for ( sExprColumn = sExprColumnList;
                  sExprColumn != NULL;
                  sExprColumn = sExprColumn->next )
            {
                /* Column�� �����ϴ��� �˻��Ѵ�. */
                IDE_TEST( qcmCache::getColumn( aStatement,
                                               sTableInfo,
                                               sExprColumn->namePos,
                                               &sExprColumnInfo )
                          != IDE_SUCCESS );

                /* Hidden Column�� ���� Function-Based Index�� �������� �ʴ´�. */
                if ( (sExprColumnInfo->flag & QCM_COLUMN_HIDDEN_COLUMN_MASK)
                     == QCM_COLUMN_HIDDEN_COLUMN_TRUE )
                {
                    sqlInfo.setSourceInfo( aStatement,
                                           &(sExprColumn->namePos) );
                    IDE_RAISE( ERR_CANNOT_USE_HIDDEN_COLUMN );
                }
                else
                {
                    /* Nothing to do */
                }

                /* LOB�� �������� �ʴ´�. */
                if ( (sExprColumnInfo->basicInfo->module->flag & MTD_COLUMN_TYPE_MASK)
                     == MTD_COLUMN_TYPE_LOB )
                {
                    sqlInfo.setSourceInfo( aStatement,
                                           &(sExprColumn->namePos) );
                    IDE_RAISE( ERR_NOT_SUPPORT_LOB_COLUMN );
                }
                else
                {
                    /* Nothing to do */
                }

                /* ���� Column�� �������� �ʴ´�. */
                if ( (sExprColumnInfo->basicInfo->module->flag & MTD_ENCRYPT_TYPE_MASK)
                     == MTD_ENCRYPT_TYPE_TRUE )
                {
                    sqlInfo.setSourceInfo( aStatement,
                                           &(sExprColumn->namePos) );
                    IDE_RAISE( ERR_NOT_SUPPORT_ENCRYPTED_COLUMN );
                }
                else
                {
                    /* Nothing to do */
                }
            }

            /* BUG-35445 Check Constraint, Function-Based Index���� ��� ���� Function�� ����/���� ���� */
            IDE_TEST( qmsDefaultExpr::makeFunctionNameListFromExpression(
                            aStatement,
                            &(sParseTree->relatedFunctionNames),
                            sColumn->defaultValue,
                            NULL )
                      != IDE_SUCCESS );

            /* Estimate�� �����Ѵ�. */
            IDE_TEST( qdbCommon::validateDefaultExprDefinition(
                          aStatement,
                          sColumn->defaultValue,
                          NULL,
                          sParseTree->defaultExprFrom )
                      != IDE_SUCCESS );

            /* �÷� ������ �����Ѵ�. */
            sFlag = sColumn->basicInfo->column.flag & SMI_COLUMN_ORDER_MASK;
            *(sColumn->basicInfo) = *QTC_STMT_COLUMN( aStatement, sColumn->defaultValue );
            
            sColumn->basicInfo->column.flag &= ~SMI_COLUMN_ORDER_MASK;
            sColumn->basicInfo->column.flag |= (sFlag & SMI_COLUMN_ORDER_MASK);

            // set SMI_COLUMN_STORAGE_MASK
            if ( ( sTableType == SMI_TABLE_MEMORY ) ||
                 ( sTableType == SMI_TABLE_VOLATILE ) )
            {
                sColumn->basicInfo->column.flag &= ~SMI_COLUMN_STORAGE_MASK;
                sColumn->basicInfo->column.flag |= SMI_COLUMN_STORAGE_MEMORY;
            }
            else
            {
                sColumn->basicInfo->column.flag &= ~SMI_COLUMN_STORAGE_MASK;
                sColumn->basicInfo->column.flag |= SMI_COLUMN_STORAGE_DISK;
            }
            
            sColumn->flag |= QCM_COLUMN_TYPE_DEFAULT;
            sColumn->inRowLength = ID_UINT_MAX;

            /* Column �߰��� �ʿ��� ������ ������ �ڷ� ������ �����Ѵ�. */
            if ( sParseTree->addColumns == NULL )
            {
                IDE_TEST( qcm::copyQcmColumns( QC_QMP_MEM(aStatement),
                                               sColumn,
                                               &sNewColumns,
                                               1 )
                          != IDE_SUCCESS );
                sParseTree->addColumns = sNewColumns;
            }
            else
            {
                IDE_TEST( qcm::copyQcmColumns( QC_QMP_MEM(aStatement),
                                               sColumn,
                                               &(sNewColumns->next),
                                               1 )
                          != IDE_SUCCESS );
                sNewColumns = sNewColumns->next;
            }
        }
        else
        {
            /* Nothing to do */
        }
    }
    
    if ( sIsFunctionBasedIndex == ID_TRUE )
    {
        /* create index�� validation�� �����Ѵ�. */
        IDE_TEST( qdx::validate( aStatement ) != IDE_SUCCESS );

        /* ���� ���ʹ� alter table add column�� validation, execution�� �����Ѵ�. */
        IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(qdTableParseTree),
                                                 (void**)&sTableParseTree )
                  != IDE_SUCCESS );
        idlOS::memcpy( &(sTableParseTree->common),
                       &(sParseTree->common),
                       ID_SIZEOF(qcParseTree) );
        QD_TABLE_PARSE_TREE_INIT( sTableParseTree );

        sTableParseTree->userName.stmtText = sTableInfo->tableOwnerName;
        sTableParseTree->userName.offset   = 0;
        sTableParseTree->userName.size     = idlOS::strlen( sTableInfo->tableOwnerName );

        sTableParseTree->tableName.stmtText = sTableInfo->name;
        sTableParseTree->tableName.offset   = 0;
        sTableParseTree->tableName.size     = idlOS::strlen( sTableInfo->name );

        sTableParseTree->columns = sParseTree->addColumns;

        sTableParseTree->ncharList = sParseTree->ncharList;

        // PROJ-1502 PARTITIONED DISK TABLE
        IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(qdPartitionedTable),
                                                 (void**)&sTableParseTree->partTable )
                  != IDE_SUCCESS );
        QD_SET_INIT_PART_TABLE(sTableParseTree->partTable);
        sTableParseTree->partTable->partAttr = NULL;

        /* PROJ-1090 Function-based Index */
        sTableParseTree->createIndexParseTree = sParseTree;
        sTableParseTree->addHiddenColumn = ID_TRUE;
        
        sTableParseTree->common.validate = qdbAlter::validateAddCol;
        sTableParseTree->common.execute  = qdbAlter::executeAddCol;

        aStatement->myPlan->parseTree = (qcParseTree *)sTableParseTree;
    }
    else
    {
        /* Nothing to do */
    }
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION(ERR_NOT_EXIST_TABLE);
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QCV_NOT_EXISTS_TABLE,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION( ERR_CANNOT_USE_HIDDEN_COLUMN );
    {
        (void)sqlInfo.init( aStatement->qmeMem );
        IDE_SET( ideSetErrorCode(qpERR_ABORT_QDB_CANNOT_USE_HIDDEN_COLUMN,
                                 sqlInfo.getErrMessage() ) );
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION( ERR_NOT_SUPPORT_LOB_COLUMN );
    {
        (void)sqlInfo.init( aStatement->qmeMem );
        IDE_SET( ideSetErrorCode(qpERR_ABORT_QDX_NOT_SUPPORT_LOB_COLUMN,
                                 sqlInfo.getErrMessage() ) );
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION( ERR_NOT_SUPPORT_ENCRYPTED_COLUMN );
    {
        (void)sqlInfo.init( aStatement->qmeMem );
        IDE_SET( ideSetErrorCode(qpERR_ABORT_QDX_NOT_SUPPORT_ENCRYPTED_COLUMN,
                                 sqlInfo.getErrMessage() ) );
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION(ERR_CANNOT_DDL_DICTIONARY_TABLE)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDB_CANNOT_DDL_DML_DICTIONARY_TABLE));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

/***********************************************************************
 * VALIDATE
 **********************************************************************/
// CREATE INDEX

// Validation in Parser
//  - check duplicated column name in specified column list
IDE_RC qdx::validate(qcStatement * aStatement)
{
/***********************************************************************
 *
 * Description :
 *    CREATE INDEX ... �� validation ����
 *
 * Implementation :
 *    1. �����ϴ� ���̺����� üũ
 *    2. ����� ���̺��� ���̸� ���� ��ȯ
 *    3. Replication�� �ɷ�������, Unique Index, Function-based Index ���θ� Ȯ��
 *    4. ����� �ε����� �̸��� �̹� ������ ���� ��ȯ
 *    5. create index ������ �ִ��� üũ
 *    6. �ε����� �ɷ��� �ϴ� �÷��� �����ϴ��� üũ
 *    7. �ε����� �ɷ��� �ϴ� �÷����� �̹� ������ �ε����� �����ϴ��� üũ
 *    8. TABLESPACE �� ���� validation �ڵ� �߰�
 *    if ( TABLESPACENAME ����� ��� )
 *    {
 *      8.1.1 SM���� �����ϴ� ���̺����̽������� �˻�
 *      8.1.2 �������� ������ ����
 *      8.1.3 ���̺����̽��� ������ UNDO �Ǵ� temporary tablespace�̸� ����
 *      8.1.4 USER_ID(�ε��� ������) �� TBS_ID �� SYS_TBS_USERS_ �˻��ؼ�
 *            ���ڵ尡 �����ϰ� IS_ACCESS ���� OFF �̸� ����
 *      8.1.5 (To Fix PR-9770) ���� ��ü�� �������� �˻��Ͽ�
 *            ���� ��ü�� �ٸ��� ����
 *    }
 *    else // TABLESPACENAME ������� ���� ���
 *    {
 *      8.2.1 USER_ID(�ε��� ������) �� SYS_USERS_ �˻��� DEFAULT_TBS_ID ����
 *            �о �ε����� ���� ���̺����̽��� ����
 *    }
 *    9. key size limit �˻� �ڵ� �߰�
 *
 ***********************************************************************/

#define IDE_FN "qdx::validate"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qdx::validate"));

    qdIndexParseTree    * sParseTree;
    qcmTableInfo        * sTableInfo;
    qcmColumn           * sColumn;
    qcmColumn           * sColumnInfo;
    SInt                  sKeyColCount = 0;
    UInt                  sIndexID;
    UInt                  sTableID;
    UInt                  i;
    SInt                  sSize;
    UInt                  sType = ID_UINT_MAX;
    UInt                  sTableType;
    SChar                 sIndexType[4096];
    smiTableSpaceType     sTBSType;
    qcuSqlSourceInfo      sqlInfo;

    UInt                  sFlag;

    idBool                sNeedCheck = ID_TRUE;

    sParseTree = (qdIndexParseTree *)aStatement->myPlan->parseTree;

    // check table exist.
    if ( qdbCommon::checkTableInfo( aStatement,
                                    sParseTree->userNameOfTable,
                                    sParseTree->tableName,
                                    &(sParseTree->userIDOfTable),
                                    &(sParseTree->tableInfo),
                                    &(sParseTree->tableHandle),
                                    &(sParseTree->tableSCN))
         != IDE_SUCCESS)
    {
        sqlInfo.setSourceInfo( aStatement,
                               & sParseTree->tableName );
        IDE_RAISE(ERR_NOT_EXIST_TABLE);
    }
    else
    {
        /* BUG-48290 shard object�� ���� DDL ���� */
        IDE_TEST( sdi::checkShardObjectForDDL( aStatement, SDI_DDL_TYPE_INDEX ) != IDE_SUCCESS );
    }

    IDE_TEST(qcm::lockTableForDDLValidation(aStatement,
                                            sParseTree->tableHandle,
                                            sParseTree->tableSCN)
             != IDE_SUCCESS);

    sTableInfo = sParseTree->tableInfo;

    // PROJ-1502 PARTITIONED DISK TABLE
    if( sTableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
    {
        // ��� ��Ƽ�ǿ� LOCK(IS)
        // ��Ƽ�� ����Ʈ�� �Ľ�Ʈ���� �޾Ƴ��´�.
        IDE_TEST( qdbCommon::checkAndSetAllPartitionInfo(
                      aStatement,
                      sTableInfo->tableID,
                      & (sParseTree->partIndex->partInfoList))
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }
        
    // check view
    if ( ( sTableInfo->tableType == QCM_VIEW ) ||
         ( sTableInfo->tableType == QCM_MVIEW_VIEW ) )
    {
        sqlInfo.setSourceInfo(
            aStatement,
            & sParseTree->tableName );
        IDE_RAISE(ERR_DDL_ON_VIEW);
    }
    else
    {
        // Nothing to do.
    }

    // check hidden table
    if ( ( sTableInfo->tableType == QCM_INDEX_TABLE ) ||
         ( sTableInfo->tableType == QCM_SEQUENCE_TABLE ) )
    {
        sqlInfo.setSourceInfo(
            aStatement,
            & sParseTree->tableName );
        IDE_RAISE( ERR_NOT_EXIST_TABLE );
    }
    else
    {
        // Nothing to do.
    }

    // if same index name exists, then error
    if( qcm::checkIndexByUser(
            aStatement,
            sParseTree->userNameOfIndex,
            sParseTree->indexName,
            &(sParseTree->userIDOfIndex),
            &sTableID,
            &sIndexID)
        == IDE_SUCCESS)
    {
        IDE_RAISE( ERR_DUPLCATED_INDEX );
    }
    else
    {
        if( ideGetErrorCode() == qpERR_ABORT_QCM_NOT_EXISTS_INDEX )
        {
            // �ش� �ε����� �������� ������ ����.
            // �����ڵ� Ŭ����.
            ideClearError();
        }
        else
        {
            // index��Ÿ�˻��� ����. ������ �״�� �н�
            IDE_TEST(1);
        }
    }

    // check grant
    IDE_TEST( qdpRole::checkDDLCreateIndexPriv( aStatement,
                                                sTableInfo,
                                                sParseTree->userIDOfIndex )
              != IDE_SUCCESS );
    
    // BUG-16131
    // create�� ������ index type ����
    if ( QC_IS_NULL_NAME( sParseTree->indexType ) == ID_FALSE )
    {
        sSize = sParseTree->indexType.size < (SInt)(ID_SIZEOF(sIndexType)-1) ?
            sParseTree->indexType.size : (SInt)(ID_SIZEOF(sIndexType)-1) ;

        idlOS::strncpy( sIndexType,
                        sParseTree->indexType.stmtText + sParseTree->indexType.offset,
                        sSize );
        sIndexType[sSize] = '\0';

        IDE_TEST( smiFindIndexType( sIndexType, &sType ) != IDE_SUCCESS);
    }
    else
    {
        // Nothing to do.
    }

    // key size limit �˻�
    sTableType = sTableInfo->tableFlag & SMI_TABLE_TYPE_MASK;

    // fix BUG-27231 [CodeSonar] Buffer Underrun
    IDE_ASSERT( sParseTree->keyColumns != NULL );

    for ( sColumn = sParseTree->keyColumns;
          sColumn != NULL;
          sColumn = sColumn->next )
    {
        /* PROJ-1090 Function-based Index
         *  Function-based Index�� ���, �÷� ������ �����Ѵ�.
         */
        if ( (sColumn->flag & QCM_COLUMN_HIDDEN_COLUMN_MASK)
             == QCM_COLUMN_HIDDEN_COLUMN_TRUE )
        {
            sColumnInfo = sColumn;
        }
        else
        {
            // check existence of columns
            IDE_TEST( qcmCache::getColumn( aStatement,
                                           sTableInfo,
                                           sColumn->namePos,
                                           &sColumnInfo )
                      != IDE_SUCCESS );

            /* Hidden Column�� ���� �Ϲ� Index�� �������� �ʴ´�. */
            if ( (sColumnInfo->flag & QCM_COLUMN_HIDDEN_COLUMN_MASK)
                 == QCM_COLUMN_HIDDEN_COLUMN_TRUE )
            {
                sqlInfo.setSourceInfo( aStatement,
                                       &(sColumn->namePos) );
                IDE_RAISE( ERR_CANNOT_USE_HIDDEN_COLUMN );
            }
            else
            {
                /* Nothing to do */
            }
        }
        
        if (sColumnInfo->basicInfo->type.dataTypeId == MTD_GEOMETRY_ID)
        {
            sNeedCheck = ID_FALSE;
        }
        else
        {
            /* Nothing to do */
        }
        
        // PROJ-1362
        // check lob column
        IDE_TEST_RAISE(
            (sColumnInfo->basicInfo->module->flag & MTD_COLUMN_TYPE_MASK)
            == MTD_COLUMN_TYPE_LOB,
            ERR_INVALID_INDEX_COLS );

        // Key Column�� Order ������ ������ �־�� �Ѵ�.
        sFlag = sColumn->basicInfo->column.flag & SMI_COLUMN_ORDER_MASK;

        // fix BUG-33258
        if( sColumn->basicInfo != sColumnInfo->basicInfo )
        {
            *(sColumn->basicInfo) = *(sColumnInfo->basicInfo);
        }

        sColumn->basicInfo->column.flag &= ~SMI_COLUMN_ORDER_MASK;
        sColumn->basicInfo->column.flag |= (sFlag & SMI_COLUMN_ORDER_MASK);

        // PROJ-1705
        if( sTableType == SMI_TABLE_DISK )
        {
            IDE_TEST(
                qdbCommon::setIndexKeyColumnTypeFlag( sColumn->basicInfo )
                != IDE_SUCCESS );
        }
        else
        {
            // Nothing To Do
        }

        // BUG-16131
        // create�� index type�� �������� �ʾ��� ���
        // indexȤ�� composite index�� ù��° column�� default index type ����
        if ( sType == ID_UINT_MAX )
        {
            sType = mtd::getDefaultIndexTypeID( sColumn->basicInfo->module );
        }
        else
        {
            // Nothing to do.
        }

        // ���õ� index type�� �� index column�� ��� �����ؾ� �Ѵ�.
        IDE_TEST_RAISE( mtd::isUsableIndexType(
                            sColumn->basicInfo->module,
                            sType ) != ID_TRUE,
                        not_supported_type ) ;

        sKeyColCount++;
    }

    IDE_TEST_RAISE( sType == ID_UINT_MAX, ERR_INVALID_INDEX_TYPE )

    // To Fix PR-15189
    // geometry Ÿ���� unique index�� ������ �� ����.
    IDE_TEST_RAISE(
        ( (sParseTree->flag & SMI_INDEX_UNIQUE_MASK) ==
          SMI_INDEX_UNIQUE_ENABLE ) &&
        ( smiCanUseUniqueIndex( sType ) == ID_FALSE ),
        ERR_INVALID_INDEX_COLS );

    // BUG-16218
    // geometry Ÿ���� composite index�� ������ �� ����.
    IDE_TEST_RAISE(
        ( sKeyColCount > 1 ) &&
        ( smiCanUseCompositeIndex( sType ) == ID_FALSE ),
        ERR_INVALID_INDEX_COLS );

    IDE_TEST_RAISE(sKeyColCount > QC_MAX_KEY_COLUMN_COUNT,
                   ERR_MAX_KEY_COLUMN_COUNT);

    sColumn = sParseTree->keyColumns;

    for (i = 0; i < sTableInfo->indexCount; i++)
    {
        IDE_TEST_RAISE(
            qdn::matchColumnId( sColumn,
                                (UInt*) smiGetIndexColumns(
                                    sTableInfo->indices[i].indexHandle),
                                sTableInfo->indices[i].keyColCount,
                                sTableInfo->indices[i].keyColsFlag)
            == ID_TRUE, ERR_DUPLICATED_INDEX_COLS);
    }

    sParseTree->keyColCount = sKeyColCount;

    // Index TableSpace�� ���� Validation�� ������ ȹ����.
    IDE_TEST( qdtCommon::getAndValidateIndexTBS( aStatement,
                                                 sTableInfo->TBSID,
                                                 sTableInfo->TBSType,
                                                 sParseTree->TBSName,
                                                 sParseTree->userIDOfIndex,
                                                 & sParseTree->TBSID,
                                                 & sTBSType )
              != IDE_SUCCESS );

    /* BUG-40099 
     * - Temporary Table �� Index ���� ��, table�� ���� tablespace ���� ���.
     */
    if( qcuTemporaryObj::isTemporaryTable( sTableInfo ) == ID_TRUE )
    {
        /* temporary table index�� tablespace�� ���� �� �� ����.
         * �׷���, table�� tablespace�� �����ϸ� ����Ѵ�.
         */
        IDE_TEST_RAISE( sTableInfo->TBSID != sParseTree->TBSID,
                        ERR_CANNOT_ALLOW_TBS_NAME_FOR_TEMPORARY_INDEX );
    }
    else
    {
        /* Nothing to do */
    }

    // PROJ-1502 PARTITIONED DISK TABLE
    // ��Ƽ�ǵ� �ε��� ������ ���
    if( sParseTree->partIndex->partIndexType != QCM_NONE_PARTITIONED_INDEX )
    {
        // ���� �ε��� ���� ��, validation
        IDE_TEST( validatePartitionedIndexOnCreateIndex( aStatement,
                                                         sParseTree,
                                                         sTableInfo,
                                                         sType )
                  != IDE_SUCCESS );
    }
    // ����Ƽ�ǵ� �ε��� ������ ���
    else
    {
        // ���� ����ũ �ε��� ������ �� ����
        IDE_TEST_RAISE( (sParseTree->flag & SMI_INDEX_LOCAL_UNIQUE_MASK) ==
                                            SMI_INDEX_LOCAL_UNIQUE_ENABLE,
                        ERR_LOCAL_UNIQUE_KEY_ON_NON_PART_TABLE );
    }

    /* PROJ-2464 hybrid partitioned table ����
     *  - ��ü�� ���� ���� ������ �� ���� ��츦 �˻��Ѵ�.
     *  - ������ �� �ִ� Index�� ���, Index ������ �˻��Ѵ�.
     */
    IDE_TEST( validateIndexRestriction( aStatement,
                                        sNeedCheck,
                                        sType )
              != IDE_SUCCESS );

    // PROJ-1502 PARTITIONED DISK TABLE
    if ( ( sTableInfo->tablePartitionType == QCM_PARTITIONED_TABLE ) &&
         ( sParseTree->partIndex->partIndexType == QCM_NONE_PARTITIONED_INDEX ) )
    {  
        IDE_TEST_RAISE( ( qrc::isDDLSync( aStatement ) == ID_TRUE ) &&
                        ( sParseTree->tableInfo->replicationCount > 0 ), 
                        ERR_NOT_SUPPORT_GLOBAL_NON_PARTITION_INDEX ); 

        // PROJ-1624 global non-partitioned index
        IDE_TEST( validateNonPartitionedIndex(
                      aStatement,
                      sParseTree->userNameOfTable,
                      sParseTree->indexName,
                      sParseTree->indexTableName,
                      sParseTree->keyIndexName,
                      sParseTree->ridIndexName )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do
    }

    if ( ( sParseTree->tableInfo->replicationCount > 0 ) ||
         ( QCG_GET_SESSION_IS_NEED_DDL_INFO( aStatement ) == ID_TRUE ) )
    {
        qrc::setDDLSrcInfo( aStatement,
                            ID_TRUE,
                            1,
                            &(sParseTree->tableInfo->tableOID),
                            0,
                            NULL );
    }
    else
    {
        // Nothing to do.
    }

    // Segment�� Storage ���� ���� validation ����
    IDE_TEST( qdbCommon::validateAndSetSegStoAttr( sTableType,
                                                   NULL,
                                                   & ( sParseTree->segStoAttr ),
                                                   & ( sParseTree->existSegStoAttr ),
                                                   ID_FALSE )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION (ERR_CANNOT_ALLOW_TBS_NAME_FOR_TEMPORARY_INDEX )
    {
        IDE_SET(ideSetErrorCode( qpERR_ABORT_QDX_TEMPORARY_INDEX_NOT_ALLOW_TBS_NAME ));
    }
    IDE_EXCEPTION(ERR_INVALID_INDEX_COLS)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDX_INVALID_INDEX_COLS));
    }
    IDE_EXCEPTION(ERR_DUPLCATED_INDEX);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDX_DUPLICATE_INDEX));
    }
    IDE_EXCEPTION(ERR_DUPLICATED_INDEX_COLS);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDX_DUPLICATE_INDEX_COLS));
    }
    IDE_EXCEPTION(ERR_NOT_EXIST_TABLE);
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QCV_NOT_EXISTS_TABLE,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION(ERR_DDL_ON_VIEW);
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QDV_DDL_ON_VIEW,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION(ERR_MAX_KEY_COLUMN_COUNT)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDX_INVALID_KEY_FIELD_COUNT));
    }
    // To Fix PR-15190
    IDE_EXCEPTION(not_supported_type);
    {
        IDE_SET( ideSetErrorCode(
                     qpERR_ABORT_QDX_CANNOT_CREATE_INDEX_DATATYPE ) ) ;
    }
    IDE_EXCEPTION(ERR_LOCAL_UNIQUE_KEY_ON_NON_PART_TABLE);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDN_CANNOT_CREATE_LOCAL_UNIQUE_KEY_CONSTR_ON_NON_PART_TABLE));
    }
    IDE_EXCEPTION(ERR_INVALID_INDEX_TYPE)
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qdx::validate",
                                  "Invalid index type" ));
    }
    IDE_EXCEPTION( ERR_CANNOT_USE_HIDDEN_COLUMN );
    {
        (void)sqlInfo.init( aStatement->qmeMem );
        IDE_SET( ideSetErrorCode(qpERR_ABORT_QDB_CANNOT_USE_HIDDEN_COLUMN,
                                 sqlInfo.getErrMessage() ) );
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION( ERR_NOT_SUPPORT_GLOBAL_NON_PARTITION_INDEX );
    {
        IDE_SET( ideSetErrorCode( rpERR_ABORT_RP_DDL_SYNC_NOT_SUPPORT_GLOBAL_NON_PARTITION_INDEX ) );
    }


    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qdx::validatePartitionedIndexOnCreateIndex(
    qcStatement      * aStatement,
    qdIndexParseTree * aParseTree,
    qcmTableInfo     * aTableInfo,
    UInt            /* aIndexType */ )
{
/***********************************************************************
 *
 * Description :
 *      PROJ-1502 PARTITIONED DISK TABLE
 *
 *      ��Ƽ�ǵ� �ε��� ���� ��, validation
 *
 *
 * Implementation :
 *      1. �ε����� �����Ϸ��� ���̺��� ��Ƽ�ǵ� ��ü���� üũ
 *
 *      2. (�۷ι�)����ũ �ε��� �����̸� �����Ƚ��� �ε������� üũ
 *
 *      3. �ε��� Ÿ�� üũ
 *         ��� �� ������ Ÿ���̾�� �Ѵ�.
 *
 *      4. ���� �ε����̸鼭 PARTITIONED INDEX�� TBS�� ���� ��, ����
 *         ex) CREATE INDEX IDX1 T1 ( I1 ) LOCAL TABLESPACE TBS1;
 *
 *      5. ������ �ε��� ��Ƽ�� ������ŭ �ݺ�
 *          5-1. �ε��� ��Ƽ�� �̸� validation
 *          5-2. ������ ���̺� ��Ƽ���� �����ϴ��� üũ
 *          5-3. ���̺� ��Ƽ�� �̸� validation
 *          5-4. ���̺� �����̽� validation
 *
 *      6. ������ �ε��� ��Ƽ�� ���� üũ
 *         ���̺� ��Ƽ���� ���������� ������ �� �ִ�.
 *
 ***********************************************************************/

    qdPartitionedIndex      * sPartIndex;
    qdPartitionAttribute    * sPartAttr;
    qdPartitionAttribute    * sTempPartAttr;
    UInt                      sIndexPartCount = 0;
    UInt                      sTablePartCount;
    qcuSqlSourceInfo          sqlInfo;
    qcmTableInfo            * sPartitionInfo;
    qcmPartitionInfoList    * sPartInfoList = NULL;
    qcmPartitionInfoList    * sTempPartInfoList = NULL;
    idBool                    sIsFound = ID_FALSE;
    idBool                    sIsLocalIndex;

    sPartInfoList = aParseTree->partIndex->partInfoList;
    sPartIndex = aParseTree->partIndex;

    // ------------------------------------------------------------
    // 1. ��Ƽ�ǵ� ���̺����� üũ
    // ------------------------------------------------------------
    IDE_TEST_RAISE( aTableInfo->partitionMethod == QCM_PARTITION_METHOD_NONE,
                    ERR_CREATE_PART_INDEX_ON_NONE_PART_TABLE );

    // ------------------------------------------------------------
    // BUG-41001
    // 2. (�۷ι�)����ũ �ε��� �����̸� partition key�� �����ϴ��� üũ
    // ------------------------------------------------------------
    if( (aParseTree->flag & SMI_INDEX_UNIQUE_MASK) ==
        SMI_INDEX_UNIQUE_ENABLE )
    {
        IDE_TEST( checkLocalIndexOnAlterTable( aStatement,
                                               aTableInfo,
                                               aParseTree->keyColumns,
                                               aTableInfo->partKeyColumns,
                                               & sIsLocalIndex )
                  != IDE_SUCCESS );

        // local index�� �Ұ��� ��� ����
        IDE_TEST_RAISE( sIsLocalIndex == ID_FALSE,
                        ERR_UNIQUE_PARTITIONED_INDEX );
    }

    // ------------------------------------------------------------
    // 3. �ε��� Ÿ���� ��� �� ������ Ÿ������ üũ
    // ------------------------------------------------------------
    // BUG-36741
    // global partitioned index�� ��쿡�� ��� �񱳰� ������ Ÿ���� �ʿ��һ�
    // local partitioned index�� ��쿡�� ��� �񱳸� ���� �ʴ´�.

    // ------------------------------------------------------------
    // 4. ���� �ε��� ���� ��, ��Ƽ�ǵ� �ε����� TBS�� ���� �� ����
    // ex) CREATE INDEX IDX1 T1 ( I1 ) LOCAL TABLESPACE TBS1;
    // ------------------------------------------------------------
    if( QC_IS_NULL_NAME( aParseTree->TBSName ) != ID_TRUE )
    {
        sqlInfo.setSourceInfo(aStatement,
                              & aParseTree->TBSName);

        IDE_RAISE( ERR_TBS_POSITION_OF_LOCAL_PARTITIONED_INDEX );
    }

    // ------------------------------------------------------------
    // 5. ������ �ε��� ������ŭ �ݺ��ϸ� validation
    // ------------------------------------------------------------
    for( sPartAttr = sPartIndex->partAttr;
         sPartAttr != NULL;
         sPartAttr = sPartAttr->next )
    {
        // ------------------------------------------------------------
        // 5-1. �ε��� ��Ƽ�� �̸� �ߺ� �˻�
        // ------------------------------------------------------------
        for( sTempPartAttr = sPartIndex->partAttr;
             sTempPartAttr != sPartAttr;
             sTempPartAttr = sTempPartAttr->next )
        {
            if ( QC_IS_NAME_MATCHED( sPartAttr->indexPartName, sTempPartAttr->indexPartName ) )
            {
                sqlInfo.setSourceInfo(aStatement,
                                      & sPartAttr->indexPartName );
                IDE_RAISE( ERR_DUPLICATE_PARTITION_NAME );
            }
        }

        // ------------------------------------------------------------
        // 5-2. �ε����� �����Ϸ��� ��Ƽ�ǵ� ���̺�
        //      ������ ���̺� ��Ƽ���� �����ϴ��� üũ.
        //      ��Ƽ�� ������ �����´�.
        // ------------------------------------------------------------
        sIsFound = ID_FALSE;

        for( sTempPartInfoList = sPartInfoList;
             sTempPartInfoList != NULL;
             sTempPartInfoList = sTempPartInfoList->next )
        {
            sPartitionInfo = sTempPartInfoList->partitionInfo;

            if( idlOS::strMatch(sPartitionInfo->name,
                                idlOS::strlen(sPartitionInfo->name),
                                sPartAttr->tablePartName.stmtText +
                                sPartAttr->tablePartName.offset,
                                sPartAttr->tablePartName.size ) == 0 )
            {
                sIsFound = ID_TRUE;
                break;
            }
        }

        if( sIsFound == ID_FALSE )
        {
            sqlInfo.setSourceInfo(aStatement,
                                  & sPartAttr->tablePartName );
            IDE_RAISE( ERR_NOT_EXIST_TABLE_PARTITION );
        }

        // ------------------------------------------------------------
        // 5-3. ���̺� ��Ƽ���� �̸��� �ߺ� �˻�
        // ------------------------------------------------------------
        for( sTempPartAttr = sPartIndex->partAttr;
             sTempPartAttr != sPartAttr;
             sTempPartAttr = sTempPartAttr->next )
        {
            if ( QC_IS_NAME_MATCHED( sPartAttr->tablePartName, sTempPartAttr->tablePartName ) )
            {
                sqlInfo.setSourceInfo(aStatement,
                                      & sPartAttr->tablePartName );
                IDE_RAISE( ERR_DUPLICATE_PARTITION_NAME );
            }
        }

        // ------------------------------------------------------------
        // 5-4. ���̺� �����̽� validation
        // ------------------------------------------------------------
        IDE_TEST( qdtCommon::getAndValidateTBSOfIndexPartition( aStatement,
                                                                sPartitionInfo->TBSID,
                                                                sPartitionInfo->TBSType,
                                                                sPartAttr->TBSName,
                                                                aParseTree->userIDOfIndex,
                                                                & sPartAttr->TBSAttr.mID,
                                                                & sPartAttr->TBSAttr.mType )
                  != IDE_SUCCESS );

        sIndexPartCount++;
    }

    // ���̺� ��Ƽ�� ����
    IDE_TEST( qcmPartition::getPartitionCount( aStatement,
                                               aTableInfo->tableID,
                                               & sTablePartCount )
              != IDE_SUCCESS );

    // ------------------------------------------------------------
    // 6. ������ �ε��� ��Ƽ���� ���� üũ
    // ------------------------------------------------------------
    IDE_TEST_RAISE( sIndexPartCount > sTablePartCount,
                    ERR_INDEX_PARTITION_COUNT );

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_DUPLICATE_PARTITION_NAME)
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDB_DUPLICATE_PARTITION_NAME,
                                sqlInfo.getErrMessage()));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION(ERR_TBS_POSITION_OF_LOCAL_PARTITIONED_INDEX)
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(ideSetErrorCode(
                    qpERR_ABORT_QDX_CANNOT_SPECIFY_TBS_OF_LOCAL_PARTITIONED_INDEX,
                    sqlInfo.getErrMessage()) );
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION(ERR_NOT_EXIST_TABLE_PARTITION)
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(ideSetErrorCode( qpERR_ABORT_QCM_NOT_EXIST_TABLE_PARTITION,
                                 sqlInfo.getErrMessage()) );
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION(ERR_CREATE_PART_INDEX_ON_NONE_PART_TABLE)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDX_CANNOT_CREATE_PART_INDEX_ON_NONE_PART_TABLE));
    }
    IDE_EXCEPTION(ERR_INDEX_PARTITION_COUNT)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDX_INVALID_INDEX_PARTITION_COUNT));
    }
    IDE_EXCEPTION(ERR_UNIQUE_PARTITIONED_INDEX)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDX_CANNOT_CREATE_PARTITIONED_INDEX));
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdx::validatePartitionedIndexOnAlterTable(
    qcStatement        * aStatement,
    qcNamePosition       aPartIndexTBSName,
    qdPartitionedIndex * aPartIndex,
    qcmTableInfo       * aTableInfo )
{
/***********************************************************************
 *
 * Description :
 *      fix BUG-18937
 *
 *      ��Ƽ�ǵ� ���̺� ���� ALTER TABLE ��,
 *      PRIMARY KEY, UNIQUE, LOCAL UNIQUE�� ������ ���
 *      �� �ε��� ��Ƽ�ǿ� ���ؼ� TABLESPACE�� ���� ������ �� �ִ�.
 *
 *         ex) ALTER TABLE T1 ADD COLUMN
 *                          ( I2 INTEGER PRIMARY KEY
 *                             USING INDEX
 *                             LOCAL
 *                                (
 *                                    PARTITION P1_PRI ON P1 TABLESPACE TBS1,
 *                                    PARTITION P1_PRI ON P2 TABLESPACE TBS2,
 *                                    PARTITION P1_PRI ON P3 TABLESPACE TBS3
 *                                )
 *                          );
 *
 *
 * Implementation :
 *      1. ���� �ε����̸鼭 PARTITIONED INDEX�� TBS�� ���� ��, ����
 *         ex) ALTER TABLE T1 ADD COLUMN ( I2 INTEGER PRIMARY KEY
 *                                         USING INDEX TABLESPACE TBS1
 *                                         LOCAL );
 *
 *      2. ������ �ε��� ��Ƽ�� ������ŭ �ݺ�
 *          2-1. �ε��� ��Ƽ�� �̸� validation
 *          2-2. ������ ���̺� ��Ƽ���� �����ϴ��� üũ
 *          2-3. ���̺� ��Ƽ�� �̸� validation
 *          2-4. ���̺� �����̽� validation
 *
 *      3. ������ �ε��� ��Ƽ�� ���� üũ
 *         ���̺� ��Ƽ���� ���������� ������ �� �ִ�.
 *
 ***********************************************************************/

    qdPartitionAttribute    * sPartAttr;
    qdPartitionAttribute    * sTempPartAttr;
    UInt                      sIndexPartCount = 0;
    UInt                      sTablePartCount;
    qcuSqlSourceInfo          sqlInfo;
    qcmTableInfo            * sPartitionInfo;
    qcmPartitionInfoList    * sPartInfoList = NULL;
    qcmPartitionInfoList    * sTempPartInfoList = NULL;
    idBool                    sIsFound = ID_FALSE;


    // ------------------------------------------------------------
    // 1. ���� �ε��� ���� ��, ��Ƽ�ǵ� �ε����� TBS�� ���� �� ����
    //     ex) ALTER TABLE T1 ADD COLUMN ( I2 INTEGER PRIMARY KEY
    //                                     USING INDEX TABLESPACE TBS1
    //                                     LOCAL );
    // ------------------------------------------------------------
    if( QC_IS_NULL_NAME( aPartIndexTBSName ) != ID_TRUE )
    {
        sqlInfo.setSourceInfo(aStatement,
                              & aPartIndexTBSName);

        IDE_RAISE( ERR_TBS_POSITION_OF_LOCAL_PARTITIONED_INDEX );
    }
    else
    {
        // Nothing to do
    }

    // ------------------------------------------------------------
    // ��Ƽ�� ����, Handle, SCN�� ����Ʈ�� ���ؼ� �Ľ�Ʈ���� �޾Ƴ��´�.
    // ------------------------------------------------------------

    // ��� ��Ƽ�ǿ� LOCK(IS)
    // ��Ƽ�� ����Ʈ�� �Ľ�Ʈ���� �޾Ƴ��´�.
    IDE_TEST( qdbCommon::checkAndSetAllPartitionInfo(
                        aStatement,
                        aTableInfo->tableID,
                        & (aPartIndex->partInfoList))
              != IDE_SUCCESS );

    sPartInfoList = aPartIndex->partInfoList;

    // ------------------------------------------------------------
    // 2. ������ �ε��� ������ŭ �ݺ��ϸ� validation
    // ------------------------------------------------------------
    for( sIndexPartCount = 0, sPartAttr = aPartIndex->partAttr;
         sPartAttr != NULL;
         sIndexPartCount++, sPartAttr = sPartAttr->next )
    {
        // ------------------------------------------------------------
        // 2-1. �ε��� ��Ƽ�� �̸� �ߺ� �˻�
        // ------------------------------------------------------------
        for( sTempPartAttr = aPartIndex->partAttr;
             sTempPartAttr != sPartAttr;
             sTempPartAttr = sTempPartAttr->next )
        {
            if ( QC_IS_NAME_MATCHED( sPartAttr->indexPartName, sTempPartAttr->indexPartName ) )
            {
                sqlInfo.setSourceInfo(aStatement,
                                      & sPartAttr->indexPartName );
                IDE_RAISE( ERR_DUPLICATE_PARTITION_NAME );
            }
        }

        // ------------------------------------------------------------
        // 2-2. �ε����� �����Ϸ��� ��Ƽ�ǵ� ���̺�
        //      ������ ���̺� ��Ƽ���� �����ϴ��� üũ.
        //      ��Ƽ�� ������ �����´�.
        // ------------------------------------------------------------
        sIsFound = ID_FALSE;

        for( sTempPartInfoList = sPartInfoList;
             sTempPartInfoList != NULL;
             sTempPartInfoList = sTempPartInfoList->next )
        {
            sPartitionInfo = sTempPartInfoList->partitionInfo;

            if( idlOS::strMatch(sPartitionInfo->name,
                                idlOS::strlen(sPartitionInfo->name),
                                sPartAttr->tablePartName.stmtText +
                                sPartAttr->tablePartName.offset,
                                sPartAttr->tablePartName.size ) == 0 )
            {
                sIsFound = ID_TRUE;
                break;
            }
        }

        if( sIsFound == ID_FALSE )
        {
            sqlInfo.setSourceInfo(aStatement,
                                  & sPartAttr->tablePartName );
            IDE_RAISE( ERR_NOT_EXIST_TABLE_PARTITION );
        }

        // ------------------------------------------------------------
        // 2-3. ���̺� ��Ƽ���� �̸��� �ߺ� �˻�
        // ------------------------------------------------------------
        for( sTempPartAttr = aPartIndex->partAttr;
             sTempPartAttr != sPartAttr;
             sTempPartAttr = sTempPartAttr->next )
        {
            if ( QC_IS_NAME_MATCHED( sPartAttr->tablePartName, sTempPartAttr->tablePartName ) )
            {
                sqlInfo.setSourceInfo(aStatement,
                                      & sPartAttr->tablePartName );
                IDE_RAISE( ERR_DUPLICATE_PARTITION_NAME );
            }
        }

        // ------------------------------------------------------------
        // 2-4. ���̺� �����̽� validation
        // ------------------------------------------------------------
        IDE_TEST( qdtCommon::getAndValidateTBSOfIndexPartition( aStatement,
                                                sPartitionInfo->TBSID,
                                                sPartitionInfo->TBSType,
                                                sPartAttr->TBSName,
                                                aTableInfo->tableOwnerID,
                                                & sPartAttr->TBSAttr.mID,
                                                & sPartAttr->TBSAttr.mType )
                  != IDE_SUCCESS );
    }

    // ���̺� ��Ƽ�� ����
    IDE_TEST( qcmPartition::getPartitionCount( aStatement,
                                               aTableInfo->tableID,
                                               & sTablePartCount )
              != IDE_SUCCESS );

    // ------------------------------------------------------------
    // 3. ������ �ε��� ��Ƽ���� ���� üũ
    // ------------------------------------------------------------
    IDE_TEST_RAISE( sIndexPartCount > sTablePartCount,
                    ERR_INDEX_PARTITION_COUNT );

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_DUPLICATE_PARTITION_NAME)
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDB_DUPLICATE_PARTITION_NAME,
                                sqlInfo.getErrMessage()));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION(ERR_TBS_POSITION_OF_LOCAL_PARTITIONED_INDEX)
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(ideSetErrorCode(
                    qpERR_ABORT_QDX_CANNOT_SPECIFY_TBS_OF_LOCAL_PARTITIONED_INDEX,
                    sqlInfo.getErrMessage()) );
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION(ERR_NOT_EXIST_TABLE_PARTITION)
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(ideSetErrorCode( qpERR_ABORT_QCM_NOT_EXIST_TABLE_PARTITION,
                                 sqlInfo.getErrMessage()) );
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION(ERR_INDEX_PARTITION_COUNT)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDX_INVALID_INDEX_PARTITION_COUNT));
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdx::validatePartitionedIndexOnCreateTable(
    qcStatement        * aStatement,
    qdTableParseTree   * aParseTree,
    qcNamePosition       aPartIndexTBSName,
    qdPartitionedIndex * aPartIndex )
{
/***********************************************************************
 *
 * Description :
 *      fix BUG-18937
 *
 *      ��Ƽ�ǵ� ���̺� ���� ALTER TABLE ��,
 *      PRIMARY KEY, UNIQUE, LOCAL UNIQUE�� ������ ���
 *      �� �ε��� ��Ƽ�ǿ� ���ؼ� TABLESPACE�� ���� ������ �� �ִ�.
 *
 *      ex)
 *          CREATE TABLE T1
 *          (
 *              I1 INTEGER PRIMARY KEY USING INDEX
 *                              LOCAL(
 *                                      PARTITION P1_PRI ON P1 TABLESPACE TBS6,
 *                                      PARTITION P2_PRI ON P2 TABLESPACE TBS7,
 *                                      PARTITION P3_PRI ON P3 TABLESPACE TBS8
 *                                   ),
 *              I2 INTEGER
 *          )
 *          PARTITION BY RANGE(I1)
 *          (
 *              PARTITION P1 VALUES LESS THAN (100) TABLESPACE TBS1,
 *              PARTITION P2 VALUES LESS THAN (200) TABLESPACE TBS2,
 *              PARTITION P3 VALUES DEFAULT TABLESPACE TBS3
 *          ) TABLESPACE TBS4;
 *
 *
 * Implementation :
 *      1. ���� �ε����̸鼭 PARTITIONED INDEX�� TBS�� ���� ��, ����
 *         ex) ALTER TABLE T1 ADD COLUMN ( I2 INTEGER PRIMARY KEY
 *                                         USING INDEX TABLESPACE TBS1
 *                                         LOCAL );
 *
 *      2. ������ �ε��� ��Ƽ�� ���� üũ
 *         ���̺� ��Ƽ���� ���������� ������ �� �ִ�.
 *
 *      3. ������ �ε��� ��Ƽ�� ������ŭ �ݺ�
 *          3-1. �ε��� ��Ƽ�� �̸� validation
 *          3-2. ������ ���̺� ��Ƽ���� �����ϴ��� üũ
 *          3-3. ���̺� ��Ƽ�� �̸� validation
 *          3-4. ���̺� �����̽� validation
 *
 *
 ***********************************************************************/

    qdPartitionAttribute    * sTablePartAttr;
    qdPartitionAttribute    * sIndexPartAttr;
    qdPartitionAttribute    * sTempPartAttr;
    UInt                      sIndexPartCount = 0;
    qcuSqlSourceInfo          sqlInfo;
    idBool                    sIsFound = ID_FALSE;


    // ------------------------------------------------------------
    // 1. ���� �ε��� ���� ��, ��Ƽ�ǵ� �ε����� TBS�� ���� �� ����
    //  CREATE TABLE T1
    //  (
    //      I1 INTEGER PRIMARY KEY USING INDEX TABLESPACE TBS5,
    //      I2 INTEGER
    //  )
    //  PARTITION BY RANGE(I1)
    //  (
    //      PARTITION P1 VALUES LESS THAN (100) TABLESPACE TBS1,
    //      PARTITION P2 VALUES LESS THAN (200) TABLESPACE TBS2,
    //      PARTITION P3 VALUES DEFAULT TABLESPACE TBS3
    //  ) TABLESPACE TBS4;
    // ------------------------------------------------------------
    if( QC_IS_NULL_NAME( aPartIndexTBSName ) != ID_TRUE )
    {
        sqlInfo.setSourceInfo(aStatement,
                              & aPartIndexTBSName);

        IDE_RAISE( ERR_TBS_POSITION_OF_LOCAL_PARTITIONED_INDEX );
    }
    else
    {
        // Nothing to do
    }

    // ������ �ε��� ��Ƽ�� ����
    for( sIndexPartCount = 0, sIndexPartAttr = aPartIndex->partAttr;
         sIndexPartAttr != NULL;
         sIndexPartCount++, sIndexPartAttr = sIndexPartAttr->next ) ;

    // ------------------------------------------------------------
    // 2. ������ �ε��� ��Ƽ���� ���� üũ
    // ------------------------------------------------------------
    IDE_TEST_RAISE( sIndexPartCount > aParseTree->partTable->partCount,
                    ERR_INDEX_PARTITION_COUNT );


    // ------------------------------------------------------------
    // 3. ������ �ε��� ������ŭ �ݺ��ϸ� validation
    // ------------------------------------------------------------
    for( sIndexPartCount = 0, sIndexPartAttr = aPartIndex->partAttr;
         sIndexPartAttr != NULL;
         sIndexPartCount++, sIndexPartAttr = sIndexPartAttr->next )
    {
        // ------------------------------------------------------------
        // 3-1. �ε��� ��Ƽ�� �̸� �ߺ� �˻�
        // ------------------------------------------------------------
        for( sTempPartAttr = aPartIndex->partAttr;
             sTempPartAttr != sIndexPartAttr;
             sTempPartAttr = sTempPartAttr->next )
        {
            if ( QC_IS_NAME_MATCHED( sTempPartAttr->indexPartName, sIndexPartAttr->indexPartName ) )
            {
                sqlInfo.setSourceInfo(aStatement,
                                      & sIndexPartAttr->indexPartName );
                IDE_RAISE( ERR_DUPLICATE_PARTITION_NAME );
            }
        }

        // ------------------------------------------------------------
        // 3-2. �ε����� �����Ϸ��� ��Ƽ�ǵ� ���̺�
        //      ������ ���̺� ��Ƽ���� �����ϴ��� üũ.
        // ------------------------------------------------------------
        sIsFound = ID_FALSE;

        for( sTablePartAttr = aParseTree->partTable->partAttr;
             sTablePartAttr != NULL;
             sTablePartAttr = sTablePartAttr->next )
        {
            sTempPartAttr = sTablePartAttr;

            if ( QC_IS_NAME_MATCHED( sTablePartAttr->tablePartName, sIndexPartAttr->tablePartName ) )
            {
                sIsFound = ID_TRUE;
                break;
            }
        }

        if( sIsFound == ID_FALSE )
        {
            sqlInfo.setSourceInfo(aStatement,
                                  & sIndexPartAttr->tablePartName );
            IDE_RAISE( ERR_NOT_EXIST_TABLE_PARTITION );
        }

        // ------------------------------------------------------------
        // 3-3. ���̺� ��Ƽ���� �̸��� �ߺ� �˻�
        // ------------------------------------------------------------
        for( sTablePartAttr = aPartIndex->partAttr;
             sTablePartAttr != sIndexPartAttr;
             sTablePartAttr = sTablePartAttr->next )
        {
            if ( QC_IS_NAME_MATCHED( sTablePartAttr->tablePartName, sIndexPartAttr->tablePartName ) )
            {
                sqlInfo.setSourceInfo(aStatement,
                                      & sIndexPartAttr->tablePartName );
                IDE_RAISE( ERR_DUPLICATE_PARTITION_NAME );
            }
        }

        // ------------------------------------------------------------
        // 3-4. ���̺� �����̽� validation
        // ------------------------------------------------------------
        IDE_TEST( qdtCommon::getAndValidateTBSOfIndexPartition( aStatement,
                                        sTempPartAttr->TBSAttr.mID,
                                        sTempPartAttr->TBSAttr.mType,
                                        sIndexPartAttr->TBSName,
                                        aParseTree->userID,
                                        & sIndexPartAttr->TBSAttr.mID,
                                        & sIndexPartAttr->TBSAttr.mType )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_DUPLICATE_PARTITION_NAME)
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDB_DUPLICATE_PARTITION_NAME,
                                sqlInfo.getErrMessage()));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION(ERR_TBS_POSITION_OF_LOCAL_PARTITIONED_INDEX)
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(ideSetErrorCode(
                    qpERR_ABORT_QDX_CANNOT_SPECIFY_TBS_OF_LOCAL_PARTITIONED_INDEX,
                    sqlInfo.getErrMessage()) );
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION(ERR_NOT_EXIST_TABLE_PARTITION)
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(ideSetErrorCode( qpERR_ABORT_QCM_NOT_EXIST_TABLE_PARTITION,
                                 sqlInfo.getErrMessage()) );
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION(ERR_INDEX_PARTITION_COUNT)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDX_INVALID_INDEX_PARTITION_COUNT));
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdx::validateNonPartitionedIndex(
    qcStatement     * aStatement,
    qcNamePosition    aUserName,
    qcNamePosition    aIndexName,
    SChar           * aIndexTableName,
    SChar           * aKeyIndexName,
    SChar           * aRidIndexName )
{
/***********************************************************************
 *
 * Description : PROJ-1624 global non-partitioned index
 *      ��Ƽ�ǵ� ���̺� ���� non-partitioned index������ index table��
 *      �����Ѵ�. �̶� index table�� ������������ �˻��Ѵ�.
 *
 * Implementation :
 *      index table name �˻�
 *
 ***********************************************************************/

    // index table name ���� & �˻�
    if ( QC_IS_NULL_NAME(aIndexName) == ID_FALSE )
    {
        IDE_TEST( checkIndexTableName( aStatement,
                                       aUserName,
                                       aIndexName,
                                       aIndexTableName,
                                       aKeyIndexName,
                                       aRidIndexName )
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

/***********************************************************************
 *
 * Description :
 *    PROJ-2461 pk, uk constraint���� prefix index ���� ��ȭ
 *    partitioned index�� partition key�� pk/uk�� constraint column�� ���� ���ԵǴ��� üũ
 *    �ش� ������ local index�� ���̺� ��ü�� uniqueness�� ������ �� �ִ� ��������
 *    �̸� �����ؾ� local prefixed/non-prefixed index�� PK/UK�� ����� �� �ִ�.
 *
 * Implementation :
 *
 ***********************************************************************/
IDE_RC qdx::checkLocalIndexOnCreateTable( qcmColumn   * aIndexKeyColumns,
                                          qcmColumn   * aPartKeyColumns,
                                          idBool      * aIsLocalIndex )
{
    qcmColumn * sPartKeyColumn;
    qcmColumn * sIndexKeyColumn;
    idBool      sFound = ID_FALSE;

    for ( sPartKeyColumn = aPartKeyColumns;
          sPartKeyColumn != NULL;
          sPartKeyColumn = sPartKeyColumn->next )
    {
        sFound = ID_FALSE;

        for ( sIndexKeyColumn = aIndexKeyColumns;
              sIndexKeyColumn != NULL;
              sIndexKeyColumn = sIndexKeyColumn->next )
        {
            if ( QC_IS_NAME_MATCHED( sIndexKeyColumn->namePos, sPartKeyColumn->namePos ) )
            {
                sFound = ID_TRUE;
                break;
            }
            else
            {
                /* Nothing to do */
            }
        }

        if ( sFound == ID_FALSE )
        {
            break;
        }
        else
        {
            /* Nothing to do */
        }
    }

    *aIsLocalIndex = sFound;

    return IDE_SUCCESS;
}

/***********************************************************************
 *
 * Description :
 *    PROJ-2461 pk, uk constraint���� prefix index ���� ��ȭ
 *    partitioned index�� partition key�� pk/uk�� constraint column�� ���� ���ԵǴ��� üũ
 *    �ش� ������ local index�� ���̺� ��ü�� uniqueness�� ������ �� �ִ� ��������
 *    �̸� �����ؾ� local prefixed/non-prefixed index�� PK/UK�� ����� �� �ִ�.
 *
 * Implementation :
 *
 ***********************************************************************/
IDE_RC qdx::checkLocalIndexOnAlterTable( qcStatement  * aStatement,
                                         qcmTableInfo * aTableInfo,
                                         qcmColumn    * aIndexKeyColumns,
                                         qcmColumn    * aPartKeyColumns,
                                         idBool       * aIsLocalIndex )
{
    qcmColumn * sPartKeyColumn;
    qcmColumn * sIndexKeyColumn;
    qcmColumn * sIndexKeyColumnInfo;
    qcmColumn * sPartKeyColumnInfo;
    idBool      sFound = ID_FALSE;

    IDE_DASSERT( aTableInfo != NULL );

    for ( sPartKeyColumn = aPartKeyColumns;
          sPartKeyColumn != NULL;
          sPartKeyColumn = sPartKeyColumn->next )
    {
        sFound = ID_FALSE;

        IDE_TEST( qcmCache::getColumnByID( aTableInfo,
                                           sPartKeyColumn->basicInfo->column.id,
                                           &sPartKeyColumnInfo )
                  != IDE_SUCCESS );

        for ( sIndexKeyColumn = aIndexKeyColumns;
              sIndexKeyColumn != NULL;
              sIndexKeyColumn = sIndexKeyColumn->next )
        {
            if ( qcmCache::getColumn( aStatement,
                                      aTableInfo,
                                      sIndexKeyColumn->namePos,
                                      &sIndexKeyColumnInfo )
                 != IDE_SUCCESS )
            {
                /* ALTER TABLE ADD COLUMN
                 * ADD COLUMN������ inline constraint �ۿ� ������ �����Ƿ�
                 * part key column�� constraint key column�� ������ ����ġ.
                 * �� sFound�� ��� ID_FALSE�� �ȴ�.
                 */
                sFound = ID_FALSE;
                break;
            }
            else
            {
                /* ALTER TABLE ADD CONSTRAINTS */
                if ( idlOS::strMatch( sIndexKeyColumnInfo->name,
                                      idlOS::strlen( sIndexKeyColumnInfo->name ),
                                      sPartKeyColumnInfo->name,
                                      idlOS::strlen( sPartKeyColumnInfo->name ) ) == 0 )
                {
                    sFound = ID_TRUE;
                    break;
                }
                else
                {
                    /* Nothing to do */
                }
            }
        }

        if ( sFound == ID_FALSE )
        {
            break;
        }
        else
        {
            /* Nothing to do. */
        }
    }

    *aIsLocalIndex = sFound;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdx::validateKeySizeLimit(
    qcStatement      * /* aStatement */,
    iduVarMemList    * aMemory,
    UInt               aTableType,
    void             * aKeyColumns,
    UInt               aKeyColCount,
    UInt               aIndexType)
{
/***********************************************************************
 *
 * Description :
 *    key size limit �˻�
 *
 * Implementation :
 *    1. key size estimation : smiEstimateMaxKeySize
 *    2. system key size limit : smiGetKeySizeLimit
 *    3. if 1 > 2 then error
 *
 *    �� �Լ���
 *    (1) validation �ܰ迡���� �Ϲ����̺� ���� ��������ó����
 *    (2) execution �ܰ迡���� TEMP ���̺� ���� �ε��� ������
 *    key size limit �˻縦 ���� ȣ��ȴ�.
 *
 *    ����, �Է� ���� �� aMemory �� aKeyColumns ��
 *    �Ϲ� ���̺�� TEMP ���̺� ���� �ٸ��� ó���� �־�� ��.
 *
 *      A.  void * aKeyColumns �� ��쿡 ���� ������ ����
 *          �Լ� �����Ͱ� �Ѿ��.
 *         . �Ϲ� ���̺� ���� constraint ������ ���� qtcColumn *
 *         . TEMP ���̺� ���� �ε��� �����ô� mtcColumn *
 *
 *      B. iduMemory * aMemory �� �� �ܰ迡 ���� �޸� ������ ���е�.
 *         . prepare�ܰ��    statement->qmpMem
 *         . execution �ܰ�� statement->qmxMem
 *
 ***********************************************************************/

#define IDE_FN "qdx::validateKeySizeLimit"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qdx::validateKeySizeLimit"));

    UInt                  i;
    UInt                  sOffset;
    UInt                  sEstimateMaxKeySize = 0;
    UInt                  sSystemKeySizeLimit = 0;
    smiColumn           * sKeyColumn;
    qcmColumn           * sKeyQcmCol;
    UInt                * sMaxLengths;

    if( aTableType == SMI_TABLE_DISK ) 
    {
        // Key Column ������ ���� ���� �Ҵ�
        IDU_LIMITPOINT("qdx::validateKeySizeLimit::malloc3");
        IDE_TEST( aMemory->alloc(ID_SIZEOF(smiColumn) * aKeyColCount,
                                 (void**) & sKeyColumn ) != IDE_SUCCESS );

        // Column Precision ������ ���� ���� �Ҵ�
        IDU_LIMITPOINT("qdx::validateKeySizeLimit::malloc4");
        IDE_TEST( aMemory->alloc(ID_SIZEOF(UInt) * aKeyColCount,
                                 (void**) & sMaxLengths ) != IDE_SUCCESS );

        //----------------------------------------------
        // �Ϲ� ���̺� ���� index key size limit check
        //----------------------------------------------

        sOffset = 0;
        for ( i = 0,
              sKeyQcmCol = (qcmColumn *)aKeyColumns;
              ( i < aKeyColCount ) && ( sKeyQcmCol != NULL );
              i++,
              sKeyQcmCol = sKeyQcmCol->next)
        {
            idlOS::memcpy( &(sKeyColumn[i]),
                           sKeyQcmCol->basicInfo, ID_SIZEOF(smiColumn) );
            sMaxLengths[i] = sKeyQcmCol->basicInfo->column.size;

            // To Fix PR-8111
            if ( ( sKeyQcmCol->basicInfo->column.flag & SMI_COLUMN_TYPE_MASK )
                 == SMI_COLUMN_TYPE_FIXED )
            {
                sOffset =
                    idlOS::align(
                        sOffset,
                        sKeyQcmCol->basicInfo->module->align );
                sKeyColumn[i].offset = sOffset;
                sKeyColumn[i].value = NULL;
                sOffset += sKeyQcmCol->basicInfo->column.size;
            }
            else
            {
                sOffset = idlOS::align( sOffset, 8 );
                sKeyColumn[i].offset = sOffset;
                sKeyColumn[i].value = NULL;
                sOffset += smiGetVariableColumnSize4DiskIndex();
            }
        }

        sEstimateMaxKeySize = smiEstimateMaxKeySize( aKeyColCount,
                                                     sKeyColumn,
                                                     sMaxLengths  );

        sSystemKeySizeLimit =
            smiGetIndexKeySizeLimit( aTableType, aIndexType );

        IDE_TEST_RAISE( sEstimateMaxKeySize > sSystemKeySizeLimit,
                        ERR_MAXIMUM_KEY_SIZE_EXCEED );
    }
    else if( ( aTableType == SMI_TABLE_MEMORY ) || ( aTableType == SMI_TABLE_VOLATILE ) )
    {
        // BUG-23113
        // �� column�� ũ�Ⱑ key size limit�� �����ؾ� �Ѵ�.

        //----------------------------------------------
        // �Ϲ� ���̺� ���� index key size limit check
        //----------------------------------------------

        for ( i = 0,
                  sKeyQcmCol = (qcmColumn *)aKeyColumns;
              ( i < aKeyColCount ) && ( sKeyQcmCol != NULL );
              i++,
                  sKeyQcmCol = sKeyQcmCol->next)
        {
            sEstimateMaxKeySize = smiEstimateMaxKeySize(
                1,
                & sKeyQcmCol->basicInfo->column,
                (UInt*) & sKeyQcmCol->basicInfo->column.size );

            sSystemKeySizeLimit =
                smiGetIndexKeySizeLimit( aTableType, aIndexType );

            IDE_TEST_RAISE( sEstimateMaxKeySize > sSystemKeySizeLimit,
                            ERR_MAXIMUM_KEY_SIZE_EXCEED );
        }
    }
    else
    {
        // Nothing to do.
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_MAXIMUM_KEY_SIZE_EXCEED)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDX_MAXIMUM_KEY_SIZE_EXCEED));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qdx::validateAlter(qcStatement * aStatement)
{
/***********************************************************************
 *
 * Description :
 *    ALTER INDEX ... �� validation ����
 *
 * Implementation :
 *    1. �����ϴ� �ε������� üũ, table ID, index ID ã��
 *    2. table ID �� qcmTableInfo ã��
 *    3. AlterIndex ������ �ִ��� üũ
 *
 ***********************************************************************/

#define IDE_FN "qdx::validateAlter"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qdx::validate"));

    qdIndexParseTree     * sParseTree         = NULL;
    UInt                   sIndexID           = 0;
    UInt                   sTableID           = 0;
    qcmIndex             * sIndex             = NULL;
    SInt                   sCountDiskPart     = 0;
    SInt                   sCountVolatilePart = 0;
    UInt                   sTableType         = 0;

    sParseTree = (qdIndexParseTree *)aStatement->myPlan->parseTree;

    // if index does not exists, raise error
    IDE_TEST(qcm::checkIndexByUser(
                 aStatement,
                 sParseTree->userNameOfIndex,
                 sParseTree->indexName,
                 &(sParseTree->userIDOfIndex),
                 &sTableID,
                 &sIndexID)
             != IDE_SUCCESS);

    /* BUG-48290 shard object�� ���� DDL ���� */
    IDE_TEST( sdi::checkShardObjectForDDL( aStatement, SDI_DDL_TYPE_INDEX ) != IDE_SUCCESS );

    IDE_TEST(qcm::getTableInfoByID(aStatement,
                                   sTableID,
                                   &(sParseTree->tableInfo),
                                   &(sParseTree->tableSCN),
                                   &(sParseTree->tableHandle))
             != IDE_SUCCESS);

    IDE_TEST(qcm::lockTableForDDLValidation(aStatement,
                                            sParseTree->tableHandle,
                                            sParseTree->tableSCN)
             != IDE_SUCCESS);

    /* PROJ-2464 hybrid partitioned table ���� */
    sTableType = sParseTree->tableInfo->tableFlag & SMI_TABLE_TYPE_MASK;

    // check grant
    IDE_TEST( qdpRole::checkDDLAlterIndexPriv( aStatement,
                                               sParseTree->tableInfo,
                                               sParseTree->userIDOfIndex )
              != IDE_SUCCESS );
    
    /*
     * BUG-31517 alter index �� index �� disk index �ε��� �Ʒ��� ������ ������:
     *
     *              ALTER INDEX IDX SET PERSISTENT = ON;
     *                                  ^^^^^^^^^^^^^^^
     *           ��Ģ�� �����ϸ� �ȵ�.
     *
     * BUGBUG : �׷���, ���⼭ SET PERSISTENT=OFF �� �̼������δ� �����ϸ� �ȵǳ� �������ν��
     *          ����ٰ� OFF �� �������� �ľ��� ����� �����Ƿ� �׳� �н�.
     *          BUG-31517 �� ���󰡴� ���� �� ������ ���õ� ���װ� ����.
     */
    if (smiIsDiskTable(sParseTree->tableHandle) == ID_TRUE)
    {
        IDE_TEST_RAISE((sParseTree->flag & SMI_INDEX_PERSISTENT_MASK) ==
                       SMI_INDEX_PERSISTENT_ENABLE,
                       ERR_IRREGULAR_PERSISTENT_OPTION);
    }
    else
    {
        /* nothing to do */
    }

    if ( sParseTree->tableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
    {
        IDE_TEST( qdbCommon::checkAndSetAllPartitionInfo( aStatement,
                                                          sParseTree->tableInfo->tableID,
                                                          & sParseTree->partIndex->partInfoList )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    /* PROJ-2464 hybrid partitioned table ����
     *  - HPT �� ��쿡, Memory, Disk ��ü�� ��� ���� �� �ִ�.
     *  - Partitioned �� ����ϴ� ó���� �߰��Ѵ�.
     *     1. ��� Index�� ������ �����´�.
     *     2. Partition ������ �����´�.
     *       2.1. PROJ-1624 non-partitioned index�� ����ó���Ѵ�.
     *       2.2. Pre-pruned Partition�� ���ؼ��� ������� �ʴ´�.
     *       2.3. Partition ������ �����´�.
     *     3. Memory Type�� Persistent �ɼ��� �����Ѵ�.
     */
    if ( ( sParseTree->flag & SMI_INDEX_PERSISTENT_MASK ) == SMI_INDEX_PERSISTENT_ENABLE )
    {
        /* 1. ��� Index�� ������ �����´�. */
        IDE_TEST( qcmCache::getIndex( sParseTree->tableInfo,
                                      sParseTree->indexName,
                                      & sIndex )
                  != IDE_SUCCESS );

        /* 2. Partition ������ �����´�. */
        if ( sParseTree->tableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
        {
            /* 2.1. PROJ-1624 non-partitioned index�� ����ó���Ѵ�. */
            IDE_TEST_RAISE( sIndex->indexPartitionType == QCM_NONE_PARTITIONED_INDEX,
                            ERR_IRREGULAR_PERSISTENT_OPTION );

            /* 2.2. Pre-pruned Partition�� ���ؼ��� ������� �ʴ´�. */
            IDE_DASSERT( ( sIndex->indexPartitionType != QCM_LOCAL_INDEX_PARTITION ) ||
                         ( sIndex->indexPartitionType != QCM_GLOBAL_INDEX_PARTITION ) );

            /* 2.3. Partition ������ �����´�. */
            qdbCommon::getTableTypeCountInPartInfoList( & sTableType,
                                                        sParseTree->partIndex->partInfoList,
                                                        & sCountDiskPart,
                                                        NULL,
                                                        & sCountVolatilePart );

            /* 3. Memory Type�� Persistent �ɼ��� �����Ѵ�. */
            IDE_TEST_RAISE( ( sCountDiskPart + sCountVolatilePart ) > 0,
                            ERR_IRREGULAR_PERSISTENT_OPTION );
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

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_IRREGULAR_PERSISTENT_OPTION)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDX_NON_MEMORY_INDEX_PERSISTENT_OPTION));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

/*
 * -----------------------------------------------------------------------------
 * Description :
 *    ALTER INDEX ... ALLOCATE EXTENT ... �� validation
 *
 * Implementation :
 *      ���� ��ƾ�� qdx::validateAlter() �� ������ ��,
 *      Index �� ��ũ �ε������� üũ
 * -----------------------------------------------------------------------------
 */
IDE_RC qdx::validateAlterAllocExtent( qcStatement *aStatement )
{
    qdIndexParseTree * sParseTree = NULL;
    qcmIndex         * sIndex     = NULL;
    UInt               sIndexID   = 0;
    UInt               sTableID   = 0;

    /* PROJ-2464 hybrid partitioned table ����
     *  - HPT �� ��쿡, Memory, Disk ��ü�� ��� ���� �� �ִ�.
     *  - ���� Memory ��ü�� ���ԵǾ �ش� �ɼ��� ����� �� �ִ�.
     *  - Partitioned �� ����ϴ� ó���� �߰��Ѵ�.
     */

    sParseTree = (qdIndexParseTree *)aStatement->myPlan->parseTree;

    /* 1. Index�� �����ϴ��� Ȯ���Ѵ�. */
    IDE_TEST( qcm::checkIndexByUser( aStatement,
                                     sParseTree->userNameOfIndex,
                                     sParseTree->indexName,
                                     &( sParseTree->userIDOfIndex ),
                                     & sTableID,
                                     & sIndexID )
              != IDE_SUCCESS );

    /* 2. Table Info�� �����´�. */
    IDE_TEST( qcm::getTableInfoByID( aStatement,
                                     sTableID,
                                     &( sParseTree->tableInfo ),
                                     &( sParseTree->tableSCN ),
                                     &( sParseTree->tableHandle ) )
              != IDE_SUCCESS );

    /* 3. Validation Lock�� �����Ѵ�. */
    IDE_TEST( qcm::lockTableForDDLValidation( aStatement,
                                              sParseTree->tableHandle,
                                              sParseTree->tableSCN )
              != IDE_SUCCESS );

    /* 4. Index ������ �˻��Ѵ�. */
    IDE_TEST( qdpRole::checkDDLAlterIndexPriv( aStatement,
                                               sParseTree->tableInfo,
                                               sParseTree->userIDOfIndex )
              != IDE_SUCCESS );

    /* BUG-29382 ALTER INDEX �� ALLOCATE ������ disk index �� ���ؼ��� �� �� ������,
     * �ش� ������ sm �� �ƴ� qp ���� ���� ��� ��.
     */
    IDE_TEST_RAISE( smiIsDiskTable( sParseTree->tableHandle ) != ID_TRUE,
                    ERR_NO_DISK_INDEX );

    /* 5. ��� Index�� ������ �����´�. */
    IDE_TEST( qcmCache::getIndex( sParseTree->tableInfo,
                                  sParseTree->indexName,
                                  & sIndex )
              != IDE_SUCCESS );

    /* 6. Partition ������ �����´�. */
    if ( sParseTree->tableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
    {
        if ( sIndex->indexPartitionType != QCM_NONE_PARTITIONED_INDEX )
        {
            /* 6.1. Pre-pruned Partition�� ���ؼ��� ������� �ʴ´�. */
            IDE_DASSERT( ( sIndex->indexPartitionType != QCM_LOCAL_INDEX_PARTITION ) ||
                         ( sIndex->indexPartitionType != QCM_GLOBAL_INDEX_PARTITION ) );

            /* 6.2. Partition List�� �����´�. */
            IDE_TEST( qdbCommon::checkAndSetAllPartitionInfo(
                          aStatement,
                          sParseTree->tableInfo->tableID,
                          &( sParseTree->partIndex->partInfoList ) )
                      != IDE_SUCCESS );
        }
        else
        {
            /* PROJ-1624 non-partitioned index */
            IDE_TEST( qdx::makeAndLockIndexTable(
                          aStatement,
                          ID_FALSE,
                          sIndex->indexTableID,
                          &( sParseTree->oldIndexTables ) )
                      != IDE_SUCCESS );
        }
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NO_DISK_INDEX )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDB_NO_DISK_INDEX ) );
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdx::validateAlterSegAttr( qcStatement * aStatement )
{
/***********************************************************************
 *
 * Description :
 *    ALTER INDEX ... �� validation ����
 *
 * Implementation :
 *    1. �����ϴ� �ε������� üũ, table ID, index ID ã��
 *    2. table ID �� qcmTableInfo ã��
 *    3. AlterIndex ������ �ִ��� üũ
 *    4. ���̺� ����ȭ�� �ɷ������� ���� ��ȯ
 *
 ***********************************************************************/

    qdIndexParseTree * sParseTree = NULL;
    qcmIndex         * sIndex     = NULL;
    UInt               sIndexID   = 0;
    UInt               sTableID   = 0;
    UInt               sTableType = 0;
    smiSegAttr         sSrcSegAttr;

    /* PROJ-2464 hybrid partitioned table ����
     *  - HPT �� ��쿡, Memory, Disk ��ü�� ��� ���� �� �ִ�.
     *  - ���� Memory ��ü�� ���ԵǾ �ش� �ɼ��� ����� �� �ִ�.
     *  - Partitioned �� ����ϴ� ó���� �߰��Ѵ�.
     */

    sParseTree = (qdIndexParseTree *)aStatement->myPlan->parseTree;

    /* 1. Index�� �����ϴ��� Ȯ���Ѵ�. */
    IDE_TEST( qcm::checkIndexByUser( aStatement,
                                     sParseTree->userNameOfIndex,
                                     sParseTree->indexName,
                                     &( sParseTree->userIDOfIndex ),
                                     & sTableID,
                                     & sIndexID )
              != IDE_SUCCESS );

    /* 2. Table Info�� �����´�. */
    IDE_TEST( qcm::getTableInfoByID( aStatement,
                                     sTableID,
                                     &( sParseTree->tableInfo ),
                                     &( sParseTree->tableSCN ),
                                     &( sParseTree->tableHandle ) )
              != IDE_SUCCESS );

    /* 3. Validation Lock�� �����Ѵ�. */
    IDE_TEST( qcm::lockTableForDDLValidation( aStatement,
                                              sParseTree->tableHandle,
                                              sParseTree->tableSCN )
              != IDE_SUCCESS );

    sTableType = sParseTree->tableInfo->tableFlag & SMI_TABLE_TYPE_MASK;

    /* 4. Index ������ �˻��Ѵ�. */
    IDE_TEST( qdpRole::checkDDLAlterIndexPriv( aStatement,
                                               sParseTree->tableInfo,
                                               sParseTree->userIDOfIndex )
              != IDE_SUCCESS );

    IDE_TEST_RAISE( smiIsDiskTable( sParseTree->tableHandle ) != ID_TRUE,
                    ERR_NO_DISK_INDEX );

    /* 5. ��� Index�� ������ �����´�. */
    IDE_TEST( qcmCache::getIndex( sParseTree->tableInfo,
                                  sParseTree->indexName,
                                  & sIndex )
             != IDE_SUCCESS );

    /* 6. ���� SegAttr �ɼ��� �����´�. */
    sSrcSegAttr = smiTable::getIndexSegAttr( sIndex->indexHandle );

    /* 7. Partition ������ �����´�.. */
    if ( sParseTree->tableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
    {
        if ( sIndex->indexPartitionType != QCM_NONE_PARTITIONED_INDEX )
        {
            /* 7.1. Pre-pruned Partition�� ���ؼ��� ������� �ʴ´�. */
            IDE_DASSERT( ( sIndex->indexPartitionType != QCM_LOCAL_INDEX_PARTITION ) ||
                         ( sIndex->indexPartitionType != QCM_GLOBAL_INDEX_PARTITION ) );

            /* 7.2. Partition List�� �����´�. */
            IDE_TEST( qdbCommon::checkAndSetAllPartitionInfo( aStatement,
                                                              sParseTree->tableInfo->tableID,
                                                              &( sParseTree->partIndex->partInfoList ) )
                      != IDE_SUCCESS );
        }
        else
        {
            /* PROJ-1624 non-partitioned index */
            IDE_TEST( qdx::makeAndLockIndexTable( aStatement,
                                                  ID_FALSE,
                                                  sIndex->indexTableID,
                                                  &( sParseTree->oldIndexTables ) )
                      != IDE_SUCCESS );
        }
    }
    else
    {
        /* Nothing to do */
    }

    /* 8. SegAttr �ɼ��� �����Ѵ�. */
    IDE_TEST( qdbCommon::validateAndSetSegAttr( sTableType,
                                                & sSrcSegAttr,
                                                & ( sParseTree->segAttr ),
                                                ID_FALSE )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NO_DISK_INDEX )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDB_NO_DISK_INDEX ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
 * -----------------------------------------------------------------------------
 * Description :
 *    ALTER INDEX ... STORAGE ... �� validation ����
 *
 * Implementation :
 *      qdx::validateAlter �Լ��� ALTER INDEX �� ��������
 *      validation ���� ��, Index �� ��ũ �ε������� üũ
 * -----------------------------------------------------------------------------
 */
IDE_RC qdx::validateAlterSegStoAttr( qcStatement *aStatement )
{
    qdIndexParseTree  * sParseTree = NULL;
    qcmIndex          * sIndex     = NULL;
    UInt                sIndexID   = 0;
    UInt                sTableID   = 0;
    UInt                sTableType = 0;
    smiSegStorageAttr   sSrcSegStoAttr;

    /* PROJ-2464 hybrid partitioned table ����
     *  - HPT �� ��쿡, Memory, Disk ��ü�� ��� ���� �� �ִ�.
     *  - ���� Memory ��ü�� ���ԵǾ �ش� �ɼ��� ����� �� �ִ�.
     *  - Partitioned �� ����ϴ� ó���� �߰��Ѵ�.
     */

    sParseTree = (qdIndexParseTree *)aStatement->myPlan->parseTree;

    /* 1. Index�� �����ϴ��� Ȯ���Ѵ�. */
    IDE_TEST( qcm::checkIndexByUser(
                  aStatement,
                  sParseTree->userNameOfIndex,
                  sParseTree->indexName,
                  &( sParseTree->userIDOfIndex ),
                  & sTableID,
                  & sIndexID )
              != IDE_SUCCESS );

    /* 2. Table Info�� �����´�. */
    IDE_TEST( qcm::getTableInfoByID( aStatement,
                                     sTableID,
                                     &( sParseTree->tableInfo ),
                                     &( sParseTree->tableSCN ),
                                     &( sParseTree->tableHandle ) )
              != IDE_SUCCESS );

    /* 3. Validation Lock�� �����Ѵ�. */
    IDE_TEST( qcm::lockTableForDDLValidation( aStatement,
                                              sParseTree->tableHandle,
                                              sParseTree->tableSCN )
              != IDE_SUCCESS );

    sTableType = sParseTree->tableInfo->tableFlag & SMI_TABLE_TYPE_MASK;

    /* 4. Index ������ �˻��Ѵ�. */
    IDE_TEST( qdpRole::checkDDLAlterIndexPriv( aStatement,
                                               sParseTree->tableInfo,
                                               sParseTree->userIDOfIndex )
              != IDE_SUCCESS );

    IDE_TEST_RAISE( smiIsDiskTable( sParseTree->tableHandle ) != ID_TRUE,
                    ERR_NO_DISK_INDEX );

    /* 5. ��� Index�� ������ �����´�. */
    IDE_TEST( qcmCache::getIndex( sParseTree->tableInfo,
                                  sParseTree->indexName,
                                  & sIndex )
              != IDE_SUCCESS );

    /* 6. ���� SegStoAttr �ɼ��� �����´�. */
    sSrcSegStoAttr = smiTable::getIndexSegStoAttr( sIndex->indexHandle );

    /* 7. Partition ������ �����´�.. */
    if ( sParseTree->tableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
    {
        if ( sIndex->indexPartitionType != QCM_NONE_PARTITIONED_INDEX )
        {
            /* 7.1. Pre-pruned Partition�� ���ؼ��� ������� �ʴ´�. */
            IDE_DASSERT( ( sIndex->indexPartitionType != QCM_LOCAL_INDEX_PARTITION ) ||
                         ( sIndex->indexPartitionType != QCM_GLOBAL_INDEX_PARTITION ) );

            /* 7.2. Partition List�� �����´�. */
            IDE_TEST( qdbCommon::checkAndSetAllPartitionInfo( aStatement,
                                                              sParseTree->tableInfo->tableID,
                                                              &( sParseTree->partIndex->partInfoList ) )
                      != IDE_SUCCESS );
        }
        else
        {
            /* PROJ-1624 non-partitioned index */
            IDE_TEST( qdx::makeAndLockIndexTable( aStatement,
                                                  ID_FALSE,
                                                  sIndex->indexTableID,
                                                  &( sParseTree->oldIndexTables ) )
                      != IDE_SUCCESS );
        }
    }
    else
    {
        /* Nothing to do */
    }

    /* 8. SegStoAttr �ɼ��� �����Ѵ�. */
    IDE_TEST( qdbCommon::validateAndSetSegStoAttr( sTableType,
                                                   & sSrcSegStoAttr,
                                                   & ( sParseTree->segStoAttr ),
                                                   & ( sParseTree->existSegStoAttr ),
                                                   ID_FALSE )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NO_DISK_INDEX )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDB_NO_DISK_INDEX ) );
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdx::validateAlterRebuild(qcStatement * aStatement)
{
/***********************************************************************
 *
 * Description :
 *    ALTER INDEX idx1 REBUILD ������ validation
 *
 * Implementation :
 *
 ***********************************************************************/

    qdIndexParseTree    * sParseTree;
    qcmIndex            * sIndex;
    UInt                  sTableID;
    UInt                  sIndexID;
    
    sParseTree = (qdIndexParseTree *)aStatement->myPlan->parseTree;

    // ---------------------------------------
    // ALTER INDEX�� ���� �������� validation
    // ---------------------------------------

    // if index does not exists, raise error
    IDE_TEST(qcm::checkIndexByUser(
                 aStatement,
                 sParseTree->userNameOfIndex,
                 sParseTree->indexName,
                 &(sParseTree->userIDOfIndex),
                 &sTableID,
                 &sIndexID) != IDE_SUCCESS);

    IDE_TEST(qcm::getTableInfoByID(aStatement,
                                   sTableID,
                                   &(sParseTree->tableInfo),
                                   &(sParseTree->tableSCN),
                                   &(sParseTree->tableHandle) )
             != IDE_SUCCESS);

    // ��Ƽ�ǵ� ���̺� LOCK(IS)
    IDE_TEST( qcm::lockTableForDDLValidation(aStatement,
                                             sParseTree->tableHandle,
                                             sParseTree->tableSCN)
              != IDE_SUCCESS );

    // check grant
    IDE_TEST( qdpRole::checkDDLAlterIndexPriv( aStatement,
                                               sParseTree->tableInfo,
                                               sParseTree->userIDOfIndex )
              != IDE_SUCCESS );
    
    // if specified tables is replicated, the error
    // PROJ-1567
    // IDE_TEST_RAISE(sParseTree->tableInfo->replicationCount > 0,
    //                ERR_DDL_WITH_REPLICATED_TABLE);

    // ---------------------------------------
    // �ε��� ��Ƽ�ǿ� ���� validation
    // ---------------------------------------

    if ( sParseTree->tableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
    {
        IDE_TEST( qcmCache::getIndexByID( sParseTree->tableInfo,
                                          sIndexID,
                                          & sIndex )
                  != IDE_SUCCESS );

        // Pre-pruned Partition�� ���ؼ��� ������� �ʴ´�.
        IDE_DASSERT( (sIndex->indexPartitionType != QCM_LOCAL_INDEX_PARTITION) ||
                     (sIndex->indexPartitionType != QCM_GLOBAL_INDEX_PARTITION) );

        // non-partitioned index�� rebuild�� ���� �ʿ��ϴ�.
        IDE_TEST( qdbCommon::checkAndSetAllPartitionInfo(
                      aStatement,
                      sParseTree->tableInfo->tableID,
                      & (sParseTree->partIndex->partInfoList))
                  != IDE_SUCCESS );
            
        // PROJ-1624 non-partitioned index
        if( sIndex->indexPartitionType == QCM_NONE_PARTITIONED_INDEX )
        {
            IDE_TEST( qdx::makeAndLockIndexTable(
                          aStatement,
                          ID_FALSE,
                          sIndex->indexTableID,
                          &(sParseTree->oldIndexTables) )
                      != IDE_SUCCESS );
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

IDE_RC qdx::validateAlterRebuildPartition(qcStatement * aStatement)
{
/***********************************************************************
 *
 * Description :
 *    PROJ-1502 PARTITIONED DISK TABLE
 *
 *    ALTER INDEX idx1 REBUILD PARTITION p1_idx1; ������ validation
 *
 * Implementation :
 *
 ***********************************************************************/

    qdIndexParseTree    * sParseTree;
    qcmIndex            * sIndex;
    UInt                  sCount = 0;
    SChar                 sRebuildPartName[QC_MAX_OBJECT_NAME_LEN + 1];
    idBool                sIsFound = ID_FALSE;
    UInt                  sTableID;
    UInt                  sIndexID;
    UInt                  sIndexCount;
    qcuSqlSourceInfo      sqlInfo;
    qcmTableInfo        * sPartInfo;
    void                * sPartHandle;
    smSCN                 sPartSCN;
    UInt                  sTablePartitionID;
    qcmPartitionInfoList* sPartInfoList = NULL;
    qdPartitionAttribute* sPartAttr;
    
    sParseTree = (qdIndexParseTree *)aStatement->myPlan->parseTree;

    sPartAttr = sParseTree->partIndex->partAttr;

    // ---------------------------------------
    // ALTER INDEX�� ���� �������� validation
    // ---------------------------------------

    // if index does not exists, raise error
    IDE_TEST(qcm::checkIndexByUser(
                 aStatement,
                 sParseTree->userNameOfIndex,
                 sParseTree->indexName,
                 &(sParseTree->userIDOfIndex),
                 &sTableID,
                 &sIndexID) != IDE_SUCCESS);

    IDE_TEST(qcm::getTableInfoByID(aStatement,
                                   sTableID,
                                   &(sParseTree->tableInfo),
                                   &(sParseTree->tableSCN),
                                   &(sParseTree->tableHandle) )
             != IDE_SUCCESS);

    // ��Ƽ�ǵ� ���̺� LOCK(IS)
    IDE_TEST( qcm::lockTableForDDLValidation(aStatement,
                                             sParseTree->tableHandle,
                                             sParseTree->tableSCN)
              != IDE_SUCCESS );

    IDE_TEST( qdpRole::checkDDLAlterIndexPriv( aStatement,
                                               sParseTree->tableInfo,
                                               sParseTree->userIDOfIndex )
              != IDE_SUCCESS );
    
    // if specified tables is replicated, the error
    // PROJ-1567
    // IDE_TEST_RAISE(sParseTree->tableInfo->replicationCount > 0,
    //                ERR_DDL_WITH_REPLICATED_TABLE);

    // ---------------------------------------
    // �ε��� ��Ƽ�ǿ� ���� validation
    // ---------------------------------------

    // �������� �ε����� ã�´�.
    for( sIndexCount = 0;
         sIndexCount < sParseTree->tableInfo->indexCount;
         sIndexCount++ )
    {
        sIndex = & sParseTree->tableInfo->indices[sIndexCount];

        if( idlOS::strMatch(sIndex->name,
                            idlOS::strlen(sIndex->name),
                            sParseTree->indexName.stmtText +
                            sParseTree->indexName.offset,
                            sParseTree->indexName.size) == 0 )
        {
            sIsFound = ID_TRUE;
            break;
        }
    }

    if( sIsFound == ID_FALSE )
    {
        sqlInfo.setSourceInfo(aStatement,
                              & sParseTree->indexName );
        IDE_RAISE( ERR_NOT_EXIST_PARTITIONED_INDEX );
    }

    // ��Ƽ�ǵ� �ε������� üũ�Ѵ�.
    IDE_TEST_RAISE( sIndex->indexPartitionType == QCM_NONE_PARTITIONED_INDEX,
                    ERR_ALTER_INDEX_REBUILD_ON_NONE_PART_INDEX );

    // �ε��� ��Ƽ�� Name
    QC_STR_COPY( sRebuildPartName, sParseTree->rebuildPartName );

    // �������� �ε��� ��Ƽ���� �����ϴ��� üũ�Ѵ�.
    IDE_TEST( qcmPartition::getIndexPartitionCount(
                  aStatement,
                  sIndex->indexId,
                  sRebuildPartName,
                  idlOS::strlen(sRebuildPartName),
                  & sCount )
              != IDE_SUCCESS );

    IDE_TEST_RAISE( sCount == 0,
                    ERR_NOT_EXIST_INDEX_PARTITION );

    IDE_TEST( qcmPartition::getPartitionIdByIndexName(
                  QC_SMI_STMT( aStatement ),
                  sIndex->indexId,
                  sParseTree->rebuildPartName.stmtText + sParseTree->rebuildPartName.offset,
                  sParseTree->rebuildPartName.size,
                  & sTablePartitionID )
              != IDE_SUCCESS );

    // ���̺� ��Ƽ�� ID�� ��Ƽ�� ��Ÿ ������ �����´�.
    IDE_TEST( qcmPartition::getPartitionInfoByID( aStatement,
                                                  sTablePartitionID,
                                                  & sPartInfo,
                                                  & sPartSCN,
                                                  & sPartHandle )
              != IDE_SUCCESS );

    // ���̺� ��Ƽ�ǿ� LOCK(IS)
    IDE_TEST( qcmPartition::validateAndLockOnePartition( aStatement,
                                                         sPartHandle,
                                                         sPartSCN,
                                                         SMI_TBSLV_DDL_DML, // TBS Validation �ɼ�
                                                         SMI_TABLE_LOCK_IS,
                                                         smiGetDDLLockTimeOut((QC_SMI_STMT(aStatement))->getTrans()) )
              != IDE_SUCCESS );

    // ���̺� ��Ƽ�� ������ �Ľ�Ʈ���� �޾Ƴ��´�.
    IDU_LIMITPOINT("qdx::validateAlterRebuildPartition::malloc");
    IDE_TEST(QC_QMP_MEM(aStatement)->alloc(ID_SIZEOF(qcmPartitionInfoList),
                                           (void**)&(sPartInfoList))
             != IDE_SUCCESS);

    sPartInfoList->partitionInfo = sPartInfo;
    sPartInfoList->partHandle = sPartHandle;
    sPartInfoList->partSCN = sPartSCN;
    sPartInfoList->next = NULL;

    sParseTree->partIndex->partInfoList = sPartInfoList;

    // fix BUG-18937
    // ���̺� �����̽� validation
    IDE_TEST( qdtCommon::getAndValidateTBSOfIndexPartition( aStatement,
                                            sPartInfo->TBSID,
                                            sPartInfo->TBSType,
                                            sPartAttr->TBSName,
                                            sParseTree->tableInfo->tableOwnerID,
                                            & sPartAttr->TBSAttr.mID,
                                            & sPartAttr->TBSAttr.mType )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_ALTER_INDEX_REBUILD_ON_NONE_PART_INDEX)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDX_CANNOT_ALTER_INDEX_REBUILD_ON_NONE_PART_INDEX));
    }
    IDE_EXCEPTION(ERR_NOT_EXIST_INDEX_PARTITION)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDX_NOT_EXIST_INDEX_PARTITION));
    }
    IDE_EXCEPTION(ERR_NOT_EXIST_PARTITIONED_INDEX);
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(ideSetErrorCode( qpERR_ABORT_QDB_NOT_EXIST_PARTITIONED_INDEX,
                                 sqlInfo.getErrMessage()) );
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdx::validateAgingIndex(qcStatement * aStatement)
{
/***********************************************************************
 *
 * Description :
 *    PROJ-1502 PARTITIONED DISK TABLE
 *
 *    ALTER INDEX idx1 AGING; ������ validation
 *
 * Implementation :
 *
 ***********************************************************************/

    qdIndexParseTree     * sParseTree     = NULL;
    UInt                   sTableID       = 0;
    UInt                   sIndexID       = 0;
    qcmIndex             * sIndex         = NULL;
    UInt                   sTableType     = 0;
    qcmPartitionInfoList * sPartInfoList  = NULL;
    SInt                   sCountDiskPart = 0;

    sParseTree = (qdIndexParseTree *)aStatement->myPlan->parseTree;

    // ---------------------------------------
    // ALTER INDEX�� ���� �������� validation
    // ---------------------------------------

    // if index does not exists, raise error
    IDE_TEST(qcm::checkIndexByUser(
                 aStatement,
                 sParseTree->userNameOfIndex,
                 sParseTree->indexName,
                 &(sParseTree->userIDOfIndex),
                 &sTableID,
                 &sIndexID) != IDE_SUCCESS);

    IDE_TEST(qcm::getTableInfoByID(aStatement,
                                   sTableID,
                                   &(sParseTree->tableInfo),
                                   &(sParseTree->tableSCN),
                                   &(sParseTree->tableHandle) )
             != IDE_SUCCESS);

    // ���̺� LOCK(IS)
    IDE_TEST( qcm::lockTableForDDLValidation( aStatement,
                                              sParseTree->tableHandle,
                                              sParseTree->tableSCN )
              != IDE_SUCCESS );

    IDE_TEST(qcmCache::getIndex(sParseTree->tableInfo,
                                sParseTree->indexName,
                                &sIndex)
             != IDE_SUCCESS);

    sTableType = sParseTree->tableInfo->tableFlag & SMI_TABLE_TYPE_MASK;

    // check grant
    IDE_TEST( qdpRole::checkDDLAlterIndexPriv( aStatement,
                                               sParseTree->tableInfo,
                                               sParseTree->userIDOfIndex )
              != IDE_SUCCESS );

    if ( sParseTree->tableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
    {
        if( sIndex->indexPartitionType != QCM_NONE_PARTITIONED_INDEX )
        {
            // Pre-pruned Partition�� ���ؼ��� ������� �ʴ´�.
            IDE_DASSERT( (sIndex->indexPartitionType != QCM_LOCAL_INDEX_PARTITION) ||
                         (sIndex->indexPartitionType != QCM_GLOBAL_INDEX_PARTITION) );
            
            IDE_TEST( qdbCommon::checkAndSetAllPartitionInfo(
                          aStatement,
                          sParseTree->tableInfo->tableID,
                          & (sParseTree->partIndex->partInfoList))
                      != IDE_SUCCESS );

            sPartInfoList = sParseTree->partIndex->partInfoList;

        }
        else
        {
            /* PROJ-2464 hybrid partitioned table ���� */
            IDE_TEST_RAISE( smiIsAgableIndex( sIndex->indexHandle ) == ID_FALSE,
                            ERR_INVALID_INDEX_TYPE );

            // PROJ-1624 non-partitioned index
            IDE_TEST( qdx::makeAndLockIndexTable(
                          aStatement,
                          ID_FALSE,
                          sIndex->indexTableID,
                          &(sParseTree->oldIndexTables) )
                      != IDE_SUCCESS );
        }
    }
    else
    {
        IDE_TEST_RAISE( smiIsAgableIndex( sIndex->indexHandle ) == ID_FALSE, ERR_INVALID_INDEX_TYPE );
    }

    /* PROJ-2464 hybrid partitioned table ����
     *  - HPT �� ��쿡, Memory, Disk Partition�� ��� ���� �� �ִ�.
     *  - ���� Disk Partition�� ���� ��츸, Aging�� ����� �� ����.
     */
    qdbCommon::getTableTypeCountInPartInfoList( & sTableType,
                                                sPartInfoList,
                                                & sCountDiskPart,
                                                NULL,
                                                NULL );

    IDE_TEST_RAISE( sCountDiskPart == 0, ERR_INVALID_INDEX_TYPE );

    if ( ( sParseTree->tableInfo->replicationCount > 0 ) ||
         ( QCG_GET_SESSION_IS_NEED_DDL_INFO( aStatement ) == ID_TRUE ) )
    {
        qrc::setDDLSrcInfo( aStatement,
                            ID_FALSE,
                            1,
                            &(sParseTree->tableInfo->tableOID),
                            0,
                            NULL );
    }
    else
    {
        // Nothing to do.
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_INVALID_INDEX_TYPE)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDX_UNSUPPORTED_INDEXTYPE));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * EXECUTE
 **********************************************************************/

IDE_RC qdx::execute(qcStatement * aStatement)
{
/***********************************************************************
 *
 * Description :
 *    CREATE INDEX ... �� execution ����
 *
 * Implementation :
 *    1. smiColumnList �� �÷� ����ŭ �Ҵ�
 *    2. �ε��� Ÿ���� ��õǾ� ���� ������ ����Ʈ �ε��� Ÿ�� �ο�
 *    3. �ε��� ID �ο�
 *    4. ParseTree->keyColumn ���κ��� smiColumn �� �����͸� 1���� �Ҵ���
 *       smiColumnList �� column �� �ο�
 *    5. smiTable::createIndex
 *    6. �ε��� �̸� �ο�
 *    7. SYS_INDICES_ ��Ÿ ���̺� �Է�
 *    8. SYS_INDEX_COLUMNS_ ��Ÿ ���̺� �Է�
 *    9. ��Ÿ ĳ�� �籸��
 *
 * Replication�� �ɸ� Table�� ���� DDL�� ���, �߰������� �Ʒ��� �۾��� �Ѵ�.
 *    1. Validate�� Execute�� �ٸ� Transaction�̹Ƿ�, ������Ƽ �˻�� Execute���� �Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "qdx::execute"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qdx::execute"));

    qdIndexParseTree     * sParseTree;
    qcmTableInfo         * sTableInfo = NULL;
    qcmTableInfo         * sNewTableInfo = NULL;
    UInt                   sIndexID;
    UInt                   sTableID;
    smOID                  sTableOID;
    UInt                   sType;
    qcmColumn            * sKeyCol;
    smiColumnList        * sColumnList;
    smiColumnList        * sColumnListAtKey;
    mtcColumn            * sColumnsAtKey;
    const void           * sIndex;
    idBool                 sIsUnique;
    idBool                 sIsRange;
    idBool                 sIsAscending;
    UInt                   i;
    SChar                  sIdxName[ QC_MAX_OBJECT_NAME_LEN + 1 ];
    SChar                  sIndexType[4096];
    SInt                   sSize;
    UInt                   sOffset;
    UInt                   sTableType;
    idBool                 sIsPartitionedTable = ID_FALSE;
    idBool                 sIsPartitionedIndex = ID_FALSE;
    smSCN                  sSCN;
    void                 * sTableHandle;
    qcmPartitionInfoList * sOldPartInfoList = NULL;
    qcmPartitionInfoList * sNewPartInfoList = NULL;
    UInt                   sBuildFlag;
    UInt                   sTableFlag;
    UInt                   sTableParallelDegree;
    qcmColumn            * sIndexTableColumns;
    UInt                   sIndexTableColumnCount;
    qcNamePosition         sIndexTableNamePos;
    qdIndexTableList     * sIndexTable = NULL;
    qcmTableInfo         * sIndexTableInfo;
    UInt                   sIndexTableID = 0;
    UInt                   sLocalIndexType = 0;

    idBool                 sIsUniqueIndex = ID_FALSE;
    idBool                 sIsLocalUniqueIndex = ID_FALSE;
    idBool                 sIsReplicatedTable = ID_FALSE;

    smOID                  sNewTableOID = 0;

    smOID                * sOldPartitionOID = NULL;
    UInt                   sPartitionCount = 0;

    smOID                * sOldTableOIDArray = NULL;
    smOID                * sNewTableOIDArray = NULL;
    UInt                   sTableOIDCount = 0;
    UInt                   sDDLSupplementalLog = QCU_DDL_SUPPLEMENTAL_LOG;
    UInt                   sDDLRequireLevel = 0;

    sParseTree = (qdIndexParseTree *)aStatement->myPlan->parseTree;

    // TASK-2176
    // Table�� ���� Lock�� ȹ���Ѵ�.
    IDE_TEST( qcm::validateAndLockTable(aStatement,
                                        sParseTree->tableHandle,
                                        sParseTree->tableSCN,
                                        SMI_TABLE_LOCK_X)
              != IDE_SUCCESS);

    sTableInfo = sParseTree->tableInfo;

    /* PROJ-1407 Temporary table
     * session temporary table�� �����ϴ� ��� DDL�� �� �� ����. */
    IDE_TEST_RAISE( qcuTemporaryObj::existSessionTable( sTableInfo ) == ID_TRUE,
                    ERR_SESSION_TEMPORARY_TABLE_EXIST );

    // PROJ-1502 PARTITIONED DISK TABLE
    if ( sParseTree->partIndex->partInfoList == NULL )
    {
        sIsPartitionedTable = ID_FALSE;
    }
    else
    {
        sIsPartitionedTable = ID_TRUE;

        // ��� ��Ƽ�ǿ� LOCK(X)
        IDE_TEST( qcmPartition::validateAndLockPartitionInfoList( aStatement,
                                                                  sParseTree->partIndex->partInfoList,
                                                                  SMI_TBSLV_DDL_DML, // TBS Validation �ɼ�
                                                                  SMI_TABLE_LOCK_X,
                                                                  smiGetDDLLockTimeOut((QC_SMI_STMT(aStatement))->getTrans()) )
                  != IDE_SUCCESS );

        // ���� ó���� ���Ͽ�, Lock�� ���� �Ŀ� Partition List�� �����Ѵ�.
        sOldPartInfoList = sParseTree->partIndex->partInfoList;

        if ( ( sTableInfo->replicationCount > 0 ) ||
             ( sDDLSupplementalLog == 1 ) )
        {
            IDE_TEST( qcmPartition::getAllPartitionOID( QC_QMX_MEM(aStatement),
                                                        sOldPartInfoList,
                                                        &sOldPartitionOID,
                                                        &sPartitionCount )
                      != IDE_SUCCESS );
        }
        else
        {
            /* do nothing */
        }
    }

    /* PROJ-1442 Replication Online �� DDL ���
     * Validate�� Execute�� �ٸ� Transaction�̹Ƿ�, ������Ƽ �˻�� Execute���� �Ѵ�.
     * PROJ-2642 Table on Replication Allow DDL
     */
    if(sTableInfo->replicationCount > 0)
    {
        sDDLRequireLevel = 0;

        if ( (sParseTree->flag & SMI_INDEX_UNIQUE_MASK) == SMI_INDEX_UNIQUE_ENABLE )
        {
            sIsUniqueIndex = ID_TRUE;
            sDDLRequireLevel = 1;
        }
        else
        {
            /* do nothing */
        }

        if ( (sParseTree->flag & SMI_INDEX_LOCAL_UNIQUE_MASK ) == SMI_INDEX_LOCAL_UNIQUE_ENABLE )
        {
            sIsLocalUniqueIndex = ID_TRUE;
            sDDLRequireLevel = 1;
        }
        else
        {
            /* do nothing */
        }

        IDE_TEST( qci::mManageReplicationCallback.mIsDDLEnableOnReplicatedTable( sDDLRequireLevel,
                                                                                 sTableInfo )
                  != IDE_SUCCESS );

        // PROJ-1624 global non-partitioned index
        // non-partitioned index table�� �����Ǵ� ��� receiver�� �����Ѵ�.
        if ( ( sIsUniqueIndex == ID_TRUE ) ||
             ( sIsLocalUniqueIndex == ID_TRUE ) ||
             ( ( sParseTree->partIndex->partIndexType == QCM_NONE_PARTITIONED_INDEX ) &&
               ( sIsPartitionedTable == ID_TRUE ) ) )
        {
            IDE_TEST_RAISE( QC_SMI_STMT( aStatement )->getTrans()->getReplicationMode() == SMI_TRANSACTION_REPL_NONE,
                            ERR_CANNOT_WRITE_REPL_INFO );

            // ���� Receiver Thread ����
            if ( sIsPartitionedTable == ID_TRUE )
            {
                sOldTableOIDArray = sOldPartitionOID;
                sTableOIDCount = sPartitionCount;
            }
            else
            {
                sTableOID = smiGetTableId(sTableInfo->tableHandle);

                sOldTableOIDArray = &sTableOID;
                sTableOIDCount = 1;
            }

            IDE_TEST( qciMisc::checkRunningEagerReplicationByTableOID( aStatement,
                                                                       sOldTableOIDArray,
                                                                       sTableOIDCount )
                      != IDE_SUCCESS );

            // BUG-22703 : Begin Statement�� ������ �Ŀ� Hang�� �ɸ���
            // �ʾƾ� �մϴ�.
            // mStatistics ��� ������ ���� �մϴ�.
            IDE_TEST( qci::mManageReplicationCallback.mStopReceiverThreads( QC_SMI_STMT(aStatement),
                                                                            aStatement->mStatistics,
                                                                            sOldTableOIDArray,
                                                                            sTableOIDCount )
                      != IDE_SUCCESS );

            sIsReplicatedTable = ID_TRUE;
        }
        else
        {
            /* do nothing */
        }
    }
    else
    {
        // Nothing to do.
    }

    IDU_FIT_POINT( "qdx::execute::alloc::sColumnList", 
                    idERR_ABORT_InsufficientMemory );

    IDE_TEST( aStatement->qmxMem->alloc(
            ID_SIZEOF(smiColumnList) * (sParseTree->keyColCount),
            (void**)&sColumnList)
        != IDE_SUCCESS);

    IDU_FIT_POINT( "qdx::execute::alloc::sColumnListAtKey",
                    idERR_ABORT_InsufficientMemory );

    IDE_TEST( aStatement->qmxMem->alloc(
            ID_SIZEOF(smiColumnList) * (sParseTree->keyColCount),
            (void**)&sColumnListAtKey)
        != IDE_SUCCESS);

    // Key Column ������ ���� ���� �Ҵ�
    IDU_FIT_POINT( "qdx::execute::alloc::sColumnsAtKey",
                    idERR_ABORT_InsufficientMemory );

    IDE_TEST( aStatement->qmxMem->alloc(
            ID_SIZEOF(mtcColumn) * sParseTree->keyColCount,
            (void**) & sColumnsAtKey )
        != IDE_SUCCESS);

    sTableType = sTableInfo->tableFlag & SMI_TABLE_TYPE_MASK;

    if ( QC_IS_NULL_NAME( sParseTree->indexType ) )
    {
        sType = mtd::getDefaultIndexTypeID( sParseTree->keyColumns
                                            ->basicInfo->module );
    }
    else
    {
        sSize = sParseTree->indexType.size < (SInt)(ID_SIZEOF(sIndexType)-1) ?
            sParseTree->indexType.size : (SInt)(ID_SIZEOF(sIndexType)-1) ;

        idlOS::strncpy( sIndexType,
                        sParseTree->indexType.stmtText + sParseTree->indexType.offset,
                        sSize );
        sIndexType[sSize] = '\0';

        IDE_TEST( smiFindIndexType( sIndexType, &sType ) != IDE_SUCCESS);

        // To Fix PR-15190
        IDE_TEST_RAISE( mtd::isUsableIndexType(
                            sParseTree->keyColumns->basicInfo->module,
                            sType ) != ID_TRUE,
                        not_supported_type ) ;
    }

    sOffset = 0;
    for ( i = 0,
              sKeyCol = sParseTree->keyColumns;
          i < sParseTree->keyColCount && sKeyCol != NULL;
          i++,
              sKeyCol = sKeyCol->next)
    {
        sColumnList[i].column = &(sKeyCol->basicInfo->column);
        idlOS::memcpy( &(sColumnsAtKey[i]),
                       sKeyCol->basicInfo, ID_SIZEOF(mtcColumn) );

        if ( sTableType == SMI_TABLE_DISK )
        {
            // To Fix PR-8111
            if ( ( sKeyCol->basicInfo->column.flag & SMI_COLUMN_TYPE_MASK )
                 == SMI_COLUMN_TYPE_FIXED )
            {
                sOffset = idlOS::align( sOffset,
                                        sKeyCol->basicInfo->module->align );
                sColumnsAtKey[i].column.offset = sOffset;
                sColumnsAtKey[i].column.value = NULL;
                sOffset += sKeyCol->basicInfo->column.size;
            }
            else
            {
                sOffset = idlOS::align( sOffset, 8 );
                sColumnsAtKey[i].column.offset = sOffset;
                sColumnsAtKey[i].column.value = NULL;
                sOffset += smiGetVariableColumnSize4DiskIndex();
            }
        }
        sColumnListAtKey[i].column = (smiColumn*) &(sColumnsAtKey[i]);

        if (i == sParseTree->keyColCount - 1)
        {
            sColumnList[i].next = NULL;
            sColumnListAtKey[i].next = NULL;
        }
        else
        {
            sColumnList[i].next = &sColumnList[i+1];
            sColumnListAtKey[i].next = &sColumnListAtKey[i+1];
        }
    }

    IDE_TEST(qcm::getNextIndexID(aStatement,
                                 &sIndexID)
             != IDE_SUCCESS);

    QC_STR_COPY( sIdxName, sParseTree->indexName );

    // BUG-17848 : �������� �Ӽ��� �ֹ߼� �Ӽ� �и�
    sBuildFlag = sParseTree->buildFlag;
    sBuildFlag |= SMI_INDEX_BUILD_UNCOMMITTED_ROW_DISABLE;

    // To Fix BUG-13127
    IDE_TEST( smiTable::createIndex(aStatement->mStatistics,
                                    QC_SMI_STMT( aStatement ),
                                    sParseTree->TBSID,
                                    sTableInfo->tableHandle,
                                    (SChar*)sIdxName,
                                    sIndexID,
                                    sType,
                                    sColumnListAtKey,
                                    sParseTree->flag,
                                    sParseTree->parallelDegree,
                                    sBuildFlag,
                                    sParseTree->segAttr,
                                    sParseTree->segStoAttr,
                                    sParseTree->mDirectKeyMaxSize,
                                    &sIndex )
              != IDE_SUCCESS );

    // PROJ-1502 PARTITIONED DISK TABLE
    if( sIsPartitionedTable == ID_TRUE )
    {
        // PROJ-1624 global non-partitioned index
        if ( sParseTree->partIndex->partIndexType == QCM_NONE_PARTITIONED_INDEX )
        {
            sIsPartitionedIndex = ID_FALSE;
            
            //--------------------------------
            // (global) non-partitioned index
            //--------------------------------

            // index table columns ����
            IDE_TEST( makeColumns4CreateIndexTable( aStatement,
                                                    sParseTree->keyColumns,
                                                    sParseTree->keyColCount,
                                                    & sIndexTableColumns,
                                                    & sIndexTableColumnCount )
                      != IDE_SUCCESS );

            // index table columns �˻�
            IDE_TEST( qdbCommon::validateColumnListForCreateInternalTable(
                          aStatement,
                          ID_TRUE,  // in execution time
                          SMI_TABLE_DISK,
                          sParseTree->TBSID,
                          sIndexTableColumns )
                      != IDE_SUCCESS );

            // index ����
            sTableFlag = sParseTree->tableInfo->tableFlag;
            sTableParallelDegree = sParseTree->tableInfo->parallelDegree;

            sIndexTableNamePos.stmtText = sParseTree->indexTableName;
            sIndexTableNamePos.offset   = 0;
            sIndexTableNamePos.size     = idlOS::strlen( sParseTree->indexTableName );
                
            IDE_TEST( qdx::createIndexTable( aStatement,
                                             sParseTree->userIDOfIndex,
                                             sIndexTableNamePos,
                                             sIndexTableColumns,
                                             sIndexTableColumnCount,
                                             sParseTree->TBSID,
                                             sTableInfo->segAttr,  // sParseTree�� segAttr�� index���̴�.
                                             sParseTree->segStoAttr,
                                             QDB_TABLE_ATTR_MASK_ALL,
                                             sTableFlag, /* Flag Value */
                                             sTableParallelDegree,
                                             & sIndexTable )
                      != IDE_SUCCESS );
                
            IDE_TEST( qdx::createIndexTableIndices(
                          aStatement,
                          sParseTree->userIDOfIndex,
                          sIndexTable,
                          sParseTree->keyColumns,
                          sParseTree->keyIndexName,
                          sParseTree->ridIndexName,
                          sParseTree->TBSID,
                          sType,
                          sParseTree->flag,
                          sParseTree->parallelDegree,
                          sBuildFlag,
                          sParseTree->segAttr,
                          sParseTree->segStoAttr,
                          0 ) /* BUG-42124 : direct key index�� partitioned table�� �������� �ʴ´�. */
                      != IDE_SUCCESS );

            sIndexTableID = sIndexTable->tableID;
            sIndexTableInfo = sIndexTable->tableInfo;
            
            IDE_TEST(qcm::makeAndSetQcmTableInfo(QC_SMI_STMT(aStatement),
                                                 sIndexTable->tableID,
                                                 sIndexTable->tableOID)
                     != IDE_SUCCESS);
            
            IDE_TEST(qcm::getTableInfoByID(aStatement,
                                           sIndexTable->tableID,
                                           &(sIndexTable->tableInfo),
                                           &(sIndexTable->tableSCN),
                                           &(sIndexTable->tableHandle))
                     != IDE_SUCCESS);
            (void)qcm::destroyQcmTableInfo( sIndexTableInfo );
        }
        else
        {
            sIsPartitionedIndex = ID_TRUE;
        }
    }
    else
    {
        // Nothing to do.
    }
    
    sIsRange = smiGetIndexRange( sIndex );

    if ( (sParseTree->flag & SMI_INDEX_UNIQUE_MASK)
         == SMI_INDEX_UNIQUE_ENABLE )
    {
        sIsUnique = ID_TRUE;
    }
    else
    {
        sIsUnique = ID_FALSE;
    }

    IDE_TEST(insertIndexIntoMeta( aStatement,
                                  sParseTree->TBSID,
                                  sParseTree->userIDOfIndex,
                                  sTableInfo->tableID,
                                  sIndexID,
                                  sIdxName,
                                  sType,
                                  sIsUnique,
                                  sParseTree->keyColCount,
                                  sIsRange,
                                  sIsPartitionedIndex,
                                  sIndexTableID,
                                  sParseTree->flag )
             != IDE_SUCCESS);

    i = 0;

    for ( sKeyCol = sParseTree->keyColumns;
          sKeyCol != NULL;
          sKeyCol = sKeyCol->next)
    {
        if ((sKeyCol->basicInfo->column.flag
             & SMI_COLUMN_ORDER_MASK) == SMI_COLUMN_ORDER_ASCENDING)
        {
            sIsAscending = ID_TRUE;
        }
        else
        {
            sIsAscending = ID_FALSE;
        }
        IDE_TEST(insertIndexColumnIntoMeta(aStatement,
                                           sParseTree->userIDOfIndex,
                                           sIndexID,
                                           sKeyCol->basicInfo->column.id,
                                           i,
                                           sIsAscending,
                                           sTableInfo->tableID)
                 != IDE_SUCCESS);
        i++;
    }

    // PROJ-1502 PARTITIONED DISK TABLE
    if( sIsPartitionedIndex == ID_TRUE )
    {
        IDE_TEST(insertPartIndexIntoMeta(aStatement,
                                         sParseTree->userIDOfIndex,
                                         sTableInfo->tableID,
                                         sIndexID,
                                         sLocalIndexType,
                                         sParseTree->flag)
                 != IDE_SUCCESS);
        
        for ( sKeyCol = sParseTree->keyColumns;
              sKeyCol != NULL;
              sKeyCol = sKeyCol->next )
        {
            IDE_TEST( insertIndexPartKeyColumnIntoMeta( aStatement,
                                                        sParseTree->userIDOfIndex,
                                                        sIndexID,
                                                        sKeyCol->basicInfo->column.id,
                                                        sTableInfo,
                                                        QCM_INDEX_OBJECT_TYPE )
                      != IDE_SUCCESS );
        }
    }
    else
    {
        // Nothing to do.
    }

    // PROJ-1624 global non-partitioned index
    // index table�� ���ڵ带 �Է��Ѵ�.
    if( ( sIsPartitionedTable == ID_TRUE ) &&
        ( sIsPartitionedIndex == ID_FALSE ) )
    {
        IDE_DASSERT( sIndexTable != NULL );

        IDE_TEST( buildIndexTable( aStatement,
                                   sIndexTable,
                                   sParseTree->keyColumns,
                                   sParseTree->keyColCount,
                                   sTableInfo,
                                   sOldPartInfoList )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    if (sParseTree->userIDOfIndex != QC_SYSTEM_USER_ID)
    {
        sTableID = sTableInfo->tableID;
        sTableOID = smiGetTableId(sTableInfo->tableHandle);


        IDE_TEST(qcm::touchTable(QC_SMI_STMT( aStatement ),
                                 sTableID,
                                 SMI_TBSLV_DDL_DML )
                 != IDE_SUCCESS);

        IDE_TEST(qcm::makeAndSetQcmTableInfo(
                     QC_SMI_STMT( aStatement ),
                     sTableID,
                     sTableOID) != IDE_SUCCESS);

        IDE_TEST(qcm::getTableInfoByID(aStatement,
                                       sTableID,
                                       &sNewTableInfo,
                                       &sSCN,
                                       &sTableHandle)
                 != IDE_SUCCESS);

        // PROJ-1502 PARTITIONED DISK TABLE
        if ( sIsPartitionedIndex == ID_TRUE )
        {
            // ������ �ε��� ��Ƽ�� ���� �� ��Ÿ ���̺� �Է�
            IDE_TEST( createIndexPartition( aStatement,
                                            sNewTableInfo,
                                            sIndexID,
                                            sType,
                                            sColumnListAtKey )
                      != IDE_SUCCESS );
        }
        else
        {
            // Nothing to do
        }

        if ( sIsPartitionedTable == ID_TRUE )
        {
            IDE_TEST( qcmPartition::touchPartitionInfoList( QC_SMI_STMT( aStatement ),
                                                            sOldPartInfoList )
                      != IDE_SUCCESS );

            IDE_TEST( qcmPartition::makeAndSetAndGetQcmPartitionInfoList( aStatement,
                                                                          sNewTableInfo,
                                                                          sOldPartInfoList,
                                                                          & sNewPartInfoList )
                      != IDE_SUCCESS );
        }
        else
        {
            /* Nothing to do */
        }

        // PROJ-2642 Table on Replication Allow DDL
        if ( ( sIsReplicatedTable == ID_TRUE ) ||
             ( sDDLSupplementalLog == 1 ) )
        {
            if ( sIsPartitionedTable == ID_TRUE )
            {
                sOldTableOIDArray = sOldPartitionOID;

                IDE_TEST( qcmPartition::getAllPartitionOID( QC_QMX_MEM(aStatement),
                                                            sNewPartInfoList,
                                                            &sNewTableOIDArray,
                                                            &sTableOIDCount )
                          != IDE_SUCCESS );
            }
            else
            {
                sNewTableOID = smiGetTableId( sNewTableInfo->tableHandle );

                sOldTableOIDArray = &sTableOID;
                sNewTableOIDArray = &sNewTableOID;
                sTableOIDCount = 1;
            }

            IDE_TEST( qciMisc::writeTableMetaLogForReplication( aStatement,
                                                                sOldTableOIDArray,
                                                                sNewTableOIDArray,
                                                                sTableOIDCount )
                      != IDE_SUCCESS );
        }
        else
        {
            /* do nothing */
        }

        if ( sIsPartitionedTable == ID_TRUE )
        {

            (void)qcmPartition::destroyQcmPartitionInfoList( sOldPartInfoList );
        }
        else
        {
            /* do nothing */
        }

        (void)qcm::destroyQcmTableInfo(sTableInfo);
        sTableInfo = NULL;
    }
    else
    {
        // Nothing to do.
    }

    if ( ( sIsReplicatedTable == ID_TRUE ) ||
         ( QCG_GET_SESSION_IS_NEED_DDL_INFO( aStatement ) == ID_TRUE ) )
    {
        qrc::setDDLDestInfo( aStatement, 
                             0,
                             NULL,
                             0,
                             NULL );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_SESSION_TEMPORARY_TABLE_EXIST )
    {
        IDE_SET(ideSetErrorCode( qpERR_ABORT_QDB_TEMPORARY_TABLE_DDL_DISABLE ));
    }
    // To Fix PR-15190
    IDE_EXCEPTION(not_supported_type);
    {
        IDE_SET( ideSetErrorCode(
                     qpERR_ABORT_QDX_CANNOT_CREATE_INDEX_DATATYPE ) ) ;
    }
    IDE_EXCEPTION( ERR_CANNOT_WRITE_REPL_INFO )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDB_CANNOT_WRITE_REPL_INFO ) );
    }
    IDE_EXCEPTION_END;

    (void)qcm::destroyQcmTableInfo( sNewTableInfo );
    (void)qcmPartition::destroyQcmPartitionInfoList( sNewPartInfoList );

    if ( sIndexTable != NULL )
    {
        (void)qcm::destroyQcmTableInfo( sIndexTable->tableInfo );
    }
    else
    {
        /* Nothing to do */
    }

    qcmPartition::restoreTempInfo( sTableInfo,
                                   sOldPartInfoList,
                                   NULL );

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qdx::createIndexPartition(
    qcStatement   * aStatement,
    qcmTableInfo  * aTableInfo,
    UInt            aPartIndexID,
    UInt            aIndexType,
    smiColumnList * aColumnListAtKey )
{
/***********************************************************************
 *
 * Description :
 *    CREATE INDEX ��, ��Ƽ�ǵ� �ε����� ����
 *    ������ �ε��� ��Ƽ���� �����Ѵ�.
 *
 * Implementation :
 *      1. �ε��� ��Ƽ�� ������ŭ �ݺ�
 *          1-1. ���̺� ��Ƽ�� ��Ÿ ���� ������
 *          1-2. �ε��� ����
 *          1-3. ��Ÿ ���� �Է�
 *          1-4. ��Ƽ�� ��Ÿ ĳ�� �����
 *
 ***********************************************************************/

    qdIndexParseTree        * sParseTree;
    qdPartitionAttribute    * sPartAttr;
    UInt                      sIndexPartID;
    const void              * sIndex;
    SChar                   * sPartMinValue = NULL;
    SChar                   * sPartMaxValue = NULL;
    SChar                   * sTablePartName;
    SChar                   * sIndexPartName;
    qcmTableInfo            * sPartitionInfo;
    void                    * sPartitionHandle;
    UInt                      sIndexPartCount = 0;
    UInt                      sTablePartCount;
    qcmPartitionInfoList    * sPartInfoList;
    idBool                    sIsFound = ID_FALSE;
    qdPartitionedIndex      * sPartIndex;
    UInt                      sBuildFlag;
    smiSegAttr                sSegAttr;
    smiSegStorageAttr         sSegStoAttr;
    UInt                      sFlag             = 0;
    ULong                     sDirectKeyMaxSize = 0L;

    sParseTree = (qdIndexParseTree *)aStatement->myPlan->parseTree;
    sPartIndex = sParseTree->partIndex;
    sPartAttr = sParseTree->partIndex->partAttr;

    // ������ �ε��� ��Ƽ�� ����
    for( sPartAttr = sParseTree->partIndex->partAttr;
         sPartAttr != NULL;
         sPartAttr = sPartAttr->next )
    {
        sIndexPartCount++;
    }

    // ���̺� ��Ƽ�� ����
    IDE_TEST( qcmPartition::getPartitionCount( aStatement,
                                               aTableInfo->tableID,
                                               & sTablePartCount )
              != IDE_SUCCESS );

    // ------------------------------------------------------------
    //  ���̺� ��Ƽ���� ������ŭ �ε��� ��Ƽ���� �������� �ʾ�����,
    //  �������� ���� �ε��� ��Ƽ�Ǳ��� �����ؼ� �Ľ�Ʈ���� �޾Ƴ���
    // ------------------------------------------------------------
    if( sIndexPartCount < sTablePartCount )
    {
        // �� �ε��� ��Ƽ���� ���� ����ü�� �����Ѵ�.(qdPartitionAttribute)
        IDE_TEST( qdx::makeIndexPartition( aStatement,
                                           sPartIndex->partInfoList,
                                           sPartIndex )
                  != IDE_SUCCESS );
    }

    // ----------------------------------------------------
    // 1. �ε��� ��Ƽ�� ������ŭ �ݺ�
    // ----------------------------------------------------
    for( sPartAttr = sParseTree->partIndex->partAttr;
         sPartAttr != NULL;
         sPartAttr = sPartAttr->next )
    {
        if( sPartAttr->tablePartNameStr == NULL )
        {
            IDU_LIMITPOINT("qdx::createIndexPartition::malloc1");
            IDE_TEST(STRUCT_ALLOC_WITH_SIZE(aStatement->qmxMem,
                                            SChar,
                                            QC_MAX_OBJECT_NAME_LEN + 1,
                                            & sTablePartName)
                     != IDE_SUCCESS);

            QC_STR_COPY( sTablePartName, sPartAttr->tablePartName );

            sPartAttr->tablePartNameStr = sTablePartName;
        }

        if( sPartAttr->indexPartNameStr == NULL )
        {
            IDU_LIMITPOINT("qdx::createIndexPartition::malloc2");
            IDE_TEST(STRUCT_ALLOC_WITH_SIZE(aStatement->qmxMem,
                                            SChar,
                                            QC_MAX_OBJECT_NAME_LEN + 1,
                                            & sIndexPartName)
                     != IDE_SUCCESS);

            QC_STR_COPY( sIndexPartName, sPartAttr->indexPartName );

            sPartAttr->indexPartNameStr = sIndexPartName;
        }

        // ----------------------------------------------------
        // 1-2. ���̺� ��Ƽ�� �̸�����
        //      �ʿ��� ��Ƽ�� ��Ÿ ������ PartInfoList�κ��� ���
        //      �ش� ��Ƽ���� SCN�� Handle�� ��´�.
        // ----------------------------------------------------
        sIsFound = ID_FALSE;

        for( sPartInfoList = sPartIndex->partInfoList;
             sPartInfoList != NULL;
             sPartInfoList = sPartInfoList->next )
        {
            sPartitionInfo = sPartInfoList->partitionInfo;
            sPartitionHandle = sPartInfoList->partHandle;

            if ( idlOS::strMatch( sPartitionInfo->name,
                                  idlOS::strlen( sPartitionInfo->name ),
                                  sPartAttr->tablePartNameStr,
                                  idlOS::strlen( sPartAttr->tablePartNameStr ) ) == 0 )
            {
                sIsFound = ID_TRUE;
                break;
            }
        }

        // validation�� �̹� �˻��ߴ�.
        IDE_ASSERT( sIsFound == ID_TRUE );

        // BUG-17848 : �������� �Ӽ��� �ֹ߼� �Ӽ� �и�
        sBuildFlag = sParseTree->buildFlag;
        sBuildFlag |= SMI_INDEX_BUILD_UNCOMMITTED_ROW_DISABLE;

        /* PROJ-2464 hybrid partitioned table ����
         *  - Column �Ǵ� Index �� �ϳ��� �����ؾ� �Ѵ�.
         */
        IDE_TEST( qdbCommon::adjustIndexColumn( sPartitionInfo->columns,
                                                NULL,
                                                NULL,
                                                aColumnListAtKey )
                  != IDE_SUCCESS );

        /* PROJ-2464 hybrid partitioned table ����
         *  - Partition Info�� ������ ����, Table Option�� Partitioned Table�� ������ �����Ѵ�.
         *  - ����, PartInfo�� ������ �̿����� �ʰ�, TBSID�� ���� ������ ������ �����ؼ� �̿��Ѵ�.
         */
        qdbCommon::adjustIndexAttr( sPartAttr->TBSAttr.mID,
                                    sParseTree->segAttr,
                                    sParseTree->segStoAttr,
                                    sParseTree->flag,
                                    sParseTree->mDirectKeyMaxSize,
                                    & sSegAttr,
                                    & sSegStoAttr,
                                    & sFlag,
                                    & sDirectKeyMaxSize );

        // To Fix BUG-13127
        // ----------------------------------------------------
        // 1-3. �ε��� ����
        // ----------------------------------------------------
        IDE_TEST( smiTable::createIndex(aStatement->mStatistics,
                                        QC_SMI_STMT( aStatement ),
                                        sPartAttr->TBSAttr.mID,
                                        sPartitionHandle,
                                        (SChar*)sPartAttr->indexPartNameStr,
                                        aPartIndexID,
                                        aIndexType,
                                        aColumnListAtKey,
                                        sFlag,
                                        sParseTree->parallelDegree,
                                        sBuildFlag,
                                        sSegAttr,
                                        sSegStoAttr,
                                        0, /* BUG-42124 : direct key index�� partitioned table�� �������� �ʴ´�. */
                                        & sIndex )
                  != IDE_SUCCESS );

        if( sParseTree->partIndex->partIndexType ==
            QCM_GLOBAL_PREFIXED_PARTITIONED_INDEX )
        {
            // ���� �۷ι� �ε����� �������� ����.
            IDE_ASSERT(0);
        }
        else
        {
            sPartMinValue = NULL;
            sPartMaxValue = NULL;
        }

        // fix BUG-19175
        if( QC_IS_NULL_NAME(sPartAttr->indexPartName) == ID_TRUE )
        {
            // �� �ε��� ��Ƽ���� �������� ���� ��쿡��
            // makeIndexPartition()���� �̹� indexPartID�� �����.
            sIndexPartID = sPartAttr->indexPartID;
        }
        else
        {
            // �ε��� ID ����
            IDE_TEST( qcmPartition::getNextIndexPartitionID( aStatement,
                                                             & sIndexPartID )
                      != IDE_SUCCESS );
        }

        // ----------------------------------------------------
        // 1-4. ��Ÿ ���� �Է�
        // ----------------------------------------------------
        IDE_TEST(insertIndexPartitionsIntoMeta(aStatement,
                                               sParseTree->userIDOfIndex,
                                               aTableInfo->tableID,
                                               aPartIndexID,
                                               sPartitionInfo->partitionID,
                                               sIndexPartID,
                                               sPartAttr->indexPartNameStr,
                                               sPartMinValue,
                                               sPartMaxValue,
                                               sPartAttr->TBSAttr.mID)
                 != IDE_SUCCESS);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdx::makeIndexPartition( qcStatement          * aStatement,
                                qcmPartitionInfoList * aPartInfoList,
                                qdPartitionedIndex   * aPartIndex )
{
/***********************************************************************
 *
 * Description :
 *      PROJ-1502 PARTITIONED DISK TABLE
 *
 *      �ε��� ��Ƽ�� ������ ���� �ε��� ������ ����Ʈ��
 *      ������ ���´�.
 *      �ε��� ������ validation �ܰ迡�� ����ȴ�.
 *
 * Implementation :
 *      1. ���̺� ��Ƽ���� ������ŭ �ݺ�
 *          1-1. �ε��� ��Ƽ���� �����ߴ��� ã�´�.
 *          1-2. �ε��� ��Ƽ���� �������� ���� ���
 *               �ε��� ��Ƽ�� ������ �����Ѵ�.
 *      2. ������ �ε��� ��Ƽ�� ���� ����Ʈ�� �Ľ�Ʈ���� �޾Ƴ��´�.
 *
 ***********************************************************************/

    qdPartitionAttribute  * sPartAttr = NULL;
    qdPartitionAttribute  * sFirstPartAttr = NULL;
    qdPartitionAttribute  * sNewPartAttr;
    SChar                 * sIndexPartName;
    idBool                  sIsFound = ID_FALSE;
    UInt                    sIndexPartID;
    qcmTableInfo          * sPartInfo;
    qcmPartitionInfoList  * sPartInfoList;

    // ----------------------------------------------------------
    // 1. ���̺� ��Ƽ���� ������ŭ �ݺ�
    // ----------------------------------------------------------
    for( sPartInfoList = aPartInfoList;
         sPartInfoList != NULL;
         sPartInfoList = sPartInfoList->next )
    {
        sPartInfo = sPartInfoList->partitionInfo;

        sIsFound = ID_FALSE;

        // ----------------------------------------------------------
        // 1-1. �ε��� ��Ƽ���� �����ߴ��� ã�´�.
        // ----------------------------------------------------------
        for( sPartAttr = aPartIndex->partAttr;
             sPartAttr != NULL;
             sPartAttr = sPartAttr->next )
        {
            if( idlOS::strMatch( sPartInfo->name,
                                 idlOS::strlen(sPartInfo->name),
                                 sPartAttr->tablePartName.stmtText +
                                 sPartAttr->tablePartName.offset,
                                 sPartAttr->tablePartName.size ) == 0 )
            {
                sIsFound = ID_TRUE;
                break;
            }
        }

        // ----------------------------------------------------------
        // 1-2. ���̺� ��Ƽ�ǿ� �ش��ϴ� �ε��� ��Ƽ���� �������� ���� ���
        //      �ε��� ��Ƽ�� ������ �����Ѵ�.
        // ----------------------------------------------------------
        if( sIsFound == ID_FALSE )
        {
            // sNewPartAttr�� ����
            IDU_LIMITPOINT("qdx::makeIndexPartition::malloc1");
            IDE_TEST( STRUCT_ALLOC_WITH_COUNT( aStatement->qmxMem,
                                               qdPartitionAttribute,
                                               1,
                                               & sNewPartAttr )
                      != IDE_SUCCESS );

            QD_SET_INIT_PARTITION_ATTR(sNewPartAttr);

            IDU_LIMITPOINT("qdx::makeIndexPartition::malloc2");
            IDE_TEST(STRUCT_ALLOC_WITH_SIZE(aStatement->qmxMem,
                                            SChar,
                                            QC_MAX_OBJECT_NAME_LEN + 1,
                                            & sIndexPartName)
                     != IDE_SUCCESS);

            // �ε��� ��Ƽ�� �̸� ���� �� ����
            IDE_TEST( qcmPartition::getNextIndexPartitionID(
                          aStatement,
                          & sIndexPartID)
                      != IDE_SUCCESS );

            sNewPartAttr->indexPartID = sIndexPartID;

            idlOS::snprintf( sIndexPartName, QC_MAX_OBJECT_NAME_LEN + 1,
                             "%sIDX_ID_%"ID_INT32_FMT"",
                             QC_SYS_PARTITIONED_OBJ_NAME_HEADER,
                             sIndexPartID );

            sIndexPartName[idlOS::strlen(sIndexPartName)] = '\0';

            sNewPartAttr->indexPartNameStr = sIndexPartName;

            // ���̺� ��Ƽ�� �̸� ����
            sNewPartAttr->tablePartNameStr = sPartInfo->name;

            // ���̺����̽� ����(���̺� ��Ƽ���� TBS�� ������)
            sNewPartAttr->TBSAttr.mID = sPartInfo->TBSID;
            sNewPartAttr->TBSAttr.mType = sPartInfo->TBSType;

            // sNewPartAttr�� �ε��� ��Ƽ�� ����Ʈ�� ����
            if( sFirstPartAttr == NULL )
            {
                sNewPartAttr->next = NULL;
                sFirstPartAttr = sNewPartAttr;
            }
            else
            {
                sNewPartAttr->next = sFirstPartAttr;
                sFirstPartAttr = sNewPartAttr;
            }
        }
        else
        {
            // Nothing to do
        }
    }

    // ----------------------------------------------------------
    // 2. ������ �ε��� ��Ƽ�� ���� ����Ʈ�� �Ľ�Ʈ���� �޾Ƴ��´�.
    // ----------------------------------------------------------
    // �ε��� ��Ƽ���� 1���� �������� ���� ���
    if( aPartIndex->partAttr == NULL )
    {
        aPartIndex->partAttr = sFirstPartAttr;
    }
    // �ε��� ��Ƽ���� 1�� �̻� ������ ���
    else
    {
        for( sPartAttr = aPartIndex->partAttr;
             sPartAttr->next != NULL;
             sPartAttr = sPartAttr->next ) ;

        sPartAttr->next = sFirstPartAttr;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC qdx::executeAlterPers(qcStatement * aStatement)
{
/***********************************************************************
 *
 * Description :
 *    ALTER INDEX ...SET PERSISTENT = ON/OFF �� execution ����
 *
 * Implementation :
 *    1. ��Ÿ ĳ������ �ش� �ε����� qcmIndex ����ü ã��
 *    2. ON/OFF �� ���� IndexOption �ο�
 *    3. smiTable::alterIndexInfo
 *    4. SYS_INDICES_ ��Ÿ ���̺��� IS_PERS �� ����
 *
 ***********************************************************************/

    qdIndexParseTree *sParseTree;
    qcmIndex *sIndex;
    UInt      sIndexOption;

    sParseTree = (qdIndexParseTree *)aStatement->myPlan->parseTree;

    // TASK-2176
    // Table�� ���� Lock�� ȹ���Ѵ�.
    IDE_TEST( qcm::validateAndLockTable(aStatement,
                                        sParseTree->tableHandle,
                                        sParseTree->tableSCN,
                                        SMI_TABLE_LOCK_X)
              != IDE_SUCCESS);

    IDE_TEST(qcmCache::getIndex(sParseTree->tableInfo,
                                sParseTree->indexName,
                                &sIndex)
             != IDE_SUCCESS);

    sIndexOption = smiTable::getIndexInfo(sIndex->indexHandle);
    if ( ((sParseTree->flag) & QDX_IDX_OPT_PERSISTENT_MASK)
         == QDX_IDX_OPT_PERSISTENT_TRUE)
    {
        sIndexOption &= ~(SMI_INDEX_PERSISTENT_MASK);
        sIndexOption |=  SMI_INDEX_PERSISTENT_ENABLE;
    }
    else
    {
        sIndexOption &= ~(SMI_INDEX_PERSISTENT_MASK);
        sIndexOption |= SMI_INDEX_PERSISTENT_DISABLE;
    }

    IDE_TEST(smiTable::alterIndexInfo(
                 QC_SMI_STMT( aStatement ),
                 (const void*)(sParseTree->tableHandle),
                 (const void*)(sIndex->indexHandle),
                 (const UInt)sIndexOption)
             != IDE_SUCCESS);

    /* BUGBUG Partition�� �ݿ����� �ʴ´�. Manual������ ��������. */

    IDE_TEST(updateIndexPers(aStatement,
                             sIndex->indexId,
                             sIndexOption)
             != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdx::executeAlterSegAttr(qcStatement * aStatement)
{
/***********************************************************************
 *
 * Description :
 *    ALTER INDEX ...INITRANS .. MAXTRANS ..�� execution ����
 *
 * Implementation :
 *    1. ��Ÿ ĳ������ �ش� �ε����� qcmIndex ����ü ã��
 *    2. ON/OFF �� ���� IndexOption �ο�
 *    3. smiTable::alterIndexInfo
 *    4. SYS_INDICES_ ��Ÿ ���̺��� IS_PERS �� ����
 *
 ***********************************************************************/

#define IDE_FN "qdx::executeAlterSegAttr"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qdx::executeAlterSegAttr"));

    qdIndexParseTree      * sParseTree;
    qcmIndex              * sIndex;
    qcmTableInfo          * sPartInfo;
    qcmPartitionInfoList  * sPartInfoList;
    qdIndexTableList      * sOldIndexTable;
    UInt                    i;

    sParseTree = (qdIndexParseTree *)aStatement->myPlan->parseTree;

    // TASK-2176
    // Table�� ���� Lock�� ȹ���Ѵ�.
    IDE_TEST( qcm::validateAndLockTable( aStatement,
                                         sParseTree->tableHandle,
                                         sParseTree->tableSCN,
                                         SMI_TABLE_LOCK_X )
              != IDE_SUCCESS );

    // -----------------------------------------------------
    // 1. �ε��� ��Ÿ ������ �����´�.
    // -----------------------------------------------------
    IDE_TEST( qcmCache::getIndex( sParseTree->tableInfo,
                                  sParseTree->indexName,
                                  &sIndex )
              != IDE_SUCCESS );

    if ( sParseTree->tableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
    {
        if( sIndex->indexPartitionType != QCM_NONE_PARTITIONED_INDEX )
        {
            // ��� ��Ƽ�ǿ� LOCK(X)
            IDE_TEST( qcmPartition::validateAndLockPartitionInfoList( aStatement,
                                                                      sParseTree->partIndex->partInfoList,
                                                                      SMI_TBSLV_DDL_DML, // TBS Validation �ɼ�
                                                                      SMI_TABLE_LOCK_X,
                                                                      smiGetDDLLockTimeOut((QC_SMI_STMT(aStatement))->getTrans()) )
                      != IDE_SUCCESS );
        }
        else
        {
            sOldIndexTable = sParseTree->oldIndexTables;
            
            IDE_TEST( qdx::validateAndLockIndexTableList( aStatement,
                                                          sOldIndexTable,
                                                          SMI_TBSLV_DDL_DML, // TBS Validation �ɼ�
                                                          SMI_TABLE_LOCK_X,
                                                          smiGetDDLLockTimeOut((QC_SMI_STMT(aStatement))->getTrans()) )
                      != IDE_SUCCESS );
        }
    }
    else
    {
        // Nothing to do.
    }
    
    // -----------------------------------------------------
    // 2. �ε����� ALTERING
    // -----------------------------------------------------
    IDE_TEST(smiTable::alterIndexSegAttr(
                 QC_SMI_STMT( aStatement ),
                 (const void*)(sParseTree->tableHandle),
                 (const void*)(sIndex->indexHandle),
                 sParseTree->segAttr )
             != IDE_SUCCESS);

    if ( sParseTree->tableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
    {
        if( sIndex->indexPartitionType != QCM_NONE_PARTITIONED_INDEX )
        {
            sPartInfoList = sParseTree->partIndex->partInfoList;
            
            for( ; sPartInfoList != NULL; sPartInfoList = sPartInfoList->next )
            {
                sPartInfo = sPartInfoList->partitionInfo;
                for( i = 0; i < sPartInfo->indexCount; i++ )
                {
                    /* PROJ-2464 hybrid partitioned table ����
                     *  - Disk Partition �� ��쿡�� �����ϸ�, Memory �� ��쿡 �����Ѵ�.
                     *    1. ��� Index���� �˻��Ѵ�.
                     *    2. �´ٸ�, ��ü�� �˻��� �� �۾��� �����Ѵ�.
                     */
                    /* 1. ��� Index���� �˻��Ѵ�. */
                    if ( sPartInfo->indices[i].indexId == sIndex->indexId )
                    {
                        break;
                    }
                    else
                    {
                        /* Nothing to do */
                    }
                }

                if ( i != sPartInfo->indexCount )
                {
                    /* 2. �´ٸ�, ��ü�� �˻��� �� �۾��� �����Ѵ�. */
                    if ( smiTableSpace::isDiskTableSpace( sPartInfo->indices[i].TBSID ) == ID_TRUE )
                    {
                        IDE_TEST(smiTable::alterIndexSegAttr(
                                     QC_SMI_STMT( aStatement ),
                                     (const void*)(sPartInfo->tableHandle),
                                     (const void*)(sPartInfo->indices[i].indexHandle),
                                     sParseTree->segAttr )
                                 != IDE_SUCCESS);

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
            }
        }
        else
        {
            // PROJ-1624 global non-partitioned index
            sOldIndexTable = sParseTree->oldIndexTables;

            for ( i = 0; i < sOldIndexTable->tableInfo->indexCount; i++ )
            {
                sIndex = &(sOldIndexTable->tableInfo->indices[i]);
                
                IDE_TEST(smiTable::alterIndexSegAttr(
                             QC_SMI_STMT( aStatement ),
                             (const void*)(sOldIndexTable->tableHandle),
                             (const void*)(sIndex->indexHandle),
                             sParseTree->segAttr )
                         != IDE_SUCCESS);
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

IDE_RC qdx::executeAlterSegStoAttr(qcStatement * aStatement)
{
/***********************************************************************
 *
 * Description :
 *    ALTER INDEX ...STORAGE �� execution ����
 *
 * Implementation :
 *    1. ��Ÿ ĳ������ �ش� �ε����� qcmIndex ����ü ã��
 *    2. smiTable::alterIndexSegStoAttr
 *
 ***********************************************************************/

#define IDE_FN "qdx::executeAlterSegStoAttr"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qdx::executeAlterSegStoAttr"));

    qdIndexParseTree      * sParseTree     = NULL;
    qcmIndex              * sIndex         = NULL;
    qcmTableInfo          * sPartInfo      = NULL;
    qcmPartitionInfoList  * sPartInfoList  = NULL;
    qdIndexTableList      * sOldIndexTable = NULL;
    UInt                    i              = 0;

    sParseTree = (qdIndexParseTree *)aStatement->myPlan->parseTree;

    // TASK-2176
    // Table�� ���� Lock�� ȹ���Ѵ�.
    IDE_TEST( qcm::validateAndLockTable(aStatement,
                                        sParseTree->tableHandle,
                                        sParseTree->tableSCN,
                                        SMI_TABLE_LOCK_X)
              != IDE_SUCCESS);

    IDE_TEST(qcmCache::getIndex(sParseTree->tableInfo,
                                sParseTree->indexName,
                                &sIndex)
             != IDE_SUCCESS);

    /* PROJ-2464 hybrid partitioned table ����
     *  - Disk Partition�� ��쿡�� �����ϸ�, Memory �� ��쿡 �����Ѵ�.
     *  - Partitioned �� ����ϴ� ó���� �߰��Ѵ�.
     *     1. Partition ������ Lock ȹ��
     *     2. Table ó��
     *     3. Table Partition ó��
     *        3.1. ��� Index���� �˻��Ѵ�.
     *        3.2. �´ٸ�, ��ü�� �˻��� �� �۾��� �����Ѵ�.
     */
    /* 1. Partition ������ Lock ȹ�� */
    if ( sParseTree->tableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
    {
        if ( sIndex->indexPartitionType != QCM_NONE_PARTITIONED_INDEX )
        {
            /* ��� ��Ƽ�ǿ� LOCK(X) */
            IDE_TEST( qcmPartition::validateAndLockPartitionInfoList( aStatement,
                                                                      sParseTree->partIndex->partInfoList,
                                                                      SMI_TBSLV_DDL_DML, // TBS Validation �ɼ�
                                                                      SMI_TABLE_LOCK_X,
                                                                      smiGetDDLLockTimeOut((QC_SMI_STMT(aStatement))->getTrans()) )
                      != IDE_SUCCESS );
        }
        else
        {
            sOldIndexTable = sParseTree->oldIndexTables;

            IDE_TEST( qdx::validateAndLockIndexTableList( aStatement,
                                                          sOldIndexTable,
                                                          SMI_TBSLV_DDL_DML, // TBS Validation �ɼ�
                                                          SMI_TABLE_LOCK_X,
                                                          smiGetDDLLockTimeOut((QC_SMI_STMT(aStatement))->getTrans()) )
                      != IDE_SUCCESS );
        }
    }
    else
    {
        /* Nothing to do. */
    }

    /* 2. Table ó�� */
    IDE_TEST( smiTable::alterIndexSegStoAttr(
                  QC_SMI_STMT( aStatement ),
                  (const void*)( sParseTree->tableHandle ),
                  (const void*)( sIndex->indexHandle ),
                  sParseTree->segStoAttr ) != IDE_SUCCESS );

    /* 3. Table Partition ó�� */
    if ( sParseTree->tableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
    {
        if ( sIndex->indexPartitionType != QCM_NONE_PARTITIONED_INDEX )
        {
            for ( sPartInfoList  = sParseTree->partIndex->partInfoList;
                  sPartInfoList != NULL;
                  sPartInfoList  = sPartInfoList->next )
            {
                sPartInfo = sPartInfoList->partitionInfo;

                for ( i = 0; i < sPartInfo->indexCount; i++ )
                {
                    /* 3.1. ��� Index���� �˻��Ѵ�. */
                    if ( sPartInfo->indices[i].indexId == sIndex->indexId )
                    {
                        break;
                    }
                    else
                    {
                        /* Nothing to do */
                    }
                }

                if ( i != sPartInfo->indexCount )
                {
                    /* 3.2. �´ٸ�, ��ü�� �˻��� �� �۾��� �����Ѵ�. */
                    if ( smiTableSpace::isDiskTableSpace( sPartInfo->indices[i].TBSID ) == ID_TRUE )
                    {
                        IDE_TEST( smiTable::alterIndexSegStoAttr(
                                      QC_SMI_STMT( aStatement ),
                                      (const void*)( sPartInfo->tableHandle ),
                                      (const void*)( sPartInfo->indices[i].indexHandle ),
                                      sParseTree->segStoAttr )
                                  != IDE_SUCCESS );
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
            }
        }
        else
        {
            /* PROJ-1624 global non-partitioned index */
            sOldIndexTable = sParseTree->oldIndexTables;

            for ( i = 0; i < sOldIndexTable->tableInfo->indexCount; i++ )
            {
                sIndex = &( sOldIndexTable->tableInfo->indices[i] );

                IDE_TEST( smiTable::alterIndexSegStoAttr(
                              QC_SMI_STMT( aStatement ),
                              (const void*)( sOldIndexTable->tableHandle ),
                              (const void*)( sIndex->indexHandle ),
                              sParseTree->segStoAttr )
                          != IDE_SUCCESS);
            }
        }
    }
    else
    {
        /* Nothing to do. */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qdx::executeAlterAllocExts(qcStatement * aStatement)
{
/***********************************************************************
 *
 * Description :
 *    ALTER INDEX ... ALLOCATE EXTENT ( SIZE .. )�� execution ����
 *
 * Implementation :
 *    1. ��Ÿ ĳ������ �ش� �ε����� qcmIndex ����ü ã��
 *    2. smiTable::alterIndexAllocExts
 *
 ***********************************************************************/

#define IDE_FN "qdx::executeAlterSegStoAttr"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qdx::executeAlterSegStoAttr"));

    qdIndexParseTree      * sParseTree     = NULL;
    qcmIndex              * sIndex         = NULL;
    qcmTableInfo          * sPartInfo      = NULL;
    qcmPartitionInfoList  * sPartInfoList  = NULL;
    qdIndexTableList      * sOldIndexTable = NULL;
    UInt                    i              = 0;

    sParseTree = (qdIndexParseTree *)aStatement->myPlan->parseTree;

    // TASK-2176
    // Table�� ���� Lock�� ȹ���Ѵ�.
    IDE_TEST( qcm::validateAndLockTable(aStatement,
                                        sParseTree->tableHandle,
                                        sParseTree->tableSCN,
                                        SMI_TABLE_LOCK_X)
              != IDE_SUCCESS);

    IDE_TEST(qcmCache::getIndex(sParseTree->tableInfo,
                                sParseTree->indexName,
                                &sIndex)
             != IDE_SUCCESS);

    /* PROJ-2464 hybrid partitioned table ����
     *  - Disk Partition�� ��쿡�� �����ϸ�, Memory �� ��쿡 �����Ѵ�.
     *  - Partitioned �� ����ϴ� ó���� �߰��Ѵ�.
     *     1. Partition ������ Lock ȹ��
     *     2. Table Partition ó��
     *        2.1. ��� Index���� �˻��Ѵ�.
     *        2.2. �´ٸ�, ��ü�� �˻��� �� �۾��� �����Ѵ�.
     */
    /* 1. Partition ������ Lock ȹ�� */
    if ( sParseTree->tableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
    {
        if ( sIndex->indexPartitionType != QCM_NONE_PARTITIONED_INDEX )
        {
            /* ��� ��Ƽ�ǿ� LOCK(X) */
            IDE_TEST( qcmPartition::validateAndLockPartitionInfoList( aStatement,
                                                                      sParseTree->partIndex->partInfoList,
                                                                      SMI_TBSLV_DDL_DML, // TBS Validation �ɼ�
                                                                      SMI_TABLE_LOCK_X,
                                                                      smiGetDDLLockTimeOut((QC_SMI_STMT(aStatement))->getTrans()) )
                      != IDE_SUCCESS );
        }
        else
        {
            sOldIndexTable = sParseTree->oldIndexTables;

            IDE_TEST( qdx::validateAndLockIndexTableList( aStatement,
                                                          sOldIndexTable,
                                                          SMI_TBSLV_DDL_DML, // TBS Validation �ɼ�
                                                          SMI_TABLE_LOCK_X,
                                                          smiGetDDLLockTimeOut((QC_SMI_STMT(aStatement))->getTrans()))
                      != IDE_SUCCESS );
        }
    }
    else
    {
        /* Nothing to do. */
    }

    IDE_TEST(smiTable::alterIndexAllocExts(
                 QC_SMI_STMT( aStatement ),
                 (const void*)(sParseTree->tableHandle),
                 (const void*)(sIndex->indexHandle),
                 sParseTree->altAllocExtSize ) != IDE_SUCCESS);

    /* 2. Table Partition ó�� */
    if ( sParseTree->tableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
    {
        if ( sIndex->indexPartitionType != QCM_NONE_PARTITIONED_INDEX )
        {
            for ( sPartInfoList  = sParseTree->partIndex->partInfoList;
                  sPartInfoList != NULL;
                  sPartInfoList  = sPartInfoList->next )
            {
                sPartInfo = sPartInfoList->partitionInfo;
                for ( i = 0; i < sPartInfo->indexCount; i++ )
                {
                    /* 2.1. ��� Index���� �˻��Ѵ�. */
                    if ( sPartInfo->indices[i].indexId == sIndex->indexId )
                    {
                        break;
                    }
                    else
                    {
                        /* Nothing to do */
                    }
                }

                if ( i != sPartInfo->indexCount )
                {
                    /* 2.2. �´ٸ�, ��ü�� �˻��� �� �۾��� �����Ѵ�. */
                    if ( smiTableSpace::isDiskTableSpace( sPartInfo->indices[i].TBSID ) == ID_TRUE )
                    {
                        IDE_TEST( smiTable::alterIndexAllocExts(
                                      QC_SMI_STMT( aStatement ),
                                      (const void*)( sPartInfo->tableHandle ),
                                      (const void*)( sPartInfo->indices[i].indexHandle ),
                                      sParseTree->altAllocExtSize )
                                 != IDE_SUCCESS );
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
            }
        }
        else
        {
            /* PROJ-1624 global non-partitioned index */
            sOldIndexTable = sParseTree->oldIndexTables;

            for ( i = 0; i < sOldIndexTable->tableInfo->indexCount; i++ )
            {
                sIndex = &( sOldIndexTable->tableInfo->indices[i] );

                IDE_TEST( smiTable::alterIndexAllocExts(
                              QC_SMI_STMT( aStatement ),
                              (const void*)( sOldIndexTable->tableHandle ),
                              (const void*)( sIndex->indexHandle ),
                              sParseTree->altAllocExtSize )
                          != IDE_SUCCESS );
            }
        }
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qdx::executeAlterRebuild(qcStatement * aStatement)
{
/***********************************************************************
 *
 * Description :
 *    ALTER INDEX idx1 REBUILD ������ execution
 *
 * Implementation :
 *
 ***********************************************************************/

    qdIndexParseTree     * sParseTree;
    qcmIndex             * sIndex;
    qcmPartitionInfoList * sPartInfoList;
    qcmPartitionInfoList * sOldPartInfoList = NULL;
    qcmPartitionInfoList * sNewPartInfoList = NULL;
    qcmTableInfo         * sPartInfo;
    qcmIndex             * sLocalIndex;
    qcmIndex             * sTempLocalIndex;
    UInt                   sLocalIndexCount;
    UInt                   sFlag = 0;
    smiSegAttr             sSegAttr;
    smiSegStorageAttr      sSegStoAttr;
    UInt                   sMaxKeySize;
    smiColumnList        * sColumnListAtKey;
    const void           * sIndexHandle;
    SChar                * sSqlStr;
    vSLong                 sRowCnt;
    qcmTableInfo         * sOldTableInfo = NULL;
    qcmTableInfo         * sNewTableInfo = NULL;
    qdIndexTableList     * sOldIndexTable = NULL;
    qdIndexTableList     * sNewIndexTable = NULL;
    qcmIndex             * sIndexTableIndex[2];
    qcNamePosition         sIndexTableNamePos;
    UInt                   sIndexTableFlag;
    UInt                   sIndexTableParallelDegree;
    qcmTableInfo         * sIndexTableInfo;
    UInt                   sIndexTableID = 0;
    qcmColumn            * sColumns;
    UInt                   sColumnCount;
    smSCN                  sSCN;
    void                 * sTableHandle;
    static const SChar   * sTrueFalseStr[2] = {"T", "F"};
    const SChar          * sIsDirectKey;

    qcmColumn            * sTempColumns = NULL;

    sParseTree = (qdIndexParseTree *)aStatement->myPlan->parseTree;

    // Table�� ���� Lock�� ȹ���Ѵ�.
    IDE_TEST( qcm::validateAndLockTable(aStatement,
                                        sParseTree->tableHandle,
                                        sParseTree->tableSCN,
                                        SMI_TABLE_LOCK_X)
              != IDE_SUCCESS);

    // -----------------------------------------------------
    // 1. �ε��� ��Ÿ ������ �����´�.
    // -----------------------------------------------------

    IDE_TEST(qcmCache::getIndex(sParseTree->tableInfo,
                                sParseTree->indexName,
                                &sIndex)
             != IDE_SUCCESS);

    if ( sParseTree->tableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
    {
        if( sIndex->indexPartitionType != QCM_NONE_PARTITIONED_INDEX )
        {
            // ��� ��Ƽ�ǿ� LOCK(X)
            IDE_TEST( qcmPartition::validateAndLockPartitionInfoList( aStatement,
                                                                      sParseTree->partIndex->partInfoList,
                                                                      SMI_TBSLV_DDL_DML, // TBS Validation �ɼ�
                                                                      SMI_TABLE_LOCK_X,
                                                                      smiGetDDLLockTimeOut((QC_SMI_STMT(aStatement))->getTrans()) )
                      != IDE_SUCCESS );
        }
        else
        {
            sOldIndexTable = sParseTree->oldIndexTables;
            
            // PROJ-1624 non-partitioned index
            // rebuild�ÿ��� IS_LOCK�� �ʿ��ϴ�.
            IDE_TEST( qcmPartition::validateAndLockPartitionInfoList( aStatement,
                                                                      sParseTree->partIndex->partInfoList,
                                                                      SMI_TBSLV_DDL_DML, // TBS Validation �ɼ�
                                                                      SMI_TABLE_LOCK_IS,
                                                                      smiGetDDLLockTimeOut((QC_SMI_STMT(aStatement))->getTrans()) )
                      != IDE_SUCCESS );

            IDE_TEST( qdx::validateAndLockIndexTableList( aStatement,
                                                          sOldIndexTable,
                                                          SMI_TBSLV_DDL_DML, // TBS Validation �ɼ�
                                                          SMI_TABLE_LOCK_X,
                                                          smiGetDDLLockTimeOut((QC_SMI_STMT(aStatement))->getTrans()) )
                      != IDE_SUCCESS );
        }
    }
    else
    {
        // Nothing to do.
    }
    
    //-----------------------------------------------------
    // rebuild index
    //-----------------------------------------------------
    
    sOldTableInfo = sParseTree->tableInfo;
    sOldPartInfoList = sParseTree->partIndex->partInfoList;

    if ( sParseTree->tableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
    {
        if( sIndex->indexPartitionType != QCM_NONE_PARTITIONED_INDEX )
        {
            for( sPartInfoList = sOldPartInfoList;
                 sPartInfoList != NULL;
                 sPartInfoList = sPartInfoList->next )
            {
                sPartInfo = sPartInfoList->partitionInfo;

                // -----------------------------------------------------
                // ������ �ε��� ��Ƽ���� ã�´�.
                // -----------------------------------------------------

                sLocalIndex = NULL;

                for( sLocalIndexCount = 0;
                     sLocalIndexCount < sPartInfo->indexCount;
                     sLocalIndexCount++ )
                {
                    sTempLocalIndex = & sPartInfo->indices[sLocalIndexCount];

                    if( sTempLocalIndex->indexId == sIndex->indexId )
                    {
                        sLocalIndex = sTempLocalIndex;
                        break;
                    }
                }

                IDE_ASSERT( sLocalIndex != NULL );

                // -----------------------------------------------------
                // 4. �ش� �ε��� ��Ƽ�� ����
                // -----------------------------------------------------

                // �ε��� ������ ���� flag
                sFlag = smiTable::getIndexInfo(sLocalIndex->indexHandle);
                sSegAttr = smiTable::getIndexSegAttr(sLocalIndex->indexHandle);
                sSegStoAttr = smiTable::getIndexSegStoAttr(sLocalIndex->indexHandle);

                IDE_TEST(smiTable::dropIndex(QC_SMI_STMT( aStatement ),
                                             sPartInfo->tableHandle,
                                             sLocalIndex->indexHandle,
                                             SMI_TBSLV_DDL_DML )
                         != IDE_SUCCESS);

                // -----------------------------------------------------
                // 5. �ε��� ������ ���� Ű �÷� ������ ���Ѵ�.
                // -----------------------------------------------------

                // Ű �÷��� ���� �ε����� sLocalIndex�̴�.
                IDE_TEST( qdx::getKeyColumnList( aStatement,
                                                 sLocalIndex,
                                                 & sColumnListAtKey )
                          != IDE_SUCCESS );

                // -----------------------------------------------------
                // 6. �ε��� ��Ƽ�� ����
                // -----------------------------------------------------

                IDE_TEST( smiTable::createIndex(aStatement->mStatistics,
                                                QC_SMI_STMT( aStatement ),
                                                sLocalIndex->TBSID,
                                                sPartInfo->tableHandle,
                                                (SChar*)sLocalIndex->name,
                                                sLocalIndex->indexId,
                                                sLocalIndex->indexTypeId,
                                                sColumnListAtKey,
                                                sFlag,
                                                QD_INDEX_DEFAULT_PARALLEL_DEGREE,
                                                SMI_INDEX_BUILD_UNCOMMITTED_ROW_DISABLE,
                                                sSegAttr,
                                                sSegStoAttr,
                                                0, /* BUG-42124 : direct key index�� partitioned table�� �������� �ʴ´�. */
                                                & sIndexHandle )
                          != IDE_SUCCESS );
            }
        }
        else
        {
            // PROJ-1624 global non-partitioned index
            // ���ο� index table�� �����Ѵ�.
            sOldIndexTable = sParseTree->oldIndexTables;

            //---------------------------
            // drop old index table
            //---------------------------
            
            IDE_TEST( qdx::dropIndexTable( aStatement,
                                           sOldIndexTable,
                                           ID_FALSE /* aIsDropTablespace */ )
                      != IDE_SUCCESS );
            
            //---------------------------
            // create new index table
            //---------------------------
            
            sIndexTableFlag = sOldIndexTable->tableInfo->tableFlag;
            sIndexTableParallelDegree = sOldIndexTable->tableInfo->parallelDegree;

            sIndexTableNamePos.stmtText = sOldIndexTable->tableInfo->name;
            sIndexTableNamePos.offset   = 0;
            sIndexTableNamePos.size     =
                idlOS::strlen(sOldIndexTable->tableInfo->name);

            /* BUG-45503 Table ���� ���Ŀ� ���� ��, Table Meta Cache�� Column ������ �������� �ʴ� ��찡 �ֽ��ϴ�. */
            IDE_TEST( qcm::copyQcmColumns( QC_QMX_MEM( aStatement ),
                                           sOldIndexTable->tableInfo->columns,
                                           & sTempColumns,
                                           sOldIndexTable->tableInfo->columnCount )
                      != IDE_SUCCESS );

            IDE_TEST( qdx::createIndexTable( aStatement,
                                             sOldIndexTable->tableInfo->tableOwnerID,
                                             sIndexTableNamePos,
                                             sTempColumns,
                                             sOldIndexTable->tableInfo->columnCount,
                                             sOldIndexTable->tableInfo->TBSID,
                                             sOldIndexTable->tableInfo->segAttr,
                                             sOldIndexTable->tableInfo->segStoAttr,
                                             QDB_TABLE_ATTR_MASK_ALL,
                                             sIndexTableFlag, /* Flag Value */
                                             sIndexTableParallelDegree,
                                             & sNewIndexTable )
                      != IDE_SUCCESS );

            // key index, rid index�� ã�´�.
            IDE_TEST( qdx::getIndexTableIndices( sOldIndexTable->tableInfo,
                                                 sIndexTableIndex )
                      != IDE_SUCCESS );
            
            sFlag = smiTable::getIndexInfo(sIndexTableIndex[0]->indexHandle);
            sSegAttr = smiTable::getIndexSegAttr(sIndexTableIndex[0]->indexHandle);
            sSegStoAttr = smiTable::getIndexSegStoAttr(sIndexTableIndex[0]->indexHandle);
            
            IDE_TEST( qdx::createIndexTableIndices(
                          aStatement,
                          sOldIndexTable->tableInfo->tableOwnerID,
                          sNewIndexTable,
                          NULL,
                          sIndexTableIndex[0]->name,
                          sIndexTableIndex[1]->name,
                          sIndexTableIndex[0]->TBSID,
                          sIndexTableIndex[0]->indexTypeId,
                          sFlag,
                          QD_INDEX_DEFAULT_PARALLEL_DEGREE,
                          SMI_INDEX_BUILD_UNCOMMITTED_ROW_DISABLE,
                          sSegAttr,
                          sSegStoAttr,
                          0 ) /* BUG-42124 : direct key index�� partitioned table�� �������� �ʴ´�. */
                      != IDE_SUCCESS );
            
            sIndexTableID = sNewIndexTable->tableID;
            sIndexTableInfo = sNewIndexTable->tableInfo;
            
            IDE_TEST(qcm::makeAndSetQcmTableInfo(QC_SMI_STMT(aStatement),
                                                 sNewIndexTable->tableID,
                                                 sNewIndexTable->tableOID)
                     != IDE_SUCCESS);
            
            IDE_TEST(qcm::getTableInfoByID(aStatement,
                                           sNewIndexTable->tableID,
                                           &(sNewIndexTable->tableInfo),
                                           &(sNewIndexTable->tableSCN),
                                           &(sNewIndexTable->tableHandle))
                     != IDE_SUCCESS);
            
            (void)qcm::destroyQcmTableInfo(sIndexTableInfo);

            //---------------------------
            // build new index table
            //---------------------------

            // index key mtcColumn�� table qcmColumn���� ��ȯ�Ѵ�.
            IDE_TEST( makeColumns4BuildIndexTable( aStatement,
                                                   sOldTableInfo,
                                                   sIndex->keyColumns,
                                                   sIndex->keyColCount,
                                                   & sColumns,
                                                   & sColumnCount )
                      != IDE_SUCCESS );
            
            IDE_TEST( buildIndexTable( aStatement,
                                       sNewIndexTable,
                                       sColumns,
                                       sColumnCount,
                                       sOldTableInfo,
                                       sOldPartInfoList )
                      != IDE_SUCCESS );
        }
    }
    else
    {
        // -----------------------------------------------------
        // 4. �ش� �ε��� ��Ƽ�� ����
        // -----------------------------------------------------

        // �ε��� ������ ���� flag
        sFlag = smiTable::getIndexInfo(sIndex->indexHandle);
        sSegAttr = smiTable::getIndexSegAttr(sIndex->indexHandle);
        sSegStoAttr = smiTable::getIndexSegStoAttr(sIndex->indexHandle);
        sMaxKeySize = smiTable::getIndexMaxKeySize( sIndex->indexHandle ); /* PROJ-2433 */

        IDE_TEST(smiTable::dropIndex(QC_SMI_STMT( aStatement ),
                                     sOldTableInfo->tableHandle,
                                     sIndex->indexHandle,
                                     SMI_TBSLV_DDL_DML )
                 != IDE_SUCCESS);

        // -----------------------------------------------------
        // 5. �ε��� ������ ���� Ű �÷� ������ ���Ѵ�.
        // -----------------------------------------------------

        // Ű �÷��� ���� �ε����� sIndex�̴�.
        IDE_TEST( qdx::getKeyColumnList( aStatement,
                                         sIndex,
                                         & sColumnListAtKey )
                  != IDE_SUCCESS );

        // -----------------------------------------------------
        // 6. �ε��� ��Ƽ�� ����
        // -----------------------------------------------------

        IDE_TEST( smiTable::createIndex(aStatement->mStatistics,
                                        QC_SMI_STMT( aStatement ),
                                        sIndex->TBSID,
                                        sOldTableInfo->tableHandle,
                                        (SChar*)sIndex->name,
                                        sIndex->indexId,
                                        sIndex->indexTypeId,
                                        sColumnListAtKey,
                                        sFlag,
                                        QD_INDEX_DEFAULT_PARALLEL_DEGREE,
                                        SMI_INDEX_BUILD_UNCOMMITTED_ROW_DISABLE,
                                        sSegAttr,
                                        sSegStoAttr,
                                        sMaxKeySize,
                                        & sIndexHandle )
                  != IDE_SUCCESS );
    }

    // -----------------------------------------------------
    // meta ������ ������Ʈ�Ѵ�.
    // -----------------------------------------------------

    // PR-14394
    IDU_FIT_POINT( "qdx::executeAlterRebuild::alloc::sSqlStr",
                    idERR_ABORT_InsufficientMemory );

    IDE_TEST(STRUCT_ALLOC_WITH_SIZE(aStatement->qmxMem,
                                    SChar,
                                    QD_MAX_SQL_LENGTH,
                                    &sSqlStr)
             != IDE_SUCCESS);

    /* PROJ-2433 Direct Key Index
     * executeAlterDireckey() �Լ����� index rebuild�� ���� �� �Լ��� ȣ���Ѵ�.
     * ���⼭ MEAT�� IS_DIRECTKEY ������ �����Ѵ�. */
    if ( ( sFlag & SMI_INDEX_DIRECTKEY_MASK ) == SMI_INDEX_DIRECTKEY_TRUE )
    {
        sIsDirectKey = sTrueFalseStr[0];
    }
    else
    {
        sIsDirectKey = sTrueFalseStr[1];
    }

    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                     "UPDATE SYS_INDICES_ "
                     "   SET INDEX_TABLE_ID = INTEGER'%"ID_INT32_FMT"', "
                     "       IS_DIRECTKEY = CHAR'%s', "
                     "       LAST_DDL_TIME = SYSDATE "
                     "WHERE  INDEX_ID =  INTEGER'%"ID_INT32_FMT"' "
                     "  AND  INDEX_TYPE = INTEGER'%"ID_INT32_FMT"' ",
                     sIndexTableID,
                     sIsDirectKey,
                     sIndex->indexId,
                     sIndex->indexTypeId );

    IDE_TEST(qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                sSqlStr,
                                & sRowCnt ) != IDE_SUCCESS);

    IDE_TEST_RAISE(sRowCnt != 1, ERR_META_CRASH);

    //---------------------------
    // cached meta �����
    //---------------------------
    
    if ( sParseTree->tableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
    {
        if ( sIndex->indexPartitionType != QCM_NONE_PARTITIONED_INDEX )
        {
            IDE_TEST( qcmPartition::touchPartitionInfoList( QC_SMI_STMT( aStatement ),
                                                            sOldPartInfoList )
                      != IDE_SUCCESS );

            // -----------------------------------------------------
            // 7. ��Ƽ�� ��Ÿ ĳ�� ����
            // -----------------------------------------------------
            IDE_TEST( qcmPartition::makeAndSetAndGetQcmPartitionInfoList( aStatement,
                                                                          sOldTableInfo,
                                                                          sOldPartInfoList,
                                                                          & sNewPartInfoList )
                      != IDE_SUCCESS );
        }
        else
        {
            // PROJ-1624 global non-partitioned index
            // index table�� ������ߴٸ� tableInfo�� �����Ѵ�.
            
            IDE_TEST( qcm::touchTable( QC_SMI_STMT( aStatement ),
                                       sOldTableInfo->tableID,
                                       SMI_TBSLV_DDL_DML )
                      != IDE_SUCCESS);
        
            IDE_TEST( qcm::makeAndSetQcmTableInfo(
                          QC_SMI_STMT( aStatement ),
                          sOldTableInfo->tableID,
                          smiGetTableId(sOldTableInfo->tableHandle) )
                      != IDE_SUCCESS );
        
            IDE_TEST( qcm::getTableInfoByID(aStatement,
                                            sOldTableInfo->tableID,
                                            &sNewTableInfo,
                                            &sSCN,
                                            &sTableHandle)
                      != IDE_SUCCESS);
        }
    }
    else
    {
        IDE_TEST( qcm::touchTable( QC_SMI_STMT( aStatement ),
                                   sOldTableInfo->tableID,
                                   SMI_TBSLV_DDL_DML )
                  != IDE_SUCCESS);

        IDE_TEST( qcm::makeAndSetQcmTableInfo(
                      QC_SMI_STMT( aStatement ),
                      sOldTableInfo->tableID,
                      smiGetTableId(sOldTableInfo->tableHandle) )
                  != IDE_SUCCESS );

        IDE_TEST( qcm::getTableInfoByID(aStatement,
                                        sOldTableInfo->tableID,
                                        &sNewTableInfo,
                                        &sSCN,
                                        &sTableHandle)
                  != IDE_SUCCESS);
    }
    
    // -----------------------------------------------------
    // old tableInfo�� �����Ѵ�.
    // -----------------------------------------------------

    if ( sParseTree->tableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
    {
        if( sIndex->indexPartitionType != QCM_NONE_PARTITIONED_INDEX )
        {
            (void)qcmPartition::destroyQcmPartitionInfoList( sOldPartInfoList );
        }
        else
        {
            sOldIndexTable = sParseTree->oldIndexTables;
            
            (void)qcm::destroyQcmTableInfo(sOldIndexTable->tableInfo);
        }
    }
    else
    {
        (void)qcm::destroyQcmTableInfo(sOldTableInfo);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_META_CRASH)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCM_META_CRASH));
    }
    IDE_EXCEPTION_END;

    (void)qcm::destroyQcmTableInfo( sNewTableInfo );
    (void)qcmPartition::destroyQcmPartitionInfoList( sNewPartInfoList );
    if ( sNewIndexTable != NULL )
    {
        (void)qcm::destroyQcmTableInfo( sNewIndexTable->tableInfo );
    }
    else
    {
        /* Nothing to do */
    }

    // on failure, restore tempinfo.
    qcmPartition::restoreTempInfo( sOldTableInfo,
                                   sOldPartInfoList,
                                   sOldIndexTable );
    
    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qdx::executeAlterRebuildPartition(qcStatement * aStatement)
{
/***********************************************************************
 *
 * Description :
 *    PROJ-1502 PARTITIONED DISK TABLE
 *
 *    ALTER INDEX ... REBUILD PARTITION�� ����
 *
 * Implementation :
 *      1. �ε��� ��Ÿ ������ �����´�.
 *      2. ���̺� ��Ƽ�� ��Ÿ ���� ����Ʈ�� �����´�.
 *      3. ��Ƽ�� ������ŭ �ݺ�
 *          3-1. ������ �ε��� ��Ƽ���� ã�´�.
 *      4. �ش� �ε��� ��Ƽ�� ����
 *      5. �ε��� �籸���� ���� Ű �÷� ������ ���Ѵ�.
 *      6. �ε��� ��Ƽ�� ����
 *      7. ��Ƽ�� ��Ÿ ĳ�� ����
 *
 ***********************************************************************/

    qdIndexParseTree        * sParseTree;
    qcmIndex                * sIndex;
    qcmTableInfo            * sTableInfo  = NULL;
    qcmTableInfo            * sPartInfo   = NULL;

    qcmIndex                * sLocalIndex = NULL;
    qcmIndex                * sTempLocalIndex = NULL;
    UInt                      sLocalIndexCount;
    UInt                      sFlag = 0;

    smiColumnList           * sColumnListAtKey;
    const void              * sIndexHandle;
    smOID                     sPartitionOID;

    SChar                   * sSqlStr;
    vSLong                    sRowCnt;
    qdPartitionAttribute    * sPartAttr;
    smiSegAttr                sSegAttr;
    smiSegStorageAttr         sSegStoAttr;

    qcmTableInfo            * sNewPartInfo   = NULL;
    void                    * sNewPartHandle = NULL;
    smSCN                     sNewSCN        = SM_SCN_INIT;

    sParseTree = (qdIndexParseTree *)aStatement->myPlan->parseTree;

    sPartAttr = sParseTree->partIndex->partAttr;

    // ��Ƽ�ǵ� ���̺� LOCK(IX)
    // ��Ƽ�ǿ� LOCK(X)
    IDE_TEST( qcmPartition::validateAndLockTableAndPartitions(
                                      aStatement,
                                      sParseTree->tableHandle,
                                      sParseTree->tableSCN,
                                      sParseTree->partIndex->partInfoList,
                                      SMI_TABLE_LOCK_IX,
                                      ID_FALSE ) //aIsSetViewSCN
              != IDE_SUCCESS );

    sTableInfo = sParseTree->tableInfo;

    // �Ľ�Ʈ������ PartInfo, SCN, Handle������ �����´�.
    sPartInfo = sParseTree->partIndex->partInfoList->partitionInfo;

    // -----------------------------------------------------
    // 1. �ε��� ��Ÿ ������ �����´�.
    // -----------------------------------------------------
    IDE_TEST(qcmCache::getIndex(sTableInfo,
                                sParseTree->indexName,
                                &sIndex)
             != IDE_SUCCESS);

    // -----------------------------------------------------
    // ������ �ε��� ��Ƽ���� ã�´�.
    // -----------------------------------------------------
    for( sLocalIndexCount = 0;
         sLocalIndexCount < sPartInfo->indexCount;
         sLocalIndexCount++ )
    {
        sTempLocalIndex = & sPartInfo->indices[sLocalIndexCount];

        if( sTempLocalIndex->indexId == sIndex->indexId )
        {
            sLocalIndex = sTempLocalIndex;
            break;
        }
    }

    IDE_TEST_RAISE( sLocalIndex == NULL, ERR_META_CRASH);

    // -----------------------------------------------------
    // 4. �ش� �ε��� ��Ƽ�� ����
    // -----------------------------------------------------

    // �ε��� ������ ���� flag
    sFlag = smiTable::getIndexInfo(sLocalIndex->indexHandle);
    sSegAttr = smiTable::getIndexSegAttr(sLocalIndex->indexHandle);
    sSegStoAttr = smiTable::getIndexSegStoAttr(sLocalIndex->indexHandle);

    IDE_TEST(smiTable::dropIndex(QC_SMI_STMT( aStatement ),
                                 sPartInfo->tableHandle,
                                 sLocalIndex->indexHandle,
                                 SMI_TBSLV_DDL_DML )
             != IDE_SUCCESS);

    // -----------------------------------------------------
    // 5. �ε��� ������ ���� Ű �÷� ������ ���Ѵ�.
    // -----------------------------------------------------
    // Ű �÷��� ���� �ε����� sLocalIndex�̴�.
    IDE_TEST( qdx::getKeyColumnList( aStatement,
                                     sLocalIndex,
                                     & sColumnListAtKey )
              != IDE_SUCCESS );

    // -----------------------------------------------------
    // 6. �ε��� ��Ƽ�� ����
    // -----------------------------------------------------
    IDE_TEST( smiTable::createIndex(aStatement->mStatistics,
                                    QC_SMI_STMT( aStatement ),
                                    sPartAttr->TBSAttr.mID, // fix BUG-18937
                                    sPartInfo->tableHandle,
                                    (SChar*)sLocalIndex->name,
                                    sLocalIndex->indexId,
                                    sLocalIndex->indexTypeId,
                                    sColumnListAtKey,
                                    sFlag,
                                    QD_INDEX_DEFAULT_PARALLEL_DEGREE,
                                    SMI_INDEX_BUILD_UNCOMMITTED_ROW_DISABLE,
                                    sSegAttr,
                                    sSegStoAttr,
                                    0, /* BUG-42124 : direct key index�� partitioned table�� �������� �ʴ´�. */
                                    & sIndexHandle )
              != IDE_SUCCESS );

    // PR-14394
    IDU_LIMITPOINT("qdx::executeAlterRebuildPartition::malloc");
    IDE_TEST(STRUCT_ALLOC_WITH_SIZE(aStatement->qmxMem,
                                    SChar,
                                    QD_MAX_SQL_LENGTH,
                                    &sSqlStr)
        != IDE_SUCCESS);

    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                     "UPDATE SYS_INDICES_ "
                     "   SET LAST_DDL_TIME = SYSDATE "
                     "WHERE  INDEX_ID =  INTEGER'%"ID_INT32_FMT"' "
                     "  AND  INDEX_TYPE = INTEGER'%"ID_INT32_FMT"' ",
                     sLocalIndex->indexId,
                     sLocalIndex->indexTypeId );

    IDE_TEST(qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                sSqlStr,
                                & sRowCnt ) != IDE_SUCCESS);

    IDE_TEST_RAISE(sRowCnt != 1, ERR_META_CRASH);

    // fix BUG-18937
    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                     "UPDATE SYS_INDEX_PARTITIONS_ "
                     "   SET TBS_ID = INTEGER'%"ID_INT32_FMT"', "
                     "   LAST_DDL_TIME = SYSDATE "
                     "WHERE  INDEX_ID =  INTEGER'%"ID_INT32_FMT"' "
                     "  AND  INDEX_PARTITION_NAME = '%s' ",
                     sPartAttr->TBSAttr.mID,
                     sLocalIndex->indexId,
                     sLocalIndex->name );

    IDE_TEST(qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                sSqlStr,
                                & sRowCnt ) != IDE_SUCCESS);

    IDE_TEST_RAISE(sRowCnt != 1, ERR_META_CRASH);

    // To fix BUG-17547
    IDE_TEST(qcmPartition::touchPartition(
                    QC_SMI_STMT( aStatement ),
                    sPartInfo->partitionID)
             != IDE_SUCCESS);

    sPartitionOID = smiGetTableId(sPartInfo->tableHandle);

    // -----------------------------------------------------
    // 7. ��Ƽ�� ��Ÿ ĳ�� ����
    // -----------------------------------------------------
    IDE_TEST( qcmPartition::makeAndSetQcmPartitionInfo(
                  QC_SMI_STMT( aStatement ),
                  sPartInfo->partitionID,
                  sPartitionOID,
                  sTableInfo,
                  NULL )
              != IDE_SUCCESS );

    IDE_TEST( qcmPartition::getPartitionInfo( aStatement,
                                              sTableInfo->tableID,
                                              (UChar *)sPartInfo->name,
                                              (SInt)idlOS::strlen( sPartInfo->name ),
                                              & sNewPartInfo,
                                              & sNewSCN,
                                              & sNewPartHandle )
              != IDE_SUCCESS );

    (void)qcmPartition::destroyQcmPartitionInfo(sPartInfo);

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_META_CRASH)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCM_META_CRASH));
    }
    IDE_EXCEPTION_END;

    (void)qcmPartition::destroyQcmPartitionInfo( sNewPartInfo );

    qcmPartition::restoreTempInfoForPartition( sTableInfo,
                                               sPartInfo );

    return IDE_FAILURE;
}

IDE_RC qdx::executeAgingIndex(qcStatement * aStatement)
{
/***********************************************************************
 *
 * Description :
 *    PROJ-1704 MVCC Renewal
 *
 *    ALTER INDEX ... AGING�� ����
 *
 * Implementation :
 *    1. ��Ÿ ĳ������ �ش� �ε����� qcmIndex ����ü ã��
 *    2. smiTable::agingIndex
 *
 ***********************************************************************/

    qdIndexParseTree      * sParseTree;
    qcmIndex              * sIndex;
    qcmTableInfo          * sPartInfo;
    qcmPartitionInfoList  * sPartInfoList;
    qdIndexTableList      * sOldIndexTable;
    UInt                    i;

    sParseTree = (qdIndexParseTree *)aStatement->myPlan->parseTree;

    // Table�� ���� Lock�� ȹ���Ѵ�.
    IDE_TEST( qcm::validateAndLockTable(aStatement,
                                        sParseTree->tableHandle,
                                        sParseTree->tableSCN,
                                        SMI_TABLE_LOCK_X)
              != IDE_SUCCESS);
    // -----------------------------------------------------
    // 1. �ε��� ��Ÿ ������ �����´�.
    // -----------------------------------------------------
    IDE_TEST(qcmCache::getIndex(sParseTree->tableInfo,
                                sParseTree->indexName,
                                &sIndex)
             != IDE_SUCCESS);

    if ( sParseTree->tableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
    {
        if( sIndex->indexPartitionType != QCM_NONE_PARTITIONED_INDEX )
        {
            // ��� ��Ƽ�ǿ� LOCK(X)
            IDE_TEST( qcmPartition::validateAndLockPartitionInfoList( aStatement,
                                                                      sParseTree->partIndex->partInfoList,
                                                                      SMI_TBSLV_DDL_DML, // TBS Validation �ɼ�
                                                                      SMI_TABLE_LOCK_X,
                                                                      smiGetDDLLockTimeOut((QC_SMI_STMT(aStatement))->getTrans()) )
                      != IDE_SUCCESS );
        }
        else
        {
            sOldIndexTable = sParseTree->oldIndexTables;
            
            IDE_TEST( qdx::validateAndLockIndexTableList( aStatement,
                                                          sOldIndexTable,
                                                          SMI_TBSLV_DDL_DML, // TBS Validation �ɼ�
                                                          SMI_TABLE_LOCK_X,
                                                          smiGetDDLLockTimeOut((QC_SMI_STMT(aStatement))->getTrans()) )
                      != IDE_SUCCESS );
        }
    }
    else
    {
        // Nothing to do.
    }
    
    // -----------------------------------------------------
    // 2. �ε����� AGING
    // -----------------------------------------------------
    if ( sParseTree->tableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
    {
        if( sIndex->indexPartitionType != QCM_NONE_PARTITIONED_INDEX )
        {
            sPartInfoList = sParseTree->partIndex->partInfoList;
            
            for( ; sPartInfoList != NULL; sPartInfoList = sPartInfoList->next )
            {
                sPartInfo = sPartInfoList->partitionInfo;
                for( i = 0; i < sPartInfo->indexCount; i++ )
                {
                    /* PROJ-2464 hybrid partitioned table ����
                     *  - Disk Partition �� ��쿡�� �����ϸ�, Memory �� ��쿡 �����Ѵ�.
                     *    1. ��� Index���� �˻��Ѵ�.
                     *    2. �´ٸ�, ��ü�� �˻��� �� �۾��� �����Ѵ�.
                     */
                    /* 1. ��� Index���� �˻��Ѵ�. */
                    if ( sPartInfo->indices[i].indexId == sIndex->indexId )
                    {
                        break;
                    }
                    else
                    {
                        /* Nothing to do */
                    }
                }

                if ( i != sPartInfo->indexCount )
                {
                    /* 2. �´ٸ�, ��ü�� �˻��� �� �۾��� �����Ѵ�. */
                    if ( ( smiTableSpace::isDiskTableSpace( sPartInfo->indices[i].TBSID ) == ID_TRUE ) &&
                         ( smiIsAgableIndex( sPartInfo->indices[i].indexHandle ) == ID_TRUE ) )
                    {
                        IDE_TEST(smiTable::agingIndex(
                                     QC_SMI_STMT( aStatement ),
                                     (const void*)(sPartInfo->indices[i].indexHandle) )
                                 != IDE_SUCCESS);
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
            }
        }
        else
        {
            // PROJ-1624 global non-partitioned index
            sOldIndexTable = sParseTree->oldIndexTables;

            for ( i = 0; i < sOldIndexTable->tableInfo->indexCount; i++ )
            {
                sIndex = &(sOldIndexTable->tableInfo->indices[i]);
                
                IDE_TEST(smiTable::agingIndex(
                             QC_SMI_STMT( aStatement ),
                             (const void*)(sIndex->indexHandle) )
                         != IDE_SUCCESS);
            }
        }
    }
    else
    {
        IDE_TEST(smiTable::agingIndex(
                     QC_SMI_STMT( aStatement ),
                     (const void*)(sIndex->indexHandle) )
                 != IDE_SUCCESS);
    }

    if ( ( sParseTree->tableInfo->replicationCount > 0 ) ||
         ( QCG_GET_SESSION_IS_NEED_DDL_INFO( aStatement ) == ID_TRUE ) )
    {
        qrc::setDDLDestInfo( aStatement, 
                             1,
                             &(sParseTree->tableInfo->tableOID),
                             0,
                             NULL );
    } 

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdx::createAllIndexOfTablePart( qcStatement               * aStatement,
                                       qcmTableInfo              * aTableInfo,
                                       qcmTableInfo              * aTablePartInfo,
                                       qdIndexPartitionAttribute * aIndexTBSAttr )
{
/***********************************************************************
 *
 * Description :
 *    PROJ-1502 PARTITIONED DISK TABLE
 *
 *    Ư�� ���̺� ��Ƽ�ǿ� ���� �ε��� ��Ƽ���� �����Ѵ�.
 *    (�ٸ� ��Ƽ�ǿ� �ִ� ���� �ε����� ������ �Ȱ��� �����ؾ� �Ѵ�.)
 *
 *    �Ʒ� ������ ���� ��, �� �Լ��� ȣ��ȴ�.
 *    ALTER TABLE SPLIT PARTITION,
 *
 * Implementation :
 *
 ***********************************************************************/

    UInt                        sIndexPartID;
    SChar                       sIndexPartName[QC_MAX_OBJECT_NAME_LEN + 1];
    smiColumnList             * sColumnListAtKey = NULL;
    SChar                     * sPartMinValue    = NULL;
    SChar                     * sPartMaxValue    = NULL;
    UInt                        sFlag = 0;
    const void                * sIndexHandle;
    UInt                        sIndexCount  = 0;
    qcmTableInfo              * sNewPartInfo = NULL;
    scSpaceID                   sNewTBSID    = SC_NULL_SPACEID;
    idBool                      sFound;
    qcmIndex                  * sIndex;
    idBool                      sIsPrimary = ID_FALSE;
    qdIndexPartitionAttribute * sTempAttr  = NULL;
    smiSegAttr                  sSegAttr;
    smiSegStorageAttr           sSegStoAttr;
    UInt                        sMaxKeySize;

    smiSegAttr                  sNewSegAttr;
    smiSegStorageAttr           sNewSegStoAttr;
    UInt                        sNewFlag       = 0;
    ULong                       sNewMaxKeySize = 0L;

    sNewPartInfo = aTablePartInfo;

    // �ٸ� ���̺� ��Ƽ�ǿ� �ִ�
    // ���� �ε����� ������ �Ȱ��� �����ؾ� �Ѵ�.
    for( sIndexCount = 0;
         sIndexCount < aTableInfo->indexCount;
         sIndexCount++ )
    {
        sIndex = & aTableInfo->indices[sIndexCount];

        if ( aTableInfo->primaryKey != NULL )
        {
            if ( aTableInfo->primaryKey->indexId == sIndex->indexId )
            {
                sIsPrimary = ID_TRUE;
            }
            else
            {
                sIsPrimary = ID_FALSE;
            }
        }
        else
        {
            // Nothing to do.
        }
        
        // PROJ-1624 non-partitioned index
        // partitioned index�� ������Ѵ�.
        if ( ( sIndex->indexPartitionType != QCM_NONE_PARTITIONED_INDEX ) ||
             ( sIsPrimary == ID_TRUE ) )
        {
            sIndexPartName[0] = '\0';

            // �ε��� ID ����
            IDE_TEST( qcmPartition::getNextIndexPartitionID(
                          aStatement,
                          & sIndexPartID )
                      != IDE_SUCCESS );

            sFound = ID_FALSE;

            // �ε��� ��Ƽ���� ����� ���
            if ( aIndexTBSAttr != NULL )
            {
                // ���� �̸��� ��Ƽ�ǵ� �ε����� ã�´�.
                for ( sTempAttr = aIndexTBSAttr;
                      sTempAttr != NULL;
                      sTempAttr = sTempAttr->next )
                {
                    if( idlOS::strMatch(sIndex->name,
                                        idlOS::strlen(sIndex->name),
                                        sTempAttr->partIndexName.stmtText +
                                        sTempAttr->partIndexName.offset,
                                        sTempAttr->partIndexName.size) == 0 )
                    {
                        sNewTBSID = sTempAttr->TBSAttr.mID;

                        QC_STR_COPY( sIndexPartName, sTempAttr->indexPartName );

                        sFound = ID_TRUE;
                        break;
                    }
                }
            }
            else
            {
                /* Nothing to do */
            }

            // �ε��� ��Ƽ���� ������� ���� ���
            if( idlOS::strlen( sIndexPartName ) == 0 )
            {
                sNewTBSID = sNewPartInfo->TBSID;

                idlOS::memset( sIndexPartName, 0x00, QC_MAX_OBJECT_NAME_LEN + 1 );

                // �ε��� ��Ƽ�� �̸� ����
                idlOS::snprintf( sIndexPartName, QC_MAX_OBJECT_NAME_LEN + 1,
                                 "%sIDX_ID_%"ID_INT32_FMT"",
                                 QC_SYS_PARTITIONED_OBJ_NAME_HEADER,
                                 sIndexPartID );

                sIndexPartName[idlOS::strlen(sIndexPartName)] = '\0';
            }
            else
            {
                IDE_TEST_RAISE( sFound == ID_FALSE,
                                ERR_NOT_EXIST_PARTITIONED_INDEX );
            }

            // �ε��� ������ ���� flag
            sFlag = smiTable::getIndexInfo(aTableInfo->indices[sIndexCount].indexHandle);
            sSegAttr = smiTable::getIndexSegAttr(aTableInfo->indices[sIndexCount].indexHandle);
            sSegStoAttr = smiTable::getIndexSegStoAttr(aTableInfo->indices[sIndexCount].indexHandle);
            sMaxKeySize = smiTable::getIndexMaxKeySize( aTableInfo->indices[sIndexCount].indexHandle ); /* PROJ-2433 */

            // �ε��� ������ ���� Ű �÷� ������ ���Ѵ�.
            IDE_TEST( qdx::getKeyColumnList( aStatement,
                                             & aTableInfo->indices[sIndexCount],
                                             & sColumnListAtKey )
                      != IDE_SUCCESS );

            /* PROJ-2464 hybrid partitioned table ����
             *  - Column �Ǵ� Index �� �ϳ��� �����ؾ� �Ѵ�.
             */
            IDE_TEST( qdbCommon::adjustIndexColumn( sNewPartInfo->columns,
                                                    NULL,
                                                    NULL,
                                                    sColumnListAtKey )
                      != IDE_SUCCESS );

            /* PROJ-2464 hybrid partitioned table ����
             *  - Partition Info�� ������ ����, Table Option�� Partitioned Table�� ������ �����Ѵ�.
             *  - ����, PartInfo�� ������ �̿����� �ʰ�, TBSID�� ���� ������ ������ �����ؼ� �̿��Ѵ�.
             */
            qdbCommon::adjustIndexAttr( sNewTBSID,
                                        sSegAttr,
                                        sSegStoAttr,
                                        sFlag,
                                        sMaxKeySize,
                                        & sNewSegAttr,
                                        & sNewSegStoAttr,
                                        & sNewFlag,
                                        & sNewMaxKeySize );

            // �ε��� ����
            IDE_TEST( smiTable::createIndex(aStatement->mStatistics,
                                            QC_SMI_STMT( aStatement ),
                                            sNewTBSID,
                                            sNewPartInfo->tableHandle,
                                            (SChar*)sIndexPartName,
                                            aTableInfo->indices[sIndexCount].indexId,
                                            aTableInfo->indices[sIndexCount].indexTypeId,
                                            sColumnListAtKey,
                                            sNewFlag,
                                            QD_INDEX_DEFAULT_PARALLEL_DEGREE,
                                            SMI_INDEX_BUILD_UNCOMMITTED_ROW_ENABLE,
                                            sNewSegAttr,
                                            sNewSegStoAttr,
                                            0, /* sNewMaxKeySize, BUG-42124 : direct key index�� partitioned table�� �������� �ʴ´�. */
                                            & sIndexHandle )
                      != IDE_SUCCESS );

            // ��Ÿ ���� �Է�
            if( aTableInfo->indices[sIndexCount].indexPartitionType ==
                QCM_GLOBAL_PREFIXED_PARTITIONED_INDEX )
            {
                // ���� �۷ι� �ε����� �������� ����.
                IDE_ASSERT(0);
            }
            else
            {
                sPartMinValue = NULL;
                sPartMaxValue = NULL;
            }

            IDE_TEST(insertIndexPartitionsIntoMeta(aStatement,
                                                   aTableInfo->tableOwnerID,
                                                   aTableInfo->tableID,
                                                   aTableInfo->indices[sIndexCount].indexId,
                                                   sNewPartInfo->partitionID,
                                                   sIndexPartID,
                                                   sIndexPartName,
                                                   sPartMinValue,
                                                   sPartMaxValue,
                                                   sNewTBSID)
                     != IDE_SUCCESS);
        }
        else
        {
            // Nothing to do.
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NOT_EXIST_PARTITIONED_INDEX )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qdx::createAllIndexOfTablePart(4)",
                                  "Not exist partitioned index" ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdx::createAllIndexOfTableForAlterTablespace( qcStatement               * aStatement,
                                                     qcmTableInfo              * aOldTableInfo,
                                                     qcmTableInfo              * aNewTableInfo,
                                                     qdIndexPartitionAttribute * aIndexTBSAttr )
{
/***********************************************************************
 *
 * Description :
 *      PROJ-2465 Tablespace Alteration for Table
 *
 *      Tablespace�� �����ϱ� ����, ���� Table�� �����Ͽ� �� Table�� Index�� �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    smiColumnList             * sColumnListAtKey = NULL;
    const void                * sIndexHandle     = NULL;
    UInt                        sIndexCount      = 0;
    scSpaceID                   sNewTBSID        = SC_NULL_SPACEID;
    qcmIndex                  * sIndex           = NULL;
    qdIndexPartitionAttribute * sTempAttr        = NULL;

    smiSegAttr                  sSegAttr;
    smiSegStorageAttr           sSegStoAttr;
    UInt                        sFlag            = 0;
    UInt                        sMaxKeySize      = 0;

    smiSegAttr                  sNewSegAttr;
    smiSegStorageAttr           sNewSegStoAttr;
    UInt                        sNewFlag         = 0;
    ULong                       sNewMaxKeySize   = 0L;

    for ( sIndexCount = 0;
          sIndexCount < aOldTableInfo->indexCount;
          sIndexCount++ )
    {
        sIndex = & aOldTableInfo->indices[sIndexCount];

        // ���� �̸��� �ε����� ã�´�.
        for ( sTempAttr = aIndexTBSAttr;
              sTempAttr != NULL;
              sTempAttr = sTempAttr->next )
        {
            if ( idlOS::strMatch( sIndex->name,
                                  idlOS::strlen( sIndex->name ),
                                  sTempAttr->partIndexName.stmtText +
                                  sTempAttr->partIndexName.offset,
                                  sTempAttr->partIndexName.size ) == 0 )
            {
                sNewTBSID = sTempAttr->TBSAttr.mID;
                break;
            }
            else
            {
                /* Nothing to do */
            }
        }

        IDE_TEST_RAISE( sTempAttr == NULL, ERR_INDEX_NOT_FOUND );

        // �ε��� ������ ���� flag
        sSegAttr    = smiTable::getIndexSegAttr( sIndex->indexHandle );
        sSegStoAttr = smiTable::getIndexSegStoAttr( sIndex->indexHandle );
        sFlag       = smiTable::getIndexInfo( sIndex->indexHandle );
        sMaxKeySize = smiTable::getIndexMaxKeySize( sIndex->indexHandle ); /* PROJ-2433 */

        // �ε��� ������ ���� Ű �÷� ������ ���Ѵ�.
        IDE_TEST( qdx::getKeyColumnList( aStatement,
                                         sIndex,
                                         & sColumnListAtKey )
                  != IDE_SUCCESS );

        /* PROJ-2464 hybrid partitioned table ����
         *  - Column �Ǵ� Index �� �ϳ��� �����ؾ� �Ѵ�.
         */
        IDE_TEST( qdbCommon::adjustIndexColumn( aNewTableInfo->columns,
                                                NULL,
                                                NULL,
                                                sColumnListAtKey )
                  != IDE_SUCCESS );

        /* PROJ-2464 hybrid partitioned table ����
         *  - Partition Info�� ������ ����, Table Option�� Partitioned Table�� ������ �����Ѵ�.
         *  - ����, PartInfo�� ������ �̿����� �ʰ�, TBSID�� ���� ������ ������ �����ؼ� �̿��Ѵ�.
         */
        qdbCommon::adjustIndexAttr( sNewTBSID,
                                    sSegAttr,
                                    sSegStoAttr,
                                    sFlag,
                                    sMaxKeySize,
                                    & sNewSegAttr,
                                    & sNewSegStoAttr,
                                    & sNewFlag,
                                    & sNewMaxKeySize );

        /* BUG-42124 : direct key index�� partitioned table�� �������� �ʴ´�. */
        if ( aOldTableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
        {
            sNewMaxKeySize = 0;
        }
        else
        {
            /* Nothing to do */
        }

        // �ε��� ����
        IDE_TEST( smiTable::createIndex( aStatement->mStatistics,
                                         QC_SMI_STMT( aStatement ),
                                         sNewTBSID,
                                         aNewTableInfo->tableHandle,
                                         sIndex->name,
                                         sIndex->indexId,
                                         sIndex->indexTypeId,
                                         sColumnListAtKey,
                                         sNewFlag,
                                         QD_INDEX_DEFAULT_PARALLEL_DEGREE,
                                         SMI_INDEX_BUILD_UNCOMMITTED_ROW_ENABLE,
                                         sNewSegAttr,
                                         sNewSegStoAttr,
                                         sNewMaxKeySize,
                                         & sIndexHandle )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INDEX_NOT_FOUND )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qdx::createAllIndexOfTableForAlterTablespace",
                                  "Index Not Found" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdx::getKeyColumnList(qcStatement          * aStatement,
                             qcmIndex             * aIndex,
                             smiColumnList       ** aColumnListAtKey)
{
/***********************************************************************
 *
 * Description :
 *     PROJ-1502 PARTITIONED DISK TABLE
 *
 *     �ε��� �����带 ���� Ű �÷� ����Ʈ�� �����Ѵ�.
 *
 * Implementation :
 *      1. Ű �÷� ������ ���� ���� �Ҵ�
 *      2. Ű �÷� ������ŭ �ݺ�
 *      3. smiColumnList ����
 *
 ***********************************************************************/

    smiColumnList        * sColumnListAtKey;
    mtcColumn            * sKeyColumns;
    UInt                   sKeyColCount;

    // ------------------------------------------
    // 1. Ű �÷� ������ ���� ���� �Ҵ�
    // ------------------------------------------
    IDU_LIMITPOINT("qdx::getKeyColumnList::malloc1");
    IDE_TEST(
        aStatement->qmxMem->alloc(
            ID_SIZEOF(smiColumnList) * aIndex->keyColCount,
            (void**)&sColumnListAtKey)
        != IDE_SUCCESS);

    IDU_LIMITPOINT("qdx::getKeyColumnList::malloc2");
    IDE_TEST(
        aStatement->qmxMem->alloc(
            ID_SIZEOF(mtcColumn) * aIndex->keyColCount,
            (void**)&sKeyColumns)
        != IDE_SUCCESS);

    idlOS::memcpy( (void*)sKeyColumns,
                   (void*)aIndex->keyColumns,
                   ID_SIZEOF(mtcColumn) * aIndex->keyColCount );

    // ------------------------------------------
    // 2. Ű �÷� ������ŭ �ݺ�
    // ------------------------------------------
    for ( sKeyColCount = 0;
          sKeyColCount < aIndex->keyColCount;
          sKeyColCount++ )
    {
        // ------------------------------------------
        // 2-1. smiColumnList ����
        // ------------------------------------------

        // BUG-24012
        // key column�� order�� �����Ѵ�.
        sKeyColumns[sKeyColCount].column.flag &= ~SMI_COLUMN_ORDER_MASK;
        sKeyColumns[sKeyColCount].column.flag |=
            (aIndex->keyColsFlag[sKeyColCount] & SMI_COLUMN_ORDER_MASK);

        sColumnListAtKey[sKeyColCount].column =
            (smiColumn*) & (sKeyColumns[sKeyColCount].column);

        if (sKeyColCount == aIndex->keyColCount - 1)
        {
            sColumnListAtKey[sKeyColCount].next = NULL;
        }
        else
        {
            sColumnListAtKey[sKeyColCount].next =
                & sColumnListAtKey[sKeyColCount+1];
        }
    }

    *aColumnListAtKey = sColumnListAtKey;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdx::insertIndexIntoMeta(qcStatement *aStatement,
                                scSpaceID    aTBSID,
                                UInt         aUserID,
                                UInt         aTableID,
                                UInt         aIndexID,
                                SChar       *aIdxName,
                                SInt         aType,
                                idBool       aIsUnique,
                                SInt         aKeyColCount,
                                idBool       aIsRange,
                                idBool       aIsPartitioned,
                                UInt         aIndexTableID,
                                SInt         aFlag)
{
/***********************************************************************
 *
 * Description :
 *      CREATE INDEX �� SYS_INDICES_ �� �Է�
 *
 * Implementation :
 *      1. SYS_INDICES_ ��Ÿ ���̺� ������ �ε��� ���� �Է�
 *
 ***********************************************************************/

#define IDE_FN "qdb::insertIndexIntoMeta"

    SChar               * sSqlStr;
    static const SChar  * sTrueFalseStr[2] = {"T", "F"};
    const SChar         * sIsUnique;
    const SChar         * sIsRange;
    const SChar         * sIsPers;
    const SChar         * sIsDirectKey;
    const SChar         * sIsPart;
    vSLong                sRowCnt;

    // set parameter.
    if (aIsUnique == ID_TRUE)
    {
        sIsUnique  = sTrueFalseStr[0];
    }
    else
    {
        sIsUnique = sTrueFalseStr[1];
    }
    if (aIsRange == ID_TRUE)
    {
        sIsRange = sTrueFalseStr[0];
    }
    else
    {
        sIsRange = sTrueFalseStr[1];
    }
    if ((aFlag & SMI_INDEX_PERSISTENT_MASK) == SMI_INDEX_PERSISTENT_ENABLE)
    {
        sIsPers = sTrueFalseStr[0];
    }
    else
    {
        sIsPers = sTrueFalseStr[1];
    }
    /* PROJ-2433 Direct Key Index */
    if ( ( aFlag & SMI_INDEX_DIRECTKEY_MASK ) == SMI_INDEX_DIRECTKEY_TRUE )
    {
        sIsDirectKey = sTrueFalseStr[0];
    }
    else
    {
        sIsDirectKey = sTrueFalseStr[1];
    }
    // PRJ-1502 PARTITIONED DISK TABLE
    if( aIsPartitioned == ID_TRUE )
    {
        sIsPart = sTrueFalseStr[0];
    }
    else
    {
        sIsPart = sTrueFalseStr[1];
    }

    IDU_FIT_POINT( "qdx::insertIndexIntoMeta::alloc::sSqlStr",
                    idERR_ABORT_InsufficientMemory );

    IDE_TEST(STRUCT_ALLOC_WITH_SIZE(aStatement->qmxMem,
                                    SChar,
                                    QD_MAX_SQL_LENGTH,
                                    &sSqlStr)
             != IDE_SUCCESS);

    // PROJ-1502 PARTITIONED DISK TABLE
    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                     "INSERT INTO SYS_INDICES_ VALUES ( "
                     "INTEGER'%"ID_INT32_FMT"', "
                     "INTEGER'%"ID_INT32_FMT"', "
                     "INTEGER'%"ID_INT32_FMT"', "
                     "VARCHAR'%s', "
                     "INTEGER'%"ID_INT32_FMT"', "
                     "CHAR'%s', "
                     "INTEGER'%"ID_INT32_FMT"', "
                     "CHAR'%s', "
                     "CHAR'%s', "
                     "CHAR'%s', "
                     "INTEGER'%"ID_INT32_FMT"', "
                     "CHAR'%s', "
                     "INTEGER'%"ID_INT32_FMT"', "
                     "SYSDATE, SYSDATE )",
                     aUserID,
                     aTableID,
                     aIndexID,
                     aIdxName,
                     aType,
                     sIsUnique,
                     aKeyColCount,
                     sIsRange,
                     sIsPers,
                     sIsDirectKey, /* PROJ-2433 */
                     (mtdIntegerType) aTBSID,
                     sIsPart,
                     aIndexTableID );

    IDE_TEST_RAISE(qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                      sSqlStr,
                                      & sRowCnt ) != IDE_SUCCESS,
                   ERR_DUPLICATED_KEY);

    IDE_TEST_RAISE(sRowCnt != 1, ERR_META_CRASH);

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_META_CRASH)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCM_META_CRASH));
    }
    IDE_EXCEPTION(ERR_DUPLICATED_KEY)
    {
        // to fix BUG-6119
        if (ideGetErrorCode() == smERR_ABORT_smnUniqueViolation)
        {
            IDE_SET(ideSetErrorCode(qpERR_ABORT_QDX_DUPLICATE_INDEX));
        }
        else
        {
            // just pass error code
        }
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qdx::updateIndexPers(qcStatement *aStatement,
                            UInt         aIndexID,
                            SInt         aFlag)
{
/***********************************************************************
 *
 * Description :
 *      ALTER INDEX .. SET PERSISTENT = ON/OFF �������κ��� ȣ��
 *
 * Implementation :
 *      1. SYS_INDICES_ ��Ÿ ���̺��� IS_PERS �� ����
 *
 ***********************************************************************/

#define IDE_FN "qdb::updateIndexPers"

    SChar                 * sSqlStr;
    static const SChar    * sTrueFalseStr[2] = {"T", "F"};
    const SChar           * sIsPers;
    vSLong                  sRowCnt;

    // set parameter.
    if ((aFlag & SMI_INDEX_PERSISTENT_MASK) == SMI_INDEX_PERSISTENT_ENABLE)
    {
        sIsPers = sTrueFalseStr[0];
    }
    else
    {
        sIsPers = sTrueFalseStr[1];
    }

    IDU_FIT_POINT( "qdx::updateIndexPers::alloc::sSqlStr", 
                    idERR_ABORT_InsufficientMemory );

    IDE_TEST(STRUCT_ALLOC_WITH_SIZE(aStatement->qmxMem,
                                    SChar,
                                    QD_MAX_SQL_LENGTH,
                                    &sSqlStr)
             != IDE_SUCCESS);

    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                     "UPDATE SYS_INDICES_ "
                     "SET IS_PERS = CHAR'%s', "
                     "    LAST_DDL_TIME = SYSDATE "
                     "WHERE  INDEX_ID =  INTEGER'%"ID_INT32_FMT"' ",
                     sIsPers,
                     aIndexID );

    IDE_TEST(qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                sSqlStr,
                                & sRowCnt ) != IDE_SUCCESS);

    IDE_TEST_RAISE(sRowCnt != 1, ERR_META_CRASH);

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_META_CRASH)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCM_META_CRASH));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}


IDE_RC qdx::insertIndexColumnIntoMeta(qcStatement *aStatement,
                                      UInt         aUserID,
                                      UInt         aIndexID,
                                      UInt         aColumnID,
                                      UInt         aColIndexOrder,
                                      idBool       aIsAscending,
// TRUE : ascending, FALSE : descending
                                      UInt         aTableID)
{
/***********************************************************************
 *
 * Description :
 *      CREATE INDEX �� �ε��� �÷� �Է�
 *
 * Implementation :
 *      1. SYS_INDEX_COLUMNS_ ��Ÿ ���̺� �ε��� ���� �÷� �Է�
 *
 ***********************************************************************/

#define IDE_FN "qdx::insertIndexColumnIntoMeta"
    static const SChar *sColOrderStr[2] = {"A", "D"};
    const SChar *sColOrder;
    SChar       *sSqlStr;
    vSLong sRowCnt;

    if (aIsAscending == ID_TRUE)
    {
        sColOrder = sColOrderStr[0];
    }
    else
    {
        sColOrder = sColOrderStr[1];
    }

    IDU_FIT_POINT( "qdx::insertIndexColumnIntoMeta::alloc::sSqlStr",
                    idERR_ABORT_InsufficientMemory );

    IDE_TEST(STRUCT_ALLOC_WITH_SIZE(aStatement->qmxMem,
                                    SChar,
                                    QD_MAX_SQL_LENGTH,
                                    &sSqlStr)
             != IDE_SUCCESS);

    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                     "INSERT INTO SYS_INDEX_COLUMNS_ VALUES ( "
                     "INTEGER'%"ID_INT32_FMT"', "
                     "INTEGER'%"ID_INT32_FMT"', "
                     "INTEGER'%"ID_INT32_FMT"', "
                     "INTEGER'%"ID_INT32_FMT"', "
                     "CHAR'%s', INTEGER'%"ID_INT32_FMT"' )",
                     aUserID,
                     aIndexID,
                     aColumnID,
                     aColIndexOrder,
                     sColOrder,
                     aTableID);

    IDE_TEST(qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                sSqlStr,
                                & sRowCnt ) != IDE_SUCCESS);

    IDE_TEST_RAISE(sRowCnt != 1, ERR_META_CRASH);

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_META_CRASH)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCM_META_CRASH));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qdx::copyIndexRelatedMeta( qcStatement * aStatement,
                                  UInt          aToTableID,
                                  UInt          aToIndexID,
                                  UInt          aFromIndexID )
{
/***********************************************************************
 *
 * Description :
 *      SYS_INDEX_RELATED_�� Index ������ �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    SChar  * sSqlStr = NULL;
    vSLong   sRowCnt = ID_vLONG(0);

    IDU_FIT_POINT( "qdx::copyIndexRelatedMeta::STRUCT_ALLOC_WITH_SIZE::sSqlStr",
                   idERR_ABORT_InsufficientMemory );

    IDE_TEST( STRUCT_ALLOC_WITH_SIZE( QC_QMX_MEM( aStatement ),
                                      SChar,
                                      QD_MAX_SQL_LENGTH,
                                      & sSqlStr )
              != IDE_SUCCESS );

    idlOS::snprintf( sSqlStr,
                     QD_MAX_SQL_LENGTH,
                     "INSERT INTO SYS_INDEX_RELATED_ SELECT "
                     "USER_ID, "
                     "INTEGER'%"ID_INT32_FMT"', "
                     "INTEGER'%"ID_INT32_FMT"', "
                     "RELATED_USER_ID, RELATED_PROC_NAME FROM SYS_INDEX_RELATED_ WHERE INDEX_ID = "
                     "INTEGER'%"ID_INT32_FMT"'",
                     aToTableID,
                     aToIndexID,
                     aFromIndexID );

    IDE_TEST( qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                 sSqlStr,
                                 & sRowCnt )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdx::insertPartIndexIntoMeta(qcStatement * aStatement,
                                    UInt          aUserID,
                                    UInt          aTableID,
                                    UInt          aIndexID,
                                    UInt          aPartIndexType,
                                    SInt          aFlag)
{
/***********************************************************************
 *
 * Description :
 *      PROJ-1502 PARTITIONED DISK TABLE
 *      CREATE INDEX �� SYS_PART_INDICES_ �� �Է�
 *
 * Implementation :
 *      1. SYS_PART_INDICES_ ��Ÿ ���̺� ������ �ε��� ���� �Է�
 *
 ***********************************************************************/

    SChar               * sSqlStr;
    static const SChar  * sTrueFalseStr[2] = {"T", "F"};
    const SChar         * sIsLocalUnique;
    vSLong                sRowCnt;

    if( (aFlag & SMI_INDEX_LOCAL_UNIQUE_MASK) == SMI_INDEX_LOCAL_UNIQUE_ENABLE )
    {
        sIsLocalUnique = sTrueFalseStr[0];
    }
    else
    {
        sIsLocalUnique = sTrueFalseStr[1];
    }

    IDU_LIMITPOINT("qdx::insertPartIndexIntoMeta::malloc");
    IDE_TEST(STRUCT_ALLOC_WITH_SIZE(aStatement->qmxMem,
                                    SChar,
                                    QD_MAX_SQL_LENGTH,
                                    &sSqlStr)
             != IDE_SUCCESS);

    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                     "INSERT INTO SYS_PART_INDICES_ VALUES ( "
                     "INTEGER'%"ID_INT32_FMT"', "
                     "INTEGER'%"ID_INT32_FMT"', "
                     "INTEGER'%"ID_INT32_FMT"', "
                     "INTEGER'%"ID_INT32_FMT"', "
                     "CHAR'%s' )",
                     aUserID,
                     aTableID,
                     aIndexID,
                     aPartIndexType,
                     sIsLocalUnique );

    IDE_TEST_RAISE(qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                      sSqlStr,
                                      & sRowCnt ) != IDE_SUCCESS,
                   ERR_DUPLICATED_KEY);

    IDE_TEST_RAISE(sRowCnt != 1, ERR_META_CRASH);

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_META_CRASH)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCM_META_CRASH));
    }
    IDE_EXCEPTION(ERR_DUPLICATED_KEY)
    {
        // to fix BUG-6119
        if (ideGetErrorCode() == smERR_ABORT_smnUniqueViolation)
        {
            IDE_SET(ideSetErrorCode(qpERR_ABORT_QDX_DUPLICATE_INDEX));
        }
        else
        {
            // just pass error code
        }
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdx::insertIndexPartKeyColumnIntoMeta( qcStatement  * aStatement,
                                              UInt           aUserID,
                                              UInt           aIndexID,
                                              UInt           aColumnID,
                                              qcmTableInfo * aTableInfo,
                                              UInt           aObjType )
{
/***********************************************************************
 *
 * Description :
 *      PROJ-1502 PARTITIONED DISK TABLE
 *      CREATE INDEX �� SYS_PART_KEY_COLUMNS_ �� �Է�
 *
 * Implementation :
 *      1. SYS_PART_KEY_COLUMNS_ ��Ÿ ���̺� ������ �ε��� ���� �Է�
 *
 ***********************************************************************/

    SChar               * sSqlStr;
    vSLong                sRowCnt;
    qcmColumn           * sPartKeyCol;
    UInt                  sPartKeyColOrder;

    IDU_LIMITPOINT("qdx::insertIndexPartKeyColumnIntoMeta::malloc");
    IDE_TEST(STRUCT_ALLOC_WITH_SIZE(aStatement->qmxMem,
                                    SChar,
                                    QD_MAX_SQL_LENGTH,
                                    &sSqlStr)
             != IDE_SUCCESS);

    sPartKeyColOrder = 0;

    for( sPartKeyCol = aTableInfo->partKeyColumns;
         sPartKeyCol != NULL;
         sPartKeyCol = sPartKeyCol->next )
    {
        if ( sPartKeyCol->basicInfo->column.id == aColumnID )
        {
            idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                             "INSERT INTO SYS_PART_KEY_COLUMNS_ VALUES ( "
                             "INTEGER'%"ID_INT32_FMT"', "
                             "INTEGER'%"ID_INT32_FMT"', "
                             "INTEGER'%"ID_INT32_FMT"', "
                             "INTEGER'%"ID_INT32_FMT"', "
                             "INTEGER'%"ID_INT32_FMT"' )",
                             aUserID,
                             aIndexID,
                             sPartKeyCol->basicInfo->column.id,
                             aObjType,
                             sPartKeyColOrder );

            IDE_TEST( qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                         sSqlStr,
                                         & sRowCnt )
                      != IDE_SUCCESS );

            IDE_TEST_RAISE( sRowCnt != 1, ERR_META_CRASH );

            break;
        }
        else
        {
            /* Nothing to do */
        }

        sPartKeyColOrder++;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_META_CRASH)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCM_META_CRASH));
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdx::insertIndexPartitionsIntoMeta(
    qcStatement  * aStatement,
    UInt           aUserID,
    UInt           aTableID,
    UInt           aPartIndexID,
    UInt           aTablePartID,
    UInt           aIndexPartID,
    SChar        * aIndexPartName,
    SChar        * aPartMinValue,
    SChar        * aPartMaxValue,
    scSpaceID      aTBSID )
{
/***********************************************************************
 *
 * Description :
 *      PROJ-1502 PARTITIONED DISK TABLE
 *      CREATE INDEX �� SYS_INDEX_PARTITIONS_ �� �Է�
 *
 * Implementation :
 *      1. SYS_INDEX_PARTITIONS_ ��Ÿ ���̺� ������ �ε��� ��Ƽ�� ���� �Է�
 *
 ***********************************************************************/

    SChar               * sSqlStr;
    vSLong                sRowCnt;
    SChar               * sPartMinValueStr;
    SChar               * sPartMaxValueStr;

    IDU_LIMITPOINT("qdx::insertIndexPartitionsIntoMeta::malloc1");
    IDE_TEST(STRUCT_ALLOC_WITH_SIZE(aStatement->qmxMem,
                                    SChar,
                                    QD_MAX_SQL_LENGTH,
                                    &sSqlStr)
             != IDE_SUCCESS);

    IDU_LIMITPOINT("qdx::insertIndexPartitionsIntoMeta::malloc2");
    IDE_TEST(STRUCT_ALLOC_WITH_SIZE(aStatement->qmxMem,
                                    SChar,
                                    QD_MAX_SQL_LENGTH,
                                    &sPartMinValueStr )
             != IDE_SUCCESS);

    IDU_LIMITPOINT("qdx::insertIndexPartitionsIntoMeta::malloc3");
    IDE_TEST(STRUCT_ALLOC_WITH_SIZE(aStatement->qmxMem,
                                    SChar,
                                    QD_MAX_SQL_LENGTH,
                                    &sPartMaxValueStr )
             != IDE_SUCCESS);

    if( aPartMinValue == NULL )
    {
        idlOS::snprintf( sPartMinValueStr,
                         QD_MAX_SQL_LENGTH,
                         "%s",
                         "NULL" );
    }
    else
    {
        idlOS::snprintf( sPartMinValueStr,
                         QD_MAX_SQL_LENGTH,
                         "VARCHAR'%s'",
                         aPartMinValue );
    }

    if( aPartMaxValue == NULL )
    {
        idlOS::snprintf( sPartMaxValueStr,
                         QD_MAX_SQL_LENGTH,
                         "%s",
                         "NULL" );
    }
    else
    {
        idlOS::snprintf( sPartMaxValueStr,
                         QD_MAX_SQL_LENGTH,
                         "VARCHAR'%s'",
                         aPartMaxValue );
    }

    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                     "INSERT INTO SYS_INDEX_PARTITIONS_ VALUES ( "
                     "INTEGER'%"ID_INT32_FMT"', "
                     "INTEGER'%"ID_INT32_FMT"', "
                     "INTEGER'%"ID_INT32_FMT"', "
                     "INTEGER'%"ID_INT32_FMT"', "
                     "INTEGER'%"ID_INT32_FMT"', "
                     "VARCHAR'%s', "
                     "%s, "
                     "%s, "
                     "INTEGER'%"ID_INT32_FMT"', "
                     "SYSDATE, "
                     "SYSDATE )",
                     aUserID,
                     aTableID,
                     aPartIndexID,
                     aTablePartID,
                     aIndexPartID,
                     aIndexPartName,
                     sPartMinValueStr,
                     sPartMaxValueStr,
                     (mtdIntegerType) aTBSID );

    IDE_TEST(qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                sSqlStr,
                                & sRowCnt )
             != IDE_SUCCESS);

    IDE_TEST_RAISE(sRowCnt != 1, ERR_META_CRASH);

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_META_CRASH)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCM_META_CRASH));
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdx::validateAlterRename( qcStatement * aStatement )
{
/***********************************************************************
 *
 * Description : BUG-15235
 *       alter index [index_name] rename to [new_index_name]
 *
 * Implementation :
 *        (1) dblink�˻�
 *        (2) index���� �˻�(������ ����)
 *        (3) ���� �˻�
 *        (4) �ٲ� �̸��� �̹� �����ϴ��� �˻�
 *
 ***********************************************************************/
    
    qdIndexParseTree    * sParseTree;
    qcmIndex            * sIndex;
    UInt                  sIndexID;
    UInt                  sTableID;
    idBool                sIsFunctionBasedIndex = ID_FALSE;
    qcuSqlSourceInfo      sqlInfo;
    
    sParseTree = (qdIndexParseTree *)aStatement->myPlan->parseTree;

    // if index does not exists, raise error
    IDE_TEST(qcm::checkIndexByUser(
                 aStatement,
                 sParseTree->userNameOfIndex,
                 sParseTree->indexName,
                 &(sParseTree->userIDOfIndex),
                 &sTableID,
                 &sIndexID)
             != IDE_SUCCESS);

    /* BUG-48290 shard object�� ���� DDL ���� */
    IDE_TEST( sdi::checkShardObjectForDDL( aStatement, SDI_DDL_TYPE_INDEX ) != IDE_SUCCESS );

    IDE_TEST(qcm::getTableInfoByID(aStatement,
                                   sTableID,
                                   &(sParseTree->tableInfo),
                                   &(sParseTree->tableSCN),
                                   &(sParseTree->tableHandle))
             != IDE_SUCCESS);

    IDE_TEST(qcm::lockTableForDDLValidation(aStatement,
                                            sParseTree->tableHandle,
                                            sParseTree->tableSCN)
             != IDE_SUCCESS);

    IDE_TEST( qdpRole::checkDDLAlterIndexPriv( aStatement,
                                               sParseTree->tableInfo,
                                               sParseTree->userIDOfIndex )
              != IDE_SUCCESS );

    // cache�� ���ؼ� index������ �����´�.
    IDE_TEST(qcmCache::getIndex(sParseTree->tableInfo,
                                sParseTree->indexName,
                                &sIndex)
             != IDE_SUCCESS);
    
    /* PROJ-1090 Function-based Index */
    IDE_TEST( qmsDefaultExpr::isFunctionBasedIndex(
                  sParseTree->tableInfo,
                  sIndex,
                  &sIsFunctionBasedIndex )
              != IDE_SUCCESS );

    /* PROJ-1090 Function-based Index */
    /* Hidden Column ���̰� 128�� �ʰ��ϴ��� �˻��Ѵ�. */
    if ( ( sIsFunctionBasedIndex == ID_TRUE ) &&
         ( sParseTree->newIndexName.size > QC_MAX_FUNCTION_BASED_INDEX_NAME_LEN ) )
    {
        sqlInfo.setSourceInfo( aStatement,
                               &(sParseTree->newIndexName) );
        IDE_RAISE( ERR_MAX_NAME_LENGTH_OVERFLOW );
    }
    else
    {
        /* Nothing to do */
    }

    // PROJ-1624 global non-partitioned index
    if ( sParseTree->tableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
    {
        // ��� ��Ƽ�ǿ� LOCK(IS)
        // ��Ƽ�� ����Ʈ�� �Ľ�Ʈ���� �޾Ƴ��´�.
        IDE_TEST( qdbCommon::checkAndSetAllPartitionInfo(
                      aStatement,
                      sParseTree->tableInfo->tableID,
                      & (sParseTree->partIndex->partInfoList) )
                  != IDE_SUCCESS );

        // PROJ-1624 non-partitioned index
        if ( sIndex->indexPartitionType == QCM_NONE_PARTITIONED_INDEX )
        {
            IDE_TEST( makeAndLockIndexTable( aStatement,
                                             ID_FALSE,
                                             sIndex->indexTableID,
                                             &(sParseTree->oldIndexTables) )
                      != IDE_SUCCESS );
        }
    }
    else
    {
        // Nothing to do.
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_MAX_NAME_LENGTH_OVERFLOW )
    {
        (void)sqlInfo.init( aStatement->qmeMem );
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QCP_MAX_NAME_LENGTH_OVERFLOW,
                                  sqlInfo.getErrMessage() ) );
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qdx::executeAlterRename( qcStatement * aStatement )
{
/***********************************************************************
 *
 * Description : BUG-15235
 *       alter index [index_name] rename to [new_index_name]
 *
 * Implementation :
 *        (1) �ε����� �����ִ� table�� lock ȹ��
 *        (2) ��Ÿ�� �ε����̸� ����
 *        (3) ���̺�ĳ�� �����
 *        (4) smiTable::alterIndexName ȣ���� ���� sm���� �ε��� �̸� ����
 *        (5) hidden column name ����
 *            index_name$idx1 -> new_index_name$idx1
 *        (6) index table column name ����
 *            index_name$idx1 -> new_index_name$idx1
 *
 ***********************************************************************/
    
    qdIndexParseTree      * sParseTree;
    qcmTableInfo          * sOldTableInfo = NULL;
    qcmTableInfo          * sNewTableInfo = NULL;
    qcmIndex              * sIndex;
    UInt                    sIndexID;
    UInt                    sTableID;
    qcmPartitionInfoList  * sOldPartInfoList = NULL;
    qcmPartitionInfoList  * sNewPartInfoList = NULL;
    SChar                   sNewIndexNameStr[ QC_MAX_OBJECT_NAME_LEN + 1 ];
    qdIndexTableList      * sOldIndexTable = NULL;
    qcmIndex              * sIndexTableIndex[2];
    qcNamePosition          sIndexTableNamePos;
    qcNamePosition          sIndexNamePos;
    smSCN                   sSCN;
    qcmTableInfo          * sNewIndexTableInfo = NULL;
    void                  * sTableHandle;
    idBool                  sIsFunctionBasedIndex = ID_FALSE;
    mtcColumn             * sColumn;
    qcmColumn             * sOldColumn;
    qcmColumn             * sOldIndexTableColumn;
    qcmColumn               sNewColumn[QC_MAX_KEY_COLUMN_COUNT];
    UInt                    sColumnNumber = 0;
    UInt                    i;

    sParseTree = (qdIndexParseTree *)aStatement->myPlan->parseTree;

    // Table�� ���� Lock�� ȹ���Ѵ�.
    IDE_TEST( qcm::validateAndLockTable(aStatement,
                                        sParseTree->tableHandle,
                                        sParseTree->tableSCN,
                                        SMI_TABLE_LOCK_X)
              != IDE_SUCCESS);

    sOldTableInfo = sParseTree->tableInfo;
    
    // PROJ-1502 PARTITIONED DISK TABLE
    if( sOldTableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
    {
        // ��� ��Ƽ�ǿ� LOCK(X)
        IDE_TEST( qcmPartition::validateAndLockPartitionInfoList( aStatement,
                                                                  sParseTree->partIndex->partInfoList,
                                                                  SMI_TBSLV_DDL_DML, // TBS Validation �ɼ�
                                                                  SMI_TABLE_LOCK_X,
                                                                  smiGetDDLLockTimeOut((QC_SMI_STMT(aStatement))->getTrans()) )
                  != IDE_SUCCESS );

        // ���� ó���� ���Ͽ�, Lock�� ���� �Ŀ� Partition List�� �����Ѵ�.
        sOldPartInfoList = sParseTree->partIndex->partInfoList;
        
        // PROJ-1624 global non-partitioned index
        if ( sParseTree->oldIndexTables != NULL )
        {
            IDE_TEST( qdx::validateAndLockIndexTableList( aStatement,
                                                          sParseTree->oldIndexTables,
                                                          SMI_TBSLV_DDL_DML, // TBS Validation �ɼ�
                                                          SMI_TABLE_LOCK_X,
                                                          smiGetDDLLockTimeOut((QC_SMI_STMT(aStatement))->getTrans()))
                      != IDE_SUCCESS );

            // ���� ó���� ���Ͽ�, Lock�� ���� �Ŀ� Index Table List�� �����Ѵ�.
            sOldIndexTable = sParseTree->oldIndexTables;
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

    // cache�� ���ؼ� index������ �����´�.
    IDE_TEST(qcmCache::getIndex(sParseTree->tableInfo,
                                sParseTree->indexName,
                                &sIndex)
             != IDE_SUCCESS);

    if( qcm::checkIndexByUser(
            aStatement,
            sParseTree->userNameOfIndex,
            sParseTree->newIndexName,
            &(sParseTree->userIDOfIndex),
            &sTableID,
            &sIndexID)
        == IDE_SUCCESS )
    {
        // �ٲپ�� �� �̸��� �̹� ����. ����.
        IDE_RAISE( ERR_EXIST_OBJECT_NAME );
    }
    else
    {
        if( ideGetErrorCode() == qpERR_ABORT_QCM_NOT_EXISTS_INDEX )
        {
            // �ٲپ�� �� �̸��� ������ ����.
            // �����ڵ� Ŭ����.
            ideClearError();
        }
        else
        {
            // index��Ÿ�˻��� ����. ������ �״�� �н�
            IDE_TEST(0);
        }
    }

    // PROJ-1624 global non-partitioned index
    if ( sOldIndexTable != NULL )
    {
        // ���̸� �˻�
        IDE_TEST( checkIndexTableName( aStatement,
                                       sParseTree->userNameOfIndex,
                                       sParseTree->newIndexName,
                                       sParseTree->indexTableName,
                                       sParseTree->keyIndexName,
                                       sParseTree->ridIndexName )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }
    
    /* PROJ-1090 Function-based Index */
    /* hidden column name ���� */
    IDE_TEST( qmsDefaultExpr::isFunctionBasedIndex(
                  sParseTree->tableInfo,
                  sIndex,
                  &sIsFunctionBasedIndex )
              != IDE_SUCCESS );

    if ( sIsFunctionBasedIndex == ID_TRUE )
    {
        for ( i = 0; i < sIndex->keyColCount; i++ )
        {
            sColumn = &(sIndex->keyColumns[i]);

            IDE_TEST( qcmCache::getColumnByID( sParseTree->tableInfo,
                                               sColumn->column.id,
                                               &sOldColumn )
                      != IDE_SUCCESS );

            if ( (sOldColumn->flag & QCM_COLUMN_HIDDEN_COLUMN_MASK)
                 == QCM_COLUMN_HIDDEN_COLUMN_TRUE )
            {
                QCM_COLUMN_INIT( (&(sNewColumn[i])) );
                
                /* Hidden Column�� Name�� �����Ѵ�.
                 *    Index Name + $ + IDX + Number
                 */
                QC_STR_COPY( sNewColumn[i].name, sParseTree->newIndexName );
                (void)idlVA::appendFormat( sNewColumn[i].name,
                                           QC_MAX_OBJECT_NAME_LEN + 1,
                                           "$IDX%"ID_UINT32_FMT,
                                           ++sColumnNumber ); // �ִ� 32

                sNewColumn[i].namePos.stmtText = sNewColumn[i].name;
                sNewColumn[i].namePos.offset   = 0;
                sNewColumn[i].namePos.size     = idlOS::strlen( sNewColumn[i].name );
                
                IDE_TEST( qdbAlter::updateColumnName(
                              aStatement,
                              sOldColumn,        // old column
                              &(sNewColumn[i]) ) // new column
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
        /* Nothing to do */
    }
    
    // index�̸��� ��Ÿ���� ����.
    IDE_TEST(updateIndexNameFromMeta(aStatement,
                                     sIndex->indexId,
                                     sParseTree->newIndexName)
             != IDE_SUCCESS );

    // index�̸��� ����Ǹ� tableInfo�� �籸�� �Ǿ�� �ϹǷ�
    // touchTable�� �Ѵ�.
    IDE_TEST(qcm::touchTable( QC_SMI_STMT( aStatement ),
                              sOldTableInfo->tableID,
                              SMI_TBSLV_DDL_DML )
             != IDE_SUCCESS);

    IDE_TEST( qcm::makeAndSetQcmTableInfo(
                  QC_SMI_STMT( aStatement ),
                  sOldTableInfo->tableID,
                  smiGetTableId(sOldTableInfo->tableHandle) )
              != IDE_SUCCESS );

    IDE_TEST( qcm::getTableInfoByID( aStatement,
                                     sOldTableInfo->tableID,
                                     &sNewTableInfo,
                                     &sSCN,
                                     &sTableHandle )
              != IDE_SUCCESS );
    
    //fix BUG-22395
    QC_STR_COPY( sNewIndexNameStr, sParseTree->newIndexName );

    IDE_TEST(smiTable::alterIndexName(
            aStatement->mStatistics,
            QC_SMI_STMT( aStatement ),
            (const void*)(sParseTree->tableHandle),
            (const void*)(sIndex->indexHandle),
            sNewIndexNameStr ) != IDE_SUCCESS);

    // PROJ-1502 PARTITIONED DISK TABLE
    if ( sOldPartInfoList != NULL )
    {
        IDE_TEST( qcmPartition::touchPartitionInfoList( QC_SMI_STMT( aStatement ),
                                                        sOldPartInfoList )
                  != IDE_SUCCESS );

        IDE_TEST( qcmPartition::makeAndSetAndGetQcmPartitionInfoList( aStatement,
                                                                      sNewTableInfo,
                                                                      sOldPartInfoList,
                                                                      & sNewPartInfoList )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }
    
    // PROJ-1624 global non-partitioned index
    if ( sOldIndexTable != NULL )
    {
        //------------------------
        // rename index table
        //------------------------

        sIndexTableNamePos.stmtText = sParseTree->indexTableName;
        sIndexTableNamePos.offset   = 0;
        sIndexTableNamePos.size     = idlOS::strlen(sParseTree->indexTableName);
            
        IDE_TEST(qdbCommon::updateTableSpecFromMeta(
                     aStatement,
                     sParseTree->userNameOfIndex,
                     sIndexTableNamePos,
                     sOldIndexTable->tableID,
                     sOldIndexTable->tableOID,
                     sOldIndexTable->tableInfo->columnCount,
                     sOldIndexTable->tableInfo->parallelDegree )
                 != IDE_SUCCESS);

        //------------------------
        // rename index table column
        //------------------------
        
        if ( sIsFunctionBasedIndex == ID_TRUE )
        {
            for ( i = 0; i < sIndex->keyColCount; i++ )
            {
                sColumn = &(sIndex->keyColumns[i]);

                IDE_TEST( qcmCache::getColumnByID( sParseTree->tableInfo,
                                                   sColumn->column.id,
                                                   &sOldColumn )
                          != IDE_SUCCESS );

                if ( (sOldColumn->flag & QCM_COLUMN_HIDDEN_COLUMN_MASK)
                     == QCM_COLUMN_HIDDEN_COLUMN_TRUE )
                {
                    sOldIndexTableColumn = & sOldIndexTable->tableInfo->columns[i];
                    
                    IDE_TEST( qdbAlter::updateColumnName(
                                  aStatement,
                                  sOldIndexTableColumn, // old column
                                  &(sNewColumn[i]) )    // new column
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
            /* Nothing to do */
        }

        //------------------------
        // rename index table index
        //------------------------

        // key index, rid index�� ã�´�.
        IDE_TEST( getIndexTableIndices( sOldIndexTable->tableInfo,
                                        sIndexTableIndex )
                  != IDE_SUCCESS );

        sIndexNamePos.stmtText = sParseTree->keyIndexName;
        sIndexNamePos.offset   = 0;
        sIndexNamePos.size     = idlOS::strlen(sParseTree->keyIndexName);

        // index�̸��� ��Ÿ���� ����.
        IDE_TEST(updateIndexNameFromMeta(aStatement,
                                         sIndexTableIndex[0]->indexId,
                                         sIndexNamePos)
                 != IDE_SUCCESS );

        IDE_TEST(smiTable::alterIndexName(
                     aStatement->mStatistics,
                     QC_SMI_STMT( aStatement ),
                     (const void*)(sOldIndexTable->tableHandle),
                     (const void*)(sIndexTableIndex[0]->indexHandle),
                     sParseTree->keyIndexName )
                 != IDE_SUCCESS);
        
        sIndexNamePos.stmtText = sParseTree->ridIndexName;
        sIndexNamePos.offset   = 0;
        sIndexNamePos.size     = idlOS::strlen(sParseTree->ridIndexName);
        
        // index�̸��� ��Ÿ���� ����.
        IDE_TEST(updateIndexNameFromMeta(aStatement,
                                         sIndexTableIndex[1]->indexId,
                                         sIndexNamePos)
                 != IDE_SUCCESS );

        IDE_TEST(smiTable::alterIndexName(
                     aStatement->mStatistics,
                     QC_SMI_STMT( aStatement ),
                     (const void*)(sOldIndexTable->tableHandle),
                     (const void*)(sIndexTableIndex[1]->indexHandle),
                     sParseTree->ridIndexName )
                 != IDE_SUCCESS);
        
        // related VIEW
        IDE_TEST(qcmView::setInvalidViewOfRelated(
                     aStatement,
                     sOldIndexTable->tableInfo->tableOwnerID,
                     sOldIndexTable->tableInfo->name,
                     idlOS::strlen((SChar*)sOldIndexTable->tableInfo->name),
                     QS_TABLE)
                 != IDE_SUCCESS);

        IDE_TEST(qcm::touchTable( QC_SMI_STMT( aStatement ),
                                  sOldIndexTable->tableID,
                                  SMI_TBSLV_DDL_DML )
                 != IDE_SUCCESS);

        IDE_TEST(qcm::makeAndSetQcmTableInfo( QC_SMI_STMT( aStatement ),
                                              sOldIndexTable->tableID,
                                              sOldIndexTable->tableOID )
                 != IDE_SUCCESS);

        IDE_TEST(qcm::getTableInfoByID(aStatement,
                                       sOldIndexTable->tableID,
                                       &sNewIndexTableInfo,
                                       &sSCN,
                                       &sTableHandle)
                 != IDE_SUCCESS);

        // BUG-11266
        IDE_TEST(qcmView::recompileAndSetValidViewOfRelated(
                     aStatement,
                     sNewIndexTableInfo->tableOwnerID,
                     sNewIndexTableInfo->name,
                     idlOS::strlen((SChar*)sNewIndexTableInfo->name),
                     QS_TABLE)
                 != IDE_SUCCESS);
    }
    else
    {
        // Nothing to do.
    }
    
    (void)qcm::destroyQcmTableInfo(sOldTableInfo);

    (void)qcmPartition::destroyQcmPartitionInfoList( sOldPartInfoList );
    
    if ( sOldIndexTable != NULL )
    {
        (void)qcm::destroyQcmTableInfo(sOldIndexTable->tableInfo);
    }
    else
    {
        // Nothing to do.
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_EXIST_OBJECT_NAME);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDB_EXIST_OBJECT_NAME));
    }
    IDE_EXCEPTION_END;

    (void)qcm::destroyQcmTableInfo( sNewTableInfo );
    (void)qcmPartition::destroyQcmPartitionInfoList( sNewPartInfoList );
    (void)qcm::destroyQcmTableInfo( sNewIndexTableInfo );

    qcmPartition::restoreTempInfo( sOldTableInfo,
                                   sOldPartInfoList,
                                   sOldIndexTable );
    
    return IDE_FAILURE;
}

IDE_RC
qdx::updateIndexNameFromMeta( qcStatement *  aStatement,
                              UInt           aIndexID,
                              qcNamePosition aNewIndexName )
{
/***********************************************************************
 *
 * Description : BUG-15235
 *     index name�� �����ϴ� udpate������ meta�� ����
 *
 * Implementation :
 *        (1) sql string alloc
 *        (2) new index name position -> char string���� ��ȯ
 *        (3) update����
 *          - index�̸� ����
 *          - last ddl time �� sysdate�� ����
 *        (4) ddl ����
 *        (5) rowcount�� 1�� �ƴ� ��� meta crash error
 *
 ***********************************************************************/
    SChar     * sSqlStr;
    SChar       sNewIndexNameStr[ QC_MAX_OBJECT_NAME_LEN + 1 ];
    vSLong      sRowCnt;

    IDU_FIT_POINT( "qdx::updateIndexNameFromMeta::alloc::sSqlStr",
                    idERR_ABORT_InsufficientMemory );

    IDE_TEST( STRUCT_ALLOC_WITH_SIZE( aStatement->qmxMem,
                                      SChar,
                                      QD_MAX_SQL_LENGTH,
                                      &sSqlStr )
              != IDE_SUCCESS);

    QC_STR_COPY( sNewIndexNameStr, aNewIndexName );

    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                     "UPDATE SYS_INDICES_ "
                     "SET INDEX_NAME = '%s', "
                     "LAST_DDL_TIME = SYSDATE "
                     "WHERE INDEX_ID = INTEGER'%"ID_INT32_FMT"'",
                     sNewIndexNameStr,
                     aIndexID );

    IDE_TEST(qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                sSqlStr,
                                & sRowCnt ) != IDE_SUCCESS);

    IDE_TEST_RAISE(sRowCnt != 1, ERR_META_CRASH);

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_META_CRASH)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCM_META_CRASH));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdx::checkIndexTableName( qcStatement     * aStatement,
                                 qcNamePosition    aUserName,
                                 qcNamePosition    aIndexName,
                                 SChar           * aIndexTableName,
                                 SChar           * aKeyIndexName,
                                 SChar           * aRidIndexName )
{
/***********************************************************************
 *
 * Description : PROJ-1624 global non-partitioned index
 *      ��Ƽ�ǵ� ���̺� ���� non-partitioned index������ index table��
 *      �����Ѵ�. �̶� index table�� ������������ �˻��Ѵ�.
 *
 * Implementation :
 *      1. index name���� index table name�� �����Ѵ�.
 *      2. index table name�� �����ϴ� �� �˻��Ѵ�.
 *      3. index table�� �����ϴ� index name�� �����Ѵ�.
 *      4. index table index name�� �����ϴ� �� �˻��Ѵ�.
 *
 ***********************************************************************/

    SChar           sObjName[ QC_MAX_OBJECT_NAME_LEN + 1 ];
    qcNamePosition  sTableNamePos;
    qcNamePosition  sIndexNamePos;
    idBool          sExist = ID_FALSE;
    qsOID           sProcID;
    UInt            sUserID;
    UInt            sTableID;
    UInt            sIndexID;

    //-----------------------------------
    // index table name ����
    //-----------------------------------
    
    QC_STR_COPY( sObjName, aIndexName );

    IDE_TEST( makeIndexTableName( aStatement,
                                  aIndexName,
                                  sObjName,
                                  aIndexTableName,
                                  aKeyIndexName,
                                  aRidIndexName )
              != IDE_SUCCESS );
    
    //-----------------------------------
    // index table name �̸� �˻�
    //-----------------------------------

    sTableNamePos.stmtText = aIndexTableName;
    sTableNamePos.offset   = 0;
    sTableNamePos.size     = idlOS::strlen( aIndexTableName );
    
    // index table name �˻�
    IDE_TEST( qcm::existObject(
                  aStatement,
                  ID_FALSE,
                  aUserName,
                  sTableNamePos,
                  QS_OBJECT_MAX,
                  &sUserID,
                  &sProcID,
                  &sExist)
              != IDE_SUCCESS );

    IDE_TEST_RAISE( sExist == ID_TRUE, ERR_EXIST_OBJECT_NAME );

    //-----------------------------------
    // index table key index �̸� �˻�
    //-----------------------------------
    
    sIndexNamePos.stmtText = aKeyIndexName;
    sIndexNamePos.offset   = 0;
    sIndexNamePos.size     = idlOS::strlen( aKeyIndexName );
    
    // if same index name exists, then error
    if( qcm::checkIndexByUser(
            aStatement,
            aUserName,
            sIndexNamePos,
            &sUserID,
            &sTableID,
            &sIndexID )
        == IDE_SUCCESS )
    {
        IDE_RAISE( ERR_DUPLCATED_INDEX );
    }
    else
    {
        if( ideGetErrorCode() == qpERR_ABORT_QCM_NOT_EXISTS_INDEX )
        {
            // �ش� �ε����� �������� ������ ����.
            // �����ڵ� Ŭ����.
            ideClearError();
        }
        else
        {
            // index��Ÿ�˻��� ����. ������ �״�� �н�
            IDE_TEST(1);
        }
    }

    //-----------------------------------
    // index table rid index �̸� �˻�
    //-----------------------------------
    
    sIndexNamePos.stmtText = aRidIndexName;
    sIndexNamePos.offset   = 0;
    sIndexNamePos.size     = idlOS::strlen( aRidIndexName );
    
    // if same index name exists, then error
    if( qcm::checkIndexByUser(
            aStatement,
            aUserName,
            sIndexNamePos,
            &sUserID,
            &sTableID,
            &sIndexID )
        == IDE_SUCCESS )
    {
        IDE_RAISE( ERR_DUPLCATED_INDEX );
    }
    else
    {
        if( ideGetErrorCode() == qpERR_ABORT_QCM_NOT_EXISTS_INDEX )
        {
            // �ش� �ε����� �������� ������ ����.
            // �����ڵ� Ŭ����.
            ideClearError();
        }
        else
        {
            // index��Ÿ�˻��� ����. ������ �״�� �н�
            IDE_TEST(1);
        }
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_EXIST_OBJECT_NAME);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDB_EXIST_OBJECT_NAME));
    }
    IDE_EXCEPTION(ERR_DUPLCATED_INDEX);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDX_DUPLICATE_INDEX));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qdx::makeIndexTableName( qcStatement     * aStatement,
                         qcNamePosition    aIndexNamePos,
                         SChar           * aIndexName,
                         SChar           * aIndexTableName,
                         SChar           * aKeyIndexName,
                         SChar           * aRidIndexName )
{
/***********************************************************************
 *
 * Description : PROJ-1624 global non-partitioned index
 *      ��Ƽ�ǵ� ���̺� ���� non-partitioned index������ index table��
 *      �����Ѵ�. �̶� index table�� ������������ �˻��Ѵ�.
 *
 * Implementation :
 *      1. index name���� index table name�� �����Ѵ�.
 *      2. index table name�� �����ϴ� �� �˻��Ѵ�.
 *      3. index table�� �����ϴ� index name�� �����Ѵ�.
 *      4. index table index name�� �����ϴ� �� �˻��Ѵ�.
 *
 ***********************************************************************/

    UInt              sIndexNameSize;
    qcuSqlSourceInfo  sqlInfo;

    sIndexNameSize = idlOS::strlen( aIndexName );
    
    if ( QC_IS_NULL_NAME( aIndexNamePos ) == ID_TRUE )
    {
        // �ڵ� ������ �̸��� ����� ���̰˻�
        IDE_TEST_RAISE(
            ( sIndexNameSize + QD_INDEX_TABLE_PREFIX_SIZE           > QC_MAX_OBJECT_NAME_LEN ) ||
            ( sIndexNameSize + QD_INDEX_TABLE_KEY_INDEX_PREFIX_SIZE > QC_MAX_OBJECT_NAME_LEN ) ||
            ( sIndexNameSize + QD_INDEX_TABLE_RID_INDEX_PREFIX_SIZE > QC_MAX_OBJECT_NAME_LEN ),
            ERR_INDEX_NAME_TOO_LONG );
    }
    else
    {
        // ����ڰ� �Է��� �̸��� ��� ���̰˻�
        if ( ( sIndexNameSize + QD_INDEX_TABLE_PREFIX_SIZE           > QC_MAX_OBJECT_NAME_LEN ) ||
             ( sIndexNameSize + QD_INDEX_TABLE_KEY_INDEX_PREFIX_SIZE > QC_MAX_OBJECT_NAME_LEN ) ||
             ( sIndexNameSize + QD_INDEX_TABLE_RID_INDEX_PREFIX_SIZE > QC_MAX_OBJECT_NAME_LEN ) )
        {
            sqlInfo.setSourceInfo( aStatement, & aIndexNamePos );
            
            IDE_RAISE( ERR_INDEX_TABLE_NAME_LENGTH_OVERFLOW );
        }
        else
        {
            // Nothing to do.
        }
    }

    // index table name ����
    // "$GIT_IDX1"
    idlOS::snprintf( aIndexTableName,
                     QC_MAX_OBJECT_NAME_LEN + 1,
                     "%s%s",
                     QD_INDEX_TABLE_PREFIX,
                     aIndexName );

    // key index name ����
    // "$GIK_IDX1"
    idlOS::snprintf( aKeyIndexName,
                     QC_MAX_OBJECT_NAME_LEN + 1,
                     "%s%s",
                     QD_INDEX_TABLE_KEY_INDEX_PREFIX,
                     aIndexName );
                    
    // rid index name ����
    // "$GIR_IDX1"
    idlOS::snprintf( aRidIndexName,
                     QC_MAX_OBJECT_NAME_LEN + 1,
                     "%s%s",
                     QD_INDEX_TABLE_RID_INDEX_PREFIX,
                     aIndexName );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INDEX_NAME_TOO_LONG );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qdx::makeIndexTableName",
                                  "index name is too long" ));
    }
    IDE_EXCEPTION( ERR_INDEX_TABLE_NAME_LENGTH_OVERFLOW );
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCP_MAX_NAME_LENGTH_OVERFLOW,
                                sqlInfo.getErrMessage()));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qdx::makeColumns4CreateIndexTable( qcStatement  * aStatement,
                                   qcmColumn    * aIndexColumn,
                                   UInt           aIndexColumnCount,
                                   qcmColumn   ** aTableColumn,
                                   UInt         * aTableColumnCount )
{
/***********************************************************************
 *
 * Description : PROJ-1624 global non-partitioned index
 *
 * Implementation :
 *     index key columns | oid column | rid column
 *     ���� index table column�� �����Ѵ�.
 *
 ***********************************************************************/

    mtcColumn * sMtcColumns;
    qcmColumn * sTableColumns;
    qcmColumn * sIndexColumn;
    qcmColumn * sTableColumn;
    SChar       sIndexNameBuf[QC_MAX_OBJECT_NAME_LEN + 1];
    SChar     * sIndexName;
    SInt        sIndexNameSize;
    UInt        sColumnCount;
    UInt        i;

    // 2���� �÷��� �߰��ؾ��Ѵ�.
    sColumnCount = aIndexColumnCount + 2;
    
    //-------------------------
    // alloc qcmColumns
    //-------------------------

    IDE_TEST( QC_QMX_MEM(aStatement)->alloc( ID_SIZEOF(qcmColumn) * sColumnCount,
                                             (void**)&sTableColumns )
              != IDE_SUCCESS );
    
    IDE_TEST( QC_QMX_MEM(aStatement)->alloc( ID_SIZEOF(mtcColumn) * sColumnCount,
                                             (void**)&sMtcColumns )
              != IDE_SUCCESS );

    //-------------------------
    // copy index column
    //-------------------------
    
    for( i = 0, sIndexColumn = aIndexColumn, sTableColumn = sTableColumns;
         (i < aIndexColumnCount) && (sIndexColumn != NULL);
         i++, sIndexColumn = sIndexColumn->next, sTableColumn++ )
    {
        if ( sIndexColumn->namePos.size > 0 )
        {
            // ��������� ���� �����Ѵ�.
            QC_STR_COPY( sIndexNameBuf, sIndexColumn->namePos );
            
            // create table
            sIndexName     = sIndexNameBuf;
            sIndexNameSize = sIndexColumn->namePos.size;
        }
        else
        {
            // alter table
            sIndexName     = sIndexColumn->name;
            sIndexNameSize = idlOS::strlen( sIndexColumn->name );
        }
        
        IDE_TEST_RAISE( idlOS::strMatch( sIndexName,
                                         sIndexNameSize,
                                         QD_OID_COLUMN_NAME,
                                         QD_OID_COLUMN_NAME_SIZE ) == 0,
                        ERR_DUPLICATE_COLUMN_NAME );
        
        IDE_TEST_RAISE( idlOS::strMatch( sIndexName,
                                         sIndexNameSize,
                                         QD_RID_COLUMN_NAME,
                                         QD_RID_COLUMN_NAME_SIZE ) == 0,
                        ERR_DUPLICATE_COLUMN_NAME );

        idlOS::memcpy( sTableColumn,
                       sIndexColumn,
                       ID_SIZEOF(qcmColumn) );
    }

    for( i = 0, sIndexColumn = aIndexColumn, sTableColumn = sTableColumns;
         (i < aIndexColumnCount) && (sIndexColumn != NULL);
         i++, sIndexColumn = sIndexColumn->next, sTableColumn++, sMtcColumns++ )
    {
        idlOS::memcpy( sMtcColumns,
                       sIndexColumn->basicInfo,
                       ID_SIZEOF(mtcColumn) );
        
        sTableColumn->basicInfo = sMtcColumns;

        // flag �ʱ�ȭ
        sTableColumn->flag &= ~QCM_COLUMN_HIDDEN_COLUMN_MASK;
        sTableColumn->defaultValue = NULL;
        sTableColumn->defaultValueStr = NULL;
        sTableColumn->next = sTableColumn + 1;
    }

    //-------------------------
    // add oid column
    //-------------------------

    IDE_DASSERT( ID_SIZEOF(smOID) <= ID_SIZEOF(mtdBigintType) );
    
    // oid column�� basicInfo �ʱ�ȭ
    // dataType�� bigint, language�� session�� language�� ����
    IDE_TEST( mtc::initializeColumn(
                  sMtcColumns,
                  MTD_BIGINT_ID,
                  0,
                  0,
                  0 )
              != IDE_SUCCESS );
    
    sTableColumn->basicInfo = sMtcColumns;

    sTableColumn->flag = QCM_COLUMN_TYPE_FIXED;
    idlOS::snprintf( sTableColumn->name, QC_MAX_OBJECT_NAME_LEN + 1, QD_OID_COLUMN_NAME );
    sTableColumn->namePos.stmtText = NULL;
    sTableColumn->namePos.offset  = 0;
    sTableColumn->namePos.size    = 0;
    
    sTableColumn->defaultValue = NULL;

    sTableColumn->next            = sTableColumn + 1;
    sTableColumn->defaultValueStr = NULL;

    // PROJ-1557 varchar32k
    sTableColumn->inRowLength = ID_UINT_MAX;

    sTableColumn++;
    sMtcColumns++;
    
    //-------------------------
    // add rid column
    //-------------------------

    IDE_DASSERT( ID_SIZEOF(scGRID) <= ID_SIZEOF(mtdBigintType) );
    
    // rid column�� basicInfo �ʱ�ȭ
    // dataType�� bigint, language�� session�� language�� ����
    IDE_TEST( mtc::initializeColumn(
                  sMtcColumns,
                  MTD_BIGINT_ID,
                  0,
                  0,
                  0 )
              != IDE_SUCCESS );
    
    sTableColumn->basicInfo = sMtcColumns;

    sTableColumn->flag = QCM_COLUMN_TYPE_FIXED;
    idlOS::snprintf( sTableColumn->name, QC_MAX_OBJECT_NAME_LEN + 1, QD_RID_COLUMN_NAME );
    sTableColumn->namePos.stmtText = NULL;
    sTableColumn->namePos.offset  = 0;
    sTableColumn->namePos.size    = 0;
    
    sTableColumn->defaultValue = NULL;

    sTableColumn->next            = NULL;
    sTableColumn->defaultValueStr = NULL;

    // PROJ-1557 varchar32k
    sTableColumn->inRowLength = ID_UINT_MAX;

    *aTableColumn      = sTableColumns;
    *aTableColumnCount = sColumnCount;
    
    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_DUPLICATE_COLUMN_NAME );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDB_DUPLICATE_COLUMN,
                                  sIndexName ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qdx::createIndexTable( qcStatement       * aStatement,
                       UInt                aUserID,
                       qcNamePosition      aTableName,
                       qcmColumn         * aColumns,
                       UInt                aColumnCount,
                       scSpaceID           aTBSID,
                       smiSegAttr          aSegAttr,
                       smiSegStorageAttr   aSegStoAttr,
                       UInt                aInitFlagMask,
                       UInt                aInitFlagValue,
                       UInt                aParallelDegree,
                       qdIndexTableList ** aIndexTable )
{
/***********************************************************************
 *
 * Description : PROJ-1624 global non-partitioned index
 *
 * Implementation :
 *     index table�� �����Ѵ�.
 *
 ***********************************************************************/

    qdIndexTableList * sIndexTable;
    qcmTableInfo     * sTableInfo   = NULL;
    void             * sTableHandle;
    smSCN              sSCN;
    ULong              sMaxRows = ID_ULONG_MAX;
    UInt               sTableID;
    smOID              sTableOID;
    UInt               sCreateFlag = 0;

    /* BUG-35460 Add TABLE_TYPE G in SYS_TABLES_ */
    sCreateFlag &= ~QDV_HIDDEN_INDEX_TABLE_MASK;
    sCreateFlag |= QDV_HIDDEN_INDEX_TABLE_TRUE;
    
    IDE_TEST( qcm::getNextTableID( aStatement, &sTableID ) != IDE_SUCCESS );
    
    IDE_TEST( qdbCommon::createTableOnSM( aStatement,
                                          aColumns,
                                          aUserID,
                                          sTableID,
                                          sMaxRows,
                                          aTBSID,
                                          aSegAttr,
                                          aSegStoAttr,
                                          aInitFlagMask,
                                          aInitFlagValue,
                                          aParallelDegree,
                                          &sTableOID )
              != IDE_SUCCESS );

    IDE_TEST( qdbCommon::insertTableSpecIntoMeta( aStatement,
                                                  ID_FALSE,
                                                  sCreateFlag,
                                                  aTableName,
                                                  aUserID,
                                                  sTableOID,
                                                  sTableID,
                                                  aColumnCount,
                                                  sMaxRows,
                                                  /* PROJ-2359 Table/Partition Access Option */
                                                  QCM_ACCESS_OPTION_READ_WRITE,
                                                  aTBSID,
                                                  aSegAttr,
                                                  aSegStoAttr,
                                                  QCM_TEMPORARY_ON_COMMIT_NONE,
                                                  aParallelDegree,      // PROJ-1071
                                                  QCM_SHARD_FLAG_TABLE_NONE ) // TASK-7307
              != IDE_SUCCESS );
    
    IDE_TEST( qdbCommon::insertColumnSpecIntoMeta( aStatement,
                                                   aUserID,
                                                   sTableID,
                                                   aColumns,
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
    
    IDE_TEST( aStatement->qmxMem->alloc(
                  ID_SIZEOF(qdIndexTableList),
                  (void**)&sIndexTable )
              != IDE_SUCCESS);

    sIndexTable->tableID     = sTableID;
    sIndexTable->tableOID    = sTableOID;
    sIndexTable->tableInfo   = sTableInfo;
    sIndexTable->tableHandle = sTableHandle;
    sIndexTable->tableSCN    = sSCN;
    sIndexTable->next        = NULL;

    *aIndexTable = sIndexTable;
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    (void)qcm::destroyQcmTableInfo( sTableInfo );

    return IDE_FAILURE;
}

IDE_RC
qdx::createIndexTableIndices( qcStatement      * aStatement,
                              UInt               aUserID,
                              qdIndexTableList * aIndexTable,
                              qcmColumn        * sKeyColumns,
                              SChar            * aKeyIndexName,
                              SChar            * aRidIndexName,
                              scSpaceID          aTBSID,
                              UInt               aIndexType,
                              UInt               aIndexFlag,
                              UInt               aParallelDegree,
                              UInt               aBuildFlag,
                              smiSegAttr         aSegAttr,
                              smiSegStorageAttr  aSegStoAttr,
                              ULong              aDirectKeyMaxSize )
{
/***********************************************************************
 *
 * Description : PROJ-1624 global non-partitioned index
 *
 * Implementation :
 *     index table�� ������ ���� �����Ǿ� �ִ�.
 *     | key(1) | key(2) | ...| key(n) | oid | rid |
 *
 *     1. index table�� key index�� �����Ѵ�.
 *     2. index table�� rid index�� �����Ѵ�.
 *
 ***********************************************************************/

    qcmColumn  * sColumn;
    UInt         sIndexType;
    UInt         sIndexFlag;
    UInt         i;
    
    // �ݵ�� 2�� �̻��̴�.
    IDE_DASSERT( aIndexTable->tableInfo->columnCount > 2 );
    
    //-------------------------
    // create key index
    //-------------------------

    IDE_TEST( createIndex4IndexTable( aStatement,
                                      aUserID,
                                      aIndexTable->tableID,
                                      aIndexTable->tableHandle,
                                      aIndexTable->tableInfo->columns,
                                      aIndexTable->tableInfo->columnCount - 2,
                                      sKeyColumns,
                                      aKeyIndexName,
                                      aTBSID,
                                      aIndexType,
                                      aIndexFlag,
                                      aParallelDegree,
                                      aBuildFlag,
                                      aSegAttr,
                                      aSegStoAttr,
                                      aDirectKeyMaxSize ) /* PROJ-2433 */
              != IDE_SUCCESS );

    //-------------------------
    // create rid index
    //-------------------------

    sColumn = aIndexTable->tableInfo->columns;
    for ( i = 0; i < aIndexTable->tableInfo->columnCount - 1; i++ )
    {
        sColumn = sColumn->next;
    }

    sIndexType = mtd::getDefaultIndexTypeID( sColumn->basicInfo->module );
    sIndexFlag = SMI_INDEX_UNIQUE_ENABLE | SMI_INDEX_TYPE_NORMAL | SMI_INDEX_USE_ENABLE;
    sIndexFlag |= (aIndexFlag & SMI_INDEX_PERSISTENT_MASK);

    IDE_TEST( createIndex4IndexTable( aStatement,
                                      aUserID,
                                      aIndexTable->tableID,
                                      aIndexTable->tableHandle,
                                      sColumn,
                                      1,
                                      NULL,
                                      aRidIndexName,
                                      aTBSID,
                                      sIndexType,
                                      sIndexFlag,
                                      aParallelDegree,
                                      aBuildFlag,
                                      aSegAttr,
                                      aSegStoAttr,
                                      aDirectKeyMaxSize ) /* PROJ-2433 */
              != IDE_SUCCESS );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qdx::createIndex4IndexTable( qcStatement     * aStatement,
                             UInt              aUserID,
                             UInt              aTableID,
                             const void      * aTableHandle,
                             qcmColumn       * aColumns,
                             UInt              aColumnCount,
                             qcmColumn       * aKeyColumns,
                             SChar           * aIndexName,
                             scSpaceID         aTBSID,
                             UInt              aIndexType,
                             UInt              aIndexFlag,
                             UInt              aParallelDegree,
                             UInt              aBuildFlag,
                             smiSegAttr        aSegAttr,
                             smiSegStorageAttr aSegStoAttr,
                             ULong             aDirectKeyMaxSize )
{
/***********************************************************************
 *
 * Description : PROJ-1624 global non-partitioned index
 *
 * Implementation :
 *     1. index table�� key index�� �����Ѵ�.
 *     2. index table�� rid index�� �����Ѵ�.
 *
 ***********************************************************************/

    qcmColumn       * sColumn;
    qcmColumn       * sKeyColumn;
    smiColumnList     sColumnListAtKey[QC_MAX_KEY_COLUMN_COUNT];
    mtcColumn         sColumnAtKey[QC_MAX_KEY_COLUMN_COUNT];
    const void      * sIndexHandle;
    UInt              sIndexID;
    UInt              sOffset;
    idBool            sUnique;
    UInt              i;
    
    for ( i = 0; i < QC_MAX_KEY_COLUMN_COUNT; i++ )
    {
        sColumnListAtKey[i].next   = NULL;
        sColumnListAtKey[i].column = NULL;
    }
    
    for ( i = 0, sColumn = aColumns, sOffset = 0;
          i < aColumnCount;
          i++, sColumn = sColumn->next )
    {
        idlOS::memcpy( &(sColumnAtKey[i]),
                       sColumn->basicInfo,
                       ID_SIZEOF(mtcColumn) );
        
        // PROJ-1705
        IDE_TEST( qdbCommon::setIndexKeyColumnTypeFlag( &(sColumnAtKey[i]) )
                  != IDE_SUCCESS );

        // To Fix PR-8111
        if ( (sColumnAtKey[i].column.flag & SMI_COLUMN_TYPE_MASK)
             == SMI_COLUMN_TYPE_FIXED )
        {
            sOffset = idlOS::align( sOffset,
                                    sColumn->basicInfo->module->align );
            sColumnAtKey[i].column.offset = sOffset;
            sColumnAtKey[i].column.value = NULL;
            sOffset += sColumn->basicInfo->column.size;
        }
        else
        {
            sOffset = idlOS::align( sOffset, 8 );
            sColumnAtKey[i].column.offset = sOffset;
            sColumnAtKey[i].column.value = NULL;
            sOffset += smiGetVariableColumnSize4DiskIndex();
        }

        sColumnListAtKey[i].column = (smiColumn*) &(sColumnAtKey[i]);

        if ( i + 1 < aColumnCount )
        {
            sColumnListAtKey[i].next = &sColumnListAtKey[i+1];
        }
        else
        {
            sColumnListAtKey[i].next = NULL;
        }
    }

    // column order
    if ( aKeyColumns != NULL )
    {
        for ( i = 0, sKeyColumn = aKeyColumns;
              i < aColumnCount;
              i++, sKeyColumn = sKeyColumn->next )
        {
            // �ݵ�� �����ؾ���
            IDE_TEST_RAISE( sKeyColumn == NULL, ERR_NOT_EXIST_KEY_COLUMN );
            
            sColumnAtKey[i].column.flag &= ~SMI_COLUMN_ORDER_MASK;
            sColumnAtKey[i].column.flag |=
                (sKeyColumn->basicInfo->column.flag & SMI_COLUMN_ORDER_MASK);
        }
    }
    else
    {
        // Nothing to do.
    }
    
    IDE_TEST( qcm::getNextIndexID( aStatement, &sIndexID )
              != IDE_SUCCESS );
    
    if ( smiTable::createIndex( aStatement->mStatistics,
                                QC_SMI_STMT( aStatement ),
                                aTBSID,
                                aTableHandle,
                                aIndexName,
                                sIndexID,
                                aIndexType,
                                sColumnListAtKey,
                                aIndexFlag,
                                aParallelDegree,
                                aBuildFlag,
                                aSegAttr,
                                aSegStoAttr,
                                aDirectKeyMaxSize, /* PROJ-2433 */
                                & sIndexHandle )
         != IDE_SUCCESS )
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

    if ( (aIndexFlag & SMI_INDEX_UNIQUE_MASK) == SMI_INDEX_UNIQUE_ENABLE )
    {
        sUnique = ID_TRUE;
    }
    else
    {
        sUnique = ID_FALSE;
    }

    IDE_TEST( insertIndexIntoMeta( aStatement,
                                   aTBSID,
                                   aUserID,
                                   aTableID,
                                   sIndexID,
                                   aIndexName,
                                   aIndexType,
                                   sUnique,
                                   aColumnCount,
                                   ID_TRUE,
                                   ID_FALSE,
                                   0,
                                   aIndexFlag )
              != IDE_SUCCESS );

    for ( i = 0; i < aColumnCount; i++ )
    {
        IDE_TEST( insertIndexColumnIntoMeta(
                      aStatement,
                      aUserID,
                      sIndexID,
                      sColumnAtKey[i].column.id,
                      i,
                      ( ( (sColumnAtKey[i].column.flag &
                           SMI_COLUMN_ORDER_MASK) == SMI_COLUMN_ORDER_ASCENDING )
                        ? ID_TRUE : ID_FALSE ),
                      aTableID )
                  != IDE_SUCCESS);
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_HAS_NULL_VALUE );
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDB_NOTNULL_HAS_NULL));
    }
    IDE_EXCEPTION( ERR_NOT_EXIST_KEY_COLUMN );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qcm::createIndex4IndexTable",
                                  "key column not found" ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qdx::makeAndLockIndexTableList( qcStatement       * aStatement,
                                idBool              aInExecutionTime,
                                qcmTableInfo      * aTableInfo,
                                qdIndexTableList ** aIndexTables )
{
/***********************************************************************
 *
 * Description : PROJ-1624 global non-partitioned index
 *
 * Implementation :
 *
 ***********************************************************************/

    qcmIndex         * sIndex;
    UInt               i;

    *aIndexTables = NULL;
    
    for ( i = 0; i < aTableInfo->indexCount; i++ )
    {
        sIndex = &(aTableInfo->indices[i]);
        
        if ( sIndex->indexPartitionType == QCM_NONE_PARTITIONED_INDEX )
        {
            IDE_TEST_RAISE( sIndex->indexTableID == 0,
                            ERR_META_CRASH );
            
            IDE_TEST( makeAndLockIndexTable( aStatement,
                                             aInExecutionTime,
                                             sIndex->indexTableID,
                                             aIndexTables )
                      != IDE_SUCCESS );
        }
        else
        {
            if ( sIndex->indexTableID != 0 )
            {
                // partitioned index��� index table�� �����ϸ� �ȵ�
                ideLog::log( IDE_QP_0,
                             "Warning : a partitioned index has index table id "
                             "[IndexID-%"ID_UINT32_FMT", "
                             "IndexTableID-%"ID_UINT32_FMT"]",
                             sIndex->indexId,
                             sIndex->indexTableID );
            }
            else
            {
                // Nothing to do.
            }
        }
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_META_CRASH)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCM_META_CRASH));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qdx::makeAndLockIndexTable( qcStatement       * aStatement,
                            idBool              aInExecutionTime,
                            UInt                aIndexTableID,
                            qdIndexTableList ** aIndexTables )
{
/***********************************************************************
 *
 * Description : PROJ-1624 global non-partitioned index
 *
 * Implementation :
 *
 ***********************************************************************/

    qdIndexTableList * sIndexTable;
    qcmTableInfo     * sTableInfo;
    void             * sTableHandle;
    smSCN              sSCN;

    IDE_TEST_RAISE( aIndexTableID == 0, ERR_META_CRASH );

    if ( aInExecutionTime == ID_TRUE )
    {
        IDE_TEST( QC_QMX_MEM(aStatement)->alloc(
                      ID_SIZEOF(qdIndexTableList),
                      (void**)&sIndexTable )
                  != IDE_SUCCESS);
    }
    else
    {
        IDE_TEST( QC_QMP_MEM(aStatement)->alloc(
                      ID_SIZEOF(qdIndexTableList),
                      (void**)&sIndexTable )
                  != IDE_SUCCESS);
    }
            
    IDE_TEST( qcm::getTableInfoByID( aStatement,
                                     aIndexTableID,
                                     &sTableInfo,
                                     &sSCN,
                                     &sTableHandle )
              != IDE_SUCCESS );

    if ( aInExecutionTime == ID_TRUE )
    {
        // execution time�ÿ��� ���� lock�� ȹ���Ѵ�.
        // Nothing to do.
    }
    else
    {
        // validation lock
        IDE_TEST( smiValidateAndLockObjects( (QC_SMI_STMT( aStatement ))->getTrans(),
                                             sTableHandle,
                                             sSCN,
                                             SMI_TBSLV_DDL_DML, // TBS Validation �ɼ�
                                             SMI_TABLE_LOCK_IS,
                                             smiGetDDLLockTimeOut((QC_SMI_STMT(aStatement))->getTrans()),
                                             ID_FALSE ) // BUG-28752 ����� Lock�� ������ Lock�� �����մϴ�.
                  != IDE_SUCCESS );
    }
    
    sIndexTable->tableID     = aIndexTableID;
    sIndexTable->tableOID    = smiGetTableId(sTableHandle);
    sIndexTable->tableInfo   = sTableInfo;
    sIndexTable->tableHandle = sTableHandle;
    sIndexTable->tableSCN    = sSCN;
    sIndexTable->next        = NULL;

    // link index table
    sIndexTable->next = *aIndexTables;
    *aIndexTables = sIndexTable;
    
    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_META_CRASH)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCM_META_CRASH));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdx::validateAndLockIndexTableList( qcStatement         * aStatement,
                                           qdIndexTableList    * aIndexTables,
                                           smiTBSLockValidType   aTBSLvType,
                                           smiTableLockMode      aLockMode,
                                           ULong                 aLockWaitMicroSec )
{
/***********************************************************************
 *
 * Description : PROJ-1624 global non-partitioned index
 *               DDL���� ȣ���Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    qdIndexTableList * sIndexTable;

    for ( sIndexTable = aIndexTables;
          sIndexTable != NULL;
          sIndexTable = sIndexTable->next )
    {
        IDE_TEST(smiValidateAndLockObjects( (QC_SMI_STMT( aStatement ))->getTrans(),
                                            sIndexTable->tableHandle,
                                            sIndexTable->tableSCN,
                                            aTBSLvType, // TBS Validation �ɼ�
                                            aLockMode,
                                            aLockWaitMicroSec,
                                            ID_FALSE ) // BUG-28752 ����� Lock�� ������ Lock�� �����մϴ�.
                 != IDE_SUCCESS);
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qdx::dropIndexTable( qcStatement      * aStatement,
                     qdIndexTableList * aIndexTable,
                     idBool             aIsDropTablespace )
{
/***********************************************************************
 *
 * Description : PROJ-1624 global non-partitioned index
 *
 * Implementation :
 *
 ***********************************************************************/

    qcmTableInfo  * sTableInfo;
    UInt            sTableID;

    sTableInfo = aIndexTable->tableInfo;
    sTableID   = aIndexTable->tableID;
    
    // DELETE from qcm_constraints_ where table id = aTableID;
    IDE_TEST(qdd::deleteConstraintsFromMeta(aStatement, sTableID)
             != IDE_SUCCESS);

    IDE_TEST(qdd::deleteIndicesFromMeta(aStatement, sTableInfo)
             != IDE_SUCCESS);

    IDE_TEST(qdd::deleteTableFromMeta(aStatement, sTableID)
             != IDE_SUCCESS);

    IDE_TEST(qdpDrop::removePriv4DropTable(aStatement, sTableID)
             != IDE_SUCCESS);

    // related VIEW
    IDE_TEST(qcmView::setInvalidViewOfRelated(
                 aStatement,
                 sTableInfo->tableOwnerID,
                 sTableInfo->name,
                 idlOS::strlen(sTableInfo->name),
                 QS_TABLE)
             != IDE_SUCCESS);

    // BUG-21387 COMMENT
    IDE_TEST(qdbComment::deleteCommentTable(
                 aStatement,
                 sTableInfo->tableOwnerName,
                 sTableInfo->name )
             != IDE_SUCCESS);

    // PROJ-2223 audit
    IDE_TEST( qcmAudit::deleteObject(
                  aStatement,
                  sTableInfo->tableOwnerID,
                  sTableInfo->name )
              != IDE_SUCCESS );

    // BUG-35135
    IDE_TEST( smiTable::dropTable( QC_SMI_STMT( aStatement ),
                                   sTableInfo->tableHandle,
                                   ( aIsDropTablespace == ID_TRUE ) ? SMI_TBSLV_DROP_TBS:
                                                                      SMI_TBSLV_DDL_DML )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qdx::makeSmiValue4IndexTableWithSmiValue( smiValue     * aInsertedTableRow,
                                          smiValue     * aInsertedIndexRow,
                                          qcmIndex     * aTableIndex,
                                          smOID        * aPartOID,
                                          scGRID       * aRowGRID )
{
/***********************************************************************
 *
 * Description : PROJ-1624 global non-partitioned index
 *
 * Implementation :
 *
 ***********************************************************************/

    UInt  sColOrder;
    UInt  i;
    
    // key column
    for ( i = 0; i < aTableIndex->keyColCount; i++ )
    {
        sColOrder = aTableIndex->keyColumns[i].column.id & SMI_COLUMN_ID_MASK;
        aInsertedIndexRow[i] = aInsertedTableRow[sColOrder];
    }
    
    // oid
    aInsertedIndexRow[i].value = (void*) aPartOID;
    aInsertedIndexRow[i].length = ID_SIZEOF(mtdBigintType);
    i++;
    
    // rid
    aInsertedIndexRow[i].value = (void*) aRowGRID;
    aInsertedIndexRow[i].length = ID_SIZEOF(mtdBigintType);
    
    return IDE_SUCCESS;
}

IDE_RC
qdx::makeSmiValue4IndexTableWithRow( const void   * aTableRow,
                                     qcmColumn    * aTableColumn,
                                     smiValue     * aInsertedRow,
                                     smOID        * aPartOID,
                                     scGRID       * aRowGRID )
{
/***********************************************************************
 *
 * Description : PROJ-1624 global non-partitioned index
 *
 * Implementation :
 *
 ***********************************************************************/

    qcmColumn   * sTableColumn;
    mtcColumn   * sColumn;
    void        * sValue;
    UInt          sNonStoringSize;
    UInt          i;
    
    // key column
    for ( i = 0, sTableColumn = aTableColumn;
          sTableColumn != NULL;
          i++, sTableColumn = sTableColumn->next )
    {
        sColumn = sTableColumn->basicInfo;
        sValue = (void*)((UChar*)aTableRow + sColumn->column.offset);

        if ( sColumn->module->isNull( NULL, sValue ) == ID_TRUE )
        {
            aInsertedRow[i].value  = NULL;
            aInsertedRow[i].length = 0;
        }
        else
        {
            IDE_TEST( mtc::getNonStoringSize(&(sColumn->column), &sNonStoringSize )
                      != IDE_SUCCESS );
            
            aInsertedRow[i].value  = (UChar*)sValue + sNonStoringSize;
            aInsertedRow[i].length =
                sColumn->module->actualSize( sColumn, sValue ) - sNonStoringSize;
        }
    }
    
    // oid
    aInsertedRow[i].value = (void*) aPartOID;
    aInsertedRow[i].length = ID_SIZEOF(mtdBigintType);
    i++;
    
    // rid
    aInsertedRow[i].value = (void*) aRowGRID;
    aInsertedRow[i].length = ID_SIZEOF(mtdBigintType);
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdx::updateIndexSpecFromMeta( qcStatement  * aStatement,
                                     UInt           aIndexID,
                                     UInt           aIndexTableID )
{
/***********************************************************************
 *
 * Description :
 *
 * Implementation :
 *
 ***********************************************************************/

    SChar                   * sSqlStr;
    vSLong                    sRowCnt;

    IDE_TEST(STRUCT_ALLOC_WITH_SIZE(aStatement->qmxMem,
                                    SChar,
                                    QD_MAX_SQL_LENGTH,
                                    &sSqlStr)
             != IDE_SUCCESS);

    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                     "UPDATE SYS_INDICES_ "
                     "SET INDEX_TABLE_ID = INTEGER'%"ID_INT32_FMT"' "
                     "WHERE INDEX_ID = INTEGER'%"ID_INT32_FMT"' ",
                     aIndexTableID,
                     aIndexID );

    IDE_TEST(qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                sSqlStr,
                                & sRowCnt ) != IDE_SUCCESS);

    IDE_TEST_RAISE(sRowCnt != 1, ERR_META_CRASH);

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_META_CRASH)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCM_META_CRASH));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdx::buildIndexTable( qcStatement          * aStatement,
                             qdIndexTableList     * aIndexTable,
                             qcmColumn            * aTableColumns,
                             UInt                   aTableColumnCount,
                             qcmTableInfo         * aTableInfo,
                             qcmPartitionInfoList * aPartInfoList )
{
/***********************************************************************
 *
 * Description :
 *
 * Implementation :
 *
 ***********************************************************************/

    // Table Cursor�� ���� ��������
    smiTableCursor         sCursor;
    smiCursorProperties    sCursorProperty;
    idBool                 sCursorOpen = ID_FALSE;
    
    // Partition Cursor�� ���� ��������
    smiTableCursor         sPartCursor;
    smiCursorProperties    sPartCursorProperty;
    idBool                 sPartCursorOpen = ID_FALSE;
    smiFetchColumnList   * sFetchColumnList;

    // Record �˻��� ���� ���� ����
    UInt                   sRowSize;
    void                 * sTmpRow;
    const void           * sRow;
    const void           * sOrgRow = NULL;
    scGRID                 sTmpRid;
    scGRID                 sRowGRID;
    smOID                  sPartOID;
    smiValue             * sInsertedRow;
    
    qcmPartitionInfoList * sPartInfoList;
    qcmTableInfo         * sPartInfo;
    qcmColumn            * sColumn;
    UInt                   i;

    //---------------------------------------------
    // ���ռ� �˻�
    //---------------------------------------------
    
    IDE_DASSERT( aTableInfo->tablePartitionType == QCM_PARTITIONED_TABLE );
    
    //---------------------------------------------
    // �ʱ�ȭ
    //---------------------------------------------
    
    sCursor.initialize();
    sPartCursor.initialize();
    
    SMI_CURSOR_PROP_INIT_FOR_FULL_SCAN( &sCursorProperty, aStatement->mStatistics );
    SMI_CURSOR_PROP_INIT_FOR_FULL_SCAN( &sPartCursorProperty, aStatement->mStatistics );

    sCursorProperty.mIsUndoLogging = ID_FALSE;
    
    //----------------------------
    // Record ���� Ȯ��
    //----------------------------

    // Disk Table�� ���
    // Record Read�� ���� ������ �Ҵ��Ѵ�.
    // To Fix BUG-12977 : parent�� rowsize�� �ƴ�, �ڽ��� rowsize��
    //                    ������ �;���
    IDE_TEST( qdbCommon::getDiskRowSize( aTableInfo,
                                         & sRowSize )
              != IDE_SUCCESS );
    
    // To fix BUG-14820
    // Disk-variable �÷��� rid�񱳸� ���� �ʱ�ȭ �ؾ� ��.
    IDE_TEST( aStatement->qmxMem->cralloc( sRowSize,
                                           (void **) & sRow )
              != IDE_SUCCESS);

    sOrgRow = sRow;

    IDE_TEST( aStatement->qmxMem->alloc(
            ID_SIZEOF(smiValue) * aIndexTable->tableInfo->columnCount,
            (void**)&sInsertedRow)
        != IDE_SUCCESS);

    //----------------------------
    // open cusrsor
    //----------------------------

    IDE_TEST( sCursor.open( QC_SMI_STMT( aStatement ),
                            aIndexTable->tableHandle,
                            NULL,
                            aIndexTable->tableSCN,
                            NULL,
                            smiGetDefaultKeyRange(),
                            smiGetDefaultKeyRange(),
                            smiGetDefaultFilter(),
                            SMI_LOCK_WRITE|SMI_TRAVERSE_FORWARD|
                            SMI_PREVIOUS_DISABLE,
                            SMI_INSERT_CURSOR,
                            & sCursorProperty )
              != IDE_SUCCESS );

    sCursorOpen = ID_TRUE;
    
    //--------------------------------------
    // PROJ-1705 fetch column list ����
    //--------------------------------------

    // fetch column list�� �ʱ�ȭ�Ѵ�.
    qdbCommon::initFetchColumnList( & sFetchColumnList );
    
    // fetch column list�� �����Ѵ�.
    for ( i = 0, sColumn = aTableColumns;
          i < aTableColumnCount;
          i++, sColumn = sColumn->next )
    {
        IDE_TEST( qdbCommon::addFetchColumnList(
                      aStatement->qmxMem,
                      sColumn->basicInfo,
                      & sFetchColumnList )
                  != IDE_SUCCESS );
    }
    
    sPartCursorProperty.mFetchColumnList = sFetchColumnList;

    //--------------------------------------
    // read row
    //--------------------------------------
    
    for ( sPartInfoList = aPartInfoList;
          sPartInfoList != NULL;
          sPartInfoList = sPartInfoList->next )
    {
        sPartInfo = sPartInfoList->partitionInfo;

        sPartOID = smiGetTableId( sPartInfo->tableHandle );
        
        IDE_TEST(sPartCursor.open( QC_SMI_STMT( aStatement ),
                                   sPartInfo->tableHandle,
                                   NULL,
                                   smiGetRowSCN(sPartInfo->tableHandle),
                                   NULL,
                                   smiGetDefaultKeyRange(),
                                   smiGetDefaultKeyRange(),
                                   smiGetDefaultFilter(),
                                   SMI_LOCK_READ|
                                   SMI_TRAVERSE_FORWARD|
                                   SMI_PREVIOUS_DISABLE,
                                   SMI_SELECT_CURSOR,
                                   & sPartCursorProperty )
                 != IDE_SUCCESS);

        sPartCursorOpen = ID_TRUE;

        IDE_TEST( sPartCursor.beforeFirst() != IDE_SUCCESS );

        //----------------------------
        // �ݺ� �˻�
        //----------------------------

        IDE_TEST( sPartCursor.readRow( & sRow, & sRowGRID, SMI_FIND_NEXT)
                  != IDE_SUCCESS );

        while ( sRow != NULL )
        {
            //------------------------------
            // index table�� insert
            //------------------------------

            // make smiValues
            IDE_TEST( makeSmiValue4IndexTableWithRow( sRow,
                                                      aTableColumns,
                                                      sInsertedRow,
                                                      & sPartOID,
                                                      & sRowGRID )
                      != IDE_SUCCESS );
            
            // insert index table
            IDE_TEST( sCursor.insertRow( sInsertedRow,
                                         & sTmpRow,
                                         & sTmpRid )
                      != IDE_SUCCESS );

            IDE_TEST( sPartCursor.readRow( & sRow, & sRowGRID, SMI_FIND_NEXT )
                      != IDE_SUCCESS );

        }
        sRow = sOrgRow;

        sPartCursorOpen = ID_FALSE;
        IDE_TEST( sPartCursor.close() != IDE_SUCCESS );
    }

    sCursorOpen = ID_FALSE;
    IDE_TEST( sCursor.close() != IDE_SUCCESS );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sPartCursorOpen == ID_TRUE )
    {
        (void) sPartCursor.close();
    }

    if ( sCursorOpen == ID_TRUE )
    {
        (void) sCursor.close();
    }

    return IDE_FAILURE;
}
    
IDE_RC
qdx::getIndexTableIndices( qcmTableInfo * aIndexTableInfo,
                           qcmIndex     * aIndexTableIndex[2] )
{
/***********************************************************************
 *
 * Description :
 *
 * Implementation :
 *
 ***********************************************************************/

    qcmIndex * sIndex;
    UInt       i;

    // �ʱ�ȭ
    aIndexTableIndex[0] = NULL;
    aIndexTableIndex[1] = NULL;
    
    for ( i = 0; i < aIndexTableInfo->indexCount; i++ )
    {
        sIndex = &(aIndexTableInfo->indices[i]);

        if ( ( idlOS::strlen( sIndex->name ) >= QD_INDEX_TABLE_KEY_INDEX_PREFIX_SIZE ) &&
             ( idlOS::strMatch( sIndex->name,
                                QD_INDEX_TABLE_KEY_INDEX_PREFIX_SIZE,
                                QD_INDEX_TABLE_KEY_INDEX_PREFIX,
                                QD_INDEX_TABLE_KEY_INDEX_PREFIX_SIZE ) == 0 ) )
        {
            aIndexTableIndex[0] = sIndex;
        }
        else if ( ( idlOS::strlen( sIndex->name ) >= QD_INDEX_TABLE_RID_INDEX_PREFIX_SIZE ) &&
                  ( idlOS::strMatch( sIndex->name,
                                     QD_INDEX_TABLE_RID_INDEX_PREFIX_SIZE,
                                     QD_INDEX_TABLE_RID_INDEX_PREFIX,
                                     QD_INDEX_TABLE_RID_INDEX_PREFIX_SIZE ) == 0 ) )
        {
            aIndexTableIndex[1] = sIndex;
        }
        else
        {
            // Nothing to do.
        }
    }
                        
    // index table�� index�� �ݵ�� �����Ѵ�.
    IDE_TEST_RAISE( ( aIndexTableIndex[0] == NULL ) ||
                    ( aIndexTableIndex[1] == NULL ) ,
                    ERR_META_CRASH );

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_META_CRASH)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCM_META_CRASH));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qdx::makeColumns4BuildIndexTable( qcStatement   * aStatement,
                                  qcmTableInfo  * aTableInfo,
                                  mtcColumn     * aKeyColumns,
                                  UInt            aKeyColumnCount,
                                  qcmColumn    ** aColumns,
                                  UInt          * aColumnCount )
{
/***********************************************************************
 *
 * Description :
 *
 * Implementation :
 *
 ***********************************************************************/

    mtcColumn  * sKeyColumn;
    qcmColumn  * sTableColumn;
    qcmColumn  * sColumns;
    UInt         sColOrder;
    UInt         i;

    IDE_TEST( STRUCT_ALLOC_WITH_COUNT( aStatement->qmxMem,
                                       qcmColumn,
                                       aKeyColumnCount,
                                       & sColumns )
              != IDE_SUCCESS );
    
    for ( i = 0; i < aKeyColumnCount; i++ )
    {
        sKeyColumn = &(aKeyColumns[i]);
        
        sColOrder = sKeyColumn->column.id & SMI_COLUMN_ID_MASK;

        IDE_TEST_RAISE( sColOrder >= aTableInfo->columnCount,
                        ERR_META_CRASH );

        sTableColumn = &(aTableInfo->columns[sColOrder]);
        
        IDE_TEST_RAISE( sKeyColumn->column.id != sTableColumn->basicInfo->column.id,
                        ERR_META_CRASH );

        idlOS::memcpy( &(sColumns[i]),
                       sTableColumn,
                       ID_SIZEOF(qcmColumn) );

        if ( i + 1 < aKeyColumnCount )
        {
            sColumns[i].next = &(sColumns[i+1]);
        }
        else
        {
            sColumns[i].next = NULL;
        }
    }

    *aColumns = sColumns;
    *aColumnCount = aKeyColumnCount;
    
    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_META_CRASH)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCM_META_CRASH));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qdx::makeColumns4ModifyColumn( qcStatement   * aStatement,
                               qcmTableInfo  * aTableInfo,
                               mtcColumn     * aKeyColumns,
                               UInt            aKeyColumnCount,
                               scSpaceID       aTBSID,
                               qcmColumn    ** aColumns,
                               UInt          * aColumnCount )
{
/***********************************************************************
 *
 * Description :
 *
 * Implementation :
 *
 ***********************************************************************/

    qcmColumn  * sTmpColumns;
    UInt         sTmpColumnCount;
    qcmColumn  * sColumns;
    UInt         sColumnCount;

    // key column���� qcmColumn�� ����
    IDE_TEST( makeColumns4BuildIndexTable( aStatement,
                                           aTableInfo,
                                           aKeyColumns,
                                           aKeyColumnCount,
                                           &sTmpColumns,
                                           &sTmpColumnCount )
              != IDE_SUCCESS );

    // qcmColumn���� index table�� qcmColumn(+oid,+rid)�� ����
    IDE_TEST( makeColumns4CreateIndexTable( aStatement,
                                            sTmpColumns,
                                            sTmpColumnCount,
                                            &sColumns,
                                            &sColumnCount )
              != IDE_SUCCESS );
    
    // index table columns �˻�
    IDE_TEST( qdbCommon::validateColumnListForCreateInternalTable(
                  aStatement,
                  ID_TRUE,  // in execution time
                  SMI_TABLE_DISK,
                  aTBSID,
                  sColumns )
              != IDE_SUCCESS );

    *aColumns = sColumns;
    *aColumnCount = sColumnCount;
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qdx::findIndexTableInList( qdIndexTableList  * aIndexTables,
                           UInt                aIndexTableID,
                           qdIndexTableList ** aFoundIndexTable )
{
/***********************************************************************
 *
 * Description : PROJ-1623 non-partitioned index
 *     index table list���� indexTableID�� �ش��ϴ� index table�� ã�´�.
 *
 * Implementation :
 *
 ***********************************************************************/

    qdIndexTableList  * sIndexTable;
    idBool              sFound = ID_FALSE;

    for ( sIndexTable = aIndexTables;
          sIndexTable != NULL;
          sIndexTable = sIndexTable->next )
    {
        if ( sIndexTable->tableID == aIndexTableID )
        {
            sFound = ID_TRUE;
            break;
        }
        else
        {
            // Nothing to do.
        }
    }
                
    IDE_TEST_RAISE( sFound == ID_FALSE, ERR_META_CRASH );

    *aFoundIndexTable = sIndexTable;
    
    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_META_CRASH)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCM_META_CRASH));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qdx::findIndexTableIDInIndices( qcmIndex       * aIndices,
                                UInt             aIndexCount,
                                UInt             aIndexTableID,
                                qcmIndex      ** aFoundIndex )
{
/***********************************************************************
 *
 * Description : PROJ-1623 non-partitioned index
 *     aIndices���� indexTableID�� �ش��ϴ� index�� ã�´�.
 *
 * Implementation :
 *
 ***********************************************************************/

    qcmIndex  * sIndex;
    idBool      sFound = ID_FALSE;
    UInt        i;

    for ( i = 0; i < aIndexCount; i++ )
    {
        sIndex = &(aIndices[i]);
        
        if ( ( sIndex->indexPartitionType == QCM_NONE_PARTITIONED_INDEX ) &&
             ( sIndex->indexTableID == aIndexTableID ) )
        {
            *aFoundIndex = sIndex;
            sFound = ID_TRUE;
            break;
        }
        else
        {
            // Nothing to do.
        }
    }
                
    IDE_TEST_RAISE( sFound == ID_FALSE, ERR_META_CRASH );

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_META_CRASH)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCM_META_CRASH));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qdx::findIndexIDInIndices( qcmIndex     * aIndices,
                           UInt           aIndexCount,
                           UInt           aIndexID,
                           qcmIndex    ** aFoundIndex )
{
/***********************************************************************
 *
 * Description : PROJ-1623 non-partitioned index
 *     aIndices���� indexID�� �ش��ϴ� index�� ã�´�.
 *
 * Implementation :
 *
 ***********************************************************************/

    qcmIndex  * sIndex;
    idBool      sFound = ID_FALSE;
    UInt        i;

    for ( i = 0; i < aIndexCount; i++ )
    {
        sIndex = &(aIndices[i]);
        
        if ( sIndex->indexId == aIndexID )
        {
            *aFoundIndex = sIndex;
            sFound = ID_TRUE;
            break;
        }
        else
        {
            // Nothing to do.
        }
    }
    
    IDE_TEST_RAISE( sFound == ID_FALSE, ERR_META_CRASH );

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_META_CRASH)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCM_META_CRASH));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qdx::createIndexTableOfTable( qcStatement       * aStatement,
                              qcmTableInfo      * aTableInfo,
                              qcmIndex          * aNewIndices,
                              qdIndexTableList  * aOldIndexTables,
                              qdIndexTableList ** aNewIndexTables )
{
/***********************************************************************
 *
 * Description :
 *
 * Implementation :
 *
 ***********************************************************************/

    qdIndexTableList  * sOldIndexTable;
    qdIndexTableList  * sNewIndexTable;
    qcmIndex          * sIndex;
    qcmIndex          * sIndexTableIndex[2];
    qcNamePosition      sIndexTableNamePos;
    qcmTableInfo      * sIndexTableInfo;
    UInt                sIndexTableFlag;
    UInt                sIndexTableParallelDegree;
    UInt                sFlag;
    smiSegAttr          sSegAttr;
    smiSegStorageAttr   sSegStoAttr;
    UInt                i;

    qcmColumn         * sTempColumns = NULL;

    for ( i = 0; i < aTableInfo->indexCount; i++ )
    {
        sIndex = &(aTableInfo->indices[i]);
        
        if ( sIndex->indexPartitionType == QCM_NONE_PARTITIONED_INDEX )
        {
            // non-partitioned index�� �ش��ϴ� index table�� ã�´�.
            IDE_TEST( findIndexTableInList( aOldIndexTables,
                                            sIndex->indexTableID,
                                            & sOldIndexTable )
                      != IDE_SUCCESS );
            
            sIndexTableFlag = sOldIndexTable->tableInfo->tableFlag;
            sIndexTableParallelDegree = sOldIndexTable->tableInfo->parallelDegree;

            sIndexTableNamePos.stmtText = sOldIndexTable->tableInfo->name;
            sIndexTableNamePos.offset   = 0;
            sIndexTableNamePos.size     =
                idlOS::strlen(sOldIndexTable->tableInfo->name);

            /* BUG-45503 Table ���� ���Ŀ� ���� ��, Table Meta Cache�� Column ������ �������� �ʴ� ��찡 �ֽ��ϴ�. */
            IDE_TEST( qcm::copyQcmColumns( QC_QMX_MEM( aStatement ),
                                           sOldIndexTable->tableInfo->columns,
                                           & sTempColumns,
                                           sOldIndexTable->tableInfo->columnCount )
              != IDE_SUCCESS );

            IDE_TEST( createIndexTable( aStatement,
                                        sOldIndexTable->tableInfo->tableOwnerID,
                                        sIndexTableNamePos,
                                        sTempColumns,
                                        sOldIndexTable->tableInfo->columnCount,
                                        sOldIndexTable->tableInfo->TBSID,
                                        sOldIndexTable->tableInfo->segAttr,
                                        sOldIndexTable->tableInfo->segStoAttr,
                                        QDB_TABLE_ATTR_MASK_ALL,
                                        sIndexTableFlag, /* Flag Value */
                                        sIndexTableParallelDegree,
                                        & sNewIndexTable )
                      != IDE_SUCCESS );

            // link new index table
            sNewIndexTable->next = *aNewIndexTables;
            *aNewIndexTables = sNewIndexTable;

            // key index, rid index�� ã�´�.
            IDE_TEST( getIndexTableIndices( sOldIndexTable->tableInfo,
                                            sIndexTableIndex )
                      != IDE_SUCCESS );
                        
            sFlag = smiTable::getIndexInfo(sIndexTableIndex[0]->indexHandle);
            sSegAttr = smiTable::getIndexSegAttr(sIndexTableIndex[0]->indexHandle);
            sSegStoAttr = smiTable::getIndexSegStoAttr(sIndexTableIndex[0]->indexHandle);
                        
            IDE_TEST( createIndexTableIndices(
                          aStatement,
                          sOldIndexTable->tableInfo->tableOwnerID,
                          sNewIndexTable,
                          NULL,
                          sIndexTableIndex[0]->name,
                          sIndexTableIndex[1]->name,
                          sIndexTableIndex[0]->TBSID,
                          sIndexTableIndex[0]->indexTypeId,
                          sFlag,
                          QD_INDEX_DEFAULT_PARALLEL_DEGREE,
                          SMI_INDEX_BUILD_UNCOMMITTED_ROW_DISABLE,
                          sSegAttr,
                          sSegStoAttr,
                          0 ) /* BUG-42124 : direct key index�� partitioned table�� �������� �ʴ´�. */
                      != IDE_SUCCESS );
            
            // tableInfo �����
            sIndexTableInfo = sNewIndexTable->tableInfo;
                    
            IDE_TEST(qcm::makeAndSetQcmTableInfo(QC_SMI_STMT(aStatement),
                                                 sNewIndexTable->tableID,
                                                 sNewIndexTable->tableOID)
                     != IDE_SUCCESS);
            
            IDE_TEST(qcm::getTableInfoByID(aStatement,
                                           sNewIndexTable->tableID,
                                           &(sNewIndexTable->tableInfo),
                                           &(sNewIndexTable->tableSCN),
                                           &(sNewIndexTable->tableHandle))
                     != IDE_SUCCESS);
            
            (void)qcm::destroyQcmTableInfo(sIndexTableInfo);
                        
            // index table id ����
            aNewIndices[i].indexTableID = sNewIndexTable->tableID;
        }
        else
        {
            // Nothing to do.
        }
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qdx::updateIndexTableSpecFromMeta( qcStatement   * aStatement,
                                   qcmTableInfo  * aTableInfo,
                                   qcmIndex      * aNewIndices )
{
/***********************************************************************
 *
 * Description :
 *
 * Implementation :
 *
 ***********************************************************************/

    qcmIndex * sIndex;
    UInt       i;
    
    for ( i = 0; i < aTableInfo->indexCount; i++ )
    {
        sIndex = &(aTableInfo->indices[i]);

        if ( sIndex->indexPartitionType == QCM_NONE_PARTITIONED_INDEX )
        {
            IDE_DASSERT( sIndex->indexTableID > 0 );
            IDE_DASSERT( aNewIndices[i].indexTableID > 0 );
            IDE_DASSERT( sIndex->indexTableID != aNewIndices[i].indexTableID );
                
            IDE_TEST( updateIndexSpecFromMeta( aStatement,
                                               sIndex->indexId,
                                               aNewIndices[i].indexTableID )
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
}

IDE_RC
qdx::deletePartitionInIndexTableList( qcStatement      * aStatement,
                                      qdIndexTableList * aIndexTables,
                                      smOID              aPartitionOID )
{
/***********************************************************************
 *
 * Description :
 *
 * Implementation :
 *
 ***********************************************************************/

    qdIndexTableList    * sIndexTable;
    qcmTableInfo        * sIndexTableInfo;
    qcmColumn           * sOIDColumn;
    smiTableCursor        sCursor;
    smiCursorProperties   sCursorProperty;
    smiFetchColumnList    sFetchColumn;
    UInt                  sMaxRowSize = 0;
    UInt                  sRowSize;
    const void          * sOrgRow;
    const void          * sRow;
    scGRID                sRid;
    smOID                 sPartOID;
    UInt                  sStage = 0;

    SMI_CURSOR_PROP_INIT_FOR_FULL_SCAN( &sCursorProperty, aStatement->mStatistics );

    for ( sIndexTable = aIndexTables;
          sIndexTable != NULL;
          sIndexTable = sIndexTable->next )
    {
        sIndexTableInfo = sIndexTable->tableInfo;
        
        IDE_TEST( qdbCommon::getDiskRowSize( sIndexTableInfo,
                                             & sRowSize )
                  != IDE_SUCCESS );

        if ( sRowSize > sMaxRowSize )
        {
            sMaxRowSize = sRowSize;
        }
        else
        {
            // Nothing to do.
        }
    }
    
    IDE_TEST( aStatement->qmxMem->cralloc( sMaxRowSize,
                                           (void **) & sOrgRow )
              != IDE_SUCCESS );
    
    sCursor.initialize();
        
    for ( sIndexTable = aIndexTables;
          sIndexTable != NULL;
          sIndexTable = sIndexTable->next )
    {
        sIndexTableInfo = sIndexTable->tableInfo;

        //------------------------
        // make fetch column list
        //------------------------
        
        IDE_DASSERT( sIndexTableInfo->columnCount > 2 );
        
        sOIDColumn = &(sIndexTableInfo->columns[sIndexTableInfo->columnCount - 2]);

        IDE_DASSERT( idlOS::strMatch( sOIDColumn->name,
                                      idlOS::strlen( sOIDColumn->name ),
                                      QD_OID_COLUMN_NAME,
                                      QD_OID_COLUMN_NAME_SIZE ) == 0 );
        
        // only oid
        sFetchColumn.column =
            (smiColumn*)&(sOIDColumn->basicInfo->column);
        sFetchColumn.columnSeq = SMI_GET_COLUMN_SEQ( sFetchColumn.column );
        sFetchColumn.copyDiskColumn = 
            qdbCommon::getCopyDiskColumnFunc( sOIDColumn->basicInfo );

        sFetchColumn.next = NULL;

        sCursorProperty.mFetchColumnList = & sFetchColumn;

        //------------------------
        // open cursor
        //------------------------

        IDE_TEST( sCursor.open( QC_SMI_STMT( aStatement ),
                                sIndexTable->tableHandle,
                                NULL,
                                sIndexTable->tableSCN,
                                NULL,
                                smiGetDefaultKeyRange(),
                                smiGetDefaultKeyRange(),
                                smiGetDefaultFilter(),
                                SMI_LOCK_WRITE|SMI_TRAVERSE_FORWARD|SMI_PREVIOUS_DISABLE,
                                SMI_DELETE_CURSOR,
                                & sCursorProperty )
                  != IDE_SUCCESS );
        sStage = 1;

        IDE_TEST( sCursor.beforeFirst() != IDE_SUCCESS );

        //------------------------
        // delete row
        //------------------------
        
        sRow = sOrgRow;
        
        IDE_TEST( sCursor.readRow(&sRow, &sRid, SMI_FIND_NEXT) != IDE_SUCCESS );

        while ( sRow != NULL )
        {
            sPartOID = *(smOID*) ((UChar*)sRow + sOIDColumn->basicInfo->column.offset);

            if ( sPartOID == aPartitionOID )
            {
                IDE_TEST( sCursor.deleteRow() != IDE_SUCCESS );
            }
            else
            {
                // Nothing to do.
            }
            
            IDE_TEST( sCursor.readRow(&sRow, &sRid, SMI_FIND_NEXT) != IDE_SUCCESS );
        }

        sStage = 0;
        IDE_TEST( sCursor.close() != IDE_SUCCESS );
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sStage == 1 )
    {
        (void)sCursor.close();
    }
    
    return IDE_FAILURE;
}

IDE_RC
qdx::initializeInsertIndexTableCursors( qcStatement         * aStatement,
                                        qdIndexTableList    * aIndexTables,
                                        qdIndexTableCursors * aCursorInfo,
                                        qcmIndex            * aIndices,
                                        UInt                  aIndexCount,
                                        UInt                  aColumnCount,
                                        smiCursorProperties * aCursorProperty )
{
/***********************************************************************
 *
 * Description :
 *
 * Implementation :
 *
 ***********************************************************************/

    qdIndexTableList    * sIndexTable;
    qdIndexCursor       * sIndexCursor;
    UInt                  sIndexTableCount = 0;
    UInt                  sOpenCount = 0;
    UInt                  i;
    
    IDE_DASSERT( aIndexTables != NULL );
    IDE_DASSERT( aCursorInfo  != NULL );
    
    for ( sIndexTable = aIndexTables;
          sIndexTable != NULL;
          sIndexTable = sIndexTable->next )
    {
        sIndexTableCount++;
    }

    IDE_DASSERT( sIndexTableCount > 0 );

    // �ʱ�ȭ
    aCursorInfo->indexTables     = aIndexTables;
    aCursorInfo->indexTableCount = sIndexTableCount;
    aCursorInfo->indexCursors    = NULL;
    aCursorInfo->row             = NULL;
    aCursorInfo->newRow          = NULL;

    // non-partitioned index table�� cursor��
    IDE_TEST( STRUCT_ALLOC_WITH_COUNT( aStatement->qmxMem,
                                       qdIndexCursor,
                                       sIndexTableCount,
                                       & aCursorInfo->indexCursors )
              != IDE_SUCCESS );

    // inserted index row
    IDE_TEST( STRUCT_ALLOC_WITH_COUNT( aStatement->qmxMem,
                                       smiValue,
                                       aColumnCount + 2,  // +oid,rid
                                       & aCursorInfo->newRow )
              != IDE_SUCCESS );

    for ( i = 0, sIndexTable = aIndexTables;
          sIndexTable != NULL;
          i++, sIndexTable = sIndexTable->next )
    {
        sIndexCursor = & aCursorInfo->indexCursors[i];
        
        // non-partitioned index�� �ش��ϴ� index�� ã�´�.
        IDE_TEST( findIndexTableIDInIndices( aIndices,
                                             aIndexCount,
                                             sIndexTable->tableID,
                                             & sIndexCursor->index )
                  != IDE_SUCCESS );

        sIndexCursor->cursor.initialize();

        // cursor open
        IDE_TEST( sIndexCursor->cursor.open( QC_SMI_STMT( aStatement ),
                                             sIndexTable->tableHandle,
                                             NULL,
                                             sIndexTable->tableSCN,
                                             NULL,
                                             smiGetDefaultKeyRange(),
                                             smiGetDefaultKeyRange(),
                                             smiGetDefaultFilter(),
                                             SMI_LOCK_WRITE|SMI_TRAVERSE_FORWARD|
                                             SMI_PREVIOUS_DISABLE,
                                             SMI_INSERT_CURSOR,
                                             aCursorProperty )
                  != IDE_SUCCESS );
        
        sIndexCursor->cursorOpened = ID_TRUE;
        sOpenCount++;
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    for ( i = 0; i < sOpenCount; i++ )
    {
        sIndexCursor = & aCursorInfo->indexCursors[i];
        
        sIndexCursor->cursorOpened = ID_FALSE;
        
        (void) sIndexCursor->cursor.close();
    }
    
    return IDE_FAILURE;
}

IDE_RC
qdx::insertIndexTableCursors( qdIndexTableCursors * aCursorInfo,
                              smiValue            * aNewRow,
                              smOID                 aNewPartOID,
                              scGRID                aNewGRID )
{
/***********************************************************************
 *
 * Description : PROJ-1624 non-partitioned index
 *     update $GIT set oid = aNewPartOID, rid = aNewGRID where rid = aOldGRID
 *
 * Implementation :
 *
 ***********************************************************************/

    qdIndexCursor  * sIndexCursor;
    smOID            sPartOID = aNewPartOID;
    scGRID           sRowGRID = aNewGRID;
    scGRID           sGRID;
    void           * sRow;
    UInt             i;

    for ( i = 0; i < aCursorInfo->indexTableCount; i++ )
    {
        sIndexCursor = & aCursorInfo->indexCursors[i];
        
        IDE_DASSERT( sIndexCursor->cursorOpened == ID_TRUE );
        
        // make smiValues
        IDE_TEST( makeSmiValue4IndexTableWithSmiValue( aNewRow,
                                                       aCursorInfo->newRow,
                                                       sIndexCursor->index,
                                                       & sPartOID,
                                                       & sRowGRID )
                  != IDE_SUCCESS );
                
        // insert index table
        IDE_TEST( sIndexCursor->cursor.insertRow( aCursorInfo->newRow,
                                                  & sRow,
                                                  & sGRID )
                  != IDE_SUCCESS );
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC
qdx::closeInsertIndexTableCursors( qdIndexTableCursors * aCursorInfo )
{
/***********************************************************************
 *
 * Description :
 *
 * Implementation :
 *
 ***********************************************************************/

    qdIndexCursor  * sIndexCursor;
    UInt             i;
    
    for ( i = 0; i < aCursorInfo->indexTableCount; i++ )
    {
        sIndexCursor = & aCursorInfo->indexCursors[i];
        
        if ( sIndexCursor->cursorOpened == ID_TRUE )
        {
            sIndexCursor->cursorOpened = ID_FALSE;
            
            IDE_TEST( sIndexCursor->cursor.close() != IDE_SUCCESS );
        }
        else
        {
            // Nothing to do.
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

void
qdx::finalizeInsertIndexTableCursors( qdIndexTableCursors * aCursorInfo )
{
/***********************************************************************
 *
 * Description :
 *
 * Implementation :
 *
 ***********************************************************************/

    qdIndexCursor  * sIndexCursor;
    UInt             i;
    
    for ( i = 0; i < aCursorInfo->indexTableCount; i++ )
    {
        sIndexCursor = & aCursorInfo->indexCursors[i];
        
        if ( sIndexCursor->cursorOpened == ID_TRUE )
        {
            sIndexCursor->cursorOpened = ID_FALSE;
            
            (void) sIndexCursor->cursor.close();
        }
        else
        {
            // Nothing to do.
        }
    }
}

IDE_RC
qdx::initializeUpdateIndexTableCursors( qcStatement         * aStatement,
                                        qdIndexTableList    * aIndexTables,
                                        qdIndexTableCursors * aCursorInfo )
{
/***********************************************************************
 *
 * Description :
 *
 * Implementation :
 *
 ***********************************************************************/
    
    qdIndexTableList    * sIndexTable;
    qdIndexCursor       * sIndexCursor;
    UInt                  sIndexTableCount = 0;
    qcmTableInfo        * sIndexTableInfo;
    qcmIndex            * sIndexTableIndex[2];
    qcmColumn           * sOIDColumn;
    qcmColumn           * sRIDColumn;
    UInt                  sMaxRowSize = 0;
    UInt                  sRowSize;
    UInt                  i;

    IDE_DASSERT( aIndexTables != NULL );
    IDE_DASSERT( aCursorInfo  != NULL );
    
    for ( sIndexTable = aIndexTables;
          sIndexTable != NULL;
          sIndexTable = sIndexTable->next )
    {
        sIndexTableCount++;
    }
    
    IDE_DASSERT( sIndexTableCount > 0 );

    // �ʱ�ȭ
    aCursorInfo->indexTables     = aIndexTables;
    aCursorInfo->indexTableCount = sIndexTableCount;
    aCursorInfo->indexCursors    = NULL;
    aCursorInfo->row             = NULL;
    aCursorInfo->newRow          = NULL;

    // non-partitioned index table�� cursor��
    IDE_TEST( STRUCT_ALLOC_WITH_COUNT( aStatement->qmxMem,
                                       qdIndexCursor,
                                       sIndexTableCount,
                                       & aCursorInfo->indexCursors )
              != IDE_SUCCESS );
        
    // inserted index row
    IDE_TEST( STRUCT_ALLOC_WITH_COUNT( aStatement->qmxMem,
                                       smiValue,
                                       2,  // oid,rid
                                       & aCursorInfo->newRow )
              != IDE_SUCCESS );
    
    for ( i = 0, sIndexTable = aIndexTables;
          sIndexTable != NULL;
          i++, sIndexTable = sIndexTable->next )
    {
        sIndexCursor    = & aCursorInfo->indexCursors[i];
        sIndexTableInfo = sIndexTable->tableInfo;

        IDE_DASSERT( sIndexTableInfo->columnCount > 2 );
            
        sOIDColumn = &(sIndexTableInfo->columns[sIndexTableInfo->columnCount - 2]);
        sRIDColumn = &(sIndexTableInfo->columns[sIndexTableInfo->columnCount - 1]);
        
        // initialize
        sIndexCursor->cursor.initialize();
        
        // fetch column, only rid
        sIndexCursor->fetchColumn.column =
            (smiColumn*)&(sRIDColumn->basicInfo->column);
        sIndexCursor->fetchColumn.columnSeq = 
            SMI_GET_COLUMN_SEQ( sIndexCursor->fetchColumn.column );
        sIndexCursor->fetchColumn.copyDiskColumn = 
            qdbCommon::getCopyDiskColumnFunc( sRIDColumn->basicInfo );


        sIndexCursor->fetchColumn.next = NULL;

        // rid index handle
        IDE_TEST( getIndexTableIndices( sIndexTableInfo,
                                        sIndexTableIndex )
                  != IDE_SUCCESS );

        sIndexCursor->ridIndexHandle = sIndexTableIndex[1]->indexHandle;
        sIndexCursor->ridIndexType = sIndexTableIndex[1]->indexTypeId;
        
        // update column list, oid and rid
        sIndexCursor->updateColumnList[0].column =
            (smiColumn*)&(sOIDColumn->basicInfo->column);
        sIndexCursor->updateColumnList[0].next   =
            &(sIndexCursor->updateColumnList[1]);
        sIndexCursor->updateColumnList[1].column =
            (smiColumn*)&(sRIDColumn->basicInfo->column);
        sIndexCursor->updateColumnList[1].next   =
            NULL;

        // range column
        sIndexCursor->ridColumn = sRIDColumn->basicInfo;

        // row size
        IDE_TEST( qdbCommon::getDiskRowSize( sIndexTableInfo,
                                             & sRowSize )
                  != IDE_SUCCESS );

        if ( sRowSize > sMaxRowSize )
        {
            sMaxRowSize = sRowSize;
        }
        else
        {
            // Nothing to do.
        }

        sIndexCursor->cursorOpened = ID_FALSE;
    }

    IDE_TEST( STRUCT_ALLOC_WITH_SIZE( aStatement->qmxMem,
                                      UChar,
                                      sMaxRowSize,
                                      & aCursorInfo->row )
              != IDE_SUCCESS );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC
qdx::updateIndexTableCursors( qcStatement         * aStatement,
                              qdIndexTableCursors * aCursorInfo,
                              smOID                 aNewPartOID,
                              scGRID                aOldGRID,
                              scGRID                aNewGRID )
{
/***********************************************************************
 *
 * Description : PROJ-1624 non-partitioned index
 *     update $GIT set oid = aNewPartOID, rid = aNewGRID where rid = aOldGRID
 *
 * Implementation :
 *
 ***********************************************************************/

    qdIndexTableList    * sIndexTable;
    qdIndexCursor       * sIndexCursor;
    smiCursorProperties   sCursorProperty;
    qtcMetaRangeColumn    sRangeColumn;
    smiRange              sRange;
    const void          * sRow;
    scGRID                sRowGRID;
    UInt                  i;
    
    IDE_DASSERT( aCursorInfo  != NULL );
    
    for ( i = 0, sIndexTable = aCursorInfo->indexTables;
          sIndexTable != NULL;
          i++, sIndexTable = sIndexTable->next )
    {
        sIndexCursor = & aCursorInfo->indexCursors[i];
        
        // make smiRange
        qcm::makeMetaRangeSingleColumn( & sRangeColumn,
                                        sIndexCursor->ridColumn,
                                        & aOldGRID,
                                        & sRange );

        if ( sIndexCursor->cursorOpened == ID_FALSE )
        {
            SMI_CURSOR_PROP_INIT_FOR_INDEX_SCAN( & sCursorProperty,
                                                 aStatement->mStatistics,
                                                 sIndexCursor->ridIndexType );

            sCursorProperty.mFetchColumnList = & (sIndexCursor->fetchColumn);
            
            // open
            IDE_TEST( sIndexCursor->cursor.open( QC_SMI_STMT( aStatement ),
                                                 sIndexTable->tableHandle,
                                                 sIndexCursor->ridIndexHandle,
                                                 sIndexTable->tableSCN,
                                                 sIndexCursor->updateColumnList,
                                                 & sRange,
                                                 smiGetDefaultKeyRange(),
                                                 smiGetDefaultFilter(),
                                                 SMI_LOCK_WRITE|SMI_TRAVERSE_FORWARD|
                                                 SMI_PREVIOUS_DISABLE,
                                                 SMI_UPDATE_CURSOR,
                                                 & sCursorProperty )
                      != IDE_SUCCESS );

            sIndexCursor->cursorOpened = ID_TRUE;
        }
        else
        {
            // restart
            IDE_TEST( sIndexCursor->cursor.restart( & sRange,
                                                    smiGetDefaultKeyRange(),
                                                    smiGetDefaultFilter() )
                      != IDE_SUCCESS );
        }

        // readRow
        IDE_TEST( sIndexCursor->cursor.beforeFirst()
                  != IDE_SUCCESS );

        sRow = aCursorInfo->row;
                    
        IDE_TEST( sIndexCursor->cursor.readRow( (const void **) & sRow,
                                                & sRowGRID,
                                                SMI_FIND_NEXT )
                  != IDE_SUCCESS );
                    
        // �ݵ�� �����ؾ��Ѵ�.
        IDE_TEST_RAISE( sRow == NULL, ERR_RID_NOT_FOUND );
                        
        // make smiValues
        aCursorInfo->newRow[0].value = (void*) &aNewPartOID;
        aCursorInfo->newRow[0].length = ID_SIZEOF(mtdBigintType);
        aCursorInfo->newRow[1].value = (void*) &aNewGRID;
        aCursorInfo->newRow[1].length = ID_SIZEOF(mtdBigintType);
        
        // update
        IDE_TEST( sIndexCursor->cursor.updateRow( aCursorInfo->newRow )
                  != IDE_SUCCESS );
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_RID_NOT_FOUND )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qdx::updateIndexTableCursors",
                                  "rid is not found" ));
    }
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC
qdx::closeUpdateIndexTableCursors( qdIndexTableCursors * aCursorInfo )
{
/***********************************************************************
 *
 * Description :
 *
 * Implementation :
 *
 ***********************************************************************/

    qdIndexCursor  * sIndexCursor;
    UInt             i;
    
    for ( i = 0; i < aCursorInfo->indexTableCount; i++ )
    {
        sIndexCursor = & aCursorInfo->indexCursors[i];
        
        if ( sIndexCursor->cursorOpened == ID_TRUE )
        {
            sIndexCursor->cursorOpened = ID_FALSE;
            
            IDE_TEST( sIndexCursor->cursor.close() != IDE_SUCCESS );
        }
        else
        {
            // Nothing to do.
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

void
qdx::finalizeUpdateIndexTableCursors( qdIndexTableCursors * aCursorInfo )
{
/***********************************************************************
 *
 * Description :
 *
 * Implementation :
 *
 ***********************************************************************/

    qdIndexCursor  * sIndexCursor;
    UInt             i;
    
    for ( i = 0; i < aCursorInfo->indexTableCount; i++ )
    {
        sIndexCursor = & aCursorInfo->indexCursors[i];
        
        if ( sIndexCursor->cursorOpened == ID_TRUE )
        {
            sIndexCursor->cursorOpened = ID_FALSE;
            
            (void) sIndexCursor->cursor.close();
        }
        else
        {
            // Nothing to do.
        }
    }
}

/*********************************************************************
 * FUNCTION DESCRIPTION : qdx::validateAlterDirectKey                *
 * ------------------------------------------------------------------*
 * PROJ-2433 Direct Key Index
 * ALTER INDEX ~ DIRECTKEY [MAXSIZE n] [OFF] ������ validation �Լ�
 *
 * aStatement  - [IN]  ��������
 *********************************************************************/
IDE_RC qdx::validateAlterDirectKey( qcStatement * aStatement )
{
    qdIndexParseTree      * sParseTree     = NULL;
    UInt                    sTableID       = 0;
    UInt                    sIndexID       = 0;
    qcmIndex              * sIndex         = NULL;
    SInt                    sCountDiskType = 0;
    SInt                    sCountMemType  = 0;
    SInt                    sCountVolType  = 0;
    UInt                    sTableType     = 0;
    qcmPartitionInfoList  * sPartInfoList  = NULL;

    /*
     * index rebuild�� ���� validate Ȯ���Ѵ�.
     * ����,
     * ALTER INDEX�� ���� �������� validation
     * Note : Table �� lock (IS) ����
     */
    IDE_TEST( validateAlterRebuild( aStatement )
              != IDE_SUCCESS );

    sParseTree = (qdIndexParseTree *)aStatement->myPlan->parseTree;

    IDE_TEST( qcm::checkIndexByUser( aStatement,
                                     sParseTree->userNameOfIndex,
                                     sParseTree->indexName,
                                     &(sParseTree->userIDOfIndex),
                                     &sTableID,
                                     &sIndexID) != IDE_SUCCESS);

    IDE_TEST( qcm::getTableInfoByID( aStatement,
                                     sTableID,
                                     &(sParseTree->tableInfo),
                                     &(sParseTree->tableSCN),
                                     &(sParseTree->tableHandle) )
              != IDE_SUCCESS );

    /* PROJ-2464 hybrid partitioned table ���� */
    sTableType = sParseTree->tableInfo->tableFlag & SMI_TABLE_TYPE_MASK;

    IDE_TEST( qcmCache::getIndex( sParseTree->tableInfo,
                                  sParseTree->indexName,
                                  & sIndex )
              != IDE_SUCCESS );

    if ( sParseTree->tableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
    {
        sPartInfoList = sParseTree->partIndex->partInfoList;
    }
    else
    {
        /* Nothing to do */
    }

    /* PROJ-2464 hybrid partitioned table ����
     *  - ����� Partition ������ �����ϰ� �����Ѵ�.
     */
    qdbCommon::getTableTypeCountInPartInfoList( & sTableType,
                                                sPartInfoList,
                                                & sCountDiskType,
                                                & sCountMemType,
                                                & sCountVolType );

    if ( ( sParseTree->flag & SMI_INDEX_DIRECTKEY_MASK ) ==
         SMI_INDEX_DIRECTKEY_TRUE )
    {
        /* PROJ-2464 hybrid partitioned table ����
         *  - �Ʒ��� ���� �߻����� �ʴ´�.
         */
        IDE_TEST_RAISE( sIndex->keyColumns == NULL, ERR_NO_EXIST_KEYCOLUMN );

        /* PROJ-2464 hybrid partitioned table ����
         *  - Property ���� �����ؾ� �ϹǷ� aIsUserTable�� ID_FALSE�� ������ �˻縦 ȸ���ϰ� �Ѵ�.
         *  - ���ó��� : PROJ-2433 Direct Key Index
         */
        IDE_TEST( qdbCommon::validateAndSetDirectKey( sIndex->keyColumns,
                                                      ID_FALSE,
                                                      sCountDiskType,
                                                      sCountMemType,
                                                      sCountVolType,
                                                      &( sParseTree->mDirectKeyMaxSize ),
                                                      NULL )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NO_EXIST_KEYCOLUMN )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qdx::validateAlterDirectKey",
                                  "No exist index key column" ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : qdx::executeAlterDirectKey                 *
 * ------------------------------------------------------------------*
 * PROJ-2433 Direct Key Index
 * ALTER INDEX ~ DIRECTKEY [MAXSIZE n] [OFF] ������ execution �Լ�
 *
 * - index rebuild�� ����ȴ�.
 *
 * aStatement  - [IN]  ��������
 *********************************************************************/
IDE_RC qdx::executeAlterDirectKey( qcStatement * aStatement )
{
    qdIndexParseTree    * sParseTree;
    qcmIndex            * sIndex;
    UInt                  sIndexOption;

    sParseTree = (qdIndexParseTree *)aStatement->myPlan->parseTree;

    // TASK-2176
    // Table�� ���� Lock�� ȹ���Ѵ�.
    IDE_TEST( qcm::validateAndLockTable( aStatement,
                                         sParseTree->tableHandle,
                                         sParseTree->tableSCN,
                                         SMI_TABLE_LOCK_X )
              != IDE_SUCCESS );

    IDE_TEST( qcmCache::getIndex( sParseTree->tableInfo,
                                  sParseTree->indexName,
                                  &sIndex )
              != IDE_SUCCESS );

    sIndexOption = smiTable::getIndexInfo( sIndex->indexHandle );
    if ( ( sParseTree->flag & SMI_INDEX_DIRECTKEY_MASK )
         == SMI_INDEX_DIRECTKEY_TRUE )
    {
        sIndexOption &= ~(SMI_INDEX_DIRECTKEY_MASK);
        sIndexOption |=  SMI_INDEX_DIRECTKEY_TRUE;
    }
    else
    {
        sIndexOption &= ~(SMI_INDEX_DIRECTKEY_MASK);
        sIndexOption |= SMI_INDEX_DIRECTKEY_FALSE;
    }

    /* PROJ-2433 Direct Key Index
     * ���⼭�� ���� �������ְ�, ������ �α׸� �������ʴ´�.
     * �Ʒ� rebuild�� ���� executeAlterRebuild() �Լ����� �α׸� �����. */
    smiTable::setIndexInfo( sIndex->indexHandle,
                            sIndexOption );
    smiTable::setIndexMaxKeySize( sIndex->indexHandle,
                                  sParseTree->mDirectKeyMaxSize );

    /* index rebuild ���� */
    IDE_TEST( executeAlterRebuild( aStatement )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdx::validateIndexRestriction( qcStatement * aStatement,
                                      idBool        aCheckKeySizeLimit,
                                      UInt          aIndexType )
{
/***********************************************************************
 *
 * Description :
 *      PROJ-2464 hybrid partitioned table ����
 *
 *      Index ���� ��, ������ Index�� ���������� �˻��ϴ� �Լ��̴�.
 *      ��� Partition Type �Ǵ� Table Type�� ���� ���������� �������� ���ϸ� ������ ó���Ѵ�.
 *      ���ó��� : BUG-19621 : PERSISTENT option�� memory table���� ���� ����. - BUG-31517
 *                 BUG-17848 : LOGGING option�� disk table���� ���� ����.
 *                 INI/MAXTRANS Validation
 *                 PROJ-2334 : PMT PMT��� ��Ƽ�ǵ� ���̺� ����Ƽ�ǵ� �ε��� ������ �� ����
 *                 TASK-3171 : B-tree for spatial
 *                 BUG-17449 : �� ���̺� Ÿ�Կ� �´� �ε��� ���� ���� �Ǵ�
 *                 BUG-31517 : PRIMARY KEY/UNIQUE constraint�� �߰��� ���, key size limit �˻縦 �����ؾ� ��
 *
 * Implementation :
 *      1. ��ü Type�� �˻��Ѵ�.
 *
 *      2. INDEX PERSISTENT �ɼ��� �˻��Ѵ�
 *         2.1. Memory ��ü �� �ٸ� ��ü���� INDEX PERSISTENT �ɼ��� �������� �ʴ´�.
 *
 *      3. Memory ��ü�� ���� �ÿ��� INDEX LOGGING �ɼ��� �������� �ʴ´�.
 *
 *      4. Memory ��ü�� ���� �ÿ��� Global Index�� �������� �ʴ´�.
 *
 *      5. Index���� ������ SegAttr�� �˻��Ѵ�.
 *
 *      6. Index ������ ���� ������� �˻�
 *
 *      7. DirectKeyMaxSize �ɼ��� �˻��Ѵ�.
 *
 ***********************************************************************/

    qdIndexParseTree     * sParseTree     = NULL;
    qcmPartitionInfoList * sPartInfoList  = NULL;
    idBool                 sIsPartitioned = ID_FALSE;
    idBool                 sIsUserTable   = ID_FALSE;
    idBool                 sIsPers        = ID_FALSE;
    SInt                   sCountDiskType = 0;
    SInt                   sCountMemType  = 0;
    SInt                   sCountVolType  = 0;
    UInt                   sTableType     = 0;

    sParseTree   = (qdIndexParseTree *)aStatement->myPlan->parseTree;
    sTableType   = sParseTree->tableInfo->tableFlag & SMI_TABLE_TYPE_MASK;
    sIsUserTable = smiTableSpace::isDataTableSpaceType( sParseTree->tableInfo->TBSType );

    /* 1. ��ü Type�� �˻��Ѵ�. */
    if ( sParseTree->tableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
    {
        sPartInfoList  = sParseTree->partIndex->partInfoList;
        sIsPartitioned = ID_TRUE; 
    }
    else
    {
        /* Nothing to do */
    }

    qdbCommon::getTableTypeCountInPartInfoList( & sTableType,
                                                sPartInfoList,
                                                & sCountDiskType,
                                                & sCountMemType,
                                                & sCountVolType );

    /* 2. INDEX PERSISTENT �ɼ��� �˻��Ѵ�. */
    if ( ( sParseTree->flag & SMI_INDEX_PERSISTENT_MASK ) == SMI_INDEX_PERSISTENT_ENABLE )
    {
        sIsPers = ID_TRUE;
    }
    else
    {
        sIsPers = ID_FALSE;
    }

    /* 2.1. Memory ��ü �� �ٸ� ��ü���� INDEX PERSISTENT �ɼ��� �������� �ʴ´�. */
    IDE_TEST( qdbCommon::validateAndSetPersistent( sCountDiskType,
                                                   sCountVolType,
                                                   & sIsPers,
                                                   &( sParseTree->flag ) )
              != IDE_SUCCESS );

    /* 3. Memory ��ü�� ���� �ÿ��� INDEX LOGGING �ɼ��� �������� �ʴ´�. */
    IDE_TEST_RAISE( ( sParseTree->buildFlag != SMI_INDEX_BUILD_DEFAULT ) &&
                    ( ( sCountMemType + sCountVolType ) > 0 ),
                    ERR_IRREGULAR_LOGGING_OPTION );

    /* 4. Memory ��ü�� ���� �ÿ��� Global Index�� �������� �ʴ´�. */
    IDE_TEST_RAISE( ( sParseTree->partIndex->partIndexType == QCM_NONE_PARTITIONED_INDEX ) &&
                    ( sParseTree->tableInfo->partitionMethod != QCM_PARTITION_METHOD_NONE ) &&
                    ( ( sCountMemType + sCountVolType ) > 0 ),
                    ERR_CANNOT_CREATE_NONE_PART_INDEX_ON_PART_TABLE );

    /* 5. Index���� ������ SegAttr�� �˻��Ѵ�    */
    IDE_TEST( qdbCommon::validateAndSetSegAttr( sTableType,
                                                NULL,
                                                & ( sParseTree->segAttr ),
                                                ID_FALSE )
              != IDE_SUCCESS );

    /* 6. Index ������ ���� ������� �˻� */
    if ( aCheckKeySizeLimit == ID_TRUE )
    {
        IDE_TEST( qdbCommon::validateIndexKeySize( aStatement,
                                                   sTableType,
                                                   sParseTree->keyColumns,
                                                   sParseTree->keyColCount,
                                                   aIndexType,
                                                   sParseTree->partIndex->partInfoList,
                                                   NULL,
                                                   sIsPartitioned,
                                                   ID_TRUE )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    /* 7. DirectKeyMaxSize �ɼ��� �˻��Ѵ�.
     *    -  Memory User Data Table���� ����� �� �ִ�.
     */
    IDE_TEST( qdbCommon::validateAndSetDirectKey( sParseTree->keyColumns[0].basicInfo,
                                                  sIsUserTable,
                                                  sCountDiskType,
                                                  sCountMemType,
                                                  sCountVolType,
                                                  &( sParseTree->mDirectKeyMaxSize ),
                                                  &( sParseTree->flag ) )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_IRREGULAR_LOGGING_OPTION )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDX_NON_DISK_INDEX_LOGGING_OPTION ) );
    }
    IDE_EXCEPTION( ERR_CANNOT_CREATE_NONE_PART_INDEX_ON_PART_TABLE )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDX_CANNOT_CREATE_NONE_PART_INDEX_ON_MEM_PART_TABLE ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdx::validateAlterReorganization(qcStatement * aStatement)
{
/***********************************************************************
 *
 * Description :
 *    ALTER INDEX ... REORGANIZATION�� validation ����
 *
 * Implementation :
 *    1. �����ϴ� �ε������� üũ, table ID, index ID ã��
 *    2. table ID �� qcmTableInfo ã��
 *    3. AlterIndex ������ �ִ��� üũ
 *
 ***********************************************************************/
    qdIndexParseTree     * sParseTree         = NULL;
    UInt                   sIndexID           = 0;
    UInt                   sTableID           = 0;
    UInt                   sTableType         = 0;
    qcmIndex             * sIndex             = NULL;
    SInt                   sCountMemType      = 0;
    SInt                   sCountVolType      = 0;

    sParseTree = (qdIndexParseTree *)aStatement->myPlan->parseTree;

    // if index does not exists, raise error
    IDE_TEST(qcm::checkIndexByUser(
                 aStatement,
                 sParseTree->userNameOfIndex,
                 sParseTree->indexName,
                 &(sParseTree->userIDOfIndex),
                 &sTableID,
                 &sIndexID)
             != IDE_SUCCESS);

    IDE_TEST(qcm::getTableInfoByID(aStatement,
                                   sTableID,
                                   &(sParseTree->tableInfo),
                                   &(sParseTree->tableSCN),
                                   &(sParseTree->tableHandle))
             != IDE_SUCCESS);

    // ��Ƽ�ǵ� ���̺� LOCK(IS)
    IDE_TEST( qcm::lockTableForDDLValidation(aStatement,
                                             sParseTree->tableHandle,
                                             sParseTree->tableSCN)
              != IDE_SUCCESS );

    // check grant
    IDE_TEST( qdpRole::checkDDLAlterIndexPriv( aStatement,
                                               sParseTree->tableInfo,
                                               sParseTree->userIDOfIndex )
              != IDE_SUCCESS );

    IDE_TEST(qcmCache::getIndex(sParseTree->tableInfo,
                                sParseTree->indexName,
                                &sIndex)
             != IDE_SUCCESS);

    if ( sParseTree->tableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
    {
        IDE_TEST( qdbCommon::checkAndSetAllPartitionInfo( aStatement,
                                                          sParseTree->tableInfo->tableID,
                                                          &sParseTree->partIndex->partInfoList )
                  != IDE_SUCCESS );

        sTableType = sParseTree->tableInfo->tableFlag & SMI_TABLE_TYPE_MASK;

        /*  Partition ������ �˻��Ѵ�. */
        qdbCommon::getTableTypeCountInPartInfoList( & sTableType,
                                                    sParseTree->partIndex->partInfoList,
                                                    NULL,
                                                    & sCountMemType,
                                                    & sCountVolType );

        /* Memory ��ü���־���Ѵ� */
        IDE_TEST_RAISE( ( sCountMemType + sCountVolType ) == 0,
                        ERR_INCORRENT_REORG_OPTION );
    }
    else
    {
        IDE_TEST_RAISE( smiIsDiskTable(sParseTree->tableHandle) == ID_TRUE, ERR_INCORRENT_REORG_OPTION )
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_INCORRENT_REORG_OPTION)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDX_NON_MEMORY_BTREE_INDEX_REORGANIZATION_OPTION));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdx::executeAlterReorganization(qcStatement * aStatement)
{
    qdIndexParseTree     * sParseTree    = NULL;
    qcmIndex             * sIndex        = NULL;
    qcmIndex             * sTempIndex    = NULL;
    qcmPartitionInfoList * sPartInfoList = NULL;
    qcmTableInfo         * sPartInfo     = NULL;
    UInt                   sCount        = 0;

    sParseTree = (qdIndexParseTree *)aStatement->myPlan->parseTree;

    /* Table�� ���� Lock�� ȹ���Ѵ�. */
    /* table/index header�� �ǵ帮�� �۾��� �ƴϹǷ� X lock �� ���� �ʾƵ� �ȴ�. */
    IDE_TEST( qcm::validateAndLockTable( aStatement,
                                         sParseTree->tableHandle,
                                         sParseTree->tableSCN,
                                         SMI_TABLE_LOCK_IX )
              != IDE_SUCCESS );

    IDE_TEST( qcmCache::getIndex( sParseTree->tableInfo,
                                  sParseTree->indexName,
                                  &sIndex)
              != IDE_SUCCESS );

    if ( sParseTree->partIndex->partInfoList == NULL )
    {
        IDE_TEST_RAISE( sIndex->indexTypeId == SMI_ADDITIONAL_RTREE_INDEXTYPE_ID, ERR_INCORRENT_REORG_OPTION );
        IDE_TEST( smiTable::indexReorganization( sIndex->indexHandle ) != IDE_SUCCESS );
    }
    else
    {
        /* Partition Table �� IX Lock�� ��´� */
        /* table/index header�� �ǵ帮�� �۾��� �ƴϹǷ� X lock �� ���� �ʾƵ� �ȴ�. */
        for ( sPartInfoList = sParseTree->partIndex->partInfoList;
              sPartInfoList != NULL;
              sPartInfoList = sPartInfoList->next )
        {
            if ( smiIsDiskTable( sPartInfoList->partHandle ) != ID_TRUE )
            {
                IDE_TEST( smiValidateAndLockObjects( (QC_SMI_STMT(aStatement))->getTrans(),
                                                     sPartInfoList->partHandle,
                                                     sPartInfoList->partSCN,
                                                     SMI_TBSLV_DDL_DML,
                                                     SMI_TABLE_LOCK_IX,
                                                     smiGetDDLLockTimeOut((QC_SMI_STMT(aStatement))->getTrans()),
                                                     ID_FALSE ) // BUG-28752 isExplicitLock
                          != IDE_SUCCESS );
            }
            else
            {
                /* Nothing to do */
            }
        }

        for ( sPartInfoList = sParseTree->partIndex->partInfoList;
              sPartInfoList != NULL;
              sPartInfoList = sPartInfoList->next )
        {
            if ( smiIsDiskTable( sPartInfoList->partHandle ) != ID_TRUE )
            {
                sPartInfo = sPartInfoList->partitionInfo;

                for ( sCount = 0; sCount < sPartInfo->indexCount; sCount++ )
                {
                    sTempIndex = &sPartInfo->indices[sCount];

                    if ( sTempIndex->indexId == sIndex->indexId )
                    {
                        IDE_TEST_RAISE( sTempIndex->indexTypeId == SMI_ADDITIONAL_RTREE_INDEXTYPE_ID, ERR_INCORRENT_REORG_OPTION );
                        IDE_TEST( smiTable::indexReorganization( sTempIndex->indexHandle ) != IDE_SUCCESS );
                        break;
                    }
                    else
                    {
                        /* Nothing to do */
                    }
                }
            }
            else
            {
                /* Nothing to do */
            }
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INCORRENT_REORG_OPTION )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDX_NON_MEMORY_BTREE_INDEX_REORGANIZATION_OPTION ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

