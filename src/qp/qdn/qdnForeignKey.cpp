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
 * $Id: qdnForeignKey.cpp 90270 2021-03-21 23:20:18Z bethy $
 *
 * Description :
 *
 *     Foreign Key Reference Constraint ó���� ���� ���
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#include <idl.h>
#include <ide.h>
#include <qcg.h>
#include <qcmCache.h>
#include <qcmUser.h>
#include <qdpPrivilege.h>
#include <qdnForeignKey.h>
#include <qdnTrigger.h>
#include <qcuTemporaryObj.h>
#include <qmoPartition.h>
#include <qmx.h>
#include <qdnCheck.h>
#include <qmsDefaultExpr.h>
#include <qmnUpdate.h>
#include <qdpRole.h>

IDE_RC
qdnForeignKey::reorderForeignKeySpec( qdConstraintSpec * aUniqueKeyConstr,
                                      qdConstraintSpec * aForeignKeyConstr )
{
/***********************************************************************
 *
 * Description :
 *    BUG-27001
 *    foreign key�� ���� ������ �����Ǵ� unique key �ε����� �÷�������
 *    ��ġ�ϵ��� foreign key �÷����� ������ ������ ��
 *
 * Implementation :
 *    sOldRefCols�� ����Ʈ���� unique key�� �÷� ������� �Ѱ���
 *    �÷��� ������, sNewRefCols�� �籸����
 *
 * Remark :
 *    �ݵ�� aUniqueKeyConstr�� aForeignKeyConstr�� constraintColumns,
 *    aForeignKeyConstr�� referentialConstraintSpec->referencedColList
 *    ���̴� ���ƾ� �Ѵ�
 *
 ***********************************************************************/
    qcmColumn* sUniqueKeyCols = aUniqueKeyConstr->constraintColumns;

    qcmColumn* sOldRefCols
        = aForeignKeyConstr->referentialConstraintSpec->referencedColList;
    qcmColumn* sNewRefCols = NULL;
    qcmColumn* sNewRefTail = NULL;
    qcmColumn* sCurrRefCol;
    qcmColumn* sPrevRefCol;

    qcmColumn* sOldConstrCols = aForeignKeyConstr->constraintColumns;
    qcmColumn* sNewConstrCols = NULL;
    qcmColumn* sNewConstrTail = NULL;
    qcmColumn* sCurrConstrCol;
    qcmColumn* sPrevConstrCol;

    while( sUniqueKeyCols != NULL )
    {
        sCurrRefCol = sOldRefCols;
        sPrevRefCol = NULL;

        sCurrConstrCol = sOldConstrCols;
        sPrevConstrCol = NULL;

        while( sCurrRefCol != NULL )
        {
            // unique key column�� ��ġ�ϴ� ref. key column�˻�
            if ( QC_IS_NAME_MATCHED( sUniqueKeyCols->namePos, sCurrRefCol->namePos ) )
            {
                break;
            }
            else
            {
                // Nothing to do.
            }
            sPrevRefCol = sCurrRefCol;
            sCurrRefCol = sCurrRefCol->next;

            sPrevConstrCol = sCurrConstrCol;
            sCurrConstrCol = sCurrConstrCol->next;
        }
        IDE_ASSERT( sCurrRefCol != NULL );
        IDE_ASSERT( sCurrConstrCol != NULL );

        // old list���� �� ���� ��带 ����
        if( sCurrRefCol == sOldRefCols )
        {
            // ù ��° ��带 ������ ���, ���� ��带 ù ��° ���� ����
            sOldRefCols = sCurrRefCol->next;
            sOldConstrCols = sCurrConstrCol->next;
        }
        else
        {
            // �߰����� ������ ���, ���� ����� ���� ��带 ���� �����
            // �������� ����
            sPrevRefCol->next = sCurrRefCol->next;
            sPrevConstrCol->next = sCurrConstrCol->next;
        }

        // new list�� tail�� ��带 �߰��Ѵ�
        if( sNewRefTail == NULL )
        {
            // new list�� ����ִ� ���
            sNewRefCols = sCurrRefCol;
            sNewRefTail = sCurrRefCol;

            sNewConstrCols = sCurrConstrCol;
            sNewConstrTail = sCurrConstrCol;
        }
        else
        {
            // new list�� ������� ���� ���
            sNewRefTail->next = sCurrRefCol;    // ��� �߰�
            sNewRefTail = sNewRefTail->next;    // list�� tail ����

            sNewConstrTail->next = sCurrConstrCol;
            sNewConstrTail = sNewConstrTail->next;
        }
        sNewRefTail->next = NULL;
        sNewConstrTail->next = NULL;

        sUniqueKeyCols = sUniqueKeyCols->next;  // next
    }
    aForeignKeyConstr->referentialConstraintSpec->referencedColList = sNewRefCols;
    aForeignKeyConstr->constraintColumns = sNewConstrCols;

    return IDE_SUCCESS;
}

IDE_RC
qdnForeignKey::reorderForeignKeySpec( qcmIndex         * aUniqueIndex,
                                      qdConstraintSpec * aForeignKeyConstr )
{
/***********************************************************************
 *
 * Description :
 *    BUG-27001
 *    foreign key�� ���� ������ �����Ǵ� unique key �ε����� �÷�������
 *    ��ġ�ϵ��� foreign key �÷����� ������ ������ ��
 *
 * Implementation :
 *    sOldRefCols�� ����Ʈ���� unique key�� �÷� ������� �Ѱ���
 *    �÷��� ������, sNewRefCols�� �籸����
 *
 * Remark :
 *    �ݵ�� aUniqueKeyConstr�� aForeignKeyConstr�� constraintColumns,
 *    aForeignKeyConstr�� referentialConstraintSpec->referencedColList
 *    ���̴� ���ƾ� �Ѵ�
 *
 ***********************************************************************/
    UInt       i;

    qcmColumn* sOldRefCols
        = aForeignKeyConstr->referentialConstraintSpec->referencedColList;
    qcmColumn* sNewRefCols = NULL;
    qcmColumn* sNewRefTail = NULL;
    qcmColumn* sCurrRefCol;
    qcmColumn* sPrevRefCol;

    qcmColumn* sOldConstrCols = aForeignKeyConstr->constraintColumns;
    qcmColumn* sNewConstrCols = NULL;
    qcmColumn* sNewConstrTail = NULL;
    qcmColumn* sCurrConstrCol;
    qcmColumn* sPrevConstrCol;

    for( i = 0; i < aUniqueIndex->keyColCount; i++ )
    {
        sCurrRefCol = sOldRefCols;
        sPrevRefCol = NULL;

        sCurrConstrCol = sOldConstrCols;
        sPrevConstrCol = NULL;

        while( sCurrRefCol != NULL )
        {
            // unique key column�� ��ġ�ϴ� ref. key column�˻�
            if( aUniqueIndex->keyColumns[i].column.id ==
                sCurrRefCol->basicInfo->column.id )
            {
                break;
            }
            else
            {
                // Nothing to do.
            }
            sPrevRefCol = sCurrRefCol;
            sCurrRefCol = sCurrRefCol->next;

            sPrevConstrCol = sCurrConstrCol;
            sCurrConstrCol = sCurrConstrCol->next;
        }
        IDE_ASSERT( sCurrRefCol != NULL );
        IDE_ASSERT( sCurrConstrCol != NULL );

        // old list���� �� ���� ��带 ����
        if( sCurrRefCol == sOldRefCols )
        {
            // ù ��° ��带 ������ ���, ���� ��带 ù ��° ���� ����
            sOldRefCols = sCurrRefCol->next;
            sOldConstrCols = sCurrConstrCol->next;
        }
        else
        {
            // �߰����� ������ ���, ���� ����� ���� ��带 ���� �����
            // �������� ����
            sPrevRefCol->next = sCurrRefCol->next;
            sPrevConstrCol->next = sCurrConstrCol->next;
        }

        // new list�� tail�� ��带 �߰��Ѵ�
        if( sNewRefTail == NULL )
        {
            // new list�� ����ִ� ���
            sNewRefCols = sCurrRefCol;
            sNewRefTail = sCurrRefCol;

            sNewConstrCols = sCurrConstrCol;
            sNewConstrTail = sCurrConstrCol;
        }
        else
        {
            // new list�� ������� ���� ���
            sNewRefTail->next = sCurrRefCol;    // ��� �߰�
            sNewRefTail = sNewRefTail->next;    // list�� tail ����

            sNewConstrTail->next = sCurrConstrCol;
            sNewConstrTail = sNewConstrTail->next;
        }
        sNewRefTail->next = NULL;
        sNewConstrTail->next = NULL;
    }
    aForeignKeyConstr->referentialConstraintSpec->referencedColList = sNewRefCols;
    aForeignKeyConstr->constraintColumns = sNewConstrCols;

    return IDE_SUCCESS;
}

