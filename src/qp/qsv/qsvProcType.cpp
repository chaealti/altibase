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
 * $Id: qsvProcType.cpp 89187 2020-11-09 05:19:18Z hykim $
 **********************************************************************/

#include <idl.h>
#include <ide.h>
#include <qcmTableInfo.h>
#include <qcuSqlSourceInfo.h>
#include <qsv.h>
#include <qsvProcType.h>
#include <qsvProcStmts.h>
#include <qsvPkgStmts.h>
#include <qsvProcVar.h>
#include <qcmSynonym.h>
#include <qdpPrivilege.h>
#include <qsxRelatedProc.h>
#include <qdpRole.h>
#include <qsxUtil.h>

// BUG-44667 Support STANDARD package.
qcNamePosition gSysName      = {(SChar*)"SYS"     , 0, 3};
qcNamePosition gStandardName = {(SChar*)"STANDARD", 0, 8};

IDE_RC qsvProcType::validateRecordTypeDeclare( qcStatement    * aStatement,
                                               qsTypes        * aType,
                                               qcNamePosition * aTypeName,
                                               idBool           aIsTriggerVariable )
{
/***********************************************************************
 *
 * Description : PROJ-1075 row/record type�� validate
 *               row/record type�� ���id�� �ٸ��� ���� ������
 *               �����ϴ�.
 *
 * Implementation :
 *         (1) Ÿ���� �����Ͽ� rowtype / recordtype module����
 *         (2) ��� �̸� �ο�
 *         (3) ���� Ÿ�������� ����
 *         (4) type column ����
 *         (5) null value ����
 *
 ***********************************************************************/
    
    qcmColumn        * sCurrColumn = NULL;
    void*              sValueTemp;
    void*              sNullValue;
    qcuSqlSourceInfo   sqlInfo;
    mtdModule        * sMtdModule = NULL;
    
    /* ------------------------------------------------
     * (1) Ÿ���� �����Ͽ� rowtype / recordtype module����
     * ----------------------------------------------*/
    // module memory �Ҵ�.
    IDE_TEST( STRUCT_ALLOC( QC_QMP_MEM(aStatement),
                            qtcModule,
                            &aType->typeModule )
              != IDE_SUCCESS );

    // type�� ���� ����� ���� module�� ����.
    // module�����ϴ� �κ��� �������� �κ���.
    if( aType->variableType == QS_ROW_TYPE )
    {
        idlOS::memcpy( aType->typeModule, &qtc::spRowTypeModule, ID_SIZEOF(mtdModule) );
    }
    else if ( aType->variableType == QS_RECORD_TYPE )
    {    
        idlOS::memcpy( aType->typeModule, &qtc::spRecordTypeModule, ID_SIZEOF(mtdModule) );
    }
    else
    {
        // ���ռ� �˻�. �ٸ� type�� ���� ����
        IDE_ERROR( 0 );
    }

    /* ------------------------------------------------
     * (2) ��� �̸� �ο�
     * ----------------------------------------------*/
    // module name ���� �Ҵ�
    IDE_TEST( STRUCT_ALLOC( QC_QMP_MEM(aStatement),
                            mtcName,
                            &aType->typeModule->module.names )
              != IDE_SUCCESS );
    
    aType->typeModule->module.names->next = NULL;
    aType->typeModule->module.names->length = idlOS::strlen( aType->name );
    aType->typeModule->module.names->string = (void*)aType->name;

    /* ------------------------------------------------
     * (3) ���� Ÿ�������� ����
     * ----------------------------------------------*/
    // qtcModule�� Ȯ���ؾ� �� �� ����.
    aType->typeModule->typeInfo = aType;
    

    /* ------------------------------------------------
     * (4) type column ����
     * ----------------------------------------------*/
    // BUG-36772
    // ���ռ� �˻縦 �տ��� �߱� ������, rowtype �ƴϸ� record type �̴�.
    if( aType->variableType == QS_ROW_TYPE )
    {
        IDE_TEST( makeRowTypeColumn( aType,
                                     aIsTriggerVariable )
                  != IDE_SUCCESS );
    }
    else
    {
        IDE_TEST( makeRecordTypeColumn( aStatement,
                                        aType )
                  != IDE_SUCCESS );
    }

    /* ------------------------------------------------
     * (5) null value ����
     * ----------------------------------------------*/
    IDE_TEST( QC_QMP_MEM(aStatement)->alloc( aType->typeSize,
                                             (void**)&sNullValue )
              != IDE_SUCCESS );

    // ������ �÷��� �����Ǵ� null�� �����Ѵ�.
    for( sCurrColumn = aType->columns;
         sCurrColumn != NULL;
         sCurrColumn = sCurrColumn->next )
    {
        sValueTemp = (void*)mtc::value( sCurrColumn->basicInfo,
                                        sNullValue,
                                        MTD_OFFSET_USE );

        sMtdModule = (mtdModule*)sCurrColumn->basicInfo->module;

        // ���� type�� ���� column type�� ���� ���ϵ��� �Ѵ�.
        // ex) TYPE REC_TYPE1 IS RECORD (C1 REC_TYPE1);
        IDE_TEST_RAISE( ((qtcModule*)sMtdModule)->typeInfo == aType,
                        ERR_INVALID_DATATYPE );

        // PROJ-1904 Extend UDT
        if ( sMtdModule->id == MTD_ASSOCIATIVE_ARRAY_ID )
        {
            aType->flag |= QTC_UD_TYPE_HAS_ARRAY_TRUE;
        }
        else if ( (sMtdModule->id == MTD_ROWTYPE_ID) ||
                  (sMtdModule->id == MTD_RECORDTYPE_ID) )
        {
            aType->flag |= ((((qtcModule*)sMtdModule)->typeInfo)->flag &
                            QTC_UD_TYPE_HAS_ARRAY_MASK);

            IDE_TEST( qsxUtil::initRecordVar( aStatement,
                                              sCurrColumn->basicInfo,
                                              sValueTemp,
                                              ID_TRUE )
                      != IDE_SUCCESS );
        }
        else
        {
            // initialize primitive, row, record type column.
            sMtdModule->null( sCurrColumn->basicInfo,
                           sValueTemp );
        }
    }

    aType->typeModule->module.staticNull = sNullValue;
    
    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_INVALID_DATATYPE);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDB_CREATE_DISABLE_DATA_TYPE));
    }
    IDE_EXCEPTION_END;

    // BUG-38883 print error position
    // trigger variable(new/old row)�� �ش� variable�� validate�� ��
    // error position�� �����մϴ�.
    if ( ( ideHasErrorPosition() == ID_FALSE ) &&
         ( aIsTriggerVariable == ID_FALSE ) )
    {
        sqlInfo.setSourceInfo( aStatement,
                               aTypeName );

        // set original error code.
        qsxEnv::setErrorCode( QC_QSX_ENV(aStatement) );

        (void)sqlInfo.initWithBeforeMessage(
            QC_QME_MEM(aStatement) );

        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QSX_SQLTEXT_WRAPPER,
                            sqlInfo.getBeforeErrMessage(),
                            sqlInfo.getErrMessage()));
        (void)sqlInfo.fini();

        // set sophisticated error message.
        qsxEnv::setErrorMessage( QC_QSX_ENV(aStatement) );
    }
    else
    {
        // Nothing to do.
    }

    return IDE_FAILURE;

}

