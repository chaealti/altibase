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
#include <qdnCheck.h>
#include <qcuSqlSourceInfo.h>
#include <qcmCache.h>
#include <qcmPartition.h>
#include <qdbCommon.h>
#include <qdbAlter.h>
#include <qdn.h>
#include <qcpUtil.h>

IDE_RC qdnCheck::makeColumnNameListFromExpression(
                qcStatement         * aStatement,
                qcNamePosList      ** aColumnNameList,
                qtcNode             * aNode,
                qcNamePosition        aColumnName )
{
/***********************************************************************
 *
 * Description :
 *  Parsing �ܰ��� expression�� �������, Ư�� �÷��� Column Name List�� �����.
 *      - subquery�� �������� �ʴ´�.
 *
 * Implementation :
 *  (1) qtc::makeColumn()���� ������ �׸����� Column���� Ȯ���ϰ�,
 *      Column Name�� ��ġ�ϸ� Column Name List�� �����Ѵ�.
 *  (2) arguments�� Recursive Call
 *  (3) next�� Recursive Call
 *
 ***********************************************************************/

    qcNamePosList * sColumnName;
    qcNamePosList * sLastColumnName = NULL;
    qcNamePosList * sNewColumnName  = NULL;

    /* qtc::makeColumn()���� ������ �׸����� Column���� Ȯ���ϰ� */
    if ( (QC_IS_NULL_NAME( aNode->columnName ) == ID_FALSE) &&
         (aNode->node.module == &qtc::columnModule) )
    {
        /* Column Name�� ��ġ�ϸ� Column Name List�� �����Ѵ�. */
        if ( QC_IS_NAME_MATCHED( aNode->columnName, aColumnName ) )
        {
            IDE_TEST( STRUCT_ALLOC( QC_QMP_MEM(aStatement), qcNamePosList, &sNewColumnName )
                      != IDE_SUCCESS );
            SET_POSITION( sNewColumnName->namePos, aNode->columnName );
            sNewColumnName->next = NULL;

            if ( *aColumnNameList == NULL )
            {
                *aColumnNameList = sNewColumnName;
            }
            else
            {
                for ( sColumnName = *aColumnNameList;
                      sColumnName != NULL;
                      sColumnName = sColumnName->next )
                {
                    sLastColumnName = sColumnName;
                }
                sLastColumnName->next = sNewColumnName;
            }
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

    /* arguments�� Recursive Call */
    if ( aNode->node.arguments != NULL )
    {
        IDE_TEST( makeColumnNameListFromExpression(
                        aStatement,
                        aColumnNameList,
                        (qtcNode *)aNode->node.arguments,
                        aColumnName )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    /* next�� Recursive Call */
    if ( aNode->node.next != NULL )
    {
        IDE_TEST( makeColumnNameListFromExpression(
                        aStatement,
                        aColumnNameList,
                        (qtcNode *)aNode->node.next,
                        aColumnName )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdnCheck::renameColumnInExpression(
                qcStatement         * aStatement,
                SChar              ** aNewExpressionStr,
                qtcNode             * aNode,
                qcNamePosition        aOldColumnName,
                qcNamePosition        aNewColumnName )
{
/***********************************************************************
 *
 * Description :
 *  expression ���� �÷� �̸��� �����Ͽ�, ���ο� expression ���ڿ��� �����.
 *      - ����ǥ�� �̿��� �÷� �̸��� �����Ѵ�.
 *      - subquery�� �������� �ʴ´�.
 *
 * Implementation :
 *  (1) ����ǥ�� ����Ͽ�, ���ο� �÷� �̸��� Offset�� Size�� ���Ѵ�.
 *  (2) ���� �÷� �̸����� expression���� Column Name List�� ��´�.
 *  (3) ���ο� expression ���ڿ��� ũ�⸦ ���ϰ� �޸𸮸� �Ҵ��Ѵ�.
 *  (4) ���ο� expression�� �����. (�Ʒ� �ݺ�)
 *    a. ����ǥ�� ����Ͽ�, ���� �÷� �̸��� Offset�� Size�� ���Ѵ�.
 *    b. ���� �������� ���� �������� �����Ѵ�.
 *    c. ���ο� �÷� �̸��� �����Ѵ�.
 *  (5) ������ �÷� �̸� ������, ���� expression ���ڿ��� �����Ѵ�.
 ***********************************************************************/

    qcNamePosList * sColumnNameList = NULL;
    qcNamePosList * sColumnName;
    SChar         * sNewExpressionStr = NULL;
    SInt            sNewExpressionStrSize;
    SInt            sNextCopyOffset;
    SInt            sOldColumnNameOffset;
    SInt            sOldColumnNameSize;
    SInt            sNewColumnNameOffset;
    SInt            sNewColumnNameSize;

    /* ����ǥ�� ����Ͽ�, ���ο� �÷� �̸��� Offset�� Size�� ���Ѵ�. */
    if ( (aNewColumnName.offset > 0) &&
         (aNewColumnName.size > 0) )
    {
        if ( aNewColumnName.stmtText[aNewColumnName.offset - 1] == '"' )
        {
            sNewColumnNameOffset = aNewColumnName.offset - 1;
            sNewColumnNameSize   = aNewColumnName.size + 2;
        }
        else
        {
            sNewColumnNameOffset = aNewColumnName.offset;
            sNewColumnNameSize   = aNewColumnName.size;
        }
    }
    else
    {
        sNewColumnNameOffset = aNewColumnName.offset;
        sNewColumnNameSize   = aNewColumnName.size;
    }

    /* ���� �÷� �̸����� expression���� Column Name List�� ��´�. */
    IDE_TEST( makeColumnNameListFromExpression( aStatement,
                                                &sColumnNameList,
                                                aNode,
                                                aOldColumnName )
              != IDE_SUCCESS );

    if ( sColumnNameList != NULL )
    {
        /* ���ο� expression ���ڿ��� ũ�⸦ ���ϰ� �޸𸮸� �Ҵ��Ѵ�. (NULL ����) */
        sNewExpressionStrSize = aNode->position.size;
        for ( sColumnName = sColumnNameList;
              sColumnName != NULL;
              sColumnName = sColumnName->next )
        {
            if ( (sColumnName->namePos.offset > 0) &&
                 (sColumnName->namePos.size > 0) )
            {
                if ( sColumnName->namePos.stmtText[sColumnName->namePos.offset - 1] == '"' )
                {
                    sOldColumnNameSize = sColumnName->namePos.size + 2;
                }
                else
                {
                    sOldColumnNameSize = sColumnName->namePos.size;
                }
            }
            else
            {
                sOldColumnNameSize = sColumnName->namePos.size;
            }

            sNewExpressionStrSize += sNewColumnNameSize - sOldColumnNameSize;
        }
        sNewExpressionStrSize++;

        IDE_TEST( QC_QMP_MEM(aStatement)->alloc( sNewExpressionStrSize,
                                                 (void**)&sNewExpressionStr )
                  != IDE_SUCCESS );
        idlOS::memset( sNewExpressionStr, 0x00, sNewExpressionStrSize );

        /* ���ο� expression�� �����. */
        sNextCopyOffset = aNode->position.offset;
        for ( sColumnName = sColumnNameList;
              sColumnName != NULL;
              sColumnName = sColumnName->next )
        {
            /* ����ǥ�� ����Ͽ�, ���� �÷� �̸��� Offset�� Size�� ���Ѵ�. */
            if ( (sColumnName->namePos.offset > 0) &&
                 (sColumnName->namePos.size > 0) )
            {
                if ( sColumnName->namePos.stmtText[sColumnName->namePos.offset - 1] == '"' )
                {
                    sOldColumnNameOffset = sColumnName->namePos.offset - 1;
                    sOldColumnNameSize   = sColumnName->namePos.size + 2;
                }
                else
                {
                    sOldColumnNameOffset = sColumnName->namePos.offset;
                    sOldColumnNameSize   = sColumnName->namePos.size;
                }
            }
            else
            {
                sOldColumnNameOffset = sColumnName->namePos.offset;
                sOldColumnNameSize   = sColumnName->namePos.size;
            }

            /* ���� �������� ���� �������� �����ϰ�, ������ ������ ������ �����Ѵ�. */
            if ( sNextCopyOffset < sOldColumnNameOffset )
            {
                (void)idlVA::appendString( sNewExpressionStr,
                                           sNewExpressionStrSize,
                                           &(aNode->position.stmtText[sNextCopyOffset]),
                                           (UInt)(sOldColumnNameOffset - sNextCopyOffset) );
            }
            else
            {
                /* Nothing to do */
            }
            sNextCopyOffset = sOldColumnNameOffset + sOldColumnNameSize;

            /* ���ο� �÷� �̸��� �����Ѵ�. */
            (void)idlVA::appendString( sNewExpressionStr,
                                       sNewExpressionStrSize,
                                       &(aNewColumnName.stmtText[sNewColumnNameOffset]),
                                       (UInt)sNewColumnNameSize );
        }

        /* ������ �÷� �̸� ������, ���� expression ���ڿ��� �����Ѵ�. */
        if ( sNextCopyOffset < (aNode->position.offset + aNode->position.size) )
        {
            (void)idlVA::appendString( sNewExpressionStr,
                                       sNewExpressionStrSize,
                                       &(aNode->position.stmtText[sNextCopyOffset]),
                                       (UInt)(aNode->position.offset
                                            + aNode->position.size
                                            - sNextCopyOffset) );
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

    *aNewExpressionStr = sNewExpressionStr;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdnCheck::addCheckConstrSpecRelatedToColumn(
                qcStatement         * aStatement,
                qdConstraintSpec   ** aConstrSpec,
                qcmCheck            * aChecks,
                UInt                  aCheckCount,
                UInt                  aColumnID )
{
/***********************************************************************
 *
 * Description :
 *  Ư�� Column�� ���õ� Check Constraint Spec List�� ��´�.
 *
 * Implementation :
 *  �� Check Constraint�� ���� �Ʒ��� �ݺ��Ѵ�.
 *  1. Check Constraint�� Ư�� Column�� �ִ��� Ȯ���Ѵ�.
 *  2. Check Constraint Spec List�� �̹� �ִ� Check Constraint���� Ȯ���Ѵ�.
 *  3. Check Constraint Spec List�� �������� �߰��Ѵ�.
 ***********************************************************************/

    qcmCheck         * sCheck;
    qdConstraintSpec * sCurrConstrSpec;
    qdConstraintSpec * sLastConstrSpec;
    qdConstraintSpec * sNewConstrSpec;
    qtcNode          * sNode[2];
    UInt               i;

    for ( i = 0; i < aCheckCount; i++ )
    {
        sCheck = &(aChecks[i]);

        if ( qdn::intersectColumn( sCheck->constraintColumn,
                                   sCheck->constraintColumnCount,
                                   &aColumnID,
                                   1 )
             != ID_TRUE )
        {
            continue;
        }
        else
        {
            /* Nothing to do */
        }

        /* �ߺ��� Ȯ���Ѵ�. */
        sLastConstrSpec = NULL;
        for ( sCurrConstrSpec = *aConstrSpec;
              sCurrConstrSpec != NULL;
              sCurrConstrSpec = sCurrConstrSpec->next )
        {
            if ( idlOS::strMatch( sCurrConstrSpec->constrName.stmtText + sCurrConstrSpec->constrName.offset,
                                  sCurrConstrSpec->constrName.size,
                                  sCheck->name,
                                  idlOS::strlen( sCheck->name ) ) == 0 )
            {
                break;
            }
            else
            {
                sLastConstrSpec = sCurrConstrSpec;
            }
        }

        if ( sCurrConstrSpec != NULL )
        {
            continue;
        }
        else
        {
            /* Nothing to do */
        }

        IDE_TEST( STRUCT_ALLOC( QC_QMP_MEM(aStatement),
                                qdConstraintSpec,
                                &sNewConstrSpec )
                  != IDE_SUCCESS );
        QD_SET_INIT_CONSTRAINT_SPEC( sNewConstrSpec );

        sNewConstrSpec->constrName.stmtText = sCheck->name;
        sNewConstrSpec->constrName.offset   = 0;
        sNewConstrSpec->constrName.size     = idlOS::strlen( sCheck->name );
        sNewConstrSpec->constrType          = QD_CHECK;
        sNewConstrSpec->constrColumnCount   = sCheck->constraintColumnCount;

        /* get_condition_statement�� DEFAULT�� �������� �ۼ��Ǿ� �����Ƿ�,
         * DEFAULT�� expression�� ��� �Լ��� ȣ���Ѵ�.
         */
        IDE_TEST( qcpUtil::makeDefaultExpression(
                                aStatement,
                                sNode,
                                sCheck->checkCondition,
                                idlOS::strlen( sCheck->checkCondition ) )
                  != IDE_SUCCESS );
        sNewConstrSpec->checkCondition = sNode[0];

        /* adjust expression position */
        sNewConstrSpec->checkCondition->position.offset = 7; /* "RETURN " */
        sNewConstrSpec->checkCondition->position.size   = idlOS::strlen( sCheck->checkCondition );

        /* make constraint column */
        IDE_TEST( qcpUtil::makeConstraintColumnsFromExpression(
                                aStatement,
                                &(sNewConstrSpec->constraintColumns),
                                sNewConstrSpec->checkCondition )
                  != IDE_SUCCESS );

        if ( *aConstrSpec == NULL )
        {
            *aConstrSpec = sNewConstrSpec;
        }
        else
        {
            sLastConstrSpec->next = sNewConstrSpec;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdnCheck::makeCheckConstrSpecRelatedToTable(
                qcStatement         * aStatement,
                qdConstraintSpec   ** aConstrSpec,
                qcmCheck            * aChecks,
                UInt                  aCheckCount )
{
/***********************************************************************
 *
 * Description :
 *  Table�� ���õ� Check Constraint Spec List�� ��´�.
 *
 * Implementation :
 *  1. qcmCheck �迭�� qdConstraintSpec List�� ��ȯ�Ѵ�.
 ***********************************************************************/

    qcmCheck         * sCheck;
    qdConstraintSpec * sLastConstrSpec = NULL;
    qdConstraintSpec * sNewConstrSpec;
    qtcNode          * sNode[2];
    UInt               i;

    *aConstrSpec = NULL;

    for ( i = 0; i < aCheckCount; i++ )
    {
        sCheck = &(aChecks[i]);

        IDE_TEST( STRUCT_ALLOC( QC_QMP_MEM(aStatement),
                                qdConstraintSpec,
                                &sNewConstrSpec )
                  != IDE_SUCCESS );
        QD_SET_INIT_CONSTRAINT_SPEC( sNewConstrSpec );

        sNewConstrSpec->constrName.stmtText = sCheck->name;
        sNewConstrSpec->constrName.offset   = 0;
        sNewConstrSpec->constrName.size     = idlOS::strlen( sCheck->name );
        sNewConstrSpec->constrType          = QD_CHECK;
        sNewConstrSpec->constrColumnCount   = sCheck->constraintColumnCount;

        /* get_condition_statement�� DEFAULT�� �������� �ۼ��Ǿ� �����Ƿ�,
         * DEFAULT�� expression�� ��� �Լ��� ȣ���Ѵ�.
         */
        IDE_TEST( qcpUtil::makeDefaultExpression(
                                aStatement,
                                sNode,
                                sCheck->checkCondition,
                                idlOS::strlen( sCheck->checkCondition ) )
                  != IDE_SUCCESS );
        sNewConstrSpec->checkCondition = sNode[0];

        /* adjust expression position */
        sNewConstrSpec->checkCondition->position.offset = 7; /* "RETURN " */
        sNewConstrSpec->checkCondition->position.size   = idlOS::strlen( sCheck->checkCondition );

        /* make constraint column */
        IDE_TEST( qcpUtil::makeConstraintColumnsFromExpression(
                                aStatement,
                                &(sNewConstrSpec->constraintColumns),
                                sNewConstrSpec->checkCondition )
                  != IDE_SUCCESS );

        if ( *aConstrSpec == NULL )
        {
            *aConstrSpec = sNewConstrSpec;
        }
        else
        {
            sLastConstrSpec->next = sNewConstrSpec;
        }

        sLastConstrSpec = sNewConstrSpec;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdnCheck::renameColumnInCheckConstraint(
                qcStatement      * aStatement,
                qdConstraintSpec * aConstrList,
                qcmTableInfo     * aTableInfo,
                qcmColumn        * aOldColumn,
                qcmColumn        * aNewColumn )
{
/***********************************************************************
 *
 * Description :
 *  ��� Check Constraint�� Column Name�� �����Ѵ�.
 *
 * Implementation :
 *  �� Check Constraint�� ���� �Ʒ��� �ݺ��Ѵ�.
 *  (1) ���ο� Column Name���� ������ Check Condition�� ���Ѵ�.
 *  (2) SYS_CONSTRAINTS_ ��Ÿ ���̺��� CHECK_CONDITION �÷��� �����Ѵ�.
 *
 ***********************************************************************/

    qdConstraintSpec   * sConstr;
    qcmCheck           * sCheck;
    SChar              * sCheckCondition = NULL;
    UInt                 i;

    for ( sConstr = aConstrList;
          sConstr != NULL;
          sConstr = sConstr->next )
    {
        if ( sConstr->constrType == QD_CHECK )
        {
            for ( i = 0; i < aTableInfo->checkCount; i++ )
            {
                sCheck = &(aTableInfo->checks[i]);

                if ( idlOS::strMatch( sConstr->constrName.stmtText + sConstr->constrName.offset,
                                      sConstr->constrName.size,
                                      sCheck->name,
                                      idlOS::strlen( sCheck->name ) ) == 0 )
                {
                    IDE_TEST( renameColumnInExpression(
                                            aStatement,
                                            &sCheckCondition,
                                            sConstr->checkCondition,
                                            aOldColumn->namePos,
                                            aNewColumn->namePos )
                              != IDE_SUCCESS );

                    IDE_TEST( qdbAlter::updateCheckCondition(
                                            aStatement,
                                            sCheck->constraintID,
                                            sCheckCondition )
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
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdnCheck::setMtcColumnToCheckConstrList(
                qcStatement      * aStatement,
                qcmTableInfo     * aTableInfo,
                qdConstraintSpec * aConstrList )
{
/***********************************************************************
 *
 * Description :
 *  Check Constraint List�� Constraint Column�� mtcColumn�� �����Ѵ�.
 *
 * Implementation :
 *  �� Constraint�� ���� �Ʒ��� �ݺ��Ѵ�.
 *  1. Check Constraint�̸�, Constraint Column�� mtcColumn�� �����Ѵ�.
 *
 ***********************************************************************/

    qdConstraintSpec   * sConstr;
    qcmColumn          * sColumn;
    qcmColumn          * sColumnInfo;

    for ( sConstr = aConstrList;
          sConstr != NULL;
          sConstr = sConstr->next )
    {
        if ( sConstr->constrType == QD_CHECK )
        {
            for ( sColumn = sConstr->constraintColumns;
                  sColumn != NULL;
                  sColumn = sColumn->next )
            {
                IDE_TEST( qcmCache::getColumn( aStatement,
                                               aTableInfo,
                                               sColumn->namePos,
                                               &sColumnInfo )
                          != IDE_SUCCESS );

                sColumn->basicInfo = sColumnInfo->basicInfo;
            }
        }
        else
        {
            /* Nothing to do */
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdnCheck::verifyCheckConstraintList(
                qcTemplate       * aTemplate,
                qdConstraintSpec * aConstrList )
{
/***********************************************************************
 *
 * Description :
 *  �����Ͱ� Check Constraint ���� ������ �����ϴ��� �˻��Ѵ�.
 *
 * Implementation :
 *  �� Check Constraint�� ���� �Ʒ��� �ݺ��Ѵ�.
 *  1. qtc::calculate()
 *  2. NULL �Ǵ� TRUE�̸� ����, FALSE�̸� ����
 *
 ***********************************************************************/

    qdConstraintSpec    * sConstr = NULL;
    mtcColumn           * sColumn = NULL;
    void                * sValue  = NULL;
    idBool                sIsTrue = ID_FALSE;
    SChar                 sConstrName[QC_MAX_OBJECT_NAME_LEN + 1] = { '\0', };

    for ( sConstr = aConstrList;
          sConstr != NULL;
          sConstr = sConstr->next )
    {
        if ( sConstr->constrType == QD_CHECK )
        {
            IDE_TEST_RAISE( qtc::calculate( sConstr->checkCondition, aTemplate )
                            != IDE_SUCCESS, ERR_INVALID_EXPRESSION );

            sColumn = aTemplate->tmplate.stack[0].column;
            sValue  = aTemplate->tmplate.stack[0].value;

            if ( sColumn->module->isNull( sColumn,
                                          sValue ) == ID_TRUE )
            {
                /* Nothing to do */
            }
            else
            {
                IDE_TEST_RAISE( sColumn->module->isTrue( &sIsTrue,
                                                          sColumn,
                                                          sValue )
                                != IDE_SUCCESS, ERR_INVALID_EXPRESSION );

                IDE_TEST_RAISE( sIsTrue == ID_FALSE,
                                ERR_VIOLATE_CHECK_CONDITION );
            }
        }
        else
        {
            /* Nothing to do */
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_EXPRESSION )
    {
        if ( QC_IS_NULL_NAME( sConstr->constrName ) != ID_TRUE )
        {
            QC_STR_COPY( sConstrName, sConstr->constrName );
        }
        else
        {
            /* Nothing to do */
        }

        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDN_INVALID_CHECK_CONSTRAINT_EXPRESSION,
                                  sConstrName ) );
    }
    IDE_EXCEPTION( ERR_VIOLATE_CHECK_CONDITION )
    {
        if ( QC_IS_NULL_NAME( sConstr->constrName ) != ID_TRUE )
        {
            QC_STR_COPY( sConstrName, sConstr->constrName );
        }
        else
        {
            /* Nothing to do */
        }

        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDN_VIOLATE_CHECK_CONSTRAINT,
                                  sConstrName ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdnCheck::verifyCheckConstraintListWithFullTableScan(
                qcStatement      * aStatement,
                qmsTableRef      * aTableRef,
                qdConstraintSpec * aCheckConstrList )
{
/***********************************************************************
 *
 * Description : PROJ-1107 Check Constraint ����
 *  �̹� ������ Table�� Check Constraint ���� ������ �߰��� ��,
 *  ������ Data�� ���Ͽ� check condition�� �˻��ؾ� �Ѵ�.
 *
 * Implementation :
 *  1. ���� �۾�
 *      - �Ķ���� Ȯ��
 *      - ���� �ʱ�ȭ
 *      - Ŀ�� �ʱ�ȭ
 *      - Record ���� Ȯ�� �� fetch column list ���� (Disk Table)
 *  2. �� ��Ƽ�ǿ� ���� Check Constraint �˻�
 *      - Table Full Scan
 *      - Check Condition ���� �� ��� Ȯ��
 *
 ***********************************************************************/

    qcTemplate           * sTemplate;
    mtcTuple             * sTableTuple     = NULL;
    mtcTuple             * sPartitionTuple = NULL;
    mtcTuple               sCopyTuple;
    iduMemoryStatus        sQmxMemStatus;

    // Cursor�� ���� ��������
    smiTableCursor         sReadCursor;
    smiCursorProperties    sCursorProperty;
    idBool                 sCursorOpen = ID_FALSE;

    idBool                 sNeedToRecoverTuple = ID_FALSE;

    // Record �˻��� ���� ���� ����
    UInt                   sPartType = 0;
    UInt                   sRowSize  = 0;
    void                 * sTupleRow = NULL;
    void                 * sDiskRow  = NULL;

    qmsPartitionRef      * sPartitionRefList = NULL;
    qmsPartitionRef      * sPartitionRefNode = NULL;
    qmsPartitionRef        sFakePartitionRef;
    qcmTableInfo         * sPartInfo;
    qcmTableInfo         * sDiskPartInfo = NULL;
    smiFetchColumnList   * sFetchColumnList;

    //---------------------------------------------
    // ���ռ� �˻�
    //---------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aTableRef != NULL );
    IDE_DASSERT( aCheckConstrList != NULL );
    IDE_DASSERT( aCheckConstrList->constrType == QD_CHECK );
    IDE_DASSERT( aCheckConstrList->checkCondition != NULL );

    //---------------------------------------------
    // �ʱ�ȭ
    //---------------------------------------------

    sTemplate = QC_PRIVATE_TMPLATE(aStatement);
    sTableTuple = & sTemplate->tmplate.rows[aTableRef->table];

    // PROJ-1502 PARTITIONED DISK TABLE
    if ( aTableRef->tableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
    {
        sPartitionRefList = aTableRef->partitionRef;
    }
    else
    {
        sFakePartitionRef.partitionInfo = aTableRef->tableInfo;
        sFakePartitionRef.table         = aTableRef->table;
        sFakePartitionRef.next          = NULL;

        sPartitionRefList = &sFakePartitionRef;
    }

    //---------------------------------------------
    // Check Condition ���� ���� �˻�
    //---------------------------------------------

    //----------------------------
    // Ŀ�� �ʱ�ȭ
    //----------------------------

    // To fix BUG-14818
    sReadCursor.initialize();

    //----------------------------
    // Record ���� Ȯ��
    //----------------------------

    for ( sPartitionRefNode = sPartitionRefList;
          sPartitionRefNode != NULL;
          sPartitionRefNode = sPartitionRefNode->next )
    {
        sPartInfo = sPartitionRefNode->partitionInfo;
        sPartType = sPartInfo->tableFlag & SMI_TABLE_TYPE_MASK;

        if ( sPartType == SMI_TABLE_DISK )
        {
            sDiskPartInfo = sPartInfo;
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
        IDE_TEST( aStatement->qmxMem->cralloc( sRowSize,
                                               (void **) & sDiskRow )
                  != IDE_SUCCESS );

        //--------------------------------------
        // PROJ-1705 fetch column list ����
        //--------------------------------------

        // fetch column list�� �ʱ�ȭ�Ѵ�.
        qdbCommon::initFetchColumnList( & sFetchColumnList );

        IDE_TEST( qdbCommon::addCheckConstrListToFetchColumnList(
                        aStatement->qmxMem,
                        aCheckConstrList,
                        sDiskPartInfo->columns,
                        &sFetchColumnList )
                  != IDE_SUCCESS );
    }
    else
    {
        // Memory Table�� ���
        // Nothing to do.
    }

    /* PROJ-2464 hybrid partitioned table ���� */
    qmx::copyMtcTupleForPartitionDML( &sCopyTuple, sTableTuple );
    sNeedToRecoverTuple = ID_TRUE;

    sTupleRow = sTableTuple->row;

    for ( sPartitionRefNode = sPartitionRefList;
          sPartitionRefNode != NULL;
          sPartitionRefNode = sPartitionRefNode->next )
    {
        sPartInfo = sPartitionRefNode->partitionInfo;
        sPartType = sPartInfo->tableFlag & SMI_TABLE_TYPE_MASK;

        /* PROJ-2464 hybrid partitioned table ���� */
        sPartitionTuple = &(sTemplate->tmplate.rows[sPartitionRefNode->table]);

        qmx::adjustMtcTupleForPartitionDML( sTableTuple, sPartitionTuple );

        SMI_CURSOR_PROP_INIT_FOR_FULL_SCAN( &sCursorProperty, aStatement->mStatistics );

        if ( sPartType == SMI_TABLE_DISK )
        {
            sCursorProperty.mFetchColumnList = sFetchColumnList;
            sTableTuple->row = sDiskRow;
        }
        else
        {
            sTableTuple->row = NULL;
        }

        IDE_TEST( sReadCursor.open( QC_SMI_STMT( aStatement ),
                                    sPartInfo->tableHandle,
                                    NULL,
                                    smiGetRowSCN( sPartInfo->tableHandle ),
                                    NULL,
                                    smiGetDefaultKeyRange(),
                                    smiGetDefaultKeyRange(),
                                    smiGetDefaultFilter(),
                                    SMI_LOCK_READ|
                                    SMI_TRAVERSE_FORWARD|
                                    SMI_PREVIOUS_DISABLE,
                                    SMI_SELECT_CURSOR,
                                    & sCursorProperty )
                  != IDE_SUCCESS );

        sCursorOpen = ID_TRUE;

        IDE_TEST( sReadCursor.beforeFirst() != IDE_SUCCESS );

        //----------------------------
        // �ݺ� �˻�
        //----------------------------

        IDE_TEST( sReadCursor.readRow( (const void**) & (sTableTuple->row),
                                       & sTableTuple->rid,
                                       SMI_FIND_NEXT )
                  != IDE_SUCCESS );

        while ( sTableTuple->row != NULL )
        {
            // Memory ������ ���Ͽ� ���� ��ġ ���
            IDE_TEST_RAISE( aStatement->qmxMem->getStatus( & sQmxMemStatus ) != IDE_SUCCESS, ERR_MEM_OP );

            // Check Condition ���� �� ��� Ȯ��
            IDE_TEST( verifyCheckConstraintList( sTemplate, aCheckConstrList )
                      != IDE_SUCCESS );

            // Memory ������ ���� Memory �̵�
            IDE_TEST_RAISE( aStatement->qmxMem->setStatus( & sQmxMemStatus ) != IDE_SUCCESS, ERR_MEM_OP );

            IDE_TEST( sReadCursor.readRow( (const void**) & (sTableTuple->row),
                                           & sTableTuple->rid,
                                           SMI_FIND_NEXT )
                      != IDE_SUCCESS );
        }

        sCursorOpen = ID_FALSE;

        IDE_TEST( sReadCursor.close() != IDE_SUCCESS );
    }

    /* PROJ-2464 hybrid partitioned table ���� */
    sNeedToRecoverTuple = ID_FALSE;
    qmx::copyMtcTupleForPartitionDML( sTableTuple, &sCopyTuple );

    sTableTuple->row = sTupleRow;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_MEM_OP )
    {
        ideLog::log( IDE_ERR_0,
                     "Unexpected errors may have occurred:"
                     " qdnCheck::verifyCheckConstraintListWithFullTableScan"
                     " memory error" );
    }
    IDE_EXCEPTION_END;

    if ( sCursorOpen == ID_TRUE )
    {
        (void) sReadCursor.close();
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
