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
 * $Id: qmvUnpivotTransform.cpp 90008 2021-02-17 06:03:16Z andrew.shin $
 *
 * PROJ-2523 Unpivot clause
 *
 * [ Unpivot Transformation�� 2�ܰ�� ������. ]
 *
 *   1. validateQmsTableRef �ʱ� �ܰ迡�� unpivot ������ �ؼ��Ͽ� TableRef->view��
 *      unpivot transformed view�� ���� �Ǵ� �����ϴ� �κ��̴�.
 *      ���� �����Ǵ� unpivot transformed view���� loop clause�� �߰� �Ǵµ�,
 *      �� loop clause�� ���� ���� ��ȯ�Ǵ� ���� ������ŭ record�� ���� �����ȴ�.
 *      �� ��, unpivot transformed view�� target�� unpivot ������ ���ǵ� column���� ���ڷ� �ϴ�
 *      DECODE_OFFSET �Լ���� �����Ǹ�, loop_level pseudo column�� ���� �ش��ϴ� ������ column�� �������� �ȴ�.
 *
 *   2. Target�� validation�ϱ� �� �� qmsValidateTarget �Լ��� ȣ��Ǳ� ����
 *      ���� 1. �� transform ���� �� DECODE_OFFSET target ����������
 *      ���ڷ� ��� ���� ���� column�� �� expand�Ѵ�.
 *
 * [ Unpivot transformation ]
 *
 *   < User query >
 *
 *     select *
 *       from t1
 *    unpivot exclude nulls ( value_col_name1 for measure_col_name1 in ( c1 as 'a',
 *                                                                       c2 as 'b',
 *                                                                       c5 as 'c' ) ) u1,
 *            t2
 *    unpivot exclude nulls ( value_col_name2 for measure_col_name2 in ( c1 as 'a',
 *                                                                       c2 as 'b',
 *                                                                       c5 as 'c' ) ) u2
 *      where u1.3 = u2.i3;
 *
 *   < Transformed query >
 *
 *     select *
 *       from ( select c3, c4, c6,
 *                     decode_offset( loop_level, 'a', 'b', 'c' ) measure_col_name1,
 *                     decode_offset( loop_level, c1, c2, c3 ) value_col_name1
 *                from t1
 *                loop 3 ) u1,
 *            ( select c3, c4, c6,
 *                     decode_offset( loop_level, 'a', 'b', 'c' ) measure_col_name2,
 *                     decode_offset( loop_level, c1, c2, c3 ) value_col_name2
 *                from t2
 *                loop 3 ) u2
 *      where ( u1.3 = u2.i3 ) and ( value_col_name1 is not null or value_col_name2 is not null  );
 *
 **********************************************************************/

#include <idl.h>
#include <qcg.h>
#include <qtc.h>
#include <qmvQTC.h>
#include <qmvUnpivotTransform.h>
#include <qmvQuerySet.h>
#include <qmv.h>
#include <qmo.h>
#include <qmx.h>

#define LOOP_MAX_LENGTH 16

extern mtfModule mtfDecodeOffset;
extern mtfModule mtfIsNotNull;
extern mtfModule mtfOr;
extern mtfModule mtfAnd;

