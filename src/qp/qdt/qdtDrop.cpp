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
 * $Id: qdtDrop.cpp 88963 2020-10-19 03:33:18Z jake.jang $
 **********************************************************************/

#include <idl.h>
#include <smiTableSpace.h>
#include <qcg.h>
#include <qcmTableSpace.h>
#include <qcmProc.h>
#include <qcmView.h>
#include <qcmMView.h>
#include <qdd.h>
#include <qdm.h>
#include <qdtDrop.h>
#include <qdpPrivilege.h>
#include <qdpDrop.h>
#include <qdnTrigger.h>
#include <qdbComment.h>
#include <qdx.h>
#include <qcuTemporaryObj.h>
#include <qcmAudit.h>
#include <qcmPkg.h>
#include <qdpRole.h>

IDE_RC qdtDrop::validate(qcStatement * aStatement)
{
/***********************************************************************
 *
 * Description :
 *
 *    DROP TABLESPACE ... �� validation ����
 *
 * Implementation :
 *
 *    1. ���� �˻� qdpPrivilege::checkDDLAlterTableSpacePriv()
 *    2. ����� ���̺����̽����� �����ͺ��̽� ���� �̹� �����ϴ���
 *       ��Ÿ �˻�
 *    3. ���̺����̽��� SYSTEM TABLESPACE(DATA,TEMP,UNDO)�̸� ����
 *    4. INCLUDING CONTENTS ��� ���ο� ���� ���� �˻�
 *    if ( INCLUDING CONTENTS ����� ��� )
 *    {
 *      if ( CASCADE CONSTRAINTS ����� ��� )
 *      {
 *        // nothing to do
 *      }
 *      else // CASCADE CONSTRAINTS ������� ���� ���
 *      {
 *        ����� ���̺����̽��� ���� primary key,unique key �� ���õ�
 *        referential constraint ���� �ٸ� ���̺����̽��� �����ϸ� ���� �߻�
 *      }
 *    }
 *    else // INCLUDING CONTENTS ������� ���� ���
 *    {
 *      ���̺����̽��� ���� ��ü�� �ϳ� �̻� �����ϸ� ���� �߻�
 *    }
 *
 ***********************************************************************/

#define IDE_FN "qdtDrop::validate"

    UInt                    i;
    qdDropTBSParseTree    * sParseTree;
    smiTableSpaceAttr       sTBSAttr;

    // BUG-28049
    qcmRefChildInfo       * sRefChildInfo;  
    qcmRefChildInfo       * sTempRefChildInfo;
    
    qcmTableInfo          * sTableInfo;
    qcmTableInfo          * sTablePartInfo;
    qcmTableInfo          * sIndexPartInfo;
    qcmTableInfoList      * sTableInfoList = NULL;
    qcmTableInfoList      * sTableInfoListHead = NULL;
    qcmTableInfoList      * sTablePartInfoList = NULL;
    qcmIndex              * sIndexInfo = NULL;
    qcmIndexInfoList      * sIndexInfoList = NULL;
    qcmIndexInfoList      * sIndexPartInfoList = NULL;
    qcmIndexInfoList      * sIndexInfoListHead = NULL;
    idBool                  sExist;
    idBool                  sFound = ID_FALSE;
    UInt                    sTableIDByPartID = 0;
    
    sParseTree = (qdDropTBSParseTree *)aStatement->myPlan->parseTree;

    //-----------------------------------------
    // ���� �˻�
    //-----------------------------------------

    IDE_TEST( qdpRole::checkDDLDropTableSpacePriv(
                  aStatement,
                  QCG_GET_SESSION_USER_ID( aStatement ) )
              != IDE_SUCCESS );

    //-----------------------------------------
    // ���̺����̽� �̸����� ��Ÿ���̺� �˻�
    //-----------------------------------------

    IDE_TEST ( qcmTablespace::getTBSAttrByName(
                   aStatement,
                   sParseTree->TBSAttr->mName,
                   sParseTree->TBSAttr->mNameLength,
                   &sTBSAttr) != IDE_SUCCESS );

    //-----------------------------------------
    // �⺻������ �����Ǵ� ���̺����̽����� �˻�(MEMORY,DATA,UNDO)
    //-----------------------------------------
    IDE_TEST_RAISE( smiTableSpace::isSystemTableSpace( sTBSAttr.mID )
                    == ID_TRUE,
                    ERR_NO_DROP_SYSTEM_TBS);

    /* To Fix BUG-17292 [PROJ-1548] Tablespace DDL�� Tablespace�� X�� ��´�
     * Tablespace�� Lock�� ���� �ʰ� Tablespace���� Table�� ��ȸ�ϰ� �Ǹ�
     * �� ���̿� ���ο� Table�� ���ܳ� �� �ִ�.
     * �̷��� ������ �̿��� �����ϱ� ����
     * Tablespace�� X Lock�� ���� ��� Tablespace Validation/Execution�� ���� */
    IDE_TEST( smiValidateAndLockTBS(
                  QC_SMI_STMT( aStatement ),
                  sTBSAttr.mID,
                  SMI_TBS_LOCK_EXCLUSIVE,
                  SMI_TBSLV_DROP_TBS, // TBS Validation �ɼ�
                  smiGetDDLLockTimeOut((QC_SMI_STMT(aStatement))->getTrans()))
                  != IDE_SUCCESS );

    if ( ( sParseTree->flag & QDT_DROP_INCLUDING_CONTENTS_MASK )
         == QDT_DROP_INCLUDING_CONTENTS_TRUE )
    {
        //-----------------------------------------
        // (1) DROP TABLESPACE tbs_name INCLUDING CONTENTS CASCADE CONSTRAINTS;
        //-----------------------------------------

        if ( ( sParseTree->flag & QDT_DROP_CASCADE_CONSTRAINTS_MASK )
             == QDT_DROP_CASCADE_CONSTRAINTS_TRUE )
        {
            // Nothing To Do
            // referential constraints�� �ٸ� ���̺����̽��� �����ϴ���
            // validate �� �˻��� �ʿ� ���� execute �� delete.
            IDE_TEST( qcmTablespace::findTableInfoListInTBS(
                          aStatement,
                          sTBSAttr.mID,
                          ID_TRUE,
                          &sTableInfoListHead) != IDE_SUCCESS );

            IDE_TEST( qcmTablespace::findIndexInfoListInTBS(
                          aStatement,
                          sTBSAttr.mID,
                          ID_TRUE,
                          &sIndexInfoListHead) != IDE_SUCCESS );
        }
        else
        {

            //-----------------------------------------
            // (2) DROP TABLESPACE tbs_name INCLUDING CONTENTS;
            //-----------------------------------------

            // ���̺����̽��� ���� ���̺��� ã�Ƽ�  sTableInfoList�� �޾� ��.
            IDE_TEST( qcmTablespace::findTableInfoListInTBS(
                          aStatement,
                          sTBSAttr.mID,
                          ID_TRUE,
                          &sTableInfoListHead) != IDE_SUCCESS );

            sTableInfoList = sTableInfoListHead;

            while ( sTableInfoList != NULL )
            {
                sTableInfo = sTableInfoList->tableInfo;

                // ���̺��� primary key, unique key �� ���õ�
                // referential integrity constraints ��
                // �ٸ� ���̺����̽��� ���� �ϴ���  check
                for (i=0; i<sTableInfo->uniqueKeyCount; i++)
                {
                    sIndexInfo = sTableInfo->uniqueKeys[i].constraintIndex;

                    // BUG-28049
                    IDE_TEST( qcm::getChildKeys(aStatement,
                                                sIndexInfo,
                                                sTableInfo,
                                                & sRefChildInfo)
                              != IDE_SUCCESS );

                    sTempRefChildInfo = sRefChildInfo;
                    
                    while ( sTempRefChildInfo != NULL )
                    {
                        IDE_TEST_RAISE(
                            sTempRefChildInfo->childTableRef->tableInfo->TBSID !=
                            sTBSAttr.mID,
                            ERR_REFERENTIAL_CONSTRAINT_EXIST );
                        
                        sTempRefChildInfo = sTempRefChildInfo->next;
                    }
                }

                sTableInfoList = sTableInfoList->next;
            }

            // ���̺����̽��� ���� �ε����� ã�Ƽ� sIndexInfoList�� �޾� ��.
            IDE_TEST( qcmTablespace::findIndexInfoListInTBS(
                          aStatement,
                          sTBSAttr.mID,
                          ID_TRUE,
                          &sIndexInfoListHead) != IDE_SUCCESS );

            sIndexInfoList = sIndexInfoListHead;

            while ( sIndexInfoList != NULL )
            {
                sTableInfo = sIndexInfoList->tableInfo;

                sIndexInfo = NULL;
                for ( i = 0; i < sTableInfo->indexCount; i++ )
                {
                    if ( sTableInfo->indices[i].indexId ==
                         sIndexInfoList->indexID )
                    {
                        sIndexInfo = &sTableInfo->indices[i];
                        break;
                    }
                }
                IDE_TEST_RAISE( sIndexInfo == NULL, ERR_NOT_EXIST_INDEX);

                // unique key �� ���õ�
                // referential integrity constraints ��
                // �ٸ� ���̺����̽��� ���� �ϴ���  check
                if ( sIndexInfo->isUnique == ID_TRUE )
                {
                    // BUG-28049
                    IDE_TEST( qcm::getChildKeys(aStatement,
                                                sIndexInfo,
                                                sTableInfo,
                                                & sRefChildInfo)
                              != IDE_SUCCESS );

                    sTempRefChildInfo = sRefChildInfo;
                    
                    while ( sTempRefChildInfo != NULL )
                    {
                        IDE_TEST_RAISE(
                            sTempRefChildInfo->childTableRef->tableInfo->TBSID !=
                            sTBSAttr.mID,
                            ERR_REFERENTIAL_CONSTRAINT_EXIST );
                        
                        sTempRefChildInfo = sTempRefChildInfo->next;
                    }
                }
                sIndexInfoList = sIndexInfoList->next;
            }
        }

        // PROJ-1502 PARTITIONED DISK TABLE
        // ���̺����̽��� Partitioned Table�� �������� �ʰ�,
        // Table_Partition�� �����Ѵٸ� �ش� ���̺����̽��� ������ �� ����.
        IDE_TEST( qcmTablespace::findTablePartInfoListInTBS(
                      aStatement,
                      sTBSAttr.mID,
                      &sTablePartInfoList) != IDE_SUCCESS );

        while ( sTablePartInfoList != NULL )
        {
            sTablePartInfo = sTablePartInfoList->tableInfo;

            IDE_TEST( qcmPartition::getTableIDByPartitionID(
                          QC_SMI_STMT(aStatement),
                          sTablePartInfo->partitionID,
                          & sTableIDByPartID )
                      != IDE_SUCCESS );

            sFound = ID_FALSE;

            sTableInfoList = sTableInfoListHead;
            while( sTableInfoList != NULL )
            {
                sTableInfo = sTableInfoList->tableInfo;

                if( sTableInfo->tableID == sTableIDByPartID )
                {
                    sFound = ID_TRUE;
                    break;
                }
                sTableInfoList = sTableInfoList->next;

            }
            IDE_TEST_RAISE( sFound == ID_FALSE,
                            ERR_PART_TABLE_IN_DIFFERENT_TBS );

            sTablePartInfoList = sTablePartInfoList->next;
        }

        // PROJ-1502 PARTITIONED DISK TABLE
        // ���̺����̽��� Partitioned Table�̳� Partitioned Index�� �������� �ʰ�,
        // Index Partition�� �����Ѵٸ� �ش� ���̺����̽��� ������ �� ����.
        IDE_TEST( qcmTablespace::findIndexPartInfoListInTBS(
                      aStatement,
                      sTBSAttr.mID,
                      &sIndexPartInfoList) != IDE_SUCCESS );

        while ( sIndexPartInfoList != NULL )
        {
            sIndexPartInfo = sIndexPartInfoList->tableInfo;

            sFound = ID_FALSE;

            sTableInfoList = sTableInfoListHead;
            while( sTableInfoList != NULL )
            {
                sTableInfo = sTableInfoList->tableInfo;

                if( sTableInfo->tableID == sIndexPartInfo->tableID )
                {
                    sFound = ID_TRUE;
                    break;
                }
                sTableInfoList = sTableInfoList->next;
            }

            if( sFound == ID_FALSE )
            {
                sIndexInfoList = sIndexInfoListHead;
                while( sIndexInfoList != NULL )
                {
                    if( sIndexInfoList->indexID == sIndexPartInfoList->indexID )
                    {
                        sFound = ID_TRUE;
                        break;
                    }
                    /*
                     * BUG-24515 : �ε��� ��Ƽ�Ǹ� �����ϴ� ���̺����̽� ������
                     *             ������ ����մϴ�.
                     */
                    sIndexInfoList = sIndexInfoList->next;
                }

                IDE_TEST_RAISE( sFound == ID_FALSE,
                                ERR_PART_INDEX_IN_DIFFERENT_TBS );
            }

            sIndexPartInfoList = sIndexPartInfoList->next;
        }
    }
    else
    {
        //-----------------------------------------
        // (3) DROP TABLESPACE tbs_name;
        //-----------------------------------------

        // ���̺����̽��� ���� ��ü�� �ִ��� �˻�
        IDE_TEST( qcmTablespace::existObject( aStatement,
                                              sTBSAttr.mID,
                                              &sExist )
                  != IDE_SUCCESS );

        // ���̺����̽��� ���� ��ü�� �ϳ��� ������ ���� ���
        IDE_TEST_RAISE( sExist == ID_TRUE, ERR_OBJECT_EXIST );
    }

    sParseTree->TBSAttr->mID   = sTBSAttr.mID;
    sParseTree->TBSAttr->mType = sTBSAttr.mType;

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_NO_DROP_SYSTEM_TBS)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDT_NO_DROP_SYSTEM_TBS));
    }
    IDE_EXCEPTION(ERR_REFERENTIAL_CONSTRAINT_EXIST);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDT_REFERENTIAL_CONSTRAINT_EXIST));
    }
    IDE_EXCEPTION(ERR_OBJECT_EXIST);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDT_OBJECT_EXIST));
    }
    IDE_EXCEPTION(ERR_NOT_EXIST_INDEX)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCM_NOT_EXISTS_INDEX));
    }
    IDE_EXCEPTION(ERR_PART_TABLE_IN_DIFFERENT_TBS)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDT_PART_TABLE_IN_DIFFERENT_TBS));
    }
    IDE_EXCEPTION(ERR_PART_INDEX_IN_DIFFERENT_TBS)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDT_PART_INDEX_IN_DIFFERENT_TBS));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qdtDrop::execute(qcStatement * aStatement)
{
/***********************************************************************
 *
 * Description :
 *    DROP TABLESPACE ... �� execution ����
 *
 * Implementation :
 *    if ( INCLUDING CONTENTS ����� ��� )
 *    {
 *        1. ���̺����̽��� ���� ���̺��� ã�Ƽ� sTableInfoList�� �޾� ��.
 *        while()
 *        {
 *            executeDropTableInTBS() ȣ��
 *            : ������ ���̺� ���ؼ� ���̺� ��ü�� qp meta������ ����
 *        }
 *        2. MView View�� Materialized View�� �����ϰ�, MVIew View List�� ����
 *        3. ���̺����̽� ���� �ε����� �Ҽӵ� ���̺��� ã�Ƽ� sIndexInfoList�� �޾� ��.
 *        while()
 *        {
 *            executeDropIndexInTBS() ȣ��
 *            : ������ �ε����� ���ؼ� �ε��� ��ü�� qp meta������ ����
 *        }
 *    }
 *    else // INCLUDING CONTENTS ������� ���� ���
 *    {
 *        // nothing to do
 *    }
 *    4. ���̺����̽� ��ü�� ����
 *    if ( INCLUDING CONTENTS ����� ��� )
 *    {
 *        5. ���̺����̽� ���� �ε����� �Ҽӵ� ���̺� ���ؼ� new cached meta�� ����
 *           : new cached meta�� ����ٰ� ������ ���(ABORT����) ����ó���� ����
 *           : sStage�� 1�� ������.
 *        6. ���̺����̽� ���� ���̺��� ������ Trigger cached meta�� qcmTriggerInfo�� ����
 *        7. MVIew View�� Trigger cached meta�� ����
 *        8. ���̺����̽� ���� �ε����� �Ҽӵ� ���̺� ���ؼ� old cached meta�� ����
 *        9. ���̺����̽� ���� ���̺� ���ؼ� cached meta�� ����
 *       10. MVIew View�� cached meta�� ����
 *    }
 *    else // INCLUDING CONTENTS ������� ���� ���
 *    {
 *        // nothing to do
 *    }
 *
 * Attention :
 *    ���� ���� ���� �� 5. 6. 7. 8, 9, 10�� ������ �ſ� �߿� ��.
 *
 * < DDL�� DROP EXECUTION ���� �ݵ�� ���Ѿ� �� ��Ģ 2����>
 *
 * (1) ��ü��, ��ü�� ���õ� qp meta ���̺��� ���ڵ带 ���� ��
 *     QP���� �����ϰ� �ִ� cached meta �� free�ؾ� �Ѵ�.
 *     ������,
 *         ��ü��, qp meta ���̺��� ���ڵ带 ���� �� ����
 *         sm ��⿡�� log�� ����Ƿ�, �߰��� ���и� �ϴ��� ������ �����ϳ�
 *         cached meta�� �ѹ� free�Ǹ� ������ �����.
 *         ���� ��ü�� qp meta ���̺��� ���ڵ带 ��� ���������� ���� �� ������
 *         cached meta�� free�� �ش�.
 *         �׸��� cached meta�� ������ ���� ���� ���ٰ� ������ �ϸ�
 *         ���� ������ ���� FATAL ������ ���ϵǾ� ������ ����ȴ�.
 *
 * (2) ��ü�� ���õ� qp meta���̺���� ���ڵ带 ������ ��
 *     �ݵ�� ���� ������ ������ �ؾ� �Ѵ�.
 *     ���� ���,
 *         qdd::executeDropTable������
 *             delete SYS_CONSTRAINTS_
 *             delete SYS_TABLES_
 *         ������� qp meta���̺��� �����Ͽ� ���ڵ带 �����ϰ�
 *         qdt::executeDropTableInTBS ������
 *             delete SYS_TABLES_
 *             delete SYS_CONSTRAINTS_
 *         ������� qp meta���̺��� �����Ͽ� ���ڵ带 �����ϰ�
 *
 *         �ٸ� ���ǿ��� ���� �� �Լ��� ���ÿ� ȣ��� ���
 *         dead lock�� �߻��ϰ� �ȴ�.
 *
 ***********************************************************************/

#define IDE_FN "qdtDrop::execute"

    qdDropTBSParseTree    * sParseTree;
    smiTrans              * sTrans;
    qcmTableInfo          * sTableInfo;
    qcmTableInfoList      * sTableInfoList = NULL;
    qcmTableInfoList      * sMViewOfRelatedInfoList = NULL;
    qcmTableInfoList      * sTempTableInfoList = NULL;
    qcmIndexInfoList      * sIndexInfoList = NULL;
    qcmIndexInfoList      * sTempIndexInfoList = NULL;
    smiTouchMode            sTouchMode = SMI_ALL_NOTOUCH;
    UInt                    sPartTableCount = 0;
    UInt                    i = 0;
    qcmPartitionInfoList ** sTablePartInfoList = NULL;
    qcmPartitionInfoList ** sIndexPartInfoList = NULL;
    qcmPartitionInfoList  * sTempPartInfoList = NULL;
    qdIndexTableList     ** sIndexTableList = NULL;
    qdIndexTableList      * sIndexTable = NULL;

    sParseTree = (qdDropTBSParseTree *)aStatement->myPlan->parseTree;

    sTrans = (QC_SMI_STMT( aStatement ))->getTrans();

    // To Fix BUG-17292
    //        [PROJ-1548] Tablespace DDL�� Tablespace�� X�� ��´�
    //
    // Tablespace�� Lock�� ���� �ʰ� Tablespace���� Table�� ��ȸ�ϰ� �Ǹ�
    // �� ���̿� ���ο� Table�� ���ܳ� �� �ִ�.
    //
    // �̷��� ������ �̿��� �����ϱ� ����
    // Tablespace�� X Lock�� ���� ��� Tablespace Validation/Execution�� ����
    IDE_TEST( smiValidateAndLockTBS(
                  QC_SMI_STMT( aStatement ),
                  sParseTree->TBSAttr->mID,
                  SMI_TBS_LOCK_EXCLUSIVE,
                  SMI_TBSLV_DROP_TBS, // TBS Validation �ɼ�
                  smiGetDDLLockTimeOut((QC_SMI_STMT(aStatement))->getTrans()))
                  != IDE_SUCCESS );

    // INCLUDING CONTENTS ������ ����� ���
    // ���̺����̽� ���� ��� ��ü���� �����Ѵ�.
    if ( ( sParseTree->flag & QDT_DROP_INCLUDING_CONTENTS_MASK )
         == QDT_DROP_INCLUDING_CONTENTS_TRUE )
    {
        // ���̺����̽��� ���� ���̺��� ã�Ƽ� sTableInfoList�� �޾� ��.
        IDE_TEST( qcmTablespace::findTableInfoListInTBS(
                      aStatement,
                      sParseTree->TBSAttr->mID,
                      ID_FALSE,
                      &sTableInfoList) != IDE_SUCCESS );

        /* PROJ-1407 Temporary table
         * session temporary table�� �����ϴ� ��� tablespace��
         * DDL�� �� �� ����. */
        for( sTempTableInfoList = sTableInfoList;
             sTempTableInfoList != NULL;
             sTempTableInfoList = sTempTableInfoList->next )
        {
            /* tablespace lock�� ������Ƿ� �ٸ� Transaction��
             * table�� DDL�� �� �� ����. table lock�� �����ʰ�
             * table info�� ���� �� �� �ִ�.*/
            IDE_TEST_RAISE( qcuTemporaryObj::existSessionTable(
                                sTempTableInfoList->tableInfo ) == ID_TRUE,
                            ERR_SESSION_TEMPORARY_TABLE_EXIST );
        }

        // PROJ-1502 PARTITIONED DISK TABLE
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
            IDU_FIT_POINT( "qdtDrop::execute::alloc::sTablePartInfoList",
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
                /* To Fix BUG-17285
                 * [PROJ-1548-test] Disk Tablespace OFFLINE/DICARD
                 * �� DROP�� �����߻� */
                IDE_TEST( qcmPartition::validateAndLockPartitionInfoList( aStatement,
                                                                          sTablePartInfoList[i],
                                                                          SMI_TBSLV_DROP_TBS, // TBS Validation �ɼ�
                                                                          SMI_TABLE_LOCK_X,
                                                                          smiGetDDLLockTimeOut((QC_SMI_STMT(aStatement))->getTrans()) )
                          != IDE_SUCCESS );

                // PROJ-1624 non-partitioned index
                IDE_TEST( qdx::makeAndLockIndexTableList(
                              aStatement,
                              ID_TRUE,
                              sTableInfo,
                              & sIndexTableList[i] )
                          != IDE_SUCCESS );
            
                /* To Fix BUG-17285
                 * [PROJ-1548-test] Disk Tablespace OFFLINE/DICARD
                 * �� DROP�� �����߻� */
                IDE_TEST( qdx::validateAndLockIndexTableList( aStatement,
                                                              sIndexTableList[i],
                                                              SMI_TBSLV_DROP_TBS, // TBS Validation �ɼ�
                                                              SMI_TABLE_LOCK_X,
                                                              smiGetDDLLockTimeOut((QC_SMI_STMT(aStatement))->getTrans()) )
                          != IDE_SUCCESS );

                i++;
            }
        }

        // ���̺����̽��� ���� �ε����� ã�Ƽ� sIndexInfoList�� �޾� ��.
        IDE_TEST( qcmTablespace::findIndexInfoListInTBS(
                      aStatement,
                      sParseTree->TBSAttr->mID,
                      ID_FALSE,
                      &sIndexInfoList) != IDE_SUCCESS );

        // PROJ-1502 PARTITIONED DISK TABLE
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
            IDU_FIT_POINT( "qdtDrop::execute::alloc::sIndexPartInfoList",
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
                /* To Fix BUG-17285
                 * [PROJ-1548-test] Disk Tablespace OFFLINE/DICARD
                 * �� DROP�� �����߻� */
                IDE_TEST( qcmPartition::validateAndLockPartitionInfoList( aStatement,
                                                                          sIndexPartInfoList[i],
                                                                          SMI_TBSLV_DROP_TBS, // TBS Validation �ɼ�
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
                
                    /* To Fix BUG-17285
                     * [PROJ-1548-test] Disk Tablespace OFFLINE/DICARD
                     * �� DROP�� �����߻� */
                    IDE_TEST(smiValidateAndLockObjects( (QC_SMI_STMT( aStatement ))->getTrans(),
                                                        sIndexTable->tableHandle,
                                                        sIndexTable->tableSCN,
                                                        SMI_TBSLV_DROP_TBS, // TBS Validation �ɼ�
                                                        SMI_TABLE_LOCK_X,
                                                        smiGetDDLLockTimeOut((QC_SMI_STMT(aStatement))->getTrans()),
                                                        ID_FALSE ) // BUG-28752 ����� Lock�� ������ Lock�� �����մϴ�.
                             != IDE_SUCCESS);
                }
                else
                {
                    // Nothing to do.
                }
                
                i++;
            }
        }

        for( sTempTableInfoList = sTableInfoList, i = 0;
             sTempTableInfoList != NULL;
             sTempTableInfoList = sTempTableInfoList->next )
        {
            sTableInfo = sTempTableInfoList->tableInfo;

            //-----------------------------------------
            // ������ ���̺� ���ؼ�
            // ���̺� ��ü�� qp meta������ ����
            //-----------------------------------------

            if( sTableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
            {
                IDE_ASSERT( sTablePartInfoList != NULL );

                IDE_TEST( executeDropTableInTBS(aStatement,
                                                sTempTableInfoList,
                                                sTablePartInfoList[i],
                                                sIndexTableList[i] )
                          != IDE_SUCCESS );

                i++;
            }
            else
            {
                IDE_TEST( executeDropTableInTBS(aStatement,
                                                sTempTableInfoList,
                                                NULL,
                                                NULL )
                          != IDE_SUCCESS );
            }
        }

        /* PROJ-2211 Materialized View */
        IDE_TEST( executeDropMViewOfRelated(
                        aStatement,
                        sTableInfoList,
                        &sMViewOfRelatedInfoList )
                  != IDE_SUCCESS );

        for( sTempIndexInfoList = sIndexInfoList, i = 0;
             sTempIndexInfoList != NULL;
             sTempIndexInfoList = sTempIndexInfoList->next )
        {
            sTableInfo = sTempIndexInfoList->tableInfo;

            //-----------------------------------------
            // ������ �ε����� ���ؼ�
            // �ε��� ��ü�� qp meta������ ����
            //-----------------------------------------

            if( sTableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
            {
                IDE_ASSERT( sIndexPartInfoList != NULL );

                IDE_TEST( executeDropIndexInTBS(aStatement,
                                                sTempIndexInfoList,
                                                sIndexPartInfoList[i])
                        != IDE_SUCCESS );

                i++;
            }
            else
            {
                IDE_TEST( executeDropIndexInTBS(aStatement,
                                                sTempIndexInfoList,
                                                NULL)
                        != IDE_SUCCESS );
            }
        }

        //-----------------------------------------
        // AND DATAFILES ������ ����ϸ�,
        // SMI_ALL_TOUCH�� �����ؼ� sm���� �Ѱ� ��.
        //-----------------------------------------

        if ( ( sParseTree->flag & QDT_DROP_AND_DATAFILES_MASK )
             == QDT_DROP_AND_DATAFILES_TRUE )
        {
            sTouchMode = SMI_ALL_TOUCH;
        }
        else
        {
            sTouchMode = SMI_ALL_NOTOUCH;
        }
    }
    else
    {
        // Nothing To Do
    }

    //-----------------------------------------
    // ���̺����̽� ��ü�� ����
    //-----------------------------------------

    IDE_TEST( smiTableSpace::drop(aStatement->mStatistics,
                                  sTrans,
                                  sParseTree->TBSAttr->mID,
                                  sTouchMode)
          != IDE_SUCCESS );

    // INCLUDING CONTENTS ������ ����� ��� ���̺����̽� ���� ��� ��ü���� �����Ѵ�.
    if ( ( sParseTree->flag & QDT_DROP_INCLUDING_CONTENTS_MASK )
         == QDT_DROP_INCLUDING_CONTENTS_TRUE )
    {
        //-----------------------------------------
        // ���̺����̽� ���� �ε����� �Ҽӵ� ���̺� ���ؼ�
        // new cached meta�� ����
        // ������ ����� ����ó���� ���� sStage�� 1�� ������
        //-----------------------------------------
        for( sTempIndexInfoList = sIndexInfoList, i = 0;
             sTempIndexInfoList != NULL;
             sTempIndexInfoList = sTempIndexInfoList->next )
        {
            sTableInfo = sTempIndexInfoList->tableInfo;

            IDE_TEST( qcm::touchTable(QC_SMI_STMT( aStatement ),
                                      sTableInfo->tableID,
                                      SMI_TBSLV_DROP_TBS)
                    != IDE_SUCCESS );

            // BUG-16422
            IDE_TEST( qcmTableInfoMgr::makeTableInfo( aStatement,
                                                      sTableInfo,
                                                      NULL )
                      != IDE_SUCCESS );

            // PROJ-1502 PARTITIONED DISK TABLE
            if( sTableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
            {
                IDE_ASSERT( sIndexPartInfoList != NULL );

                for( sTempPartInfoList = sIndexPartInfoList[i];
                     sTempPartInfoList != NULL;
                     sTempPartInfoList = sTempPartInfoList->next )
                {
                    IDE_TEST( smiTable::touchTable( QC_SMI_STMT( aStatement ),
                                                    sTempPartInfoList->partitionInfo->tableHandle,
                                                    SMI_TBSLV_DROP_TBS )
                              != IDE_SUCCESS );

                    IDE_TEST( qcmTableInfoMgr::makeTableInfo(
                                  aStatement,
                                  sTempPartInfoList->partitionInfo,
                                  NULL )
                              != IDE_SUCCESS);
                }
                
                // PROJ-1624 global non-partitioned index
                // index table tableinfo�� destroy�Ѵ�.
                if ( sTempIndexInfoList->isPartitionedIndex == ID_FALSE )
                {
                    sIndexTable = & sTempIndexInfoList->indexTable;
                
                    IDE_TEST( qcmTableInfoMgr::destroyTableInfo(
                                  aStatement,
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

        //-----------------------------------------
        // ���̺����̽� ���� ���̺��� ������
        // Trigger cached meta�� qcmTriggerInfo�� ����
        //-----------------------------------------

        for( sTempTableInfoList = sTableInfoList;
             sTempTableInfoList != NULL;
             sTempTableInfoList = sTempTableInfoList->next )
        {
            // ���� ����ǰų�, �����̸� FATAL�� �̹� �����Ǿ� ����.
            IDE_TEST( qdnTrigger::freeTriggerCaches4DropTable(
                          sTempTableInfoList->tableInfo ) != IDE_SUCCESS );
        }

        //-----------------------------------------
        // ���̺����̽� ���� �ε����� �Ҽӵ� ���̺� ���ؼ�
        // old cached meta�� ����
        //-----------------------------------------

        // �ε����� ������ tableInfo�� ������ ���̹Ƿ�
        // tableInfo�� ���������� �ʴ´�.

        //-----------------------------------------
        // ���̺����̽� ���� ���̺� ���ؼ�
        // cached meta�� ����
        //-----------------------------------------

        for( sTempTableInfoList = sTableInfoList, i = 0;
             sTempTableInfoList != NULL;
             sTempTableInfoList = sTempTableInfoList->next )
        {
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
                    IDE_TEST( qcmTableInfoMgr::destroyTableInfo(
                                  aStatement,
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

        /* PROJ-2211 Materialized View */
        for( sTempTableInfoList = sMViewOfRelatedInfoList;
             sTempTableInfoList != NULL;
             sTempTableInfoList = sTempTableInfoList->next )
        {
            /* BUG-16422 */
            IDE_TEST( qcmTableInfoMgr::destroyTableInfo(
                          aStatement,
                          sTempTableInfoList->tableInfo )
                      != IDE_SUCCESS );
        }
    }
    else
    {
        // Nothing To Do
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_SESSION_TEMPORARY_TABLE_EXIST )
    {
        IDE_SET(ideSetErrorCode( qpERR_ABORT_QDT_DROP_TBS_DISABLE_BECAUSE_TEMP_TABLE ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qdtDrop::executeDropTableInTBS(qcStatement          * aStatement,
                                      qcmTableInfoList     * aTableInfoList,
                                      qcmPartitionInfoList * aPartInfoList,
                                      qdIndexTableList     * aIndexTables)
{
/***********************************************************************
 *
 * Description :
 *    qdtDrop::execute�� ���� ȣ��
 *    ���̺����̽� ���� ���̺� ��ü�� qp meta������ ����
 *
 * Implementation :
 *    1. ���̺� unique �ε����� ���� ��� referential constraint��
 *       ã�Ƽ� ����
 *    2. SYS_CONSTRAINTS_ ���� ���õ� constraint ���� ����
 *    3. SYS_INDICES_, SYS_INDEX_COLUMNS_ ���� ���̺� ���� ����
 *    4. SYS_TABLES_, SYS_COLUMNS_ ���� ���̺� ���� ����
 *    5. SYS_GRANT_OBJECT_ ���� ���̺�� ���õ� privilege ���� ����
 *    6. Trigger ��ü ������
 *       SYS_TRIGGERS_ ..�� ���� ���̺�� ���õ� Trigger ���� ����
 *    7. related PSM �� invalid ���·� ����
 *    8. related VIEW �� invalid ���·� ����
 *    9. Constraint�� ���õ� Procedure�� ���� ������ ����
 *   10. Index�� ���õ� Procedure�� ���� ������ ����
 *   11. ���̺��� ��ü�� ����
 *
 ***********************************************************************/
#define IDE_FN "qdtDrop::executeDropTableInTBS"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qdtDrop::executeDropTableInTBS"));

    qcmTableInfo          * sTableInfo;
    qcmIndex              * sIndexInfo = NULL;
    UInt                    i;
    qcmPartitionInfoList  * sPartInfoList = NULL;
    qcmTableInfo          * sPartInfo;
    qdIndexTableList      * sIndexTable;

    //-----------------------------------------
    // ���̺� unique �ε����� ���� ���
    // referential constraint�� ã�Ƽ� ����
    //-----------------------------------------

    sTableInfo = aTableInfoList->tableInfo;

    /* PROJ-1407 Temporary table
     * session temporary table�� �����ϴ� ��� DDL�� �� �� ����.
     * �տ��� session table�� ���������� �̸� Ȯ���Ͽ���.
     * session table�� ������ ����.*/
    IDE_DASSERT( qcuTemporaryObj::existSessionTable( sTableInfo ) == ID_FALSE );

    // PROJ-1502 PARTITIONED DISK TABLE
    if( sTableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
    {
        for( sPartInfoList = aPartInfoList;
             sPartInfoList != NULL;
             sPartInfoList = sPartInfoList->next )
        {
            sPartInfo = sPartInfoList->partitionInfo;

            // BUG-35135
            IDE_TEST( qdd::dropTablePartition( aStatement,
                                               sPartInfo,
                                               ID_TRUE, /* aIsDropTablespace */
                                               ID_FALSE )
                      != IDE_SUCCESS );
        }
            
        // PROJ-1624 non-partitioned index
        for ( sIndexTable = aIndexTables;
              sIndexTable != NULL;
              sIndexTable = sIndexTable->next )
        {
            // BUG-35135
            IDE_TEST( qdx::dropIndexTable( aStatement,
                                           sIndexTable,
                                           ID_TRUE /* aIsDropTablespace */ )
                      != IDE_SUCCESS );
        }
    }

    for (i = 0; i < sTableInfo->uniqueKeyCount; i++)
    {
        sIndexInfo = sTableInfo->uniqueKeys[i].constraintIndex;

        IDE_TEST( qcm::getChildKeysForDelete( aStatement,
                                              QC_EMPTY_USER_ID,
                                              sIndexInfo,
                                              sTableInfo,
                                              ID_TRUE /* aDropTablespace */,
                                              &(aTableInfoList->childInfo) )
                  != IDE_SUCCESS );

        IDE_TEST( qdd::deleteFKConstraints( aStatement,
                                            sTableInfo,
                                            aTableInfoList->childInfo,
                                            ID_TRUE /* aDropTablespace */ )
                  != IDE_SUCCESS );
    }

    //-----------------------------------------
    // SYS_CONSTRAINTS_ ���� ���õ� constraint ���� ����
    //-----------------------------------------

    IDE_TEST( qdd::deleteConstraintsFromMeta(aStatement,
                                             sTableInfo->tableID)
              != IDE_SUCCESS );

    //-----------------------------------------
    // SYS_INDICES_, SYS_INDEX_COLUMNS_ ���� ���̺� ���� ����
    //-----------------------------------------

    IDE_TEST( qdd::deleteIndicesFromMeta(aStatement,
                                         sTableInfo)
              != IDE_SUCCESS );

    //-----------------------------------------
    // SYS_TABLES_, SYS_COLUMNS_ ���� ���̺� ���� ����
    //-----------------------------------------

    IDE_TEST( qdd::deleteTableFromMeta(aStatement,
                                       sTableInfo->tableID)
              != IDE_SUCCESS );

    //-----------------------------------------
    // SYS_GRANT_OBJECT_ ���� ���̺�� ���õ� privilege ���� ����
    //-----------------------------------------

    IDE_TEST( qdpDrop::removePriv4DropTable(aStatement,
                                            sTableInfo->tableID)
              != IDE_SUCCESS );

    //-----------------------------------------
    // Trigger ��ü��
    // SYS_TRIGGERS_ ..�� ���� ���̺�� ���õ� Trigger ���� ����
    //-----------------------------------------

    IDE_TEST(qdnTrigger::dropTrigger4DropTable(aStatement, sTableInfo )
             != IDE_SUCCESS);

    /* PROJ-2197 PSM Renewal */
    //-----------------------------------------
    // related PSM �� invalid ���·� ����
    //-----------------------------------------

    IDE_TEST( qcmProc::relSetInvalidProcOfRelated(
                aStatement,
                sTableInfo->tableOwnerID,
                sTableInfo->name,
                idlOS::strlen((SChar*)sTableInfo->name),
                QS_TABLE) != IDE_SUCCESS );

    // PROJ-1073 Package
    //-----------------------------------------
    // related PSM �� invalid ���·� ����
    //-----------------------------------------
    IDE_TEST( qcmPkg::relSetInvalidPkgOfRelated(
                aStatement,
                sTableInfo->tableOwnerID,
                sTableInfo->name,
                idlOS::strlen((SChar*)sTableInfo->name),
                QS_TABLE) != IDE_SUCCESS );
        
    //-----------------------------------------
    // related VIEW �� invalid ���·� ����
    //-----------------------------------------

    IDE_TEST( qcmView::setInvalidViewOfRelated(
                  aStatement,
                  sTableInfo->tableOwnerID,
                  sTableInfo->name,
                  idlOS::strlen((SChar*)sTableInfo->name),
                  QS_TABLE) != IDE_SUCCESS );

    /* BUG-35445 Check Constraint, Function-Based Index���� ��� ���� Function�� ����/���� ���� */
    IDE_TEST( qcmProc::relRemoveRelatedToConstraintByTableID(
                    aStatement,
                    sTableInfo->tableID )
              != IDE_SUCCESS );

    IDE_TEST( qcmProc::relRemoveRelatedToIndexByTableID(
                    aStatement,
                    sTableInfo->tableID )
              != IDE_SUCCESS );

    //-----------------------------------------
    // BUG-21387 COMMENT
    // SYS_COMMENTS_���� Comment�� �����Ѵ�.
    //-----------------------------------------
    IDE_TEST( qdbComment::deleteCommentTable(
                  aStatement,
                  sTableInfo->tableOwnerName,
                  sTableInfo->name)
              != IDE_SUCCESS );

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
                                   SMI_TBSLV_DROP_TBS )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qdtDrop::executeDropIndexInTBS(qcStatement          * aStatement,
                                      qcmIndexInfoList     * aIndexInfoList,
                                      qcmPartitionInfoList * aPartInfoList)
{
/***********************************************************************
 *
 * Description :
 *    qdtDrop::execute�� ���� ȣ��
 *    ���̺����̽� ���� �ε��� ��ü�� qp meta������ ����
 *
 * Implementation :
 *    1. unique �ε����� ���� ��� referential constraint��
 *       ã�Ƽ� ����
 *    2. �ε��� ��ü�� ����
 *    3. SYS_INDICES_, SYS_INDEX_COLUMNS_ ���� �ε��� ���� ����
 *    4. related PSM �� invalid ���·� ����
 *
 ***********************************************************************/
#define IDE_FN "qdtDrop::executeDropIndexInTBS"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qdtDrop::executeDropIndexInTBS"));

    qcmTableInfo          * sTableInfo;
    qcmIndex              * sIndexInfo = NULL;
    idBool                  sIsPrimary = ID_FALSE;
    UInt                    i;
    qcmPartitionInfoList  * sPartInfoList = NULL;
    void                  * sIndexHandle;

    sTableInfo = aIndexInfoList->tableInfo;

    sPartInfoList = aPartInfoList;

    for (i = 0; i < sTableInfo->indexCount; i++ )
    {
        if ( sTableInfo->indices[i].indexId ==
             aIndexInfoList->indexID )
        {
            sIndexInfo = &sTableInfo->indices[i];

            // BUG-28321 
            sIndexHandle = (void *)smiGetTableIndexByID(
                sTableInfo->tableHandle,
                sIndexInfo->indexId );

            break;
        }
    }
    IDE_TEST_RAISE( sIndexInfo == NULL, ERR_NOT_EXIST_INDEX);

    //-----------------------------------------
    // unique �ε����� ���� ��� referential constraint�� ã�Ƽ� ����
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
            // BUG-35135
            IDE_TEST( qdx::dropIndexTable( aStatement,
                                           & aIndexInfoList->indexTable,
                                           ID_TRUE /* aIsDropTablespace */ )
                      != IDE_SUCCESS );
        }
        else
        {
            // Nothing to do.
        }
        
        if ( ( sIndexInfo->indexPartitionType != QCM_NONE_PARTITIONED_INDEX ) ||
             ( sIsPrimary == ID_TRUE ) )
        {
            // BUG-35135
            IDE_TEST( qdd::dropIndexPartitions( aStatement,
                                                sPartInfoList,
                                                sIndexInfo->indexId,
                                                ID_TRUE /* aIsCascade */,
                                                ID_TRUE /* aIsDropTablespace */ )
                      != IDE_SUCCESS );
        }
        else
        {
            // Nothing to do.
        }
    }

    if ( sIndexInfo->isUnique == ID_TRUE )
    {
        IDE_TEST( qcm::getChildKeysForDelete( aStatement,
                                              QC_EMPTY_USER_ID,
                                              sIndexInfo,
                                              sTableInfo,
                                              ID_TRUE /* aDropTablespace */,
                                              &(aIndexInfoList->childInfo) )
                  != IDE_SUCCESS );

        IDE_TEST( qdd::deleteFKConstraints( aStatement,
                                            sTableInfo,
                                            aIndexInfoList->childInfo,
                                            ID_TRUE /* aDropTablespace */ )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    //-----------------------------------------
    // �ε��� ��ü�� ����
    //-----------------------------------------

    IDE_TEST( smiTable::dropIndex(QC_SMI_STMT( aStatement ),
                                  sTableInfo->tableHandle,
                                  sIndexHandle,
                                  SMI_TBSLV_DROP_TBS)
              != IDE_SUCCESS );

    //-----------------------------------------
    // SYS_INDICES_, SYS_INDEX_COLUMNS_ ���� �ε��� ���� ����
    //-----------------------------------------

    IDE_TEST( qdd::deleteIndicesFromMetaByIndexID(aStatement,
                                                  sIndexInfo->indexId)
              != IDE_SUCCESS );

    //-----------------------------------------
    // BUG-17326
    // Constraint�� ������ �ε����� ��� Constraint�� �Բ� ����
    // SYS_CONSTRAINTS_, SYS_CONSTRAINT_COLUMNS_ ���� Constraint ���� ����
    //-----------------------------------------

    for ( i = 0; i < sTableInfo->uniqueKeyCount; i++ )
    {
        if ( sTableInfo->uniqueKeys[i].constraintIndex->indexId ==
             sIndexInfo->indexId )
        {
            IDE_TEST( qdd::deleteConstraintsFromMetaByConstraintID(
                          aStatement,
                          sTableInfo->uniqueKeys[i].constraintID )
                      != IDE_SUCCESS );
        }
        else
        {
            // Nothing to do.
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_NOT_EXIST_INDEX)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCM_NOT_EXISTS_INDEX));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

/***********************************************************************
 *
 * Description :
 *    qdtDrop::execute()���� Materialized View�� MVIew Table�� ������ �κ���
 *    �����ϱ� ���� ȣ���Ѵ�. (Meta Cache�� ����)
 *    ���߿� Meta Cache�� �����ϱ� ����, MVIew View List�� ��ȯ�Ѵ�.
 *
 *    Table List�� MView Table�� ������,
 *    (1) MView View�� �����Ѵ�. (Meta Cache�� ����)
 *    (2) Materialized View�� Meta Table���� �����Ѵ�.
 *
 * Implementation :
 *    MVIew Table�̸�, �Ʒ��� �۾��� �����Ѵ�.
 *    1. lock & get view information
 *    2. Meta Table���� View ����
 *    3. Meta Table���� Object Privilege ���� (View)
 *    4. Trigger ���� (Meta Table, Object)
 *    5. related PSM �� Invalid ���·� ���� (View)
 *    6. related VIEW �� Invalid ���·� ���� (View)
 *    7. smiTable::dropTable (View)
 *    8. Meta Table���� Materialized View ����
 *    9. MVIew View List�� View�� �߰��Ѵ�.
 *
 ***********************************************************************/
IDE_RC qdtDrop::executeDropMViewOfRelated(
                        qcStatement       * aStatement,
                        qcmTableInfoList  * aTableInfoList,
                        qcmTableInfoList ** aMViewOfRelatedInfoList )
{
    qcmTableInfoList * sTableInfoList     = NULL;
    qcmTableInfoList * sMViewInfoList     = NULL;
    qcmTableInfoList * sLastTableInfoList = NULL;
    qcmTableInfoList * sNewTableInfoList  = NULL;
    qcmTableInfo     * sTableInfo         = NULL;

    SChar              sViewName[QC_MAX_OBJECT_NAME_LEN + 1];
    qcmTableInfo     * sViewInfo          = NULL;
    void             * sViewHandle        = NULL;
    smSCN              sViewSCN;
    UInt               sMViewID;

    for ( sTableInfoList = aTableInfoList;
          sTableInfoList != NULL;
          sTableInfoList = sTableInfoList->next )
    {
        sTableInfo = sTableInfoList->tableInfo;

        if ( sTableInfo->tableType == QCM_MVIEW_TABLE )
        {
            (void)idlOS::memset( sViewName, 0x00, QC_MAX_OBJECT_NAME_LEN + 1 );
            (void)idlVA::appendString( sViewName,
                                       QC_MAX_OBJECT_NAME_LEN + 1,
                                       sTableInfo->name,
                                       idlOS::strlen( sTableInfo->name ) );
            (void)idlVA::appendString( sViewName,
                                       QC_MAX_OBJECT_NAME_LEN + 1,
                                       QDM_MVIEW_VIEW_SUFFIX,
                                       QDM_MVIEW_VIEW_SUFFIX_SIZE );

            /* lock & get view information */
            IDE_TEST( qcm::getTableHandleByName(
                            QC_SMI_STMT( aStatement ),
                            sTableInfo->tableOwnerID,
                            (UChar *)sViewName,
                            (SInt)idlOS::strlen( sViewName ),
                            &sViewHandle,
                            &sViewSCN )
                      != IDE_SUCCESS );

            IDE_TEST( smiValidateAndLockObjects( (QC_SMI_STMT( aStatement ))->getTrans(),
                                                 sViewHandle,
                                                 sViewSCN,
                                                 SMI_TBSLV_DROP_TBS, // TBS Validation �ɼ�
                                                 SMI_TABLE_LOCK_X,
                                                 smiGetDDLLockTimeOut((QC_SMI_STMT(aStatement))->getTrans()),
                                                 ID_FALSE ) // BUG-28752 ����� Lock�� ������ Lock�� �����մϴ�.
                      != IDE_SUCCESS );

            IDE_TEST( smiGetTableTempInfo( sViewHandle,
                                           (void**)&sViewInfo )
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
                                sTableInfo->tableOwnerID,
                                sViewName,
                                ID_SIZEOF(sViewName),
                                QS_TABLE )
                      != IDE_SUCCESS );

            // PROJ-1073 Package
            IDE_TEST( qcmPkg::relSetInvalidPkgOfRelated(
                                aStatement,
                                sTableInfo->tableOwnerID,
                                sViewName,
                                ID_SIZEOF(sViewName),
                                QS_TABLE )
                      != IDE_SUCCESS );

            IDE_TEST( qcmView::setInvalidViewOfRelated(
                                aStatement,
                                sTableInfo->tableOwnerID,
                                sViewName,
                                ID_SIZEOF(sViewName),
                                QS_TABLE )
                      != IDE_SUCCESS );

            IDE_TEST( smiTable::dropTable( QC_SMI_STMT( aStatement ),
                                           sViewInfo->tableHandle,
                                           SMI_TBSLV_DROP_TBS )
                      != IDE_SUCCESS );

            /* drop materialized view */
            IDE_TEST( qcmMView::getMViewID(
                                QC_SMI_STMT( aStatement ),
                                sTableInfo->tableOwnerID,
                                sTableInfo->name,
                                idlOS::strlen( sTableInfo->name ),
                                &sMViewID )
                      != IDE_SUCCESS );

            IDE_TEST( qdd::deleteMViewFromMeta( aStatement, sMViewID )
                      != IDE_SUCCESS );

            /* make mview views list */
            IDU_LIMITPOINT( "qdtDrop::executeDropMViewOfRelated::malloc");
            IDE_TEST( QC_QMX_MEM( aStatement )->alloc(
                    ID_SIZEOF(qcmTableInfoList),
                    (void **)&sNewTableInfoList )
                != IDE_SUCCESS);

            sNewTableInfoList->tableInfo = sViewInfo;
            sNewTableInfoList->childInfo = NULL;
            sNewTableInfoList->next      = NULL;

            if ( sMViewInfoList == NULL )
            {
                sMViewInfoList = sNewTableInfoList;
            }
            else
            {
                sLastTableInfoList->next = sNewTableInfoList;
            }
            sLastTableInfoList = sNewTableInfoList;
        }
        else
        {
            /* Nothing to do */
        }
    }

    *aMViewOfRelatedInfoList = sMViewInfoList;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