IDE_RC qsvProcType::validateArrTypeDeclare( qcStatement    * aStatement,
                                            qsTypes        * aType,
                                            qcNamePosition * aTypeName )
{
/***********************************************************************
 *
 * Description : PROJ-1075 associative array type�� declare
 *               array type�� index column( integer or varchar),
 *               element column(primitive or recordtype)
 *               ���� �����ȴ�.
 *
 * Implementation :
 *         (1) arrtype module ����
 *         (2) ��� �̸� �ο�
 *         (3) ���� Ÿ�������� ����
 *         (4) type column ����
 *         (4.1) type�� primitive�� ��� �Ľ̴ܰ迡�� ������ �Ȱ� �״�� �̿�
 *         (4.2) type�� recordtype�� ��� Ÿ�԰˻��Ͽ� column�� ����
 *
 ***********************************************************************/    

    qcmColumn        * sIdxColumn;
    qcmColumn        * sRowColumn;
    qsTypes          * sNestedType;
    qcuSqlSourceInfo   sqlInfo;

    /* ------------------------------------------------
     * (1) arrtype module ����
     * ----------------------------------------------*/
    // module memory �Ҵ�.
    IDE_TEST( STRUCT_ALLOC( QC_QMP_MEM(aStatement),
                            qtcModule,
                            &aType->typeModule )
              != IDE_SUCCESS );

    idlOS::memcpy( aType->typeModule, &qtc::spArrTypeModule, ID_SIZEOF(mtdModule) );

    aType->flag |= QTC_UD_TYPE_HAS_ARRAY_TRUE;

    /* ------------------------------------------------
     * (2) ��� �̸� �ο�
     * ----------------------------------------------*/
    // module name ���� �Ҵ�
    IDE_TEST( STRUCT_ALLOC( QC_QMP_MEM(aStatement),
                            mtcName,
                            &aType->typeModule->module.names )
              != IDE_SUCCESS );
    
    aType->typeModule->module.names->next = NULL;
    aType->typeModule->module.names->length = idlOS::strlen( aType->name );
    aType->typeModule->module.names->string = (void*)aType->name;

    /* ------------------------------------------------
     * (3) ���� Ÿ�������� ����
     * ----------------------------------------------*/
    // qtcModule�� Ȯ���ؾ� �� �� ����.
    aType->typeModule->typeInfo = aType;

    /* ------------------------------------------------
     * (4) type column ����
     * ----------------------------------------------*/
    // arr type�� index column �ϳ��� �������� row column( column 2�� )
    // index column�� integer/varchar
    // row column�� primitive/record
    // row column�� record�� ��� �˻��� �;� ��.

    // ���ռ� �˻�. column�� �� 2���� �־�� ��.
    IDE_DASSERT( aType->columns != NULL );
    IDE_DASSERT( aType->columns->next != NULL );
    IDE_DASSERT( aType->columns->next->next == NULL );
    
    sIdxColumn = aType->columns;
    sRowColumn = aType->columns->next;

    // index column�� validation
    if( sIdxColumn->basicInfo->module->id == MTD_INTEGER_ID )
    {
        // Nothing to do.
    }
    else if( sIdxColumn->basicInfo->module->id == MTD_VARCHAR_ID )
    {
        // precision�� index��� ũ�⸦ ������ �������� ��.
    }
    else
    {
        // ����. index column�� varchar, integer�� ���
        IDE_RAISE( ERR_NOT_SUPPORTED_INDEX_DATATYPE );
    }

    // index column�� offset�� �׻� 0�� ��.
    sIdxColumn->basicInfo->column.offset = 0;

    if( sRowColumn->basicInfo == NULL )
    {
        // type�� �������� �ʾ���.(record type)
        IDE_TEST( checkTypes( aStatement,
                              (qsVariableItems*)aType,
                              &sRowColumn->userNamePos,
                              &sRowColumn->tableNamePos,
                              &sRowColumn->namePos,
                              &sNestedType )
                  != IDE_SUCCESS );

        // mtcColumn�� ����.
        IDE_TEST( STRUCT_ALLOC( QC_QMP_MEM(aStatement),
                                mtcColumn,
                                &sRowColumn->basicInfo )
                  != IDE_SUCCESS );

        IDE_TEST( mtc::initializeColumn(
                      sRowColumn->basicInfo,
                      (mtdModule*)sNestedType->typeModule,
                      1,
                      sNestedType->typeSize,
                      0 )
                  != IDE_SUCCESS );
    }
    else
    {
        // ���� �Ұ��� Ÿ���� ������� ����
        if ( ( sRowColumn->basicInfo->module->flag & MTD_PSM_TYPE_MASK ) == MTD_PSM_TYPE_DISABLE )
        {
            IDE_RAISE(ERR_INVALID_DATATYPE);
        }
        else
        {
            // Nothing to do.
        }
    }
    
    // row column�� offset�� 0��.
    // template�� ������ �Ǿ�� �ϹǷ�.
    sRowColumn->basicInfo->column.offset = 0;

    // type size, column count ����.
    // type size�� 0���� �����ص� �������.(array type�� fixedũ����.)
    // columnCount�� ������2��.(index column����)
    aType->typeSize = 0;
    aType->columnCount = 2;
        
    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_INVALID_DATATYPE);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDB_CREATE_DISABLE_DATA_TYPE));
    }
    IDE_EXCEPTION(ERR_NOT_SUPPORTED_INDEX_DATATYPE);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QSX_UNSUPPORTED_ARRAY_INDEX_TYPE));
    }
    IDE_EXCEPTION_END;

    // BUG-38883 print error position
    if ( ideHasErrorPosition() == ID_FALSE )
    {
        sqlInfo.setSourceInfo( aStatement,
                               aTypeName );

        // set original error code.
        qsxEnv::setErrorCode( QC_QSX_ENV(aStatement) );

        (void)sqlInfo.initWithBeforeMessage(
            QC_QME_MEM(aStatement) );

        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QSX_SQLTEXT_WRAPPER,
                            sqlInfo.getBeforeErrMessage(),
                            sqlInfo.getErrMessage()));
        (void)sqlInfo.fini();

        // set sophisticated error message.
        qsxEnv::setErrorMessage( QC_QSX_ENV(aStatement) );
    }
    else
    {
        // Nothing to do.
    }

    return IDE_FAILURE;

}

