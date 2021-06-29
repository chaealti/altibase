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
 * $Id$
 **********************************************************************/

#include <idl.h>
#include <ide.h>
#include <mtuProperty.h>
#include <mtcDef.h>
#include <qdbCommon.h>
#include <qdbJoin.h>
#include <qdbDisjoin.h>
#include <qcm.h>
#include <qcmPartition.h>
#include <qcuProperty.h>
#include <qcg.h>
#include <smiDef.h>
#include <smiTableSpace.h>
#include <qdpDrop.h>
#include <qdnTrigger.h>
#include <qcmTableSpace.h>
#include <qdd.h>
#include <qcmProc.h>
#include <qcmPkg.h>
#include <qcmView.h>
#include <qcmAudit.h>
#include <qdpRole.h>
#include <qdbComment.h>
#include <qdtCommon.h>
#include <sdi.h>

IDE_RC qdbJoin::validateJoinTable( qcStatement * aStatement )
{
/***********************************************************************
 *
 * Description :
 *    JOIN TABLE ... PARTITION BY RANGE/LIST ( ... )
 *    ( TABLE ... TO PARTITION ...,
 *      TABLE ... TO PARTITION ...,
 *      ... )
 *    ENABLE/DISABLE ROW MOVEMENT
 *    READ ONLY / READ APPEND / READ WRITE
 *    TABLESPACE ...
 *    PCTUSED int PCTFREE int INITRANS int MAXTRANS int
 *    STORAGE ( INITEXTENTS int NEXTEXTENTS int MINEXTENTS int MAXEXTENTS INT/UNLIMITED )
 *    LOGGING/NOLOGGING
 *    PARALLEL int
 *    LOB ( ... ) STORE AS ( TABLESPACE ... LOGGING/NOLOGGING BUFFER/NOBUFFER )
 *
 * Implementation :
 *    1. ���̺� dollar name üũ
 *    2. ��Ƽ�� �̸�(��ȯ�� ��) �ߺ� �˻�
 *       2-1. ���̺� �̸�(��ȯ�Ǳ� ��, JOIN TABLE�� ���� �͵�) �ߺ� �˻�
 *    3. ��Ƽ�ǵ� ���̺� �̸� �ߺ� üũ(���� ����)
 *    4. ���̺� ���� ���� üũ
 *    5. ��� ���̺� ��ü �̸����� ���� ���� �˻�
 *    6. �ɼ� �˻�
 *       6-1. TBS
 *       6-2. Segment Storage �Ӽ�
 *    7. ��� ���̺� ��ü validation
 *    8. ��� ���̺� ��ü schema ��ġ ���� �˻�
 *    9. Į�� type�� LOB�� �ִ� ��� �߰��� lob column�� ���� validation�� �Ѵ�.
 *    10. ��Ƽ�� �ɼ� üũ
 *       10-1. ��Ƽ�� Ű �÷� validation
 *       10-2. ���� ��Ƽ���� ���, ��Ƽ�� Ű ���� ���� üũ
 *       10-3. �ߺ��� ��Ƽ�� ���� �� üũ
 *       10-4. Hybrid Partitioned Table�̸� ����
 *
 ***********************************************************************/

    qcuSqlSourceInfo         sqlInfo;
    qdTableParseTree       * sParseTree = NULL;

    qcmColumn              * sColumn = NULL;
    qdPartitionedTable     * sPartTable = NULL;
    qdPartitionAttribute   * sPartAttr = NULL;
    qdPartitionAttribute   * sTempPartAttr = NULL;

    qcmPartitionInfoList   * sPartInfoList = NULL;
    qcmPartitionInfoList   * sLastPartInfoList = NULL;

    qsOID                    sProcID = QS_EMPTY_OID;
    idBool                   sExist = ID_FALSE;
    UInt                     sUserID = 0;
    UInt                     sColumnCount = 0;
    
    SInt                     sCountDiskType = 0;
    SInt                     sCountMemType  = 0;
    SInt                     sCountVolType  = 0;
    SInt                     sTotalCount    = 0;
    UInt                     sTableType     = 0;
    
    sParseTree = (qdTableParseTree *)aStatement->myPlan->parseTree;
    sPartTable = sParseTree->partTable;

    /* 1. ���̺� dollar name üũ */
    /* BUG-30059 */
    if ( qdbCommon::containDollarInName( &(sParseTree->tableName) ) == ID_TRUE )
    {
        sqlInfo.setSourceInfo( aStatement,
                               &(sParseTree->tableName) );

        IDE_RAISE( CANT_USE_RESERVED_WORD );
    }
    else
    {
        /* Nothing To Do */
    }

    /* 2. ��Ƽ��/���̺� �̸� �ߺ� üũ(���� �̸� �־�������) */
    for ( sPartAttr = sPartTable->partAttr;
          sPartAttr != NULL;
          sPartAttr = sPartAttr->next )
    {
        for ( sTempPartAttr = sPartAttr->next;
              sTempPartAttr != NULL;
              sTempPartAttr = sTempPartAttr->next )
        {
            /* partition */
            if ( QC_IS_NAME_MATCHED( sPartAttr->tablePartName, sTempPartAttr->tablePartName )
                 == ID_TRUE )
            {
                sqlInfo.setSourceInfo(aStatement,
                                      & sTempPartAttr->tablePartName );

                IDE_RAISE( ERR_DUPLICATE_PARTITION_NAME );
            }
            else
            {
                /* Nothing To Do */
            }
            /* table */
            if ( QC_IS_NAME_MATCHED( sPartAttr->oldTableName, sTempPartAttr->oldTableName )
                 == ID_TRUE )
            {
                sqlInfo.setSourceInfo(aStatement,
                                      & sTempPartAttr->oldTableName );

                IDE_RAISE( ERR_DUPLICATE_TABLE_NAME );
            }
            else
            {
                /* Nothing To Do */
            }
        }
    }
    
    /* 3. ���̺� �̸� �ߺ� üũ(���� ����) */
    IDE_TEST( qcm::existObject( aStatement,
                                ID_FALSE,
                                sParseTree->userName,
                                sParseTree->tableName,
                                QS_OBJECT_MAX,
                                &(sParseTree->userID),
                                &sProcID,
                                &sExist )
              != IDE_SUCCESS);

    IDE_TEST_RAISE( sExist == ID_TRUE, ERR_EXIST_OBJECT_NAME );

    /* 4. ���̺� ���� ���� üũ */
    IDE_TEST( qdpRole::checkDDLCreateTablePriv( aStatement,
                                                sParseTree->userID )
              != IDE_SUCCESS );
    IDE_TEST( qdpRole::checkDDLDropTablePriv( aStatement,
                                                sParseTree->userID )
              != IDE_SUCCESS );

    /* 5. ��� ���̺� ��ü �̸����� ���� ���� �˻� */
    for ( sPartAttr = sPartTable->partAttr;
          sPartAttr != NULL;
          sPartAttr = sPartAttr->next )
    {
        IDE_TEST( QC_QMP_MEM( aStatement )->alloc( ID_SIZEOF( qcmPartitionInfoList ),
                                                 (void**)( &sPartInfoList ) )
                  != IDE_SUCCESS );

        IDE_TEST( qdbCommon::checkTableInfo( aStatement,
                                             sParseTree->userName,
                                             sPartAttr->oldTableName,
                                             &(sUserID),
                                             &(sPartInfoList->partitionInfo),
                                             &(sPartInfoList->partHandle),
                                             &(sPartInfoList->partSCN) )
                  != IDE_SUCCESS );

        // ��Ƽ�ǵ� ���̺� LOCK(IS)
        IDE_TEST( qcm::lockTableForDDLValidation( aStatement,
                                                  sPartInfoList->partHandle,
                                                  sPartInfoList->partSCN )
                  != IDE_SUCCESS );

        // partAttr�� ���� ������ parse tree�� info list�� �ۼ��Ѵ�.
        if ( sLastPartInfoList == NULL )
        {
            sPartInfoList->next = NULL;
            sLastPartInfoList = sPartInfoList;
            sPartTable->partInfoList = sLastPartInfoList;
        }
        else
        {
            sPartInfoList->next = NULL;
            sLastPartInfoList->next = sPartInfoList;
            sLastPartInfoList = sLastPartInfoList->next;
        }

        /* BUG-48290 shard object�� ���� DDL ���� */
        IDE_TEST( sdi::checkShardObjectForDDLInternal( aStatement,
                                                       sParseTree->userName,
                                                       sPartAttr->oldTableName ) != IDE_SUCCESS );
    }

    /* 6. �ɼ� �˻� */
    IDE_TEST( qdbCreate::validateTableSpace( aStatement )
              != IDE_SUCCESS );
    
    /* PROJ-2464 hybrid partitioned table ����
     *  - ���ó��� : PRJ-1671 Bitmap TableSpace And Segment Space Management
     *               Segment Storage �Ӽ� validation
     */
    IDE_TEST( qdbCommon::validatePhysicalAttr( sParseTree )
              != IDE_SUCCESS );

    // logging, nologging �Ӽ�
    IDE_TEST( qdbCreate::calculateTableAttrFlag( aStatement,
                                                 sParseTree )
              != IDE_SUCCESS );

    /* 7~9. ���̺�, �÷� validation & schema ���� �˻� */
    IDE_TEST( validateAndCompareTables( aStatement ) != IDE_SUCCESS );

    /* copy columns */
    /* ù��° �÷��� ������� ���� �����Ѵ�. */
    /* �׸��� lob �÷��� �ɼ��� �ٸ� �� �����Ƿ�, �÷��� �ɼ��� �����Ѵ�. */
    sColumnCount = sPartInfoList->partitionInfo->columnCount;
    IDE_TEST( STRUCT_ALLOC_WITH_COUNT(  QC_QMP_MEM( aStatement ),
                                        qcmColumn,
                                        sColumnCount,
                                        &sParseTree->columns )
              != IDE_SUCCESS );

    IDE_TEST( qcm::copyQcmColumns( QC_QMP_MEM( aStatement ),
                                   sPartInfoList->partitionInfo->columns,
                                   &sParseTree->columns,
                                   sColumnCount )
              != IDE_SUCCESS );

    for ( sColumn = sParseTree->columns;
          sColumn != NULL;
          sColumn = sColumn->next )
    {
        sColumn->defaultValue = NULL;
        sColumn->defaultValueStr = NULL;
    }

    /* 10. ��Ƽ�� �ɼ� üũ */
    IDE_TEST( qdbCommon::validatePartitionedTable( aStatement,
                                                   ID_FALSE ) /* aIsCreateTable */
              != IDE_SUCCESS );

    /* PROJ-2464 hybrid partitioned table ����
     *  - 10-4. Hybrid Partitioned Table�̸� ����
     */
    /* 10-4.1. Partitioned ������ �˻��Ѵ�. */
    sTableType = qdbCommon::getTableTypeFromTBSID( sParseTree->TBSAttr.mID );

    /* 10-4.2. Partition ������ �˻��Ѵ�. */
    qdbCommon::getTableTypeCountInPartInfoList( & sTableType,
                                                sPartTable->partInfoList,
                                                & sCountDiskType,
                                                & sCountMemType,
                                                & sCountVolType );

    /* 10-4.3. Hybrid Partitioned Table ������ Join Table�� �������� �ʴ´�. */
    sTotalCount = sCountDiskType + sCountMemType + sCountVolType;

    /* 10-4.4. ��� ���� Type���� �˻��Ѵ�. Hybrid Partitioned Table�� �ƴϴ�. */
    IDE_TEST_RAISE( !( ( sTotalCount == sCountDiskType ) ||
                       ( sTotalCount == sCountMemType ) ||
                       ( sTotalCount == sCountVolType ) ),
                    ERR_UNSUPPORT_ON_HYBRID_PARTITIONED_TABLE );
    
    /* lob column attribute üũ */
    IDE_TEST( qdbCommon::validateLobAttributeList( aStatement,
                                                   NULL,
                                                   sParseTree->columns,
                                                   &sParseTree->TBSAttr,
                                                   sParseTree->lobAttr )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( CANT_USE_RESERVED_WORD )
    {
        (void)sqlInfo.init( aStatement->qmeMem );
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDB_RESERVED_WORD_IN_OBJECT_NAME,
                                  sqlInfo.getErrMessage() ) );
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION( ERR_DUPLICATE_PARTITION_NAME )
    {
        (void)sqlInfo.init( aStatement->qmeMem );
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDB_DUPLICATE_PARTITION_NAME,
                                  sqlInfo.getErrMessage() ) );
        (void)sqlInfo.fini();

    }
    IDE_EXCEPTION( ERR_DUPLICATE_TABLE_NAME )
    {
        (void)sqlInfo.init( aStatement->qmeMem );
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDB_DUPLICATE_TABLE_NAME,
                                  sqlInfo.getErrMessage() ) );
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION( ERR_EXIST_OBJECT_NAME )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDB_EXIST_OBJECT_NAME ) );
    }
    /* PROJ-2464 hybrid partitioned table ���� */
    IDE_EXCEPTION( ERR_UNSUPPORT_ON_HYBRID_PARTITIONED_TABLE );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDB_CANNOT_SUPPORT_ON_HYBRID_PARTITIONED_TABLE ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdbJoin::validateAndCompareTables( qcStatement   * aStatement )
{
/***********************************************************************
 *
 * Description :
 *      PROJ-1810 Partition Exchange
 *      JOIN TABLE���� ���̺� ���� validate�Ѵ�.
 *      JOIN TABLE���� ��Ƽ������ ��ȯ�Ǵ� ��� ���̺��� schema�� ��ġ�ϴ��� �˻��Ѵ�.
 *      �߰��� Lob �÷��� ������ validate�Ѵ�.
 *      validateJoinTable���� ȣ��ȴ�.
 *
 * Implementation :
 *      schema �˻� ����� column ����, type, precision, scale, �̸�, ����,
 *      constraint ����, ����, �÷�, condition(CHECK)��.
 *    1. ���̺� ���� ���� ������ �˻��Ѵ�.
 *       1-1. TBS type ��Ƽ�ǵ� ���̺�� ��ġ, Table Type�� 'T'(���̺�), check Operatable(DROP)
 *       1-2. ��Ƽ�ǵ� ���̺� -> x
 *       1-3. replication X
 *       1-4. �ε���, PK, Unique, FK, Trigger(�ڽ��� DML ����� �� ����) ���� -> x
 *       1-5. hidden/compression/encrypted column -> x
 *       1-6. ACCESS�� READ_WRITE�� ���̺� ����
 *    2. ���̺�� �÷��� ��ȸ�ϸ鼭 ���� ���� ��ȸ�� ���̺�� �÷�, constraint�� ���Ѵ�.
 *       2-1. column ����, constraint ����
 *       2-2. column type/precision/scale
 *       2-3. column �̸�, ����
 *       2-4. constraint �̸�, ����, ����, condition(CHECK)
 *       2-5. compressed logging flag
 *    3. Į�� type�� LOB�� �ִ� ��� �߰��� lob column�� ���� validation�� �Ѵ�.
 *
 ***********************************************************************/

    SChar                    sErrorObjName[ 2 * QC_MAX_OBJECT_NAME_LEN + 2 ]; // table_name.col_name
    qdTableParseTree       * sParseTree = NULL;
    qdPartitionedTable     * sPartTable = NULL;
    qcmPartitionInfoList   * sPartInfoList = NULL;
    qcmColumn              * sColumn = NULL;

    /* table �� schema ���� �� ��� */
    qcmTableInfo           * sPrevInfo = NULL;
    qcmTableInfo           * sTableInfo = NULL;
    UInt                     sColOrder = 0;
    UInt                     sPrevColOrder = 0;
    UInt                     sColCount = 0;
    UInt                     sNotNullCount = 0;
    UInt                     sCheckCount = 0;

    sParseTree = (qdTableParseTree *)aStatement->myPlan->parseTree;
    sPartTable = sParseTree->partTable;

    /* parse tree�� �޸� table info list�� ��ȸ�ϸ鼭 validation, schema ��. */
    for ( sPartInfoList = sPartTable->partInfoList;
          sPartInfoList != NULL;
          sPartInfoList = sPartInfoList->next )
    {
        sTableInfo = sPartInfoList->partitionInfo;
        sColumn = NULL;

        /* 1-1. TBS Type */
        IDE_TEST_RAISE( qdtCommon::isSameTBSType( sTableInfo->TBSType,
                                                  sParseTree->TBSAttr.mType ) == ID_FALSE,
                        ERR_NO_CREATE_PARTITION_IN_COMPOSITE_TBS );

        /* Table Type & opreatable */
        IDE_TEST_RAISE( sTableInfo->tableType != QCM_USER_TABLE,
                        ERR_NOT_EXIST_TABLE );
        IDE_TEST_RAISE( QCM_IS_OPERATABLE_QP_DROP_TABLE( sTableInfo->operatableFlag ) != ID_TRUE,
                        ERR_NOT_EXIST_TABLE );

        /* 1-2. ��Ƽ�ǵ� ���̺� -> x */
        IDE_TEST_RAISE( sTableInfo->tablePartitionType != QCM_NONE_PARTITIONED_TABLE,
                        ERR_CANNOT_JOIN_PARTITIONED_TABLE );

        /* 1-3. replication X */
        IDE_TEST_RAISE( sTableInfo->replicationCount > 0,
                        ERR_DDL_WITH_REPLICATED_TABLE );

        /* 1-4. �ε���, PK, Unique, FK, Trigger(�ڽ��� DML ����� �� ����) ���� -> x */
        IDE_TEST_RAISE( sTableInfo->primaryKey != NULL,
                        ERR_JOIN_DISJOIN_TABLE_SPEC );

        IDE_TEST_RAISE( sTableInfo->uniqueKeyCount != 0,
                        ERR_JOIN_DISJOIN_TABLE_SPEC );

        IDE_TEST_RAISE( sTableInfo->foreignKeyCount != 0,
                        ERR_JOIN_DISJOIN_TABLE_SPEC );

        IDE_TEST_RAISE( sTableInfo->indexCount != 0,
                        ERR_JOIN_DISJOIN_TABLE_SPEC );

        IDE_TEST_RAISE( sTableInfo->triggerCount != 0,
                        ERR_JOIN_DISJOIN_TABLE_SPEC );

        /* TASK-7307 DML Data Consistency in Shard
         *   USABLE�� ���̺� ����: ACCESS �ɼ��� ������ */
        IDE_TEST_RAISE( ( QCG_CHECK_SHARD_DML_CONSISTENCY( aStatement ) == ID_TRUE ) &&
                        ( sTableInfo->mIsUsable != ID_TRUE ),
                        ERR_JOIN_DISJOIN_NOT_USABLE );

        /* 1-6. READ_WRITE�� ���̺� ���� */
        IDE_TEST_RAISE( sTableInfo->accessOption != QCM_ACCESS_OPTION_READ_WRITE,
                        ERR_JOIN_DISJOIN_NOT_READ_WRITE );

        /* 2-1. �÷� ����/constraint �� */
        if ( sPrevInfo != NULL )
        {
            /* �÷� ����, constraint ������ ���ƾ� �Ѵ�. */
            /* compressed logging �ɼ��� ���ƾ� �Ѵ�. */
            IDE_TEST_RAISE( sTableInfo->columnCount != sPrevInfo->columnCount,
                            ERR_JOIN_NOT_SAME_TABLE_SPEC );

            IDE_TEST_RAISE( sTableInfo->notNullCount != sPrevInfo->notNullCount,
                            ERR_JOIN_NOT_SAME_TABLE_SPEC );

            IDE_TEST_RAISE( sTableInfo->checkCount != sPrevInfo->checkCount,
                            ERR_JOIN_NOT_SAME_TABLE_SPEC );

            IDE_TEST_RAISE( ( sTableInfo->timestamp==NULL ) && ( sPrevInfo->timestamp != NULL ),
                            ERR_JOIN_NOT_SAME_TABLE_SPEC );

            IDE_TEST_RAISE( ( sTableInfo->timestamp!=NULL ) && ( sPrevInfo->timestamp == NULL ),
                            ERR_JOIN_NOT_SAME_TABLE_SPEC );

            IDE_TEST_RAISE( ( smiGetTableFlag( sTableInfo->tableHandle ) &
                              SMI_TBS_ATTR_LOG_COMPRESS_MASK ) !=
                            ( smiGetTableFlag( sPrevInfo->tableHandle ) &
                              SMI_TBS_ATTR_LOG_COMPRESS_MASK ),
                            ERR_JOIN_NOT_SAME_TABLE_SPEC );

            /* compare NOT NULL */
            for ( sNotNullCount = 0;
                  sNotNullCount < sTableInfo->notNullCount;
                  sNotNullCount++ )
            {
                /* not null �÷��� column order�� ���� ���� ���̺��� �Ͱ� ���Ѵ�. */
                sColOrder = ( sTableInfo->notNulls[sNotNullCount].constraintColumn[0] %
                              SMI_COLUMN_ID_MAXIMUM );
                sPrevColOrder = ( sPrevInfo->notNulls[sNotNullCount].constraintColumn[0] %
                                  SMI_COLUMN_ID_MAXIMUM );

                IDE_TEST_RAISE( sColOrder != sPrevColOrder,
                                ERR_JOIN_NOT_SAME_CONSTRAINT );
            }

            /* compare CHECK */
            // check ������ ������ ���缭 ���Ѵ�.
            for ( sCheckCount = 0;
                  sCheckCount < sTableInfo->checkCount;
                  sCheckCount++ )
            {
                /* compare check string */
                IDE_TEST_RAISE( idlOS::strMatch( sPrevInfo->checks[sCheckCount].checkCondition,
                                                 idlOS::strlen( sPrevInfo->checks[sCheckCount].checkCondition ),
                                                 sTableInfo->checks[sCheckCount].checkCondition,
                                                 idlOS::strlen( sTableInfo->checks[sCheckCount].checkCondition ) )
                                != 0,
                                ERR_JOIN_NOT_SAME_CONSTRAINT );
            }

            /* compare timestamp */
            if ( sTableInfo->timestamp != NULL )
            {
                sColOrder = ( sTableInfo->timestamp->constraintColumn[0] %
                              SMI_COLUMN_ID_MAXIMUM );
                sPrevColOrder = ( sPrevInfo->timestamp->constraintColumn[0] %
                                  SMI_COLUMN_ID_MAXIMUM );
                IDE_TEST_RAISE( sColOrder != sPrevColOrder,
                                ERR_JOIN_NOT_SAME_CONSTRAINT );
            }
            else
            {
                /* Nothing To Do */
            }
        }
        else
        {
            /* sPrevInfo = NULL -> ù ��° ���̺� �˻�. */
            /* Nothing To Do */
        }

        /* �÷� ���� validation + Schema �� */
        for ( sColumn = sTableInfo->columns, sColCount = 0;
              sColumn != NULL;
              sColumn = sColumn->next, sColCount++  )
        {
            /* 1-5. hidden/compression/encrypted �÷� ����� �Ѵ�. */
            IDE_TEST_RAISE( ( sColumn->flag & QCM_COLUMN_HIDDEN_COLUMN_MASK )
                            == QCM_COLUMN_HIDDEN_COLUMN_TRUE,
                            ERR_JOIN_DISJOIN_COLUMN_SPEC );

            IDE_TEST_RAISE( ( sColumn->basicInfo->column.flag & SMI_COLUMN_COMPRESSION_MASK )
                            == SMI_COLUMN_COMPRESSION_TRUE,
                            ERR_JOIN_DISJOIN_COLUMN_SPEC );

            IDE_TEST_RAISE( ( sColumn->basicInfo->module->flag & MTD_ENCRYPT_TYPE_MASK )
                            == MTD_ENCRYPT_TYPE_TRUE,
                            ERR_JOIN_DISJOIN_COLUMN_SPEC );

            /* 2. schema �� */
            if ( sPrevInfo != NULL )
            {
                /* �� ���� table�� ���� ���� �÷��� ���Ѵ�.*/
                // �÷� �̸� string ��
                IDE_TEST_RAISE( idlOS::strMatch( sColumn->name,
                                                 idlOS::strlen( sColumn->name ),
                                                 sPrevInfo->columns[sColCount].name,
                                                 idlOS::strlen( sPrevInfo->columns[sColCount].name ) ) != 0,
                                ERR_JOIN_NOT_SAME_TABLE_SPEC );

                // mtcColumn ��
                IDE_TEST_RAISE( sColumn->basicInfo->type.dataTypeId !=
                                sPrevInfo->columns[sColCount].basicInfo->type.dataTypeId,
                                ERR_JOIN_NOT_SAME_TABLE_SPEC );

                IDE_TEST_RAISE( sColumn->basicInfo->type.languageId !=
                                sPrevInfo->columns[sColCount].basicInfo->type.languageId,
                                ERR_JOIN_NOT_SAME_TABLE_SPEC );

                IDE_TEST_RAISE( sColumn->basicInfo->precision !=
                                sPrevInfo->columns[sColCount].basicInfo->precision,
                                ERR_JOIN_NOT_SAME_TABLE_SPEC );

                IDE_TEST_RAISE( sColumn->basicInfo->scale !=
                                sPrevInfo->columns[sColCount].basicInfo->scale,
                                ERR_JOIN_NOT_SAME_TABLE_SPEC );

                IDE_TEST_RAISE( sColumn->basicInfo->flag !=
                                sPrevInfo->columns[sColCount].basicInfo->flag,
                                ERR_JOIN_NOT_SAME_TABLE_SPEC );

                // smiColumn ��
                IDE_TEST_RAISE( sColumn->basicInfo->column.flag !=
                                sPrevInfo->columns[sColCount].basicInfo->column.flag,
                                ERR_JOIN_NOT_SAME_TABLE_SPEC );

                IDE_TEST_RAISE( sColumn->basicInfo->column.offset !=
                                sPrevInfo->columns[sColCount].basicInfo->column.offset,
                                ERR_JOIN_NOT_SAME_TABLE_SPEC );

                IDE_TEST_RAISE( sColumn->basicInfo->column.vcInOutBaseSize !=
                                sPrevInfo->columns[sColCount].basicInfo->column.vcInOutBaseSize,
                                ERR_JOIN_NOT_SAME_TABLE_SPEC );

                IDE_TEST_RAISE( sColumn->basicInfo->column.size !=
                                sPrevInfo->columns[sColCount].basicInfo->column.size,
                                ERR_JOIN_NOT_SAME_TABLE_SPEC );
            }
            else
            {
                /* ù ���̺��̶� �� ��� ����. �׳� continue */
                /* Nothing To Do */
            }
        }

        /* ���� �������� �Ѿ�鼭 prev ������ ���� ���̺� info�� ����. */
        sPrevInfo = sTableInfo;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NO_CREATE_PARTITION_IN_COMPOSITE_TBS )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDB_NO_CREATE_PARTITION_IN_COMPOSITE_TBS,
                                  sErrorObjName ) );
    }
    IDE_EXCEPTION( ERR_CANNOT_JOIN_PARTITIONED_TABLE) 
    {
        idlOS::snprintf( sErrorObjName,
                         QC_MAX_OBJECT_NAME_LEN + 1,
                         "%s",
                         sTableInfo->name );
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDB_CANNOT_JOIN_PARTITIONED_TABLE,
                                  sErrorObjName ) );
    }
    IDE_EXCEPTION( ERR_DDL_WITH_REPLICATED_TABLE )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDB_DDL_WITH_REPLICATED_TBL ) );
    }
    IDE_EXCEPTION( ERR_JOIN_DISJOIN_TABLE_SPEC ) // pk/uk/fk/index/trigger
    {
        idlOS::snprintf( sErrorObjName,
                         QC_MAX_OBJECT_NAME_LEN + 1,
                         "%s",
                         sTableInfo->name );
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDB_CANNOT_JOIN_DISJOIN_TABLE_SPEC,
                                  sErrorObjName ) );
    }
    IDE_EXCEPTION( ERR_NOT_EXIST_TABLE )
    {
        idlOS::snprintf( sErrorObjName,
                         QC_MAX_OBJECT_NAME_LEN + 1,
                         "%s",
                         sTableInfo->name );
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QCV_NOT_EXISTS_TABLE,
                                  sErrorObjName ) );
    }
    IDE_EXCEPTION( ERR_JOIN_DISJOIN_NOT_USABLE )
    {
        idlOS::snprintf( sErrorObjName,
                         QC_MAX_OBJECT_NAME_LEN + 1,
                         "%s",
                         sTableInfo->name );
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMX_TABLE_PARTITION_UNUSABLE,
                                  sErrorObjName ) );
    }
    IDE_EXCEPTION( ERR_JOIN_DISJOIN_NOT_READ_WRITE )
    {
        idlOS::snprintf( sErrorObjName,
                         QC_MAX_OBJECT_NAME_LEN + 1,
                         "%s",
                         sTableInfo->name );
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDB_JOIN_DISJOIN_NOT_READ_WRITE,
                                  sErrorObjName ) );
    }
    IDE_EXCEPTION( ERR_JOIN_NOT_SAME_TABLE_SPEC ) // different type/scale/precision/count/etc
    {
        if ( sColumn == NULL )
        {
            idlOS::snprintf( sErrorObjName,
                             QC_MAX_OBJECT_NAME_LEN + 1,
                             "%s",
                             sTableInfo->name );
        }
        else
        {
            idlOS::snprintf( sErrorObjName,
                             2 * QC_MAX_OBJECT_NAME_LEN + 2,
                             "%s.%s",
                             sTableInfo->name,
                             sColumn->name );
        }
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDB_JOIN_DIFFERENT_TABLE_SPEC,
                                  sErrorObjName ) );
    }
    IDE_EXCEPTION( ERR_JOIN_NOT_SAME_CONSTRAINT ) //not null, timestamp, check
    {
        idlOS::snprintf( sErrorObjName,
                         QC_MAX_OBJECT_NAME_LEN + 1,
                         "%s",
                         sTableInfo->name );
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDB_JOIN_DIFFERENT_CONSTRAINT_SPEC,
                                  sErrorObjName ) );
    }
    IDE_EXCEPTION( ERR_JOIN_DISJOIN_COLUMN_SPEC )  // hidden/compression/encrypted column
    {
        idlOS::snprintf( sErrorObjName,
                         2 * QC_MAX_OBJECT_NAME_LEN + 2,
                         "%s.%s",
                         sTableInfo->name,
                         sColumn->name );
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDB_CANNOT_JOIN_DISJOIN_COLUMN_SPEC,
                                  sErrorObjName ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdbJoin::executeJoinTable( qcStatement * aStatement )
{
/***********************************************************************
 *
 * Description :
 *    JOIN TABLE ... PARTITION BY RANGE/LIST ( ... )
 *    ( TABLE ... TO PARTITION ...,
 *      TABLE ... TO PARTITION ...,
 *      ... )
 *    ENABLE/DISABLE ROW MOVEMENT
 *    READ ONLY / READ APPEND / READ WRITE
 *    TABLESPACE ...
 *    PCTUSED int PCTFREE int INITRANS int MAXTRANS int
 *    STORAGE ( INITEXTENTS int NEXTEXTENTS int MINEXTENTS int MAXEXTENTS INT/UNLIMITED )
 *    LOGGING/NOLOGGING
 *    PARALLEL int
 *    LOB ( ... ) STORE AS ( TABLESPACE ... LOGGING/NOLOGGING BUFFER/NOBUFFER )
 *
 * Implementation :
 *    1. JOIN ����� ���̺���� info�� �����´�.
 *    2. ��Ƽ�ǵ� ���̺��� �����Ѵ�.
 *    3. ���� ���� ��Ƽ�ǵ� ���̺��� ��Ÿ ������ INSERT�Ѵ�.
 *    4. ���̺�(���� �� ��Ƽ��) ���� ������ �����Ѵ�.
 *       4-1. ���̺��� ���� partition ID�� ����.
 *       4-2. ���̺���� ������ ������ partition �����͸� ��Ÿ ���̺� �߰�
 *       4-3. ��Ƽ�� ���� column ID�� �����Ѵ�. (��Ƽ�ǵ� ���̺� ������ modifyColumn)
 *       4-4. ���� ���̺���� ��Ÿ ������ �����Ѵ�.
 *       4-5. ���� psm, pkg, view invalid
 *       4-6. constraint ���� function/procedure ���� ����
 *       4-7. ���̺�(������ ��Ƽ��) ��Ÿ ĳ�� �籸��
 *   5. ��Ƽ�ǵ� ���̺� ��Ÿ ĳ�� ����
 *
 ***********************************************************************/
    qdTableParseTree       * sParseTree = NULL;
    UInt                     sTableID = 0;
    smOID                    sTableOID = QS_EMPTY_OID;
    UInt                     sColumnCount = 0;
    UInt                     sPartKeyCount = 0;
    qcmColumn              * sPartKeyColumns = NULL;
    smSCN                    sSCN;
    void                   * sTableHandle = NULL;
    qcmPartitionInfoList   * sPartInfoList = NULL;
    qcmTableInfo           * sNewTableInfo = NULL;
    qcmTableInfo           * sPartInfo = NULL;
    qcmColumn              * sColumn = NULL;
    qcmTableInfo          ** sNewPartitionInfoArr = NULL;
    UInt                     sPartitionCount = 0;
    ULong                    sAllocSize = 0;
    UInt                     i = 0;

    /* for partitions */
    UInt                      sPartitionID = 0;
    qcmPartitionIdList      * sPartIdList = NULL;
    qcmPartitionIdList      * sFirstPartIdList = NULL;
    qcmPartitionIdList      * sLastPartIdList = NULL;
    qdPartitionAttribute    * sPartAttr = NULL;
    SChar                   * sPartMinVal = NULL;
    SChar                   * sPartMaxVal = NULL;
    SChar                   * sOldPartMaxVal = NULL;
    SChar                     sNewTablePartName[QC_MAX_OBJECT_NAME_LEN + 1];
    qcmPartitionMethod        sPartMethod;
    UInt                      sPartCount = 0;

    //---------------------------------
    // �⺻ ���� �ʱ�ȭ
    //---------------------------------
    sParseTree = (qdTableParseTree *)aStatement->myPlan->parseTree;

    /* ��� ��Ƽ��(������ ���̺�)�� LOCK(X) */
    for ( sPartInfoList = sParseTree->partTable->partInfoList, sPartitionCount = 0;
          sPartInfoList != NULL;
          sPartInfoList = sPartInfoList->next, sPartitionCount++ )
    {
        IDE_TEST( qcm::validateAndLockTable( aStatement,
                                             sPartInfoList->partHandle,
                                             sPartInfoList->partSCN,
                                             SMI_TABLE_LOCK_X )
                  != IDE_SUCCESS );
    }

    /* �ٽ� ù ��° ��Ƽ��(������ ���̺�)�� ����Ų�� */
    sPartInfoList = sParseTree->partTable->partInfoList;
    sPartInfo = sPartInfoList->partitionInfo;

    if ( sParseTree->parallelDegree == 0 )
    {
        // PROJ-1071 Parallel query
        // Table ���� �� parallel degree �� �������� �ʾҴٸ�
        // �ּҰ��� 1 �� �����Ѵ�.
        sParseTree->parallelDegree = 1;
    }
    else
    {
        /* nothing to do */
    }

    if ( ( QCU_FORCE_PARALLEL_DEGREE > 1 ) && ( sParseTree->parallelDegree == 1 ) )
    {
        // 1. Dictionary table �� �ƴ� ��
        // 2. Parallel degree �� 1 �� ��
        // ������ parallel degree �� property ũ��� �����Ѵ�.
        sParseTree->parallelDegree = QCU_FORCE_PARALLEL_DEGREE;
    }
    else
    {
        /* nothing to do */
    }

    /* 2. ��Ƽ�ǵ� ���̺��� �����Ѵ�. */
    IDE_TEST( qcm::getNextTableID( aStatement, &sTableID ) != IDE_SUCCESS );

    IDE_TEST( qdbCommon::createTableOnSM( aStatement,
                                          sParseTree->columns, /* �ȿ��� column ID ���õȴ�. */
                                          sParseTree->userID,
                                          sTableID,            /* new table ID */
                                          0,        // partitioned table�� maxrow �Ұ���
                                          sParseTree->TBSAttr.mID,
                                          sParseTree->segAttr,
                                          sParseTree->segStoAttr,
                                          sParseTree->tableAttrMask,
                                          sParseTree->tableAttrValue,
                                          sParseTree->parallelDegree,
                                          &sTableOID )
              != IDE_SUCCESS );

    sColumnCount = sPartInfo->columnCount;
    IDE_TEST( qdbCommon::insertTableSpecIntoMeta( aStatement,
                                                  ID_TRUE,
                                                  sParseTree->flag,
                                                  sParseTree->tableName,    /* new table name */
                                                  sParseTree->userID,
                                                  sTableOID,
                                                  sTableID,     /* new table ID */
                                                  sColumnCount,
                                                  sParseTree->maxrows,
                                                  sParseTree->accessOption,
                                                  sParseTree->TBSAttr.mID,
                                                  sParseTree->segAttr,
                                                  sParseTree->segStoAttr,
                                                  QCM_TEMPORARY_ON_COMMIT_NONE,
                                                  sParseTree->parallelDegree,     // PROJ-1071
                                                  QCM_SHARD_FLAG_TABLE_NONE )     // TASK-7307
             != IDE_SUCCESS);

    /* ���� �����Ǵ� ��Ƽ�ǵ� ���̺��� �÷� ID�� createTableOnSM���� �ڵ����� �Էµȴ�. */
    IDE_TEST( qdbCommon::insertColumnSpecIntoMeta( aStatement,
                                                   sParseTree->userID,
                                                   sTableID,            /* new table ID */
                                                   sParseTree->columns, /* new table columns */
                                                   ID_FALSE )
              != IDE_SUCCESS );

    for ( sPartKeyColumns = sParseTree->partTable->partKeyColumns;
          sPartKeyColumns != NULL;
          sPartKeyColumns = sPartKeyColumns->next )
    {
        sPartKeyCount++;
    }
    
    IDE_TEST( qdbCommon::insertPartTableSpecIntoMeta( aStatement,
                                                      sParseTree->userID,
                                                      sTableID,         /* new table ID */
                                                      sParseTree->partTable->partMethod,
                                                      sPartKeyCount,    /* part key count */
                                                      sParseTree->isRowmovement )
              != IDE_SUCCESS );

    IDE_TEST( qdbCommon::insertPartKeyColumnSpecIntoMeta( aStatement,
                                                          sParseTree->userID,
                                                          sTableID,     /* new table ID */
                                                          sParseTree->columns,  /* new columns */
                                                          sParseTree->partTable->partKeyColumns,
                                                          QCM_TABLE_OBJECT_TYPE )
             != IDE_SUCCESS );
    
    /* ��� ���̺���� ������� constraint ���� */
    /* DISJOIN������ ���̺�->��Ƽ�� 1������ copy�� �Ѵ�. */
    /* JOIN������ ���̺�(��Ƽ������ ��ȯ��)->��Ƽ�ǵ� ���̺�(�űԻ���)�� 1���� copy. */
    IDE_TEST( qdbDisjoin::copyConstraintSpec( aStatement,
                                              sPartInfo->tableID,   /* old table(1st part) ID */
                                              sTableID )            /* new table ID */
              != IDE_SUCCESS );
    
    /* ��Ƽ�ǵ� ���̺� ��Ÿ ĳ�� ���� */
    IDE_TEST( qcm::makeAndSetQcmTableInfo( QC_SMI_STMT( aStatement ),
                                           sTableID,
                                           sTableOID ) != IDE_SUCCESS );

    IDE_TEST( qcm::getTableInfoByID( aStatement,
                                     sTableID,
                                     &sNewTableInfo,
                                     &sSCN,
                                     &sTableHandle )
              != IDE_SUCCESS );

    /* PROJ-1723 [MDW/INTEGRATOR] Altibase Plugin ����
       DDL Statement Text�� �α�
     */
    if ( QCU_DDL_SUPPLEMENTAL_LOG == 1 )
    {
        IDE_TEST( qciMisc::writeDDLStmtTextLog( aStatement,
                                                sParseTree->userID,
                                                sNewTableInfo->name )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing To Do */
    }

    IDE_TEST( qcm::validateAndLockTable( aStatement,
                                         sTableHandle,
                                         sSCN,
                                         SMI_TABLE_LOCK_X )
              != IDE_SUCCESS );

    /* 4. ��Ƽ�� ���� �ʿ��� meta ������ ���� */
    IDU_LIMITPOINT("qdbJoin::executeJoin::malloc1");
    IDE_TEST( STRUCT_ALLOC_WITH_SIZE( aStatement->qmxMem,
                                      SChar,
                                      QC_MAX_PARTKEY_COND_VALUE_LEN+1,
                                      & sPartMaxVal )
             != IDE_SUCCESS );

    IDU_LIMITPOINT("qdbJoin::executeJoin::malloc2");
    IDE_TEST( STRUCT_ALLOC_WITH_SIZE( aStatement->qmxMem,
                                      SChar,
                                      QC_MAX_PARTKEY_COND_VALUE_LEN+1,
                                      & sPartMinVal )
              != IDE_SUCCESS );

    IDU_LIMITPOINT("qdbJoin::executeJoin::malloc3");
    IDE_TEST( STRUCT_ALLOC_WITH_SIZE( aStatement->qmxMem,
                                      SChar,
                                      QC_MAX_PARTKEY_COND_VALUE_LEN+1,
                                      & sOldPartMaxVal )
              != IDE_SUCCESS);

    /* �ٽ� ù ��° ��Ƽ��(������ ���̺�)�� ����Ų�� */
    sPartInfoList = sParseTree->partTable->partInfoList;
    sPartMethod = sParseTree->partTable->partMethod;

    for ( sPartAttr = sParseTree->partTable->partAttr, sPartCount = 0;
          sPartAttr != NULL;
          sPartAttr = sPartAttr->next, sPartCount++ )
    {
        sPartInfo = sPartInfoList->partitionInfo;
        IDE_TEST( qdbCommon::getPartitionMinMaxValue( aStatement,
                                                      sPartAttr,        /* partition attr */
                                                      sPartCount,       /* partition ���� */
                                                      sPartMethod,
                                                      sPartMaxVal,
                                                      sPartMinVal,
                                                      sOldPartMaxVal )
                  != IDE_SUCCESS );

        /* get partition ID */
        IDE_TEST( qcmPartition::getNextTablePartitionID( aStatement,
                                                         & sPartitionID)    /* new part ID */
                  != IDE_SUCCESS );

        IDE_TEST( aStatement->qmxMem->alloc( ID_SIZEOF(qcmPartitionIdList),
                                             (void**)&sPartIdList )
                  != IDE_SUCCESS );
        sPartIdList->partId = sPartitionID;

        if ( sFirstPartIdList == NULL )
        {
            sPartIdList->next = NULL;
            sFirstPartIdList = sPartIdList;
            sLastPartIdList = sFirstPartIdList;
        }
        else
        {
            sPartIdList->next = NULL;
            sLastPartIdList->next = sPartIdList;
            sLastPartIdList = sLastPartIdList->next;
        }

        /* ��Ÿ �߰� */
        QC_STR_COPY( sNewTablePartName, sPartAttr->tablePartName );

        /* sys_table_partitions_ */
        IDE_TEST( createTablePartitionSpec( aStatement,
                                            sPartMinVal,                /* part min val */
                                            sPartMaxVal,                /* part max val */
                                            sNewTablePartName,          /* new part name */
                                            sPartInfo->tableID,         /* old table ID */
                                            sTableID,                   /* new table ID */
                                            sPartitionID )              /* new part ID */
                  != IDE_SUCCESS);

        /* sys_part_lobs_ */
        sColumn = sPartInfo->columns;
        IDE_TEST( createPartLobSpec( aStatement,
                                     sColumnCount,          /* column count */
                                     sColumn,               /* old column */
                                     sParseTree->columns,   /* new column */
                                     sTableID,              /* new table ID */
                                     sPartitionID )         /* new part ID */
                  != IDE_SUCCESS );

        /* modify column id */
        sColumn = sPartInfo->columns;
        IDE_TEST( qdbDisjoin::modifyColumnID( aStatement,
                                              sColumn,                   /* old column(table) ptr */
                                              sPartInfo->columnCount,    /* column count */
                                              sParseTree->columns->basicInfo->column.id, /* new column ID */
                                              sPartInfo->tableHandle )   /* old table handle */
                  != IDE_SUCCESS );

        /* ��Ÿ ���� */

        /* delete from constraint related meta */
        IDE_TEST( qdd::deleteConstraintsFromMeta( aStatement, sPartInfo->tableID )
                  != IDE_SUCCESS );
    
        /* delete from index related meta */
        IDE_TEST( qdd::deleteIndicesFromMeta( aStatement, sPartInfo )
                  != IDE_SUCCESS);
    
        /* delete from table/columns related meta */
        IDE_TEST( qdd::deleteTableFromMeta( aStatement, sPartInfo->tableID )
                  != IDE_SUCCESS);
    
        /* delete from grant meta */
        IDE_TEST( qdpDrop::removePriv4DropTable( aStatement, sPartInfo->tableID )                  != IDE_SUCCESS );
    
        // PROJ-1359
        IDE_TEST( qdnTrigger::dropTrigger4DropTable( aStatement, sPartInfo )
                  != IDE_SUCCESS );

        /* ���� psm, pkg, view invalid */
        /* PROJ-2197 PSM Renewal */
        // related PSM
        IDE_TEST( qcmProc::relSetInvalidProcOfRelated( aStatement,
                                                       sParseTree->userID,
                                                       sPartInfo->name,
                                                       (UInt)idlOS::strlen( sPartInfo->name ),
                                                       QS_TABLE )
                  != IDE_SUCCESS );

        // PROJ-1073 Package
        IDE_TEST( qcmPkg::relSetInvalidPkgOfRelated( aStatement,
                                                     sParseTree->userID,
                                                     sPartInfo->name,
                                                     (UInt)idlOS::strlen( sPartInfo->name ),
                                                     QS_TABLE)
                  != IDE_SUCCESS );

        // related VIEW
        IDE_TEST( qcmView::setInvalidViewOfRelated( aStatement,
                                                    sParseTree->userID,
                                                    sPartInfo->name,
                                                    (UInt)idlOS::strlen( sPartInfo->name ),
                                                    QS_TABLE )
                  != IDE_SUCCESS );

        /* constraint ���� function, procedure ���� ���� */
        /* BUG-35445 Check Constraint, Function-Based Index���� ��� ���� Function�� ����/���� ���� */
        IDE_TEST( qcmProc::relRemoveRelatedToConstraintByTableID(
                        aStatement,
                        sPartInfo->tableID )
                  != IDE_SUCCESS );

        IDE_TEST( qcmProc::relRemoveRelatedToIndexByTableID(
                        aStatement,
                        sPartInfo->tableID )
                  != IDE_SUCCESS );

        // BUG-21387 COMMENT
        IDE_TEST( qdbComment::deleteCommentTable( aStatement,
                                                  sPartInfo->tableOwnerName,
                                                  sPartInfo->name )
                  != IDE_SUCCESS);

        // PROJ-2223 audit
        IDE_TEST( qcmAudit::deleteObject( aStatement,
                                          sPartInfo->tableOwnerID,
                                          sPartInfo->name )
                  != IDE_SUCCESS );

        /* ���� ���̺�(���� ��Ƽ������ ��ȯ�� ����)�� �Ѿ��. */
        sPartInfoList = sPartInfoList->next;
        
        IDU_FIT_POINT_RAISE( "qdbJoin::executeJoinTable::joinTableLoop::fail",
                             ERR_FIT_TEST );
    }

    /* ���̺�(������ ��Ƽ��) ��Ÿ ĳ�� �籸�� */

    // ���ο� qcmPartitionInfo���� pointer������ ������ �迭 ����
    sAllocSize = (ULong)sPartitionCount * ID_SIZEOF(qcmTableInfo*);
    IDU_FIT_POINT_RAISE( "qdbJoin::executeJoinTable::cralloc::sNewPartitionInfoArr",
                         ERR_MEMORY_ALLOCATION );
    IDE_TEST_RAISE( aStatement->qmxMem->cralloc( sAllocSize,
                                                 (void**) & sNewPartitionInfoArr )
                    != IDE_SUCCESS, ERR_MEMORY_ALLOCATION );

    sPartIdList = sFirstPartIdList;

    for ( sPartInfoList = sParseTree->partTable->partInfoList, i = 0;
          sPartInfoList != NULL;
          sPartInfoList = sPartInfoList->next, sPartIdList = sPartIdList->next, i++ )
    { 
        sPartInfo = sPartInfoList->partitionInfo;
        sTableID = sPartInfo->tableID;

        IDE_TEST( smiTable::touchTable( QC_SMI_STMT( aStatement ),
                                        sPartInfo->tableHandle,
                                        SMI_TBSLV_DDL_DML )
                  != IDE_SUCCESS );

        IDE_TEST( qcmPartition::makeAndSetQcmPartitionInfo( QC_SMI_STMT( aStatement ),
                                                            sPartIdList->partId,
                                                            sPartInfo->tableOID,
                                                            sNewTableInfo,
                                                            NULL )
                  != IDE_SUCCESS );

        IDE_TEST( smiGetTableTempInfo(
                      smiGetTable( sPartInfo->tableOID ),
                      (void**)(&sNewPartitionInfoArr[i]) )
                  != IDE_SUCCESS );

        /////////////////////////////////////////////////////////////////////////
        // ���� JOIN TABLE �������� Ʈ���� �ִ� ���̺��� ������� �����Ƿ�
        // ������� �ʴ´�.
        // IDE_TEST( qdnTrigger::freeTriggerCaches4DropTable( sPartInfo )
        //           != IDE_SUCCESS );
        /////////////////////////////////////////////////////////////////////////
        
        IDU_FIT_POINT_RAISE( "qdbJoin::executeJoinTable::makeAndSetQcmPartitionInfo::fail",
                             ERR_FIT_TEST );
    }

    /* (��) ��Ƽ�� ��Ÿ ĳ�� ���� */
    for ( sPartInfoList = sParseTree->partTable->partInfoList;
          sPartInfoList != NULL;
          sPartInfoList = sPartInfoList->next )
    { 
        sPartInfo = sPartInfoList->partitionInfo;
        (void)qcm::destroyQcmTableInfo( sPartInfo );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_MEMORY_ALLOCATION )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_MEMORY_ALLOCATION ) );
    }
    IDE_EXCEPTION( ERR_FIT_TEST )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qdbJoin::executeJoinTable",
                                  "FIT test" ) );
    }
    IDE_EXCEPTION_END;
    
    if ( sNewPartitionInfoArr != NULL )
    {
        for ( i = 0; i < sPartitionCount; i++ )
        {
            (void)qcmPartition::destroyQcmPartitionInfo( sNewPartitionInfoArr[i] );
        }
    }
    else
    {
        /* Nothing to do */
    }

    (void)qcm::destroyQcmTableInfo( sNewTableInfo );
    
    // on fail, must restore old table info.
    for ( sPartInfoList = sParseTree->partTable->partInfoList;
          sPartInfoList != NULL;
          sPartInfoList = sPartInfoList->next )
    {
        qcmPartition::restoreTempInfo( sPartInfoList->partitionInfo,
                                       NULL,
                                       NULL );
    }

    return IDE_FAILURE;
}

