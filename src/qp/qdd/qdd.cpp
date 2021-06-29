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
 * $Id: qdd.cpp 90824 2021-05-13 05:35:21Z minku.kang $
 **********************************************************************/

#include <idl.h>
#include <ide.h>
#include <qci.h>
#include <qdd.h>
#include <qdbCommon.h>
#include <qds.h>
#include <qdm.h>
#include <qcmUser.h>
#include <qcmView.h>
#include <qcmMView.h>
#include <qcuTemporaryObj.h>
#include <qcuSqlSourceInfo.h>
#include <qcg.h>
#include <qcmProc.h>
#include <qsx.h>
#include <qdpPrivilege.h>
#include <qdpDrop.h>
#include <qdnTrigger.h>
#include <qcmTrigger.h>
#include <qcmSynonym.h>
#include <qcmDirectory.h>

#include <qcmPartition.h>
#include <qmv.h>
#include <qcsModule.h>
#include <qdbComment.h>
#include <qdx.h>
#include <qcmCache.h>
#include <qdbAlter.h>
#include <qmsDefaultExpr.h>
#include <qdkParseTree.h>
#include <qdk.h>
#include <qcmAudit.h>
#include <qcmDictionary.h>
#include <qcmPkg.h>
#include <qdpRole.h>
/* PROJ-2441 flashback */
#include <qdbFlashback.h>
#include <smiTableSpace.h>
#include <sdi.h>

/***********************************************************************
 * VALIDATE
 **********************************************************************/

IDE_RC qdd::parseDropIndex( qcStatement * aStatement )
{
/***********************************************************************
 *
 * Description :
 *    DROP INDEX ... �� parsing ����
 *
 * Implementation :
 *    1. Table�� Index ������ ��´�.
 *    2. Table Lock�� ȹ���Ѵ�.
 *    3. Function-based Index�̸�,
 *       'ALTER TABLE ... DROP COLUMN ...'���� �����Ѵ�.
 *
 ***********************************************************************/

    qdDropParseTree        * sDropParseTree        = NULL;
    qdTableParseTree       * sTableParseTree       = NULL;
    UInt                     sTableID;
    UInt                     sIndexID;
    qcmTableInfo           * sInfo                 = NULL;
    idBool                   sIsFunctionBasedIndex = ID_FALSE;
    mtcColumn              * sMtcColumn            = NULL;
    qcmColumn              * sQcmColumn            = NULL;
    qcmColumn              * sLastColumn           = NULL;
    qcmIndex               * sIndex                = NULL;
    UInt                     i;

    sDropParseTree = (qdDropParseTree *)aStatement->myPlan->parseTree;

    IDE_TEST( qcm::checkIndexByUser( aStatement,
                                     sDropParseTree->userName,
                                     sDropParseTree->objectName,
                                     &(sDropParseTree->userID),
                                     &sTableID,
                                     &sIndexID )
              != IDE_SUCCESS );

    IDE_TEST( qcm::getTableInfoByID( aStatement,
                                     sTableID,
                                     &(sDropParseTree->tableInfo),
                                     &(sDropParseTree->tableSCN),
                                     &(sDropParseTree->tableHandle) )
              != IDE_SUCCESS );

    IDE_TEST( qcm::lockTableForDDLValidation( aStatement,
                                              sDropParseTree->tableHandle,
                                              sDropParseTree->tableSCN )
              != IDE_SUCCESS );

    sInfo = sDropParseTree->tableInfo;

    for ( i = 0; i < sInfo->indexCount; i++ )
    {
        sIndex = &(sInfo->indices[i]);
        if ( sIndexID == sIndex->indexId )
        {
            IDE_TEST( qmsDefaultExpr::isFunctionBasedIndex(
                          sInfo,
                          sIndex,
                          &sIsFunctionBasedIndex )
                      != IDE_SUCCESS );
            break;
        }
        else
        {
            /* Nothing to do */
        }
    }

    if ( sIsFunctionBasedIndex == ID_TRUE )
    {
        IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(qdTableParseTree),
                                                 (void**)&sTableParseTree )
                  != IDE_SUCCESS );
        idlOS::memcpy( &(sTableParseTree->common),
                       &(sDropParseTree->common),
                       ID_SIZEOF(qcParseTree) );
        QD_TABLE_PARSE_TREE_INIT( sTableParseTree );

        sTableParseTree->userName.stmtText = sInfo->tableOwnerName;
        sTableParseTree->userName.offset   = 0;
        sTableParseTree->userName.size     = idlOS::strlen( sInfo->tableOwnerName );

        sTableParseTree->tableName.stmtText = sInfo->name;
        sTableParseTree->tableName.offset   = 0;
        sTableParseTree->tableName.size     = idlOS::strlen( sInfo->name );

        for ( i = 0; i < sIndex->keyColCount; i++ )
        {
            sMtcColumn = &(sIndex->keyColumns[i]);

            IDE_TEST( qcmCache::getColumnByID( sInfo,
                                               sMtcColumn->column.id,
                                               &sQcmColumn )
                      != IDE_SUCCESS );

            if ( (sQcmColumn->flag & QCM_COLUMN_HIDDEN_COLUMN_MASK)
                 == QCM_COLUMN_HIDDEN_COLUMN_TRUE )
            {
                if ( sTableParseTree->columns == NULL )
                {
                    IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(qcmColumn),
                                                             (void**)&sLastColumn )
                              != IDE_SUCCESS );
                    sTableParseTree->columns = sLastColumn;
                }
                else
                {
                    IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(qcmColumn),
                                                             (void**)&sLastColumn->next )
                              != IDE_SUCCESS );
                    sLastColumn = sLastColumn->next;
                }

                QCM_COLUMN_INIT( sLastColumn );

                sLastColumn->namePos.stmtText = sQcmColumn->name;
                sLastColumn->namePos.offset   = 0;
                sLastColumn->namePos.size     = idlOS::strlen( sQcmColumn->name );
            }
            else
            {
                /* Nothing to do */
            }
        }

        IDU_FIT_POINT( "qdd::parseDropIndex::alloc::partTable",
                       idERR_ABORT_InsufficientMemory );

        // PROJ-1502 PARTITIONED DISK TABLE
        IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(qdPartitionedTable),
                                                 (void **) & sTableParseTree->partTable )
                  != IDE_SUCCESS );
        QD_SET_INIT_PART_TABLE( sTableParseTree->partTable );

        /* PROJ-1090 Function-based Index */
        sTableParseTree->dropHiddenColumn = ID_TRUE;

        sTableParseTree->common.validate = qdbAlter::validateDropCol;
        sTableParseTree->common.execute  = qdbAlter::executeDropCol;

        aStatement->myPlan->parseTree = (qcParseTree *)sTableParseTree;
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdd::validateDropTable(qcStatement * aStatement)
{
/***********************************************************************
 *
 * Description :
 *    DROP TABLE ... �� validation ����
 *
 * Implementation :
 *    1. ���̺� ���� ���� üũ
 *    2. ��Ÿ ���̺��̸� ����
 *    3. sequence �� view �̸� ����
 *    4. DropTable ������ �ִ��� üũ
 *    5. ���̺� ����ȭ�� �ɷ������� ����
 *    6. ���̺� uniquekey �� �ְ�, �� Ű�� �����ϴ� child �� ������ ����
 *
 ***********************************************************************/

#define IDE_FN "qdd::validateDropTable"

    qdDropParseTree        * sParseTree;
    qcmTableInfo           * sInfo;
    UInt                     i;
    qcmIndex               * sIndexInfo;
    qcmRefChildInfo        * sChildInfo = NULL;  // BUG-28049
    qcuSqlSourceInfo         sqlInfo;
    idBool                   sMemRecyclebinAvail  = ID_TRUE;
    idBool                   sDiskRecyclebinAvail = ID_TRUE;
    qcmColumn              * sColumn    = NULL;
    
    sParseTree = (qdDropParseTree *)aStatement->myPlan->parseTree;

    /* BUG-30059 */
    if ( qdbCommon::containDollarInName( &(sParseTree->objectName) ) == ID_TRUE )
    {
        sqlInfo.setSourceInfo(
            aStatement,
            &(sParseTree->objectName) );

        IDE_RAISE( CANT_USE_RESERVED_WORD );
    }

    // check table exist.
    IDE_TEST( qdbCommon::checkTableInfo( aStatement,
                                         sParseTree->userName,
                                         sParseTree->objectName,
                                         &(sParseTree->userID),
                                         &(sParseTree->tableInfo),
                                         &(sParseTree->tableHandle),
                                         &(sParseTree->tableSCN))
              != IDE_SUCCESS);

    /* BUG-48290 shard object�� ���� DDL ���� */
    IDE_TEST( sdi::checkShardObjectForDDL( aStatement, SDI_DDL_TYPE_DROP ) != IDE_SUCCESS );

    IDE_TEST( qcm::lockTableForDDLValidation(aStatement,
                                             sParseTree->tableHandle,
                                             sParseTree->tableSCN)
              != IDE_SUCCESS );

    sInfo = sParseTree->tableInfo;

    if ( QCG_GET_SESSION_USER_ID(aStatement) != QC_SYSTEM_USER_ID )
    {
        // sParseTree->userID is a owner of table
        IDE_TEST_RAISE(sParseTree->userID == QC_SYSTEM_USER_ID,
                       ERR_NOT_DROP_META);
    }
    else
    {
        if ( ( aStatement->session->mQPSpecific.mFlag
               & QC_SESSION_ALTER_META_MASK )
             == QC_SESSION_ALTER_META_DISABLE )
        {
            IDE_RAISE(ERR_NOT_DROP_META);
        }
    }

    // Proj-1360 Queue
    if (sInfo->tableType == QCM_QUEUE_TABLE)
    {
        IDE_TEST_RAISE((sParseTree->flag & QDQ_QUEUE_MASK) != QDQ_QUEUE_TRUE,
                       ERR_DROP_TABLE_USING_DROP_QUEUE);
    }
    else
    {
        IDE_TEST_RAISE((sParseTree->flag & QDQ_QUEUE_MASK) == QDQ_QUEUE_TRUE,
                       ERR_DROP_QUEUE_NOT_A_QUEUE);
    }

    // PR-13725
    // CHECK OPERATABLE
    if( QCM_IS_OPERATABLE_QP_DROP_TABLE( sInfo->operatableFlag ) != ID_TRUE )
    {
        sqlInfo.setSourceInfo( aStatement, &(sParseTree->objectName) );
        IDE_RAISE(ERR_NOT_EXIST_TABLE);
    }

    // check grant
    IDE_TEST( qdpRole::checkDDLDropTablePriv( aStatement,
                                              sParseTree->userID )
              != IDE_SUCCESS );

    // if specified tables is replicated, the ERR_DDL_WITH_REPLICATED_TABLE
    IDE_TEST_RAISE(sInfo->replicationCount > 0,
                   ERR_DDL_WITH_REPLICATED_TABLE);

    //proj-1608:replicationCount�� 0�� �� recovery count�� �׻� 0�̾�� ��
    IDE_DASSERT(sInfo->replicationRecoveryCount == 0);

    // PROJ-1509, BUG-14168
    // DROP TABLE ... CASCADE �ɼ� �߰�
    if( ( sParseTree->flag & QDD_DROP_CASCADE_CONSTRAINTS_MASK )
        == QDD_DROP_CASCADE_CONSTRAINTS_FALSE )
    {
        // CASCADE �ɼ��� ���� ���� ���.
        // check referential constraint
        for (i = 0; i < sInfo->uniqueKeyCount; i++)
        {
            sIndexInfo = sInfo->uniqueKeys[i].constraintIndex;

            IDE_TEST(qcm::getChildKeys(aStatement,
                                       sIndexInfo,
                                       sInfo,
                                       &sChildInfo)
                     != IDE_SUCCESS);

            // BUG-28049
            while (sChildInfo != NULL) // foreign key exists.
            {
                // not self
                IDE_TEST_RAISE( sChildInfo->childTableRef->tableInfo != sInfo,
                                ERR_ABORT_REFERENTIAL_CONSTRAINT_EXIST );

                sChildInfo = sChildInfo->next;
            }
        }
    }
    else
    {
        // CASCADE CONSTRAINTS �ɼ��� �� ���.
        // Nothing To Do
    }

    // PROJ-1502 PARTITIONED DISK TABLE
    if( sInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
    {
        // ��� ��Ƽ�ǿ� LOCK(IS)
        // ��Ƽ�� ����Ʈ�� �Ľ�Ʈ���� �޾Ƴ��´�.
        IDE_TEST( qdbCommon::checkAndSetAllPartitionInfo(
                      aStatement,
                      sInfo->tableID,
                      & (sParseTree->partInfoList) )
                  != IDE_SUCCESS );

        // PROJ-1624 non-partitioned index
        IDE_TEST( qdx::makeAndLockIndexTableList(
                      aStatement,
                      ID_FALSE,
                      sInfo,
                      &(sParseTree->oldIndexTables) )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    /* PROJ-2441 flashback */
    if ( ( QCG_GET_SESSION_RECYCLEBIN_ENABLE( aStatement )
           == QCU_RECYCLEBIN_OFF ) ||
         ( smiTableSpace::isVolatileTableSpaceType( sInfo->TBSType )
           == ID_TRUE ) ||
         ( qcuTemporaryObj::isTemporaryTable( sInfo )
           == ID_TRUE ) ||
         ( sInfo->tableType == QCM_QUEUE_TABLE ) )
    {
        /* Nothing to do */
    }
    else
    {
        sParseTree->useRecycleBin = ID_TRUE;

        /* PROJ-2441 flashback ���� �÷��� ���� ��� recyclebin �̿� ���� �� �Ѵ�.*/
        for ( i = 0; i < sInfo->columnCount; i++ )
        {
            sColumn = &sInfo->columns[i];
            IDE_TEST_RAISE( (sColumn->basicInfo->module->flag & MTD_ENCRYPT_TYPE_MASK)
                             == MTD_ENCRYPT_TYPE_TRUE,
                             ERR_EXIST_ENCRYPTED_COLUMN );
        }

        /* PROJ-2441 flashback recyclebin usage check */
        IDE_TEST( qdbFlashback::checkRecyclebinSpace( aStatement,
                                                      sParseTree->tableInfo,
                                                      &sMemRecyclebinAvail,
                                                      &sDiskRecyclebinAvail )
                 != IDE_SUCCESS );
        IDE_TEST_RAISE( ( sMemRecyclebinAvail == ID_FALSE ) &&
                        ( sDiskRecyclebinAvail == ID_FALSE ),
                        ERR_CANNOT_DROP_RECYCLEBIN_BOTH_FULL );
        IDE_TEST_RAISE( sMemRecyclebinAvail == ID_FALSE,
                        ERR_CANNOT_DROP_RECYCLEBIN_MEM_FULL );
        IDE_TEST_RAISE( sDiskRecyclebinAvail == ID_FALSE,
                        ERR_CANNOT_DROP_RECYCLEBIN_DISK_FULL );
    }

    if ( QCG_GET_SESSION_IS_NEED_DDL_INFO( aStatement ) == ID_TRUE )
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

    IDE_EXCEPTION(ERR_NOT_DROP_META);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDD_NO_DROP_META_TABLE));
    }
    IDE_EXCEPTION(ERR_ABORT_REFERENTIAL_CONSTRAINT_EXIST);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDB_REFERENTIAL_CONSTRAINT_EXIST));
    }
    IDE_EXCEPTION(ERR_DDL_WITH_REPLICATED_TABLE)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDD_DDL_WITH_REPLICATED_TBL));
    }
    IDE_EXCEPTION(ERR_NOT_EXIST_TABLE)
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QCV_NOT_EXISTS_TABLE,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION(ERR_DROP_TABLE_USING_DROP_QUEUE);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDQ_QUEUE_DROP_TABLE_DENIED));
    }
    IDE_EXCEPTION(ERR_DROP_QUEUE_NOT_A_QUEUE);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDQ_TABLE_DROP_QUEUE_DENIED));
    }
    IDE_EXCEPTION( CANT_USE_RESERVED_WORD );
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QDB_RESERVED_WORD_IN_OBJECT_NAME,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION( ERR_CANNOT_DROP_RECYCLEBIN_BOTH_FULL )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDD_CANNOT_DROP_NOT_ENOUGH_RECYCLEBIN_SPACE, "memory and disk" ) );
    }
    IDE_EXCEPTION( ERR_CANNOT_DROP_RECYCLEBIN_MEM_FULL )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDD_CANNOT_DROP_NOT_ENOUGH_RECYCLEBIN_SPACE, "memory" ) );
    }
    IDE_EXCEPTION( ERR_CANNOT_DROP_RECYCLEBIN_DISK_FULL )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDD_CANNOT_DROP_NOT_ENOUGH_RECYCLEBIN_SPACE, "disk" ) );
    }
    IDE_EXCEPTION( ERR_EXIST_ENCRYPTED_COLUMN );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_EXIST_ENCRYPTED_COLUMN ) );
    }
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qdd::validateDropIndex(qcStatement * aStatement )
{
/***********************************************************************
 *
 * Description :
 *    DROP INDEX ... �� validation ����
 *
 * Implementation :
 *    1. �ε��� ���� ���� üũ, tableID, indexID �� ������
 *    2. ���̺� lock �ɸ鼭 table info ���ϱ� => qcm::getTableInfoByID
 *    3. DropIndex ������ �ִ��� üũ
 *    4. �����ϰ��� �ϴ� �ε����� ���̺��� unique constraint �̸� ����
 *    5. ���̺� uniquekey �� �ְ�, �� Ű�� �����ϴ� child �� ������ ����
 *    6. Replication�� �ɷ�������, Unique Index ���θ� Ȯ��
 *
 ***********************************************************************/

#define IDE_FN "qdd::validateDropIndex"

    qdDropParseTree        * sParseTree;
    UInt                     sTableID;
    UInt                     sIndexID;
    qcmTableInfo           * sInfo;
    UInt                     i;

    sParseTree = (qdDropParseTree *)aStatement->myPlan->parseTree;

    IDE_TEST(qcm::checkIndexByUser( aStatement,
                                    sParseTree->userName,
                                    sParseTree->objectName,
                                    &(sParseTree->userID),
                                    &sTableID,
                                    &sIndexID)
             != IDE_SUCCESS);

    /* BUG-48290 shard object�� ���� DDL ���� */
    IDE_TEST( sdi::checkShardObjectForDDL( aStatement, SDI_DDL_TYPE_DROP ) != IDE_SUCCESS );

    IDE_TEST(qcm::getTableInfoByID(aStatement,
                                   sTableID,
                                   &(sParseTree->tableInfo),
                                   &(sParseTree->tableSCN),
                                   &(sParseTree->tableHandle))
             != IDE_SUCCESS);

    IDE_TEST( qcm::lockTableForDDLValidation(aStatement,
                                             sParseTree->tableHandle,
                                             sParseTree->tableSCN)
              != IDE_SUCCESS );

    sInfo = sParseTree->tableInfo;
    
    // check grant
    IDE_TEST( qdpRole::checkDDLDropIndexPriv( aStatement,
                                              sInfo,
                                              sParseTree->userID )
              != IDE_SUCCESS );
    
    // check primary key or unique key
    if (sInfo->primaryKey != NULL)
    {
        IDE_TEST_RAISE(sInfo->primaryKey->indexId == sIndexID,
                       ERR_CANNOT_DROP_SYSTEM_INDEX);
    }

    if (sInfo->uniqueKeyCount > 0)
    {
        for (i = 0; i < sInfo->uniqueKeyCount; i++)
        {
            IDE_TEST_RAISE(sInfo->uniqueKeys[i].constraintIndex->indexId
                           == sIndexID, ERR_CANNOT_DROP_SYSTEM_INDEX);
        }
    }

    IDE_TEST_RAISE(sInfo->tableOwnerID == QC_SYSTEM_USER_ID,
                   ERR_NOT_ALTER_META);

    // PROJ-1502 PARTITIONED DISK TABLE
    if( sInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
    {
        // ��� ��Ƽ�ǿ� LOCK(IS)
        // ��Ƽ�� ����Ʈ�� �Ľ�Ʈ���� �޾Ƴ��´�.
        IDE_TEST( qdbCommon::checkAndSetAllPartitionInfo(
                      aStatement,
                      sInfo->tableID,
                      & (sParseTree->partInfoList) )
                  != IDE_SUCCESS );

        // PROJ-1624 non-partitioned index
        IDE_TEST( qdx::makeAndLockIndexTableList(
                      aStatement,
                      ID_FALSE,
                      sInfo,
                      &(sParseTree->oldIndexTables) )
                  != IDE_SUCCESS );
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

    IDE_EXCEPTION(ERR_CANNOT_DROP_SYSTEM_INDEX);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDD_DROP_SYSTEM_INDEX));
    }
    IDE_EXCEPTION(ERR_NOT_ALTER_META);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDD_NO_DROP_META_TABLE));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qdd::validateDropUser(qcStatement * aStatement)
{
/***********************************************************************
 *
 * Description :
 *    DROP USER ... �� validation ����
 *
 * Implementation :
 *    1. DropUser ������ �ִ��� üũ
 *
 ***********************************************************************/

#define IDE_FN "qdd::validateDropUser"

    // check grant
    IDE_TEST( qdpRole::checkDDLDropUserPriv( aStatement ) != IDE_SUCCESS );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qdd::validateDropSequence(qcStatement * aStatement)
{
/***********************************************************************
 *
 * Description :
 *    DROP SEQUENCE ... �� validation ����
 *
 * Implementation :
 *    1. SEQUENCE ���� ���� üũ
 *    2. DropSequence ������ �ִ��� üũ
 *
 ***********************************************************************/

#define IDE_FN "qdd::validateDropSequence"
    qdDropParseTree    * sParseTree;
    qcmSequenceInfo      sSequenceInfo;
    void               * sSequenceHandle;
    
    sParseTree = (qdDropParseTree *)aStatement->myPlan->parseTree;

    // if specified sequence doesn't exist, then error
    IDE_TEST(qds::checkSequenceExist( aStatement,
                                      sParseTree->userName,
                                      sParseTree->objectName,
                                      &(sParseTree->userID),
                                      &sSequenceInfo,
                                      &sSequenceHandle)
             != IDE_SUCCESS);

    if ( QCG_GET_SESSION_USER_ID(aStatement) != QC_SYSTEM_USER_ID )
    {
        // sParseTree->userID is a owner of table
        IDE_TEST_RAISE(sParseTree->userID == QC_SYSTEM_USER_ID,
                       ERR_NOT_DROP_META);
    }
    else
    {
        if ( ( aStatement->session->mQPSpecific.mFlag
               & QC_SESSION_ALTER_META_MASK )
             == QC_SESSION_ALTER_META_DISABLE )
        {
            IDE_RAISE(ERR_NOT_DROP_META);
        }
    }

    // check grant
    IDE_TEST( qdpRole::checkDDLDropSequencePriv( aStatement,
                                                 sParseTree->userID )
              != IDE_SUCCESS );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_NOT_DROP_META);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDD_NO_DROP_META_TABLE));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qdd::validateTruncateTable(qcStatement * aStatement)
{
/***********************************************************************
 *
 * Description :
 *    TRUNCATE TABLE ... �� validation ����
 *
 * Implementation :
 *    1. ���̺� ���� ���� üũ
 *    2. ��Ÿ ���̺��̸� ����
 *    3. view �̸� ����
 *    4. AlterTable ������ �ִ��� üũ
 *    5. ���̺� Replication�� �ɷ�������, Recovery ���θ� Ȯ��
 *    6. ���̺� uniquekey �� �ְ�, �� Ű�� �����ϴ� child �� ������ ����
 *
 ***********************************************************************/

#define IDE_FN "qdd::validateTruncateTable"

    qdDropParseTree      * sParseTree;
    qcmTableInfo         * sInfo;
    qcmIndex             * sIndexInfo;
    UInt                   i;
    qcmRefChildInfo      * sChildInfo = NULL;  // BUG-28049
    qcuSqlSourceInfo       sqlInfo;
    UInt                   sColumnCount = 0;
    
    sParseTree = (qdDropParseTree *)aStatement->myPlan->parseTree;
    
    /* BUG-30059 */
    if ( qdbCommon::containDollarInName( &(sParseTree->objectName) ) == ID_TRUE )
    {
        sqlInfo.setSourceInfo(
            aStatement,
            &(sParseTree->objectName) );

        IDE_RAISE( CANT_USE_RESERVED_WORD );
    }

    // check table exist.
    IDE_TEST( qdbCommon::checkTableInfo( aStatement,
                                         sParseTree->userName,
                                         sParseTree->objectName,
                                         &(sParseTree->userID),
                                         &(sParseTree->tableInfo),
                                         &(sParseTree->tableHandle),
                                         &(sParseTree->tableSCN) )
              != IDE_SUCCESS );

    /* BUG-48290 shard object�� ���� DDL ���� */
    IDE_TEST( sdi::checkShardObjectForDDL( aStatement, SDI_DDL_TYPE_DROP ) != IDE_SUCCESS );
    
    IDE_TEST( qcm::lockTableForDDLValidation(aStatement,
                                             sParseTree->tableHandle,
                                             sParseTree->tableSCN)
              != IDE_SUCCESS );

    sInfo = sParseTree->tableInfo;

    // PROJ-1407 temporary table
    if ( qcuTemporaryObj::isTemporaryTable( sInfo ) == ID_TRUE )
    {
        sParseTree->common.execute = qdd::executeTruncateTempTable;
        
        sParseTree->tableID        = sInfo->tableID;
        sParseTree->temporaryType  = sInfo->temporaryInfo.type;
    }
    else
    {
        /* PROJ-1723 [MDW/INTEGRATOR] Altibase Plugin ����
           DDL Statement Text�� �α�
        */
        if (QCU_DDL_SUPPLEMENTAL_LOG == 1)
        {
            IDE_TEST( qciMisc::writeDDLStmtTextLog( aStatement,
                                                    sInfo->tableOwnerID,
                                                    sInfo->name )
                      != IDE_SUCCESS );
        }
        else
        {
            /* Nothing to do */
        }

        if ( QCG_GET_SESSION_USER_ID(aStatement) != QC_SYSTEM_USER_ID )
        {
            // sParseTree->userID is a owner of table
            IDE_TEST_RAISE(sParseTree->userID == QC_SYSTEM_USER_ID,
                           ERR_NOT_DROP_META);
        }
        else
        {
            if ( ( aStatement->session->mQPSpecific.mFlag & QC_SESSION_ALTER_META_MASK )
                 == QC_SESSION_ALTER_META_DISABLE )
            {
                if ( qrc::isInternalDDL( aStatement ) != ID_TRUE )
                {
                    IDE_RAISE(ERR_NOT_DROP_META);
                }
            }
            else
            {
                /* Nothing to do */
            }
        }

        // PR-13725
        // CHECK OPERATABLE
        if ( QCM_IS_OPERATABLE_QP_TRUNC_TABLE( sInfo->operatableFlag ) != ID_TRUE )
        {
            sqlInfo.setSourceInfo( aStatement, &(sParseTree->objectName) );
            IDE_RAISE(ERR_NOT_EXIST_TABLE);
        }
        else
        {
            /* Nothing to do */
        }
        
        // check grant
        IDE_TEST( qdpRole::checkDDLAlterTablePriv( aStatement,
                                                   sInfo )
                  != IDE_SUCCESS );

        /* PROJ-1442 Replication Online �� DDL ���
         * Recovery ��ɰ� ���� ����� �� ����
         */
        if (sInfo->replicationCount > 0)
        {
            IDE_TEST_RAISE(sInfo->replicationRecoveryCount > 0,
                           ERR_CANNOT_DDL_WITH_RECOVERY);
        }
        else
        {
            /* Nothing to do */
        }

        // check referential constraint
        for (i = 0; i < sInfo->uniqueKeyCount; i++)
        {
            sIndexInfo = sInfo->uniqueKeys[i].constraintIndex;

            IDE_TEST(qcm::getChildKeys(aStatement,
                                       sIndexInfo,
                                       sInfo,
                                       &sChildInfo)
                     != IDE_SUCCESS);

            // BUG-28049
            while (sChildInfo != NULL) // foreign key exists.
            {
                // not self
                IDE_TEST_RAISE( sChildInfo->childTableRef->tableInfo != sInfo,
                                ERR_ABORT_REFERENTIAL_CONSTRAINT_EXIST );

                sChildInfo = sChildInfo->next;
            }
        }

        // PROJ-1502 PARTITIONED DISK TABLE
        if ( sInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
        {
            // ��� ��Ƽ�ǿ� LOCK(IS)
            // ��Ƽ�� ����Ʈ�� �Ľ�Ʈ���� �޾Ƴ��´�.
            IDE_TEST( qdbCommon::checkAndSetAllPartitionInfo(
                          aStatement,
                          sInfo->tableID,
                          & (sParseTree->partInfoList) )
                      != IDE_SUCCESS );

            // PROJ-1624 non-partitioned index
            IDE_TEST( qdx::makeAndLockIndexTableList(
                          aStatement,
                          ID_FALSE,
                          sInfo,
                          &(sParseTree->oldIndexTables) )
                      != IDE_SUCCESS );
        }
        else
        {
            // Nothing to do.
        }
    }

    if ( sInfo->replicationCount > 0 )
    {
        sColumnCount = sInfo->columnCount;

        for ( i = 0 ; i < sColumnCount ; i++ )
        {
            if ( ( sInfo->columns[i].basicInfo->column.flag & SMI_COLUMN_COMPRESSION_MASK )
                    == SMI_COLUMN_COMPRESSION_TRUE )
            {
                IDE_RAISE( ERR_TRUNCATE_COMPRESSED_TABLE );
            }
            else
            {
                /* Nothing to do */
            }
        }
    }
    else
    {
        // Nothing to do.
    }

    if ( ( sInfo->replicationCount > 0 ) ||
         ( QCG_GET_SESSION_IS_NEED_DDL_INFO( aStatement ) == ID_TRUE ) )
    {    
        qrc::setDDLSrcInfo( aStatement,
                            ID_TRUE,
                            1,
                            &(sParseTree->tableInfo->tableOID),
                            0,
                            NULL );
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_NOT_DROP_META);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDD_NO_DROP_META_TABLE));
    }
    IDE_EXCEPTION(ERR_NOT_EXIST_TABLE)
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QCV_NOT_EXISTS_TABLE,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION(ERR_CANNOT_DDL_WITH_RECOVERY)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDB_CANNOT_DDL_WITH_RECOVERY));
    }
    IDE_EXCEPTION(ERR_ABORT_REFERENTIAL_CONSTRAINT_EXIST);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDB_REFERENTIAL_CONSTRAINT_EXIST));
    }
    IDE_EXCEPTION( CANT_USE_RESERVED_WORD );
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QDB_RESERVED_WORD_IN_OBJECT_NAME,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION(ERR_TRUNCATE_COMPRESSED_TABLE)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDB_CANNOT_TRUNCATE_COMPRESSED_TABLE));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qdd::validateDropView(qcStatement * aStatement)
{
/***********************************************************************
 *
 * Description :
 *    DROP VIEW ... �� validation ����
 *
 * Implementation :
 *    1. VIEW ���� ���� üũ
 *    2. tableType �� view �� �ƴϸ� ����
 *    3. ��Ÿ ���̺��̸� ����
 *    4. DropView ������ �ִ��� üũ
 *
 ***********************************************************************/

#define IDE_FN "qdd::validateDropView"

    qdDropParseTree    * sParseTree;
    qcmTableInfo       * sInfo;
    qcuSqlSourceInfo     sqlInfo;
    
    sParseTree = (qdDropParseTree *)aStatement->myPlan->parseTree;
    
    /* BUG-30059 */
    if ( qdbCommon::containDollarInName( &(sParseTree->objectName) ) == ID_TRUE )
    {
        sqlInfo.setSourceInfo(
            aStatement,
            &(sParseTree->objectName) );

        IDE_RAISE( CANT_USE_RESERVED_WORD );
    }

    // validation of view name
    IDE_TEST( qdbCommon::checkTableInfo( aStatement,
                                         sParseTree->userName,
                                         sParseTree->objectName,
                                         &(sParseTree->userID),
                                         &(sParseTree->tableInfo),
                                         &(sParseTree->tableHandle),
                                         &(sParseTree->tableSCN))
              != IDE_SUCCESS);

    IDE_TEST( qcm::lockTableForDDLValidation(aStatement,
                                             sParseTree->tableHandle,
                                             sParseTree->tableSCN)
              != IDE_SUCCESS );

    sInfo = sParseTree->tableInfo;

    if ( QCG_GET_SESSION_USER_ID(aStatement) != QC_SYSTEM_USER_ID )
    {
        // sParseTree->userID is a owner of table
        IDE_TEST_RAISE(sParseTree->userID == QC_SYSTEM_USER_ID,
                       ERR_NOT_DROP_META);
    }
    else
    {
        if ( ( aStatement->session->mQPSpecific.mFlag
               & QC_SESSION_ALTER_META_MASK )
             == QC_SESSION_ALTER_META_DISABLE )
        {
            IDE_RAISE(ERR_NOT_DROP_META);
        }
    }

    // PR-13725
    // CHECK OPERATABLE
    if( QCM_IS_OPERATABLE_QP_DROP_VIEW( sInfo->operatableFlag ) != ID_TRUE )
    {
        sqlInfo.setSourceInfo(aStatement, &sParseTree->objectName);
        IDE_RAISE(ERR_NOT_EXIST_VIEW);
    }
    
    // check grant
    IDE_TEST( qdpRole::checkDDLDropViewPriv( aStatement,
                                             sParseTree->userID )
              != IDE_SUCCESS );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_NOT_EXIST_VIEW)
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCV_NOT_EXISTS_TABLE,
                                sqlInfo.getErrMessage()));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION(ERR_NOT_DROP_META);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDD_NO_DROP_META_TABLE));
    }
    IDE_EXCEPTION( CANT_USE_RESERVED_WORD );
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QDB_RESERVED_WORD_IN_OBJECT_NAME,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