IDE_RC qsvProcType::validateRefCurTypeDeclare( qcStatement    * aStatement,
                                               qsTypes        * aType,
                                               qcNamePosition * aTypeName )
{
/***********************************************************************
 *
 * Description : PROJ-1386 ref cursor type�� declare
 *               ref cursor Ÿ���� �ܼ��� statement�� ��ũ�̴�.
 *
 * Implementation :
 *         (1) ref cursor type module ����
 *         (2) ��� �̸� �ο�
 *         (3) ���� Ÿ�������� ����
 *
 ***********************************************************************/    
    
    qcuSqlSourceInfo   sqlInfo;

    /* ------------------------------------------------
     * (1) ref cursor type module ����
     * ----------------------------------------------*/
    // module memory �Ҵ�.
    IDE_TEST( STRUCT_ALLOC( QC_QMP_MEM(aStatement),
                            qtcModule,
                            &aType->typeModule )
              != IDE_SUCCESS );

    idlOS::memcpy( aType->typeModule, &qtc::spRefCurTypeModule, ID_SIZEOF(mtdModule) );

    /* ------------------------------------------------
     * (2) ��� �̸� �ο�
     * ----------------------------------------------*/
    // module name ���� �Ҵ�
    IDE_TEST( STRUCT_ALLOC( QC_QMP_MEM(aStatement),
                            mtcName,
                            &aType->typeModule->module.names )
              != IDE_SUCCESS );
    
    aType->typeModule->module.names->next = NULL;
    aType->typeModule->module.names->length = idlOS::strlen( aType->name );
    aType->typeModule->module.names->string = (void*)aType->name;

    /* ------------------------------------------------
     * (3) ���� Ÿ�������� ����
     * ----------------------------------------------*/
    // qtcModule�� Ȯ���ؾ� �� �� ����.
    aType->typeModule->typeInfo = aType;
  
    // type size, column count ����.
    // type size��
    // columnCount�� ������0��.
    aType->typeSize = 0;
    aType->columnCount = 0;
        
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    // BUG-38883 print error position
    if ( ideHasErrorPosition() == ID_FALSE )
    {
        sqlInfo.setSourceInfo( aStatement,
                               aTypeName );

        // set original error code.
        qsxEnv::setErrorCode( QC_QSX_ENV(aStatement) );

        (void)sqlInfo.initWithBeforeMessage(
            QC_QME_MEM(aStatement) );

        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QSX_SQLTEXT_WRAPPER,
                            sqlInfo.getBeforeErrMessage(),
                            sqlInfo.getErrMessage()));
        (void)sqlInfo.fini();

        // set sophisticated error message.
        qsxEnv::setErrorMessage( QC_QSX_ENV(aStatement) );
    }
    else
    {
        // Nothing to do.
    }

    return IDE_FAILURE;
}


