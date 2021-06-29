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
 * $Id: qdn.cpp 90824 2021-05-13 05:35:21Z minku.kang $
 **********************************************************************/

#include <idl.h>
#include <ide.h>
#include <qdx.h>
#include <qcm.h>
#include <qcmCache.h>
#include <qcmUser.h>
#include <qcmView.h>
#include <qcmTableSpace.h>
#include <qcuTemporaryObj.h>
#include <qcuSqlSourceInfo.h>
#include <qcg.h>
#include <qdbCommon.h>
#include <qdbAlter.h>
#include <qdd.h>
#include <qcmProc.h>
#include <qdpPrivilege.h>
#include <qdtCommon.h>
#include <qdn.h>
#include <qdnForeignKey.h>
#include <qmv.h>
#include <qmvQuerySet.h>
#include <qmsDefaultExpr.h>
#include <qdnCheck.h>
#include <mtdTypes.h>
#include <smErrorCode.h>
#include <smiTableSpace.h>
#include <qdbComment.h>
#include <qdnTrigger.h>
#include <qdpRole.h>
#include <qmoPartition.h>
#include <sdi.h>

/***********************************************************************
 * VALIDATE
 **********************************************************************/
// Validation in Parser
//  - check duplicated column name in specified column list
//              (definition list, unique key column, primary key column)
//  - check duplicated constraint name in specified constraint list
//  - checking duplicate or conflicting NULL and/or NOT NULL specifications

IDE_RC qdn::validateAddConstr(qcStatement * aStatement)
{
#define IDE_FN "qdn::validateAddConstr"

    // To Fix PR-10909
    qdTableParseTree    * sParseTree;
    UInt                  sUserID;
    UInt                  sUniqueKeyCnt;
    UInt                  sFlag = 0;
    qdConstraintSpec    * sConstr;
    qcmColumn           * sColumn;
    qcmColumn           * sColumnInfo;
    qmsPartitionRef     * sPartitionRef             = NULL;
    qcuSqlSourceInfo      sqlInfo;
    idBool                sIsPartitioned            = ID_FALSE;
    idBool                sNeedToMakePartitionTuple = ID_FALSE;

    sParseTree = (qdTableParseTree *)aStatement->myPlan->parseTree;

    // check table exist.
    IDE_TEST( qdbCommon::checkTableInfo( aStatement,
                                         sParseTree->userName,
                                         sParseTree->tableName,
                                         &sUserID,
                                         &(sParseTree->tableInfo),
                                         &(sParseTree->tableHandle),
                                         &(sParseTree->tableSCN))
              != IDE_SUCCESS);

    /* BUG-48290 shard object�� ���� DDL ���� */
    IDE_TEST( sdi::checkShardObjectForDDL( aStatement, SDI_DDL_TYPE_TABLE ) != IDE_SUCCESS );

    IDE_TEST( qcm::lockTableForDDLValidation(aStatement,
                                             sParseTree->tableHandle,
                                             sParseTree->tableSCN)
              != IDE_SUCCESS );

    IDE_TEST_RAISE(sUserID == QC_SYSTEM_USER_ID, ERR_NOT_ALTER_META);

    // PR-13725
    // CHECK OPERATABLE
    IDE_TEST( qdbAlter::checkOperatable( aStatement,
                                         sParseTree->tableInfo )
              != IDE_SUCCESS );

    // To Fix PR-10909
    // validateConstraints()�� ���Ͽ� ������ �������ش�.
    sParseTree->columns = sParseTree->tableInfo->columns;
    
    // check grant
    IDE_TEST( qdpRole::checkDDLAlterTablePriv( aStatement,
                                               sParseTree->tableInfo )
              != IDE_SUCCESS );
    
    // PROJ-1502 PARTITIONED DISK TABLE
    if( sParseTree->tableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
    {
        // ��� ��Ƽ�ǿ� LOCK(IS)
        // ��Ƽ�� ����Ʈ�� �Ľ�Ʈ���� �޾Ƴ��´�.
        IDE_TEST( qdbCommon::checkAndSetAllPartitionInfo(
                            aStatement,
                            sParseTree->tableInfo->tableID,
                            & (sParseTree->partTable->partInfoList) )
                  != IDE_SUCCESS );

        /* PROJ-2464 hybrid partitioned table ���� */
        sIsPartitioned = ID_TRUE;
    }

    /* PROJ-1107 Check Constraint ���� */
    /* check existence of table and get table META Info */
    sFlag &= ~QMV_PERFORMANCE_VIEW_CREATION_MASK;
    sFlag |=  QMV_PERFORMANCE_VIEW_CREATION_FALSE;
    sFlag &= ~QMV_VIEW_CREATION_MASK;
    sFlag |=  QMV_VIEW_CREATION_FALSE;

    /* BUG-17409 */
    sParseTree->from->tableRef->flag &= ~QMS_TABLE_REF_SCAN_FOR_NON_SELECT_MASK;
    sParseTree->from->tableRef->flag |=  QMS_TABLE_REF_SCAN_FOR_NON_SELECT_TRUE;

    IDE_TEST( qmvQuerySet::validateQmsTableRef( aStatement,
                                                NULL,
                                                sParseTree->from->tableRef,
                                                sFlag,
                                                MTC_COLUMN_NOTNULL_TRUE ) /* PR-13597 */
              != IDE_SUCCESS );

    // validation of constraints
    IDE_TEST(validateConstraints( aStatement,
                                  sParseTree->tableInfo,
                                  sParseTree->tableInfo->tableOwnerID,
                                  sParseTree->tableInfo->TBSID,
                                  sParseTree->tableInfo->TBSType,
                                  sParseTree->constraints,
                                  QDN_ON_ADD_CONSTRAINT,
                                  &sUniqueKeyCnt)
             != IDE_SUCCESS);

    /* PROJ-2464 hybrid partitioned table ����
     *  - qdn::validateConstraints, Tablespace Vaildate ���Ŀ� ȣ���Ѵ�.
     */
    IDE_TEST( qdbCommon::validateConstraintRestriction( aStatement,
                                                        sParseTree )
              != IDE_SUCCESS );

    /* PROJ-2464 hybrid partitioned table ����
     *  Check Constraint���� qtc::calculate()�� ȣ���ϱ� ����, Partition Tuple�� �����.
     *      Partition Tuple�� ����� ����, Partition Info�� �ʿ��ϴ�.
     *      Partition Info�� Tuple ID�� �����ϱ� ����, qmsPartitionRef List�� �����.
     */
    if ( sIsPartitioned == ID_TRUE )
    {
        for ( sConstr = sParseTree->constraints;
              sConstr != NULL;
              sConstr = sConstr->next )
        {
            if ( sConstr->constrType == QD_CHECK )
            {
                if ( sConstr->constrState == NULL )
                {
                    sNeedToMakePartitionTuple = ID_TRUE;
                    break;
                }
                else
                {
                    if ( sConstr->constrState->validate == ID_TRUE )
                    {
                        sNeedToMakePartitionTuple = ID_TRUE;
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
    else
    {
        /* Nothing to do */
    }

    if ( sNeedToMakePartitionTuple == ID_TRUE )
    {
        IDE_TEST( qmoPartition::makePartitions( aStatement,
                                                sParseTree->from->tableRef )
                  != IDE_SUCCESS );

        /* PROJ-2464 hybrid partitioned table ���� */
        IDE_TEST( qcmPartition::makePartitionSummary( aStatement, sParseTree->from->tableRef )
                  != IDE_SUCCESS );

        for ( sPartitionRef = sParseTree->from->tableRef->partitionRef;
              sPartitionRef != NULL;
              sPartitionRef = sPartitionRef->next )
        {
            IDE_TEST( qtc::nextTable( &(sPartitionRef->table),
                                      aStatement,
                                      sPartitionRef->partitionInfo,
                                      QCM_TABLE_TYPE_IS_DISK( sPartitionRef->partitionInfo->tableFlag ),
                                      MTC_COLUMN_NOTNULL_TRUE ) // PR-13597
                      != IDE_SUCCESS );
        }
    }
    else
    {
        /* Nothing to do */
    }

    /* PROJ-1090 Function-based Index */
    for ( sConstr = sParseTree->constraints;
          sConstr != NULL;
          sConstr = sConstr->next )
    {
        for ( sColumn = sConstr->constraintColumns;
              sColumn != NULL;
              sColumn = sColumn->next )
        {
            IDE_TEST( qcmCache::getColumn( aStatement,
                                           sParseTree->tableInfo,
                                           sColumn->namePos,
                                           &sColumnInfo )
                      != IDE_SUCCESS );

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
    }

    // BUG-10889 ��� ��쿡 ���̺� �����̽� ID�� �ʱ�ȭ���־� UMR ���� �ʵ���
    sParseTree->TBSAttr.mID = sParseTree->tableInfo->TBSID;

    /* To Fix BUG-13528
    // key size limit �˻�
    IDE_TEST( qdbCommon::validateKeySizeLimit(
    aStatement,
    sParseTree->tableInfo,
    sParseTree->constraints) != IDE_SUCCESS );
    */

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
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION(ERR_NOT_ALTER_META);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDD_NO_DROP_META_TABLE));
    }
    IDE_EXCEPTION( ERR_CANNOT_USE_HIDDEN_COLUMN );
    {
        (void)sqlInfo.init( aStatement->qmeMem );
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDB_CANNOT_USE_HIDDEN_COLUMN,
                                  sqlInfo.getErrMessage() ) );
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qdn::validateDropConstr(qcStatement * aStatement)
{
/***********************************************************************
 *
 * Description :
 *    ALTER TABLE ... DROP CONSTAINT �� validation  ����
 *
 * Implementation :
 *    1. �����ϴ� ���̺����� �˻�
 *    2. AlterTable �� ������ �� �ִ� ������ �ִ��� �˻�
 *    3. ����ȭ�� �ɷ� �ִ� ���̺��̸� ���� ��ȯ
 *    4. drop �ϰ��� �ϴ� constraint �� �����ϴ��� üũ
 *    5. check existence of referencing constraint.
 *    6. drop �ϰ��� �ϴ� constraint �� not null constraint ��,
 *       �� �÷��� primary key �÷��� �ƴ��� üũ
 *
 ***********************************************************************/

#define IDE_FN "qdn::validateDropConstr"

    // To Fix PR-10909
    qdTableParseTree     * sParseTree;
    UInt                   sConstraintID;
    SChar                  sConstraintName[ QC_MAX_OBJECT_NAME_LEN + 1 ];
    UInt                   i;
    qcmUnique            * sUniqueConstr;
    qcmNotNull           * sNotNullConstr;
    qcmRefChildInfo      * sChildInfo;  // BUG-28049
    UInt                   sUserID;
    qcmIndex             * sIndex = NULL;

    sParseTree = (qdTableParseTree *)aStatement->myPlan->parseTree;

    IDE_TEST( qdbCommon::checkTableInfo( aStatement,
                                         sParseTree->userName,
                                         sParseTree->tableName,
                                         &sUserID,
                                         &(sParseTree->tableInfo),
                                         &(sParseTree->tableHandle),
                                         &(sParseTree->tableSCN))
              != IDE_SUCCESS);

    /* BUG-48290 shard object�� ���� DDL ���� */
    IDE_TEST( sdi::checkShardObjectForDDL( aStatement, SDI_DDL_TYPE_TABLE ) != IDE_SUCCESS );

    IDE_TEST( qcm::lockTableForDDLValidation(aStatement,
                                             sParseTree->tableHandle,
                                             sParseTree->tableSCN)
              != IDE_SUCCESS );

    IDE_TEST_RAISE(sUserID == QC_SYSTEM_USER_ID, ERR_NOT_ALTER_META);

    // PR-13725
    // CHECK OPERATABLE
    IDE_TEST( qdbAlter::checkOperatable( aStatement,
                                         sParseTree->tableInfo )
              != IDE_SUCCESS );

    // To Fix PR-10909
    // �̹� �����ϴ� ���̺��� columns ������ ������.
    sParseTree->columns = sParseTree->tableInfo->columns;
    
    // check grant
    IDE_TEST( qdpRole::checkDDLAlterTablePriv( aStatement,
                                               sParseTree->tableInfo )
              != IDE_SUCCESS );

    // check existence of constraint
    QC_STR_COPY( sConstraintName, sParseTree->constraints->constrName );

    sConstraintID =
        qcmCache::getConstraintIDByName(sParseTree->tableInfo,
                                        sConstraintName,
                                        &sIndex);

    IDE_TEST_RAISE(sConstraintID == 0, ERR_NOT_EXIST_CONSTRAINT_NAME);

    // check existence of referencing constraint.
    for (i = 0; i < sParseTree->tableInfo->uniqueKeyCount; i++)
    {
        sUniqueConstr = &(sParseTree->tableInfo->uniqueKeys[i]);

        if (sUniqueConstr->constraintID == sConstraintID)
        {
            IDE_TEST(qcm::getChildKeys(aStatement,
                                       sUniqueConstr->constraintIndex,
                                       sParseTree->tableInfo,
                                       &sChildInfo)
                     != IDE_SUCCESS);

            // fix BUG-19735 foreign key�� ���� ��� drop �ź�
            IDE_TEST_RAISE( sChildInfo != NULL,
                            ERR_ABORT_REFERENTIAL_CONSTRAINT_EXIST );
        }
    }
    // if this constraint is not null constraint, we have to check
    // the column is a primary key constraint column.
    for (i = 0; i < sParseTree->tableInfo->notNullCount; i++)
    {
        sNotNullConstr = &(sParseTree->tableInfo->notNulls[i]);
        if (sNotNullConstr->constraintID == sConstraintID)
        {
            if (sParseTree->tableInfo->primaryKey != NULL)
            {
                IDE_TEST_RAISE(intersectColumn(
                                   sNotNullConstr->constraintColumn,
                                   sNotNullConstr->constraintColumnCount,
                                   (UInt*) smiGetIndexColumns(
                                       sParseTree->tableInfo->primaryKey->indexHandle),
                                   sParseTree->tableInfo->primaryKey->keyColCount) == ID_TRUE,
                               ERR_NOT_ALLOWED_DROP_NOT_NULL);
            }
        }
    }

    // check TIMESTAMP constraint
    if (sParseTree->tableInfo->timestamp != NULL)
    {
        IDE_TEST_RAISE(
            sParseTree->tableInfo->timestamp->constraintID == sConstraintID,
            ERR_CANNOT_DROP_TIMESTAMP_CONSTRAINT);
    }

    // PROJ-1502 PARTITIONED DISK TABLE
    if( sParseTree->tableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
    {
        // ��� ��Ƽ�ǿ� LOCK(IS)
        // ��Ƽ�� ����Ʈ�� �Ľ�Ʈ���� �޾Ƴ��´�.
        IDE_TEST( qdbCommon::checkAndSetAllPartitionInfo(
                            aStatement,
                            sParseTree->tableInfo->tableID,
                            & (sParseTree->partTable->partInfoList) )
                  != IDE_SUCCESS );
        
        // PROJ-1624 global non-partitioned index
        if ( sIndex != NULL )
        {
            if ( sIndex->indexPartitionType == QCM_NONE_PARTITIONED_INDEX )
            {
                IDE_TEST( qdx::makeAndLockIndexTable( aStatement,
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
    }
    else
    {
        // Nothing to do.
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

    return IDE_SUCCESS;
    
    IDE_EXCEPTION(ERR_ABORT_REFERENTIAL_CONSTRAINT_EXIST);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDB_REFERENTIAL_CONSTRAINT_EXIST));
    }
    IDE_EXCEPTION(ERR_NOT_ALLOWED_DROP_NOT_NULL);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDB_NOT_ALLOWED_DROP_NOT_NULL));
    }
    IDE_EXCEPTION(ERR_NOT_EXIST_CONSTRAINT_NAME)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDN_NOT_EXISTS_CONSTRAINT));
    }
    IDE_EXCEPTION(ERR_NOT_ALTER_META);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDD_NO_DROP_META_TABLE));
    }
    IDE_EXCEPTION(ERR_CANNOT_DROP_TIMESTAMP_CONSTRAINT);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDN_CANNOT_DROP_TIMESTAMP));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qdn::validateRenameConstr(qcStatement * aStatement)
{
/***********************************************************************
 *
 * Description :
 *    ALTER TABLE ... RENAME CONSTAINT �� validation  ����
 *
 * Implementation :
 *    1. �����ϴ� ���̺����� �˻�
 *    2. AlterTable �� ������ �� �ִ� ������ �ִ��� �˻�
 *    3. rename �ϰ��� �ϴ� constraint �� �����ϴ��� üũ
 *
 ***********************************************************************/

    // To Fix PR-10909
    qdTableParseTree     * sParseTree;
    UInt                   sConstraintID;
    SChar                  sConstraintName[ QC_MAX_OBJECT_NAME_LEN + 1 ];
    UInt                   sUserID;
    qcmIndex             * sIndex = NULL;

    sParseTree = (qdTableParseTree *)aStatement->myPlan->parseTree;

    IDE_TEST( qdbCommon::checkTableInfo( aStatement,
                                         sParseTree->userName,
                                         sParseTree->tableName,
                                         &sUserID,
                                         &(sParseTree->tableInfo),
                                         &(sParseTree->tableHandle),
                                         &(sParseTree->tableSCN))
              != IDE_SUCCESS);

    /* BUG-48290 shard object�� ���� DDL ���� */
    IDE_TEST( sdi::checkShardObjectForDDL( aStatement, SDI_DDL_TYPE_TABLE ) != IDE_SUCCESS );

    IDE_TEST( qcm::lockTableForDDLValidation(aStatement,
                                             sParseTree->tableHandle,
                                             sParseTree->tableSCN)
              != IDE_SUCCESS );

    IDE_TEST_RAISE(sUserID == QC_SYSTEM_USER_ID, ERR_NOT_ALTER_META);

    // PR-13725
    // CHECK OPERATABLE
    IDE_TEST( qdbAlter::checkOperatable( aStatement,
                                         sParseTree->tableInfo )
              != IDE_SUCCESS );

    // check grant
    IDE_TEST( qdpRole::checkDDLAlterTablePriv( aStatement,
                                               sParseTree->tableInfo )
              != IDE_SUCCESS );

    // check existence of constraint
    QC_STR_COPY( sConstraintName, sParseTree->constraints->constrName );

    sConstraintID =
        qcmCache::getConstraintIDByName(sParseTree->tableInfo,
                                        sConstraintName,
                                        &sIndex);

    IDE_TEST_RAISE(sConstraintID == 0, ERR_NOT_EXIST_CONSTRAINT_NAME);

    // PROJ-1502 PARTITIONED DISK TABLE
    if( sParseTree->tableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
    {
        // ��� ��Ƽ�ǿ� LOCK(IS)
        // ��Ƽ�� ����Ʈ�� �Ľ�Ʈ���� �޾Ƴ��´�.
        IDE_TEST( qdbCommon::checkAndSetAllPartitionInfo(
                            aStatement,
                            sParseTree->tableInfo->tableID,
                            & (sParseTree->partTable->partInfoList) )
                  != IDE_SUCCESS );
        
        // PROJ-1624 global non-partitioned index
        if ( sIndex != NULL )
        {
            if ( sIndex->indexPartitionType == QCM_NONE_PARTITIONED_INDEX )
            {
                IDE_TEST( qdx::makeAndLockIndexTable( aStatement,
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
    }
    else
    {
        // Nothing to do.
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

    return IDE_SUCCESS;
    
    IDE_EXCEPTION(ERR_NOT_EXIST_CONSTRAINT_NAME)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDN_NOT_EXISTS_CONSTRAINT));
    }
    IDE_EXCEPTION(ERR_NOT_ALTER_META);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDD_NO_DROP_META_TABLE));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC qdn::validateDropUnique(qcStatement * aStatement)
{
/***********************************************************************
 *
 * Description :
 *    ALTER TABLE ... DROP UNIQUE ... �� validation ����
 *
 * Implementation :
 *    1. �����ϴ� ���̺����� �˻�
 *    2. AlterTable �� ������ �� �ִ� ������ �ִ��� �˻�
 *    4. �÷��� �����ϴ��� Ȯ���ϰ�, column ID �� flag �� �ο��Ѵ�
 *    5. 4 ���� ���� �÷����� unique �� �����ϴ��� üũ
 *    6. �� unique key �� �����ϴ� child �� �ְ�, �� �����ϴ� ���̺���
 *       alter �ϰ��� �ϴ� ���̺��� �ƴϸ� ����(��, �ٸ� ���̺��� alter
 *       �Ϸ��� ���̺��� �����ϰ� ������..)
 *
 ***********************************************************************/

#define IDE_FN "qdn::validateDropUnique"

    // To Fix PR-10909
    qdTableParseTree        * sParseTree;
    qcmColumn               * sColumn;
    qcmColumn               * sColumnInfo;
    SInt                      sKeyColCount = 0;
    UInt                      sKeyCols[QC_MAX_KEY_COLUMN_COUNT];
    UInt                      sKeyColsFlag[QC_MAX_KEY_COLUMN_COUNT];
    qcmUnique               * sUniqueConstr = NULL;
    qcmRefChildInfo         * sChildInfo;  // BUG-28049
    UInt                      sUserID;
    qcmIndex                * sIndex;
    UInt                      sFlag;

    sParseTree = (qdTableParseTree *)aStatement->myPlan->parseTree;

    // check table exist.
    IDE_TEST( qdbCommon::checkTableInfo( aStatement,
                                         sParseTree->userName,
                                         sParseTree->tableName,
                                         &sUserID,
                                         &(sParseTree->tableInfo),
                                         &(sParseTree->tableHandle),
                                         &(sParseTree->tableSCN))
              != IDE_SUCCESS);

    /* BUG-48290 shard object�� ���� DDL ���� */
    IDE_TEST( sdi::checkShardObjectForDDL( aStatement, SDI_DDL_TYPE_TABLE ) != IDE_SUCCESS );

    IDE_TEST( qcm::lockTableForDDLValidation(aStatement,
                                             sParseTree->tableHandle,
                                             sParseTree->tableSCN)
              != IDE_SUCCESS );

    IDE_TEST_RAISE(sUserID == QC_SYSTEM_USER_ID, ERR_NOT_ALTER_META);

    // PR-13725
    // CHECK OPERATABLE
    IDE_TEST( qdbAlter::checkOperatable( aStatement,
                                         sParseTree->tableInfo )
              != IDE_SUCCESS );

    // To Fix PR-10909
    // �̹� �����ϴ� ���̺��� columns ������ ������.
    sParseTree->columns = sParseTree->tableInfo->columns;

    // check grant
    IDE_TEST( qdpRole::checkDDLAlterTablePriv( aStatement,
                                               sParseTree->tableInfo )
              != IDE_SUCCESS );

    sKeyColCount = 0;
    // check existence of columns in unique key
    for (sColumn = sParseTree->constraints->constraintColumns;
         sColumn != NULL;
         sColumn = sColumn->next)
    {
        IDE_TEST(qcmCache::getColumn(aStatement,
                                     sParseTree->tableInfo,
                                     sColumn->namePos,
                                     &sColumnInfo) != IDE_SUCCESS);

        // BUG-44924
        // Failed to drop unique constraints with columns having descending order.
        sFlag = sColumn->basicInfo->column.flag & SMI_COLUMN_ORDER_MASK;

        // fix BUG-33258
        if( sColumn->basicInfo != sColumnInfo->basicInfo )
        {
            *(sColumn->basicInfo) = *(sColumnInfo->basicInfo);
            // BUG-44924
            sColumn->basicInfo->column.flag &= ~SMI_COLUMN_ORDER_MASK;
            sColumn->basicInfo->column.flag |= sFlag;
        }

        sKeyCols[sKeyColCount]     = sColumn->basicInfo->column.id;
        sKeyColsFlag[sKeyColCount] = sColumn->basicInfo->column.flag;

        sKeyColCount++;
    }
    sUniqueConstr = qcmCache::getUniqueByCols(sParseTree->tableInfo,
                                              sKeyColCount,
                                              sKeyCols,
                                              sKeyColsFlag);
    IDE_TEST_RAISE(sUniqueConstr == NULL, ERR_NOT_EXIST_UNIQUE_KEY);

    IDE_TEST_RAISE(sUniqueConstr->constraintType == QD_PRIMARYKEY,
                   ERR_NOT_EXIST_UNIQUE_KEY);

    // fix BUG-19187, BUG-19190
    IDE_TEST_RAISE(sUniqueConstr->constraintType == QD_LOCAL_UNIQUE,
                   ERR_NOT_EXIST_UNIQUE_KEY);

    IDE_TEST(qcm::getChildKeys(aStatement,
                               sUniqueConstr->constraintIndex,
                               sParseTree->tableInfo,
                               &sChildInfo)
             != IDE_SUCCESS);

    // fix BUG-20684
    IDE_TEST_RAISE( sChildInfo != NULL,
                    ERR_ABORT_REFERENTIAL_CONSTRAINT_EXIST );

    // PROJ-1502 PARTITIONED DISK TABLE
    if( sParseTree->tableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
    {
        // ��� ��Ƽ�ǿ� LOCK(IS)
        // ��Ƽ�� ����Ʈ�� �Ľ�Ʈ���� �޾Ƴ��´�.
        IDE_TEST( qdbCommon::checkAndSetAllPartitionInfo(
                            aStatement,
                            sParseTree->tableInfo->tableID,
                            & (sParseTree->partTable->partInfoList) )
                  != IDE_SUCCESS );

        sIndex = sUniqueConstr->constraintIndex;

        // PROJ-1624 global non-partitioned index
        if ( sIndex->indexPartitionType == QCM_NONE_PARTITIONED_INDEX )
        {
            IDE_TEST( qdx::makeAndLockIndexTable( aStatement,
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

    return IDE_SUCCESS;
    
    IDE_EXCEPTION(ERR_ABORT_REFERENTIAL_CONSTRAINT_EXIST);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDB_REFERENTIAL_CONSTRAINT_EXIST));
    }
    IDE_EXCEPTION(ERR_NOT_EXIST_UNIQUE_KEY);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDN_NOT_EXISTS_UNIQUE_KEY));
    }
    IDE_EXCEPTION(ERR_NOT_ALTER_META);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDD_NO_DROP_META_TABLE));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qdn::validateDropLocalUnique(qcStatement * aStatement)
{
/***********************************************************************
 *
 * Description :
 *    PROJ-1502 PARTITIONED DISK TABLE
 *    ALTER TABLE ... DROP LOCAL UNIQUE ... �� validation ����
 *
 * Implementation :
 *
 ***********************************************************************/

    qdTableParseTree        * sParseTree;
    qcmColumn               * sColumn;
    qcmColumn               * sColumnInfo;
    SInt                      sKeyColCount = 0;
    UInt                      sKeyCols[QC_MAX_KEY_COLUMN_COUNT];
    UInt                      sKeyColsFlag[QC_MAX_KEY_COLUMN_COUNT];
    qcmUnique                 sLocalUnique;
    UInt                      sUserID;
    UInt                      sFlag;

    sParseTree = (qdTableParseTree *)aStatement->myPlan->parseTree;

    // check table exist.
    IDE_TEST( qdbCommon::checkTableInfo( aStatement,
                                         sParseTree->userName,
                                         sParseTree->tableName,
                                         &sUserID,
                                         &(sParseTree->tableInfo),
                                         &(sParseTree->tableHandle),
                                         &(sParseTree->tableSCN))
              != IDE_SUCCESS);

    /* BUG-48290 shard object�� ���� DDL ���� */
    IDE_TEST( sdi::checkShardObjectForDDL( aStatement, SDI_DDL_TYPE_TABLE ) != IDE_SUCCESS );

    IDE_TEST( qcm::lockTableForDDLValidation(aStatement,
                                             sParseTree->tableHandle,
                                             sParseTree->tableSCN)
              != IDE_SUCCESS );

    IDE_TEST_RAISE(sUserID == QC_SYSTEM_USER_ID, ERR_NOT_ALTER_META);

    // PR-13725
    // CHECK OPERATABLE
    IDE_TEST( qdbAlter::checkOperatable( aStatement,
                                         sParseTree->tableInfo )
              != IDE_SUCCESS );

    // �̹� �����ϴ� ���̺��� columns ������ ������.
    sParseTree->columns = sParseTree->tableInfo->columns;

    // check grant
    IDE_TEST( qdpRole::checkDDLAlterTablePriv( aStatement,
                                               sParseTree->tableInfo )
              != IDE_SUCCESS );

    sKeyColCount = 0;
    // check existence of columns in unique key
    for (sColumn = sParseTree->constraints->constraintColumns;
         sColumn != NULL;
         sColumn = sColumn->next)
    {
        IDE_TEST(qcmCache::getColumn(aStatement,
                                     sParseTree->tableInfo,
                                     sColumn->namePos,
                                     &sColumnInfo) != IDE_SUCCESS);

        // BUG-44924
        // Failed to drop unique constraints with columns having descending order.
        sFlag = sColumn->basicInfo->column.flag & SMI_COLUMN_ORDER_MASK;

        // fix BUG-33258
        if( sColumn->basicInfo != sColumnInfo->basicInfo )
        {
            *(sColumn->basicInfo) = *(sColumnInfo->basicInfo);
            // BUG-44924
            sColumn->basicInfo->column.flag &= ~SMI_COLUMN_ORDER_MASK;
            sColumn->basicInfo->column.flag |= sFlag;
        }

        sKeyCols[sKeyColCount]     = sColumn->basicInfo->column.id;
        sKeyColsFlag[sKeyColCount] = sColumn->basicInfo->column.flag;

        sKeyColCount++;
    }

    IDE_TEST( qcm::getQcmLocalUniqueByCols( QC_SMI_STMT( aStatement ),
                                            sParseTree->tableInfo,
                                            (UInt)sKeyColCount,
                                            sKeyCols,
                                            sKeyColsFlag,
                                            &sLocalUnique )
              != IDE_SUCCESS );

    // PROJ-1502 PARTITIONED DISK TABLE
    if( sParseTree->tableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
    {
        // ��� ��Ƽ�ǿ� LOCK(IS)
        // ��Ƽ�� ����Ʈ�� �Ľ�Ʈ���� �޾Ƴ��´�.
        IDE_TEST( qdbCommon::checkAndSetAllPartitionInfo(
                            aStatement,
                            sParseTree->tableInfo->tableID,
                            & (sParseTree->partTable->partInfoList) )
                  != IDE_SUCCESS );
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

    return IDE_SUCCESS;
    
    IDE_EXCEPTION(ERR_NOT_ALTER_META);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDD_NO_DROP_META_TABLE));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdn::validateDropPrimary(qcStatement * aStatement)
{
/***********************************************************************
 *
 * Description :
 *    ALTER TABLE ... DROP PRIMARY KEY ... �� validation ����
 *
 * Implementation :
 *    1. �����ϴ� ���̺����� �˻�
 *    2. AlterTable �� ������ �� �ִ� ������ �ִ��� �˻�
 *    3. ����ȭ�� �ɷ� �ִ� ���̺��̸� ����
 *    4. PRIMARY KEY �� �ִ��� Ȯ��
 *    6. PRIMARY KEY �� �����ϴ� child �� �ְ�, �� �����ϴ� ���̺���
 *       alter �ϰ��� �ϴ� ���̺��� �ƴϸ� ����(��, �ٸ� ���̺��� alter
 *       �Ϸ��� ���̺��� �����ϰ� ������..)
 *
 ***********************************************************************/

#define IDE_FN "qdn::validateDropPrimary"

    // To Fix PR-10909
    qdTableParseTree        * sParseTree;
    UInt                      i;
    qcmUnique               * sUniqueConstr;
    qcmRefChildInfo         * sChildInfo;  // BUG-28049
    UInt                      sUserID;
    UInt                      sTableFlag;
    qcmIndex                * sIndex = NULL;

    sParseTree = (qdTableParseTree *)aStatement->myPlan->parseTree;

    // check table exist.
    IDE_TEST( qdbCommon::checkTableInfo( aStatement,
                                         sParseTree->userName,
                                         sParseTree->tableName,
                                         &sUserID,
                                         &(sParseTree->tableInfo),
                                         &(sParseTree->tableHandle),
                                         &(sParseTree->tableSCN))
              != IDE_SUCCESS);

    /* BUG-48290 shard object�� ���� DDL ���� */
    IDE_TEST( sdi::checkShardObjectForDDL( aStatement, SDI_DDL_TYPE_TABLE ) != IDE_SUCCESS );

    IDE_TEST( qcm::lockTableForDDLValidation(aStatement,
                                             sParseTree->tableHandle,
                                             sParseTree->tableSCN)
              != IDE_SUCCESS );

    IDE_TEST_RAISE(sUserID == QC_SYSTEM_USER_ID, ERR_NOT_ALTER_META);

    // PR-13725
    // CHECK OPERATABLE
    IDE_TEST( qdbAlter::checkOperatable( aStatement,
                                         sParseTree->tableInfo )
              != IDE_SUCCESS );

    // �̹� �����ϴ� ���̺��� columns ������ ������.
    sParseTree->columns = sParseTree->tableInfo->columns;

    // check grant
    IDE_TEST( qdpRole::checkDDLAlterTablePriv( aStatement,
                                               sParseTree->tableInfo )
              != IDE_SUCCESS );

    // if specified tables is replicated, the error
    IDE_TEST_RAISE(sParseTree->tableInfo->replicationCount > 0,
                   ERR_DDL_WITH_REPLICATED_TABLE);
    //proj-1608:replicationCount�� 0�� �� recovery count�� �׻� 0�̾�� ��
    IDE_DASSERT(sParseTree->tableInfo->replicationRecoveryCount == 0);

    // PROJ-1723 SUPPLEMENTAL LOGGING
    sTableFlag = sParseTree->tableInfo->tableFlag;
    IDE_TEST_RAISE( (sTableFlag & SMI_TABLE_SUPPLEMENTAL_LOGGING_MASK)
                    == SMI_TABLE_SUPPLEMENTAL_LOGGING_TRUE,
                    ERR_NO_SUPP_LOGGING_WITHOUT_PK );

    // check existence of primary key
    IDE_TEST_RAISE(sParseTree->tableInfo->primaryKey == NULL,
                   ERR_NOT_EXIST_PRIMARY_KEY);

    for (i = 0; i < sParseTree->tableInfo->uniqueKeyCount; i++)
    {
        sUniqueConstr = &(sParseTree->tableInfo->uniqueKeys[i]);

        if (sUniqueConstr->constraintType == QD_PRIMARYKEY)
        {
            IDE_TEST(qcm::getChildKeys(aStatement,
                                       sUniqueConstr->constraintIndex,
                                       sParseTree->tableInfo,
                                       &sChildInfo)
                     != IDE_SUCCESS);

            // fix BUG-20684
            IDE_TEST_RAISE( sChildInfo != NULL,
                            ERR_ABORT_REFERENTIAL_CONSTRAINT_EXIST );

            sIndex = sUniqueConstr->constraintIndex;
            break;
        }
        else
        {
            // Nothing to do.
        }
    }

    // PROJ-1502 PARTITIONED DISK TABLE
    if( sParseTree->tableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
    {
        // ��� ��Ƽ�ǿ� LOCK(IS)
        // ��Ƽ�� ����Ʈ�� �Ľ�Ʈ���� �޾Ƴ��´�.
        IDE_TEST( qdbCommon::checkAndSetAllPartitionInfo(
                            aStatement,
                            sParseTree->tableInfo->tableID,
                            & (sParseTree->partTable->partInfoList) )
                  != IDE_SUCCESS );

        // PROJ-1624 global non-partitioned index
        if ( sIndex->indexPartitionType == QCM_NONE_PARTITIONED_INDEX )
        {
            IDE_TEST( qdx::makeAndLockIndexTable( aStatement,
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
    
    IDE_EXCEPTION(ERR_DDL_WITH_REPLICATED_TABLE)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDB_DDL_WITH_REPLICATED_TBL));
    }
    IDE_EXCEPTION(ERR_ABORT_REFERENTIAL_CONSTRAINT_EXIST);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDB_REFERENTIAL_CONSTRAINT_EXIST));
    }
    IDE_EXCEPTION(ERR_NOT_EXIST_PRIMARY_KEY)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDN_NOT_EXISTS_PRIMARY_KEY));
    }
    IDE_EXCEPTION(ERR_NOT_ALTER_META);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDD_NO_DROP_META_TABLE));
    }
    IDE_EXCEPTION( ERR_NO_SUPP_LOGGING_WITHOUT_PK )
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDB_NO_SUPP_LOGGING_WITHOUT_PK));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qdn::validateModifyConstr(qcStatement * aStatement)
{
/***********************************************************************
 *
 * Description :
 *    ALTER TABLE ... MODIFY CONSTAINT �� validation  ����
 *
 * Implementation :
 *    1. AlterTable �� ������ �� �ִ� ������ �ִ��� �˻�
 *    2. modify �ϰ��� �ϴ� constraint �� �����ϴ��� �˻�
 *    3. Constraint�� ���� ������ State���� �˻�
 *
 ***********************************************************************/

    qdTableParseTree     * sParseTree;
    qdConstraintState    * sConstraintState;
    UInt                   sConstraintID;
    SChar                  sConstraintName[ QC_MAX_OBJECT_NAME_LEN + 1 ];
    UInt                   sUserID;
    qcmIndex             * sIndex = NULL;
    qcuSqlSourceInfo       sqlInfo;

    sParseTree = (qdTableParseTree *)aStatement->myPlan->parseTree;

    sConstraintState = sParseTree->constraints->constrState;

    IDE_ASSERT(sConstraintState != NULL);

    // Table�� Table Info ��ȸ
    IDE_TEST( qdbCommon::checkTableInfo( aStatement,
                                         sParseTree->userName,
                                         sParseTree->tableName,
                                         &sUserID,
                                         &(sParseTree->tableInfo),
                                         &(sParseTree->tableHandle),
                                         &(sParseTree->tableSCN))
              != IDE_SUCCESS);

    /* BUG-48290 shard object�� ���� DDL ���� */
    IDE_TEST( sdi::checkShardObjectForDDL( aStatement, SDI_DDL_TYPE_TABLE ) != IDE_SUCCESS );

    // Lock Table
    IDE_TEST( qcm::lockTableForDDLValidation(aStatement,
                                             sParseTree->tableHandle,
                                             sParseTree->tableSCN)
              != IDE_SUCCESS );

    // Meta Table�� DDL ����
    IDE_TEST_RAISE(sUserID == QC_SYSTEM_USER_ID, ERR_NOT_ALTER_META);

    // PR-13725
    // CHECK OPERATABLE
    IDE_TEST( qdbAlter::checkOperatable( aStatement,
                                         sParseTree->tableInfo )
              != IDE_SUCCESS );

    // check grant
    IDE_TEST( qdpRole::checkDDLAlterTablePriv( aStatement,
                                               sParseTree->tableInfo )
              != IDE_SUCCESS );

    // check existence of constraint
    QC_STR_COPY( sConstraintName, sParseTree->constraints->constrName );

    sConstraintID =
        qcmCache::getConstraintIDByName(sParseTree->tableInfo,
                                        sConstraintName,
                                        &sIndex);

    IDE_TEST_RAISE(sConstraintID == 0, ERR_NOT_EXIST_CONSTRAINT_NAME);

    /* BUG-42321  The smi cursor type of smistatement is set wrongly in
     * partitioned table
     */
    IDE_TEST( qdbCommon::checkForeignKeyParentTableInfo( aStatement,
                                                         sParseTree->tableInfo,
                                                         sConstraintName,
                                                         sConstraintState->validate )
              != IDE_SUCCESS );

    // Modify�� Constraint�� ���� ������ State���� �˻�
    // ===============================================================

    //                    | Primary  | Unique   | Foreign  | Not Null |
    // ---------------------------------------------------------------
    // enable/            |     O    |     O    |     O    |     O    |
    //  disable           |     O    |     O    |     O    |     O    |
    // ---------------------------------------------------------------
    // validate/          |     X    |     X    |    *O    |     O    |
    //  novalidate        |     X    |     X    |    *O    |     O    |
    // ---------------------------------------------------------------
    // deferrable/        |  can't   |  can't   |  can't   |  can't   |
    //  not deferrable    |  modify  |  modify  |  modify  |  modify  |
    // ---------------------------------------------------------------
    // initial deferred/  |     O    |     O    |     O    |     O    |
    //  initial immediate |     O    |     O    |     O    |     O    |
    // ---------------------------------------------------------------
    // * ǥ�õ� State�� ����

    // Primary key, Unique key�� NOVALIDATE�� �� �� ����.
    // ���� VALIDATE/NOVALIDATE�� ���� �����ϹǷ� PK, UK�� ����ó���Ѵ�.
    // Constraint�� ����� Index�� �ִٸ� Primary Ȥ�� Unique key�̴�.
    if( sIndex != NULL )
    {
        sqlInfo.setSourceInfo(
            aStatement,
            & sConstraintState->validatePosition );
        IDE_RAISE( ERR_NOT_SUPPORTED_CONSTR_STATE );
    }

    // PROJ-1502 PARTITIONED DISK TABLE
    if( sParseTree->tableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
    {
        // ��� ��Ƽ�ǿ� LOCK(IS)
        // ��Ƽ�� ����Ʈ�� �Ľ�Ʈ���� �޾Ƴ��´�.
        IDE_TEST( qdbCommon::checkAndSetAllPartitionInfo(
                            aStatement,
                            sParseTree->tableInfo->tableID,
                            & (sParseTree->partTable->partInfoList) )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;
    
    IDE_EXCEPTION(ERR_NOT_EXIST_CONSTRAINT_NAME)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDN_NOT_EXISTS_CONSTRAINT));
    }
    IDE_EXCEPTION(ERR_NOT_ALTER_META);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDD_NO_DROP_META_TABLE));
    }
    IDE_EXCEPTION(ERR_NOT_SUPPORTED_CONSTR_STATE);
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QDN_NOT_SUPPORTED_CONSTR_STATE,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


