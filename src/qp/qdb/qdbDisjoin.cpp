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
#include <mtcDef.h>
#include <qdbCommon.h>
#include <qdbDisjoin.h>
#include <qcm.h>
#include <qcmPartition.h>
#include <qcg.h>
#include <smiDef.h>
#include <qcmTableSpace.h>
#include <qdpDrop.h>
#include <qdnTrigger.h>
#include <qdd.h>
#include <qcmProc.h>
#include <qcmPkg.h>
#include <qcmView.h>
#include <qcmAudit.h>
#include <qdpRole.h>
#include <qdbComment.h>
#include <sdi.h>

IDE_RC qdbDisjoin::validateDisjoinTable( qcStatement * aStatement )
{
/***********************************************************************
 *
 * Description :
 *    DISJOIN TABLE ... ( PARTITION ... TO TABLE ... , ... )
 *
 * Implementation :
 *    1. ���̺� dollar name üũ
 *    2. ���� �� �����Ǵ� ���̺���� �̸��� �̹� �����ϴ��� üũ
 *    3. ���̺� ���� ���� üũ
 *       3-1. ���̺� LOCK(IS)
 *    4. ��Ƽ�ǵ� ���̺� �ƴϸ� ����
 *       4-1. �ؽ� ��Ƽ�ǵ� ���̺��̸� ����
 *    5. ��Ƽ�� �̸��� ����� ���� ���� üũ
 *       5.1. Hybrid Partitioned Table�̸� ����
 *    6. ACCESS�� READ_WRITE�� ���̺� ����
 *    7. statement�� ���̺��� ��Ƽ�� ���� �Է��ߴ��� Ȯ��
 *    8. �ε���, PK, UK, FK�� ������ ����
 *    9. ���̺� ����ȭ�� �ɷ������� ����
 *    10. ����, operatable üũ (CREATE & DROP)
 *    11. compression Į���� ������ ����
 *    12. hidden Į���� ������ ����
 *    13. ���� Į���� ������ ����
 *    14. �ڽſ� ���� �̺�Ʈ�� �߻��ϴ� Ʈ���Ű� ������ ����(SYS_TRIGGERS_�� TABLE ID�� ������ ����)
 *
 ***********************************************************************/

    qdDisjoinTableParseTree  * sParseTree = NULL;
    qcuSqlSourceInfo           sqlInfo;
    qcmTableInfo             * sTableInfo = NULL;
    qcmColumn                * sColumn = NULL;
    qdDisjoinTable           * sDisjoin = NULL;
    qsOID                      sProcID = QS_EMPTY_OID;
    idBool                     sExist = ID_FALSE;
    UInt                       sPartCount = 0;

    SInt                       sCountDiskType = 0;
    SInt                       sCountMemType  = 0;
    SInt                       sCountVolType  = 0;
    SInt                       sTotalCount    = 0;
    UInt                       sTableType     = 0;

    sParseTree = (qdDisjoinTableParseTree *)aStatement->myPlan->parseTree;

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

    /* 2. ���� �� �����Ǵ� ���̺���� �̸��� �̹� �����ϴ��� üũ */
    for ( sDisjoin = sParseTree->disjoinTable;
          sDisjoin != NULL;
          sDisjoin = sDisjoin->next )
    {
        IDE_TEST( qcm::existObject( aStatement,
                                    ID_FALSE,
                                    sParseTree->userName,   /* empty name */
                                    sDisjoin->newTableName,
                                    QS_OBJECT_MAX,
                                    &(sParseTree->userID),
                                    &sProcID,
                                    &sExist )
                  != IDE_SUCCESS );

        if ( sExist == ID_TRUE )
        {
            sqlInfo.setSourceInfo( aStatement,
                                   &(sDisjoin->newTableName) );
            IDE_RAISE( ERR_DUPLICATE_TABLE_NAME );
        }
        else
        {
            /* Nothing To Do */
        }
    }

    /* 3. ���̺� ���� ���� üũ */
    IDE_TEST( qdbCommon::checkTableInfo( aStatement,
                                         sParseTree->userName,  /* empty */
                                         sParseTree->tableName,
                                         &(sParseTree->userID),
                                         &(sParseTree->tableInfo),
                                         &(sParseTree->tableHandle),
                                         &(sParseTree->tableSCN) )
              != IDE_SUCCESS);

    /* BUG-48290 shard object�� ���� DDL ���� */
    IDE_TEST( sdi::checkShardObjectForDDL( aStatement, SDI_DDL_TYPE_DISJOIN ) != IDE_SUCCESS );

    // ��Ƽ�ǵ� ���̺� LOCK(IS)
    IDE_TEST( smiValidateAndLockObjects( ( QC_SMI_STMT( aStatement ) )->getTrans(),
                                         sParseTree->tableHandle,
                                         sParseTree->tableSCN,
                                         SMI_TBSLV_DROP_TBS, // TBS Validation �ɼ�
                                         SMI_TABLE_LOCK_IS,
                                         smiGetDDLLockTimeOut((QC_SMI_STMT(aStatement))->getTrans()),
                                         ID_FALSE )
              != IDE_SUCCESS );

    sTableInfo = sParseTree->tableInfo;

    IDE_TEST( qcmPartition::getPartitionInfoList( aStatement,
                                                  QC_SMI_STMT( aStatement ),
                                                  QC_QMP_MEM( aStatement ),
                                                  sTableInfo->tableID,
                                                  & (sParseTree->partInfoList) )
              != IDE_SUCCESS );

    // ��� ��Ƽ�ǿ� LOCK(IS)
    IDE_TEST( qcmPartition::validateAndLockPartitionInfoList( aStatement,
                                                              sParseTree->partInfoList,
                                                              SMI_TBSLV_DROP_TBS, // TBS Validation �ɼ�
                                                              SMI_TABLE_LOCK_IS,
                                                              smiGetDDLLockTimeOut((QC_SMI_STMT(aStatement))->getTrans()))
              != IDE_SUCCESS );

    /* PROJ-2464 hybrid partitioned table ���� */
    sTableType = sTableInfo->tableFlag & SMI_TABLE_TYPE_MASK;

    /* 4. ��Ƽ�ǵ� ���̺� �ƴϸ� ���� */
    IDE_TEST_RAISE( sTableInfo->tablePartitionType  == QCM_NONE_PARTITIONED_TABLE,
                    ERR_DISJOIN_TABLE_NON_PART_TABLE );
    /* 4-1. �ؽ� ��Ƽ�ǵ� ���̺��̸� ���� */
    IDE_TEST_RAISE( sTableInfo->partitionMethod == QCM_PARTITION_METHOD_HASH,
                    ERR_DISJOIN_TABLE_NON_PART_TABLE );

    /* 5. ��Ƽ�� �̸��� ����� ���� ����(�����ϴ���), ��Ƽ�� ��ü �̸��� ����� üũ */
    /*    + partition info list �����ͼ� parse tree�� �޾� ���´�. */
    IDE_TEST( checkPartitionExistByName( aStatement,
                                         sParseTree->partInfoList,
                                         sParseTree->disjoinTable )
              != IDE_SUCCESS );

    /* PROJ-2464 hybrid partitioned table ����
     *  - 5.1. Hybrid Partitioned Table�̸� ����
     */
    /* 5.1.1. Partition ������ �˻��Ѵ�. */
    qdbCommon::getTableTypeCountInPartInfoList( & sTableType,
                                                sParseTree->partInfoList,
                                                & sCountDiskType,
                                                & sCountMemType,
                                                & sCountVolType );

    /* 5.1.2. Hybrid Partitioned Table ������ Disjoin Table�� �������� �ʴ´�. */
    sTotalCount = sCountDiskType + sCountMemType + sCountVolType;

    /* 5.1.3. ��� ���� Type���� �˻��Ѵ�. Hybrid Partitioned Table�� �ƴϴ�. */
    IDE_TEST_RAISE( !( ( sTotalCount == sCountDiskType ) ||
                       ( sTotalCount == sCountMemType ) ||
                       ( sTotalCount == sCountVolType ) ),
                    ERR_UNSUPPORT_ON_HYBRID_PARTITIONED_TABLE );
    
    /* TASK-7307 DML Data Consistency in Shard
     *   USABLE�� ���̺� ����: ACCESS �ɼ��� ������ */
    if ( ( QCG_CHECK_SHARD_DML_CONSISTENCY( aStatement ) == ID_TRUE ) &&
         ( sTableInfo->mIsUsable != ID_TRUE ) )
    {
        sqlInfo.setSourceInfo( aStatement,
                               &(sParseTree->tableName) );
        IDE_RAISE( ERR_JOIN_DISJOIN_NOT_USABLE );
    }
    else
    {
        /* Nothing To Do */
    }

    /* 6. READ_WRITE�� ���̺� ���� */
    if ( sTableInfo->accessOption != QCM_ACCESS_OPTION_READ_WRITE )
    {
        sqlInfo.setSourceInfo( aStatement,
                               &(sParseTree->tableName) );
        IDE_RAISE( ERR_JOIN_DISJOIN_NOT_READ_WRITE );
    }
    else
    {
        /* Nothing To Do */
    }

    /* TASK-7307 USABLE�� ���̺� ���� */
    /* 6-1. READ_WRITE�� ��Ƽ�Ǹ� ���� */
    for ( sDisjoin = sParseTree->disjoinTable;
          sDisjoin != NULL;
          sDisjoin = sDisjoin->next )
    {
        if ( ( QCG_CHECK_SHARD_DML_CONSISTENCY( aStatement ) == ID_TRUE ) &&
             ( sDisjoin->oldPartInfo->mIsUsable != ID_TRUE ) )
        {
            sqlInfo.setSourceInfo( aStatement,
                                   &(sDisjoin->oldPartName) );
            IDE_RAISE( ERR_JOIN_DISJOIN_NOT_USABLE );
        }
        else if ( sDisjoin->oldPartInfo->accessOption != QCM_ACCESS_OPTION_READ_WRITE )
        {
            sqlInfo.setSourceInfo( aStatement,
                                   &(sDisjoin->oldPartName) );
            IDE_RAISE( ERR_JOIN_DISJOIN_NOT_READ_WRITE );
        }
        else
        {
            /* Nothing To Do */
        }
    }

    /* 7. statement�� ���̺��� ��Ƽ�� ���� �Է��ߴ��� Ȯ�� */
    IDE_TEST( qcmPartition::getPartitionCount( aStatement,
                                               sTableInfo->tableID,
                                               & sPartCount )
              != IDE_SUCCESS );

    IDE_TEST_RAISE( sPartCount != sParseTree->partCount,
                    ERR_DISJOIN_MISS_SOME_PARTITION );

    /* 8. �ε���, PK, Unique, FK, Trigger(�ڽſ� ���� DML �ߵ��ϴ� �͸�)�� ������ ���� */
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

    /* 9. ���̺� ����ȭ�� �ɷ� ������ ���� */
    IDE_TEST_RAISE( sTableInfo->replicationCount > 0,
                    ERR_DDL_WITH_REPLICATED_TABLE );
    //proj-1608:replicationCount�� 0�� �� recovery count�� �׻� 0�̾�� ��
    IDE_DASSERT( sTableInfo->replicationRecoveryCount == 0 );

    /* 10. ����, operatable üũ (CREATE & DROP) */
    IDE_TEST( qdpRole::checkDDLCreateTablePriv( aStatement,
                                                sParseTree->userID )
              != IDE_SUCCESS );

    IDE_TEST( qdpRole::checkDDLDropTablePriv( aStatement,
                                                sParseTree->userID )
              != IDE_SUCCESS );

    IDE_TEST_RAISE( QCM_IS_OPERATABLE_QP_DROP_TABLE( sTableInfo->operatableFlag ) != ID_TRUE,
                    ERR_NOT_EXIST_TABLE );

    /* 11. compression Į���� ������ ���� */
    /* 12. hidden Į���� ������ ���� */
    /* 13. ���� Į���� ������ ���� */
    for ( sColumn = sTableInfo->columns;
          sColumn != NULL;
          sColumn = sColumn->next  )
    {
        IDE_TEST_RAISE( ( sColumn->flag & QCM_COLUMN_HIDDEN_COLUMN_MASK )
                        == QCM_COLUMN_HIDDEN_COLUMN_TRUE,
                        ERR_JOIN_DISJOIN_COLUMN_SPEC );

        IDE_TEST_RAISE( ( sColumn->basicInfo->column.flag & SMI_COLUMN_COMPRESSION_MASK )
                        == SMI_COLUMN_COMPRESSION_TRUE,
                        ERR_JOIN_DISJOIN_COLUMN_SPEC );

        IDE_TEST_RAISE( ( sColumn->basicInfo->module->flag & MTD_ENCRYPT_TYPE_MASK )
                        == MTD_ENCRYPT_TYPE_TRUE,
                        ERR_JOIN_DISJOIN_COLUMN_SPEC );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( CANT_USE_RESERVED_WORD )
    {
        (void)sqlInfo.init( aStatement->qmeMem );
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDB_RESERVED_WORD_IN_OBJECT_NAME,
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
    IDE_EXCEPTION( ERR_DISJOIN_MISS_SOME_PARTITION )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDB_DISJOIN_NOT_ALL_PARTITION ) );
    }
    IDE_EXCEPTION( ERR_DISJOIN_TABLE_NON_PART_TABLE) //non-partitioned or hash table
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDB_CANNOT_JOIN_DISJOIN_HASH_OR_NON_PART_TBL ) );
    }
    IDE_EXCEPTION( ERR_JOIN_DISJOIN_TABLE_SPEC ) // pk/uk/fk/index/trigger
    {
        sqlInfo.setSourceInfo( aStatement,
                               &(sParseTree->tableName) );
        (void)sqlInfo.init( aStatement->qmeMem );
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDB_CANNOT_JOIN_DISJOIN_TABLE_SPEC,
                                  sqlInfo.getErrMessage() ) );
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION( ERR_DDL_WITH_REPLICATED_TABLE )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDB_DDL_WITH_REPLICATED_TBL ) );
    }
    IDE_EXCEPTION( ERR_JOIN_DISJOIN_NOT_USABLE )
    {
        (void)sqlInfo.init( aStatement->qmeMem );
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMX_TABLE_PARTITION_UNUSABLE,
                                  sqlInfo.getErrMessage() ) );
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION( ERR_JOIN_DISJOIN_NOT_READ_WRITE )
    {
        (void)sqlInfo.init( aStatement->qmeMem );
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDB_JOIN_DISJOIN_NOT_READ_WRITE,
                                  sqlInfo.getErrMessage() ) );
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION( ERR_NOT_EXIST_TABLE )
    {
        sqlInfo.setSourceInfo( aStatement,
                               & sParseTree->tableName );
        (void)sqlInfo.init( aStatement->qmeMem );
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QCV_NOT_EXISTS_TABLE,
                                  sqlInfo.getErrMessage() ) );
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION( ERR_JOIN_DISJOIN_COLUMN_SPEC )  // hidden/compression/encrypted column
    {
        sqlInfo.setSourceInfo( aStatement,
                               & sParseTree->tableName );
        (void)sqlInfo.init( aStatement->qmeMem );
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDB_CANNOT_JOIN_DISJOIN_COLUMN_SPEC,
                                  sqlInfo.getErrMessage() ) );
        (void)sqlInfo.fini();
    }
    /* PROJ-2464 hybrid partitioned table ���� */
    IDE_EXCEPTION( ERR_UNSUPPORT_ON_HYBRID_PARTITIONED_TABLE );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDB_CANNOT_SUPPORT_ON_HYBRID_PARTITIONED_TABLE ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdbDisjoin::executeDisjoinTable( qcStatement * aStatement )
{
/***********************************************************************
 *
 * Description :
 *    DISJOIN TABLE ... ( PARTITION ... TO TABLE ... , ... )
 *
 * Implementation :
 *    1. table/partition info�� �����´�
 *    2. ��Ƽ�� ���� ������ �����Ѵ�. (��Ÿ INSERT�� ���� ���̺� �����͸� �����ؼ� ����)
 *       2-1. ��Ƽ���� ���� table ID�� ����.
 *       2-2. ��Ƽ���� ���� table �����͸� ��Ÿ ���̺� �߰�
 *       2-3. ��Ƽ���� ���� column ID�� �����ϰ� column �����͸� ��Ÿ ���̺� �߰�
 *       2-4. constraint ���� ID ����, �����͸� ��Ÿ�� �߰�
 *    3. ���� ���̺��� ��Ÿ ������ �����Ѵ�.
 *    4. ���� psm, pkg, view invalid
 *    5. constraint ���� function/procedure ���� ����
 *    6. smi::droptable
 *    7. ��Ƽ��(������ ���̺�) ��Ÿ ĳ�� �籸��
 *    8. ���̺� ��Ÿ ĳ������ ����
 *
 ***********************************************************************/

    qdDisjoinTableParseTree * sParseTree = NULL;
    qcmPartitionInfoList    * sPartInfoList = NULL;
    qcmPartitionInfoList    * sOldPartInfoList = NULL;

    qcmTableInfo            * sTableInfo = NULL;
    qcmTableInfo            * sPartInfo = NULL;
    qdDisjoinTable          * sDisjoin = NULL;
    qcmColumn               * sColumn = NULL;
    qcmTableInfo           ** sNewTableInfoArr = NULL;
    smiTableSpaceAttr         sTBSAttr;

    UInt                      sTableID = 0;
    UInt                      sNewColumnID = 0;
    UInt                      sPartitionCount = 0;
    ULong                     sAllocSize = 0;
    UInt                      i = 0;

    sParseTree = (qdDisjoinTableParseTree *)aStatement->myPlan->parseTree;

    // TASK-2176
    // Table�� ���� Lock�� ȹ���Ѵ�.
    IDE_TEST( smiValidateAndLockObjects( ( QC_SMI_STMT( aStatement ) )->getTrans(),
                                         sParseTree->tableHandle,
                                         sParseTree->tableSCN,
                                         SMI_TBSLV_DROP_TBS, // TBS Validation �ɼ�
                                         SMI_TABLE_LOCK_X,
                                         smiGetDDLLockTimeOut((QC_SMI_STMT(aStatement))->getTrans()),
                                         ID_FALSE )
              != IDE_SUCCESS );

    sTableInfo = sParseTree->tableInfo;  /* old table info */

    // PROJ-1502 PARTITIONED DISK TABLE
    // ��� ��Ƽ�ǿ� LOCK(X)
    IDE_TEST( qcmPartition::validateAndLockPartitionInfoList( aStatement,
                                                              sParseTree->partInfoList,
                                                              SMI_TBSLV_DROP_TBS, // TBS Validation �ɼ�
                                                              SMI_TABLE_LOCK_X,
                                                              smiGetDDLLockTimeOut((QC_SMI_STMT(aStatement))->getTrans()))
              != IDE_SUCCESS );

    // ���� ó���� ���Ͽ�, Lock�� ���� �Ŀ� Partition List�� �����Ѵ�.
    sOldPartInfoList = sParseTree->partInfoList;

    for ( sPartInfoList = sOldPartInfoList;
          sPartInfoList != NULL;
          sPartInfoList = sPartInfoList->next )
    {
        sPartitionCount++;
    }

    // ���ο� qcmPartitionInfo���� pointer������ ������ �迭 ����
    sAllocSize = (ULong)sPartitionCount * ID_SIZEOF(qcmTableInfo*);
    IDU_FIT_POINT_RAISE( "qdbDisjoin::executeDisjoinTable::cralloc::sNewTableInfoArr",
                         ERR_MEMORY_ALLOCATION );
    IDE_TEST_RAISE( aStatement->qmxMem->cralloc(
                    sAllocSize,
                    (void**) & sNewTableInfoArr )
                    != IDE_SUCCESS, ERR_MEMORY_ALLOCATION );

    /* PROJ-1723 [MDW/INTEGRATOR] Altibase Plugin ����
     DDL Statement Text�� �α�
     */
    if ( QCU_DDL_SUPPLEMENTAL_LOG == 1 )
    {
        IDE_TEST( qciMisc::writeDDLStmtTextLog( aStatement,
                                                sTableInfo->tableOwnerID,
                                                sTableInfo->name )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    /* 2. ��Ƽ�� ���� ��Ÿ copy ���� */
    /* �ʿ��� table ID, column ID, constraint ID ���� ���� �� */
    /* ���� ��Ƽ�ǵ� ���̺��� �����͸� ������� ��Ÿ ���̺� ������ copy */

    /* sDisjoin�� ��Ƽ�� �ϳ��� �����ȴ�. */
    /* sPartInfo�� �� ��Ƽ���� tableInfo��. */
    for ( sDisjoin = sParseTree->disjoinTable;
          sDisjoin != NULL;
          sDisjoin = sDisjoin->next )
    {
        sPartInfo = sDisjoin->oldPartInfo;

        IDE_TEST( qcm::getNextTableID( aStatement, &sDisjoin->newTableID )
                  != IDE_SUCCESS );
        sTableID = sDisjoin->newTableID;

        /* ��Ÿ ���� */

        /* sys_tables_ */
        IDE_TEST( qcmTablespace::getTBSAttrByID( sPartInfo->TBSID,
                                                 &sTBSAttr )
                  != IDE_SUCCESS );

        IDE_TEST( copyTableSpec( aStatement,
                                 sDisjoin,
                                 sTBSAttr.mName,
                                 sTableInfo->tableID )    /* table ID(old) */
                  != IDE_SUCCESS );

        /* sys_columns_, sys_lobs_ */
        sNewColumnID = sTableID * SMI_COLUMN_ID_MAXIMUM;
        sColumn = sTableInfo->columns;

        /* ��Ƽ�� ����� column ID�� ������ �� ��Ÿ�� �� ���̺��� column spec�� �߰��Ѵ�. */
        /* ��Ƽ�ǵ� ���̺��� ù ��° Į��(qcmcolumn)�� */
        /* �� ���̺��� ù��° Į�� ID�� �Ѱ��ش�. */
        IDE_TEST( modifyColumnID( aStatement,
                                  sDisjoin->oldPartInfo->columns,   /* column(partition's first) */
                                  sTableInfo->columnCount,          /* column count */
                                  sNewColumnID,                     /* new column ID(first) */
                                  sPartInfo->tableHandle )          /* partition handle */
                  != IDE_SUCCESS );
        
        for ( i = 0; i < sTableInfo->columnCount; i++ )
        {
            /* �÷� �ϳ��� �����͸� ī�� */
            IDE_TEST( copyColumnSpec( aStatement,
                                      sParseTree->userID,               /* user ID */
                                      sTableInfo->tableID,              /* table ID(old) */
                                      sTableID,                         /* table ID(new) */
                                      sPartInfo->partitionID,           /* partition ID(old) */
                                      sColumn[i].basicInfo->column.id,  /* column ID(old) */
                                      sNewColumnID + i,                 /* column ID(new) */
                                      sColumn[i].basicInfo->module->flag ) /* column flag */
                      != IDE_SUCCESS );
        }

        /* constraint ���� �� �̸�, ID�� �����ϰ� ��Ÿ�� �����Ѵ�. */
        /* sys_constraints_ */
        /* constraint ID�� ������� search�ϰ� constraint name ����, ��Ÿ copy */

        IDE_TEST( copyConstraintSpec( aStatement,
                                      sTableInfo->tableID,  /* old table ID */
                                      sTableID )   /* new table ID */
                  != IDE_SUCCESS );
        
        IDU_FIT_POINT_RAISE( "qdbDisjoin::executeDisjoinTable::disjoinTableLoop::fail",
                             ERR_FIT_TEST );
    }

    /* 3. ��Ƽ�ǵ� ���̺��� ��Ÿ delete */

    /* delete from partition related meta */
    for ( sDisjoin = sParseTree->disjoinTable;
          sDisjoin != NULL;
          sDisjoin = sDisjoin->next )
    {
        IDE_TEST( qdd::deleteTablePartitionFromMeta( aStatement,
                                                     sDisjoin->oldPartID )
                 != IDE_SUCCESS );
    } 

    /* delete from constraint related meta */
    IDE_TEST( qdd::deleteConstraintsFromMeta( aStatement, sTableInfo->tableID )
              != IDE_SUCCESS );

    /* delete from index related meta */
    IDE_TEST( qdd::deleteIndicesFromMeta( aStatement, sTableInfo )
              != IDE_SUCCESS);

    /* delete from table/columns related meta */
    IDE_TEST( qdd::deleteTableFromMeta( aStatement, sTableInfo->tableID )
              != IDE_SUCCESS);

    /* delete from grant meta */
    IDE_TEST( qdpDrop::removePriv4DropTable( aStatement, sTableInfo->tableID )
              != IDE_SUCCESS );

    // PROJ-1359
    IDE_TEST( qdnTrigger::dropTrigger4DropTable( aStatement, sTableInfo )
              != IDE_SUCCESS );

    /*  4. ���� psm, pkg, view invalid */
    /* PROJ-2197 PSM Renewal */
    // related PSM
    IDE_TEST( qcmProc::relSetInvalidProcOfRelated( aStatement,
                                                   sParseTree->userID,
                                                   (SChar *)( sParseTree->tableName.stmtText +
                                                              sParseTree->tableName.offset ),
                                                   sParseTree->tableName.size,
                                                   QS_TABLE )
              != IDE_SUCCESS );

    // PROJ-1073 Package
    IDE_TEST( qcmPkg::relSetInvalidPkgOfRelated( aStatement,
                                                 sParseTree->userID,
                                                 (SChar *)( sParseTree->tableName.stmtText +
                                                            sParseTree->tableName.offset ),
                                                 sParseTree->tableName.size,
                                                 QS_TABLE )
              != IDE_SUCCESS );

    // related VIEW
    IDE_TEST( qcmView::setInvalidViewOfRelated( aStatement,
                                                sParseTree->userID,
                                                (SChar *)( sParseTree->tableName.stmtText +
                                                           sParseTree->tableName.offset ),
                                                sParseTree->tableName.size,
                                                QS_TABLE )
              != IDE_SUCCESS );

    /*  5. constraint/index ���� function/procedure ���� ���� */
    /* BUG-35445 Check Constraint, Function-Based Index���� ��� ���� Function�� ����/���� ���� */
    IDE_TEST( qcmProc::relRemoveRelatedToConstraintByTableID(
                    aStatement,
                    sTableInfo->tableID )
              != IDE_SUCCESS );

    IDE_TEST( qcmProc::relRemoveRelatedToIndexByTableID(
                    aStatement,
                    sTableInfo->tableID )
              != IDE_SUCCESS );

    // BUG-21387 COMMENT
    IDE_TEST( qdbComment::deleteCommentTable( aStatement,
                                              sParseTree->tableInfo->tableOwnerName,
                                              sParseTree->tableInfo->name )
              != IDE_SUCCESS);

    // PROJ-2223 audit
    IDE_TEST( qcmAudit::deleteObject( aStatement,
                                      sParseTree->tableInfo->tableOwnerID,
                                      sParseTree->tableInfo->name )
              != IDE_SUCCESS );

    /*  6. smi::droptable */
    IDE_TEST( smiTable::dropTable( QC_SMI_STMT( aStatement ),
                                   sTableInfo->tableHandle,
                                   SMI_TBSLV_DROP_TBS )
              != IDE_SUCCESS);

    /* 7. ��Ƽ��(������ ���̺�) ��Ÿ ĳ�� �籸�� */
    for ( sDisjoin = sParseTree->disjoinTable, i = 0;
          sDisjoin != NULL;
          sDisjoin = sDisjoin->next, i++ )
    {
        sTableID = sDisjoin->newTableID;

        IDE_TEST( smiTable::touchTable( QC_SMI_STMT( aStatement ),
                                        sDisjoin->oldPartInfo->tableHandle,
                                        SMI_TBSLV_DROP_TBS )
                  != IDE_SUCCESS );

        IDE_TEST( qcm::makeAndSetQcmTableInfo(
                      QC_SMI_STMT( aStatement ), sTableID, sDisjoin->oldPartOID )
                  != IDE_SUCCESS );

        IDE_TEST( smiGetTableTempInfo(
                      smiGetTable( sDisjoin->oldPartOID ),
                      (void**)&sNewTableInfoArr[i] )
                  != IDE_SUCCESS );

        IDU_FIT_POINT_RAISE( "qdbDisjoin::executeDisjoinTable::makeAndSetQcmTableInfo::fail",
                             ERR_FIT_TEST );
    }

    /////////////////////////////////////////////////////////////////////////
    // ���� DISJOIN TABLE �������� Ʈ���� �ִ� ���̺��� ������� �����Ƿ�
    // ������� �ʴ´�.
    // IDE_TEST( qdnTrigger::freeTriggerCaches4DropTable( sTableInfo )
    //           != IDE_SUCCESS );
    /////////////////////////////////////////////////////////////////////////

    /* (��) ��Ƽ�� ��Ÿ ĳ�� ���� */
    (void)qcmPartition::destroyQcmPartitionInfoList( sOldPartInfoList );

    /* 8. ���̺� ��Ÿ ĳ������ ���� */
    (void)qcm::destroyQcmTableInfo( sTableInfo );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_MEMORY_ALLOCATION )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_MEMORY_ALLOCATION ) );
    }
    IDE_EXCEPTION( ERR_FIT_TEST )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qdbDisjoin::executeDisjoinTable",
                                  "FIT test" ) );
    }
    IDE_EXCEPTION_END;

    // ���� �� ���� ���� table info�� �����Ѵ�.
    if ( sNewTableInfoArr != NULL )
    {
        for ( i = 0; i < sPartitionCount; i++ )
        {
            (void)qcm::destroyQcmTableInfo( sNewTableInfoArr[i] );
        }
    }
    else
    {
        /* Nothing to do */
    }

    // on fail, must restore old table info.
    qcmPartition::restoreTempInfo( sTableInfo,
                                   sOldPartInfoList,
                                   NULL );

    return IDE_FAILURE;
}