IDE_RC qmvUnpivotTransform::checkUnpivotSyntax( qmsUnpivot * aUnpivot )
{
/***********************************************************************
 *
 * Description :
 *     Unpivot ������ ���� validation �˻縦 �����Ѵ�.
 *
 *     unpivot_value_column, unpivot_measure_column, unpivot_in_column, unpivot_in_alias �� ���ǵ�
 *     column�� ������ �����ؾ� �Ѵ�.
 *
 * Implementation :
 *     1. Unpivot value column�� ������ ���Ѵ�.
 *     2. Unpivot measure column�� ������ ���ϰ�, value columns�� ������ �ٸ��� ����
 *     3. Unpivot_in_clause�� column������ alias ������ ���ϰ�, value column�� ������ �ٸ��� ����
 *
 * Arguments :
 *
 ***********************************************************************/
    qmsUnpivotColName   * sValueColumnName = NULL;
    qmsUnpivotColName   * sMeasureColumnName = NULL;

    qmsUnpivotInColInfo * sInColInfo = NULL;

    qmsUnpivotInNode    * sInColumn = NULL;
    qmsUnpivotInNode    * sInAlias = NULL;

    UInt sValueColumnCnt = 0;
    UInt sMeasureColumnCnt = 0;

    UInt sInColumnCnt = 0;
    UInt sInAliasCnt = 0;

    IDU_FIT_POINT_FATAL( "qmvUnpivotTransform::checkUnpivotSyntax::__FT__" );

    for ( sValueColumnName  = aUnpivot->valueColName, sValueColumnCnt = 0;
          sValueColumnName != NULL;
          sValueColumnName  = sValueColumnName->next, sValueColumnCnt++ )
    {
        /* Nothing to do */
    }

    for ( sMeasureColumnName  = aUnpivot->measureColName, sMeasureColumnCnt = 0;
          sMeasureColumnName != NULL;
          sMeasureColumnName  = sMeasureColumnName->next, sMeasureColumnCnt++ )
    {
        /* Nothing to do */
    }

    /* Value_column�� count�� measure column�� count�� �ٸ��� �ȵȴ�. */
    IDE_TEST_RAISE( sValueColumnCnt != sMeasureColumnCnt, ERR_INVALID_UNPIVOT_ARGUMENT );

    for ( sInColInfo  = aUnpivot->inColInfo;
          sInColInfo != NULL;
          sInColInfo  = sInColInfo->next )
    {
        for ( sInColumn  = sInColInfo->inColumn, sInColumnCnt = 0;
              sInColumn != NULL;
              sInColumn  = sInColumn->next, sInColumnCnt++ )
        {
            /* Nothing to do */
        }

        /* Value_column�� count�� in_clause�� column count�� �ٸ��� �ȵȴ�. */
        IDE_TEST_RAISE( sValueColumnCnt != sInColumnCnt, ERR_INVALID_UNPIVOT_ARGUMENT );

        for ( sInAlias  = sInColInfo->inAlias, sInAliasCnt = 0;
              sInAlias != NULL;
              sInAlias  = sInAlias->next, sInAliasCnt++ )
        {
            /* Nothing to do */
        }

        /* Value_column�� count�� in_clause�� alias count�� �ٸ��� �ȵȴ�. */
        IDE_TEST_RAISE( sValueColumnCnt != sInAliasCnt, ERR_INVALID_UNPIVOT_ARGUMENT );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_UNPIVOT_ARGUMENT );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMV_INVALID_UNPIVOT_ARGUMENT ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmvUnpivotTransform::makeViewForUnpivot( qcStatement * aStatement,
                                                qmsTableRef * aTableRef )
{
/***********************************************************************
 *
 * Description :
 *     Unpivot ��� table( �Ǵ� view )�� ���ؼ�
 *     Parse tree�� �����ϴ� Transformation�� �����Ѵ�.
 *
 * Implementation :
 *     1. Transformed view statement�� ����
 *     2. Transformed view parse tree ����
 *     3. Transformed view query set ����
 *     4. Transformed view SFWGH ����
 *     5. Transformed view from ����
 *     7. Original tableRef�� table ( �Ǵ� view )�� ���� �� transformed view from�� ����
 *     8. Original tableRef�� transformed view statement�� in-line view�� ����
 *
 * Arguments :
 *
 ***********************************************************************/

    qcStatement  * sStatement = NULL;
    qmsParseTree * sParseTree = NULL;
    qmsQuerySet  * sQuerySet  = NULL;
    qmsSFWGH     * sSFWGH     = NULL;
    qmsFrom      * sFrom      = NULL;
    qmsTableRef  * sTableRef  = NULL;
    qcNamePosition sNullPosition;

    IDU_FIT_POINT_FATAL( "qmvUnpivotTransform::makeViewForUnpivot::__FT__" );

    SET_EMPTY_POSITION( sNullPosition );

    /* Allocation & initialization for transformed view */

    IDU_FIT_POINT( "qmvUnpivotTransform::makeViewForUnpivot::STRUCT_ALLOC::sStatement",
                   idERR_ABORT_InsufficientMemory );

    IDE_TEST( STRUCT_ALLOC( QC_QMP_MEM( aStatement ),
                            qcStatement,
                            &sStatement )
              != IDE_SUCCESS );

    IDU_FIT_POINT( "qmvUnpivotTransform::makeViewForUnpivot::STRUCT_ALLOC::sParseTree",
                   idERR_ABORT_InsufficientMemory );

    IDE_TEST( STRUCT_ALLOC( QC_QMP_MEM( aStatement ),
                            qmsParseTree,
                            &sParseTree )
              != IDE_SUCCESS );

    QC_SET_INIT_PARSE_TREE( sParseTree, sNullPosition );

    IDU_FIT_POINT( "qmvUnpivotTransform::makeViewForUnpivot::STRUCT_ALLOC::sQuerySet",
                   idERR_ABORT_InsufficientMemory );

    IDE_TEST( STRUCT_ALLOC( QC_QMP_MEM( aStatement ),
                            qmsQuerySet,
                            &sQuerySet )
              != IDE_SUCCESS );

    QCP_SET_INIT_QMS_QUERY_SET( sQuerySet );

    IDU_FIT_POINT( "qmvUnpivotTransform::makeViewForUnpivot::STRUCT_ALLOC::sSFWGH",
                   idERR_ABORT_InsufficientMemory );

    IDE_TEST( STRUCT_ALLOC( QC_QMP_MEM( aStatement ),
                            qmsSFWGH,
                            &sSFWGH )
              != IDE_SUCCESS );
    
    QCP_SET_INIT_QMS_SFWGH( sSFWGH );

    /* UNPIVOT flag setting for expanding target */
    sSFWGH->lflag &= ~QMV_SFWGH_UNPIVOT_MASK;
    sSFWGH->lflag |= QMV_SFWGH_UNPIVOT_TRUE;

    IDU_FIT_POINT( "qmvUnpivotTransform::makeViewForUnpivot::STRUCT_ALLOC::sFrom",
                   idERR_ABORT_InsufficientMemory );

    IDE_TEST( STRUCT_ALLOC( QC_QMP_MEM( aStatement ),
                            qmsFrom,
                            &sFrom )
              != IDE_SUCCESS );

    QCP_SET_INIT_QMS_FROM( sFrom );

    IDU_FIT_POINT( "qmvUnpivotTransform::makeViewForUnpivot::STRUCT_ALLOC::sTableRef",
                   idERR_ABORT_InsufficientMemory );

    IDE_TEST( STRUCT_ALLOC( QC_QMP_MEM( aStatement ),
                            qmsTableRef,
                            &sTableRef )
              != IDE_SUCCESS );

    QCP_SET_INIT_QMS_TABLE_REF( sTableRef );

    /* Copy original tableRef info to new tableRef except unpivot, noMergeHint and alias */
    idlOS::memcpy( sTableRef, aTableRef, ID_SIZEOF( qmsTableRef ) );
    QCP_SET_INIT_QMS_TABLE_REF( aTableRef );

    /* Set alias */
    SET_POSITION( aTableRef->aliasName, sTableRef->aliasName );

    /* BUG-48544 Pivot ���� ���� �Լ��� Inline View Alias, With Alias �� Coulmn �Ǵ� Subquery �� ����ϴ� ��� ������ �߻��մϴ�. */
    if ( sTableRef->view == NULL )
    {
        SET_EMPTY_POSITION( sTableRef->aliasName );
    }
    else
    {
        /* Nothing to do */
    }

    /* TASK-7219 */
    SET_POSITION( aTableRef->position, sTableRef->position );
    SET_EMPTY_POSITION( sTableRef->position );    

    /* Set noMergeHint */
    aTableRef->noMergeHint = sTableRef->noMergeHint;

    /* Set unpivot */
    aTableRef->unpivot = sTableRef->unpivot;
    sTableRef->unpivot = NULL;

    /* Set transformed view query set */ 
    sFrom->tableRef      = sTableRef;
    sSFWGH->from         = sFrom;
    sSFWGH->thisQuerySet = sQuerySet;
    sQuerySet->SFWGH     = sSFWGH;

    /* Set transformed view parse tree */
    sParseTree->withClause         = NULL;
    sParseTree->querySet           = sQuerySet;
    sParseTree->orderBy            = NULL;
    sParseTree->limit              = NULL;
    sParseTree->loopNode           = NULL;
    sParseTree->forUpdate          = NULL;
    sParseTree->isTransformed      = ID_FALSE;
    sParseTree->isSiblings         = ID_FALSE;
    sParseTree->isView             = ID_TRUE;
    sParseTree->isShardView        = ID_FALSE;
    sParseTree->common.currValSeqs = NULL;
    sParseTree->common.nextValSeqs = NULL;
    sParseTree->common.parse       = qmv::parseSelect;
    sParseTree->common.validate    = qmv::validateSelect;
    sParseTree->common.optimize    = qmo::optimizeSelect;
    sParseTree->common.execute     = qmx::executeSelect;

    /* Set transformed view statement */
    QC_SET_STATEMENT( sStatement, aStatement, sParseTree );
    sStatement->myPlan->parseTree->stmtKind = QCI_STMT_SELECT;

    /* Link transformed view statement on the original tableRef as a in-line view */
    aTableRef->view = sStatement;
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;    
}

IDE_RC qmvUnpivotTransform::makeArgsForDecodeOffset( qcStatement         * aStatement,
                                                     qtcNode             * aNode,
                                                     qmsUnpivotInColInfo * aUnpivotIn,
                                                     idBool                aIsValueColumn,
                                                     ULong                 aOrder )
{
/***********************************************************************
 *
 * Description :
 *     Unpivot transfored view�� target�� DECODE_OFFSET �Լ��� ���ڵ��� �����Ѵ�.
 *
 * Implementation :
 *     1. LOOP_LEVEL pseudo column ����
 *     2. DECODE_OFFSET �Լ��� ù �� ° ���ڷ� LOOP_LEVEL pseudo column�� ���
 *     3. unpivot_in_clause�� column �Ǵ� alias�� nodes��
 *        DECODE_OFFSET �Լ��� search parameter�� ���
 *     4. DECODE_OFFSET �Լ��� ���� ������ flag�� ����
 *
 * Arguments :
 *     aIsValueColumn - �� �Լ��� unpivot_value_column�� ���� ���� �� ��( ID_TRUE )��
 *                      unpivot_in_clause�� column nodes�� DECODE_OFFSET �Լ��� search parameter�� ����ϰ�,
 *                      unpivot_measure_column�� ���� ���� �� ��( ID_FALSE )��
 *                      unpivot_in_clause�� alias nodes�� search parameter�� ����Ѵ�.
 * 
 *     aOrder - unpivot�� multiple unpivotted columns�� ���� �� ���
 *
 *             ex ) unpivot (    value1,   value2,   value3
 *                      for    measure1, measure2, measure3
 *                       in ( ( column1,  column2,  column3 )
 *                         as (  alias1,   alias2,   alias3 ),
 *                            ( column4,  column5,  column6 )
 *                         as (  alias4,   alias5,   alias6 )
 *                         ... ) )
 *
 *               value1�� ���ؼ� ���� �ɶ��� column1, column 4
 *               measure2�� ���ؼ� ���� �� ���� alias2, alias5 ��
 *               DECODE_OFFSET�� search parameter�� ��ϵȴ�.
 *               �ڽſ��� �ش��ϴ� ������ nodes�� ����ϱ� ���� aOrder�� ���� �޴´�.
 *
 **********************************************************************/

    qtcNode             * sLoopNode[2];
    static SChar        * sLoopLevelName = (SChar*)"LOOP_LEVEL";
    qcNamePosition        sNamePosition;

    qmsUnpivotInColInfo * sUnpivotIn = NULL;
    qmsUnpivotInNode    * sInColumn  = NULL;
    mtcNode             * sPrevNode  = NULL;
    qmsUnpivotInNode    * sInAlias   = NULL;

    UInt                 sCount = 0;
    UInt                 sArgCnt = 0;

    IDU_FIT_POINT_FATAL( "qmvUnpivotTransform::makeViewForUnpivot::__FT__" );

    sNamePosition.stmtText = sLoopLevelName;
    sNamePosition.offset   = 0;
    sNamePosition.size     = idlOS::strlen( sLoopLevelName );

    /* Make LOOP_LEVEL pseudo column for the offset of DECODE_OFFSET function */
    IDE_TEST( qtc::makeColumn( aStatement,
                               sLoopNode,
                               NULL,
                               NULL,
                               &sNamePosition,
                               NULL )
              != IDE_SUCCESS );

    /* Add LOOP_LEVEL pseudo column node on DECODE_OFFSET as the first argument */
    aNode->node.arguments  = &(sLoopNode[0]->node );
    sPrevNode = aNode->node.arguments;

    /* For setting the argument count of decodeOffset */
    sArgCnt++;

    /* Make arguments for search parameters of DECODE_OFFSET function */
    for ( sUnpivotIn  = aUnpivotIn;
          sUnpivotIn != NULL;
          sUnpivotIn  = sUnpivotIn->next )
    {
        if ( aIsValueColumn == ID_TRUE )
        {
            /* For value column */
            for ( sInColumn  = sUnpivotIn->inColumn, sCount = 0;
                  ( sInColumn != NULL ) && ( sCount < aOrder );
                  sInColumn  = sInColumn->next, sCount++ )
            {
                /* Nothing to do */
            }

            IDE_TEST_RAISE( sInColumn == NULL, ERR_UNEXPECTED_ERROR );

            sPrevNode->next = &( sInColumn->inNode->node );
            sPrevNode = &( sInColumn->inNode->node );
            sArgCnt++;
        }
        else
        {
            /* For value measure */
            for ( sInAlias  = sUnpivotIn->inAlias, sCount = 0;
                  ( sInAlias != NULL ) && ( sCount < aOrder );
                  sInAlias  = sInAlias->next, sCount++ )
            {
                /* Nothing to do */
            }

            IDE_TEST_RAISE( sInAlias == NULL, ERR_UNEXPECTED_ERROR );

            sPrevNode->next = &( sInAlias->inNode->node );
            sPrevNode = &( sInAlias->inNode->node );
            sArgCnt++;
        }
    }

    /* Set arugment count of DECODE_OFFSET function. */
    aNode->node.lflag  = sArgCnt;
    aNode->node.lflag |= mtfDecodeOffset.lflag & ~MTC_NODE_COLUMN_COUNT_MASK;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_UNEXPECTED_ERROR )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmvUnpivotTransform::makeArgsForDecodeOffset",
                                  "Invalid Unpivot Target" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmvUnpivotTransform::makeUnpivotTarget( qcStatement * aStatement,
                                               qmsSFWGH    * aSFWGH,
                                               qmsTableRef * aTableRef )
{
/***********************************************************************
 *
 * Description :
 *     Unpivot transfored view�� target���� DECODE_OFFSET �Լ����� �����Ѵ�.
 *     unpivot_value_column�� unpivot_measure_column �ϳ��� �ϳ���
 *     DECODE_OFFSET �Լ��� �����ȴ�.
 *
 * Implementation :
 *     1. unpivot_measure_column�� ���� target�� �����Ѵ�.
 *        1-a. qmsTarget ����
 *        1-b. target expand�� ������ �� �� flag( QMS_TARGET_UNPIVOT_COLUMN_TRUE ) ó�� 
 *        1-c. unpivot_measure_column�� ��ϵ� �̸��� ������ target�� alias�� ����
 *        1-d. DECODE_OFFSET function�� �����ؼ� targetColumn�� ���
 *        1-e. DECODE_OFFSET�� ���� INTERMEDIATE tuple�� �Ҵ�
 *     2. unpivot_value_column�� ���� target�� �����Ѵ�.
 *        1-a. qmsTarget ����
 *        1-b. target expand�� ������ �� �� flag( QMS_TARGET_UNPIVOT_COLUMN_TRUE ) ó�� 
 *        1-c. unpivot_value_column�� ��ϵ� �̸��� ������ target�� alias�� ����
 *        1-d. DECODE_OFFSET function�� �����ؼ� targetColumn�� ���
 *        1-e. DECODE_OFFSET�� ���� INTERMEDIATE tuple�� �Ҵ�
 *
 * Arguments :
 *
 **********************************************************************/

    qmsTarget           * sTarget         = NULL;
    qmsTarget           * sPrevTarget     = NULL;
    
    qmsUnpivot          * sUnpivot        = NULL;
    qmsUnpivotColName   * sValueColName   = NULL;
    qmsUnpivotColName   * sMeasureColName = NULL;

    UInt                  sValueColumns   = 0;
    UInt                  sMeasureColumns = 0;

    qcNamePosition        sNullName;
    qtcNode             * sDecodeOffsetNode[2];

    IDU_FIT_POINT_FATAL( "qmvUnpivotTransform::makeUnpivotTarget::__FT__" );

    SET_EMPTY_POSITION( sNullName );

    sUnpivot = aTableRef->unpivot;
    
    /* Make DECODE_OFFSET for measure column(s) as a target for unpivot transformed view */
    for ( sMeasureColName  = sUnpivot->measureColName, sMeasureColumns = 0;
          sMeasureColName != NULL;
          sMeasureColName  = sMeasureColName->next, sMeasureColumns++ )
    {
        IDU_FIT_POINT( "qmvUnpivotTransform::makeUnpivotTarget::STRUCT_ALLOC::sTargetOfMeasueColName",
                       idERR_ABORT_InsufficientMemory );

        /* Alloc qmsTarget */
        IDE_TEST( STRUCT_ALLOC( QC_QMP_MEM( aStatement ),
                                qtcNode,
                                &sTarget )
                  != IDE_SUCCESS );

        QMS_TARGET_INIT( sTarget );

        IDU_FIT_POINT( "qmvUnpivotTransform::makeUnpivotTarget::STRUCT_ALLOC::nameOfMeasueColName",
                       idERR_ABORT_InsufficientMemory );

        /* Set written name of measure column on the created target as an alias */
        IDE_TEST( STRUCT_ALLOC_WITH_SIZE( QC_QMP_MEM( aStatement ),
                                          SChar,
                                          sMeasureColName->colName.size + 1,
                                          &( sTarget->aliasColumnName.name ) )
                  != IDE_SUCCESS );
        
        QC_STR_COPY( sTarget->aliasColumnName.name, sMeasureColName->colName ); // null terminator added.
        sTarget->aliasColumnName.size = sMeasureColName->colName.size;

        sTarget->displayName = sTarget->aliasColumnName;
        
        IDE_TEST( qtc::makeNode( aStatement,
                                 sDecodeOffsetNode,
                                 &sNullName,
                                 &mtfDecodeOffset )
                  != IDE_SUCCESS );

        sTarget->targetColumn = sDecodeOffsetNode[0];

        /* Make the arguments of DECODE_OFFSET function */
        IDE_TEST( makeArgsForDecodeOffset( aStatement,
                                           sTarget->targetColumn,
                                           sUnpivot->inColInfo,
                                           ID_FALSE,
                                           sMeasureColumns )
                  != IDE_SUCCESS );

        IDE_TEST( qtc::nextColumn( QC_QMP_MEM(aStatement),
                                   sTarget->targetColumn,
                                   aStatement,
                                   QC_SHARED_TMPLATE(aStatement),
                                   MTC_TUPLE_TYPE_INTERMEDIATE,
                                   (UInt)(mtfDecodeOffset.lflag & MTC_NODE_COLUMN_COUNT_MASK) )
                  != IDE_SUCCESS );

        if ( sMeasureColumns == 0 )
        {
            aSFWGH->target = sTarget;
            sPrevTarget    = sTarget;
        }
        else
        {
            sPrevTarget->next = sTarget;
            sPrevTarget = sTarget;            
        }
    }

    /* Make DECODE_OFFSET for value column(s) as a target for unpivot transformed view */
    for ( sValueColName  = sUnpivot->valueColName, sValueColumns = 0;
          sValueColName != NULL;
          sValueColName  = sValueColName->next, sValueColumns++ )
    {
        IDU_FIT_POINT( "qmvUnpivotTransform::makeUnpivotTarget::STRUCT_ALLOC::sTargetOfValueColName",
                       idERR_ABORT_InsufficientMemory );

        /* Alloc qmsTarget */
        IDE_TEST( STRUCT_ALLOC( QC_QMP_MEM( aStatement ),
                                qtcNode,
                                &sTarget )
                  != IDE_SUCCESS );

        QMS_TARGET_INIT( sTarget ) ;
        
        /* For expanding unpivot target */
        sTarget->flag &= ~QMS_TARGET_UNPIVOT_COLUMN_MASK;
        sTarget->flag |=  QMS_TARGET_UNPIVOT_COLUMN_TRUE;

        IDU_FIT_POINT( "qmvUnpivotTransform::makeUnpivotTarget::STRUCT_ALLOC::nameOfValueColName",
                       idERR_ABORT_InsufficientMemory );

        /* Set written name of value column on the created target as an alias */
        IDE_TEST( STRUCT_ALLOC_WITH_SIZE( QC_QMP_MEM( aStatement ),
                                          SChar,
                                          sValueColName->colName.size + 1,
                                          &( sTarget->aliasColumnName.name ) )
                  != IDE_SUCCESS );

        QC_STR_COPY( sTarget->aliasColumnName.name, sValueColName->colName ); // null terminator added.
        sTarget->aliasColumnName.size = sValueColName->colName.size;

        sTarget->displayName = sTarget->aliasColumnName;

        IDE_TEST( qtc::makeNode( aStatement,
                                 sDecodeOffsetNode,
                                 &sNullName,
                                 &mtfDecodeOffset )
                  != IDE_SUCCESS );

        sTarget->targetColumn = sDecodeOffsetNode[0];

        /* Make the arguments of DECODE_OFFSET function */
        IDE_TEST( makeArgsForDecodeOffset( aStatement,
                                           sTarget->targetColumn,
                                           sUnpivot->inColInfo,
                                           ID_TRUE,
                                           sValueColumns )
                  != IDE_SUCCESS );

        IDE_TEST( qtc::nextColumn( QC_QMP_MEM(aStatement),
                                   sTarget->targetColumn,
                                   aStatement,
                                   QC_SHARED_TMPLATE(aStatement),
                                   MTC_TUPLE_TYPE_INTERMEDIATE,
                                   (UInt)(mtfDecodeOffset.lflag & MTC_NODE_COLUMN_COUNT_MASK) )
                  != IDE_SUCCESS );

        // To remove a codesonar warning.
        IDE_TEST_RAISE( sPrevTarget == NULL, ERR_UNEXPECTED_ERROR );

        sPrevTarget->next = sTarget;
        sPrevTarget = sTarget;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_UNEXPECTED_ERROR )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmvUnpivotTransform::makeUnpivotTarget",
                                  "Invalid Unpivot Clause" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmvUnpivotTransform::makeLoopClause( qcStatement         * aStatement,
                                            qmsParseTree        * aParseTree,
                                            qmsUnpivotInColInfo * aInColInfo )
{
/***********************************************************************
 *
 * Description :
 *     Unpivot transfored view�� parse tree�� loop node�� �߰��Ѵ�.
 *
 * Implementation :
 *
 * Arguments :
 *     aInColInfo - unpivot_in�� element����
 *
 **********************************************************************/
    qtcNode             * sLoopNode[2];
    SChar                 sValue[LOOP_MAX_LENGTH + 1];
    qcNamePosition        sPosition;
    UInt                  sLoopLevelCount = 0;
    qmsUnpivotInColInfo * sUnpivotIn = NULL;

    IDU_FIT_POINT_FATAL( "qmvUnpivotTransform::makeLoopClause::__FT__" );

    for ( sUnpivotIn  = aInColInfo, sLoopLevelCount = 0;
          sUnpivotIn != NULL;
          sUnpivotIn  = sUnpivotIn->next, sLoopLevelCount++ )
    {
        /* nothing to do */
    }

    idlOS::snprintf( sValue,
                     LOOP_MAX_LENGTH + 1,
                     "%"ID_UINT32_FMT,
                     sLoopLevelCount );

    SET_EMPTY_POSITION( sPosition );
    
    IDE_TEST( qtc::makeValue( aStatement,
                              sLoopNode,
                              (const UChar*)"INTEGER",
                              7,
                              &sPosition,
                              (const UChar *)sValue,
                              (UInt)idlOS::strlen(sValue),
                              MTC_COLUMN_NORMAL_LITERAL )
              != IDE_SUCCESS );

    aParseTree->loopNode = sLoopNode[0];

    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmvUnpivotTransform::expandUnpivotTarget( qcStatement * aStatement,
                                                 qmsSFWGH    * aSFWGH)
{
/***********************************************************************
 *
 * Description :
 *     Validate phase�� validateQmsTarget ������ ȣ�� �Ǿ�
 *     Unpivot clause�� ������ ���� column���� target�� Ȯ���Ѵ�.
 *     �� ��, �� �� transformation �ܰ迡�� ������ DECODE_OFFSET target���� �� �� target���� ��ϵȴ�.
 *     ex ) t1 := c1, c2, c3, c4, c5
 *
 *     select *
 *       from ( select decode_offset( loop_level, c1, c2, c4 ),...
 *              ...
 *            );
 *
 *                                |
 *                                v
 *
 *     select *
 *       from ( select c3, c5, decode_offset( loop_level, c1, c2, c4 ),...
 *              ...
 *            );
 *
 * Implementation :
 *     1. TableRef�� columnCount��ŭ �ݺ��ϸ�, tableInfo�� �ִ� column���� target�� ����Ѵ�.
 *        �� ��,  TableInfo�� column�߿� �ռ� transformation �ܰ迡�� DECODE_OFFSET �Լ��� ���ڷ� ��� ��
 *        column���� expand���� �ʴ´�.
 *
 * Arguments :
 *
 ***********************************************************************/
    qmsTarget   * sUnpivotTarget      = NULL;
    qmsTarget   * sDecodeOffsetTarget = NULL; 

    qmsTableRef * sUnpivotTableRef = NULL;
    qcmColumn   * sColumn          = NULL;
    mtcNode     * sMtcNode         = NULL;

    UInt          sIndex           = 0;
    idBool        sIsFound         = ID_FALSE;
    UInt          sLen             = 0;
    UInt          sSize            = 0;
    SChar       * sName            = NULL;

    qmsTarget   * sTarget          = NULL;
    qmsTarget   * sPrevTarget      = NULL;

    qtcNode     * sNode            = NULL;

    IDU_FIT_POINT_FATAL( "qmvUnpivotTransform::expandUnpivotTarget::__FT__" );

    IDE_DASSERT( aSFWGH->target != NULL );
    IDE_DASSERT( aSFWGH->from->tableRef != NULL );

    sUnpivotTableRef    = aSFWGH->from->tableRef;
    sDecodeOffsetTarget = aSFWGH->target;

    sColumn = sUnpivotTableRef->tableInfo->columns;

    for ( sIndex = 0;
          sIndex < sUnpivotTableRef->tableInfo->columnCount;
          sIndex++, sColumn = sColumn->next )
    {
        sIsFound = ID_FALSE;

        /* �ش� column�� unpivot transformed view target�� ��� �Ǿ����� Ȯ�� */
        for ( sUnpivotTarget  = sDecodeOffsetTarget;
              sUnpivotTarget != NULL;
              sUnpivotTarget  = sUnpivotTarget->next )
        {
            if ( ( sUnpivotTarget->flag & QMS_TARGET_UNPIVOT_COLUMN_MASK )
                 == QMS_TARGET_UNPIVOT_COLUMN_TRUE )
            {
                IDE_DASSERT( sUnpivotTarget->targetColumn->node.module == &mtfDecodeOffset );

                /* DECODE_OFFSET�� search parameter�� 2�� ° argument���� �����Ѵ�. */
                for ( sMtcNode  = sUnpivotTarget->targetColumn->node.arguments->next;
                      sMtcNode != NULL;
                      sMtcNode  = sMtcNode->next )
                {
                    sNode = ( qtcNode *)sMtcNode;
                    sName = sNode->columnName.stmtText + sNode->columnName.offset;
                    sSize = sNode->columnName.size;

                    if ( idlOS::strMatch( sUnpivotTableRef->columnsName[ sIndex ],
                                          idlOS::strlen( sUnpivotTableRef->columnsName[ sIndex ] ),
                                          sName,
                                          sSize ) == 0 )
                    {
                        /* Unpivot transformed view target�� ��� �� column�̴�. */
                        sIsFound = ID_TRUE;
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
                /* Nohting to do */
            }

            if ( sIsFound == ID_TRUE )
            {
                break;
            }
            else
            {
                /* Nothing to do */
            }
        }

        /* Unpivot �� ���� column�� target expand���� �����Ѵ�. */
        if ( sIsFound == ID_TRUE )
        {
            continue;
        }
        else
        {
            /* Nothing to do */
        }

        IDU_FIT_POINT( "qmvUnpivotTransform::expandUnpivotTarget::STRUCT_ALLOC::sTarget",
                       idERR_ABORT_InsufficientMemory );

        /* Alloc & initialize qmsTarget */
        IDE_TEST( STRUCT_ALLOC( QC_QMP_MEM( aStatement ),
                                qmsTarget,
                                &sTarget )
                  != IDE_SUCCESS );
        
        QMS_TARGET_INIT( sTarget );

        IDU_FIT_POINT( "qmvUnpivotTransform::expandUnpivotTarget::STRUCT_ALLOC::targetColumn",
                       idERR_ABORT_InsufficientMemory );

        /* Alloc & initialize target column */
        IDE_TEST( STRUCT_ALLOC( QC_QMP_MEM( aStatement ),
                                qtcNode,
                                &( sTarget->targetColumn ) )
                  != IDE_SUCCESS );

        QTC_NODE_INIT( sTarget->targetColumn );

        sTarget->targetColumn->node.module = &qtc::columnModule;
        sTarget->targetColumn->node.lflag = qtc::columnModule.lflag & ~MTC_NODE_COLUMN_COUNT_MASK;

        /* Set column name */
        sLen = idlOS::strlen( sUnpivotTableRef->columnsName[ sIndex ] );

        IDU_FIT_POINT( "qmvUnpivotTransform::expandUnpivotTarget::STRUCT_ALLOC::stmtText",
                       idERR_ABORT_InsufficientMemory );

        IDE_TEST( STRUCT_ALLOC_WITH_SIZE( QC_QMP_MEM( aStatement ),
                                          SChar,
                                          sLen + 1,
                                          &( sTarget->targetColumn->position.stmtText ) )
                  != IDE_SUCCESS );

        idlOS::memcpy( sTarget->targetColumn->position.stmtText,
                       sUnpivotTableRef->columnsName[ sIndex ],
                       sLen );

        sTarget->targetColumn->position.stmtText[ sLen ]= '\0';
        sTarget->targetColumn->position.offset = 0;
        sTarget->targetColumn->position.size = sLen;
        sTarget->targetColumn->columnName = sTarget->targetColumn->position;

        /* Add the created target to the target list */
        if ( sPrevTarget == NULL )
        {
            aSFWGH->target = sTarget;
        }
        else
        {
            sPrevTarget->next = sTarget;
            sPrevTarget->targetColumn->node.next = &( sTarget->targetColumn->node );
        }

        sPrevTarget = sTarget;
    }

    /* Link the transformed target( DECODE_OFFSET ) to the expanded target next */
    if ( sPrevTarget == NULL )
    {
        aSFWGH->target = sDecodeOffsetTarget;
    }
    else
    {
        sPrevTarget->next = sDecodeOffsetTarget;
        sPrevTarget->targetColumn->node.next = &( sDecodeOffsetTarget->targetColumn->node );
    }

    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;    
}

IDE_RC qmvUnpivotTransform::makeNotNullPredicate( qcStatement * aStatement,
                                                  qmsSFWGH    * aSFWGH,
                                                  qmsTableRef * aTableRef )
{
/***********************************************************************
 *
 * Description :
 *     Unpivot clause�� EXCLUDE NULLS option(DEFAULT)�� ��� �Ǿ��� ���,
 *     Original query set�� WHERE predicate��
 *     value_column�� not null predicate���� �߰� �� �ش�.
 *
 *     ( rownum�� ��� COUNTER�� STOPKEY operator�� �ٸ� predicate���Ŀ�
 *       ���� �Ǳ� ������ ����������  �����Ѵ�. )
 *
 * Implementation :
 *     1. Unpivot_value_column �� �����Ѵ�.
 *     2. Not null predicate node�� �����Ѵ�.
 *     3. Not null predicate node�� ������ unpivot_value_column�� argument�� ����Ѵ�.
 *     4. ������ ������ not null predicate�� ������ Or node�� ���� �� �����Ͽ�
 *        Predicate tree�� �����Ѵ�.
 *     5. Not null predicate tree�� ������ ������, original query set�� where��
 *        �ٸ� predicate�� ������, where�� not null predicate tree�� ���� �� �ְ�,
 *        �ٸ� predicate�� ������, And node�� �߰��� ������
 *        �߰��� ������ not null predicate tree�� ���� predicate���� And�� ����
 *        Where�� �� And node�� �����Ѵ�.
 *
 * Arguments :
 *
 ***********************************************************************/

    qtcNode * sNotNullHead = NULL;

    qcNamePosition sNullName;

    qmsUnpivotColName * sValueColumn = NULL;

    qtcNode * sNode[2];
    qtcNode * sNotNullPredicate[2];

    qtcNode * sOrNode[2] = { NULL, NULL };
    qtcNode * sAndNode[2];

    IDU_FIT_POINT_FATAL( "qmvUnpivotTransform::makeNotNullPredicate::__FT__" );

    /* EXCLUDE NULLS Option exists. */
    if ( aTableRef->unpivot->isIncludeNulls == ID_FALSE )
    {
        sValueColumn = aTableRef->unpivot->valueColName;

        SET_EMPTY_POSITION( sNullName );
        /*************************************************************
         * 1. Make not null predicate list
         *
         *  (    or     )                              
         *        |  
         * ( is not null )->( is not null )->( is not null )   ...
         *        |                 |              |
         * ( valueColumn1 ) ( valueColumn2 ) ( valueColumn3 )  ...
         **************************************************************/
        for ( sValueColumn  = aTableRef->unpivot->valueColName;
              sValueColumn != NULL;
              sValueColumn  = sValueColumn->next )
        {
            // BUG-41878
            // Unpivotted table�� tuple variable�� ���� ���, �ش� tuple variable�� �����Ͽ� �����Ѵ�.
            IDE_TEST( qtc::makeColumn( aStatement,
                                       sNode,
                                       NULL,
                                       &( aTableRef->aliasName ),
                                       &( sValueColumn->colName ),
                                       NULL )
                      != IDE_SUCCESS );

            /* Make not null predicate node */
            IDE_TEST( qtc::makeNode( aStatement,
                                     sNotNullPredicate,
                                     &sNullName,
                                     &mtfIsNotNull )
                      != IDE_SUCCESS );

            /* Add the column node on the not null node as an argument */
            IDE_TEST( qtc::addArgument( sNotNullPredicate, sNode )
                      != IDE_SUCCESS );

            if ( sValueColumn == aTableRef->unpivot->valueColName )
            {
                sNotNullHead = sNotNullPredicate[0];
            }
            else
            {
                if ( sOrNode[0] == NULL )
                {
                    /* Make OR node for making a not null predicate tree */
                    IDE_TEST(qtc::makeNode(aStatement,
                                           sOrNode,
                                           &sNullName,
                                           &mtfOr )
                             != IDE_SUCCESS);

                    IDE_TEST( qtc::addArgument( sOrNode, &sNotNullHead ) != IDE_SUCCESS );

                    sNotNullHead = sOrNode[0];
                }
                else
                {
                    // Nothing to do.
                }

                IDE_TEST( qtc::addArgument( sOrNode, sNotNullPredicate ) != IDE_SUCCESS );
            }
        }

        if ( aSFWGH->where == NULL )
        {
            /* Original querySet�� �ٸ� predicate�� ���� ��� */
            aSFWGH->where = sNotNullHead;
        }
        else
        {
            /* Original querySet�� �ٸ� predicate�� �ִ� ��� */
            /* �ٸ� predicate�� ������ ������ not null predicate���� and�� ���� �� �ش�. */
            IDE_TEST(qtc::makeNode(aStatement,
                                   sAndNode,
                                   &sNullName,
                                   &mtfAnd )
                     != IDE_SUCCESS);

            IDE_TEST( qtc::addAndArgument( aStatement,
                                           sAndNode, &sNotNullHead, &aSFWGH->where )
                      != IDE_SUCCESS );

            aSFWGH->where = sAndNode[0];
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

IDE_RC qmvUnpivotTransform::doTransform( qcStatement * aStatement,
                                         qmsSFWGH    * aSFWGH,
                                         qmsTableRef * aTableRef )
{
/***********************************************************************
 *
 * Description :
 *     �ϳ��� TableRef�� ���õ� Unpivot ������ ���� Transformation�� �����Ѵ�.
 *     Validation Phase�� validateQmsTableRef���� ȣ�� �ȴ�.
 *
 * Implementation :
 *     1. Check unpivot syntax.
 *     2. Create unpivot parse tree. ( making the transformed view for unpivot )
 *     3. Create target for unpivot transformed view.
 *     4. Create loop clause for unpivot transformed view.
 *     5. Create not null predicates for unpivot transformed view,
 *        If there is EXCLUDE NULLS option on unpivot clause.
 *
 * Arguments :
 *
 ***********************************************************************/

    qmsParseTree        * sTransformedViewParseTree = NULL;

    IDU_FIT_POINT_FATAL( "qmvUnpivotTransform::makeNotNullPredicate::__FT__" );

    /**********************************
     * Check valid unpivot clause
     **********************************/
    IDE_TEST( checkUnpivotSyntax( aTableRef->unpivot )
              != IDE_SUCCESS );

    /**********************************
     * Make unpivot parse tree
     **********************************/
    IDE_TEST( makeViewForUnpivot( aStatement,
                                  aTableRef )
              != IDE_SUCCESS );

    sTransformedViewParseTree = ( qmsParseTree * )aTableRef->view->myPlan->parseTree;    

    /**********************************
     * Make unpivot view target
     **********************************/
    IDE_TEST( makeUnpivotTarget( aStatement,
                                 sTransformedViewParseTree->querySet->SFWGH,
                                 aTableRef )
              != IDE_SUCCESS );

    /**********************************
     * Add loop clause
     **********************************/
    
    IDE_TEST( makeLoopClause( aStatement,
                              sTransformedViewParseTree,
                              aTableRef->unpivot->inColInfo )
              != IDE_SUCCESS );
    
    /**********************************
     * Add not null predicate
     **********************************/
    IDE_TEST( makeNotNullPredicate( aStatement,
                                    aSFWGH,
                                    aTableRef )
              != IDE_SUCCESS );

    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;    
}