/***********************************************************************
 *
 * Description :
 *    DROP MATERIALIZED VIEW ... �� validation ����
 *
 * Implementation :
 *    1. Meta Table���� Ȯ��
 *    2. Materialized View, Table, View ���� ���� Ȯ��
 *    3. Table�� MView Table����, View�� MView View���� Ȯ��
 *    4. Materialized View ���� ������ �ִ��� Ȯ��
 *    5. Replication ���� ���� Ȯ��
 *    6. Table�� unique key�� �ְ�, �װ��� �����ϴ� child�� �ִ��� Ȯ��
 *
 ***********************************************************************/
IDE_RC qdd::validateDropMView( qcStatement * aStatement )
{
    qdDropParseTree  * sParseTree = NULL;
    qcmTableInfo     * sTableInfo = NULL;
    UInt               i;
    qcmIndex         * sIndexInfo = NULL;
    qcmRefChildInfo  * sChildInfo = NULL; /* BUG-28049 */
    qcuSqlSourceInfo   sqlInfo;
    UInt               sMViewID;
    qcmTableInfo     * sViewInfo  = NULL;
    
    sParseTree = (qdDropParseTree *)aStatement->myPlan->parseTree;

    /* BUG-30059 */
    if ( qdbCommon::containDollarInName( &(sParseTree->objectName) ) == ID_TRUE )
    {
        sqlInfo.setSourceInfo( aStatement, &(sParseTree->objectName) );
        IDE_RAISE( ERR_RESERVED_WORD_IN_OBJECT_NAME );
    }
    else
    {
        /* Nothing to do */
    }

    QC_STR_COPY( sParseTree->mviewViewName, sParseTree->objectName );
    (void)idlVA::appendString( sParseTree->mviewViewName,
                               QC_MAX_OBJECT_NAME_LEN + 1,
                               QDM_MVIEW_VIEW_SUFFIX,
                               QDM_MVIEW_VIEW_SUFFIX_SIZE );

    /* check user existence */
    if ( QC_IS_NULL_NAME( sParseTree->userName ) == ID_TRUE )
    {
        sParseTree->userID = QCG_GET_SESSION_USER_ID( aStatement );
    }
    else
    {
        IDE_TEST( qcmUser::getUserID( aStatement,
                                      sParseTree->userName,
                                      &(sParseTree->userID) )
                  != IDE_SUCCESS );
    }

    if ( QCG_GET_SESSION_USER_ID( aStatement ) != QC_SYSTEM_USER_ID )
    {
        /* sParseTree->userID is a owner of materialized view */
        IDE_TEST_RAISE( sParseTree->userID == QC_SYSTEM_USER_ID,
                        ERR_NOT_DROP_META );
    }
    else
    {
        IDE_TEST_RAISE( (aStatement->session->mQPSpecific.mFlag &
                           QC_SESSION_ALTER_META_MASK)
                        == QC_SESSION_ALTER_META_DISABLE,
                        ERR_NOT_DROP_META );
    }

    /* check materialized view existence */
    IDE_TEST( qcmMView::getMViewID(
                        QC_SMI_STMT( aStatement ),
                        sParseTree->userID,
                        (sParseTree->objectName.stmtText +
                         sParseTree->objectName.offset),
                        (UInt)sParseTree->objectName.size,
                        &sMViewID )
              != IDE_SUCCESS );

    /* check the table exists */
    IDE_TEST( qcm::getTableHandleByName(
                    QC_SMI_STMT( aStatement ),
                    sParseTree->userID,
                    (UChar *)(sParseTree->objectName.stmtText +
                              sParseTree->objectName.offset),
                    sParseTree->objectName.size,
                    &(sParseTree->tableHandle),
                    &(sParseTree->tableSCN) )
              != IDE_SUCCESS );

    IDE_TEST( qcm::lockTableForDDLValidation(
                    aStatement,
                    sParseTree->tableHandle,
                    sParseTree->tableSCN )
              != IDE_SUCCESS );

    IDE_TEST( smiGetTableTempInfo( sParseTree->tableHandle,
                                   (void**)&sTableInfo )
              != IDE_SUCCESS );
    sParseTree->tableInfo = sTableInfo;

    /* check the view exists */
    IDE_TEST( qcm::getTableHandleByName(
                    QC_SMI_STMT( aStatement ),
                    sParseTree->userID,
                    (UChar *)sParseTree->mviewViewName,
                    (SInt)idlOS::strlen( sParseTree->mviewViewName ),
                    &(sParseTree->mviewViewHandle),
                    &(sParseTree->mviewViewSCN) )
              != IDE_SUCCESS );

    IDE_TEST( qcm::lockTableForDDLValidation(
                    aStatement,
                    sParseTree->mviewViewHandle,
                    sParseTree->mviewViewSCN )
              != IDE_SUCCESS );

    IDE_TEST( smiGetTableTempInfo( sParseTree->mviewViewHandle,
                                   (void**)&sViewInfo )
              != IDE_SUCCESS );
    sParseTree->mviewViewInfo = sViewInfo;

    /* check table type */
    if ( (sTableInfo->tableType != QCM_MVIEW_TABLE) ||
         (sViewInfo->tableType != QCM_MVIEW_VIEW) )
    {
        sqlInfo.setSourceInfo( aStatement, &(sParseTree->objectName) );
        IDE_RAISE( ERR_NOT_EXIST_MVIEW );
    }
    else
    {
        /* Nothing to do */
    }

    /* check grant */
    IDE_TEST( qdpRole::checkDDLDropMViewPriv( aStatement,
                                              sParseTree->userID )
              != IDE_SUCCESS );

    /* if specified tables is replicated, the ERR_DDL_WITH_REPLICATED_TABLE */
    IDE_TEST_RAISE( sTableInfo->replicationCount > 0,
                    ERR_DDL_WITH_REPLICATED_TABLE );
    /* PROJ-1608 replicationCount�� 0�� ��, recovery count�� �׻� 0�̾�� �� */
    IDE_DASSERT( sTableInfo->replicationRecoveryCount == 0 );

    /* CASCADE �ɼ��� �������� �����Ƿ�, check referential constraint */
    for ( i = 0; i < sTableInfo->uniqueKeyCount; i++ )
    {
        sIndexInfo = sTableInfo->uniqueKeys[i].constraintIndex;

        IDE_TEST( qcm::getChildKeys( aStatement,
                                     sIndexInfo,
                                     sTableInfo,
                                     &sChildInfo )
                  != IDE_SUCCESS);

        /* BUG-28049 */
        while ( sChildInfo != NULL ) /* if foreign key exists. */
        {
            /* if not self, error */
            IDE_TEST_RAISE( sChildInfo->childTableRef->tableInfo != sTableInfo,
                            ERR_ABORT_REFERENTIAL_CONSTRAINT_EXIST );

            sChildInfo = sChildInfo->next;
        }
    }

    /* PROJ-1502 PARTITIONED DISK TABLE */
    if ( sTableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
    {
        /* ��� ��Ƽ�ǿ� LOCK(IS)
         * ��Ƽ�� ����Ʈ�� �Ľ�Ʈ���� �޾Ƴ��´�.
         */
        IDE_TEST( qdbCommon::checkAndSetAllPartitionInfo(
                      aStatement,
                      sTableInfo->tableID,
                      &(sParseTree->partInfoList) )
                  != IDE_SUCCESS );

        // PROJ-1624 non-partitioned index
        IDE_TEST( qdx::makeAndLockIndexTableList(
                      aStatement,
                      ID_FALSE,
                      sTableInfo,
                      &(sParseTree->oldIndexTables) )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NOT_DROP_META );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDD_NO_DROP_META_TABLE ) );
    }
    IDE_EXCEPTION( ERR_ABORT_REFERENTIAL_CONSTRAINT_EXIST );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDB_REFERENTIAL_CONSTRAINT_EXIST ) );
    }
    IDE_EXCEPTION( ERR_DDL_WITH_REPLICATED_TABLE )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDD_DDL_WITH_REPLICATED_TBL ) );
    }
    IDE_EXCEPTION( ERR_NOT_EXIST_MVIEW )
    {
        (void)sqlInfo.init( aStatement->qmeMem );
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDD_NOT_EXIST_MVIEW,
                                  sqlInfo.getErrMessage() ) );
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION( ERR_RESERVED_WORD_IN_OBJECT_NAME );
    {
        (void)sqlInfo.init( aStatement->qmeMem );
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDB_RESERVED_WORD_IN_OBJECT_NAME,
                                  sqlInfo.getErrMessage() ) );
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 * EXECUTE
 **********************************************************************/