IDE_RC
qdnForeignKey::validateForeignKeySpec( qcStatement      * aStatement,
                                       UInt               aTableOwnerID,
                                       qcmTableInfo     * aTableInfo,
                                       qdConstraintSpec * aForeignKeyConstr )
{
/***********************************************************************
 *
 * Description :
 *    validateConstraints ���� ȣ��, reference �� validation ����
 *
 * Implementation :
 *    �����Ǵ� ���̺��� self ���̺��� ��
 *    -- if create
 *         1. �÷��� ��õ��� �ʾ����� primary key �÷��� �����ϵ���
 *            �÷��� ��õǾ����� ��õ� �÷��� �����ϵ���
 *         2. �����ϴ� �÷��� �����Ǵ� �÷��� Ÿ���� Compatible ���� üũ
 *         3. �����Ǵ� �÷����� ������ primary key
 *            �Ǵ� unique �� �����ϴ��� üũ
 *    -- else (add col, add constraint)
 *         1. �����Ǵ� �÷��� ��õ��� �ʾ�����
 *            primary key �÷��� �ִ��� üũ
 *            �����Ǵ� �÷��� ��õǾ����� ��õ� �÷��� �����ϴ��� üũ
 *         2. �����Ǵ� �÷����� ������ unique key
 *            �Ǵ� constraint �� �����ϴ��� üũ
 *         3. �����ϴ� �÷��� �����Ǵ� �÷��� Ÿ���� Compatible ���� üũ
 *
 *    �����Ǵ� ���̺��� �ٸ� ���̺��� ��
 *    1. �����Ǵ� ���̺��� qcmTableInfo ���ϱ�
 *    2. ���� ������ �ִ��� �˻�
 *    3. ������ �ִ��� �˻�
 *    4. �����Ǵ� �÷��� ��õ��� �ʾ�����,
 *          ������ ���̺� primary key �÷��� �ִ��� üũ
 *       �����Ǵ� �÷��� ��õǾ�����,
 *          ��õ� �÷��� ������ ���̺� �����ϴ��� üũ
 *    5. �����Ǵ� �÷����� ������ unique key �� �����ϴ��� üũ
 *    6. �����ϴ� �÷��� �����Ǵ� �÷��� Ÿ���� Compatible ���� üũ
 *
 ***********************************************************************/

#define IDE_FN "qdnForeignKey::validateForeignKeySpec"

    qcmTableInfo *sReferencedTableInfo;
    smSCN         sSCN;
    qdTableParseTree *sParseTree;
    qcmColumn    *sColumn;
    qcmColumn    *sReferencedCol;
    qcmColumn    *sColumnInfo;
    idBool        sMatchIdxExist = ID_FALSE;
    UInt          i;
    UInt          sColumnCount = 0;
    UInt          sConstraintColCount = 0;
    UInt          sReferencedColCount = 0;
    void        * sTableHandle;
    qcmIndex    * sIndexInfo  = NULL;
    UInt          sConstrType = QD_CONSTR_MAX;
    struct qdReferenceSpec * sRefSpec;
    
    qdConstraintSpec * sParentConstraint = NULL;

    sParseTree = (qdTableParseTree*)(aStatement->myPlan->parseTree);
    sRefSpec   = aForeignKeyConstr->referentialConstraintSpec;

    if (QC_IS_NULL_NAME(sRefSpec->referencedUserName) == ID_FALSE)
    {  // username specified in referenced object clause.
        IDE_TEST(qcmUser::getUserID(aStatement,
                                    sRefSpec->referencedUserName,
                                    &sRefSpec->referencedUserID)
                 != IDE_SUCCESS);
    }
    else
    {  // username omitted.
        sRefSpec->referencedUserID = QCG_GET_SESSION_USER_ID(aStatement);
    }

    for( sColumn = aForeignKeyConstr->constraintColumns, sConstraintColCount = 0;
         sColumn != NULL;
         sColumn = sColumn->next, sConstraintColCount++ )
    {
        // Nothing to do.
    }

    // check self condition & create table.
    if ( ( sRefSpec->referencedUserID == QCG_GET_SESSION_USER_ID(aStatement)) &&
         QC_IS_NAME_MATCHED( sRefSpec->referencedTableName, sParseTree->tableName ) )
    { // self condition.
        if (aTableInfo == NULL)
        {
            //-------------------------------------
            // in create table.
            //-------------------------------------

            // PROJ-1407 Temporary Table
            // temporary table���� self referencing foreign key constraint�� ������ �� ����.
            IDE_TEST_RAISE( ( sParseTree->flag & QDT_CREATE_TEMPORARY_MASK )
                            == QDT_CREATE_TEMPORARY_TRUE,
                            ERR_CANNOT_CREATE_FOREIGN_KEY_ON_TEMPORARY_TABLE );

            sRefSpec->referencedTableID = UINT_MAX;

            //--------------------------------
            // check referenced columns exist.
            //--------------------------------

            if (sRefSpec->referencedColList == NULL)
            {
                //-----------------------------------------
                // this foreign key references primary key.
                //-----------------------------------------

                IDE_TEST(getPrimaryKeyFromDefinition(
                             sParseTree,
                             sRefSpec) != IDE_SUCCESS);
            }
            else
            {
                //-----------------------------------------
                // referenced column specified.
                //-----------------------------------------

                sReferencedCol = sRefSpec->referencedColList;
                for (sColumn = sRefSpec->referencedColList;
                     sColumn != NULL;
                     sColumn = sColumn->next)
                {
                    IDE_TEST( qdn::getColumnFromDefinitionList(
                                  aStatement,
                                  sParseTree->columns,
                                  sColumn->namePos,
                                  & sColumnInfo )
                              != IDE_SUCCESS);

                    IDU_LIMITPOINT("qdnForeignKey::validateForeignKeySpec::malloc1");
                    IDE_TEST(
                        QC_QMP_MEM(aStatement)->alloc(
                            ID_SIZEOF(mtcColumn),
                            (void**)&(sColumn->basicInfo))
                        != IDE_SUCCESS);

                    // To Fix PR-10247
                    // QDX_SET_MTC_COLUMN(sColumn->basicInfo,
                    //                    sColumnInfo->basicInfo);

                    // To Fix PR-10247
                    // ���� ���� Macro�� ����� �ڷ� ������ ���濡
                    // �����ϰ� ��ó�� �� ����.

                    // fix BUG-33258
                    if( sColumn->basicInfo != sColumnInfo->basicInfo )
                    {
                        *(sColumn->basicInfo) = *(sColumnInfo->basicInfo);
                    }
                }
            }

            // check unique index for the referenced columns exists.
            // we must search unique or primary key constraint
            // for parent constraint index.
            sParentConstraint = sParseTree->constraints;
            sColumn = sRefSpec->referencedColList;
            while (sParentConstraint != NULL)
            {
                if( ( sParentConstraint->constrType == QD_PRIMARYKEY ) ||
                    ( sParentConstraint->constrType == QD_UNIQUE ) )
                {
                    if ( qdn::matchColumnListOutOfOrder(
                             sColumn,
                             sParentConstraint->constraintColumns ) == ID_TRUE )
                    {
                        sMatchIdxExist = ID_TRUE;
                        sRefSpec->referencedIndexID = 0;
                        break;
                    }
                }

                sParentConstraint = sParentConstraint->next;
            }
            IDE_TEST_RAISE(sMatchIdxExist == ID_FALSE, err_constraint_not_exist);
        } // in create table end.
        else
        {
            //--------------------------------------------
            // in alter table. add col / add constraint.
            //--------------------------------------------

            // PROJ-1407 Temporary Table
            // temporary table���� foreign key�� ������ �� ����.
            IDE_TEST_RAISE( qcuTemporaryObj::isTemporaryTable( aTableInfo ) == ID_TRUE,
                            ERR_CANNOT_CREATE_FOREIGN_KEY_ON_TEMPORARY_TABLE );

            //-------------------------------------
            // check referenced columns exist.
            //-------------------------------------

            sRefSpec->referencedTableID     = aTableInfo->tableID;
            sRefSpec->referencedTableInfo   = aTableInfo;
            sRefSpec->referencedTableHandle = aTableInfo->tableHandle;
            sRefSpec->referencedTableSCN    =
                smiGetRowSCN(aTableInfo->tableHandle);

            if (sRefSpec->referencedColList == NULL)
            {
                //---------------------------------------------
                // get primary key for referenced constraints.
                //----------------------------------------------

                IDE_TEST_RAISE(aTableInfo->primaryKey == NULL,
                               err_constraint_not_exist);

                IDE_TEST(getPrimaryKeyFromCache(
                             QC_QMP_MEM(aStatement),
                             aTableInfo,
                             &(sRefSpec->referencedColList),
                             &(sRefSpec->referencedColCount)) != IDE_SUCCESS);
            }
            else
            {
                //-----------------------------------------
                // referenced column specified.
                //-----------------------------------------

                sReferencedCol = sRefSpec->referencedColList;
                for (sColumn = sRefSpec->referencedColList;
                     sColumn != NULL;
                     sColumn = sColumn->next)
                {
                    IDE_TEST(qcmCache::getColumn(aStatement,
                                                 aTableInfo,
                                                 sColumn->namePos,
                                                 &sColumnInfo)
                             != IDE_SUCCESS);

                    IDU_LIMITPOINT("qdnForeignKey::validateForeignKeySpec::malloc2");
                    IDE_TEST(
                        QC_QMP_MEM(aStatement)->alloc(
                            ID_SIZEOF(mtcColumn),
                            (void**)&(sColumn->basicInfo))
                        != IDE_SUCCESS);

                    // To Fix PR-10247
                    // QDX_SET_MTC_COLUMN(sColumn->basicInfo,
                    //                    sColumnInfo->basicInfo);

                    // To Fix PR-10247
                    // ���� ���� Macro�� ����� �ڷ� ������ ���濡
                    // �����ϰ� ��ó�� �� ����.

                    // fix BUG-33258
                    if( sColumn->basicInfo != sColumnInfo->basicInfo )
                    {
                        *(sColumn->basicInfo) = *(sColumnInfo->basicInfo);
                    }
                }
            }

            //-------------------------------------
            // check referenced index exist.
            //-------------------------------------

            sColumn = sRefSpec->referencedColList;

            // BUG-17126
            // Foreign Key Constraint�� Unique Constraint�� Primary Constraint��
            // �������� ������ �� �ִ�.
            for (i = 0; i < aTableInfo->uniqueKeyCount; i++)
            {
                sConstrType = aTableInfo->uniqueKeys[i].constraintType;

                // fix BUG-19187, BUG-19190
                if( (sConstrType == QD_PRIMARYKEY) ||
                    (sConstrType == QD_UNIQUE) )
                {
                    sIndexInfo = aTableInfo->uniqueKeys[i].constraintIndex;

                    if (qdn::matchColumnIdOutOfOrder(
                            sColumn,
                            (UInt*) smiGetIndexColumns(sIndexInfo->indexHandle),
                            sIndexInfo->keyColCount,
                            NULL ) == ID_TRUE)
                    {
                        sMatchIdxExist = ID_TRUE;
                        sRefSpec->referencedIndexID = sIndexInfo->indexId;
                        break;
                    }
                }
                else
                {
                    // LocalUnique Constraint�� ��쿡�� FK�� ������ �� ����.
                    // Nothing to do

                }
            }

            //------------------------------------------------------
            // if referenced index not exist, check constraint list.
            //------------------------------------------------------

            if (sMatchIdxExist == ID_FALSE)
            {
                for (sParentConstraint = sParseTree->constraints;
                     sParentConstraint != NULL;
                     sParentConstraint = sParentConstraint->next)
                {
                    // To Fix BUG-10341
                    if ( ( sParentConstraint->constrType == QD_PRIMARYKEY ) ||
                         ( sParentConstraint->constrType == QD_UNIQUE ) )
                    {
                        if ( qdn::matchColumnListOutOfOrder(
                                 sColumn,
                                 sParentConstraint->constraintColumns ) == ID_TRUE)
                        {
                            sMatchIdxExist = ID_TRUE;
                            sRefSpec->referencedIndexID = 0;
                            break;
                        }
                        else
                        {
                            // Nothing to do.
                        }
                    }
                    else
                    {
                        // nothing to do
                    }
                }
            }
            else
            {
                // Nothing to do.
            }

            IDE_TEST_RAISE(sMatchIdxExist == ID_FALSE, err_constraint_not_exist);
        }

        // referencing column�� referenced column�� ������ �����ؾ� �Ѵ�.
        for( sReferencedCol = sRefSpec->referencedColList,
                 sReferencedColCount = 0;
             sReferencedCol != NULL;
             sReferencedCol = sReferencedCol->next,
                 sReferencedColCount++ )
        {
            // Nothing to do.
        }

        sRefSpec->referencedColCount = sReferencedColCount;

        IDE_TEST_RAISE(
            sConstraintColCount != sRefSpec->referencedColCount,
            err_mismatched_column_count );

        if( sRefSpec->referencedIndexID == 0 )
        {
            if( sParentConstraint->constraintColumns !=
                sRefSpec->referencedColList )
            {
                // create table�� index�� foreign key�� �Բ� �����ϸ�
                // ������ �÷��� ����� ��쿡 foreign key ������
                IDE_TEST( reorderForeignKeySpec( sParentConstraint, aForeignKeyConstr ) );
            }
            else
            {
                // create table�� index�� foreign key�� �Բ� �����ϰų�
                // alter table�� foreign key�� �߰������� ���� ��
                // �÷��� ������� ���� ���
            }
        }
        else
        {
            // alter table�� �̹� �����ϴ� index�� foreign key ����
            IDE_TEST( reorderForeignKeySpec( sIndexInfo, aForeignKeyConstr ) );
        }

        // check column compatible
        sColumn = aForeignKeyConstr->constraintColumns;
        sReferencedCol = sRefSpec->referencedColList;
        sColumnCount = 0;

        while (sColumn != NULL)
        {
            IDE_TEST_RAISE( sReferencedCol == NULL,
                            err_mismatched_column_count );

            // fix BUG-10931
            // foreign key ������ ���� Ÿ�Գ����� ����.
            if( ( sColumn->basicInfo->type.dataTypeId ==
                  sReferencedCol->basicInfo->type.dataTypeId ) &&
                ( sColumn->basicInfo->type.languageId ==
                  sReferencedCol->basicInfo->type.languageId ) )
            {
                // Nothing to do
            }
            else
            {
                IDE_RAISE( err_not_compatible_type );
            }

            sRefSpec->referencedColID[sColumnCount] =
                sReferencedCol->basicInfo->column.id;
            sColumnCount ++;
            sColumn = sColumn->next;
            sReferencedCol = sReferencedCol->next;
        }
    } // self condition end.
    else
    {
        IDE_TEST(qcm::getTableInfo(aStatement,
                                   sRefSpec->referencedUserID,
                                   sRefSpec->referencedTableName,
                                   &sReferencedTableInfo,
                                   &sSCN,
                                   &sTableHandle)
                 != IDE_SUCCESS);

        IDE_TEST(qcm::lockTableForDDLValidation(aStatement,
                                                sTableHandle,
                                                sSCN)
                 != IDE_SUCCESS);

        // PROJ-1407 Temporary Table
        // temporary table���� referencing & referenced foreign key constraint
        // ��� ������ �� ����.
        if ( aTableInfo == NULL )
        {
            IDE_TEST_RAISE( ( ( sParseTree->flag & QDT_CREATE_TEMPORARY_MASK )
                              == QDT_CREATE_TEMPORARY_TRUE ) ||
                            ( qcuTemporaryObj::isTemporaryTable(
                                sReferencedTableInfo ) == ID_TRUE ),
                            ERR_CANNOT_CREATE_FOREIGN_KEY_ON_TEMPORARY_TABLE );
        }
        else
        {
            IDE_TEST_RAISE( ( qcuTemporaryObj::isTemporaryTable(
                                  aTableInfo ) == ID_TRUE ) ||
                            ( qcuTemporaryObj::isTemporaryTable(
                                sReferencedTableInfo ) == ID_TRUE  ),
                            ERR_CANNOT_CREATE_FOREIGN_KEY_ON_TEMPORARY_TABLE );
        }

        // check grant
        IDE_TEST( qdpRole::checkReferencesPriv( aStatement,
                                                aTableOwnerID,
                                                sReferencedTableInfo )
                  != IDE_SUCCESS );

        /* BUG-42881 
         * CHECK_FK_IN_CREATE_REPLICATION_DISABLE �� 1�� �����Ǿ� ������ 
         * Replication ��� ���̺��̾ FK �� �߰��ϰų� �����Ҽ� �ֽ��ϴ�.
         */                     
        if ( QCU_CHECK_FK_IN_CREATE_REPLICATION_DISABLE == 0 )
        {
            // if referenced tables is replicated, the error 
            IDE_TEST_RAISE( sReferencedTableInfo->replicationCount > 0,
                            ERR_DDL_WITH_REPLICATED_TABLE );
            //proj-1608:replicationCount�� 0�� �� recovery count�� �׻� 0�̾�� ��
            IDE_DASSERT( sReferencedTableInfo->replicationRecoveryCount == 0 );
        }
        else
        {
            /* do nothing */
        }
        
        sRefSpec->referencedTableID     = sReferencedTableInfo->tableID;
        sRefSpec->referencedTableInfo   = sReferencedTableInfo;
        sRefSpec->referencedTableHandle = sTableHandle;
        sRefSpec->referencedTableSCN    = sSCN;

        //-------------------------------------
        // check referenced columns exist.
        //-------------------------------------

        if (sRefSpec->referencedColList == NULL)
        {
            //---------------------------------------------
            // get primary key for referenced constraints.
            //----------------------------------------------

            IDE_TEST(getPrimaryKeyFromCache(QC_QMP_MEM(aStatement),
                                            sReferencedTableInfo,
                                            &sRefSpec->referencedColList,
                                            &sRefSpec->referencedColCount)
                     != IDE_SUCCESS);
        }
        else
        {
            //-----------------------------------------
            // referenced column specified.
            //-----------------------------------------

            for (sColumn = sRefSpec->referencedColList;
                 sColumn != NULL;
                 sColumn = sColumn->next)
            {
                IDE_TEST(qcmCache::getColumn(aStatement,
                                             sReferencedTableInfo,
                                             sColumn->namePos,
                                             &sColumnInfo)
                         != IDE_SUCCESS);

                IDU_LIMITPOINT("qdnForeignKey::validateForeignKeySpec::malloc3");
                IDE_TEST(
                    QC_QMP_MEM(aStatement)->alloc(ID_SIZEOF(mtcColumn),
                                                  (void**)&(sColumn->basicInfo))
                    != IDE_SUCCESS);

                // To Fix PR-10247
                // QDX_SET_MTC_COLUMN(sColumn->basicInfo,
                //                    sColumnInfo->basicInfo);

                // To Fix PR-10247
                // ���� ���� Macro�� ����� �ڷ� ������ ���濡
                // �����ϰ� ��ó�� �� ����.

                // fix BUG-33258
                if( sColumn->basicInfo != sColumnInfo->basicInfo )
                {
                    *(sColumn->basicInfo) = *(sColumnInfo->basicInfo);
                }
            }
        }

        //------------------------------------
        // check referenced index exist.
        //------------------------------------

        sColumn = sRefSpec->referencedColList;

        // BUG-17126
        // Foreign Key Constraint�� Unique Constraint�� Primary Constraint��
        // �������� ������ �� �ִ�.
        for (i = 0; i < sReferencedTableInfo->uniqueKeyCount; i++)
        {
            sConstrType = sReferencedTableInfo->uniqueKeys[i].constraintType;

            // fix BUG-19187, BUG-19190
            if( (sConstrType == QD_PRIMARYKEY) ||
                (sConstrType == QD_UNIQUE) )
            {
                sIndexInfo = 
                            sReferencedTableInfo->uniqueKeys[i].constraintIndex;

                if (qdn::matchColumnIdOutOfOrder(
                        sColumn,
                        (UInt*) smiGetIndexColumns( sIndexInfo->indexHandle ),
                        sIndexInfo->keyColCount,
                        NULL ) == ID_TRUE)
                {
                    sMatchIdxExist = ID_TRUE;
                    sRefSpec->referencedIndexID = sIndexInfo->indexId;
                    sRefSpec->referencedColCount = sIndexInfo->keyColCount;
                    break;
                }
            }
            else
            {
                // LocalUnique Constraint�� ��쿡�� FK�� ������ �� ����.
                // Nothing to do
            }
        }

        IDE_TEST_RAISE(sMatchIdxExist == ID_FALSE, err_constraint_not_exist);

        // PROJ-1509
        // referencing column�� referenced column�� ������ �����ؾ� �Ѵ�.
        IDE_TEST_RAISE( sConstraintColCount != sRefSpec->referencedColCount,
                        err_mismatched_column_count );

        IDE_TEST( reorderForeignKeySpec( sIndexInfo, aForeignKeyConstr ) );

        // check column compatible
        for (sColumn        = aForeignKeyConstr->constraintColumns,
                 sReferencedCol = sRefSpec->referencedColList,
                 sColumnCount   = 0;
             sColumn != NULL;
             sColumn        = sColumn->next,
                 sReferencedCol = sReferencedCol->next,
                 sColumnCount++)
        {
            // fix BUG-10931
            // foreign key ������ ���� Ÿ�Գ����� ����.
            if( ( sColumn->basicInfo->type.dataTypeId ==
                  sReferencedCol->basicInfo->type.dataTypeId ) &&
                ( sColumn->basicInfo->type.languageId ==
                  sReferencedCol->basicInfo->type.languageId ) )
            {
                // Nothing to do
            }
            else
            {
                IDE_RAISE( err_not_compatible_type );
            }

            sRefSpec->referencedColID[sColumnCount] =
                sReferencedCol->basicInfo->column.id;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_not_compatible_type);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDN_NOT_COMPATIBLE_TYPE));
    }
    IDE_EXCEPTION(err_constraint_not_exist);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDN_REFERENCED_CONSTRAINT_NOT_FOUND));
    }
    IDE_EXCEPTION(err_mismatched_column_count);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDN_MISMATCHED_REFERENCING_COLUMN_COUNT));
    }
    IDE_EXCEPTION(ERR_DDL_WITH_REPLICATED_TABLE);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDB_DDL_WITH_REPLICATED_TBL));
    }
    IDE_EXCEPTION(ERR_CANNOT_CREATE_FOREIGN_KEY_ON_TEMPORARY_TABLE);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDN_CANNOT_CREATE_FOREIGN_KEY_ON_TEMPORARY_TABLE));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qdnForeignKey::checkRef4AddConst( qcStatement      * aStatement,
                                  qcmTableInfo     * aTableInfo,
                                  qdConstraintSpec * aForeignSpec )
{
/***********************************************************************
 *
 * Description :
 *    To Fix PR-10385
 *    �̹� ������ Table�� ���Ͽ� Foreign Key ���� ������ �߰��� ��,
 *    ������ Data�� ���Ͽ� Foreign Key �˻縦 �ؾ� �Ѵ�.
 *
 * Implementation :
 *    ���� Table�� Record�� �о�
 *    Parent Table�� �����ϴ� ���� �˻��Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "qdnForeignKey::checkRef4AddConst"

    UInt              i;

    // ���� ���� ���� ������ ���� ���� ����
    qdReferenceSpec * sRefSpec;
    qcmTableInfo    * sParentTableInfo;
    qcmIndex        * sParentIndexInfo;

    // ���� ���� �˻� ��� ����� ���� ��������
    qcmForeignKey     sForeignKey;
    qcmParentInfo     sParentRefInfo;
    qcmColumn       * sConstrColumn;

    // Cursor�� ���� ��������
    smiTableCursor      sReadCursor;
    smiCursorProperties sCursorProperty;
    idBool              sCursorOpen;

    // Record �˻��� ���� ���� ����
    UInt                sRowSize;
    const void *        sRow;
    const void *        sOrgRow = NULL;
    scGRID              sRid;

    qcmColumn            * sColumnsForRow;

    iduMemoryStatus        sQmxMemStatus;
    qmsTableRef          * sTableRef;
    qcmPartitionInfoList * sPartInfoList;
    qcmPartitionInfoList * sPartInfoNode;
    qcmTableInfo         * sPartInfo;
    qcmPartitionInfoList   sFakePartitionInfoList;
    qdTableParseTree     * sParseTree;
    smiFetchColumnList   * sFetchColumnList;
    
    qmsIndexTableRef       sIndexTable;
    qcmIndex             * sIndexTableIndex;

    /* PROJ-2464 hybrid partitioned table ���� */
    qcmTableInfo         * sDiskPartInfo    = NULL;

    //---------------------------------------------
    // ���ռ� �˻�
    //---------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aTableInfo != NULL );
    IDE_DASSERT( aForeignSpec != NULL );
    IDE_DASSERT( aForeignSpec->constrType == QD_FOREIGN );
    IDE_DASSERT( aForeignSpec->referentialConstraintSpec != NULL );

    //---------------------------------------------
    // �ʱ�ȭ
    //---------------------------------------------

    sParseTree = (qdTableParseTree *)aStatement->myPlan->parseTree;    
    
    // To fix BUG-14818
    sReadCursor.initialize();

    sRefSpec = aForeignSpec->referentialConstraintSpec;
    sCursorOpen = ID_FALSE;

    //---------------------------------------------
    // Parent Table�� Index�� Meta Cache ���� ȹ��
    //---------------------------------------------

    IDE_TEST( getParentTableAndIndex( aStatement,
                                      sRefSpec,
                                      aTableInfo,
                                      & sParentTableInfo,
                                      & sParentIndexInfo )
              != IDE_SUCCESS );

    // PROJ-1624 non-partitioned index
    if( ( sParentTableInfo->tablePartitionType == QCM_PARTITIONED_TABLE ) &&
        ( sParentIndexInfo->indexPartitionType == QCM_NONE_PARTITIONED_INDEX ) )
    {
        IDE_TEST( qmsIndexTable::makeOneIndexTableRef( aStatement,
                                                       sParentIndexInfo,
                                                       & sIndexTable )
                  != IDE_SUCCESS );
        
        // index Table Validate and Lock
        IDE_TEST( qmsIndexTable::validateAndLockOneIndexTableRef( aStatement,
                                                                  &sIndexTable,
                                                                  SMI_TABLE_LOCK_S )
                  != IDE_SUCCESS );
        
        // key index�� ã�´�.
        IDE_TEST( qmsIndexTable::findKeyIndex( sIndexTable.tableInfo,
                                               & sIndexTableIndex )
                  != IDE_SUCCESS );
        
        sParentTableInfo = sIndexTable.tableInfo;
        sParentIndexInfo = sIndexTableIndex;
    }
    else
    {
        // Nothing to do.
    }

    //---------------------------------------------
    // �ڵ� ������ ���� ���� Meta Cache ����
    //---------------------------------------------

    //----------------------------
    // ���� Foreign Key Cache ����
    //----------------------------

    idlOS::memset( & sForeignKey, 0x00, ID_SIZEOF( qcmForeignKey ) );

    idlOS::strcpy( sForeignKey.name, "FOREIGN KEY" );  // Current Foreign Key
    sForeignKey.constraintID = 0;                      // No Meaning

    sForeignKey.constraintColumnCount = aForeignSpec->constrColumnCount;
    for ( i = 0, sConstrColumn = aForeignSpec->constraintColumns;
          i < aForeignSpec->constrColumnCount;
          i++, sConstrColumn = sConstrColumn->next )
    {
        sForeignKey.referencingColumn[i] = sConstrColumn->basicInfo->column.id;
    }

    sForeignKey.referenceRule = sRefSpec->referenceRule;
    sForeignKey.referencedTableID = sRefSpec->referencedTableID;
    sForeignKey.referencedIndexID = sRefSpec->referencedIndexID;

    //----------------------------
    // ���� Parent Cache ����
    //----------------------------

    idlOS::memset( & sParentRefInfo, 0x00, ID_SIZEOF( qcmParentInfo ) );

    IDU_LIMITPOINT("qdnForeignKey::checkRef4AddConst::malloc1");
    IDE_TEST(aStatement->qmxMem->cralloc(ID_SIZEOF(qmsTableRef),
                                         (void**)&sTableRef)
             != IDE_SUCCESS);

    sTableRef->tableHandle = sParentTableInfo->tableHandle;
    sTableRef->tableSCN = smiGetRowSCN( sParentTableInfo->tableHandle );
    sTableRef->tableInfo = sParentTableInfo;

    // PROJ-1502 PARTITIONED DISK TABLE
    if( sParentTableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
    {
        sTableRef->flag &= ~QMS_TABLE_REF_PRE_PRUNING_MASK;
        sTableRef->flag |= QMS_TABLE_REF_PRE_PRUNING_FALSE;

        IDE_TEST( qmoPartition::makePartitions( aStatement,
                                                sTableRef )
                  != IDE_SUCCESS );

        IDE_TEST( qcmPartition::validateAndLockPartitions(
                      aStatement,
                      sTableRef,
                      SMI_TABLE_LOCK_S )
                  != IDE_SUCCESS );

        /* PROJ-2464 hybrid partitioned table ���� */
        IDE_TEST( qcmPartition::makePartitionSummary( aStatement, sTableRef )
                  != IDE_SUCCESS );
    }

    sParentRefInfo.foreignKey = & sForeignKey;
    sParentRefInfo.parentIndex = sParentIndexInfo;
    sParentRefInfo.parentIndexTable = NULL;
    sParentRefInfo.parentIndexTableIndex = NULL;
    sParentRefInfo.parentTableRef = sTableRef;
    sParentRefInfo.next = NULL;

    // PROJ-1502 PARTITIONED DISK TABLE
    if ( aTableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
    {
        sPartInfoList = sParseTree->partTable->partInfoList;
    }
    else
    {
        sFakePartitionInfoList.partitionInfo = aTableInfo;
        sFakePartitionInfoList.next = NULL;

        sPartInfoList = &sFakePartitionInfoList;
    }

    //---------------------------------------------
    // Foreign Key ���� ���� �˻�
    //---------------------------------------------

    // ���� Table�� �����ϴ� Record�� �о�
    // Parent Table�� �����ϴ� �� Ȯ���Ѵ�.

    //----------------------------
    // Ŀ�� �ʱ�ȭ
    //----------------------------

    //----------------------------
    // Record ���� Ȯ��
    //----------------------------

    /* PROJ-2464 hybrid partitioned table ���� */
    for ( sPartInfoNode = sPartInfoList;
          sPartInfoNode != NULL;
          sPartInfoNode = sPartInfoNode->next )
    {
        if ( QCM_TABLE_TYPE_IS_DISK( sPartInfoNode->partitionInfo->tableFlag ) == ID_TRUE )
        {
            sDiskPartInfo = sPartInfoNode->partitionInfo;
            break;
        }
        else
        {
            /* Nothing to do */
        }
    }

    if ( sDiskPartInfo != NULL )
    {
        // Disk Table�� ���
        // Record Read�� ���� ������ �Ҵ��Ѵ�.
        // To Fix BUG-12977 : parent�� rowsize�� �ƴ�, �ڽ��� rowsize��
        //                    ������ �;���
        IDE_TEST( qdbCommon::getDiskRowSize( sDiskPartInfo,
                                             & sRowSize )
                  != IDE_SUCCESS );

        // To fix BUG-14820
        // Disk-variable �÷��� rid�񱳸� ���� �ʱ�ȭ �ؾ� ��.
        IDU_LIMITPOINT("qdnForeignKey::checkRef4AddConst::malloc2");
        IDE_TEST( aStatement->qmxMem->cralloc( sRowSize,
                                               (void **) & sRow )
                  != IDE_SUCCESS );
        
        sOrgRow = sRow;

        //--------------------------------------
        // PROJ-1705 fetch column list ����
        //--------------------------------------

        IDE_TEST( qdbCommon::makeFetchColumnList(
                      QC_PRIVATE_TMPLATE(aStatement),
                      sDiskPartInfo->columnCount,
                      sDiskPartInfo->columns,
                      ID_TRUE, // alloc smiColumnList
                      & sFetchColumnList ) != IDE_SUCCESS );
    }
    else
    {
        // Memory Table�� ���
        // Nothing to do.
    }

    for( sPartInfoNode = sPartInfoList;
         sPartInfoNode != NULL;
         sPartInfoNode = sPartInfoNode->next )
    {
        sPartInfo = sPartInfoNode->partitionInfo;

        sColumnsForRow = sPartInfo->columns;

        SMI_CURSOR_PROP_INIT_FOR_FULL_SCAN( &sCursorProperty, aStatement->mStatistics );
        
        if ( QCM_TABLE_TYPE_IS_DISK( sPartInfo->tableFlag ) == ID_TRUE )
        {
            sCursorProperty.mFetchColumnList = sFetchColumnList;
        }
        else
        {
            // Nothing To Do 
        }

        IDE_TEST(sReadCursor.open( QC_SMI_STMT( aStatement ),
                                   sPartInfo->tableHandle,
                                   NULL,
                                   smiGetRowSCN( sPartInfo->tableHandle),
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

        sCursorOpen = ID_TRUE;

        IDE_TEST(sReadCursor.beforeFirst() != IDE_SUCCESS);

        //----------------------------
        // �ݺ� �˻�
        //----------------------------

        IDE_TEST( sReadCursor.readRow( & sRow, & sRid, SMI_FIND_NEXT)
                  != IDE_SUCCESS );

        while ( sRow != NULL )
        {
            //------------------------------
            // ���� ���� �˻�
            //------------------------------

            // Memory ������ ���Ͽ� ���� ��ġ ���
            IDE_TEST_RAISE( aStatement->qmxMem->getStatus(&sQmxMemStatus) != IDE_SUCCESS, ERR_MEM_OP );

            IDE_TEST( checkParentRef4Const( aStatement,
                                            NULL,
                                            & sParentRefInfo,
                                            sRow,
                                            sColumnsForRow,
                                            0 )
                      != IDE_SUCCESS);

            // Memory ������ ���� Memory �̵�
            IDE_TEST_RAISE( aStatement->qmxMem->setStatus(&sQmxMemStatus) != IDE_SUCCESS, ERR_MEM_OP );

            IDE_TEST( sReadCursor.readRow( & sRow, & sRid, SMI_FIND_NEXT)
                      != IDE_SUCCESS );

        }
        sRow = sOrgRow;

        sCursorOpen = ID_FALSE;
        IDE_TEST( sReadCursor.close() != IDE_SUCCESS );

    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_MEM_OP )
    {
        ideLog::log( IDE_ERR_0,
                     "Unexpected errors may have occurred:"
                     " qdnForeignKey::checkRef4AddConst"
                     " memory error" );
    }
    IDE_EXCEPTION_END;

    if ( sCursorOpen == ID_TRUE )
    {
        (void) sReadCursor.close();
    }

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qdnForeignKey::checkRef4ModConst( qcStatement      * aStatement,
                                  qcmTableInfo     * aTableInfo,
                                  qcmForeignKey    * aForeignKey )
{
/***********************************************************************
 *
 * Description :
 *    PROJ-1874
 *    �̹� ������ Foreign Key�� ���Ͽ� Validation ���θ� ������ ��,
 *    ������ Data�� ���Ͽ� Foreign Key �˻縦 �ؾ� �Ѵ�.
 *
 * Implementation :
 *    ���� Table�� Record�� �о�
 *    Parent Table�� �����ϴ� ���� �˻��Ѵ�.
 *
 ***********************************************************************/

    UInt              i;

    // ���� ���� ���� ������ ���� ���� ����
    qcmTableInfo    * sParentTableInfo;
    qcmIndex        * sParentIndexInfo;
    qcmIndex        * sIndexInfo;
    
    // ���� ���� �˻� ��� ����� ���� ��������
    qcmParentInfo     sParentRefInfo;

    // Cursor�� ���� ��������
    smiTableCursor      sReadCursor;
    smiCursorProperties sCursorProperty;
    idBool              sCursorOpen;

    // Record �˻��� ���� ���� ����
    UInt                sRowSize;
    const void *        sRow;
    const void *        sOrgRow = NULL;
    scGRID              sRid;

    qcmColumn            * sColumnsForRow;

    iduMemoryStatus        sQmxMemStatus;
    qmsTableRef          * sTableRef;
    qcmPartitionInfoList * sPartInfoList;
    qcmPartitionInfoList * sPartInfoNode;
    qcmTableInfo         * sPartInfo;
    qcmPartitionInfoList   sFakePartitionInfoList;
    qdTableParseTree     * sParseTree;
    smiFetchColumnList   * sFetchColumnList;

    smSCN                  sParentTableSCN;
    void                 * sParentTableHandle;

    qmsIndexTableRef       sIndexTable;
    qcmIndex             * sIndexTableIndex;

    /* PROJ-2464 hybrid partitioned table ���� */
    qcmTableInfo         * sDiskPartInfo    = NULL;

    //---------------------------------------------
    // ���ռ� �˻�
    //---------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aTableInfo != NULL );
    IDE_DASSERT( aForeignKey != NULL );
    
    //---------------------------------------------
    // �ʱ�ȭ
    //---------------------------------------------

    sParseTree = (qdTableParseTree *)aStatement->myPlan->parseTree;    
    
    // To fix BUG-14818
    sReadCursor.initialize();

    sCursorOpen = ID_FALSE;

    //---------------------------------------------
    // Parent Table�� Index�� Meta Cache ���� ȹ��
    //---------------------------------------------

    IDE_TEST( qcm::getTableInfoByID( aStatement,
                                     aForeignKey->referencedTableID,
                                     & sParentTableInfo,
                                     & sParentTableSCN,
                                     & sParentTableHandle )
              != IDE_SUCCESS );

    // Parent Table Validate and Lock
    // Foreign Key ������ Execute �ܰ迡�� ȣ���ϴµ�, Parent Table�� �б⸸ �Ѵ�.
    IDE_TEST( qcm::validateAndLockTable( aStatement,
                                         sParentTableHandle,
                                         sParentTableSCN,
                                         SMI_TABLE_LOCK_S )
              != IDE_SUCCESS );
    
    sParentIndexInfo = NULL;

    // BUG-17126
    // Unique Index�� �ƴ϶� Unique Constraint�� Primary Constraint��
    // �־�� �Ѵ�.
    for ( i = 0; i < sParentTableInfo->uniqueKeyCount; i++ )
    {
        sIndexInfo = sParentTableInfo->uniqueKeys[i].constraintIndex;

        if( aForeignKey->referencedIndexID == sIndexInfo->indexId )
        {
            sParentIndexInfo = sIndexInfo;
            break;
        }
        else
        {
            // ���� ���� Column�� ���յ��� �ʴ� Index��
            // Go Go
        }
    }

    IDE_TEST_RAISE( sParentIndexInfo == NULL,
                    ERR_CONSTRAINT_NOT_EXIST );

    // PROJ-1624 non-partitioned index
    if( ( sParentTableInfo->tablePartitionType == QCM_PARTITIONED_TABLE ) &&
        ( sParentIndexInfo->indexPartitionType == QCM_NONE_PARTITIONED_INDEX ) )
    {
        IDE_TEST( qmsIndexTable::makeOneIndexTableRef( aStatement,
                                                       sParentIndexInfo,
                                                       & sIndexTable )
                  != IDE_SUCCESS );
        
        // index Table Validate and Lock
        IDE_TEST( qmsIndexTable::validateAndLockOneIndexTableRef( aStatement,
                                                                  &sIndexTable,
                                                                  SMI_TABLE_LOCK_S )
                  != IDE_SUCCESS );
        
        // key index�� ã�´�.
        IDE_TEST( qmsIndexTable::findKeyIndex( sIndexTable.tableInfo,
                                               & sIndexTableIndex )
                  != IDE_SUCCESS );
        
        sParentTableInfo = sIndexTable.tableInfo;
        sParentIndexInfo = sIndexTableIndex;
    }
    else
    {
        // Nothing to do.
    }

    //----------------------------
    // Parent Cache ����
    //----------------------------

    idlOS::memset( & sParentRefInfo, 0x00, ID_SIZEOF( qcmParentInfo ) );

    IDE_TEST(aStatement->qmxMem->cralloc(ID_SIZEOF(qmsTableRef),
                                         (void**)&sTableRef)
             != IDE_SUCCESS);

    sTableRef->tableHandle = sParentTableInfo->tableHandle;
    sTableRef->tableSCN = smiGetRowSCN( sParentTableInfo->tableHandle );
    sTableRef->tableInfo = sParentTableInfo;

    // PROJ-1502 PARTITIONED DISK TABLE
    if( sParentTableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
    {
        sTableRef->flag &= ~QMS_TABLE_REF_PRE_PRUNING_MASK;
        sTableRef->flag |= QMS_TABLE_REF_PRE_PRUNING_FALSE;

        IDE_TEST( qmoPartition::makePartitions( aStatement,
                                                sTableRef )
                  != IDE_SUCCESS );

        IDE_TEST( qcmPartition::validateAndLockPartitions(
                      aStatement,
                      sTableRef,
                      SMI_TABLE_LOCK_S )
                  != IDE_SUCCESS );

        /* PROJ-2464 hybrid partitioned table ���� */
        IDE_TEST( qcmPartition::makePartitionSummary( aStatement, sTableRef )
                  != IDE_SUCCESS );
    }

    sParentRefInfo.foreignKey  = aForeignKey;
    sParentRefInfo.parentIndex = sParentIndexInfo;
    sParentRefInfo.parentIndexTable = NULL;
    sParentRefInfo.parentIndexTableIndex = NULL;
    sParentRefInfo.parentTableRef = sTableRef;
    sParentRefInfo.next = NULL;

    // PROJ-1502 PARTITIONED DISK TABLE
    if ( aTableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
    {
        sPartInfoList = sParseTree->partTable->partInfoList;
    }
    else
    {
        sFakePartitionInfoList.partitionInfo = aTableInfo;
        sFakePartitionInfoList.next = NULL;

        sPartInfoList = &sFakePartitionInfoList;
    }

    //=========================================================================
    // Foreign Key ���� ���� �˻�
    //  ; ���� Table�� �����ϴ� Record�� �о� Parent Table�� �����ϴ� �� Ȯ���Ѵ�.
    //=========================================================================

    //----------------------------
    // Record ���� Ȯ��
    //----------------------------

    /* PROJ-2464 hybrid partitioned table ���� */
    for ( sPartInfoNode = sPartInfoList;
          sPartInfoNode != NULL;
          sPartInfoNode = sPartInfoNode->next )
    {
        if ( QCM_TABLE_TYPE_IS_DISK( sPartInfoNode->partitionInfo->tableFlag ) == ID_TRUE )
        {
            sDiskPartInfo = sPartInfoNode->partitionInfo;
            break;
        }
        else
        {
            /* Nothing to do */
        }
    }

    if ( sDiskPartInfo != NULL )
    {
        // Disk Table�� ���
        // Record Read�� ���� ������ �Ҵ��Ѵ�.
        // To Fix BUG-12977 : parent�� rowsize�� �ƴ�, �ڽ��� rowsize�� ������ �;���
        IDE_TEST( qdbCommon::getDiskRowSize( sDiskPartInfo,
                                             & sRowSize )
                  != IDE_SUCCESS );

        // To fix BUG-14820
        // Disk-variable �÷��� rid�񱳸� ���� �ʱ�ȭ �ؾ� ��.
        IDE_TEST( aStatement->qmxMem->cralloc( sRowSize,
                                               (void **) & sRow )
                  != IDE_SUCCESS );
        
        sOrgRow = sRow;

        //--------------------------------------
        // PROJ-1705 fetch column list ����
        //--------------------------------------

        IDE_TEST( qdbCommon::makeFetchColumnList(
                      QC_PRIVATE_TMPLATE(aStatement),
                      sDiskPartInfo->columnCount,
                      sDiskPartInfo->columns,
                      ID_TRUE, // alloc smiColumnList
                      & sFetchColumnList ) != IDE_SUCCESS );
    }
    else
    {
        // Memory Table�� ���
        // Nothing to do.
    }

    for( sPartInfoNode = sPartInfoList;
         sPartInfoNode != NULL;
         sPartInfoNode = sPartInfoNode->next )
    {
        sPartInfo = sPartInfoNode->partitionInfo;

        sColumnsForRow = sPartInfo->columns;

        //----------------------------
        // Ŀ�� �ʱ�ȭ
        //----------------------------
        SMI_CURSOR_PROP_INIT_FOR_FULL_SCAN( &sCursorProperty, aStatement->mStatistics );
        
        if ( QCM_TABLE_TYPE_IS_DISK( sPartInfo->tableFlag ) == ID_TRUE )
        {
            sCursorProperty.mFetchColumnList = sFetchColumnList;
        }
        else
        {
            // Nothing To Do
        }

        IDE_TEST(sReadCursor.open( QC_SMI_STMT( aStatement ),
                                   sPartInfo->tableHandle,
                                   NULL,
                                   smiGetRowSCN( sPartInfo->tableHandle),
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

        sCursorOpen = ID_TRUE;

        IDE_TEST(sReadCursor.beforeFirst() != IDE_SUCCESS);

        //----------------------------
        // �ݺ� �˻�
        //----------------------------

        IDE_TEST( sReadCursor.readRow( & sRow, & sRid, SMI_FIND_NEXT)
                  != IDE_SUCCESS );

        while ( sRow != NULL )
        {
            //------------------------------
            // ���� ���� �˻�
            //------------------------------

            // Memory ������ ���Ͽ� ���� ��ġ ���
            IDE_TEST_RAISE( aStatement->qmxMem->getStatus(&sQmxMemStatus) != IDE_SUCCESS, ERR_MEM_OP );

            IDE_TEST( checkParentRef4Const( aStatement,
                                            NULL,
                                            & sParentRefInfo,
                                            sRow,
                                            sColumnsForRow,
                                            0 )
                      != IDE_SUCCESS);

            // Memory ������ ���� Memory �̵�
            IDE_TEST_RAISE( aStatement->qmxMem->setStatus(&sQmxMemStatus) != IDE_SUCCESS, ERR_MEM_OP );

            IDE_TEST( sReadCursor.readRow( & sRow, & sRid, SMI_FIND_NEXT)
                      != IDE_SUCCESS );

        }
        sRow = sOrgRow;

        sCursorOpen = ID_FALSE;
        IDE_TEST( sReadCursor.close() != IDE_SUCCESS );

    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_MEM_OP )
    {
        ideLog::log( IDE_ERR_0,
                     "Unexpected errors may have occurred:"
                     " qdnForeignKey::checkRef4ModConst"
                     " memory error" );
    }
    IDE_EXCEPTION(ERR_CONSTRAINT_NOT_EXIST);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDN_REFERENCED_CONSTRAINT_NOT_FOUND));
    }
    IDE_EXCEPTION_END;

    if ( sCursorOpen == ID_TRUE )
    {
        (void) sReadCursor.close();
    }

    return IDE_FAILURE;
}

IDE_RC
qdnForeignKey::checkParentRef( qcStatement   * aStatement,
                               UInt          * aChangedColumns,
                               qcmParentInfo * aParentInfo,
                               mtcTuple      * aCheckedTuple,
                               const void    * aCheckedRow,
                               UInt            aUpdatedColCount )
{
/***********************************************************************
 *
 * Description :
 *      foreign key �� �ɷ��ִ� �÷��� ����ǰų� �߰��� ��, parent ��
 *      �ش� ���� �����ϴ��� �˻�
 *
 * Implementation :
 *      1. ����Ǵ� �÷��� foreign key �� �����ϰ� �ִ� �÷��̸�
 *      2. �����ϴ� ���̺� �� ���� �����ϰ� �ִ��� üũ
 *
 ***********************************************************************/

#define IDE_FN "qdnForeignKey::checkParentRef"

    qcmParentInfo *sParentInfo;
    sParentInfo = aParentInfo;

    IDE_DASSERT( aCheckedRow != NULL );    

    while (sParentInfo != NULL)
    {
        if ( qdn::intersectColumn(
                 sParentInfo->foreignKey->referencingColumn,
                 sParentInfo->foreignKey->constraintColumnCount,
                 aChangedColumns,
                 aUpdatedColCount ) == ID_TRUE)
        {
            IDE_TEST( searchParentKey( aStatement,
                                       sParentInfo,
                                       aCheckedTuple,
                                       aCheckedRow,
                                       NULL )
                      != IDE_SUCCESS);
        }
        sParentInfo = sParentInfo->next;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qdnForeignKey::checkParentRef4Const( qcStatement   * aStatement,
                                            UInt          * aChangedColumns,
                                            qcmParentInfo * aParentInfo,
                                            const void    * aCheckedRow,
                                            qcmColumn     * aColumnsForCheckedRow,
                                            UInt            aUpdatedColCount )
{
/***********************************************************************
 *
 * Description :
 *      foreign key �� �ɷ��ִ� �÷��� ����ǰų� �߰��� ��, parent ��
 *      �ش� ���� �����ϴ��� �˻�
 *
 * Implementation :
 *      1. ����Ǵ� �÷��� foreign key �� �����ϰ� �ִ� �÷��̸�
 *      2. �����ϴ� ���̺� �� ���� �����ϰ� �ִ��� üũ
 *
 ***********************************************************************/

#define IDE_FN "qdnForeignKey::checkParentRef4Const"

    qcmParentInfo *sParentInfo;
    sParentInfo = aParentInfo;

    IDE_DASSERT( aCheckedRow != NULL );    

    while (sParentInfo != NULL)
    {
        if ( qdn::intersectColumn(
                 sParentInfo->foreignKey->referencingColumn,
                 sParentInfo->foreignKey->constraintColumnCount,
                 aChangedColumns,
                 aUpdatedColCount ) == ID_TRUE)
        {
            IDE_TEST( searchParentKey( aStatement,
                                       sParentInfo,
                                       NULL,
                                       aCheckedRow,
                                       aColumnsForCheckedRow )
                      != IDE_SUCCESS);
        }
        else
        {
            /* Nothing to do */
        }

        sParentInfo = sParentInfo->next;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qdnForeignKey::checkChildRefOnDelete( qcStatement     * aStatement,
                                      qcmRefChildInfo * aChildConstraints,
                                      UInt              aParentTableID,
                                      mtcTuple        * aCheckedTuple,
                                      const void      * aCheckedRow,
                                      idBool            aIsOnDelete )
{
/***********************************************************************
 *
 * Description :
 *    ������ ���ڵ带 �����ϰ� �ִ� foreign key �� �ִ��� �˻�
 *
 * Implementation :
 *    searchForeignKey �Լ� ȣ��
 *       child ���̺��� �� ���� �����ϰ� �ִ� ���ڵ尡 �ִ��� üũ
 *       -> cascade�� �ƴϰ� �����ϴ� ���ڵ尡 �ִٸ� ���� �߻�
 *       -> cascade�̸� �ش� ���ڵ� delete
 *
 ***********************************************************************/

#define IDE_FN "qdnForeignKey::checkChildRefOnDelete"

    qcmRefChildInfo     * sChildInfo;

    //-------------------------------
    // 1. ���� ����� ID_TRUE
    // 2. ON DELETE SET NULL �̸� UPDATE NULL �� ���� �� ����
    //    grand child �� ���ڵ� ���� ���� üũ ���� ����.
    // 3. grand child �� Ȯ�� grand child null�� ��� for loop ���� �Ǹ� �ʵ�.
    // ex>
    //       T1 : I1 (1)
    //            ^
    //            |
    //       T2 : I1 (1)
    //            ^
    //            |
    //       T3 : I1 (NULL) ----> FOR LOOP ���� ID_FALSE
    //            ^               update null ���� �Ǿ�� �ϳ� ���� ���� ����.
    //            |
    //       T4 : I1 (1)
    //-------------------------------
    
    if ( aIsOnDelete == ID_TRUE )
    {
        for (sChildInfo  = aChildConstraints;
             sChildInfo != NULL;
             sChildInfo = sChildInfo->next)
        {
            if( sChildInfo->parentTableID == aParentTableID )
            {
                IDE_TEST( searchForeignKey( aStatement,
                                            aChildConstraints,
                                            sChildInfo,
                                            aCheckedTuple,
                                            aCheckedRow,
                                            ID_TRUE )
                          != IDE_SUCCESS);
            }
            else
            {
                // Nothing to do.
            }
        }
    }
    else
    {
        if ( aChildConstraints != NULL )
        {
            IDE_TEST(searchForeignKey( aStatement,
                                       aChildConstraints,
                                       aChildConstraints,
                                       aCheckedTuple,
                                       aCheckedRow,
                                       ID_FALSE )
                     != IDE_SUCCESS);
        }
        else
        {
            // Nothing To Do
        }
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qdnForeignKey::checkChildRefOnUpdate( qcStatement     * aStatement,
                                      qmsTableRef     * aTableRef,
                                      qcmTableInfo    * aTableInfo,
                                      UInt            * aChangedColumns,
                                      qcmRefChildInfo * aChildConstraints,
                                      UInt              aParentTableID,
                                      mtcTuple        * aCheckedTuple,
                                      const void      * aCheckedRow,
                                      UInt              aChangedColCount )
{
/***********************************************************************
 *
 * Description :
 *  qmx::update �κ��� ȣ��, ���̺� update �ÿ� �����
 *  child ���̺��� �����ϰ� �ִ� ���� ����Ǿ� �������� �ʴ��� �˻���
 *
 * Implementation :
 *    1. ����Ǵ� �÷��� reference �ǰ� �ִ� �÷��̸�
 *    2. searchSelf : parent ���̺��� ������ ���� �����Ŀ� ��������?
 *       -> ���࿡ �����Ѵٸ� searchForeignKey �� �� �ʿ䰡 ����
 *    3. 2�� ������� �������� �ʴ´ٸ�, searchForeignKey �Լ� ȣ��
 *       child ���̺��� �� ���� �����ϰ� �ִ� ���ڵ尡 �ִ��� üũ
 *       -> �ִٸ� ���� �߻�
 *
 ***********************************************************************/

#define IDE_FN "qdnForeignKey::checkChildRefOnUpdate"

    qcmRefChildInfo  * sChildInfo;

    for (sChildInfo = aChildConstraints;
         sChildInfo != NULL;
         sChildInfo = sChildInfo->next)
    {
        if( sChildInfo->parentTableID == aParentTableID )
        {
            if ( qdn::intersectColumn( sChildInfo->parentIndex->keyColumns,
                                       sChildInfo->parentIndex->keyColCount,
                                       aChangedColumns,
                                       aChangedColCount )
                 == ID_TRUE)
            {
                // in case of changing unique column.
                if ( searchSelf( aStatement,
                                 aTableRef,
                                 aTableInfo,
                                 sChildInfo->parentIndex,
                                 aCheckedTuple,
                                 aCheckedRow ) != IDE_SUCCESS)
                {
                    IDE_TEST(searchForeignKey( aStatement,
                                               aChildConstraints,
                                               sChildInfo,
                                               aCheckedTuple,
                                               aCheckedRow,
                                               ID_FALSE )
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
        else
        {
            // Nothing to do.
        }
    }
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}


idBool
qdnForeignKey::haveToOpenBeforeCursor( qcmRefChildInfo  * aChildInfo,
                                       UInt             * aChangedColumns,
                                       UInt               aChangedColCount )
{
/***********************************************************************
 *
 * Description :
 *  qmx::update, qmx::executeDelete �Լ����� ȣ��,
 *  �����ϰų� �����ϰ��� �ϴ� �÷��� �����ϰ� �ִ� child �� ������ �˻�
 *
 * Implementation :
 *    �����ϰų� �����ϰ��� �ϴ� �÷��� �����ϰ� �ִ� child �� ������
 *    ID_TRUE �� ��ȯ, �׷��� ������ ID_FALSE �� ��ȯ
 *
 ***********************************************************************/

#define IDE_FN "qdnForeignKey::haveToOpenBeforeCursor"

    idBool             sRetval    = ID_FALSE;
    qcmRefChildInfo  * sChildInfo = aChildInfo;

    while (sChildInfo != NULL)
    {
        if ( qdn::intersectColumn( sChildInfo->parentIndex->keyColumns,
                                   sChildInfo->parentIndex->keyColCount,
                                   aChangedColumns,
                                   aChangedColCount ) == ID_TRUE )
        {
            sRetval = ID_TRUE;
            break;
        }
        else
        {
            /* Nothing to do */
        }

        sChildInfo = sChildInfo->next;

    }

    return sRetval;
    
#undef IDE_FN
}

idBool
qdnForeignKey::haveToCheckParent( qcmTableInfo * aTableInfo,
                                  UInt         * aChangedColumns,
                                  UInt           aChangedColCount )
{
/***********************************************************************
 *
 * Description :
 *      ����Ǵ� �÷��� foreign key �� �����ϰ� �ִ� �÷����� �˻�
 *
 * Implementation :
 *      1. intersectColumn ȣ��
 *
 ***********************************************************************/

#define IDE_FN "qdnForeignKey::haveToCheckParent"

    UInt i;
    idBool sRetVal = ID_FALSE;

    for (i = 0; i < aTableInfo->foreignKeyCount; i++)
    {
        if ( qdn::intersectColumn(
                 aTableInfo->foreignKeys[i].referencingColumn,
                 aTableInfo->foreignKeys[i].constraintColumnCount,
                 aChangedColumns,
                 aChangedColCount ) == ID_TRUE)
        {
            sRetVal = ID_TRUE;
            break;
        }
    }

    return sRetVal;

#undef IDE_FN
}

IDE_RC
qdnForeignKey::getPrimaryKeyFromDefinition( qdTableParseTree * aParseTree,
                                            qdReferenceSpec  * aReferenceSpec )
{
/***********************************************************************
 *
 * Description :
 *    Parse Tree �� qdConstraintSpec ���� primary key �� ã�Ƽ�
 *    qdReferenceSpec �� �÷��� �ο�
 *
 * Implementation :
 *    1. Parse Tree �� qdConstraintSpec ���� primary key ã��
 *    2. aReferenceSpec �� �÷��� 1 ���� ã�� primary key �� �÷� �ο�
 *    3. ������ ����
 *
 ***********************************************************************/

#define IDE_FN "qdnForeignKey::getPrimaryKeyFromDefinition"

    qdConstraintSpec *sConstraintSpec;
    aReferenceSpec->referencedColList = NULL;
    sConstraintSpec = aParseTree->constraints;

    while (sConstraintSpec != NULL)
    {
        if (sConstraintSpec->constrType == QD_PRIMARYKEY)
        {
            aReferenceSpec->referencedColList =
                sConstraintSpec->constraintColumns;
            break;
        }
        else
        {
            sConstraintSpec = sConstraintSpec->next;
        }
    }

    IDE_TEST_RAISE(aReferenceSpec->referencedColList == NULL,
                   err_constraint_not_exist);

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_constraint_not_exist);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDN_REFERENCED_CONSTRAINT_NOT_FOUND));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qdnForeignKey::getPrimaryKeyFromCache( iduVarMemList * aMem,
                                       qcmTableInfo  * aTableInfo,
                                       qcmColumn    ** aColumns,
                                       UInt          * aColCount )
{
/***********************************************************************
 *
 * Description :
 *    validateReferenceSpec ���� ȣ��, PK �� �����ϰ� �ִ� �÷� ���ϱ�
 *
 * Implementation :
 *    qcmTableInfo �� primaryKey �� �����ϴ� �÷��� qcmColumn ����Ʈ
 *    ���·� ��ȯ�Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "qdnForeignKey::getPriamryKeyFromCache"

    UInt i;
    // SInt sColumnOrder;
    qcmColumn * sColumn = NULL;
    qcmColumn * sLastColumn = NULL;

    IDE_TEST_RAISE(aTableInfo->primaryKey == NULL, err_constraint_not_exist);

    for (i = 0; i < aTableInfo->primaryKey->keyColCount; i ++)
    {
        IDU_LIMITPOINT("qdnForeignKey::getPrimaryKeyFromCache::malloc");
        IDE_TEST(aMem->alloc(ID_SIZEOF(qcmColumn), (void**)&sColumn)
                 != IDE_SUCCESS);

        sColumn->basicInfo = & (aTableInfo->primaryKey->keyColumns[i]);
        sColumn->next = NULL;

        if (sLastColumn == NULL)
        {
            *aColumns = sColumn;
        }
        else
        {
            sLastColumn->next = sColumn;
        }
        sLastColumn = sColumn;
    }
    *aColCount = aTableInfo->primaryKey->keyColCount;

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_constraint_not_exist);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDN_REFERENCED_CONSTRAINT_NOT_FOUND));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qdnForeignKey::getParentTableAndIndex( qcStatement     * aStatement,
                                       qdReferenceSpec * aRefSpec,
                                       qcmTableInfo    * aChildTableInfo,
                                       qcmTableInfo   ** aParentTableInfo,
                                       qcmIndex       ** aParentIndexInfo )
{
/***********************************************************************
 *
 * Description :
 *
 *    Foreign Key ���� �������� ����
 *    Parent Table�� Meta Cache ������
 *    ���� ���ǰ� ���õ� Index �� Meta Cache ������ �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qdnForeignKey::getParentTableAndIndex"

    UInt              i;

    qcmTableInfo    * sParentTableInfo;
    qcmIndex        * sParentIndexInfo;
    qcmIndex        * sIndexInfo;

    //---------------------------------------------
    // ���ռ� �˻�
    //---------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aRefSpec != NULL );

    //---------------------------------------------
    // Parent Table�� Meta Cache ���� ȹ��
    //---------------------------------------------

    sParentTableInfo = NULL;

    if ( aRefSpec->referencedTableID == UINT_MAX )
    {
        //-----------------------------------
        // Self Reference�� ���
        //-----------------------------------

        sParentTableInfo = aChildTableInfo;
    }
    else
    {
        //-----------------------------------
        // �ٸ� Table�� �����ϴ� ���
        //-----------------------------------

        // Foreign Key �߰��� Execute �ܰ迡�� ȣ���ϴµ�, Parent Table�� �б⸸ �Ѵ�.
        IDE_TEST( qcm::validateAndLockTable( aStatement,
                                             aRefSpec->referencedTableHandle,
                                             aRefSpec->referencedTableSCN,
                                             SMI_TABLE_LOCK_S )
                      != IDE_SUCCESS );

        sParentTableInfo = aRefSpec->referencedTableInfo;
    }

    //---------------------------------------------
    // Parent Index�� Meta Cache ���� ȹ��
    //---------------------------------------------

    sParentIndexInfo = NULL;

    // BUG-17126
    // Unique Index�� �ƴ϶� Unique Constraint�� Primary Constraint��
    // �־�� �Ѵ�.
    for ( i = 0; i < sParentTableInfo->uniqueKeyCount; i++ )
    {
        sIndexInfo = sParentTableInfo->uniqueKeys[i].constraintIndex;

        if ( qdn::matchColumnId(
                 aRefSpec->referencedColList,
                 (UInt*) smiGetIndexColumns( sIndexInfo->indexHandle ),
                 sIndexInfo->keyColCount,
                 NULL )
             == ID_TRUE)
        {
            // Foreign Key ���� ���� �÷���
            // ��Ȯ�� �����ϴ� Index ��
            sParentIndexInfo = sIndexInfo;
            break;
        }
        else
        {
            // ���� ���� Column�� ���յ��� �ʴ� Index��
            // Go Go
        }
    }

    IDE_TEST_RAISE( sParentIndexInfo == NULL,
                    err_constraint_not_exist );

    *aParentTableInfo = sParentTableInfo;
    *aParentIndexInfo = sParentIndexInfo;

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_constraint_not_exist);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDN_REFERENCED_CONSTRAINT_NOT_FOUND));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qdnForeignKey::searchParentKey(
    qcStatement   * aStatement,
    qcmParentInfo * aParentInfo,
    mtcTuple      * aCheckedTuple,
    const void    * aCheckedRow,
    qcmColumn     * aColumnsForCheckedRow )
{
/***********************************************************************
 *
 * Description :
 *  checkReferentialParent �κ��� ȣ��, ����Ű�� �ɷ��ִ� �÷���
 *  insert �ϰų�, update �� ��� �� ���� parent ���̺� �����ϴ��� �˻�
 *  DDL(Foreign Key �߰�/����)�� DML���� ȣ��ȴ�.
 *
 * Implementation :
 *    1. insert �Ǵ� update �Ǵ� ���� �� ������ �˻�
 *    2. �� ���� �ƴϸ�, �� ���� parent ���̺� �����ϴ��� �˻�,
 *       �������� ������ ���� �߻�
 *
 ***********************************************************************/

#define IDE_FN "qdnForeignKey::searchParentKey"

    UInt                    i;
    SInt                    sColumnOrder;
    mtcColumn             * sConstraintColumn;
    qcmTableInfo          * sParentTableInfo;
    qcmIndex              * sParentIndex;
    qtcMetaRangeColumn      sRangeColumn[QC_MAX_KEY_COLUMN_COUNT];
    smiRange                sRange;
    const void            * sRow;
    idBool                  sHasNullValue = ID_FALSE;
    smiTableCursor          sCursor;
    smiCursorProperties     sCursorProperty;
    scGRID                  sRid; // Disk Table�� ���� Record IDentifier
    idBool                  sIsCursorOpened = ID_FALSE;

    UInt                    sRowSize;
    mtcColumn             * sKeyColumn;

    qmsPartCondValList      sPartCondVal;
    UInt                    sPartOrder;

    UInt                    sIndexKeyOrder;
    qmsTableRef           * sParentTableRef = aParentInfo->parentTableRef;
    qcmTableInfo          * sSelectedTableInfo;
    qmsPartitionRef       * sSelectedPartitionRef;
    void                  * sIndexHandle = NULL;
    UInt                    sIndexType = SMI_BUILTIN_SEQUENTIAL_INDEXTYPE_ID; // Default
    UInt                    sCompareType;
    void                  * sValueTemp;
    smiFetchColumnList    * sFetchColumnList = NULL;
    UInt                    sLockRowType = QCU_FOREIGN_KEY_LOCK_ROW;
    smiTableLockMode        sLockMode    = SMI_TABLE_LOCK_IS;

    // fix BUG-39754
    if( sLockRowType == 2 )
    {
        sLockMode = SMI_TABLE_LOCK_S;
    }
    else
    {
        /* Nothing to do */
    }

    // fix BUG-12874
    IDE_TEST( qcm::validateAndLockTable( aStatement,
                                         sParentTableRef->tableHandle,
                                         sParentTableRef->tableSCN,
                                         sLockMode )
              != IDE_SUCCESS );

    sParentTableInfo  = sParentTableRef->tableInfo;
    sParentIndex      = aParentInfo->parentIndex;

    // PROJ-1502 PARTITIONED DISK TABLE
    if( sParentTableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
    {
        // PROJ-1624 non-partitioned index
        if ( sParentIndex->indexPartitionType == QCM_NONE_PARTITIONED_INDEX )
        {
            IDE_DASSERT( aParentInfo->parentIndexTable != NULL );

            IDE_TEST( qmsIndexTable::validateAndLockOneIndexTableRef( aStatement,
                                                                      aParentInfo->parentIndexTable,
                                                                      sLockMode )
                      != IDE_SUCCESS );
    
            // parent table�� index table�� ��ü�Ѵ�.
            sParentTableInfo = aParentInfo->parentIndexTable->tableInfo;
            sParentIndex     = aParentInfo->parentIndexTableIndex;
        }
        else
        {
            IDE_TEST( qcmPartition::validateAndLockPartitions(
                          aStatement,
                          sParentTableRef,
                          sLockMode )
                      != IDE_SUCCESS );
        }
    }
    else
    {
        // Nothing to do.
    }

    sCursor.initialize();

    //----------------------------------------------
    // parent table�� ���� Cursor ����
    //----------------------------------------------
    
    IDU_LIMITPOINT("qdnForeignKey::searchParentKey::malloc1");
    IDE_TEST( QC_QMX_MEM( aStatement )->alloc(
                  ID_SIZEOF( mtcColumn ) *
                  aParentInfo->foreignKey->constraintColumnCount,
                  (void**) &sKeyColumn) != IDE_SUCCESS);

    sPartCondVal.partCondValCount = 0;
    sPartCondVal.partCondValType = QMS_PARTCONDVAL_NORMAL;

    // null check.
    for ( i = 0; i < aParentInfo->foreignKey->constraintColumnCount; i++ )
    {
        sColumnOrder = aParentInfo->foreignKey->referencingColumn[i] &
            SMI_COLUMN_ID_MASK;

        if ( aCheckedTuple != NULL )
        {
            sConstraintColumn = &( aCheckedTuple->columns[sColumnOrder] );
        }
        else
        {
            // for DDL
            sConstraintColumn = aColumnsForCheckedRow[sColumnOrder].basicInfo;
        }

        sValueTemp = (void*)mtc::value( sConstraintColumn,
                                        aCheckedRow,
                                        MTD_OFFSET_USE );

        if ( sConstraintColumn->module->isNull( sConstraintColumn,
                                                sValueTemp ) == ID_TRUE)
        {
            sHasNullValue = ID_TRUE;
            break;
        }
        else
        {
            if( sParentTableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
            {
                if( i < sParentTableInfo->partKeyColCount )
                {
                    sPartCondVal.partCondValCount++;
                    sPartCondVal.partCondValType = QMS_PARTCONDVAL_NORMAL;
                    sPartCondVal.partCondValues[i] =
                        (void*)mtc::value( sConstraintColumn,
                                           aCheckedRow,
                                           MTD_OFFSET_USE );
                }
            }
        }
    }

    // if null valued column(s) exist(s), return ok. else search parent.
    if ( sHasNullValue != ID_TRUE )
    {
        // PROJ-1502 PARTITIONED DISK TABLE
        if( sParentTableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
        {
            switch( sParentTableInfo->partitionMethod )
            {
                case QCM_PARTITION_METHOD_RANGE:
                    {
                        IDE_TEST( qmoPartition::rangePartitionFilteringWithValues(
                                      sParentTableRef,
                                      &sPartCondVal,
                                      &sSelectedPartitionRef )
                                  != IDE_SUCCESS );
                    }
                    break;
                case QCM_PARTITION_METHOD_LIST:
                    {
                        IDE_TEST( qmoPartition::listPartitionFilteringWithValue(
                                      sParentTableRef,
                                      sPartCondVal.partCondValues[0],
                                      &sSelectedPartitionRef )
                                  != IDE_SUCCESS );
                    }
                    break;
                case QCM_PARTITION_METHOD_HASH:
                    {
                        IDE_TEST( qmoPartition::getPartOrder(
                                      sParentTableRef->tableInfo->partKeyColumns,
                                      sParentTableRef->partitionCount,
                                      &sPartCondVal,
                                      &sPartOrder )
                                  != IDE_SUCCESS );

                        IDE_TEST( qmoPartition::hashPartitionFilteringWithPartOrder(
                                      sParentTableRef,
                                      sPartOrder,
                                      &sSelectedPartitionRef )
                                  != IDE_SUCCESS );
                    }
                    break;
                /* BUG-46065 support range using hash */
                case QCM_PARTITION_METHOD_RANGE_USING_HASH:
                    {
                        IDE_TEST( qmoPartition::getPartOrder( sParentTableRef->tableInfo->partKeyColumns,
                                                              QMO_RANGE_USING_HASH_MAX_VALUE,
                                                              &sPartCondVal,
                                                              &sPartOrder )
                                  != IDE_SUCCESS );
                        sPartCondVal.partCondValCount = 1;
                        sPartCondVal.partCondValType  = QMS_PARTCONDVAL_NORMAL;
                        sPartCondVal.partCondValues[0] = &sPartOrder;
                        IDE_TEST( qmoPartition::rangeUsingHashPartitionFilteringWithValues(
                                      sParentTableRef,
                                      &sPartCondVal,
                                      &sSelectedPartitionRef )
                                  != IDE_SUCCESS );
                    }
                    break;
                default:
                    IDE_ASSERT(0);
                    break;
            }

            sSelectedTableInfo = sSelectedPartitionRef->partitionInfo;

            for( i = 0; i < sSelectedTableInfo->indexCount; i++ )
            {
                if( sParentIndex->indexId == sSelectedTableInfo->indices[i].indexId )
                {
                    sParentIndex = &sSelectedTableInfo->indices[i];
                    sIndexHandle = sSelectedTableInfo->indices[i].indexHandle;
                    sIndexType = sSelectedTableInfo->indices[i].indexTypeId;
                    break;
                }
            }

            IDE_DASSERT( sIndexHandle != NULL );
        }
        else
        {
            sSelectedTableInfo = sParentTableInfo;
            sIndexHandle = sParentIndex->indexHandle;
            sIndexType = sParentIndex->indexTypeId;
        }

        if ( QCM_TABLE_TYPE_IS_DISK( sSelectedTableInfo->tableFlag ) == ID_TRUE )
        {
            // Disk Table�� ���
            // Record Read�� ���� ������ �Ҵ��Ѵ�.
            IDE_TEST( qdbCommon::getDiskRowSize( sSelectedTableInfo,
                                                 & sRowSize )
                      != IDE_SUCCESS );

            // To fix BUG-14820
            // Disk-variable �÷��� rid�񱳸� ���� �ʱ�ȭ �ؾ� ��.
            IDU_LIMITPOINT("qdnForeignKey::searchParentKey::malloc2");
            IDE_TEST( aStatement->qmxMem->cralloc( sRowSize,
                                                   (void **) & sRow )
                      != IDE_SUCCESS );

            // PROJ-1509
            // parent row�� ���� �������Ἲ �˻縦 ����
            // select�� �����ϰ�
            // select�� row�� variable column�� �������� �ʱ⶧����,
            // variable column�� �о�� value point�� �����ϴ� ����
            // (smiColumn->value�� ����)�� ���� �ʾƵ� ��.
        }
        else
        {
            // Memory Table�� ���
            // Nothing To Do
        }

        // PROJ-1872
        // index�� �ִ� Į���� meta range�� ���� �Ǹ�
        // disk index column�� compare�� stored type�� mt type ���� ���̴�.
        if ( QCM_TABLE_TYPE_IS_DISK( sSelectedTableInfo->tableFlag ) == ID_TRUE )
        {
            sCompareType = MTD_COMPARE_STOREDVAL_MTDVAL;
        }
        else
        {
            sCompareType = MTD_COMPARE_MTDVAL_MTDVAL;
        }

        qtc::initializeMetaRange( &sRange, sCompareType );

        for ( i = 0; i < aParentInfo->foreignKey->constraintColumnCount; i++ )
        {
            sColumnOrder = aParentInfo->foreignKey->referencingColumn[i] & SMI_COLUMN_ID_MASK;

            if ( aCheckedTuple != NULL )
            {
                sConstraintColumn = &( aCheckedTuple->columns[sColumnOrder] );
            }
            else
            {
                // for DDL
                sConstraintColumn = aColumnsForCheckedRow[sColumnOrder].basicInfo;
            }

            // To Fix PR-20839
            // Key Range ���� ��
            // Index Key Column�� Order�� ����Ͽ��� ��.
            sIndexKeyOrder = sParentIndex->keyColsFlag[i] & SMI_COLUMN_ORDER_MASK;

            idlOS::memcpy( &(sKeyColumn[i]),
                           &sParentIndex->keyColumns[i],
                           ID_SIZEOF(mtcColumn) );

            // Bug-11178 fix, by kumdory
            // aCheckedRow�κ���
            // ���� offset�� ���ؼ� value�� ������ �ȵ�.
            // variable �÷��� ���� ���� ���� �� ������.
            // �ݵ�� value�� ��� ���ؼ���
            // mtc::value�� ���ؼ� ���� ��.
            qtc::setMetaRangeColumn(
                &sRangeColumn[i],
                &sKeyColumn[i],
                mtc::value( sConstraintColumn,
                            aCheckedRow,
                            MTD_OFFSET_USE ),
                sIndexKeyOrder,
                i ); //Column Idx

            qtc::addMetaRangeColumn( &sRange, &sRangeColumn[i] );
        }

        qtc::fixMetaRange( &sRange );

        SMI_CURSOR_PROP_INIT_FOR_INDEX_SCAN( &sCursorProperty,
                                             aStatement->mStatistics,
                                             sIndexType );
        
        // PROJ-1705
        if ( QCM_TABLE_TYPE_IS_DISK( sSelectedTableInfo->tableFlag ) == ID_TRUE )
        {
            IDE_TEST( qdbCommon::makeFetchColumnList4Index(
                          QC_PRIVATE_TMPLATE(aStatement),
                          sSelectedTableInfo,
                          sParentIndex,
                          ID_TRUE,
                          & sFetchColumnList ) != IDE_SUCCESS );

            sCursorProperty.mFetchColumnList = sFetchColumnList;
        }
        else
        {
            // Nothing to do.
        }        

        // fix BUG-39754
        if( sLockRowType == 1 )
        {
            IDE_TEST( sCursor.open( QC_SMI_STMT( aStatement ),
                            sSelectedTableInfo->tableHandle,
                            sIndexHandle,
                            smiGetRowSCN(sSelectedTableInfo->tableHandle),
                            NULL,
                            &sRange,
                            smiGetDefaultKeyRange(),
                            smiGetDefaultFilter(),
                            SMI_LOCK_WRITE | SMI_TRAVERSE_FORWARD | SMI_PREVIOUS_DISABLE,
                            SMI_LOCKROW_CURSOR, // referntial constraint BUG-17940
                            & sCursorProperty )
                      != IDE_SUCCESS);
        }
        else
        {
            IDE_TEST( sCursor.open( QC_SMI_STMT( aStatement ),
                            sSelectedTableInfo->tableHandle,
                            sIndexHandle,
                            smiGetRowSCN(sSelectedTableInfo->tableHandle),
                            NULL,
                            &sRange,
                            smiGetDefaultKeyRange(),
                            smiGetDefaultFilter(),
                            SMI_TRAVERSE_FORWARD|SMI_PREVIOUS_DISABLE,
                            SMI_LOCKROW_CURSOR,
                            & sCursorProperty )
                      != IDE_SUCCESS );
        }
        sIsCursorOpened = ID_TRUE;

        IDE_TEST(sCursor.beforeFirst() != IDE_SUCCESS);

        IDE_TEST(sCursor.readRow(&sRow, &sRid, SMI_FIND_NEXT) != IDE_SUCCESS);

        IDE_TEST_RAISE(sRow == NULL, ERR_PARENT_NOT_FOUND);

        // fix BUG-39754
        if( sLockRowType == 1 )
        {
            // BUG-17940 parent key�� lock�� ��ƾ� ��.
            IDE_TEST(sCursor.lockRow( ) != IDE_SUCCESS);
        }

        sIsCursorOpened = ID_FALSE;
        IDE_TEST(sCursor.close() != IDE_SUCCESS);
    }
    else
    {
        // Nothing to do.
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_PARENT_NOT_FOUND);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QMX_NOT_FOUND_PARENT_ROW,
                                aParentInfo->foreignKey->name));
    }
    IDE_EXCEPTION_END;

    if ( sIsCursorOpened == ID_TRUE )
    {
        (void) sCursor.close();
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qdnForeignKey::searchForeignKey( qcStatement       * aStatement,
                                 qcmRefChildInfo   * aChildConstraints,
                                 qcmRefChildInfo   * aRefChildInfo,
                                 mtcTuple          * aCheckedTuple,
                                 const void        * aCheckedRow,
                                 idBool              aIsOnDelete )
{
/***********************************************************************
 *
 * Description :
 *    checkReferentialChildOnUpdate �κ��� ȣ��, ���̺� update �ÿ�
 *    ������ ���� child ���̺��� �����ϰ� �־��� ���� �ƴ��� �˻�
 *
 * Implementation :
 *    1. deleteChildRowWithRow
 *       : delete cascade�� ���,
 *         delete�Ǳ� ���� ���� �ΰ��� �ƴϸ�,
 *         child ���̺� �����ϴ��� �˻��ؼ� �����ϸ� delete��Ŵ.
 *
 *    2. getChildRowWithRow
 *       : delete, update �Ǳ� ���� ���� �ΰ��� �ƴϸ�,
 *         child ���̺� �����ϴ��� �˻�
 *
 ***********************************************************************/

#define IDE_FN "qdnForeignKey::searchForeignKey"
    UInt            i;
    SInt            sColumnOrder;
    idBool          sHasNullValue = ID_FALSE;
    qcmForeignKey * sForeignKey;
    UInt            sReferenceRule;
    mtcColumn     * sConstraintColumn;
    qcmIndex      * sUniqueConstr;
    void          * sValueTemp;

    sUniqueConstr  = aRefChildInfo->parentIndex;
    sForeignKey    = aRefChildInfo->foreignKey;
    sReferenceRule = aRefChildInfo->referenceRule;

    IDE_TEST_RAISE(sForeignKey == NULL, ERR_NOT_FOUND_FOREIGNKEY_CONSTRAINT);
    
    // check using aCheckedRow - on update and delete.
    for (i = 0; i < sUniqueConstr->keyColCount; i++)
    {
        sColumnOrder = sUniqueConstr->keyColumns[i].column.id &
            SMI_COLUMN_ID_MASK;

        sConstraintColumn = &( aCheckedTuple->columns[sColumnOrder] );
        
        sValueTemp = (void*)mtc::value( sConstraintColumn,
                                        aCheckedRow,
                                        MTD_OFFSET_USE );

        if (sConstraintColumn->module->isNull(sConstraintColumn,
                                              sValueTemp ) == ID_TRUE)
        {
            sHasNullValue = ID_TRUE;
            break;
        }
        else
        {
            /* Nothing to do */
        }
    }

    if (sHasNullValue == ID_FALSE)
    {
        // PROJ-1509

        // BUG-29728
        // ���� ��Ÿ���� �������� �ʱ� ���� DML flag�� �����ؼ�
        // sForeignKey->referenceRule�� option ������ �� �ְ�,
        // QD_FOREIGN_DELETE_CASCADE�� option ����(�ڼ��ڸ�) ����
        // ���ϱ� ���� MASK�� ����Ѵ�.
        
        // PROJ-2212 foreien key set null
        if( ( (sReferenceRule & QD_FOREIGN_OPTION_MASK)
              == (QD_FOREIGN_DELETE_CASCADE & QD_FOREIGN_OPTION_MASK) )
            ) // fix BUG-32922
        {
            if ( aIsOnDelete == ID_TRUE )
            {
                // on delete cascade (child deletion)
                IDE_TEST( deleteChildRowWithRow( aStatement,
                                                 aChildConstraints,
                                                 aRefChildInfo,
                                                 aCheckedTuple,
                                                 aCheckedRow )
                          != IDE_SUCCESS);
            }
            else
            {
                // on delete cascade and update statement.
                IDE_TEST( getChildRowWithRow( aStatement,
                                              aRefChildInfo,
                                              aCheckedTuple,
                                              aCheckedRow )
                          != IDE_SUCCESS);
            }
        }
        else if( ( (sReferenceRule & QD_FOREIGN_OPTION_MASK)
                   == (QD_FOREIGN_DELETE_SET_NULL & QD_FOREIGN_OPTION_MASK) ) )
        {
            if ( aIsOnDelete == ID_TRUE )
            {
                // on delete set null (only update child)
                IDE_TEST( updateNullChildRowWithRow( aStatement,
                                                     aChildConstraints,
                                                     aRefChildInfo,
                                                     aCheckedTuple,
                                                     aCheckedRow )
                          != IDE_SUCCESS);
            }
            else
            {
                // on delete set null (grand child check)
                IDE_TEST( getChildRowWithRow( aStatement,
                                              aRefChildInfo,
                                              aCheckedTuple,
                                              aCheckedRow )
                          != IDE_SUCCESS);
            }
        }
        else
        {
            // on delete no action (child check)
            IDE_TEST( getChildRowWithRow( aStatement,
                                          aRefChildInfo,
                                          aCheckedTuple,
                                          aCheckedRow )
                      != IDE_SUCCESS);
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_NOT_FOUND_FOREIGNKEY_CONSTRAINT);
    {
        IDE_SET(ideSetErrorCode(qpERR_FATAL_QDN_NOT_FOUND_FOREIGNKEY));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qdnForeignKey::searchSelf( qcStatement   * aStatement,
                           qmsTableRef   * aTableRef,
                           qcmTableInfo  * aTableInfo,
                           qcmIndex      * aSelfIndex,
                           mtcTuple      * aCheckedTuple,
                           const void    * aCheckedRow )
{
/***********************************************************************
 *
 * Description :
 *    checkReferentialChildOnUpdate �κ��� ȣ��, ���̺� update �ÿ� �����
 *    child ���̺��� �����ϰ� �ִ� ���� ����� �Ŀ��� ��� �� ���̺�
 *    ���� ���� �����ϴ��� �˻���
 *
 * Implementation :
 *    1. update �Ǵ� ���� �� ������ �˻�
 *    2. �� ���� �ƴϸ�, ����Ǳ� ���� ���� update ���� parent ���̺�
 *       �����ϴ��� �˻�? ���� �ִٸ� searchForeignKey �� �� �ʿ䰡 ����
 *
 ***********************************************************************/

#define IDE_FN "qdnForeignKey::searchSelf"
    UInt                    i;
    SInt                    sColumnOrder;
    UInt                    sIndexKeyOrder;
    mtcColumn             * sConstraintColumn;
    qtcMetaRangeColumn      sRangeColumn[QC_MAX_KEY_COLUMN_COUNT];
    smiRange                sRange;
    const void            * sRow;
    idBool                  sHasNullValue = ID_FALSE;
    smiTableCursor          sCursor;
    smiCursorProperties     sCursorProperty;
    scGRID                  sRid; // Disk Table�� ���� Record IDentifier
    idBool                  sIsCursorOpened = ID_FALSE;

    qcmTableInfo          * sTableInfo;
    UInt                    sTableType;
    UInt                    sRowSize;

    mtcColumn             * sKeyColumn;
    const void            * sValue;
    const void            * sIndexHandle;
    UInt                    sIndexType;
    qcmIndex              * sSelfIndex;
    UInt                    sCompareType;
    void                  * sValueTemp;

    smiFetchColumnList    * sFetchColumnList = NULL;
    qmsIndexTableRef      * sIndexTable = NULL;
    idBool                  sFound = ID_FALSE;

    sCursor.initialize();
    
    sTableInfo = aTableInfo;
    sSelfIndex = aSelfIndex;

    sTableType = sTableInfo->tableFlag & SMI_TABLE_TYPE_MASK;

    //---------------------------------
    // index table ��ü
    //---------------------------------
    
    // PROJ-1502 PARTITIONED DISK TABLE
    if ( sTableInfo->tablePartitionType == QCM_TABLE_PARTITION )
    {
        // PROJ-1624 non-partitioned index
        if ( sSelfIndex->indexPartitionType == QCM_NONE_PARTITIONED_INDEX )
        {
            // non-partitioned index�� ��� index table������
            // ���� ���� �����ϴ� �� �˻��Ѵ�.
            
            IDE_TEST( qmsIndexTable::findIndexTableRefInList(
                          aTableRef->indexTableRef,
                          sSelfIndex->indexId,
                          & sIndexTable )
                      != IDE_SUCCESS );

            // tableInfo�� ��ü�Ѵ�.
            sTableInfo = sIndexTable->tableInfo;
            
            // key index�� ã�´�.
            IDE_TEST( qmsIndexTable::findKeyIndex( sIndexTable->tableInfo,
                                                   & sSelfIndex )
                      != IDE_SUCCESS );
        }
        else
        {
            /* PROJ-2334 PMT
             * memory partition�� ��� �ش� index�� �����;� ��. */
            for ( i = 0; i < sTableInfo->indexCount; i++ )
            {
                if ( aSelfIndex->indexId ==
                     sTableInfo->indices[i].indexId )
                {
                    sSelfIndex = &sTableInfo->indices[i];
                    sFound = ID_TRUE;
                    break;
                }
                else
                {
                    /* Nothing To Do */
                }
            }
                
            // �ݵ�� �����ؾ� �Ѵ�.
            IDE_TEST_RAISE( sFound == ID_FALSE, ERR_NOT_EXIST_INDEX );
        }
    }
    else
    {
        // Nothing to do.
    }
    
    //---------------------------------
    // Key Range ������ ����
    //---------------------------------
    
    sKeyColumn = sSelfIndex->keyColumns;

    // PROJ-1872
    // index�� �ִ� Į���� meta range�� ���� �Ǹ�
    // disk index column�� compare�� stored type�� mt type ���� ���̴�.
    if ( sTableType == SMI_TABLE_DISK )
    {
        sCompareType = MTD_COMPARE_STOREDVAL_MTDVAL;
    }
    else
    {
        sCompareType = MTD_COMPARE_MTDVAL_MTDVAL;
    }
    
    qtc::initializeMetaRange(&sRange, sCompareType);

    for (i = 0; i < sSelfIndex->keyColCount; i ++)
    {
        sColumnOrder = sSelfIndex->keyColumns[i].column.id &
            SMI_COLUMN_ID_MASK;

        // To Fix PR-20839
        // Key Range ���� ��
        // Index Key Column�� Order�� ����Ͽ��� ��.
        sIndexKeyOrder =
            sSelfIndex->keyColsFlag[i] & SMI_COLUMN_ORDER_MASK;

        sConstraintColumn = &( aCheckedTuple->columns[sColumnOrder] );
        
        sValueTemp = (void*)mtc::value( sConstraintColumn,
                                        aCheckedRow,
                                        MTD_OFFSET_USE );

        if ( sConstraintColumn->module->isNull( sConstraintColumn,
                                                sValueTemp ) == ID_TRUE)
        {
            sHasNullValue = ID_TRUE;
            break;
        }
        else
        {
            // Bug-10704 fix
            // sConstraintColumn->basicInfo�� ���� ������ �ʿ䰡 ����
            // by kumdory, 2005-03-09

            // To Fix PR-10592
            // Disk Varchar���� ������ �ʿ䰡 ����

            sValue = mtc::value( sConstraintColumn,
                                 aCheckedRow,
                                 MTD_OFFSET_USE );

            qtc::setMetaRangeColumn( & sRangeColumn[i],
                                     & sKeyColumn[i],
                                     sValue,
                                     sIndexKeyOrder,
                                     i ); //Column Idx 

            qtc::addMetaRangeColumn(&sRange, &sRangeColumn[i]);
        }
    }
    qtc::fixMetaRange(&sRange);

    //---------------------------------
    // Reference ���� ���� �˻�
    //---------------------------------

    // if null valued column(s) exist(s), return ok. else search parent.
    if (sHasNullValue != ID_TRUE)
    {
        sIndexHandle = sSelfIndex->indexHandle;
        sIndexType = sSelfIndex->indexTypeId;
        
        IDE_DASSERT( sIndexHandle != NULL );
        
        SMI_CURSOR_PROP_INIT_FOR_INDEX_SCAN(&sCursorProperty,
                                            aStatement->mStatistics,
                                            sIndexType);
        
        if ( sTableType == SMI_TABLE_DISK )
        {
            //---------------------------------
            // Row�� ���� ���� �Ҵ�
            //---------------------------------

            // Disk Table�� ���
            // Record Read�� ���� ������ �Ҵ��Ѵ�.
            IDE_TEST( qdbCommon::getDiskRowSize( sTableInfo,
                                                 & sRowSize )
                      != IDE_SUCCESS );
            
            // To fix BUG-14820
            // Disk-variable �÷��� rid�񱳸� ���� �ʱ�ȭ �ؾ� ��.
            IDU_LIMITPOINT("qdnForeignKey::searchSelf::malloc");
            IDE_TEST( aStatement->qmxMem->cralloc( sRowSize,
                                                   (void **) & sRow )
                      != IDE_SUCCESS );

            //---------------------------------
            // PROJ-1705
            // fetch column list �ۼ�
            //---------------------------------

            IDE_TEST( qdbCommon::makeFetchColumnList4Index(
                          QC_PRIVATE_TMPLATE(aStatement),
                          sTableInfo,
                          sSelfIndex,
                          ID_TRUE,
                          & sFetchColumnList ) != IDE_SUCCESS );

            sCursorProperty.mFetchColumnList = sFetchColumnList;
        }
        else
        {
            // Nothing to do.
        }

        //---------------------------------
        // �ش� Row�� ���� ���� �˻�
        //---------------------------------

        IDE_TEST(sCursor.open( QC_SMI_STMT( aStatement ),
                               sTableInfo->tableHandle,
                               sIndexHandle,
                               smiGetRowSCN(sTableInfo->tableHandle),
                               NULL,
                               &sRange,
                               smiGetDefaultKeyRange(),
                               smiGetDefaultFilter(),
                               QCM_META_CURSOR_FLAG,
                               SMI_SELECT_CURSOR,
                               & sCursorProperty ) != IDE_SUCCESS);
        sIsCursorOpened = ID_TRUE;

        IDE_TEST(sCursor.beforeFirst() != IDE_SUCCESS);

        IDE_TEST(sCursor.readRow(&sRow, &sRid, SMI_FIND_NEXT) != IDE_SUCCESS);

        IDE_TEST_RAISE(sRow == NULL, ERR_CHILD_EXIST);

        sIsCursorOpened = ID_FALSE;
        IDE_TEST(sCursor.close() != IDE_SUCCESS);
    }
    else
    {
        // Null Value�� �����ϴ� ��� ������ �˻簡 �ʿ� ����
        // Nothing To Do
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_CHILD_EXIST)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QMX_CHILD_EXIST,
                                "SELF REFERENCE"));
    }
    IDE_EXCEPTION( ERR_NOT_EXIST_INDEX )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qdnForeignKey::searchSelf",
                                  "NOT EXIST INDEX" ));
    }
    IDE_EXCEPTION_END;

    if ( sIsCursorOpened == ID_TRUE )
    {
        (void) sCursor.close();
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qdnForeignKey::getChildRowWithRow( qcStatement       * aStatement,
                                   qcmRefChildInfo   * aRefChildInfo,
                                   mtcTuple          * aCheckedTuple,
                                   const void        * aCheckedRow )
{
/***********************************************************************
 *
 * Description :
 *    searchForeignKey �κ��� ȣ��, ���̺� update �ÿ�
 *    ������ ���� child ���̺��� �����ϰ� �־��� ���� �ƴ��� �˻�
 *
 * Implementation :
 *    1. getChildRowWithRow
 *       : update �Ǳ� ���� ���� child ���̺� �����ϴ��� �˻�
 *    PROJ-2212 Foreign key set null ���� �Լ� ��� ������ �о���
 *    - on delete no action �� ��� child �� ���� �ϴ� �� üũ.
 *    - on delete cascade �� ��� update statement
 *      ��� �� ��� child ���� �ϴ��� üũ.
 *    - on delete set null �� ��� grand child �� ���� �ϴ��� üũ.
 *
 ***********************************************************************/

    qcmIndex           * sSelectedIndex;
    qcmIndex           * sUniqueConstraint;
    UInt                 sKeyRangeColumnCnt;
    qcmTableInfo       * sChildTableInfo;
    qmsTableRef        * sChildTableRef;

    // PROJ-1624 global non-partitioned index
    qdnFKScanMethod      sScanMethod;
    qmsIndexTableRef   * sIndexTable;
    qcmIndex           * sIndexTableIndex;

    // child table�� scan method�� �����ϰ� ������ lock�� ȹ���Ѵ�.
    IDE_TEST( validateScanMethodAndLockTable( aStatement,
                                              aRefChildInfo,
                                              & sSelectedIndex,
                                              & sKeyRangeColumnCnt,
                                              & sIndexTable,
                                              & sScanMethod )
              != IDE_SUCCESS );

    sChildTableRef    = aRefChildInfo->childTableRef;
    sUniqueConstraint = aRefChildInfo->parentIndex;
    sChildTableInfo   = sChildTableRef->tableInfo; 

    switch ( sScanMethod )
    {
        case QDN_FK_TABLE_FULL_SCAN:
        case QDN_FK_TABLE_INDEX_SCAN:
        {
            // table full scan�� ���
            // table index scan�� ���
            IDE_TEST( getChildRowWithRow4Table( aStatement,
                                                sChildTableRef,
                                                sChildTableInfo,
                                                ID_FALSE,
                                                aRefChildInfo->foreignKey,
                                                sKeyRangeColumnCnt,
                                                sSelectedIndex,
                                                sUniqueConstraint,
                                                aCheckedTuple,
                                                aCheckedRow )
                      != IDE_SUCCESS );
            break;
        }
        
        case QDN_FK_PARTITION_FULL_SCAN:
        case QDN_FK_PARTITION_INDEX_SCAN:
        {
            // table partition full scan�� ���
            // table partition local index scan�� ���
            IDE_TEST( getChildRowWithRow4Partition( aStatement,
                                                    sChildTableRef,
                                                    aRefChildInfo->foreignKey,
                                                    sKeyRangeColumnCnt,
                                                    sSelectedIndex,
                                                    sUniqueConstraint,
                                                    aCheckedTuple,
                                                    aCheckedRow )
                      != IDE_SUCCESS );
            break;
        }

        case QDN_FK_PARTITION_ONLY_INDEX_TABLE_SCAN:
        {
            // index table scan�� ���
            IDE_DASSERT( sIndexTable != NULL );
            
            // key index�� ã�´�.
            IDE_TEST( qmsIndexTable::findKeyIndex( sIndexTable->tableInfo,
                                                   & sIndexTableIndex )
                      != IDE_SUCCESS );

            // child�� ��ü�Ѵ�.
            sChildTableInfo = sIndexTable->tableInfo;
            sSelectedIndex = sIndexTableIndex;
            
            IDE_TEST( getChildRowWithRow4Table( aStatement,
                                                sChildTableRef, // BUGBUG sIndexTable->table ����
                                                sChildTableInfo,
                                                ID_TRUE,  // is indexTable
                                                aRefChildInfo->foreignKey,
                                                sKeyRangeColumnCnt,
                                                sSelectedIndex,
                                                sUniqueConstraint,
                                                aCheckedTuple,
                                                aCheckedRow )
                      != IDE_SUCCESS );
            break;
        }

        case QDN_FK_PARTITION_INDEX_TABLE_SCAN:
        {
            // index table scan + partition scan�� ���
            IDE_TEST( getChildRowWithRow4IndexTable( aStatement,
                                                     sIndexTable,
                                                     sChildTableRef,
                                                     sChildTableInfo,
                                                     aRefChildInfo->foreignKey,
                                                     sKeyRangeColumnCnt,
                                                     sSelectedIndex,
                                                     sUniqueConstraint,
                                                     aCheckedTuple,
                                                     aCheckedRow )
                      != IDE_SUCCESS );
            break;
        }
        
        default:
        {
            IDE_DASSERT(0);
            break;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qdnForeignKey::deleteChildRowWithRow(
    qcStatement       * aStatement,
    qcmRefChildInfo   * aChildConstraints,
    qcmRefChildInfo   * aRefChildInfo,
    mtcTuple          * aCheckedTuple,
    const void        * aCheckedRow )
{
/***********************************************************************
 *
 * Description :
 *    delete cascade�� child table ���ڵ� ����
 *
 * Implementation :
 *       : delete �Ǳ� ���� ���� �ΰ��� �ƴϸ�,
 *         child ���̺� �����ϴ� ���ڵ� ����
 *
 ***********************************************************************/

    qcmIndex           * sSelectedIndex;
    qcmIndex           * sUniqueConstraint;
    UInt                 sKeyRangeColumnCnt;
    qcmTableInfo       * sChildTableInfo;
    qmsTableRef        * sChildTableRef;

    // PROJ-1624 global non-partitioned index
    qdnFKScanMethod      sScanMethod;
    qmsIndexTableRef   * sIndexTable;

    // child table�� scan method�� �����ϰ� ������ lock�� ȹ���Ѵ�.
    IDE_TEST( validateScanMethodAndLockTableForDelete( aStatement,
                                                       aRefChildInfo,
                                                       & sSelectedIndex,
                                                       & sKeyRangeColumnCnt,
                                                       & sIndexTable,
                                                       & sScanMethod )
              != IDE_SUCCESS );
    
    sChildTableRef    = aRefChildInfo->childTableRef;
    sChildTableInfo   = sChildTableRef->tableInfo;
    sUniqueConstraint = aRefChildInfo->parentIndex;

    switch ( sScanMethod )
    {
        case QDN_FK_TABLE_FULL_SCAN:
        case QDN_FK_TABLE_INDEX_SCAN:
        {
            // table full scan�� ���
            // table index scan�� ���
            IDE_TEST( deleteChildRowWithRow4Table( aStatement,
                                                   aChildConstraints,
                                                   sChildTableRef,
                                                   sChildTableInfo,
                                                   aRefChildInfo->foreignKey,
                                                   sKeyRangeColumnCnt,
                                                   sSelectedIndex,
                                                   sUniqueConstraint,
                                                   aCheckedTuple,
                                                   aCheckedRow )
                      != IDE_SUCCESS );
            break;
        }
        
        case QDN_FK_PARTITION_FULL_SCAN:
        case QDN_FK_PARTITION_INDEX_SCAN:
        {
            // table partition full scan�� ���
            // table partition local index scan�� ���
            IDE_TEST( deleteChildRowWithRow4Partition( aStatement,
                                                       aChildConstraints,
                                                       sChildTableRef,
                                                       sChildTableInfo,
                                                       aRefChildInfo->foreignKey,
                                                       sKeyRangeColumnCnt,
                                                       sSelectedIndex,
                                                       sUniqueConstraint,
                                                       aCheckedTuple,
                                                       aCheckedRow )
                      != IDE_SUCCESS );
            break;
        }

        case QDN_FK_PARTITION_ONLY_INDEX_TABLE_SCAN:
        case QDN_FK_PARTITION_INDEX_TABLE_SCAN:
        {
            // index table scan�� ���
            // index table scan + partition scan�� ���
            IDE_TEST( deleteChildRowWithRow4IndexTable( aStatement,
                                                        aChildConstraints,
                                                        sIndexTable,
                                                        sChildTableRef,
                                                        sChildTableInfo,
                                                        aRefChildInfo->foreignKey,
                                                        sKeyRangeColumnCnt,
                                                        sUniqueConstraint,
                                                        aCheckedTuple,
                                                        aCheckedRow )
                      != IDE_SUCCESS );
            break;
        }
        
        default:
        {
            IDE_DASSERT(0);
            break;
        }
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qdnForeignKey::updateNullChildRowWithRow(
    qcStatement       * aStatement,
    qcmRefChildInfo   * aChildConstraints,
    qcmRefChildInfo   * aRefChildInfo,
    mtcTuple          * aCheckedTuple,
    const void        * aCheckedRow )
{
/***********************************************************************
 *
 * Description : PROJ-2212 foreign key on delete set null
 *    delete on set null�� child table ���ڵ���
 *    fk column NULL update / grand child check.
 *
 *    deleteChildRowWithRown �Լ��� ���� ������  delete row ��ƾ��
 *    update null ����.
 *    update after trigger ���� ���� ����. update before trigger ����.
 *
 * Implementation :
 *       1. delete �Ǳ� ���� ���� �ΰ��� �ƴϸ�,
 *          child ���̺� �����ϴ� ���ڵ��� fk column NULL update.
 *       2. update before trigger ����.
 *       3. grand child check.
 *
 ***********************************************************************/

    qcmIndex              * sSelectedIndex;
    qcmIndex              * sUniqueConstraint;
    qcmForeignKey         * sForeignKey;
    UInt                    sKeyRangeColumnCnt;
    qcmTableInfo          * sChildTableInfo;
    qmsTableRef           * sChildTableRef;
    qdConstraintSpec      * sChildCheckConstrList;

    /* PROJ-1090 Function-based Index */
    qmsTableRef           * sDefaultTableRef;
    qcmColumn             * sDefaultExprColumns;
    qcmColumn             * sDefaultExprBaseColumns;

    // on delete set null
    UInt                    sUpdateColCount;
    UInt                  * sUpdateColumnID;
    qcmColumn             * sUpdateColumn;
    smiColumnList         * sUpdateColumnList;
    qcmRefChildUpdateType   sUpdateRowMovement;
    
    // PROJ-1624 global non-partitioned index
    qdnFKScanMethod         sScanMethod;
    qmsIndexTableRef      * sIndexTable;

    // child table�� scan method�� �����ϰ� ������ lock�� ȹ���Ѵ�.
    IDE_TEST( validateScanMethodAndLockTableForDelete( aStatement,
                                                       aRefChildInfo,
                                                       & sSelectedIndex,
                                                       & sKeyRangeColumnCnt,
                                                       & sIndexTable,
                                                       & sScanMethod )
              != IDE_SUCCESS );
    
    sChildTableRef        = aRefChildInfo->childTableRef;
    sChildTableInfo       = sChildTableRef->tableInfo;
    sChildCheckConstrList = aRefChildInfo->childCheckConstrList;
    sUniqueConstraint     = aRefChildInfo->parentIndex;
    sForeignKey           = aRefChildInfo->foreignKey;

    sDefaultTableRef        = aRefChildInfo->defaultTableRef;
    sDefaultExprColumns     = aRefChildInfo->defaultExprColumns;
    sDefaultExprBaseColumns = aRefChildInfo->defaultExprBaseColumns;

    sUpdateColCount    = aRefChildInfo->updateColCount;
    sUpdateColumnID    = aRefChildInfo->updateColumnID;
    sUpdateColumn      = aRefChildInfo->updateColumn;
    sUpdateColumnList  = aRefChildInfo->updateColumnList;
    sUpdateRowMovement = aRefChildInfo->updateRowMovement;
    
    switch ( sScanMethod )
    {
        case QDN_FK_TABLE_FULL_SCAN:
        case QDN_FK_TABLE_INDEX_SCAN:
        {
            // table full scan�� ���
            // table index scan�� ���
            IDE_TEST( updateNullChildRowWithRow4Table( aStatement,
                                                       aChildConstraints,
                                                       sChildTableRef,
                                                       sChildTableInfo,
                                                       sChildCheckConstrList,
                                                       sDefaultTableRef,
                                                       sDefaultExprColumns,
                                                       sDefaultExprBaseColumns,
                                                       sUpdateColCount,
                                                       sUpdateColumnID,
                                                       sUpdateColumn,
                                                       sUpdateColumnList,
                                                       sUpdateRowMovement,
                                                       sForeignKey,
                                                       sKeyRangeColumnCnt,
                                                       sSelectedIndex,
                                                       sUniqueConstraint,
                                                       aCheckedTuple,
                                                       aCheckedRow )
                      != IDE_SUCCESS );
            break;
        }
        
        case QDN_FK_PARTITION_FULL_SCAN:
        case QDN_FK_PARTITION_INDEX_SCAN:
        {
            // table partition full scan�� ���
            // table partition local index scan�� ���
            IDE_TEST( updateNullChildRowWithRow4Partition( aStatement,
                                                           aChildConstraints,
                                                           sChildTableRef,
                                                           sChildTableInfo,
                                                           sChildCheckConstrList,
                                                           sDefaultTableRef,
                                                           sDefaultExprColumns,
                                                           sDefaultExprBaseColumns,
                                                           sUpdateColCount,
                                                           sUpdateColumnID,
                                                           sUpdateColumn,
                                                           sUpdateRowMovement,
                                                           sForeignKey,
                                                           sKeyRangeColumnCnt,
                                                           sSelectedIndex,
                                                           sUniqueConstraint,
                                                           aCheckedTuple,
                                                           aCheckedRow )
                      != IDE_SUCCESS );
            break;
        }

        case QDN_FK_PARTITION_ONLY_INDEX_TABLE_SCAN:
        case QDN_FK_PARTITION_INDEX_TABLE_SCAN:
        {
            // index table scan�� ���
            // index table scan + partition scan�� ���
            IDE_TEST( updateNullChildRowWithRow4IndexTable( aStatement,
                                                            aChildConstraints,
                                                            sIndexTable,
                                                            sChildTableRef,
                                                            sChildTableInfo,
                                                            sChildCheckConstrList,
                                                            sDefaultTableRef,
                                                            sDefaultExprColumns,
                                                            sDefaultExprBaseColumns,
                                                            sUpdateColCount,
                                                            sUpdateColumnID,
                                                            sUpdateColumn,
                                                            sUpdateRowMovement,
                                                            sForeignKey,
                                                            sKeyRangeColumnCnt,
                                                            sUniqueConstraint,
                                                            aCheckedTuple,
                                                            aCheckedRow )
                      != IDE_SUCCESS );
            break;
        }
        
        default:
        {
            IDE_DASSERT(0);
            break;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qdnForeignKey::getChildKeyRangeInfo( qcmTableInfo  * aChildTableInfo,
                                     qcmForeignKey * aForeignKey,
                                     qcmIndex     ** aSelectedIndex,
                                     UInt          * aKeyColumnCnt )
{
/***********************************************************************
 *
 * Description :
 *    To Fix PR-10592
 *    Master Table�� ���� UPDATE�� DELETE �߻���
 *    �ش� Record�� Primary Key�� �����ϴ� Child Table�� Record��
 *    �����ϴ� �� �˻��ϱ� ���� Key Range ���� ������ ȹ���Ѵ�.
 *    ��� ������ Index ������ Key Range�� ������ �� �ִ� Key Column��
 *    ������ �����Ѵ�.
 *
 * Implementation :
 *
 *  Child Table�� ���� �˻� �� index�� ����� �� �־�� �Ѵ�.
 *    Optimizer Code�� �̿����� �ٶ����ϳ�,
 *    SELECT ... FROM ... WHERE ... �� ������ �ƴϾ �̸� ����� �� ����.
 *    ����, ������ ���� ������ �ΰ� �ε����� ����� �� �ֵ��� �Ѵ�.
 *        - Access Method ���ý� Cost �񱳸� �� �� ����.
 *           : ���� ���� Key Column�� ����� �� �ִ� Index�� ����Ѵ�.
 *        - qtcNode�� �����Ǿ� ���� �ʾ� Filter ó���� �Ұ����ϴ�.
 *           : Key Range�� ���õ� Column�� �����ϰ� ��� Column��
 *             Record�� ���� �� �Ǵ��Ͽ��� �Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "qdnForeignKey::getChildKeyRangeInfo"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    UInt                i;
    UInt                j;

    qcmIndex          * sSelectedIndex;
    UInt                sKeyRangeColumnCnt;
    UInt                sKeyCnt;

    //-----------------------------------
    // ���ռ� �˻�
    //-----------------------------------

    IDE_DASSERT( aChildTableInfo != NULL );
    IDE_DASSERT( aForeignKey != NULL );
    IDE_DASSERT( aSelectedIndex != NULL );
    IDE_DASSERT( aKeyColumnCnt != NULL );

    //-----------------------------------
    // ��� ������ Index�� �˻�
    //-----------------------------------

    sSelectedIndex = NULL;
    sKeyRangeColumnCnt = 0;

    for ( i = 0; i < aChildTableInfo->indexCount; i++ )
    {
        sKeyCnt = 0;

        for ( j = 0;
              (j < aForeignKey->constraintColumnCount) &&
                  (j < aChildTableInfo->indices[i].keyColCount);
              j++ )
        {
            if ( aForeignKey->referencingColumn[j] ==
                 aChildTableInfo->indices[i].keyColumns[j].column.id )
            {
                // Referencing Column�� Key Column�� ������ �����.
                sKeyCnt++;
            }
            else
            {
                // Referencing Column�� Key Column�� �ٸ�
                break;
            }
        }

        if ( sKeyCnt > sKeyRangeColumnCnt )
        {
            // ���� ���� Index�� ������.

            sSelectedIndex = & aChildTableInfo->indices[i];
            sKeyRangeColumnCnt = sKeyCnt;

            if ( sKeyRangeColumnCnt == aForeignKey->constraintColumnCount )
            {
                // ��� Referencing Column�� �����ϴ� Index�� ã��
                break;
            }
            else
            {
                // ���� ��� Referencing Column�� �����ϴ�
                // Index�� ã�� ����
                // Nothing To Do
            }
        }
        else
        {
            // ������ ���õ� Index�� �� ���� �����.
            // Nothing To Do
        }
    }

    //-----------------------------------
    // ���� ���� Index�� ����� �� �ִ� Key Column ������ ����
    //-----------------------------------

    if ( sSelectedIndex != NULL )
    {
        *aSelectedIndex = sSelectedIndex;
        *aKeyColumnCnt = sKeyRangeColumnCnt;
    }
    else
    {
        *aSelectedIndex = NULL;
        *aKeyColumnCnt = 0;
    }

    return IDE_SUCCESS;

#undef IDE_FN
}

IDE_RC
qdnForeignKey::validateScanMethodAndLockTable( qcStatement       * aStatement,
                                               qcmRefChildInfo   * aRefChildInfo,
                                               qcmIndex         ** aSelectedIndex,
                                               UInt              * aKeyRangeColumnCnt,
                                               qmsIndexTableRef ** aSelectedIndexTable,
                                               qdnFKScanMethod   * aScanMethod )
{
/***********************************************************************
 *
 * Description :
 *    getChildRowWithRow �κ��� ȣ��, child table�� is_lock�� ȸ���ϰ�
 *    foreign key scan method�� �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    UInt                 sKeyRangeColumnCnt;
    qcmTableInfo       * sChildTableInfo;
    qmsTableRef        * sChildTableRef;
    qcmIndex           * sSelectedIndex;
    qmsIndexTableRef   * sSelectedIndexTable = NULL;
    qdnFKScanMethod      sScanMethod;
    
    sChildTableRef = aRefChildInfo->childTableRef;

    // PROJ-1509 BUG-14438
    IDE_TEST( qcm::validateAndLockTable( aStatement,
                                         sChildTableRef->tableHandle,
                                         sChildTableRef->tableSCN,
                                         SMI_TABLE_LOCK_IS )
              != IDE_SUCCESS );
    
    sChildTableInfo = sChildTableRef->tableInfo;
    
    //---------------------------------------
    // To Fix PR-10592
    // Access Method ����
    // Index �� ��� ������ key column ������ ȹ��
    //---------------------------------------

    IDE_TEST( getChildKeyRangeInfo( sChildTableInfo,
                                    aRefChildInfo->foreignKey,
                                    & sSelectedIndex,
                                    & sKeyRangeColumnCnt )
              != IDE_SUCCESS );

    // PROJ-1502 PARTITIONED DISK TABLE
    if( sChildTableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
    {
        if ( sSelectedIndex != NULL )
        {
            // PROJ-1624 global non-partitioned index
            if ( sSelectedIndex->indexPartitionType == QCM_NONE_PARTITIONED_INDEX )
            {
                IDE_TEST( qmsIndexTable::findIndexTableRefInList(
                              sChildTableRef->indexTableRef,
                              sSelectedIndex->indexId,
                              & sSelectedIndexTable )
                          != IDE_SUCCESS );

                // index table�� LOCK(IS)
                IDE_TEST( qmsIndexTable::validateAndLockOneIndexTableRef( aStatement,
                                                                          sSelectedIndexTable,
                                                                          SMI_TABLE_LOCK_IS )
                          != IDE_SUCCESS );
                
                if ( sKeyRangeColumnCnt < aRefChildInfo->foreignKey->constraintColumnCount )
                {
                    // table partition�� LOCK(IS)
                    IDE_TEST( qcmPartition::validateAndLockPartitions(
                                  aStatement,
                                  sChildTableRef,
                                  SMI_TABLE_LOCK_IS )
                              != IDE_SUCCESS );
                    
                    // non-partitioned index�� ���õǾ����� index table�����δ� ��� �˻��� �� ����
                    // table scan�� �ʿ��� ���
                    // ex) foreign key (i1,i2), index (i1)
                    //     i1���� index�� Ÿ���� i2�� table���� ���� �Ѵ�.
                    sScanMethod = QDN_FK_PARTITION_INDEX_TABLE_SCAN;
                }
                else
                {
                    // non-partitioned index�� ���õǾ��� index table�����ε� �˻簡 ������ ���
                    // ex) foreign key (i1,i2), index (i1,i2,i3)
                    //     i1,i2��� index table���� �˻簡���ϴ�.
                    sScanMethod = QDN_FK_PARTITION_ONLY_INDEX_TABLE_SCAN;
                }
            }
            else
            {
                // table partition�� LOCK(IS)
                IDE_TEST( qcmPartition::validateAndLockPartitions(
                              aStatement,
                              sChildTableRef,
                              SMI_TABLE_LOCK_IS )
                          != IDE_SUCCESS );
                
                // table partition local index scan�ϴ� ���
                sScanMethod = QDN_FK_PARTITION_INDEX_SCAN;
            }
        }
        else
        {
            // table partition�� LOCK(IS)
            IDE_TEST( qcmPartition::validateAndLockPartitions(
                          aStatement,
                          sChildTableRef,
                          SMI_TABLE_LOCK_IS )
                      != IDE_SUCCESS );
            
            // table partition full scan�ϴ� ���
            sScanMethod = QDN_FK_PARTITION_FULL_SCAN;
        }
    }
    else
    {
        if ( sSelectedIndex != NULL )
        {
            // table index scan�ϴ� ���
            sScanMethod = QDN_FK_TABLE_INDEX_SCAN;
        }
        else
        {
            // table full scan�ϴ� ���
            sScanMethod = QDN_FK_TABLE_FULL_SCAN;
        }
    }

    *aSelectedIndex      = sSelectedIndex;
    *aKeyRangeColumnCnt  = sKeyRangeColumnCnt;
    *aSelectedIndexTable = sSelectedIndexTable;
    *aScanMethod         = sScanMethod;
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qdnForeignKey::validateScanMethodAndLockTableForDelete( qcStatement       * aStatement,
                                                        qcmRefChildInfo   * aRefChildInfo,
                                                        qcmIndex         ** aSelectedIndex,
                                                        UInt              * aKeyRangeColumnCnt,
                                                        qmsIndexTableRef ** aSelectedIndexTable,
                                                        qdnFKScanMethod   * aScanMethod )
{
/***********************************************************************
 *
 * Description :
 *    deleteChildRowWithRow Ȥ�� updateNullChildRowWithRow �κ��� ȣ��,
 *    child table�� ix_lock�� ȸ���ϰ� foreign key scan method�� �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    UInt                 sKeyRangeColumnCnt;
    qcmTableInfo       * sChildTableInfo;
    qmsTableRef        * sChildTableRef;
    qcmIndex           * sSelectedIndex;
    qmsIndexTableRef   * sSelectedIndexTable = NULL;
    qdnFKScanMethod      sScanMethod;
    
    sChildTableRef = aRefChildInfo->childTableRef;

    // PROJ-1509 BUG-14438
    IDE_TEST( qcm::validateAndLockTable( aStatement,
                                         sChildTableRef->tableHandle,
                                         sChildTableRef->tableSCN,
                                         SMI_TABLE_LOCK_IX )
              != IDE_SUCCESS );
    
    sChildTableInfo = sChildTableRef->tableInfo;
    
    //---------------------------------------
    // To Fix PR-10592
    // Access Method ����
    // Index �� ��� ������ key column ������ ȹ��
    //---------------------------------------

    IDE_TEST( getChildKeyRangeInfo( sChildTableInfo,
                                    aRefChildInfo->foreignKey,
                                    & sSelectedIndex,
                                    & sKeyRangeColumnCnt )
              != IDE_SUCCESS );

    // PROJ-1502 PARTITIONED DISK TABLE
    if( sChildTableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
    {
        // table partition�� LOCK(IX)
        IDE_TEST( qcmPartition::validateAndLockPartitions( aStatement,
                                                           sChildTableRef,
                                                           SMI_TABLE_LOCK_IX )
                  != IDE_SUCCESS );

        // index table�� LOCK(IX)
        IDE_TEST( qmsIndexTable::validateAndLockIndexTableRefList( aStatement,
                                                                   sChildTableRef->indexTableRef,
                                                                   SMI_TABLE_LOCK_IX )
                  != IDE_SUCCESS );
        
        if ( sSelectedIndex != NULL )
        {
            // PROJ-1624 global non-partitioned index
            if ( sSelectedIndex->indexPartitionType == QCM_NONE_PARTITIONED_INDEX )
            {
                IDE_TEST( qmsIndexTable::findIndexTableRefInList(
                              sChildTableRef->indexTableRef,
                              sSelectedIndex->indexId,
                              & sSelectedIndexTable )
                          != IDE_SUCCESS );

                if ( sKeyRangeColumnCnt < aRefChildInfo->foreignKey->constraintColumnCount )
                {
                    // non-partitioned index�� ���õǾ����� index table�����δ� ��� �˻��� �� ����
                    // table scan�� �ʿ��� ���
                    // ex) foreign key (i1,i2), index (i1)
                    //     i1���� index�� Ÿ���� i2�� table���� ���� �Ѵ�.
                    sScanMethod = QDN_FK_PARTITION_INDEX_TABLE_SCAN;
                }
                else
                {
                    // non-partitioned index�� ���õǾ��� index table�����ε� �˻簡 ������ ���
                    // ex) foreign key (i1,i2), index (i1,i2,i3)
                    //     i1,i2��� index table���� �˻簡���ϴ�.
                    sScanMethod = QDN_FK_PARTITION_ONLY_INDEX_TABLE_SCAN;
                }
            }
            else
            {
                // table partition local index scan�ϴ� ���
                sScanMethod = QDN_FK_PARTITION_INDEX_SCAN;
            }
        }
        else
        {
            // table partition full scan�ϴ� ���
            sScanMethod = QDN_FK_PARTITION_FULL_SCAN;
        }
    }
    else
    {
        if ( sSelectedIndex != NULL )
        {
            // table index scan�ϴ� ���
            sScanMethod = QDN_FK_TABLE_INDEX_SCAN;
        }
        else
        {
            // table full scan�ϴ� ���
            sScanMethod = QDN_FK_TABLE_FULL_SCAN;
        }
    }

    *aSelectedIndex      = sSelectedIndex;
    *aKeyRangeColumnCnt  = sKeyRangeColumnCnt;
    *aSelectedIndexTable = sSelectedIndexTable;
    *aScanMethod         = sScanMethod;
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdnForeignKey::makeKeyRangeAndIndex( UInt                  aTableType,
                                            UInt                  aKeyRangeColumnCnt,
                                            qcmIndex            * aSelectedIndex,
                                            qcmIndex            * aUniqueConstraint,
                                            mtcTuple            * aCheckedTuple,
                                            const void          * aCheckedRow,
                                            qtcMetaRangeColumn  * aRangeColumn,
                                            smiRange            * aRange,
                                            qcmIndex           ** aIndex,
                                            smiRange           ** aKeyRange )
{
/***********************************************************************
 *
 * Description :
 *    child table scan�� key range�� �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    const void  * sValue;
    SInt          sColumnOrder;
    UInt          sIndexKeyOrder;
    UInt          sCompareType;
    UInt          i;

    if ( aKeyRangeColumnCnt > 0 )
    {
        idlOS::memset( (void*) aRangeColumn,
                       0x00,
                       ID_SIZEOF(qtcMetaRangeColumn) * aKeyRangeColumnCnt );
    
        // PROJ-1872
        // index�� �ִ� Į���� meta range�� ���� �Ǹ�
        // disk index column�� compare�� stored type�� mt type ���� ���̴�
        if ( aTableType == SMI_TABLE_DISK )
        {
            sCompareType = MTD_COMPARE_STOREDVAL_MTDVAL;
        }
        else
        {
            sCompareType = MTD_COMPARE_MTDVAL_MTDVAL;
        }
        
        qtc::initializeMetaRange( aRange, sCompareType );

        for ( i = 0; i < aKeyRangeColumnCnt; i++ )
        {
            // fix BUG-17282
            sColumnOrder   = aUniqueConstraint->keyColumns[i].column.id & SMI_COLUMN_ID_MASK;
            sIndexKeyOrder = aSelectedIndex->keyColsFlag[i] & SMI_COLUMN_ORDER_MASK;

            sValue = mtc::value( &( aCheckedTuple->columns[sColumnOrder]),
                                 aCheckedRow,
                                 MTD_OFFSET_USE );
            
            qtc::setMetaRangeColumn( & aRangeColumn[i],
                                     & aSelectedIndex->keyColumns[i],
                                     sValue,
                                     sIndexKeyOrder,
                                     i ); //Column Idx
            
            qtc::addMetaRangeColumn( aRange, & aRangeColumn[i] );
        }

        qtc::fixMetaRange( aRange );

        *aIndex    = aSelectedIndex;
        *aKeyRange = aRange;
    }
    else
    {
        *aIndex    = NULL;
        *aKeyRange = smiGetDefaultKeyRange();
    }
    
    return IDE_SUCCESS;
}

IDE_RC qdnForeignKey::changePartIndex( qcmTableInfo   * aPartitionInfo,
                                       UInt             aKeyRangeColumnCnt,
                                       qcmIndex       * aSelectedIndex,
                                       qcmIndex      ** aIndex )
{
/***********************************************************************
 *
 * Description :
 *    child table scan�� key range�� �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    UInt  i;
    
    if ( aKeyRangeColumnCnt > 0 )
    {
        for ( i = 0; i < aPartitionInfo->indexCount; i++ )
        {
            if ( aSelectedIndex->indexId == aPartitionInfo->indices[i].indexId )
            {
                *aIndex = &(aPartitionInfo->indices[i]);
                break;
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
        
    return IDE_SUCCESS;
}

IDE_RC
qdnForeignKey::getChildRowWithRow4Table( qcStatement    * aStatement,
                                         qmsTableRef    * aChildTableRef,
                                         qcmTableInfo   * aChildTableInfo,
                                         idBool           aIsIndexTable,
                                         qcmForeignKey  * aForeignKey,
                                         UInt             aKeyRangeColumnCnt,
                                         qcmIndex       * aSelectedIndex,
                                         qcmIndex       * aUniqueConstraint,
                                         mtcTuple       * aCheckedTuple,
                                         const void     * aCheckedRow )
{
/***********************************************************************
 *
 * Description :
 *    table�� ���� getChildRowWithRow
 *
 * Implementation :
 *
 ***********************************************************************/

    smiTableCursor        sCursor;
    smiCursorProperties   sCursorProperty;
    smiFetchColumnList  * sFetchColumnList = NULL;
    const void          * sRow;
    scGRID                sRid;
    UInt                  sTableType;
    UInt                  sRowSize = 0;
    idBool                sIsCursorOpened = ID_FALSE;

    // To Fix PR-10592
    // Index�� ����ϱ� ���� ������
    qcmIndex            * sIndex;
    void                * sIndexHandle = NULL;
    smiRange            * sKeyRange = NULL;
    qtcMetaRangeColumn    sRangeColumn[QC_MAX_KEY_COLUMN_COUNT];
    smiRange              sRange;
    
    mtcTuple            * sChildTuple;

    //---------------------------------------
    // ���� ��ü�� ���� ���� �Ҵ�
    //---------------------------------------

    sChildTuple = &(QC_PRIVATE_TMPLATE(aStatement)->tmplate.rows[aChildTableRef->table]);
        
    sTableType = aChildTableInfo->tableFlag & SMI_TABLE_TYPE_MASK;

    if ( sTableType == SMI_TABLE_DISK )
    {
        // Disk Table�� ���
        IDE_TEST( qdbCommon::getDiskRowSize( aChildTableInfo,
                                             & sRowSize )
                  != IDE_SUCCESS );
        
        // Record Read�� ���� ������ �Ҵ��Ѵ�.
        // To fix BUG-14820
        // Disk-variable �÷��� rid�񱳸� ���� �ʱ�ȭ �ؾ� ��.
        IDU_LIMITPOINT("qdnForeignKey::getChildRowWithRow::malloc");
        IDE_TEST( aStatement->qmxMem->cralloc( sRowSize,
                                               (void **) & sRow )
                  != IDE_SUCCESS );
    }
    else
    {
        // Memory Table�� ���
        // Nothing to do.
    }

    //---------------------------------------
    // PROJ-1705  fetch column list ����
    //---------------------------------------

    if ( sTableType == SMI_TABLE_DISK )
    {
        if ( aIsIndexTable == ID_TRUE )
        {
            // PROJ-1624 non-partitioned index
            // index table�� ���� fetch column list�� ���鶧��
            // foreign key�� �����ϰ� index�θ� �����Ѵ�.
            IDE_TEST( qdbCommon::makeFetchColumnList4Index(
                          QC_PRIVATE_TMPLATE(aStatement),
                          aChildTableInfo,
                          aSelectedIndex,
                          ID_TRUE,
                          & sFetchColumnList )
                      != IDE_SUCCESS );
        }
        else
        {
            IDE_TEST( qdbCommon::makeFetchColumnList4ChildTable(
                          QC_PRIVATE_TMPLATE(aStatement),
                          aChildTableInfo,
                          aForeignKey,
                          aSelectedIndex,
                          ID_TRUE,
                          & sFetchColumnList )
                      != IDE_SUCCESS );
        }
    }
    else
    {
        // Nothing to do.
    }
    
    //---------------------------------------
    // Index�� Key Range ����
    //---------------------------------------

    IDE_TEST( makeKeyRangeAndIndex( sTableType,
                                    aKeyRangeColumnCnt,
                                    aSelectedIndex,
                                    aUniqueConstraint,
                                    aCheckedTuple,
                                    aCheckedRow,
                                    sRangeColumn,
                                    & sRange,
                                    & sIndex,
                                    & sKeyRange )
              != IDE_SUCCESS );

    //---------------------------------------
    // child scan
    //---------------------------------------

    if ( sIndex != NULL )
    {
        SMI_CURSOR_PROP_INIT_FOR_INDEX_SCAN( &sCursorProperty,
                                             aStatement->mStatistics,
                                             sIndex->indexTypeId );

        sIndexHandle = sIndex->indexHandle;
    }
    else
    {
        SMI_CURSOR_PROP_INIT_FOR_FULL_SCAN( &sCursorProperty,
                                            aStatement->mStatistics );
    }
    
    sCursorProperty.mFetchColumnList = sFetchColumnList;
    
    sCursor.initialize();
            
    IDE_TEST( sCursor.open( QC_SMI_STMT(aStatement),
                            aChildTableInfo->tableHandle,
                            sIndexHandle,
                            // BUGBUG is this can be correct?
                            smiGetRowSCN(aChildTableInfo->tableHandle),
                            NULL,
                            sKeyRange,
                            smiGetDefaultKeyRange(),
                            smiGetDefaultFilter(),
                            QCM_META_CURSOR_FLAG,
                            SMI_SELECT_CURSOR,
                            & sCursorProperty )
              != IDE_SUCCESS );
    sIsCursorOpened = ID_TRUE;
            
    IDE_TEST(sCursor.beforeFirst() != IDE_SUCCESS);
    IDE_TEST(sCursor.readRow(&sRow, &sRid, SMI_FIND_NEXT) != IDE_SUCCESS);

    while ( sRow != NULL )
    {
        IDE_TEST( checkChildRow( aForeignKey,
                                 aKeyRangeColumnCnt,
                                 aUniqueConstraint,
                                 aCheckedTuple,
                                 sChildTuple,
                                 aCheckedRow,
                                 sRow )
                  != IDE_SUCCESS );

        IDE_TEST( sCursor.readRow(&sRow, &sRid, SMI_FIND_NEXT) != IDE_SUCCESS );
    }

    sIsCursorOpened = ID_FALSE;
    IDE_TEST( sCursor.close() != IDE_SUCCESS );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sIsCursorOpened == ID_TRUE )
    {
        (void) sCursor.close();
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_FAILURE;
}

IDE_RC
qdnForeignKey::getChildRowWithRow4Partition( qcStatement    * aStatement,
                                             qmsTableRef    * aChildTableRef,
                                             qcmForeignKey  * aForeignKey,
                                             UInt             aKeyRangeColumnCnt,
                                             qcmIndex       * aSelectedIndex,
                                             qcmIndex       * aUniqueConstraint,
                                             mtcTuple       * aCheckedTuple,
                                             const void     * aCheckedRow )
{
/***********************************************************************
 *
 * Description :
 *    partitioned table�� ���� getChildRowWithRow
 *
 * Implementation :
 *
 ***********************************************************************/

    smiTableCursor        sCursor;
    smiCursorProperties   sCursorProperty;
    smiFetchColumnList  * sFetchColumnList = NULL;
    const void          * sRow;
    const void          * sOrgRow = NULL;
    scGRID                sRid;
    UInt                  sRowSize = 0;
    idBool                sIsCursorOpened = ID_FALSE;

    // To Fix PR-10592
    // Index�� ����ϱ� ���� ������
    qcmIndex            * sIndex = NULL;
    void                * sIndexHandle = NULL;
    smiRange            * sKeyRange = NULL;
    smiRange            * sDiskKeyRange = NULL;
    smiRange            * sMemoryKeyRange = NULL;
    qtcMetaRangeColumn    sDiskRangeColumn[QC_MAX_KEY_COLUMN_COUNT];
    qtcMetaRangeColumn    sMemoryRangeColumn[QC_MAX_KEY_COLUMN_COUNT];
    smiRange              sDiskRange;
    smiRange              sMemoryRange;

    qcmTableInfo        * sSelectedTableInfo;
    qmsPartitionRef     * sSelectedPartitionRef;
    
    mtcTuple            * sChildTuple;

    //---------------------------------------
    // ���� ��ü�� ���� ���� �Ҵ�
    //---------------------------------------

    if ( aChildTableRef->partitionSummary->diskPartitionRef != NULL )
    {
        IDE_TEST( qdbCommon::getDiskRowSize( aChildTableRef->partitionSummary->diskPartitionRef->partitionInfo,
                                             & sRowSize )
                  != IDE_SUCCESS );

        // Record Read�� ���� ������ �Ҵ��Ѵ�.
        // To fix BUG-14820
        // Disk-variable �÷��� rid�񱳸� ���� �ʱ�ȭ �ؾ� ��.
        IDU_LIMITPOINT("qdnForeignKey::getChildRowWithRow::malloc");
        IDE_TEST( aStatement->qmxMem->cralloc( sRowSize,
                                               (void **) & sRow )
                  != IDE_SUCCESS );

        sOrgRow = sRow;
    }
    else
    {
        /* Nothing to do */
    }

    //---------------------------------------
    // PROJ-1705  fetch column list ����
    //---------------------------------------

    if ( aChildTableRef->partitionSummary->diskPartitionRef != NULL )
    {
        IDE_TEST( qdbCommon::makeFetchColumnList4ChildTable(
                      QC_PRIVATE_TMPLATE(aStatement),
                      aChildTableRef->partitionSummary->diskPartitionRef->partitionInfo,
                      aForeignKey,
                      aSelectedIndex,
                      ID_TRUE,
                      & sFetchColumnList )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }
    
    //---------------------------------------
    // Index�� Key Range ����
    //---------------------------------------

    if ( aChildTableRef->partitionSummary->diskPartitionRef != NULL )
    {
        IDE_TEST( changePartIndex( aChildTableRef->partitionSummary->diskPartitionRef->partitionInfo,
                                   aKeyRangeColumnCnt,
                                   aSelectedIndex,
                                   & sIndex )
                  != IDE_SUCCESS );

        IDE_TEST( makeKeyRangeAndIndex( SMI_TABLE_DISK,
                                        aKeyRangeColumnCnt,
                                        sIndex,
                                        aUniqueConstraint,
                                        aCheckedTuple,
                                        aCheckedRow,
                                        sDiskRangeColumn,
                                        & sDiskRange,
                                        & sIndex,
                                        & sDiskKeyRange )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    if ( aChildTableRef->partitionSummary->memoryOrVolatilePartitionRef != NULL )
    {
        IDE_TEST( changePartIndex( aChildTableRef->partitionSummary->memoryOrVolatilePartitionRef->partitionInfo,
                                   aKeyRangeColumnCnt,
                                   aSelectedIndex,
                                   & sIndex )
                  != IDE_SUCCESS );

        IDE_TEST( makeKeyRangeAndIndex( SMI_TABLE_MEMORY,
                                        aKeyRangeColumnCnt,
                                        sIndex,
                                        aUniqueConstraint,
                                        aCheckedTuple,
                                        aCheckedRow,
                                        sMemoryRangeColumn,
                                        & sMemoryRange,
                                        & sIndex,
                                        & sMemoryKeyRange )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    //---------------------------------------
    // child scan
    //---------------------------------------
            
    sSelectedPartitionRef = aChildTableRef->partitionRef;

    while( sSelectedPartitionRef != NULL )
    {
        sChildTuple = &(QC_PRIVATE_TMPLATE(aStatement)->tmplate.rows[sSelectedPartitionRef->table]);
        sSelectedTableInfo = sSelectedPartitionRef->partitionInfo;

        IDE_TEST( changePartIndex( sSelectedTableInfo,
                                   aKeyRangeColumnCnt,
                                   aSelectedIndex,
                                   & sIndex )
                  != IDE_SUCCESS );

        if ( sIndex != NULL )
        {
            SMI_CURSOR_PROP_INIT_FOR_INDEX_SCAN( &sCursorProperty,
                                                 aStatement->mStatistics,
                                                 sIndex->indexTypeId );

            sIndexHandle = sIndex->indexHandle;
        }
        else
        {
            SMI_CURSOR_PROP_INIT_FOR_FULL_SCAN( &sCursorProperty,
                                                aStatement->mStatistics );
        }
        
        /* PROJ-2464 hybrid partitioned table ���� */
        if ( QCM_TABLE_TYPE_IS_DISK( sSelectedPartitionRef->partitionInfo->tableFlag ) == ID_TRUE )
        {
            sCursorProperty.mFetchColumnList = sFetchColumnList;
            sKeyRange = sDiskKeyRange;
        }
        else
        {
            sCursorProperty.mFetchColumnList = NULL;
            sKeyRange = sMemoryKeyRange;
        }

        //---------------------------------------
        // Cursor ����
        //---------------------------------------
        
        sCursor.initialize();

        //---------------------------------------
        // PROJ-1705  fetch column list ����
        //---------------------------------------

        IDE_TEST( sCursor.open( QC_SMI_STMT(aStatement),
                                sSelectedTableInfo->tableHandle,
                                sIndexHandle,
                                // BUGBUG is this can be correct?
                                smiGetRowSCN(sSelectedTableInfo->tableHandle),
                                NULL,
                                sKeyRange,
                                smiGetDefaultKeyRange(),
                                smiGetDefaultFilter(),
                                QCM_META_CURSOR_FLAG,
                                SMI_SELECT_CURSOR,
                                & sCursorProperty )
                  != IDE_SUCCESS );
        sIsCursorOpened = ID_TRUE;
                
        IDE_TEST( sCursor.beforeFirst() != IDE_SUCCESS );
        IDE_TEST( sCursor.readRow(&sRow, &sRid, SMI_FIND_NEXT) != IDE_SUCCESS );

        while ( sRow != NULL )
        {
            IDE_TEST( checkChildRow( aForeignKey,
                                     aKeyRangeColumnCnt,
                                     aUniqueConstraint,
                                     aCheckedTuple,
                                     sChildTuple,
                                     aCheckedRow,
                                     sRow )
                      != IDE_SUCCESS );

            IDE_TEST( sCursor.readRow(&sRow, &sRid, SMI_FIND_NEXT) != IDE_SUCCESS );
        }

        sRow = sOrgRow;

        sIsCursorOpened = ID_FALSE;
        IDE_TEST( sCursor.close() != IDE_SUCCESS );
        
        sSelectedPartitionRef = sSelectedPartitionRef->next;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sIsCursorOpened == ID_TRUE )
    {
        (void) sCursor.close();
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_FAILURE;
}

IDE_RC
qdnForeignKey::getChildRowWithRow4IndexTable( qcStatement      * aStatement,
                                              qmsIndexTableRef * aChildIndexTable,
                                              qmsTableRef      * aChildTableRef,
                                              qcmTableInfo     * aChildTableInfo,
                                              qcmForeignKey    * aForeignKey,
                                              UInt               aKeyRangeColumnCnt,
                                              qcmIndex         * aSelectedIndex,
                                              qcmIndex         * aUniqueConstraint,
                                              mtcTuple         * aCheckedTuple,
                                              const void       * aCheckedRow )
{
/***********************************************************************
 *
 * Description :
 *    index table�� ���� getChildRowWithRow
 *
 * Implementation :
 *
 ***********************************************************************/

    qcmTableInfo        * sIndexTableInfo;
    smiTableCursor        sCursor;
    smiTableCursor      * sPartCursor = NULL;
    idBool              * sPartCursorOpened = NULL;
    smiCursorProperties   sCursorProperty;
    smiCursorProperties   sPartCursorProperty;
    smiFetchColumnList  * sFetchColumnList = NULL;
    smiFetchColumnList  * sPartFetchColumnList = NULL;
    const void          * sRow;
    const void          * sPartRow;
    const void          * sOrgPartRow;
    scGRID                sRid;
    UInt                  sTableType;
    UInt                  sRowSize;
    UInt                  sIndexRowSize;
    idBool                sIsCursorOpened = ID_FALSE;

    mtcTuple            * sChildTuple;
    
    // To Fix PR-10592
    // Index�� ����ϱ� ���� ������
    qcmIndex            * sIndex;
    void                * sIndexHandle = NULL;
    smiRange            * sKeyRange = NULL;
    qtcMetaRangeColumn    sRangeColumn[QC_MAX_KEY_COLUMN_COUNT];
    smiRange              sRange;

    qcmTableInfo        * sSelectedTableInfo;
    qmsPartitionRef     * sSelectedPartitionRef;
    
    qcmIndex            * sIndexTableIndex = NULL;
    qcmColumn           * sOIDColumn;
    qcmColumn           * sRIDColumn;
    smOID                 sPartOID;
    scGRID                sRowGRID;
    qmsPartRefIndexInfo   sPartIndexInfo;
    UInt                  sPartOrder = 0;
    UInt                  i;

    sChildTuple = &(QC_PRIVATE_TMPLATE(aStatement)->tmplate.rows[aChildTableRef->table]);
    
    sTableType = aChildTableInfo->tableFlag & SMI_TABLE_TYPE_MASK;

    IDE_DASSERT( sTableType == SMI_TABLE_DISK );

    sIndexTableInfo = aChildIndexTable->tableInfo;
    
    //---------------------------------------
    // partitionRef index ����
    //---------------------------------------

    IDE_TEST( qmsIndexTable::initializePartitionRefIndex(
                  aStatement,
                  aChildTableRef->partitionRef,
                  aChildTableRef->partitionCount,
                  & sPartIndexInfo )
              != IDE_SUCCESS );
    
    //---------------------------------------
    // partition cursor�� ���� ���� �Ҵ�
    //---------------------------------------

    IDE_TEST( aStatement->qmxMem->alloc(
                  ID_SIZEOF(smiTableCursor) * aChildTableRef->partitionCount,
                  (void **) & sPartCursor )
              != IDE_SUCCESS );
    
    IDE_TEST( aStatement->qmxMem->alloc(
                  ID_SIZEOF(idBool) * aChildTableRef->partitionCount,
                  (void **) & sPartCursorOpened )
              != IDE_SUCCESS );

    // �ʱ�ȭ
    for ( i = 0; i < aChildTableRef->partitionCount; i++ )
    {
        sPartCursorOpened[i] = ID_FALSE;
    }
    
    //---------------------------------------
    // ���� ��ü�� ���� ���� �Ҵ�
    //---------------------------------------

    // index table�� �����Ҵ�
    IDE_TEST( qdbCommon::getDiskRowSize( sIndexTableInfo,
                                         & sIndexRowSize )
              != IDE_SUCCESS );
    
    // Record Read�� ���� ������ �Ҵ��Ѵ�.
    // To fix BUG-14820
    // Disk-variable �÷��� rid�񱳸� ���� �ʱ�ȭ �ؾ� ��.
    IDU_LIMITPOINT("qdnForeignKey::getChildRowWithRow::malloc");
    IDE_TEST( aStatement->qmxMem->cralloc( sIndexRowSize,
                                           (void **) & sRow )
              != IDE_SUCCESS );

    // partition�� �����Ҵ�
    IDE_TEST( qdbCommon::getDiskRowSize( aChildTableInfo,
                                         & sRowSize )
              != IDE_SUCCESS );
    
    // Record Read�� ���� ������ �Ҵ��Ѵ�.
    // To fix BUG-14820
    // Disk-variable �÷��� rid�񱳸� ���� �ʱ�ȭ �ؾ� ��.
    IDU_LIMITPOINT("qdnForeignKey::getChildRowWithRow::malloc");
    IDE_TEST( aStatement->qmxMem->cralloc( sRowSize,
                                           (void **) & sPartRow )
              != IDE_SUCCESS );
    sOrgPartRow = sPartRow;

    //---------------------------------------
    // PROJ-1705  fetch column list ����
    //---------------------------------------

    IDE_TEST( qdbCommon::makeFetchColumnList4ChildTable(
                  QC_PRIVATE_TMPLATE(aStatement),
                  aChildTableInfo,
                  aForeignKey,
                  aSelectedIndex,
                  ID_TRUE,
                  & sPartFetchColumnList )
              != IDE_SUCCESS );

    // index table�� fetch column list ����
    // key, oid, rid ��� fetch�Ѵ�.
    IDE_TEST( qdbCommon::makeFetchColumnList(
                  QC_PRIVATE_TMPLATE(aStatement),
                  sIndexTableInfo->columnCount,
                  sIndexTableInfo->columns,
                  ID_TRUE,
                  & sFetchColumnList )
              != IDE_SUCCESS );
    
    //---------------------------------------
    // Index�� Key Range ����
    //---------------------------------------

    IDE_TEST( qmsIndexTable::findKeyIndex( sIndexTableInfo,
                                           & sIndexTableIndex )
              != IDE_SUCCESS );

    // index table�� key range ����
    IDE_TEST( makeKeyRangeAndIndex( sTableType,
                                    aKeyRangeColumnCnt,
                                    sIndexTableIndex,
                                    aUniqueConstraint,
                                    aCheckedTuple,
                                    aCheckedRow,
                                    sRangeColumn,
                                    & sRange,
                                    & sIndex,
                                    & sKeyRange )
              != IDE_SUCCESS );

    //---------------------------------------
    // column ����
    //---------------------------------------
    
    IDE_DASSERT( sIndexTableInfo->columnCount > 2 );
    
    sOIDColumn = &(sIndexTableInfo->columns[sIndexTableInfo->columnCount - 2]);

    IDE_DASSERT( idlOS::strMatch( sOIDColumn->name,
                                  idlOS::strlen( sOIDColumn->name ),
                                  QD_OID_COLUMN_NAME,
                                  QD_OID_COLUMN_NAME_SIZE ) == 0 );
    
    sRIDColumn = &(sIndexTableInfo->columns[sIndexTableInfo->columnCount - 1]);

    IDE_DASSERT( idlOS::strMatch( sRIDColumn->name,
                                  idlOS::strlen( sRIDColumn->name ),
                                  QD_RID_COLUMN_NAME,
                                  QD_RID_COLUMN_NAME_SIZE ) == 0 );
   
    //---------------------------------------
    // index table scan
    //---------------------------------------
    
    if ( sIndex != NULL )
    {
        SMI_CURSOR_PROP_INIT_FOR_INDEX_SCAN( &sCursorProperty, aStatement->mStatistics,
                                             sIndex->indexTypeId );
        sIndexHandle = sIndex->indexHandle;
    }
    else
    {
        SMI_CURSOR_PROP_INIT_FOR_FULL_SCAN( &sCursorProperty, aStatement->mStatistics );
    }
    sCursorProperty.mFetchColumnList = sFetchColumnList;
    
    SMI_CURSOR_PROP_INIT_FOR_FULL_SCAN( &sPartCursorProperty, aStatement->mStatistics );
    sPartCursorProperty.mFetchColumnList = sPartFetchColumnList;
    
    sCursor.initialize();

    IDE_TEST( sCursor.open( QC_SMI_STMT(aStatement),
                            sIndexTableInfo->tableHandle,
                            sIndexHandle,
                            // BUGBUG is this can be correct?
                            smiGetRowSCN(sIndexTableInfo->tableHandle),
                            NULL,
                            sKeyRange,
                            smiGetDefaultKeyRange(),
                            smiGetDefaultFilter(),
                            QCM_META_CURSOR_FLAG,
                            SMI_SELECT_CURSOR,
                            & sCursorProperty )
              != IDE_SUCCESS );
    sIsCursorOpened = ID_TRUE;
            
    IDE_TEST(sCursor.beforeFirst() != IDE_SUCCESS);
    IDE_TEST(sCursor.readRow(&sRow, &sRid, SMI_FIND_NEXT) != IDE_SUCCESS);

    while ( sRow != NULL )
    {
        sPartOID = *(smOID*) ((UChar*)sRow + sOIDColumn->basicInfo->column.offset);
        sRowGRID = *(scGRID*) ((UChar*)sRow + sRIDColumn->basicInfo->column.offset);

        // partOID�� ã�´�.
        IDE_TEST( qmsIndexTable::findPartitionRefIndex( & sPartIndexInfo,
                                                        sPartOID,
                                                        & sSelectedPartitionRef,
                                                        & sPartOrder )
                  != IDE_SUCCESS );

        sSelectedTableInfo = sSelectedPartitionRef->partitionInfo;
        
        // open cursor
        if ( sPartCursorOpened[sPartOrder] == ID_FALSE )
        {
            sPartCursor[sPartOrder].initialize();

            IDE_TEST( sPartCursor[sPartOrder].open( QC_SMI_STMT(aStatement),
                                                    sSelectedTableInfo->tableHandle,
                                                    NULL,
                                                    // BUGBUG is this can be correct?
                                                    smiGetRowSCN(sSelectedTableInfo->tableHandle),
                                                    NULL,
                                                    smiGetDefaultKeyRange(),
                                                    smiGetDefaultKeyRange(),
                                                    smiGetDefaultFilter(),
                                                    QCM_META_CURSOR_FLAG,
                                                    SMI_SELECT_CURSOR,
                                                    & sPartCursorProperty )
                      != IDE_SUCCESS );

            sPartCursorOpened[sPartOrder] = ID_TRUE;
        }
        else
        {
            // Nothing to do.
        }
            
        sPartRow = sOrgPartRow;
        
        IDE_TEST( sPartCursor[sPartOrder].readRowFromGRID( & sPartRow, sRowGRID )
                  != IDE_SUCCESS );

        // �ݵ�� �����ؾ� �Ѵ�.
        IDE_TEST_RAISE( sPartRow == NULL, ERR_NO_ROW );
            
        IDE_TEST( checkChildRow( aForeignKey,
                                 aKeyRangeColumnCnt,
                                 aUniqueConstraint,
                                 aCheckedTuple,
                                 sChildTuple,
                                 aCheckedRow,
                                 sPartRow )
                  != IDE_SUCCESS );

        IDE_TEST( sCursor.readRow(&sRow, &sRid, SMI_FIND_NEXT) != IDE_SUCCESS );
    }

    sIsCursorOpened = ID_FALSE;
    IDE_TEST( sCursor.close() != IDE_SUCCESS );

    for ( i = 0; i < aChildTableRef->partitionCount; i++ )
    {
        if ( sPartCursorOpened[i] == ID_TRUE )
        {
            sPartCursorOpened[i] = ID_FALSE;
            
            IDE_TEST( sPartCursor[i].close() != IDE_SUCCESS );
        }
        else
        {
            // Nothing to do.
        }
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NO_ROW )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qdnForeignKey::getChildRowWithRow4IndexTable",
                                  "row is not found" ));
    }
    IDE_EXCEPTION_END;

    if ( sIsCursorOpened == ID_TRUE )
    {
        (void) sCursor.close();
    }
    else
    {
        /* Nothing to do */
    }

    if ( sPartCursorOpened != NULL )
    {
        for ( i = 0; i < aChildTableRef->partitionCount; i++ )
        {
            if ( sPartCursorOpened[i] == ID_TRUE )
            {
                (void) sPartCursor[i].close();
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
    
    return IDE_FAILURE;
}

IDE_RC
qdnForeignKey::checkChildRow( qcmForeignKey    * aForeignKey,
                              UInt               aKeyRangeColumnCnt,
                              qcmIndex         * aUniqueConstraint,
                              mtcTuple         * aCheckedTuple,
                              mtcTuple         * aChildTuple,
                              const void       * aCheckedRow,
                              const void       * aChildRow )
{
/***********************************************************************
 *
 * Description :
 *    getChildRowWithRow �κ��� ȣ��, ���̺� update �ÿ�
 *    ������ ���� child ���̺��� �����ϰ� �־��� ���� �ƴ��� �˻�
 *
 * Implementation :
 *
 ***********************************************************************/

    idBool         sExactExist = ID_TRUE;
    SInt           sColumnOrder;
    SInt           sChildColumnOrder;
    mtdValueInfo   sValueInfo1;
    mtdValueInfo   sValueInfo2;
    UInt           i;
    mtcColumn    * sColumnsForChildRow;
    mtcColumn    * sColumnsForCheckedRow;
    
    // Key Range�� ���Ե��� ���� Column�� ���Ͽ� ���������� �˻��Ѵ�.
    for ( i = aKeyRangeColumnCnt; i < aUniqueConstraint->keyColCount; i++ )
    {
        sColumnOrder      = aUniqueConstraint->keyColumns[i].column.id & SMI_COLUMN_ID_MASK;
        sChildColumnOrder = aForeignKey->referencingColumn[i] & SMI_COLUMN_ID_MASK;

        sColumnsForCheckedRow = &( aCheckedTuple->columns[sColumnOrder] );
        sColumnsForChildRow = &( aChildTuple->columns[sChildColumnOrder] );
        
        sValueInfo1.column = sColumnsForCheckedRow;
        sValueInfo1.value  = aCheckedRow;
        sValueInfo1.flag   = MTD_OFFSET_USE;

        sValueInfo2.column = sColumnsForChildRow;
        sValueInfo2.value  = aChildRow;
        sValueInfo2.flag   = MTD_OFFSET_USE;

        if ( sColumnsForCheckedRow->module->
             keyCompare[MTD_COMPARE_MTDVAL_MTDVAL][MTD_COMPARE_ASCENDING]( &sValueInfo1,
                                                                           &sValueInfo2 ) != 0 )
        {
            sExactExist = ID_FALSE;
            break;
        }
        else
        {
            // Nothing to do.
        }
    }
    
    IDE_TEST_RAISE( sExactExist == ID_TRUE, ERR_CHILD_EXIST );
            
    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_CHILD_EXIST )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMX_CHILD_EXIST,
                                  aForeignKey->name ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qdnForeignKey::deleteChildRowWithRow4Table( qcStatement      * aStatement,
                                            qcmRefChildInfo  * aChildConstraints,
                                            qmsTableRef      * aChildTableRef,
                                            qcmTableInfo     * aChildTableInfo,
                                            qcmForeignKey    * aForeignKey,
                                            UInt               aKeyRangeColumnCnt,
                                            qcmIndex         * aSelectedIndex,
                                            qcmIndex         * aUniqueConstraint,
                                            mtcTuple         * aCheckedTuple,
                                            const void       * aCheckedRow )
{
/***********************************************************************
 *
 * Description :
 *    table�� ���� deleteChildRowWithRow
 *
 * Implementation :
 *
 ***********************************************************************/

    smiTableCursor        sCursor;
    smiCursorProperties   sCursorProperty;
    smiFetchColumnList  * sFetchColumnList = NULL;
    qcmColumn           * sColumnsForRow = NULL;
    UInt                  sTableType;
    UInt                  sMemoryFixRowSize;    
    UInt                  sRowSize;
    scGRID                sRid;
    const void          * sRow;
    const void          * sOrgRow = NULL;
    const void          * sOldRow;
    void                * sTriggerOldRow;
    idBool                sNeedTriggerRow;
    idBool                sIsCursorOpened = ID_FALSE;

    // To Fix PR-10592
    // Index�� ����ϱ� ���� ������
    qcmIndex            * sIndex = NULL;
    void                * sIndexHandle = NULL;
    smiRange            * sKeyRange = NULL;
    qtcMetaRangeColumn    sRangeColumn[QC_MAX_KEY_COLUMN_COUNT];
    smiRange              sRange;

    mtcTuple            * sChildTuple;

    //---------------------------------------
    // ���� ��ü�� ���� ���� �Ҵ�
    //---------------------------------------

    sChildTuple = &(QC_PRIVATE_TMPLATE(aStatement)->tmplate.rows[aChildTableRef->table]);
    
    sTableType = aChildTableInfo->tableFlag & SMI_TABLE_TYPE_MASK;

    //---------------------------------------
    // Record Read�� ���� ���� �Ҵ��� ���� �ο� ������ ���
    // trigger������ ���� ���� �Ҵ��� ���� �ο� ������ ���
    //---------------------------------------
    
    if ( sTableType == SMI_TABLE_DISK )
    {
        // Disk Table�� ���
        IDE_TEST( qdbCommon::getDiskRowSize( aChildTableInfo,
                                             & sRowSize )
                  != IDE_SUCCESS );
        
        // To fix BUG-14820
        // Disk-variable �÷��� rid�񱳸� ���� �ʱ�ȭ �ؾ� ��.
        IDU_LIMITPOINT("qdnForeignKey::deleteChildRowWithRow::malloc1");
        IDE_TEST( aStatement->qmxMem->cralloc( sRowSize,
                                               (void **) & sRow )
                  != IDE_SUCCESS );

        sOrgRow = sRow;
    }
    else
    {
        // Memory Table�� ���
        // trigger�� ���� rowsize�� ����Ѵ�.
        IDE_TEST( qdbCommon::getMemoryRowSize( aChildTableInfo,
                                               & sMemoryFixRowSize,
                                               & sRowSize )
                  != IDE_SUCCESS );
    }

    //------------------------------------------
    // delete row trigger�ʿ����� �˻�
    // Trigger Old Row Referencing�� ���� ���� �Ҵ�
    //------------------------------------------

    // To fix BUG-12622
    // before, after trigger������ row�� �ʿ����� �˻�.
    IDE_TEST( qdnTrigger::needTriggerRow( aStatement,
                                          aChildTableInfo,
                                          QCM_TRIGGER_BEFORE,
                                          QCM_TRIGGER_EVENT_DELETE,
                                          NULL,
                                          & sNeedTriggerRow )
              != IDE_SUCCESS );

    if( sNeedTriggerRow == ID_FALSE )
    {    
        IDE_TEST( qdnTrigger::needTriggerRow( aStatement,
                                              aChildTableInfo,
                                              QCM_TRIGGER_AFTER,
                                              QCM_TRIGGER_EVENT_DELETE,
                                              NULL,
                                              & sNeedTriggerRow )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }
    
    if ( sNeedTriggerRow == ID_TRUE )
    {
        IDU_LIMITPOINT("qdnForeignKey::deleteChildRowWithRow::malloc2");
        IDE_TEST( aStatement->qmxMem->cralloc( sRowSize,
                                               (void **) & sTriggerOldRow )
                  != IDE_SUCCESS );
    }
    else
    {
        sTriggerOldRow = NULL;
    }
    
    //---------------------------------------
    // PROJ-1705  fetch column list ����
    // delete cascade�� ����,
    // foreignKey �˻縦 ���� ��� child table���� �˻��ؾ� �ϴµ�,
    // �� ���,
    // trigger �Ǵ� �˻��ؾ��ϴ� ��� child column�鿡 ����
    // fetch colum�� �������Ⱑ ���� �ʾ�
    // child table�� ��� �÷��� fetch column ������� ��´�.
    // ����������.
    //---------------------------------------

    if ( sTableType == SMI_TABLE_DISK )
    {
        IDE_TEST( qdbCommon::makeFetchColumnList(
                      QC_PRIVATE_TMPLATE(aStatement),
                      aChildTableInfo->columnCount,
                      aChildTableInfo->columns,
                      ID_TRUE,
                      & sFetchColumnList )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    //---------------------------------------
    // Index�� Key Range ����
    //---------------------------------------

    IDE_TEST( makeKeyRangeAndIndex( sTableType,
                                    aKeyRangeColumnCnt,
                                    aSelectedIndex,
                                    aUniqueConstraint,
                                    aCheckedTuple,
                                    aCheckedRow,
                                    sRangeColumn,
                                    & sRange,
                                    & sIndex,
                                    & sKeyRange )
              != IDE_SUCCESS );

    //---------------------------------------
    // child scan
    //---------------------------------------

    sColumnsForRow = aChildTableInfo->columns;

    if ( sIndex != NULL )
    {
        SMI_CURSOR_PROP_INIT_FOR_INDEX_SCAN( &sCursorProperty,
                                             aStatement->mStatistics,
                                             sIndex->indexTypeId );
        sIndexHandle = sIndex->indexHandle;
    }
    else
    {
        SMI_CURSOR_PROP_INIT_FOR_FULL_SCAN( &sCursorProperty,
                                            aStatement->mStatistics );
    }
    
    sCursorProperty.mFetchColumnList = sFetchColumnList;
    
    sCursor.initialize();

    IDE_TEST( sCursor.open( QC_SMI_STMT( aStatement ),
                            aChildTableInfo->tableHandle,
                            sIndexHandle,
                            // BUGBUG is this can be correct?
                            smiGetRowSCN(aChildTableInfo->tableHandle),
                            NULL,
                            sKeyRange,
                            smiGetDefaultKeyRange(),
                            smiGetDefaultFilter(),
                            //QCM_META_CURSOR_FLAG,
                            SMI_LOCK_WRITE | SMI_TRAVERSE_FORWARD | SMI_PREVIOUS_DISABLE,
                            SMI_DELETE_CURSOR,
                            & sCursorProperty )
              != IDE_SUCCESS );
    sIsCursorOpened = ID_TRUE;
        
    IDE_TEST( sCursor.beforeFirst() != IDE_SUCCESS );
    IDE_TEST( sCursor.readRow( &sRow, &sRid, SMI_FIND_NEXT )
              != IDE_SUCCESS );
    
    while ( sRow != NULL )
    {
        IDE_TEST( deleteChildRow( aStatement,
                                  aChildTableRef,
                                  aChildTableInfo,
                                  NULL /* aChildPartitionRef */,
                                  NULL /* aChildPartitionInfo */,
                                  aForeignKey,
                                  aKeyRangeColumnCnt,
                                  aUniqueConstraint,
                                  & sCursor,
                                  aCheckedTuple,
                                  aCheckedRow,
                                  sRow,
                                  sTriggerOldRow,
                                  sRowSize,
                                  sColumnsForRow,
                                  NULL,
                                  NULL,
                                  SC_NULL_GRID )
                  != IDE_SUCCESS );

        IDE_TEST( sCursor.readRow( &sRow, &sRid, SMI_FIND_NEXT )
                  != IDE_SUCCESS);
    }

    sIsCursorOpened = ID_FALSE;
    IDE_TEST( sCursor.close() != IDE_SUCCESS );

    //---------------------------------------
    // delete�� row�� ���� child table cascade delete ����.
    //---------------------------------------
            
    //------------------------------------------
    // DELETE�� �ο� �˻��� ����,
    // ���ſ����� ����� ù��° row ���� ��ġ�� cursor ��ġ ����
    //------------------------------------------

    IDE_TEST( sCursor.beforeFirstModified( SMI_FIND_MODIFIED_OLD )
              != IDE_SUCCESS );

    //------------------------------------------
    // Referencing �˻縦 ���� ������ Row���� �˻�
    //------------------------------------------
            
    sOldRow     = sOrgRow;

    IDE_TEST( sCursor.readOldRow( &sOldRow, &sRid )
              != IDE_SUCCESS );

    while ( sOldRow != NULL )
    {
        IDE_TEST( checkChild4DeleteChildRow( aStatement,
                                             aChildConstraints,
                                             aChildTableInfo,
                                             sChildTuple,
                                             sOldRow )
                  != IDE_SUCCESS );
            
        IDE_TEST( sCursor.readOldRow( &sOldRow, &sRid )
                  != IDE_SUCCESS );
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sIsCursorOpened == ID_TRUE )
    {
        (void) sCursor.close();
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_FAILURE;
}

IDE_RC
qdnForeignKey::deleteChildRowWithRow4Partition( qcStatement      * aStatement,
                                                qcmRefChildInfo  * aChildConstraints,
                                                qmsTableRef      * aChildTableRef,
                                                qcmTableInfo     * aChildTableInfo,
                                                qcmForeignKey    * aForeignKey,
                                                UInt               aKeyRangeColumnCnt,
                                                qcmIndex         * aSelectedIndex,
                                                qcmIndex         * aUniqueConstraint,
                                                mtcTuple         * aCheckedTuple,
                                                const void       * aCheckedRow )
{
/***********************************************************************
 *
 * Description :
 *    partitioned table�� ���� deleteChildRowWithRow
 *
 * Implementation :
 *
 ***********************************************************************/

    smiTableCursor         sCursor;
    smiCursorProperties    sCursorProperty;
    smiFetchColumnList   * sFetchColumnList = NULL;
    qcmColumn            * sColumnsForRow = NULL;
    UInt                   sRowSize;
    UInt                   sDiskRowSize;
    UInt                   sMemoryRowSize;
    scGRID                 sRid;
    const void           * sRow;
    const void           * sOrgRow              = NULL;
    const void           * sOldRow;
    void                 * sTriggerOldRow;
    void                 * sDiskTriggerOldRow   = NULL;
    void                 * sMemoryTriggerOldRow = NULL;
    idBool                 sNeedTriggerRow;
    idBool                 sIsCursorOpened = ID_FALSE;

    // To Fix PR-10592
    // Index�� ����ϱ� ���� ������
    qcmIndex             * sIndex = NULL;
    void                 * sIndexHandle = NULL;
    smiRange             * sKeyRange = NULL;
    smiRange             * sDiskKeyRange = NULL;
    smiRange             * sMemoryKeyRange = NULL;
    qtcMetaRangeColumn     sDiskRangeColumn[QC_MAX_KEY_COLUMN_COUNT];
    qtcMetaRangeColumn     sMemoryRangeColumn[QC_MAX_KEY_COLUMN_COUNT];
    smiRange               sDiskRange;
    smiRange               sMemoryRange;
    
    qcmTableInfo         * sSelectedTableInfo;
    qmsPartitionRef      * sSelectedPartitionRef;

    // PROJ-1624 non-partitioned index
    qmsIndexTableCursors   sIndexTableCursorInfo;
    qmsIndexTableCursors * sIndexTableCursor = NULL;
    UInt                   sMemoryFixRowSize;

    mtcTuple             * sChildTuple;

    sChildTuple = &(QC_PRIVATE_TMPLATE(aStatement)->tmplate.rows[aChildTableRef->table]);
    
    //---------------------------------------
    // Record Read�� ���� ���� �Ҵ��� ���� �ο� ������ ���
    // trigger������ ���� ���� �Ҵ��� ���� �ο� ������ ���
    //---------------------------------------

    /* PROJ-2464 hybrid partitioned table ���� */
    if ( aChildTableRef->partitionSummary->diskPartitionRef != NULL )
    {
        IDE_TEST( qdbCommon::getDiskRowSize( aChildTableRef->partitionSummary->diskPartitionRef->partitionInfo,
                                             & sDiskRowSize )
                  != IDE_SUCCESS );

        // To fix BUG-14820
        // Disk-variable �÷��� rid�񱳸� ���� �ʱ�ȭ �ؾ� ��.
        IDU_LIMITPOINT("qdnForeignKey::deleteChildRowWithRow::malloc1");
        IDE_TEST( aStatement->qmxMem->cralloc( sDiskRowSize,
                                               (void **) & sRow )
                  != IDE_SUCCESS );
        sOrgRow = sRow;
    }
    else
    {
        /* Nothing to do */
    }

    if ( aChildTableRef->partitionSummary->memoryOrVolatilePartitionRef != NULL )
    {
        // Memory Table�� ���
        // trigger�� ���� rowsize�� ����Ѵ�.
        IDE_TEST( qdbCommon::getMemoryRowSize( aChildTableRef->partitionSummary->
                                                   memoryOrVolatilePartitionRef->partitionInfo,
                                               & sMemoryFixRowSize,
                                               & sMemoryRowSize )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    //------------------------------------------
    // delete row trigger�ʿ����� �˻�
    // Trigger Old Row Referencing�� ���� ���� �Ҵ�
    //------------------------------------------

    // To fix BUG-12622
    // before, after trigger������ row�� �ʿ����� �˻�.
    IDE_TEST( qdnTrigger::needTriggerRow( aStatement,
                                          aChildTableInfo,
                                          QCM_TRIGGER_BEFORE,
                                          QCM_TRIGGER_EVENT_DELETE,
                                          NULL,
                                          & sNeedTriggerRow )
              != IDE_SUCCESS );

    if ( sNeedTriggerRow == ID_FALSE )
    {
        IDE_TEST( qdnTrigger::needTriggerRow( aStatement,
                                              aChildTableInfo,
                                              QCM_TRIGGER_AFTER,
                                              QCM_TRIGGER_EVENT_DELETE,
                                              NULL,
                                              & sNeedTriggerRow )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    if ( sNeedTriggerRow == ID_TRUE )
    {
        IDU_LIMITPOINT("qdnForeignKey::deleteChildRowWithRow::malloc2");

        /* PROJ-2464 hybrid partitioned table ���� */
        if ( aChildTableRef->partitionSummary->diskPartitionRef != NULL )
        {
            IDE_TEST( aStatement->qmxMem->cralloc( sDiskRowSize,
                                                   (void **) & sDiskTriggerOldRow )
                      != IDE_SUCCESS );
        }
        else
        {
            /* Nothing to do */
        }

        if ( aChildTableRef->partitionSummary->memoryOrVolatilePartitionRef != NULL )
        {
            IDE_TEST( aStatement->qmxMem->cralloc( sMemoryRowSize,
                                                   (void **) & sMemoryTriggerOldRow )
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
    
    //---------------------------------------
    // PROJ-1705  fetch column list ����
    // delete cascade�� ����,
    // foreignKey �˻縦 ���� ��� child table���� �˻��ؾ� �ϴµ�,
    // �� ���,
    // trigger �Ǵ� �˻��ؾ��ϴ� ��� child column�鿡 ����
    // fetch colum�� �������Ⱑ ���� �ʾ�
    // child table�� ��� �÷��� fetch column ������� ��´�.
    // ����������.
    //---------------------------------------

    //---------------------------------------
    // Index�� Key Range ����
    //---------------------------------------

    if ( aChildTableRef->partitionSummary->diskPartitionRef != NULL )
    {
        IDE_TEST( qdbCommon::makeFetchColumnList(
                      QC_PRIVATE_TMPLATE(aStatement),
                      aChildTableRef->partitionSummary->diskPartitionRef->partitionInfo->columnCount,
                      aChildTableRef->partitionSummary->diskPartitionRef->partitionInfo->columns,
                      ID_TRUE,
                      & sFetchColumnList )
                  != IDE_SUCCESS );

        IDE_TEST( changePartIndex( aChildTableRef->partitionSummary->diskPartitionRef->partitionInfo,
                                   aKeyRangeColumnCnt,
                                   aSelectedIndex,
                                   & sIndex )
                  != IDE_SUCCESS );

        IDE_TEST( makeKeyRangeAndIndex( SMI_TABLE_DISK,
                                        aKeyRangeColumnCnt,
                                        sIndex,
                                        aUniqueConstraint,
                                        aCheckedTuple,
                                        aCheckedRow,
                                        sDiskRangeColumn,
                                        & sDiskRange,
                                        & sIndex,
                                        & sDiskKeyRange )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    if ( aChildTableRef->partitionSummary->memoryOrVolatilePartitionRef != NULL )
    {
        IDE_TEST( changePartIndex( aChildTableRef->partitionSummary->memoryOrVolatilePartitionRef->partitionInfo,
                                   aKeyRangeColumnCnt,
                                   aSelectedIndex,
                                   & sIndex )
                  != IDE_SUCCESS );

        IDE_TEST( makeKeyRangeAndIndex( SMI_TABLE_MEMORY,
                                        aKeyRangeColumnCnt,
                                        sIndex,
                                        aUniqueConstraint,
                                        aCheckedTuple,
                                        aCheckedRow,
                                        sMemoryRangeColumn,
                                        & sMemoryRange,
                                        & sIndex,
                                        & sMemoryKeyRange )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    //---------------------------------------
    // index table cursor ����
    //---------------------------------------

    if ( aChildTableRef->indexTableRef != NULL )
    {
        IDE_TEST( qmsIndexTable::initializeIndexTableCursors(
                      aStatement,
                      aChildTableRef->indexTableRef,
                      aChildTableRef->indexTableCount,
                      NULL,
                      & sIndexTableCursorInfo )
                  != IDE_SUCCESS );
        
        sIndexTableCursor = & sIndexTableCursorInfo;
    }
    else
    {
        // Nothing to do.
    }
    
    //---------------------------------------
    // child scan
    //---------------------------------------
    
    sSelectedPartitionRef = aChildTableRef->partitionRef;

    while( sSelectedPartitionRef != NULL )
    {
        sSelectedTableInfo = sSelectedPartitionRef->partitionInfo;

        sColumnsForRow = sSelectedTableInfo->columns;

        IDE_TEST( changePartIndex( sSelectedTableInfo,
                                   aKeyRangeColumnCnt,
                                   aSelectedIndex,
                                   & sIndex )
                  != IDE_SUCCESS );

        if ( sIndex != NULL )
        {
            SMI_CURSOR_PROP_INIT_FOR_INDEX_SCAN( &sCursorProperty,
                                                 aStatement->mStatistics,
                                                 sIndex->indexTypeId );
            sIndexHandle = sIndex->indexHandle;
        }
        else
        {
            SMI_CURSOR_PROP_INIT_FOR_FULL_SCAN( &sCursorProperty,
                                                aStatement->mStatistics );
        }
        
        //---------------------------------------
        // PROJ-1705  fetch column list ����
        //---------------------------------------

        /* PROJ-2464 hybrid partitioned table ���� */
        if ( QCM_TABLE_TYPE_IS_DISK( sSelectedPartitionRef->partitionInfo->tableFlag ) == ID_TRUE )
        {
            sCursorProperty.mFetchColumnList = sFetchColumnList;
            sKeyRange                        = sDiskKeyRange;
            sTriggerOldRow                   = sDiskTriggerOldRow;
            sRowSize                         = sDiskRowSize;
        }
        else
        {
            sCursorProperty.mFetchColumnList = NULL;
            sKeyRange                        = sMemoryKeyRange;
            sTriggerOldRow                   = sMemoryTriggerOldRow;
            sRowSize                         = sMemoryRowSize;
        }

        //---------------------------------------
        // Cursor ����
        //---------------------------------------
        
        sCursor.initialize();

        IDE_TEST( sCursor.open( QC_SMI_STMT( aStatement ),
                                sSelectedTableInfo->tableHandle,
                                sIndexHandle,
                                // BUGBUG is this can be correct?
                                smiGetRowSCN(sSelectedTableInfo->tableHandle),
                                NULL,
                                sKeyRange,
                                smiGetDefaultKeyRange(),
                                smiGetDefaultFilter(),
                                //QCM_META_CURSOR_FLAG,
                                SMI_LOCK_WRITE | SMI_TRAVERSE_FORWARD | SMI_PREVIOUS_DISABLE,
                                SMI_DELETE_CURSOR,
                                & sCursorProperty )
                  != IDE_SUCCESS );
        sIsCursorOpened = ID_TRUE;
        
        IDE_TEST( sCursor.beforeFirst() != IDE_SUCCESS );
        IDE_TEST( sCursor.readRow( &sRow, &sRid, SMI_FIND_NEXT )
                  != IDE_SUCCESS );

        while ( sRow != NULL )
        {
            IDE_TEST( deleteChildRow( aStatement,
                                      aChildTableRef,
                                      aChildTableInfo,
                                      sSelectedPartitionRef,
                                      sSelectedTableInfo, /* PROJ-2359 Table/Partition Access Option */
                                      aForeignKey,
                                      aKeyRangeColumnCnt,
                                      aUniqueConstraint,
                                      & sCursor,
                                      aCheckedTuple,
                                      aCheckedRow,
                                      sRow,
                                      sTriggerOldRow,
                                      sRowSize,
                                      sColumnsForRow,
                                      NULL,
                                      sIndexTableCursor,
                                      sRid )
                      != IDE_SUCCESS );

            IDE_TEST( sCursor.readRow( &sRow, &sRid, SMI_FIND_NEXT )
                      != IDE_SUCCESS);
        }

        sRow = sOrgRow;

        sIsCursorOpened = ID_FALSE;
        IDE_TEST( sCursor.close() != IDE_SUCCESS );

        if ( sIndexTableCursor != NULL )
        {
            IDE_TEST( qmsIndexTable::closeIndexTableCursors(
                          sIndexTableCursor )
                      != IDE_SUCCESS );
        }
        else
        {
            // Nothing to do.
        }
    
        //---------------------------------------
        // delete�� row�� ���� child table cascade delete ����.
        //---------------------------------------
            
        //------------------------------------------
        // DELETE�� �ο� �˻��� ����,
        // ���ſ����� ����� ù��° row ���� ��ġ�� cursor ��ġ ����
        //------------------------------------------

        IDE_TEST( sCursor.beforeFirstModified( SMI_FIND_MODIFIED_OLD )
                  != IDE_SUCCESS );

        //------------------------------------------
        // Referencing �˻縦 ���� ������ Row���� �˻�
        //------------------------------------------
            
        sOldRow     = sOrgRow;

        IDE_TEST( sCursor.readOldRow( &sOldRow, &sRid )
                  != IDE_SUCCESS );

        while ( sOldRow != NULL )
        {
            IDE_TEST( checkChild4DeleteChildRow( aStatement,
                                                 aChildConstraints,
                                                 sSelectedTableInfo,
                                                 sChildTuple,
                                                 sOldRow )
                      != IDE_SUCCESS );
                
            IDE_TEST( sCursor.readOldRow( &sOldRow, &sRid )
                      != IDE_SUCCESS );
        }
        
        sSelectedPartitionRef = sSelectedPartitionRef->next;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sIsCursorOpened == ID_TRUE )
    {
        (void) sCursor.close();
    }
    else
    {
        /* Nothing to do */
    }

    if ( sIndexTableCursor != NULL )
    {
        qmsIndexTable::finalizeIndexTableCursors(
            sIndexTableCursor );
    }
    else
    {
        /* Nothing to do */
    }
    
    return IDE_FAILURE;
}

IDE_RC
qdnForeignKey::deleteChildRowWithRow4IndexTable( qcStatement      * aStatement,
                                                 qcmRefChildInfo  * aChildConstraints,
                                                 qmsIndexTableRef * aChildIndexTable,
                                                 qmsTableRef      * aChildTableRef,
                                                 qcmTableInfo     * aChildTableInfo,
                                                 qcmForeignKey    * aForeignKey,
                                                 UInt               aKeyRangeColumnCnt,
                                                 qcmIndex         * aUniqueConstraint,
                                                 mtcTuple         * aCheckedTuple,
                                                 const void       * aCheckedRow )
{
/***********************************************************************
 *
 * Description :
 *    index table�� ���� deleteChildRowWithRow
 *
 * Implementation :
 *
 ***********************************************************************/

    qcmTableInfo         * sIndexTableInfo;
    smiTableCursor         sCursor;
    smiTableCursor       * sPartCursor = NULL;
    UChar                * sPartCursorStatus = NULL;
    smiCursorProperties    sCursorProperty;
    smiCursorProperties    sPartCursorProperty;
    smiFetchColumnList   * sFetchColumnList = NULL;
    smiFetchColumnList   * sPartFetchColumnList = NULL;
    qcmColumn            * sColumnsForRow = NULL;
    UInt                   sTableType;
    UInt                   sRowSize;
    UInt                   sIndexRowSize;
    idBool                 sIsCursorOpened = ID_FALSE;
    scGRID                 sRid;
    const void           * sRow;
    const void           * sPartRow;
    const void           * sOrgPartRow;
    const void           * sOldRow;
    void                 * sTriggerOldRow;
    idBool                 sNeedTriggerRow;

    mtcTuple             * sChildTuple;
    
    // To Fix PR-10592
    // Index�� ����ϱ� ���� ������
    qcmIndex             * sIndex;
    void                 * sIndexHandle = NULL;
    smiRange             * sKeyRange = NULL;
    qtcMetaRangeColumn     sRangeColumn[QC_MAX_KEY_COLUMN_COUNT];
    smiRange               sRange;
    
    qcmTableInfo         * sSelectedTableInfo;
    qmsPartitionRef      * sSelectedPartitionRef;
    qmsPartitionRef      * sPartitionRef;
    
    qcmIndex             * sIndexTableIndex = NULL;
    qcmColumn            * sOIDColumn;
    qcmColumn            * sRIDColumn;
    smOID                  sPartOID;
    scGRID                 sRowGRID;
    qmsPartRefIndexInfo    sPartIndexInfo;
    UInt                   sPartOrder = 0;
    UInt                   i;

    // PROJ-1624 non-partitioned index
    qmsIndexTableCursors   sIndexTableCursorInfo;
    qmsIndexTableCursors * sIndexTableCursor = NULL;
    
    sChildTuple = &(QC_PRIVATE_TMPLATE(aStatement)->tmplate.rows[aChildTableRef->table]);
    
    sTableType = aChildTableInfo->tableFlag & SMI_TABLE_TYPE_MASK;

    IDE_DASSERT( sTableType == SMI_TABLE_DISK );

    sIndexTableInfo = aChildIndexTable->tableInfo;
    
    //---------------------------------------
    // partitionRef index ����
    //---------------------------------------

    IDE_TEST( qmsIndexTable::initializePartitionRefIndex(
                  aStatement,
                  aChildTableRef->partitionRef,
                  aChildTableRef->partitionCount,
                  & sPartIndexInfo )
              != IDE_SUCCESS );
    
    //---------------------------------------
    // partition cursor�� ���� ���� �Ҵ�
    //---------------------------------------

    IDE_TEST( aStatement->qmxMem->alloc(
                  ID_SIZEOF(smiTableCursor) * aChildTableRef->partitionCount,
                  (void **) & sPartCursor )
              != IDE_SUCCESS );
    
    IDE_TEST( aStatement->qmxMem->alloc(
                  ID_SIZEOF(UChar) * aChildTableRef->partitionCount,
                  (void **) & sPartCursorStatus )
              != IDE_SUCCESS );

    // �ʱ�ȭ
    for ( i = 0; i < aChildTableRef->partitionCount; i++ )
    {
        sPartCursorStatus[i] = QDN_PART_CURSOR_ALLOCED;
    }
    
    //---------------------------------------
    // ���� ��ü�� ���� ���� �Ҵ�
    //---------------------------------------

    // index table�� �����Ҵ�
    IDE_TEST( qdbCommon::getDiskRowSize( sIndexTableInfo,
                                         & sIndexRowSize )
              != IDE_SUCCESS );
    
    // Record Read�� ���� ������ �Ҵ��Ѵ�.
    // To fix BUG-14820
    // Disk-variable �÷��� rid�񱳸� ���� �ʱ�ȭ �ؾ� ��.
    IDU_LIMITPOINT("qdnForeignKey::getChildRowWithRow::malloc");
    IDE_TEST( aStatement->qmxMem->cralloc( sIndexRowSize,
                                           (void **) & sRow )
              != IDE_SUCCESS );

    // partition�� �����Ҵ�
    IDE_TEST( qdbCommon::getDiskRowSize( aChildTableInfo,
                                         & sRowSize )
              != IDE_SUCCESS );

    // Record Read�� ���� ������ �Ҵ��Ѵ�.
    // To fix BUG-14820
    // Disk-variable �÷��� rid�񱳸� ���� �ʱ�ȭ �ؾ� ��.
    IDU_LIMITPOINT("qdnForeignKey::getChildRowWithRow::malloc");
    IDE_TEST( aStatement->qmxMem->cralloc( sRowSize,
                                           (void **) & sPartRow )
              != IDE_SUCCESS );
    sOrgPartRow = sPartRow;
    
    //------------------------------------------
    // delete row trigger�ʿ����� �˻�
    // Trigger Old Row Referencing�� ���� ���� �Ҵ�
    //------------------------------------------

    // To fix BUG-12622
    // before, after trigger������ row�� �ʿ����� �˻�.
    IDE_TEST( qdnTrigger::needTriggerRow( aStatement,
                                          aChildTableInfo,
                                          QCM_TRIGGER_BEFORE,
                                          QCM_TRIGGER_EVENT_DELETE,
                                          NULL,
                                          & sNeedTriggerRow )
              != IDE_SUCCESS );

    if( sNeedTriggerRow == ID_FALSE )
    {    
        IDE_TEST( qdnTrigger::needTriggerRow( aStatement,
                                              aChildTableInfo,
                                              QCM_TRIGGER_AFTER,
                                              QCM_TRIGGER_EVENT_DELETE,
                                              NULL,
                                              & sNeedTriggerRow )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }
    
    if ( sNeedTriggerRow == ID_TRUE )
    {
        IDU_LIMITPOINT("qdnForeignKey::deleteChildRowWithRow::malloc2");
        IDE_TEST( aStatement->qmxMem->cralloc( sRowSize,
                                               (void **) & sTriggerOldRow )
                  != IDE_SUCCESS );
    }
    else
    {
        sTriggerOldRow = NULL;
    }
    
    //---------------------------------------
    // PROJ-1705  fetch column list ����
    // delete cascade�� ����,
    // foreignKey �˻縦 ���� ��� child table���� �˻��ؾ� �ϴµ�,
    // �� ���,
    // trigger �Ǵ� �˻��ؾ��ϴ� ��� child column�鿡 ����
    // fetch colum�� �������Ⱑ ���� �ʾ�
    // child table�� ��� �÷��� fetch column ������� ��´�.
    // ����������.
    //---------------------------------------

    IDE_TEST( qdbCommon::makeFetchColumnList(
                  QC_PRIVATE_TMPLATE(aStatement),
                  aChildTableInfo->columnCount,
                  aChildTableInfo->columns,
                  ID_TRUE,
                  & sPartFetchColumnList )
              != IDE_SUCCESS );
    
    // index table�� fetch column list ����
    // key, oid, rid ��� fetch�Ѵ�.
    IDE_TEST( qdbCommon::makeFetchColumnList(
                  QC_PRIVATE_TMPLATE(aStatement),
                  sIndexTableInfo->columnCount,
                  sIndexTableInfo->columns,
                  ID_TRUE,
                  & sFetchColumnList )
              != IDE_SUCCESS );

    //---------------------------------------
    // Index�� Key Range ����
    //---------------------------------------

    IDE_TEST( qmsIndexTable::findKeyIndex( sIndexTableInfo,
                                           & sIndexTableIndex )
              != IDE_SUCCESS );

    // index table�� key range ����
    IDE_TEST( makeKeyRangeAndIndex( sTableType,
                                    aKeyRangeColumnCnt,
                                    sIndexTableIndex,
                                    aUniqueConstraint,
                                    aCheckedTuple,
                                    aCheckedRow,
                                    sRangeColumn,
                                    & sRange,
                                    & sIndex,
                                    & sKeyRange )
              != IDE_SUCCESS );

    //---------------------------------------
    // index table cursor ����
    //---------------------------------------

    IDE_TEST( qmsIndexTable::initializeIndexTableCursors(
                  aStatement,
                  aChildTableRef->indexTableRef,
                  aChildTableRef->indexTableCount,
                  aChildIndexTable,
                  & sIndexTableCursorInfo )
              != IDE_SUCCESS );
    
    sIndexTableCursor = & sIndexTableCursorInfo;
    
    //---------------------------------------
    // column ����
    //---------------------------------------
    
    IDE_DASSERT( sIndexTableInfo->columnCount > 2 );
    
    sOIDColumn = &(sIndexTableInfo->columns[sIndexTableInfo->columnCount - 2]);

    IDE_DASSERT( idlOS::strMatch( sOIDColumn->name,
                                  idlOS::strlen( sOIDColumn->name ),
                                  QD_OID_COLUMN_NAME,
                                  QD_OID_COLUMN_NAME_SIZE ) == 0 );
    
    sRIDColumn = &(sIndexTableInfo->columns[sIndexTableInfo->columnCount - 1]);

    IDE_DASSERT( idlOS::strMatch( sRIDColumn->name,
                                  idlOS::strlen( sRIDColumn->name ),
                                  QD_RID_COLUMN_NAME,
                                  QD_RID_COLUMN_NAME_SIZE ) == 0 );
   
    //---------------------------------------
    // index table scan
    //---------------------------------------
    
    if ( sIndex != NULL )
    {
        SMI_CURSOR_PROP_INIT_FOR_INDEX_SCAN( &sCursorProperty, aStatement->mStatistics,
                                             sIndex->indexTypeId );
        sIndexHandle = sIndex->indexHandle;
    }
    else
    {
        SMI_CURSOR_PROP_INIT_FOR_FULL_SCAN( &sCursorProperty, aStatement->mStatistics );
    }
    sCursorProperty.mFetchColumnList = sFetchColumnList;
    
    SMI_CURSOR_PROP_INIT_FOR_FULL_SCAN( &sPartCursorProperty, aStatement->mStatistics );
    sPartCursorProperty.mFetchColumnList = sPartFetchColumnList;
    
    sCursor.initialize();

    IDE_TEST( sCursor.open( QC_SMI_STMT( aStatement ),
                            sIndexTableInfo->tableHandle,
                            sIndexHandle,
                            // BUGBUG is this can be correct?
                            smiGetRowSCN(sIndexTableInfo->tableHandle),
                            NULL,
                            sKeyRange,
                            smiGetDefaultKeyRange(),
                            smiGetDefaultFilter(),
                            //QCM_META_CURSOR_FLAG,
                            SMI_LOCK_WRITE | SMI_TRAVERSE_FORWARD | SMI_PREVIOUS_DISABLE,
                            SMI_DELETE_CURSOR,
                            & sCursorProperty )
              != IDE_SUCCESS );
    sIsCursorOpened = ID_TRUE;
    
    IDE_TEST( sCursor.beforeFirst() != IDE_SUCCESS );
    IDE_TEST( sCursor.readRow( &sRow, &sRid, SMI_FIND_NEXT )
              != IDE_SUCCESS );

    while ( sRow != NULL )
    {
        sPartOID = *(smOID*) ((UChar*)sRow + sOIDColumn->basicInfo->column.offset);
        sRowGRID = *(scGRID*) ((UChar*)sRow + sRIDColumn->basicInfo->column.offset);

        // partOID�� ã�´�.
        IDE_TEST( qmsIndexTable::findPartitionRefIndex( & sPartIndexInfo,
                                                        sPartOID,
                                                        & sSelectedPartitionRef,
                                                        & sPartOrder )
                  != IDE_SUCCESS );
        
        sSelectedTableInfo = sSelectedPartitionRef->partitionInfo;
        
        sColumnsForRow = sSelectedTableInfo->columns;
        
        // open cursor
        if ( sPartCursorStatus[sPartOrder] == QDN_PART_CURSOR_ALLOCED )
        {
            sPartCursor[sPartOrder].initialize();

            IDE_TEST( sPartCursor[sPartOrder].open(
                          QC_SMI_STMT(aStatement),
                          sSelectedTableInfo->tableHandle,
                          NULL,
                          // BUGBUG is this can be correct?
                          smiGetRowSCN(sSelectedTableInfo->tableHandle),
                          NULL,
                          smiGetDefaultKeyRange(),
                          smiGetDefaultKeyRange(),
                          smiGetDefaultFilter(),
                          SMI_LOCK_WRITE | SMI_TRAVERSE_FORWARD | SMI_PREVIOUS_DISABLE,
                          SMI_DELETE_CURSOR,
                          & sPartCursorProperty )
                      != IDE_SUCCESS );
            
            sPartCursorStatus[sPartOrder] = QDN_PART_CURSOR_OPENED;
        }
        else
        {
            // Nothing to do.
        }
        
        sPartRow = sOrgPartRow;        
            
        IDE_TEST( sPartCursor[sPartOrder].readRowFromGRID( & sPartRow, sRowGRID )
                  != IDE_SUCCESS );

        // �ݵ�� �����ؾ� �Ѵ�.
        IDE_TEST_RAISE( sPartRow == NULL, ERR_NO_ROW );
        
        IDE_TEST( deleteChildRow( aStatement,
                                  aChildTableRef,
                                  aChildTableInfo,
                                  sSelectedPartitionRef,
                                  sSelectedTableInfo, /* PROJ-2359 Table/Partition Access Option */
                                  aForeignKey,
                                  aKeyRangeColumnCnt,
                                  aUniqueConstraint,
                                  & sPartCursor[sPartOrder],
                                  aCheckedTuple,
                                  aCheckedRow,
                                  sPartRow,
                                  sTriggerOldRow,
                                  sRowSize,
                                  sColumnsForRow,
                                  & sCursor,
                                  sIndexTableCursor,
                                  sRowGRID )
                  != IDE_SUCCESS );
        
        IDE_TEST( sCursor.readRow( &sRow, &sRid, SMI_FIND_NEXT )
                  != IDE_SUCCESS);
    }

    sIsCursorOpened = ID_FALSE;
    IDE_TEST( sCursor.close() != IDE_SUCCESS );

    for ( i = 0; i < aChildTableRef->partitionCount; i++ )
    {
        if ( sPartCursorStatus[i] == QDN_PART_CURSOR_OPENED )
        {
            sPartCursorStatus[i] = QDN_PART_CURSOR_INITIALIZED;
            
            IDE_TEST( sPartCursor[i].close() != IDE_SUCCESS );
        }
        else
        {
            // Nothing to do.
        }
    }

    IDE_TEST( qmsIndexTable::closeIndexTableCursors(
                  sIndexTableCursor )
              != IDE_SUCCESS );
    
    //---------------------------------------
    // delete�� row�� ���� child table cascade delete ����.
    //---------------------------------------

    for ( i = 0, sPartitionRef = aChildTableRef->partitionRef;
          sPartitionRef != NULL;
          i++, sPartitionRef = sPartitionRef->next )
    {
        if ( sPartCursorStatus[i] == QDN_PART_CURSOR_INITIALIZED )
        {
            sSelectedTableInfo = sPartitionRef->partitionInfo;
        
            sColumnsForRow = sSelectedTableInfo->columns;
            
            //------------------------------------------
            // DELETE�� �ο� �˻��� ����,
            // ���ſ����� ����� ù��° row ���� ��ġ�� cursor ��ġ ����
            //------------------------------------------

            IDE_TEST( sPartCursor[i].beforeFirstModified( SMI_FIND_MODIFIED_OLD )
                      != IDE_SUCCESS );

            //------------------------------------------
            // Referencing �˻縦 ���� ������ Row���� �˻�
            //------------------------------------------
            
            sOldRow     = sOrgPartRow;

            IDE_TEST( sPartCursor[i].readOldRow( &sOldRow, &sRid )
                      != IDE_SUCCESS );

            while ( sOldRow != NULL )
            {
                IDE_TEST( checkChild4DeleteChildRow( aStatement,
                                                     aChildConstraints,
                                                     aChildTableInfo,
                                                     sChildTuple,
                                                     sOldRow )
                          != IDE_SUCCESS );
                
                IDE_TEST( sPartCursor[i].readOldRow( &sOldRow, &sRid )
                          != IDE_SUCCESS );
            }
        }
        else
        {
            // Nothing to do.
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NO_ROW )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qdnForeignKey::deleteChildRowWithRow4IndexTable",
                                  "row is not found" ));
    }
    IDE_EXCEPTION_END;

    if ( sIsCursorOpened == ID_TRUE )
    {
        (void) sCursor.close();
    }
    else
    {
        /* Nothing to do */
    }

    if ( sPartCursorStatus != NULL )
    {
        for ( i = 0; i < aChildTableRef->partitionCount; i++ )
        {
            if ( sPartCursorStatus[i] == QDN_PART_CURSOR_OPENED )
            {
                (void) sPartCursor[i].close();
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

    if ( sIndexTableCursor != NULL )
    {
        qmsIndexTable::finalizeIndexTableCursors(
            sIndexTableCursor );
    }
    else
    {
        /* Nothing to do */
    }
    
    return IDE_FAILURE;
}

IDE_RC
qdnForeignKey::deleteChildRow( qcStatement          * aStatement,
                               qmsTableRef          * aChildTableRef,
                               qcmTableInfo         * aChildTableInfo,
                               qmsPartitionRef      * aChildPartitionRef,
                               qcmTableInfo         * aChildPartitionInfo,
                               qcmForeignKey        * aForeignKey,
                               UInt                   aKeyRangeColumnCnt,
                               qcmIndex             * aUniqueConstraint,
                               smiTableCursor       * aChildCursor,
                               mtcTuple             * aCheckedTuple,
                               const void           * aCheckedRow,
                               const void           * aChildRow,
                               void                 * aChildTriggerRow,
                               UInt                   aChildRowSize,
                               qcmColumn            * aColumnsForChildRow,
                               smiTableCursor       * aSelectedIndexTableCursor,
                               qmsIndexTableCursors * aIndexTableCursor,
                               scGRID                 aIndexGRID )
{
/***********************************************************************
 *
 * Description :
 *    deleteChildRowWithRow �κ��� ȣ��, delete cascade�� child table
 *    ���ڵ� ����
 *
 * Implementation :
 *
 ***********************************************************************/

    idBool         sExactExist = ID_TRUE;
    SInt           sColumnOrder;
    SInt           sChildColumnOrder;
    mtdValueInfo   sValueInfo1;
    mtdValueInfo   sValueInfo2;
    UInt           i;
    mtcTuple     * sChildTuple;
    mtcColumn    * sColumnsForCheckedRow;
    mtcColumn    * sColumnsForChildRow;
    
    /* BUG-39445 */
    if ( aChildPartitionRef != NULL )
    {
        sChildTuple = &( QC_PRIVATE_TMPLATE(aStatement)->tmplate.rows[aChildPartitionRef->table] );
    }
    else
    {
        sChildTuple = &( QC_PRIVATE_TMPLATE(aStatement)->tmplate.rows[aChildTableRef->table] );
    }

    // Key Range�� ���Ե��� ���� Column�� ���Ͽ� ���������� �˻��Ѵ�.
    for ( i = aKeyRangeColumnCnt; i < aUniqueConstraint->keyColCount; i++ )
    {
        sColumnOrder      = aUniqueConstraint->keyColumns[i].column.id & SMI_COLUMN_ID_MASK;
        sChildColumnOrder = aForeignKey->referencingColumn[i] & SMI_COLUMN_ID_MASK;

        sColumnsForCheckedRow = &( aCheckedTuple->columns[sColumnOrder] );
        sColumnsForChildRow = &( sChildTuple->columns[sChildColumnOrder] );

        sValueInfo1.column = sColumnsForCheckedRow;
        sValueInfo1.value  = aCheckedRow;
        sValueInfo1.flag   = MTD_OFFSET_USE;

        sValueInfo2.column = sColumnsForChildRow;
        sValueInfo2.value  = aChildRow;
        sValueInfo2.flag   = MTD_OFFSET_USE;

        if ( sColumnsForCheckedRow->module->
             keyCompare[MTD_COMPARE_MTDVAL_MTDVAL][MTD_COMPARE_ASCENDING]( &sValueInfo1,
                                                                           &sValueInfo2 ) != 0 )
        {
            sExactExist = ID_FALSE;
            break;
        }
        else
        {
            // Nothing to do.
        }
    }

    if ( sExactExist == ID_TRUE )
    {
        /* PROJ-2359 Table/Partition Access Option */
        IDE_TEST( qmx::checkAccessOption( aChildTableInfo,
                                          ID_FALSE /* aIsInsertion */ )
                  != IDE_SUCCESS );

        if ( aChildPartitionInfo != NULL )
        {
            IDE_TEST( qmx::checkAccessOption( aChildPartitionInfo,
                                              ID_FALSE /* aIsInsertion */ )
                      != IDE_SUCCESS );
        }
        else
        {
            /* Nothing to do */
        }

        if ( aChildTriggerRow != NULL )
        {
            // OLD ROW REFERENCING�� ���� ����
            idlOS::memcpy( aChildTriggerRow,
                           aChildRow,
                           aChildRowSize );
        }
        else
        {
            // Nothing To Do
        }

        // To fix BUG-12622
        // delete cascade�� before trigger ȣ��.
        IDE_TEST( qdnTrigger::fireTrigger(
                      aStatement,
                      aStatement->qmxMem,
                      aChildTableInfo,
                      QCM_TRIGGER_ACTION_EACH_ROW,
                      QCM_TRIGGER_BEFORE,
                      QCM_TRIGGER_EVENT_DELETE,
                      NULL,                  // UPDATE Column
                      aChildCursor,          /* Table Cursor */
                      aIndexGRID,            /* Row GRID */
                      aChildTriggerRow,      // OLD ROW
                      aColumnsForChildRow,   // OLD ROW column
                      NULL,                  // NEW ROW
                      NULL )                 // NEW ROW column
                  != IDE_SUCCESS );

        // delete
        IDE_TEST( aChildCursor->deleteRow() != IDE_SUCCESS );
        
        // index table�� �����Ѵ�.
        if ( aIndexTableCursor != NULL )
        {
            // selected index table���� ���ڵ� ����
            if ( aSelectedIndexTableCursor != NULL )
            {
                IDE_TEST( aSelectedIndexTableCursor->deleteRow() != IDE_SUCCESS );
            }
            else
            {
                // Nothing to do.
            }
            
            // ������ index table���� ���ڵ� ����
            IDE_TEST( qmsIndexTable::deleteIndexTableCursors(
                          aStatement,
                          aIndexTableCursor,
                          aIndexGRID )
                      != IDE_SUCCESS );
        }
        else
        {
            // Nothing to do.
        }
        
        IDE_TEST( qdnTrigger::fireTrigger(
                      aStatement,
                      aStatement->qmxMem,
                      aChildTableInfo,
                      QCM_TRIGGER_ACTION_EACH_ROW,
                      QCM_TRIGGER_AFTER,
                      QCM_TRIGGER_EVENT_DELETE,
                      NULL,                  // UPDATE Column
                      aChildCursor,          /* Table Cursor */
                      aIndexGRID,            /* Row GRID */
                      aChildTriggerRow,      // OLD ROW
                      aColumnsForChildRow,   // OLD ROW column
                      NULL,                  // NEW ROW
                      NULL )                 // NEW ROW column
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing To Do
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
    
IDE_RC
qdnForeignKey::checkChild4DeleteChildRow( qcStatement      * aStatement,
                                          qcmRefChildInfo  * aChildConstraints,
                                          qcmTableInfo     * aChildTableInfo,
                                          mtcTuple         * aCheckedTuple,
                                          const void       * aOldRow )
{
/***********************************************************************
 *
 * Description :
 *    deleteChildRowWithRow �κ��� ȣ��, delete cascade�� child table
 *    ���ڵ� ����
 *
 * Implementation :
 *
 ***********************************************************************/

    qcmRefChildInfo  * sChildInfo = aChildConstraints;
    iduMemoryStatus    sQmxMemStatus;
    
    while ( sChildInfo != NULL )
    {
        if( sChildInfo->parentTableID == aChildTableInfo->tableID )
        {
            IDE_TEST_RAISE( aStatement->qmxMem->getStatus( & sQmxMemStatus ) != IDE_SUCCESS, ERR_MEM_OP );

            IDE_TEST( checkChildRefOnDelete(
                          aStatement,
                          aChildConstraints,
                          sChildInfo->parentTableID,
                          aCheckedTuple,
                          aOldRow,
                          ID_TRUE )
                      != IDE_SUCCESS );

            IDE_TEST_RAISE( aStatement->qmxMem->setStatus( &sQmxMemStatus ) != IDE_SUCCESS, ERR_MEM_OP );
        }
        else
        {
            // Nothing to do.
        }
                
        sChildInfo = sChildInfo->next;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_MEM_OP )
    {
        ideLog::log( IDE_ERR_0,
                     "Unexpected errors may have occurred:"
                     " qdnForeignKey::checkChild4DeleteChildRow"
                     " memory error" );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qdnForeignKey::updateNullChildRowWithRow4Table( qcStatement           * aStatement,
                                                qcmRefChildInfo       * aChildConstraints,
                                                qmsTableRef           * aChildTableRef,
                                                qcmTableInfo          * aChildTableInfo,
                                                qdConstraintSpec      * aChildCheckConstrList,
                                                qmsTableRef           * aDefaultTableRef,
                                                qcmColumn             * aDefaultExprColumns,
                                                qcmColumn             * aDefaultExprBaseColumns,
                                                UInt                    aUpdateColCount,
                                                UInt                  * aUpdateColumnID,
                                                qcmColumn             * aUpdateColumn,
                                                smiColumnList         * aUpdateColumnList,
                                                qcmRefChildUpdateType   aUpdateRowMovement,
                                                qcmForeignKey         * aForeignKey,
                                                UInt                    aKeyRangeColumnCnt,
                                                qcmIndex              * aSelectedIndex,
                                                qcmIndex              * aUniqueConstraint,
                                                mtcTuple              * aCheckedTuple,
                                                const void            * aCheckedRow )
{
/***********************************************************************
 *
 * Description :
 *    updateNullChildRowWithRow �κ��� ȣ��, set null�� child table
 *    ���ڵ� ������Ʈ
 *
 * Implementation :
 *
 ***********************************************************************/

    smiTableCursor        sCursor;
    smiCursorProperties   sCursorProperty;
    smiFetchColumnList  * sFetchColumnList = NULL;
    qcmColumn           * sColumnsForRow = NULL;
    UInt                  sTableType;
    UInt                  sMemoryFixRowSize;    
    UInt                  sRowSize;
    scGRID                sRid;
    const void          * sRow;
    const void          * sOrgRow = NULL;
    const void          * sOldRow;
    void                * sTriggerOldRow;
    void                * sTriggerNewRow;
    idBool                sNeedTriggerRow;
    idBool                sIsCursorOpened = ID_FALSE;

    // To Fix PR-10592
    // Index�� ����ϱ� ���� ������
    qcmIndex            * sIndex;
    void                * sIndexHandle = NULL;
    smiRange            * sKeyRange = NULL;
    qtcMetaRangeColumn    sRangeColumn[QC_MAX_KEY_COLUMN_COUNT];
    smiRange              sRange;

    // PROJ-2212 foreign key on delete set null
    // hidden column���� ���� 32 column���� Ŭ �� �ִ�.
    mtcColumn           * sUpdateColumn;
    smiValue            * sNullValues;
    void                * sDefaultExprRow;
    UInt                  i;

    // PROJ-2264
    const void         * sDicTableHandle;

    mtcTuple           * sChildTuple;

    //---------------------------------------
    // ���� ��ü�� ���� ���� �Ҵ�
    //---------------------------------------

    sChildTuple = &(QC_PRIVATE_TMPLATE(aStatement)->tmplate.rows[aChildTableRef->table]);
    
    sTableType = aChildTableInfo->tableFlag & SMI_TABLE_TYPE_MASK;

    //---------------------------------------
    // Record Read�� ���� ���� �Ҵ��� ���� �ο� ������ ���
    // trigger������ ���� ���� �Ҵ��� ���� �ο� ������ ���
    //---------------------------------------
    
    if ( sTableType == SMI_TABLE_DISK )
    {
        // Disk Table�� ���
        IDE_TEST( qdbCommon::getDiskRowSize( aChildTableInfo,
                                             & sRowSize )
                  != IDE_SUCCESS );
    
        // To fix BUG-14820
        // Disk-variable �÷��� rid�񱳸� ���� �ʱ�ȭ �ؾ� ��.
        IDU_LIMITPOINT("qdnForeignKey::deleteChildRowWithRow::malloc1");
        IDE_TEST( aStatement->qmxMem->cralloc( sRowSize,
                                               (void **) & sRow )
                  != IDE_SUCCESS );
        
        sOrgRow = sRow;
    }
    else
    {
        // Memory Table�� ���
        // trigger�� ���� rowsize�� ����Ѵ�.
        IDE_TEST( qdbCommon::getMemoryRowSize( aChildTableInfo,
                                               & sMemoryFixRowSize,
                                               & sRowSize )
                  != IDE_SUCCESS );
    }

    //------------------------------------------
    // Default Expr�� Row Buffer ����
    //------------------------------------------

    if ( aDefaultExprColumns != NULL )
    {
        if ( sTableType == SMI_TABLE_DISK )
        {
            // Disk Table�� ���
            IDE_TEST( aStatement->qmxMem->alloc( sRowSize,
                                                 (void **) & sDefaultExprRow )
                      != IDE_SUCCESS );
        }
        else
        {
            // Memory Table�� ���
            IDE_TEST( aStatement->qmxMem->alloc(
                          QC_PRIVATE_TMPLATE(aStatement)->tmplate.rows[aDefaultTableRef->table].rowOffset,
                          (void **) & sDefaultExprRow )
                      != IDE_SUCCESS );
        }
    }
    else
    {
        /* Nothing to do. */
    }

    //------------------------------------------
    // update row trigger�ʿ����� �˻�
    // Trigger Old Row Referencing�� ���� ���� �Ҵ�
    //------------------------------------------

    IDE_TEST( qdnTrigger::needTriggerRow( aStatement,
                                          aChildTableInfo,
                                          QCM_TRIGGER_AFTER,
                                          QCM_TRIGGER_EVENT_UPDATE,
                                          NULL,
                                          & sNeedTriggerRow )
              != IDE_SUCCESS );
    
    if ( sNeedTriggerRow == ID_TRUE )
    {
        IDU_LIMITPOINT("qdnForeignKey::updateChildRowWithRow::malloc2");
        IDE_TEST( aStatement->qmxMem->cralloc( sRowSize,
                                               (void **) & sTriggerOldRow )
                  != IDE_SUCCESS );
    }
    else
    {
        sTriggerOldRow = NULL;
    }
    
    if ( ( sNeedTriggerRow == ID_TRUE ) ||
         ( aChildCheckConstrList != NULL ) )
    {
        IDU_LIMITPOINT("qdnForeignKey::updateChildRowWithRow::malloc3");
        IDE_TEST( aStatement->qmxMem->cralloc( sRowSize,
                                               (void **) & sTriggerNewRow )
                  != IDE_SUCCESS );
    }
    else
    {
        sTriggerNewRow = NULL;
    }
        
    //---------------------------------------
    // PROJ-1705  fetch column list ����
    // delete cascade�� ����,
    // foreignKey �˻縦 ���� ��� child table���� �˻��ؾ� �ϴµ�,
    // �� ���,
    // trigger �Ǵ� �˻��ؾ��ϴ� ��� child column�鿡 ����
    // fetch colum�� �������Ⱑ ���� �ʾ�
    // child table�� ��� �÷��� fetch column ������� ��´�.
    // ����������.
    //---------------------------------------

    if ( sTableType == SMI_TABLE_DISK )
    {
        IDE_TEST( qdbCommon::makeFetchColumnList(
                      QC_PRIVATE_TMPLATE(aStatement),
                      aChildTableInfo->columnCount,
                      aChildTableInfo->columns,
                      ID_TRUE,
                      & sFetchColumnList )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }    

    //---------------------------------------
    // Index�� Key Range ����
    //---------------------------------------

    IDE_TEST( makeKeyRangeAndIndex( sTableType,
                                    aKeyRangeColumnCnt,
                                    aSelectedIndex,
                                    aUniqueConstraint,
                                    aCheckedTuple,
                                    aCheckedRow,
                                    sRangeColumn,
                                    & sRange,
                                    & sIndex,
                                    & sKeyRange )
              != IDE_SUCCESS );
        
    //---------------------------------------
    // PROJ-2212  null value list ����
    //---------------------------------------

    IDE_TEST( aStatement->qmxMem->alloc(
                  ID_SIZEOF(smiValue) * aUpdateColCount,
                  (void **) & sNullValues )
              != IDE_SUCCESS );
    
    for ( i = 0; i < aUpdateColCount; i++ )
    {
        // set null : update column ������ ����(update ����� �Ǵ� column)
        // ex> foreign key (i2,i3,i4)
        sUpdateColumn = aUpdateColumn[i].basicInfo;
        
        IDE_TEST_RAISE( ( sUpdateColumn->flag & MTC_COLUMN_NOTNULL_MASK )
                        == MTC_COLUMN_NOTNULL_TRUE,
                        ERR_NOT_ALLOW_NULL );
            
        if ( sTableType == SMI_TABLE_DISK )
        {
            sNullValues[i].value  = NULL;
            sNullValues[i].length = 0;
        }
        else
        {
            // PROJ-2264 Dictionary table
            if( (sUpdateColumn->column.flag & SMI_COLUMN_COMPRESSION_MASK)
                == SMI_COLUMN_COMPRESSION_FALSE )
            {
                if ( ( sUpdateColumn->column.flag & SMI_COLUMN_TYPE_MASK )
                     == SMI_COLUMN_TYPE_FIXED )
                {
                    sNullValues[i].value  = sUpdateColumn->module->staticNull;
                    sNullValues[i].length = sUpdateColumn->module->nullValueSize();
                }
                else
                {
                    // variable, lob
                    sNullValues[i].value  = NULL;
                    sNullValues[i].length = 0;
                }
            }
            else
            {
                // PROJ-2264 Dictionary table
                sDicTableHandle = smiGetTable( sUpdateColumn->column.mDictionaryTableOID );

                // OID �� canonize�� �ʿ����.
                // OID �� memory table �̹Ƿ� mtd value �� storing value �� �����ϴ�.
                sNullValues[i].value  = (const void *) & (SMI_MISC_TABLE_HEADER(sDicTableHandle)->mNullOID);
                sNullValues[i].length = ID_SIZEOF(smOID);
            }
        }
    }

    sColumnsForRow = aChildTableInfo->columns;
        
    //---------------------------------------
    // PROJ-1705  fetch column list ����
    //---------------------------------------

    if ( sIndex != NULL )
    {
        SMI_CURSOR_PROP_INIT_FOR_INDEX_SCAN( &sCursorProperty,
                                             aStatement->mStatistics,
                                             sIndex->indexTypeId );
        sIndexHandle = sIndex->indexHandle;
    }
    else
    {
        SMI_CURSOR_PROP_INIT_FOR_FULL_SCAN( &sCursorProperty,
                                            aStatement->mStatistics );
    }
    
    sCursorProperty.mFetchColumnList = sFetchColumnList;

    //---------------------------------------
    // child scan
    //---------------------------------------

    sCursor.initialize();

    IDE_TEST( sCursor.open( QC_SMI_STMT( aStatement ),
                            aChildTableInfo->tableHandle,
                            sIndexHandle,
                            smiGetRowSCN(aChildTableInfo->tableHandle),
                            aUpdateColumnList,
                            sKeyRange,
                            smiGetDefaultKeyRange(),
                            smiGetDefaultFilter(),
                            SMI_LOCK_WRITE | SMI_TRAVERSE_FORWARD | SMI_PREVIOUS_DISABLE,
                            SMI_UPDATE_CURSOR,
                            & sCursorProperty )
              != IDE_SUCCESS );
    sIsCursorOpened = ID_TRUE;
        
    IDE_TEST( sCursor.beforeFirst() != IDE_SUCCESS );
    IDE_TEST( sCursor.readRow( &sRow, &sRid, SMI_FIND_NEXT )
              != IDE_SUCCESS );
        
    while ( sRow != NULL )
    {
        /* PROJ-1090 Function-based Index */
        if ( aDefaultExprColumns != NULL )
        {
            QC_PRIVATE_TMPLATE(aStatement)->tmplate.
                rows[aChildTableRef->table].row = (void*) sRow;
        
            qmsDefaultExpr::setRowBufferFromBaseColumn(
                &(QC_PRIVATE_TMPLATE(aStatement)->tmplate),
                aChildTableRef->table,
                aDefaultTableRef->table,
                aDefaultExprBaseColumns,
                sDefaultExprRow );

            IDE_TEST( qmsDefaultExpr::setRowBufferFromSmiValueArray(
                          &(QC_PRIVATE_TMPLATE(aStatement)->tmplate),
                          aDefaultTableRef,
                          aUpdateColumn,
                          sDefaultExprRow,
                          sNullValues,
                          QCM_TABLE_TYPE_IS_DISK( aChildTableInfo->tableFlag ) )
                      != IDE_SUCCESS );
        
            IDE_TEST( qmsDefaultExpr::calculateDefaultExpression(
                          QC_PRIVATE_TMPLATE(aStatement),
                          aDefaultTableRef,
                          aUpdateColumn,
                          aDefaultExprColumns,
                          sDefaultExprRow,
                          sNullValues,
                          aChildTableInfo->columns )
                      != IDE_SUCCESS );
        
            QC_PRIVATE_TMPLATE(aStatement)->tmplate.
                rows[aChildTableRef->table].row = NULL;
        }
        else
        {
            /* Nothing to do */
        }
        
        IDE_TEST( updateNullChildRow( aStatement,
                                      aChildTableRef,
                                      NULL, /* aChildPartitionRef */
                                      aChildTableInfo,
                                      aForeignKey,
                                      aChildCheckConstrList,
                                      aKeyRangeColumnCnt,
                                      aUniqueConstraint,
                                      & sCursor,
                                      sFetchColumnList,
                                      aUpdateColCount,
                                      aUpdateColumnID,
                                      aUpdateColumnList,
                                      aUpdateColumn,
                                      aUpdateRowMovement,
                                      sNullValues,
                                      NULL, /* checkValue */
                                      NULL, /* insertValue */
                                      NULL, /* insertLobInfo */
                                      aCheckedTuple,
                                      aCheckedRow,
                                      sRow,
                                      sTriggerOldRow,
                                      sTriggerNewRow,
                                      sRowSize,
                                      sTriggerOldRow,
                                      sTriggerNewRow,
                                      sRowSize,
                                      sColumnsForRow,
                                      NULL, /* child index table */
                                      NULL, /* selected index table cursor */
                                      NULL, /* aIndexNullValue */
                                      NULL, /* index table cursor */
                                      NULL, /* partition index info */
                                      NULL, /* partition Cursor */
                                      SC_NULL_GRID )
                  != IDE_SUCCESS );

        IDE_TEST( sCursor.readRow( &sRow, &sRid, SMI_FIND_NEXT )
                  != IDE_SUCCESS );
    }

    sIsCursorOpened = ID_FALSE;
    IDE_TEST( sCursor.close() != IDE_SUCCESS );

    //---------------------------------------
    // PROJ-2212 foreign key on delete set null
    // null update��  row�� �� �����ϴ� grand child table
    // row ���� ���� üũ.
    //
    // ex> delete from t1 where i1=1;
    //   T1 : I1 -- t1 �� i1=1�� row ����
    //        ^
    //        |     
    //   T2 : I1 -- ���� updateRow() ���� i1=1�� column�� null update
    //        ^
    //        |
    //   T3 : I1 -- i1=1 �� row ���� ���� �˻� ( grand child table)
    //        ^
    //        |
    //   T4 : I1
    //     
    //---------------------------------------
            
    //------------------------------------------
    // update�� �ο� �˻��� ����,
    // ���ſ����� ����� ù��° row ���� ��ġ�� cursor ��ġ ����
    //------------------------------------------

    IDE_TEST( sCursor.beforeFirstModified( SMI_FIND_MODIFIED_OLD )
              != IDE_SUCCESS );

    //------------------------------------------
    // Referencing �˻縦 ���� ������ Row���� �˻�
    //------------------------------------------
            
    sOldRow     = sOrgRow;

    IDE_TEST( sCursor.readOldRow( &sOldRow, &sRid )
              != IDE_SUCCESS );
        
    while ( sOldRow != NULL )
    {
        IDE_TEST( checkChild4UpdateNullChildRow( aStatement,
                                                 aChildConstraints,
                                                 aChildTableInfo,
                                                 aForeignKey,
                                                 sChildTuple,
                                                 sOldRow )
                  != IDE_SUCCESS );
                
        IDE_TEST( sCursor.readOldRow( &sOldRow, &sRid )
                  != IDE_SUCCESS );
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NOT_ALLOW_NULL )
    {
        /* BUG-45680 insert ����� not null column�� ���� �����޽��� ������ column ���� ���. */
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMX_NOT_NULL_CONSTRAINT,
                                  "",
                                  "" ) );
    }
    IDE_EXCEPTION_END;

    if ( sIsCursorOpened == ID_TRUE )
    {
        (void) sCursor.close();
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_FAILURE;
}

IDE_RC
qdnForeignKey::updateNullChildRowWithRow4Partition( qcStatement           * aStatement,
                                                    qcmRefChildInfo       * aChildConstraints,
                                                    qmsTableRef           * aChildTableRef,
                                                    qcmTableInfo          * aChildTableInfo,
                                                    qdConstraintSpec      * aChildCheckConstrList,
                                                    qmsTableRef           * aDefaultTableRef,
                                                    qcmColumn             * aDefaultExprColumns,
                                                    qcmColumn             * aDefaultExprBaseColumns,
                                                    UInt                    aUpdateColCount,
                                                    UInt                  * aUpdateColumnID,
                                                    qcmColumn             * aUpdateColumn,
                                                    qcmRefChildUpdateType   aUpdateRowMovement,
                                                    qcmForeignKey         * aForeignKey,
                                                    UInt                    aKeyRangeColumnCnt,
                                                    qcmIndex              * aSelectedIndex,
                                                    qcmIndex              * aUniqueConstraint,
                                                    mtcTuple              * aCheckedTuple,
                                                    const void            * aCheckedRow )
{
/***********************************************************************
 *
 * Description :
 *    updateNullChildRowWithRow �κ��� ȣ��, set null�� child table
 *    ���ڵ� ������Ʈ
 *
 * Implementation :
 *
 ***********************************************************************/

    smiCursorType          sCursorType;
    smiTableCursor         sCursor;
    smiCursorProperties    sCursorProperty;
    smiFetchColumnList   * sFetchColumnList = NULL;
    qcmColumn            * sColumnsForRow = NULL;
    UInt                   sTableType;
    UInt                   sDiskRowSizeForPartitioned;
    UInt                   sDiskRowSize;
    UInt                   sMemoryRowSize;
    scGRID                 sRid;
    const void           * sRow;
    const void           * sOrgRow = NULL;
    const void           * sOldRow;
    void                 * sDiskTriggerOldRow   = NULL;
    void                 * sDiskTriggerNewRow   = NULL;
    void                 * sMemoryTriggerOldRow = NULL;
    void                 * sMemoryTriggerNewRow = NULL;
    idBool                 sNeedTriggerRow;
    idBool                 sIsCursorOpened  = ID_FALSE;

    // To Fix PR-10592
    // Index�� ����ϱ� ���� ������
    qcmIndex             * sIndex = NULL;
    void                 * sIndexHandle = NULL;
    smiRange             * sKeyRange = NULL;
    smiRange             * sDiskKeyRange = NULL;
    smiRange             * sMemoryKeyRange = NULL;
    qtcMetaRangeColumn     sDiskRangeColumn[QC_MAX_KEY_COLUMN_COUNT];
    qtcMetaRangeColumn     sMemoryRangeColumn[QC_MAX_KEY_COLUMN_COUNT];
    smiRange               sDiskRange;
    smiRange               sMemoryRange;

    mtcTuple             * sChildTuple;
    
    // PROJ-2212 foreign key on delete set null
    UInt                   sColumnOrder;
    mtcColumn            * sUpdateColumn;
    smiColumnList        * sUpdateColumnList;
    smiValue             * sNullValues;
    smiValue             * sCheckValues;
    smiValue             * sInsertValues = NULL;
    qmxLobInfo           * sInsertLobInfo = NULL;
    void                 * sDefaultExprRow;
    void                 * sPartitionTupleRow;
    UInt                   i;

    qcmTableInfo         * sSelectedTableInfo;
    qmsPartitionRef      * sSelectedPartitionRef;

    // PROJ-1624 non-partitioned index
    qmsIndexTableCursors   sIndexTableCursorInfo;
    qmsIndexTableCursors * sIndexTableCursor = NULL;
    UInt                   sMemoryFixRowSize;    

    sTableType = aChildTableInfo->tableFlag & SMI_TABLE_TYPE_MASK;
    
    if ( aUpdateRowMovement == QCM_CHILD_UPDATE_ROWMOVEMENT )
    {
        IDE_DASSERT( aChildTableRef->table != 0 );
        
        sCursorType = SMI_COMPOSITE_CURSOR;
    }
    else
    {
        sCursorType = SMI_UPDATE_CURSOR;
    }
    
    //---------------------------------------
    // Record Read�� ���� ���� �Ҵ��� ���� �ο� ������ ���
    // trigger������ ���� ���� �Ҵ��� ���� �ο� ������ ���
    //---------------------------------------
    /* PROJ-2464 hybrid partitioned table ���� */
    if ( aChildTableRef->partitionSummary->diskPartitionRef != NULL )
    {
        IDE_TEST( qdbCommon::getDiskRowSize( aChildTableRef->partitionSummary->diskPartitionRef->partitionInfo,
                                             & sDiskRowSize )
                  != IDE_SUCCESS );

        // To fix BUG-14820
        // Disk-variable �÷��� rid�񱳸� ���� �ʱ�ȭ �ؾ� ��.
        IDU_LIMITPOINT("qdnForeignKey::deleteChildRowWithRow::malloc1");
        IDE_TEST( aStatement->qmxMem->cralloc( sDiskRowSize,
                                               (void **) & sRow )
                  != IDE_SUCCESS );
        sOrgRow = sRow;
    }
    else
    {
        /* Nothing to do */
    }

    if ( aChildTableRef->partitionSummary->memoryOrVolatilePartitionRef != NULL )
    {
        // Memory Table�� ���
        // trigger�� ���� rowsize�� ����Ѵ�.
        IDE_TEST( qdbCommon::getMemoryRowSize( aChildTableRef->partitionSummary->
                                                   memoryOrVolatilePartitionRef->partitionInfo,
                                               & sMemoryFixRowSize,
                                               & sMemoryRowSize )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    //------------------------------------------
    // Default Expr�� Row Buffer ����
    //------------------------------------------

    if ( aDefaultExprColumns != NULL )
    {
        if ( sTableType == SMI_TABLE_DISK )
        {
            IDE_TEST( qdbCommon::getDiskRowSize( aChildTableInfo,
                                                 & sDiskRowSizeForPartitioned )
                      != IDE_SUCCESS );

            IDE_TEST( aStatement->qmxMem->alloc(
                          sDiskRowSizeForPartitioned,
                          (void **) & sDefaultExprRow )
                      != IDE_SUCCESS );
        }
        else
        {
            IDE_TEST( aStatement->qmxMem->alloc(
                          QC_PRIVATE_TMPLATE(aStatement)->tmplate.
                          rows[aDefaultTableRef->table].rowOffset,
                          (void **) & sDefaultExprRow )
                      != IDE_SUCCESS );
        }
    }
    else
    {
        /* Nothing to do. */
    }
    
    //------------------------------------------
    // update row trigger�ʿ����� �˻�
    // Trigger Old Row Referencing�� ���� ���� �Ҵ�
    //------------------------------------------

    IDE_TEST( qdnTrigger::needTriggerRow( aStatement,
                                          aChildTableInfo,
                                          QCM_TRIGGER_AFTER,
                                          QCM_TRIGGER_EVENT_UPDATE,
                                          NULL,
                                          & sNeedTriggerRow )
              != IDE_SUCCESS );
    
    if ( sNeedTriggerRow == ID_TRUE )
    {
        IDU_LIMITPOINT("qdnForeignKey::updateChildRowWithRow::malloc2");

        /* PROJ-2464 hybrid partitioned table ���� */
        if ( aChildTableRef->partitionSummary->diskPartitionRef != NULL )
        {
            IDE_TEST( aStatement->qmxMem->alloc( sDiskRowSize,
                                                 (void **) & sDiskTriggerOldRow )
                      != IDE_SUCCESS );
        }
        else
        {
            /* Nothing to do */
        }

        if ( aChildTableRef->partitionSummary->memoryOrVolatilePartitionRef != NULL )
        {
            IDE_TEST( aStatement->qmxMem->alloc( sMemoryRowSize,
                                                 (void **) & sMemoryTriggerOldRow )
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
    
    if ( ( sNeedTriggerRow == ID_TRUE ) ||
         ( aChildCheckConstrList != NULL ) )
    {
        IDU_LIMITPOINT("qdnForeignKey::updateChildRowWithRow::malloc3");

        /* PROJ-2464 hybrid partitioned table ���� */
        if ( aChildTableRef->partitionSummary->diskPartitionRef != NULL )
        {
            IDE_TEST( aStatement->qmxMem->alloc( sDiskRowSize,
                                                 (void **) & sDiskTriggerNewRow )
                      != IDE_SUCCESS );
        }
        else
        {
            /* Nothing to do */
        }

        if ( aChildTableRef->partitionSummary->memoryOrVolatilePartitionRef != NULL )
        {
            IDE_TEST( aStatement->qmxMem->alloc( sMemoryRowSize,
                                                 (void **) & sMemoryTriggerNewRow )
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
        
    //---------------------------------------
    // PROJ-1705  fetch column list ����
    // delete cascade�� ����,
    // foreignKey �˻縦 ���� ��� child table���� �˻��ؾ� �ϴµ�,
    // �� ���,
    // trigger �Ǵ� �˻��ؾ��ϴ� ��� child column�鿡 ����
    // fetch colum�� �������Ⱑ ���� �ʾ�
    // child table�� ��� �÷��� fetch column ������� ��´�.
    // ����������.
    //---------------------------------------

    if ( aChildTableRef->partitionSummary->diskPartitionRef != NULL )
    {
        IDE_TEST( qdbCommon::makeFetchColumnList(
                      QC_PRIVATE_TMPLATE(aStatement),
                      aChildTableRef->partitionSummary->diskPartitionRef->partitionInfo->columnCount,
                      aChildTableRef->partitionSummary->diskPartitionRef->partitionInfo->columns,
                      ID_TRUE,
                      & sFetchColumnList )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing To Do */
    }

    //---------------------------------------
    // Index�� Key Range ����
    //---------------------------------------

    if ( aChildTableRef->partitionSummary->diskPartitionRef != NULL )
    {
        IDE_TEST( changePartIndex( aChildTableRef->partitionSummary->diskPartitionRef->partitionInfo,
                                   aKeyRangeColumnCnt,
                                   aSelectedIndex,
                                   & sIndex )
                  != IDE_SUCCESS );

        IDE_TEST( makeKeyRangeAndIndex( SMI_TABLE_DISK,
                                        aKeyRangeColumnCnt,
                                        sIndex,
                                        aUniqueConstraint,
                                        aCheckedTuple,
                                        aCheckedRow,
                                        sDiskRangeColumn,
                                        & sDiskRange,
                                        & sIndex,
                                        & sDiskKeyRange )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    if ( aChildTableRef->partitionSummary->memoryOrVolatilePartitionRef != NULL )
    {
        IDE_TEST( changePartIndex( aChildTableRef->partitionSummary->memoryOrVolatilePartitionRef->partitionInfo,
                                   aKeyRangeColumnCnt,
                                   aSelectedIndex,
                                   & sIndex )
                  != IDE_SUCCESS );

        IDE_TEST( makeKeyRangeAndIndex( SMI_TABLE_MEMORY,
                                        aKeyRangeColumnCnt,
                                        sIndex,
                                        aUniqueConstraint,
                                        aCheckedTuple,
                                        aCheckedRow,
                                        sMemoryRangeColumn,
                                        & sMemoryRange,
                                        & sIndex,
                                        & sMemoryKeyRange )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }
        
    //---------------------------------------
    // PROJ-2212  update column list, null value list �Ҵ�
    //---------------------------------------

    IDE_TEST( aStatement->qmxMem->alloc(
                  ID_SIZEOF(smiColumnList) * aUpdateColCount,
                  (void **) & sUpdateColumnList )
              != IDE_SUCCESS );
    
    IDE_TEST( aStatement->qmxMem->alloc(
                  ID_SIZEOF(smiValue) * aUpdateColCount,
                  (void **) & sNullValues )
              != IDE_SUCCESS );

    //------------------------------------------
    // PROJ-2212  null value list ����
    //------------------------------------------

    for ( i = 0; i < aUpdateColCount; i++ )
    {
        // set null : update column ������ ����(update ����� �Ǵ� column)
        // ex> foreign key (i2,i3,i4)
        sColumnOrder = aUpdateColumn[i].basicInfo->column.id
            & SMI_COLUMN_ID_MASK;

        sUpdateColumn = aChildTableInfo->columns[sColumnOrder].basicInfo;

        IDE_TEST_RAISE( ( sUpdateColumn->flag & MTC_COLUMN_NOTNULL_MASK )
                                             == MTC_COLUMN_NOTNULL_TRUE,
                        ERR_NOT_ALLOW_NULL );

        /* PROJ-2334 PMT */
        if ( sTableType == SMI_TABLE_DISK )
        {
            sNullValues[i].value  = NULL;
            sNullValues[i].length = 0;
        }
        else
        {
            if ( ( sUpdateColumn->column.flag & SMI_COLUMN_TYPE_MASK )
                                             == SMI_COLUMN_TYPE_FIXED )
            {
                sNullValues[i].value  = sUpdateColumn->module->staticNull;
                sNullValues[i].length = sUpdateColumn->module->nullValueSize();
            }
            else
            {
                // variable, lob
                sNullValues[i].value  = NULL;
                sNullValues[i].length = 0;
            }
        }
    }

    //---------------------------------------
    // row movement�� insert values ����
    //---------------------------------------
    
    IDE_TEST( aStatement->qmxMem->alloc(
                  ID_SIZEOF(smiValue) * aChildTableInfo->columnCount,
                  (void **) & sCheckValues )
              != IDE_SUCCESS );

    if ( aUpdateRowMovement == QCM_CHILD_UPDATE_ROWMOVEMENT )
    {
        // lob info �ʱ�ȭ
        if ( aChildTableInfo->lobColumnCount > 0 )
        {
            // PROJ-1362
            IDE_TEST( qmx::initializeLobInfo(
                          aStatement,
                          & sInsertLobInfo,
                          (UShort)aChildTableInfo->lobColumnCount )
                      != IDE_SUCCESS );
        }
        else
        {
            // Nothing to do.
        }
        
        // insert smiValues �ʱ�ȭ
        IDE_TEST( aStatement->qmxMem->alloc(
                      ID_SIZEOF(smiValue) * aChildTableInfo->columnCount,
                      (void**)& sInsertValues )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    //---------------------------------------
    // index table cursor ����
    //---------------------------------------

    if ( aChildTableRef->indexTableRef != NULL )
    {
        IDE_TEST( qmsIndexTable::initializeIndexTableCursors(
                      aStatement,
                      aChildTableRef->indexTableRef,
                      aChildTableRef->indexTableCount,
                      NULL,
                      & sIndexTableCursorInfo )
                  != IDE_SUCCESS );
        
        sIndexTableCursor = & sIndexTableCursorInfo;
    }
    else
    {
        // Nothing to do.
    }
    
    //---------------------------------------
    // child scan
    //---------------------------------------

    sSelectedPartitionRef = aChildTableRef->partitionRef;

    while( sSelectedPartitionRef != NULL )
    {
        /* PROJ-2464 hybrid partitioned table ���� */
        sChildTuple = &(QC_PRIVATE_TMPLATE(aStatement)->tmplate.rows[sSelectedPartitionRef->table]);

        sSelectedTableInfo = sSelectedPartitionRef->partitionInfo;

        sColumnsForRow = sSelectedTableInfo->columns;

        IDE_TEST( changePartIndex( sSelectedTableInfo,
                                   aKeyRangeColumnCnt,
                                   aSelectedIndex,
                                   & sIndex )
                  != IDE_SUCCESS );

        //---------------------------------------
        // PROJ-1705  fetch column list ����
        //---------------------------------------

        if ( sIndex != NULL )
        {
            SMI_CURSOR_PROP_INIT_FOR_INDEX_SCAN( &sCursorProperty, aStatement->mStatistics,
                                                 sIndex->indexTypeId );
            sIndexHandle = sIndex->indexHandle;
        }
        else
        {
            SMI_CURSOR_PROP_INIT_FOR_FULL_SCAN( &sCursorProperty, aStatement->mStatistics );
        }
        
        /* PROJ-2464 hybrid partitioned table ���� */
        if ( QCM_TABLE_TYPE_IS_DISK( sSelectedPartitionRef->partitionInfo->tableFlag ) == ID_TRUE )
        {
            sCursorProperty.mFetchColumnList = sFetchColumnList;
            sKeyRange = sDiskKeyRange;
        }
        else
        {
            sCursorProperty.mFetchColumnList = NULL;
            sKeyRange = sMemoryKeyRange;
        }
    
        //------------------------------------------
        // PROJ-2212  update column list ����
        //------------------------------------------

        for ( i = 0; i < aUpdateColCount; i++ )
        {
            // set null : update column ������ ����(update ����� �Ǵ� column)
            // ex> foreign key (i2,i3,i4)
            sColumnOrder = aUpdateColumn[i].basicInfo->column.id
                & SMI_COLUMN_ID_MASK;

            sUpdateColumn = sSelectedTableInfo->columns[sColumnOrder].basicInfo;
            
            sUpdateColumnList[i].column = (smiColumn*) sUpdateColumn;

            if ( i < aUpdateColCount - 1 )
            {
                sUpdateColumnList[i].next = &sUpdateColumnList[i+1];
            }
            else
            {
                sUpdateColumnList[i].next = NULL;
            }
        }
        
        //---------------------------------------
        // Cursor ����
        //---------------------------------------
        
        sCursor.initialize();

        IDE_TEST( sCursor.open( QC_SMI_STMT( aStatement ),
                                sSelectedTableInfo->tableHandle,
                                sIndexHandle,
                                smiGetRowSCN(sSelectedTableInfo->tableHandle),
                                sUpdateColumnList,
                                sKeyRange,
                                smiGetDefaultKeyRange(),
                                smiGetDefaultFilter(),
                                SMI_LOCK_WRITE | SMI_TRAVERSE_FORWARD | SMI_PREVIOUS_DISABLE,
                                sCursorType,
                                & sCursorProperty )
                  != IDE_SUCCESS );
        sIsCursorOpened = ID_TRUE;
        
        IDE_TEST( sCursor.beforeFirst() != IDE_SUCCESS );
        IDE_TEST( sCursor.readRow( &sRow, &sRid, SMI_FIND_NEXT )
                  != IDE_SUCCESS );
        
        while ( sRow != NULL )
        {
            /* PROJ-1090 Function-based Index */
            if ( aDefaultExprColumns != NULL )
            {
                sPartitionTupleRow = sChildTuple->row;
                sChildTuple->row = (void *)sRow;
        
                qmsDefaultExpr::setRowBufferFromBaseColumn(
                    &(QC_PRIVATE_TMPLATE(aStatement)->tmplate),
                    sSelectedPartitionRef->table,
                    aDefaultTableRef->table,
                    aDefaultExprBaseColumns,
                    sDefaultExprRow );

                IDE_TEST( qmsDefaultExpr::setRowBufferFromSmiValueArray(
                              &(QC_PRIVATE_TMPLATE(aStatement)->tmplate),
                              aDefaultTableRef,
                              aUpdateColumn,
                              sDefaultExprRow,
                              sNullValues,
                              QCM_TABLE_TYPE_IS_DISK( aChildTableInfo->tableFlag ) )
                          != IDE_SUCCESS );
        
                IDE_TEST( qmsDefaultExpr::calculateDefaultExpression(
                              QC_PRIVATE_TMPLATE(aStatement),
                              aDefaultTableRef,
                              aUpdateColumn,
                              aDefaultExprColumns,
                              sDefaultExprRow,
                              sNullValues,
                              aChildTableInfo->columns )
                          != IDE_SUCCESS );
        
                sChildTuple->row = sPartitionTupleRow;
            }
            else
            {
                /* Nothing to do */
            }
            
            IDE_TEST( updateNullChildRow( aStatement,
                                          aChildTableRef,
                                          sSelectedPartitionRef,
                                          sSelectedTableInfo,
                                          aForeignKey,
                                          aChildCheckConstrList,
                                          aKeyRangeColumnCnt,
                                          aUniqueConstraint,
                                          & sCursor,
                                          sFetchColumnList,
                                          aUpdateColCount,
                                          aUpdateColumnID,
                                          sUpdateColumnList,
                                          aUpdateColumn,
                                          aUpdateRowMovement,
                                          sNullValues,
                                          sCheckValues,
                                          sInsertValues,
                                          sInsertLobInfo,
                                          aCheckedTuple,
                                          aCheckedRow,
                                          sRow,
                                          sDiskTriggerOldRow,
                                          sDiskTriggerNewRow,
                                          sDiskRowSize,
                                          sMemoryTriggerOldRow,
                                          sMemoryTriggerNewRow,
                                          sMemoryRowSize,
                                          sColumnsForRow,
                                          NULL, /* child index table */
                                          NULL, /* selected index table cursor */
                                          NULL, /* aIndexNullValue */
                                          sIndexTableCursor,
                                          NULL, /* partition index info */
                                          NULL, /* partition Cursor */
                                          sRid )
                      != IDE_SUCCESS );

            IDE_TEST( sCursor.readRow( &sRow, &sRid, SMI_FIND_NEXT )
                      != IDE_SUCCESS );
        }

        sRow = sOrgRow;

        sIsCursorOpened = ID_FALSE;
        IDE_TEST( sCursor.close() != IDE_SUCCESS );

        if ( sIndexTableCursor != NULL )
        {
            IDE_TEST( qmsIndexTable::closeIndexTableCursors(
                          sIndexTableCursor )
                      != IDE_SUCCESS );
        }
        else
        {
            // Nothing to do.
        }
    
        //---------------------------------------
        // PROJ-2212 foreign key on delete set null
        // null update��  row�� �� �����ϴ� grand child table
        // row ���� ���� üũ.
        //
        // ex> delete from t1 where i1=1;
        //   T1 : I1 -- t1 �� i1=1�� row ����
        //        ^
        //        |     
        //   T2 : I1 -- ���� updateRow() ���� i1=1�� column�� null update
        //        ^
        //        |
        //   T3 : I1 -- i1=1 �� row ���� ���� �˻� ( grand child table)
        //        ^
        //        |
        //   T4 : I1
        //     
        //---------------------------------------
            
        //------------------------------------------
        // update�� �ο� �˻��� ����,
        // ���ſ����� ����� ù��° row ���� ��ġ�� cursor ��ġ ����
        //------------------------------------------

        IDE_TEST( sCursor.beforeFirstModified( SMI_FIND_MODIFIED_OLD )
                  != IDE_SUCCESS );

        //------------------------------------------
        // Referencing �˻縦 ���� ������ Row���� �˻�
        //------------------------------------------
            
        sOldRow     = sOrgRow;

        IDE_TEST( sCursor.readOldRow( &sOldRow, &sRid )
                  != IDE_SUCCESS );
        
        while ( sOldRow != NULL )
        {
            IDE_TEST( checkChild4UpdateNullChildRow( aStatement,
                                                     aChildConstraints,
                                                     sSelectedTableInfo,
                                                     aForeignKey,
                                                     sChildTuple,
                                                     sOldRow )
                      != IDE_SUCCESS );
                
            IDE_TEST( sCursor.readOldRow( &sOldRow, &sRid )
                      != IDE_SUCCESS );
        }
        
        sSelectedPartitionRef = sSelectedPartitionRef->next;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NOT_ALLOW_NULL )
    {
        /* BUG-45680 insert ����� not null column�� ���� �����޽��� ������ column ���� ���. */
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMX_NOT_NULL_CONSTRAINT,
                                  "",
                                  "" ) );
    }
    IDE_EXCEPTION_END;

    if ( sIsCursorOpened == ID_TRUE )
    {
        (void) sCursor.close();
    }
    else
    {
        /* Nothing to do */
    }

    if ( sIndexTableCursor != NULL )
    {
        qmsIndexTable::finalizeIndexTableCursors(
            sIndexTableCursor );
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_FAILURE;
}

IDE_RC
qdnForeignKey::updateNullChildRowWithRow4IndexTable( qcStatement           * aStatement,
                                                     qcmRefChildInfo       * aChildConstraints,
                                                     qmsIndexTableRef      * aChildIndexTable,
                                                     qmsTableRef           * aChildTableRef,
                                                     qcmTableInfo          * aChildTableInfo,
                                                     qdConstraintSpec      * aChildCheckConstrList,
                                                     qmsTableRef           * aDefaultTableRef,
                                                     qcmColumn             * aDefaultExprColumns,
                                                     qcmColumn             * aDefaultExprBaseColumns,
                                                     UInt                    aUpdateColCount,
                                                     UInt                  * aUpdateColumnID,
                                                     qcmColumn             * aUpdateColumn,
                                                     qcmRefChildUpdateType   aUpdateRowMovement,
                                                     qcmForeignKey         * aForeignKey,
                                                     UInt                    aKeyRangeColumnCnt,
                                                     qcmIndex              * aUniqueConstraint,
                                                     mtcTuple              * aCheckedTuple,
                                                     const void            * aCheckedRow )
{
/***********************************************************************
 *
 * Description :
 *    updateNullChildRowWithRow �κ��� ȣ��, set null�� child table
 *    ���ڵ� ������Ʈ
 *
 * Implementation :
 *
 ***********************************************************************/

    qcmTableInfo         * sIndexTableInfo;
    smiCursorType          sCursorType;
    smiTableCursor         sCursor;
    smiTableCursor       * sPartCursor = NULL;
    UChar                * sPartCursorStatus = NULL;
    smiCursorProperties    sCursorProperty;
    smiCursorProperties    sPartCursorProperty;
    smiFetchColumnList   * sFetchColumnList = NULL;
    smiFetchColumnList   * sPartFetchColumnList = NULL;
    qcmColumn            * sColumnsForRow = NULL;
    UInt                   sTableType;
    UInt                   sRowSize = 0;
    UInt                   sIndexRowSize;
    UInt                   sIsCursorOpened = ID_FALSE;
    scGRID                 sRid;
    const void           * sRow;
    const void           * sPartRow;
    const void           * sOrgPartRow      = NULL;
    const void           * sOldRow;
    void                 * sTriggerOldRow;
    void                 * sTriggerNewRow;
    idBool                 sNeedTriggerRow;

    mtcTuple             * sChildTuple;
    
    // To Fix PR-10592
    // Index�� ����ϱ� ���� ������
    qcmIndex             * sIndex;
    void                 * sIndexHandle = NULL;
    smiRange             * sKeyRange = NULL;
    qtcMetaRangeColumn     sRangeColumn[QC_MAX_KEY_COLUMN_COUNT];
    smiRange               sRange;

    // PROJ-2212 foreign key on delete set null
    SInt                   sColumnOrder;
    mtcColumn            * sUpdateColumn;
    smiColumnList        * sUpdateColumnList;
    smiValue             * sNullValues;
    smiValue             * sCheckValues;
    smiValue             * sInsertValues = NULL;
    qmxLobInfo           * sInsertLobInfo = NULL;
    void                 * sDefaultExprRow;
    
    qcmTableInfo         * sSelectedTableInfo;
    qmsPartitionRef      * sSelectedPartitionRef;
    qmsPartitionRef      * sPartitionRef;
    
    qcmIndex             * sIndexTableIndex = NULL;
    qcmColumn            * sOIDColumn;
    qcmColumn            * sRIDColumn;
    smOID                  sPartOID;
    scGRID                 sRowGRID;
    qmsPartRefIndexInfo    sPartIndexInfo;
    UInt                   sPartOrder;
    UInt                   i;
    
    // PROJ-1624 non-partitioned index
    qmsIndexTableCursors   sIndexTableCursorInfo;
    qmsIndexTableCursors * sIndexTableCursor = NULL;
    
    UInt                   sIndexUpdateColumnCount;
    smiColumnList        * sIndexUpdateColumnList;
    UInt                   sIndexNullValueCount;
    smiValue             * sIndexNullValues;
    
    sChildTuple = &(QC_PRIVATE_TMPLATE(aStatement)->tmplate.rows[aChildTableRef->table]);
    
    sTableType = aChildTableInfo->tableFlag & SMI_TABLE_TYPE_MASK;

    sIndexTableInfo = aChildIndexTable->tableInfo;

    if ( aUpdateRowMovement == QCM_CHILD_UPDATE_ROWMOVEMENT )
    {
        IDE_DASSERT( aChildTableRef->table != 0 );
        
        sCursorType = SMI_COMPOSITE_CURSOR;
    }
    else
    {
        sCursorType = SMI_UPDATE_CURSOR;
    }
    
    //---------------------------------------
    // partitionRef index ����
    //---------------------------------------

    IDE_TEST( qmsIndexTable::initializePartitionRefIndex(
                  aStatement,
                  aChildTableRef->partitionRef,
                  aChildTableRef->partitionCount,
                  & sPartIndexInfo )
              != IDE_SUCCESS );
    
    //---------------------------------------
    // partition cursor�� ���� ���� �Ҵ�
    //---------------------------------------

    IDE_TEST( aStatement->qmxMem->alloc(
                  ID_SIZEOF(smiTableCursor) * aChildTableRef->partitionCount,
                  (void **) & sPartCursor )
              != IDE_SUCCESS );
    
    IDE_TEST( aStatement->qmxMem->alloc(
                  ID_SIZEOF(UChar) * aChildTableRef->partitionCount,
                  (void **) & sPartCursorStatus )
              != IDE_SUCCESS );

    // �ʱ�ȭ
    for ( i = 0; i < aChildTableRef->partitionCount; i++ )
    {
        sPartCursorStatus[i] = QDN_PART_CURSOR_ALLOCED;
    }

    //---------------------------------------
    // Record Read�� ���� ���� �Ҵ��� ���� �ο� ������ ���
    // trigger������ ���� ���� �Ҵ��� ���� �ο� ������ ���
    //---------------------------------------
    if ( sTableType == SMI_TABLE_DISK )
    {
        // index table�� �����Ҵ�
        IDE_TEST( qdbCommon::getDiskRowSize( sIndexTableInfo,
                                             & sIndexRowSize )
                  != IDE_SUCCESS );
    
        // Record Read�� ���� ������ �Ҵ��Ѵ�.
        // To fix BUG-14820
        // Disk-variable �÷��� rid�񱳸� ���� �ʱ�ȭ �ؾ� ��.
        IDU_LIMITPOINT("qdnForeignKey::getChildRowWithRow::malloc");
        IDE_TEST( aStatement->qmxMem->cralloc( sIndexRowSize,
                                               (void **) & sRow )
                  != IDE_SUCCESS );

        // partition�� �����Ҵ�
        IDE_TEST( qdbCommon::getDiskRowSize( aChildTableInfo,
                                             & sRowSize )
                  != IDE_SUCCESS );

        // Record Read�� ���� ������ �Ҵ��Ѵ�.
        // To fix BUG-14820
        // Disk-variable �÷��� rid�񱳸� ���� �ʱ�ȭ �ؾ� ��.
        IDU_LIMITPOINT("qdnForeignKey::getChildRowWithRow::malloc");
        IDE_TEST( aStatement->qmxMem->cralloc( sRowSize,
                                               (void **) & sPartRow )
                  != IDE_SUCCESS );
        sOrgPartRow = sPartRow;
    }
    else
    {
        /* Nothing To Do */
    }
    
    //------------------------------------------
    // Default Expr�� Row Buffer ����
    //------------------------------------------

    if ( aDefaultExprColumns != NULL )
    {
        IDE_TEST( aStatement->qmxMem->alloc( sRowSize,
                                             (void **) & sDefaultExprRow )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do. */
    }
    
    //------------------------------------------
    // update row trigger�ʿ����� �˻�
    // Trigger Old Row Referencing�� ���� ���� �Ҵ�
    //------------------------------------------

    IDE_TEST( qdnTrigger::needTriggerRow( aStatement,
                                          aChildTableInfo,
                                          QCM_TRIGGER_AFTER,
                                          QCM_TRIGGER_EVENT_UPDATE,
                                          NULL,
                                          & sNeedTriggerRow )
              != IDE_SUCCESS );
    
    if ( sNeedTriggerRow == ID_TRUE )
    {
        IDU_LIMITPOINT("qdnForeignKey::updateChildRowWithRow::malloc2");
        IDE_TEST( aStatement->qmxMem->cralloc( sRowSize,
                                               (void **) & sTriggerOldRow )
                  != IDE_SUCCESS );
    }
    else
    {
        sTriggerOldRow = NULL;
    }
    
    if ( ( sNeedTriggerRow == ID_TRUE ) ||
         ( aChildCheckConstrList != NULL ) )
    {
        IDU_LIMITPOINT("qdnForeignKey::updateChildRowWithRow::malloc3");
        IDE_TEST( aStatement->qmxMem->cralloc( sRowSize,
                                               (void **) & sTriggerNewRow )
                  != IDE_SUCCESS );
    }
    else
    {
        sTriggerNewRow = NULL;
    }
        
    //---------------------------------------
    // PROJ-1705  fetch column list ����
    // delete cascade�� ����,
    // foreignKey �˻縦 ���� ��� child table���� �˻��ؾ� �ϴµ�,
    // �� ���,
    // trigger �Ǵ� �˻��ؾ��ϴ� ��� child column�鿡 ����
    // fetch colum�� �������Ⱑ ���� �ʾ�
    // child table�� ��� �÷��� fetch column ������� ��´�.
    // ����������.
    //---------------------------------------

    IDE_TEST( qdbCommon::makeFetchColumnList(
                  QC_PRIVATE_TMPLATE(aStatement),
                  aChildTableInfo->columnCount,
                  aChildTableInfo->columns,
                  ID_TRUE,
                  & sPartFetchColumnList )
              != IDE_SUCCESS );
    
    // index table�� fetch column list ����
    // key, oid, rid ��� fetch�Ѵ�.
    IDE_TEST( qdbCommon::makeFetchColumnList(
                  QC_PRIVATE_TMPLATE(aStatement),
                  sIndexTableInfo->columnCount,
                  sIndexTableInfo->columns,
                  ID_TRUE,
                  & sFetchColumnList )
              != IDE_SUCCESS );

    //---------------------------------------
    // Index�� Key Range ����
    //---------------------------------------

    IDE_TEST( qmsIndexTable::findKeyIndex( sIndexTableInfo,
                                           & sIndexTableIndex )
              != IDE_SUCCESS );
    
    // index table�� key range ����
    IDE_TEST( makeKeyRangeAndIndex( sTableType,
                                    aKeyRangeColumnCnt,
                                    sIndexTableIndex,
                                    aUniqueConstraint,
                                    aCheckedTuple,
                                    aCheckedRow,
                                    sRangeColumn,
                                    & sRange,
                                    & sIndex,
                                    & sKeyRange )
              != IDE_SUCCESS );
        
    //---------------------------------------
    // PROJ-2212  null value list ����
    //---------------------------------------

    IDE_TEST( aStatement->qmxMem->alloc(
                  ID_SIZEOF(smiColumnList) * aUpdateColCount,
                  (void **) & sUpdateColumnList )
              != IDE_SUCCESS );
    
    IDE_TEST( aStatement->qmxMem->alloc(
                  ID_SIZEOF(smiValue) * aUpdateColCount,
                  (void **) & sNullValues )
              != IDE_SUCCESS );
    
    for ( i = 0; i < aUpdateColCount; i++ )
    {
        sUpdateColumn = aUpdateColumn[i].basicInfo;
        
        IDE_TEST_RAISE( ( sUpdateColumn->flag & MTC_COLUMN_NOTNULL_MASK )
                        == MTC_COLUMN_NOTNULL_TRUE,
                        ERR_NOT_ALLOW_NULL );
        
        sNullValues[i].value  = NULL;
        sNullValues[i].length = 0;
    }
    
    //---------------------------------------
    // row movement�� insert values ����
    //---------------------------------------
    
    IDE_TEST( aStatement->qmxMem->alloc(
                  ID_SIZEOF(smiValue) * aChildTableInfo->columnCount,
                  (void **) & sCheckValues )
              != IDE_SUCCESS );

    if ( aUpdateRowMovement == QCM_CHILD_UPDATE_ROWMOVEMENT )
    {
        // lob info �ʱ�ȭ
        if ( aChildTableInfo->lobColumnCount > 0 )
        {
            // PROJ-1362
            IDE_TEST( qmx::initializeLobInfo(
                          aStatement,
                          & sInsertLobInfo,
                          (UShort)aChildTableInfo->lobColumnCount )
                      != IDE_SUCCESS );
        }
        else
        {
            // Nothing to do.
        }
        
        // insert smiValues �ʱ�ȭ
        IDE_TEST( aStatement->qmxMem->alloc(
                      ID_SIZEOF(smiValue) * aChildTableInfo->columnCount,
                      (void**)& sInsertValues )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    //---------------------------------------
    // index table cursor ����
    //---------------------------------------

    if ( aUpdateRowMovement == QCM_CHILD_UPDATE_ROWMOVEMENT )
    {
        IDE_TEST( aStatement->qmxMem->alloc(
                      ID_SIZEOF(smiColumnList) * (aUpdateColCount + 2),  // +oid,rid
                      (void **) & sIndexUpdateColumnList )
                  != IDE_SUCCESS );
        
        IDE_TEST( aStatement->qmxMem->alloc(
                      ID_SIZEOF(smiValue) * (aUpdateColCount + 2),  // +oid,rid
                      (void **) & sIndexNullValues )
                  != IDE_SUCCESS );
    }
    else
    {
        IDE_TEST( aStatement->qmxMem->alloc(
                      ID_SIZEOF(smiColumnList) * aUpdateColCount,
                      (void **) & sIndexUpdateColumnList )
                  != IDE_SUCCESS );
        
        IDE_TEST( aStatement->qmxMem->alloc(
                      ID_SIZEOF(smiValue) * aUpdateColCount,
                      (void **) & sIndexNullValues )
                  != IDE_SUCCESS );
    }

    IDE_TEST( qmsIndexTable::initializeIndexTableCursors(
                  aStatement,
                  aChildTableRef->indexTableRef,
                  aChildTableRef->indexTableCount,
                  aChildIndexTable,
                  & sIndexTableCursorInfo )
              != IDE_SUCCESS );
    
    sIndexTableCursor = & sIndexTableCursorInfo;
    
    //---------------------------------------
    // column ����
    //---------------------------------------
    
    IDE_DASSERT( sIndexTableInfo->columnCount > 2 );
    
    sOIDColumn = &(sIndexTableInfo->columns[sIndexTableInfo->columnCount - 2]);

    IDE_DASSERT( idlOS::strMatch( sOIDColumn->name,
                                  idlOS::strlen( sOIDColumn->name ),
                                  QD_OID_COLUMN_NAME,
                                  QD_OID_COLUMN_NAME_SIZE ) == 0 );
    
    sRIDColumn = &(sIndexTableInfo->columns[sIndexTableInfo->columnCount - 1]);

    IDE_DASSERT( idlOS::strMatch( sRIDColumn->name,
                                  idlOS::strlen( sRIDColumn->name ),
                                  QD_RID_COLUMN_NAME,
                                  QD_RID_COLUMN_NAME_SIZE ) == 0 );
   
    //---------------------------------------
    // index table scan
    //---------------------------------------

    if ( sIndex != NULL )
    {
        SMI_CURSOR_PROP_INIT_FOR_INDEX_SCAN( &sCursorProperty, aStatement->mStatistics,
                                             sIndex->indexTypeId );
        sIndexHandle = sIndex->indexHandle;
    }
    else
    {
        SMI_CURSOR_PROP_INIT_FOR_FULL_SCAN( &sCursorProperty, aStatement->mStatistics );
    }
    sCursorProperty.mFetchColumnList = sFetchColumnList;
    
    SMI_CURSOR_PROP_INIT_FOR_FULL_SCAN( &sPartCursorProperty, aStatement->mStatistics );
    sPartCursorProperty.mFetchColumnList = sPartFetchColumnList;

    if ( aUpdateRowMovement == QCM_CHILD_UPDATE_ROWMOVEMENT )
    {
        // update�� �÷��� ã�´�.
        IDE_TEST( qmsIndexTable::makeUpdateSmiColumnList( aUpdateColCount,
                                                          aUpdateColumnID,
                                                          aChildIndexTable,
                                                          ID_TRUE ,
                                                          & sIndexUpdateColumnCount,
                                                          sIndexUpdateColumnList )
                  != IDE_SUCCESS );

        // sIndexNullValues�� updateNullChildRow�� �����Ѵ�.
    }
    else
    {
        // update�� �÷��� ã�´�.
        IDE_TEST( qmsIndexTable::makeUpdateSmiColumnList( aUpdateColCount,
                                                          aUpdateColumnID,
                                                          aChildIndexTable,
                                                          ID_FALSE,
                                                          & sIndexUpdateColumnCount,
                                                          sIndexUpdateColumnList )
                  != IDE_SUCCESS );
        
        // update�� ���� �����Ѵ�.
        IDE_TEST( qmsIndexTable::makeUpdateSmiValue( aUpdateColCount,
                                                     aUpdateColumnID,
                                                     sNullValues,
                                                     aChildIndexTable,
                                                     ID_FALSE,
                                                     NULL,
                                                     NULL,
                                                     & sIndexNullValueCount,
                                                     sIndexNullValues )
                  != IDE_SUCCESS );

        IDE_DASSERT( sIndexUpdateColumnCount == sIndexNullValueCount );
    }

    //---------------------------------------
    // init partition cursor
    //---------------------------------------

    if ( aUpdateRowMovement == QCM_CHILD_UPDATE_ROWMOVEMENT )
    {
        // ��� ����д�.
        for ( sPartOrder = 0, sSelectedPartitionRef = aChildTableRef->partitionRef;
              sSelectedPartitionRef != NULL;
              sPartOrder++, sSelectedPartitionRef = sSelectedPartitionRef->next )
        {
            sSelectedTableInfo = sSelectedPartitionRef->partitionInfo;
        
            //------------------------------------------
            // PROJ-2212  update column list ����
            //------------------------------------------
    
            for ( i = 0; i < aUpdateColCount; i++ )
            {
                // set null : update column ������ ����(update ����� �Ǵ� column)
                // ex> foreign key (i2,i3,i4)
                sColumnOrder = aUpdateColumn[i].basicInfo->column.id & SMI_COLUMN_ID_MASK;

                sUpdateColumn = sSelectedTableInfo->columns[sColumnOrder].basicInfo;

                sUpdateColumnList[i].column = (smiColumn*) sUpdateColumn;
            
                if ( i < aUpdateColCount - 1 )
                {
                    sUpdateColumnList[i].next = &sUpdateColumnList[i + 1];
                }
                else
                {
                    sUpdateColumnList[i].next = NULL;
                }
            }
        
            // open cursor
            if ( sPartCursorStatus[sPartOrder] == QDN_PART_CURSOR_ALLOCED )
            {
                sPartCursor[sPartOrder].initialize();

                IDE_TEST( sPartCursor[sPartOrder].open(
                              QC_SMI_STMT( aStatement ),
                              sSelectedTableInfo->tableHandle,
                              NULL,
                              // BUGBUG is this can be correct?
                              smiGetRowSCN(sSelectedTableInfo->tableHandle),
                              sUpdateColumnList,
                              smiGetDefaultKeyRange(),
                              smiGetDefaultKeyRange(),
                              smiGetDefaultFilter(),
                              SMI_LOCK_WRITE | SMI_TRAVERSE_FORWARD | SMI_PREVIOUS_DISABLE,
                              sCursorType,
                              & sPartCursorProperty )
                          != IDE_SUCCESS );

                sPartCursorStatus[sPartOrder] = QDN_PART_CURSOR_OPENED;
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
    
    //---------------------------------------
    // open index table
    //---------------------------------------
    
    // index table scan�� update�� key�� �ݵ�� �����Ѵ�.
    IDE_DASSERT( sIndexUpdateColumnCount > 0 );
    
    sCursor.initialize();

    IDE_TEST( sCursor.open( QC_SMI_STMT( aStatement ),
                            sIndexTableInfo->tableHandle,
                            sIndexHandle,
                            // BUGBUG is this can be correct?
                            smiGetRowSCN(sIndexTableInfo->tableHandle),
                            sIndexUpdateColumnList,
                            sKeyRange,
                            smiGetDefaultKeyRange(),
                            smiGetDefaultFilter(),
                            //QCM_META_CURSOR_FLAG,
                            SMI_LOCK_WRITE | SMI_TRAVERSE_FORWARD | SMI_PREVIOUS_DISABLE,
                            SMI_UPDATE_CURSOR,
                            & sCursorProperty )
              != IDE_SUCCESS );
    sIsCursorOpened = ID_TRUE;
    
    IDE_TEST( sCursor.beforeFirst() != IDE_SUCCESS );
    IDE_TEST( sCursor.readRow( &sRow, &sRid, SMI_FIND_NEXT )
              != IDE_SUCCESS );

    while ( sRow != NULL )
    {
        sPartOID = *(smOID*) ((UChar*)sRow + sOIDColumn->basicInfo->column.offset);
        sRowGRID = *(scGRID*) ((UChar*)sRow + sRIDColumn->basicInfo->column.offset);

        // partOID�� ã�´�.
        IDE_TEST( qmsIndexTable::findPartitionRefIndex( & sPartIndexInfo,
                                                        sPartOID,
                                                        & sSelectedPartitionRef,
                                                        & sPartOrder )
                  != IDE_SUCCESS );

        sSelectedTableInfo = sSelectedPartitionRef->partitionInfo;
        
        sColumnsForRow = sSelectedTableInfo->columns;
        
        //------------------------------------------
        // PROJ-2212  update column list ����
        //------------------------------------------
    
        for ( i = 0; i < aUpdateColCount; i++ )
        {
            // set null : update column ������ ����(update ����� �Ǵ� column)
            // ex> foreign key (i2,i3,i4)
            sColumnOrder = aUpdateColumn[i].basicInfo->column.id & SMI_COLUMN_ID_MASK;

            sUpdateColumn = sSelectedTableInfo->columns[sColumnOrder].basicInfo;

            sUpdateColumnList[i].column = (smiColumn*) sUpdateColumn;
            
            if ( i < aUpdateColCount - 1 )
            {
                sUpdateColumnList[i].next = &sUpdateColumnList[i + 1];
            }
            else
            {
                sUpdateColumnList[i].next = NULL;
            }
        }
        
        // open cursor
        if ( sPartCursorStatus[sPartOrder] == QDN_PART_CURSOR_ALLOCED )
        {
            sPartCursor[sPartOrder].initialize();

            IDE_TEST( sPartCursor[sPartOrder].open(
                          QC_SMI_STMT( aStatement ),
                          sSelectedTableInfo->tableHandle,
                          NULL,
                          // BUGBUG is this can be correct?
                          smiGetRowSCN(sSelectedTableInfo->tableHandle),
                          sUpdateColumnList,
                          smiGetDefaultKeyRange(),
                          smiGetDefaultKeyRange(),
                          smiGetDefaultFilter(),
                          SMI_LOCK_WRITE | SMI_TRAVERSE_FORWARD | SMI_PREVIOUS_DISABLE,
                          sCursorType,
                          & sPartCursorProperty )
                      != IDE_SUCCESS );

            sPartCursorStatus[sPartOrder] = QDN_PART_CURSOR_OPENED;
        }
        else
        {
            // Nothing to do.
        }
        
        sPartRow = sOrgPartRow;        
            
        IDE_TEST( sPartCursor[sPartOrder].readRowFromGRID( & sPartRow, sRowGRID )
                  != IDE_SUCCESS );

        // �ݵ�� �����ؾ� �Ѵ�.
        IDE_TEST_RAISE( sPartRow == NULL, ERR_NO_ROW );

        /* PROJ-1090 Function-based Index */
        if ( aDefaultExprColumns != NULL )
        {
            QC_PRIVATE_TMPLATE(aStatement)->tmplate.
                rows[aChildTableRef->table].row = (void*) sPartRow;
        
            qmsDefaultExpr::setRowBufferFromBaseColumn(
                &(QC_PRIVATE_TMPLATE(aStatement)->tmplate),
                aChildTableRef->table,
                aDefaultTableRef->table,
                aDefaultExprBaseColumns,
                sDefaultExprRow );

            IDE_TEST( qmsDefaultExpr::setRowBufferFromSmiValueArray(
                          &(QC_PRIVATE_TMPLATE(aStatement)->tmplate),
                          aDefaultTableRef,
                          aUpdateColumn,
                          sDefaultExprRow,
                          sNullValues,
                          QCM_TABLE_TYPE_IS_DISK( aChildTableInfo->tableFlag ) )
                      != IDE_SUCCESS );
        
            IDE_TEST( qmsDefaultExpr::calculateDefaultExpression(
                          QC_PRIVATE_TMPLATE(aStatement),
                          aDefaultTableRef,
                          aUpdateColumn,
                          aDefaultExprColumns,
                          sDefaultExprRow,
                          sNullValues,
                          aChildTableInfo->columns )
                      != IDE_SUCCESS );
        
            QC_PRIVATE_TMPLATE(aStatement)->tmplate.
                rows[aChildTableRef->table].row = NULL;
        }
        else
        {
            /* Nothing to do */
        }
        
        IDE_TEST( updateNullChildRow( aStatement,
                                      aChildTableRef,
                                      sSelectedPartitionRef,
                                      sSelectedTableInfo,
                                      aForeignKey,
                                      aChildCheckConstrList,
                                      aKeyRangeColumnCnt,
                                      aUniqueConstraint,
                                      & sPartCursor[sPartOrder],
                                      sPartFetchColumnList,
                                      aUpdateColCount,
                                      aUpdateColumnID,
                                      sUpdateColumnList,
                                      aUpdateColumn,
                                      aUpdateRowMovement,
                                      sNullValues,
                                      sCheckValues,
                                      sInsertValues,
                                      sInsertLobInfo,
                                      aCheckedTuple,
                                      aCheckedRow,
                                      sPartRow,
                                      sTriggerOldRow,
                                      sTriggerNewRow,
                                      sRowSize,
                                      sTriggerOldRow,
                                      sTriggerNewRow,
                                      sRowSize,
                                      sColumnsForRow,
                                      aChildIndexTable,
                                      & sCursor,
                                      sIndexNullValues,
                                      sIndexTableCursor,
                                      & sPartIndexInfo,
                                      sPartCursor,
                                      sRowGRID )
                  != IDE_SUCCESS );

        IDE_TEST( sCursor.readRow( &sRow, &sRid, SMI_FIND_NEXT )
                  != IDE_SUCCESS );
    }

    sIsCursorOpened = ID_FALSE;
    IDE_TEST( sCursor.close() != IDE_SUCCESS );

    for ( i = 0; i < aChildTableRef->partitionCount; i++ )
    {
        if ( sPartCursorStatus[i] == QDN_PART_CURSOR_OPENED )
        {
            sPartCursorStatus[i] = QDN_PART_CURSOR_INITIALIZED;
            
            IDE_TEST( sPartCursor[i].close() != IDE_SUCCESS );
        }
        else
        {
            // Nothing to do.
        }
    }

    IDE_TEST( qmsIndexTable::closeIndexTableCursors(
                  sIndexTableCursor )
              != IDE_SUCCESS );
    
    //---------------------------------------
    // PROJ-2212 foreign key on delete set null
    // null update��  row�� �� �����ϴ� grand child table
    // row ���� ���� üũ.
    //
    // ex> delete from t1 where i1=1;
    //   T1 : I1 -- t1 �� i1=1�� row ����
    //        ^
    //        |     
    //   T2 : I1 -- ���� updateRow() ���� i1=1�� column�� null update
    //        ^
    //        |
    //   T3 : I1 -- i1=1 �� row ���� ���� �˻� ( grand child table)
    //        ^
    //        |
    //   T4 : I1
    //     
    //---------------------------------------

    for ( i = 0, sPartitionRef = aChildTableRef->partitionRef;
          sPartitionRef != NULL;
          i++, sPartitionRef = sPartitionRef->next )
    {
        if ( sPartCursorStatus[i] == QDN_PART_CURSOR_INITIALIZED )
        {
            sSelectedTableInfo = sPartitionRef->partitionInfo;
        
            sColumnsForRow = sSelectedTableInfo->columns;
            
            //------------------------------------------
            // update�� �ο� �˻��� ����,
            // ���ſ����� ����� ù��° row ���� ��ġ�� cursor ��ġ ����
            //------------------------------------------

            IDE_TEST( sPartCursor[i].beforeFirstModified( SMI_FIND_MODIFIED_OLD )
                      != IDE_SUCCESS );

            //------------------------------------------
            // Referencing �˻縦 ���� update�� Row���� �˻�
            //------------------------------------------
            
            sOldRow     = sOrgPartRow;

            IDE_TEST( sPartCursor[i].readOldRow( &sOldRow, &sRid )
                      != IDE_SUCCESS );

            while ( sOldRow != NULL )
            {
                IDE_TEST( checkChild4UpdateNullChildRow( aStatement,
                                                         aChildConstraints,
                                                         sSelectedTableInfo,
                                                         aForeignKey,
                                                         sChildTuple,
                                                         sOldRow )
                          != IDE_SUCCESS );
                
                IDE_TEST( sPartCursor[i].readOldRow( &sOldRow, &sRid )
                          != IDE_SUCCESS );
            }
        }
        else
        {
            // Nothing to do.
        }
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NOT_ALLOW_NULL )
    {
        /* BUG-45680 insert ����� not null column�� ���� �����޽��� ������ column ���� ���. */
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMX_NOT_NULL_CONSTRAINT,
                                  "",
                                  "" ) );
    }
    IDE_EXCEPTION( ERR_NO_ROW )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qdnForeignKey::deleteChildRowWithRow4IndexTable",
                                  "row is not found" ));
    }
    IDE_EXCEPTION_END;

    if ( sIsCursorOpened == ID_TRUE )
    {
        (void) sCursor.close();
    }
    else
    {
        /* Nothing to do */
    }

    if ( sPartCursorStatus != NULL )
    {
        for ( i = 0; i < aChildTableRef->partitionCount; i++ )
        {
            if ( sPartCursorStatus[i] == QDN_PART_CURSOR_OPENED )
            {
                (void) sPartCursor[i].close();
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
    
    if ( sIndexTableCursor != NULL )
    {
        qmsIndexTable::finalizeIndexTableCursors(
            sIndexTableCursor );
    }
    else
    {
        /* Nothing to do */
    }
    
    return IDE_FAILURE;
}

IDE_RC
qdnForeignKey::updateNullChildRow( qcStatement           * aStatement,
                                   qmsTableRef           * aChildTableRef,
                                   qmsPartitionRef       * aChildPartitionRef,
                                   qcmTableInfo          * aChildTableInfo,
                                   qcmForeignKey         * aForeignKey,
                                   qdConstraintSpec      * aChildCheckConstrList,
                                   UInt                    aKeyRangeColumnCnt,
                                   qcmIndex              * aUniqueConstraint,
                                   smiTableCursor        * aChildCursor,
                                   smiFetchColumnList    * aFetchColumnList,
                                   UInt                    aUpdateColumnCount,
                                   UInt                  * aUpdateColumnID,
                                   smiColumnList         * aUpdateColumnList,
                                   qcmColumn             * aUpdateColumns,
                                   qcmRefChildUpdateType   aUpdateRowMovement,
                                   smiValue              * aNullValues,
                                   smiValue              * aCheckValues,
                                   smiValue              * aInsertValues,
                                   struct qmxLobInfo     * aInsertLobInfo,
                                   mtcTuple              * aCheckedTuple,
                                   const void            * aCheckedRow,
                                   const void            * aChildRow,
                                   void                  * aChildDiskTriggerOldRow,
                                   void                  * aChildDiskTriggerNewRow,
                                   UInt                    aChildDiskRowSize,
                                   void                  * aChildMemoryTriggerOldRow,
                                   void                  * aChildMemoryTriggerNewRow,
                                   UInt                    aChildMemoryRowSize,
                                   qcmColumn             * aColumnsForChildRow,
                                   qmsIndexTableRef      * aChildIndexTable,
                                   smiTableCursor        * aSelectedIndexTableCursor,
                                   smiValue              * aIndexNullValues,
                                   qmsIndexTableCursors  * aIndexTableCursor,
                                   qmsPartRefIndexInfo   * aPartIndexInfo,
                                   smiTableCursor        * aPartCursor,
                                   scGRID                  aIndexGRID )
{
/***********************************************************************
 *
 * Description :
 *    deleteChildRowWithRow �κ��� ȣ��, delete cascade�� child table
 *    ���ڵ� ����
 *
 * Implementation :
 *
 ***********************************************************************/

    void                * sChildTriggerOldRow;
    void                * sChildTriggerNewRow;
    UInt                  sChildTriggerOldRowSize;
    UInt                  sChildTriggerNewRowSize;
    smiTableCursor      * sInsertCursor = NULL;
    smiTableCursor        sCursor;
    smiCursorProperties   sCursorProperty;
    idBool                sIsCursorOpened = ID_FALSE;
    qmsPartitionRef     * sSelectedPartitionRef = NULL;
    idBool                sIsPartitionUpdate = ID_TRUE;
    idBool                sExactExist = ID_TRUE;
    SInt                  sColumnOrder;
    SInt                  sChildColumnOrder;
    mtdValueInfo          sValueInfo1;
    mtdValueInfo          sValueInfo2;
    void                * sInsertRow = NULL;
    scGRID                sInsertGRID;
    smOID                 sPartOID;
    UInt                  sPartOrder = 0;
    UInt                  i;

    mtcColumn           * sColumnsForChildRow;
    mtcColumn           * sColumnsForCheckedRow;
    UInt                  sIndexNullValueCount;
    mtcTuple            * sChildTuple;
    void                * sPartitionTupleRow;

    mtcTuple            * sTableTuple             = NULL;
    mtcTuple            * sSelectedPartitionTuple = NULL;
    mtcTuple              sCopyTuple;
    qcmColumn           * sColumnsForNewRow;
    smiValue              sValuesForPartition[QC_MAX_KEY_COLUMN_COUNT];
    smiValue            * sSmiValues;
    idBool                sNeedToRecoverTuple = ID_FALSE;

    /* BUG-39445 */
    if ( aChildPartitionRef != NULL )
    {
        sChildTuple = &( QC_PRIVATE_TMPLATE(aStatement)->tmplate.rows[aChildPartitionRef->table] );
    }
    else
    {
        sChildTuple = &( QC_PRIVATE_TMPLATE(aStatement)->tmplate.rows[aChildTableRef->table] );
    }

    // Key Range�� ���Ե��� ���� Column�� ���Ͽ� ���������� �˻��Ѵ�.
    for ( i = aKeyRangeColumnCnt; i < aUniqueConstraint->keyColCount; i++ )
    {
        sColumnOrder      = aUniqueConstraint->keyColumns[i].column.id & SMI_COLUMN_ID_MASK;
        sChildColumnOrder = aForeignKey->referencingColumn[i] & SMI_COLUMN_ID_MASK;

        sColumnsForCheckedRow = &( aCheckedTuple->columns[sColumnOrder] );
        sColumnsForChildRow = &( sChildTuple->columns[sChildColumnOrder] );

        sValueInfo1.column = sColumnsForCheckedRow;
        sValueInfo1.value  = aCheckedRow;
        sValueInfo1.flag   = MTD_OFFSET_USE;

        sValueInfo2.column = sColumnsForChildRow;
        sValueInfo2.value  = aChildRow;
        sValueInfo2.flag   = MTD_OFFSET_USE;

        if ( sColumnsForCheckedRow->module->
             keyCompare[MTD_COMPARE_MTDVAL_MTDVAL][MTD_COMPARE_ASCENDING]( &sValueInfo1,
                                                                           &sValueInfo2 ) != 0 )
        {
            sExactExist = ID_FALSE;
            break;
        }
        else
        {
            // Nothing to do.
        }
    }

    if ( sExactExist == ID_TRUE )
    {
        /* PROJ-2359 Table/Partition Access Option */
        IDE_TEST( qmx::checkAccessOption( aChildTableRef->tableInfo,
                                          ID_FALSE /* aIsInsertion */ )
                  != IDE_SUCCESS );

        if ( aChildTableInfo->tablePartitionType == QCM_TABLE_PARTITION )
        {
            IDE_TEST( qmx::checkAccessOption( aChildTableInfo,
                                              ID_FALSE /* aIsInsertion */ )
                      != IDE_SUCCESS );
        }
        else
        {
            /* Nothing to do */
        }

        /* PROJ-2464 hybrid partitioned table ���� */
        if ( QCM_TABLE_TYPE_IS_DISK( aChildTableInfo->tableFlag ) == ID_TRUE )
        {
            sChildTriggerOldRow     = aChildDiskTriggerOldRow;
            sChildTriggerOldRowSize = aChildDiskRowSize;
            sChildTriggerNewRow     = aChildDiskTriggerNewRow;
            sChildTriggerNewRowSize = aChildDiskRowSize;
        }
        else
        {
            sChildTriggerOldRow     = aChildMemoryTriggerOldRow;
            sChildTriggerOldRowSize = aChildMemoryRowSize;
            sChildTriggerNewRow     = aChildMemoryTriggerNewRow;
            sChildTriggerNewRowSize = aChildMemoryRowSize;
        }

        if ( sChildTriggerOldRow != NULL )
        {
            // OLD ROW REFERENCING�� ���� ����
            idlOS::memcpy( sChildTriggerOldRow,
                           aChildRow,
                           sChildTriggerOldRowSize );
        }
        else
        {
            // Nothing To Do
        }

        // BUG-35379 on delete set null�� row movement�� ����ؾ���
        if( aChildTableRef->tableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
        {
            switch ( aUpdateRowMovement )
            {
                case QCM_CHILD_UPDATE_CHECK_ROWMOVEMENT:
                {
                    //------------------------------------------
                    // check row movement update�� ���
                    //------------------------------------------

                    // BUG-37188
                    sPartitionTupleRow = sChildTuple->row;
                    sChildTuple->row = (void *)aChildRow;
                    SC_COPY_GRID( aIndexGRID, sChildTuple->rid );

                    IDE_TEST( qmx::makeSmiValueForChkRowMovement(
                                  aUpdateColumnList,
                                  aNullValues,
                                  aChildTableRef->tableInfo->partKeyColumns,
                                  sChildTuple,
                                  aCheckValues )
                              != IDE_SUCCESS );

                    sChildTuple->row = sPartitionTupleRow;

                    IDE_TEST( qmoPartition::partitionFilteringWithRow(
                                  aChildTableRef,
                                  aCheckValues,
                                  & sSelectedPartitionRef )
                              != IDE_SUCCESS );

                    // ������ partition�� �ƴ϶�� ����
                    IDE_TEST_RAISE( sSelectedPartitionRef->partitionHandle !=
                                    aChildTableInfo->tableHandle,
                                    ERR_NO_ROW_MOVEMENT );

                    /* PROJ-2464 hybrid partitioned table ���� */
                    sSelectedPartitionTuple = &(QC_PRIVATE_TMPLATE(aStatement)->tmplate.rows
                                              [sSelectedPartitionRef->table]);

                    sColumnsForNewRow = sSelectedPartitionRef->partitionInfo->columns;

                    break;
                }

                case QCM_CHILD_UPDATE_ROWMOVEMENT:
                {
                    //------------------------------------------
                    // row movement update�� ���
                    //------------------------------------------

                    if ( aInsertLobInfo != NULL )
                    {
                        (void) qmx::initLobInfo( aInsertLobInfo );
                    }
                    else
                    {
                        // Nothing to do.
                    }

                    // BUG-37188
                    sPartitionTupleRow = sChildTuple->row;
                    sChildTuple->row = (void *)aChildRow;
                    SC_COPY_GRID( aIndexGRID, sChildTuple->rid );

                    // row movement�� smi value ���� ����
                    IDE_TEST( qmx::makeSmiValueForRowMovement(
                                  aChildTableRef->tableInfo,
                                  aUpdateColumnList,
                                  aNullValues,
                                  sChildTuple,
                                  aChildCursor,
                                  NULL,
                                  aInsertValues,
                                  aInsertLobInfo )
                              != IDE_SUCCESS );

                    sChildTuple->row = sPartitionTupleRow;

                    IDE_TEST( qmoPartition::partitionFilteringWithRow(
                                  aChildTableRef,
                                  aInsertValues,
                                  & sSelectedPartitionRef )
                              != IDE_SUCCESS );

                    /* PROJ-2359 Table/Partition Access Option */
                    IDE_TEST( qmx::checkAccessOption( sSelectedPartitionRef->partitionInfo,
                                                      ID_TRUE, /* aIsInsertion */
                                                      QCG_CHECK_SHARD_DML_CONSISTENCY( aStatement ) )
                              != IDE_SUCCESS );

                    sPartOID = sSelectedPartitionRef->partitionOID;

                    if ( sSelectedPartitionRef->partitionHandle !=
                         aChildTableInfo->tableHandle )
                    {
                        sIsPartitionUpdate = ID_FALSE;
                    }
                    else
                    {
                        // Nothing to do.
                    }

                    /* PROJ-2464 hybrid partitioned table ���� */
                    if ( QCM_TABLE_TYPE_IS_DISK( sSelectedPartitionRef->partitionInfo->tableFlag ) == ID_TRUE )
                    {
                        sChildTriggerNewRow     = aChildDiskTriggerNewRow;
                        sChildTriggerNewRowSize = aChildDiskRowSize;
                    }
                    else
                    {
                        sChildTriggerNewRow     = aChildMemoryTriggerNewRow;
                        sChildTriggerNewRowSize = aChildMemoryRowSize;
                    }

                    sSelectedPartitionTuple = &(QC_PRIVATE_TMPLATE(aStatement)->tmplate.rows
                                              [sSelectedPartitionRef->table]);

                    sColumnsForNewRow = sSelectedPartitionRef->partitionInfo->columns;

                    break;
                }

                default:
                {
                    //------------------------------------------
                    // �Ϲ� update�� ���
                    //------------------------------------------

                    /* PROJ-2464 hybrid partitioned table ���� */
                    sSelectedPartitionTuple = &(QC_PRIVATE_TMPLATE(aStatement)->tmplate.rows
                                              [aChildPartitionRef->table]);

                    sColumnsForNewRow = aChildPartitionRef->partitionInfo->columns;

                    break;
                }
            }
        }
        else
        {
            //------------------------------------------
            // �Ϲ� update�� ���
            //------------------------------------------

            sColumnsForNewRow = aColumnsForChildRow;
        }

        if ( sIsPartitionUpdate == ID_TRUE )
        {
            //------------------------------------------
            // ���� partition�� update�� ���
            //------------------------------------------

            if ( QCM_TABLE_TYPE_IS_DISK( aChildTableRef->tableInfo->tableFlag ) !=
                 QCM_TABLE_TYPE_IS_DISK( aChildTableInfo->tableFlag ) )
            {
                /* PROJ-2464 hybrid partitioned table ����
                 * Partitioned Table�� �������� ���� smiValue Array�� Table Partition�� �°� ��ȯ�Ѵ�.
                 */
                IDE_TEST( qmx::makeSmiValueWithSmiValue( aChildTableRef->tableInfo,
                                                         aChildTableInfo,
                                                         aUpdateColumns,
                                                         aUpdateColumnCount,
                                                         aNullValues,
                                                         sValuesForPartition )
                          != IDE_SUCCESS );

                sSmiValues = sValuesForPartition;
            }
            else
            {
                sSmiValues = aNullValues;
            }

            // update
            IDE_TEST( aChildCursor->updateRow( sSmiValues,
                                               & sInsertRow,
                                               & sInsertGRID )
                      != IDE_SUCCESS );

            if ( ( sChildTriggerNewRow != NULL ) ||
                 ( aChildCheckConstrList != NULL ) )
            {
                // NEW ROW�� ȹ��
                IDE_TEST( aChildCursor->getLastModifiedRow( & sChildTriggerNewRow,
                                                            sChildTriggerNewRowSize )
                          != IDE_SUCCESS );
            }
            else
            {
                // Nothing To Do
            }
        }
        else
        {
            //------------------------------------------
            // row movement update, insert & delete�� ���
            //------------------------------------------

            if ( aPartIndexInfo != NULL )
            {
                // partOID�� ã�´�.
                IDE_TEST( qmsIndexTable::findPartitionRefIndex( aPartIndexInfo,
                                                                sPartOID,
                                                                & sSelectedPartitionRef,
                                                                & sPartOrder )
                          != IDE_SUCCESS );

                sInsertCursor = & aPartCursor[sPartOrder];
            }
            else
            {
                // initialize
                sCursor.initialize();
                
                SMI_CURSOR_PROP_INIT_FOR_FULL_SCAN( & sCursorProperty, aStatement->mStatistics );
                
                /* PROJ-2464 hybrid partitioned table ����
                 *  smiTableCursor::getLastModifiedRow()�� Disk Row�� ��������, Fetch Column List�� �ʿ��ϴ�.
                 */
                if ( QCM_TABLE_TYPE_IS_DISK( sSelectedPartitionRef->partitionInfo->tableFlag ) == ID_TRUE )
                {
                    sCursorProperty.mFetchColumnList = aFetchColumnList;
                }
                else
                {
                    sCursorProperty.mFetchColumnList = NULL;
                }

                // open insert cursor
                IDE_TEST( sCursor.open( QC_SMI_STMT( aStatement ),
                                        sSelectedPartitionRef->partitionHandle,
                                        NULL,
                                        sSelectedPartitionRef->partitionSCN,
                                        NULL,
                                        smiGetDefaultKeyRange(),
                                        smiGetDefaultKeyRange(),
                                        smiGetDefaultFilter(),
                                        SMI_LOCK_WRITE | SMI_TRAVERSE_FORWARD|
                                        SMI_PREVIOUS_DISABLE,
                                        SMI_INSERT_CURSOR,
                                        & sCursorProperty )
                          != IDE_SUCCESS );
                sIsCursorOpened = ID_TRUE;
                
                sInsertCursor = & sCursor;
            }

            if ( QCM_TABLE_TYPE_IS_DISK( aChildTableRef->tableInfo->tableFlag ) !=
                 QCM_TABLE_TYPE_IS_DISK( sSelectedPartitionRef->partitionInfo->tableFlag ) )
            {
                /* PROJ-2464 hybrid partitioned table ����
                 * Partitioned Table�� �������� ���� smiValue Array�� Table Partition�� �°� ��ȯ�Ѵ�.
                 */
                IDE_TEST( qmx::makeSmiValueWithSmiValue( aChildTableRef->tableInfo,
                                                         sSelectedPartitionRef->partitionInfo,
                                                         aChildTableRef->tableInfo->columns,
                                                         aChildTableRef->tableInfo->columnCount,
                                                         aInsertValues,
                                                         sValuesForPartition )
                          != IDE_SUCCESS );

                sSmiValues = sValuesForPartition;
            }
            else
            {
                sSmiValues = aInsertValues;
            }

            // insert row
            IDE_TEST( sInsertCursor->insertRow( sSmiValues,
                                                & sInsertRow,
                                                & sInsertGRID )
                      != IDE_SUCCESS );

            /* PROJ-2464 hybrid partitioned table ����
             *  Disk Partition�� ���, Disk Type�� Lob Column�� �ʿ��ϴ�.
             *  Memory/Volatile Partition�� ���, �ش� Partition�� Lob Column�� �ʿ��ϴ�.
             */
            if ( ( QCM_TABLE_TYPE_IS_DISK( aChildTableRef->tableInfo->tableFlag ) !=
                   QCM_TABLE_TYPE_IS_DISK( sSelectedPartitionRef->partitionInfo->tableFlag ) ) ||
                 ( QCM_TABLE_TYPE_IS_DISK( sSelectedPartitionRef->partitionInfo->tableFlag ) != ID_TRUE ) )
            {
                IDE_TEST( qmx::changeLobColumnInfo( aInsertLobInfo,
                                                    sSelectedPartitionRef->partitionInfo->columns )
                          != IDE_SUCCESS );
            }
            else
            {
                /* Nothing to do */
            }

            IDE_TEST( qmx::copyAndOutBindLobInfo( aStatement,
                                                  aInsertLobInfo,
                                                  sInsertCursor,
                                                  sInsertRow,
                                                  sInsertGRID )
                      != IDE_SUCCESS );

            if ( ( sChildTriggerNewRow != NULL ) ||
                 ( aChildCheckConstrList != NULL ) )
            {
                // NEW ROW�� ȹ��
                IDE_TEST( sInsertCursor->getLastModifiedRow( & sChildTriggerNewRow,
                                                             sChildTriggerNewRowSize )
                          != IDE_SUCCESS );
            }
            else
            {
                // Nothing To Do
            }

            // close
            if ( sIsCursorOpened == ID_TRUE )
            {
                sIsCursorOpened = ID_FALSE;
                IDE_TEST( sCursor.close() != IDE_SUCCESS );
            }
            else
            {
                // Nothing to do.
            }
        
            // delete row
            IDE_TEST( aChildCursor->deleteRow() != IDE_SUCCESS );
        }

        if ( aUpdateRowMovement == QCM_CHILD_UPDATE_ROWMOVEMENT )
        {
            //------------------------------------------
            // row movement enable�� ���
            // index table update�� key�� �Բ� oid, rid���� update�Ѵ�.
            //------------------------------------------
            
            // update index table
            if ( aIndexTableCursor != NULL )
            {
                if ( aSelectedIndexTableCursor != NULL )
                {
                    // selected index table�� update
                    IDE_TEST( qmsIndexTable::makeUpdateSmiValue(
                                  aUpdateColumnCount,
                                  aUpdateColumnID,
                                  aNullValues,
                                  aChildIndexTable,
                                  ID_TRUE,
                                  & sPartOID,
                                  & sInsertGRID,
                                  & sIndexNullValueCount,
                                  aIndexNullValues )
                              != IDE_SUCCESS );

                    IDE_TEST( aSelectedIndexTableCursor->updateRow(
                                  aIndexNullValues )
                              != IDE_SUCCESS );
                }
                else
                {
                    // Nothing to do.
                }
        
                // �ٸ� index table�� update
                IDE_TEST( qmsIndexTable::updateIndexTableCursors(
                              aStatement,
                              aIndexTableCursor,
                              aUpdateColumnCount,
                              aUpdateColumnID,
                              aNullValues,
                              ID_TRUE,
                              & sPartOID,
                              & sInsertGRID,
                              aIndexGRID )
                          != IDE_SUCCESS );
            }
            else
            {
                // Nothing to do.
            }            
        }
        else
        {
            //------------------------------------------
            // row movement disable�� ���
            // index table update�� key�� update�Ѵ�.
            //------------------------------------------
            
            // index table�� update
            if ( aIndexTableCursor != NULL )
            {
                if ( aSelectedIndexTableCursor != NULL )
                {
                    // selected index table�� update
                    IDE_TEST( aSelectedIndexTableCursor->updateRow(
                                  aIndexNullValues )
                              != IDE_SUCCESS );
                }
                else
                {
                    // Nothing to do.
                }
        
                // �ٸ� index table�� update
                IDE_TEST( qmsIndexTable::updateIndexTableCursors(
                              aStatement,
                              aIndexTableCursor,
                              aUpdateColumnCount,
                              aUpdateColumnID,
                              aNullValues,
                              ID_FALSE,
                              NULL,
                              NULL,
                              aIndexGRID )
                          != IDE_SUCCESS );
            }
            else
            {
                // Nothing to do.
            }
        }
            
        /* PROJ-2464 hybrid partitioned table ���� */
        if ( aChildTableRef->tableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
        {
            if ( aChildTableRef->partitionSummary->isHybridPartitionedTable == ID_TRUE )
            {
                /* BUG-44469 [qx] codesonar warning in QX, MT, ST */
                sTableTuple = &( QC_PRIVATE_TMPLATE(aStatement)->tmplate.rows[aChildTableRef->table] );

                qmx::copyMtcTupleForPartitionDML( &sCopyTuple, sTableTuple );
                sNeedToRecoverTuple = ID_TRUE;

                qmx::adjustMtcTupleForPartitionDML( sTableTuple, sSelectedPartitionTuple );
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

        // BUG-34440
        if ( aChildCheckConstrList != NULL )
        {
            QC_PRIVATE_TMPLATE(aStatement)->tmplate.
                rows[aChildTableRef->table].row = sChildTriggerNewRow;
                    
            IDE_TEST( qdnCheck::verifyCheckConstraintList(
                          QC_PRIVATE_TMPLATE(aStatement),
                          aChildCheckConstrList )
                      != IDE_SUCCESS );
                    
            QC_PRIVATE_TMPLATE(aStatement)->tmplate.
                rows[aChildTableRef->table].row = NULL;
        }
        else
        {
            // Nothing to do.
        }

        // each row update before trigger �������� ����.
        IDE_TEST( qdnTrigger::fireTrigger(
                      aStatement,
                      aStatement->qmxMem,       // valuelist memory
                      aChildTableRef->tableInfo,
                      QCM_TRIGGER_ACTION_EACH_ROW,
                      QCM_TRIGGER_AFTER,
                      QCM_TRIGGER_EVENT_UPDATE,
                      aUpdateColumnList,        // UPDATE Column
                      aChildCursor,             /* Table Cursor */
                      aIndexGRID,               /* Row GRID */
                      sChildTriggerOldRow,      // OLD ROW
                      aColumnsForChildRow,      // OLD ROW column
                      sChildTriggerNewRow,      // NEW ROW
                      sColumnsForNewRow )       // NEW ROW column
                  != IDE_SUCCESS );

        /* PROJ-2464 hybrid partitioned table ���� */
        if ( sNeedToRecoverTuple == ID_TRUE )
        {
            sNeedToRecoverTuple = ID_FALSE;
            qmx::copyMtcTupleForPartitionDML( sTableTuple, &sCopyTuple );
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

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NO_ROW_MOVEMENT )
    {
        IDE_SET( ideSetErrorCode(qpERR_ABORT_QMV_INVALID_PARTITION_KEY_INSERT) );
    }
    IDE_EXCEPTION_END;

    if ( sIsCursorOpened == ID_TRUE )
    {
        (void) sCursor.close();
    }
    else
    {
        /* Nothing to do */
    }

    /* PROJ-2464 hybrid partitioned table ���� */
    if ( sNeedToRecoverTuple == ID_TRUE )
    {
        qmx::copyMtcTupleForPartitionDML( sTableTuple, &sCopyTuple );
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_FAILURE;
}

IDE_RC
qdnForeignKey::checkChild4UpdateNullChildRow( qcStatement      * aStatement,
                                              qcmRefChildInfo  * aChildConstraints,
                                              qcmTableInfo     * aChildTableInfo,
                                              qcmForeignKey    * aForeignKey,
                                              mtcTuple         * aCheckedTuple,
                                              const void       * aOldRow )
{
/***********************************************************************
 *
 * Description :
 *    deleteChildRowWithRow �κ��� ȣ��, delete cascade�� child table
 *    ���ڵ� ����
 *
 * Implementation :
 *
 ***********************************************************************/

    qcmRefChildInfo  * sChildInfo = aChildConstraints;
    qcmIndex         * sChildUniqueConstraint;
    iduMemoryStatus    sQmxMemStatus;
    idBool             sIsSkip;
    idBool             sFound;
    UInt               i;
    UInt               j;
    
    while ( sChildInfo != NULL )
    {
        if ( sChildInfo->parentTableID == aChildTableInfo->tableID )
        {
            //---------------------------------------
            // PROJ-2212 Foreign key set null
            // self reference�� ��� ���� ������ �÷��� ����Ű��
            // foreign key�� ��츸 ���ȣ��ȴ�.
            //---------------------------------------
            sIsSkip = ID_FALSE;
                    
            if ( sChildInfo->isSelfRef == ID_TRUE )
            {
                sChildUniqueConstraint = sChildInfo->parentIndex;

                // sChildUniqueConstraint�� aForeignKey�� ���Ե��� ������ skip
                for ( i = 0; i < sChildUniqueConstraint->keyColCount; i++ )
                {
                    sFound = ID_FALSE;
                    for ( j = 0; j < aForeignKey->constraintColumnCount; j++ )
                    {
                        if ( sChildUniqueConstraint->keyColumns[i].column.id ==
                             aForeignKey->referencingColumn[j] )
                        {
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
                        sIsSkip = ID_TRUE;
                        break;
                    }
                    else
                    {
                        // Nothing to do.
                    }
                }
            }
            else
            {
                // Nothing to do
            }
                    
            // grand child 
            if ( sIsSkip == ID_FALSE )
            {
                IDE_TEST_RAISE( aStatement->qmxMem->getStatus( & sQmxMemStatus ) != IDE_SUCCESS, ERR_MEM_OP );
                        
                IDE_TEST( checkChildRefOnDelete(
                              aStatement,
                              sChildInfo,
                              sChildInfo->parentTableID,
                              aCheckedTuple,
                              aOldRow,
                              ID_FALSE )
                          != IDE_SUCCESS );
                        
                IDE_TEST_RAISE( aStatement->qmxMem->setStatus( &sQmxMemStatus ) != IDE_SUCCESS, ERR_MEM_OP );
            }
            else
            {
                // Nothing To Do
            }
        }
                
        sChildInfo = sChildInfo->next;
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_MEM_OP )
    {
        ideLog::log( IDE_ERR_0,
                     "Unexpected errors may have occurred:"
                     " qdnForeignKey::checkChild4UpdateNullChildRow"
                     " memory error" );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