// CREATE TABLE, ALTER TABLE ADD COLUMN, ALTER TABLE ADD CONSTRAINT
IDE_RC qdn::validateConstraints(
    qcStatement      * aStatement,
    qcmTableInfo     * aTableInfo,
    UInt               aTableOwnerID,
    scSpaceID          aTableTBSID,
    smiTableSpaceType  aTableTBSType,
    qdConstraintSpec * aConstr,
    UInt               aConstraintOption,
    UInt             * aUniqueKeyCount)
{
/***********************************************************************
 *
 * Description :
 *    CREATE TABLE, ALTER TABLE ADD COLUMN, ALTER TABLE ADD CONSTRAINT
 *    ����� Constraints �� validation
 *
 * Implementation :
 *    1. constraint �̸��� ������ �̸��� �ش��ϴ� constraint ���翩�� �˻�
 *    2. constraint �ɸ� �÷� ������ŭ constraint ����Ʈ�� ���ؼ�
 *       �Ʒ� if/else �ݺ�
 *       if ALTER_TABLE
 *          if add constraint
 *              constraint �� �߰��ϴ� �÷��� �����ϴ��� �˻�
 *          primary key, unique, not null constraint �� ���� column flag ����
 *          if primary key �Ǵ� not null constraint �̸鼭 ADD_COLUMN
 *              defaultValue �� ��õ��� �ʰ�, ���̺� �̹� ���ڵ尡 �����ϸ�
 *              ���� �߻�
 *       else : CREATE_TABLE
 *           ���������κ��� �÷��� ���ؼ� flag ����
 *    3. constraint ����Ʈ �� foreign key, primary key, unique key,
 *       local unique key, check constraint�� ���ؼ� validation ����
 *    4. constraint ����Ʈ�� ���ؼ� Constraint �ߺ� �˻�
 *
 ***********************************************************************/

#define IDE_FN "qdn::validateConstraints"

    qdConstraintSpec    * sCurrConstr;
    idBool                sHasPrimary = ID_FALSE;
    qcmColumn           * sColumnList = NULL;
    qcmColumn           * sColumn;
    qcmColumn           * sColumnInfo;
    qcmColumn           * sDefaultColumns;
    SInt                  sKeyColCount;
    qcuSqlSourceInfo      sqlInfo;
    SChar                 sName[ QC_MAX_OBJECT_NAME_LEN + 1 ];
    UInt                  i;
    volatile UInt         sStage;

    const void *          sTmpRow;
    const void *          sRow = NULL;

    smiTableCursor       sTmpCursor;
    smiCursorProperties  sCursorProperty;
    scGRID               sRid; // Disk Table�� ���� Record IDentifier

    UInt                 sTableType;
    UInt                 sRowSize;

    UInt                 sFlag;

    idBool               sExistSameConstrName;
    qdTableParseTree   * sParseTree;
    idBool               sIsPartitioned = ID_FALSE;
    qcmTableInfo       * sPartInfo;
    qcmPartitionInfoList * sPartInfoList = NULL;
    qcmPartitionInfoList * sTempPartInfoList = NULL;
    qdPartitionedIndex   * sPartIndex = NULL;
    idBool                 sIsDiskTBS = ID_FALSE;
    idBool                 sIsLocalIndex;

    qcmTableInfo       * sDiskInfo = NULL;

    IDE_FT_BEGIN();

    IDU_FIT_POINT_FATAL( "qdn::validateConstraints::__FT__" );

    sParseTree = (qdTableParseTree *)aStatement->myPlan->parseTree;
    sStage = 0;

    // PROJ-1502 PARTITIONED DISK TABLE
    if( aTableInfo != NULL )
    {
        if( aTableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
        {
            sIsPartitioned = ID_TRUE;

            // �Ľ�Ʈ������ ��Ƽ�� ���� ����Ʈ�� �����´�.
            sPartInfoList = sParseTree->partTable->partInfoList;
        }
        else
        {
            sIsPartitioned = ID_FALSE;
        }
    }
    else
    {
        if( sParseTree->partTable != NULL )
        {
            sIsPartitioned = ID_TRUE;
        }
        else
        {
            sIsPartitioned = ID_FALSE;
        }
    }

    /* PROJ-2334 PMT */
    if ( sIsPartitioned == ID_TRUE )
    {
        sIsDiskTBS = smiTableSpace::isDiskTableSpaceType( aTableTBSType );
    }
    else
    {
        /* Nothing To Do */
    }
    
    // PROJ-1874 Novalidate
    // CREATE �ÿ��� Constraint State�� �������� �ʴ´�.
    if ( (aConstraintOption == QDN_ON_CREATE_TABLE) &&
         (aConstr != NULL) )
    {
        if ( aConstr->constrState != NULL )
        {
            if( QC_IS_NULL_NAME(aConstr->constrState->validatePosition) != ID_TRUE )
            {
                sqlInfo.setSourceInfo(aStatement,
                                      & aConstr->constrState->validatePosition );
                IDE_RAISE( ERR_NOT_SUPPORTED_CONSTR_STATE );
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

    if ( (aConstraintOption == QDN_ON_CREATE_TABLE) ||
         (aConstraintOption == QDN_ON_ADD_COLUMN) ||
         (aConstraintOption == QDN_ON_MODIFY_COLUMN) )
    {
        sColumnList = ((qdTableParseTree *)aStatement->myPlan->parseTree)->columns;
    }
    else
    {
        // Nothing to do.
    }

    // ALTER TABLE ADD COLUMN, ALTER TABLE ADD CONSTRAINT
    if ( aTableInfo != NULL )
    {
        if ( aTableInfo->primaryKey != NULL )
        {
            sHasPrimary = ID_TRUE;
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

    /* constraintType ��  QD_NOT_NULL �Ǵ�  QD_PRIMARYKEY �̰�,
     * aConstraintOption == QDN_ON_ADD_COLUMN  �̸鼭
     * �÷��� defaultValue == NULL �� �� sTmpRow �� ����ϰ� �� */
    if ( aTableInfo != NULL )
    {
        sTableType = aTableInfo->tableFlag & SMI_TABLE_TYPE_MASK;

        /* PROJ-2464 hybrid partitioned table ����
         *  - Alter ������ ���, sPartInfoList�� ������ TBS ������ �� �ִ�.
         *  - ���� Disk Partition�� ���ԵǾ��� �� �˻��ϰ�, Row Buffer�� �����Ѵ�.
         *  - ����(15-03-12)���� aConstraintOption == QDN_ON_ADD_COLUMN �� ��츸 Row Buffer�� ����ϰ� �ִ�.
         */
        if ( sIsPartitioned == ID_TRUE )
        {
            for ( sTempPartInfoList = sPartInfoList;
                  sTempPartInfoList != NULL;
                  sTempPartInfoList = sTempPartInfoList->next )
            {
                sPartInfo = sTempPartInfoList->partitionInfo;

                if ( ( sPartInfo->tableFlag & SMI_TABLE_TYPE_MASK ) == SMI_TABLE_DISK )
                {
                    sDiskInfo = sPartInfo;
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
            if ( sTableType == SMI_TABLE_DISK )
            {
                sDiskInfo = aTableInfo;
            }
            else
            {
                /* Nothing to do */
            }
        }

        if ( sDiskInfo != NULL )
        {
            // Disk Table�� ���
            // Record Read�� ���� ������ �Ҵ��Ѵ�.
            IDE_TEST( qdbCommon::getDiskRowSize( sDiskInfo,
                                                 & sRowSize )
                      != IDE_SUCCESS );

            // To Fix PR-10247
            // Disk Variable Column Loading�� RID�� �̿���
            // �ߺ� Loading �˻縦 ���Ͽ� �ʱ�ȭ���־�� ��.
            IDU_LIMITPOINT("qdn::validateConstraints::malloc1");
            IDE_TEST( QC_QMP_MEM(aStatement)->cralloc( sRowSize,
                                                       (void **) & sTmpRow )
                      != IDE_SUCCESS );

            // ��Ƽ�ǵ� ���̺��� ��� �� ��Ƽ���� �ݺ��ϸ� �˻��ؾ��ϱ�
            // ������ ó�� sTempRow�� �����س��´�.
            sRow = sTmpRow;
        }
        else
        {
            // Memory Table�� ���
            // Nothing To Do
        }
    }
    else
    {
        /* Nothing to do */
    }

    for (sCurrConstr = aConstr;
         sCurrConstr != NULL;
         sCurrConstr = sCurrConstr->next)
    {
        //---------------------------------
        // ������ Constraint Name�� ���� constraint�� �ִ��� �˻�
        //---------------------------------

        if (QC_IS_NULL_NAME(sCurrConstr->constrName) == ID_FALSE)
        {
            // To Fix BUG-10341
            QC_STR_COPY( sName, sCurrConstr->constrName );

            IDE_TEST( qdn::existSameConstrName( aStatement,
                                                sName,
                                                aTableOwnerID,
                                                & sExistSameConstrName )
                      != IDE_SUCCESS );

            if ( sExistSameConstrName == ID_TRUE )
            {
                sqlInfo.setSourceInfo( aStatement,
                                       & sCurrConstr->constrName );
                IDE_RAISE(ERR_DUP_CONSTR_NAME);
            }
            else
            {
                // nothing to do
            }
        }

        if (sCurrConstr->constrType == QD_PRIMARYKEY)
        {
            // �߰��� constraint�� primary key �̰�,
            // �ش� Table�� �̹� Primary Key�� �����ϴ� ���
            IDE_TEST_RAISE(sHasPrimary == ID_TRUE,
                           ERR_DUP_PRIMARY);
            sHasPrimary = ID_TRUE;
        }

        for (sKeyColCount = 0,
                 sColumn = sCurrConstr->constraintColumns;
             sColumn != NULL;
             sColumn = sColumn->next)
        {
            if (aTableInfo != NULL)
            {
                if (qcmCache::getColumn(
                        aStatement,
                        aTableInfo,
                        sColumn->namePos,
                        &sColumnInfo) != IDE_SUCCESS)
                { // for add column..
                    if (aConstraintOption != QDN_ON_ADD_CONSTRAINT)
                    {    // alter column add constraints.
                        IDE_TEST(getColumnFromDefinitionList(
                                     aStatement,
                                     sColumnList,
                                     sColumn->namePos,
                                     &sColumnInfo) != IDE_SUCCESS);
                        sColumnInfo->basicInfo->column.id = 0;
                    }
                    else
                    {
                        IDE_RAISE(ERR_NOT_EXIST_COLUMN);
                    }

                }

                if ( ( sCurrConstr->constrType == QD_PRIMARYKEY ) ||
                     ( sCurrConstr->constrType == QD_UNIQUE )     ||
                     ( sCurrConstr->constrType == QD_LOCAL_UNIQUE ) )
                {
                    // To Fix PR-10247
                    // QDX_SET_MTC_COLUMN( sColumn->basicInfo,
                    //                     sColumnInfo->basicInfo);
                    // PR-4442
                    // sColumn->basicInfo->column.flag |=
                    //     (sColumnInfo->basicInfo->column.flag
                    //         & SMI_COLUMN_TYPE_MASK);

                    // To Fix PR-10247
                    // ���� ���� Macro�� ����� �ڷ� ������ ���濡
                    // �����ϰ� ��ó�� �� ����.
                    // Key Column�� Order ������ ������ �־�� �Ѵ�.
                    sFlag = sColumn->basicInfo->column.flag
                        & SMI_COLUMN_ORDER_MASK;

                    // fix BUG-33258
                    if( sColumn->basicInfo != sColumnInfo->basicInfo )
                    {
                        *(sColumn->basicInfo) = *(sColumnInfo->basicInfo);
                    }

                    sColumn->basicInfo->column.flag &=
                        ~SMI_COLUMN_ORDER_MASK;
                    sColumn->basicInfo->column.flag |=
                        (sFlag & SMI_COLUMN_ORDER_MASK);

                    // To Fix PR-10207
                    // geometry, blob, clob type�� primary key��
                    // unique key�� ������ �� ����.
                    // PROJ-1362
                    // blob, clob�߰�
                    // Data Type�� ����� �� �ִ� �ε����� ����
                    // primary key�� unique key�� ���� ���ΰ� ������
                    if ( smiCanUseUniqueIndex(
                             mtd::getDefaultIndexTypeID(
                                 sColumn->basicInfo->module ) ) == ID_FALSE )
                    {
                        sqlInfo.setSourceInfo( aStatement,
                                               & sColumn->namePos );
                        IDE_RAISE(ERR_NOT_ALLOW_PK_AND_UNIQUE);
                    }
                    else
                    {
                        // Nothing To Do
                    }

                    // PROJ-1502 PARTITIONED DISK TABLE
                    if( sCurrConstr->constrType == QD_LOCAL_UNIQUE )
                    {
                        IDE_TEST_RAISE(sIsPartitioned == ID_FALSE,
                                       ERR_LOCAL_UNIQUE_KEY_ON_NON_PART_TABLE);
                    }
                }
                else if (sCurrConstr->constrType == QD_NOT_NULL)
                {
                    IDU_LIMITPOINT("qdn::validateConstraints::malloc2");
                    IDE_TEST( QC_QMP_MEM(aStatement)->alloc(ID_SIZEOF(mtcColumn),
                                                            (void**)&(sColumn->basicInfo))
                              != IDE_SUCCESS);

                    // To Fix PR-10247
                    // QDX_SET_MTC_COLUMN(sColumn->basicInfo,
                    //                    sColumnInfo->basicInfo);
                    // sColumn->basicInfo->column.flag =
                    //     sColumnInfo->basicInfo->column.flag;
                    // sColumn->basicInfo->flag = sColumnInfo->basicInfo->flag;
                    // sColumn->basicInfo->flag |= MTC_COLUMN_NOTNULL_TRUE;

                    // To Fix PR-10247
                    // ���� ���� Macro�� ����� �ڷ� ������ ���濡
                    // �����ϰ� ��ó�� �� ����.
                    // NOT NULL ������ ������ �־�� ��.

                    // fix BUG-33258
                    if( sColumn->basicInfo != sColumnInfo->basicInfo )
                    {
                        *(sColumn->basicInfo) = *(sColumnInfo->basicInfo);
                    }

                    sColumn->basicInfo->flag &= ~MTC_COLUMN_NOTNULL_MASK;
                    sColumn->basicInfo->flag |= MTC_COLUMN_NOTNULL_TRUE;

                    for (i = 0; i < aTableInfo->notNullCount; i++)
                    {
                        IDE_TEST_RAISE(intersectColumn(
                                           aTableInfo->notNulls[i].constraintColumn,
                                           aTableInfo->notNulls[i].constraintColumnCount,
                                           &(sColumn->basicInfo->column.id), 1) == ID_TRUE,
                                       ERR_DUP_NOTNULL);
                    }
                }
                else
                {
                    /* QD_FOREIGN, QD_NULL, QD_CHECK */
                    sColumn->basicInfo = sColumnInfo->basicInfo;
                }

                if ( ((sCurrConstr->constrType == QD_NOT_NULL) ||
                      (sCurrConstr->constrType == QD_PRIMARYKEY))
                     && aConstraintOption == QDN_ON_ADD_COLUMN )
                {
                    // If a added column has no default value,
                    //      then raise error.
                    // But if a added column were timestamp column,
                    //      then a added column may have no default value.
                    if ( ( sColumnInfo->defaultValue == NULL ) &&
                         ( ( sColumnInfo->basicInfo->flag
                             & MTC_COLUMN_TIMESTAMP_MASK )
                           == MTC_COLUMN_TIMESTAMP_FALSE ) )
                    {
                        if( sIsPartitioned == ID_TRUE )
                        {
                            for( sTempPartInfoList = sPartInfoList;
                                 sTempPartInfoList != NULL;
                                 sTempPartInfoList = sTempPartInfoList->next )
                            {
                                sPartInfo = sTempPartInfoList->partitionInfo;

                                //----------------------------------------------------
                                // PROJ-1705 fetch column list ����
                                // �� �Լ���������
                                // ���ǿ� �´� ���ڵ� ���� ������ üũ�ϱ� ������
                                // cursor property�� mFetchColumnList�� NULL�� ������.
                                // mFetchColumnList�� NULL�� ���� ���
                                // sm������ �÷��� �����۾�����
                                // qp���� ������ �޸������͸� ��ȯ�Ѵ�.
                                //----------------------------------------------------

                                SMI_CURSOR_PROP_INIT_FOR_FULL_SCAN( &sCursorProperty, aStatement->mStatistics );
                                sCursorProperty.mFetchColumnList = NULL;

                                // PROJ-1705
                                // sm���� ���ڵ� ��ġ�� rid������ �������� �ʾƵ� ��.

                                sTmpCursor.initialize();

                                IDE_TEST(sTmpCursor.open(
                                             QC_SMI_STMT( aStatement ),
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
                                             & sCursorProperty)
                                         != IDE_SUCCESS);
                                sStage = 1;

                                IDU_FIT_POINT_FATAL( "qdn::validateConstraints::__FT__::STAGE1Partition" );

                                IDE_TEST(sTmpCursor.beforeFirst() != IDE_SUCCESS);

                                // ���� sRow�� ����
                                sTmpRow = sRow;
                                IDE_TEST(sTmpCursor.readRow(&sTmpRow,
                                                            &sRid,
                                                            SMI_FIND_NEXT)
                                         != IDE_SUCCESS);
                                sStage = 0;
                                IDE_TEST(sTmpCursor.close() != IDE_SUCCESS);
                                IDE_TEST_RAISE(sTmpRow != NULL,
                                               ERR_ADD_COL_NO_DEFAULT_NOTNULL);
                            }
                        }
                        else
                        {
                            //----------------------------------------------------
                            // PROJ-1705 fetch column list ����
                            // �� �Լ���������
                            // ���ǿ� �´� ���ڵ� ���� ������ üũ�ϱ� ������
                            // cursor property�� mFetchColumnList�� NULL�� ������.
                            // mFetchColumnList�� NULL�� ���� ���
                            // sm������ �÷��� �����۾�����
                            // qp���� ������ �޸������͸� ��ȯ�Ѵ�.
                            //----------------------------------------------------
                            SMI_CURSOR_PROP_INIT_FOR_FULL_SCAN( &sCursorProperty, aStatement->mStatistics );
                            sCursorProperty.mFetchColumnList = NULL;

                            // PROJ-1705
                            // sm���� ���ڵ� ��ġ�� rid������ �������� �ʾƵ� ��.

                            sTmpCursor.initialize();

                            IDE_TEST(sTmpCursor.open(
                                         QC_SMI_STMT( aStatement ),
                                         aTableInfo->tableHandle,
                                         NULL,
                                         smiGetRowSCN(aTableInfo->tableHandle),
                                         NULL,
                                         smiGetDefaultKeyRange(),
                                         smiGetDefaultKeyRange(),
                                         smiGetDefaultFilter(),
                                         SMI_LOCK_READ|
                                         SMI_TRAVERSE_FORWARD|
                                         SMI_PREVIOUS_DISABLE,
                                         SMI_SELECT_CURSOR,
                                         & sCursorProperty)
                                     != IDE_SUCCESS);
                            sStage = 1;

                            IDU_FIT_POINT_FATAL( "qdn::validateConstraints::__FT__::STAGE1Table" );

                            IDE_TEST(sTmpCursor.beforeFirst() != IDE_SUCCESS);
                            IDE_TEST(sTmpCursor.readRow(&sTmpRow,
                                                        &sRid,
                                                        SMI_FIND_NEXT)
                                     != IDE_SUCCESS);
                            sStage = 0;
                            IDE_TEST(sTmpCursor.close() != IDE_SUCCESS);
                            IDE_TEST_RAISE(sTmpRow != NULL,
                                           ERR_ADD_COL_NO_DEFAULT_NOTNULL);
                        }
                    }
                }
            }
            else // on create table stage.
            {
                IDE_TEST(getColumnFromDefinitionList(
                             aStatement,
                             sColumnList,
                             sColumn->namePos,
                             &sColumnInfo) != IDE_SUCCESS);

                if ( ( sCurrConstr->constrType == QD_PRIMARYKEY ) ||
                     ( sCurrConstr->constrType == QD_UNIQUE )     ||
                     ( sCurrConstr->constrType == QD_LOCAL_UNIQUE ) )
                {
                    // To Fix PR-10247
                    // QDX_SET_MTC_COLUMN(sColumn->basicInfo,
                    //                    sColumnInfo->basicInfo);

                    // To Fix PR-10247
                    // ���� ���� Macro�� ����� �ڷ� ������ ���濡
                    // �����ϰ� ��ó�� �� ����.
                    // Key Column�� Order ������ ������ �־�� �Ѵ�.
                    sFlag = sColumn->basicInfo->column.flag
                        & SMI_COLUMN_ORDER_MASK;

                    // fix BUG-33258
                    if( sColumn->basicInfo != sColumnInfo->basicInfo )
                    {
                        *(sColumn->basicInfo) = *(sColumnInfo->basicInfo);
                    }

                    sColumn->basicInfo->column.flag &=
                        ~SMI_COLUMN_ORDER_MASK;
                    sColumn->basicInfo->column.flag |=
                        (sFlag & SMI_COLUMN_ORDER_MASK);

                    // To Fix PR-10207
                    // geometry type�� primary key��
                    // unique key�� ������ �� ����.
                    // PROJ-1362
                    // blob, clob�߰�
                    // Data Type�� ����� �� �ִ� �ε����� ����
                    // primary key�� unique key�� ���� ���ΰ� ������
                    if ( smiCanUseUniqueIndex(
                             mtd::getDefaultIndexTypeID(
                                 sColumn->basicInfo->module ) ) == ID_FALSE )
                    {
                        sqlInfo.setSourceInfo( aStatement,
                                               & sColumn->namePos );
                        IDE_RAISE(ERR_NOT_ALLOW_PK_AND_UNIQUE);
                    }
                    else
                    {
                        // Nothing To Do
                    }

                    // PROJ-1502 PARTITIONED DISK TABLE
                    if( sCurrConstr->constrType == QD_LOCAL_UNIQUE )
                    {
                        IDE_TEST_RAISE(sIsPartitioned == ID_FALSE,
                                       ERR_LOCAL_UNIQUE_KEY_ON_NON_PART_TABLE);
                    }
                }
                else if (sCurrConstr->constrType == QD_NOT_NULL)
                {
                    sColumn->basicInfo = sColumnInfo->basicInfo;
                    sColumn->basicInfo->flag |= MTC_COLUMN_NOTNULL_TRUE;
                }
                else
                {
                    /* QD_FOREIGN, QD_NULL, QD_CHECK */
                    sColumn->basicInfo = sColumnInfo->basicInfo;
                }
            }
            sKeyColCount++;
        }
        sCurrConstr->constrColumnCount = sKeyColCount;

        IDE_TEST_RAISE(sKeyColCount > QC_MAX_KEY_COLUMN_COUNT,
                       ERR_MAX_KEY_COLUMN_COUNT);
    }

    for (sCurrConstr = aConstr;
         sCurrConstr != NULL;
         sCurrConstr = sCurrConstr->next)
    {
        if (sCurrConstr->constrType == QD_FOREIGN)
        {
            IDE_TEST( qdnForeignKey::validateForeignKeySpec(
                          aStatement,
                          aTableOwnerID,
                          aTableInfo,
                          sCurrConstr )!= IDE_SUCCESS );
        }
        else if ( ( sCurrConstr->constrType == QD_PRIMARYKEY ) ||
                  ( sCurrConstr->constrType == QD_UNIQUE )     ||
                  ( sCurrConstr->constrType == QD_LOCAL_UNIQUE ) )
        {
            // To Fix PR-10437
            // Constraint�� Index TableSpace ���� ȹ��
            IDE_TEST( qdtCommon::getAndValidateIndexTBS(
                          aStatement,
                          aTableTBSID,
                          aTableTBSType,
                          sCurrConstr->indexTBSName,
                          aTableOwnerID,
                          & sCurrConstr->indexTBSID,
                          & sCurrConstr->indexTBSType )
                      != IDE_SUCCESS );

            /* BUG-40099
             * - Temporary Table �� PK/UK ���� ��, table�� ���� tablespace ���� ���. 
             *
             * ����) sCurrConstr->indexTBSID ��
             *       qcply.y���� QD_SET_INIT_CONSTRAINT_SPEC() �� ȣ��,
             *       ID_USHORT_MAX �� �ʱ�ȭ�ȴ�.
             */
            if ( aTableInfo == NULL ) /* create table */
            {
                /* qdbCreate::validateCreateTable()
                 *   qdbCreate::validateTableSpace()
                 * table tablespace�� volatile ������ �� �Լ� ������ Ȯ�εȴ�.
                 * Constraint�� TBS ID�� �̿� �������� Ȯ���ϸ� �ȴ�.
                 */
                if ( ( sParseTree->flag & QDT_CREATE_TEMPORARY_MASK )
                     == QDT_CREATE_TEMPORARY_TRUE )
                {
                    IDE_TEST_RAISE( sCurrConstr->indexTBSID != aTableTBSID,
                                    ERR_CANNOT_ALLOW_TBS_NAME_FOR_TEMPORARY_INDEX );
                }
                else
                {
                    /* Nothing To Do */
                }
            }
            else /* alter table */
            {
                /* temporary table ������ Ȯ�εǸ�, tablespace�� volatile ������ ����
                 * Ȯ���� �ʿ����. 
                 */
                if( qcuTemporaryObj::isTemporaryTable( aTableInfo ) == ID_TRUE )
                {
                    IDE_TEST_RAISE( sCurrConstr->indexTBSID != aTableTBSID,
                                    ERR_CANNOT_ALLOW_TBS_NAME_FOR_TEMPORARY_INDEX );
                }
                else
                {
                    /* Nothing To Do */
                }
            }

            /* PROJ-2461 pk, uk constraint���� prefix index ���� ��ȭ
             * local/global index �����ϴ� ��츦 ���� validation.
             * localunique�� ����ϰ� validation�ϱ� ������ ����Ѵ�.
             * ���� validation �ڵ�� �����Ѵ�.
             */
            if ( sIsPartitioned == ID_TRUE )
            {
                /* Partitioned Table ������� PK/UK�� ������ ��
                 * using index local ���� ������ ���� ���� �⺻�� local index��.
                 * �� local index�� ���� �����ϸ� �׷��� �Ѵ�.
                 *
                 * �̸� ���ؼ� index partition key�� index key�� ���ԵǴ��� �˻��Ѵ�.
                 * ���ԵǾ� ���� -> local index�� ���� (PMT/PDT ���)
                 * ���� -> 1) PDT�� global non-partitioned index�� ����
                 *         2) PMT�� ERROR(PMT�� ������ local�� �����ؾ� �ϱ� ����)
                 */
                sIsLocalIndex = ID_TRUE;
                if ( ( sCurrConstr->constrType == QD_PRIMARYKEY ) ||
                     ( sCurrConstr->constrType == QD_UNIQUE ) )
                {
                    /* Partitioned Table�� �������� �˻��Ѵ�.
                     * Table Partition�� qdbCommon::validateConstraintRestriction()���� �˻��Ѵ�.
                     */
                    IDE_TEST( qdn::checkLocalIndex( aStatement,
                                                    sCurrConstr,
                                                    aTableInfo,
                                                    &sIsLocalIndex,
                                                    sIsDiskTBS )
                              != IDE_SUCCESS );
                }
                else
                {
                    /* Nothing To Do */
                }

                /* PK, UK ó��
                 * local/global index �����ϴ� ��츦 ���� validation�Ѵ�.
                 * local index�� ���, using index local ���� �������� �ʿ�.
                 */
                if ( sIsLocalIndex != ID_TRUE )
                {
                    /* Global index�� ��� */
                    if ( sCurrConstr->partIndex != NULL )
                    {
                        /* USING INDEX ���� �ִ� ��� */

                        /* global index���� USING INDEX LOCAL �� ����� ���� */
                        IDE_TEST_RAISE( (sCurrConstr->partIndex->partIndexType !=
                                         QCM_NONE_PARTITIONED_INDEX),
                                        ERR_CREATE_PART_INDEX_ON_NONE_PART_TABLE );

                        /* global non-partitioned index validation */
                        IDE_TEST( qdx::validateNonPartitionedIndex(
                                      aStatement,
                                      sParseTree->userName,
                                      sCurrConstr->constrName,
                                      sCurrConstr->indexTableName,
                                      sCurrConstr->keyIndexName,
                                      sCurrConstr->ridIndexName )
                                      != IDE_SUCCESS );
                    }
                    else
                    {
                        /* global non-partitioned index validation */
                        IDE_TEST( qdx::validateNonPartitionedIndex(
                                      aStatement,
                                      sParseTree->userName,
                                      sCurrConstr->constrName,
                                      sCurrConstr->indexTableName,
                                      sCurrConstr->keyIndexName,
                                      sCurrConstr->ridIndexName )
                                      != IDE_SUCCESS );

                        /* �ش� constraint�� USING INDEX���� ������� �ʾҴ�.
                         * sConstr->partIndex�� �޾Ƴ��´�.
                         * EXECUTION �ܰ迡�� ����ϱ� ���� �޾Ƴ���.
                         */
                        IDU_LIMITPOINT("qdn::validateConstraints::malloc3");
                        IDE_TEST( STRUCT_ALLOC_WITH_COUNT( QC_QMP_MEM(aStatement),
                                                           qdPartitionedIndex,
                                                           1,
                                                           & sPartIndex )
                                  != IDE_SUCCESS );
        
                        QD_SET_INIT_PARTITIONED_INDEX(sPartIndex);
        
                        sCurrConstr->partIndex = sPartIndex;
                    }
                }
                else
                {
                    /* Local index�� ��� */
                    if ( sCurrConstr->partIndex != NULL )
                    {
                        /* Local�̸鼭, USING INDEX ���� �ִ� ���
                         * Partitioned index validation
                         * PK, UK LOCALUNIQUE ��� �ش�
                         * using index local���� ������ �ش� ������
                         * ����ڰ� �Է��� ����(��Ƽ�� �̸� ��)�� validate�ؾ� �Ѵ�.
                         */
                        if ( sCurrConstr->partIndex->partIndexType != QCM_NONE_PARTITIONED_INDEX )
                        {
                            /* USING INDEX LOCAL �� ��� */
                            if ( aTableInfo != NULL )
                            {
                                /* called by alter table */
                                IDE_TEST( qdx::validatePartitionedIndexOnAlterTable( aStatement,
                                                                                     sCurrConstr->indexTBSName,
                                                                                     sCurrConstr->partIndex,
                                                                                     aTableInfo )
                                          != IDE_SUCCESS );
                            }
                            else
                            {
                                /* called by create table */
                                IDE_TEST( qdx::validatePartitionedIndexOnCreateTable( aStatement,
                                                                                      sParseTree,
                                                                                      sCurrConstr->indexTBSName,
                                                                                      sCurrConstr->partIndex )
                                          != IDE_SUCCESS );
                            }
                        }
                        else
                        {
                            /* USING INDEX ���� ������ �ڿ� LOCAL�� �� ���� ���
                             * �� ���� validate�� ����� ����.
                             *
                             * Nothing To Do
                             * */
                        }
                    }
                    else
                    {
                        /* Local������ USING INDEX���� ������� ���� ���
                         * sConstr->partIndex�� �޾Ƴ��´�.
                         * EXECUTION �ܰ迡�� ����ϱ� ���� �޾Ƴ���.
                         */
                        IDU_LIMITPOINT("qdn::validateConstraints::malloc3");
                        IDE_TEST( STRUCT_ALLOC_WITH_COUNT( QC_QMP_MEM(aStatement),
                                                           qdPartitionedIndex,
                                                           1,
                                                           & sPartIndex )
                                  != IDE_SUCCESS );

                        QD_SET_INIT_PARTITIONED_INDEX(sPartIndex);

                        sCurrConstr->partIndex = sPartIndex;
                    }
                }
            }
            else
            {
                /* Non-Partitioned Table�� USING INDEX ������
                 * LOCAL PARTITIONED INDEX�� ���� �����ϸ� ����
                 */
                if( sCurrConstr->partIndex != NULL )
                {
                    IDE_TEST_RAISE( (sCurrConstr->partIndex->partIndexType !=
                                     QCM_NONE_PARTITIONED_INDEX),
                                    ERR_CREATE_PART_INDEX_ON_NONE_PART_TABLE );
                }
                else
                {
                    /* �ش� constraint�� USING INDEX���� ������� ���� ���
                     * Nothing to do
                     */
                }
            }
        }
        /* PROJ-1107 Check Constraint ���� */
        else if ( sCurrConstr->constrType == QD_CHECK )
        {
            /* Check Constraint�� Nchar List�� ���Ѵ�. */
            IDE_TEST( qdbCommon::makeNcharLiteralStrForConstraint(
                                    aStatement,
                                    sParseTree->ncharList,
                                    sCurrConstr )
                      != IDE_SUCCESS );

            /* Default�� Nchar List���� Check�� ���� �����Ѵ�. */
            if ( aConstraintOption == QDN_ON_MODIFY_COLUMN )
            {
                sDefaultColumns = sParseTree->modifyColumns;
            }
            else
            {
                sDefaultColumns = sParseTree->columns;
            }

            for ( sColumn = sDefaultColumns;
                  sColumn != NULL;
                  sColumn = sColumn->next )
            {
                /* CREATE TABLE T1 ( I1 NCHAR DEFAULT N'��'||N'��' CHECK ( I1 = '��' ),
                 *                   I2 ... );
                 * �� Table�� ������ ��,
                 * DEFAULT�� �����ڰ� ��� '��', '��', '��'�� DEFAULT�� Nchar Literal�� ����Ѵ�.
                 * �׷���, '��'�� Check�� ���̹Ƿ�, DEFAULT�� Nchar Literal List���� '��'�� �����Ѵ�.
                 * ALTER TABLE ... ADD CONSTRAINT �� �����ϰ� �����Ѵ�.
                 */
                qdbCommon::removeNcharLiteralStr( &(sColumn->ncharLiteralPos),
                                                  sCurrConstr->ncharList );
            }

            /* LOB�� �������� �ʴ´�. */
            for ( sColumn = sCurrConstr->constraintColumns;
                  sColumn != NULL;
                  sColumn = sColumn->next )
            {
                if ( (sColumn->basicInfo->module->flag & MTD_COLUMN_TYPE_MASK)
                                                      == MTD_COLUMN_TYPE_LOB )
                {
                    sqlInfo.setSourceInfo( aStatement, &(sColumn->namePos) );
                    IDE_RAISE( ERR_USE_LOB_IN_CHECK_CONSTRAINT );
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
                                    sCurrConstr->checkCondition,
                                    sCurrConstr->checkCondition )
                      != IDE_SUCCESS );

            /* Estimate�� �����Ѵ�. */
            if ( aConstraintOption == QDN_ON_ADD_CONSTRAINT )
            {
                IDE_TEST( qdbCommon::validateCheckConstrDefinition(
                                        aStatement,
                                        sCurrConstr,
                                        NULL,
                                        sParseTree->from )
                          != IDE_SUCCESS );
            }
            else
            {
                /* Nothing to do */
            }
        }
        else
        {
            // Nothing To Do
        }
    }

    for (sCurrConstr = aConstr;
         sCurrConstr != NULL;
         sCurrConstr = sCurrConstr->next)
    {
        IDE_TEST(validateDuplicateConstraintSpec( sCurrConstr,
                                                  aTableInfo,
                                                  aConstr) != IDE_SUCCESS);
    }

    /* BUG-38721 Compressed column with the PK, unique, timestamp type reason not to support for table. */
    for ( sCurrConstr = aConstr ;
          sCurrConstr != NULL ;
          sCurrConstr = sCurrConstr->next )
    {
        for ( sColumn = sCurrConstr->constraintColumns ;
              sColumn != NULL ;
              sColumn = sColumn->next )
        {
            if ( ( sColumn->basicInfo->column.flag &
                   SMI_COLUMN_COMPRESSION_MASK )
                 == SMI_COLUMN_COMPRESSION_TRUE )
            {
                if ( ( sCurrConstr->constrType == QD_UNIQUE ) ||
                     ( sCurrConstr->constrType == QD_TIMESTAMP ) ||
                     ( sCurrConstr->constrType == QD_PRIMARYKEY ) )
                {
                    sqlInfo.setSourceInfo( aStatement,
                                           &( sColumn->namePos ) );

                    IDE_RAISE( ERR_NOT_SUPPORT_TYPE );
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

    if ( aUniqueKeyCount != NULL )
    {
        // To Fix UMR
        *aUniqueKeyCount = 0;

        for (sCurrConstr = aConstr;
             sCurrConstr != NULL;
             sCurrConstr = sCurrConstr->next)
        {
            if( (sCurrConstr->constrType == QD_PRIMARYKEY) ||
                (sCurrConstr->constrType == QD_UNIQUE) ||
                (sCurrConstr->constrType == QD_LOCAL_UNIQUE) )
            {
                (*aUniqueKeyCount)++;
            }
        }
    }

    IDE_FT_END();

    return IDE_SUCCESS;

    IDE_EXCEPTION_SIGNAL()
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_FAULT_TOLERATED ) );
    }
    IDE_EXCEPTION(ERR_NOT_SUPPORT_TYPE)
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(
                qpERR_ABORT_QDN_NOT_SUPPORT_CONSTRAINT_IN_COMPRESSED_COLUMN,
                sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION(ERR_DUP_CONSTR_NAME);
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QDN_DUPLICATE_CONSTRAINT,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION(ERR_DUP_PRIMARY);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDN_DUPLICATE_PRIMARY_KEY));
    }
    IDE_EXCEPTION(ERR_DUP_NOTNULL);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDN_DUPLICATE_CONSTRAINT_SPEC ));
    }
    IDE_EXCEPTION(ERR_NOT_EXIST_COLUMN);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCM_NOT_EXIST_COLUMN));
    }
    IDE_EXCEPTION(ERR_ADD_COL_NO_DEFAULT_NOTNULL);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDN_ADD_COL_NO_DEFAULT_NOTNULL));
    }
    IDE_EXCEPTION(ERR_MAX_KEY_COLUMN_COUNT);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDN_MAX_KEY_COLUMN_COUNT));
    }
    IDE_EXCEPTION(ERR_NOT_ALLOW_PK_AND_UNIQUE);
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(
                qpERR_ABORT_QDN_NOT_ALLOWED_PRIMARY_AND_UNIQUE_KEY,
                sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION(ERR_LOCAL_UNIQUE_KEY_ON_NON_PART_TABLE);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDN_CANNOT_CREATE_LOCAL_UNIQUE_KEY_CONSTR_ON_NON_PART_TABLE));
    }
    IDE_EXCEPTION(ERR_CREATE_PART_INDEX_ON_NONE_PART_TABLE)
    {
        IDE_SET(ideSetErrorCode(
                qpERR_ABORT_QDX_CANNOT_CREATE_PART_INDEX_ON_NONE_PART_TABLE));
    }
    IDE_EXCEPTION(ERR_NOT_SUPPORTED_CONSTR_STATE);
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QDN_NOT_SUPPORTED_CONSTR_STATE,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION( ERR_USE_LOB_IN_CHECK_CONSTRAINT );
    {
        (void)sqlInfo.init( aStatement->qmeMem );
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDN_NOT_SUPPORT_LOB_COLUMN_IN_CHECK_CONSTRAINT,
                                  sqlInfo.getErrMessage() ) );
        (void)sqlInfo.fini();
    }
    /* BUG-40099 */
    IDE_EXCEPTION (ERR_CANNOT_ALLOW_TBS_NAME_FOR_TEMPORARY_INDEX )
    {
        IDE_SET(ideSetErrorCode( qpERR_ABORT_QDX_TEMPORARY_INDEX_NOT_ALLOW_TBS_NAME ));
    }
    IDE_EXCEPTION_END;

    IDE_FT_EXCEPTION_BEGIN();

    switch (sStage)
    {
        case 1:
            sTmpCursor.close();
    }

    IDE_FT_EXCEPTION_END();

    return IDE_FAILURE;