IDE_RC qdd::executeDropTable(qcStatement * aStatement)
{
/***********************************************************************
 *
 * Description :
 *    DROP TABLE ... �� execution ����
 *
 * Implementation :
 *    1. get table info
 *    2. ��Ÿ ���̺��� constraint ����
 *    3. ��Ÿ ���̺��� index ����
 *    4. ��Ÿ ���̺��� table,column ����
 *    5. ��Ÿ ���̺��� privilege ����
 *    -. ��Ÿ ���̺��� trigger ����
 *    6. related PSM �� invalid ���·� ����
 *    7. related VIEW �� invalid ���·� ����
 *    8. Constraint�� ���õ� Procedure�� ���� ������ ����
 *    9. Index�� ���õ� Procedure�� ���� ������ ����
 *   10. smiTable::dropTable
 *   11. ��Ÿ ĳ������ ����
 *
 ***********************************************************************/

#define IDE_FN "qdd::executeDropTable"

    qdDropParseTree         * sParseTree;
    qcmTableInfo            * sInfo            = NULL;
    qcmIndex                * sIndexInfo;
    qcmRefChildInfo         * sChildInfo;  // BUG-28049
    qcmPartitionInfoList    * sPartInfoList = NULL;
    qcmPartitionInfoList    * sOldPartInfoList = NULL;
    qcmPartitionInfoList    * sNewPartInfoList = NULL;
    qcmTableInfo            * sPartInfo;
    idBool                    sIsPartitioned = ID_FALSE;
    idBool                    sUseRecyclebin = ID_FALSE;
    qdIndexTableList        * sIndexTable;
    qcmColumn               * sColumn;
    qcmTableInfo            * sDicInfo;
    UInt                      sTableID      = 0;
    smOID                     sTableOID     = 0;
    SInt                      sIdx          = 0;
    qcmTableInfo            * sNewTableInfo = NULL;
    smSCN                     sSCN;
    void                    * sTableHandle;
    UInt                      i = 0;

    sParseTree = (qdDropParseTree *)aStatement->myPlan->parseTree;

    // TASK-2176
    // Table�� ���� Lock�� ȹ���Ѵ�.
    IDE_TEST( qcm::validateAndLockTable(aStatement,
                                        sParseTree->tableHandle,
                                        sParseTree->tableSCN,
                                        SMI_TABLE_LOCK_X)
              != IDE_SUCCESS );

    sInfo = sParseTree->tableInfo;

    /* PROJ-2441 flashback */
    sTableID = sInfo->tableID;
    sTableOID = smiGetTableId( sInfo->tableHandle );

    /* recylebin ��� ���� */
    sUseRecyclebin = sParseTree->useRecycleBin;

    // PROJ-1502 PARTITIONED DISK TABLE
    if ( sInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
    {
        sIsPartitioned = ID_TRUE;

        // ��� ��Ƽ�ǿ� LOCK(X)
        IDE_TEST( qcmPartition::validateAndLockPartitionInfoList( aStatement,
                                                                  sParseTree->partInfoList,
                                                                  SMI_TBSLV_DDL_DML, // TBS Validation �ɼ�
                                                                  SMI_TABLE_LOCK_X,
                                                                  smiGetDDLLockTimeOut((QC_SMI_STMT(aStatement))->getTrans()))
                  != IDE_SUCCESS );

        // ���� ó���� ���Ͽ�, Lock�� ���� �Ŀ� Partition List�� �����Ѵ�.
        sOldPartInfoList = sParseTree->partInfoList;

        // PROJ-1624 non-partitioned index
        IDE_TEST( qdx::validateAndLockIndexTableList( aStatement,
                                                      sParseTree->oldIndexTables,
                                                      SMI_TBSLV_DDL_DML, // TBS Validation �ɼ�
                                                      SMI_TABLE_LOCK_X,
                                                      smiGetDDLLockTimeOut((QC_SMI_STMT(aStatement))->getTrans()))
                  != IDE_SUCCESS );
    }
    else
    {
        sIsPartitioned = ID_FALSE;
    }

    // PROJ-1407 Temporary table
    // session temporary table�� �����ϴ� ��� DDL�� �� �� ����.
    IDE_TEST_RAISE( qcuTemporaryObj::existSessionTable( sInfo ) == ID_TRUE,
                    ERR_SESSION_TEMPORARY_TABLE_EXIST );

    /* PROJ-1723 [MDW/INTEGRATOR] Altibase Plugin ����
       DDL Statement Text�� �α�
    */
    if (QCU_DDL_SUPPLEMENTAL_LOG == 1)
    {
        IDE_TEST( qciMisc::writeDDLStmtTextLog( aStatement,
                                                sInfo->tableOwnerID,
                                                sInfo->name )
                  != IDE_SUCCESS );

        IDE_TEST(qci::mManageReplicationCallback.mWriteTableMetaLog(
                                                aStatement,
                                                smiGetTableId(sInfo->tableHandle),
                                                0)
                 != IDE_SUCCESS);
    }

    // PROJ-1509, BUG-14168
    if ( ( sParseTree->flag & QDD_DROP_CASCADE_CONSTRAINTS_MASK )
        == QDD_DROP_CASCADE_CONSTRAINTS_TRUE )
    {
        // primary or uniquey key�� ������
        // �ٸ� ���̺��� referential integrity constraint ����
        for( i = 0; i < sInfo->uniqueKeyCount; i++ )
        {
            sIndexInfo = sInfo->uniqueKeys[i].constraintIndex;

            IDE_TEST( qcm::getChildKeysForDelete( aStatement,
                                                  QC_EMPTY_USER_ID,
                                                  sIndexInfo,
                                                  sInfo,
                                                  ID_FALSE /* aDropTablespace */,
                                                  & sChildInfo )
                      != IDE_SUCCESS );

            IDE_TEST( deleteFKConstraints( aStatement,
                                           sInfo,
                                           sChildInfo,
                                           ID_FALSE /* aDropTablespace */ )
                      != IDE_SUCCESS );
        } // for
    } // QDD_DROP_CONSTRAINTS_TRUE
    else
    {
        // QDD_DROP_CONSTRAINTS_FALSE
        // Nothing To Do
    }

    // PROJ-1502 PARTITIONED DISK TABLE
    if ( sIsPartitioned == ID_TRUE )
    {
        // -----------------------------------------------------
        // ���̺� ��Ƽ�� ������ŭ �ݺ�
        // -----------------------------------------------------
        for ( sPartInfoList = sOldPartInfoList;
              sPartInfoList != NULL;
              sPartInfoList = sPartInfoList->next )
        {
            sPartInfo = sPartInfoList->partitionInfo;

            IDE_TEST( dropTablePartition( aStatement,
                                          sPartInfo,
                                          ID_FALSE, /* aIsDropTablespace */
                                          sUseRecyclebin )
                      != IDE_SUCCESS );
        }

        // PROJ-1624 non-partitioned index
        for ( sIndexTable = sParseTree->oldIndexTables;
              sIndexTable != NULL;
              sIndexTable = sIndexTable->next )
        {
            IDE_TEST( qdx::dropIndexTable( aStatement,
                                           sIndexTable,
                                           ID_FALSE /* aIsDropTablespace */ )
                      != IDE_SUCCESS );
        }
    }
    else
    {
        // Nothing to do
    }

    // DELETE from qcm_constraints_ where table id = aTableID;
    IDE_TEST(qdd::deleteConstraintsFromMeta(aStatement, sInfo->tableID)
             != IDE_SUCCESS);

    IDE_TEST(qdd::deleteIndicesFromMeta(aStatement,
                                        sInfo)
             != IDE_SUCCESS);
    
    /* PROJ-2441 flashback */
    if ( sUseRecyclebin == ID_FALSE )
    {
        IDE_TEST(qdd::deleteTableFromMeta(aStatement, sInfo->tableID)
                 != IDE_SUCCESS);
    }
    else
    {
        IDE_TEST( qdbFlashback::dropToRecyclebin( aStatement,
                                                  sInfo )
                  != IDE_SUCCESS );
    }

    IDE_TEST(qdpDrop::removePriv4DropTable(aStatement, sInfo->tableID)
             != IDE_SUCCESS);

    // PROJ-1359
    IDE_TEST(qdnTrigger::dropTrigger4DropTable(aStatement, sInfo )
             != IDE_SUCCESS);

    /* PROJ-2197 PSM Renewal */
    // related PSM
    IDE_TEST(qcmProc::relSetInvalidProcOfRelated(
                aStatement,
                sParseTree->userID,
                (SChar *) (sParseTree->objectName.stmtText + sParseTree->objectName.offset),
                sParseTree->objectName.size,
                QS_TABLE) != IDE_SUCCESS);

    // PROJ-1073 Package
    IDE_TEST(qcmPkg::relSetInvalidPkgOfRelated(
                aStatement,
                sParseTree->userID,
                (SChar *) (sParseTree->objectName.stmtText + sParseTree->objectName.offset),
                sParseTree->objectName.size,
                QS_TABLE) != IDE_SUCCESS);

    // related VIEW
    IDE_TEST(qcmView::setInvalidViewOfRelated(
                 aStatement,
                 sParseTree->userID,
                 (SChar *) (sParseTree->objectName.stmtText + sParseTree->objectName.offset),
                 sParseTree->objectName.size,
                 QS_TABLE) != IDE_SUCCESS);

    /* BUG-35445 Check Constraint, Function-Based Index���� ��� ���� Function�� ����/���� ���� */
    IDE_TEST( qcmProc::relRemoveRelatedToConstraintByTableID(
                    aStatement,
                    sInfo->tableID )
              != IDE_SUCCESS );

    IDE_TEST( qcmProc::relRemoveRelatedToIndexByTableID(
                    aStatement,
                    sInfo->tableID )
              != IDE_SUCCESS );

    // BUG-21387 COMMENT
    IDE_TEST(qdbComment::deleteCommentTable( aStatement,
                                             sParseTree->tableInfo->tableOwnerName,
                                             sParseTree->tableInfo->name )
             != IDE_SUCCESS);

    // PROJ-2223 audit
    IDE_TEST( qcmAudit::deleteObject( aStatement,
                                      sParseTree->tableInfo->tableOwnerID,
                                      sParseTree->tableInfo->name )
              != IDE_SUCCESS );

    /* PROJ-2441 flashback */
    if ( sUseRecyclebin == ID_FALSE )
    {
        IDE_TEST( smiTable::dropTable( QC_SMI_STMT( aStatement ),
                                       sInfo->tableHandle,
                                       SMI_TBSLV_DDL_DML )
                  != IDE_SUCCESS);
    }
    else
    {
        /* PROJ-2441 flashback : Table index drop */
        for ( sIdx = sInfo->indexCount - 1; sIdx >= 0; sIdx-- )
        {
            sIndexInfo = &sInfo->indices[sIdx];
            
            IDE_TEST( smiTable::dropIndex(
                              QC_SMI_STMT(aStatement),
                              sInfo->tableHandle,
                              smiGetTableIndexByID( sInfo->tableHandle,
                                                    sIndexInfo->indexId ),
                              SMI_TBSLV_DDL_DML )
                       != IDE_SUCCESS );
        }
    }

    /* PROJ-2441 flashback */
    if ( sUseRecyclebin == ID_FALSE )
    {
        // PROJ-2264 Dictionary table
        // Drop all dictionary tables
        for( sColumn  = sInfo->columns;
             sColumn != NULL;
             sColumn  = sColumn->next )
        {
            if ( (sColumn->basicInfo->column.flag & SMI_COLUMN_COMPRESSION_MASK)
                == SMI_COLUMN_COMPRESSION_TRUE )
            {
                IDE_TEST( qcmDictionary::removeDictionaryTable( aStatement,
                                                                sColumn )
                          != IDE_SUCCESS );
            }
            else
            {
                // Not compression column
                // Nothing to do.
            }
        }

        // To Fix BUG-12034
        // SM�� ���̻� ������ ������ ���� ���� QP���� �����ϰ� �ִ�
        // �޸𸮵��� �������ش�.
        IDE_TEST( qdnTrigger::freeTriggerCaches4DropTable( sInfo )
                  != IDE_SUCCESS );
    
        // BUG-36719
        // Table info �� �����Ѵ�.
        for( sColumn  = sInfo->columns;
             sColumn != NULL;
             sColumn  = sColumn->next )
        {
            if ( (sColumn->basicInfo->column.flag & SMI_COLUMN_COMPRESSION_MASK)
                == SMI_COLUMN_COMPRESSION_TRUE )
            {
                sDicInfo = (qcmTableInfo *)smiGetTableRuntimeInfoFromTableOID(
                    sColumn->basicInfo->column.mDictionaryTableOID );

                (void)qcm::destroyQcmTableInfo(sDicInfo);
            }
            else
            {
                // Not compression column
                // Nothing to do.
            }
        }
        
        // PROJ-2002 Column Security
        // ���� �÷� ������ ���� ��⿡ �˸���.
        for ( i = 0; i < sInfo->columnCount; i++ )
        {
            sColumn = & sInfo->columns[i];

            if ( (sColumn->basicInfo->module->flag & MTD_ENCRYPT_TYPE_MASK)
                 == MTD_ENCRYPT_TYPE_TRUE )
            {
                (void) qcsModule::unsetColumnPolicy(
                    sInfo->tableOwnerName,
                    sInfo->name,
                    sColumn->name );
            }
            else
            {
                // Nothing to do.
            }
        }
    }
    else
    {
        IDE_TEST( qcm::touchTable( QC_SMI_STMT( aStatement ),
                                   sTableID,
                                   SMI_TBSLV_DDL_DML )
                  != IDE_SUCCESS );
        
        IDE_TEST( qcm::makeAndSetQcmTableInfo(
                      QC_SMI_STMT( aStatement ), sTableID, sTableOID )
                  != IDE_SUCCESS );

        IDE_TEST(qcm::getTableInfoByID(aStatement,
                                       sTableID,
                                       &sNewTableInfo,
                                       &sSCN,
                                       &sTableHandle)
                 != IDE_SUCCESS);
        
        if ( sIsPartitioned == ID_TRUE )
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
        
        // To Fix BUG-12034
        // SM�� ���̻� ������ ������ ���� ���� QP���� �����ϰ� �ִ�
        // �޸𸮵��� �������ش�.
        IDE_TEST( qdnTrigger::freeTriggerCaches4DropTable( sInfo )
                  != IDE_SUCCESS );
    }
 
    if ( QCG_GET_SESSION_IS_NEED_DDL_INFO( aStatement ) == ID_TRUE ) 
    {
        qrc::setDDLDestInfo( aStatement, 
                             0,
                             NULL,
                             0,
                             NULL );
    }
   
    (void)qcm::destroyQcmTableInfo(sInfo);

    if ( sIsPartitioned == ID_TRUE )
    {
        (void)qcmPartition::destroyQcmPartitionInfoList( sOldPartInfoList );
        
        for ( sIndexTable = sParseTree->oldIndexTables;
              sIndexTable != NULL;
              sIndexTable = sIndexTable->next )
        {
            (void)qcm::destroyQcmTableInfo(sIndexTable->tableInfo);
        }
    }
    else
    {
        /* Nothing to do */
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_SESSION_TEMPORARY_TABLE_EXIST )
    {
        IDE_SET(ideSetErrorCode( qpERR_ABORT_QDB_TEMPORARY_TABLE_DDL_DISABLE ));
    }
    IDE_EXCEPTION_END;
    
    (void)qcm::destroyQcmTableInfo( sNewTableInfo );
    (void)qcmPartition::destroyQcmPartitionInfoList( sNewPartInfoList );

    qcmPartition::restoreTempInfo( sInfo,
                                   sOldPartInfoList,
                                   NULL );

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qdd::dropTablePartition( qcStatement  * aStatement,
                                qcmTableInfo * aPartInfo,
                                idBool         aIsDropTablespace,
                                idBool         aUseRecyclebin )
{
/***********************************************************************
 *
 * Description :
 *      PROJ-1502 PARTITIONED DISK TABLE����
 *      �ϳ��� ���̺� ��Ƽ���� �����Ѵ�.
 *
 * Implementation :
 *
 *
 ***********************************************************************/

    UInt       sIndexCount = 0;
    SInt       sIdx        = 0;
    qcmIndex * sIndexInfo  = NULL;

    // -----------------------------------------------------
    // 2-1. ���̺� ��Ƽ�� ���� ��Ÿ ���̺� ���� ����
    // -----------------------------------------------------
    if ( aUseRecyclebin == ID_FALSE )
    {
        IDE_TEST(qdd::deleteTablePartitionFromMeta(aStatement,
                                                   aPartInfo->partitionID)
                 != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    // -----------------------------------------------------
    // 2-2. �ε��� ��Ƽ�� ���� ��Ÿ ���̺� ���� ����
    // -----------------------------------------------------
    for( sIndexCount = 0;
         sIndexCount < aPartInfo->indexCount;
         sIndexCount++ )
    {
        IDE_TEST(qdd::deleteIndexPartitionFromMeta(aStatement,
                                                   aPartInfo->indices[sIndexCount].indexPartitionID)
                 != IDE_SUCCESS );
    }

    // -----------------------------------------------------
    // 2-3. ��Ƽ�� ��ü ����
    // -----------------------------------------------------

    if ( aUseRecyclebin == ID_FALSE )
    {
        IDE_TEST( smiTable::dropTable( QC_SMI_STMT( aStatement ),
                                       aPartInfo->tableHandle,
                                       ( aIsDropTablespace == ID_TRUE ) ? SMI_TBSLV_DROP_TBS:
                                                                          SMI_TBSLV_DDL_DML )
                  != IDE_SUCCESS );
    }
    else
    {
        /* PROJ-2441 flashback : index drop */
        for ( sIdx = aPartInfo->indexCount - 1; sIdx >= 0; sIdx-- )
        {
            sIndexInfo = &aPartInfo->indices[sIdx];
            
            IDE_TEST( smiTable::dropIndex(
                          QC_SMI_STMT(aStatement),
                          aPartInfo->tableHandle,
                          smiGetTableIndexByID( aPartInfo->tableHandle,
                                                sIndexInfo->indexId ),
                          SMI_TBSLV_DDL_DML )
                      != IDE_SUCCESS );
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdd::dropTablePartitionWithoutMeta( qcStatement  * aStatement,
                                           qcmTableInfo * aPartInfo )
{
/***********************************************************************
 *
 * Description :
 *      PROJ-1502 PARTITIONED DISK TABLE����
 *      �ϳ��� ���̺� ��Ƽ���� �����Ѵ�. �� ��Ÿ������ ���� �ʴ´�.
 *
 * Implementation :
 *
 *
 ***********************************************************************/

    UInt    sIndexCount;

    // -----------------------------------------------------
    // 2-1. �ε��� ��Ƽ�� ���� ��Ÿ ���̺� ���� ����
    // -----------------------------------------------------
    for( sIndexCount = 0;
         sIndexCount < aPartInfo->indexCount;
         sIndexCount++ )
    {
        IDE_TEST(qdd::deleteIndexPartitionFromMeta(aStatement,
                                                   aPartInfo->indices[sIndexCount].indexPartitionID)
                 != IDE_SUCCESS );
    }

    // -----------------------------------------------------
    // 2-2. ��Ƽ�� ��ü ����
    // -----------------------------------------------------
    IDE_TEST( smiTable::dropTable( QC_SMI_STMT( aStatement ),
                                   aPartInfo->tableHandle,
                                   SMI_TBSLV_DDL_DML )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdd::executeDropIndex(qcStatement * aStatement)
{
/***********************************************************************
 *
 * Description :
 *    DROP INDEX ... �� execution ����
 *
 * Implementation :
 *    1. �ε��� ���� ���� üũ, tableID, indexID �� ������
 *    2. table info ���ϱ�
 *    3. 2 ���� ���� info �߿��� indexID �� �ش��ϴ� qcmIndex ���ϱ�
 *       => ������ ERR_NOT_EXIST_INDEX ����
 *    4. smiTable::dropIndex
 *    5. delete index from meta
 *    6. Index�� ���õ� Procedure�� ���� ������ ����
 *    7. ��Ÿ ĳ�� �籸��
 *
 * Replication�� �ɸ� Table�� ���� DDL�� ���, �߰������� �Ʒ��� �۾��� �Ѵ�.
 *    1. Validate�� Execute�� �ٸ� Transaction�̹Ƿ�, ������Ƽ �˻�� Execute���� �Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "qdd::executeDropIndex"

    qdDropParseTree      * sParseTree;
    qcmIndex             * sIndex = NULL;
    UInt                   sTableID;
    UInt                   sIndexID;
    smOID                  sTableOID;
    UInt                   i = 0;
    qcmTableInfo         * sInfo = NULL;
    qcmTableInfo         * sNewTableInfo = NULL;
    qcmPartitionInfoList * sOldPartInfoList = NULL;
    qcmPartitionInfoList * sNewPartInfoList = NULL;
    idBool                 sIsPartitioned = ID_FALSE;
    idBool                 sIsPrimary = ID_FALSE;
    qdIndexTableList     * sOldIndexTable = NULL;
    smSCN                  sSCN;
    void                 * sTableHandle;

    idBool                 sIsReplicatedTable = ID_FALSE;

    smOID                * sOldPartitionOID = NULL;
    UInt                   sPartitionCount = 0;

    smOID                * sOldTableOIDArray = NULL;
    smOID                * sNewTableOIDArray = NULL;
    UInt                   sTableOIDCount = 0;
    UInt                   sDDLSupplementalLog = QCU_DDL_SUPPLEMENTAL_LOG;
    UInt                   sDDLRequireLevel = 0;

    sParseTree = (qdDropParseTree *)aStatement->myPlan->parseTree;

    // TASK-2176
    // Table�� ���� Lock�� ȹ���Ѵ�.
    IDE_TEST(qcm::validateAndLockTable(aStatement,
                                       sParseTree->tableHandle,
                                       sParseTree->tableSCN,
                                       SMI_TABLE_LOCK_X)
             != IDE_SUCCESS);

    sInfo = sParseTree->tableInfo;

    // PROJ-1502 PARTITIONED DISK TABLE
    if( sInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
    {
        sIsPartitioned = ID_TRUE;

        // ��� ��Ƽ�ǿ� LOCK(X)
        IDE_TEST( qcmPartition::validateAndLockPartitionInfoList( aStatement,
                                                                  sParseTree->partInfoList,
                                                                  SMI_TBSLV_DDL_DML, // TBS Validation �ɼ�
                                                                  SMI_TABLE_LOCK_X,
                                                                  smiGetDDLLockTimeOut((QC_SMI_STMT(aStatement))->getTrans()))
                  != IDE_SUCCESS );
        
        // �Ľ�Ʈ������ ��Ƽ�� ���� ����Ʈ�� �����´�.
        sOldPartInfoList = sParseTree->partInfoList;

        // PROJ-1624 non-partitioned index
        IDE_TEST( qdx::validateAndLockIndexTableList( aStatement,
                                                      sParseTree->oldIndexTables,
                                                      SMI_TBSLV_DDL_DML, // TBS Validation �ɼ�
                                                      SMI_TABLE_LOCK_X,
                                                      smiGetDDLLockTimeOut((QC_SMI_STMT(aStatement))->getTrans()))
                  != IDE_SUCCESS );

        if ( ( sInfo->replicationCount > 0 ) ||
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
    else
    {
        // Nothing to do.
    }

    // PROJ-1407 Temporary table
    // session temporary table�� �����ϴ� ��� DDL�� �� �� ����.
    IDE_TEST_RAISE( qcuTemporaryObj::existSessionTable( sInfo ) == ID_TRUE,
                    ERR_SESSION_TEMPORARY_TABLE_EXIST );

    /* PROJ-1723 [MDW/INTEGRATOR] Altibase Plugin ����
       DDL Statement Text�� �α�
    */
    if ( sDDLSupplementalLog == 1 )
    {
        IDE_TEST( qciMisc::writeDDLStmtTextLog( aStatement,
                                                sInfo->tableOwnerID,
                                                sInfo->name )
                  != IDE_SUCCESS );
    }

    IDE_TEST(qcm::checkIndexByUser( aStatement,
                                    sParseTree->userName,
                                    sParseTree->objectName,
                                    &(sParseTree->userID),
                                    &sTableID,
                                    &sIndexID)
             != IDE_SUCCESS);

    for ( i = 0; i < sInfo->indexCount; i++)
    {
        if (sInfo->indices[i].indexId == sIndexID)
        {
            sIndex = &sInfo->indices[i];
        }
    }

    IDE_TEST_RAISE(sIndex == NULL, ERR_NOT_EXIST_INDEX);

    /* PROJ-1442 Replication Online �� DDL ���
     * Validate�� Execute�� �ٸ� Transaction�̹Ƿ�, ������Ƽ �˻�� Execute���� �Ѵ�.
     * PROJ-2642 Table on Replication Allow DDL
     */
    if ( sInfo->replicationCount > 0 )
    {
        sDDLRequireLevel = 0;

        if ( ( sIndex->isUnique == ID_TRUE ) ||
             ( sIndex->isLocalUnique == ID_TRUE ) )
        {
            sDDLRequireLevel = 1;
        }
        else
        {
            /* do nothing */
        }

        IDE_TEST( qci::mManageReplicationCallback.mIsDDLEnableOnReplicatedTable( sDDLRequireLevel,
                                                                                 sInfo )
                  != IDE_SUCCESS );

        if ( ( sIndex->isUnique == ID_TRUE ) ||
             ( sIndex->isLocalUnique == ID_TRUE ) ||
             ( ( sIsPartitioned == ID_TRUE) &&
               ( sIndex->indexPartitionType == QCM_NONE_PARTITIONED_INDEX ) ) )
        {

            IDE_TEST_RAISE( QC_SMI_STMT( aStatement )->getTrans()->getReplicationMode() == SMI_TRANSACTION_REPL_NONE,
                            ERR_CANNOT_WRITE_REPL_INFO );

            if ( sIsPartitioned == ID_TRUE )
            {
                sOldTableOIDArray = sOldPartitionOID;
                sTableOIDCount = sPartitionCount;
            }
            else
            {
                sTableOID = smiGetTableId( sInfo->tableHandle );

                sOldTableOIDArray = &sTableOID;
                sTableOIDCount = 1;
            }

            IDE_TEST( qciMisc::checkRunningEagerReplicationByTableOID( aStatement,
                                                                       sOldTableOIDArray,
                                                                       sTableOIDCount )
                      != IDE_SUCCESS );

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
        /* do nothing */
    }

    // PROJ-1502 PARTITIONED DISK TABLE
    if( sIsPartitioned == ID_TRUE )
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
        if ( ( sIndex->indexPartitionType == QCM_NONE_PARTITIONED_INDEX ) ||
             ( sIsPrimary == ID_TRUE ) )
        {
            IDE_TEST( qdx::findIndexTableInList( sParseTree->oldIndexTables,
                                                 sIndex->indexTableID,
                                                 & sOldIndexTable )
                      != IDE_SUCCESS );
            
            IDE_TEST( qdx::dropIndexTable( aStatement,
                                           sOldIndexTable,
                                           ID_FALSE /* aIsDropTablespace */ )
                      != IDE_SUCCESS );
        }
        else
        {
            // Nothing to do.
        }
        
        if ( ( sIndex->indexPartitionType != QCM_NONE_PARTITIONED_INDEX ) ||
             ( sIsPrimary == ID_TRUE ) )
        {
            IDE_TEST( dropIndexPartitions( aStatement,
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

    IDE_TEST(smiTable::dropIndex(
                     QC_SMI_STMT( aStatement ),
                     sInfo->tableHandle,
                     sIndex->indexHandle,
                     SMI_TBSLV_DDL_DML )
             != IDE_SUCCESS);

    // delete index from meta
    IDE_TEST(deleteIndicesFromMetaByIndexID(aStatement, sIndex->indexId)
             != IDE_SUCCESS);

    /* BUG-35445 Check Constraint, Function-Based Index���� ��� ���� Function�� ����/���� ���� */
    IDE_TEST( qcmProc::relRemoveRelatedToIndexByIndexID(
                    aStatement,
                    sIndex->indexId )
              != IDE_SUCCESS );

    sTableOID = smiGetTableId(sInfo->tableHandle);


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
    // PROJ-1624 non-partitioned index
    if ( sIsPartitioned == ID_TRUE )
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
        // nothing to do
    }

    if ( ( sIsReplicatedTable == ID_TRUE ) ||
         ( sDDLSupplementalLog == 1 ) )
    {
        if ( sIsPartitioned == ID_TRUE )
        {
            sOldTableOIDArray = sOldPartitionOID;
            sNewTableOIDArray = sOldPartitionOID;
            sTableOIDCount = sPartitionCount;
        }
        else
        {
            sOldTableOIDArray = &sTableOID;
            sNewTableOIDArray = &sTableOID;
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

    if ( sIsPartitioned == ID_TRUE )
    {
        (void)qcmPartition::destroyQcmPartitionInfoList( sOldPartInfoList );
    }
    else
    {
        // nothing to do
    }

    // index table info�� destroy
    if ( sOldIndexTable != NULL )
    {
        (void)qcm::destroyQcmTableInfo(sOldIndexTable->tableInfo);
    }
    else
    {
        // nothing to do
    }
    
    (void)qcm::destroyQcmTableInfo(sInfo);

    if ( ( sIsReplicatedTable == ID_TRUE ) ||
         ( QCG_GET_SESSION_IS_NEED_DDL_INFO( aStatement ) == ID_TRUE ) )
    {
        qrc::setDDLDestInfo( aStatement, 
                             1,
                             &(sNewTableInfo->tableOID),
                             0,
                             NULL );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_SESSION_TEMPORARY_TABLE_EXIST )
    {
        IDE_SET(ideSetErrorCode( qpERR_ABORT_QDB_TEMPORARY_TABLE_DDL_DISABLE ));
    }
    IDE_EXCEPTION(ERR_NOT_EXIST_INDEX);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCM_NOT_EXISTS_INDEX));
    }
    IDE_EXCEPTION( ERR_CANNOT_WRITE_REPL_INFO )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDB_CANNOT_WRITE_REPL_INFO ) );
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

IDE_RC qdd::dropIndexPartitions( qcStatement          * aStatement,
                                 qcmPartitionInfoList * aPartInfoList,
                                 UInt                   aDropIndexID,
                                 idBool                 aIsCascade,
                                 idBool                 aIsDropTablespace )
{
/***********************************************************************
 *
 * Description :
 *      PROJ-1502 PARTITIONED DISK TABLE
 *      ������ �ε��� ��Ƽ���� ��� �����Ѵ�.
 *
 * Implementation :
 *      1. ��Ƽ���� ������ŭ �ݺ�
 *          1-1. ������ �ε����� ã�´�.
 *          1-2. ���� �ε��� ����
 *          1-3. ��Ÿ ���̺� ����
 *
 ***********************************************************************/

    qcmPartitionInfoList    * sPartInfoList;
    qcmTableInfo            * sPartInfo;
    qcmIndex                * sLocalIndex = NULL;
    qcmIndex                * sTempLocalIndex = NULL;
    UInt                      sLocalIndexCount;
    void                    * sIndexHandle  = NULL;

    // -----------------------------------------------------
    // 1. ��Ƽ���� ������ŭ �ݺ�
    // -----------------------------------------------------
    for( sPartInfoList = aPartInfoList;
         sPartInfoList != NULL;
         sPartInfoList = sPartInfoList->next )
    {
        sPartInfo = sPartInfoList->partitionInfo;

        // -----------------------------------------------------
        // 1-1. ������ �ε����� ã�´�.
        // -----------------------------------------------------
        for( sLocalIndexCount = 0;
             sLocalIndexCount < sPartInfo->indexCount;
             sLocalIndexCount++ )
        {
            sTempLocalIndex = & sPartInfo->indices[sLocalIndexCount];

            if( sTempLocalIndex->indexId == aDropIndexID )
            {
                sLocalIndex = sTempLocalIndex;

                sIndexHandle = (void *)smiGetTableIndexByID(
                    sPartInfo->tableHandle,
                    sTempLocalIndex->indexId);

                break;
            }
            else
            {
                // Nothing to do
            }
        }

        // �ε����� �ִ��� üũ�� �̹� �߱� ������ ���� �ε�����
        // �ݵ�� �־�� �Ѵ�.
        IDE_ASSERT( sLocalIndex != NULL );

        // -----------------------------------------------------
        // 1-2. ���� �ε��� ����
        // -----------------------------------------------------

        // BUG-35135
        if( aIsCascade == ID_TRUE )
        {
            IDE_TEST(smiTable::dropIndex( QC_SMI_STMT( aStatement ),
                                          sPartInfo->tableHandle,
                                          sIndexHandle,
                                          ( aIsDropTablespace == ID_TRUE ) ? SMI_TBSLV_DROP_TBS:
                                                                             SMI_TBSLV_DDL_DML )
                     != IDE_SUCCESS);
        }
        else
        {
            IDE_TEST(smiTable::dropIndex( QC_SMI_STMT( aStatement ),
                                          sPartInfo->tableHandle,
                                          sLocalIndex->indexHandle,
                                          ( aIsDropTablespace == ID_TRUE ) ? SMI_TBSLV_DROP_TBS:
                                                                             SMI_TBSLV_DDL_DML )
                     != IDE_SUCCESS);
        }

        // -----------------------------------------------------
        // 1-3. ��Ÿ ���̺� ����
        // -----------------------------------------------------
        IDE_TEST(qdd::deleteIndexPartitionFromMeta(aStatement,
                                                   sLocalIndex->indexPartitionID)
                 != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdd::executeDropUser(qcStatement * aStatement)
{
/***********************************************************************
 *
 * Description :
 *    DROP USER ����
 *
 * Implementation :
 *    1. USER ���� ���� �˻�
 *    2. SYS, SYSTEM �������� �˻�
 *    3. �ش� ������ ���� object �� ������ �˻�
 *    4. DBA_USERS_ ��Ÿ ���̺��� ����
 *    5. ���� ���� ���� ����
 *      5.1 DELETE FROM SYS_GRANT_SYSTEM_ WHERE GRANTEE_ID=?
 *      5.2 DELETE FROM SYS_GRANT_OBJECT_ WHERE GRANTEE_ID=?
 *      5.3 DELETE FROM SYS_GRANT_SYSTEM_ WHERE GRANTOR_ID=?
 *    6. ���� TABLESPACE ���� ����
 *      6.1 DELETE FROM SYS_TBS_USERS_ WHERE USER_ID=?
 *
 ***********************************************************************/

#define IDE_FN "qdd::executeDropUser"

    qdDropParseTree    * sParseTree;
    UInt                 sUserID;
    idBool               sIsTrue;
    qcuSqlSourceInfo     sqlInfo;
    SChar              * sSqlStr;
    vSLong               sRowCnt;

    sParseTree = (qdDropParseTree *)aStatement->myPlan->parseTree;
    
    // if specified user doesn't exist, then error
    if (qcmUser::getUserID(aStatement, sParseTree->objectName, &sUserID)
        != IDE_SUCCESS)
    {
        IDE_CLEAR();
        sqlInfo.setSourceInfo( aStatement,
                               & sParseTree->objectName );
        IDE_RAISE(ERR_NOT_EXIST_USER);
    }

    // if specified user is SYS, then error
    IDE_TEST_RAISE((sUserID == QC_SYS_USER_ID) ||
                   (sUserID == QC_SYSTEM_USER_ID),
                   ERR_NO_DROP_SYS_USER);

    // if specified user has at least one object
    //  (table, sequence, procedure, index, , etc), then error
    IDE_TEST(qcm::checkObjectByUserID(aStatement, sUserID, &sIsTrue)
             != IDE_SUCCESS);

    IDE_TEST_RAISE(sIsTrue == ID_TRUE, ERR_EXIST_OWN_OBJECT);
    
    IDU_FIT_POINT( "qdd::executeDropUser::alloc::sSqlStr",
                    idERR_ABORT_InsufficientMemory );

    IDE_TEST(STRUCT_ALLOC_WITH_SIZE(aStatement->qmxMem,
                                    SChar,
                                    QD_MAX_SQL_LENGTH,
                                    &sSqlStr)
             != IDE_SUCCESS);
    
    // BUG-41230 SYS_USERS_ => DBA_USERS_
    idlOS::snprintf( (SChar*)sSqlStr, QD_MAX_SQL_LENGTH,
                     "DELETE FROM DBA_USERS_ "
                     "WHERE USER_ID = INTEGER'%"ID_INT32_FMT"'",
                     sUserID );

    IDE_TEST(qcg::runDMLforDDL(QC_SMI_STMT( aStatement ),
                               (SChar*)sSqlStr, &sRowCnt) != IDE_SUCCESS);

    IDE_TEST_RAISE(sRowCnt != 1, ERR_META_CRASH);
    
    /* PROJ-1812 ROLE
     * remove GRANTEE_ID = aUserID from SYS_USER_ROLES_ */
    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                     "DELETE FROM SYS_USER_ROLES_ "
                     "WHERE GRANTEE_ID = INTEGER'%"ID_INT32_FMT"'",
                     sUserID );

    IDE_TEST(qcg::runDMLforDDL(QC_SMI_STMT( aStatement ),
                               (SChar*)sSqlStr, &sRowCnt) != IDE_SUCCESS);

    /* PROJ-1812 ROLE
     * remove GRANTOR_ID = aUserID from SYS_USER_ROLES_ */
    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                     "DELETE FROM SYS_USER_ROLES_ "
                     "WHERE GRANTOR_ID = INTEGER'%"ID_INT32_FMT"'",
                     sUserID );

    IDE_TEST(qcg::runDMLforDDL(QC_SMI_STMT( aStatement ),
                               (SChar*)sSqlStr, &sRowCnt) != IDE_SUCCESS);
    
    IDE_TEST(qdpDrop::removePriv4DropUser( aStatement, sUserID )
             != IDE_SUCCESS);

    /* PROJ-2207 Password policy support */
    idlOS::snprintf( (SChar*)sSqlStr, QD_MAX_SQL_LENGTH,
                     "DELETE FROM SYS_PASSWORD_HISTORY_ "
                     "WHERE USER_ID = INTEGER'%"ID_INT32_FMT"'",
                     sUserID );

    IDE_TEST(qcg::runDMLforDDL(QC_SMI_STMT( aStatement ),
                               (SChar*)sSqlStr, &sRowCnt) != IDE_SUCCESS);

    idlOS::snprintf( (SChar*)sSqlStr, QD_MAX_SQL_LENGTH,
                     "DELETE FROM SYS_TBS_USERS_ "
                     "WHERE USER_ID = INTEGER'%"ID_INT32_FMT"'",
                     sUserID );

    IDE_TEST(qcg::runDMLforDDL(QC_SMI_STMT( aStatement ),
                               (SChar*)sSqlStr, &sRowCnt) != IDE_SUCCESS);
   
    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_NO_DROP_SYS_USER);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDD_NO_DROP_SYS_USER));
    }
    IDE_EXCEPTION(ERR_NOT_EXIST_USER);
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QDR_NOT_EXISTS_USER,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION(ERR_EXIST_OWN_OBJECT);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDD_EXIST_OWN_OBJECT));
    }
    IDE_EXCEPTION(ERR_META_CRASH);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDB_META_CRASH));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qdd::executeDropUserCascade(qcStatement * aStatement)
{
/***********************************************************************
 *
 * Description :
 *    DROP USER ... CASCADE ����
 *
 * Implementation :
 *    1. �ش� ������ �����ϴ��� �˻��Ѵ�
 *    2. ���� ������ �������� �˻��Ѵ� (SYS, SYSTEM������ ������ �� ����)
 *    3. ���� ������ ��ü ����Ʈ�� ���� �´�.
 *        3.1. DROP �ؾ� �� ���̺��� ����Ʈ�� �޾ƿ´� ( TABLE, VIEW, QUEUE, MATERIALIZED VIEW )
 *        3.2. DROP �ؾ� �� �ε����� ����Ʈ�� �޾ƿ´� ( INDEX )
 *        3.3. DROP �ؾ� �� Ʈ������ ����Ʈ�� �޾ƿ´� ( TRIGGER )
 *        3.4. DROP �ؾ� �� �������� ����Ʈ�� �޾ƿ´� ( SEQUENCE )  // BUG-16980
 *        3.5. DROP �ؾ� �� ���ν����� ����Ʈ�� �޾ƿ´� ( PROCEDURE )
 *    4. ���� ������ ��ü�� ��Ÿ ���̺� ���ڵ忡�� ����
 *        4.1. QUEUE, VIEW, TABLE, MATERIALIZED VIEW
 *              executeDropTableOwnedByUser()
 *        4.2. INDEX
 *              executeDropIndexOwnedByUser()
 *        4.3. TRIGGER
 *              executeDropTriggerOwnedByUser()
 *        4.4. SEQUENCE
 *              executeDropSequenceOwnedByUser()  // BUG-16980
 *        4.5. PROCEDURE
 *              executeDropProcOwnedByUser()
 *    5. ���� ���� ��ü�� ��Ÿ ���̺��� �����Ѵ�
 *    6. ���� ���� ������ cached meta�� ����
 *        6.1. TRIGGER : make new cached meta ����
 *        6.2. INDEX : make new cached meta ����
 *        6.3. TRIGGER : old cached meta ����
 *        6.4. INDEX : old cached meta ����
 *        6.5. TABLE, VIEW, QUEUE, MATERIALIZED VIEW cached meta ����
 *
 ***********************************************************************/

    qdDropParseTree         * sParseTree;
    UInt                      sUserID;
    qcuSqlSourceInfo          sqlInfo;
    qcmTableInfoList        * sTableInfoList        = NULL;
    qcmTableInfoList        * sTempTableInfoList    = NULL;
    qcmIndexInfoList        * sIndexInfoList        = NULL;
    qcmIndexInfoList        * sTempIndexInfoList    = NULL;
    qcmTriggerInfoList      * sTriggerInfoList      = NULL;
    qcmTriggerInfoList      * sTempTriggerInfoList  = NULL;
    qcmProcInfoList         * sProcInfoList         = NULL;
    qcmProcInfoList         * sTempProcInfoList     = NULL;
    qcmSequenceInfoList     * sSequenceInfoList     = NULL;
    qcmSequenceInfoList     * sTempSequenceInfoList = NULL;
    qcmTableInfo            * sTableInfo;
    qdnTriggerCache         * sTriggerCache;
    void                    * sTriggerHandle;
    UInt                      i;
    UInt                      sPartTableCount       = 0;
    UInt                      sProcLatchCount       = 0;
    qsxProcInfo             * sProcInfo;
    qcmPartitionInfoList   ** sTablePartInfoList    = NULL;
    qcmPartitionInfoList   ** sIndexPartInfoList    = NULL;
    qcmPartitionInfoList   ** sTriggerPartInfoList  = NULL;
    qcmPartitionInfoList    * sTempPartInfoList     = NULL;
    qdIndexTableList       ** sIndexTableList       = NULL;
    qdIndexTableList        * sIndexTable           = NULL;
    // PROJ-1073 Package
    qcmPkgInfoList          * sPkgInfoList          = NULL;
    qcmPkgInfoList          * sTempPkgInfoList      = NULL;
    UInt                      sPkgLatchCount        = 0;
    qsxPkgInfo              * sPkgInfo;
    // BUG-37248
    UInt                      sStage                = 0;

    sParseTree = (qdDropParseTree *)aStatement->myPlan->parseTree;

    //-----------------------------------------
    // �ش� ������ �����ϴ��� �˻��Ѵ�
    //-----------------------------------------

    if (qcmUser::getUserID(aStatement, sParseTree->objectName, &sUserID)
        != IDE_SUCCESS)
    {
        IDE_CLEAR();
        sqlInfo.setSourceInfo( aStatement,
                               & sParseTree->objectName );
        IDE_RAISE(ERR_NOT_EXIST_USER);
    }

    //-----------------------------------------
    // ���� ������ �������� �˻��Ѵ�
    // SYS, SYSTEM������ ������ �� ����
    //-----------------------------------------

    IDE_TEST_RAISE((sUserID == QC_SYS_USER_ID) ||
                   (sUserID == QC_SYSTEM_USER_ID),
                   ERR_NO_DROP_SYS_USER);

    //-----------------------------------------
    // ���� ������ ��ü ����Ʈ�� ���� �´�.
    //-----------------------------------------

    // DROP �ؾ� �� ���̺��� ����Ʈ�� �޾ƿ´� ( TABLE, VIEW, QUEUE, MATERIALIZED VIEW )
    IDE_TEST(qcmUser::findTableListByUserID(aStatement,
                                            sUserID,
                                            &sTableInfoList)
             != IDE_SUCCESS);

    // DROP �ؾ� �� �ε����� ����Ʈ�� �޾ƿ´� ( INDEX )
    // �ٸ� ���� ������ ���̺� ���� �ε���
    IDE_TEST(qcmUser::findIndexListByUserID(aStatement,
                                            sUserID,
                                            &sIndexInfoList)
             != IDE_SUCCESS);

    // PROJ-1407 Temporary table
    // session temporary table�� �����ϴ� ��� DDL�� �� �� ����.
    for( sTempTableInfoList = sTableInfoList;
         sTempTableInfoList != NULL;
         sTempTableInfoList = sTempTableInfoList->next )
    {
        sTableInfo = sTempTableInfoList->tableInfo;

        IDE_TEST_RAISE( qcuTemporaryObj::existSessionTable( sTableInfo )
                        == ID_TRUE, ERR_SESSION_TEMPORARY_TABLE_EXIST );
    }

    sPartTableCount = 0;
    for( sTempTableInfoList = sTableInfoList;
         sTempTableInfoList != NULL;
         sTempTableInfoList = sTempTableInfoList->next )
    {
        sTableInfo = sTempTableInfoList->tableInfo;

        if( sTableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
        {
            sPartTableCount++;
        }
    }

    if( sPartTableCount != 0 )
    {
        IDU_FIT_POINT( "qdd::executeDropUserCascade::alloc::sTablePartInfoList",
                        idERR_ABORT_InsufficientMemory );

        IDE_TEST( STRUCT_ALLOC_WITH_COUNT( aStatement->qmxMem,
                                                 qcmPartitionInfoList*,
                                                 sPartTableCount,
                                                 & sTablePartInfoList )
                        != IDE_SUCCESS);
        
        IDE_TEST( STRUCT_ALLOC_WITH_COUNT( aStatement->qmxMem,
                                                 qdIndexTableList*,
                                                 sPartTableCount,
                                                 & sIndexTableList )
                        != IDE_SUCCESS);
    }

    // PROJ-1502 PARTITIONED DISK TABLE
    for( sTempTableInfoList = sTableInfoList, i = 0;
         sTempTableInfoList != NULL;
         sTempTableInfoList = sTempTableInfoList->next )
    {
        sTableInfo = sTempTableInfoList->tableInfo;

        if( sTableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
        {
            IDE_ASSERT( sTablePartInfoList != NULL );

            IDE_TEST( qcmPartition::getPartitionInfoList(
                          aStatement,
                          QC_SMI_STMT( aStatement ),
                          aStatement->qmxMem,
                          sTableInfo->tableID,
                          & sTablePartInfoList[i] )
                      != IDE_SUCCESS );

            // ��� ��Ƽ�ǿ� LOCK(X)
            IDE_TEST( qcmPartition::validateAndLockPartitionInfoList( aStatement,
                                                                      sTablePartInfoList[i],
                                                                      SMI_TBSLV_DDL_DML, // TBS Validation �ɼ�
                                                                      SMI_TABLE_LOCK_X,
                                                                      smiGetDDLLockTimeOut((QC_SMI_STMT(aStatement))->getTrans()))
                      != IDE_SUCCESS );

            // PROJ-1624 non-partitioned index
            IDE_TEST( qdx::makeAndLockIndexTableList(
                          aStatement,
                          ID_TRUE,
                          sTableInfo,
                          & sIndexTableList[i] )
                      != IDE_SUCCESS );
            
            IDE_TEST( qdx::validateAndLockIndexTableList( aStatement,
                                                          sIndexTableList[i],
                                                          SMI_TBSLV_DDL_DML, // TBS Validation �ɼ�
                                                          SMI_TABLE_LOCK_X,
                                                          smiGetDDLLockTimeOut((QC_SMI_STMT(aStatement))->getTrans()))
                      != IDE_SUCCESS );
            
            i++;
        }
    }

    sPartTableCount = 0;
    for( sTempIndexInfoList = sIndexInfoList;
         sTempIndexInfoList != NULL;
         sTempIndexInfoList = sTempIndexInfoList->next )
    {
        sTableInfo = sTempIndexInfoList->tableInfo;

        if( sTableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
        {
            sPartTableCount++;
        }
    }

    if( sPartTableCount != 0 )
    {
        IDU_FIT_POINT( "qdd::executeDropUserCascade::alloc::sIndexPartInfoList",
                        idERR_ABORT_InsufficientMemory );

        IDE_TEST( STRUCT_ALLOC_WITH_COUNT( aStatement->qmxMem,
                                           qcmPartitionInfoList,
                                           sPartTableCount,
                                           & sIndexPartInfoList )
                  != IDE_SUCCESS);
    }

    // PROJ-1502 PARTITIONED DISK TABLE
    for( sTempIndexInfoList = sIndexInfoList, i = 0;
         sTempIndexInfoList != NULL;
         sTempIndexInfoList = sTempIndexInfoList->next )
    {
        sTableInfo = sTempIndexInfoList->tableInfo;

        if( sTableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
        {
            IDE_ASSERT( sIndexPartInfoList != NULL );

            IDE_TEST( qcmPartition::getPartitionInfoList(
                          aStatement,
                          QC_SMI_STMT( aStatement ),
                          aStatement->qmxMem,
                          sTableInfo->tableID,
                          & sIndexPartInfoList[i] )
                      != IDE_SUCCESS );

            // ��� ��Ƽ�ǿ� LOCK(X)
            IDE_TEST( qcmPartition::validateAndLockPartitionInfoList( aStatement,
                                                                      sIndexPartInfoList[i],
                                                                      SMI_TBSLV_DDL_DML, // TBS Validation �ɼ�
                                                                      SMI_TABLE_LOCK_X,
                                                                      smiGetDDLLockTimeOut((QC_SMI_STMT(aStatement))->getTrans()))
                      != IDE_SUCCESS );

            // PROJ-1624 global non-partitioned index
            // non-partitioned index�ϳ��� lock
            if ( sTempIndexInfoList->isPartitionedIndex == ID_FALSE )
            {
                sIndexTable = & sTempIndexInfoList->indexTable;
                
                IDE_TEST(qcm::getTableInfoByID(aStatement,
                                               sIndexTable->tableID,
                                               &sIndexTable->tableInfo,
                                               &sIndexTable->tableSCN,
                                               &sIndexTable->tableHandle)
                         != IDE_SUCCESS);
                
                IDE_TEST(qcm::validateAndLockTable(aStatement,
                                                   sIndexTable->tableHandle,
                                                   sIndexTable->tableSCN,
                                                   SMI_TABLE_LOCK_X)
                         != IDE_SUCCESS);
            }
            else
            {
                // Nothing to do.
            }
            
            i++;
        }
    }

    // DROP �ؾ� �� Ʈ������ ����Ʈ�� �޾ƿ´� ( TRIGGER )
    // �ٸ� ���� ������ ���̺� ���� Ʈ����
    IDE_TEST(qcmUser::findTriggerListByUserID(aStatement,
                                              sUserID,
                                              &sTriggerInfoList)
             != IDE_SUCCESS);

    sPartTableCount = 0;
    for ( sTempTriggerInfoList = sTriggerInfoList;
          sTempTriggerInfoList != NULL;
          sTempTriggerInfoList = sTempTriggerInfoList->next )
    {
        sTableInfo = sTempTriggerInfoList->tableInfo;

        if ( sTableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
        {
            sPartTableCount++;
        }
        else
        {
            /* Nothing to do */
        }
    }

    if ( sPartTableCount != 0 )
    {
        IDU_FIT_POINT( "qdd::executeDropUserCascade::alloc::sTriggerPartInfoList",
                        idERR_ABORT_InsufficientMemory );

        IDE_TEST( STRUCT_ALLOC_WITH_COUNT( QC_QMX_MEM( aStatement ),
                                           qcmPartitionInfoList,
                                           sPartTableCount,
                                           & sTriggerPartInfoList )
                  != IDE_SUCCESS);
    }
    else
    {
        /* Nothing to do */
    }

    // PROJ-1502 PARTITIONED DISK TABLE
    for ( sTempTriggerInfoList = sTriggerInfoList, i = 0;
          sTempTriggerInfoList != NULL;
          sTempTriggerInfoList = sTempTriggerInfoList->next )
    {
        sTableInfo = sTempTriggerInfoList->tableInfo;

        if ( sTableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
        {
            IDE_ASSERT( sTriggerPartInfoList != NULL );

            IDE_TEST( qcmPartition::getPartitionInfoList( aStatement,
                                                          QC_SMI_STMT( aStatement ),
                                                          QC_QMX_MEM( aStatement ),
                                                          sTableInfo->tableID,
                                                          & sTriggerPartInfoList[i] )
                      != IDE_SUCCESS );

            // ��� ��Ƽ�ǿ� LOCK(X)
            IDE_TEST( qcmPartition::validateAndLockPartitionInfoList( aStatement,
                                                                      sTriggerPartInfoList[i],
                                                                      SMI_TBSLV_DDL_DML, // TBS Validation �ɼ�
                                                                      SMI_TABLE_LOCK_X,
                                                                      smiGetDDLLockTimeOut((QC_SMI_STMT(aStatement))->getTrans()))
                      != IDE_SUCCESS );

            i++;
        }
        else
        {
            /* Nothing to do */
        }
    }

    // BUG-16980
    // DROP �ؾ� �� �������� ����Ʈ�� �޾ƿ´� ( SEQUENCE )
    IDE_TEST(qcmUser::findSequenceListByUserID(aStatement,
                                               sUserID,
                                               &sSequenceInfoList)
             != IDE_SUCCESS);

    // DROP �ؾ� �� ���ν����� ����Ʈ�� �޾ƿ´� ( PROCEDURE )
    IDE_TEST(qcmUser::findProcListByUserID(aStatement,
                                           sUserID,
                                           &sProcInfoList)
             != IDE_SUCCESS);

    sStage = 1;
    // proj-1535
    // DROP �ؾ� �� ���������� ���� X-LATCH�� ȹ���Ѵ�.
    sTempProcInfoList = sProcInfoList;
    while (sTempProcInfoList != NULL)
    {
        IDE_TEST( qsxProc::latchX( sTempProcInfoList->procOID,
                                   ID_TRUE ) != IDE_SUCCESS );
        sProcLatchCount ++;

        sTempProcInfoList = sTempProcInfoList->next;
    }

    sStage = 2;
    // PROJ-1073 Package
    // DROP �ؾ� �� package�� ����Ʈ�� �޾ƿ´�.
    IDE_TEST(qcmUser::findPkgListByUserID(aStatement,
                                          sUserID,
                                          &sPkgInfoList)
             != IDE_SUCCESS );

    sTempPkgInfoList = sPkgInfoList;
    while (sTempPkgInfoList != NULL)
    {
        IDE_TEST( qsxPkg::latchX( sTempPkgInfoList->pkgOID,
                                  ID_TRUE ) != IDE_SUCCESS );
        sPkgLatchCount ++;

        sTempPkgInfoList = sTempPkgInfoList->next;
    }

    //-----------------------------------------
    // ���� ������ ������ ����
    //-----------------------------------------

    // VIEW, TABLE, QUEUE, MATERIALIZED VIEW
    // ���̺��� ���,
    // ���̺� ���� �ε����� Ʈ������ ��ü, ��Ÿ ���̺� ���ڵ���� ���� ��.
    for( sTempTableInfoList = sTableInfoList, i = 0;
         sTempTableInfoList != NULL;
         sTempTableInfoList = sTempTableInfoList->next )
    {
        sTableInfo = sTempTableInfoList->tableInfo;

        if( sTableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
        {
            IDE_ASSERT( sTablePartInfoList != NULL );

            IDE_TEST( executeDropTableOwnedByUser(
                          aStatement,
                          sTempTableInfoList,
                          sTablePartInfoList[i],
                          sIndexTableList[i] )
                      != IDE_SUCCESS );

            i++;
        }
        else
        {
            IDE_TEST( executeDropTableOwnedByUser(
                          aStatement,
                          sTempTableInfoList,
                          NULL,
                          NULL )
                      != IDE_SUCCESS );
        }
    }

    // INDEX : �ٸ� ���� ������ ���̺� ���� �ε���
    for( sTempIndexInfoList = sIndexInfoList, i = 0;
         sTempIndexInfoList != NULL;
         sTempIndexInfoList = sTempIndexInfoList->next )
    {
        sTableInfo = sTempIndexInfoList->tableInfo;

        if( sTableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
        {
            IDE_ASSERT( sIndexPartInfoList != NULL );

            IDE_TEST( executeDropIndexOwnedByUser(
                          aStatement,
                          sTempIndexInfoList,
                          sIndexPartInfoList[i] )
                      != IDE_SUCCESS );

            i++;
        }
        else
        {
            IDE_TEST( executeDropIndexOwnedByUser(
                          aStatement,
                          sTempIndexInfoList,
                          NULL )
                      != IDE_SUCCESS );
        }
    }

    // TRIGGER : �ٸ� ���� ������ ���̺� ���� Ʈ����
    sTempTriggerInfoList = sTriggerInfoList;
    while (sTempTriggerInfoList != NULL)
    {
        IDE_TEST( executeDropTriggerOwnedByUser(
                      aStatement,
                      sTempTriggerInfoList)
                  != IDE_SUCCESS );

        sTempTriggerInfoList = sTempTriggerInfoList->next;
    }

    // BUG-16980
    // SEQUENCE
    sTempSequenceInfoList = sSequenceInfoList;
    while (sTempSequenceInfoList != NULL)
    {
        IDE_TEST( executeDropSequenceOwnedByUser(
                      aStatement,
                      sTempSequenceInfoList)
                  != IDE_SUCCESS );

        sTempSequenceInfoList = sTempSequenceInfoList->next;
    }

    // PROCEDURE
    sTempProcInfoList = sProcInfoList;
    while (sTempProcInfoList != NULL)
    {
        IDE_TEST( executeDropProcOwnedByUser(
                      aStatement,
                      sUserID,
                      sTempProcInfoList)
                  != IDE_SUCCESS );

        sTempProcInfoList = sTempProcInfoList->next;
    }

    /* PROJ-2197 PSM Renewal */
    for( sTempTableInfoList = sTableInfoList, i = 0;
         sTempTableInfoList != NULL;
         sTempTableInfoList = sTempTableInfoList->next )
    {
        sTableInfo = sTempTableInfoList->tableInfo;

        // related PSM
        IDE_TEST(qcmProc::relSetInvalidProcOfRelated(
            aStatement,
            sTableInfo->tableOwnerID,
            sTableInfo->name,
            idlOS::strlen((SChar*)sTableInfo->name),
            QS_TABLE) != IDE_SUCCESS);
    }

    // PROJ-1073 Package
    sTempPkgInfoList = sPkgInfoList;
    while( sTempPkgInfoList != NULL )
    {
        IDE_TEST( executeDropPkgOwnedByUser( 
                      aStatement,
                      sUserID,
                      sTempPkgInfoList )
                  != IDE_SUCCESS );
 
        sTempPkgInfoList = sTempPkgInfoList->next;
    }

    for( sTempTableInfoList = sTableInfoList, i = 0;
         sTempTableInfoList != NULL;
         sTempTableInfoList = sTempTableInfoList->next )
    {
        sTableInfo = sTempTableInfoList->tableInfo;

        IDE_TEST(qcmPkg::relSetInvalidPkgOfRelated(
                aStatement,
                sTableInfo->tableOwnerID,
                sTableInfo->name,
                idlOS::strlen((SChar*)sTableInfo->name),
                QS_TABLE) != IDE_SUCCESS);
    }

    //-----------------------------------------
    // ���� ������ ��ü���� qp meta ������ ����
    // Ʈ���ſ� ���ν����� ��ü�� �����ϸ鼭 ��Ÿ ���̺� �Բ� ������
    //-----------------------------------------

    IDE_TEST(deleteObjectFromMetaByUserID(aStatement, sUserID)
             != IDE_SUCCESS);

    /* PROJ-1832 New database link */
    IDE_TEST( qdkDropDatabaseLinkByUserId( aStatement, sUserID )
              != IDE_SUCCESS );
    
    //-----------------------------------------
    // ���� ������ ������ cached meta�� ����
    //-----------------------------------------

    // TRIGGER : make new cached meta
    for ( sTempTriggerInfoList = sTriggerInfoList, i = 0;
          sTempTriggerInfoList != NULL;
          sTempTriggerInfoList = sTempTriggerInfoList->next )
    {
        sTableInfo = sTempTriggerInfoList->tableInfo;

        IDE_TEST( qcm::touchTable( QC_SMI_STMT( aStatement ),
                                   sTableInfo->tableID,
                                   SMI_TBSLV_DDL_DML )
                  != IDE_SUCCESS);

        // BUG-16422
        IDE_TEST( qcmTableInfoMgr::makeTableInfo( aStatement,
                                                  sTableInfo,
                                                  NULL )
                  != IDE_SUCCESS);

        // PROJ-1502 PARTITIONED DISK TABLE
        if ( sTableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
        {
            IDE_ASSERT( sTriggerPartInfoList != NULL );

            IDE_TEST( qcmPartition::touchPartitionInfoList( QC_SMI_STMT( aStatement ),
                                                            sTriggerPartInfoList[i] )
                      != IDE_SUCCESS );

            for ( sTempPartInfoList = sTriggerPartInfoList[i];
                  sTempPartInfoList != NULL;
                  sTempPartInfoList = sTempPartInfoList->next )
            {
                IDE_TEST( qcmTableInfoMgr::makeTableInfo( aStatement,
                                                          sTempPartInfoList->partitionInfo,
                                                          NULL )
                          != IDE_SUCCESS);
            }

            i++;
        }
        else
        {
            /* Nothing to do */
        }
    }

    // INDEX : make new cached meta
    for( sTempIndexInfoList = sIndexInfoList, i = 0;
         sTempIndexInfoList != NULL;
         sTempIndexInfoList = sTempIndexInfoList->next )
    {
        sTableInfo = sTempIndexInfoList->tableInfo;

        IDE_TEST( qcm::touchTable(QC_SMI_STMT( aStatement ),
                                  sTableInfo->tableID,
                                  SMI_TBSLV_DDL_DML )
                  != IDE_SUCCESS );

        // BUG-16422
        IDE_TEST( qcmTableInfoMgr::makeTableInfo( aStatement,
                                                  sTableInfo,
                                                  NULL )
                  != IDE_SUCCESS);

        // PROJ-1502 PARTITIONED DISK TABLE
        if( sTableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
        {
            IDE_ASSERT( sIndexPartInfoList != NULL );

            IDE_TEST( qcmPartition::touchPartitionInfoList( QC_SMI_STMT( aStatement ),
                                                            sIndexPartInfoList[i] )
                      != IDE_SUCCESS );

            for( sTempPartInfoList = sIndexPartInfoList[i];
                 sTempPartInfoList != NULL;
                 sTempPartInfoList = sTempPartInfoList->next )
            {
                IDE_TEST( qcmTableInfoMgr::makeTableInfo( aStatement,
                                                          sTempPartInfoList->partitionInfo,
                                                          NULL )
                          != IDE_SUCCESS);
            }
            
            // PROJ-1624 global non-partitioned index
            // index table tableinfo�� destroy�Ѵ�.
            if ( sTempIndexInfoList->isPartitionedIndex == ID_FALSE )
            {
                sIndexTable = & sTempIndexInfoList->indexTable;
                
                IDE_TEST( qcmTableInfoMgr::destroyTableInfo( aStatement,
                                                             sIndexTable->tableInfo )
                          != IDE_SUCCESS );
            }
            else
            {
                // Nothing to do.
            }

            i++;
        }
    }

    // TRIGGER
    for( sTempTriggerInfoList = sTriggerInfoList;
         sTempTriggerInfoList != NULL;
         sTempTriggerInfoList = sTempTriggerInfoList->next )
    {
        sTriggerHandle = (void*) smiGetTable( sTempTriggerInfoList->triggerOID );

        IDE_TEST( smiObject::getObjectTempInfo( sTriggerHandle,
                                                (void**)&sTriggerCache )
                  != IDE_SUCCESS );

        IDE_TEST( qdnTrigger::freeTriggerCache( sTriggerCache )
                  != IDE_SUCCESS );

        // trigger�� ������ tableInfo�� ������ ���̹Ƿ�
        // tableInfo�� ���������� �ʴ´�.
    }

    // INDEX
    // index�� ������ tableInfo�� ������ ���̹Ƿ�
    // tableInfo�� ���������� �ʴ´�.

    // TABLE, VIEW, QUEUE, MATERIALIZED VIEW
    for( sTempTableInfoList = sTableInfoList, i = 0;
         sTempTableInfoList != NULL;
         sTempTableInfoList = sTempTableInfoList->next )
    {
        // PROJ-1888 INSTEAD OF TRIGGER
        if ( ( sTempTableInfoList->tableInfo->tableType == QCM_USER_TABLE )  ||
             ( sTempTableInfoList->tableInfo->tableType == QCM_VIEW )        ||
             ( sTempTableInfoList->tableInfo->tableType == QCM_MVIEW_TABLE ) )
        {
            IDE_TEST( qdnTrigger::freeTriggerCaches4DropTable(
                          sTempTableInfoList->tableInfo)
                      != IDE_SUCCESS );
        }
        else
        {
            // nothing to do.
        }

        // BUG-16422
        IDE_TEST( qcmTableInfoMgr::destroyTableInfo(
                      aStatement,
                      sTempTableInfoList->tableInfo )
                  != IDE_SUCCESS );

        // PROJ-1502 PARTITIONED DISK TABLE
        if( sTempTableInfoList->tableInfo->tablePartitionType
            == QCM_PARTITIONED_TABLE )
        {
            IDE_ASSERT( sTablePartInfoList != NULL );

            for( sTempPartInfoList = sTablePartInfoList[i];
                 sTempPartInfoList != NULL;
                 sTempPartInfoList = sTempPartInfoList->next )
            {
                IDE_TEST( qcmTableInfoMgr::destroyTableInfo( aStatement,
                                                             sTempPartInfoList->partitionInfo)
                          != IDE_SUCCESS );
            }
            
            IDE_ASSERT( sIndexTableList != NULL );
            
            for ( sIndexTable = sIndexTableList[i];
                  sIndexTable != NULL;
                  sIndexTable = sIndexTable->next )
            {
                IDE_TEST( qcmTableInfoMgr::destroyTableInfo( aStatement,
                                                             sIndexTable->tableInfo )
                          != IDE_SUCCESS );
            }

            i++;
        }
    }

    // SEQUENCE
    // sequence�� cached meta�� ����.
    // PROJ-1073 Package
    for( sTempPkgInfoList = sPkgInfoList;
         sTempPkgInfoList != NULL;
         sTempPkgInfoList = sTempPkgInfoList->next )
    {
        IDE_TEST( qsxPkg::getPkgInfo( sTempPkgInfoList->pkgOID,
                                      & sPkgInfo )
                  != IDE_SUCCESS );

        IDE_TEST( qsxPkg::destroyPkgInfo ( & sPkgInfo )
                  != IDE_SUCCESS );

        IDE_TEST( qsxPkg::disablePkgObjectInfo( sTempPkgInfoList->pkgOID )
                  != IDE_SUCCESS );
    }
    sStage = 1;

    sPkgLatchCount = 0;
    sTempPkgInfoList = sPkgInfoList;
    while (sTempPkgInfoList != NULL)
    {
        (void) qsxPkg::unlatch( sTempPkgInfoList->pkgOID );

        sTempPkgInfoList = sTempPkgInfoList->next;
    }

    // PROCEDURE
    for( sTempProcInfoList = sProcInfoList;
         sTempProcInfoList != NULL;
         sTempProcInfoList = sTempProcInfoList->next )
    {
        IDE_TEST( qsxProc::getProcInfo( sTempProcInfoList->procOID,
                                        & sProcInfo )
                  != IDE_SUCCESS );

        IDE_TEST( qsxProc::destroyProcInfo ( & sProcInfo )
                  != IDE_SUCCESS );

        IDE_TEST( qsxProc::disableProcObjectInfo( sTempProcInfoList->procOID )
                  != IDE_SUCCESS );
    }
    sStage = 0;

    sProcLatchCount = 0;
    sTempProcInfoList = sProcInfoList;
    while (sTempProcInfoList != NULL)
    {
        (void) qsxProc::unlatch( sTempProcInfoList->procOID );

        sTempProcInfoList = sTempProcInfoList->next;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_SESSION_TEMPORARY_TABLE_EXIST )
    {
        IDE_SET(ideSetErrorCode( qpERR_ABORT_QDD_DROP_USER_DISABLE_BECAUSE_TEMP_TABLE ));
    }
    IDE_EXCEPTION(ERR_NO_DROP_SYS_USER);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDD_NO_DROP_SYS_USER));
    }
    IDE_EXCEPTION(ERR_NOT_EXIST_USER);
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QDR_NOT_EXISTS_USER,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION_END;

    // BUG-37248
    switch( sStage )
    {
        case 2:
            for ( i = 0, sTempPkgInfoList = sPkgInfoList;
                  i < sPkgLatchCount;
                  i++, sTempPkgInfoList = sTempPkgInfoList->next )
            {
                (void)qsxPkg::unlatch( sTempPkgInfoList->pkgOID );
            }
            /* fall through */
        case 1:
            for ( i = 0, sTempProcInfoList = sProcInfoList;
                  i < sProcLatchCount;
                  i++, sTempProcInfoList = sTempProcInfoList->next )
            {
                (void) qsxProc::unlatch( sTempProcInfoList->procOID );
            }
            break;
        default :
            break;
    }

    return IDE_FAILURE;
}

IDE_RC qdd::executeDropSequence(qcStatement * aStatement)
{
/***********************************************************************
 *
 * Description :
 *    DROP SEQUENCE ... �� execution ����
 *
 * Implementation :
 *    1. ������ �ڵ�, info ���ϱ�
 *    2. ��Ÿ ���̺��� sequence ����
 *    3. ��Ÿ ���̺��� privilege ����
 *    4. smiTable::dropSequence
 *    6. related PSM �� invalid ���·� ����
 *    8. ��Ÿ ĳ������ ����
 *
 ***********************************************************************/

#define IDE_FN "qdd::executeDropSequence"

    qdDropParseTree    * sParseTree;
    qcmSequenceInfo      sSequenceInfo;
    void               * sSequenceHandle = NULL;
    SLong                sStartValue;
    SLong                sCurrIncrementValue;
    SLong                sCurrMinValue;
    SLong                sCurrMaxValue;
    SLong                sCurrCacheValue;
    UInt                 sOption;
    SChar                sSeqTableNameStr[ QC_MAX_OBJECT_NAME_LEN + 1 ];
    qcNamePosition       sSeqTableName;

    // PROJ-2365 sequence table ����
    qcmTableInfo       * sSeqTableInfo = NULL;
    void               * sSeqTableHandle;
    smSCN                sSeqTableSCN;
    
    sParseTree = (qdDropParseTree *)aStatement->myPlan->parseTree;

    IDE_TEST(qcm::checkSequenceInfo(aStatement,
                                    sParseTree->userID,
                                    sParseTree->objectName,
                                    &sSequenceInfo,
                                    &sSequenceHandle)
             != IDE_SUCCESS);

    // QUEUE_SEQUENCE�� Drop�� �� ����.
    IDE_TEST_RAISE(sSequenceInfo.sequenceType != QCM_SEQUENCE,
                   ERR_NOT_EXIST_SEQUENCE);

    /* PROJ-1723 [MDW/INTEGRATOR] Altibase Plugin ����
       DDL Statement Text�� �α�
    */
    if (QCU_DDL_SUPPLEMENTAL_LOG == 1)
    {
        IDE_TEST( qciMisc::writeDDLStmtTextLog( aStatement,
                                                sSequenceInfo.sequenceOwnerID,
                                                sSequenceInfo.name )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    IDE_TEST(smiTable::getSequence( sSequenceHandle,
                                    &sStartValue,
                                    &sCurrIncrementValue,
                                    &sCurrCacheValue,
                                    &sCurrMaxValue,
                                    &sCurrMinValue,
                                    &sOption)
             != IDE_SUCCESS);
    
    // PROJ-2365 sequence table
    // �̹� ������ sequence table�� ���� ������ alter�� ���� ����
    if ( ( sOption & SMI_SEQUENCE_TABLE_MASK ) == SMI_SEQUENCE_TABLE_TRUE )
    {
        // make sequence table name : SEQ1$SEQ
        IDE_TEST_RAISE( sParseTree->objectName.size + QDS_SEQ_TABLE_SUFFIX_LEN
                        > QC_MAX_OBJECT_NAME_LEN,
                        ERR_SEQUENCE_NAME_LEN );
        
        QC_STR_COPY( sSeqTableNameStr, sParseTree->objectName );
        idlOS::strncat( sSeqTableNameStr,
                        QDS_SEQ_TABLE_SUFFIX_STR,
                        QDS_SEQ_TABLE_SUFFIX_LEN );
        
        sSeqTableName.stmtText = sSeqTableNameStr;
        sSeqTableName.offset   = 0;
        sSeqTableName.size     = sParseTree->objectName.size + QDS_SEQ_TABLE_SUFFIX_LEN;
        
        IDE_TEST_RAISE( qcm::getTableHandleByName(
                            QC_SMI_STMT(aStatement),
                            sParseTree->userID,
                            (UChar*) sSeqTableName.stmtText,
                            sSeqTableName.size,
                            &sSeqTableHandle,
                            &sSeqTableSCN ) != IDE_SUCCESS,
                        ERR_NOT_EXIST_TABLE );
        
        IDE_TEST( smiGetTableTempInfo( sSeqTableHandle,
                                       (void**)&sSeqTableInfo )
                  != IDE_SUCCESS );
        
        // sequence table�� DDL lock
        IDE_TEST( qcm::validateAndLockTable( aStatement,
                                             sSeqTableHandle,
                                             sSeqTableSCN,
                                             SMI_TABLE_LOCK_X )
                  != IDE_SUCCESS );
        
        // if specified tables is replicated, the ERR_DDL_WITH_REPLICATED_TABLE
        IDE_TEST_RAISE( sSeqTableInfo->replicationCount > 0,
                        ERR_DDL_WITH_REPLICATED_TABLE );

        //proj-1608:replicationCount�� 0�� �� recovery count�� �׻� 0�̾�� ��
        IDE_DASSERT( sSeqTableInfo->replicationRecoveryCount == 0 );
        
        // drop sequence table
        IDE_TEST( qds::dropSequenceTable( aStatement,
                                          sSeqTableInfo )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }
    
    IDE_TEST(qdd::deleteTableFromMeta(aStatement, sSequenceInfo.sequenceID)
             != IDE_SUCCESS);

    IDE_TEST(qdpDrop::removePriv4DropTable(aStatement, sSequenceInfo.sequenceID)
             != IDE_SUCCESS);

    // PROJ-2223 audit
    IDE_TEST( qcmAudit::deleteObject( aStatement,
                                      sSequenceInfo.sequenceOwnerID,
                                      sSequenceInfo.name )
              != IDE_SUCCESS );
    
    // delete from sm
    IDE_TEST( smiTable::dropSequence( QC_SMI_STMT( aStatement ),
                                      sSequenceHandle )
              != IDE_SUCCESS );

    /* PROJ-2197 PSM Renewal */
    // related PSM
    IDE_TEST(qcmProc::relSetInvalidProcOfRelated(
        aStatement,
        sParseTree->userID,
        sSequenceInfo.name,
        idlOS::strlen((SChar*)sSequenceInfo.name),
        QS_TABLE) != IDE_SUCCESS);

    // PROJ-1073 Package
    IDE_TEST(qcmPkg::relSetInvalidPkgOfRelated(
        aStatement,
        sParseTree->userID,
        sSequenceInfo.name,
        idlOS::strlen((SChar*)sSequenceInfo.name),
        QS_TABLE) != IDE_SUCCESS);

    // PROJ-2365
    // sequence tableInfo ����
    if ( sSeqTableInfo != NULL )
    {
        (void) qcm::destroyQcmTableInfo( sSeqTableInfo );
    }
    else
    {
        // Nothing to do.
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_NOT_EXIST_SEQUENCE);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCM_NOT_EXIST_SEQUENCE));
    }
    IDE_EXCEPTION( ERR_NOT_EXIST_TABLE )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QCV_NOT_EXISTS_TABLE, "" ) );
    }
    IDE_EXCEPTION( ERR_DDL_WITH_REPLICATED_TABLE )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDD_DDL_WITH_REPLICATED_TBL ) );
    }
    IDE_EXCEPTION( ERR_SEQUENCE_NAME_LEN )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qdd::executeDropSequence",
                                  "sequence name is too long" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qdd::executeTruncateTable(qcStatement * aStatement)
{
/***********************************************************************
 *
 * Description :
 *    TRUNCATE TABLE ... �� execution ����
 *
 * Implementation :
 *    1. get table info. ���̺� �ڵ�, ���̺� ID ���صα�
 *    2. ���̺��� �ε��� ������ŭ qcmIndex ���� �Ҵ�޾Ƽ�
 *       ������ �ε��� ���� ����
 *    3. ���̺� ���� �����Ͽ� tableOID �ο��ޱ�
 *    4. SYS_TABLES_ �� tableOID �� 3 ���� ���� ������ ����
 *    5. �ε��� ���� ����
 *    6. related PSM �� invalid ���·� ����
 *    7. drop old table
 *    8. ��Ÿ ĳ�� �籸��
 *
 * Replication�� �ɸ� Table�� ���� DDL�� ���, �߰������� �Ʒ��� �۾��� �Ѵ�.
 *    1. Validate�� Execute�� �ٸ� Transaction�̹Ƿ�, ������Ƽ �˻�� Execute���� �Ѵ�.
 *    2. ���� Receiver Thread ����
 *    3. SYS_REPL_ITEMS_�� TABLE_OID �÷� ����
 *    4. Table Meta Log Record ���
 *
 ***********************************************************************/

#define IDE_FN "qdd::executeTruncateTable"

    qdDropParseTree      * sParseTree;
    qcmTableInfo         * sInfo = NULL;
    smSCN                  sSCN;
    smOID                  sTableOID;
    UInt                   sTableID;         // table Meta ID.
    void                 * sOldTableHandle;
    void                 * sTableHandle;
    qcmIndex             * sNewTableIndex;
    qcmIndex            ** sNewPartIndex  = NULL;
    UInt                   sNewPartIndexCount = 0;
    idBool                 sIsPartitioned = ID_FALSE;
    qdIndexTableList     * sIndexTable;
    qcmPartitionInfoList * sPartInfoList = NULL;
    qcmPartitionInfoList * sOldPartInfoList = NULL;
    qcmTableInfo         * sPartInfo = NULL;
    qcmTableInfo        ** sNewPartitionInfoArr  = NULL;
    smOID                * sNewPartitionOID = NULL;
    smOID                  sOldTableOID;
    smOID                * sOldPartitionOID = NULL;
    qcmTableInfo         * sTempTableInfo = NULL;
    void                ** sOldPartitionHandle;
    UInt                   sPartitionCount = 0;
    UInt                   i;
    UInt                 * sPartitionID;
    UInt                   sOrgTableFlag;
    UInt                   sOrgTableParallelDegree;
    idBool                 sIsReplicatedTable = ID_FALSE;

    smiSegAttr             sSegAttr;
    smiSegStorageAttr      sSegStoAttr;
    UInt                   sPartType = 0;

    smOID                * sOldTableOIDArray = NULL;
    smOID                * sNewTableOIDArray = NULL;
    UInt                   sTableOIDCount = 0;

    qcmColumn            * sTempColumns = NULL;

    sParseTree = (qdDropParseTree *)aStatement->myPlan->parseTree;

    // TASK-2176
    // Table�� ���� Lock�� ȹ���Ѵ�.
    IDE_TEST( qcm::validateAndLockTable(aStatement,
                                        sParseTree->tableHandle,
                                        sParseTree->tableSCN,
                                        SMI_TABLE_LOCK_X)
              != IDE_SUCCESS );

    sInfo           = sParseTree->tableInfo;
    sTableID        = sInfo->tableID;
    sOldTableHandle = sInfo->tableHandle;
    sOldTableOID    = smiGetTableId(sOldTableHandle);

    if( sInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
    {
        sIsPartitioned = ID_TRUE;

        // -----------------------------------------------------
        // Partition OID ����Ʈ��  �����ϱ� ���� �޸� ũ�� ���
        // -----------------------------------------------------

        // ��� ��Ƽ�ǿ� LOCK(X)
        IDE_TEST( qcmPartition::validateAndLockPartitionInfoList( aStatement,
                                                                  sParseTree->partInfoList,
                                                                  SMI_TBSLV_DDL_DML, // TBS Validation �ɼ�
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
        
        // PROJ-1624 non-partitioned index
        IDE_TEST( qdx::validateAndLockIndexTableList( aStatement,
                                                      sParseTree->oldIndexTables,
                                                      SMI_TBSLV_DDL_DML, // TBS Validation �ɼ�
                                                      SMI_TABLE_LOCK_X,
                                                      smiGetDDLLockTimeOut((QC_SMI_STMT(aStatement))->getTrans()))
                  != IDE_SUCCESS );
        
        IDU_FIT_POINT( "qdd::executeTruncateTable::alloc::sOldPartitionOID",
                        idERR_ABORT_InsufficientMemory );

        IDE_TEST(aStatement->qmxMem->alloc(ID_SIZEOF(smOID) *
                                           sPartitionCount,
                                           (void**)&sOldPartitionOID)
                 != IDE_SUCCESS);

        for(i = 0, sPartInfoList = sOldPartInfoList;
            sPartInfoList != NULL;
            sPartInfoList = sPartInfoList->next, i++)
        {
            sPartInfo = sPartInfoList->partitionInfo;
            sOldPartitionOID[i] = smiGetTableId(sPartInfo->tableHandle);
        }
    }
    else
    {
        // Nothing to do.
    }

    /* PROJ-1442 Replication Online �� DDL ���
     * Validate�� Execute�� �ٸ� Transaction�̹Ƿ�, ������Ƽ �˻�� Execute���� �Ѵ�.
     */
    if(sInfo->replicationCount > 0)
    {
        IDE_TEST( qci::mManageReplicationCallback.mIsDDLEnableOnReplicatedTable( 0, 
                                                                                 sInfo )
                  != IDE_SUCCESS );

        IDE_TEST_RAISE(QC_SMI_STMT(aStatement)->getTrans()->getReplicationMode()
                       == SMI_TRANSACTION_REPL_NONE,
                       ERR_CANNOT_WRITE_REPL_INFO);

        /* PROJ-1442 Replication Online �� DDL ���
         * ���� Receiver Thread ����
         */
        if ( sIsPartitioned == ID_TRUE )
        {
            sOldTableOIDArray = sOldPartitionOID;
            sTableOIDCount = sPartitionCount;
        }
        else
        {
            sOldTableOIDArray = &sOldTableOID;
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

    if (sInfo->indexCount > 0)
    {
        // for non-partitioned index, partitioned index
        IDU_FIT_POINT( "qdd::executeTruncateTable::alloc::sNewTableIndex",
                        idERR_ABORT_InsufficientMemory );

        IDE_TEST(aStatement->qmxMem->alloc( ID_SIZEOF(qcmIndex) *
                                                  sInfo->indexCount,
                                                  (void**)&sNewTableIndex)
                       != IDE_SUCCESS);

        idlOS::memcpy(sNewTableIndex, sInfo->indices,
                      ID_SIZEOF(qcmIndex) * sInfo->indexCount);

        // for index partition
        // fix BUG-18706
        if( sIsPartitioned == ID_TRUE )
        {
            IDU_FIT_POINT( "qdd::executeTruncateTable::alloc::sNewPartIndex",
                            idERR_ABORT_InsufficientMemory );

            IDE_TEST(aStatement->qmxMem->alloc(ID_SIZEOF(qcmIndex*) *
                                               sPartitionCount,
                                               (void**)& sNewPartIndex)
                     != IDE_SUCCESS);

            for( i = 0, sPartInfoList = sOldPartInfoList;
                 sPartInfoList != NULL;
                 sPartInfoList = sPartInfoList->next, i++ )
            {
                sPartInfo = sPartInfoList->partitionInfo;

                if ( sPartInfo->indexCount > 0 )
                {
                    IDU_FIT_POINT( "qdd::executeTruncateTable::alloc::sNewPartIndexItem",
                                    idERR_ABORT_InsufficientMemory );

                    IDE_TEST(aStatement->qmxMem->alloc(ID_SIZEOF(qcmIndex) *
                                                       sPartInfo->indexCount,
                                                       (void**)&sNewPartIndex[i])
                             != IDE_SUCCESS);

                    if ( sNewPartIndexCount == 0 )
                    {
                        sNewPartIndexCount = sPartInfo->indexCount;
                    }
                    else
                    {
                        IDE_DASSERT( sNewPartIndexCount == sPartInfo->indexCount );
                    }

                    idlOS::memcpy( sNewPartIndex[i],
                                   sPartInfo->indices,
                                   ID_SIZEOF(qcmIndex) * sPartInfo->indexCount );
                }
                else
                {
                    // ��� non-partitioned index�� ������ ���
                    sNewPartIndex[i] = NULL;
                }
            }
        }
        else
        {
            // Nothing to do
        }
    }
    else
    {
        sNewTableIndex = NULL;
    }

    //-----------------
    // create new table
    //-----------------

    // ���ο� ���̺�
    // ���� ���̺��� Flag�� Parallel Degree�� ����
    // ���� ������.
    sOrgTableFlag = sInfo->tableFlag;
    sOrgTableParallelDegree = sInfo->parallelDegree;

    /* BUG-45503 Table ���� ���Ŀ� ���� ��, Table Meta Cache�� Column ������ �������� �ʴ� ��찡 �ֽ��ϴ�. */
    IDE_TEST( qcm::copyQcmColumns( QC_QMX_MEM( aStatement ),
                                   sInfo->columns,
                                   & sTempColumns,
                                   sInfo->columnCount )
              != IDE_SUCCESS );

    // create new table.
    IDE_TEST(qdbCommon::createTableOnSM( aStatement,
                                         sTempColumns,
                                         sInfo->tableOwnerID,
                                         sTableID,
                                         sInfo->maxrows,
                                         sInfo->TBSID,
                                         sInfo->segAttr,
                                         sInfo->segStoAttr,
                                         /* ���� Table Flag��
                                            ��°�� ���� =>
                                            MASK ��Ʈ�� ��� 1�μ���*/
                                         QDB_TABLE_ATTR_MASK_ALL,
                                         sOrgTableFlag, /* Flag Value */
                                         sOrgTableParallelDegree,
                                         &sTableOID)
             != IDE_SUCCESS);

    // BUG-44814 ddl ���� ����� ������� ���縦 �ؾ���
    smiStatistics::copyTableStats( smiGetTable(sTableOID), sOldTableHandle, NULL, 0 );

    // update table spec (tableOID).
    IDE_TEST(qdbCommon::updateTableSpecFromMeta( aStatement,
                                                 sParseTree->userName,
                                                 sParseTree->objectName,
                                                 sTableID,
                                                 sTableOID,
                                                 sInfo->columnCount,
                                                 sOrgTableParallelDegree )
             != IDE_SUCCESS);

    // PROJ-1502 PARTITIONED DISK TABLE
    if( sIsPartitioned == ID_TRUE )
    {
        IDU_FIT_POINT( "qdd::executeTruncateTable::alloc::sNewPartitionOID",
                        idERR_ABORT_InsufficientMemory );

        IDE_TEST(aStatement->qmxMem->alloc(ID_SIZEOF(smOID) * sPartitionCount,
                                           (void**)&sNewPartitionOID)
                 != IDE_SUCCESS);

        IDU_FIT_POINT( "qdd::executeTruncateTable::alloc::sOldPartitionHandle",
                        idERR_ABORT_InsufficientMemory );

        IDE_TEST(aStatement->qmxMem->alloc(ID_SIZEOF(void*) * sPartitionCount,
                                           (void**)&sOldPartitionHandle)
                 != IDE_SUCCESS);

        IDU_FIT_POINT( "qdd::executeTruncateTable::alloc::sPartitionID",
                        idERR_ABORT_InsufficientMemory );

        IDE_TEST(aStatement->qmxMem->alloc(ID_SIZEOF(UInt) * sPartitionCount,
                                           (void**)&sPartitionID)
                 != IDE_SUCCESS);

        // -----------------------------------------------------
        // ���̺� ��Ƽ�� ������ŭ �ݺ�
        // -----------------------------------------------------
        for( i = 0, sPartInfoList = sOldPartInfoList;
             sPartInfoList != NULL;
             sPartInfoList = sPartInfoList->next, i++ )
        {
            sPartInfo              = sPartInfoList->partitionInfo;
            sOldPartitionHandle[i] = sPartInfo->tableHandle;
            sPartitionID[i]        = sPartInfo->partitionID;
            sPartType              = sPartInfo->tableFlag & SMI_TABLE_TYPE_MASK;

            /* PROJ-2464 hybrid partitioned table ����
             *  - Partition Info�� ������ ����, Table Option�� Partitioned Table�� ������ �����Ѵ�.
             *  - ����, PartInfo�� ������ �̿����� �ʰ�, TBSID�� ���� ������ ������ �����ؼ� �̿��Ѵ�.
             */
            qdbCommon::adjustPhysicalAttr( sPartType,
                                           sInfo->segAttr,
                                           sInfo->segStoAttr,
                                           & sSegAttr,
                                           & sSegStoAttr,
                                           ID_TRUE /* aIsTable */ );

            /* BUG-45503 Table ���� ���Ŀ� ���� ��, Table Meta Cache�� Column ������ �������� �ʴ� ��찡 �ֽ��ϴ�. */
            IDE_TEST( qcm::copyQcmColumns( QC_QMX_MEM( aStatement ),
                                           sPartInfo->columns,
                                           & sTempColumns,
                                           sPartInfo->columnCount )
                      != IDE_SUCCESS );

            IDE_TEST(qdbCommon::createTableOnSM( aStatement,
                                                 sTempColumns,
                                                 sPartInfo->tableOwnerID,
                                                 sTableID,
                                                 sInfo->maxrows,
                                                 sPartInfo->TBSID,
                                                 sSegAttr,
                                                 sSegStoAttr,
                                                 /* ���� Table Flag��
                                                    ��°�� ���� =>
                                                    MASK ��Ʈ��
                                                    ��� 1�μ���*/
                                                 QDB_TABLE_ATTR_MASK_ALL,
                                                 sOrgTableFlag, /* Flag Value */
                                                 sOrgTableParallelDegree,
                                                 &sNewPartitionOID[i] )
                     != IDE_SUCCESS);
            // BUG-44814 ddl ���� ����� ������� ���縦 �ؾ���
            smiStatistics::copyTableStats( smiGetTable(sNewPartitionOID[i]), sPartInfo->tableHandle, NULL, 0 );

            IDE_TEST(qdbCommon::updatePartTableSpecFromMeta( aStatement,
                                                             sTableID,
                                                             sPartitionID[i],
                                                             sNewPartitionOID[i] )
                     != IDE_SUCCESS );
        }
    }
    else
    {
        // Nothing to do.
    }
    
    // PROJ-1624 non-partitioned index
    // index table�� createIndex�� �ٽ� �����ǹǷ� meta�� ���� �̸� �����.
    if( sIsPartitioned == ID_TRUE )
    {
        for ( sIndexTable = sParseTree->oldIndexTables;
              sIndexTable != NULL;
              sIndexTable = sIndexTable->next )
        {
            IDE_TEST( qdx::dropIndexTable( aStatement,
                                           sIndexTable,
                                           ID_FALSE /* aIsDropTablespace */ )
                      != IDE_SUCCESS );
        }
    }
    else
    {
        // Nothing to do.
    }
    
    // re-create index!!
    // index table�� ���� �����Ѵ�.
    IDE_TEST(qdbCommon::createIndexFromInfo(aStatement,
                                            sInfo,
                                            sTableOID,
                                            sPartitionCount,
                                            sNewPartitionOID,
                                            SMI_INDEX_BUILD_UNCOMMITTED_ROW_DISABLE,
                                            sNewTableIndex,
                                            sNewPartIndex,
                                            sNewPartIndexCount,
                                            sParseTree->oldIndexTables,
                                            &(sParseTree->newIndexTables),
                                            NULL,
                                            ID_FALSE)
             != IDE_SUCCESS);

    // PROJ-1624 global non-partitioned index
    // index meta���� index table id�� �����Ѵ�.
    if( sIsPartitioned == ID_TRUE )
    {
        for ( i = 0; i < sInfo->indexCount; i++ )
        {
            if ( ( sInfo->indices[i].indexPartitionType ==
                   QCM_NONE_PARTITIONED_INDEX )
                 &&
                 ( sInfo->indices[i].indexTableID !=
                   sNewTableIndex[i].indexTableID ) )
            {
                IDE_TEST( qdx::updateIndexSpecFromMeta(
                              aStatement,
                              sInfo->indices[i].indexId,
                              sNewTableIndex[i].indexTableID )
                          != IDE_SUCCESS );
            }
            else
            {
                // Nothing to do.
            }
        }
    }
    else
    {
        // Nothing to do.
    }
    
    /* PROJ-1442 Replication Online �� DDL ���
     * SYS_REPL_ITEMS_�� TABLE_OID �÷� ����
     */
    if(sIsReplicatedTable == ID_TRUE)
    {
        if(sIsPartitioned == ID_TRUE)
        {
            IDE_TEST(qci::mCatalogReplicationCallback.mUpdateReplItemsTableOIDArray( aStatement,
                                                                                     sOldPartitionOID,
                                                                                     sNewPartitionOID,
                                                                                     sPartitionCount )
                     != IDE_SUCCESS);
        }
        else
        {
            IDE_TEST(qci::mCatalogReplicationCallback.mUpdateReplItemsTableOIDArray( aStatement,
                                                                                     &sOldTableOID,
                                                                                     &sTableOID,
                                                                                     1 )
                     != IDE_SUCCESS);
        }
    }

    // PROJ-1502 PARTITIONED DISK TABLE
    if( sIsPartitioned == ID_TRUE )
    {
        for( i = 0; i < sPartitionCount; i++ )
        {
            IDE_TEST( smiTable::dropTable( QC_SMI_STMT( aStatement ),
                                           sOldPartitionHandle[i],
                                           SMI_TBSLV_DDL_DML )
                      != IDE_SUCCESS );
        }
        
        // drop old partitioned table
        IDE_TEST( smiTable::dropTable( QC_SMI_STMT( aStatement ),
                                       sOldTableHandle,
                                       SMI_TBSLV_DDL_DML )
                  != IDE_SUCCESS );
    }
    else
    {
        // drop old table
        IDE_TEST( smiTable::dropTable( QC_SMI_STMT( aStatement ),
                                       sOldTableHandle,
                                       SMI_TBSLV_DDL_DML )
                  != IDE_SUCCESS );
    }

    IDE_TEST(qcm::makeAndSetQcmTableInfo( QC_SMI_STMT( aStatement ),
                                          sTableID,
                                          sTableOID )
             != IDE_SUCCESS);

    IDE_TEST(qcm::getTableInfoByID(aStatement,
                                   sTableID,
                                   &sTempTableInfo,
                                   &sSCN,
                                   &sTableHandle)
             != IDE_SUCCESS);

    // PROJ-1502 PARTITIONED DISK TABLE
    if( sIsPartitioned == ID_TRUE )
    {
        // ���ο� qcmPartitionInfo���� pointer������ ������ �迭 ����
        IDU_FIT_POINT( "qdd::executeTruncateTable::cralloc::sNewPartitionInfoArr",
                        idERR_ABORT_InsufficientMemory );

        IDE_TEST( aStatement->qmxMem->cralloc( ID_SIZEOF(qcmTableInfo *) * sPartitionCount,
                                               (void **) & sNewPartitionInfoArr )
                  != IDE_SUCCESS );

        for ( i = 0; i < sPartitionCount; i++ )
        {
            IDE_TEST( qcmPartition::makeAndSetQcmPartitionInfo( QC_SMI_STMT( aStatement ),
                                                                sPartitionID[i],
                                                                sNewPartitionOID[i],
                                                                sTempTableInfo,
                                                                NULL )
                      != IDE_SUCCESS );

            IDE_TEST( smiGetTableTempInfo( smiGetTable( sNewPartitionOID[i] ),
                                           (void **) & sNewPartitionInfoArr[i] )
                      != IDE_SUCCESS );
        }
    }
    else
    {
        // nothing to do
    }

    /* PROJ-1442 Replication Online �� DDL ���
     * Table Meta Log Record ���
     * PROJ-2642 Table on Replication Allow DDL
     */
    if((sIsReplicatedTable == ID_TRUE) ||
       (QCU_DDL_SUPPLEMENTAL_LOG == 1))
    {
        if( sIsPartitioned == ID_TRUE )
        {
            sOldTableOIDArray = sOldPartitionOID;
            sNewTableOIDArray = sNewPartitionOID;
        }
        else
        {
            sOldTableOIDArray = &sOldTableOID;
            sNewTableOIDArray = &sTableOID;
            sTableOIDCount = 1;
        }

        IDE_TEST( qciMisc::writeTableMetaLogForReplication( aStatement,
                                                            sOldTableOIDArray,
                                                            sNewTableOIDArray,
                                                            sTableOIDCount )
                  != IDE_SUCCESS );

    }

    (void)qcm::destroyQcmTableInfo(sInfo);
    sInfo = NULL;

    // old index table tableinfo ����
    if ( sIsPartitioned == ID_TRUE )
    {
        (void)qcmPartition::destroyQcmPartitionInfoList( sOldPartInfoList );

        for ( sIndexTable = sParseTree->oldIndexTables;
              sIndexTable != NULL;
              sIndexTable = sIndexTable->next )
        {
            (void)qcm::destroyQcmTableInfo(sIndexTable->tableInfo);
        }
    }
    else
    {
        // Nothing to do.
    }
    
    if ( ( sIsReplicatedTable == ID_TRUE ) ||
         ( QCG_GET_SESSION_IS_NEED_DDL_INFO( aStatement ) == ID_TRUE ) )
    {
        qrc::setDDLDestInfo( aStatement, 
                             1,
                             &(sTempTableInfo->tableOID),
                             0,
                             NULL );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_CANNOT_WRITE_REPL_INFO)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDB_CANNOT_WRITE_REPL_INFO));
    }
    IDE_EXCEPTION_END;

    (void)qcm::destroyQcmTableInfo( sTempTableInfo );

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

    for ( sIndexTable = sParseTree->newIndexTables;
          sIndexTable != NULL;
          sIndexTable = sIndexTable->next )
    {
        (void)qcm::destroyQcmTableInfo( sIndexTable->tableInfo );
    }

    // on fail, must restore temp info.
    qcmPartition::restoreTempInfo( sInfo,
                                   sOldPartInfoList,
                                   NULL );

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qdd::executeTruncateTempTable(qcStatement * aStatement)
{
/***********************************************************************
 *
 * Description :
 *    TRUNCATE TABLE ... �� execution ����
 *    temporary table�� truncate�Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    qdDropParseTree  * sParseTree;

    IDE_ASSERT( aStatement != NULL );

    sParseTree = (qdDropParseTree *)(aStatement->myPlan->parseTree);

    IDE_TEST( qcuTemporaryObj::truncateTempTable( aStatement,
                                                  sParseTree->tableID,
                                                  sParseTree->temporaryType )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdd::deleteConstraintsFromMeta(qcStatement * aStatement,
                                      UInt          aTableID)
{
/***********************************************************************
 *
 * Description :
 *      executeDropTable �κ��� ȣ��, constraint ���� ����
 *
 * Implementation :
 *      1. SYS_CONSTRAINTS_ ��Ÿ ���̺��� constraint ���� ����
 *
 ***********************************************************************/

#define IDE_FN "qdd::deleteConstraintsFromMeta"

    SChar         * sSqlStr;
    vSLong          sRowCnt;

    IDU_FIT_POINT( "qdd::deleteConstraintsFromMeta::alloc::sSqlStr",
                    idERR_ABORT_InsufficientMemory );

    IDE_TEST(STRUCT_ALLOC_WITH_SIZE(aStatement->qmxMem,
                                    SChar,
                                    QD_MAX_SQL_LENGTH,
                                    &sSqlStr)
             != IDE_SUCCESS);


    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                     "DELETE FROM SYS_CONSTRAINTS_ "
                     "WHERE TABLE_ID = INTEGER'%"ID_INT32_FMT"'",
                     aTableID );
    IDE_TEST(qcg::runDMLforDDL(QC_SMI_STMT( aStatement ),
                               sSqlStr,
                               & sRowCnt ) != IDE_SUCCESS);

    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                     "DELETE FROM SYS_CONSTRAINT_COLUMNS_ "
                     "WHERE TABLE_ID = INTEGER'%"ID_INT32_FMT"'",
                     aTableID );
    IDE_TEST(qcg::runDMLforDDL(QC_SMI_STMT( aStatement ),
                               sSqlStr,
                               & sRowCnt ) != IDE_SUCCESS);
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qdd::deleteConstraintsFromMetaByConstraintID(qcStatement * aStatement,
                                                    UInt          aConstraintID)
{
/***********************************************************************
 *
 * Description :
 *      BUG-17326
 *      executeDropIndexInTBS �κ��� ȣ��, constraint ���� ����
 *
 * Implementation :
 *      1. SYS_CONSTRAINTS_ ��Ÿ ���̺��� constraint ���� ����
 *
 ***********************************************************************/

#define IDE_FN "qdd::deleteConstraintsFromMetaByConstraintID"

    SChar         * sSqlStr;
    vSLong          sRowCnt;

    IDU_FIT_POINT( "qdd::deleteConstraintsFromMetaByConstraintID::alloc::sSqlStr",
                    idERR_ABORT_InsufficientMemory );

    IDE_TEST(STRUCT_ALLOC_WITH_SIZE(aStatement->qmxMem,
                                    SChar,
                                    QD_MAX_SQL_LENGTH,
                                    &sSqlStr)
             != IDE_SUCCESS);


    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                     "DELETE FROM SYS_CONSTRAINTS_ "
                     "WHERE CONSTRAINT_ID = INTEGER'%"ID_INT32_FMT"'",
                     aConstraintID );
    IDE_TEST(qcg::runDMLforDDL(QC_SMI_STMT( aStatement ),
                               sSqlStr,
                               & sRowCnt ) != IDE_SUCCESS);

    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                     "DELETE FROM SYS_CONSTRAINT_COLUMNS_ "
                     "WHERE CONSTRAINT_ID = INTEGER'%"ID_INT32_FMT"'",
                     aConstraintID );
    IDE_TEST(qcg::runDMLforDDL(QC_SMI_STMT( aStatement ),
                               sSqlStr,
                               & sRowCnt ) != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qdd::deleteFKConstraintsFromMeta(qcStatement * aStatement,
                                        UInt          aTableID,
                                        UInt          aReferencedTableID)
{
/***********************************************************************
 *
 * Description :
 *      qdtDrop::exeuct(), qdd::executeDropUserCascade �κ��� ȣ��,
 *      constraint ���� ����
 *
 * Implementation :
 *      1. SYS_CONSTRAINTS_ ��Ÿ ���̺���
 *         foreign key constraint ���� ����
 *
 ***********************************************************************/

#define IDE_FN "qdd::deleteFKConstraintsFromMeta"

    SChar               * sSqlStr;
    vSLong sRowCnt;

    IDU_FIT_POINT( "qdd::deleteFKConstraintsFromMeta::alloc::sSqlStr",
                    idERR_ABORT_InsufficientMemory );

    IDE_TEST(STRUCT_ALLOC_WITH_SIZE(aStatement->qmxMem,
                                    SChar,
                                    QD_MAX_SQL_LENGTH,
                                    &sSqlStr)
             != IDE_SUCCESS);

    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                     "DELETE FROM SYS_CONSTRAINT_COLUMNS_ A "
                     "WHERE A.TABLE_ID = INTEGER'%"ID_INT32_FMT"'"
                     "  AND A.CONSTRAINT_ID = ("
                     "      SELECT B.CONSTRAINT_ID    "
                     "        FROM SYS_CONSTRAINTS_ B"
                     "       WHERE B.TABLE_ID = INTEGER'%"ID_INT32_FMT"'"
                     "         AND B.REFERENCED_TABLE_ID=INTEGER'%"ID_INT32_FMT"'"
                     "         AND B.CONSTRAINT_ID = A.CONSTRAINT_ID )",
                     aTableID, aTableID, aReferencedTableID );
    IDE_TEST(qcg::runDMLforDDL(QC_SMI_STMT( aStatement ),
                               sSqlStr,
                               & sRowCnt ) != IDE_SUCCESS);

    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                     "DELETE FROM SYS_CONSTRAINTS_ "
                     "WHERE TABLE_ID = INTEGER'%"ID_INT32_FMT"'"
                     "  AND REFERENCED_TABLE_ID=INTEGER'%"ID_INT32_FMT"'",
                     aTableID, aReferencedTableID );
    IDE_TEST(qcg::runDMLforDDL(QC_SMI_STMT( aStatement ),
                               sSqlStr,
                               & sRowCnt ) != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qdd::deleteTableFromMeta( qcStatement * aStatement,
                                 UInt          aTableID )
{
/***********************************************************************
 *
 * Description :
 *      executeDropTable,executeDropSequence �κ��� ȣ��
 *
 * Implementation :
 *      1. SYS_TABLES_, SYS_COLUMNS_ ��Ÿ ���̺��� �ش� ���̺� ���� ����
 *
 ***********************************************************************/

#define IDE_FN "qdd::deleteTableFromMeta"

    SChar               * sSqlStr;
    vSLong                sRowCnt;

    IDU_FIT_POINT( "qdd::deleteTableFromMeta::alloc::sSqlStr",
                    idERR_ABORT_InsufficientMemory );

    IDE_TEST(STRUCT_ALLOC_WITH_SIZE(aStatement->qmxMem,
                                    SChar,
                                    QD_MAX_SQL_LENGTH,
                                    &sSqlStr)
             != IDE_SUCCESS);

    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                     "DELETE FROM SYS_TABLES_ "
                     "WHERE TABLE_ID = INTEGER'%"ID_INT32_FMT"'",
                     aTableID );
    IDE_TEST(qcg::runDMLforDDL(QC_SMI_STMT( aStatement ),
                               sSqlStr,
                               & sRowCnt ) != IDE_SUCCESS);

    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                     "DELETE FROM SYS_COLUMNS_ "
                     "WHERE TABLE_ID = INTEGER'%"ID_INT32_FMT"'",
                     aTableID );
    IDE_TEST(qcg::runDMLforDDL(QC_SMI_STMT( aStatement ),
                               sSqlStr,
                               & sRowCnt ) != IDE_SUCCESS);

    // PROJ-1362
    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                     "DELETE FROM SYS_LOBS_ "
                     "WHERE TABLE_ID = INTEGER'%"ID_INT32_FMT"'",
                     aTableID );
    IDE_TEST(qcg::runDMLforDDL(QC_SMI_STMT(aStatement),
                               sSqlStr,
                               & sRowCnt ) != IDE_SUCCESS);

    // PROJ-1502 PARTITIONED DISK TABLE
    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                     "DELETE FROM SYS_PART_LOBS_ "
                     "WHERE TABLE_ID = INTEGER'%"ID_INT32_FMT"'",
                     aTableID );
    IDE_TEST(qcg::runDMLforDDL(QC_SMI_STMT(aStatement),
                               sSqlStr,
                               & sRowCnt ) != IDE_SUCCESS);

    // PROJ-1502 PARTITIONED DISK TABLE
    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                     "DELETE FROM SYS_PART_TABLES_ "
                     "WHERE TABLE_ID = INTEGER'%"ID_INT32_FMT"'",
                     aTableID );
    IDE_TEST(qcg::runDMLforDDL(QC_SMI_STMT(aStatement),
                               sSqlStr,
                               & sRowCnt ) != IDE_SUCCESS);

    // PROJ-1502 PARTITIONED DISK TABLE
    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                     "DELETE FROM SYS_PART_KEY_COLUMNS_ "
                     "WHERE PARTITION_OBJ_ID = INTEGER'%"ID_INT32_FMT"'"
                     "AND OBJECT_TYPE = INTEGER'%"ID_INT32_FMT"'",
                     aTableID,
                     QCM_TABLE_OBJECT_TYPE ); /* TABLE */

    IDE_TEST(qcg::runDMLforDDL(QC_SMI_STMT(aStatement),
                               sSqlStr,
                               & sRowCnt ) != IDE_SUCCESS);

    // PROJ-2002 Column Security
    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                     "DELETE FROM SYS_ENCRYPTED_COLUMNS_ "
                     "WHERE TABLE_ID = INTEGER'%"ID_INT32_FMT"'",
                     aTableID );
    IDE_TEST(qcg::runDMLforDDL(QC_SMI_STMT(aStatement),
                               sSqlStr,
                               & sRowCnt ) != IDE_SUCCESS);
    // PROJ-2422 srid
    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                     "DELETE FROM SYS_GEOMETRIES_ "
                     "WHERE TABLE_ID = INTEGER'%"ID_INT32_FMT"'",
                     aTableID );
    IDE_TEST( qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                 sSqlStr,
                                 &sRowCnt ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qdd::deleteTablePartitionFromMeta( qcStatement  * aStatement,
                                          UInt           aPartitionID )
{
/***********************************************************************
 *
 * Description :
 *      PROJ-1502 PARTITIONED DISK TABLE
 *
 * Implementation :
 *
 ***********************************************************************/

    SChar               * sSqlStr;
    vSLong                sRowCnt;

    IDU_FIT_POINT( "qdd::deleteTablePartitionFromMeta::alloc::sSqlStr",
                    idERR_ABORT_InsufficientMemory );

    IDE_TEST(STRUCT_ALLOC_WITH_SIZE(aStatement->qmxMem,
                                    SChar,
                                    QD_MAX_SQL_LENGTH,
                                    &sSqlStr)
             != IDE_SUCCESS);

    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                     "DELETE FROM SYS_TABLE_PARTITIONS_ "
                     "WHERE PARTITION_ID = INTEGER'%"ID_INT32_FMT"'",
                     aPartitionID );
    IDE_TEST(qcg::runDMLforDDL(QC_SMI_STMT(aStatement),
                               sSqlStr,
                               & sRowCnt ) != IDE_SUCCESS);

    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                     "DELETE FROM SYS_PART_LOBS_ "
                     "WHERE PARTITION_ID = INTEGER'%"ID_INT32_FMT"'",
                     aPartitionID );
    IDE_TEST(qcg::runDMLforDDL(QC_SMI_STMT(aStatement),
                               sSqlStr,
                               & sRowCnt ) != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdd::deleteIndexPartitionFromMeta( qcStatement  * aStatement,
                                          UInt           aIndexPartID )
{
/***********************************************************************
 *
 * Description :
 *      PROJ-1502 PARTITIONED DISK TABLE
 *
 * Implementation :
 *
 ***********************************************************************/

    SChar               * sSqlStr;
    vSLong                sRowCnt;

    IDU_LIMITPOINT("qdd::deleteIndexPartitionFromMeta::malloc");
    IDE_TEST(STRUCT_ALLOC_WITH_SIZE(aStatement->qmxMem,
                                    SChar,
                                    QD_MAX_SQL_LENGTH,
                                    &sSqlStr)
             != IDE_SUCCESS);

    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                     "DELETE FROM SYS_INDEX_PARTITIONS_ "
                     "WHERE INDEX_PARTITION_ID = INTEGER'%"ID_INT32_FMT"'",
                     aIndexPartID );
    IDE_TEST(qcg::runDMLforDDL(QC_SMI_STMT(aStatement),
                               sSqlStr,
                               & sRowCnt ) != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdd::deleteViewFromMeta( qcStatement * aStatement,
                                UInt          aViewID )
{
/***********************************************************************
 *
 * Description :
 *      executeDropView �κ��� ȣ��
 *
 * Implementation :
 *      1. SYS_TABLES_, SYS_COLUMNS_, SYS_VIEWS_, SYS_VIEW_PARSE_,
 *         SYS_VIEW_RELATED_ ��Ÿ ���̺��� �ش� ���̺� ���� ����
 *
 ***********************************************************************/

#define IDE_FN "qdd::deleteViewFromMeta"

    SChar               * sSqlStr;
    vSLong sRowCnt;

    IDU_FIT_POINT( "qdd::deleteViewFromMeta::alloc::sSqlStr",
                    idERR_ABORT_InsufficientMemory );

    IDE_TEST(STRUCT_ALLOC_WITH_SIZE(aStatement->qmxMem,
                                    SChar,
                                    QD_MAX_SQL_LENGTH,
                                    &sSqlStr)
             != IDE_SUCCESS);

    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                     "DELETE FROM SYS_TABLES_ "
                     "WHERE TABLE_ID = INTEGER'%"ID_INT32_FMT"'",
                     aViewID );
    IDE_TEST(qcg::runDMLforDDL(QC_SMI_STMT( aStatement ),
                               sSqlStr,
                               & sRowCnt ) != IDE_SUCCESS);

    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                     "DELETE FROM SYS_COLUMNS_ "
                     "WHERE TABLE_ID = INTEGER'%"ID_INT32_FMT"'",
                     aViewID );
    IDE_TEST(qcg::runDMLforDDL(QC_SMI_STMT( aStatement ),
                               sSqlStr,
                               & sRowCnt ) != IDE_SUCCESS);

    // PROJ-1362
    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                     "DELETE FROM SYS_LOBS_ "
                     "WHERE TABLE_ID = INTEGER'%"ID_INT32_FMT"'",
                     aViewID );
    IDE_TEST(qcg::runDMLforDDL(QC_SMI_STMT(aStatement),
                               sSqlStr,
                               & sRowCnt ) != IDE_SUCCESS);

    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                     "DELETE FROM SYS_VIEWS_ "
                     "WHERE VIEW_ID = INTEGER'%"ID_INT32_FMT"'",
                     aViewID);
    IDE_TEST(qcg::runDMLforDDL(QC_SMI_STMT( aStatement ),
                               sSqlStr,
                               & sRowCnt ) != IDE_SUCCESS);

    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                     "DELETE FROM SYS_VIEW_PARSE_ "
                     "WHERE VIEW_ID = INTEGER'%"ID_INT32_FMT"'",
                     aViewID );
    IDE_TEST(qcg::runDMLforDDL(QC_SMI_STMT( aStatement ),
                               sSqlStr,
                               & sRowCnt ) != IDE_SUCCESS);

    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                     "DELETE FROM SYS_VIEW_RELATED_ "
                     "WHERE VIEW_ID = INTEGER'%"ID_INT32_FMT"'",
                     aViewID );
    IDE_TEST(qcg::runDMLforDDL(QC_SMI_STMT( aStatement ),
                               sSqlStr,
                               & sRowCnt ) != IDE_SUCCESS);

    // PROJ-2422 srid
    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                     "DELETE FROM SYS_GEOMETRIES_ "
                     "WHERE TABLE_ID = INTEGER'%"ID_INT32_FMT"'",
                     aViewID );
    IDE_TEST( qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                 sSqlStr,
                                 &sRowCnt ) != IDE_SUCCESS );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qdd::deleteIndicesFromMeta(qcStatement  * aStatement,
                                  qcmTableInfo * aTableInfo)
{
/***********************************************************************
 *
 * Description :
 *      executeDropTable �κ��� ȣ��
 *
 * Implementation :
 *      1. SYS_INDICES_, SYS_INDEX_COLUMNS_ �ش� ���̺� ���� ����
 *
 ***********************************************************************/

#define IDE_FN "qdd::deleteIndicesFromMeta"

    SChar  * sSqlStr;
    vSLong   sRowCnt;
    UInt     i;

    IDU_FIT_POINT( "qdd::deleteIndicesFromMeta::alloc::sSqlStr",
                    idERR_ABORT_InsufficientMemory );

    IDE_TEST(STRUCT_ALLOC_WITH_SIZE(aStatement->qmxMem,
                                    SChar,
                                    QD_MAX_SQL_LENGTH,
                                    &sSqlStr)
             != IDE_SUCCESS);

    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                     "DELETE FROM SYS_INDICES_ "
                     "WHERE TABLE_ID = INTEGER'%"ID_INT32_FMT"'",
                     aTableInfo->tableID );
    IDE_TEST(qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                sSqlStr,
                                & sRowCnt ) != IDE_SUCCESS);

    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                     "DELETE FROM SYS_INDEX_COLUMNS_ "
                     "WHERE TABLE_ID = INTEGER'%"ID_INT32_FMT"'",
                     aTableInfo->tableID );
    IDE_TEST(qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                sSqlStr,
                                & sRowCnt ) != IDE_SUCCESS);

    // PROJ-1502 PARTITIONED DISK TABLE
    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                     "DELETE FROM SYS_PART_INDICES_ "
                     "WHERE TABLE_ID = INTEGER'%"ID_INT32_FMT"'",
                     aTableInfo->tableID );
    IDE_TEST(qcg::runDMLforDDL(QC_SMI_STMT(aStatement),
                               sSqlStr,
                               & sRowCnt ) != IDE_SUCCESS);

    if( aTableInfo->indices != NULL )
    {
        // PROJ-1502 PARTITIONED DISK TABLE
        for( i = 0; i < aTableInfo->indexCount; i++ )
        {
            idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                             "DELETE FROM SYS_PART_KEY_COLUMNS_ "
                             "WHERE PARTITION_OBJ_ID = INTEGER'%"ID_INT32_FMT"'"
                             "AND OBJECT_TYPE = INTEGER'%"ID_INT32_FMT"'",
                             aTableInfo->indices[i].indexId,
                             QCM_INDEX_OBJECT_TYPE );

            IDE_TEST(qcg::runDMLforDDL(QC_SMI_STMT(aStatement),
                                       sSqlStr,
                                       & sRowCnt ) != IDE_SUCCESS);
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qdd::deleteIndicesFromMetaByIndexID(qcStatement * aStatement,
                                           UInt          aIndexID)
{
/***********************************************************************
 *
 * Description :
 *      executeDropIndex �κ��� ȣ��
 *
 * Implementation :
 *      1. SYS_INDICES_, SYS_INDEX_COLUMNS_ �ش� ���̺� ���� ����
 *
 ***********************************************************************/

#define IDE_FN "qdd::deleteIndicesFromMetaByIndexID"

    SChar               * sSqlStr;
    vSLong sRowCnt;

    IDU_FIT_POINT( "qdd::deleteIndicesFromMetaByIndexID::alloc::sSqlStr",
                    idERR_ABORT_InsufficientMemory );

    IDE_TEST(STRUCT_ALLOC_WITH_SIZE(aStatement->qmxMem,
                                    SChar,
                                    QD_MAX_SQL_LENGTH,
                                    &sSqlStr)
             != IDE_SUCCESS);

    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                     "DELETE FROM SYS_INDICES_ "
                     "WHERE INDEX_ID = INTEGER'%"ID_INT32_FMT"'",
                     aIndexID);
    IDE_TEST(qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                sSqlStr,
                                & sRowCnt ) != IDE_SUCCESS);

    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                     "DELETE FROM SYS_INDEX_COLUMNS_ "
                     "WHERE INDEX_ID = INTEGER'%"ID_INT32_FMT"'",
                     aIndexID );
    IDE_TEST(qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                sSqlStr,
                                & sRowCnt ) != IDE_SUCCESS);

    // PROJ-1502 PARTITIONED DISK TABLE
    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                     "DELETE FROM SYS_PART_INDICES_ "
                     "WHERE INDEX_ID = INTEGER'%"ID_INT32_FMT"'",
                     aIndexID );
    IDE_TEST(qcg::runDMLforDDL(QC_SMI_STMT(aStatement),
                               sSqlStr,
                               & sRowCnt ) != IDE_SUCCESS);

    // PROJ-1502 PARTITIONED DISK TABLE
    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                     "DELETE FROM SYS_PART_KEY_COLUMNS_ "
                     "WHERE PARTITION_OBJ_ID = INTEGER'%"ID_INT32_FMT"'"
                     "AND OBJECT_TYPE = INTEGER'%"ID_INT32_FMT"'",
                     aIndexID,
                     QCM_INDEX_OBJECT_TYPE ); /* INDEX */

    IDE_TEST(qcg::runDMLforDDL(QC_SMI_STMT(aStatement),
                               sSqlStr,
                               & sRowCnt ) != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}