IDE_RC qdbJoin::createTablePartitionSpec( qcStatement             * aStatement,
                                          SChar                   * aPartMinVal,
                                          SChar                   * aPartMaxVal,
                                          SChar                   * aNewPartName,
                                          UInt                      aOldTableID,
                                          UInt                      aNewTableID,
                                          UInt                      aNewPartID )
{
/***********************************************************************
 *
 *  Description : PROJ-1810 Partition Exchange
 *                ��Ƽ������ ��ȯ�Ǵ� ���� ���̺��� table ��Ÿ ������ �����Ͽ�
 *                �ϳ��� table partition spec�� ��Ÿ�� �߰��Ѵ�.
 *
 *  Implementation :
 *      SYS_TABLES_���� ��Ƽ�ǵ� ���̺��� TABLE ID�� SELECT�� ��
 *      PARTITION_ID, PARTITION_NAME, MIN VALUE, MAX VALUE�� ���ڷ� ���� ���� ����
 *      �������� SYS_TABLES_�� ���� ����� SYS_TABLE_PARTITIONS_�� INSERT�Ѵ�.
 *
 ***********************************************************************/

    SChar       sSqlStr[ QD_MAX_SQL_LENGTH + 1 ];
    vSLong      sRowCnt = 0;
    SChar       sPartMinValueStr[ QC_MAX_PARTKEY_COND_VALUE_LEN + 10 ];
    SChar       sPartMaxValueStr[ QC_MAX_PARTKEY_COND_VALUE_LEN + 10 ];
    SChar       sPartValueTmp[ QC_MAX_PARTKEY_COND_VALUE_LEN * 2 + 19 ];
    SChar     * sPartValue = NULL; 

    // PARTITION_MIN_VALUE
    if ( idlOS::strlen( aPartMinVal ) == 0 )
    {
        idlOS::snprintf( sPartMinValueStr,
                         QC_MAX_PARTKEY_COND_VALUE_LEN + 10,
                         "%s",
                         "NULL" );
    }
    else
    {
        sPartValue = (SChar*)sPartValueTmp;
        // ���ڿ� �ȿ� '�� ���� ��� '�� �ϳ� �� �־������
        (void)qdbCommon::addSingleQuote4PartValue( aPartMinVal,
                                                   (SInt)idlOS::strlen( aPartMinVal ),
                                                   &sPartValue );

        idlOS::snprintf( sPartMinValueStr,
                         QC_MAX_PARTKEY_COND_VALUE_LEN + 10,
                         "VARCHAR'%s'",
                         sPartValue );
    }

    // PARTITION_MAX_VALUE
    if ( idlOS::strlen( aPartMaxVal ) == 0 )
    {
        idlOS::snprintf( sPartMaxValueStr,
                         QC_MAX_PARTKEY_COND_VALUE_LEN + 10,
                         "%s",
                         "NULL" );
    }
    else
    {
        sPartValue = (SChar*)sPartValueTmp;
        // ���ڿ� �ȿ� '�� ���� ��� '�� �ϳ� �� �־������
        (void)qdbCommon::addSingleQuote4PartValue( aPartMaxVal,
                                                   (SInt)idlOS::strlen( aPartMaxVal ),
                                                   &sPartValue );

        idlOS::snprintf( sPartMaxValueStr,
                         QC_MAX_PARTKEY_COND_VALUE_LEN + 10,
                         "VARCHAR'%s'",
                         sPartValue );
    }

    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH + 1,
                     "INSERT INTO SYS_TABLE_PARTITIONS_ "
                         "SELECT USER_ID, "
                         "INTEGER'%"ID_INT32_FMT"', "       /* TABLE_ID (new) */
                         "TABLE_OID, "
                         "INTEGER'%"ID_INT32_FMT"', "       /* PARTITION_ID (new) */
                         "VARCHAR'%s', "                    /* PARTITION_NAME(new) */
                         "%s, "                             /* PARTITION MIN VALUE */
                         "%s, "                             /* PARTITION MAX VALUE */
                         "NULL, "
                         "TBS_ID, "                         /* TBS_ID(table) */
                         "ACCESS, "
                         "USABLE, "                         /* PARITION_USABLE (TASK-7307) */
                         "REPLICATION_COUNT, "
                         "REPLICATION_RECOVERY_COUNT, "
                         "CREATED, "
                         "SYSDATE "                         /* LAST_DDL_TIME */
                         "FROM SYS_TABLES_ "
                         "WHERE TABLE_ID=INTEGER'%"ID_INT32_FMT"' ", /* TABLE_ID(old) */
                     aNewTableID,                      /* table ID(new) */
                     aNewPartID,                       /* partition ID(new) */
                     aNewPartName,                     /* partition name(new) */
                     sPartMinValueStr,                 /* partition min value(new) */
                     sPartMaxValueStr,                 /* partition max value(new) */
                     aOldTableID );                    /* table ID(old) */

    IDE_TEST( qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                 sSqlStr,
                                 & sRowCnt ) != IDE_SUCCESS );

    IDE_TEST_RAISE( sRowCnt != 1, ERR_META_CRASH );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_META_CRASH )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDB_META_CRASH ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdbJoin::createPartLobSpec( qcStatement             * aStatement,
                                   UInt                      aColumnCount,
                                   qcmColumn               * aOldColumn,
                                   qcmColumn               * aNewColumn,
                                   UInt                      aNewTableID,
                                   UInt                      aNewPartID )
{
/***********************************************************************
 *
 *  Description : PROJ-1810 Partition Exchange
 *                ���� ���̺��� lob column �ϳ��� ��Ÿ ������ �����Ͽ�
 *                �ش� lob column spec�� sys_part_lobs ��Ÿ�� �߰��Ѵ�.
 *
 *  Implementation :
 *      SYS_COLUMNS_���� ��Ƽ�ǵ� ���̺� Į���� COLUMN_ID�� SELECT�� ��
 *      COLUMN_ID, TABLE_ID�� �����ؼ� SYS_COLUMNS_�� INSERT.
 *
 ***********************************************************************/

    SChar               sSqlStr[ QD_MAX_SQL_LENGTH + 1 ];
    vSLong              sRowCnt = 0;
    qcmColumn         * sColumn = NULL;
    UInt                sOldColumnID = 0;
    UInt                sNewColumnID = 0;
    UInt                i = 0;

    sColumn = aOldColumn;

    for ( i = 0; i < aColumnCount; i++, sColumn = sColumn->next )
    {
        if ( ( sColumn->basicInfo->module->flag & MTD_COLUMN_TYPE_MASK )
             == MTD_COLUMN_TYPE_LOB )
        {
            sOldColumnID = sColumn->basicInfo->column.id; /* old column ID */
            sNewColumnID = aNewColumn[i].basicInfo->column.id; /* new column ID */

            idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH + 1,
                             "INSERT INTO SYS_PART_LOBS_ "
                                 "SELECT USER_ID, "
                                 "INTEGER'%"ID_INT32_FMT"', "       /* TABLE_ID(new) */
                                 "INTEGER'%"ID_INT32_FMT"', "       /* PARTITION_ID(new) */
                                 "INTEGER'%"ID_INT32_FMT"', "       /* COLUMN_ID(new) */
                                 "TBS_ID, "
                                 "LOGGING, "
                                 "BUFFER "
                                 "FROM SYS_LOBS_ "
                                 "WHERE COLUMN_ID=INTEGER'%"ID_INT32_FMT"' ", /* COL_ID(old) */
                             aNewTableID,                           /* table ID(new) */
                             aNewPartID,                            /* partition ID(new) */
                             sNewColumnID,                          /* column id(new) */
                             sOldColumnID );                        /* column id(old) */

            IDE_TEST( qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                         sSqlStr,
                                         & sRowCnt ) != IDE_SUCCESS );

            IDE_TEST_RAISE( sRowCnt != 1, ERR_META_CRASH );
        }
        else
        {
            /* Nothing To Do */
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_META_CRASH )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDB_META_CRASH ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