#undef IDE_FN
}

/***********************************************************************
 *
 * Description :
 *    PROJ-2461 pk, uk constraint���� prefix index ���� ��ȭ 
 *    table�� partition key�� constraint column(pk/uk�� index key)�� ���ԵǴ��� üũ
 *    �� local index�� ���̺� ��ü�� uniqueness�� ������ �� �ִ��� üũ�Ѵ�.
 *
 *    partition key ���ΰ� constraint column�� �ִٸ� aIsLocalIndex�� TRUE�� �Ѵ�.
 *
 *    ���� �׷��� �ʰ�, partition key �Ϻ�/��ü�� constraint column�� ���ԵǾ� ���� �ʴٸ�
 *    1) PDT -> PK/UK�� ����� �ε����� �۷ι� ����Ƽ�ǵ� �ε����� ����
 *           -> aIsLocalIndex�� FALSE�� �Ѵ�.
 *    2) PMT -> PMT���� PK/UK�� �ݵ�� local index ��� �ϹǷ� ���� �߻���Ų��.
 *
 * Implementation : 
 *
 ***********************************************************************/
IDE_RC qdn::checkLocalIndex( qcStatement      * aStatement,
                             qdConstraintSpec * aConstr,
                             qcmTableInfo     * aTableInfo,
                             idBool           * aIsLocalIndex,
                             idBool             aIsDiskTBS )
{
    qdTableParseTree   * sParseTree;
        
    sParseTree = (qdTableParseTree *)aStatement->myPlan->parseTree;

    if ( aTableInfo != NULL )
    {
        /* called by alter table */
        IDE_TEST( qdx::checkLocalIndexOnAlterTable( aStatement,
                                                    aTableInfo,
                                                    aConstr->constraintColumns,
                                                    aTableInfo->partKeyColumns,
                                                    aIsLocalIndex )
                  != IDE_SUCCESS );
    }
    else
    {
        /* called by create table */
        IDE_TEST( qdx::checkLocalIndexOnCreateTable( aConstr->constraintColumns,
                                                     sParseTree->partTable->partKeyColumns,
                                                     aIsLocalIndex )
                  != IDE_SUCCESS );

    }

    /* Global Index�� Disk Partitioned Table���� ����� �� �ִ�. */
    IDE_TEST_RAISE( ( *aIsLocalIndex != ID_TRUE ) && ( aIsDiskTBS != ID_TRUE ),
                    ERR_GLOBAL_INDEX_IN_NON_DISK_TABLE );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_GLOBAL_INDEX_IN_NON_DISK_TABLE )
    {
        IDE_SET( ideSetErrorCode(
                     qpERR_ABORT_QDN_NOT_ALLOW_MEM_TBS_PK_UK_OF_GLOBAL_INDEX ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdn::validateDuplicateConstraintSpec(
    qdConstraintSpec *aConstr,
    qcmTableInfo *aTableInfo,
    qdConstraintSpec *aAllConstr)
{
/***********************************************************************
 *
 * Description :
 *    validateConstraints ���� ȣ��, Constraint �ߺ� �˻�
 *
 * Implementation :
 *    constraint Ÿ���� PRIMARY KEY �Ǵ� UNIQUE �� ��
 *    1. �̹� �ִ� uniquekey �Ǵ� �ε����� �÷��� �ߺ��Ǵ��� üũ
 *    2. �߰��ϴ� constraint ���̿��� �÷������� �ߺ��� ���� ������ üũ
 *
 *    constraint Ÿ���� NOT NULL �� ��
 *    1. �̹� �ִ� NOT NULL constraint �� �÷��� �ߺ��Ǵ��� üũ
 *    2. �߰��ϴ� constraint ���̿��� �÷������� �ߺ��� ���� ������ üũ
 *
 *    constraint Ÿ���� CHECK �� ��, �ߺ� ���
 *
 *    constraint Ÿ���� FOREIGN KEY �� ��
 *    1. �̹� �ִ� FOREIGN KEY �� �����ϴ� ���̺�.�÷��� �ε����� �ߺ� �Ǵ��� üũ
 *    2. �߰��ϴ� FOREIGN KEY ���̿��� �����ϴ� ������ �ߺ��� ���� ������ üũ
 *
 ***********************************************************************/

#define IDE_FN "qdn::validateDuplicateConstraintSpec"

    qdConstraintSpec * sTempConstr;
    UInt               i;

    if( (aConstr->constrType == QD_PRIMARYKEY) ||
        (aConstr->constrType == QD_UNIQUE) ||
        (aConstr->constrType == QD_LOCAL_UNIQUE) )
    {
        // 1. �̹� �ִ� uniquekey �Ǵ� �ε����� �÷��� �ߺ��Ǵ��� üũ
        if (aTableInfo != NULL)
        {
            for (i = 0; i < aTableInfo->uniqueKeyCount; i++)
            {
                IDE_TEST_RAISE(matchColumnId(
                                   aConstr->constraintColumns,
                                   aTableInfo->uniqueKeys[i].constraintColumn,
                                   aTableInfo->uniqueKeys[i].constraintColumnCount,
                                   aTableInfo->uniqueKeys[i].constraintIndex->keyColsFlag)
                               == ID_TRUE, ERR_DUPLICATE_CONSTRAINT);
            }
            for (i = 0; i < aTableInfo->indexCount; i++)
            {
                IDE_TEST_RAISE(matchColumnId(
                                   aConstr->constraintColumns,
                                   (UInt*) smiGetIndexColumns(
                                       aTableInfo->indices[i].indexHandle),
                                   aTableInfo->indices[i].keyColCount,
                                   aTableInfo->indices[i].keyColsFlag)
                               == ID_TRUE, ERR_DUPLICATED_INDEX_COLS);
            }
        }

        // 2. �߰��ϴ� constraint ���̿��� �÷������� �ߺ��� ���� ������ üũ
        sTempConstr = aAllConstr;
        while (sTempConstr != NULL)
        {
            if (sTempConstr != aConstr)
            {
                if( (sTempConstr->constrType == QD_PRIMARYKEY) ||
                    (sTempConstr->constrType == QD_UNIQUE) ||
                    (sTempConstr->constrType == QD_LOCAL_UNIQUE) )
                {
                    IDE_TEST_RAISE(matchColumnOffsetLengthOrder(
                                       sTempConstr->constraintColumns,
                                       aConstr->constraintColumns)
                                   == QDN_MATCH_COLUMN_WITH_ORDER,
                                   ERR_DUPLICATE_CONSTRAINT);
                }
            }
            sTempConstr = sTempConstr->next;
        }
    }
    else if (aConstr->constrType == QD_NOT_NULL)
    {
        // 1. �̹� �ִ� NOT NULL constraint �� �÷��� �ߺ��Ǵ��� üũ
        if (aTableInfo != NULL)
        {
            for (i = 0; i < aTableInfo->notNullCount; i++)
            {
                IDE_TEST_RAISE(matchColumnId(
                                   aConstr->constraintColumns,
                                   aTableInfo->notNulls[i].constraintColumn,
                                   aTableInfo->notNulls[i].constraintColumnCount,
                                   NULL ) == ID_TRUE,
                               ERR_DUPLICATE_CONSTRAINT);
            }
        }

        // 2. �߰��ϴ� constraint ���̿��� �÷������� �ߺ��� ���� ������ üũ
        sTempConstr = aAllConstr;
        while (sTempConstr != NULL)
        {
            if( (sTempConstr != aConstr) &&
                (sTempConstr->constrType == QD_NOT_NULL) )
            {
                IDE_TEST_RAISE(matchColumnOffsetLengthOrder(
                                   sTempConstr->constraintColumns,
                                   aConstr->constraintColumns)
                               == QDN_MATCH_COLUMN_WITH_ORDER,
                               ERR_DUPLICATE_CONSTRAINT);
            }
            sTempConstr = sTempConstr->next;
        }
    }
    /* PROJ-1107 Check Constraint ���� */
    else if ( aConstr->constrType == QD_CHECK )
    {
        /* �ߺ��� ����Ѵ�. */
    }
    else if (aConstr->constrType == QD_FOREIGN)
    {
        // 1. �̹� �ִ� FOREIGN KEY �� �����ϴ� ���̺�.�÷��� �ε����� �ߺ� �Ǵ��� üũ
        if (aTableInfo != NULL)
        {
            for (i = 0; i < aTableInfo->foreignKeyCount; i++)
            {
                if ( matchColumnId(
                         aConstr->constraintColumns,
                         aTableInfo->foreignKeys[i].referencingColumn,
                         aTableInfo->foreignKeys[i].constraintColumnCount,
                         NULL ) == ID_TRUE)
                {
                    if (aConstr->referentialConstraintSpec != NULL)
                    {
                        IDE_TEST_RAISE(
                            (aConstr->referentialConstraintSpec->referencedTableID
                             == aTableInfo->foreignKeys[i].referencedTableID) &&
                            (aConstr->referentialConstraintSpec->referencedIndexID
                             == aTableInfo->foreignKeys[i].referencedIndexID),
                            ERR_DUPLICATE_CONSTRAINT);
                    }
                }
            }
        }

        // 2. �߰��ϴ� FOREIGN KEY ���̿��� �����ϴ� ������ �ߺ��� ���� ������ üũ
        sTempConstr = aAllConstr;
        while (sTempConstr != NULL)
        {
            if( (sTempConstr != aConstr) &&
                (sTempConstr->constrType == QD_FOREIGN) )
            {
                if (matchColumnOffsetLengthOrder(
                        sTempConstr->constraintColumns,
                        aConstr->constraintColumns)
                    == QDN_MATCH_COLUMN_WITH_ORDER)
                {
                    if ( ( aConstr->referentialConstraintSpec != NULL ) &&
                         ( sTempConstr->referentialConstraintSpec != NULL ) )
                    {
                        if( ( aConstr->referentialConstraintSpec->referencedTableID
                             == sTempConstr->referentialConstraintSpec->referencedTableID )
                            &&
                            ( aConstr->referentialConstraintSpec->referencedIndexID
                             == sTempConstr->referentialConstraintSpec->referencedIndexID ) )
                        {
                            // BUG-27001 ���� �ε����� ���������� ���� ������ �ٸ� �� �����Ƿ�
                            // foreign key ������ �Ŀ� ���� �������� ���ƾ߸� �ߺ����� �Ǵ�
                            IDE_TEST_RAISE(
                                matchColumnList( aConstr->constraintColumns,
                                                 sTempConstr->constraintColumns )
                                == ID_TRUE,
                                ERR_DUPLICATE_CONSTRAINT );
                        }
                    }
                }
            }
            sTempConstr = sTempConstr->next;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_DUPLICATED_INDEX_COLS);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDX_DUPLICATE_INDEX_COLS));
    }

    IDE_EXCEPTION(ERR_DUPLICATE_CONSTRAINT);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDN_DUPLICATE_CONSTRAINT_SPEC));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