IDE_RC qdd::executeDropView(qcStatement * aStatement)
{
/***********************************************************************
 *
 * Description :
 *    DROP VIEW ... �� execution ����
 *
 * Implementation :
 *    1. get table info
 *    2. ��Ÿ ���̺��� view ����
 *    3. ��Ÿ ���̺��� privilege ����
 *    4. related PSM �� invalid ���·� ����
 *    5. related VIEW �� invalid ���·� ����
 *    6. smiTable::dropTable
 *    7. ��Ÿ ĳ������ ����
 *
 ***********************************************************************/

#define IDE_FN "qdd::executeDropView"

    qdDropParseTree    * sParseTree;
    qcmTableInfo       * sInfo;

    sParseTree = (qdDropParseTree *)aStatement->myPlan->parseTree;

    // TASK-2176
    // Table�� ���� Lock�� ȹ���Ѵ�.
    IDE_TEST( qcm::validateAndLockTable(aStatement,
                                        sParseTree->tableHandle,
                                        sParseTree->tableSCN,
                                        SMI_TABLE_LOCK_X)
              != IDE_SUCCESS );

    // BUG-30741 validate�������� sParseTree�� ���س��� tableInfo��
    // ��ȿ���� ������ �����Ƿ� �ٽ� �����´�.
    IDE_TEST( smiGetTableTempInfo( sParseTree->tableHandle,
                                   (void**)&sParseTree->tableInfo )
              != IDE_SUCCESS );
    sInfo = sParseTree->tableInfo;

    IDE_TEST(qdd::deleteViewFromMeta(aStatement, sInfo->tableID)
             != IDE_SUCCESS);

    IDE_TEST(qdpDrop::removePriv4DropTable(aStatement, sInfo->tableID)
             != IDE_SUCCESS);

    /* PROJ-1888 INSTEAD OF TRIGGER drop view Ʈ���ŵ�  drop*/
    IDE_TEST(qdnTrigger::dropTrigger4DropTable(aStatement, sInfo )
             != IDE_SUCCESS);

    // related PSM
    IDE_TEST(qcmProc::relSetInvalidProcOfRelated(
                 aStatement,
                 sParseTree->userID,
                 (SChar *) (sParseTree->objectName.stmtText + sParseTree->objectName.offset),
                 sParseTree->objectName.size,
                 QS_TABLE)
             != IDE_SUCCESS);

    // PROJ-1073 Package
    IDE_TEST(qcmPkg::relSetInvalidPkgOfRelated(
                 aStatement,
                 sParseTree->userID,
                 (SChar *) (sParseTree->objectName.stmtText + sParseTree->objectName.offset),
                 sParseTree->objectName.size,
                 QS_TABLE)
             != IDE_SUCCESS);

    // related VIEW
    IDE_TEST(qcmView::setInvalidViewOfRelated(
                 aStatement,
                 sParseTree->userID,
                 (SChar *) (sParseTree->objectName.stmtText + sParseTree->objectName.offset),
                 sParseTree->objectName.size,
                 QS_TABLE)
             != IDE_SUCCESS);

    // BUG-21387 COMMENT
    IDE_TEST(qdbComment::deleteCommentTable( aStatement,
                                             sParseTree->tableInfo->tableOwnerName,
                                             sParseTree->tableInfo->name )
             != IDE_SUCCESS);

    // PROJ-2223 audit
    IDE_TEST( qcmAudit::deleteObject( aStatement,
                                      sParseTree->tableInfo->tableOwnerID,
                                      sParseTree->tableInfo->name )
              != IDE_SUCCESS );
    
    IDE_TEST( smiTable::dropTable( QC_SMI_STMT( aStatement ),
                                   sInfo->tableHandle,
                                   SMI_TBSLV_DDL_DML )
              != IDE_SUCCESS );

    (void)qdnTrigger::freeTriggerCaches4DropTable( sInfo );

    (void)qcm::destroyQcmTableInfo(sInfo);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}