IDE_RC qdbDisjoin::copyTableSpec( qcStatement     * aStatement,
                                  qdDisjoinTable  * aDisjoin,
                                  SChar           * aTBSName,
                                  UInt              aOldTableID )
{
/***********************************************************************
 *
 *  Description : PROJ-1810 Partition Exchange
 *                ���� ���̺��� table ��Ÿ ������ �����Ͽ�
 *                ���� �����Ǵ� ���̺� �ϳ��� table spec�� ��Ÿ�� �߰��Ѵ�.
 *
 *  Implementation :
 *      SYS_TABLES_���� ��Ƽ�ǵ� ���̺��� TABLE ID�� SELECT�� ��
 *      TABLE_ID, TABLE_OID, TABLE_NAME � �����ؼ� SYS_TABLES_�� INSERT.
 *
 *      TABLE_ID, TABLE_NAME�� ���� �������� ���� ���� ����ϰ�
 *      (table ID�� execution �������� ����, table name�� ����ڰ� ��� or �ڵ� ����)
 *
 *      TABLE_OID�� ��Ƽ���� OID�� ���.
 *
 *      TABLE_TYPE, IS_PARTITIONED, TEMPORARY, HIDDEN, PARALLEL_DEGREE�� ������ ���� ����Ѵ�.
 *      ACCESS�� ���� USABLE, SHARD_FLAG�� ������ ���� ����Ѵ�(TASK-7307)
 *      LAST_DDL_TIME�� SYSDATE�� �Է��Ѵ�.
 *      �� �ܿ��� ��Ƽ�ǵ� ���̺��� ������ �״�� �����Ѵ�.
 *
 ***********************************************************************/

    SChar       sSqlStr[ QD_MAX_SQL_LENGTH + 1 ];
    vSLong      sRowCnt = 0;
    SChar       sTableName[ QC_MAX_OBJECT_NAME_LEN + 1 ];

    QC_STR_COPY( sTableName, aDisjoin->newTableName );

    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH + 1,
                     "INSERT INTO SYS_TABLES_ "
                         "SELECT USER_ID, "
                         "INTEGER'%"ID_INT32_FMT"', "       /* TABLE_ID (new) */
                         "BIGINT'%"ID_INT64_FMT"', "        /* TABLE_OID(partition OID) */
                         "COLUMN_COUNT, "
                         "VARCHAR'%s', "                    /* TABLE_NAME(new) */
                         "CHAR'T', "                        /* TABLE_TYPE */
                         "INTEGER'0', "                     /* REPLICATION_COUNT */
                         "INTEGER'0', "                     /* REPLICATION_RECOVERY_COUNT */
                         "MAXROW, "
                         "INTEGER'%"ID_INT32_FMT"', "       /* TBS_ID(partition) */
                         "VARCHAR'%s', "                    /* TBS_NAME(partition) */
                         "PCTFREE, "
                         "PCTUSED, "
                         "INIT_TRANS, "
                         "MAX_TRANS, "
                         "INITEXTENTS, "
                         "NEXTEXTENTS, "
                         "MINEXTENTS, "
                         "MAXEXTENTS, "
                         "CHAR'F', "                        /* IS_PARTITIONED */
                         "CHAR'N', "                        /* TEMPORARY */
                         "CHAR'N', "                        /* HIDDEN */
                         "CHAR'W', "                        /* ACCESS */
                         "INTEGER'1', "                     /* PARALLEL_DEGREE */
                         "CHAR'Y', "                        /* TASK-7307 USABLE */
                         "INTEGER'0', "                     /* TASK-7307 SHARD_FLAG */
                         "CREATED, "
                         "SYSDATE "                         /* LAST_DDL_TIME */
                         "FROM SYS_TABLES_ "
                         "WHERE TABLE_ID=INTEGER'%"ID_INT32_FMT"' ", /* TABLE_ID(old) */
                     aDisjoin->newTableID,             /* TABLE_ID(new) */
                     aDisjoin->oldPartOID,             /* TABLE_OID(partition OID) */
                     sTableName,                       /* TABLE_NAME(new) */
                     (UInt)aDisjoin->oldPartInfo->TBSID,     /* TBS_ID(partition) */
                     aTBSName,                         /* TBS Name(partition) */
                     aOldTableID );                    /* TABLE_ID(old) */

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