// if two column list matches, return true.
// else retrun false.
SInt qdn::matchColumnOffsetLengthOrder(qcmColumn *aColList1,
                                       qcmColumn *aColList2)
{
#define IDE_FN "qdn::matchColumnOffsetLengthOrder"

    qcmColumn *sColumn1;
    qcmColumn *sColumn2;
    SInt       sRetVal = QDN_MATCH_COLUMN_WITH_ORDER;

    sColumn1 = aColList1;
    sColumn2 = aColList2;

    while(sRetVal != QDN_NOT_MATCH_COLUMN)
    {
        if (sColumn1 == NULL && sColumn2 == NULL)
        {
            break;
        }
        else if ((sColumn1 != NULL && sColumn2 == NULL) ||
                 (sColumn1 == NULL && sColumn2 != NULL))
        {
            sRetVal = QDN_NOT_MATCH_COLUMN;
            break;
        }

        else
        {
            if ((sColumn1->basicInfo->column.offset ==
                 sColumn2->basicInfo->column.offset) &&
                (sColumn1->basicInfo->column.size ==
                 sColumn2->basicInfo->column.size))
            {
                if (sColumn1->basicInfo->column.flag !=
                    sColumn2->basicInfo->column.flag)
                {
                    sRetVal = QDN_NOT_MATCH_COLUMN;
                    break;
                }
                /* PROJ-2419
                 * smiColumn �� ��� varOrder �߰��Ǿ����Ƿ� �̰͵� ���ؾ��Ѵ�. */
                if ( sColumn1->basicInfo->column.varOrder !=
                     sColumn2->basicInfo->column.varOrder )
                {
                    sRetVal = QDN_NOT_MATCH_COLUMN;
                    break;
                }
                else
                {
                    /* Nothing to do */
                }
            }
            else
            {
                sRetVal = QDN_NOT_MATCH_COLUMN;
                break;
            }
            sColumn1 = sColumn1->next;
            sColumn2 = sColumn2->next;
        }
    }

    return sRetVal;

#undef IDE_FN
}

