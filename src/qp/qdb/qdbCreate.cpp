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
 * $Id: qdbCreate.cpp 90824 2021-05-13 05:35:21Z minku.kang $
 **********************************************************************/

#include <idl.h>
#include <ide.h>
#include <qdbCommon.h>
#include <qdbCreate.h>
#include <qdn.h>
#include <qmv.h>
#include <qtc.h>
#include <qcm.h>
#include <qcg.h>
#include <qcmCache.h>
#include <qcmUser.h>
#include <qcmTableSpace.h>
#include <qmvQuerySet.h>
#include <qmo.h>
#include <qmn.h>
#include <qcmTableInfo.h>
#include <qcuSqlSourceInfo.h>
#include <qdx.h>
#include <qdd.h>
#include <qmx.h>
#include <qdpPrivilege.h>
#include <qcmView.h>
#include <smErrorCode.h>
#include <smiTableSpace.h>
#include <qmcInsertCursor.h>
#include <qcsModule.h>
#include <qcmDictionary.h>
#include <qcuProperty.h>
#include <qdpRole.h>
#include <qmvShardTransform.h> /* TASK-7219 Shard Transformer Refactoring */

IDE_RC qdbCreate::validateCreateTable(qcStatement * aStatement)
{
/***********************************************************************
 *
 * Description :
 *      CREATE TABLE ���� validation ���� �Լ�
 *
 * Implementation :
 *      1. ���̺� �̸��� �ߺ� �˻�
 *      2. ���̺� ���� ���� �˻�
 *      3. validation of TABLESPACE
 *      4. validation of columns specification
 *      5. validation of lob column attributes
 *      6. validation of constraints
 *      7. validation of partitioned table
 *      8. validation of compress column specification
 *
 ***********************************************************************/

#define IDE_FN "qdbCreate::validateCreateTable"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qdTableParseTree     * sParseTree;
    idBool                 sExist = ID_FALSE;
    qsOID                  sProcID;
    qcuSqlSourceInfo       sqlInfo;

#if defined(DEBUG)
    qcmCompressionColumn * sCompColumn = NULL;
    qcmCompressionColumn * sCompPrev = NULL;
    qcmColumn            * sColumn = NULL;
#endif
    
#ifdef ALTI_CFG_EDITION_DISK
    SInt                   sCountMemType  = 0;
    SInt                   sCountVolType  = 0;
#endif

    sParseTree = (qdTableParseTree *)aStatement->myPlan->parseTree;

    /* BUG-30059 */
    if ( qdbCommon::containDollarInName( &(sParseTree->tableName) ) == ID_TRUE )
    {
        sqlInfo.setSourceInfo(
            aStatement,
            &(sParseTree->tableName) );

        IDE_RAISE( CANT_USE_RESERVED_WORD );
    }

    // check table exist.
    if (gQcmSynonyms == NULL)
    {
        // in createdb phase -> no synonym meta.
        // so skip check duplicated name from synonym, psm
        IDE_TEST( qdbCommon::checkDuplicatedObject(
                      aStatement,
                      sParseTree->userName,
                      sParseTree->tableName,
                      &(sParseTree->userID) )
                  != IDE_SUCCESS );
    }
    else
    {
        IDE_TEST(
            qcm::existObject(
                aStatement,
                ID_FALSE,
                sParseTree->userName,
                sParseTree->tableName,
                QS_OBJECT_MAX,
                &(sParseTree->userID),
                &sProcID,
                &sExist)
            != IDE_SUCCESS);

        IDE_TEST_RAISE(sExist == ID_TRUE, ERR_EXIST_OBJECT_NAME);
    }

    // check grant
    IDE_TEST( qdpRole::checkDDLCreateTablePriv( aStatement,
                                                sParseTree->userID )
              != IDE_SUCCESS );
    
    // validation of TABLESPACE
    IDE_TEST( validateTableSpace(aStatement) != IDE_SUCCESS );

#if defined(DEBUG)
    // BUG-38501
    // PROJ-2264 Dictionary table
    if( QCU_FORCE_COMPRESSION_COLUMN == 1 )
    {
        // ������ ��� �����ϴ� ��쿡 ������ compression column ���� �����.
        // 1. Debug ���� ���� �Ǿ��� ��
        // 2. Patition table �� �ƴ� ��
        // 3. Dictionary table �� �ƴ� ��
        // 4. Compress ���� ���� ��
        // 5. Data memory/disk table �� ��
        // 6. Compression column �� �����Ǵ� ������ Ÿ���� ��
        // 7. (BUG-38610) Queue ��ü�� ����� ��찡 �ƴ� ��

        if (smiTableSpace::isDataTableSpaceType( sParseTree->TBSAttr.mType ) == ID_TRUE )
        {
            if ((smiTableSpace::isMemTableSpaceType(sParseTree->TBSAttr.mType) != ID_TRUE) &&
                (smiTableSpace::isDiskTableSpaceType(sParseTree->TBSAttr.mType) != ID_TRUE))
            {
                /* do not compress */
                IDE_RAISE(LABEL_PASS);
            }
            else
            {
                /* nothing to do */
            }
        }
        else
        {
            /* do not compress */
            IDE_RAISE(LABEL_PASS);
        }

        if ( ( sParseTree->partTable == NULL ) &&
             ( ( sParseTree->tableAttrValue & SMI_TABLE_DICTIONARY_MASK )
               == SMI_TABLE_DICTIONARY_FALSE ) &&
             ( sParseTree->compressionColumn == NULL ) &&
             ( ( sParseTree->flag & QDQ_QUEUE_MASK ) != QDQ_QUEUE_TRUE ) )
        {
            for( sColumn = sParseTree->columns; sColumn != NULL; sColumn = sColumn->next )
            {
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
                        // Column �� ����ũ��(size)�� smOID ���� ������ skip
                        // �켱 compression �ϸ� ������ ���� ������ �� ����ϰԵǰ�
                        // in-place update �ÿ� logging ������ �߻��ϰ� �ȴ�.
                        if( sColumn->basicInfo->column.size < ID_SIZEOF(smOID) )
                        {
                            sColumn->basicInfo->column.flag &= ~SMI_COLUMN_COMPRESSION_MASK;
                            sColumn->basicInfo->column.flag |= SMI_COLUMN_COMPRESSION_FALSE;
                        }
                        else
                        {
                            // BUG-38670
                            // Hidden column �� compress ��󿡼� �����Ѵ�.
                            // (Function based index)
                            if ( (sColumn->flag & QCM_COLUMN_HIDDEN_COLUMN_MASK)
                                 == QCM_COLUMN_HIDDEN_COLUMN_TRUE )
                            {
                                sColumn->basicInfo->column.flag &= ~SMI_COLUMN_COMPRESSION_MASK;
                                sColumn->basicInfo->column.flag |= SMI_COLUMN_COMPRESSION_FALSE;
                            }
                            else
                            {
                                // Compression column ���� ��Ÿ������ flag ����
                                sColumn->basicInfo->column.flag &= ~SMI_COLUMN_COMPRESSION_MASK;
                                sColumn->basicInfo->column.flag |= SMI_COLUMN_COMPRESSION_TRUE;

                                IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(qcmCompressionColumn),
                                                                         (void**)&sCompColumn )
                                          != IDE_SUCCESS );
                                QCM_COMPRESSION_COLUMN_INIT( sCompColumn );
                                SET_POSITION(sCompColumn->namePos, sColumn->namePos);

                                if( sCompPrev == NULL )
                                {
                                    sParseTree->compressionColumn = sCompColumn;
                                    sCompPrev                     = sCompColumn;
                                }
                                else
                                {
                                    sCompPrev->next               = sCompColumn;
                                    sCompPrev                     = sCompPrev->next;
                                }
                            }
                        }
                        break;
                    default :
                        sColumn->basicInfo->column.flag &= ~SMI_COLUMN_COMPRESSION_MASK;
                        sColumn->basicInfo->column.flag |= SMI_COLUMN_COMPRESSION_FALSE;
                        break;
                }
            }
        }
    }

    IDE_EXCEPTION_CONT(LABEL_PASS);
#endif

    /* BUG-45641 disk partitioned table�� ���� �÷��� �߰��ϴٰ� �����ϴµ�, memory partitioned table ������ ���ɴϴ�. */
    // PROJ-2264 Dictionary table
    IDE_TEST( qcmDictionary::validateCompressColumn( aStatement,
                                                     sParseTree,
                                                     sParseTree->TBSAttr.mType )
              != IDE_SUCCESS );

    // validation of columns specification
    IDE_TEST( qdbCommon::validateColumnListForCreate(
                  aStatement,
                  sParseTree->columns,
                  ID_TRUE )
              != IDE_SUCCESS );

    // PROJ-1362
    // validation of lob column attributes
    IDE_TEST(qdbCommon::validateLobAttributeList(
                 aStatement,
                 NULL,
                 sParseTree->columns,
                 &(sParseTree->TBSAttr),
                 sParseTree->lobAttr )
             != IDE_SUCCESS);

    // PROJ-1502 PARTITIONED DISK TABLE
    if( sParseTree->partTable != NULL )
    {
        IDE_TEST( qdbCommon::validatePartitionedTable( aStatement,
                                                       ID_TRUE ) /* aIsCreateTable */
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do
    }

    /* PROJ-2639 Altibase Disk Edition */
#ifdef ALTI_CFG_EDITION_DISK
    if ( ( ( sParseTree->flag & QDQ_QUEUE_MASK ) == QDQ_QUEUE_TRUE ) ||
         ( ( sParseTree->flag & QDT_CREATE_TEMPORARY_MASK ) == QDT_CREATE_TEMPORARY_TRUE ) ||
         ( QCG_GET_SESSION_USER_ID(aStatement) == QC_SYSTEM_USER_ID ) )
    {
        /* Nothing to do */
    }
    else
    {
        IDE_TEST_RAISE( smiTableSpace::isDiskTableSpaceType( sParseTree->TBSAttr.mType ) == ID_FALSE,
                        ERR_NOT_SUPPORT_MEMORY_VOLATILE_TABLESPACE_IN_DISK_EDITION );

        if ( sParseTree->partTable != NULL )
        {
            qdbCommon::getTableTypeCountInPartAttrList( NULL,
                                                        sParseTree->partTable->partAttr,
                                                        NULL,
                                                        & sCountMemType,
                                                        & sCountVolType );

            IDE_TEST_RAISE( ( sCountMemType + sCountVolType ) > 0,
                            ERR_NOT_SUPPORT_MEMORY_VOLATILE_TABLESPACE_IN_DISK_EDITION );
        }
        else
        {
            /* Nothing to do */
        }
    }
#endif

    // validation of constraints
    IDE_TEST(qdn::validateConstraints( aStatement,
                                       NULL,
                                       sParseTree->userID,
                                       sParseTree->TBSAttr.mID,
                                       sParseTree->TBSAttr.mType,
                                       sParseTree->constraints,
                                       QDN_ON_CREATE_TABLE,
                                       NULL )
             != IDE_SUCCESS);

    /* PROJ-2464 [��ɼ�] hybrid partaitioned table ����
     *  - qdn::validateConstraints, qdbCommon::validatePartitionedTable ���Ŀ� ȣ��
     */
    IDE_TEST( qdbCommon::validateConstraintRestriction( aStatement,
                                                        sParseTree )
              != IDE_SUCCESS );

    IDE_TEST( calculateTableAttrFlag( aStatement,
                                      sParseTree )
              != IDE_SUCCESS );

    /* PROJ-2464 hybrid partitioned table ����
     *  - ���ó��� : PRJ-1671 Bitmap TableSpace And Segment Space Management
     *               Segment Storage �Ӽ� validation
     */
    IDE_TEST( qdbCommon::validatePhysicalAttr( sParseTree )
              != IDE_SUCCESS );

    // PROJ-1407 Temporary Table
    IDE_TEST( validateTemporaryTable( aStatement,
                                      sParseTree )
              != IDE_SUCCESS );

    if (  QCG_GET_SESSION_IS_NEED_DDL_INFO( aStatement ) == ID_TRUE ) 
    {
        qrc::setDDLSrcInfo( aStatement,
                            ID_TRUE,
                            0,
                            NULL,
                            0,
                            NULL );
    }

    return IDE_SUCCESS;
    
    IDE_EXCEPTION(ERR_EXIST_OBJECT_NAME);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDB_EXIST_OBJECT_NAME));
    }
    IDE_EXCEPTION( CANT_USE_RESERVED_WORD );
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QDB_RESERVED_WORD_IN_OBJECT_NAME,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
#ifdef ALTI_CFG_EDITION_DISK
    IDE_EXCEPTION( ERR_NOT_SUPPORT_MEMORY_VOLATILE_TABLESPACE_IN_DISK_EDITION );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDB_USE_ONLY_DISK_TABLE_PARTITION_IN_DISK_EDITION ) );
    }