IDE_RC qdd::deleteObjectFromMetaByUserID( qcStatement * aStatement,
                                          UInt          aUserID )
{
/***********************************************************************
 *
 * Description :
 *      executeDropUserCascade �κ��� ȣ��
 *
 * Implementation :
 *
 *
 ***********************************************************************/

#define IDE_FN "qdd::deleteObjectFromMetaByUserID"

    SChar              * sSqlStr;
    vSLong               sRowCnt;

    IDU_FIT_POINT( "qdd::deleteObjectFromMetaByUserID::alloc::sSqlStr",
                    idERR_ABORT_InsufficientMemory );

    IDE_TEST(STRUCT_ALLOC_WITH_SIZE(aStatement->qmxMem,
                                    SChar,
                                    QD_MAX_SQL_LENGTH,
                                    &sSqlStr)
             != IDE_SUCCESS);

    // SYS_CONSTRAINTS_
    idlOS::snprintf( (SChar*)sSqlStr, QD_MAX_SQL_LENGTH,
                     "DELETE FROM SYS_CONSTRAINTS_ "
                     "WHERE USER_ID = INTEGER'%"ID_INT32_FMT"'",
                     aUserID );

    IDE_TEST(qcg::runDMLforDDL(QC_SMI_STMT( aStatement ),
                               (SChar*)sSqlStr, &sRowCnt) != IDE_SUCCESS);

    // SYS_CONSTRAINT_COLUMNS_
    idlOS::snprintf( (SChar*)sSqlStr, QD_MAX_SQL_LENGTH,
                     "DELETE FROM SYS_CONSTRAINT_COLUMNS_ "
                     "WHERE USER_ID = INTEGER'%"ID_INT32_FMT"'",
                     aUserID);

    IDE_TEST(qcg::runDMLforDDL(QC_SMI_STMT( aStatement ),
                               (SChar*)sSqlStr, &sRowCnt) != IDE_SUCCESS);

    // SYS_INDICES_
    idlOS::snprintf( (SChar*)sSqlStr, QD_MAX_SQL_LENGTH,
                     "DELETE FROM SYS_INDICES_ "
                     "WHERE USER_ID = INTEGER'%"ID_INT32_FMT"'",
                     aUserID );

    IDE_TEST(qcg::runDMLforDDL(QC_SMI_STMT( aStatement ),
                               (SChar*)sSqlStr, &sRowCnt) != IDE_SUCCESS);

    // PROJ-1502 PARTITIONED DISK TABLE
    // SYS_PART_INDICES_
    idlOS::snprintf( (SChar*)sSqlStr, QD_MAX_SQL_LENGTH,
                     "DELETE FROM SYS_PART_INDICES_ "
                     "WHERE USER_ID = INTEGER'%"ID_INT32_FMT"'",
                     aUserID );

    IDE_TEST(qcg::runDMLforDDL(QC_SMI_STMT( aStatement ),
                               (SChar*)sSqlStr, &sRowCnt) != IDE_SUCCESS);

    // SYS_INDEX_COLUMNS_
    idlOS::snprintf( (SChar*)sSqlStr, QD_MAX_SQL_LENGTH,
                     "DELETE FROM SYS_INDEX_COLUMNS_ "
                     "WHERE USER_ID = INTEGER'%"ID_INT32_FMT"'",
                     aUserID );

    IDE_TEST(qcg::runDMLforDDL(QC_SMI_STMT( aStatement ),
                               (SChar*)sSqlStr, &sRowCnt) != IDE_SUCCESS);

    // SYS_TABLES_
    idlOS::snprintf( (SChar*)sSqlStr, QD_MAX_SQL_LENGTH,
                     "DELETE FROM SYS_TABLES_ "
                     "WHERE USER_ID = INTEGER'%"ID_INT32_FMT"'",
                     aUserID );

    IDE_TEST(qcg::runDMLforDDL(QC_SMI_STMT( aStatement ),
                               (SChar*)sSqlStr, &sRowCnt) != IDE_SUCCESS);

    // PROJ-1502 PARTITIONED DISK TABLE
    // SYS_PART_TABLES_
    idlOS::snprintf( (SChar*)sSqlStr, QD_MAX_SQL_LENGTH,
                     "DELETE FROM SYS_PART_TABLES_ "
                     "WHERE USER_ID = INTEGER'%"ID_INT32_FMT"'",
                     aUserID );

    IDE_TEST(qcg::runDMLforDDL(QC_SMI_STMT( aStatement ),
                               (SChar*)sSqlStr, &sRowCnt) != IDE_SUCCESS);

    // SYS_COLUMNS_
    idlOS::snprintf( (SChar*)sSqlStr, QD_MAX_SQL_LENGTH,
                     "DELETE FROM SYS_COLUMNS_ "
                     "WHERE USER_ID = INTEGER'%"ID_INT32_FMT"'",
                     aUserID);

    IDE_TEST(qcg::runDMLforDDL(QC_SMI_STMT( aStatement ),
                               (SChar*)sSqlStr, &sRowCnt) != IDE_SUCCESS);

    // BUG-32286
    // PROJ-2002 Column Security
    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                     "DELETE FROM SYS_ENCRYPTED_COLUMNS_ "
                     "WHERE USER_ID = INTEGER'%"ID_INT32_FMT"'",
                     aUserID );
    IDE_TEST(qcg::runDMLforDDL(QC_SMI_STMT(aStatement),
                               sSqlStr,
                               & sRowCnt ) != IDE_SUCCESS);

    // PROJ-1502 PARTITIONED DISK TABLE
    // SYS_PART_KEY_COLUMNS_
    idlOS::snprintf( (SChar*)sSqlStr, QD_MAX_SQL_LENGTH,
                     "DELETE FROM SYS_PART_KEY_COLUMNS_ "
                     "WHERE USER_ID = INTEGER'%"ID_INT32_FMT"'",
                     aUserID );

    IDE_TEST(qcg::runDMLforDDL(QC_SMI_STMT( aStatement ),
                               (SChar*)sSqlStr, &sRowCnt) != IDE_SUCCESS);

    // PROJ-1362
    // SYS_LOBS_
    idlOS::snprintf( (SChar*)sSqlStr, QD_MAX_SQL_LENGTH,
                     "DELETE FROM SYS_LOBS_ "
                     "WHERE USER_ID = INTEGER'%"ID_INT32_FMT"'",
                     aUserID);

    IDE_TEST(qcg::runDMLforDDL(QC_SMI_STMT(aStatement),
                               (SChar*)sSqlStr, &sRowCnt) != IDE_SUCCESS);

    // PROJ-1502 PARTITIONED DISK TABLE
    // SYS_PART_LOBS_
    idlOS::snprintf( (SChar*)sSqlStr, QD_MAX_SQL_LENGTH,
                     "DELETE FROM SYS_PART_LOBS_ "
                     "WHERE USER_ID = INTEGER'%"ID_INT32_FMT"'",
                     aUserID );

    IDE_TEST(qcg::runDMLforDDL(QC_SMI_STMT( aStatement ),
                               (SChar*)sSqlStr, &sRowCnt) != IDE_SUCCESS);

    // SYS_VIEWS_
    idlOS::snprintf( (SChar*)sSqlStr, QD_MAX_SQL_LENGTH,
                     "DELETE FROM SYS_VIEWS_ "
                     "WHERE USER_ID = INTEGER'%"ID_INT32_FMT"'",
                     aUserID );

    IDE_TEST(qcg::runDMLforDDL(QC_SMI_STMT( aStatement ),
                               (SChar*)sSqlStr, &sRowCnt) != IDE_SUCCESS);

    // SYS_VIEW_PARSE_
    idlOS::snprintf( (SChar*)sSqlStr, QD_MAX_SQL_LENGTH,
                     "DELETE FROM SYS_VIEW_PARSE_ "
                     "WHERE USER_ID = INTEGER'%"ID_INT32_FMT"'",
                     aUserID);

    IDE_TEST(qcg::runDMLforDDL(QC_SMI_STMT( aStatement ),
                               (SChar*)sSqlStr, &sRowCnt) != IDE_SUCCESS);

    // SYS_VIEW_RELATED_
    idlOS::snprintf( (SChar*)sSqlStr, QD_MAX_SQL_LENGTH,
                     "DELETE FROM SYS_VIEW_RELATED_ "
                     "WHERE USER_ID = INTEGER'%"ID_INT32_FMT"'",
                     aUserID );

    IDE_TEST(qcg::runDMLforDDL(QC_SMI_STMT( aStatement ),
                               (SChar*)sSqlStr, &sRowCnt) != IDE_SUCCESS);

    /* SYS_MATERIALIZED_VIEWS_ */
    idlOS::snprintf( (SChar*)sSqlStr, QD_MAX_SQL_LENGTH,
                     "DELETE FROM SYS_MATERIALIZED_VIEWS_ "
                     "WHERE USER_ID = INTEGER'%"ID_INT32_FMT"'",
                     aUserID );

    IDE_TEST( qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                 (SChar*)sSqlStr, &sRowCnt ) != IDE_SUCCESS );

    /* PROJ-1812 ROLE
     * remove GRANTEE_ID = aUserID from SYS_USER_ROLES_ */
    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                     "DELETE FROM SYS_USER_ROLES_ "
                     "WHERE GRANTEE_ID = INTEGER'%"ID_INT32_FMT"'",
                     aUserID );

    IDE_TEST(qcg::runDMLforDDL(QC_SMI_STMT( aStatement ),
                               (SChar*)sSqlStr, &sRowCnt) != IDE_SUCCESS);

    /* PROJ-1812 ROLE
     * remove GRANTOR_ID = aUserID from SYS_USER_ROLES_ */
    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                     "DELETE FROM SYS_USER_ROLES_ "
                     "WHERE GRANTOR_ID = INTEGER'%"ID_INT32_FMT"'",
                     aUserID );

    IDE_TEST(qcg::runDMLforDDL(QC_SMI_STMT( aStatement ),
                               (SChar*)sSqlStr, &sRowCnt) != IDE_SUCCESS);
    
    // SYS_GRANT_SYSTEM_, SYS_GRANT_OBJECT_,
    IDE_TEST(qdpDrop::removePriv4DropUser(aStatement, aUserID) != IDE_SUCCESS);

    // SYS_SYNONYMS_
    IDE_TEST(qcmSynonym::cascadeRemove(aStatement, aUserID)
             != IDE_SUCCESS);

    // SYS_TBS_USERS_
    idlOS::snprintf( (SChar*)sSqlStr, QD_MAX_SQL_LENGTH,
                     "DELETE FROM SYS_TBS_USERS_ "
                     "WHERE USER_ID = INTEGER'%"ID_INT32_FMT"'",
                     aUserID );

    IDE_TEST(qcg::runDMLforDDL(QC_SMI_STMT( aStatement ),
                               (SChar*)sSqlStr, &sRowCnt) != IDE_SUCCESS);

    // DBA_USERS_
    // BUG-41230 SYS_USERS_ => DBA_USERS_
    idlOS::snprintf( (SChar*)sSqlStr, QD_MAX_SQL_LENGTH,
                     "DELETE FROM DBA_USERS_ "
                     "WHERE USER_ID = INTEGER'%"ID_INT32_FMT"'",
                     aUserID );

    IDE_TEST(qcg::runDMLforDDL(QC_SMI_STMT( aStatement ),
                               (SChar*)sSqlStr, &sRowCnt) != IDE_SUCCESS);

    /* PROJ-2207 Password policy support */
    idlOS::snprintf( (SChar*)sSqlStr, QD_MAX_SQL_LENGTH,
                     "DELETE FROM SYS_PASSWORD_HISTORY_ "
                     "WHERE USER_ID = INTEGER'%"ID_INT32_FMT"'",
                     aUserID );

    IDE_TEST(qcg::runDMLforDDL(QC_SMI_STMT( aStatement ),
                               (SChar*)sSqlStr, &sRowCnt) != IDE_SUCCESS);

    // PROJ-1685
    idlOS::snprintf( (SChar*)sSqlStr, QD_MAX_SQL_LENGTH,
                     "DELETE FROM SYS_LIBRARIES_ "
                     "WHERE USER_ID = INTEGER'%"ID_INT32_FMT"'",
                     aUserID );

    IDE_TEST(qcg::runDMLforDDL(QC_SMI_STMT( aStatement ),
                               (SChar*)sSqlStr, &sRowCnt) != IDE_SUCCESS);

    // PROJ-2422 srid
    // SYS_GEOMETRIES_
    idlOS::snprintf( (SChar*)sSqlStr, QD_MAX_SQL_LENGTH,
                     "DELETE FROM SYS_GEOMETRIES_ "
                     "WHERE USER_ID = INTEGER'%"ID_INT32_FMT"'",
                     aUserID );

    IDE_TEST( qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                 (SChar*)sSqlStr, &sRowCnt ) != IDE_SUCCESS );

    /* BUG-35445 Check Constraint, Function-Based Index���� ��� ���� Function�� ����/���� ���� */
    IDE_TEST( qcmProc::relRemoveRelatedToConstraintByUserID(
                    aStatement,
                    aUserID )
              != IDE_SUCCESS );

    IDE_TEST( qcmProc::relRemoveRelatedToIndexByUserID(
                    aStatement,
                    aUserID )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qdd::executeDropTableOwnedByUser(
    qcStatement          * aStatement,
    qcmTableInfoList     * aTableInfoList,
    qcmPartitionInfoList * aPartInfoList,
    qdIndexTableList     * aIndexTables )
{
/***********************************************************************
 *
 * Description :
 *    qdd::executeDropUserCascade�� ���� ȣ��
 *    ���� ������ ���̺�, ť ���̺�, ��, MATERIALIZED VIEW(���� Table/View) ��ü�� ����
 *
 * Implementation :
 *    if ( tableType�� �� �̸� )
 *        �� ��ü�� ����
 *    else if ( tableType�� ť ���̺� �̸� )
 *        1.1 ť ���̺� ��ü�� ����
 *        1.2 ť ������ ��ü�� ����
 *    else
 *        1.1 ���̺� unique �ε����� ���� ���
 *            referential constraint�� ã�Ƽ� ����
 *        1.2 Trigger ��ü�� SYS_TRIGGERS_ ..�� ����
 *            ���̺�� ���õ� Trigger ���� ����
 *        1.3 ���̺� ��ü�� ����
 *
 *
 ***********************************************************************/
#define IDE_FN "qdd::executeDropTableOwnedByUser"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qdd::executeDropTableOwnedByUser"))

    qcmTableInfo          * sTableInfo;
    qcmIndex              * sIndexInfo = NULL;
    SChar                   sSequenceName[QC_MAX_SEQ_NAME_LEN + 1];
    void                  * sSequenceHandle;
    qcmSequenceInfo         sSequenceInfo;
    UInt                    i;
    qcmPartitionInfoList  * sPartInfoList = NULL;
    qcmTableInfo          * sPartInfo;
    qdIndexTableList      * sIndexTable;
    
    qciArgDropQueue         sArgDropQueue;

    sTableInfo = aTableInfoList->tableInfo;

    if ( ( sTableInfo->tableType == QCM_VIEW ) ||
         ( sTableInfo->tableType == QCM_MVIEW_VIEW ) )
    {
        /* PROJ-1888 INSTEAD OF TRIGGER drop view Ʈ���ŵ�  drop*/
        IDE_TEST(qdnTrigger::dropTrigger4DropTable(aStatement, sTableInfo )
                 != IDE_SUCCESS);

        // related PSM
        IDE_TEST(qcmProc::relSetInvalidProcOfRelated(
                     aStatement,
                     sTableInfo->tableOwnerID,
                     sTableInfo->name,
                     idlOS::strlen((SChar*)sTableInfo->name),
                     QS_TABLE) != IDE_SUCCESS);

        // PROJ-1073 Package
        IDE_TEST(qcmPkg::relSetInvalidPkgOfRelated(
                     aStatement,
                     sTableInfo->tableOwnerID,
                     sTableInfo->name,
                     idlOS::strlen((SChar*)sTableInfo->name),
                     QS_TABLE) != IDE_SUCCESS);

        // related VIEW
        IDE_TEST(qcmView::setInvalidViewOfRelated(
                     aStatement,
                     sTableInfo->tableOwnerID,
                     sTableInfo->name,
                     idlOS::strlen((SChar*)sTableInfo->name),
                     QS_TABLE)
                 != IDE_SUCCESS);

        // BUG-21387 COMMENT
        IDE_TEST(qdbComment::deleteCommentTable(
                     aStatement,
                     sTableInfo->tableOwnerName,
                     sTableInfo->name)
                 != IDE_SUCCESS);

        // PROJ-2223 audit
        IDE_TEST( qcmAudit::deleteObject(
                      aStatement,
                      sTableInfo->tableOwnerID,
                      sTableInfo->name )
                  != IDE_SUCCESS );
    
        //-----------------------------------------
        // �� ��ü�� ����
        //-----------------------------------------

        IDE_TEST( smiTable::dropTable( QC_SMI_STMT( aStatement ),
                                       sTableInfo->tableHandle,
                                       SMI_TBSLV_DDL_DML )
                  != IDE_SUCCESS );
    }
    else if ( sTableInfo->tableType == QCM_QUEUE_TABLE )
    {
        //-----------------------------------------
        // ť ���̺�� ���õ� ��Ÿ ���̺��� ���ڵ� ����
        //-----------------------------------------

        // DELETE
        IDE_TEST(qdd::deleteConstraintsFromMeta(aStatement, sTableInfo->tableID)
                 != IDE_SUCCESS);

        // INDEX
        IDE_TEST(qdd::deleteIndicesFromMeta(aStatement, sTableInfo)
                 != IDE_SUCCESS);

        // related VIEW
        IDE_TEST(qcmView::setInvalidViewOfRelated(
                     aStatement,
                     sTableInfo->tableOwnerID,
                     sTableInfo->name,
                     idlOS::strlen((SChar*)sTableInfo->name),
                     QS_TABLE)
                 != IDE_SUCCESS);

        //-----------------------------------------
        // ť ���̺� ��ü�� ����
        //-----------------------------------------

        IDE_TEST( smiTable::dropTable( QC_SMI_STMT( aStatement ),
                                       sTableInfo->tableHandle,
                                       SMI_TBSLV_DDL_DML )
                  != IDE_SUCCESS );

        //-----------------------------------------
        // ť �������� ���õ� ��Ÿ ���̺��� ���ڵ� ����
        //-----------------------------------------

        idlOS::snprintf(sSequenceName,
                        ID_SIZEOF(sSequenceName),
                        "%s_NEXT_MSG_ID",
                        sTableInfo->name);

        IDE_TEST( qcm::getSequenceInfoByName( QC_SMI_STMT( aStatement ),
                                              sTableInfo->tableOwnerID,
                                              (UChar*)sSequenceName,
                                              idlOS::strlen(sSequenceName),
                                              &sSequenceInfo,
                                              &sSequenceHandle )
                  != IDE_SUCCESS);

        IDE_TEST_RAISE(sSequenceInfo.sequenceType != QCM_QUEUE_SEQUENCE,
                       ERR_META_CRASH);

        IDE_TEST(qdd::deleteTableFromMeta(aStatement, sSequenceInfo.sequenceID)
                 != IDE_SUCCESS);

        IDE_TEST(qdpDrop::removePriv4DropTable(aStatement, sSequenceInfo.sequenceID)
                 != IDE_SUCCESS);

        //-----------------------------------------
        // ť ������ ��ü�� ����
        //-----------------------------------------

        IDE_TEST( smiTable::dropSequence( QC_SMI_STMT( aStatement ),
                                          sSequenceHandle )
                  != IDE_SUCCESS );

        //-----------------------------------------
        // MM�� QUEUE���� ������ ������û (commit�� ������)
        //-----------------------------------------

        // drop queue funcPtr
        sArgDropQueue.mTableID   = sTableInfo->tableID;
        sArgDropQueue.mMmSession = aStatement->session->mMmSession;
        IDE_TEST( qcg::mDropQueueFuncPtr( (void *)&sArgDropQueue )
                  != IDE_SUCCESS );
    }
    else // TABLE, MView Table
    {
        /* PROJ-1407 Temporary table
         * session temporary table�� �����ϴ� ��� DDL�� �� �� ����.
         * �տ��� session table�� ���������� �̸� Ȯ���Ͽ���.
         * session table�� ������ ����.*/
        IDE_DASSERT( qcuTemporaryObj::existSessionTable( sTableInfo )
                     == ID_FALSE );

        //-----------------------------------------
        // ���̺� unique �ε����� ���� ���
        // referential constraint�� ã�Ƽ� ����
        //-----------------------------------------

        for (i=0; i<sTableInfo->uniqueKeyCount; i++)
        {
            sIndexInfo = sTableInfo->uniqueKeys[i].constraintIndex;

            IDE_TEST( qcm::getChildKeysForDelete( aStatement,
                                                  QC_EMPTY_USER_ID,
                                                  sIndexInfo,
                                                  sTableInfo,
                                                  ID_FALSE /* aDropTablespace */,
                                                  & (aTableInfoList->childInfo) )
                      != IDE_SUCCESS );

            IDE_TEST( deleteFKConstraints( aStatement,
                                           sTableInfo,
                                           aTableInfoList->childInfo,
                                           ID_FALSE /* aDropTablespace */ )
                      != IDE_SUCCESS );
        }

        // PROJ-1502 PARTITIONED DISK TABLE
        if( sTableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
        {
            for( sPartInfoList = aPartInfoList;
                 sPartInfoList != NULL;
                 sPartInfoList = sPartInfoList->next )
            {
                sPartInfo = sPartInfoList->partitionInfo;

                IDE_TEST( qdd::dropTablePartition( aStatement,
                                                   sPartInfo,
                                                   ID_FALSE, /* aIsDropTablespace */
                                                   ID_FALSE )
                          != IDE_SUCCESS );
            }
            
            // PROJ-1624 non-partitioned index
            for ( sIndexTable = aIndexTables;
                  sIndexTable != NULL;
                  sIndexTable = sIndexTable->next )
            {
                IDE_TEST( qdx::dropIndexTable( aStatement,
                                               sIndexTable,
                                               ID_FALSE /* aIsDropTablespace */ )
                          != IDE_SUCCESS );
            }
        }

        //-----------------------------------------
        // ���̺�� ���õ� ��Ÿ ���̺��� ���ڵ� ����
        //-----------------------------------------

        // DELETE
        IDE_TEST(qdd::deleteConstraintsFromMeta(aStatement, sTableInfo->tableID)
                 != IDE_SUCCESS);

        // INDEX
        IDE_TEST(qdd::deleteIndicesFromMeta(aStatement,
                                            sTableInfo)
                 != IDE_SUCCESS);

        // TRIGGER : ��Ÿ ���̺�, ��ü
        IDE_TEST(qdnTrigger::dropTrigger4DropTable(aStatement, sTableInfo )
                 != IDE_SUCCESS);


        // related VIEW
        IDE_TEST(qcmView::setInvalidViewOfRelated(
                     aStatement,
                     sTableInfo->tableOwnerID,
                     sTableInfo->name,
                     idlOS::strlen((SChar*)sTableInfo->name),
                     QS_TABLE)
                 != IDE_SUCCESS);

        // BUG-21387 COMMENT
        IDE_TEST(qdbComment::deleteCommentTable(
                     aStatement,
                     sTableInfo->tableOwnerName,
                     sTableInfo->name)
                 != IDE_SUCCESS);

        // PROJ-2223 audit
        IDE_TEST( qcmAudit::deleteObject(
                      aStatement,
                      sTableInfo->tableOwnerID,
                      sTableInfo->name )
                  != IDE_SUCCESS );

        // PROJ-2264 Dictionary table
        // SYS_COMPRESSION_TABLES_ ���� ���� ���ڵ带 �����Ѵ�.
        IDE_TEST( qdd::deleteCompressionTableSpecFromMeta( aStatement,
                                                           sTableInfo->tableID )
                  != IDE_SUCCESS );

        //-----------------------------------------
        // ���̺� ��ü�� ����
        //-----------------------------------------

        IDE_TEST( smiTable::dropTable( QC_SMI_STMT( aStatement ),
                                       sTableInfo->tableHandle,
                                       SMI_TBSLV_DDL_DML )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_META_CRASH);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCM_META_CRASH));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qdd::executeDropIndexOwnedByUser(
    qcStatement             * aStatement,
    qcmIndexInfoList        * aIndexInfoList,
    qcmPartitionInfoList    * aPartInfoList)
{
/***********************************************************************
 *
 * Description :
 *    qdd::executeDropUserCascade�� ���� ȣ��
 *    ���� ������ �ε��� ��ü�� ����
 *
 * Implementation :
 *    1. unique �ε����� ���� ��� referential constraint�� ã�Ƽ� ����
 *    2. �ε��� ��ü�� ����
 *
 ***********************************************************************/
#define IDE_FN "qdd::executeDropIndexOwnedByUser"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qdd::executeDropIndexOwnedByUser"))

    qcmTableInfo          * sTableInfo;
    qcmIndex              * sIndexInfo = NULL;
    idBool                  sIsPrimary = ID_FALSE;
    UInt                    i;
    qcmPartitionInfoList  * sPartInfoList = NULL;
    void                  * sIndexHandle;

    sTableInfo = aIndexInfoList->tableInfo;

    sPartInfoList = aPartInfoList;

    for ( i = 0; i < sTableInfo->indexCount; i++ )
    {
        if ( sTableInfo->indices[i].indexId ==
             aIndexInfoList->indexID )
        {
            sIndexInfo = &sTableInfo->indices[i];

            // fix BUG-17427
            sIndexHandle = (void *)smiGetTableIndexByID(
                sTableInfo->tableHandle,
                sIndexInfo->indexId);

            break;
        }
        else
        {
            // Nothing To Do!!
        }
    }

    IDE_TEST_RAISE( sIndexInfo == NULL, ERR_NOT_EXIST_INDEX);

    //-----------------------------------------
    // unique �ε����� ���� ��� referential constraint�� ã�Ƽ� ����
    //-----------------------------------------

    if( sIndexInfo->isUnique == ID_TRUE )
    {
        IDE_TEST( qcm::getChildKeysForDelete( aStatement,
                                              QC_EMPTY_USER_ID,
                                              sIndexInfo,
                                              sTableInfo,
                                              ID_FALSE /* aDropTablespace */,
                                              & (aIndexInfoList->childInfo) )
                  != IDE_SUCCESS );

        IDE_TEST( deleteFKConstraints( aStatement,
                                       sTableInfo,
                                       aIndexInfoList->childInfo,
                                       ID_FALSE /* aDropTablespace */ )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    //-----------------------------------------
    // �ε��� ��ü�� ����
    //-----------------------------------------

    // PROJ-1502 PARTITIONED DISK TABLE
    if( sTableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
    {
        if ( sTableInfo->primaryKey != NULL )
        {
            if ( sTableInfo->primaryKey->indexId == sIndexInfo->indexId )
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
        if ( ( sIndexInfo->indexPartitionType == QCM_NONE_PARTITIONED_INDEX ) ||
             ( sIsPrimary == ID_TRUE ) )
        {
            IDE_TEST_RAISE( aIndexInfoList->isPartitionedIndex == ID_TRUE,
                            ERR_META_CRASH );

            IDE_TEST( qdx::dropIndexTable( aStatement,
                                           & aIndexInfoList->indexTable,
                                           ID_FALSE /* aIsDropTablespace */ )
                      != IDE_SUCCESS );
        }
        else
        {
            // Nothing to do.
        }
        
        if ( ( sIndexInfo->indexPartitionType != QCM_NONE_PARTITIONED_INDEX ) ||
             ( sIsPrimary == ID_TRUE ) )
        {
            IDE_TEST_RAISE( aIndexInfoList->isPartitionedIndex == ID_FALSE,
                            ERR_META_CRASH );
            
            IDE_TEST( dropIndexPartitions( aStatement,
                                           sPartInfoList,
                                           sIndexInfo->indexId,
                                           ID_TRUE  /* aIsCascade */,
                                           ID_FALSE /* aIsDropTablespace */ )
                      != IDE_SUCCESS );
        }
        else
        {
            // Nothing to do.
        }
    }

    IDE_TEST( smiTable::dropIndex(QC_SMI_STMT( aStatement ),
                                  sTableInfo->tableHandle,
                                  sIndexHandle,
                                  SMI_TBSLV_DDL_DML )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_NOT_EXIST_INDEX);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCM_NOT_EXISTS_INDEX));
    }
    IDE_EXCEPTION(ERR_META_CRASH);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCM_META_CRASH));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qdd::deleteFKConstraints( qcStatement     * aStatement,
                                 qcmTableInfo    * aTableInfo,
                                 qcmRefChildInfo * aChildInfo,
                                 idBool            aDropTablespace )
{
/***********************************************************************
 *
 * Description : �ε����� ����� �ٸ� ���̺� �����ϴ�
 *               foreign key ���� ����.
 *
 * Implementation :
 *
 ***********************************************************************/
#define IDE_FN "qdd::deleteFKConstraints"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qdd::deleteFKConstraints"))

    qcmRefChildInfo      * sChildInfo;
    qcmTableInfo         * sNewTableInfo;
    qcmTableInfo         * sOldTableInfo;
    UInt                   sOldTableID;
    UInt                   sRefTableID;
    qmsTableRef          * sChildTableRef;  // BUG-28049
    qmsPartitionRef      * sPartitionRef     = NULL;
    qcmTableInfo         * sNewPartitionInfo = NULL;

    smiTBSLockValidType    sTBSLockValidType = SMI_TBSLV_DDL_DML;

    /* BUG-43299 Drop Tablespace�� ���, SMI_TBSLV_DROP_TBS�� ����ؾ� �Ѵ�. */
    if ( aDropTablespace == ID_TRUE )
    {
        sTBSLockValidType = SMI_TBSLV_DROP_TBS;
    }
    else
    {
        /* Nothingn to do */
    }

    sChildInfo = aChildInfo;

    // BUG-28049
    while( sChildInfo != NULL )
    {
        sChildTableRef = sChildInfo->childTableRef;

        // childTable�� lock�� ȹ���Ѵ�.
        sOldTableInfo = sChildTableRef->tableInfo;
        sOldTableID   = sChildTableRef->tableInfo->tableID;
        sRefTableID   = sChildInfo->foreignKey->referencedTableID;

        // ������ foreign key�� �����Ѵ�.
        IDE_TEST(qdd::deleteFKConstraintsFromMeta(
                     aStatement,
                     sOldTableID,
                     sRefTableID )
                 != IDE_SUCCESS );

        // BUG-15880
        if ( aTableInfo == sOldTableInfo )
        {
            // skip self-reference
        }
        else
        {
            IDE_TEST( smiTable::touchTable( QC_SMI_STMT(aStatement),
                                            sOldTableInfo->tableHandle,
                                            sTBSLockValidType )
                      != IDE_SUCCESS );

            // BUG-16422
            IDE_TEST(qcmTableInfoMgr::makeTableInfo(
                         aStatement,
                         sOldTableInfo,
                         & sNewTableInfo)
                     != IDE_SUCCESS);

            sChildTableRef->tableInfo = sNewTableInfo;

            for ( sPartitionRef = sChildTableRef->partitionRef;
                  sPartitionRef != NULL;
                  sPartitionRef = sPartitionRef->next )
            {
                IDE_TEST( smiTable::touchTable( QC_SMI_STMT(aStatement),
                                                sPartitionRef->partitionHandle,
                                                sTBSLockValidType )
                          != IDE_SUCCESS );

                IDE_TEST( qcmTableInfoMgr::makeTableInfo( aStatement,
                                                          sPartitionRef->partitionInfo,
                                                          & sNewPartitionInfo )
                          != IDE_SUCCESS );

                sPartitionRef->partitionInfo = sNewPartitionInfo;
            }
        }

        sChildInfo = sChildInfo->next;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qdd::executeDropTriggerOwnedByUser(
    qcStatement         * aStatement,
    qcmTriggerInfoList  * aTriggerInfoList)
{
/***********************************************************************
 *
 * Description :
 *    qdd::executeDropUserCascade�� ���� ȣ��
 *    ���� ������ Ʈ���� ��ü�� ����
 *
 * Implementation :
 *    1. ���õ� PSM�� Invalid ���� ����
 *    2. Ʈ���� ��ü ����
 *
 ***********************************************************************/
#define IDE_FN "qdd::executeDropTriggerOwnedByUser"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qdd::executeDropTriggerOwnedByUser"))

    void               * sTriggerHandle;

    //---------------------------------------
    // Trigger Object ����
    //---------------------------------------

    sTriggerHandle = (void*) smiGetTable( aTriggerInfoList->triggerOID );

    IDE_TEST( smiObject::dropObject( QC_SMI_STMT( aStatement ),
                                     sTriggerHandle )
              != IDE_SUCCESS );

    IDE_TEST( qcmTrigger::removeMetaInfo( aStatement,
                                          aTriggerInfoList->triggerOID )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qdd::executeDropSequenceOwnedByUser(
    qcStatement         * aStatement,
    qcmSequenceInfoList * aSequenceInfoList)
{
/***********************************************************************
 *
 * Description :
 *    BUG-16980
 *    qdd::executeDropUserCascade�� ���� ȣ��
 *    ���� ������ ������ ��ü�� ����
 *
 * Implementation :
 *    ������ ��ü�� ����
 *
 ***********************************************************************/
#define IDE_FN "qdd::executeDropSequenceOwnedByUser"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qdd::executeDropSequenceOwnedByUser"))

    qcmSequenceInfo    * sSequenceInfo;
    SLong                sStartValue;
    SLong                sCurrIncrementValue;
    SLong                sCurrMinValue;
    SLong                sCurrMaxValue;
    SLong                sCurrCacheValue;
    UInt                 sOption;
    SChar                sSeqTableNameStr[ QC_MAX_OBJECT_NAME_LEN + 1 ];
    qcNamePosition       sSeqTableName;

    // PROJ-2365 sequence table ����
    qcmTableInfo       * sSeqTableInfo = NULL;
    void               * sSeqTableHandle;
    smSCN                sSeqTableSCN;

    sSequenceInfo = & aSequenceInfoList->sequenceInfo;

    IDE_TEST(smiTable::getSequence( sSequenceInfo->sequenceHandle,
                                    &sStartValue,
                                    &sCurrIncrementValue,
                                    &sCurrCacheValue,
                                    &sCurrMaxValue,
                                    &sCurrMinValue,
                                    &sOption)
             != IDE_SUCCESS);
    
    // PROJ-2365 sequence table
    // �̹� ������ sequence table�� ���� ������ alter�� ���� ����
    if ( ( sOption & SMI_SEQUENCE_TABLE_MASK ) == SMI_SEQUENCE_TABLE_TRUE )
    {
        // make sequence table name : SEQ1$SEQ
        IDE_TEST_RAISE( idlOS::strlen(sSequenceInfo->name) + QDS_SEQ_TABLE_SUFFIX_LEN
                        > QC_MAX_OBJECT_NAME_LEN,
                        ERR_SEQUENCE_NAME_LEN );

        idlOS::snprintf( sSeqTableNameStr, QC_MAX_OBJECT_NAME_LEN + 1,
                         "%s%s",
                         sSequenceInfo->name,
                         QDS_SEQ_TABLE_SUFFIX_STR );
        
        sSeqTableName.stmtText = sSeqTableNameStr;
        sSeqTableName.offset   = 0;
        sSeqTableName.size     = idlOS::strlen(sSeqTableNameStr);
        
        IDE_TEST_RAISE( qcm::getTableHandleByName(
                            QC_SMI_STMT(aStatement),
                            sSequenceInfo->sequenceOwnerID,
                            (UChar*) sSeqTableName.stmtText,
                            sSeqTableName.size,
                            &sSeqTableHandle,
                            &sSeqTableSCN ) != IDE_SUCCESS,
                        ERR_NOT_EXIST_TABLE );
        
        IDE_TEST( smiGetTableTempInfo( sSeqTableHandle,
                                       (void**)&sSeqTableInfo )
                  != IDE_SUCCESS );;
        
        // sequence table�� DDL lock
        IDE_TEST( qcm::validateAndLockTable( aStatement,
                                             sSeqTableHandle,
                                             sSeqTableSCN,
                                             SMI_TABLE_LOCK_X )
                  != IDE_SUCCESS );
        
        // if specified tables is replicated, the ERR_DDL_WITH_REPLICATED_TABLE
        IDE_TEST_RAISE( sSeqTableInfo->replicationCount > 0,
                        ERR_DDL_WITH_REPLICATED_TABLE );

        //proj-1608:replicationCount�� 0�� �� recovery count�� �׻� 0�̾�� ��
        IDE_DASSERT( sSeqTableInfo->replicationRecoveryCount == 0 );
        
        // drop sequence table
        IDE_TEST( qds::dropSequenceTable( aStatement,
                                          sSeqTableInfo )
                  != IDE_SUCCESS );
        
        IDE_TEST( qcmTableInfoMgr::destroyTableInfo( aStatement,
                                                     sSeqTableInfo )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }
    
    IDE_TEST(qdd::deleteTableFromMeta(aStatement, sSequenceInfo->sequenceID)
             != IDE_SUCCESS);

    IDE_TEST(qdpDrop::removePriv4DropTable(aStatement, sSequenceInfo->sequenceID)
             != IDE_SUCCESS);

    // PROJ-2223 audit
    IDE_TEST( qcmAudit::deleteObject( aStatement,
                                      sSequenceInfo->sequenceOwnerID,
                                      sSequenceInfo->name )
              != IDE_SUCCESS );
    
    //-----------------------------------------
    // ������ ��ü�� ����
    //-----------------------------------------

    IDE_TEST( smiTable::dropSequence( QC_SMI_STMT( aStatement ),
                                      sSequenceInfo->sequenceHandle )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NOT_EXIST_TABLE )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QCV_NOT_EXISTS_TABLE, "" ) );
    }
    IDE_EXCEPTION( ERR_DDL_WITH_REPLICATED_TABLE )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDD_DDL_WITH_REPLICATED_TBL ) );
    }
    IDE_EXCEPTION( ERR_SEQUENCE_NAME_LEN )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qdd::executeDropSequence",
                                  "sequence name is too long" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qdd::executeDropProcOwnedByUser(
    qcStatement         * aStatement,
    UInt                  aUserID,
    qcmProcInfoList     * aProcInfoList)
{
/***********************************************************************
 *
 * Description :
 *    qdd::executeDropUserCascade�� ���� ȣ��
 *
 * Implementation :
 *
 *    1. ���� ������ Procecure or Function ������ ��Ÿ ���̺��� ����
 *    2. ���� ������ Procecure or Function ��ü�� ����
 *
 ***********************************************************************/
#define IDE_FN "qdd::executeDropProcOwnedByUser"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qdd::executeDropProcOwnedByUser"))


        IDE_TEST( qcmProc::remove( aStatement,
                                   aProcInfoList->procOID,
                                   aUserID,
                                   aProcInfoList->procName,
                                   idlOS::strlen( aProcInfoList->procName ),
                                   ID_FALSE /* aPreservePrivInfo */ )
                  != IDE_SUCCESS );

    IDE_TEST( smiObject::dropObject( QC_SMI_STMT( aStatement ) ,
                                     smiGetTable( aProcInfoList->procOID ) )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

/***********************************************************************
 *
 * Description :
 *    DROP MATERIALIZED VIEW ... �� execution ����
 *
 * Implementation :
 *    1. lock & get table, view information
 *    2. Meta Table���� Constraint ����
 *    3. Meta Table���� Index ����
 *    4. Meta Table���� Table, Column ����
 *    5. Meta Table���� Object Privilege ���� (Table)
 *    6. Trigger ���� (Meta Table, Object)
 *    7. related VIEW �� Invalid ���·� ���� (Table)
 *    8. Constraint�� ���õ� Procedure�� ���� ������ ���� (Table)
 *    9. Index�� ���õ� Procedure�� ���� ������ ���� (Table)
 *   10. smiTable::dropTable (Table)
 *   11. Meta Table���� View ����
 *   12. Meta Table���� Object Privilege ���� (View)
 *   13. Trigger ���� (Meta Table, Object)
 *   14. related PSM �� Invalid ���·� ���� (View)
 *   15. related VIEW �� Invalid ���·� ���� (View)
 *   16. smiTable::dropTable (View)
 *   17. Meta Table���� Materialized View ����
 *   18. Meta Cache���� ���� (Table)
 *   19. Meta Cache���� ���� (View)
 *
 ***********************************************************************/
IDE_RC qdd::executeDropMView( qcStatement * aStatement )
{
    qdDropParseTree      * sParseTree       = NULL;
    qcmTableInfo         * sTableInfo       = NULL;
    UInt                   i;
    qcmPartitionInfoList * sPartInfoList    = NULL;
    qcmPartitionInfoList * sOldPartInfoList = NULL;
    qcmTableInfo         * sPartInfo        = NULL;
    idBool                 sIsPartitioned   = ID_FALSE;
    qdIndexTableList     * sIndexTable;
    qcmColumn            * sColumn          = NULL;
    UInt                   sMViewID;
    qcmTableInfo         * sViewInfo        = NULL;

    sParseTree = (qdDropParseTree *)aStatement->myPlan->parseTree;

    /* lock & get table, view */
    IDE_TEST( qcm::validateAndLockTable(
                    aStatement,
                    sParseTree->tableHandle,
                    sParseTree->tableSCN,
                    SMI_TABLE_LOCK_X )
              != IDE_SUCCESS );

    sTableInfo = sParseTree->tableInfo;

    IDE_TEST( qcm::validateAndLockTable(
                    aStatement,
                    sParseTree->mviewViewHandle,
                    sParseTree->mviewViewSCN,
                    SMI_TABLE_LOCK_X )
              != IDE_SUCCESS );

    sViewInfo = sParseTree->mviewViewInfo;

    /* PROJ-1502 PARTITIONED DISK TABLE */
    if ( sTableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
    {
        sIsPartitioned = ID_TRUE;

        /* �Ľ�Ʈ������ ��Ƽ�� ���� ����Ʈ�� �����´�. */
        sOldPartInfoList = sParseTree->partInfoList;

        /* ��� ��Ƽ�ǿ� LOCK(X) */
        IDE_TEST( qcmPartition::validateAndLockPartitionInfoList( aStatement,
                                                                  sOldPartInfoList,
                                                                  SMI_TBSLV_DDL_DML, // TBS Validation �ɼ�
                                                                  SMI_TABLE_LOCK_X,
                                                                  smiGetDDLLockTimeOut((QC_SMI_STMT(aStatement))->getTrans()))
                  != IDE_SUCCESS );
        
        // PROJ-1624 non-partitioned index
        IDE_TEST( qdx::validateAndLockIndexTableList( aStatement,
                                                      sParseTree->oldIndexTables,
                                                      SMI_TBSLV_DDL_DML, // TBS Validation �ɼ�
                                                      SMI_TABLE_LOCK_X,
                                                      smiGetDDLLockTimeOut((QC_SMI_STMT(aStatement))->getTrans()))
                  != IDE_SUCCESS );
    }
    else
    {
        sIsPartitioned = ID_FALSE;
    }

    /* PROJ-1723 [MDW/INTEGRATOR] Altibase Plugin ����
     * DDL Statement Text�� �α�
     */
    if ( QCU_DDL_SUPPLEMENTAL_LOG == 1 )
    {
        IDE_TEST( qciMisc::writeDDLStmtTextLog( aStatement,
                                                sTableInfo->tableOwnerID,
                                                sTableInfo->name )
                  != IDE_SUCCESS );

        IDE_TEST( qci::mManageReplicationCallback.mWriteTableMetaLog(
                                         aStatement,
                                         smiGetTableId( sTableInfo->tableHandle ),
                                         0 )
                  != IDE_SUCCESS );
    }

    /* PROJ-1502 PARTITIONED DISK TABLE */
    if ( sIsPartitioned == ID_TRUE )
    {
        /* ���̺� ��Ƽ�� ������ŭ �ݺ� */
        for ( sPartInfoList = sOldPartInfoList;
              sPartInfoList != NULL;
              sPartInfoList = sPartInfoList->next )
        {
            sPartInfo = sPartInfoList->partitionInfo;

            IDE_TEST( dropTablePartition( aStatement,
                                          sPartInfo,
                                          ID_FALSE, /* aIsDropTablespace */
                                          ID_FALSE )
                      != IDE_SUCCESS );
        }
        
        // PROJ-1624 non-partitioned index
        for ( sIndexTable = sParseTree->oldIndexTables;
              sIndexTable != NULL;
              sIndexTable = sIndexTable->next )
        {
            IDE_TEST( qdx::dropIndexTable( aStatement,
                                           sIndexTable,
                                           ID_FALSE /* aIsDropTablespace */ )
                      != IDE_SUCCESS );
        }
    }
    else
    {
        /* Nothing to do */
    }

    IDE_TEST( qdd::deleteConstraintsFromMeta( aStatement, sTableInfo->tableID )
              != IDE_SUCCESS );

    IDE_TEST( qdd::deleteIndicesFromMeta( aStatement, sTableInfo )
              != IDE_SUCCESS );

    IDE_TEST( qdd::deleteTableFromMeta( aStatement, sTableInfo->tableID )
              != IDE_SUCCESS );

    IDE_TEST( qdpDrop::removePriv4DropTable( aStatement, sTableInfo->tableID )
              != IDE_SUCCESS );

    /* PROJ-1359 */
    IDE_TEST( qdnTrigger::dropTrigger4DropTable( aStatement, sTableInfo )
              != IDE_SUCCESS );

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
                                              sTableInfo->tableOwnerName,
                                              sTableInfo->name)
              != IDE_SUCCESS );

    // PROJ-2223 audit
    IDE_TEST( qcmAudit::deleteObject( aStatement,
                                      sTableInfo->tableOwnerID,
                                      sTableInfo->name )
              != IDE_SUCCESS );
    
    IDE_TEST( smiTable::dropTable( QC_SMI_STMT( aStatement ),
                                   sTableInfo->tableHandle,
                                   SMI_TBSLV_DDL_DML )
              != IDE_SUCCESS );

    /* drop mview view */
    IDE_TEST( qdd::deleteViewFromMeta( aStatement, sViewInfo->tableID )
              != IDE_SUCCESS );

    IDE_TEST( qdpDrop::removePriv4DropTable( aStatement, sViewInfo->tableID )
              != IDE_SUCCESS);

    /* PROJ-1888 INSTEAD OF TRIGGER */
    IDE_TEST( qdnTrigger::dropTrigger4DropTable( aStatement, sViewInfo )
              != IDE_SUCCESS );

    IDE_TEST( qcmProc::relSetInvalidProcOfRelated(
                        aStatement,
                        sParseTree->userID,
                        (SChar *)(sParseTree->objectName.stmtText +
                                  sParseTree->objectName.offset),
                        sParseTree->objectName.size,
                        QS_TABLE )
              != IDE_SUCCESS );

    IDE_TEST( qcmProc::relSetInvalidProcOfRelated(
                        aStatement,
                        sParseTree->userID,
                        sParseTree->mviewViewName,
                        idlOS::strlen( sParseTree->mviewViewName ),
                        QS_TABLE )
              != IDE_SUCCESS );

    // BUG-37193
    IDE_TEST( qcmPkg::relSetInvalidPkgOfRelated(
                       aStatement,
                       sParseTree->userID,
                       (SChar *)(sParseTree->objectName.stmtText +
                                 sParseTree->objectName.offset),
                       sParseTree->objectName.size,
                       QS_TABLE )
              != IDE_SUCCESS );

    // PROJ-1073 Package
    IDE_TEST( qcmPkg::relSetInvalidPkgOfRelated(
                        aStatement,
                        sParseTree->userID,
                        sParseTree->mviewViewName,
                        idlOS::strlen( sParseTree->mviewViewName ),
                        QS_TABLE )
              != IDE_SUCCESS );

    IDE_TEST( qcmView::setInvalidViewOfRelated(
                        aStatement,
                        sParseTree->userID,
                        (SChar *)(sParseTree->objectName.stmtText +
                                  sParseTree->objectName.offset),
                        sParseTree->objectName.size,
                        QS_TABLE )
              != IDE_SUCCESS );

    IDE_TEST( qcmView::setInvalidViewOfRelated(
                        aStatement,
                        sParseTree->userID,
                        sParseTree->mviewViewName,
                        idlOS::strlen( sParseTree->mviewViewName ),
                        QS_TABLE )
              != IDE_SUCCESS );

    IDE_TEST( smiTable::dropTable( QC_SMI_STMT( aStatement ),
                                   sViewInfo->tableHandle,
                                   SMI_TBSLV_DDL_DML )
              != IDE_SUCCESS );

    /* drop materialized view */
    IDE_TEST( qcmMView::getMViewID(
                        QC_SMI_STMT( aStatement ),
                        sParseTree->userID,
                        (sParseTree->objectName.stmtText +
                         sParseTree->objectName.offset),
                        (UInt)sParseTree->objectName.size,
                        &sMViewID )
              != IDE_SUCCESS );

    IDE_TEST( qdd::deleteMViewFromMeta( aStatement, sMViewID )
              != IDE_SUCCESS );

    /* To Fix BUG-12034
     * SM�� ���̻� ������ ������ ���� ���� QP���� �����ϰ� �ִ�
     * �޸𸮵��� �������ش�.
     */
    (void)qdnTrigger::freeTriggerCaches4DropTable( sTableInfo );

    /* PROJ-2002 Column Security
     * ���� �÷� ������ ���� ��⿡ �˸���.
     */
    for ( i = 0; i < sTableInfo->columnCount; i++ )
    {
        sColumn = & sTableInfo->columns[i];

        if ( (sColumn->basicInfo->module->flag & MTD_ENCRYPT_TYPE_MASK)
             == MTD_ENCRYPT_TYPE_TRUE )
        {
            (void)qcsModule::unsetColumnPolicy(
                                sTableInfo->tableOwnerName,
                                sTableInfo->name,
                                sColumn->name );
        }
        else
        {
            /* Nothing to do */
        }
    }

    (void)qcm::destroyQcmTableInfo( sTableInfo );

    if ( sIsPartitioned == ID_TRUE )
    {
        (void)qcmPartition::destroyQcmPartitionInfoList( sOldPartInfoList );
        
        for ( sIndexTable = sParseTree->oldIndexTables;
              sIndexTable != NULL;
              sIndexTable = sIndexTable->next )
        {
            (void)qcm::destroyQcmTableInfo(sIndexTable->tableInfo);
        }
    }

    (void)qcm::destroyQcmTableInfo( sViewInfo );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description :
 *    executeDropMView�κ��� ȣ��
 *
 * Implementation :
 *    1. SYS_MATERIALIZED_VIEWS_
 *       ��Ÿ ���̺��� �ش� ���̺� ���� ����
 *
 ***********************************************************************/
IDE_RC qdd::deleteMViewFromMeta( qcStatement * aStatement,
                                 UInt          aMViewID )
{
    SChar  * sSqlStr;
    vSLong   sRowCnt;

    IDU_LIMITPOINT( "qdd::deleteMViewFromMeta::malloc");
    IDE_TEST( STRUCT_ALLOC_WITH_SIZE( aStatement->qmxMem,
                                      SChar,
                                      QD_MAX_SQL_LENGTH,
                                      &sSqlStr )
              != IDE_SUCCESS);

    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                     "DELETE FROM SYS_MATERIALIZED_VIEWS_ "
                     "WHERE MVIEW_ID = INTEGER'%"ID_INT32_FMT"'",
                     aMViewID );
    IDE_TEST( qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                 sSqlStr,
                                 &sRowCnt )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdd::deleteCompressionTableSpecFromMetaByDicTableID(
                qcStatement * aStatement,
                UInt          aDicTableID )
{
/***********************************************************************
 *
 * Description :
 *      PROJ-2264
 *      executeDropIndexInTBS �κ��� ȣ��
 *      Data table-Dictionary table �� ���� ������ �����Ѵ�.
 *
 * Implementation :
 *      1. SYS_COMPRESSION_TABLES_ ��Ÿ ���̺��� ���� ���� ����
 *
 ***********************************************************************/

    SChar         * sSqlStr;
    vSLong          sRowCnt;

    IDE_TEST(STRUCT_ALLOC_WITH_SIZE(aStatement->qmxMem,
                                    SChar,
                                    QD_MAX_SQL_LENGTH,
                                    &sSqlStr)
             != IDE_SUCCESS);

    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                     "DELETE FROM SYS_COMPRESSION_TABLES_ WHERE DIC_TABLE_ID = "
                     "INTEGER'%"ID_INT32_FMT"'",
                     aDicTableID );

    IDE_TEST(qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                sSqlStr,
                                &sRowCnt ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdd::deleteCompressionTableSpecFromMeta( qcStatement * aStatement,
                                                UInt          aDataTableID )
{
/***********************************************************************
 *
 * Description :
 *      PROJ-2264
 *      executeDropIndexInTBS �κ��� ȣ��
 *      Data table-Dictionary table �� ���� ������ �����Ѵ�.
 *
 * Implementation :
 *      1. SYS_COMPRESSION_TABLES_ ��Ÿ ���̺��� ���� ���� ����
 *
 ***********************************************************************/

    SChar         * sSqlStr;
    vSLong          sRowCnt;

    IDE_TEST(STRUCT_ALLOC_WITH_SIZE(aStatement->qmxMem,
                                    SChar,
                                    QD_MAX_SQL_LENGTH,
                                    &sSqlStr)
             != IDE_SUCCESS);

    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                     "DELETE FROM SYS_COMPRESSION_TABLES_ WHERE TABLE_ID = "
                     "INTEGER'%"ID_INT32_FMT"'",
                     aDataTableID );

    IDE_TEST(qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                sSqlStr,
                                &sRowCnt ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdd::executeDropPkgOwnedByUser(
    qcStatement         * aStatement,
    UInt                  aUserID,
    qcmPkgInfoList      * aPkgInfoList)
{
/***********************************************************************
 *
 * Description :
 *    qdd::executeDropUserCascade�� ���� ȣ��
 *
 * Implementation :
 *
 *    1. ���� ������ Package ������ ��Ÿ ���̺��� ����
 *    2. ���� ������ Package ��ü�� ����
 *
 ***********************************************************************/
       
    IDE_TEST( qcmPkg::remove( aStatement,
                              aPkgInfoList->pkgOID,
                              aUserID,
                              aPkgInfoList->pkgName,
                              idlOS::strlen( aPkgInfoList->pkgName ),
                              (qsObjectType)( aPkgInfoList->pkgType ),
                              ID_FALSE /* aPreservePrivInfo */ )
              != IDE_SUCCESS );

    IDE_TEST( smiObject::dropObject( QC_SMI_STMT( aStatement ) ,
                                     smiGetTable( aPkgInfoList->pkgOID ) )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