IDE_RC qdbDisjoin::copyColumnSpec( qcStatement     * aStatement,
                                   UInt              aUserID,
                                   UInt              aOldTableID,
                                   UInt              aNewTableID,
                                   UInt              aOldPartID,
                                   UInt              aOldColumnID,
                                   UInt              aNewColumnID,
                                   UInt              aFlag )
{
/***********************************************************************
 *
 *  Description : PROJ-1810 Partition Exchange
 *                ���� ���̺��� column, lob column ��Ÿ ������ �����Ͽ�
 *                ���� �����Ǵ� ���̺� �ϳ��� column spec, lob spec�� ��Ÿ�� �߰��Ѵ�.
 *
 *  Implementation :
 *      SYS_COLUMNS_���� ��Ƽ�ǵ� ���̺� Į���� COLUMN_ID�� SELECT�� ��
 *      COLUMN_ID, TABLE_ID�� �����ؼ� SYS_COLUMNS_�� INSERT.
 *
 ***********************************************************************/

    SChar               sSqlStr[ QD_MAX_SQL_LENGTH + 1 ];
    vSLong              sRowCnt = 0;

    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH + 1,
                     "INSERT INTO SYS_COLUMNS_ "
                         "SELECT INTEGER'%"ID_INT32_FMT"', " /* COLUMN_ID(new) */
                         "DATA_TYPE, "
                         "LANG_ID, "
                         "OFFSET, "
                         "SIZE, "
                         "USER_ID, "
                         "INTEGER'%"ID_INT32_FMT"', "       /* TABLE_ID(new) */
                         "PRECISION, "
                         "SCALE, "
                         "COLUMN_ORDER, "
                         "COLUMN_NAME, "
                         "IS_NULLABLE, "
                         "DEFAULT_VAL, "
                         "STORE_TYPE, "
                         "IN_ROW_SIZE, "
                         "REPL_CONDITION, "
                         "IS_HIDDEN, "
                         "IS_KEY_PRESERVED "
                         "FROM SYS_COLUMNS_ "
                         "WHERE COLUMN_ID=INTEGER'%"ID_INT32_FMT"' ",   /* COLUMN_ID(old) */
                     aNewColumnID,                          /* column id(new) */
                     aNewTableID,                           /* table id(new) */
                     aOldColumnID );                        /* column id(old) */

    IDE_TEST( qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                 sSqlStr,
                                 & sRowCnt ) != IDE_SUCCESS );

    IDE_TEST_RAISE( sRowCnt != 1, ERR_META_CRASH );

    if ( ( aFlag & MTD_COLUMN_TYPE_MASK ) == MTD_COLUMN_TYPE_LOB )
    {
        /* LOB column�� ��� SYS_LOBS_�� �߰��� ����Ѵ�. */
        idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH + 1,
                         "INSERT INTO SYS_LOBS_ "
                             "SELECT A.USER_ID, "
                             "INTEGER'%"ID_INT32_FMT"', "       /* TABLE_ID(new) */
                             "INTEGER'%"ID_INT32_FMT"', "       /* COLUMN_ID(new) */
                             "B.TBS_ID, "
                             "B.LOGGING, "
                             "B.BUFFER, "
                             "A.IS_DEFAULT_TBS "
                             "FROM SYS_LOBS_ A, SYS_PART_LOBS_ B "
                             "WHERE A.COLUMN_ID=INTEGER'%"ID_INT32_FMT"' AND "   /* COLUMN_ID(old) */
                             "A.COLUMN_ID=B.COLUMN_ID AND "
                             "B.PARTITION_ID=INTEGER'%"ID_INT32_FMT"' AND " /* part ID(old) */
                             "B.USER_ID=INTEGER'%"ID_INT32_FMT"' AND " /* user ID */
                             "B.TABLE_ID=INTEGER'%"ID_INT32_FMT"' ", /* table ID(old) */
                         aNewTableID,                           /* table ID(new) */
                         aNewColumnID,                          /* column ID(new) */
                         aOldColumnID,                          /* column ID(old) */
                         aOldPartID,                            /* partition ID(old) */
                         aUserID,                               /* user ID */
                         aOldTableID );                         /* table ID(old) */

        IDE_TEST( qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                     sSqlStr,
                                     & sRowCnt ) != IDE_SUCCESS );

        IDE_TEST_RAISE( sRowCnt != 1, ERR_META_CRASH );
    }
    else
    {
        /* Nothing To Do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_META_CRASH )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDB_META_CRASH ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdbDisjoin::copyConstraintSpec( qcStatement     * aStatement,
                                       UInt              aOldTableID,
                                       UInt              aNewTableID )
{
/***********************************************************************
 *
 *  Description : PROJ-1810 Partition Exchange
 *                ���� ���̺��� constraint ��Ÿ ������ �����Ͽ�
 *                ���� �����Ǵ� ���̺���� constraint spec�� ��Ÿ�� �߰��Ѵ�.
 *
 *  Implementation :
 *      SYS_CONSTRAINTS_ ��Ÿ�� search�ؼ� �ʿ��� �������� ���� �Ѵ�.
 *      ���� ���Ӱ� ������ constraint ID�� ����� ���ο� constraint �̸��� �����.
 *      �̷� �۾����� ���� ��Ÿ���� ������ constraint ID, table ID�� ã�� �����Ѵ�.
 *
 *      ���������� ���� constraint�� table ID, constraint ID�� ��Ÿ�� �� row�� select�ϰ�
 *      table ID, constraint ID, constraint name�� ������ insert�Ѵ�.
 *
 *      constraint ID, name�� �ռ� ���� ���� ����Ѵ�.
 *
 *      �߰��� SYS_CONSTRAINT_COLUMNS_ ��Ÿ�� ���������� insert.
 *
 ***********************************************************************/


    SChar                 sSqlStr[ QD_MAX_SQL_LENGTH + 1 ];
    const void          * sRow = NULL;
    smiRange              sRange;
    smiTableCursor        sCursor;
    scGRID                sRid;
    SInt                  sStage = 0;
    vSLong                sRowCnt = 0;
    mtcColumn           * sConstrConstraintIDCol = NULL;
    mtcColumn           * sConstrTableIDCol = NULL;
    mtcColumn           * sConstrTypeCol = NULL;
    mtcColumn           * sConstrColumnCountCol = NULL;
    mtdIntegerType        sConstrConstraintID = 0;
    UInt                  sConstrType = 0;
    qtcMetaRangeColumn    sRangeColumn;
    smiCursorProperties   sCursorProperty;
    qdDisjoinConstr     * sConstrInfo = NULL;
    qdDisjoinConstr     * sFirstConstrInfo = NULL;
    qdDisjoinConstr     * sLastConstrInfo = NULL;

    sCursor.initialize();

    // constraint user_id column ����
    IDE_TEST( smiGetTableColumns( gQcmConstraints,
                                  QCM_CONSTRAINTS_CONSTRAINT_ID_COL_ORDER,
                                  (const smiColumn**)&sConstrConstraintIDCol )
              != IDE_SUCCESS );
    // mtdModule ����
    IDE_TEST( mtd::moduleById( &sConstrConstraintIDCol->module,
                               sConstrConstraintIDCol->type.dataTypeId )
              != IDE_SUCCESS );

    // constraint table_id column ����
    IDE_TEST( smiGetTableColumns( gQcmConstraints,
                                  QCM_CONSTRAINTS_TABLE_ID_COL_ORDER,
                                  (const smiColumn**)&sConstrTableIDCol )
              != IDE_SUCCESS );
    // mtdModule ����
    IDE_TEST( mtd::moduleById( &sConstrTableIDCol->module,
                               sConstrTableIDCol->type.dataTypeId )
              != IDE_SUCCESS );

    // constraint type column ����
    IDE_TEST( smiGetTableColumns( gQcmConstraints,
                                  QCM_CONSTRAINTS_CONSTRAINT_TYPE_COL_ORDER,
                                  (const smiColumn**)&sConstrTypeCol )
              != IDE_SUCCESS );
    // mtdModule ����
    IDE_TEST( mtd::moduleById( &sConstrTypeCol->module,
                               sConstrTypeCol->type.dataTypeId )
              != IDE_SUCCESS );

    // constraint column count column ����
    IDE_TEST( smiGetTableColumns( gQcmConstraints,
                                  QCM_CONSTRAINTS_COLUMN_CNT_COL_ORDER,
                                  (const smiColumn**)&sConstrColumnCountCol )
              != IDE_SUCCESS );
    // mtdModule ����
    IDE_TEST( mtd::moduleById( &sConstrColumnCountCol->module,
                               sConstrColumnCountCol->type.dataTypeId )
              != IDE_SUCCESS );

    qcm::makeMetaRangeSingleColumn( &sRangeColumn,
                                    (const mtcColumn*) sConstrTableIDCol,
                                    (const void *) & aOldTableID,
                                    &sRange );

    SMI_CURSOR_PROP_INIT_FOR_META_INDEX_SCAN( &sCursorProperty, NULL );

    // cursor open
    IDE_TEST( sCursor.open( QC_SMI_STMT( aStatement ),
                            gQcmConstraints,
                            gQcmConstraintsIndex[QCM_CONSTRAINTS_TABLEID_INDEXID_IDX_ORDER],
                            smiGetRowSCN(gQcmConstraints),
                            NULL,
                            &sRange,
                            smiGetDefaultKeyRange(),
                            smiGetDefaultFilter(),
                            QCM_META_CURSOR_FLAG,
                            SMI_SELECT_CURSOR,
                            &sCursorProperty) != IDE_SUCCESS );

    sStage = 1;

    IDE_TEST( sCursor.beforeFirst() != IDE_SUCCESS );
    
    IDE_TEST( sCursor.readRow( &sRow, &sRid, SMI_FIND_NEXT ) != IDE_SUCCESS );
    
    while ( sRow != NULL )
    {
        IDE_TEST( STRUCT_ALLOC( QC_QMX_MEM( aStatement ), 
                                qdDisjoinConstr,
                                (void**)&sConstrInfo )
                  != IDE_SUCCESS );
    
        sConstrConstraintID = *(mtdIntegerType *)
                        ((UChar *)sRow + sConstrConstraintIDCol->column.offset);

        sConstrType = *((UInt *)
                        ((UChar *)sRow + sConstrTypeCol->column.offset));

        /* copy constraint column count */
        sConstrInfo->columnCount = *((UInt *)
                                     ((UChar *)sRow + sConstrColumnCountCol->column.offset));

        /* copy old constraint ID */
        sConstrInfo->oldConstrID = (UInt)sConstrConstraintID;

        /* create new constraint ID */
        IDE_TEST( qcm::getNextConstrID( aStatement, &sConstrInfo->newConstrID )
                  != IDE_SUCCESS );

        /* create new constraint name */
        if ( sConstrType == QD_NOT_NULL )
        {
            idlOS::snprintf( sConstrInfo->newConstrName,
                             QC_MAX_OBJECT_NAME_LEN + 1,
                             "%sCON_NOT_NULL_ID%"ID_INT32_FMT"",
                             QC_SYS_OBJ_NAME_HEADER,
                             sConstrInfo->newConstrID );
        }
        else if ( sConstrType == QD_NULL )
        {
            idlOS::snprintf( sConstrInfo->newConstrName,
                             QC_MAX_OBJECT_NAME_LEN + 1,
                             "%sCON_ID%"ID_INT32_FMT"",
                             QC_SYS_OBJ_NAME_HEADER,
                             sConstrInfo->newConstrID );
        }
        else if ( sConstrType == QD_TIMESTAMP )
        {
            idlOS::snprintf( sConstrInfo->newConstrName,
                             QC_MAX_OBJECT_NAME_LEN + 1,
                             "%sCON_TIMESTAMP_ID%"ID_INT32_FMT"",
                             QC_SYS_OBJ_NAME_HEADER,
                             sConstrInfo->newConstrID );
        }
        else if ( sConstrType == QD_CHECK )
        {
            idlOS::snprintf( sConstrInfo->newConstrName,
                             QC_MAX_OBJECT_NAME_LEN + 1,
                             "%sCON_CHECK_ID%"ID_INT32_FMT"",
                             QC_SYS_OBJ_NAME_HEADER,
                             sConstrInfo->newConstrID );
        }
        else
        {
            /* not null, null, timestamp, check �̿��� constraint�� ���� ����. */
            IDE_DASSERT( 0 );
        }

        if ( sFirstConstrInfo == NULL )
        {
            sConstrInfo->next = NULL;
            sFirstConstrInfo = sConstrInfo;
            sLastConstrInfo = sConstrInfo;
        }
        else
        {
            sConstrInfo->next = NULL;
            sLastConstrInfo->next = sConstrInfo;
            sLastConstrInfo = sLastConstrInfo->next;
        }

        IDE_TEST( sCursor.readRow( &sRow, &sRid, SMI_FIND_NEXT ) != IDE_SUCCESS );
    }

    sStage = 0;

    IDE_TEST( sCursor.close() != IDE_SUCCESS );

    /* ���� constraint id ���� sys_constraints_�� insert�Ѵ�. */
    for ( sConstrInfo = sFirstConstrInfo;
          sConstrInfo != NULL;
          sConstrInfo = sConstrInfo->next )
    {
        /* table_id, constraint_id, constraint_name�� �ٲ�� �Ѵ�. */
        idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH + 1,
                         "INSERT INTO SYS_CONSTRAINTS_ "
                            "SELECT USER_ID, "
                            "INTEGER'%"ID_INT32_FMT"', "            /* TABLE_ID(new) */
                            "INTEGER'%"ID_INT32_FMT"', "            /* CONSTRAINT_ID(new) */
                            "VARCHAR'%s', "                         /* CONSTRAINT_NAME(new) */
                            "CONSTRAINT_TYPE, "
                            "INDEX_ID, "
                            "COLUMN_CNT, "
                            "REFERENCED_TABLE_ID, "
                            "REFERENCED_INDEX_ID, "
                            "DELETE_RULE, "
                            "CHECK_CONDITION, "
                            "VALIDATED "
                            "FROM SYS_CONSTRAINTS_ "
                            "WHERE CONSTRAINT_ID="
                                "INTEGER'%"ID_INT32_FMT"' ", /* CONSTR_ID(old) */
                            aNewTableID,                    /* table ID(new) */
                            sConstrInfo->newConstrID,       /* constraint ID(new) */
                            sConstrInfo->newConstrName,     /* constraint name(new) */
                            sConstrInfo->oldConstrID );     /* constraint ID(old) */
    
        IDE_TEST( qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                     sSqlStr,
                                     & sRowCnt ) != IDE_SUCCESS );

        IDE_TEST_RAISE( sRowCnt != 1, ERR_META_CRASH );

        /***********************************************************************
         * constraint column ��Ÿ���� ���� �ٲٴ� column ID�� �̷��� ã�´�.
         * 
         * column ID�� SMI_COLUMN_ID_MAXIMUM���� mod�ϸ� column order�� ���´�.
         * new table ID�� SMI_COLUMN_ID_MAXIMUM�� ���ϸ�
         * new table�� ù��° �÷��� ID�� ���´�.
         * �� ���� ���ϸ� new table���� �����Ǵ� constraint�� column ID�� ���´�.
         *
         * constraint ID: ���� SYS_CONSTRAINTS_ copy�� �� ����� list ��Ȱ���Ѵ�.
         ***********************************************************************/

        idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH + 1,
                         "INSERT INTO SYS_CONSTRAINT_COLUMNS_ "
                            "SELECT USER_ID, "
                            "INTEGER'%"ID_INT32_FMT"', "            /* TABLE_ID(new) */
                            "INTEGER'%"ID_INT32_FMT"', "            /* CONSTRAINT_ID(new) */
                            "CONSTRAINT_COL_ORDER, "
                            "MOD(COLUMN_ID,INTEGER'%"ID_INT32_FMT"')" /* SMI COL ID_MAX */
                            "+INTEGER'%"ID_INT32_FMT"' "           /* new col ID(first) */
                            "FROM SYS_CONSTRAINT_COLUMNS_ "
                            "WHERE CONSTRAINT_ID=INTEGER'%"ID_INT32_FMT"' AND " /* CONST_ID(old) */
                            "TABLE_ID=INTEGER'%"ID_INT32_FMT"' ",    /* TABLE_ID(old) */
                            aNewTableID,                        /* table ID(new) */
                            sConstrInfo->newConstrID,       /* constraint ID(new) */
                            SMI_COLUMN_ID_MAXIMUM,
                            aNewTableID * SMI_COLUMN_ID_MAXIMUM,  /* new table's 1st col. ID */
                            sConstrInfo->oldConstrID,       /* constraint ID(old) */
                            aOldTableID );                  /* table ID(old) */

        IDE_TEST( qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                     sSqlStr,
                                     & sRowCnt ) != IDE_SUCCESS );
    
        IDE_TEST_RAISE( sRowCnt != sConstrInfo->columnCount, ERR_META_CRASH );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_META_CRASH )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDB_META_CRASH ) );
    }
    IDE_EXCEPTION_END;

    switch ( sStage )
    {
        case 1 :
            sCursor.close();
            break;
        default :
            IDE_DASSERT( 0 );
    }

    return IDE_FAILURE;
}