#endif
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qdbCreate::parseCreateTableAsSelect(qcStatement * aStatement)
{
/***********************************************************************
 *
 * Description :
 *    CREATE TABLE ... AS SELECT ... �� parse ����
 *
 * Implementation :
 *    1. SELECT �� parse ���� => qmv::parseSelect
 *
 ***********************************************************************/

#define IDE_FN "qdbCreate::parseCreateTableAsSelect"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qdTableParseTree    * sParseTree;

    sParseTree = (qdTableParseTree *)aStatement->myPlan->parseTree;

    /* TASK-7219 Shard Transformer Refactoring */
    IDE_TEST( qmv::parseSelectInternal( sParseTree->select )
              != IDE_SUCCESS );

    IDE_TEST( qmvShardTransform::doTransform( sParseTree->select )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qdbCreate::validateCreateTableAsSelect(qcStatement * aStatement)
{
/***********************************************************************
 *
 * Description :
 *    CREATE TABLE ... AS SELECT ... �� validation ����
 *
 * Implementation :
 *    1. ���̺� �̸��� �̹� �����ϴ��� üũ
 *    2. CREATE TABLE ������ �ִ��� üũ
 *    3. validation of TABLESPACE
 *    4. validation of SELECT statement
 *    5. validation of column and target
 *    6. validation of constraints
 *
 ***********************************************************************/

#define IDE_FN "qdbCreate::validateCreateTableAsSelect"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qdTableParseTree     * sParseTree;
    qsOID                  sProcID;
    idBool                 sExist = ID_FALSE;
    qdPartitionAttribute * sPartAttr;
    UInt                   sPartCount = 0;
    qmsPartCondValList     sOldPartCondVal;
    qcmColumn            * sColumns;
    qcuSqlSourceInfo       sqlInfo;
    qdConstraintSpec     * sConstraint;

#ifdef ALTI_CFG_EDITION_DISK
    SInt                   sCountMemType  = 0;
    SInt                   sCountVolType  = 0;
#endif

    sParseTree = (qdTableParseTree *)aStatement->myPlan->parseTree;
    
    /* BUG-30059 */
    if ( qdbCommon::containDollarInName( &(sParseTree->tableName) ) == ID_TRUE )
    {
        sqlInfo.setSourceInfo(
            aStatement,
            &(sParseTree->tableName) );

        IDE_RAISE( CANT_USE_RESERVED_WORD );
    }
    
    IDE_TEST(
        qcm::existObject(
            aStatement,
            ID_FALSE,
            sParseTree->userName,
            sParseTree->tableName,
            QS_OBJECT_MAX,
            &(sParseTree->userID),
            &sProcID,
            &sExist)
        != IDE_SUCCESS);

    IDE_TEST_RAISE(sExist == ID_TRUE, ERR_EXIST_OBJECT_NAME);

    // check grant
    IDE_TEST( qdpRole::checkDDLCreateTablePriv( aStatement,
                                                sParseTree->userID )
              != IDE_SUCCESS );
        
    // validation of TABLESPACE
    IDE_TEST( validateTableSpace(aStatement) != IDE_SUCCESS );

    // validation of SELECT statement
    ((qmsParseTree*)(sParseTree->select->myPlan->parseTree))->querySet->lflag
        &= ~(QMV_PERFORMANCE_VIEW_CREATION_MASK);
    ((qmsParseTree*)(sParseTree->select->myPlan->parseTree))->querySet->lflag
        |= (QMV_PERFORMANCE_VIEW_CREATION_FALSE);
    ((qmsParseTree*)(sParseTree->select->myPlan->parseTree))->querySet->lflag
        &= ~(QMV_VIEW_CREATION_MASK);
    ((qmsParseTree*)(sParseTree->select->myPlan->parseTree))->querySet->lflag
        |= (QMV_VIEW_CREATION_FALSE);
    IDE_TEST(qmv::validateSelect(sParseTree->select) != IDE_SUCCESS);

    // fix BUG-18752
    aStatement->myPlan->parseTree->currValSeqs =
        sParseTree->select->myPlan->parseTree->currValSeqs;

    // validation of column and target
    IDE_TEST(validateTargetAndMakeColumnList(aStatement)
             != IDE_SUCCESS);

    IDE_TEST(qdbCommon::validateColumnListForCreate(
                 aStatement,
                 sParseTree->columns,
                 ID_TRUE )
             != IDE_SUCCESS);

    // fix BUG-17237
    // column�� not null ���� ����.
    for( sColumns = sParseTree->columns;
         sColumns != NULL;
         sColumns = sColumns->next )
    {
        sColumns->basicInfo->flag &= ~MTC_COLUMN_NOTNULL_MASK;
        sColumns->basicInfo->flag |= MTC_COLUMN_NOTNULL_FALSE;
    }

    // PROJ-1362
    // validation of lob column attributes
    IDE_TEST(qdbCommon::validateLobAttributeList(
                 aStatement,
                 NULL,
                 sParseTree->columns,
                 &(sParseTree->TBSAttr),
                 sParseTree->lobAttr )
             != IDE_SUCCESS);

    // validation of constraints
    IDE_TEST( qdn::validateConstraints( aStatement,
                                        NULL,
                                        sParseTree->userID,
                                        sParseTree->TBSAttr.mID,
                                        sParseTree->TBSAttr.mType,
                                        sParseTree->constraints,
                                        QDN_ON_CREATE_TABLE,
                                        NULL )
              != IDE_SUCCESS);

    /* PROJ-1107 Check Constraint ���� */
    for ( sConstraint = sParseTree->constraints;
          sConstraint != NULL;
          sConstraint = sConstraint->next )
    {
        // BUG-32627
        // Create as select with foreign key constraints not allowed.
        IDE_TEST_RAISE( sConstraint->constrType == QD_FOREIGN,
                        ERR_FOREIGN_KEY_CONSTRAINT );

        if ( sConstraint->constrType == QD_CHECK )
        {
            sqlInfo.setSourceInfo( aStatement,
                                   &(sConstraint->checkCondition->position) );
            IDE_RAISE( ERR_SET_CHECK_CONSTRAINT );
        }
        else
        {
            /* Nothing to do */
        }
    }

    // PROJ-1502 PARTITIONED DISK TABLE
    if( sParseTree->partTable != NULL )
    {
        IDE_TEST( qdbCommon::validatePartitionedTable( aStatement,
                                                       ID_TRUE ) /* aIsCreateTable */
                  != IDE_SUCCESS );

        if ( (sParseTree->partTable->partMethod == QCM_PARTITION_METHOD_RANGE) ||
             (sParseTree->partTable->partMethod == QCM_PARTITION_METHOD_RANGE_USING_HASH) ||
             (sParseTree->partTable->partMethod == QCM_PARTITION_METHOD_LIST) )
        {
            for( sPartAttr = sParseTree->partTable->partAttr, sPartCount = 0;
                 sPartAttr != NULL;
                 sPartAttr = sPartAttr->next, sPartCount++ )
            {
                IDU_LIMITPOINT("qdbCreate::validateCreateTableAsSelect::malloc1");
                IDE_TEST(STRUCT_ALLOC(QC_QMP_MEM(aStatement),
                                      qdAlterPartition,
                                      (void**)& sPartAttr->alterPart)
                         != IDE_SUCCESS);
                QD_SET_INIT_ALTER_PART( sPartAttr->alterPart );

                IDU_LIMITPOINT("qdbCreate::validateCreateTableAsSelect::malloc2");
                IDE_TEST(STRUCT_ALLOC(QC_QMP_MEM(aStatement),
                                      qmsPartCondValList,
                                      (void**)& sPartAttr->alterPart->partCondMinVal)
                         != IDE_SUCCESS);

                IDU_LIMITPOINT("qdbCreate::validateCreateTableAsSelect::malloc3");
                IDE_TEST(STRUCT_ALLOC(QC_QMP_MEM(aStatement),
                                      qmsPartCondValList,
                                      (void**)& sPartAttr->alterPart->partCondMaxVal)
                         != IDE_SUCCESS);

                if( sPartCount == 0 )
                {
                    sPartAttr->alterPart->partCondMinVal->partCondValCount = 0;
                    sPartAttr->alterPart->partCondMinVal->partCondValType =
                        QMS_PARTCONDVAL_NORMAL;
                    sPartAttr->alterPart->partCondMinVal->partCondValues[0] = 0;

                    sPartAttr->alterPart->partCondMaxVal
                        = & sPartAttr->partCondVal;
                }
                else
                {
                    idlOS::memcpy( sPartAttr->alterPart->partCondMinVal,
                                   & sOldPartCondVal,
                                   ID_SIZEOF(qmsPartCondValList) );

                    sPartAttr->alterPart->partCondMaxVal
                        = & sPartAttr->partCondVal;
                }

                sOldPartCondVal = *(sPartAttr->alterPart->partCondMaxVal );
            }
        }
    }

    /* PROJ-2639 Altibase Disk Edition */
#ifdef ALTI_CFG_EDITION_DISK
    if ( ( ( sParseTree->flag & QDQ_QUEUE_MASK ) == QDQ_QUEUE_TRUE ) ||
         ( ( sParseTree->flag & QDT_CREATE_TEMPORARY_MASK ) == QDT_CREATE_TEMPORARY_TRUE ) ||
         ( QCG_GET_SESSION_USER_ID(aStatement) == QC_SYSTEM_USER_ID ) )
    {
        /* Nothing to do */
    }
    else
    {
        IDE_TEST_RAISE( smiTableSpace::isDiskTableSpaceType( sParseTree->TBSAttr.mType ) == ID_FALSE,
                        ERR_NOT_SUPPORT_MEMORY_VOLATILE_TABLESPACE_IN_DISK_EDITION );

        if ( sParseTree->partTable != NULL )
        {
            qdbCommon::getTableTypeCountInPartAttrList( NULL,
                                                        sParseTree->partTable->partAttr,
                                                        NULL,
                                                        & sCountMemType,
                                                        & sCountVolType );

            IDE_TEST_RAISE( ( sCountMemType + sCountVolType ) > 0,
                            ERR_NOT_SUPPORT_MEMORY_VOLATILE_TABLESPACE_IN_DISK_EDITION );
        }
        else
        {
            /* Nothing to do */
        }
    }
#endif

    /* PROJ-2464 hybrid partitioned table ����
     *  - qdn::validateConstraint, qdbCommon::validatePartitionedTable�� ȣ���Ѵ�.
     */
    IDE_TEST( qdbCommon::validateConstraintRestriction( aStatement,
                                                        sParseTree )
              != IDE_SUCCESS );

    // Attribute List�κ��� Table�� Flag ���
    IDE_TEST( calculateTableAttrFlag( aStatement,
                                      sParseTree )
              != IDE_SUCCESS );

    /* PROJ-2464 hybrid partitioned table ����
     *  - ���ó��� : PRJ-1671 Bitmap TableSpace And Segment Space Management
     *               Segment Storage �Ӽ� validation
     */
    IDE_TEST( qdbCommon::validatePhysicalAttr( sParseTree )
              != IDE_SUCCESS );

    // PROJ-1407 Temporary Table
    IDE_TEST( validateTemporaryTable( aStatement,
                                      sParseTree )
              != IDE_SUCCESS );

    return IDE_SUCCESS;
   
    IDE_EXCEPTION(ERR_EXIST_OBJECT_NAME);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDB_EXIST_OBJECT_NAME));
    }
    IDE_EXCEPTION( CANT_USE_RESERVED_WORD );
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QDB_RESERVED_WORD_IN_OBJECT_NAME,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION( ERR_SET_CHECK_CONSTRAINT );
    {
        (void)sqlInfo.init( aStatement->qmeMem );
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDB_NOT_ALLOWED_CHECK_CONSTRAINT,
                                  sqlInfo.getErrMessage() ) );
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION( ERR_FOREIGN_KEY_CONSTRAINT );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDB_NOT_ALLOWED_FOREIGN_KEY_CONSTRAINT) );
    }
#ifdef ALTI_CFG_EDITION_DISK
    IDE_EXCEPTION( ERR_NOT_SUPPORT_MEMORY_VOLATILE_TABLESPACE_IN_DISK_EDITION );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDB_USE_ONLY_DISK_TABLE_PARTITION_IN_DISK_EDITION ) );
    }