IDE_RC qdn::getColumnFromDefinitionList(
    qcStatement       * aStatement,
    qcmColumn         * aColumnList,
    qcNamePosition      aColumnName,
    qcmColumn        ** aColumn)
{
/***********************************************************************
 *
 * Description :
 *    aColumnList ���� aColumnName �� �÷��� ã�Ƽ� ��ȯ
 *
 * Implementation :
 *    1. aColumnList ���� aColumnName �� �̸��� ������ �÷��� ã�´�
 *
 ***********************************************************************/

#define IDE_FN "qdn::getColumnFromDefinitionList"
    qcmColumn               * sColumn;
    qcuSqlSourceInfo          sqlInfo;

    for (sColumn = aColumnList; sColumn != NULL; sColumn = sColumn->next)
    {
        if ( QC_IS_NAME_MATCHED( sColumn->namePos, aColumnName ) )
        {
            *aColumn = sColumn;
            break;
        }
    }

    if (sColumn == NULL)
    {
        sqlInfo.setSourceInfo(
            aStatement,
            & aColumnName );
        IDE_RAISE(ERR_NOT_EXIST_COLUMN_IN_DEFINITION_LIST);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_NOT_EXIST_COLUMN_IN_DEFINITION_LIST);
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QMV_NOT_EXISTS_COLUMN,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

/***********************************************************************
 * EXECUTE
 **********************************************************************/

IDE_RC qdn::executeAddConstr(qcStatement * aStatement)
{
/***********************************************************************
 *
 * Description :
 *      ALTER TABLE ... ADD CONSTRAINT ����
 *
 * Implementation :
 *      1. primary key �Ǵ� unique �� ������ ����
 *      2. Foreign key �� ������ ����
 *      3. check constraint �� ������ ����
 *      4. qcm::touchTable
 *      5. ��Ÿ ĳ�� �籸��
 *
 ***********************************************************************/

#define IDE_FN "qdn::executeAddConstr"

    // To Fix PR-10909
    qdTableParseTree        * sParseTree;
    smOID                     sOldTableOID = 0;
    smOID                     sNewTableOID = 0;
    UInt                      sTableID;
    smSCN                     sSCN;
    void                    * sTableHandle;
    qcmTableInfo            * sOldTableInfo = NULL;
    qcmTableInfo            * sNewTableInfo = NULL;
    qcmPartitionInfoList    * sOldPartInfoList = NULL;
    qcmPartitionInfoList    * sNewPartInfoList = NULL;
    qdConstraintState       * sConstraintState;
    qdIndexTableList        * sNewIndexTable = NULL;
    smOID                   * sOldPartitionOID = NULL;
    UInt                      sOldPartitionCount = 0;
    idBool                    sIsReplicatedTable = ID_FALSE;
    smOID                   * sOldTableOIDArray = NULL;
    smOID                   * sNewTableOIDArray = NULL;
    UInt                      sTableOIDCount = 0;
    UInt                      sDDLSupplementalLog = QCU_DDL_SUPPLEMENTAL_LOG;


    sParseTree = (qdTableParseTree *)aStatement->myPlan->parseTree;
    sConstraintState = sParseTree->constraints->constrState;

    // TASK-2176
    // Table�� ���� Lock�� ȹ���Ѵ�.
    IDE_TEST( qcm::validateAndLockTable(aStatement,
                                        sParseTree->tableHandle,
                                        sParseTree->tableSCN,
                                        SMI_TABLE_LOCK_X)
              != IDE_SUCCESS);

    sOldTableInfo = sParseTree->tableInfo;

    // PROJ-1407 Temporary table
    // session temporary table�� �����ϴ� ��� DDL�� �� �� ����.
    IDE_TEST_RAISE( qcuTemporaryObj::existSessionTable( sOldTableInfo ) == ID_TRUE,
                    ERR_SESSION_TEMPORARY_TABLE_EXIST );

    if( sOldTableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
    {
        // ��� ��Ƽ�ǿ� LOCK(X)
        IDE_TEST( qcmPartition::validateAndLockPartitionInfoList( aStatement,
                                                                  sParseTree->partTable->partInfoList,
                                                                  SMI_TBSLV_DDL_DML, // TBS Validation �ɼ�
                                                                  SMI_TABLE_LOCK_X,
                                                                  smiGetDDLLockTimeOut((QC_SMI_STMT(aStatement))->getTrans()))
                  != IDE_SUCCESS );

        // ���� ó���� ���Ͽ�, Lock�� ���� �Ŀ� Partition List�� �����Ѵ�.
        sOldPartInfoList = sParseTree->partTable->partInfoList;

        if ( ( sOldTableInfo->replicationCount > 0 ) ||
             ( sDDLSupplementalLog == 1 ) )
        {
            IDE_TEST( qcmPartition::getAllPartitionOID( QC_QMX_MEM(aStatement),
                                                        sOldPartInfoList,
                                                        &sOldPartitionOID,
                                                        &sOldPartitionCount )
                      != IDE_SUCCESS );
        }
        else
        {
            /* do nothing */
        }

    }

    sOldTableOID = smiGetTableId(sOldTableInfo->tableHandle);

    // PROJ-2642 Table on Replication Allow DDL
    if ( sOldTableInfo->replicationCount > 0 )
    {
        if ( sOldTableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
        {
            sOldTableOIDArray = sOldPartitionOID;
            sTableOIDCount = sOldPartitionCount;
        }
        else
        {
            sOldTableOIDArray = &sOldTableOID;
            sTableOIDCount = 1;
        }

        IDE_TEST( qdn::checkOperatableForReplication( aStatement,
                                                      sOldTableInfo,
                                                      sParseTree->constraints->constrType,
                                                      sOldTableOIDArray,
                                                      sTableOIDCount )
                  != IDE_SUCCESS );

        sIsReplicatedTable = ID_TRUE;
    }
    else
    {
        IDE_DASSERT( sParseTree->tableInfo->replicationRecoveryCount == 0 );
    }

    switch ( sParseTree->constraints->constrType )
    {
        case QD_PRIMARYKEY:
        case QD_UNIQUE:
        case QD_LOCAL_UNIQUE:
            IDE_TEST( qdbCommon::createConstrPrimaryUnique(
                         aStatement,
                         sOldTableOID,
                         sParseTree->constraints,
                         sOldTableInfo->tableOwnerID,
                         sOldTableInfo->tableID,
                         sOldPartInfoList,
                         sOldTableInfo->maxrows )
                      != IDE_SUCCESS );

            sNewIndexTable = sParseTree->newIndexTables;
            
            break;

        case QD_FOREIGN:
            // To Fix PR-10385
            //  Foreign Key ���� ������ �����ϱ� ����
            //  �̹� �����ϴ� Data�� ���Ͽ� ������ �˻��Ͽ��� �Ѵ�.
            // PROJ-1874 Novalidate
            //  Constraint state�� �������� �ʾҰų� Validate�� ������������ �˻��Ѵ�.
            if ( sConstraintState == NULL )
            {
                IDE_TEST( qdnForeignKey::checkRef4AddConst(
                              aStatement,
                              sOldTableInfo,
                              sParseTree->constraints )
                          != IDE_SUCCESS );
            }
            else
            {
                if ( sConstraintState->validate == ID_TRUE )
                {
                    IDE_TEST( qdnForeignKey::checkRef4AddConst(
                                  aStatement,
                                  sOldTableInfo,
                                  sParseTree->constraints )
                              != IDE_SUCCESS );
                }
                else
                {
                    // Nothing To Do
                }
            }

            IDE_TEST( qdbCommon::createConstrForeign( aStatement,
                                                      sParseTree->constraints,
                                                      sOldTableInfo->tableOwnerID,
                                                      sOldTableInfo->tableID )
                      != IDE_SUCCESS );
            break;

        case QD_CHECK:
            // PROJ-1874 Novalidate
            //  Constraint state�� �������� �ʾҰų� Validate�� ������������ �˻��Ѵ�.
            if ( sConstraintState == NULL )
            {
                IDE_TEST( qdnCheck::verifyCheckConstraintListWithFullTableScan(
                              aStatement,
                              sParseTree->from->tableRef,
                              sParseTree->constraints )
                          != IDE_SUCCESS );
            }
            else
            {
                if( sConstraintState->validate == ID_TRUE )
                {
                    IDE_TEST( qdnCheck::verifyCheckConstraintListWithFullTableScan(
                                  aStatement,
                                  sParseTree->from->tableRef,
                                  sParseTree->constraints )
                              != IDE_SUCCESS );
                }
                else
                {
                    // Nothing To Do
                }
            }

            /* PROJ-1107 Check Constraint ���� */
            IDE_TEST( qdbCommon::createConstrCheck( aStatement,
                                                    sParseTree->constraints,
                                                    sOldTableInfo->tableOwnerID,
                                                    sOldTableInfo->tableID,
                                                    sParseTree->relatedFunctionNames )
                      != IDE_SUCCESS );
            break;

        default:
            break;
    }

    sTableID = sOldTableInfo->tableID;

    IDE_TEST(qcm::touchTable(QC_SMI_STMT( aStatement ),
                             sTableID,
                             SMI_TBSLV_DDL_DML )
             != IDE_SUCCESS);

    IDE_TEST(qcm::makeAndSetQcmTableInfo(
                 QC_SMI_STMT( aStatement ),
                 sTableID,
                 sOldTableOID) != IDE_SUCCESS);

    IDE_TEST(qcm::getTableInfoByID(aStatement,
                                   sTableID,
                                   &sNewTableInfo,
                                   &sSCN,
                                   &sTableHandle) != IDE_SUCCESS);

    //---------------------------
    // build new index table
    //---------------------------

    if ( sNewIndexTable != NULL )
    {
        // new index table�� �Ѱ��̴�.
        IDE_DASSERT( sNewIndexTable->next == NULL );
        
        IDE_TEST( qdx::buildIndexTable( aStatement,
                                        sNewIndexTable,
                                        sParseTree->constraints->constraintColumns,
                                        sParseTree->constraints->constrColumnCount,
                                        sNewTableInfo,
                                        sOldPartInfoList )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }
    
    /* PROJ-1723 [MDW/INTEGRATOR] Altibase Plugin ����
       DDL Statement Text�� �α�
    */
    if ( sDDLSupplementalLog == 1 )
    {
        IDE_TEST( qciMisc::writeDDLStmtTextLog( aStatement,
                                                sOldTableInfo->tableOwnerID,
                                                sOldTableInfo->name )
                  != IDE_SUCCESS );
    }

    // PROJ-1502 PARTITIONED DISK TABLE
    if( sNewTableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
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

    // PROJ-2642 Table on Replication Allow DDL
    if ( ( sIsReplicatedTable == ID_TRUE ) ||
         ( sDDLSupplementalLog == 1 ) )
    {
        sNewTableOID = smiGetTableId( sNewTableInfo->tableHandle );

        if ( sOldTableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
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
            sOldTableOIDArray = &sOldTableOID;
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

    if ( ( sParseTree->tableInfo->replicationCount > 0 ) ||
         ( QCG_GET_SESSION_IS_NEED_DDL_INFO( aStatement ) == ID_TRUE ) )
    {
        qrc::setDDLDestInfo( aStatement, 
                             1,
                             &(sNewTableInfo->tableOID),
                             0,
                             NULL );
    }

    if ( sNewTableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
    {
        (void)qcmPartition::destroyQcmPartitionInfoList( sOldPartInfoList );
    }
    else
    {
        /* do nothing */
    }

    (void)qcm::destroyQcmTableInfo(sOldTableInfo);

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_SESSION_TEMPORARY_TABLE_EXIST )
    {
        IDE_SET(ideSetErrorCode( qpERR_ABORT_QDB_TEMPORARY_TABLE_DDL_DISABLE ));
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

    qcmPartition::restoreTempInfo( sOldTableInfo,
                                   sOldPartInfoList,
                                   NULL );

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qdn::executeRenameConstr(qcStatement * aStatement)
{
/***********************************************************************
 *
 * Description :
 *      ALTER TABLE ... RENAME CONSTRAINT ����
 *
 * Implementation :
 *      1. Constraint �̸����� ConstraintID ���ϱ�
 *      2. SYS_CONSTRAINTS_, ��Ÿ ���̺��� �̸�����
 *      3. ��Ÿ ĳ�� �籸��
 *
 ***********************************************************************/

    // To Fix PR-10909
    qdTableParseTree        * sParseTree;
    UInt                      sConstraintID;
    qcmIndex                * sIndex = NULL;
    UInt                      sTableID;
    qcmTableInfo            * sOldTableInfo = NULL;
    qcmTableInfo            * sNewTableInfo = NULL;
    SChar                   * sSqlStr;
    SChar                     sConstraintName[ QC_MAX_OBJECT_NAME_LEN + 1 ];
    SChar                     sNewConstraintName[ QC_MAX_OBJECT_NAME_LEN + 1 ];
    vSLong                    sRowCnt;
    void                    * sTableHandle;
    smSCN                     sSCN;
    qcmPartitionInfoList    * sOldPartInfoList = NULL;
    qcmPartitionInfoList    * sNewPartInfoList = NULL;
    idBool                    sIsPartitioned = ID_FALSE;
    idBool                    sExistSameConstr = ID_FALSE;
    
    // PROJ-1624 global non-partitioned index
    qdIndexTableList        * sOldIndexTable = NULL;
    qcmIndex                * sIndexTableIndex[2];
    SChar                     sIndexTableName[ QC_MAX_OBJECT_NAME_LEN + 1 ];
    SChar                     sKeyIndexName[ QC_MAX_OBJECT_NAME_LEN + 1 ];
    SChar                     sRidIndexName[ QC_MAX_OBJECT_NAME_LEN + 1 ];
    qcNamePosition            sIndexTableNamePos;
    qcNamePosition            sIndexNamePos;
    qcNamePosition            sUserNamePos;
    qcmTableInfo            * sNewIndexTableInfo = NULL;

    smOID                   * sOldPartitionOID = NULL;
    UInt                      sOldPartitionCount = 0;
    idBool                    sIsReplicatedTable = ID_FALSE;
    smOID                     sOldTableOID = 0;
    smOID                     sNewTableOID = 0;
    smOID                   * sOldTableOIDArray = NULL;
    smOID                   * sNewTableOIDArray = NULL;
    UInt                      sTableOIDCount = 0;

    sParseTree = (qdTableParseTree *)aStatement->myPlan->parseTree;

    // TASK-2176
    // Table�� ���� Lock�� ȹ���Ѵ�.
    IDE_TEST( qcm::validateAndLockTable(aStatement,
                                        sParseTree->tableHandle,
                                        sParseTree->tableSCN,
                                        SMI_TABLE_LOCK_X)
              != IDE_SUCCESS);

    sOldTableInfo = sParseTree->tableInfo;
    sTableID = sOldTableInfo->tableID;
    sOldTableOID = smiGetTableId(sOldTableInfo->tableHandle);

    // PROJ-1502 PARTITIONED DISK TABLE
    if( sOldTableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
    {
        sIsPartitioned = ID_TRUE;

        // ��� ��Ƽ�ǿ� LOCK(X)
        IDE_TEST( qcmPartition::validateAndLockPartitionInfoList( aStatement,
                                                                  sParseTree->partTable->partInfoList,
                                                                  SMI_TBSLV_DDL_DML, // TBS Validation �ɼ�
                                                                  SMI_TABLE_LOCK_X,
                                                                  smiGetDDLLockTimeOut((QC_SMI_STMT(aStatement))->getTrans()))
                  != IDE_SUCCESS );

        // ���� ó���� ���Ͽ�, Lock�� ���� �Ŀ� Partition List�� �����Ѵ�.
        sOldPartInfoList = sParseTree->partTable->partInfoList;
        
        sOldIndexTable = sParseTree->oldIndexTables;
    
        // PROJ-1624 global non-partitioned index
        if ( sOldIndexTable != NULL )
        {
            IDE_TEST( qdx::validateAndLockIndexTableList( aStatement,
                                                          sOldIndexTable,
                                                          SMI_TBSLV_DDL_DML, // TBS Validation �ɼ�
                                                          SMI_TABLE_LOCK_X,
                                                          smiGetDDLLockTimeOut((QC_SMI_STMT(aStatement))->getTrans()))
                      != IDE_SUCCESS );
        }
        else
        {
            // Nothing to do.
        }

        if ( sOldTableInfo->replicationCount > 0 )
        {
            IDE_TEST( qcmPartition::getAllPartitionOID( QC_QMX_MEM(aStatement),
                                                        sOldPartInfoList,
                                                        &sOldPartitionOID,
                                                        &sOldPartitionCount )
                      != IDE_SUCCESS );
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

    QC_STR_COPY( sConstraintName, sParseTree->constraints->constrName );

    QC_STR_COPY( sNewConstraintName, sParseTree->constraints->next->constrName );

    // constraint�� �����ؾ� ��.
    sConstraintID = qcmCache::getConstraintIDByName(sParseTree->tableInfo,
                                                    sConstraintName,
                                                    &sIndex);

    IDE_TEST_RAISE(sConstraintID == 0, ERR_NOT_EXIST_CONSTRAINT_NAME);

    // PROJ-2642 Table on Replication Allow DDL
    if ( sOldTableInfo->replicationCount > 0 )
    {
        if ( sOldTableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
        {
            sOldTableOIDArray = sOldPartitionOID;
            sTableOIDCount = sOldPartitionCount;
        }
        else
        {
            sOldTableOIDArray = &sOldTableOID;
            sTableOIDCount = 1;
        }

        IDE_TEST( qdn::checkOperatableForReplication( aStatement,
                                                      sOldTableInfo,
                                                      getConstraintType( sOldTableInfo,
                                                                         sConstraintID ),
                                                      sOldTableOIDArray,
                                                      sTableOIDCount )
                  != IDE_SUCCESS );

        sIsReplicatedTable = ID_TRUE;
    }
    else
    {
        /* Nothing to do */
    }

    // ���ο� �̸��� constraint�� �����ϸ� �ȵ�.
    IDE_TEST( existSameConstrName(aStatement,
                                  sNewConstraintName,
                                  sOldTableInfo->tableOwnerID,
                                  &sExistSameConstr)
              != IDE_SUCCESS );

    IDE_TEST_RAISE(sExistSameConstr == ID_TRUE, ERR_EXIST_OBJECT_NAME);

    // PROJ-1624 global non-partitioned index
    if ( sOldIndexTable != NULL )
    {
        sUserNamePos.stmtText = sOldTableInfo->tableOwnerName;
        sUserNamePos.offset   = 0;
        sUserNamePos.size     = idlOS::strlen(sOldTableInfo->tableOwnerName);
        
        // ���̸� �˻�
        IDE_TEST( qdx::checkIndexTableName( aStatement,
                                            sUserNamePos,
                                            sParseTree->constraints->next->constrName,
                                            sIndexTableName,
                                            sKeyIndexName,
                                            sRidIndexName )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }
    
    IDU_LIMITPOINT("qdn::executeRenameConstr::malloc1");
    IDE_TEST(STRUCT_ALLOC_WITH_SIZE(aStatement->qmxMem,
                                    SChar,
                                    QD_MAX_SQL_LENGTH,
                                    &sSqlStr)
             != IDE_SUCCESS);

    QC_STR_COPY( sNewConstraintName, sParseTree->constraints->next->constrName );

    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                     "UPDATE  SYS_CONSTRAINTS_ "
                     "SET CONSTRAINT_NAME = VARCHAR'%s' "
                     "WHERE CONSTRAINT_ID = INTEGER'%"ID_INT32_FMT"'",
                     sNewConstraintName,
                     sConstraintID );

    IDE_TEST(qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                sSqlStr,
                                & sRowCnt ) != IDE_SUCCESS);

    IDE_TEST_RAISE(sRowCnt != 1, ERR_META_CRASH);

    IDE_TEST(qcm::touchTable(QC_SMI_STMT( aStatement ),
                             sTableID,
                             SMI_TBSLV_DDL_DML )
             != IDE_SUCCESS);

    IDE_TEST(qcm::makeAndSetQcmTableInfo(
                 QC_SMI_STMT( aStatement ),
                 sTableID,
                 sOldTableOID) != IDE_SUCCESS);

    IDE_TEST(qcm::getTableInfoByID(aStatement,
                                   sTableID,
                                   &sNewTableInfo,
                                   &sSCN,
                                   &sTableHandle) != IDE_SUCCESS);

    // PROJ-1624 global non-partitioned index
    // constraint �̸��� ���� index table name�� index table index name�� �����Ѵ�.
    if ( sOldIndexTable != NULL )
    {
        //------------------------
        // rename index table
        //------------------------

        sIndexTableNamePos.stmtText = sIndexTableName;
        sIndexTableNamePos.offset   = 0;
        sIndexTableNamePos.size     = idlOS::strlen(sIndexTableName);
            
        IDE_TEST(qdbCommon::updateTableSpecFromMeta(
                     aStatement,
                     sUserNamePos,
                     sIndexTableNamePos,
                     sOldIndexTable->tableID,
                     sOldIndexTable->tableOID,
                     sOldIndexTable->tableInfo->columnCount,
                     sOldIndexTable->tableInfo->parallelDegree )
                 != IDE_SUCCESS);

        // BUG-21387 COMMENT
        IDE_TEST(qdbComment::updateCommentTable(
                     aStatement,
                     sOldIndexTable->tableInfo->tableOwnerName,
                     sOldIndexTable->tableInfo->name,
                     sIndexTableName )
                 != IDE_SUCCESS);

        //------------------------
        // rename index table index
        //------------------------

        // key index, rid index�� ã�´�.
        IDE_TEST( qdx::getIndexTableIndices( sOldIndexTable->tableInfo,
                                             sIndexTableIndex )
                  != IDE_SUCCESS );

        sIndexNamePos.stmtText = sKeyIndexName;
        sIndexNamePos.offset   = 0;
        sIndexNamePos.size     = idlOS::strlen(sKeyIndexName);

        // index�̸��� ��Ÿ���� ����.
        IDE_TEST(qdx::updateIndexNameFromMeta(aStatement,
                                              sIndexTableIndex[0]->indexId,
                                              sIndexNamePos)
                 != IDE_SUCCESS );

        IDE_TEST(smiTable::alterIndexName(
                     aStatement->mStatistics,
                     QC_SMI_STMT( aStatement ),
                     (const void*)(sOldIndexTable->tableHandle),
                     (const void*)(sIndexTableIndex[0]->indexHandle),
                     sKeyIndexName )
                 != IDE_SUCCESS);
        
        sIndexNamePos.stmtText = sRidIndexName;
        sIndexNamePos.offset   = 0;
        sIndexNamePos.size     = idlOS::strlen(sRidIndexName);
        
        // index�̸��� ��Ÿ���� ����.
        IDE_TEST(qdx::updateIndexNameFromMeta(aStatement,
                                              sIndexTableIndex[1]->indexId,
                                              sIndexNamePos)
                 != IDE_SUCCESS );

        IDE_TEST(smiTable::alterIndexName(
                     aStatement->mStatistics,
                     QC_SMI_STMT( aStatement ),
                     (const void*)(sOldIndexTable->tableHandle),
                     (const void*)(sIndexTableIndex[1]->indexHandle),
                     sRidIndexName )
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
    
    /* PROJ-1723 [MDW/INTEGRATOR] Altibase Plugin ����
       DDL Statement Text�� �α�
    */
    if (QCU_DDL_SUPPLEMENTAL_LOG == 1)
    {
        IDE_TEST( qciMisc::writeDDLStmtTextLog( aStatement,
                                                sParseTree->tableInfo->tableOwnerID,
                                                sParseTree->tableInfo->name )
                  != IDE_SUCCESS );
    }

    // PROJ-1502 PARTITIONED DISK TABLE
    if( sIsPartitioned == ID_TRUE )
    {
        IDE_TEST( qcmPartition::touchPartitionInfoList( QC_SMI_STMT( aStatement ),
                                                        sOldPartInfoList )
                  != IDE_SUCCESS );

        IDE_TEST( qcmPartition::makeAndSetAndGetQcmPartitionInfoList( aStatement,
                                                                      sNewTableInfo,
                                                                      sOldPartInfoList,
                                                                      & sNewPartInfoList )
                  != IDE_SUCCESS );

        (void)qcmPartition::destroyQcmPartitionInfoList( sOldPartInfoList );
    }

    // PROJ-2642 Table on Replication Allow DDL
    if ( sIsReplicatedTable == ID_TRUE )
    {
        sNewTableOID = smiGetTableId( sNewTableInfo->tableHandle );

        if ( sOldTableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
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
            sOldTableOIDArray = &sOldTableOID;
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

    if ( ( sParseTree->tableInfo->replicationCount > 0 ) ||
         ( QCG_GET_SESSION_IS_NEED_DDL_INFO( aStatement ) == ID_TRUE ) )
    {
        qrc::setDDLDestInfo( aStatement, 
                             1,
                             &(sNewTableInfo->tableOID),
                             0, 
                             NULL );
    }

    if ( sOldIndexTable != NULL )
    {
        (void)qcm::destroyQcmTableInfo(sOldIndexTable->tableInfo);
    }
    else
    {
        // Nothing to do.
    }
    
    (void)qcm::destroyQcmTableInfo(sOldTableInfo);

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_META_CRASH);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCM_META_CRASH));
    }
    IDE_EXCEPTION(ERR_NOT_EXIST_CONSTRAINT_NAME)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDN_NOT_EXISTS_CONSTRAINT));
    }
    IDE_EXCEPTION(ERR_EXIST_OBJECT_NAME);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDB_EXIST_OBJECT_NAME));
    }
    IDE_EXCEPTION_END;

    (void)qcm::destroyQcmTableInfo( sNewTableInfo );
    (void)qcmPartition::destroyQcmPartitionInfoList( sNewPartInfoList );
    (void)qcm::destroyQcmTableInfo( sNewIndexTableInfo );

    // on fail, must restore temp info.
    qcmPartition::restoreTempInfo( sOldTableInfo,
                                   sOldPartInfoList,
                                   sOldIndexTable );
    
    return IDE_FAILURE;
}


IDE_RC qdn::executeDropConstr(qcStatement * aStatement)
{
/***********************************************************************
 *
 * Description :
 *      ALTER TABLE ... DROP CONSTRAINT ����
 *
 * Implementation :
 *      1. Constraint �̸����� ConstraintID, IndexID ���ϱ�
 *      2. smiTable::dropIndex
 *      3. SYS_INDICES_,SYS_INDEX_COLUMNS_ ��Ÿ ���̺��� ����
 *      4. SYS_CONSTRAINTS_,  SYS_CONSTRAINT_COLUMNS_ ��Ÿ ���̺��� ����
 *      5. SYS_COLUMNS_ ���� primary key, not null ���� ����
 *      6. Constraint�� ���õ� Procedure�� ���� ������ ����
 *      7. ��Ÿ ĳ�� �籸��
 *
 ***********************************************************************/

#define IDE_FN "qdn::executeDropConstr"

    // To Fix PR-10909
    qdTableParseTree        * sParseTree;
    UInt                      sConstraintID;
    UInt                      sTableID;
    smOID                     sOldTableOID = 0;
    smOID                     sNewTableOID = 0;
    qcmIndex                * sIndex = NULL;
    qcmTableInfo            * sInfo = NULL;
    qcmTableInfo            * sNewTableInfo = NULL;
    UInt                      i;
    UInt                      j;
    SChar                   * sSqlStr;
    SChar                     sConstraintName[ QC_MAX_OBJECT_NAME_LEN + 1 ];
    vSLong                    sRowCnt;
    void                    * sTableHandle;
    smSCN                     sSCN;
    qcmPartitionInfoList    * sOldPartInfoList = NULL;
    qcmPartitionInfoList    * sNewPartInfoList = NULL;
    qdIndexTableList        * sOldIndexTable = NULL;
    idBool                    sIsPrimary = ID_FALSE;
    smOID                   * sOldPartitionOID = NULL;
    UInt                      sOldPartitionCount = 0;
    idBool                    sIsReplicatedTable = ID_FALSE;
    smOID                   * sOldTableOIDArray = NULL;
    smOID                   * sNewTableOIDArray = NULL;
    UInt                      sTableOIDCount = 0;
    UInt                      sDDLSupplementalLog = QCU_DDL_SUPPLEMENTAL_LOG;

    sParseTree = (qdTableParseTree *)aStatement->myPlan->parseTree;

    // TASK-2176
    // Table�� ���� Lock�� ȹ���Ѵ�.
    IDE_TEST( qcm::validateAndLockTable(aStatement,
                                        sParseTree->tableHandle,
                                        sParseTree->tableSCN,
                                        SMI_TABLE_LOCK_X)
              != IDE_SUCCESS);

    sInfo = sParseTree->tableInfo;
    sTableID = sInfo->tableID;
    sOldTableOID = smiGetTableId( sInfo->tableHandle );

    // PROJ-1407 Temporary table
    // session temporary table�� �����ϴ� ��� DDL�� �� �� ����.
    IDE_TEST_RAISE( qcuTemporaryObj::existSessionTable( sInfo ) == ID_TRUE,
                    ERR_SESSION_TEMPORARY_TABLE_EXIST );

    // PROJ-1502 PARTITIONED DISK TABLE
    if( sInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
    {
        // ��� ��Ƽ�ǿ� LOCK(X)
        IDE_TEST( qcmPartition::validateAndLockPartitionInfoList( aStatement,
                                                                  sParseTree->partTable->partInfoList,
                                                                  SMI_TBSLV_DDL_DML, // TBS Validation �ɼ�
                                                                  SMI_TABLE_LOCK_X,
                                                                  smiGetDDLLockTimeOut((QC_SMI_STMT(aStatement))->getTrans()))
                  != IDE_SUCCESS );

        // ���� ó���� ���Ͽ�, Lock�� ���� �Ŀ� Partition List�� �����Ѵ�.
        sOldPartInfoList = sParseTree->partTable->partInfoList;
        
        sOldIndexTable = sParseTree->oldIndexTables;
    
        // PROJ-1624 global non-partitioned index
        if ( sOldIndexTable != NULL )
        {
            IDE_TEST( qdx::validateAndLockIndexTableList( aStatement,
                                                          sOldIndexTable,
                                                          SMI_TBSLV_DDL_DML, // TBS Validation �ɼ�
                                                          SMI_TABLE_LOCK_X,
                                                          smiGetDDLLockTimeOut((QC_SMI_STMT(aStatement))->getTrans()))
                      != IDE_SUCCESS );
        }
        else
        {
            // Nothing to do.
        }

        if ( ( sInfo->replicationCount > 0 ) ||
             ( sDDLSupplementalLog == 1 ) )
        {
            IDE_TEST( qcmPartition::getAllPartitionOID( QC_QMX_MEM(aStatement),
                                                        sOldPartInfoList,
                                                        &sOldPartitionOID,
                                                        &sOldPartitionCount )
                      != IDE_SUCCESS );
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

    QC_STR_COPY( sConstraintName, sParseTree->constraints->constrName );

    sConstraintID = qcmCache::getConstraintIDByName(sParseTree->tableInfo,
                                                    sConstraintName,
                                                    &sIndex);

    // PROJ-2642 Table on Replication Allow DDL
    if ( sInfo->replicationCount > 0 )
    {
        if ( sInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
        {
            sOldTableOIDArray = sOldPartitionOID;
            sTableOIDCount = sOldPartitionCount;
        }
        else
        {
            sOldTableOID = smiGetTableId( sInfo->tableHandle );

            sOldTableOIDArray = &sOldTableOID;
            sTableOIDCount = 1;
        }

        IDE_TEST( qdn::checkOperatableForReplication( aStatement,
                                                      sInfo,
                                                      getConstraintType( sInfo,
                                                                         sConstraintID ),
                                                      sOldTableOIDArray,
                                                      sTableOIDCount )
                  != IDE_SUCCESS );

        sIsReplicatedTable = ID_TRUE;
    }
    else
    {
        IDE_DASSERT( sParseTree->tableInfo->replicationRecoveryCount == 0 );
    }

    if (sIndex != NULL)
    {
        // PROJ-1502 PARTITIONED DISK TABLE
        if( sInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
        {
            if ( sInfo->primaryKey != NULL )
            {
                if ( sInfo->primaryKey->indexId == sIndex->indexId )
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
            if ( sIndex->indexPartitionType == QCM_NONE_PARTITIONED_INDEX )
            {
                if ( sOldIndexTable != NULL )
                {
                    IDE_TEST( qdx::dropIndexTable( aStatement,
                                                   sOldIndexTable,
                                                   ID_FALSE /* aIsDropTablespace */ )
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
            
            if ( ( sIndex->indexPartitionType != QCM_NONE_PARTITIONED_INDEX ) ||
                 ( sIsPrimary == ID_TRUE ) )
            {
                IDE_TEST( qdd::dropIndexPartitions( aStatement,
                                                    sOldPartInfoList,
                                                    sIndex->indexId,
                                                    ID_FALSE /* aIsCascade */,
                                                    ID_FALSE /* aIsDropTablespace */ )
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

        IDE_TEST(smiTable::dropIndex(QC_SMI_STMT( aStatement ),
                                     sInfo->tableHandle,
                                     sIndex->indexHandle,
                                     SMI_TBSLV_DDL_DML )
                 != IDE_SUCCESS);

        IDE_TEST(qdd::deleteIndicesFromMetaByIndexID(aStatement,
                                                     sIndex->indexId)
                 != IDE_SUCCESS);
    }
    else
    {
        // Nothing to do.
    }

    IDU_LIMITPOINT("qdn::executeDropConstr::malloc1");
    IDE_TEST(STRUCT_ALLOC_WITH_SIZE(aStatement->qmxMem,
                                    SChar,
                                    QD_MAX_SQL_LENGTH,
                                    &sSqlStr)
             != IDE_SUCCESS);

    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                     "DELETE FROM SYS_CONSTRAINTS_ "
                     "WHERE CONSTRAINT_ID = INTEGER'%"ID_INT32_FMT"'",
                     sConstraintID );

    IDE_TEST(qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                sSqlStr,
                                & sRowCnt ) != IDE_SUCCESS);

    IDE_TEST_RAISE(sRowCnt != 1, ERR_META_CRASH);

    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                     "DELETE FROM SYS_CONSTRAINT_COLUMNS_ "
                     "WHERE CONSTRAINT_ID = INTEGER'%"ID_INT32_FMT"'",
                     sConstraintID );

    IDE_TEST(qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                sSqlStr,
                                & sRowCnt ) != IDE_SUCCESS);

    for (i = 0; i < sInfo->uniqueKeyCount; i++)
    {
        if ((sInfo->uniqueKeys[i].constraintID == sConstraintID) &&
            (sInfo->uniqueKeys[i].constraintType == QD_PRIMARYKEY))
        {
            if ( existNotNullConstraint( sInfo,
                             (UInt*) smiGetIndexColumns(
                                 sInfo->primaryKey->indexHandle ),
                             sInfo->primaryKey->keyColCount ) == ID_FALSE )
            {
                // hove to premit nullable flag.
                for  (j = 0; j < sInfo->uniqueKeys[i].constraintColumnCount; j ++)
                {
                    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                                     "UPDATE SYS_COLUMNS_ "
                                     "SET IS_NULLABLE = 'T' "
                                     "WHERE COLUMN_ID = %"ID_INT32_FMT"",
                                     sInfo->uniqueKeys[i].constraintColumn[j]);
                    IDE_TEST(qcg::runDMLforDDL(
                                 QC_SMI_STMT( aStatement ),
                                 sSqlStr, &sRowCnt) != IDE_SUCCESS);
                    IDE_TEST_RAISE(sRowCnt != 1, ERR_META_CRASH);
                    IDE_TEST(qdbCommon::makeColumnNullable(
                                 aStatement,
                                 sInfo,
                                 sInfo->uniqueKeys[i].constraintColumn[j])
                             != IDE_SUCCESS);
                }
            }
        }
    }

    for (i = 0; i < sInfo->notNullCount; i++)
    {
        if (sInfo->notNulls[i].constraintID == sConstraintID)
        {
            for (j = 0; j < sInfo->notNulls[i].constraintColumnCount; j++)
            {
                idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                                 "UPDATE SYS_COLUMNS_ SET IS_NULLABLE = 'T' "
                                 "WHERE COLUMN_ID = %"ID_INT32_FMT"",
                                 sInfo->notNulls[i].constraintColumn[j] );
                IDE_TEST(qcg::runDMLforDDL(QC_SMI_STMT( aStatement ),
                                           sSqlStr, &sRowCnt) != IDE_SUCCESS);
                IDE_TEST_RAISE(sRowCnt != 1, ERR_META_CRASH);
                IDE_TEST(qdbCommon::makeColumnNullable(
                             aStatement,
                             sInfo,
                             sInfo->notNulls[i].constraintColumn[j])
                         != IDE_SUCCESS);
            }
        }
    }

    /* BUG-35445 Check Constraint, Function-Based Index���� ��� ���� Function�� ����/���� ���� */
    IDE_TEST( qcmProc::relRemoveRelatedToConstraintByConstraintID(
                    aStatement,
                    sConstraintID )
              != IDE_SUCCESS );

    IDE_TEST(qcm::touchTable(QC_SMI_STMT( aStatement ),
                             sTableID,
                             SMI_TBSLV_DDL_DML )
             != IDE_SUCCESS);

    IDE_TEST(qcm::makeAndSetQcmTableInfo(
                 QC_SMI_STMT( aStatement ),
                 sTableID,
                 sOldTableOID) != IDE_SUCCESS);

    IDE_TEST(qcm::getTableInfoByID(aStatement,
                                   sTableID,
                                   &sNewTableInfo,
                                   &sSCN,
                                   &sTableHandle) != IDE_SUCCESS);

    /* PROJ-1723 [MDW/INTEGRATOR] Altibase Plugin ����
       DDL Statement Text�� �α�
    */
    if ( sDDLSupplementalLog == 1 )
    {
        IDE_TEST( qciMisc::writeDDLStmtTextLog( aStatement,
                                                sParseTree->tableInfo->tableOwnerID,
                                                sParseTree->tableInfo->name )
                  != IDE_SUCCESS );
    }

    // PROJ-1502 PARTITIONED DISK TABLE
    if( sNewTableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
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

    // PROJ-2642 Table on Replication Allow DDL
    if ( ( sIsReplicatedTable == ID_TRUE ) ||
         ( sDDLSupplementalLog == 1 ) )
    {
        sNewTableOID = smiGetTableId( sNewTableInfo->tableHandle );

        if ( sInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
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
            sOldTableOIDArray = &sOldTableOID;
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


    if ( ( sParseTree->tableInfo->replicationCount > 0 ) ||
         ( QCG_GET_SESSION_IS_NEED_DDL_INFO( aStatement ) == ID_TRUE ) )
    {
        qrc::setDDLDestInfo( aStatement, 
                             1,
                             &(sNewTableInfo->tableOID),
                             0,
                             NULL );
    }

    if ( sNewTableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
    {
        (void)qcmPartition::destroyQcmPartitionInfoList( sOldPartInfoList );

        if ( sOldIndexTable != NULL )
        {
            (void)qcm::destroyQcmTableInfo(sOldIndexTable->tableInfo);
        }
        else
        {
            // Nothing to do.
        }
    }
    else
    {
        /* do nothing */
    }

    (void)qcm::destroyQcmTableInfo(sInfo);

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_SESSION_TEMPORARY_TABLE_EXIST )
    {
        IDE_SET(ideSetErrorCode( qpERR_ABORT_QDB_TEMPORARY_TABLE_DDL_DISABLE ));
    }
    IDE_EXCEPTION(ERR_META_CRASH);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCM_META_CRASH));
    }
    IDE_EXCEPTION_END;

    (void)qcm::destroyQcmTableInfo( sNewTableInfo );
    (void)qcmPartition::destroyQcmPartitionInfoList( sNewPartInfoList );

    // on fail, must restore temp info.
    qcmPartition::restoreTempInfo( sInfo,
                                   sOldPartInfoList,
                                   NULL );

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qdn::executeDropUnique(qcStatement * aStatement)
{
/***********************************************************************
 *
 * Description :
 *      ALTER TABLE ... DROP UNIQUE ����
 *
 * Implementation :
 *      1. Unique Constraint ���ϱ�
 *      2. ���� index ����
 *      3. SYS_INDICES_,SYS_INDEX_COLUMNS_ ��Ÿ ���̺��� �ε��� ���� ����
 *      4. SYS_CONSTRAINTS_, SYS_CONSTRAINT_COLUMNS_ ��Ÿ ���̺��� ����
 *      5. SYS_COLUMNS_ ���� not null ���� ����
 *      6. ���� PSM �� invalid ���·� ����
 *      7. ��Ÿ ĳ�� �籸��
 *
 ***********************************************************************/

#define IDE_FN "qdn::executeDropUnique"

    // To Fix PR-10909
    qdTableParseTree        * sParseTree;
    qcmUnique               * sUniqueConstraint;
    qcmColumn               * sColumnInfo;
    qcmIndex                * sIndex = NULL;
    SInt                      sKeyColCount = 0;
    UInt                      sKeyCols[QC_MAX_KEY_COLUMN_COUNT];
    UInt                      sKeyColsFlag[QC_MAX_KEY_COLUMN_COUNT];
    UInt                      sTableID;
    smOID                     sTableOID;
    SChar                   * sSqlStr;
    UInt                      i;
    qcmColumn               * sColumn;
    vSLong                    sRowCnt;
    void                    * sTableHandle;
    smSCN                     sSCN;
    qcmPartitionInfoList    * sOldPartInfoList = NULL;
    qcmPartitionInfoList    * sNewPartInfoList = NULL;
    qcmTableInfo            * sOldTableInfo = NULL;
    qcmTableInfo            * sNewTableInfo = NULL;
    qdIndexTableList        * sOldIndexTable = NULL;
    smOID                     sOldTableOID = 0;
    smOID                     sNewTableOID = 0;
    smOID                   * sOldPartitionOID = NULL;
    UInt                      sOldPartitionCount = 0;
    idBool                    sIsReplicatedTable = ID_FALSE;
    smOID                   * sOldTableOIDArray = NULL;
    smOID                   * sNewTableOIDArray = NULL;
    UInt                      sTableOIDCount = 0;
    UInt                      sDDLSupplementalLog = QCU_DDL_SUPPLEMENTAL_LOG;
    UInt                      sFlag;

    sParseTree = (qdTableParseTree *)aStatement->myPlan->parseTree;

    // TASK-2176
    // Table�� ���� Lock�� ȹ���Ѵ�.
    IDE_TEST( qcm::validateAndLockTable(aStatement,
                                        sParseTree->tableHandle,
                                        sParseTree->tableSCN,
                                        SMI_TABLE_LOCK_X)
              != IDE_SUCCESS);

    sOldTableInfo = sParseTree->tableInfo;

    // PROJ-1407 Temporary table
    // session temporary table�� �����ϴ� ��� DDL�� �� �� ����.
    IDE_TEST_RAISE( qcuTemporaryObj::existSessionTable( sOldTableInfo ) == ID_TRUE,
                    ERR_SESSION_TEMPORARY_TABLE_EXIST );

    // PROJ-1502 PARTITIONED DISK TABLE
    if( sOldTableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
    {
        // ��� ��Ƽ�ǿ� LOCK(X)
        IDE_TEST( qcmPartition::validateAndLockPartitionInfoList( aStatement,
                                                                  sParseTree->partTable->partInfoList,
                                                                  SMI_TBSLV_DDL_DML, // TBS Validation �ɼ�
                                                                  SMI_TABLE_LOCK_X,
                                                                  smiGetDDLLockTimeOut((QC_SMI_STMT(aStatement))->getTrans()))
                  != IDE_SUCCESS );

        // ���� ó���� ���Ͽ�, Lock�� ���� �Ŀ� Partition List�� �����Ѵ�.
        sOldPartInfoList = sParseTree->partTable->partInfoList;
        
        sOldIndexTable = sParseTree->oldIndexTables;
    
        // PROJ-1624 global non-partitioned index
        if ( sOldIndexTable != NULL )
        {
            IDE_TEST( qdx::validateAndLockIndexTableList( aStatement,
                                                          sOldIndexTable,
                                                          SMI_TBSLV_DDL_DML, // TBS Validation �ɼ�
                                                          SMI_TABLE_LOCK_X,
                                                          smiGetDDLLockTimeOut((QC_SMI_STMT(aStatement))->getTrans()))
                      != IDE_SUCCESS );
        }
        else
        {
            // Nothing to do.
        }

        if ( ( sOldTableInfo->replicationCount > 0 ) ||
             ( sDDLSupplementalLog == 1 ) )
        {
            IDE_TEST( qcmPartition::getAllPartitionOID( QC_QMX_MEM(aStatement),
                                                        sOldPartInfoList,
                                                        &sOldPartitionOID,
                                                        &sOldPartitionCount )
                      != IDE_SUCCESS );
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

    // PROJ-2642 Table on Replication Allow DDL
    if ( sOldTableInfo->replicationCount > 0 )
    {
        if ( sOldTableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
        {
            sOldTableOIDArray = sOldPartitionOID;
            sTableOIDCount = sOldPartitionCount;
        }
        else
        {
            sOldTableOID = smiGetTableId( sOldTableInfo->tableHandle );

            sOldTableOIDArray = &sOldTableOID;
            sTableOIDCount = 1;
        }

        IDE_TEST( qdn::checkOperatableForReplication( aStatement,
                                                      sOldTableInfo,
                                                      QD_UNIQUE,
                                                      sOldTableOIDArray,
                                                      sTableOIDCount )
                  != IDE_SUCCESS );

        sIsReplicatedTable = ID_TRUE;
    }
    else
    {
        IDE_DASSERT( sParseTree->tableInfo->replicationRecoveryCount == 0 );
    }
    
    /* PROJ-1723 [MDW/INTEGRATOR] Altibase Plugin ����
       DDL Statement Text�� �α�
    */
    if ( sDDLSupplementalLog == 1 )
    {
        IDE_TEST( qciMisc::writeDDLStmtTextLog( aStatement,
                                                sOldTableInfo->tableOwnerID,
                                                sOldTableInfo->name )
                  != IDE_SUCCESS );
    }

    for (sColumn = sParseTree->constraints->constraintColumns;
         sColumn != NULL;
         sColumn = sColumn->next)
    {
        IDE_TEST(qcmCache::getColumn(aStatement,
                                     sOldTableInfo,
                                     sColumn->namePos,
                                     &sColumnInfo) != IDE_SUCCESS);

        // BUG-44924
        // Failed to drop unique constraints with columns having descending order.
        sFlag = sColumn->basicInfo->column.flag & SMI_COLUMN_ORDER_MASK;

        // fix BUG-33258
        if( sColumn->basicInfo != sColumnInfo->basicInfo )
        {
            *(sColumn->basicInfo) = *(sColumnInfo->basicInfo);
            // BUG-44924
            sColumn->basicInfo->column.flag &= ~SMI_COLUMN_ORDER_MASK;
            sColumn->basicInfo->column.flag |= sFlag;
        }

        sKeyCols[sKeyColCount]     = sColumn->basicInfo->column.id;
        sKeyColsFlag[sKeyColCount] = sColumn->basicInfo->column.flag;

        sKeyColCount++;
    }

    // find constraint.
    sUniqueConstraint = qcmCache::getUniqueByCols(sOldTableInfo,
                                                  sKeyColCount,
                                                  sKeyCols,
                                                  sKeyColsFlag);
    IDE_ASSERT( sUniqueConstraint != NULL );

    // PROJ-1502 PARTITIONED DISK TABLE
    for( i = 0; i < sOldTableInfo->indexCount; i++)
    {
        if( sOldTableInfo->indices[i].indexId ==
            sUniqueConstraint->constraintIndex->indexId )
        {
            sIndex = &sOldTableInfo->indices[i];
        }
    }
    IDE_ASSERT(sIndex != NULL);

    if( sOldTableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
    {
        // PROJ-1624 non-partitioned index
        if ( sIndex->indexPartitionType == QCM_NONE_PARTITIONED_INDEX )
        {
            if ( sOldIndexTable != NULL )
            {
                IDE_TEST( qdx::dropIndexTable( aStatement,
                                               sOldIndexTable,
                                               ID_FALSE /* aIsDropTablespace */ )
                          != IDE_SUCCESS );
            }
            else
            {
                // Nothing to do.
            }
        }
        else
        {
            IDE_TEST(qdd::dropIndexPartitions( aStatement,
                                               sOldPartInfoList,
                                               sIndex->indexId,
                                               ID_FALSE /* aIsCascade */,
                                               ID_FALSE /* aIsDropTablespace */ )
                     != IDE_SUCCESS );
        }
    }

    // drop dependent index.
    IDE_TEST(smiTable::dropIndex(
                 QC_SMI_STMT( aStatement ),
                 sOldTableInfo->tableHandle,
                 sUniqueConstraint->constraintIndex->indexHandle,
                 SMI_TBSLV_DDL_DML )
             != IDE_SUCCESS);

    IDE_TEST(qdd::deleteIndicesFromMetaByIndexID(
                 aStatement,
                 sUniqueConstraint->constraintIndex->indexId) != IDE_SUCCESS);

    IDU_LIMITPOINT("qdn::executeDropUnique::malloc1");
    IDE_TEST(STRUCT_ALLOC_WITH_SIZE(aStatement->qmxMem,
                                    SChar,
                                    QD_MAX_SQL_LENGTH,
                                    &sSqlStr)
             != IDE_SUCCESS);

    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                     "DELETE FROM SYS_CONSTRAINTS_ "
                     "WHERE CONSTRAINT_ID = INTEGER'%"ID_INT32_FMT"'",
                     sUniqueConstraint->constraintID );
    IDE_TEST(qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                sSqlStr,
                                & sRowCnt ) != IDE_SUCCESS);

    IDE_TEST_RAISE(sRowCnt != 1, ERR_META_CRASH);

    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                     "DELETE FROM SYS_CONSTRAINT_COLUMNS_ "
                     "WHERE CONSTRAINT_ID = INTEGER'%"ID_INT32_FMT"'",
                     sUniqueConstraint->constraintID );

    IDE_TEST(qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                sSqlStr,
                                & sRowCnt ) != IDE_SUCCESS);

    if (sUniqueConstraint->constraintType == QD_PRIMARYKEY)
    {
        for (i = 0; i < sUniqueConstraint->constraintColumnCount; i ++)
        {
            // hove to premit nullable flag.
            idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                             "UPDATE SYS_COLUMNS_ SET IS_NULLABLE = 'T' "
                             "WHERE COLUMN_ID = %"ID_INT32_FMT"",
                             sUniqueConstraint->constraintColumn[i] );
            IDE_TEST(qcg::runDMLforDDL(QC_SMI_STMT( aStatement ),
                                       sSqlStr, &sRowCnt) != IDE_SUCCESS);
            IDE_TEST_RAISE(sRowCnt != 1, ERR_META_CRASH);
        }
    }

    sTableID  = sOldTableInfo->tableID;
    sTableOID = smiGetTableId(sOldTableInfo->tableHandle);

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
                                   &sTableHandle) != IDE_SUCCESS);

    // PROJ-1502 PARTITIONED DISK TABLE
    if( sNewTableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
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

    // PROJ-2642 Table on Replication Allow DDL
    if ( ( sIsReplicatedTable == ID_TRUE ) ||
         ( sDDLSupplementalLog == 1 ) )
    {
        sNewTableOID = smiGetTableId( sNewTableInfo->tableHandle );

        if ( sOldTableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
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
            sOldTableOIDArray = &sOldTableOID;
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

    if ( ( sParseTree->tableInfo->replicationCount > 0 ) ||
         ( QCG_GET_SESSION_IS_NEED_DDL_INFO( aStatement ) == ID_TRUE ) )
    {    
        qrc::setDDLDestInfo( aStatement, 
                             1,
                             &(sNewTableInfo->tableOID),
                             0,
                             NULL );
    }

    if ( sNewTableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
    {
        (void)qcmPartition::destroyQcmPartitionInfoList( sOldPartInfoList );
    }
    else
    {
        /* do nothing */
    }

    if ( sOldIndexTable != NULL )
    {
        (void)qcm::destroyQcmTableInfo(sOldIndexTable->tableInfo);
    }
    else
    {
        // Nothing to do.
    }
        
    (void)qcm::destroyQcmTableInfo(sOldTableInfo);

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_SESSION_TEMPORARY_TABLE_EXIST )
    {
        IDE_SET(ideSetErrorCode( qpERR_ABORT_QDB_TEMPORARY_TABLE_DDL_DISABLE ));
    }
    IDE_EXCEPTION(ERR_META_CRASH);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCM_META_CRASH));
    }
    IDE_EXCEPTION_END;

    (void)qcm::destroyQcmTableInfo( sNewTableInfo );
    (void)qcmPartition::destroyQcmPartitionInfoList( sNewPartInfoList );

    qcmPartition::restoreTempInfo( sOldTableInfo,
                                   sOldPartInfoList,
                                   NULL );

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qdn::executeDropLocalUnique(qcStatement * aStatement)
{
/***********************************************************************
 *
 * Description :
 *    PROJ-1502 PARTITIONED DISK TABLE
 *    ALTER TABLE ... DROP LOCAL UNIQUE ... �� ����
 *
 * Implementation :
 *
 ***********************************************************************/

    qdTableParseTree        * sParseTree;
    qcmUnique                 sLocalUnique;
    qcmColumn               * sColumnInfo;
    qcmIndex                * sIndex = NULL;
    SInt                      sKeyColCount = 0;
    UInt                      sKeyCols[QC_MAX_KEY_COLUMN_COUNT];
    UInt                      sKeyColsFlag[QC_MAX_KEY_COLUMN_COUNT];
    UInt                      sTableID;
    SChar                   * sSqlStr;
    UInt                      i;
    qcmColumn               * sColumn;
    vSLong                    sRowCnt;
    void                    * sTableHandle;
    smSCN                     sSCN;
    qcmPartitionInfoList    * sOldPartInfoList = NULL;
    qcmPartitionInfoList    * sNewPartInfoList = NULL;
    qcmTableInfo            * sOldTableInfo = NULL;
    qcmTableInfo            * sNewTableInfo = NULL;
    smOID                     sOldTableOID = 0;
    smOID                     sNewTableOID = 0;
    smOID                   * sOldPartitionOID = NULL;
    UInt                      sOldPartitionCount = 0;
    idBool                    sIsReplicatedTable = ID_FALSE;
    smOID                   * sOldTableOIDArray = NULL;
    smOID                   * sNewTableOIDArray = NULL;
    UInt                      sTableOIDCount = 0;
    UInt                      sDDLSupplementalLog = QCU_DDL_SUPPLEMENTAL_LOG;
    UInt                      sFlag;

    sParseTree = (qdTableParseTree *)aStatement->myPlan->parseTree;

    // TASK-2176
    // Table�� ���� Lock�� ȹ���Ѵ�.
    IDE_TEST( qcm::validateAndLockTable(aStatement,
                                        sParseTree->tableHandle,
                                        sParseTree->tableSCN,
                                        SMI_TABLE_LOCK_X)
              != IDE_SUCCESS);

    sOldTableInfo = sParseTree->tableInfo;
    sTableID  = sOldTableInfo->tableID;
    sOldTableOID = smiGetTableId(sOldTableInfo->tableHandle);


    // PROJ-1407 Temporary table
    // session temporary table�� �����ϴ� ��� DDL�� �� �� ����.
    IDE_TEST_RAISE( qcuTemporaryObj::existSessionTable( sOldTableInfo ) == ID_TRUE,
                    ERR_SESSION_TEMPORARY_TABLE_EXIST );

    // PROJ-1502 PARTITIONED DISK TABLE
    if( sOldTableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
    {
        // ��� ��Ƽ�ǿ� LOCK(X)
        IDE_TEST( qcmPartition::validateAndLockPartitionInfoList( aStatement,
                                                                  sParseTree->partTable->partInfoList,
                                                                  SMI_TBSLV_DDL_DML, // TBS Validation �ɼ�
                                                                  SMI_TABLE_LOCK_X,
                                                                  smiGetDDLLockTimeOut((QC_SMI_STMT(aStatement))->getTrans()))
                  != IDE_SUCCESS );

        // ���� ó���� ���Ͽ�, Lock�� ���� �Ŀ� Partition List�� �����Ѵ�.
        sOldPartInfoList = sParseTree->partTable->partInfoList;

        if ( ( sOldTableInfo->replicationCount > 0 ) ||
             ( sDDLSupplementalLog == 1 ) )
        {
            IDE_TEST( qcmPartition::getAllPartitionOID( QC_QMX_MEM(aStatement),
                                                        sOldPartInfoList,
                                                        &sOldPartitionOID,
                                                        &sOldPartitionCount )
                      != IDE_SUCCESS );
        }
        else
        {
            /* do nothing */
        }
    }

    // PROJ-2642 Table on Replication Allow DDL
    if ( sOldTableInfo->replicationCount > 0 )
    {
        if ( sOldTableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
        {
            sOldTableOIDArray = sOldPartitionOID;
            sTableOIDCount = sOldPartitionCount;
        }
        else
        {
            sOldTableOID = smiGetTableId( sOldTableInfo->tableHandle );

            sOldTableOIDArray = &sOldTableOID;
            sTableOIDCount = 1;
        }

        IDE_TEST( qdn::checkOperatableForReplication( aStatement,
                                                      sOldTableInfo,
                                                      QD_LOCAL_UNIQUE,
                                                      sOldTableOIDArray,
                                                      sTableOIDCount )
                  != IDE_SUCCESS );

        sIsReplicatedTable = ID_TRUE;
    }
    else
    {
        IDE_DASSERT( sParseTree->tableInfo->replicationRecoveryCount == 0 );
    }

    /* PROJ-1723 [MDW/INTEGRATOR] Altibase Plugin ����
       DDL Statement Text�� �α�
    */
    if ( sDDLSupplementalLog == 1 )
    {
        IDE_TEST( qciMisc::writeDDLStmtTextLog( aStatement,
                                                sOldTableInfo->tableOwnerID,
                                                sOldTableInfo->name )
                  != IDE_SUCCESS );
    }

    for (sColumn = sParseTree->constraints->constraintColumns;
         sColumn != NULL;
         sColumn = sColumn->next)
    {
        IDE_TEST(qcmCache::getColumn(aStatement,
                                     sOldTableInfo,
                                     sColumn->namePos,
                                     &sColumnInfo) != IDE_SUCCESS);

        // BUG-44924
        // Failed to drop unique constraints with columns having descending order.
        sFlag = sColumn->basicInfo->column.flag & SMI_COLUMN_ORDER_MASK;

        // fix BUG-33258
        if( sColumn->basicInfo != sColumnInfo->basicInfo )
        {
            *(sColumn->basicInfo) = *(sColumnInfo->basicInfo);
            // BUG-44924
            sColumn->basicInfo->column.flag &= ~SMI_COLUMN_ORDER_MASK;
            sColumn->basicInfo->column.flag |= sFlag;
        }

        sKeyCols[sKeyColCount]     = sColumn->basicInfo->column.id;
        sKeyColsFlag[sKeyColCount] = sColumn->basicInfo->column.flag;

        sKeyColCount++;
    }

    // find constraint.
    IDE_TEST( qcm::getQcmLocalUniqueByCols( QC_SMI_STMT( aStatement ),
                                            sOldTableInfo,
                                            (UInt)sKeyColCount,
                                            sKeyCols,
                                            sKeyColsFlag,
                                            &sLocalUnique )
              != IDE_SUCCESS );

    // PROJ-1502 PARTITIONED DISK TABLE
    for( i = 0; i < sOldTableInfo->indexCount; i++)
    {
        if( sOldTableInfo->indices[i].indexId ==
            sLocalUnique.constraintIndex->indexId )
        {
            sIndex = &sOldTableInfo->indices[i];
        }
    }
    IDE_ASSERT(sIndex != NULL);

    if( sOldTableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
    {
        IDE_TEST(qdd::dropIndexPartitions( aStatement,
                                           sOldPartInfoList,
                                           sIndex->indexId,
                                           ID_FALSE /* aIsCascade */,
                                           ID_FALSE /* aIsDropTablespace */ )
                 != IDE_SUCCESS );
    }

    // drop dependent index.
    IDE_TEST(smiTable::dropIndex(
                 QC_SMI_STMT( aStatement ),
                 sOldTableInfo->tableHandle,
                 sLocalUnique.constraintIndex->indexHandle,
                 SMI_TBSLV_DDL_DML )
             != IDE_SUCCESS);

    IDE_TEST(qdd::deleteIndicesFromMetaByIndexID(
                 aStatement,
                 sLocalUnique.constraintIndex->indexId) != IDE_SUCCESS);

    IDU_LIMITPOINT("qdn::executeDropLocalUnique::malloc1");
    IDE_TEST(STRUCT_ALLOC_WITH_SIZE(aStatement->qmxMem,
                                    SChar,
                                    QD_MAX_SQL_LENGTH,
                                    &sSqlStr)
             != IDE_SUCCESS);

    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                     "DELETE FROM SYS_CONSTRAINTS_ "
                     "WHERE CONSTRAINT_ID = INTEGER'%"ID_INT32_FMT"'",
                     sLocalUnique.constraintID );
    IDE_TEST(qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                sSqlStr,
                                & sRowCnt ) != IDE_SUCCESS);

    IDE_TEST_RAISE(sRowCnt != 1, ERR_META_CRASH);

    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                     "DELETE FROM SYS_CONSTRAINT_COLUMNS_ "
                     "WHERE CONSTRAINT_ID = INTEGER'%"ID_INT32_FMT"'",
                     sLocalUnique.constraintID );

    IDE_TEST(qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                sSqlStr,
                                & sRowCnt ) != IDE_SUCCESS);

    IDE_TEST(qcm::touchTable(QC_SMI_STMT( aStatement ),
                             sTableID,
                             SMI_TBSLV_DDL_DML )
             != IDE_SUCCESS);

    IDE_TEST(qcm::makeAndSetQcmTableInfo(
                 QC_SMI_STMT( aStatement ),
                 sTableID,
                 sOldTableOID) != IDE_SUCCESS);

    IDE_TEST(qcm::getTableInfoByID(aStatement,
                                   sTableID,
                                   &sNewTableInfo,
                                   &sSCN,
                                   &sTableHandle) != IDE_SUCCESS);

    // PROJ-1502 PARTITIONED DISK TABLE
    if( sNewTableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
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

    // PROJ-2642 Table on Replication Allow DDL
    if ( ( sIsReplicatedTable == ID_TRUE ) ||
         ( sDDLSupplementalLog == 1 ) )
    {
        sNewTableOID = smiGetTableId( sNewTableInfo->tableHandle );

        if ( sOldTableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
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
            sOldTableOIDArray = &sOldTableOID;
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

    if ( ( sParseTree->tableInfo->replicationCount > 0 ) ||
         ( QCG_GET_SESSION_IS_NEED_DDL_INFO( aStatement ) == ID_TRUE ) )
    {
        qrc::setDDLDestInfo( aStatement, 
                             1,
                             &(sNewTableInfo->tableOID),
                             0,
                             NULL );
    }

    if ( sNewTableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
    {
        (void)qcmPartition::destroyQcmPartitionInfoList( sOldPartInfoList );
    }
    else
    {
        /* do nothing */
    }

    (void)qcm::destroyQcmTableInfo(sOldTableInfo);

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_SESSION_TEMPORARY_TABLE_EXIST )
    {
        IDE_SET(ideSetErrorCode( qpERR_ABORT_QDB_TEMPORARY_TABLE_DDL_DISABLE ));
    }
    IDE_EXCEPTION(ERR_META_CRASH);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCM_META_CRASH));
    }
    IDE_EXCEPTION_END;

    (void)qcm::destroyQcmTableInfo( sNewTableInfo );
    (void)qcmPartition::destroyQcmPartitionInfoList( sNewPartInfoList );

    qcmPartition::restoreTempInfo( sOldTableInfo,
                                   sOldPartInfoList,
                                   NULL );

    return IDE_FAILURE;
}

IDE_RC qdn::executeDropPrimary(qcStatement * aStatement)
{
/***********************************************************************
 *
 * Description :
 *      ALTER TABLE ... DROP PRIMARY KEY ����
 *
 * Implementation :
 *      1. PRIMARYKEY �� �ش��ϴ� Constraint ���ϱ�
 *      2. smiTable::dropIndex
 *      3. SYS_INDICES_,SYS_INDEX_COLUMNS_ ��Ÿ ���̺��� ����
 *      4. SYS_CONSTRAINTS_,  SYS_CONSTRAINT_COLUMNS_ ��Ÿ ���̺��� ����
 *      5. SYS_COLUMNS_ ���� not null ���� ����
 *      6. ���� PSM �� invalid ���·� ����
 *      7. ��Ÿ ĳ�� �籸��
 *
 ***********************************************************************/

#define IDE_FN "qdn::executeDropPrimary"

    qdTableParseTree        * sParseTree;
    qcmUnique               * sUniqueConstraint = NULL;
    qcmNotNull              * sNotNullConstraint;
    qcmIndex                * sIndex = NULL;
    UInt                      sTableID;
    smOID                     sTableOID;
    SChar                   * sSqlStr;
    vSLong                    sRowCnt;
    idBool                    sIsNullable;
    UInt                      i;
    UInt                      j;
    UInt                      k;
    void                    * sTableHandle;
    smSCN                     sSCN;
    qcmPartitionInfoList    * sOldPartInfoList = NULL;
    qcmPartitionInfoList    * sNewPartInfoList = NULL;
    qcmTableInfo            * sNewTableInfo = NULL;
    qcmTableInfo            * sOldTableInfo = NULL;
    qdIndexTableList        * sOldIndexTable = NULL;

    sParseTree = (qdTableParseTree *)aStatement->myPlan->parseTree;

    // TASK-2176
    // Table�� ���� Lock�� ȹ���Ѵ�.
    IDE_TEST( qcm::validateAndLockTable(aStatement,
                                        sParseTree->tableHandle,
                                        sParseTree->tableSCN,
                                        SMI_TABLE_LOCK_X)
              != IDE_SUCCESS);

    sOldTableInfo = sParseTree->tableInfo;

    // PROJ-1407 Temporary table
    // session temporary table�� �����ϴ� ��� DDL�� �� �� ����.
    IDE_TEST_RAISE( qcuTemporaryObj::existSessionTable( sOldTableInfo ) == ID_TRUE,
                    ERR_SESSION_TEMPORARY_TABLE_EXIST );

    // PROJ-1502 PARTITIONED DISK TABLE
    if( sOldTableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
    {
        // ��� ��Ƽ�ǿ� LOCK(X)
        IDE_TEST( qcmPartition::validateAndLockPartitionInfoList( aStatement,
                                                                  sParseTree->partTable->partInfoList,
                                                                  SMI_TBSLV_DDL_DML, // TBS Validation �ɼ�
                                                                  SMI_TABLE_LOCK_X,
                                                                  smiGetDDLLockTimeOut((QC_SMI_STMT(aStatement))->getTrans()))
                  != IDE_SUCCESS );

        // ���� ó���� ���Ͽ�, Lock�� ���� �Ŀ� Partition List�� �����Ѵ�.
        sOldPartInfoList = sParseTree->partTable->partInfoList;
        
        sOldIndexTable = sParseTree->oldIndexTables;
    
        // PROJ-1624 global non-partitioned index
        if ( sOldIndexTable != NULL )
        {
            IDE_TEST( qdx::validateAndLockIndexTableList( aStatement,
                                                          sOldIndexTable,
                                                          SMI_TBSLV_DDL_DML, // TBS Validation �ɼ�
                                                          SMI_TABLE_LOCK_X,
                                                          smiGetDDLLockTimeOut((QC_SMI_STMT(aStatement))->getTrans()))
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

    /* PROJ-1723 [MDW/INTEGRATOR] Altibase Plugin ����
       DDL Statement Text�� �α�
    */
    if (QCU_DDL_SUPPLEMENTAL_LOG == 1)
    {
        IDE_TEST( qciMisc::writeDDLStmtTextLog( aStatement,
                                                sOldTableInfo->tableOwnerID,
                                                sOldTableInfo->name )
                  != IDE_SUCCESS );
    }

    for (i = 0; i < sOldTableInfo->uniqueKeyCount; i++)
    {
        if (sOldTableInfo->uniqueKeys[i].constraintType ==
            QD_PRIMARYKEY)
        {
            sUniqueConstraint = &(sOldTableInfo->uniqueKeys[i]);
            break;
        }
    }

    IDE_TEST_RAISE(sUniqueConstraint == NULL, ERR_META_CRASH);

    // PROJ-1502 PARTITIONED DISK TABLE
    for( i = 0; i < sOldTableInfo->indexCount; i++)
    {
        if( sOldTableInfo->indices[i].indexId ==
            sUniqueConstraint->constraintIndex->indexId )
        {
            sIndex = &sOldTableInfo->indices[i];
        }
    }

    IDE_TEST_RAISE(sIndex == NULL, ERR_META_CRASH);

    if( sOldTableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
    {
        // non-partitioned index ����
        if ( sIndex->indexPartitionType == QCM_NONE_PARTITIONED_INDEX )
        {
            if ( sOldIndexTable != NULL )
            {
                IDE_TEST( qdx::dropIndexTable( aStatement,
                                               sOldIndexTable,
                                               ID_FALSE /* aIsDropTablespace */ )
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

        // primary key index�� ��� non-partitioned index�� partitioned index
        // �� �� �����Ѵ�.
        // local index ����
        IDE_TEST(qdd::dropIndexPartitions( aStatement,
                                           sOldPartInfoList,
                                           sIndex->indexId,
                                           ID_FALSE /* aIsCascade */,
                                           ID_FALSE /* aIsDropTablespace */ )
                 != IDE_SUCCESS );
    }

    IDE_TEST(smiTable::dropIndex(
                 QC_SMI_STMT( aStatement ),
                 sOldTableInfo->tableHandle,
                 sUniqueConstraint->constraintIndex->indexHandle,
                 SMI_TBSLV_DDL_DML )
        != IDE_SUCCESS);

    IDE_TEST(qdd::deleteIndicesFromMetaByIndexID(
                 aStatement,
                 sUniqueConstraint->constraintIndex->indexId)
             != IDE_SUCCESS);

    IDU_LIMITPOINT("qdn::executeDropPrimary::malloc1");
    IDE_TEST(STRUCT_ALLOC_WITH_SIZE(aStatement->qmxMem,
                                    SChar,
                                    QD_MAX_SQL_LENGTH,
                                    &sSqlStr)
             != IDE_SUCCESS);

    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                     "DELETE FROM SYS_CONSTRAINTS_ "
                     "WHERE CONSTRAINT_ID = INTEGER'%"ID_INT32_FMT"'",
                     sUniqueConstraint->constraintID );

    IDE_TEST(qcg::runDMLforDDL(QC_SMI_STMT( aStatement ),
                               sSqlStr,
                               & sRowCnt ) != IDE_SUCCESS);

    IDE_TEST_RAISE(sRowCnt != 1, ERR_META_CRASH);

    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                     "DELETE FROM SYS_CONSTRAINT_COLUMNS_ "
                     "WHERE CONSTRAINT_ID = INTEGER'%"ID_INT32_FMT"'",
                     sUniqueConstraint->constraintID );

    IDE_TEST(qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                sSqlStr,
                                & sRowCnt ) != IDE_SUCCESS);

    for (i = 0; i < sUniqueConstraint->constraintColumnCount; i ++)
    {
        sIsNullable = ID_TRUE;

        // BUG-16595
        for (j = 0; j < sOldTableInfo->notNullCount; j++)
        {
            sNotNullConstraint = & (sOldTableInfo->notNulls[j]);

            for (k = 0; k < sNotNullConstraint->constraintColumnCount; k++)
            {
                if (sUniqueConstraint->constraintColumn[i] ==
                    sNotNullConstraint->constraintColumn[k])
                {
                    sIsNullable = ID_FALSE;
                    break;
                }
                else
                {
                    // Nothing to do.
                }
            }
            if (sIsNullable == ID_FALSE)
            {
                break;
            }
            else
            {
                // Nothing to do.
            }
        }

        if (sIsNullable == ID_TRUE)
        {
            // hove to premit nullable flag.
            idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                             "UPDATE SYS_COLUMNS_ SET IS_NULLABLE = 'T' "
                             "WHERE COLUMN_ID = %"ID_INT32_FMT"",
                             sUniqueConstraint->constraintColumn[i] );

            IDE_TEST(qcg::runDMLforDDL(QC_SMI_STMT( aStatement ),
                                       sSqlStr, &sRowCnt) != IDE_SUCCESS);

            IDE_TEST_RAISE(sRowCnt != 1, ERR_META_CRASH);

            IDE_TEST(qdbCommon::makeColumnNullable(
                         aStatement,
                         sOldTableInfo,
                         sUniqueConstraint->constraintColumn[i])
                     != IDE_SUCCESS);
        }
        else
        {
            // Nothing to do.
        }
    }

    sTableID  = sOldTableInfo->tableID;
    sTableOID = smiGetTableId(sOldTableInfo->tableHandle);

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
                                   &sTableHandle) != IDE_SUCCESS);

    // PROJ-1502 PARTITIONED DISK TABLE
    if( sNewTableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
    {
        IDE_TEST( qcmPartition::touchPartitionInfoList( QC_SMI_STMT( aStatement ),
                                                        sOldPartInfoList )
                  != IDE_SUCCESS );

        IDE_TEST( qcmPartition::makeAndSetAndGetQcmPartitionInfoList( aStatement,
                                                                      sNewTableInfo,
                                                                      sOldPartInfoList,
                                                                      & sNewPartInfoList )
                  != IDE_SUCCESS );

        (void)qcmPartition::destroyQcmPartitionInfoList( sOldPartInfoList );
    }

    if ( sOldIndexTable != NULL )
    {
        (void)qcm::destroyQcmTableInfo(sOldIndexTable->tableInfo);
    }
    else
    {
        // Nothing to do.
    }
        
    (void)qcm::destroyQcmTableInfo(sOldTableInfo);

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_SESSION_TEMPORARY_TABLE_EXIST )
    {
        IDE_SET(ideSetErrorCode( qpERR_ABORT_QDB_TEMPORARY_TABLE_DDL_DISABLE ));
    }
    IDE_EXCEPTION(ERR_META_CRASH);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCM_META_CRASH));
    }
    IDE_EXCEPTION_END;

    (void)qcm::destroyQcmTableInfo( sNewTableInfo );
    (void)qcmPartition::destroyQcmPartitionInfoList( sNewPartInfoList );

    qcmPartition::restoreTempInfo( sOldTableInfo,
                                   sOldPartInfoList,
                                   NULL );

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qdn::executeModifyConstr(qcStatement * aStatement)
{
/***********************************************************************
 *
 * Description :
 *      ALTER TABLE ... MODIFY CONSTRAINT ����
 *
 * Implementation :
 *      1. Constraint �̸����� ConstraintID ���ϱ�
 *      2. Constraint State ���Ἲ �˻�
 *       2-1. FK Validate check
 *      3. SYS_CONSTRAINTS_, ��Ÿ ���̺��� ��������
 *      4. ��Ÿ ĳ�� �籸��
 *
 ***********************************************************************/

    qdTableParseTree        * sParseTree;
    UInt                      sConstraintID;
    UInt                      sTableID;
    smOID                     sTableOID;
    qcmTableInfo            * sOldTableInfo = NULL;
    qcmTableInfo            * sNewTableInfo = NULL;
    SChar                   * sSqlStr;
    SChar                     sConstraintName[ QC_MAX_OBJECT_NAME_LEN + 1 ];
    vSLong                    sRowCnt;
    void                    * sTableHandle;
    smSCN                     sSCN;
    qcmPartitionInfoList    * sOldPartInfoList = NULL;
    qcmPartitionInfoList    * sNewPartInfoList = NULL;
    idBool                    sIsPartitioned = ID_FALSE;
    UInt                      i;
    qcmForeignKey           * sForeignKey = NULL;
    qdConstraintState       * sConstraintState = NULL;

    static const SChar      * sTrueFalseStr[2] = {"T", "F"};
    const SChar             * sIsValidated;
    idBool                    sDoModifyMeta = ID_TRUE;
    qcuSqlSourceInfo          sqlInfo;

    sParseTree = (qdTableParseTree *)aStatement->myPlan->parseTree;
    sConstraintState = sParseTree->constraints->constrState;

    // Table�� ���� Lock�� ȹ���Ѵ�.
    IDE_TEST( qcm::validateAndLockTable(aStatement,
                                        sParseTree->tableHandle,
                                        sParseTree->tableSCN,
                                        SMI_TABLE_LOCK_X)
              != IDE_SUCCESS);

    sOldTableInfo = sParseTree->tableInfo;

    // PROJ-1407 Temporary table
    // session temporary table�� �����ϴ� ��� DDL�� �� �� ����.
    IDE_TEST_RAISE( qcuTemporaryObj::existSessionTable( sOldTableInfo ) == ID_TRUE,
                    ERR_SESSION_TEMPORARY_TABLE_EXIST );

    // PROJ-1502 PARTITIONED DISK TABLE
    if( sOldTableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
    {
        sIsPartitioned = ID_TRUE;

        // ��� ��Ƽ�ǿ� LOCK(X)
        IDE_TEST( qcmPartition::validateAndLockPartitionInfoList( aStatement,
                                                                  sParseTree->partTable->partInfoList,
                                                                  SMI_TBSLV_DDL_DML, // TBS Validation �ɼ�
                                                                  SMI_TABLE_LOCK_X,
                                                                  smiGetDDLLockTimeOut((QC_SMI_STMT(aStatement))->getTrans()))
                  != IDE_SUCCESS );

        // ���� ó���� ���Ͽ�, Lock�� ���� �Ŀ� Partition List�� �����Ѵ�.
        sOldPartInfoList = sParseTree->partTable->partInfoList;
    }

    QC_STR_COPY( sConstraintName, sParseTree->constraints->constrName );

    // 1. constraint�� �����ؾ� ��.
    sConstraintID = qcmCache::getConstraintIDByName(sParseTree->tableInfo,
                                                    sConstraintName,
                                                    NULL);

    IDE_TEST_RAISE(sConstraintID == 0, ERR_NOT_EXIST_CONSTRAINT_NAME);

    // 2. Constraint State ���Ἲ �˻�
    sForeignKey = sOldTableInfo->foreignKeys;

    for( i = 0; i < sOldTableInfo->foreignKeyCount; i++ )
    {
        if ( idlOS::strMatch( sConstraintName,
                              idlOS::strlen( sConstraintName ),
                              sForeignKey[i].name,
                              idlOS::strlen( sForeignKey[i].name ) ) == 0 )
        {
            // if curruent constraint state is same to new one, do nothing
            if( sForeignKey[i].validated == sConstraintState->validate )
            {
                // Set flag on to avoid change of Meta table
                sDoModifyMeta = ID_FALSE;
            }
            else
            {
                // Novalidate�� ������ �ʿ䰡 ����, Validate�ÿ��� �����Ѵ�.
                if( sConstraintState->validate == ID_TRUE )
                {
                    // Check data in child table and referenced(parent) table
                    IDE_TEST( qdnForeignKey::checkRef4ModConst(
                              aStatement,
                              sOldTableInfo,
                              &(sForeignKey[i]) )
                          != IDE_SUCCESS );
                }
                else
                {
                    // Nothing to do
                }
            }

            break;
        }
        else
        {
            // Matched FK not found yet, continue.
        }
    }

    // Forein key�� �� ã������ �ٸ� Constraint(Not Null)�̹Ƿ�
    // Constraint state�� �� �� ����. ����
    if( i == sOldTableInfo->foreignKeyCount )
    {
        sqlInfo.setSourceInfo(
            aStatement,
            & sConstraintState->validatePosition );
        IDE_RAISE( ERR_NOT_SUPPORTED_CONSTR_STATE );
    }

    // 3. ��Ÿ ���� ����
    if (sDoModifyMeta == ID_TRUE )
    {
        IDE_TEST(STRUCT_ALLOC_WITH_SIZE(aStatement->qmxMem,
                                        SChar,
                                        QD_MAX_SQL_LENGTH,
                                        &sSqlStr)
                 != IDE_SUCCESS);

        if( sConstraintState->validate == ID_TRUE )
        {
            sIsValidated = sTrueFalseStr[0];
        }
        else
        {
            sIsValidated = sTrueFalseStr[1];
        }

        idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                         "UPDATE  SYS_CONSTRAINTS_ "
                         "SET VALIDATED = CHAR'%s' "
                         "WHERE CONSTRAINT_ID = INTEGER'%"ID_INT32_FMT"'",
                         sIsValidated,
                         sConstraintID );

        IDE_TEST(qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                    sSqlStr,
                                    & sRowCnt ) != IDE_SUCCESS);

        IDE_TEST_RAISE(sRowCnt != 1, ERR_META_CRASH);

        sTableID = sOldTableInfo->tableID;
        sTableOID = smiGetTableId(sOldTableInfo->tableHandle);

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
                                       &sTableHandle) != IDE_SUCCESS);

        /* PROJ-1723 [MDW/INTEGRATOR] Altibase Plugin ����
           DDL Statement Text�� �α�
        */
        if (QCU_DDL_SUPPLEMENTAL_LOG == 1)
        {
            IDE_TEST( qciMisc::writeDDLStmtTextLog( aStatement,
                                                    sParseTree->tableInfo->tableOwnerID,
                                                    sParseTree->tableInfo->name )
                      != IDE_SUCCESS );
        }

        // PROJ-1502 PARTITIONED DISK TABLE
        if( sIsPartitioned == ID_TRUE )
        {
            IDE_TEST( qcmPartition::touchPartitionInfoList( QC_SMI_STMT( aStatement ),
                                                            sOldPartInfoList )
                      != IDE_SUCCESS );

            IDE_TEST( qcmPartition::makeAndSetAndGetQcmPartitionInfoList( aStatement,
                                                                          sNewTableInfo,
                                                                          sOldPartInfoList,
                                                                          & sNewPartInfoList )
                      != IDE_SUCCESS );

            (void)qcmPartition::destroyQcmPartitionInfoList( sOldPartInfoList );
        }

        (void)qcm::destroyQcmTableInfo(sOldTableInfo);
    }
    else
    {
        // Constraint State does not changed, Nothing to do.
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_SESSION_TEMPORARY_TABLE_EXIST )
    {
        IDE_SET(ideSetErrorCode( qpERR_ABORT_QDB_TEMPORARY_TABLE_DDL_DISABLE ));
    }
    IDE_EXCEPTION(ERR_META_CRASH);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCM_META_CRASH));
    }
    IDE_EXCEPTION(ERR_NOT_EXIST_CONSTRAINT_NAME)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDN_NOT_EXISTS_CONSTRAINT));
    }
    IDE_EXCEPTION(ERR_NOT_SUPPORTED_CONSTR_STATE);
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QDN_NOT_SUPPORTED_CONSTR_STATE,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION_END;

    (void)qcm::destroyQcmTableInfo( sNewTableInfo );
    (void)qcmPartition::destroyQcmPartitionInfoList( sNewPartInfoList );

    qcmPartition::restoreTempInfo( sOldTableInfo,
                                   sOldPartInfoList,
                                   NULL );

    return IDE_FAILURE;
}

IDE_RC qdn::insertConstraintIntoMeta(
    qcStatement *aStatement,
    UInt         aUserID,
    UInt         aTableID,
    UInt         aConstrID,
    SChar*       aConstrName,
    UInt         aConstrType,
    UInt         aIndexID,
    UInt         aColumnCnt,
    UInt         aReferencedTblID,
    UInt         aReferencedIndexID,
    UInt         aReferencedRule,
    SChar       *aCheckCondition, /* PROJ-1107 Check Constraint ���� */
    idBool       aValidated )
{
/***********************************************************************
 *
 * Description :
 *      SYS_CONSTRAINTS_ ��Ÿ ���̺� constraint ���� �Է�
 *
 * Implementation :
 *
 ***********************************************************************/

    SChar               * sSqlStr;
    vSLong  sRowCnt;
    static const SChar  * sTrueFalseStr[2] = {"T", "F"};
    const SChar         * sIsValidated;

    if( aValidated == ID_TRUE )
    {
        sIsValidated = sTrueFalseStr[0];
    }
    else
    {
        sIsValidated = sTrueFalseStr[1];
    }

    IDU_FIT_POINT( "qdn::insertConstraintIntoMeta::alloc::sSqlStr",
                    idERR_ABORT_InsufficientMemory );

    IDE_TEST(STRUCT_ALLOC_WITH_SIZE(aStatement->qmxMem,
                                    SChar,
                                    QD_MAX_SQL_LENGTH,
                                    &sSqlStr)
             != IDE_SUCCESS);

    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                     "INSERT INTO SYS_CONSTRAINTS_ VALUES("
                     "INTEGER'%"ID_INT32_FMT"', "
                     "INTEGER'%"ID_INT32_FMT"', "
                     "INTEGER'%"ID_INT32_FMT"', "
                     "VARCHAR'%s', "
                     "INTEGER'%"ID_INT32_FMT"', "
                     "INTEGER'%"ID_INT32_FMT"', "
                     "INTEGER'%"ID_INT32_FMT"', "
                     "INTEGER'%"ID_INT32_FMT"', "
                     "INTEGER'%"ID_INT32_FMT"', "
                     "INTEGER'%"ID_INT32_FMT"', "
                     "VARCHAR'%s', "
                     "CHAR'%s')",
                     aUserID,
                     aTableID,
                     aConstrID,
                     aConstrName,
                     aConstrType,
                     aIndexID,
                     aColumnCnt,
                     aReferencedTblID,
                     aReferencedIndexID,
                     aReferencedRule,
                     aCheckCondition,
                     sIsValidated );

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

IDE_RC qdn::insertConstraintColumnIntoMeta(
    qcStatement *aStatement,
    UInt         aUserID,
    UInt         aTableID,
    UInt         aConstrID,
    UInt         aConstColOrder,
    UInt         aColID)
{
/***********************************************************************
 *
 * Description :
 *      SYS_CONSTRAINT_COLUMNS_ ��Ÿ ���̺� constraint �÷� ���� �Է�
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qdn::insertConstraintColumnIntoMeta"

    SChar               * sSqlStr;
    vSLong sRowCnt;

    IDU_FIT_POINT( "qdn::insertConstraintColumnIntoMeta::alloc::sSqlStr",
                    idERR_ABORT_InsufficientMemory );

    IDE_TEST(STRUCT_ALLOC_WITH_SIZE(aStatement->qmxMem,
                                    SChar,
                                    QD_MAX_SQL_LENGTH,
                                    &sSqlStr)
             != IDE_SUCCESS);

    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                     "INSERT INTO SYS_CONSTRAINT_COLUMNS_ VALUES ( "
                     "INTEGER'%"ID_INT32_FMT"', "
                     "INTEGER'%"ID_INT32_FMT"', "
                     "INTEGER'%"ID_INT32_FMT"', "
                     "INTEGER'%"ID_INT32_FMT"', "
                     "INTEGER'%"ID_INT32_FMT"')",
                     aUserID,
                     aTableID,
                     aConstrID,
                     aConstColOrder,
                     aColID );

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

IDE_RC qdn::copyConstraintRelatedMeta( qcStatement * aStatement,
                                       UInt          aToTableID,
                                       UInt          aToConstraintID,
                                       UInt          aFromConstraintID )
{
/***********************************************************************
 *
 * Description :
 *      SYS_CONSTRAINT_RELATED_�� Constraint ������ �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    SChar  * sSqlStr = NULL;
    vSLong   sRowCnt = ID_vLONG(0);

    IDU_FIT_POINT( "qdn::copyConstraintRelatedMeta::STRUCT_ALLOC_WITH_SIZE::sSqlStr",
                   idERR_ABORT_InsufficientMemory );

    IDE_TEST( STRUCT_ALLOC_WITH_SIZE( QC_QMX_MEM( aStatement ),
                                      SChar,
                                      QD_MAX_SQL_LENGTH,
                                      & sSqlStr )
              != IDE_SUCCESS );

    idlOS::snprintf( sSqlStr,
                     QD_MAX_SQL_LENGTH,
                     "INSERT INTO SYS_CONSTRAINT_RELATED_ SELECT "
                     "USER_ID, "
                     "INTEGER'%"ID_INT32_FMT"', "
                     "INTEGER'%"ID_INT32_FMT"', "
                     "RELATED_USER_ID, RELATED_PROC_NAME FROM SYS_CONSTRAINT_RELATED_ WHERE CONSTRAINT_ID = "
                     "INTEGER'%"ID_INT32_FMT"'",
                     aToTableID,
                     aToConstraintID,
                     aFromConstraintID );

    IDE_TEST( qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                 sSqlStr,
                                 & sRowCnt )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

idBool
qdn::matchColumnId( qcmColumn * aColumnList,
                    UInt      * aColIdList,
                    UInt        aKeyColCount,
                    UInt      * aColFlagList )
{
/***********************************************************************
 *
 * Description :
 *      qcmColumn �� ����Ʈ�� Column ID �� ����Ʈ�� ID ���� ��� ��������?
 *
 * Implementation :
 *      1. qcmColumn �� ����Ʈ�� ������� Column ID �� ����Ʈ�� ID ����
 *         �������� üũ
 *      2. �� ����Ʈ�� ������ �������� üũ
 *
 ***********************************************************************/

    qcmColumn *sColumn;
    UInt       i;
    idBool     sReturnVal = ID_TRUE;

    i = 0;
    sColumn = aColumnList;

    while (sColumn != NULL)
    {
        if (i >= aKeyColCount)
        {
            return ID_FALSE;
        }

        if (sColumn->basicInfo->column.id != aColIdList[i])
        {
            sReturnVal = ID_FALSE;
            break;
        }

        if (aColFlagList != NULL)
        {
            if ((sColumn->basicInfo->column.flag & SMI_COLUMN_ORDER_MASK) !=
                (aColFlagList[i] & SMI_COLUMN_ORDER_MASK) )
            {
                sReturnVal = ID_FALSE;
                break;
            }
        }

        sColumn = sColumn->next;
        i ++;
    }

    if (i != aKeyColCount)
    {
        sReturnVal = ID_FALSE;
    }

    return sReturnVal;
}

idBool
qdn::matchColumnIdOutOfOrder( qcmColumn * aColumnList,
                              UInt      * aColIdList,
                              UInt        aKeyColCount,
                              UInt      * aColFlagList )
{
/***********************************************************************
 *
 * Description :
 *      qcmColumn �� ����Ʈ�� Column ID �� ����Ʈ�� ID ���� ������
 *      ������� �������� Ȯ��
 *
 * Implementation :
 *      1. qcmColumn ����Ʈ�� �� ID�� Column ID �� ����Ʈ�� ���
 *         ���ԵǾ����� Ȯ��
 *      2. �� ����Ʈ�� ������ �������� üũ
 *
 ***********************************************************************/

    qcmColumn *sColumn;
    UInt       i;
    UInt       sColumnCount;
    idBool     sReturnVal = ID_TRUE;

    sColumnCount = 0;
    sColumn = aColumnList;

    while (sColumn != NULL)
    {
        if (sColumnCount >= aKeyColCount)
        {
            sReturnVal = ID_FALSE;
            break;
        }
        else
        {
            // Nothing to do.
        }

        for (i = 0; i < aKeyColCount; i++)
        {
            if (sColumn->basicInfo->column.id == aColIdList[i])
            {
                if (aColFlagList != NULL)
                {
                    if ((sColumn->basicInfo->column.flag & SMI_COLUMN_ORDER_MASK) !=
                        (aColFlagList[i] & SMI_COLUMN_ORDER_MASK) )
                    {
                        sReturnVal = ID_FALSE;
                        IDE_CONT( NORMAL_EXIT );
                    }
                    else
                    {
                        // Nothing to do.
                    }
                }

                break;
            }
            else
            {
                // Nothing to do.
            }
        }

        if (i >= aKeyColCount)
        {
            sReturnVal = ID_FALSE;
            break;
        }
        else
        {
            // Nothing to do.
        }

        sColumn = sColumn->next;
        sColumnCount ++;
    }

    // list�� array�� ���̰� ������ ��
    if (sColumnCount != aKeyColCount)
    {
        sReturnVal = ID_FALSE;
    }
    else
    {
        // Nothing to do.
    }

    IDE_EXCEPTION_CONT( NORMAL_EXIT );
    return sReturnVal;
}

idBool qdn::matchColumnList(
    qcmColumn   *aColList1,
    qcmColumn   *aColList2 )
{
/***********************************************************************
 *
 * Description :
 *      qcmColumn �� �� ����Ʈ�� �÷� �̸��� ��������?
 *
 * Implementation :
 *      1. qcmColumn �� �� ����Ʈ�� �÷� �̸��� �������� üũ
 *      2. �� ����Ʈ�� ������ �������� üũ
 *
 ***********************************************************************/

    qcmColumn *sColumn1;
    qcmColumn *sColumn2;

    sColumn1 = aColList1;
    sColumn2 = aColList2;

    while (sColumn1 != NULL && sColumn2 != NULL)
    {
        if ( QC_IS_NAME_MATCHED( sColumn1->namePos, sColumn2->namePos ) == ID_FALSE )
        {
            return ID_FALSE;
        }
        sColumn1 = sColumn1->next;
        sColumn2 = sColumn2->next;
    }
    if (sColumn1 != NULL || sColumn2 != NULL)
    {
        return ID_FALSE;
    }

    return ID_TRUE;
}

idBool qdn::matchColumnListOutOfOrder(
    qcmColumn   *aColList1,
    qcmColumn   *aColList2 )
{
/***********************************************************************
 *
 * Description :
 *      �� ����Ʈ�� ���Ե� Į������ ����������� ������ üũ
 *
 * Implementation :
 *      1. qcmColumn �� �� ����Ʈ�� ���Ե� Į������ ������ üũ
 *      2. �� ����Ʈ�� ������ �������� üũ
 *
 ***********************************************************************/

    qcmColumn *sColumn1;
    qcmColumn *sColumn2;
    idBool     sReturnVal = ID_TRUE;

    sColumn1 = aColList1;

    while (sColumn1 != NULL )
    {
        sColumn2 = aColList2;

        while( sColumn2 != NULL )
        {
            if ( QC_IS_NAME_MATCHED( sColumn1->namePos, sColumn2->namePos ) )
            {
                break;
            }
            else
            {
                // Nothing to do.
            }
            sColumn2 = sColumn2->next;
        }
        if( sColumn2 == NULL )
        {
            sReturnVal = ID_FALSE;
            break;
        }
        else
        {
            // Nothing to do.
        }
        sColumn1 = sColumn1->next;
    }

    if( sReturnVal == ID_TRUE )
    {
        // list�� ���̰� ������ ��
        sColumn1 = aColList1;
        sColumn2 = aColList2;
        while( ( sColumn1 != NULL ) && ( sColumn2 != NULL ) )
        {
            sColumn1 = sColumn1->next;
            sColumn2 = sColumn2->next;
        }

        if( ( sColumn1 != NULL ) || ( sColumn2 != NULL ) )
        {
            sReturnVal = ID_FALSE;
        }
        else
        {
            // Nothing to do.
        }
    }

    return sReturnVal;
}

idBool qdn::intersectColumn( UInt *aColumnIDList1,
                             UInt aColumnIDCount1,
                             UInt *aColumnIDList2,
                             UInt aColumnIDCount2)
{
/***********************************************************************
 *
 * Description :
 *      aColumnIDList2 �� aColumnIDList1 �÷��� ���ϴ��� �˻�
 *
 * Implementation :
 *      aColumnIDList2 ���� �÷��� �ϳ��� aColumnIDList1 �� ���ϸ�
 *         ID_TRUE ��ȯ
 *
 ***********************************************************************/

#define IDE_FN "qdn::intersectColumn"

    UInt   i;
    UInt   j;

    if (aColumnIDList1 == NULL || aColumnIDList2 == NULL)
    {
        return ID_TRUE;
    }

    for (i = 0; i < aColumnIDCount1; i++)
    {
        for (j = 0; j < aColumnIDCount2; j++)
        {
            if ((aColumnIDList1[i] == aColumnIDList2[j]) &&
                (aColumnIDList1[i] != 0))
            {
                return ID_TRUE;
            }
        }
    }
    return ID_FALSE;

#undef IDE_FN
}

idBool qdn::intersectColumn( mtcColumn  * aColumnList1,
                             UInt         aColumnCount1,
                             UInt       * aColumnIDList2,
                             UInt         aColumnIDCount2 )
{
/***********************************************************************
 *
 * Description :
 *      aColumnIDList2 �� aColumnIDList1 �÷��� ���ϴ��� �˻�
 *
 * Implementation :
 *      aColumnIDList2 ���� �÷��� �ϳ��� aColumnIDList1 �� ���ϸ�
 *         ID_TRUE ��ȯ
 *
 ***********************************************************************/

#define IDE_FN "qdn::intersectColumn"

    UInt   i;
    UInt   j;

    if ( ( aColumnList1 == NULL ) || ( aColumnIDList2 == NULL ) )
    {
        return ID_TRUE;
    }
    else
    {
        /* Nothing to do */
    }

    for (i = 0; i < aColumnCount1; i++)
    {
        for (j = 0; j < aColumnIDCount2; j++)
        {
            if ((aColumnList1[i].column.id == aColumnIDList2[j]) &&
                (aColumnList1[i].column.id != 0))
            {
                return ID_TRUE;
            }
            else
            {
                /* Nothing to do */
            }
        }
    }
    return ID_FALSE;

#undef IDE_FN
}

idBool qdn::existNotNullConstraint( qcmTableInfo * aTableInfo,
                                    UInt         * aCols,
                                    UInt           aColCount )
{
/***********************************************************************
 *
 * Description :
 *      aCols �� NOT NULL Constraint�� �ִ��� üũ
 *
 * Implementation :
 *      1. ���̺��� not null constraint �߿��� aCols �÷��� ���Ե� ����
 *         ������ ID_TRUE ��ȯ
 *
 ***********************************************************************/

    UInt        i;

    for (i = 0; i < aTableInfo->notNullCount; i++)
    {
        if (intersectColumn(aTableInfo->notNulls[i].constraintColumn,
                            aTableInfo->notNulls[i].constraintColumnCount,
                            aCols,
                            aColCount) == ID_TRUE)
        {
            return ID_TRUE;
        }
    }

    return ID_FALSE;
}

IDE_RC qdn::existSameConstrName(qcStatement  * aStatement,
                                SChar        * aConstrName,
                                UInt           aTableOwnerID,
                                idBool       * aExistSameConstrName )
{
/***********************************************************************
 *
 * Description :
 *      sys_constraint_ ��Ÿ ���̺� ������ constraint name��
 *      �����ϴ��� �˻�
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qdn::existSameConstrName"

    mtcColumn      * sConstrNameCol;
    mtcColumn      * sConstrUserIDCol;
    mtdCharType    * sConstrName;
    mtdIntegerType   sConstrUserID;

    SChar            sCurConstrName[ QC_MAX_OBJECT_NAME_LEN + 1 ];
    const void     * sRow;
    smiTableCursor   sCursor;
    scGRID           sRid;
    volatile SInt    sStage;

    IDE_FT_BEGIN();

    IDU_FIT_POINT_FATAL( "qdn::existSameConstrName::__FT__" );
    sCursor.initialize();

    sStage =0;
    *aExistSameConstrName = ID_FALSE;

    // To fix BUG-13544
    // constraint name�� user-id���� unique�ؾ� ��
    // constraint user_id column ����
    IDE_TEST( smiGetTableColumns( gQcmConstraints,
                                  QCM_CONSTRAINTS_USER_ID_COL_ORDER,
                                  (const smiColumn**)&sConstrUserIDCol )
              != IDE_SUCCESS );

    // constraint name column ����
    IDE_TEST( smiGetTableColumns( gQcmConstraints,
                                  QCM_CONSTRAINTS_CONSTRAINT_NAME_COL_ORDER,
                                  (const smiColumn**)&sConstrNameCol )
              != IDE_SUCCESS );

    // cursor open
    IDE_TEST(sCursor.open(QC_SMI_STMT( aStatement ),
                          gQcmConstraints,
                          NULL,
                          smiGetRowSCN(gQcmConstraints),
                          NULL,
                          smiGetDefaultKeyRange(),
                          smiGetDefaultKeyRange(),
                          smiGetDefaultFilter(),
                          QCM_META_CURSOR_FLAG,
                          SMI_SELECT_CURSOR,
                          &gMetaDefaultCursorProperty) != IDE_SUCCESS);

    sStage = 1;
    IDU_FIT_POINT_FATAL( "qdn::existSameConstrName::__FT__::STAGE1" );

    IDE_TEST(sCursor.beforeFirst() != IDE_SUCCESS);

    IDE_TEST(sCursor.readRow(&sRow, &sRid, SMI_FIND_NEXT) != IDE_SUCCESS);

    while( sRow != NULL )
    {
        sConstrName = (mtdCharType *)
            ((UChar *)sRow + sConstrNameCol->column.offset);

        sConstrUserID = *(mtdIntegerType *)
            ((UChar *)sRow + sConstrUserIDCol->column.offset);

        idlOS::memcpy( sCurConstrName,
                       sConstrName->value,
                       sConstrName->length );

        sCurConstrName[sConstrName->length] = '\0';

        // To fix BUG-13544
        // constraint name�� user-id���� unique�ؾ� ��
        if ( ( idlOS::strMatch( sCurConstrName,
                                idlOS::strlen( sCurConstrName ),
                                aConstrName ,
                                idlOS::strlen( aConstrName ) ) == 0 ) &&
             ( (UInt)sConstrUserID == aTableOwnerID ) )
        {
            *aExistSameConstrName = ID_TRUE;
            break;
        }
        else
        {
            // nothing to do
        }

        IDE_TEST(sCursor.readRow(&sRow, &sRid, SMI_FIND_NEXT) != IDE_SUCCESS);
    }

    sStage = 0;

    IDE_TEST(sCursor.close() != IDE_SUCCESS);

    IDE_FT_END();

    return IDE_SUCCESS;

    IDE_EXCEPTION_SIGNAL()
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_FAULT_TOLERATED ) );
    }
    IDE_EXCEPTION_END;

    IDE_FT_EXCEPTION_BEGIN();

    switch( sStage )
    {
        case 1 :
            sCursor.close();
            break;
        default :
            IDE_DASSERT( 0 );
    }

    IDE_FT_EXCEPTION_END();

    return IDE_FAILURE;

#undef IDE_FN
}

// PROJ-2642 Table on Replication Allow DDL
// �������� Ÿ�Կ� ���� DDL ���� ���� ���� 
IDE_RC qdn::checkOperatableForReplication( qcStatement     * aStatement,
                                           qcmTableInfo    * aTableInfo,
                                           UInt              aConstrType,
                                           smOID           * aReplicatedTableOIDArray,
                                           UInt              aReplicatedTableOIDCount )
{
    UInt        sDDLRequireLevel = 0;

    switch ( aConstrType )
    {
        case QD_FOREIGN:
            /* BUG-42881 
             * CHECK_FK_IN_CREATE_REPLICATION_DISABLE �� 1�� �����Ǿ� ������ 
             * Replication ��� ���̺��̾ FK �� �߰��ϰų� �����Ҽ� �ֽ��ϴ�.
             */   
            IDE_TEST_RAISE( QCU_CHECK_FK_IN_CREATE_REPLICATION_DISABLE == 0,
                            ERR_DDL_WITH_REPLICATED_TABLE );

            sDDLRequireLevel = 0;
            break;

        case QD_NOT_NULL:
        case QD_UNIQUE:
        case QD_LOCAL_UNIQUE:
        case QD_CHECK:
        case QD_TIMESTAMP:

            sDDLRequireLevel = 1;
            break;

        default:
            // if specified table is replicated, the error
            IDE_RAISE( ERR_DDL_WITH_REPLICATED_TABLE );
            break;
    }

    IDE_TEST( qci::mManageReplicationCallback.mIsDDLEnableOnReplicatedTable( sDDLRequireLevel,
                                                                             aTableInfo )
              != IDE_SUCCESS );

    IDE_TEST_RAISE( QC_SMI_STMT( aStatement )->getTrans()->getReplicationMode() == SMI_TRANSACTION_REPL_NONE,
                    ERR_CANNOT_WRITE_REPL_INFO );

    IDE_TEST( qciMisc::checkRunningEagerReplicationByTableOID( aStatement,
                                                               aReplicatedTableOIDArray,
                                                               aReplicatedTableOIDCount )
              != IDE_SUCCESS );

    IDE_TEST( qci::mManageReplicationCallback.mStopReceiverThreads( QC_SMI_STMT(aStatement),
                                                                    aStatement->mStatistics,
                                                                    aReplicatedTableOIDArray,
                                                                    aReplicatedTableOIDCount )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_DDL_WITH_REPLICATED_TABLE )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDB_DDL_WITH_REPLICATED_TBL ) );
    }
    IDE_EXCEPTION( ERR_CANNOT_WRITE_REPL_INFO )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDB_CANNOT_WRITE_REPL_INFO ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