IDE_RC qsvProcType::validateTypeDeclare( qcStatement    * aStatement,
                                         qsTypes        * aType,
                                         qcNamePosition * aTypeName,
                                         idBool           aIsTriggerVariable )
{
/***********************************************************************
 *
 * Description : PROJ-1075 type declare���� validate
 *
 * Implementation :
 *        type�� ������ ������ ����.
 *        (1) rowtype (rowtype���� ������ type�� ����)
 *        (2) recordtype (type�������� ����)
 *        (3) associative array type (type �������� ����)
 *        (4) ref cursor type ( type �������� ����) - PROJ-1386
 *      * rowtype�� recordtype�� ���� ��ƾ�� Ž.
 *
 ***********************************************************************/

    // type�� ���� ����� ���� module�� ����.
    // module�����ϴ� �κ��� �������� �κ���.

    switch( aType->variableType )
    {
        case QS_ROW_TYPE:
        case QS_RECORD_TYPE:
        {
            IDE_TEST( validateRecordTypeDeclare( aStatement,
                                                 aType,
                                                 aTypeName,
                                                 aIsTriggerVariable )
                      != IDE_SUCCESS );
        }
        break;
        case QS_ASSOCIATIVE_ARRAY_TYPE:
        {
            IDE_TEST( validateArrTypeDeclare( aStatement,
                                              aType,
                                              aTypeName )
                      != IDE_SUCCESS );
        }
        break;
        case QS_REF_CURSOR_TYPE:
        {
            IDE_TEST( validateRefCurTypeDeclare( aStatement,
                                                 aType,
                                                 aTypeName )
                      != IDE_SUCCESS );
        }
        break;
        
        default:
        {
            // ���ռ� �˻�. �ٸ� type�� ���� ������ ����
            IDE_DASSERT(0);
        }
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

IDE_RC qsvProcType::checkTypes( qcStatement     * aStatement,
                                qsVariableItems * aVariableItem,
                                qcNamePosition  * aUserName,
                                qcNamePosition  * aLabelName,
                                qcNamePosition  * aTypeName,
                                qsTypes        ** aType )
{
/***********************************************************************
 *
 * Description : PROJ-1075 type check
 *
 * Implementation :
 *            ����� ���� type�� ������ ���� �������� �� �� ����.
 *             (1) type_name
 *             (2) label_name.type_name
 *             (3) package_name.type_name
 *             (4) typeset_name.type_name
 *             (5) user_name.package_name.type_name
 *             (6) user_name.typeset_name.type_name
 *
 ***********************************************************************/

    qsLabels          * sLabel;
    qsAllVariables    * sCurrVar;
    idBool              sIsFound = ID_FALSE;
    qcuSqlSourceInfo    sqlInfo;
    qsPkgParseTree    * sPkgParseTree = NULL;

    sPkgParseTree = aStatement->spvEnv->createPkg;
    
    if( QC_IS_NULL_NAME((*aUserName)) == ID_TRUE )
    {
        // type_name
        if( QC_IS_NULL_NAME((*aLabelName)) == ID_TRUE )
        {
            // block�� bottom-up���� ���󰡸� type�� �˻��ϸ鼭
            // ���� �̸��� ������ type��ȯ.
            // �ش� ����
            // (1) single procedure
            // (2) subprogram in package
            for( sCurrVar = aStatement->spvEnv->allVariables,
                     sIsFound = ID_FALSE;
                 (sCurrVar != NULL) && (sIsFound == ID_FALSE);
                 sCurrVar = sCurrVar->next )
            {
                IDE_TEST( searchLocalTypes( aStatement,
                                            sCurrVar->variableItems,
                                            aVariableItem,
                                            aTypeName,
                                            &sIsFound,
                                            aType )
                          != IDE_SUCCESS );            
            }

            /* package local�� type �˻� */
            if( (sIsFound == ID_FALSE) && (sPkgParseTree != NULL) )
            {
                IDE_TEST( searchPkgLocalTypes( aStatement,
                                               aVariableItem,
                                               aTypeName,
                                               &sIsFound,
                                               aType )
                          != IDE_SUCCESS );
            }
            else
            {
                // Nothing to do.
            }
        }
        // label_name.type_name or package_name.type_name or typeset_name.type_name
        else
        {
            // ���ռ� �˻�. Ÿ���̸��� �� �־�� �Ѵ�.
            IDE_DASSERT( QC_IS_NULL_NAME((*aTypeName)) == ID_FALSE );
            
            // block�� bottom-up���� ���󰡸鼭 ���� label�� �ִ��� �˻�
            // ���� label�̶�� type�� �ִ��� �˻�
            // ���� �̸��� ������ type ��ȯ.
            for( sCurrVar = aStatement->spvEnv->allVariables,
                     sIsFound = ID_FALSE;
                 sCurrVar != NULL &&
                     sIsFound == ID_FALSE;
                 sCurrVar = sCurrVar->next )
            {
                for( sLabel = sCurrVar->labels;
                     (sLabel != NULL) && (sIsFound == ID_FALSE);
                     sLabel = sLabel->next )
                {
                    if (idlOS::strMatch(
                        sLabel->namePos.stmtText + sLabel->namePos.offset,
                        sLabel->namePos.size,
                        aLabelName->stmtText + aLabelName->offset,
                        aLabelName->size) == 0)
                    {           
                        IDE_TEST( searchLocalTypes( aStatement,
                                                    sCurrVar->variableItems,
                                                    aVariableItem,
                                                    aTypeName,
                                                    &sIsFound,
                                                    aType )
                                  != IDE_SUCCESS );
                    }
                    else
                    {
                        // Nothing to do.
                    }
                }
            }

            /* BUG-41720 
               ���� package�� ����� array type�� return value�� ��� �����ؾ� �Ѵ�.
               myPackage_name.type_name 
               aLabelName�� myPackage �̸��� �� �� �ִ�. */
            if ( (sPkgParseTree != NULL) &&
                 (sIsFound == ID_FALSE ) )
            {
                if ( QC_IS_NAME_MATCHED( sPkgParseTree->pkgNamePos,
                                         (*aLabelName) ) )
                {
                    IDE_TEST( searchPkgLocalTypes( aStatement,
                                                   aVariableItem,
                                                   aTypeName,
                                                   &sIsFound,
                                                   aType )
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
            
            if( sIsFound == ID_FALSE )
            {
                // �ش� procedure���� ������ package ������ �˻�.
                IDE_TEST( checkPkgTypes( aStatement,
                                         aUserName,
                                         aLabelName,
                                         aTypeName,
                                         &sIsFound,
                                         aType )
                          != IDE_SUCCESS );

                if( sIsFound == ID_FALSE)
                {
                    // �ش� procedure���� ������ typeset������ �˻�.
                    IDE_TEST( searchTypesFromTypeSet( aStatement,
                                                      aUserName,
                                                      aLabelName,
                                                      aTypeName,
                                                      &sIsFound,
                                                      aType )
                              != IDE_SUCCESS );
                }
                else
                {
                    // Nothint to do.
                }
            }
            else
            {
                // Nothing to do.
            }
        }    
    }
    else
    {
        /* BUG-41847
           package local�� �ִ� type�� ã�� �� �־�� �մϴ�. */
        if ( (sPkgParseTree != NULL) &&
             (sIsFound == ID_FALSE ) )
        {
            if ( QC_IS_NAME_MATCHED( sPkgParseTree->pkgNamePos,
                                     (*aLabelName) ) )
            {
                IDE_TEST( searchPkgLocalTypes( aStatement,
                                               aVariableItem,
                                               aTypeName,
                                               &sIsFound,
                                               aType )
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

        if ( sIsFound == ID_FALSE )
        {
            // user_name.package_name.type_name
            IDE_TEST( checkPkgTypes( aStatement,
                                     aUserName,
                                     aLabelName,
                                     aTypeName,
                                     &sIsFound,
                                     aType )
                      != IDE_SUCCESS );

            if( sIsFound == ID_FALSE )
            {
                // user_name.typeset_name.type_name
                IDE_TEST( searchTypesFromTypeSet( aStatement,
                                                  aUserName,
                                                  aLabelName,
                                                  aTypeName,
                                                  &sIsFound,
                                                  aType )
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

    if( sIsFound == ID_FALSE )
    {
        // BUG-44667 Support STANDARD package.
        if( (QC_IS_NULL_NAME((*aUserName))  == ID_TRUE) &&
            (QC_IS_NULL_NAME((*aLabelName)) == ID_TRUE) )
        {
            // ��� ȣ�� ��������� ���Ŀ��� ��� ȣ������ �ʴ´�.
            IDE_TEST( checkTypes( aStatement,
                                  aVariableItem,
                                  &gSysName,      // aUserName  (= "SYS")
                                  &gStandardName, // aLabelName (= "STANDARD")
                                  aTypeName,
                                  aType )
                      != IDE_SUCCESS );
        }
        else
        {
            sqlInfo.setSourceInfo(
                aStatement,
                aTypeName );
            IDE_RAISE(ERR_NOT_FOUND_VAR);
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_NOT_FOUND_VAR);
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QSV_NOT_EXIST_VARIABLE_NAME_SQLTEXT,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

IDE_RC qsvProcType::searchLocalTypes( qcStatement     * aStatement,
                                      qsVariableItems * aLocalVariableItems,
                                      qsVariableItems * aVariableItem,
                                      qcNamePosition  * aTypeName,
                                      idBool          * aIsFound,
                                      qsTypes        ** aType )
{
/***********************************************************************
 *
 * Description : PROJ-1075 local type�� �˻�
 *
 * Implementation :
 *         (1) variableItems�� �˻��ϸ鼭 itemtype��
 *             QS_TYPE�� ���� �˻�
 *         (2) �̸� ���Ͽ� ������ *aIsFound�� ID_TRUE�� �����ϰ�
 *             ã�� qsTypes�� ����.
 *
 ***********************************************************************/
   
    qsVariableItems  * sLocalVariableItem;
    qsTypes          * sLocalType;

    *aIsFound = ID_FALSE;

    // type������ ������ �ٲ�� �ȵǹǷ�
    // ����ο� �ڱ� �ڽ��� �ö������� �˻��Ѵ�.
    for( sLocalVariableItem = aLocalVariableItems;
         (sLocalVariableItem != NULL) &&
         (*aIsFound == ID_FALSE) &&
         (sLocalVariableItem != aVariableItem) &&
         // BUG-41830 Prevent inappropriate access to user defined types.
         (sLocalVariableItem != aStatement->spvEnv->currDeclItem);
         sLocalVariableItem = sLocalVariableItem->next )
    {
        if( sLocalVariableItem->itemType == QS_TYPE )
        {
            sLocalType = (qsTypes*)sLocalVariableItem;

            if( idlOS::strMatch(
                sLocalType->name,
                idlOS::strlen( sLocalType->name ),
                aTypeName->stmtText + aTypeName->offset,
                aTypeName->size) == 0 )
            {
                *aIsFound = ID_TRUE;
                *aType    = (qsTypes*)sLocalVariableItem;
                break;
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

    return IDE_SUCCESS;
    
}

IDE_RC qsvProcType::searchTypesFromTypeSet( qcStatement     * aStatement,
                                            qcNamePosition  * aUserName,
                                            qcNamePosition  * aLabelName,
                                            qcNamePosition  * aTypeName,
                                            idBool          * aIsFound,
                                            qsTypes        ** aType )
{
    qsOID                 sProcOID;
    UInt                  sProcUserID;
    qsxProcInfo         * sProcInfo;
    idBool                sExists = ID_FALSE;
    qcmSynonymInfo        sSynonymInfo;    

    // ���ռ� �˻�. spvEnv�� �ݵ�� �����ؾ� �Ѵ�.
    IDE_DASSERT( aStatement->spvEnv != NULL );

    IDE_TEST( qcmSynonym::resolvePSM( aStatement,
                                      *aUserName,
                                      *aLabelName,
                                      &sProcOID,
                                      &sProcUserID,
                                      &sExists,
                                      &sSynonymInfo )
              != IDE_SUCCESS);

    if( sExists == ID_TRUE )
    {
        // synonym���� �����Ǵ� proc�� ���
        IDE_TEST( qsvProcStmts::makeProcSynonymList(
                      aStatement,
                      & sSynonymInfo,
                      *aUserName,
                      *aLabelName,
                      sProcOID )
                  != IDE_SUCCESS );
  
        IDE_TEST( qsvProcStmts::makeRelatedObjects(
                      aStatement,
                      aUserName,
                      aLabelName,
                      & sSynonymInfo,
                      0,
                      QS_TYPESET )
                  != IDE_SUCCESS );
        
        IDE_TEST( qsxRelatedProc::prepareRelatedPlanTree(
                      aStatement,
                      sProcOID,
                      QS_TYPESET,
                      &(aStatement->spvEnv->procPlanList) )
                  != IDE_SUCCESS );

        IDE_TEST( qsxProc::getProcInfo( sProcOID,
                                        &sProcInfo )
                  != IDE_SUCCESS );

        /* BUG-45164 */
        IDE_TEST_RAISE( sProcInfo->isValid != ID_TRUE, err_object_invalid );

        IDE_TEST( qdpRole::checkDMLExecutePSMPriv( aStatement,
                                                   sProcInfo->planTree->userID,
                                                   sProcInfo->privilegeCount,
                                                   sProcInfo->granteeID,
                                                   ID_FALSE,
                                                   NULL,
                                                   NULL )
                  != IDE_SUCCESS );
        
        if( sProcInfo->planTree->objType == QS_TYPESET )
        {
            // ���ռ� �˻�. typeset�� block�� �����ؾ� �Ѵ�.
            IDE_DASSERT( sProcInfo->planTree->block != NULL );
                        
            IDE_TEST( searchLocalTypes( aStatement,
                                        sProcInfo->planTree->block->variableItems,
                                        NULL,
                                        aTypeName,
                                        aIsFound,
                                        aType )
                      != IDE_SUCCESS );                            
        }
        else
        {
            *aIsFound = ID_FALSE;
        }
    }
    else
    {
        *aIsFound = ID_FALSE;
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION( err_object_invalid );   /* BUG-45164 */ 
    {
        IDE_SET( ideSetErrorCode(qpERR_ABORT_QSX_PLAN_INVALID) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}


IDE_RC qsvProcType::makeRowType( qcStatement     * aStatement,
                                 qcmTableInfo    * aTableInfo,
                                 qcNamePosition  * aTypeName,
                                 idBool            aTriggerVariable,
                                 qsTypes        ** aType )
{
/***********************************************************************
 *
 * Description :
 *    rowtype�� ����.
 *
 * Implementation :
 *    1. tableInfo�� �̿��Ͽ� �Ľ��� �ܰ��� qsTypes�ϳ� ����
 *    2. qsTypes�� validation
 *    
 ***********************************************************************/

    qsTypes   * sType;
    qcmColumn * sCurrColumn;
    qcmColumn * sTableColumn;

    // type�� ���� �ϳ� ����.
    // rowtype�� ������ type reference������ ����.
    IDE_TEST( STRUCT_ALLOC( QC_QMP_MEM(aStatement),
                            qsTypes,
                            &sType )
              != IDE_SUCCESS );

    sType->common.itemType = QS_TYPE;
    sType->common.table    = ID_USHORT_MAX;
    sType->common.column   = ID_USHORT_MAX;
    sType->common.objectID = QS_EMPTY_OID;
    sType->common.next     = NULL;
    SET_POSITION( sType->common.name,
                  (*aTypeName) );

    QC_STR_COPY( sType->name, (*aTypeName) );
    sType->variableType = QS_ROW_TYPE;
    sType->typeModule   = NULL;
    sType->typeSize     = 0;
    sType->tableInfo    = aTableInfo;
    sType->flag         = QTC_UD_TYPE_HAS_ARRAY_FALSE;

    // rowtype�� ��� target�̸��� ������ ����.
    // cursor rowtype�� ��� ���� ���� ����.
    for( sCurrColumn = aTableInfo->columns;
         sCurrColumn != NULL;
         sCurrColumn = sCurrColumn->next )
    {
        IDE_TEST_RAISE( sCurrColumn->name[0] == 0,
                        ERR_NOT_EXIST_ALIAS_OF_TARGET );
    }

    // qcmColumn�� �����Ͽ� typeInfo�� ����.
    IDE_TEST( qcm::copyQcmColumns( QC_QMP_MEM(aStatement),
                                   aTableInfo->columns,
                                   &sType->columns,
                                   aTableInfo->columnCount )
              != IDE_SUCCESS );

    // PROJ-2002 Column Security
    for ( sCurrColumn = sType->columns;
          sCurrColumn != NULL;
          sCurrColumn = sCurrColumn->next )
    {
        if ( (sCurrColumn->basicInfo->module->flag & MTD_ENCRYPT_TYPE_MASK )
             == MTD_ENCRYPT_TYPE_TRUE )
        {
            IDE_TEST( qtc::changeColumn4Decrypt(
                          sCurrColumn->basicInfo )
                      != IDE_SUCCESS );
        }
        else
        {
            // Nothing to do.
        }
    }

    /* PROJ-1530 PSM/Trigger���� LOB ����Ÿ Ÿ�� ����
     * LOB Column�� LOB Value�� ��ȯ�Ѵ�.
     */
    for ( sCurrColumn = sType->columns, sTableColumn = aTableInfo->columns;
          (sCurrColumn != NULL) && (sTableColumn != NULL);
          sCurrColumn = sCurrColumn->next, sTableColumn = sTableColumn->next )
    {
        if ( ( sTableColumn->basicInfo->type.dataTypeId == MTD_BLOB_ID ) ||
             ( sTableColumn->basicInfo->type.dataTypeId == MTD_CLOB_ID ) )
        {
            IDE_TEST( mtc::initializeColumn( sCurrColumn->basicInfo,
                                             sTableColumn->basicInfo->type.dataTypeId,
                                             0,
                                             0,
                                             0 )
                      != IDE_SUCCESS );
        }
        /* BUG-44005 PSM���� LOB Column�� �����ϴ� Cursor�� Rowtype�� �����ؾ� �մϴ�. */
        else if ( sTableColumn->basicInfo->type.dataTypeId == MTD_BLOB_LOCATOR_ID )
        {
            IDE_TEST( mtc::initializeColumn( sCurrColumn->basicInfo,
                                             MTD_BLOB_ID,
                                             0,
                                             0,
                                             0 )
                      != IDE_SUCCESS );
        }
        /* BUG-44005 PSM���� LOB Column�� �����ϴ� Cursor�� Rowtype�� �����ؾ� �մϴ�. */
        else if ( sTableColumn->basicInfo->type.dataTypeId == MTD_CLOB_LOCATOR_ID )
        {
            IDE_TEST( mtc::initializeColumn( sCurrColumn->basicInfo,
                                             MTD_CLOB_ID,
                                             0,
                                             0,
                                             0 )
                      != IDE_SUCCESS );
        }
        else
        {
            /* Nothing to do */
        }
    }

    // type validate
    IDE_TEST( validateTypeDeclare( aStatement,
                                   sType,
                                   aTypeName,
                                   aTriggerVariable )
              != IDE_SUCCESS );

    *aType = sType;
    
    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_NOT_EXIST_ALIAS_OF_TARGET);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDB_NOT_EXISTS_ALIAS));
    }    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

IDE_RC qsvProcType::copyColumn( qcStatement * aStatement,
                                mtcColumn   * aOriColumn,
                                mtcColumn   * aColumn,
                                mtdModule  ** aModule )
{
    qsTypes * sOriType;
    qsTypes * sType;

    if( aOriColumn->type.dataTypeId == MTD_ROWTYPE_ID )
    {
        sOriType = ((qtcModule *)aOriColumn->module)->typeInfo;

        IDE_TEST( qsvProcType::copyQsType( aStatement,
                                           sOriType,
                                           &sType ) != IDE_SUCCESS );

        mtc::copyColumn( aColumn, aOriColumn );

        aColumn->module = (mtdModule *)sType->typeModule;
    }
    else
    {
        mtc::copyColumn( aColumn, aOriColumn );
    }

    *aModule = (mtdModule *)aColumn->module;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qsvProcType::copyQsType( qcStatement     * aStatement,
                                qsTypes         * aOriType,
                                qsTypes        ** aType )
{
    SChar     * sTypeName;
    qsTypes   * sType;
    qcmColumn * sCurrColumn;
    qcmColumn * sOriColumn;

    sTypeName = aOriType->name;

    // type�� ���� �ϳ� ����.
    // rowtype�� ������ type reference������ ����.
    IDE_TEST( STRUCT_ALLOC( QC_QMP_MEM(aStatement),
                            qsTypes,
                            &sType )
              != IDE_SUCCESS );

    sType->common.itemType = QS_TYPE;
    sType->common.table    = ID_USHORT_MAX;
    sType->common.column   = ID_USHORT_MAX;
    sType->common.next     = NULL;
    SET_EMPTY_POSITION( sType->common.name );

    idlOS::memcpy( sType->name,
                   sTypeName,
                   idlOS::strlen( sTypeName ) + 1 );
    sType->variableType = QS_ROW_TYPE;
    sType->typeModule   = NULL;
    sType->typeSize     = 0;
    sType->tableInfo    = NULL;

    // rowtype�� ��� target�̸��� ������ ����.
    // cursor rowtype�� ��� ���� ���� ����.
    for( sCurrColumn = aOriType->columns;
         sCurrColumn != NULL;
         sCurrColumn = sCurrColumn->next )
    {
        IDE_TEST_RAISE( sCurrColumn->name[0] == 0,
                        ERR_NOT_EXIST_ALIAS_OF_TARGET );
    }

    // qcmColumn�� �����Ͽ� typeInfo�� ����.
    IDE_TEST( qcm::copyQcmColumns( QC_QMP_MEM(aStatement),
                                   aOriType->columns,
                                   &sType->columns,
                                   aOriType->columnCount )
              != IDE_SUCCESS );    

    // PROJ-2002 Column Security
    for ( sCurrColumn = sType->columns;
          sCurrColumn != NULL;
          sCurrColumn = sCurrColumn->next )
    {
        if ( (sCurrColumn->basicInfo->module->flag & MTD_ENCRYPT_TYPE_MASK )
             == MTD_ENCRYPT_TYPE_TRUE )
        {
            IDE_TEST( qtc::changeColumn4Decrypt(
                          sCurrColumn->basicInfo )
                      != IDE_SUCCESS );
        }
        else
        {
            // Nothing to do.
        }
    }

    /* PROJ-1530 PSM/Trigger���� LOB ����Ÿ Ÿ�� ����
     * LOB Column�� LOB Value�� ��ȯ�Ѵ�.
     */
    for ( sCurrColumn = sType->columns, sOriColumn = aOriType->columns;
          (sCurrColumn != NULL) && (sOriColumn != NULL);
          sCurrColumn = sCurrColumn->next, sOriColumn = sOriColumn->next )
    {
        if ( ( sOriColumn->basicInfo->type.dataTypeId == MTD_BLOB_ID ) ||
             ( sOriColumn->basicInfo->type.dataTypeId == MTD_CLOB_ID ) )
        {
            IDE_TEST( mtc::initializeColumn( sCurrColumn->basicInfo,
                                             sOriColumn->basicInfo->type.dataTypeId,
                                             0,
                                             0,
                                             0 )
                      != IDE_SUCCESS );
        }
        /* BUG-44005 PSM���� LOB Column�� �����ϴ� Cursor�� Rowtype�� �����ؾ� �մϴ�. */
        else if ( sOriColumn->basicInfo->type.dataTypeId == MTD_BLOB_LOCATOR_ID )
        {
            IDE_TEST( mtc::initializeColumn( sCurrColumn->basicInfo,
                                             MTD_BLOB_ID,
                                             0,
                                             0,
                                             0 )
                      != IDE_SUCCESS );
        }
        /* BUG-44005 PSM���� LOB Column�� �����ϴ� Cursor�� Rowtype�� �����ؾ� �մϴ�. */
        else if ( sOriColumn->basicInfo->type.dataTypeId == MTD_CLOB_LOCATOR_ID )
        {
            IDE_TEST( mtc::initializeColumn( sCurrColumn->basicInfo,
                                             MTD_CLOB_ID,
                                             0,
                                             0,
                                             0 )
                      != IDE_SUCCESS );
        }
        else
        {
            /* Nothing to do */
        }
    }

    // type validate
    IDE_TEST( validateTypeDeclare( aStatement,
                                   sType,
                                   &sType->common.name,
                                   ID_FALSE )
              != IDE_SUCCESS );

    *aType = sType;
    
    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_NOT_EXIST_ALIAS_OF_TARGET);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDB_NOT_EXISTS_ALIAS));
    }    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

// PROJ-1073 Package
IDE_RC qsvProcType::checkPkgTypes( qcStatement     * aStatement,
                                   qcNamePosition  * aUserName,
                                   qcNamePosition  * aTableName,
                                   qcNamePosition  * aTypeName,
                                   idBool          * aIsFound,
                                   qsTypes        ** aType )
{
    qsOID            sPkgOID;
    UInt             sPkgUserID;
    qsxPkgInfo     * sPkgInfo;
    idBool           sExists = ID_FALSE;
    qcmSynonymInfo   sSynonymInfo;

    IDE_TEST( qcmSynonym::resolvePkg( aStatement,
                                      *aUserName,
                                      *aTableName,
                                      &sPkgOID,
                                      &sPkgUserID,
                                      &sExists,
                                      &sSynonymInfo )
                 != IDE_SUCCESS );

    if( sExists == ID_TRUE )
    {
        // synonym���� �����Ǵ� proc�� ���
        IDE_TEST( qsvPkgStmts::makePkgSynonymList(
                          aStatement,
                          &sSynonymInfo,
                          *aUserName,
                          *aTableName,
                          sPkgOID )
                  != IDE_SUCCESS );

        IDE_TEST( qsvPkgStmts::makeRelatedObjects(
                          aStatement,
                          aUserName,
                          aTableName,
                          & sSynonymInfo,
                          0,
                          QS_PKG )
                  != IDE_SUCCESS );

        IDE_TEST( qsxRelatedProc::prepareRelatedPlanTree(
                          aStatement,
                          sPkgOID,
                          QS_PKG,
                          &(aStatement->spvEnv->procPlanList) )
                  != IDE_SUCCESS );

        IDE_TEST( qsxPkg::getPkgInfo( sPkgOID,
                                      &sPkgInfo )
                  != IDE_SUCCESS );

        /* BUG-45164 */
        IDE_TEST_RAISE( sPkgInfo->isValid != ID_TRUE, err_object_invalid );

        IDE_TEST( qdpRole::checkDMLExecutePSMPriv( aStatement,
                                                   sPkgInfo->planTree->userID,
                                                   sPkgInfo->privilegeCount,
                                                   sPkgInfo->granteeID,
                                                   ID_FALSE,
                                                   NULL,
                                                   NULL )
                  != IDE_SUCCESS );

        IDE_TEST( searchTypesFromPkg( sPkgInfo,
                                      aTypeName,
                                      aIsFound,
                                      aType )
                  != IDE_SUCCESS );
    }
    else
    {
        *aIsFound = ID_FALSE;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_object_invalid );   /* BUG-45164 */ 
    {
        IDE_SET( ideSetErrorCode(qpERR_ABORT_QSX_PLAN_INVALID) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qsvProcType::searchTypesFromPkg( qsxPkgInfo      * aPkgInfo,
                                        qcNamePosition  * aTypeName,
                                        idBool          * aIsFound,
                                        qsTypes        ** aType )
{
    qsPkgParseTree  * sPlanTree = aPkgInfo->planTree;
    qsVariableItems * sVariable;
    qsTypes         * sType;

    *aIsFound = ID_FALSE;

    // package spec�̾�� �Ѵ�.
    IDE_DASSERT( aPkgInfo->objType == QS_PKG );

    for( sVariable = sPlanTree->block->variableItems;
         sVariable != NULL;
         sVariable = sVariable->next )
    {
        if( sVariable->itemType == QS_TYPE )
        {
            sType = (qsTypes *)sVariable;
           
            if ( idlOS::strMatch( (*aTypeName).stmtText + (*aTypeName).offset,
                                  (*aTypeName).size,
                                  sType->name,
                                  idlOS::strlen( sType->name ) ) == 0 )
            {
                *aType = (qsTypes *)sVariable;
                *aIsFound = ID_TRUE;
                break;
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

    return IDE_SUCCESS;
    
}

// BUG-36772
IDE_RC qsvProcType::makeRowTypeColumn( qsTypes     * aType,
                                       idBool        aIsTriggerVariable )
{
    qcmColumn        * sLastColumn = NULL;
    qcmColumn        * sCurrColumn = NULL;
    UInt               sOffset;
    UInt               sColumnCount = 0;

    for( sCurrColumn = aType->columns, sOffset = 0;
         sCurrColumn != NULL;
         sCurrColumn = sCurrColumn->next )
    {
        // table�� ���� �Ұ����� ���ڵ�� ���� �Ұ�.
        if ( ( sCurrColumn->basicInfo->module->flag & MTD_PSM_TYPE_MASK ) == MTD_PSM_TYPE_DISABLE )
        {
            IDE_RAISE(ERR_INVALID_DATATYPE);
        }
        else if ( ( (sCurrColumn->flag & QCM_COLUMN_HIDDEN_COLUMN_MASK)
                    == QCM_COLUMN_HIDDEN_COLUMN_TRUE ) &&
                  ( aIsTriggerVariable == ID_FALSE ) )
        {
            /* PROJ-1090 Function-based Index */
            /* rowtype�� ��� hidden column�� �����Ѵ�. */
        }
        else
        {
            // column type�� ������ fixed�ϰ� �ٲٰ�
            // offset, size, column count ���.

            // BUG-38078 TYPE_FIXED�� �����ϴ� ���, Compression �Ӽ��� �����Ѵ�.
            sCurrColumn->basicInfo->column.flag &= ~SMI_COLUMN_TYPE_MASK;
            sCurrColumn->basicInfo->column.flag |= SMI_COLUMN_TYPE_FIXED;

            sCurrColumn->basicInfo->column.flag &= ~SMI_COLUMN_COMPRESSION_MASK;
            sCurrColumn->basicInfo->column.flag |= SMI_COLUMN_COMPRESSION_FALSE;

            sOffset = idlOS::align( sOffset,
                                    sCurrColumn->basicInfo->module->align );
            sCurrColumn->basicInfo->column.offset = sOffset;

            /* BUG-36101 SInt ������ ����� �޸� �Ҵ� ���� */
            IDE_TEST_RAISE( (SLong)sOffset + (SLong)sCurrColumn->basicInfo->column.size >
                            (SLong)ID_SINT_MAX,
                            ERR_PSM_ROW_SIZE_EXCEED_LIMIT );

            sOffset += sCurrColumn->basicInfo->column.size;
            sColumnCount++;

            // BUG-41340
            if ( sLastColumn != NULL )
            {
                sLastColumn->next = sCurrColumn;
            }
            else
            {
                // Nothing to do.
            }
            
            sLastColumn = sCurrColumn;
        }
    }

    /* hidden column�� ���� columns�� ��� ������ ���� �� �ִ�. */
    if( sLastColumn != NULL )
    {
        sLastColumn->next = NULL;
    }
    else
    {
        /* Nothing to do. */
    }

    // typeSize�� ���� offsetũ�⸸ŭ�� �ȴ�.
    aType->typeSize = sOffset;
    aType->columnCount = sColumnCount;

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_INVALID_DATATYPE);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDB_CREATE_DISABLE_DATA_TYPE));
    }
    IDE_EXCEPTION( ERR_PSM_ROW_SIZE_EXCEED_LIMIT );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QSV_PSM_ROW_SIZE_EXCEED_LIMIT,
                                  (SLong)sOffset + (SLong)sCurrColumn->basicInfo->column.size,
                                  ID_SINT_MAX ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qsvProcType::makeRecordTypeColumn( qcStatement     * aStatement,
                                          qsTypes         * aType )
{
    qsVariableItems * sCurrField = NULL;
    qsVariables     * sColumn = NULL;
    UInt              sOffset;
    qcmColumn       * sQcmColumn = NULL;
    qcmColumn       * sPrevColumn = NULL;
    mtcColumn       * sMtcColumn = NULL;
    mtcColumn       * sNewMtcColumn = NULL;
    UInt              sColumnCount = 0;

    for( sCurrField = aType->fields, sOffset = 0 ;
         sCurrField != NULL ;
         sCurrField = sCurrField->next )
    {
        sColumn = ( qsVariables * ) sCurrField;

        IDE_TEST( qsvProcVar::validateLocalVariable(
                aStatement, sColumn )
            != IDE_SUCCESS );

        // 1. qcmColumn �Ҵ�
        IDE_TEST( STRUCT_ALLOC( QC_QMP_MEM(aStatement),
                                qcmColumn,
                                &sQcmColumn )
                  != IDE_SUCCESS );

        // 2. qcmColum �ʱ�ȭ
        QCM_COLUMN_INIT( sQcmColumn );
        SET_EMPTY_POSITION( sQcmColumn->userNamePos );
        SET_EMPTY_POSITION( sQcmColumn->tableNamePos );
        SET_POSITION( sQcmColumn->namePos, sCurrField->name );
        QC_STR_COPY( sQcmColumn->name, sQcmColumn->namePos );
        sQcmColumn->flag = QCM_COLUMN_TYPE_DEFAULT;

        // 3. mtcColum ���� �� �ʱ�ȭ
        sMtcColumn = QTC_STMT_COLUMN( aStatement, sColumn->variableTypeNode );
        // 3-1.mtcColumn ����
        IDE_TEST( STRUCT_ALLOC( QC_QMP_MEM(aStatement),
                                mtcColumn,
                                &sNewMtcColumn )
                  != IDE_SUCCESS );
        // 3-2. mtcColumn ����
        mtc::copyColumn( sNewMtcColumn, sMtcColumn );

        // 4. ���� ����� mtcColumn���� ����
        sNewMtcColumn->flag &= ~MTC_COLUMN_NOTNULL_TRUE;

        // table�� ���� �Ұ����� ���ڵ�� ���� �Ұ�.
        if ( ( sNewMtcColumn->module->flag & MTD_PSM_TYPE_MASK ) == MTD_PSM_TYPE_DISABLE )
        {
            IDE_RAISE(ERR_INVALID_DATATYPE);
        }
        else
        {
            // column type�� ������ fixed�ϰ� �ٲٰ�
            // offset, size, column count ���.

            // BUG-38078 TYPE_FIXED�� �����ϴ� ���, Compression �Ӽ��� �����Ѵ�.
            sNewMtcColumn->column.flag &= ~SMI_COLUMN_TYPE_MASK;
            sNewMtcColumn->column.flag |= SMI_COLUMN_TYPE_FIXED;

            sNewMtcColumn->column.flag &= ~SMI_COLUMN_COMPRESSION_MASK;
            sNewMtcColumn->column.flag |= SMI_COLUMN_COMPRESSION_FALSE;

            sOffset = idlOS::align( sOffset,
                                    sNewMtcColumn->module->align );
            sNewMtcColumn->column.offset = sOffset;

            /* BUG-36101 SInt ������ ����� �޸� �Ҵ� ���� */
            IDE_TEST_RAISE( (SLong)sOffset + (SLong)sNewMtcColumn->column.size >
                            (SLong)ID_SINT_MAX,
                            ERR_PSM_ROW_SIZE_EXCEED_LIMIT );

            sOffset += sNewMtcColumn->column.size;
            sColumnCount++;

            sQcmColumn->basicInfo = sNewMtcColumn;

            // type->columns connect
            if( aType->columns == NULL )
            {
                aType->columns = sQcmColumn;
                sPrevColumn = sQcmColumn;
            }
            else
            {
                sPrevColumn->next = sQcmColumn;
                sPrevColumn = sQcmColumn;
            }
        }
    }

    aType->typeSize = sOffset;
    aType->columnCount = sColumnCount;

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_INVALID_DATATYPE);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDB_CREATE_DISABLE_DATA_TYPE));
    }
    IDE_EXCEPTION( ERR_PSM_ROW_SIZE_EXCEED_LIMIT );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QSV_PSM_ROW_SIZE_EXCEED_LIMIT,
                                  (SLong)sOffset + (SLong)sNewMtcColumn->column.size,
                                  ID_SINT_MAX ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qsvProcType::searchPkgLocalTypes( qcStatement     * aStatement,
                                         qsVariableItems * aVariableItem,
                                         qcNamePosition  * aTypeName,
                                         idBool          * aIsFound,
                                         qsTypes        ** aType )
{
    qsPkgParseTree * sSpecParseTree = NULL;
    qsPkgParseTree * sBodyParseTree = NULL;
    
    if ( aStatement->spvEnv->pkgPlanTree == NULL )
    {
        /* create [or replace] package statement */
        sSpecParseTree = aStatement->spvEnv->createPkg;
        sBodyParseTree = NULL;
    }
    else
    {
        /* create [or replace] package body statement */
        sSpecParseTree = aStatement->spvEnv->pkgPlanTree;
        sBodyParseTree = aStatement->spvEnv->createPkg;
    }

    /* package body�� ����� type �˻� */
    if ( sBodyParseTree != NULL )
    {
        IDE_DASSERT ( sBodyParseTree->objType == QS_PKG_BODY );
        IDE_TEST( searchLocalTypes( aStatement,
                                    sBodyParseTree->block->variableItems,
                                    aVariableItem,
                                    aTypeName,
                                    aIsFound,
                                    aType )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    /* package spec�� ����� type �˻�  */
    if ( (*aIsFound) == ID_FALSE )
    {
        IDE_DASSERT ( sSpecParseTree->objType == QS_PKG );
        IDE_TEST( searchLocalTypes( aStatement,
                                    sSpecParseTree->block->variableItems,
                                    aVariableItem,
                                    aTypeName,
                                    aIsFound,
                                    aType )
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

// BUG-46032
IDE_RC qsvProcType::makeArrayTypeColumnByModule( iduVarMemList   * aMemory,
                                                 qtcModule       * aRowModule,
                                                 qsTypes        ** aType )
{
    qsTypes * sType = NULL;

    IDE_TEST( STRUCT_ALLOC( aMemory, qsTypes, &sType )
              != IDE_SUCCESS);

    sType->common.itemType = QS_TYPE;
    sType->common.table    = ID_USHORT_MAX;
    sType->common.column   = ID_USHORT_MAX;
    sType->common.objectID = QS_EMPTY_OID;
    sType->common.next     = NULL;
    SET_EMPTY_POSITION( sType->common.name );

    sType->columns      = aRowModule->typeInfo->columns;
    sType->fields       = NULL;
    sType->columnCount  = 2;
    sType->variableType = QS_ASSOCIATIVE_ARRAY_TYPE;
    sType->typeSize     = 0;
    sType->flag         = 0;
    sType->tableInfo    = NULL;
    sType->typeModule   = aRowModule;
    sType->name[0]      = '\0';

    *aType = sType;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