#endif
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qdbCreate::validateTableSpace(qcStatement * aStatement)
{
/***********************************************************************
 *
 * Description :
 *    1. TABLESPACE �� ���� validation
 *    2. INSERT HIGH LIMIT validation
 *    3. INSERT LOW LIMIT validation
 *
 * Implementation :
 *    1. TABLESPACE �� ���� validation
 *    if ( TABLESPACENAME ����� ��� )
 *    {
 *      1.1.1 SM ���� �����ϴ� ���̺����̽������� �˻�
 *      1.1.2 �������� ������ ����
 *      1.1.3 ���̺����̽��� ������ UNDO tablespace �Ǵ� temporary
 *            tablespace �̸� ����
 *      1.1.4 USER_ID �� TBS_ID �� SYS_TBS_USERS_ �˻��ؼ� ���ڵ尡 �����ϰ�
 *            QUOTA ���� 0 �̸� ����
 *    }
 *    else
 *    {
 *      1.2.1 USER_ID �� SYS_USERS_ �˻��ؼ� DEFAULT_TBS_ID ���� �о� ���̺���
 *            ���� ���̺����̽��� ����
 *    }
 *    2. PCTFREE validation
 *    if ( PCTFREE ���� ����� ��� )
 *    {
 *      0 <= PCTFREE <= 99
 *    }
 *    else
 *    {
 *      PCTFREE = QD_TABLE_DEFAULT_PCTFREE
 *    }
 *    3. PCTUSED validation
 *    if ( PCTUSED ���� ����� ��� )
 *    {
 *      0 <= PCTUSED <= 99
 *    }
 *    else
 *    {
 *      PCTUSED = QD_TABLE_DEFAULT_PCTUSED
 *    }
 *
 ***********************************************************************/

    qdTableParseTree* sParseTree;
    scSpaceID         sTBSID;

    sParseTree = (qdTableParseTree *)aStatement->myPlan->parseTree;

    if ( QC_IS_NULL_NAME(sParseTree->TBSName) == ID_TRUE )
    {
        // BUG-37377 queue �� ��� �޸� ���̺� �����̽��� �����Ǿ�� �Ѵ�.
        if( (sParseTree->flag & QDQ_QUEUE_MASK) == QDQ_QUEUE_TRUE )
        {
            sTBSID = SMI_ID_TABLESPACE_SYSTEM_MEMORY_DATA;
        }
        else
        {
            IDE_TEST( qcmUser::getDefaultTBS( aStatement,
                                              sParseTree->userID,
                                              &sTBSID) != IDE_SUCCESS );
        }

        IDE_TEST(
            qcmTablespace::getTBSAttrByID( sTBSID,
                                           &(sParseTree->TBSAttr) )
            != IDE_SUCCESS );

    }
    else
    {
        IDE_TEST( qcmTablespace::getTBSAttrByName(
                      aStatement,
                      sParseTree->TBSName.stmtText + sParseTree->TBSName.offset,
                      sParseTree->TBSName.size,
                      &(sParseTree->TBSAttr))
                  != IDE_SUCCESS );

        if( ( sParseTree->flag & QDT_CREATE_TEMPORARY_MASK ) ==
            QDT_CREATE_TEMPORARY_FALSE )
        {
            IDE_TEST_RAISE( smiTableSpace::isDataTableSpaceType(
                                sParseTree->TBSAttr.mType ) == ID_FALSE,
                            ERR_NO_CREATE_IN_SYSTEM_TBS );
        }
        else
        {
            IDE_TEST_RAISE( smiTableSpace::isVolatileTableSpaceType(
                                sParseTree->TBSAttr.mType ) == ID_FALSE,
                            ERR_NO_CREATE_NON_VOLATILE_TBS );
        }

        IDE_TEST( qdpRole::checkAccessTBS(
                      aStatement,
                      sParseTree->userID,
                      sParseTree->TBSAttr.mID) != IDE_SUCCESS );
    }


    //----------------------------------------------
    // [Cursor Flag �� ����]
    // �����ϴ� ���̺� �����̽��� ������ ����, Ager�� ������ �ٸ��� �ȴ�.
    // �̸� ���� Memory, Disk, Memory�� Disk�� �����ϴ� ����
    // ��Ȯ�ϰ� �Ǵ��Ͽ��� �Ѵ�.
    // ���ǹ��� �������� �ٸ� ���̺��� ������ �� �����Ƿ�,
    // �ش� Cursor Flag�� ORing�Ͽ� �� ������ �����ǰ� �Ѵ�.
    //----------------------------------------------
    /* PROJ-2464 hybrid partitioned table ����
     *  - HPT �� ��쿡, Memory, Disk Partition�� ��� ���� �� �ִ�.
     *  - ���� Memory Partition�� ���ԵǾ SegAttr, SegStoAttr �ɼ��� ���� �� �ִ�.
     *  - Validate�� �⺻�� ������ Partition�� TBS Validate ���Ŀ� �����Ѵ�.
     *  - qdbCommon::validatePhysicalAttr
     */
    if ((smiTableSpace::isMemTableSpaceType(sParseTree->TBSAttr.mType) == ID_TRUE) ||
        (smiTableSpace::isVolatileTableSpaceType(sParseTree->TBSAttr.mType) == ID_TRUE))
    {
        // Memory Table Space�� ����ϴ� DDL�� ���
        // Cursor Flag�� ����
        QC_SHARED_TMPLATE(aStatement)->smiStatementFlag |= SMI_STATEMENT_MEMORY_CURSOR;
    }
    else
    {
        // Disk Table Space�� ����ϴ� DDL�� ���
        // Cursor Flag�� ����
        QC_SHARED_TMPLATE(aStatement)->smiStatementFlag |= SMI_STATEMENT_DISK_CURSOR;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_NO_CREATE_IN_SYSTEM_TBS)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDT_NO_CREATE_IN_SYSTEM_TBS));
    }
    IDE_EXCEPTION(ERR_NO_CREATE_NON_VOLATILE_TBS)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDB_CANNOT_CREATE_TEMPORARY_TABLE_IN_NONVOLATILE_TBS));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdbCreate::validateTargetAndMakeColumnList(qcStatement * aStatement)
{
/***********************************************************************
 *
 * Description :
 *      CREATE TABLE ... AS SELECT ���� validation ���� �Լ��κ��� ȣ��
 *      : validation of target and make column
 *
 * Implementation :
 *      1. if... create ������ �÷��� ��� X
 *         : select �÷��� �ٸ�� �� �ߺ� �˻�
 *      2. else... create ������ �÷��� ��� O
 *         : create ������ �÷������� select �÷��� ���� ��ġ �˻�
 *
 *      fix BUG-24917
 *         : create as select �Ǵ� view�����ÿ��� �� �Լ��� ȣ���Ͽ�
 *           target���κ��� �÷��� �����ϰ�
 *           �÷��� ���� validation�� qdbCommon::validateColumnList(..)�� ȣ��
 *
 ***********************************************************************/

    qdTableParseTree    * sParseTree;
    qcmColumn           * sPrevColumn = NULL;
    qcmColumn           * sCurrColumn = NULL;
    qmsTarget           * sTarget;
    SInt                  sColumnCount = 0;
    SChar               * sColumnName;
    qcuSqlSourceInfo      sqlInfo;
    SInt                  i = 0;

    sParseTree = (qdTableParseTree *)aStatement->myPlan->parseTree;

    if (sParseTree->columns == NULL)
    {
        for (sTarget =
                 ((qmsParseTree *)(sParseTree->select->myPlan->parseTree))->querySet->target;
             sTarget != NULL;
             sTarget = sTarget->next)
        {
            IDE_TEST_RAISE(sTarget->aliasColumnName.name == NULL,
                           ERR_NOT_EXIST_ALIAS_OF_TARGET);

            // make current qcmColumn
            IDU_LIMITPOINT("qdbCreate::validateTargetAndMakeColumnList::malloc");
            IDE_TEST(STRUCT_ALLOC(QC_QMP_MEM(aStatement),
                                  qcmColumn,
                                  (void**)&sCurrColumn)
                     != IDE_SUCCESS);

            sCurrColumn->namePos.stmtText = NULL;
            sCurrColumn->namePos.offset   = (SInt)0;
            sCurrColumn->namePos.size     = (SInt)0;

            //fix BUG-17635
            sCurrColumn->flag = 0;

            // fix BUG-21021
            sCurrColumn->flag &= ~QCM_COLUMN_TYPE_MASK;
            sCurrColumn->flag |= QCM_COLUMN_TYPE_DEFAULT;
            sCurrColumn->inRowLength = ID_UINT_MAX;
            sCurrColumn->defaultValue = NULL;

            if (sTarget->aliasColumnName.size > QC_MAX_OBJECT_NAME_LEN)
            {
                if (sTarget->targetColumn != NULL)
                {
                    sqlInfo.setSourceInfo(
                        aStatement,
                        & sTarget->targetColumn->position);
                }
                else
                {
                    sqlInfo.setSourceInfo(
                        aStatement,
                        & sCurrColumn->namePos);
                }
                IDE_RAISE(ERR_MAX_COLUMN_NAME_LENGTH);
            }
            else
            {
                /* BUG-33681 */
                for ( i = 0; i < sTarget->aliasColumnName.size; i ++ )
                {
                    IDE_TEST_RAISE(*(sTarget->aliasColumnName.name + i) == '\'',
                                   ERR_NOT_EXIST_ALIAS_OF_TARGET);
                }
            }

            idlOS::strncpy(sCurrColumn->name,
                           sTarget->aliasColumnName.name,
                           sTarget->aliasColumnName.size);
            sCurrColumn->name[sTarget->aliasColumnName.size] = '\0';

            IDE_TEST(qdbCommon::getMtcColumnFromTarget(
                         aStatement,
                         sTarget->targetColumn,
                         sCurrColumn,
                         sParseTree->TBSAttr.mID) != IDE_SUCCESS);

            sCurrColumn->next = NULL;

            // link column list
            if (sParseTree->columns == NULL)
            {
                sParseTree->columns = sCurrColumn;
                sPrevColumn         = sCurrColumn;
            }
            else
            {
                sPrevColumn->next   = sCurrColumn;
                sPrevColumn         = sCurrColumn;
            }

            sColumnCount++;
        }

        for (sCurrColumn = sParseTree->columns; sCurrColumn != NULL;
             sCurrColumn = sCurrColumn->next)
        {
            for (sPrevColumn = sCurrColumn->next; sPrevColumn != NULL;
                 sPrevColumn = sPrevColumn->next)
            {
                if ( idlOS::strMatch( sCurrColumn->name,
                                      idlOS::strlen( sCurrColumn->name ),
                                      sPrevColumn->name,
                                      idlOS::strlen( sPrevColumn->name ) ) == 0 )
                {
                    sColumnName = sCurrColumn->name;
                    IDE_RAISE( ERR_DUP_ALIAS_NAME );
                }
                else
                {
                    // Nothing to do.
                }
            }
        }
    }
    else
    {
        for (sCurrColumn = sParseTree->columns,
                 sTarget = ((qmsParseTree *)(sParseTree->select->myPlan->parseTree))
                 ->querySet->target;
             sCurrColumn != NULL;
             sCurrColumn = sCurrColumn->next,
                 sTarget = sTarget->next)
        {
            IDE_TEST_RAISE(sTarget == NULL, ERR_MISMATCH_NUMBER_OF_COLUMN);

            IDE_TEST(qdbCommon::getMtcColumnFromTarget(
                         aStatement,
                         sTarget->targetColumn,
                         sCurrColumn,
                         sParseTree->TBSAttr.mID) != IDE_SUCCESS);

            sColumnCount++;
        }

        IDE_TEST_RAISE(sTarget != NULL, ERR_MISMATCH_NUMBER_OF_COLUMN);
    }

    IDE_TEST_RAISE(sColumnCount > QC_MAX_COLUMN_COUNT,
                   ERR_INVALID_COLUMN_COUNT);

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_DUP_ALIAS_NAME);
    {
        char sBuffer[256];
        idlOS::snprintf( sBuffer, ID_SIZEOF(sBuffer),
                         "\"%s\"", sColumnName );
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDB_DUPLICATE_COLUMN,
                                sBuffer));
    }
    IDE_EXCEPTION(ERR_NOT_EXIST_ALIAS_OF_TARGET);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDB_NOT_EXISTS_ALIAS));
    }
    IDE_EXCEPTION(ERR_MISMATCH_NUMBER_OF_COLUMN);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDB_MISMATCH_COL_COUNT));
    }
    IDE_EXCEPTION(ERR_INVALID_COLUMN_COUNT);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDB_INVALID_COLUMN_COUNT));
    }
    IDE_EXCEPTION(ERR_MAX_COLUMN_NAME_LENGTH);
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QCP_MAX_NAME_LENGTH_OVERFLOW,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}