// PROJ-2642 Table on Replication Allow DDL
UInt qdn::getConstraintType( qcmTableInfo       * aTableInfo,
                             UInt                 aConstraintID )
{
    UInt            i = 0;
    UInt            sConstraintType = QD_CONSTR_MAX;
    qcmUnique     * sUniqueConstraint = NULL;
    qcmForeignKey * sForeignKeyConstraint = NULL;
    qcmNotNull    * sNotNullConstraint = NULL;
    qcmCheck      * sCheckConstraint = NULL;

    /* Check Unique */
    for ( i = 0; i < aTableInfo->uniqueKeyCount; i++ )
    {
        sUniqueConstraint = &(aTableInfo->uniqueKeys[i]);

        if ( sUniqueConstraint->constraintID == aConstraintID )
        {
            if ( sUniqueConstraint->constraintType == QD_PRIMARYKEY )
            {
                sConstraintType = QD_PRIMARYKEY;
            }
            else
            {
                sConstraintType = QD_UNIQUE;
            }

            IDE_CONT( PASS );
        }
        else
        {
            /* do nothing */
        }
    }

    for ( i = 0; i < aTableInfo->foreignKeyCount; i++ )
    {
        sForeignKeyConstraint = &(aTableInfo->foreignKeys[i]);

        if ( sForeignKeyConstraint->constraintID == aConstraintID )
        {
            sConstraintType = QD_FOREIGN;

            IDE_CONT( PASS );
        }
        else
        {
            /* do nothing */
        }
    }

    /* Check Not Null */
    for ( i = 0; i < aTableInfo->notNullCount; i++ )
    {
        sNotNullConstraint = &(aTableInfo->notNulls[i]);

        if ( sNotNullConstraint->constraintID == aConstraintID )
        {
            sConstraintType = QD_NOT_NULL;

            IDE_CONT( PASS );
        }
        else
        {
            /* do nothing */
        }
    }

    /* Check Check */
    for ( i = 0; i < aTableInfo->checkCount; i++ )
    {
        sCheckConstraint = &(aTableInfo->checks[i]);

        if ( sCheckConstraint->constraintID == aConstraintID )
        {
            sConstraintType = QD_CHECK;

            IDE_CONT( PASS );
        }
        else
        {
            /* do nothing */
        }
    }

    if ( aTableInfo->timestamp != NULL )
    {
        if ( aTableInfo->timestamp->constraintID == aConstraintID )
        {
            sConstraintType = QD_TIMESTAMP;
        }
        else
        {
            /* do nothing */
        }
    }
    else
    {
        /* do nothing */
    }

    IDE_EXCEPTION_CONT( PASS );

    return sConstraintType;
}