IDE_RC qdbDisjoin::checkPartitionExistByName( qcStatement          * aStatement,
                                              qcmPartitionInfoList * aPartInfoList,
                                              qdDisjoinTable       * aDisjoin )
{
/***********************************************************************
 *
 *  Description : PROJ-1810 Partition Exchange
 *                ���̺� ID�� ��� ��Ƽ���� �����´�.
 *
 *  Implementation :
 *      DISJOIN SQL statement���� ����ڰ� ��Ƽ���� ������ ������
 *      part info list�� ��Ƽ�� info�� ����Ǵ� ������ �ٸ� �� �ִ�.
 *      ������ �� ���̺� �̸��� info�� ��ġ�����ֱ� ����
 *      part info�� �̸��� parse tree�� ����ڰ� �� ���� ���ؼ�
 *      ��ġ�ϴ� ��Ƽ�� info, ID, OID�� disjoinTable�� �޾��ش�.
 *
 ***********************************************************************/

    qcuSqlSourceInfo          sqlInfo;
    qcmTableInfo            * sPartInfo = NULL;
    qcmPartitionInfoList    * sPartInfoList = NULL;
    qdDisjoinTable          * sDisjoin = NULL;
    idBool                    sFound = ID_FALSE;

    /* ����ڰ� �Է��� ��Ƽ�� �̸����� ã�Ƽ� */
    /* qdDisjoinTable ����ü�� ��Ƽ�� ID, OID, info�� �޾� ���´�. */
    for ( sDisjoin = aDisjoin;
          sDisjoin != NULL;
          sDisjoin = sDisjoin->next )
    {
        sFound = ID_FALSE;
        /* �־��� �̸��� ��Ƽ���� �ִ��� ã�ƺ���. */
        for ( sPartInfoList = aPartInfoList;
              sPartInfoList != NULL;
              sPartInfoList = sPartInfoList->next )
        {
            sPartInfo = sPartInfoList->partitionInfo;

            // BUG-47599 default partition�� ���� ��� ����
            IDE_TEST_RAISE ( idlOS::strlen( sPartInfo->name ) == QD_EMPTY_PARTITION_NAME_SIZE,
                             ERR_UNSUPPORTED_SYNTAX );

            /* �̸����� ��Ƽ�� �� */
            if ( idlOS::strMatch( sPartInfo->name,
                                  idlOS::strlen( sPartInfo->name ),
                                  (SChar*)( sDisjoin->oldPartName.stmtText + sDisjoin->oldPartName.offset ),
                                  (UInt)sDisjoin->oldPartName.size ) == 0 )
            {
                /* ã�Ƴ��� ��Ƽ�� ID, ��Ƽ�� OID, ��Ƽ�� info�� �����Ѵ�. */
                sDisjoin->oldPartID = sPartInfo->partitionID;
                sDisjoin->oldPartOID = sPartInfo->tableOID;
                sDisjoin->oldPartInfo = sPartInfo;
                sFound = ID_TRUE;
                break;
            }
            else
            {
                /* Nothing To Do */
            }
        }

        /* ã�Ƴ��� ���ϸ� error */
        if ( sFound == ID_FALSE )
        {
            sqlInfo.setSourceInfo( aStatement,
                                   &(sDisjoin->oldPartName) );
            IDE_RAISE( ERR_NOT_EXIST_TABLE_PARTITION );
        }
        else
        {
            /* Nothing To Do */
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NOT_EXIST_TABLE_PARTITION )
    {
        (void)sqlInfo.init( aStatement->qmeMem );
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QCM_NOT_EXIST_TABLE_PARTITION,
                                  sqlInfo.getErrMessage() ) );
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION( ERR_UNSUPPORTED_SYNTAX )
    {
        IDE_SET( ideSetErrorCode(qpERR_ABORT_QCP_NOT_SUPPORTED_SYNTAX) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdbDisjoin::modifyColumnID( qcStatement     * aStatement,
                                   qcmColumn       * aColumn,
                                   UInt              aColCount,
                                   UInt              aNewColumnID,
                                   void            * aHandle )  /* table or partition */
{
/***********************************************************************
 *
 *  Description : PROJ-1810 Partition Exchange
 *                �� column ID�� ���̺�� �ٲ�� ��Ƽ���� ����� �ݿ��Ѵ�.
 *                executeDisjoin, executeJoin���� ȣ��ȴ�.
 *
 *  Implementation :
 *      �� column ID�� mtcColumn�� ����� smiColumnList ����
 *      ������ smiColumnList�� aHandle�� modifyTableInfo
 *
 ***********************************************************************/

    qcmColumn        * sColumn = NULL;
    mtcColumn        * sNewMtcColumns = NULL;
    smiColumnList    * sSmiColumnList = NULL;
    ULong              sAllocSize = 0;
    UInt               i = 0;

    sAllocSize = (ULong)aColCount * ID_SIZEOF(mtcColumn);
    IDU_FIT_POINT( "qdbDisjoin::modifyColumnID::alloc::sNewMtcColumns",
                    idERR_ABORT_InsufficientMemory );
    IDE_TEST( aStatement->qmxMem->alloc( sAllocSize,
                                         (void**)&sNewMtcColumns )
             != IDE_SUCCESS);

    sAllocSize = (ULong)aColCount * ID_SIZEOF(smiColumnList);
    IDU_FIT_POINT( "qdbDisjoin::modifyColumnID::alloc::sSmiColumnList",
                    idERR_ABORT_InsufficientMemory );
    IDE_TEST( aStatement->qmxMem->alloc( sAllocSize,
                                         (void**)&sSmiColumnList )
              != IDE_SUCCESS );

    /* partition column���� mtcColumn�� �����ϰ� column ID�� �ٲ��ش�. */
    /* mtcColumn���� smiColumnList�� �����. */
    for ( sColumn = aColumn, i = 0;
          sColumn != NULL;
          sColumn = sColumn->next, i++ )
    {
        idlOS::memcpy( &sNewMtcColumns[i], sColumn->basicInfo, ID_SIZEOF(mtcColumn) );
        sNewMtcColumns[i].column.id = aNewColumnID + i;
        sSmiColumnList[i].column = (const smiColumn*)(&sNewMtcColumns[i]);

        if ( i == aColCount - 1 )
        {
            sSmiColumnList[i].next = NULL;
        }
        else
        {
            sSmiColumnList[i].next = &sSmiColumnList[i + 1];
        }
    }

    IDE_DASSERT( sSmiColumnList != NULL );

    /* table header���� column�� ID�� ����. */
    IDE_TEST( smiTable::modifyTableInfo( QC_SMI_STMT( aStatement ),
                                         aHandle,
                                         sSmiColumnList,
                                         ID_SIZEOF(mtcColumn),
                                         NULL,
                                         0,
                                         SMI_TABLE_FLAG_UNCHANGE,
                                         SMI_TBSLV_DROP_TBS )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