IDE_RC qdbCreate::optimize(qcStatement * aStatement)
{
/***********************************************************************
 *
 * Description :
 *      CREATE TABLE ... AS SELECT ���� optimization ���� �Լ�
 *
 * Implementation :
 *      1. select ������ ���� optimization ����
 *      2. make internal PROJ
 *      3. set execution plan tree
 *
 ***********************************************************************/

#define IDE_FN "qdbCreate::optimze"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qdTableParseTree    * sParseTree;

    sParseTree = (qdTableParseTree *)aStatement->myPlan->parseTree;

    if (sParseTree->select != NULL)
    {
        if ( (sParseTree->flag & QDV_OPT_STATUS_MASK) == QDV_OPT_STATUS_VALID )
        {
            IDE_TEST(qmo::optimizeCreateSelect( sParseTree->select )
                     != IDE_SUCCESS);
        }
        else
        {
            sParseTree->select->myPlan->plan = NULL;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}


IDE_RC qdbCreate::executeCreateTable(qcStatement * aStatement)
{
/***********************************************************************
 *
 * Description :
 *      CREATE TABLE ���� execution ���� �Լ�
 *
 * Implementation :
 *      1. validateDefaultValueType
 *      2. ���ο� ���̺��� ���� TableID ���ϱ�
 *      3. createTableOnSM
 *      4. SYS_TABLES_ ��Ÿ ���̺� ���̺� ���� �Է�
 *      5. SYS_COLUMNS_ ��Ÿ ���̺� ���̺� �÷� ���� �Է�
 *      6. primary key, not null, check constraint ����
 *      7. ��Ÿ ĳ�� ����
 *
 ***********************************************************************/

#define IDE_FN "qdbCreate::executeCreateTable"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qdTableParseTree     * sParseTree;
    UInt                   sTableID;
    smOID                  sTableOID;
    UInt                   sColumnCount;
    UInt                   sPartKeyCount = 0;
    qcmColumn            * sColumn;
    qcmColumn            * sPartKeyColumns;
    qdConstraintSpec     * sConstraint;
    qcmTableInfo         * sTempTableInfo = NULL;
    smSCN                  sSCN;
    UInt                   i;
    void                 * sTableHandle;
    idBool                 sIsQueue;
    idBool                 sIsPartitioned;
    qcmPartitionInfoList * sOldPartInfoList  = NULL;
    qcmPartitionInfoList * sTempPartInfoList = NULL;
    qcmTemporaryType       sTemporaryType;
    qdIndexTableList     * sIndexTable;
    
    SChar                  sTableOwnerName[QC_MAX_OBJECT_NAME_LEN + 1] = { 0, };
    SChar                  sTableName[QC_MAX_OBJECT_NAME_LEN + 1] = { 0, };
    SChar                  sColumnName[QC_MAX_OBJECT_NAME_LEN + 1] = { 0, };

    // PROJ-2264 Dictionary table
    qcmCompressionColumn * sCompColumn;
    qcmDictionaryTable   * sDicTable;
    qcmDictionaryTable   * sDictionaryTable;

    qcmIndex             * sTempIndex = NULL; 
    UInt                   sShardFlag = 0;

    //---------------------------------
    // �⺻ ���� �ʱ�ȭ
    //---------------------------------
    sParseTree = (qdTableParseTree *)aStatement->myPlan->parseTree;

    sColumnCount = 0;
    sColumn = sParseTree->columns;
    sDictionaryTable = NULL;

    // PROJ-1502 PARTITIONED DISK TABLE
    // ����Ƽ�ǵ� ���̺� ����
    
    if( sParseTree->partTable == NULL )
    {
        sIsPartitioned = ID_FALSE;
    }
    // ��Ƽ�ǵ� ���̺� ����
    else
    {
        sIsPartitioned = ID_TRUE;

        sPartKeyCount = 0;

        for( sPartKeyColumns = sParseTree->partTable->partKeyColumns;
             sPartKeyColumns != NULL;
             sPartKeyColumns = sPartKeyColumns->next )
        {
            sPartKeyCount++;
        }
    }

    // PROJ-1407 Temporary Table
    if ( sParseTree->temporaryOption == NULL )
    {
        if ( ( sParseTree->flag & QDT_CREATE_TEMPORARY_MASK ) ==
             QDT_CREATE_TEMPORARY_TRUE )
        {
            sTemporaryType = QCM_TEMPORARY_ON_COMMIT_DELETE_ROWS;
        }
        else
        {
            sTemporaryType = QCM_TEMPORARY_ON_COMMIT_NONE;
        }
    }
    else
    {
        sTemporaryType = sParseTree->temporaryOption->temporaryType;
    }

    while(sColumn != NULL)
    {
        sColumnCount++;
        if (sColumn->defaultValue != NULL)
        {
            IDE_TEST(qdbCommon::convertDefaultValueType(
                         aStatement,
                         &sColumn->basicInfo->type,
                         sColumn->defaultValue,
                         NULL)
                     != IDE_SUCCESS);
        }
        sColumn = sColumn->next;
    }

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

    if ( QCU_FORCE_PARALLEL_DEGREE > 1 )
    {
        // 1. Dictionary table �� �ƴ� ��
        // 2. Parallel degree �� 1 �� ��
        // ������ parallel degree �� property ũ��� �����Ѵ�.
        if ( ( (sParseTree->tableAttrValue & SMI_TABLE_DICTIONARY_MASK)
               == SMI_TABLE_DICTIONARY_FALSE ) &&
             ( sParseTree->parallelDegree == 1 ) )
        {
            sParseTree->parallelDegree = QCU_FORCE_PARALLEL_DEGREE;
        }
    }

    // PROJ-2264 Dictionary Table
    for ( sCompColumn = sParseTree->compressionColumn;
          sCompColumn != NULL;
          sCompColumn = sCompColumn->next )
    {
        IDE_TEST( qcmDictionary::makeDictionaryTable( aStatement,
                                                      sParseTree->TBSAttr.mID,
                                                      sParseTree->columns,
                                                      sCompColumn,
                                                      &sDictionaryTable )
                  != IDE_SUCCESS );
    }

    IDE_TEST(qcm::getNextTableID(aStatement, &sTableID) != IDE_SUCCESS);

    IDE_TEST(qdbCommon::createTableOnSM(aStatement,
                                        sParseTree->columns,
                                        sParseTree->userID,
                                        sTableID,
                                        sParseTree->maxrows,
                                        sParseTree->TBSAttr.mID,
                                        sParseTree->segAttr,
                                        sParseTree->segStoAttr,
                                        sParseTree->tableAttrMask,
                                        sParseTree->tableAttrValue,
                                        sParseTree->parallelDegree,
                                        &sTableOID)
             != IDE_SUCCESS);

    /* TASK-7307 DML Data Consistency in Shard */
    if ( sParseTree->mShardFlag == QCM_SHARD_FLAG_TABLE_META )
    {
        sShardFlag &= ~QCM_TABLE_SHARD_FLAG_TABLE_MASK;
        sShardFlag |= QCM_TABLE_SHARD_FLAG_TABLE_META;
    }
    else if ( sParseTree->mShardFlag == QCM_SHARD_FLAG_TABLE_BACKUP )
    {
        sShardFlag &= ~QCM_TABLE_SHARD_FLAG_TABLE_MASK;
        sShardFlag |= QCM_TABLE_SHARD_FLAG_TABLE_BACKUP;
    }
    else
    {
        /* Nothing to do */
    }

    IDE_TEST(qdbCommon::insertTableSpecIntoMeta(aStatement,
                                                sIsPartitioned,
                                                sParseTree->flag,
                                                sParseTree->tableName,
                                                sParseTree->userID,
                                                sTableOID,
                                                sTableID,
                                                sColumnCount,
                                                sParseTree->maxrows,
                                                sParseTree->accessOption, /* PROJ-2359 Table/Partition Access Option */
                                                sParseTree->TBSAttr.mID,
                                                sParseTree->segAttr,
                                                sParseTree->segStoAttr,
                                                sTemporaryType,
                                                sParseTree->parallelDegree, // PROJ_1071
                                                sShardFlag)     // TASK-7307
             != IDE_SUCCESS);

    if ((sParseTree->flag & QDQ_QUEUE_MASK) == QDQ_QUEUE_TRUE)
    {
        sIsQueue = ID_TRUE;
    }
    else
    {
        sIsQueue = ID_FALSE;
    }

    IDE_TEST(qdbCommon::insertColumnSpecIntoMeta(aStatement,
                                                 sParseTree->userID,
                                                 sTableID,
                                                 sParseTree->columns,
                                                 sIsQueue)
             != IDE_SUCCESS);

    // PROJ-1502 PARTITIONED DISK TABLE
    if ( sIsPartitioned == ID_TRUE )
    {
        IDE_TEST(qdbCommon::insertPartTableSpecIntoMeta(aStatement,
                                                        sParseTree->userID,
                                                        sTableID,
                                                        sParseTree->partTable->partMethod,
                                                        sPartKeyCount,
                                                        sParseTree->isRowmovement )
                 != IDE_SUCCESS);

        IDE_TEST(qdbCommon::insertPartKeyColumnSpecIntoMeta(aStatement,
                                                            sParseTree->userID,
                                                            sTableID,
                                                            sParseTree->columns,
                                                            sParseTree->partTable->partKeyColumns,
                                                            QCM_TABLE_OBJECT_TYPE)
                 != IDE_SUCCESS);

        IDE_TEST(qcm::makeAndSetQcmTableInfo(QC_SMI_STMT( aStatement ),
                                             sTableID,
                                             sTableOID) != IDE_SUCCESS);

        IDE_TEST(qcm::getTableInfoByID(aStatement,
                                       sTableID,
                                       &sTempTableInfo,
                                       &sSCN,
                                       &sTableHandle)
                 != IDE_SUCCESS);

        IDE_TEST( executeCreateTablePartition( aStatement,
                                               sTableID,
                                               sTempTableInfo )
                  != IDE_SUCCESS );

        IDE_TEST( qcmPartition::getPartitionInfoList( aStatement,
                                                      QC_SMI_STMT( aStatement ),
                                                      aStatement->qmxMem,
                                                      sTableID,
                                                      & sTempPartInfoList )
                  != IDE_SUCCESS );

        // ��� ��Ƽ�ǿ� LOCK(X)
        IDE_TEST( qcmPartition::validateAndLockPartitionInfoList( aStatement,
                                                                  sTempPartInfoList,
                                                                  SMI_TBSLV_DDL_DML, // TBS Validation �ɼ�
                                                                  SMI_TABLE_LOCK_X,
                                                                  smiGetDDLLockTimeOut((QC_SMI_STMT(aStatement))->getTrans()))
                  != IDE_SUCCESS );

        sParseTree->partTable->partInfoList = sTempPartInfoList;
    }

    for (sConstraint = sParseTree->constraints;
         sConstraint != NULL;
         sConstraint = sConstraint->next)
    {
        fillColumnID(sParseTree->columns,
                     sConstraint);

        IDE_TEST(qdbCommon::createConstrPrimaryUnique(aStatement,
                                                      sTableOID,
                                                      sConstraint,
                                                      sParseTree->userID,
                                                      sTableID,
                                                      sTempPartInfoList,
                                                      sParseTree->maxrows)
                 != IDE_SUCCESS);

        IDE_TEST(qdbCommon::createConstrNotNull(aStatement,
                                                sConstraint,
                                                sParseTree->userID,
                                                sTableID)
                 != IDE_SUCCESS);

        /* PROJ-1107 Check Constraint ���� */
        IDE_TEST( qdbCommon::createConstrCheck( aStatement,
                                                sConstraint,
                                                sParseTree->userID,
                                                sTableID,
                                                sParseTree->relatedFunctionNames )
                  != IDE_SUCCESS );

        IDE_TEST(qdbCommon::createConstrTimeStamp(aStatement,
                                                  sConstraint,
                                                  sParseTree->userID,
                                                  sTableID)
                 != IDE_SUCCESS);
    }

    if( sTempTableInfo != NULL )
    {
        (void)qcm::destroyQcmTableInfo(sTempTableInfo);
    }
    sTempTableInfo = NULL;

    IDE_TEST(qcm::makeAndSetQcmTableInfo(QC_SMI_STMT( aStatement ),
                                         sTableID,
                                         sTableOID) != IDE_SUCCESS);

    IDE_TEST(qcm::getTableInfoByID(aStatement,
                                   sTableID,
                                   &sTempTableInfo,
                                   &sSCN,
                                   &sTableHandle)
             != IDE_SUCCESS);

    IDE_TEST(qcm::validateAndLockTable(aStatement,
                                       sTableHandle,
                                       sSCN,
                                       SMI_TABLE_LOCK_X)
             != IDE_SUCCESS);

    for (sConstraint = sParseTree->constraints;
         sConstraint != NULL;
         sConstraint = sConstraint->next)
    {
        if (sConstraint->constrType == QD_FOREIGN)
        {
            (void)qcm::destroyQcmTableInfo(sTempTableInfo);
            sTempTableInfo = NULL;

            IDE_TEST(qcm::makeAndSetQcmTableInfo(QC_SMI_STMT(aStatement),
                                                 sTableID,
                                                 sTableOID) != IDE_SUCCESS);

            IDE_TEST(qcm::getTableInfoByID(aStatement,
                                           sTableID,
                                           &sTempTableInfo,
                                           &sSCN,
                                           &sTableHandle)
                     != IDE_SUCCESS);

            IDE_TEST(qdbCommon::createConstrForeign(aStatement,
                                                    sConstraint,
                                                    sParseTree->userID,
                                                    sTableID) != IDE_SUCCESS);

            (void)qcm::destroyQcmTableInfo(sTempTableInfo);
            sTempTableInfo = NULL;

            IDE_TEST(qcm::makeAndSetQcmTableInfo(QC_SMI_STMT( aStatement ),
                                                 sTableID,
                                                 sTableOID) != IDE_SUCCESS);

            IDE_TEST(qcm::getTableInfoByID(aStatement,
                                           sTableID,
                                           &sTempTableInfo,
                                           &sSCN,
                                           &sTableHandle)
                     != IDE_SUCCESS);
        }
    }

    if (sParseTree->constraints != NULL)
    {
        if (sTempTableInfo->primaryKey != NULL)
        {
            for ( i = 0; i < sTempTableInfo->primaryKey->keyColCount; i++)
            {
                IDE_TEST(qdbCommon::makeColumnNotNull(
                        aStatement,
                        sTempTableInfo->tableHandle,
                        sParseTree->maxrows,
                        sTempPartInfoList,
                        sIsPartitioned,
                        sTempTableInfo->primaryKey->keyColumns[i].column.id)
                    != IDE_SUCCESS);
            }
        }

        (void)qcm::destroyQcmTableInfo(sTempTableInfo);
        sTempTableInfo = NULL;

        IDE_TEST(qcm::makeAndSetQcmTableInfo(QC_SMI_STMT( aStatement ),
                                             sTableID,
                                             sTableOID) != IDE_SUCCESS);

        IDE_TEST(qcm::getTableInfoByID(aStatement,
                                       sTableID,
                                       &sTempTableInfo,
                                       &sSCN,
                                       &sTableHandle)
                 != IDE_SUCCESS);
    }

    // BUG-11266
    IDE_TEST(qcmView::recompileAndSetValidViewOfRelated(
            aStatement,
            sParseTree->userID,
            (SChar *) (sParseTree->tableName.stmtText +
                       sParseTree->tableName.offset),
            sParseTree->tableName.size,
            QS_TABLE)
        != IDE_SUCCESS);

    /* PROJ-1723 [MDW/INTEGRATOR] Altibase Plugin ����
       DDL Statement Text�� �α�
     */
    if (QCU_DDL_SUPPLEMENTAL_LOG == 1)
    {
        IDE_TEST( qciMisc::writeDDLStmtTextLog( aStatement,
                                                sParseTree->userID,
                                                sTempTableInfo->name )
                  != IDE_SUCCESS );

        IDE_TEST(qci::mManageReplicationCallback.mWriteTableMetaLog(
                aStatement,
                0,
                sTableOID)
            != IDE_SUCCESS);
    }


    // PROJ-2264 Dictionary Table
    for( sDicTable = sDictionaryTable;
         sDicTable != NULL;
         sDicTable = sDicTable->next )
    {
        IDE_TEST( qdbCommon::insertCompressionTableSpecIntoMeta(
                aStatement,
                sTableID,                                    // data table id
                sDicTable->dataColumn->basicInfo->column.id, // data table's column id
                sDicTable->dictionaryTableInfo->tableID,     // dictionary table id
                sDicTable->dictionaryTableInfo->maxrows )    // dictionary table's max rows
            != IDE_SUCCESS);
    }

    (void)qcm::destroyQcmTableInfo(sTempTableInfo);
    sTempTableInfo = NULL;

    IDE_TEST(qcm::makeAndSetQcmTableInfo(QC_SMI_STMT( aStatement ),
                                         sTableID,
                                         sTableOID) != IDE_SUCCESS);

    IDE_TEST( qcm::getTableInfoByID( aStatement,
                                     sTableID,
                                     & sTempTableInfo,
                                     & sSCN,
                                     & sTableHandle )
              != IDE_SUCCESS );

    if ( sIsPartitioned == ID_TRUE )
    {
        sOldPartInfoList = sTempPartInfoList;
        IDE_TEST( qcmPartition::makeAndSetAndGetQcmPartitionInfoList( aStatement,
                                                                      sTempTableInfo,
                                                                      sOldPartInfoList,
                                                                      & sTempPartInfoList )
                  != IDE_SUCCESS );

        (void)qcmPartition::destroyQcmPartitionInfoList( sOldPartInfoList );
    }
    else
    {
        /* Nothing to do */
    }

    // PROJ-2002 Column Security
    // ���� �÷� ������ ���� ��⿡ �˸���.
    sColumn = sParseTree->columns;

    while(sColumn != NULL)
    {
        if ( (sColumn->basicInfo->module->flag & MTD_ENCRYPT_TYPE_MASK)
             == MTD_ENCRYPT_TYPE_TRUE )
        {
            if ( sTableOwnerName[0] == '\0' )
            {
                // ���� �ѹ��� ���´�.
                IDE_TEST( qcmUser::getUserName( aStatement,
                                                sParseTree->userID,
                                                sTableOwnerName )
                          != IDE_SUCCESS );

                QC_STR_COPY( sTableName, sParseTree->tableName );
            }
            else
            {
                // Nothing to do.
            }

            QC_STR_COPY( sColumnName, sColumn->namePos );

            (void) qcsModule::setColumnPolicy(
                sTableOwnerName,
                sTableName,
                sColumnName,
                sColumn->basicInfo->mColumnAttr.mEncAttr.mPolicy );
        }
        else
        {
            // Nothing to do.
        }

        sColumn = sColumn->next;
    }

    if ( QCG_GET_SESSION_IS_NEED_DDL_INFO( aStatement ) == ID_TRUE ) 
    {
        /* BUG-47811 DDL that requires NEED_DDL_INFO (transactional DDL, DDL Sync, DDL ASync )
         * should not have global non parititioned index. */
        IDE_TEST_RAISE( qcm::existGlobalNonPartitionedIndex( sTempTableInfo, &sTempIndex ) == ID_TRUE,
                        ERR_NOT_SUPPORT_GLOBAL_NON_PARTITION_INDEX );

        qrc::setDDLDestInfo( aStatement,
                             1,
                             &(sTempTableInfo->tableOID),
                             0,
                             NULL );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NOT_SUPPORT_GLOBAL_NON_PARTITION_INDEX );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_ROLLBACKABLE_DDL_GLOBAL_NOT_ALLOWED_NON_PART_INDEX,
                                  sTempIndex->name ) );

    } 
    IDE_EXCEPTION_END;

    // PROJ-2264 Dictionary Table
    // DDL ���н� dictionary table �� �����Ѵ�.
    for( sDicTable = sDictionaryTable;
         sDicTable != NULL;
         sDicTable = sDicTable->next )
    {
        // BUG-36719
        // Dictionary table �� ��Ÿ���̺��� �ѹ�Ǿ� ����� ���̴�.
        // ���⼭�� table info �� �����Ѵ�.
        (void)qcm::destroyQcmTableInfo( sDicTable->dictionaryTableInfo );
    }

    (void)qcm::destroyQcmTableInfo( sTempTableInfo );
    (void)qcmPartition::destroyQcmPartitionInfoList( sTempPartInfoList );

    for ( sIndexTable = sParseTree->newIndexTables;
          sIndexTable != NULL;
          sIndexTable = sIndexTable->next )
    {
        (void)qcm::destroyQcmTableInfo(sIndexTable->tableInfo);
    }

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qdbCreate::executeCreateTablePartition(qcStatement   * aStatement,
                                              UInt            aTableID,
                                              qcmTableInfo  * aTableInfo)
{
/***********************************************************************
 *
 * Description :
 *      PROJ-1502 PARTITIONED DISK TABLE
 *
 *      CREATE TABLE ���� execution ���� �Լ�
 *      ������ ��Ƽ���� ��� �����Ѵ�.
 *
 * Implementation :
 *      1. ��Ƽ���� ������ŭ �ݺ�
 *          1-1. ��Ƽ�� Ű ���� ��(PARTITION_MAX_VALUE, PARTITION_MIN_VALUE )
 *               �� ���Ѵ�.(�ؽ� ��Ƽ�ǵ� ���̺� ����)
 *          1-2. ��Ƽ�� ����(PARTITION_ORDER)�� ���Ѵ�.
 *               (�ؽ� ��Ƽ�ǵ� ���̺� �ش���)
 *          1-3. ��Ƽ�� ����
 *          1-4. SYS_TABLE_PARTITIONS_ ��Ÿ ���̺� ��Ƽ�� ���� �Է�
 *          1-5. SYS_PART_LOBS_ ��Ÿ ���̺� ��Ƽ�� ���� �Է�
 *          1-6. ���̺� ��Ƽ�� ��Ÿ ĳ�� ����
 *
 ***********************************************************************/

    qdTableParseTree        * sParseTree;
    UInt                      sPartitionID;
    smOID                     sPartitionOID;
    qdPartitionAttribute    * sPartAttr;
    UInt                      sPartOrder;
    SChar                   * sPartMinVal = NULL;
    SChar                   * sPartMaxVal = NULL;
    SChar                   * sOldPartMaxVal = NULL;
    qcmPartitionMethod        sPartMethod;
    UInt                      sPartCount;
    qcmTableInfo            * sPartitionInfo = NULL;
    void                    * sPartitionHandle;
    smSCN                     sSCN;
    UInt                      sColumnCount = 0;
    qcmColumn               * sColumn;
    smiSegAttr                sSegAttr;
    smiSegStorageAttr         sSegStoAttr;
    UInt                      sPartType = 0;

    IDU_LIMITPOINT("qdbCreate::executeCreateTablePartition::malloc1");
    IDE_TEST(STRUCT_ALLOC_WITH_SIZE(aStatement->qmxMem,
                                    SChar,
                                    QC_MAX_PARTKEY_COND_VALUE_LEN+1,
                                    & sPartMaxVal)
             != IDE_SUCCESS);

    IDU_LIMITPOINT("qdbCreate::executeCreateTablePartition::malloc2");
    IDE_TEST(STRUCT_ALLOC_WITH_SIZE(aStatement->qmxMem,
                                    SChar,
                                    QC_MAX_PARTKEY_COND_VALUE_LEN+1,
                                    & sPartMinVal)
             != IDE_SUCCESS);

    IDU_LIMITPOINT("qdbCreate::executeCreateTablePartition::malloc3");
    IDE_TEST(STRUCT_ALLOC_WITH_SIZE(aStatement->qmxMem,
                                    SChar,
                                    QC_MAX_PARTKEY_COND_VALUE_LEN+1,
                                    & sOldPartMaxVal)
             != IDE_SUCCESS);

    sParseTree = (qdTableParseTree *)aStatement->myPlan->parseTree;
    sPartMethod = sParseTree->partTable->partMethod;

    //-----------------------------------------------------
    // 1. ��Ƽ�� ������ŭ �ݺ��ϸ鼭 ��Ƽ���� �����Ѵ�.
    //-----------------------------------------------------
    // ���� ��Ƽ�ǵ��� ����Ʈ�� Ű ���� ���� �������� �������� ���ĵǾ�
    // �ִ� �����̴�.
    // qdbCommon::validateCondValues()�Լ����� ��������.
    for( sPartAttr = sParseTree->partTable->partAttr, sPartCount = 0;
         sPartAttr != NULL;
         sPartAttr = sPartAttr->next, sPartCount++ )
    {
        //-----------------------------------------------------
        // 1-1. ��Ƽ�� Ű ���� ���� ���Ѵ�.(�ؽ� ��Ƽ�ǵ� ���̺� ����)
        //-----------------------------------------------------
        if( sPartMethod != QCM_PARTITION_METHOD_HASH )
        {
            IDE_TEST( qdbCommon::getPartitionMinMaxValue( aStatement,
                                                          sPartAttr,
                                                          sPartCount,
                                                          sPartMethod,
                                                          sPartMaxVal,
                                                          sPartMinVal,
                                                          sOldPartMaxVal )
                      != IDE_SUCCESS );
        }
        else
        {
            sPartMinVal[0] = '\0';
            sPartMaxVal[0] = '\0';
        }

        //-----------------------------------------------------
        // 1-2. ��Ƽ�� ������ ���Ѵ�.(�ؽ� ��Ƽ�ǵ� ���̺� �ش�)
        //-----------------------------------------------------
        if( sPartMethod == QCM_PARTITION_METHOD_HASH )
        {
            sPartOrder = sPartAttr->partOrder;
        }
        else
        {
            sPartOrder = QDB_NO_PARTITION_ORDER;
        }

        //-----------------------------------------------------
        // 1-3. ��Ƽ�� ����
        //-----------------------------------------------------
        IDE_TEST(qcmPartition::getNextTablePartitionID(
                     aStatement,
                     & sPartitionID) != IDE_SUCCESS);

        // ����Ʈ �� validation
        sColumn = sPartAttr->columns;
        while(sColumn != NULL)
        {
            sColumnCount++;
            if (sColumn->defaultValue != NULL)
            {
                IDE_TEST(qdbCommon::convertDefaultValueType(
                             aStatement,
                             &sColumn->basicInfo->type,
                             sColumn->defaultValue,
                             NULL)
                         != IDE_SUCCESS);
            }
            sColumn = sColumn->next;
        }

        sPartType = qdbCommon::getTableTypeFromTBSID( sPartAttr->TBSAttr.mID );

        /* PROJ-2464 hybrid partitioned table ���� */
        qdbCommon::adjustPhysicalAttr( sPartType,
                                       sParseTree->segAttr,
                                       sParseTree->segStoAttr,
                                       & sSegAttr,
                                       & sSegStoAttr,
                                       ID_TRUE /* aIsTable */ );

        IDE_TEST(qdbCommon::createTableOnSM(aStatement,
                                            sPartAttr->columns,
                                            sParseTree->userID,
                                            aTableID,
                                            sParseTree->maxrows,
                                            sPartAttr->TBSAttr.mID,
                                            sSegAttr,
                                            sSegStoAttr,
                                            sParseTree->tableAttrMask,
                                            sParseTree->tableAttrValue,
                                            sParseTree->parallelDegree,
                                            &sPartitionOID)
                 != IDE_SUCCESS);

        //-----------------------------------------------------
        // 1-4. SYS_TABLE_PARTITIONS_ ��Ÿ ���̺� ��Ƽ�� ���� �Է�
        //-----------------------------------------------------
        IDE_TEST(qdbCommon::insertTablePartitionSpecIntoMeta(aStatement,
                                     sParseTree->userID,
                                     aTableID,
                                     sPartitionOID,
                                     sPartitionID,
                                     sPartAttr->tablePartName,
                                     sPartMinVal,
                                     sPartMaxVal,
                                     sPartOrder,
                                     sPartAttr->TBSAttr.mID,
                                     sPartAttr->accessOption)   /* PROJ-2359 Table/Partition Access Option */
                 != IDE_SUCCESS);

        //-----------------------------------------------------
        // 1-5. SYS_PART_LOBS_ ��Ÿ ���̺� ��Ƽ�� ���� �Է�
        //-----------------------------------------------------
        IDE_TEST(qdbCommon::insertPartLobSpecIntoMeta(aStatement,
                                                      sParseTree->userID,
                                                      aTableID,
                                                      sPartitionID,
                                                      sPartAttr->columns )
                 != IDE_SUCCESS);

        //-----------------------------------------------------
        // 1-6. ���̺� ��Ƽ�� ��Ÿ ĳ�� ����
        //-----------------------------------------------------
        IDE_TEST( qcmPartition::makeAndSetQcmPartitionInfo(
                      QC_SMI_STMT( aStatement ),
                      sPartitionID,
                      sPartitionOID,
                      aTableInfo,
                      NULL )
                  != IDE_SUCCESS );

        IDE_TEST(qcmPartition::getPartitionInfoByID(
                     aStatement,
                     sPartitionID,
                     & sPartitionInfo,
                     & sSCN,
                     & sPartitionHandle)
                 != IDE_SUCCESS);

        IDE_TEST( qcmPartition::validateAndLockOnePartition( aStatement,
                                                             sPartitionHandle,
                                                             sSCN,
                                                             SMI_TBSLV_DDL_DML, // TBS Validation �ɼ�
                                                             SMI_TABLE_LOCK_X,
                                                             smiGetDDLLockTimeOut((QC_SMI_STMT(aStatement))->getTrans()))
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if (sPartitionInfo != NULL)
    {
        (void)qcmPartition::destroyQcmPartitionInfo(sPartitionInfo);
    }

    return IDE_FAILURE;
}

IDE_RC qdbCreate::executeCreateTableAsSelect(qcStatement * aStatement)
{
/***********************************************************************
 *
 * Description :
 *      CREATE TABLE ... AS SELECT ���� execution ���� �Լ�
 *
 * Implementation :
 *      1. convertDefaultValueType
 *      2. ���ο� ���̺��� ���� TableID ���ϱ�
 *      3. createTableOnSM
 *      4. SYS_TABLES_ ��Ÿ ���̺� ���̺� ���� �Է�
 *      5. SYS_COLUMNS_ ��Ÿ ���̺� ���̺� �÷� ���� �Է�
 *      6. primary key, not null constraint ����
 *      7. ��Ÿ ĳ�� ����
 *      8. insertRow
 *
 ***********************************************************************/

#define IDE_FN "qdbCreate::executeCreateTableAsSelect"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qdTableParseTree    * sParseTree;
    smiValue            * sInsertedRow;
    smiValue            * sSmiValues = NULL;
    smiValue              sInsertedRowForPartition[QC_MAX_COLUMN_COUNT];
    qdConstraintSpec    * sConstraint;
    smOID                 sTableOID;
    UInt                  sTableID;
    qmcRowFlag            sRowFlag;
    smSCN                 sSCN;
    qcmTableInfo        * sTempTableInfo = NULL;
    smiTableCursor      * sCursor;
    smiTableCursor      * sPartitionCursor;
    qcmColumn           * sColumn;
    SInt                  sStage = 0;
    UInt                  sColumnCount = 0;
    UInt                  sLobColumnCount = 0;
    UInt                  i;
    iduMemoryStatus       sQmxMemStatus;
    smiCursorProperties   sCursorProperty;
    void                * sTableHandle;
    qmxLobInfo          * sLobInfo = NULL;
    void                * sRow = NULL;
    scGRID                sRowGRID;
    idBool                sIsPartitioned = ID_FALSE;
    qcmColumn           * sPartKeyColumns;
    UInt                  sPartKeyCount;
    qcmPartitionInfoList* sPartInfoList     = NULL;
    qcmPartitionInfoList* sOldPartInfoList  = NULL;
    qcmPartitionInfoList* sTempPartInfoList = NULL;
    qcmTableInfo        * sPartInfo;
    UInt                  sPartCount = 0;
    qcmTemporaryType      sTemporaryType;
    qdIndexTableList    * sIndexTable;

    qmsTableRef         * sTableRef = NULL;
    qmsPartitionRef     * sPartitionRef = NULL;
    qmsPartitionRef     * sFirstPartitionRef = NULL;
    qmcInsertCursor       sInsertCursorMgr;
    qdPartitionAttribute* sPartAttr;
    
    qdIndexTableCursors   sIndexTableCursorInfo;
    idBool                sInitedCursorInfo = ID_FALSE;
    smOID                 sPartOID;

    qcmTableInfo        * sSelectedPartitionInfo = NULL;

    sRowFlag = QMC_ROW_INITIALIZE;

    SMI_CURSOR_PROP_INIT_FOR_FULL_SCAN( &sCursorProperty, aStatement->mStatistics );

    sCursorProperty.mIsUndoLogging = ID_FALSE;

    sParseTree = (qdTableParseTree *)aStatement->myPlan->parseTree;

    for ( sColumn = sParseTree->columns;
          sColumn != NULL;
          sColumn = sColumn->next )
    {
        sColumnCount++;

        if (sColumn->defaultValue != NULL)
        {
            IDE_TEST(qdbCommon::convertDefaultValueType(
                         aStatement,
                         &sColumn->basicInfo->type,
                         sColumn->defaultValue,
                         NULL)
                     != IDE_SUCCESS);
        }

        // PROJ-1362
        if ( (sColumn->basicInfo->module->flag & MTD_COLUMN_TYPE_MASK)
             == MTD_COLUMN_TYPE_LOB )
        {
            sLobColumnCount++;
        }
        else
        {
            // Nothing to do.
        }

        // TODO : Source�� LOB Column ���� ��Ȯ�ϰ� �����ϸ�, �޸� ��뷮�� ���� �� �ִ�.
        sLobColumnCount++;
    }

    // PROJ-1502 PARTITIONED DISK TABLE
    // ����Ƽ�ǵ� ���̺� ����
    if( sParseTree->partTable == NULL )
    {
        sIsPartitioned = ID_FALSE;
    }
    // ��Ƽ�ǵ� ���̺� ����
    else
    {
        sIsPartitioned = ID_TRUE;

        sPartKeyCount = 0;

        for( sPartKeyColumns = sParseTree->partTable->partKeyColumns;
             sPartKeyColumns != NULL;
             sPartKeyColumns = sPartKeyColumns->next )
        {
            sPartKeyCount++;
        }
    }

    // PROJ-1407 Temporary Table
    if ( sParseTree->temporaryOption == NULL )
    {
        if ( ( sParseTree->flag & QDT_CREATE_TEMPORARY_MASK ) ==
             QDT_CREATE_TEMPORARY_TRUE )
        {
            sTemporaryType = QCM_TEMPORARY_ON_COMMIT_DELETE_ROWS;
        }
        else
        {
            sTemporaryType = QCM_TEMPORARY_ON_COMMIT_NONE;
        }
    }
    else
    {
        sTemporaryType = sParseTree->temporaryOption->temporaryType;
    }

    // PROJ-1362
    IDE_TEST( qmx::initializeLobInfo(aStatement,
                                     & sLobInfo,
                                     (UShort)sLobColumnCount)
              != IDE_SUCCESS );

    /* BUG-36211 �� Row Insert ��, �ش� Lob Cursor�� �ٷ� �����մϴ�. */
    qmx::setImmediateCloseLobInfo( sLobInfo, ID_TRUE );

    IDE_TEST(qcm::getNextTableID(aStatement, &sTableID) != IDE_SUCCESS);

    IDE_TEST(qdbCommon::createTableOnSM(aStatement,
                                        sParseTree->columns,
                                        sParseTree->userID,
                                        sTableID,
                                        sParseTree->maxrows,
                                        sParseTree->TBSAttr.mID,
                                        sParseTree->segAttr,
                                        sParseTree->segStoAttr,
                                        sParseTree->tableAttrMask,
                                        sParseTree->tableAttrValue,
                                        sParseTree->parallelDegree,
                                        &sTableOID)
             != IDE_SUCCESS);

    IDE_TEST(qdbCommon::insertTableSpecIntoMeta(aStatement,
                                                sIsPartitioned,
                                                sParseTree->flag,
                                                sParseTree->tableName,
                                                sParseTree->userID,
                                                sTableOID,
                                                sTableID,
                                                sColumnCount,
                                                sParseTree->maxrows,
                                                sParseTree->accessOption, /* PROJ-2359 Table/Partition Access Option */
                                                sParseTree->TBSAttr.mID,
                                                sParseTree->segAttr,
                                                sParseTree->segStoAttr,
                                                sTemporaryType,
                                                sParseTree->parallelDegree,      // PROJ-1071
                                                sParseTree->mShardFlag)          // TASK-7307
             != IDE_SUCCESS);

    IDE_TEST(qdbCommon::insertColumnSpecIntoMeta(aStatement,
                                                 sParseTree->userID,
                                                 sTableID,
                                                 sParseTree->columns,
                                                 ID_FALSE /* is queue */)
             != IDE_SUCCESS);

    // PROJ-1502 PARTITIONED DISK TABLE
    if( sIsPartitioned == ID_TRUE )
    {
        IDE_TEST(qdbCommon::insertPartTableSpecIntoMeta(aStatement,
                                                        sParseTree->userID,
                                                        sTableID,
                                                        sParseTree->partTable->partMethod,
                                                        sPartKeyCount,
                                                        sParseTree->isRowmovement )
                 != IDE_SUCCESS);

        IDE_TEST(qdbCommon::insertPartKeyColumnSpecIntoMeta(aStatement,
                                                            sParseTree->userID,
                                                            sTableID,
                                                            sParseTree->columns,
                                                            sParseTree->partTable->partKeyColumns,
                                                            QCM_TABLE_OBJECT_TYPE)
                 != IDE_SUCCESS);

        IDE_TEST(qcm::makeAndSetQcmTableInfo(QC_SMI_STMT( aStatement ),
                                             sTableID,
                                             sTableOID) != IDE_SUCCESS);

        IDE_TEST(qcm::getTableInfoByID(aStatement,
                                       sTableID,
                                       &sTempTableInfo,
                                       &sSCN,
                                       &sTableHandle)
                 != IDE_SUCCESS);

        IDE_TEST( executeCreateTablePartition( aStatement,
                                               sTableID,
                                               sTempTableInfo )
                  != IDE_SUCCESS );

        IDE_TEST( qcmPartition::getPartitionInfoList( aStatement,
                                                      QC_SMI_STMT( aStatement ),
                                                      aStatement->qmxMem,
                                                      sTableID,
                                                      & sTempPartInfoList )
                  != IDE_SUCCESS );

        // ��� ��Ƽ�ǿ� LOCK(X)
        IDE_TEST( qcmPartition::validateAndLockPartitionInfoList( aStatement,
                                                                  sTempPartInfoList,
                                                                  SMI_TBSLV_DDL_DML, // TBS Validation �ɼ�
                                                                  SMI_TABLE_LOCK_X,
                                                                  smiGetDDLLockTimeOut((QC_SMI_STMT(aStatement))->getTrans()))
                  != IDE_SUCCESS );

        sParseTree->partTable->partInfoList = sTempPartInfoList;
    }

    for (sConstraint = sParseTree->constraints;
         sConstraint != NULL;
         sConstraint = sConstraint->next)
    {
        fillColumnID( sParseTree->columns, sConstraint );

        IDE_TEST(qdbCommon::createConstrPrimaryUnique(aStatement,
                                                      sTableOID,
                                                      sConstraint,
                                                      sParseTree->userID,
                                                      sTableID,
                                                      sTempPartInfoList,
                                                      sParseTree->maxrows )
                 != IDE_SUCCESS);

        IDE_TEST(qdbCommon::createConstrNotNull(aStatement,
                                                sConstraint,
                                                sParseTree->userID,
                                                sTableID)
                 != IDE_SUCCESS);

        IDE_TEST(qdbCommon::createConstrTimeStamp(aStatement,
                                                  sConstraint,
                                                  sParseTree->userID,
                                                  sTableID)
                 != IDE_SUCCESS);
    }

    if( sTempTableInfo != NULL )
    {
        (void)qcm::destroyQcmTableInfo(sTempTableInfo);
    }
    sTempTableInfo = NULL;

    IDE_TEST(qcm::makeAndSetQcmTableInfo(QC_SMI_STMT( aStatement ),
                                         sTableID,
                                         sTableOID) != IDE_SUCCESS);

    IDE_TEST(qcm::getTableInfoByID(aStatement,
                                   sTableID,
                                   &sTempTableInfo,
                                   &sSCN,
                                   &sTableHandle)
             != IDE_SUCCESS);

    IDE_TEST(qcm::validateAndLockTable(aStatement,
                                       sTableHandle,
                                       sSCN,
                                       SMI_TABLE_LOCK_X)
             != IDE_SUCCESS);

    for (sConstraint = sParseTree->constraints;
         sConstraint != NULL;
         sConstraint = sConstraint->next)
    {
        if (sConstraint->constrType == QD_FOREIGN)
        {
            (void)qcm::destroyQcmTableInfo(sTempTableInfo);
            sTempTableInfo = NULL;

            IDE_TEST(qcm::makeAndSetQcmTableInfo(QC_SMI_STMT(aStatement),
                                                 sTableID,
                                                 sTableOID) != IDE_SUCCESS);

            IDE_TEST(qcm::getTableInfoByID(aStatement,
                                           sTableID,
                                           &sTempTableInfo,
                                           &sSCN,
                                           &sTableHandle)
                     != IDE_SUCCESS);

            IDE_TEST(qdbCommon::createConstrForeign(aStatement,
                                                    sConstraint,
                                                    sParseTree->userID,
                                                    sTableID) != IDE_SUCCESS);
        }

        if (sConstraint->constrType == QD_NOT_NULL)
        {
            for (i = 0; i < sConstraint->constrColumnCount; i++)
            {
                sColumn = sConstraint->constraintColumns;

                IDE_TEST(qdbCommon::makeColumnNotNull(
                                aStatement,
                                sTempTableInfo->tableHandle,
                                sParseTree->maxrows,
                                sTempPartInfoList,
                                sIsPartitioned,
                                sColumn->basicInfo->column.id)
                         != IDE_SUCCESS);

                sColumn = sColumn->next;
            }
        }
    }

    if (sTempTableInfo->primaryKey != NULL)
    {
        for ( i = 0; i < sTempTableInfo->primaryKey->keyColCount; i++)
        {
            IDE_TEST(qdbCommon::makeColumnNotNull(
                         aStatement,
                         sTempTableInfo->tableHandle,
                         sParseTree->maxrows,
                         sTempPartInfoList,
                         sIsPartitioned,
                         sTempTableInfo->primaryKey->keyColumns[i].column.id)
                     != IDE_SUCCESS);
        }
    }

    (void)qcm::destroyQcmTableInfo( sTempTableInfo );
    sTempTableInfo = NULL;

    IDE_TEST(qcm::makeAndSetQcmTableInfo(QC_SMI_STMT( aStatement ),
                                         sTableID,
                                         sTableOID) != IDE_SUCCESS);

    IDE_TEST(qcm::getTableInfoByID(aStatement,
                                   sTableID,
                                   &sTempTableInfo,
                                   &sSCN,
                                   &sTableHandle)
             != IDE_SUCCESS);

    // PROJ-1502 PARTITIONED DISK TABLE
    if( sIsPartitioned == ID_TRUE )
    {
        sOldPartInfoList = sTempPartInfoList;
        IDE_TEST( qcmPartition::makeAndSetAndGetQcmPartitionInfoList( aStatement,
                                                                      sTempTableInfo,
                                                                      sOldPartInfoList,
                                                                      & sTempPartInfoList )
                  != IDE_SUCCESS );

        (void)qcmPartition::destroyQcmPartitionInfoList( sOldPartInfoList );
    }

    IDU_FIT_POINT( "qdbCreate::executeCreateTableAsSelect::alloc::sInsertedRow",
                    idERR_ABORT_InsufficientMemory );

    IDE_TEST(aStatement->qmxMem->alloc(ID_SIZEOF(smiValue) * sColumnCount,
                                       (void**)&sInsertedRow)
             != IDE_SUCCESS);

    // BUG-16535
    // set SYSDATE
    IDE_TEST( qtc::setDatePseudoColumn( QC_PRIVATE_TMPLATE( aStatement ) ) != IDE_SUCCESS );

    // check session cache SEQUENCE.CURRVAL
    if (sParseTree->select->myPlan->parseTree->currValSeqs != NULL)
    {
        IDE_TEST(qmx::findSessionSeqCaches(aStatement,
                                           sParseTree->select->myPlan->parseTree)
                 != IDE_SUCCESS);
    }

    // get SEQUENCE.NEXTVAL
    if (sParseTree->select->myPlan->parseTree->nextValSeqs != NULL)
    {
        IDE_TEST(qmx::addSessionSeqCaches(aStatement,
                                          sParseTree->select->myPlan->parseTree)
                 != IDE_SUCCESS);

        ((qmncPROJ *)(sParseTree->select->myPlan->plan))->nextValSeqs =
            sParseTree->select->myPlan->parseTree->nextValSeqs;
    }

    IDE_TEST(qmnPROJ::init( QC_PRIVATE_TMPLATE(aStatement), sParseTree->select->myPlan->plan)
             != IDE_SUCCESS);

    IDU_LIMITPOINT("qdbCreate::executeCreateTableAsSelect::malloc2");
    IDE_TEST(STRUCT_ALLOC(aStatement->qmxMem,
                          qmsTableRef,
                          (void**)& sTableRef)
             != IDE_SUCCESS);
    QCP_SET_INIT_QMS_TABLE_REF( sTableRef );
    
    sTableRef->userName = sParseTree->userName;
    sTableRef->tableName = sParseTree->tableName;
    sTableRef->userID = sParseTree->userID;
    sTableRef->tableInfo = sTempTableInfo;
    sTableRef->tableHandle = sTempTableInfo->tableHandle;
    sTableRef->tableSCN = smiGetRowSCN( sTempTableInfo->tableHandle );

    // PROJ-1502 PARTITIONED DISK TABLE
    if( sIsPartitioned == ID_FALSE )
    {
        IDE_TEST( sInsertCursorMgr.initialize( aStatement->qmxMem,
                                               sTableRef,
                                               ID_TRUE,
                                               ID_TRUE ) /* init all partitions */
                  != IDE_SUCCESS );
        
        IDE_TEST( sInsertCursorMgr.openCursor( aStatement,
                                               SMI_LOCK_WRITE|
                                               SMI_TRAVERSE_FORWARD|
                                               SMI_PREVIOUS_DISABLE,
                                               & sCursorProperty )
                  != IDE_SUCCESS );

        sStage = 1;

        IDE_TEST(qmnPROJ::doIt( QC_PRIVATE_TMPLATE(aStatement),
                                sParseTree->select->myPlan->plan,
                                &sRowFlag)
                 != IDE_SUCCESS);

        /* PROJ-2359 Table/Partition Access Option */
        if ( (sRowFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
        {
            IDE_TEST( qmx::checkAccessOption( sTempTableInfo,
                                              ID_TRUE, /* aIsInsertion */
                                              QCG_CHECK_SHARD_DML_CONSISTENCY( aStatement ) )
                      != IDE_SUCCESS );
        }
        else
        {
            /* Nothing to do */
        }

        // To fix BUG-13978
        // bitwise operation(&) for flag check
        while ((sRowFlag & QMC_ROW_DATA_MASK) ==  QMC_ROW_DATA_EXIST )
        {
            IDE_TEST( QC_QMX_MEM(aStatement)-> getStatus( &sQmxMemStatus )
                      != IDE_SUCCESS);

            (void)qmx::clearLobInfo( sLobInfo );

            IDE_TEST(qmx::makeSmiValueWithResult(sParseTree->columns,
                                                 QC_PRIVATE_TMPLATE(aStatement),
                                                 sTempTableInfo,
                                                 sInsertedRow,
                                                 sLobInfo )
                     != IDE_SUCCESS);

            // To fix BUG-12622
            // not nullüũ �Լ��� makeSmiValueWithResult���� ���� �и��Ͽ���
            IDE_TEST( qmx::checkNotNullColumnForInsert(
                          sTempTableInfo->columns,
                          sInsertedRow,
                          sLobInfo,
                          ID_FALSE )
                      != IDE_SUCCESS );

            //------------------------------------------
            // INSERT�� ����
            //------------------------------------------
            
            IDE_TEST( sInsertCursorMgr.getCursor( &sCursor )
                      != IDE_SUCCESS );

            IDE_TEST( sCursor->insertRow( sInsertedRow,
                                          &sRow,
                                          &sRowGRID )
                      != IDE_SUCCESS);
            
            IDE_TEST( qmx::copyAndOutBindLobInfo( aStatement,
                                                  sLobInfo,
                                                  sCursor,
                                                  sRow,
                                                  sRowGRID )
                      != IDE_SUCCESS );

            IDE_TEST( QC_QMX_MEM(aStatement)-> setStatus( &sQmxMemStatus )
                      != IDE_SUCCESS );

            IDE_TEST(qmnPROJ::doIt( QC_PRIVATE_TMPLATE(aStatement),
                                    sParseTree->select->myPlan->plan,
                                    &sRowFlag)
                     != IDE_SUCCESS);
        }

        sStage = 0;

        IDE_TEST( sInsertCursorMgr.closeCursor() != IDE_SUCCESS );
    }
    else
    {
        //------------------------------------------
        // open partition insert cursors
        //------------------------------------------
        
        // partitionRef ����
        for( sPartInfoList = sTempPartInfoList, sPartCount = 0;
             sPartInfoList != NULL;
             sPartInfoList = sPartInfoList->next,
                 sPartCount++ )
        {
            sPartInfo = sPartInfoList->partitionInfo;

            IDU_LIMITPOINT("qdbCreate::executeCreateTableAsSelect::malloc3");
            IDE_TEST(STRUCT_ALLOC(aStatement->qmxMem,
                                  qmsPartitionRef,
                                  (void**)& sPartitionRef)
                     != IDE_SUCCESS);
            QCP_SET_INIT_QMS_PARTITION_REF( sPartitionRef );

            sPartitionRef->partitionID = sPartInfo->partitionID;
            sPartitionRef->partitionInfo = sPartInfo;
            sPartitionRef->partitionSCN = sPartInfoList->partSCN;
            sPartitionRef->partitionHandle = sPartInfoList->partHandle;

            // BUG-48782
            for ( sPartAttr = sParseTree->partTable->partAttr;
                  sPartAttr != NULL;
                  sPartAttr = sPartAttr->next )
            {
                // BUG-37032
                if ( idlOS::strMatch( sPartAttr->tablePartName.stmtText + sPartAttr->tablePartName.offset,
                                      sPartAttr->tablePartName.size,
                                      sPartInfo->name,
                                      idlOS::strlen(sPartInfo->name) ) == 0 )
                {
                    break;
                }

            }

            if(sParseTree->partTable->partMethod != QCM_PARTITION_METHOD_HASH)
            {
                sPartitionRef->minPartCondVal =
                    *(sPartAttr->alterPart->partCondMinVal);
                sPartitionRef->maxPartCondVal =
                    *(sPartAttr->alterPart->partCondMaxVal);

                // minValue�� partCondValType ����
                if( sPartitionRef->minPartCondVal.partCondValCount == 0 )
                {
                    sPartitionRef->minPartCondVal.partCondValType
                        = QMS_PARTCONDVAL_MIN;
                }
                else
                {
                    sPartitionRef->minPartCondVal.partCondValType
                        = QMS_PARTCONDVAL_NORMAL;
                }

                // maxValue�� partCondValType ����
                if( sPartitionRef->maxPartCondVal.partCondValCount == 0 )
                {
                    sPartitionRef->maxPartCondVal.partCondValType
                        = QMS_PARTCONDVAL_DEFAULT;
                }
                else
                {
                    sPartitionRef->maxPartCondVal.partCondValType
                        = QMS_PARTCONDVAL_NORMAL;
                }
            }
            else
            {
                sPartitionRef->partOrder = sPartAttr->partOrder;
            }

            if( sFirstPartitionRef == NULL )
            {
                sPartitionRef->next = NULL;
                sFirstPartitionRef = sPartitionRef;
            }
            else
            {
                sPartitionRef->next = sFirstPartitionRef;
                sFirstPartitionRef = sPartitionRef;
            }
        }

        sTableRef->partitionRef = sFirstPartitionRef;
        sTableRef->partitionCount = sPartCount;

        /* PROJ-2464 hybrid partitioned table ���� */
        IDE_TEST( qcmPartition::makePartitionSummary( aStatement, sTableRef )
                  != IDE_SUCCESS );

        IDE_TEST( sInsertCursorMgr.initialize( aStatement->qmxMem,
                                               sTableRef,
                                               ID_TRUE,
                                               ID_TRUE ) /* init all partitions */
                  != IDE_SUCCESS );

        IDE_TEST( sInsertCursorMgr.openCursor( aStatement,
                                               SMI_LOCK_WRITE|
                                               SMI_TRAVERSE_FORWARD|
                                               SMI_PREVIOUS_DISABLE,
                                               & sCursorProperty )
                  != IDE_SUCCESS );

        sStage = 1;
        
        //------------------------------------------
        // open index table insert cursors
        //------------------------------------------
        
        if ( sParseTree->newIndexTables != NULL )
        {
            IDE_TEST( qdx::initializeInsertIndexTableCursors(
                          aStatement,
                          sParseTree->newIndexTables,
                          &sIndexTableCursorInfo,
                          sTempTableInfo->indices,
                          sTempTableInfo->indexCount,
                          sColumnCount,
                          &sCursorProperty )
                      != IDE_SUCCESS );
            
            sInitedCursorInfo = ID_TRUE;
        }
        else
        {
            // Nothing to do.
        }
        
        //------------------------------------------
        // SELECT�� ����
        //------------------------------------------
        
        IDE_TEST(qmnPROJ::doIt( QC_PRIVATE_TMPLATE(aStatement),
                                sParseTree->select->myPlan->plan,
                                &sRowFlag)
                 != IDE_SUCCESS)

        /* PROJ-2359 Table/Partition Access Option */
        if ( (sRowFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
        {
            IDE_TEST( qmx::checkAccessOption( sTempTableInfo,
                                              ID_TRUE, /* aIsInsertion */
                                              QCG_CHECK_SHARD_DML_CONSISTENCY( aStatement ) )
                      != IDE_SUCCESS );
        }
        else
        {
            /* Nothing to do */
        }

        // To fix BUG-13978
        // bitwise operation(&) for flag check
        while ((sRowFlag & QMC_ROW_DATA_MASK) ==  QMC_ROW_DATA_EXIST )
        {
            IDE_TEST( QC_QMX_MEM(aStatement)-> getStatus( &sQmxMemStatus )
                      != IDE_SUCCESS);

            (void)qmx::clearLobInfo( sLobInfo );

            IDE_TEST(qmx::makeSmiValueWithResult(sParseTree->columns,
                                                 QC_PRIVATE_TMPLATE(aStatement),
                                                 sTempTableInfo,
                                                 sInsertedRow,
                                                 sLobInfo )
                     != IDE_SUCCESS);

            // To fix BUG-12622
            // not nullüũ �Լ��� makeSmiValueWithResult���� ���� �и��Ͽ���
            IDE_TEST( qmx::checkNotNullColumnForInsert(
                          sTempTableInfo->columns,
                          sInsertedRow,
                          sLobInfo,
                          ID_FALSE )
                      != IDE_SUCCESS );

            //------------------------------------------
            // INSERT�� ����
            //------------------------------------------

            IDE_TEST( sInsertCursorMgr.partitionFilteringWithRow(
                          sInsertedRow,
                          sLobInfo,
                          &sSelectedPartitionInfo )
                      != IDE_SUCCESS );

            /* PROJ-2359 Table/Partition Access Option */
            IDE_TEST( qmx::checkAccessOption( sSelectedPartitionInfo,
                                              ID_TRUE, /* aIsInsertion */
                                              QCG_CHECK_SHARD_DML_CONSISTENCY( aStatement ) )
                      != IDE_SUCCESS );

            IDE_TEST( sInsertCursorMgr.getCursor( &sPartitionCursor )
                      != IDE_SUCCESS );

            if ( QCM_TABLE_TYPE_IS_DISK( sTempTableInfo->tableFlag ) !=
                 QCM_TABLE_TYPE_IS_DISK( sSelectedPartitionInfo->tableFlag ) )
            {
                /* PROJ-2464 hybrid partitioned table ����
                 * Partitioned Table�� �������� ���� smiValue Array�� Table Partition�� �°� ��ȯ�Ѵ�.
                 */
                IDE_TEST( qmx::makeSmiValueWithSmiValue( sTempTableInfo,
                                                         sSelectedPartitionInfo,
                                                         sTempTableInfo->columns,
                                                         sColumnCount,
                                                         sInsertedRow,
                                                         sInsertedRowForPartition )
                          != IDE_SUCCESS );

                sSmiValues = sInsertedRowForPartition;
            }
            else
            {
                sSmiValues = sInsertedRow;
            }

            IDE_TEST(sPartitionCursor->insertRow( sSmiValues,
                                                  & sRow,
                                                  & sRowGRID )
                     != IDE_SUCCESS);

            //------------------------------------------
            // INSERT�� ������ Lob �÷��� ó��
            //------------------------------------------
            
            IDE_TEST( qmx::copyAndOutBindLobInfo( aStatement,
                                                  sLobInfo,
                                                  sPartitionCursor,
                                                  sRow,
                                                  sRowGRID )
                      != IDE_SUCCESS );
            
            //------------------------------------------
            // INSERT�� ������ non-partitioned index�� ó��
            //------------------------------------------

            if ( sParseTree->newIndexTables != NULL )
            {
                IDE_TEST( sInsertCursorMgr.getSelectedPartitionOID( & sPartOID )
                          != IDE_SUCCESS );
                
                IDE_TEST( qdx::insertIndexTableCursors( &sIndexTableCursorInfo,
                                                        sInsertedRow,
                                                        sPartOID,
                                                        sRowGRID )
                          != IDE_SUCCESS );
            }
            else
            {
                // Nothing to do.
            }
            
            IDE_TEST( QC_QMX_MEM(aStatement)-> setStatus( &sQmxMemStatus )
                      != IDE_SUCCESS );

            IDE_TEST(qmnPROJ::doIt( QC_PRIVATE_TMPLATE(aStatement),
                                    sParseTree->select->myPlan->plan,
                                    &sRowFlag)
                     != IDE_SUCCESS);
        }

        sStage = 0;

        IDE_TEST(sInsertCursorMgr.closeCursor() != IDE_SUCCESS);

        // close index table cursor
        if ( sParseTree->newIndexTables != NULL )
        {
            IDE_TEST( qdx::closeInsertIndexTableCursors(
                          &sIndexTableCursorInfo )
                      != IDE_SUCCESS );
        }
        else
        {
            // Nothing to do.
        }
    }

    /* PROJ-1723 [MDW/INTEGRATOR] Altibase Plugin ����
       DDL Statement Text�� �α�
    */
    if (QCU_DDL_SUPPLEMENTAL_LOG == 1)
    {
        IDE_TEST( qciMisc::writeDDLStmtTextLog( aStatement,
                                                sParseTree->userID,
                                                sTempTableInfo->name )
                  != IDE_SUCCESS );

        IDE_TEST(qci::mManageReplicationCallback.mWriteTableMetaLog(
                                                               aStatement,
                                                               0,
                                                               sTableOID)
                 != IDE_SUCCESS);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if (sStage == 1)
    {
        (void)sInsertCursorMgr.closeCursor();
    }

    if ( sInitedCursorInfo == ID_TRUE )
    {
        qdx::finalizeInsertIndexTableCursors(
            &sIndexTableCursorInfo );
    }
    else
    {
        // Nothing to do.
    }
            
    (void)qcm::destroyQcmTableInfo( sTempTableInfo );
    (void)qcmPartition::destroyQcmPartitionInfoList( sTempPartInfoList );

    for ( sIndexTable = sParseTree->newIndexTables;
          sIndexTable != NULL;
          sIndexTable = sIndexTable->next )
    {
        (void)qcm::destroyQcmTableInfo(sIndexTable->tableInfo);
    }
    
    (void)qmx::finalizeLobInfo( aStatement, sLobInfo );

    return IDE_FAILURE;

#undef IDE_FN
}

void qdbCreate::fillColumnID(
    qcmColumn        *columns,
    qdConstraintSpec *constraints)
{
/***********************************************************************
 *
 * Description :
 *    ��ü �÷��� ����Ʈ���� constraint �� �����ϴ� �÷��� ã�Ƽ� id �ο�
 *
 * Implementation :
 *    1. constraint �� �����ϴ� �÷��� �̸��� ���� ���� ��ü �÷��߿���
 *       ã�� �Ŀ� �� ID �� qdConstraintSpec �� �÷� ID �� �ο�
 *    2. constraint Ÿ���� foreign key �̰� �����ϴ� ���̺��� self ���̺��̸�
 *       1 �� ����ó�� ���ļ� ID �� �ο�
 *
 ***********************************************************************/

#define IDE_FN "qdbCreate::fillColumnID"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qcmColumn        *sTableColumn;
    qcmColumn        *sConstraintColumn;
    qcmColumn        *sReferencedColumn;
    qdConstraintSpec *sConstraint;

    sConstraint = constraints;
    while (sConstraint != NULL)
    {
        for (sConstraintColumn = sConstraint->constraintColumns;
             sConstraintColumn != NULL;
             sConstraintColumn = sConstraintColumn->next)
        {
            for (sTableColumn = columns;
                 sTableColumn != NULL;
                 sTableColumn = sTableColumn->next)
            {
                if (sConstraintColumn->namePos.size > 0)
                {
                    if ( idlOS::strMatch( sTableColumn->namePos.stmtText + sTableColumn->namePos.offset,
                                          sTableColumn->namePos.size,
                                          sConstraintColumn->namePos.stmtText + sConstraintColumn->namePos.offset,
                                          sConstraintColumn->namePos.size) == 0 )
                    {
                        // found
                        sConstraintColumn->basicInfo->column.id = sTableColumn->basicInfo->column.id;
                        break;
                    }
                }
                else
                {
                    if ( idlOS::strMatch( sTableColumn->name,
                                          idlOS::strlen(sTableColumn->name),
                                          sConstraintColumn->name,
                                          idlOS::strlen(sConstraintColumn->name) ) == 0 )
                    {
                        // found
                        sConstraintColumn->basicInfo->column.id = sTableColumn->basicInfo->column.id;
                        break;
                    }
                }
            }
        }

        if (sConstraint->constrType == QD_FOREIGN)
        {
            if ( sConstraint->referentialConstraintSpec->referencedTableID
                 == ID_UINT_MAX ) // self
            {
                sReferencedColumn = sConstraint->referentialConstraintSpec->referencedColList;
                while (sReferencedColumn != NULL)
                {
                    sTableColumn = columns;
                    while (sTableColumn != NULL)
                    {
                        if ( idlOS::strMatch( sTableColumn->namePos.stmtText + sTableColumn->namePos.offset,
                                              sTableColumn->namePos.size,
                                              sReferencedColumn->namePos.stmtText + sReferencedColumn->namePos.offset,
                                              sReferencedColumn->namePos.size) == 0 )
                        {
                            sReferencedColumn->basicInfo->column.id = sTableColumn->basicInfo->column.id;
                        }

                        sTableColumn = sTableColumn->next;
                    }
                    sReferencedColumn = sReferencedColumn->next;
                }
            }
            else
            {
                /* Nothing to do */
            }
        }
        sConstraint = sConstraint->next;
    }

#undef IDE_FN
}


/*  Table�� Attribute Flag List�κ���
    32bit Flag���� ���

    [IN] qcStatement  - �������� Statement
    [IN] aCreateTable - Create Table�� Parse Tree
 */
IDE_RC qdbCreate::calculateTableAttrFlag( qcStatement      * aStatement,
                                          qdTableParseTree * aCreateTable )
{
    qdPartitionAttribute * sPartAttr    = NULL;
    UInt                   sLoggingMode = 0;

    if ( aCreateTable->tableAttrFlagList != NULL )
    {
        IDE_TEST( qdbCommon::validateTableAttrFlagList(
                      aStatement,
                      aCreateTable->tableAttrFlagList )
                  != IDE_SUCCESS );

        IDE_TEST( qdbCommon::getTableAttrFlagFromList(
                      aCreateTable->tableAttrFlagList,
                      & aCreateTable->tableAttrMask,
                      & aCreateTable->tableAttrValue )
                  != IDE_SUCCESS );
    }
    else
    {

        // Tablespace Attribute List�� �������� ���� ���
        // �⺻������ ����
        //
        // Tablespace Attribute���� �⺻������ ���� ���� ����
        // Bit 0�� ����ϵ��� �����ȴ�.
        //
        // Mask�� 0���� �����صθ� Create Table��
        // Table�� flag�� �ǵ帮�� �ʰ� 0���� ���ܵд�.
        aCreateTable->tableAttrMask   = 0;
        aCreateTable->tableAttrValue  = 0;
    }

    // PROJ-1665 : logging / no logging mode�� �־��� ���,
    // �̸� ������
    if ( aCreateTable->loggingMode != NULL )
    {
        sLoggingMode = *(aCreateTable->loggingMode);
        aCreateTable->tableAttrMask = aCreateTable->tableAttrMask |
                                      SMI_TABLE_LOGGING_MASK;
        aCreateTable->tableAttrValue = aCreateTable->tableAttrValue |
                                       sLoggingMode;
    }
    else
    {
        // nothing to do
    }

    // Volatile Tablespace�� ��� ���� üũ �ǽ�
    if ( smiTableSpace::isVolatileTableSpace( aCreateTable->TBSAttr.mID )
         == ID_TRUE )
    {
        // COMPRESSED LOGGING ���� ���� ����
        IDE_TEST( checkError4CreateVolatileTable(aCreateTable)
                  != IDE_SUCCESS );

        // �α� ���� ���� �ʵ��� �⺻�� ����
        aCreateTable->tableAttrValue &= ~SMI_TABLE_LOG_COMPRESS_MASK;
        aCreateTable->tableAttrValue |=  SMI_TABLE_LOG_COMPRESS_FALSE;
    }

    /* PROJ-2464 hybrid partitioned table ���� */
    if ( aCreateTable->partTable != NULL )
    {
        for ( sPartAttr = aCreateTable->partTable->partAttr;
              sPartAttr != NULL;
              sPartAttr = sPartAttr->next )
        {
            if ( smiTableSpace::isVolatileTableSpace( sPartAttr->TBSAttr.mID )
                 == ID_TRUE )
            {
                // COMPRESSED LOGGING ���� ���� ����
                IDE_TEST( checkError4CreateVolatileTable( aCreateTable )
                          != IDE_SUCCESS );
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

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* Volatile Tablespace�� Table������ �����ϴ� ����ó��

   => Volatile Tablespace�� ��� Log Compression�������� �ʴ´�.
      Create Table������ COMPRESSED LOGGING
      ���� ����� ��� ����

   [IN] aAttrFlagList - Tablespace Attribute Flag�� List
*/
IDE_RC qdbCreate::checkError4CreateVolatileTable(
                      qdTableParseTree * aCreateTable )
{
    IDE_DASSERT( aCreateTable != NULL );

    qdTableAttrFlagList * sAttrFlagList = aCreateTable->tableAttrFlagList;
    
    for ( ; sAttrFlagList != NULL ; sAttrFlagList = sAttrFlagList->next )
    {
        if ( (sAttrFlagList->attrMask & SMI_TABLE_LOG_COMPRESS_MASK) != 0 )
        {
            if ( (sAttrFlagList->attrValue & SMI_TABLE_LOG_COMPRESS_MASK )
                 == SMI_TABLE_LOG_COMPRESS_TRUE )
            {
                IDE_RAISE( err_volatile_log_compress );
            }
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_volatile_log_compress );
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDT_UNABLE_TO_COMPRESS_VOLATILE_TBS_LOG));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdbCreate::validateTemporaryTable( qcStatement      * aStatement,
                                          qdTableParseTree * aParseTree )
{
/***********************************************************************
 *
 * Description :
 *    create Temporary table ���� �Լ�
 *
 * Implementation :
 *     1. temporary table�� volatile tbs������ ������ �� �ִ�.
 *     2. temporary table���� on commit option�� ����� �� �ִ�.
 *
 ***********************************************************************/
    qcuSqlSourceInfo  sqlInfo;

    IDE_DASSERT( aParseTree != NULL );

    // PROJ-1407 Temporary Table
    if ( ( aParseTree->flag & QDT_CREATE_TEMPORARY_MASK ) == QDT_CREATE_TEMPORARY_TRUE )
    {
        // temporary table�� volatile tbs������ ������ �� �ִ�.
        IDE_TEST_RAISE( smiTableSpace::isVolatileTableSpaceType(aParseTree->TBSAttr.mType)
                        != ID_TRUE,
                        ERR_CANNOT_CREATE_TEMPORARY_TABLE_IN_NONVOLATILE_TBS );
    }
    else
    {
        // temporary table���� on commit option�� ����� �� �ִ�.
        if ( aParseTree->temporaryOption != NULL )
        {
            sqlInfo.setSourceInfo( aStatement,
                                   & aParseTree->temporaryOption->temporaryPos );
            IDE_RAISE( ERR_INVALID_TABLE_OPTION );
        }
        else
        {
            // Nothing to do.
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_CANNOT_CREATE_TEMPORARY_TABLE_IN_NONVOLATILE_TBS)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDB_CANNOT_CREATE_TEMPORARY_TABLE_IN_NONVOLATILE_TBS));
    }
    IDE_EXCEPTION(ERR_INVALID_TABLE_OPTION)
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDB_INVALID_TABLE_OPTION,
                                sqlInfo.getErrMessage()) );
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
