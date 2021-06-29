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
 * $Id: qmvQuerySet.cpp 90409 2021-04-01 04:19:46Z donovan.seo $
 **********************************************************************/

#include <idl.h>
#include <ide.h>
#include <mtc.h>
#include <qcg.h>
#include <qmvQuerySet.h>
#include <qtc.h>
#include <qtcDef.h>
#include <qcmUser.h>
#include <qcmFixedTable.h>
#include <qmv.h>
#include <qmvQTC.h>
#include <qsvEnv.h>
#include <qsvProcVar.h>
#include <qsvProcStmts.h>
#include <qcuSqlSourceInfo.h>
#include <qcuProperty.h>
#include <qdpPrivilege.h>
#include <qdv.h>
#include <qmoStatMgr.h>
#include <qcmSynonym.h>
#include <qcmDump.h>
#include <qcmPartition.h> // PROJ-1502 PARTITIONED DISK TABLE
#include <qcgPlan.h>
#include <qcsModule.h>
#include <qmvGBGSTransform.h> // PROJ-2415 Grouping Sets Clause
#include <qmvPivotTransform.h>
#include <qmvHierTransform.h>
#include <qmvTableFuncTransform.h>
#include <qmsIndexTable.h>
#include <qmsDefaultExpr.h>
#include <qmsPreservedTable.h>
#include <qmr.h>
#include <qsvPkgStmts.h>
#include <qmvAvgTransform.h> /* PROJ-2361 */
#include <qdpRole.h>
#include <qmvUnpivotTransform.h> /* PROJ-2523 */
#include <qmvShardTransform.h>
#include <sdi.h>

extern mtfModule mtfAnd;
extern mtdModule mtdBinary;
extern mtdModule mtdVarchar;

extern mtfModule mtfGetBlobLocator;
extern mtfModule mtfGetClobLocator;
extern mtfModule mtfDecrypt;
extern mtfModule mtfCast;
extern mtfModule mtfList;

extern mtfModule mtfDecodeOffset;

#define QMV_QUERYSET_PARALLEL_DEGREE_LIMIT (1024)

IDE_RC qmvQuerySet::validate(
    qcStatement     * aStatement,
    qmsQuerySet     * aQuerySet,
    qmsSFWGH        * aOuterQuerySFWGH,
    qmsFrom         * aOuterQueryFrom,
    SInt              aDepth)
{
    qmsParseTree    * sParseTree;
    qmsTarget       * sLeftTarget;
    qmsTarget       * sRightTarget;
    qmsTarget       * sPrevTarget = NULL;
    qmsTarget       * sCurrTarget;
    mtcColumn       * sMtcColumn;
    UShort            sTable;
    UShort            sColumn;
    mtcNode         * sLeftNode;
    mtcNode         * sRightNode;
    qcuSqlSourceInfo  sqlInfo;

    IDU_FIT_POINT_FATAL( "qmvQuerySet::validate::__FT__" );

    IDE_TEST_RAISE(aDepth >= QCU_PROPERTY(mMaxSetOpRecursionDepth),
                   ERR_REACHED_MAX_RECURSION_DEPTH);

    IDE_DASSERT( aQuerySet != NULL );

    if (aQuerySet->setOp == QMS_NONE)
    {
        // SFWGH must be exist
        IDE_DASSERT( aQuerySet->SFWGH != NULL );
        
        // set Quter Query Pointer

        // PROJ-2418
        if ( aQuerySet->SFWGH->outerQuery != NULL )
        {
            // outerQuery�� �̹� �����Ǿ� �ִٸ�, Lateral View�̴�.
            // �̹� ������ outerQuery�� �缳������ �ʴ´�.

            // Nothing to do.
        }
        else
        {
            aQuerySet->SFWGH->outerQuery = aOuterQuerySFWGH;
        }

        // PROJ-2418
        if ( aQuerySet->SFWGH->outerFrom != NULL )
        {
            // outerFrom�� �̹� �����Ǿ� �ִٸ�, Lateral View�̴�.
            // �̹� ������ outerFrom�� �缳������ �ʴ´�.

            // Nothing to do.
        }
        else
        {
            aQuerySet->SFWGH->outerFrom = aOuterQueryFrom;
        }

        aQuerySet->SFWGH->lflag &= ~(QMV_PERFORMANCE_VIEW_CREATION_MASK);
        aQuerySet->SFWGH->lflag |= (aQuerySet->lflag&QMV_PERFORMANCE_VIEW_CREATION_MASK);
        aQuerySet->SFWGH->lflag &= ~(QMV_VIEW_CREATION_MASK);
        aQuerySet->SFWGH->lflag |= (aQuerySet->lflag&QMV_VIEW_CREATION_MASK);

        IDE_TEST( qmvQuerySet::validateQmsSFWGH( aStatement,
                                                 aQuerySet,
                                                 aQuerySet->SFWGH )
                  != IDE_SUCCESS );

        // set dependencies
        IDE_TEST( qtc::dependencyOr( & aQuerySet->SFWGH->depInfo,
                                     & aQuerySet->depInfo,
                                     & aQuerySet->depInfo )
                  != IDE_SUCCESS );

        // PROJ-1413
        // set outer column dependencies
        // PROJ-2415 Grouping Sets Cluase
        // setOuterDependencies() -> addOuterDependencies()
        IDE_TEST( qmvQTC::addOuterDependencies( aQuerySet->SFWGH,
                                                & aQuerySet->SFWGH->outerDepInfo )
                  != IDE_SUCCESS );

        IDE_TEST( qtc::dependencyOr( & aQuerySet->SFWGH->outerDepInfo,
                                     & aQuerySet->outerDepInfo,
                                     & aQuerySet->outerDepInfo )
                  != IDE_SUCCESS );

        // PROJ-2418
        // lateral View�� dependencies (lateralDepInfo) ����
        IDE_TEST( qmvQTC::setLateralDependencies( aQuerySet->SFWGH,
                                                  & aQuerySet->lateralDepInfo )
                  != IDE_SUCCESS );

        // set target
        aQuerySet->target = aQuerySet->SFWGH->target;

        aQuerySet->materializeType = aQuerySet->SFWGH->hints->materializeType;
    }
    else // UNION, UNION ALL, INTERSECT, MINUS
    {
        // FOR UPDATE clause
        sParseTree = (qmsParseTree *) aStatement->myPlan->parseTree;
        IDE_TEST_RAISE(sParseTree->forUpdate != NULL,
                       ERR_DISTINCT_SETFUNCTION_ON_FORUPDATE);

        aQuerySet->left->lflag &= ~(QMV_PERFORMANCE_VIEW_CREATION_MASK);
        aQuerySet->left->lflag |= (aQuerySet->lflag&QMV_PERFORMANCE_VIEW_CREATION_MASK);
        aQuerySet->right->lflag &= ~(QMV_PERFORMANCE_VIEW_CREATION_MASK);
        aQuerySet->right->lflag |= (aQuerySet->lflag&QMV_PERFORMANCE_VIEW_CREATION_MASK);
        aQuerySet->left->lflag &= ~(QMV_VIEW_CREATION_MASK);
        aQuerySet->left->lflag |= (aQuerySet->lflag&QMV_VIEW_CREATION_MASK);
        aQuerySet->right->lflag &= ~(QMV_VIEW_CREATION_MASK);
        aQuerySet->right->lflag |= (aQuerySet->lflag&QMV_VIEW_CREATION_MASK);

        // PROJ-2582 recursive with
        if ( ( aQuerySet->lflag & QMV_QUERYSET_RECURSIVE_VIEW_MASK )
             == QMV_QUERYSET_RECURSIVE_VIEW_TOP )
        {
            // recursive query ������ ���� right querySet flag����
            aQuerySet->right->lflag &= ~QMV_QUERYSET_RECURSIVE_VIEW_MASK;
            aQuerySet->right->lflag |= QMV_QUERYSET_RECURSIVE_VIEW_RIGHT;
            aQuerySet->right->lflag &= ~QMV_QUERYSET_RESULT_CACHE_MASK;
            aQuerySet->right->lflag |= QMV_QUERYSET_RESULT_CACHE_NO;

            aQuerySet->left->lflag &= ~QMV_QUERYSET_RECURSIVE_VIEW_MASK;
            aQuerySet->left->lflag |= QMV_QUERYSET_RECURSIVE_VIEW_LEFT;
            aQuerySet->left->lflag &= ~QMV_QUERYSET_RESULT_CACHE_MASK;
            aQuerySet->left->lflag |= QMV_QUERYSET_RESULT_CACHE_NO;
        }
        else
        {
            // nothing to do
        }
        
        IDE_TEST(qmvQuerySet::validate(aStatement,
                                       aQuerySet->left,
                                       aOuterQuerySFWGH,
                                       aOuterQueryFrom,
                                       aDepth + 1) != IDE_SUCCESS);

        IDE_TEST(qmvQuerySet::validate(aStatement,
                                       aQuerySet->right,
                                       aOuterQuerySFWGH,
                                       aOuterQueryFrom,
                                       aDepth + 1) != IDE_SUCCESS);

        if( ( aQuerySet->left->materializeType == QMO_MATERIALIZE_TYPE_VALUE )
            ||
            ( aQuerySet->right->materializeType == QMO_MATERIALIZE_TYPE_VALUE ) )
        {
            aQuerySet->materializeType = QMO_MATERIALIZE_TYPE_VALUE;
        }
        else
        {
            aQuerySet->materializeType = QMO_MATERIALIZE_TYPE_RID;
        }

        if (aQuerySet->right->setOp == QMS_NONE &&
            aQuerySet->right->SFWGH->intoVariables != NULL)
        {
            IDE_DASSERT( aQuerySet->right->SFWGH->intoVariables->intoNodes != NULL );
            
            sqlInfo.setSourceInfo(
                aStatement,
                & aQuerySet->right->SFWGH->intoVariables->intoNodes->position );
            IDE_RAISE( ERR_INAPPROPRIATE_INTO );
        }

        // for optimizer
        if (aQuerySet->setOp == QMS_UNION)
        {
            if (aQuerySet->left->setOp == QMS_UNION)
            {
                aQuerySet->left->setOp = QMS_UNION_ALL;
            }

            if (aQuerySet->right->setOp == QMS_UNION)
            {
                aQuerySet->right->setOp = QMS_UNION_ALL;
            }
        }

        /* match count of seleted items */
        sLeftTarget  = aQuerySet->left->target;
        sRightTarget = aQuerySet->right->target;

        // add tuple set
        IDE_TEST(qtc::nextTable( &(sTable), aStatement, NULL, ID_TRUE,
                                 MTC_COLUMN_NOTNULL_TRUE ) != IDE_SUCCESS); // PR-13597

        sColumn = 0;
        aQuerySet->target = NULL;
        while(sLeftTarget != NULL && sRightTarget != NULL)
        {
            if (((sLeftTarget->targetColumn->lflag & QTC_NODE_COLUMN_RID_MASK) ==
                 QTC_NODE_COLUMN_RID_EXIST) ||
                ((sRightTarget->targetColumn->lflag & QTC_NODE_COLUMN_RID_MASK) ==
                 QTC_NODE_COLUMN_RID_EXIST))
            {
                IDE_RAISE( ERR_PROWID_NOT_SUPPORTED );
            }
            
            // ����Ʈ Ÿ���� ��� ���� Ȯ��
            sMtcColumn = QTC_STMT_COLUMN( aStatement, sLeftTarget->targetColumn );
            if ( sMtcColumn->module->id == MTD_LIST_ID )
            {
                sqlInfo.setSourceInfo( aStatement,
                                       & sLeftTarget->targetColumn->position );
                IDE_RAISE( ERR_NOT_APPLICABLE_TYPE_IN_TARGET );
            }
            else
            {
                // Nothing to do.
            }
            
            sMtcColumn = QTC_STMT_COLUMN( aStatement, sRightTarget->targetColumn );
            if ( sMtcColumn->module->id == MTD_LIST_ID )
            {
                sqlInfo.setSourceInfo( aStatement,
                                       & sRightTarget->targetColumn->position );
                IDE_RAISE( ERR_NOT_APPLICABLE_TYPE_IN_TARGET );
            }
            else
            {
                // Nothing to do.
            }
            
            // BUG-16000
            // compare������ �Ұ����� Ÿ���� ����.
            if (aQuerySet->setOp != QMS_UNION_ALL)
            {
                // UNION, INTERSECT, MINUS
                // left
                if ( qtc::isEquiValidType( sLeftTarget->targetColumn,
                                           & QC_SHARED_TMPLATE(aStatement)->tmplate )
                     == ID_FALSE )
                {
                    sqlInfo.setSourceInfo( aStatement,
                                           & sLeftTarget->targetColumn->position );
                    IDE_RAISE( ERR_ABORT_QMV_USE_INCOMPARABLE_DATA_TYPE_WITH_SET );
                }
                else
                {
                    // Nothing to do.
                }

                // right
                if ( qtc::isEquiValidType( sRightTarget->targetColumn,
                                           & QC_SHARED_TMPLATE(aStatement)->tmplate )
                     == ID_FALSE )
                {
                    sqlInfo.setSourceInfo( aStatement,
                                           & sRightTarget->targetColumn->position );
                    IDE_RAISE( ERR_ABORT_QMV_USE_INCOMPARABLE_DATA_TYPE_WITH_SET );
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

            // PROJ-2582 recursive with
            if ( ( aQuerySet->lflag & QMV_QUERYSET_RECURSIVE_VIEW_MASK )
                 == QMV_QUERYSET_RECURSIVE_VIEW_TOP )
            {
                // recursive view�� ��� target type�� left�� type, precision���� �����.
                IDE_TEST(
                    qtc::makeLeftDataType4TwoNode( aStatement,
                                                   sLeftTarget->targetColumn,
                                                   sRightTarget->targetColumn )
                    != IDE_SUCCESS );
            }
            else
            {   
                // Query Set�� ���� Target�� ������ Data Type���� ������.
                IDE_TEST(
                    qtc::makeSameDataType4TwoNode( aStatement,
                                                   sLeftTarget->targetColumn,
                                                   sRightTarget->targetColumn )
                    != IDE_SUCCESS );
            }

            // make new target for SET
            IDE_TEST(STRUCT_ALLOC(QC_QMP_MEM(aStatement), qmsTarget, &sCurrTarget)
                     != IDE_SUCCESS);

            sCurrTarget->userName         = sLeftTarget->userName;
            sCurrTarget->tableName        = sLeftTarget->tableName;
            sCurrTarget->aliasTableName   = sLeftTarget->aliasTableName;
            sCurrTarget->columnName       = sLeftTarget->columnName;
            sCurrTarget->aliasColumnName  = sLeftTarget->aliasColumnName;
            sCurrTarget->displayName      = sLeftTarget->displayName;
            sCurrTarget->flag             = sLeftTarget->flag;
            sCurrTarget->flag            &= ~QMS_TARGET_IS_NULLABLE_MASK;
            sCurrTarget->flag            |= QMS_TARGET_IS_NULLABLE_TRUE;
            sCurrTarget->next             = NULL;

            // set member of qtcNode
            IDE_TEST(qtc::makeInternalColumn(
                         aStatement,
                         sTable,
                         sColumn,
                         &(sCurrTarget->targetColumn))
                     != IDE_SUCCESS);
            
            // PROJ-2415 Grouping Sets Clause
            // left�� right Node�� flag�� Set�� Target Node�� �����Ѵ�.
            if ( ( ( sLeftTarget->targetColumn->lflag & QTC_NODE_GBGS_ORDER_BY_NODE_MASK )
                   == QTC_NODE_GBGS_ORDER_BY_NODE_TRUE ) ||
                 ( ( sRightTarget->targetColumn->lflag & QTC_NODE_GBGS_ORDER_BY_NODE_MASK )
                   == QTC_NODE_GBGS_ORDER_BY_NODE_TRUE ) )
            {
                sCurrTarget->targetColumn->lflag &= ~QTC_NODE_GBGS_ORDER_BY_NODE_MASK;
                sCurrTarget->targetColumn->lflag |= QTC_NODE_GBGS_ORDER_BY_NODE_TRUE;
            }
            else
            {
                /* Nothing to do */
            }

            if (aQuerySet->target == NULL)
            {
                aQuerySet->target = sCurrTarget;
                sPrevTarget       = sCurrTarget;
            }
            else
            {
                sPrevTarget->next = sCurrTarget;
                sPrevTarget->targetColumn->node.next =
                    (mtcNode *)(sCurrTarget->targetColumn);

                sPrevTarget       = sCurrTarget;
            }

            // next
            sColumn++;
            sLeftTarget  = sLeftTarget->next;
            sRightTarget = sRightTarget->next;
        }

        IDE_TEST_RAISE( aQuerySet->target == NULL, ERR_EMPTY_TARGET );

        IDE_TEST_RAISE( (sLeftTarget != NULL && sRightTarget == NULL) ||
                        (sLeftTarget == NULL && sRightTarget != NULL),
                        ERR_NOT_MATCH_TARGET_COUNT_IN_QUERYSET);

        IDE_TEST(qtc::allocIntermediateTuple( aStatement,
                                              & QC_SHARED_TMPLATE(aStatement)->tmplate,
                                              sTable,
                                              sColumn )
                 != IDE_SUCCESS);

        // set mtcColumn
        for( sColumn = 0,
                 sCurrTarget = aQuerySet->target,
                 sLeftTarget = aQuerySet->left->target,
                 sRightTarget = aQuerySet->right->target;
             sColumn < QC_SHARED_TMPLATE(aStatement)->tmplate.rows[sTable].columnCount;
             sColumn++,
                 sCurrTarget = sCurrTarget->next,
                 sLeftTarget = sLeftTarget->next,
                 sRightTarget = sRightTarget->next )
        {
            sLeftNode = mtf::convertedNode(
                (mtcNode *)sLeftTarget->targetColumn,
                &QC_SHARED_TMPLATE(aStatement)->tmplate);
            sRightNode = mtf::convertedNode(
                (mtcNode *)sRightTarget->targetColumn,
                &QC_SHARED_TMPLATE(aStatement)->tmplate);      
            
            // To Fix Case-2175
            // Left�� Right�� ���� Size�� ū Column�� ������
            // View�� Column ������ ����Ѵ�.
            if ( QC_SHARED_TMPLATE(aStatement)->tmplate.rows[sLeftNode->table].
                 columns[sLeftNode->column].column.size
                 >=
                 QC_SHARED_TMPLATE(aStatement)->tmplate.rows[sRightNode->table].
                 columns[sRightNode->column].column.size )
            {
                // Left���� Column ������ ����
                // copy size, type, module
                mtc::copyColumn(
                    &( QC_SHARED_TMPLATE(aStatement)->tmplate.rows[sTable]
                       .columns[sColumn]),
                    &( QC_SHARED_TMPLATE(aStatement)->tmplate.rows[sLeftNode->table]
                       .columns[sLeftNode->column]) );
            }
            else
            {
                // Right���� Column ������ ����
                // copy size, type, module
                mtc::copyColumn(
                    &( QC_SHARED_TMPLATE(aStatement)->tmplate.rows[sTable]
                       .columns[sColumn]),
                    &( QC_SHARED_TMPLATE(aStatement)->tmplate.rows[sRightNode->table]
                       .columns[sRightNode->column]) );
            }

            // set execute
            IDE_TEST( qtc::estimateNodeWithoutArgument(
                          aStatement, sCurrTarget->targetColumn )
                      != IDE_SUCCESS );
        }

        // set offset
        qtc::resetTupleOffset( & QC_SHARED_TMPLATE(aStatement)->tmplate, sTable );

        // nothing to do for tuple
        QC_SHARED_TMPLATE(aStatement)->tmplate.rows[sTable].lflag
            &= ~MTC_TUPLE_ROW_SKIP_MASK;
        QC_SHARED_TMPLATE(aStatement)->tmplate.rows[sTable].lflag
            |= MTC_TUPLE_ROW_SKIP_TRUE;

        // SET�� ���� ������ �Ͻ��� View���� ǥ����
        QC_SHARED_TMPLATE(aStatement)->tmplate.rows[sTable].lflag &= ~MTC_TUPLE_VIEW_MASK;
        QC_SHARED_TMPLATE(aStatement)->tmplate.rows[sTable].lflag |= MTC_TUPLE_VIEW_TRUE;

        // set dependencies
        qtc::dependencyClear( & aQuerySet->depInfo );

        //     (1) view dependency
        qtc::dependencySet( sTable, & aQuerySet->depInfo );

        // PROJ-1358
        // SET �� ������ Dependency�� OR-ing ���� �ʴ´�.
        //     (2) left->dependencies
        // qtc::dependencyOr(aQuerySet->left->dependencies,
        //                   aQuerySet->dependencies,
        //                   aQuerySet->dependencies);
        //     (3) right->dependencies
        // qtc::dependencyOr(aQuerySet->right->dependencies,
        //                   aQuerySet->dependencies,
        //                  aQuerySet->dependencies);

        // set outer column dependencies

        // PROJ-1413
        // outer column dependency�� ���� dependency�� OR-ing�Ѵ�.
        //     (1) left->dependencies
        IDE_TEST( qtc::dependencyOr( & aQuerySet->left->outerDepInfo,
                                     & aQuerySet->outerDepInfo,
                                     & aQuerySet->outerDepInfo )
                  != IDE_SUCCESS );
        //     (2) right->dependencies
        IDE_TEST( qtc::dependencyOr( & aQuerySet->right->outerDepInfo,
                                     & aQuerySet->outerDepInfo,
                                     & aQuerySet->outerDepInfo )
                  != IDE_SUCCESS );

        // PROJ-2418
        // lateral view dependency�� ���� dependency�� OR-ing�Ѵ�.
        IDE_TEST( qtc::dependencyOr( & aQuerySet->left->lateralDepInfo,
                                     & aQuerySet->lateralDepInfo,
                                     & aQuerySet->lateralDepInfo )
                  != IDE_SUCCESS );

        IDE_TEST( qtc::dependencyOr( & aQuerySet->right->lateralDepInfo,
                                     & aQuerySet->lateralDepInfo,
                                     & aQuerySet->lateralDepInfo )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_NOT_MATCH_TARGET_COUNT_IN_QUERYSET)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QMV_MISMATCH_TARGET_COUNT));
    }
    IDE_EXCEPTION(ERR_DISTINCT_SETFUNCTION_ON_FORUPDATE)
    {
        IDE_SET(ideSetErrorCode(
                    qpERR_ABORT_QMV_DISTINCT_SETFUNCTION_ON_FORUPDATE));
    }
    IDE_EXCEPTION(ERR_INAPPROPRIATE_INTO);
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QSV_INAPPROPRIATE_INTO_SQLTEXT,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION(ERR_ABORT_QMV_USE_INCOMPARABLE_DATA_TYPE_WITH_SET);
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QMV_USE_INCOMPARABLE_DATA_TYPE_WITH_SET,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION( ERR_EMPTY_TARGET )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmvQuerySet::validate",
                                  "Target is empty" ));
    }
    IDE_EXCEPTION(ERR_REACHED_MAX_RECURSION_DEPTH)
    {
        IDE_SET(ideSetErrorCode(
                    qpERR_ABORT_REACHED_MAX_SET_OP_RECURSION_DEPTH));
    }
    IDE_EXCEPTION(ERR_PROWID_NOT_SUPPORTED)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QMV_PROWID_NOT_SUPPORTED));
    }
    IDE_EXCEPTION( ERR_NOT_APPLICABLE_TYPE_IN_TARGET );
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QMV_NOT_APPLICABLE_TYPE_IN_TARGET,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmvQuerySet::validateQmsSFWGH(
    qcStatement     * aStatement,
    qmsQuerySet     * aQuerySet,
    qmsSFWGH        * aSFWGH)
{
    qmsParseTree     * sParseTree = NULL;
    qmsFrom          * sFrom = NULL;
    qmsTarget        * sTarget = NULL;

    IDU_FIT_POINT_FATAL( "qmvQuerySet::validateQmsSFWGH::__FT__" );

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aSFWGH != NULL );

    //---------------------------------------------------
    // initialize
    //---------------------------------------------------

    if ( aQuerySet != NULL )
    {
        // BUG-21985 : replication���� WHERE�� ���� validation �Ҷ�,
        //             querySet ������ ���� �ѱ��� ����
        aQuerySet->processPhase = QMS_VALIDATE_INIT;
    }
    else
    {
        // nothing to do 
    }
    aSFWGH->validatePhase = QMS_VALIDATE_INIT;

    sParseTree = (qmsParseTree *) aStatement->myPlan->parseTree;

    IDE_TEST_RAISE( aSFWGH->selectType == QMS_DISTINCT &&
                    sParseTree->forUpdate != NULL,
                    ERR_DISTINCT_SETFUNCTION_ON_FORUPDATE);

    // codesonar::Null Pointer Dereference
    IDE_FT_ERROR( aQuerySet != NULL );

    // PROJ-2582 recursive with
    IDE_TEST_RAISE( ( aSFWGH->selectType == QMS_DISTINCT ) &&
                    ( ( aQuerySet->lflag & QMV_QUERYSET_RECURSIVE_VIEW_MASK )
                      == QMV_QUERYSET_RECURSIVE_VIEW_RIGHT ),
                    ERR_DISTINCT_NOT_ALLOWED_RECURSIVE_VIEW );

    // PROJ-2204 Join Update, Delete
    // create view, insert view, update view, delete view��
    // �ֻ��� SFWGH�� key preserved table�� �˻��Ѵ�.
    if ( ( aSFWGH->lflag & QMV_SFWGH_UPDATABLE_VIEW_MASK )
         == QMV_SFWGH_UPDATABLE_VIEW_TRUE )
    {
        IDE_TEST( qmsPreservedTable::initialize( aStatement,
                                                 aSFWGH )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }
              
    qtc::dependencyClear( & aSFWGH->depInfo );

    //-------------------------------------
    // 1473TODO Ȯ���ʿ�
    // PROJ-1473
    // �÷����������� hint������ �ʿ��ؼ� �ʱ�ȭ �۾��� ������ ���.
    //-------------------------------------

    if ( aSFWGH->hints == NULL )
    {
        // ����ڰ� ��Ʈ�� ������� ���� ��� �ʱⰪ���� ��Ʈ ����ü �Ҵ�

        IDE_TEST ( STRUCT_ALLOC( QC_QMP_MEM(aStatement),
                                 qmsHints,
                                 &(aSFWGH->hints) ) != IDE_SUCCESS);

        QCP_SET_INIT_HINTS(aSFWGH->hints);
    }
    else
    {
        // Nothing To Do 
    }

    // PROJ-1473
    // ���ǿ� ���� �÷������� �������
    // processType hint�� ���ؼ��� �Ϻ� �������Ѵ�.
    // (��: on ���� ���� �÷�ó��)
    
    if( aSFWGH->hints->materializeType == QMO_MATERIALIZE_TYPE_NOT_DEFINED )
    {
        if( QCU_OPTIMIZER_PUSH_PROJECTION == 1 )
        {
            aSFWGH->hints->materializeType = QMO_MATERIALIZE_TYPE_VALUE;
        }
        else
        {
            aSFWGH->hints->materializeType = QMO_MATERIALIZE_TYPE_RID;
        }
    }
    else
    {
        // Nothing To Do
    }

    /* BUG-36580 supported TOP */
    if ( aSFWGH->top != NULL )
    {
        IDE_TEST( qmv::validateLimit( aStatement, aSFWGH->top )
                  != IDE_SUCCESS );
    }
    else
    {		  
        // Nothing To Do
    }
    
    //---------------------------------------------------
    // validation of FROM clause
    //---------------------------------------------------

    if ( aQuerySet != NULL )
    {
        aQuerySet->processPhase = QMS_VALIDATE_FROM;
    }
    else
    {
        // nothing to do 
    }
    aSFWGH->validatePhase = QMS_VALIDATE_FROM;

    // fix BUG-14606
    IDE_TEST_RAISE( ( ( aSFWGH->from->next != NULL ) ||
                      ( aSFWGH->from->joinType != QMS_NO_JOIN ) )
                    && ( sParseTree->forUpdate != NULL ),
                    ERR_JOIN_ON_FORUPDATE);

    /* PROJ-1715 oneTable transform inlineview */
    if ( aSFWGH->hierarchy != NULL )
    {
        sFrom = aSFWGH->from;
        if ( ( sFrom->joinType == QMS_NO_JOIN  ) &&
             ( sFrom->next == NULL ) )
        {
            if ( sFrom->tableRef->view == NULL )
            {
                IDE_TEST( qmvHierTransform::doTransform( aStatement,
                                                         sFrom->tableRef )
                          != IDE_SUCCESS );
            }
            else
            {
                // PROJ-2638
                if ( ( sFrom->tableRef->view->myPlan->parseTree->stmtShard !=
                       QC_STMT_SHARD_NONE ) &&
                     ( sFrom->tableRef->view->myPlan->parseTree->stmtShard !=
                       QC_STMT_SHARD_META ) )
                {
                    IDE_TEST( qmvHierTransform::doTransform( aStatement,
                                                             sFrom->tableRef )
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
            /* Nothing to do */
        }
    }
    else
    {
        /* Nothing to do */
    }

    IDE_TEST( convertAnsiInnerJoin( aStatement, aSFWGH )
              != IDE_SUCCESS );

    for (sFrom = aSFWGH->from; sFrom != NULL; sFrom = sFrom->next)
    {
        if (sFrom->joinType != QMS_NO_JOIN) // INNER, OUTER JOIN
        {
            IDE_TEST(validateQmsFromWithOnCond( aQuerySet,
                                                aSFWGH,
                                                sFrom,
                                                aStatement,
                                                MTC_COLUMN_NOTNULL_TRUE) // PR-13597
                     != IDE_SUCCESS);
        }
        else
        {
            IDE_TEST(validateQmsTableRef( aStatement,
                                          aSFWGH,
                                          sFrom->tableRef,
                                          aSFWGH->lflag,
                                          MTC_COLUMN_NOTNULL_TRUE ) // PR-13597
                     != IDE_SUCCESS);

            // Table Map ����
            QC_SHARED_TMPLATE(aStatement)->tableMap[sFrom->tableRef->table].from = sFrom;

            // FROM ���� ���� dependencies ����
            qtc::dependencyClear( & sFrom->depInfo );
            qtc::dependencySet( sFrom->tableRef->table, & sFrom->depInfo );
            
            // PROJ-1718 Semi/anti join�� ���õ� dependency �ʱ�ȭ
            qtc::dependencyClear( & sFrom->semiAntiJoinDepInfo );

            IDE_TEST( qmsPreservedTable::addTable( aStatement,
                                                   aSFWGH,
                                                   sFrom->tableRef )
                      != IDE_SUCCESS );

            if( (sFrom->tableRef->tableInfo->tableType == QCM_FIXED_TABLE) ||
                (sFrom->tableRef->tableInfo->tableType == QCM_DUMP_TABLE) ||
                (sFrom->tableRef->tableInfo->tableType == QCM_PERFORMANCE_VIEW) ||
                ( sFrom->tableRef->remoteTable != NULL ) || /* PROJ-1832 */
                ( sFrom->tableRef->mShardObjInfo != NULL ) )
            {
                // for FT/PV, no privilege validation, only X$, V$ name validation
                /* for database link */

                /* PROJ-2462 Result Cache */
                if ( aQuerySet != NULL )
                {
                    aQuerySet->lflag &= ~QMV_QUERYSET_RESULT_CACHE_INVALID_MASK;
                    aQuerySet->lflag |= QMV_QUERYSET_RESULT_CACHE_INVALID_TRUE;
                }
                else
                {
                    /* Nothing to do */
                }
            }
            else
            {
                //check grant
                if ( (sFrom->tableRef->view == NULL) ||
                     ((sFrom->tableRef->flag) & QMS_TABLE_REF_CREATED_VIEW_MASK)
                     == QMS_TABLE_REF_CREATED_VIEW_TRUE)
                {
                    IDE_TEST( qdpRole::checkDMLSelectTablePriv(
                                  aStatement,
                                  sFrom->tableRef->tableInfo->tableOwnerID,
                                  sFrom->tableRef->tableInfo->privilegeCount,
                                  sFrom->tableRef->tableInfo->privilegeInfo,
                                  ID_FALSE,
                                  NULL,
                                  NULL )
                              != IDE_SUCCESS );
                        
                    // environment�� ���
                    IDE_TEST( qcgPlan::registerPlanPrivTable(
                                  aStatement,
                                  QCM_PRIV_ID_OBJECT_SELECT_NO,
                                  sFrom->tableRef->tableInfo )
                              != IDE_SUCCESS );
                }
                else
                {
                    // Nothing to do.
                }

                /* PROJ-2462 Result Cache */
                if ( ( sFrom->tableRef->tableInfo->temporaryInfo.type != QCM_TEMPORARY_ON_COMMIT_NONE ) &&
                     ( aQuerySet != NULL ) )
                {
                    aQuerySet->lflag &= ~QMV_QUERYSET_RESULT_CACHE_INVALID_MASK;
                    aQuerySet->lflag |= QMV_QUERYSET_RESULT_CACHE_INVALID_TRUE;
                }
                else
                {
                    /* Nothing to do */
                }
            }
        }
    }

    //---------------------------------------------------
    // validation of Hints
    //---------------------------------------------------

    IDE_TEST(validateHints(aStatement, aSFWGH) != IDE_SUCCESS);

    //---------------------------------------------------
    // validation of target
    //---------------------------------------------------

    if ( aQuerySet != NULL )
    {
        aQuerySet->processPhase = QMS_VALIDATE_TARGET;
    }
    else
    {
        // nothing to do 
    }
    aSFWGH->validatePhase = QMS_VALIDATE_TARGET;

    // BUG-41311 table function
    if ( ( aSFWGH->lflag & QMV_SFWGH_TABLE_FUNCTION_VIEW_MASK )
         == QMV_SFWGH_TABLE_FUNCTION_VIEW_TRUE )
    {
        IDE_TEST( qmvTableFuncTransform::expandTarget( aStatement,
                                                       aSFWGH )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    // PROJ-2188 Support pivot clause 
    if ( ( aSFWGH->lflag & QMV_SFWGH_PIVOT_MASK )
         == QMV_SFWGH_PIVOT_TRUE )
    {
        IDE_TEST(qmvPivotTransform::expandPivotDummy( aStatement,
                                                      aSFWGH )
                 != IDE_SUCCESS);
    }
    else
    {
        /* Nothing to do */
    }

    // PROJ-2523 Unpivot clause
    // Unpivot target�� Ȯ���Ѵ�.
    if ( ( aSFWGH->lflag & QMV_SFWGH_UNPIVOT_MASK )
         == QMV_SFWGH_UNPIVOT_TRUE )
    {
        IDE_TEST( qmvUnpivotTransform::expandUnpivotTarget( aStatement,
                                                            aSFWGH )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    IDE_TEST(validateQmsTarget(aStatement, aQuerySet, aSFWGH) != IDE_SUCCESS);

    // right outer join�� left outer join���� ��ȯ
    // (expand asterisk ������ validate target�Ŀ� ��ġ�Ѵ�.)
    for (sFrom = aSFWGH->from; sFrom != NULL; sFrom = sFrom->next)
    {
        IDE_TEST(validateJoin(aStatement, sFrom, aSFWGH) != IDE_SUCCESS);
    }

    //---------------------------------------------------
    // validation of WHERE clause
    //---------------------------------------------------

    if ( aQuerySet != NULL )
    {
        aQuerySet->processPhase = QMS_VALIDATE_WHERE;
    }
    else
    {
        // nothing to do 
    }
    aSFWGH->validatePhase = QMS_VALIDATE_WHERE;

    if (aSFWGH->where != NULL)
    {
        IDE_TEST(validateWhere(aStatement, aQuerySet, aSFWGH) != IDE_SUCCESS);
    }

    //---------------------------------------------------
    // validation of Hierarchical clause
    //---------------------------------------------------

    if ( aQuerySet != NULL )
    {
        aQuerySet->processPhase = QMS_VALIDATE_HIERARCHY;
    }
    else
    {
        // nothing to do 
    }
    aSFWGH->validatePhase = QMS_VALIDATE_HIERARCHY;

    if (aSFWGH->hierarchy != NULL)
    {
        IDE_TEST(validateHierarchy(aStatement, aQuerySet, aSFWGH)
                 != IDE_SUCCESS);
    }

    //---------------------------------------------------
    // validation of GROUP clause
    //---------------------------------------------------

    if ( aQuerySet != NULL )
    {
        aQuerySet->processPhase = QMS_VALIDATE_GROUPBY;
    }
    else
    {
        // nothing to do 
    }
    aSFWGH->validatePhase = QMS_VALIDATE_GROUPBY;

    if (aSFWGH->group != NULL)
    {
        IDE_TEST(validateGroupBy(aStatement, aQuerySet, aSFWGH) != IDE_SUCCESS);
    }

    //---------------------------------------------------
    // validation of HAVING clause
    //---------------------------------------------------

    if ( aQuerySet != NULL )
    {
        aQuerySet->processPhase = QMS_VALIDATE_HAVING;;
    }
    else
    {
        // nothing to do 
    }
    aSFWGH->validatePhase = QMS_VALIDATE_HAVING;

    if (aSFWGH->having != NULL)
    {
        IDE_TEST(validateHaving(aStatement, aQuerySet, aSFWGH) != IDE_SUCCESS);
    }

    // PROJ-2415 Grouping Sets Clause
    if ( ( aSFWGH->group != NULL ) &&
         ( ( ( aSFWGH->lflag & QMV_SFWGH_GBGS_TRANSFORM_MASK )
             == QMV_SFWGH_GBGS_TRANSFORM_MIDDLE ) ||
           ( ( aSFWGH->lflag & QMV_SFWGH_GBGS_TRANSFORM_MASK )
             == QMV_SFWGH_GBGS_TRANSFORM_BOTTOM ) ) )
    {
        // PROJ-2415 Grouping Sets Clause
        IDE_TEST( qmvGBGSTransform::removeNullGroupElements( aSFWGH ) != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    //---------------------------------------------------
    // finalize
    //---------------------------------------------------

    if ( aQuerySet != NULL )
    {
        aQuerySet->processPhase = QMS_VALIDATE_FINAL;
    }
    else
    {
        // nothing to do 
    }
    aSFWGH->validatePhase = QMS_VALIDATE_FINAL;

    // Because aggsDepth1 and aggsDepth2 is made in estimate,
    // the following code is checked after validation all SFWGH estimate.
    if (aSFWGH->group == NULL)
    {
        // If a SELECT statement contains SET functions without aGROUP BY, or
        // If a SELECT statement contains HAVING without aGROUP BY,
        //     the select list can't include any references to columns
        //     unless those references are used with a set function.
        if ( ( aSFWGH->aggsDepth1 != NULL ) || ( aSFWGH->having != NULL ) )
        {
            for (sTarget = aSFWGH->target;
                 sTarget != NULL;
                 sTarget = sTarget->next)
            {
                IDE_TEST(qmvQTC::isAggregateExpression( aStatement,
                                                        aSFWGH,
                                                        sTarget->targetColumn)
                         != IDE_SUCCESS);
            }
        }
    }

    if (aSFWGH->aggsDepth2 != NULL)
    {
        IDE_TEST_RAISE(aSFWGH->group == NULL, ERR_NESTED_AGGR_WITHOUT_GROUP);

        if (aSFWGH->having != NULL)
        {
            IDE_TEST(qmvQTC::haveNotNestedAggregate( aStatement,
                                                     aSFWGH,
                                                     aSFWGH->having)
                     != IDE_SUCCESS);
        }
    }

    // check SELECT FOR UPDATE
    if ( aSFWGH->aggsDepth1 != NULL )
    {
        IDE_TEST_RAISE(sParseTree->forUpdate != NULL,
                       ERR_AGGREGATE_ON_FORUPDATE);

        // PROJ-2582 recursive with
        IDE_TEST_RAISE( ( aQuerySet->lflag & QMV_QUERYSET_RECURSIVE_VIEW_MASK )
                        == QMV_QUERYSET_RECURSIVE_VIEW_RIGHT,
                        ERR_AGGREGATE_NOT_ALLOWED_RECURSIVE_VIEW );
    }
    else
    {
        /* Nothing to do */
    }

    /* PROJ-2361 */
    if ( ( aSFWGH->aggsDepth1 != NULL ) && ( aSFWGH->aggsDepth2 == NULL ) )
    {
        IDE_TEST( qmvAvgTransform::doTransform( aStatement,
                                                aQuerySet,
                                                aSFWGH )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    // PROJ-2204 Join Update, Delete
    IDE_TEST( qmsPreservedTable::find( aStatement,
                                       aSFWGH )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_AGGREGATE_ON_FORUPDATE)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QMV_AGGREGATE_ON_FORUPDATE));
    }
    IDE_EXCEPTION(ERR_NESTED_AGGR_WITHOUT_GROUP)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QMV_NESTED_AGGR_WITHOUT_GROUP));
    }
    IDE_EXCEPTION(ERR_DISTINCT_SETFUNCTION_ON_FORUPDATE)
    {
        IDE_SET(ideSetErrorCode(
                    qpERR_ABORT_QMV_DISTINCT_SETFUNCTION_ON_FORUPDATE));
    }
    IDE_EXCEPTION(ERR_JOIN_ON_FORUPDATE)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QMV_JOIN_ON_FORUPDATE));
    }
    IDE_EXCEPTION( ERR_DISTINCT_NOT_ALLOWED_RECURSIVE_VIEW )
    {
        IDE_SET(ideSetErrorCode( qpERR_ABORT_QMV_OPERATION_NOT_ALLOWED_RECURSIVE_VIEW,
                                 "DISTINCT") );
    }
    IDE_EXCEPTION( ERR_AGGREGATE_NOT_ALLOWED_RECURSIVE_VIEW )
    {
        IDE_SET(ideSetErrorCode( qpERR_ABORT_QMV_OPERATION_NOT_ALLOWED_RECURSIVE_VIEW,
                                 "aggregate Function") );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmvQuerySet::validateGroupOneColumn( qcStatement     * aStatement,
                                            qmsQuerySet     * aQuerySet,
                                            qmsSFWGH        * aSFWGH,
                                            qtcNode         * aNode )
{
    mtcColumn         * sColumn;
    qcuSqlSourceInfo    sqlInfo;

    IDU_FIT_POINT_FATAL( "qmvQuerySet::validateGroupOneColumn::__FT__" );

    /* PROJ-2197 PSM Renewal */
    aNode->lflag |= QTC_NODE_COLUMN_CONVERT_TRUE;
    IDE_TEST(qtc::estimate(
                 aNode,
                 QC_SHARED_TMPLATE(aStatement),
                 aStatement,
                 aQuerySet,
                 aSFWGH,
                 NULL )
             != IDE_SUCCESS);

    if ( ( aNode->node.lflag & MTC_NODE_DML_MASK )
         == MTC_NODE_DML_UNUSABLE )
    {
        sqlInfo.setSourceInfo( aStatement,
                               & aNode->position );
        IDE_RAISE( ERR_USE_CURSOR_ATTR );
    }

    if ( QTC_HAVE_AGGREGATE( aNode ) == ID_TRUE )
    {
        sqlInfo.setSourceInfo( aStatement,
                               & aNode->position );
        IDE_RAISE( ERR_NOT_ALLOWED_AGGREGATION );
    }

    // PROJ-1492
    // BUG-42041
    if ( ( MTC_NODE_IS_DEFINED_TYPE( & aNode->node ) == ID_FALSE ) &&
         ( aStatement->calledByPSMFlag == ID_FALSE ) )
    {
        sqlInfo.setSourceInfo( aStatement,
                               & aNode->position );
        IDE_RAISE( ERR_NOT_ALLOW_HOSTVAR );
    }

    // subquery not allowed here.
    if ( ( aNode->lflag & QTC_NODE_SUBQUERY_MASK )
         == QTC_NODE_SUBQUERY_EXIST )
    {
        sqlInfo.setSourceInfo( aStatement,
                               & aNode->position );
        IDE_RAISE( ERR_NOT_ALLOW_SUBQUERY );
    }

    // BUG-14094
    if ( ( aNode->node.lflag & MTC_NODE_OPERATOR_MASK )
         == MTC_NODE_OPERATOR_LIST )
    {
        sqlInfo.setSourceInfo( aStatement,
                               & aNode->position );
        IDE_RAISE( ERR_NOT_APPLICABLE_TYPE_IN_GROUP_BY );
    }
    else
    {
        // Nothing to do.
    }

    // BUG-14094
    if ( ( aNode->node.lflag & MTC_NODE_LOGICAL_CONDITION_MASK )
         == MTC_NODE_LOGICAL_CONDITION_TRUE )
    {
        sqlInfo.setSourceInfo( aStatement,
                               & aNode->position );
        IDE_RAISE( ERR_NOT_APPLICABLE_TYPE_IN_GROUP_BY );
    }
    else
    {
        // Nothing to do.
    }

    // BUG-14094
    if ( ( aNode->node.lflag & MTC_NODE_COMPARISON_MASK )
         == MTC_NODE_COMPARISON_TRUE )
    {
        sqlInfo.setSourceInfo( aStatement,
                               & aNode->position );
        IDE_RAISE( ERR_NOT_APPLICABLE_TYPE_IN_GROUP_BY );
    }
    else
    {
        // Nothing to do.
    }

    // PROJ-1362
    if ( ( aNode->lflag & QTC_NODE_BINARY_MASK )
         == QTC_NODE_BINARY_EXIST )
    {
        sqlInfo.setSourceInfo( aStatement,
                               & aNode->position );
        IDE_RAISE( ERR_NOT_ALLOWED_INCOMPARABLE_DATA_TYPE );
    }
    else
    {
        // Nothing to do.
    }

    // BUG-15896
    // BUG-24133
    sColumn = QTC_TMPL_COLUMN( QC_SHARED_TMPLATE(aStatement),
                               aNode );
                
    if ( ( sColumn->module->id == MTD_BOOLEAN_ID ) ||
         ( sColumn->module->id == MTD_ROWTYPE_ID ) ||
         ( sColumn->module->id == MTD_RECORDTYPE_ID ) ||
         ( sColumn->module->id == MTD_ASSOCIATIVE_ARRAY_ID ) || 
         ( sColumn->module->id == MTD_REF_CURSOR_ID ) )
    {
        sqlInfo.setSourceInfo( aStatement,
                               & aNode->position );
        IDE_RAISE( ERR_NOT_APPLICABLE_TYPE_IN_GROUP_BY );
    }
    else
    {
        // Nothing to do.
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_NOT_ALLOWED_AGGREGATION)
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QCV_NOT_ALLOWED_AGGREGATION,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION(ERR_USE_CURSOR_ATTR);
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QSV_USING_ATTR_IN_INAPPROPRIATE_CLAUSE,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION( ERR_NOT_ALLOW_HOSTVAR );
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QMV_NOT_ALLOWED_HOSTVAR,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION( ERR_NOT_ALLOW_SUBQUERY );
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QDB_NOT_ALLOWED_SUBQUERY,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION( ERR_NOT_APPLICABLE_TYPE_IN_GROUP_BY );
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QMV_NOT_APPLICABLE_TYPE_IN_GROUP_BY,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION(ERR_NOT_ALLOWED_INCOMPARABLE_DATA_TYPE)
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QMV_NOT_ALLOWED_INCOMPARABLE_DATA_TYPE,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC qmvQuerySet::validateHints( qcStatement* aStatement,
                                   qmsSFWGH   * aSFWGH)
{
/***********************************************************************
 *
 * Description :
 *
 *
 * Implementation :
 *
 ***********************************************************************/

    qmsParseTree            * sParseTree;
    qmsViewOptHints         * sViewOpt;
    qmsJoinMethodHints      * sJoin;
    qmsLeadingHints         * sLeading;
    qmsTableAccessHints     * sAccess;
    qmsTableAccessHints     * sCurAccess = NULL; // To Fix BUG-10465, 10449
    qmsHintTables           * sHintTable;
    qmsHintIndices          * sHintIndex;
    qmsPushPredHints        * sPushPredHint;
    qmsPushPredHints        * sValidPushPredHint = NULL;
    qmsPushPredHints        * sLastPushPredHint  = NULL;
    qmsNoMergeHints         * sNoMergeHint;
    qmsFrom                 * sFrom;
    qcmTableInfo            * sTableInfo;
    qcmIndex                * sIndex;
    UInt                      i;
    idBool                    sIsValid;

    IDU_FIT_POINT_FATAL( "qmvQuerySet::validateHints::__FT__" );

    IDE_FT_ASSERT( aSFWGH != NULL );

    if ( aSFWGH->hints == NULL )
    {
        // ����ڰ� ��Ʈ�� ������� ���� ��� �ʱⰪ���� ��Ʈ ����ü �Ҵ�

        IDE_TEST ( STRUCT_ALLOC( QC_QMP_MEM(aStatement),
                                 qmsHints,
                                 &(aSFWGH->hints) ) != IDE_SUCCESS);

        QCP_SET_INIT_HINTS(aSFWGH->hints);
    }
    else
    {
        // Nothing to do.
    }

    // PROJ-2638
    // shard �� ��� ������ hint �� ������ memory temp �� ����Ѵ�.
    if ( aSFWGH->hints->interResultType == QMO_INTER_RESULT_TYPE_NOT_DEFINED )
    {
        for ( sFrom = aSFWGH->from; sFrom != NULL; sFrom = sFrom->next )
        {
            if ( sFrom->tableRef != NULL )
            {
                if ( sFrom->tableRef->view != NULL )
                {
                    if ( ( sFrom->tableRef->view->myPlan->parseTree->stmtShard !=
                           QC_STMT_SHARD_NONE ) &&
                         ( sFrom->tableRef->view->myPlan->parseTree->stmtShard !=
                           QC_STMT_SHARD_META ) )
                    {
                        aSFWGH->hints->interResultType = QMO_INTER_RESULT_TYPE_MEMORY;
                        break;
                    }
                    else
                    {
                        /* Nothing to do. */
                    }
                }
                else
                {
                    /* Nothing to do. */
                }
            }
            else
            {
                /* Nothing to do. */
            }
        }
    }
    else
    {
        /* Nothing to do. */
    }

    /*
     *  PROJ-2206 viewMaterialize
     *  ���� from�� view�� ���� �ϰ� view�� MATERIALIZE, NO_MATERIALIZE ��Ʈ��
     *  ����Ǿ����� �Ǵ��ؼ� QMO_VIEW_OPT_TYPE_FORCE_VMTR,
     *  QMO_VIEW_OPT_TYPE_PUSH �� ������ �ش�.
     */
    for (sFrom = aSFWGH->from;
         sFrom != NULL;
         sFrom = sFrom->next)
    {
        if ( sFrom->tableRef != NULL )
        {
            if ( sFrom->tableRef->view != NULL )
            {
                // PROJ-2638
                // shard �� ��� view materialize hint �� �����Ѵ�.
                if ( ( sFrom->tableRef->view->myPlan->parseTree->stmtShard
                       == QC_STMT_SHARD_NONE ) ||
                     ( sFrom->tableRef->view->myPlan->parseTree->stmtShard
                       == QC_STMT_SHARD_META ) )
                {
                    sParseTree = (qmsParseTree *)(sFrom->tableRef->view->myPlan->parseTree);
                    IDE_FT_ASSERT( sParseTree != NULL );

                    if ( sParseTree->querySet != NULL )
                    {
                        if ( sParseTree->querySet->SFWGH != NULL )
                        {
                            if ( sParseTree->querySet->SFWGH->hints != NULL )
                            {
                                switch ( sParseTree->querySet->SFWGH->hints->viewOptMtrType )
                                {
                                    case QMO_VIEW_OPT_MATERIALIZE:
                                        sFrom->tableRef->viewOptType = QMO_VIEW_OPT_TYPE_FORCE_VMTR;
                                        sFrom->tableRef->noMergeHint = ID_TRUE;
                                        break;

                                    case QMO_VIEW_OPT_NO_MATERIALIZE:
                                        sFrom->tableRef->viewOptType = QMO_VIEW_OPT_TYPE_PUSH;
                                        sFrom->tableRef->noMergeHint = ID_TRUE;
                                        break;

                                    default:
                                        break;
                                }
                            }
                            else
                            {
                                /* Nothing to do. */
                            }
                        }
                        else
                        {
                            /* Nothing to do. */
                        }
                    }
                    else
                    {
                        /* Nothing to do. */
                    }
                }
                else
                {
                    /* Nothing to do. */
                }
            }
            else
            {
                /* Nothing to do. */
            }
        }
        else
        {
            /* Nothing to do. */
        }
    }

    // validation of VIEW OPT
    for (sViewOpt = aSFWGH->hints->viewOptHint;
         sViewOpt != NULL;
         sViewOpt = sViewOpt->next)
    {
        sHintTable = sViewOpt->table;

        sHintTable->table = NULL;
        
        // search table
        for (sFrom = aSFWGH->from;
             (sFrom != NULL) && (sHintTable->table == NULL);
             sFrom = sFrom->next)
        {
            findHintTableInFromClause(aStatement, sFrom, sHintTable);

            // PROJ-2638
            // shard �� ��� view optimize hint �� �����Ѵ�.
            if ( sHintTable->table != NULL )
            {
                if ( sHintTable->table->tableRef->view != NULL )
                {
                    if ( ( sHintTable->table->tableRef->view->myPlan->parseTree->stmtShard
                           != QC_STMT_SHARD_NONE ) &&
                         ( sHintTable->table->tableRef->view->myPlan->parseTree->stmtShard
                           != QC_STMT_SHARD_META ) )
                    {
                        sHintTable->table = NULL;
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

            // Table is found.
            if ( sHintTable->table != NULL )
            {
                sHintTable->table->tableRef->viewOptType = sViewOpt->viewOptType;

                if( sHintTable->table->tableRef->view != NULL )
                {
                    // PROJ-1413
                    // hint���� ������ view�� merge���� ����
                    sHintTable->table->tableRef->noMergeHint = ID_TRUE;
                }
                else
                {
                    // Nothing to do.
                }
            }
        }
    }

    // PROJ-1495
    // validation of PUSH_PRED hint

    for( sPushPredHint = aSFWGH->hints->pushPredHint;
         sPushPredHint != NULL;
         sPushPredHint = sPushPredHint->next )
    {
        sHintTable = sPushPredHint->table;

        sHintTable->table = NULL;
        
        // 1. search table
        for( sFrom = aSFWGH->from;
             ( sFrom != NULL ) && ( sHintTable->table == NULL );
             sFrom = sFrom->next )
        {
            findHintTableInFromClause(aStatement, sFrom, sHintTable);

            if( sHintTable->table != NULL )
            {
                if( sHintTable->table->tableRef->view != NULL )
                {
                    // �ش� view table�� ã��

                    // PROJ-1413
                    // hint���� ������ view�� merge���� ����
                    sHintTable->table->tableRef->noMergeHint = ID_TRUE;

                    break;
                }
                else
                {
                    // view�� �ƴ� base table�� �߸��� hint
                    sHintTable->table = NULL;
                }
            }
            else
            {
                // �ش� view table�� ������ �߸��� hint
                // Nothing To Do
            }
        }

        // 2. PUSH_PRED hint view�� ���ռ� �˻�
        if( sHintTable->table != NULL )
        {
            sIsValid = ID_TRUE;

            IDE_TEST(
                validatePushPredView( aStatement,
                                      sHintTable->table->tableRef,
                                      & sIsValid ) != IDE_SUCCESS );

            if( sIsValid == ID_TRUE )
            {
                // Nothing To Do
            }
            else
            {
                // VIEW�� hint�� �����ϱ⿡ ������
                sHintTable->table = NULL;
            }
        }
        else
        {
            // Nothing To Do
        }

        if( sHintTable->table != NULL )
        {
            if( sValidPushPredHint == NULL )
            {
                sValidPushPredHint = sPushPredHint;
                sLastPushPredHint  = sValidPushPredHint;
            }
            else
            {
                sLastPushPredHint->next = sPushPredHint;
                sLastPushPredHint = sLastPushPredHint->next;
            }
        }
        else
        {
            // Nothing To Do
        }
    }

    if( sValidPushPredHint != NULL )
    {
        sLastPushPredHint->next = NULL;
    }
    else
    {
        // Nothing To Do
    }
    
    // PROJ-2582 recursive with
    // recursive view�� �ִ� right subquery�� ��� push pred hint�� �����Ѵ�.
    if ( aSFWGH->recursiveViewID != ID_USHORT_MAX )
    {
        aSFWGH->hints->pushPredHint = NULL;
    }
    else
    {
        aSFWGH->hints->pushPredHint = sValidPushPredHint;
    }
    
    // PROJ-2582 recursive with    
    // recursive view�� �ִ� right subquery�� ��� ordered hint�� �����Ѵ�.
    if ( ( aSFWGH->recursiveViewID != ID_USHORT_MAX ) &&
         ( aSFWGH->hints->joinOrderType == QMO_JOIN_ORDER_TYPE_ORDERED ) )
    {
        aSFWGH->hints->joinOrderType = QMO_JOIN_ORDER_TYPE_OPTIMIZED;
    }
    else
    {
        // Nothing to do.
    }
    
    // validation of JOIN METHOD
    for (sJoin = aSFWGH->hints->joinMethod;
         sJoin != NULL;
         sJoin = sJoin->next)
    {
        for (sHintTable = sJoin->joinTables;
             sHintTable != NULL;
             sHintTable = sHintTable->next)
        {
            sHintTable->table = NULL;
            
            // search table
            for (sFrom = aSFWGH->from;
                 (sFrom != NULL) && (sHintTable->table == NULL);
                 sFrom = sFrom->next)
            {
                findHintTableInFromClause(aStatement, sFrom, sHintTable);
            }

            // To Fix PR-8045
            // Join Method�� dependencies�� �����ؾ� ��
            if ( sHintTable->table != NULL )
            {
                IDE_TEST( qtc::dependencyOr( & sJoin->depInfo,
                                             & sHintTable->table->depInfo,
                                             & sJoin->depInfo )
                          != IDE_SUCCESS );
            }
            else
            {
                // �߸��� Hint��
                // Nothing To Do
            }
        }
    }

    // BUG-42447 leading hint support
    // validation leading hints
    sLeading = aSFWGH->hints->leading;
    if ( sLeading != NULL )
    {
        for ( sHintTable = sLeading->mLeadingTables;
              sHintTable != NULL;
              sHintTable = sHintTable->next )
        {
            sHintTable->table = NULL;

            // search table
            for ( sFrom = aSFWGH->from;
                  (sFrom != NULL) && (sHintTable->table == NULL);
                  sFrom = sFrom->next )
            {
                findHintTableInFromClause(aStatement, sFrom, sHintTable);
            }

            // BUG-47410 leading ��Ʈ�� lateral view�� �ִ� ��� �����Ѵ�.
            if ( sHintTable->table != NULL )
            {
                if ( (sHintTable->table->tableRef->flag & QMS_TABLE_REF_LATERAL_VIEW_MASK) ==
                     QMS_TABLE_REF_LATERAL_VIEW_TRUE )
                {
                    aSFWGH->hints->leading = NULL;
                    aSFWGH->hints->joinOrderType = QMO_JOIN_ORDER_TYPE_OPTIMIZED; 
                    break;
                }
            }
        }
    }

    // validation of TABLE ACCESS
    // To Fix BUG-9525
    sAccess = aSFWGH->hints->tableAccess;
    while ( sAccess != NULL )
    {
        sHintTable = sAccess->table;

        if (sHintTable != NULL)
        {
            sHintTable->table = NULL;
            
            // search table
            for (sFrom = aSFWGH->from;
                 (sFrom != NULL) && (sHintTable->table == NULL);
                 sFrom = sFrom->next)
            {
                findHintTableInFromClause(aStatement, sFrom, sHintTable);

                if( sHintTable->table != NULL )
                {
                    if( sHintTable->table->tableRef->view != NULL )
                    {
                        // PROJ-1413
                        // hint���� ������ view�� merge���� ����
                        sHintTable->table->tableRef->noMergeHint = ID_TRUE;
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

            if ( sHintTable->table != NULL )
            {
                // To Fix BUG-9525
                if ( sHintTable->table->tableRef->tableAccessHints == NULL )
                {
                    sHintTable->table->tableRef->tableAccessHints =
                        sCurAccess = sAccess;
                }
                else
                {
                    for ( sCurAccess =
                              sHintTable->table->tableRef->tableAccessHints;
                          sCurAccess->next != NULL;
                          sCurAccess = sCurAccess->next ) ;
                    
                    sCurAccess->next = sAccess;
                    sCurAccess = sAccess;
                }
                
                for (sHintIndex = sAccess->indices;
                     sHintIndex != NULL;
                     sHintIndex = sHintIndex->next)
                {
                    // search index
                    sTableInfo = sHintTable->table->tableRef->tableInfo;
                    
                    if (sTableInfo != NULL)
                    {
                        for (i = 0; i < sTableInfo->indexCount; i++)
                        {
                            sIndex = &(sTableInfo->indices[i]);
                            
                            if (idlOS::strMatch(
                                    sHintIndex->indexName.stmtText +
                                    sHintIndex->indexName.offset,
                                    sHintIndex->indexName.size,
                                    sIndex->name,
                                    idlOS::strlen(sIndex->name) ) == 0)
                            {
                                sHintIndex->indexPtr = sIndex;
                                break;
                            }
                        }
                    }
                }

                if ( sAccess->count < 1 )
                {
                    sAccess->count = 1;
                }
                else
                {
                    // Nothing to do.
                }

                if ( ( sAccess->id < 1 ) || ( sAccess->id > sAccess->count ) )
                {
                    sAccess->id = 1;
                }
                else
                {
                    // Nothing to do.
                }
            }
        }
        else // parsing error
        {
            break;
        }

        // To Fix BUG-9525
        sAccess = sAccess->next;
        if ( sCurAccess != NULL )
        {
            sCurAccess->next = NULL;
        }
        else
        {
            // nothing to do
        }
    }

    /* PROJ-1071 Parallel Query */
    validateParallelHint(aStatement, aSFWGH);

    if (aSFWGH->hints->optGoalType == QMO_OPT_GOAL_TYPE_UNKNOWN)
    {
        if ( QCG_GET_SESSION_OPTIMIZER_MODE( aStatement ) == 0 ) // COST
        {
            aSFWGH->hints->optGoalType = QMO_OPT_GOAL_TYPE_ALL_ROWS;
        }
        else // RULE
        {
            aSFWGH->hints->optGoalType = QMO_OPT_GOAL_TYPE_RULE;
        }

        // environment�� ���
        qcgPlan::registerPlanProperty( aStatement,
                                       PLAN_PROPERTY_OPTIMIZER_MODE );
    }

    // PROJ-1473
    if( aSFWGH->hints->materializeType == QMO_MATERIALIZE_TYPE_NOT_DEFINED )
    {
        if( QCU_OPTIMIZER_PUSH_PROJECTION == 1 )
        {
            aSFWGH->hints->materializeType = QMO_MATERIALIZE_TYPE_VALUE;
        }
        else
        {
            aSFWGH->hints->materializeType = QMO_MATERIALIZE_TYPE_RID;
        }
    }
    else
    {
        // Nothing To Do
    }

    // BUG-23780 TEMP_TBS_MEMORY ��Ʈ ���뿩�θ� property�� ����
    if( aSFWGH->hints->interResultType == QMO_INTER_RESULT_TYPE_NOT_DEFINED )
    {
        if( QCG_GET_SESSION_OPTIMIZER_DEFAULT_TEMP_TBS_TYPE( aStatement )  == 1 )
        {
            aSFWGH->hints->interResultType = QMO_INTER_RESULT_TYPE_MEMORY;
        }
        else if( QCG_GET_SESSION_OPTIMIZER_DEFAULT_TEMP_TBS_TYPE( aStatement )  == 2 )
        {
            aSFWGH->hints->interResultType = QMO_INTER_RESULT_TYPE_DISK;
            
        }
        else
        {
            // Nothing To Do
        }

        qcgPlan::registerPlanProperty( aStatement,
                                       PLAN_PROPERTY_OPTIMIZER_DEFAULT_TEMP_TBS_TYPE );
    }
    else
    {
        // Nothing To Do
    }

    // PROJ-1413
    // validation of NO_MERGE hint
    for( sNoMergeHint = aSFWGH->hints->noMergeHint;
         sNoMergeHint != NULL;
         sNoMergeHint = sNoMergeHint->next )
    {
        sHintTable = sNoMergeHint->table;

        // BUG-43536 no_merge() ��Ʈ ����
        if ( sHintTable == NULL )
        {
            continue;
        }
        else
        {
            // nothing to do.
        }

        // no_merge( view ) hints
        sHintTable->table = NULL;

        for( sFrom = aSFWGH->from;
             ( sFrom != NULL ) && ( sHintTable->table == NULL );
             sFrom = sFrom->next )
        {
            findHintTableInFromClause(aStatement, sFrom, sHintTable);

            if( sHintTable->table != NULL )
            {
                if( sHintTable->table->tableRef->view != NULL )
                {
                    // hint���� ������ view�� merge���� ����
                    sHintTable->table->tableRef->noMergeHint = ID_TRUE;
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
    }

    // PROJ-2462 Result Cache
    if ( aSFWGH->hints->topResultCache == ID_TRUE )
    {
        QC_SHARED_TMPLATE(aStatement)->resultCache.flag &= ~QC_RESULT_CACHE_TOP_MASK;
        QC_SHARED_TMPLATE(aStatement)->resultCache.flag |= QC_RESULT_CACHE_TOP_TRUE;
    }
    else
    {
        /* Nothing to do */
    }

    /* PROJ-2462 ResultCache */
    if ( aSFWGH->thisQuerySet != NULL )
    {
        // PROJ-2582 recursive with
        if ( ( ( aSFWGH->thisQuerySet->lflag & QMV_QUERYSET_RECURSIVE_VIEW_MASK )
               == QMV_QUERYSET_RECURSIVE_VIEW_LEFT ) ||
             ( ( aSFWGH->thisQuerySet->lflag & QMV_QUERYSET_RECURSIVE_VIEW_MASK )
               == QMV_QUERYSET_RECURSIVE_VIEW_RIGHT ) )
        {
            // nothing to do
        }
        else
        {
            aSFWGH->thisQuerySet->lflag &= ~QMV_QUERYSET_RESULT_CACHE_MASK;

            if ( aSFWGH->hints->resultCacheType == QMO_RESULT_CACHE_NO )
            {
                aSFWGH->thisQuerySet->lflag |= QMV_QUERYSET_RESULT_CACHE_NO;
            }
            else if ( aSFWGH->hints->resultCacheType == QMO_RESULT_CACHE )
            {
                aSFWGH->thisQuerySet->lflag |= QMV_QUERYSET_RESULT_CACHE;
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

    // PROJ-1436
    IDE_TEST( qmv::validatePlanHints( aStatement,
                                      aSFWGH->hints )
              != IDE_SUCCESS );

    //------------------------------------------------
    // ��Ʈ�� ���� ���� ������ �Ϸ�Ǹ�, �̸� ��������
    // �Ϲ� Table�� ���� ��� ������ ����
    // qmsSFWGH->from�� ���󰡸鼭, ��� �Ϲ�  Table�� ���� ��� ���� ����
    // JOIN�� ���, ���� �Ϲ� ���̺� ��� �˻�...
    // �Ϲ� Table : qmsSFWGH->from->joinType = QMS_NO_JOIN �̰�,
    //              qmsSFWGH->from->tableRef->view == NULL ������ �˻�.
    //------------------------------------------------

    IDE_TEST( qmoStat::getStatInfo4AllBaseTables( aStatement,
                                                  aSFWGH )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

void qmvQuerySet::validateParallelHint(qcStatement* aStatement,
                                       qmsSFWGH   * aSFWGH)
{
    qmsParallelHints* sParallelHint;
    qmsHintTables   * sHintTable;
    qmsFrom         * sFrom;
    UInt              sDegree;

    IDE_DASSERT(aSFWGH != NULL);
    IDE_DASSERT(aSFWGH->hints != NULL);

    for (sParallelHint = aSFWGH->hints->parallelHint;
         sParallelHint != NULL;
         sParallelHint = sParallelHint->next)
    {
        if ((sParallelHint->parallelDegree == 0) ||
            (sParallelHint->parallelDegree > 65535))
        {
            /* invalid hint => ignore */
            continue;
        }
        else
        {
            /*
             * valid hint
             * adjust degree
             */
            sDegree = IDL_MIN(sParallelHint->parallelDegree,
                              QMV_QUERYSET_PARALLEL_DEGREE_LIMIT);
        }

        sHintTable = sParallelHint->table;

        if (sHintTable == NULL)
        {
            /*
             * parallel hint without table name
             * ex) PARALLEL(4)
             */
            setParallelDegreeOnAllTables(aSFWGH->from, sDegree);
        }
        else
        {
            sHintTable->table = NULL;

            for (sFrom = aSFWGH->from;
                 (sFrom != NULL) && (sHintTable->table == NULL);
                 sFrom = sFrom->next)
            {
                findHintTableInFromClause(aStatement, sFrom, sHintTable);

                if (sHintTable->table != NULL)
                {
                    /* found */
                    break;
                }
                else
                {
                    /* keep going */
                }
            }

            if (sHintTable->table != NULL)
            {
                /*
                 * BUG-39173
                 * USER_TABLE �� ���ؼ��� parallel ����
                 */
                if ((sHintTable->table->tableRef->view == NULL) &&
                    (sHintTable->table->tableRef->tableType == QCM_USER_TABLE))
                {
                    /* view �� �ƴ� base table �� ���ؼ��� hint ���� */
                    sHintTable->table->tableRef->mParallelDegree = sDegree;
                }
                else
                {
                    /* View �� parallel degree �� ������� �ʴ´�. */
                }
            }
            else
            {
                /* No matching table */
            }
        }
    }

}

void qmvQuerySet::setParallelDegreeOnAllTables(qmsFrom* aFrom, UInt aDegree)
{
    qmsFrom* sFrom;

    for (sFrom = aFrom; sFrom != NULL; sFrom = sFrom->next)
    {
        if (sFrom->joinType == QMS_NO_JOIN)
        {
            /*
             * BUG-39173
             * USER_TABLE �� ���ؼ��� parallel ����
             */
            if ((sFrom->tableRef->view == NULL) &&
                (sFrom->tableRef->tableType == QCM_USER_TABLE))
            {
                sFrom->tableRef->mParallelDegree = aDegree;
            }
            else
            {
                /* view �� ��� parallel hint ���� */
            }
        }
        else
        {
            /* ansi style join */
            setParallelDegreeOnAllTables(sFrom->left, aDegree);
            setParallelDegreeOnAllTables(sFrom->right, aDegree);
        }
    }

}

void qmvQuerySet::findHintTableInFromClause(
    qcStatement     * aStatement,
    qmsFrom         * aFrom,
    qmsHintTables   * aHintTable)
{
    IDE_RC            sRC;
    qmsFrom         * sTable;
    UInt              sUserID;

    if (aFrom->joinType != QMS_NO_JOIN)
    {
        if (aHintTable->table == NULL)
        {
            findHintTableInFromClause(aStatement, aFrom->left, aHintTable);
        }

        if (aHintTable->table == NULL)
        {
            findHintTableInFromClause(aStatement, aFrom->right, aHintTable);
        }
    }
    else
    {
        sTable = aFrom;

        if (QC_IS_NULL_NAME(aHintTable->userName) == ID_TRUE)
        {
            // search alias table name
            if (idlOS::strMatch(
                    aHintTable->tableName.stmtText + aHintTable->tableName.offset,
                    aHintTable->tableName.size,
                    sTable->tableRef->aliasName.stmtText + sTable->tableRef->aliasName.offset,
                    sTable->tableRef->aliasName.size) == 0 ||
                idlOS::strMatch(
                    aHintTable->tableName.stmtText + aHintTable->tableName.offset,
                    aHintTable->tableName.size,
                    sTable->tableRef->tableName.stmtText + sTable->tableRef->tableName.offset,
                    sTable->tableRef->tableName.size) == 0)
            {
                aHintTable->table = sTable;
            }
        }
        else
        {
            // search real table name
            sRC = qcmUser::getUserID(
                aStatement, aHintTable->userName, &(sUserID));

            if (sRC == IDE_SUCCESS && sTable->tableRef->userID == sUserID)
            {
                if (idlOS::strMatch(
                        aHintTable->tableName.stmtText + aHintTable->tableName.offset,
                        aHintTable->tableName.size,
                        sTable->tableRef->aliasName.stmtText + sTable->tableRef->aliasName.offset,
                        sTable->tableRef->aliasName.size) == 0 ||
                    idlOS::strMatch(
                        aHintTable->tableName.stmtText + aHintTable->tableName.offset,
                        aHintTable->tableName.size,
                        sTable->tableRef->tableName.stmtText + sTable->tableRef->tableName.offset,
                        sTable->tableRef->tableName.size) == 0)
                {
                    aHintTable->table = sTable;
                }
            }
        }
    }

}

IDE_RC qmvQuerySet::validateWhere(
    qcStatement     * aStatement,
    qmsQuerySet     * aQuerySet,
    qmsSFWGH        * aSFWGH)
{
    qcuSqlSourceInfo   sqlInfo;

    IDU_FIT_POINT_FATAL( "qmvQuerySet::validateWhere::__FT__" );

    /* PROJ-2197 PSM Renewal */
    aSFWGH->where->lflag |= QTC_NODE_COLUMN_CONVERT_TRUE;
    IDE_TEST(qtc::estimate(
                 aSFWGH->where,
                 QC_SHARED_TMPLATE(aStatement),
                 aStatement,
                 aQuerySet,
                 aSFWGH,
                 NULL )
             != IDE_SUCCESS);

    if ( ( aSFWGH->where->node.lflag &
           ( MTC_NODE_LOGICAL_CONDITION_MASK | MTC_NODE_COMPARISON_MASK ) )
         == ( MTC_NODE_LOGICAL_CONDITION_FALSE | MTC_NODE_COMPARISON_FALSE ) )
    {
        sqlInfo.setSourceInfo( aStatement,
                               & aSFWGH->where->position );
        IDE_RAISE( ERR_NOT_PREDICATE );
    }

    if ( ( aSFWGH->where->node.lflag & MTC_NODE_DML_MASK )
         == MTC_NODE_DML_UNUSABLE )
    {
        sqlInfo.setSourceInfo( aStatement,
                               & aSFWGH->where->position );
        IDE_RAISE( ERR_USE_CURSOR_ATTR );
    }

    if ( ( aSFWGH->outerHavingCase == ID_FALSE ) &&
         ( QTC_HAVE_AGGREGATE(aSFWGH->where) == ID_TRUE ) )
    {
        sqlInfo.setSourceInfo( aStatement,
                               & aSFWGH->where->position );
        IDE_RAISE( ERR_NOT_ALLOWED_AGGREGATION );
    }

    if ( QTC_HAVE_AGGREGATE2(aSFWGH->where) )
    {
        IDE_RAISE( ERR_TOO_DEEPLY_NESTED_AGGR );
    }

    if ( ( aSFWGH->where->lflag & QTC_NODE_SEQUENCE_MASK )
         == QTC_NODE_SEQUENCE_EXIST )
    {
        sqlInfo.setSourceInfo(
            aStatement,
            & aSFWGH->where->position );
        IDE_RAISE( ERR_USE_SEQUENCE_IN_WHERE );
    }

    if (((aSFWGH->where->lflag & QTC_NODE_SUBQUERY_MASK) ==
         QTC_NODE_SUBQUERY_EXIST) &&
        (aSFWGH->where->lflag & QTC_NODE_COLUMN_RID_MASK) ==
        QTC_NODE_COLUMN_RID_EXIST)
    {
        IDE_RAISE( ERR_PROWID_NOT_SUPPORTED );
    }

    /* Plan Property�� ��� */
    qcgPlan::registerPlanProperty( aStatement,
                                   PLAN_PROPERTY_QUERY_REWRITE_ENABLE );
    
    /* PROJ-1090 Function-based Index */
    if ( QCG_GET_SESSION_QUERY_REWRITE_ENABLE(aStatement) == 1 )
    {
        IDE_TEST( qmsDefaultExpr::applyFunctionBasedIndex(
                      aStatement,
                      aSFWGH->where,
                      aSFWGH->from,
                      &(aSFWGH->where) )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    // BUG-41289 subquery �� where ������ DescribeParamInfo ����
    if ( QC_SHARED_TMPLATE(aStatement)->stmt->myPlan->sBindParamCount > 0 )
    {
        // BUG-15746 DescribeParamInfo ����
        IDE_TEST( qtc::setDescribeParamInfo4Where( QC_SHARED_TMPLATE(aStatement)->stmt,
                                                   aSFWGH->where )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }    

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_NOT_PREDICATE);
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QMV_NOT_PREDICATE,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION(ERR_USE_CURSOR_ATTR);
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QSV_USING_ATTR_IN_INAPPROPRIATE_CLAUSE,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION(ERR_USE_SEQUENCE_IN_WHERE)
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QMV_USE_SEQUENCE_IN_WHERE,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION(ERR_NOT_ALLOWED_AGGREGATION)
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QCV_NOT_ALLOWED_AGGREGATION,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION(ERR_TOO_DEEPLY_NESTED_AGGR)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QMV_TOO_DEEPLY_NESTED_AGGR));
    }
    IDE_EXCEPTION(ERR_PROWID_NOT_SUPPORTED)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QMV_PROWID_NOT_SUPPORTED));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmvQuerySet::validateGroupBy(
    qcStatement     * aStatement,
    qmsQuerySet     * aQuerySet,
    qmsSFWGH        * aSFWGH)
{
    qmsParseTree            * sParseTree;
    qtcNode                 * sExpression;
    qmsConcatElement        * sConcatElement;
    qmsConcatElement        * sTemp;
    qmsTarget               * sTarget;
    qtcNode                 * sNode[2];
    qtcNode                 * sListNode;
    qcNamePosition            sPosition;
    UInt                      sGroupExtCount = 0;

    IDU_FIT_POINT_FATAL( "qmvQuerySet::validateGroupBy::__FT__" );

    sParseTree = (qmsParseTree *) aStatement->myPlan->parseTree;

    // FOR UPDATE clause
    IDE_TEST_RAISE(sParseTree->forUpdate != NULL, ERR_GROUP_ON_FORUPDATE);

    // PROJ-2582 recursive with
    IDE_TEST_RAISE( ( aQuerySet->lflag & QMV_QUERYSET_RECURSIVE_VIEW_MASK )
                    == QMV_QUERYSET_RECURSIVE_VIEW_RIGHT,
                    ERR_OPERATION_NOT_ALLOWED_RECURSIVE_VIEW );

    // PROJ-2415 Grouping Sets Clause    
    if ( ( aSFWGH->lflag & QMV_SFWGH_GBGS_TRANSFORM_MASK )
         != QMV_SFWGH_GBGS_TRANSFORM_NONE )
    {
        // Nested Aggregate Function ���Ұ�
        IDE_TEST_RAISE( aSFWGH->aggsDepth2 != NULL, ERR_NOT_NESTED_AGGR );

        // Over Cluase ���Ұ�
        IDE_TEST_RAISE( aQuerySet->analyticFuncList != NULL, ERR_NOT_WINDOWING );            
    }
    else
    {
        /* Nothing to do */
    }
    
    for (sConcatElement = aSFWGH->group;
         sConcatElement != NULL;
         sConcatElement = sConcatElement->next )
    {
        switch ( sConcatElement->type )
        {
            // PROJ-2415 Grouping Sets Clause
            case QMS_GROUPBY_NULL:
            case QMS_GROUPBY_NORMAL:
                IDE_TEST_RAISE( sConcatElement->arithmeticOrList == NULL,
                                ERR_NOT_SUPPORT_COMPLICATE_GROUP_EXT );
                sExpression = sConcatElement->arithmeticOrList;

                IDE_TEST_RAISE((sExpression->node.lflag & MTC_NODE_FUNCTON_GROUPING_MASK)
                               == MTC_NODE_FUNCTON_GROUPING_TRUE,
                               ERR_NOT_SUPPORT_GROUPING_SETS);

                IDE_TEST( validateGroupOneColumn( aStatement,
                                                  aQuerySet,
                                                  aSFWGH,
                                                  sExpression )
                          != IDE_SUCCESS );

                IDE_TEST_RAISE((sExpression->lflag & QTC_NODE_COLUMN_RID_MASK) ==
                               QTC_NODE_COLUMN_RID_EXIST,
                               ERR_PROWID_NOT_SUPPORTED);
                break;
            case QMS_GROUPBY_ROLLUP:
            case QMS_GROUPBY_CUBE:
                /* PROJ-1353 Rollup, Cube�� Group by ���� 1���� �����ϴ� */
                IDE_TEST_RAISE( sGroupExtCount > 0, ERR_NOT_SUPPORT_COMPLICATE_GROUP_EXT );

                for ( sTemp = sConcatElement->arguments;
                      sTemp != NULL;
                      sTemp = sTemp->next )
                {
                    sExpression = sTemp->arithmeticOrList;

                    IDE_TEST_RAISE((sExpression->node.lflag & MTC_NODE_FUNCTON_GROUPING_MASK)
                                   == MTC_NODE_FUNCTON_GROUPING_TRUE,
                                   ERR_NOT_SUPPORT_GROUPING_SETS);

                    if ( ( sExpression->node.lflag & MTC_NODE_OPERATOR_MASK )
                         == MTC_NODE_OPERATOR_LIST )
                    {
                        for ( sListNode = ( qtcNode * )sExpression->node.arguments;
                              sListNode != NULL;
                              sListNode = ( qtcNode * )sListNode->node.next )
                        {
                            IDE_TEST( validateGroupOneColumn( aStatement,
                                                              aQuerySet,
                                                              aSFWGH,
                                                              sListNode )
                                      != IDE_SUCCESS );
                        }
                    }
                    else
                    {
                        IDE_TEST( validateGroupOneColumn( aStatement,
                                                          aQuerySet,
                                                          aSFWGH,
                                                          sExpression )
                                  != IDE_SUCCESS );
                    }
                    IDE_TEST_RAISE((sExpression->lflag & QTC_NODE_COLUMN_RID_MASK) ==
                                   QTC_NODE_COLUMN_RID_EXIST,
                                   ERR_PROWID_NOT_SUPPORTED);
                }
                sGroupExtCount++;
                break;
            case QMS_GROUPBY_GROUPING_SETS:
                /* PROJ-2415 Grouping Sets Clause�� �ش� ������ ���̻� �߻����� �ʰ� �ȴ�.
                 * ������ Group�� Validation �ÿ� GROUPING SETS�� ���� ���� ������
                 * ���� �� CASE�� ���� �ȴٸ� Internal error�� �߻� ���Ѿ� �Ѵ�.
                 */
                IDE_RAISE( ERR_NOT_SUPPORT_GROUPING_SETS );
                break;
            default:
                IDE_RAISE( ERR_NOT_SUPPORT_COMPLICATE_GROUP_EXT );
        }
    }

    // If a query contains the GROUP BY clause,
    // the select list can contain only the following types of expressions.
    //   - constants (including SYSDATE)
    //   - group functions
    //   - expressions identical to those in the GROUP BY clause
    //   - expressions invlving the above expressions that evaluate to
    //    the same value for all rows in a group
    if ( aSFWGH->aggsDepth2 == NULL )
    {
        for ( sTarget = aSFWGH->target;
              sTarget != NULL;
              sTarget = sTarget->next )
        {
            IDE_TEST( qmvQTC::isGroupExpression( aStatement,
                                                 aSFWGH,
                                                 sTarget->targetColumn,
                                                 ID_TRUE ) // make pass node
                      != IDE_SUCCESS);
        }
    }
    else
    {
        for ( sTarget = aSFWGH->target;
              sTarget != NULL;
              sTarget = sTarget->next )
        {
            IDE_TEST(qmvQTC::isNestedAggregateExpression( aStatement,
                                                          aQuerySet,
                                                          aSFWGH,
                                                          sTarget->targetColumn )
                     != IDE_SUCCESS);
        }
    }

    /* PROJ-1353 Grouping Function Data Address Pseudo Column */
    if ( ( sGroupExtCount > 0 ) && ( aSFWGH->groupingInfoAddr == NULL ) )
    {
        SET_EMPTY_POSITION( sPosition );

        IDE_TEST( qtc::makeColumn( aStatement,
                                   sNode,
                                   NULL,
                                   NULL,
                                   &sPosition,
                                   NULL ) != IDE_SUCCESS );
        aSFWGH->groupingInfoAddr = sNode[0];

        /* make tuple for Connect By Stack Address */
        IDE_TEST(qmvQTC::makeOneTupleForPseudoColumn( aStatement,
                                                      aSFWGH->groupingInfoAddr,
                                                      (SChar *)"BIGINT",
                                                      6 )
                 != IDE_SUCCESS);
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_GROUP_ON_FORUPDATE)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QMV_GROUP_ON_FORUPDATE));
    }
    IDE_EXCEPTION(ERR_PROWID_NOT_SUPPORTED)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QMV_PROWID_NOT_SUPPORTED));
    }
    IDE_EXCEPTION(ERR_NOT_SUPPORT_GROUPING_SETS)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QMV_NOT_SUPPORT_GROUPING_SETS));
    }
    IDE_EXCEPTION(ERR_NOT_SUPPORT_COMPLICATE_GROUP_EXT)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QMV_NOT_SUPPORT_COMPLICATE_GROUP_EXT));
    }
    IDE_EXCEPTION(ERR_NOT_NESTED_AGGR)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QMV_NOT_SUPPORT_NESTED_AGGR));
    }
    IDE_EXCEPTION(ERR_NOT_WINDOWING)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QTC_INVALID_WINDOW_FUNCTION,
                                "ROLLUP, CUBE, GROUPING SETS"));
    }
    IDE_EXCEPTION( ERR_OPERATION_NOT_ALLOWED_RECURSIVE_VIEW )
    {
        IDE_SET(ideSetErrorCode( qpERR_ABORT_QMV_OPERATION_NOT_ALLOWED_RECURSIVE_VIEW,
                                 "GROUP BY" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmvQuerySet::validateHaving(
    qcStatement     * aStatement,
    qmsQuerySet     * aQuerySet,
    qmsSFWGH        * aSFWGH)
{
    qmsParseTree            * sParseTree;
    qcuSqlSourceInfo          sqlInfo;

    IDU_FIT_POINT_FATAL( "qmvQuerySet::validateHaving::__FT__" );

    sParseTree = (qmsParseTree *) aStatement->myPlan->parseTree;

    // FOR UPDATE clause
    IDE_TEST_RAISE(sParseTree->forUpdate != NULL, ERR_GROUP_ON_FORUPDATE);

    /* PROJ-2197 PSM Renewal */
    aSFWGH->having->lflag |= QTC_NODE_COLUMN_CONVERT_TRUE;
    IDE_TEST(qtc::estimate(
                 aSFWGH->having,
                 QC_SHARED_TMPLATE(aStatement),
                 aStatement,
                 aQuerySet,
                 aSFWGH,
                 NULL )
             != IDE_SUCCESS);

    if ( ( aSFWGH->having->node.lflag &
           ( MTC_NODE_LOGICAL_CONDITION_MASK | MTC_NODE_COMPARISON_MASK ) )
         == ( MTC_NODE_LOGICAL_CONDITION_FALSE | MTC_NODE_COMPARISON_FALSE ) )
    {
        sqlInfo.setSourceInfo( aStatement,
                               & aSFWGH->having->position );
        IDE_RAISE( ERR_NOT_PREDICATE );
    }

    if ( ( aSFWGH->having->node.lflag & MTC_NODE_DML_MASK )
         == MTC_NODE_DML_UNUSABLE )
    {
        sqlInfo.setSourceInfo( aStatement,
                               & aSFWGH->having->position );
        IDE_RAISE( ERR_USE_CURSOR_ATTR );
    }

    if (aSFWGH->group != NULL)
    {
        IDE_TEST(qmvQTC::isGroupExpression( aStatement,
                                            aSFWGH,
                                            aSFWGH->having,
                                            ID_TRUE ) // make pass node
                 != IDE_SUCCESS);
    }
    else
    {
        IDE_TEST(qmvQTC::isAggregateExpression( aStatement, aSFWGH, aSFWGH->having )
                 != IDE_SUCCESS);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_GROUP_ON_FORUPDATE)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QMV_GROUP_ON_FORUPDATE));
    }
    IDE_EXCEPTION(ERR_NOT_PREDICATE);
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QMV_NOT_PREDICATE,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION(ERR_USE_CURSOR_ATTR);
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QSV_USING_ATTR_IN_INAPPROPRIATE_CLAUSE,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmvQuerySet::validateHierarchy(
    qcStatement     * aStatement,
    qmsQuerySet     * aQuerySet,
    qmsSFWGH        * aSFWGH)
{
    qcuSqlSourceInfo sqlInfo;
    qtcNode        * sStart;
    qtcNode        * sConnect;

    IDU_FIT_POINT_FATAL( "qmvQuerySet::validateHierarchy::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------
    
    IDE_FT_ASSERT( aSFWGH != NULL );
    IDE_FT_ASSERT( aSFWGH->from != NULL );
    IDE_FT_ASSERT( aSFWGH->hierarchy != NULL );

    sStart    = aSFWGH->hierarchy->startWith;
    sConnect  = aSFWGH->hierarchy->connectBy;
    
    //------------------------------------------
    // validate hierarchy
    //------------------------------------------
    
    // PROJ-2582 recursive with
    IDE_TEST_RAISE( ( aQuerySet->lflag & QMV_QUERYSET_RECURSIVE_VIEW_MASK )
                    == QMV_QUERYSET_RECURSIVE_VIEW_RIGHT,
                    ERR_OPERATION_NOT_ALLOWED_RECURSIVE_VIEW );

    // validation start with clause
    if (sStart != NULL)
    {
        IDE_TEST( searchStartWithPrior( aStatement, sStart )
                  != IDE_SUCCESS );
        /* PROJ-2197 PSM Renewal */
        sStart->lflag |= QTC_NODE_COLUMN_CONVERT_TRUE;
        IDE_TEST(qtc::estimate(
                     sStart,
                     QC_SHARED_TMPLATE(aStatement),
                     aStatement,
                     aQuerySet,
                     aSFWGH,
                     NULL )
                 != IDE_SUCCESS);

        if ( ( sStart->node.lflag &
               ( MTC_NODE_LOGICAL_CONDITION_MASK | MTC_NODE_COMPARISON_MASK ) )
             == ( MTC_NODE_LOGICAL_CONDITION_FALSE | MTC_NODE_COMPARISON_FALSE ) )
        {
            sqlInfo.setSourceInfo( aStatement,
                                   & sStart->position );
            IDE_RAISE( ERR_NOT_PREDICATE );
        }

        // PROJ-1362
        if ( ( sStart->lflag & QTC_NODE_BINARY_MASK )
             == QTC_NODE_BINARY_EXIST )
        {
            sqlInfo.setSourceInfo( aStatement,
                                   & sStart->position );
            IDE_RAISE( ERR_NOT_ALLOWED_INCOMPARABLE_DATA_TYPE );
        }

        /* PROJ-1715 */
        if ( ( sStart->lflag & QTC_NODE_ISLEAF_MASK )
             == QTC_NODE_ISLEAF_EXIST )
        {
            sqlInfo.setSourceInfo( aStatement,
                                   & sStart->position );
            IDE_RAISE( ERR_NOT_USE_ISLEAF_IN_START_WITH );
        }
        else
        {
            /* Nothing to do */
        }
    }

    // validation connect by
    if (sConnect != NULL)
    {
        /* PROJ-2197 PSM Renewal */
        sConnect->lflag |= QTC_NODE_COLUMN_CONVERT_TRUE;
        IDE_TEST(qtc::estimate(
                     sConnect,
                     QC_SHARED_TMPLATE(aStatement),
                     aStatement,
                     NULL,
                     aSFWGH,
                     NULL )
                 != IDE_SUCCESS);

        if ( ( sConnect->node.lflag &
               ( MTC_NODE_LOGICAL_CONDITION_MASK | MTC_NODE_COMPARISON_MASK ) )
             == ( MTC_NODE_LOGICAL_CONDITION_FALSE | MTC_NODE_COMPARISON_FALSE ) )
        {
            sqlInfo.setSourceInfo( aStatement,
                                   & sConnect->position );
            IDE_RAISE( ERR_NOT_PREDICATE );
        }
        
        // PROJ-1362
        if ( ( sConnect->lflag & QTC_NODE_BINARY_MASK )
             == QTC_NODE_BINARY_EXIST )
        {
            sqlInfo.setSourceInfo( aStatement,
                                   & sConnect->position );
            IDE_RAISE( ERR_NOT_ALLOWED_INCOMPARABLE_DATA_TYPE );
        }

        /* PROJ-1715 */
        if ( ( sConnect->lflag & QTC_NODE_ISLEAF_MASK )
             == QTC_NODE_ISLEAF_EXIST )
        {
            sqlInfo.setSourceInfo( aStatement,
                                   & sConnect->position );
            IDE_RAISE( ERR_NOT_USE_ISLEAF_IN_CONNECT_BY );
        }
        else
        {
            /* Nothing to do */
        }
        IDE_TEST( searchConnectByAggr( aStatement, sConnect )
                  != IDE_SUCCESS );

    }

    return IDE_SUCCESS;
    
    IDE_EXCEPTION(ERR_NOT_PREDICATE);
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QMV_NOT_PREDICATE,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION(ERR_NOT_ALLOWED_INCOMPARABLE_DATA_TYPE)
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QMV_NOT_ALLOWED_INCOMPARABLE_DATA_TYPE,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION(ERR_NOT_USE_ISLEAF_IN_START_WITH)
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QMV_USE_ISLEAF_IN_START_WITH,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION(ERR_NOT_USE_ISLEAF_IN_CONNECT_BY)
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QMV_USE_ISLEAF_IN_CONNECT_BY,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION( ERR_OPERATION_NOT_ALLOWED_RECURSIVE_VIEW )
    {
        IDE_SET(ideSetErrorCode( qpERR_ABORT_QMV_OPERATION_NOT_ALLOWED_RECURSIVE_VIEW,
                                 "HIERARCHICAL query") );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmvQuerySet::validateJoin(
    qcStatement     * aStatement,
    qmsFrom         * aFrom,
    qmsSFWGH        * aSFWGH)
{
    qmsFrom           * sFromTemp;


    IDU_FIT_POINT_FATAL( "qmvQuerySet::validateJoin::__FT__" );
    
    if (aFrom->joinType != QMS_NO_JOIN)
    {
        IDE_TEST(validateJoin(aStatement, aFrom->left, aSFWGH)
                 != IDE_SUCCESS);

        IDE_TEST(validateJoin(aStatement, aFrom->right, aSFWGH)
                 != IDE_SUCCESS);

        // BUG-34295 Join ordering ANSI style query
        switch( aFrom->joinType )
        {
            case QMS_NO_JOIN :
                break;
            case QMS_INNER_JOIN :
                aSFWGH->lflag |= QMV_SFWGH_JOIN_INNER;
                break;
            case QMS_FULL_OUTER_JOIN :
                aSFWGH->lflag |= QMV_SFWGH_JOIN_FULL_OUTER;
                break;
            case QMS_LEFT_OUTER_JOIN :
                aSFWGH->lflag |= QMV_SFWGH_JOIN_LEFT_OUTER;
                break;
            case QMS_RIGHT_OUTER_JOIN :
                aSFWGH->lflag |= QMV_SFWGH_JOIN_RIGHT_OUTER;
                break;
            default:
                break;
        }

        if (aFrom->joinType == QMS_RIGHT_OUTER_JOIN)
        {
            aFrom->joinType = QMS_LEFT_OUTER_JOIN;
            sFromTemp = aFrom->right;
            aFrom->right = aFrom->left;
            aFrom->left = sFromTemp;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC qmvQuerySet::addLobLocatorFunc(
    qcStatement     * aStatement,
    qmsTarget       * aTarget )
{
    qmsTarget         * sCurrTarget;
    qtcNode           * sNode[2];
    qtcNode           * sPrevNode = NULL;
    qtcNode           * sExpression;
    mtcColumn         * sMtcColumn;    
    qcuSqlSourceInfo    sqlInfo;

    IDU_FIT_POINT_FATAL( "qmvQuerySet::addLobLocatorFunc::__FT__" );

    // add Lob-Locator function
    for ( sCurrTarget = aTarget;
          sCurrTarget != NULL;
          sCurrTarget = sCurrTarget->next)
    {
        sMtcColumn = QTC_STMT_COLUMN( aStatement,
                                      sCurrTarget->targetColumn );
            
        if ( QTC_IS_COLUMN( aStatement, sCurrTarget->targetColumn ) == ID_TRUE )
        {
            if( sMtcColumn->module->id == MTD_BLOB_ID )
            {
                // get_blob_locator �Լ��� �����.
                IDE_TEST( qtc::makeNode(aStatement,
                                        sNode,
                                        &sCurrTarget->targetColumn->columnName,
                                        &mtfGetBlobLocator )
                          != IDE_SUCCESS );

                // �Լ��� �����Ѵ�.
                sNode[0]->node.arguments = (mtcNode*)sCurrTarget->targetColumn;
                sNode[0]->node.next = sCurrTarget->targetColumn->node.next;
                sNode[0]->node.arguments->next = NULL;

                if ( sPrevNode != NULL )
                {
                    sPrevNode->node.next = (mtcNode*)sNode[0];
                }
                else
                {
                    // Nothing to do.
                }

                // �Լ��� estimate �� �����Ѵ�.
                IDE_TEST( qtc::estimateNodeWithArgument( aStatement,
                                                         sNode[0] )
                          != IDE_SUCCESS );

                // target ��带 �ٲ۴�.
                sCurrTarget->targetColumn = sNode[0];

                sPrevNode = sNode[0];
            }
            else if( sMtcColumn->module->id == MTD_CLOB_ID )
            {
                // get_clob_locator �Լ��� �����.
                IDE_TEST( qtc::makeNode(aStatement,
                                        sNode,
                                        &sCurrTarget->targetColumn->columnName,
                                        &mtfGetClobLocator )
                          != IDE_SUCCESS );

                // �Լ��� �����Ѵ�.
                sNode[0]->node.arguments = (mtcNode*)sCurrTarget->targetColumn;
                sNode[0]->node.next = sCurrTarget->targetColumn->node.next;
                sNode[0]->node.arguments->next = NULL;

                if ( sPrevNode != NULL )
                {
                    sPrevNode->node.next = (mtcNode*)sNode[0];
                }
                else
                {
                    // Nothing to do.
                }

                // �Լ��� estimate �� �����Ѵ�.
                IDE_TEST( qtc::estimateNodeWithArgument( aStatement,
                                                         sNode[0] )
                          != IDE_SUCCESS );

                // target ��带 �ٲ۴�.
                sCurrTarget->targetColumn = sNode[0];

                sPrevNode = sNode[0];
            }
            else
            {
                sPrevNode = sCurrTarget->targetColumn;
            }
        }
        else
        {
            // SP�ܿ��� select target�� lob value�� ���� ��� ����.
            if ( ( aStatement->spvEnv->createProc == NULL ) &&
                 ( aStatement->calledByPSMFlag != ID_TRUE ) &&
                 ( ( sMtcColumn->module->id == MTD_BLOB_ID ) ||
                   ( sMtcColumn->module->id == MTD_CLOB_ID ) ) )
            {
                sExpression = sCurrTarget->targetColumn;

                sqlInfo.setSourceInfo( aStatement,
                                       & sExpression->position );
                IDE_RAISE( ERR_ABORT_QSX_NOT_SUPPORTED_SQLTEXT );
            }
            else
            {
                sPrevNode = sCurrTarget->targetColumn;
            }
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_ABORT_QSX_NOT_SUPPORTED_SQLTEXT );
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QSX_NOT_SUPPORTED_SQLTEXT,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmvQuerySet::validateQmsTarget(qcStatement* aStatement,
                                      qmsQuerySet* aQuerySet,
                                      qmsSFWGH   * aSFWGH)
{
    qmsTarget        * sPrevTarget;
    qmsTarget        * sCurrTarget;
    qmsTarget        * sFirstTarget;
    qmsTarget        * sLastTarget;
    qmsFrom          * sFrom;
    qcuSqlSourceInfo   sqlInfo;
    mtcColumn        * sMtcColumn;

    /* PROJ-2197 PSM Renewal */
    qsProcStmtSql    * sSql;

    /* PROJ-2523 Unpivot clause */
    mtcNode * sMtcNode = NULL;
    UShort    sTupleID = ID_USHORT_MAX;

    IDU_FIT_POINT_FATAL( "qmvQuerySet::validateQmsTarget::__FT__" );

    // �⺻ �ʱ�ȭ
    sPrevTarget  = NULL;
    sCurrTarget  = NULL;
    sFirstTarget = NULL;
    sLastTarget  = NULL;

    // PROJ-2469 Optimize View Materialization
    // Initialization For Re-Validation
    aSFWGH->currentTargetNum = 0; 

    if (aSFWGH->target == NULL) 
    {
        //---------------------
        // SELECT * FROM ...
        // expand asterisk
        //---------------------
        for (sFrom = aSFWGH->from; sFrom != NULL; sFrom = sFrom->next)
        {
            IDE_TEST(expandAllTarget( aStatement,
                                      aQuerySet,
                                      aSFWGH,
                                      sFrom,
                                      NULL,
                                      & sFirstTarget,
                                      & sLastTarget )
                     != IDE_SUCCESS);

            if ( sPrevTarget == NULL )
            {
                aSFWGH->target = sFirstTarget;
            }
            else
            {
                sPrevTarget->next = sFirstTarget;
                sPrevTarget->targetColumn->node.next =
                    (mtcNode *)sFirstTarget->targetColumn;
            }
            sPrevTarget = sLastTarget;
        }
    }
    else
    {
        // BUG-34234
        // target���� ���� ȣ��Ʈ ������ varchar type���� ���� �����Ѵ�.
        IDE_TEST( addCastOper( aStatement,
                               aSFWGH->target )
                  != IDE_SUCCESS );
        
        for ( sCurrTarget = aSFWGH->target, sPrevTarget = NULL;
              sCurrTarget != NULL;
              sCurrTarget = sCurrTarget->next)
        {
            if ( ( sCurrTarget->flag & QMS_TARGET_ASTERISK_MASK )
                 == QMS_TARGET_ASTERISK_TRUE )
            {
                if ( QC_IS_NULL_NAME( sCurrTarget->targetColumn->tableName ) == ID_TRUE )
                {
                    //---------------------
                    // in case of *
                    // expand target
                    //---------------------

                    for (sFrom = aSFWGH->from; sFrom != NULL; sFrom = sFrom->next)
                    {
                        IDE_TEST(expandAllTarget( aStatement,
                                                  aQuerySet,
                                                  aSFWGH,
                                                  sFrom,
                                                  sCurrTarget,
                                                  & sFirstTarget,
                                                  & sLastTarget )
                                 != IDE_SUCCESS);

                        if ( sPrevTarget == NULL )
                        {
                            aSFWGH->target = sFirstTarget;
                        }
                        else
                        {
                            sPrevTarget->next = sFirstTarget;
                            sPrevTarget->targetColumn->node.next =
                                (mtcNode *)sFirstTarget->targetColumn;
                        }
                        sPrevTarget = sLastTarget;
                    }
                }
                else
                {
                    //---------------------
                    // in case of t1.*, u1.t1.*
                    // expand target & get userID
                    //---------------------
                    IDE_TEST( expandTarget( aStatement,
                                            aQuerySet,
                                            aSFWGH,
                                            sCurrTarget,
                                            & sFirstTarget,
                                            & sLastTarget )
                              != IDE_SUCCESS );

                    if ( sPrevTarget == NULL )
                    {
                        aSFWGH->target = sFirstTarget;
                    }
                    else
                    {
                        sPrevTarget->next = sFirstTarget;
                        sPrevTarget->targetColumn->node.next =
                            (mtcNode *)sFirstTarget->targetColumn;
                    }
                    sPrevTarget = sLastTarget;
                }
            }
            else 
            {
                //---------------------
                // expression
                //---------------------
                IDE_TEST( validateTargetExp( aStatement,
                                             aQuerySet,
                                             aSFWGH,
                                             sCurrTarget )
                          != IDE_SUCCESS );
                // PROJ-2469 Optimize View Materialization
                // SFWGH�� ���� Validation���� Target�� �� �� ° Target���� ����Ѵ�.
                // ( viewColumnRefList ���� �� Ȱ�� )
                aSFWGH->currentTargetNum++;
                sPrevTarget = sCurrTarget;
            }
        }
    }

    /*
     * BUG-16000
     * lob or binary type�� distinct�� ����� �� ����.
     *
     * PROJ-1789 PROWID
     * SELECT DISTINCT (_PROWID) FROM .. ���� X
     *
     * BUG-41047 distinct list type X
     */
    if ( aSFWGH->selectType == QMS_DISTINCT )
    {
        for ( sCurrTarget = aSFWGH->target;
              sCurrTarget != NULL;
              sCurrTarget = sCurrTarget->next)
        {
            IDE_TEST_RAISE(
                qtc::isEquiValidType( sCurrTarget->targetColumn,
                                      & QC_SHARED_TMPLATE(aStatement)->tmplate )
                == ID_FALSE,
                ERR_ABORT_QMV_USE_INCOMPARABLE_DATA_TYPE_WITH_DISTINCT );

            IDE_TEST_RAISE((sCurrTarget->targetColumn->lflag &
                            QTC_NODE_COLUMN_RID_MASK) ==
                           QTC_NODE_COLUMN_RID_EXIST,
                           ERR_PROWID_NOT_SUPPORTED);

            if (sCurrTarget->targetColumn->node.module == &mtfList)
            {
                sqlInfo.setSourceInfo(aStatement, &sCurrTarget->targetColumn->position );
                IDE_RAISE( ERR_NOT_APPLICABLE_TYPE_IN_TARGET );
            }
            else
            {
                /* nothing to do */
            }

        }
    }
    else
    {
        // Nothing to do.
    }

    // set isNullable
    for ( sCurrTarget = aSFWGH->target;
          sCurrTarget != NULL;
          sCurrTarget = sCurrTarget->next)
    {
        sMtcColumn = QTC_STMT_COLUMN(aStatement, sCurrTarget->targetColumn);

        if ((sMtcColumn->flag & MTC_COLUMN_NOTNULL_MASK) == MTC_COLUMN_NOTNULL_TRUE)
        {
            // BUG-20928
            if( QTC_IS_AGGREGATE( sCurrTarget->targetColumn ) == ID_TRUE )
            {
                sCurrTarget->flag &= ~QMS_TARGET_IS_NULLABLE_MASK;
                sCurrTarget->flag |= QMS_TARGET_IS_NULLABLE_TRUE;
            }
            else
            {
                sCurrTarget->flag &= ~QMS_TARGET_IS_NULLABLE_MASK;
                sCurrTarget->flag |= QMS_TARGET_IS_NULLABLE_FALSE;
            }
        }
        else
        {
            sCurrTarget->flag &= ~QMS_TARGET_IS_NULLABLE_MASK;
            sCurrTarget->flag |= QMS_TARGET_IS_NULLABLE_TRUE;
        }
    }

    // PROJ-2002 Column Security
    // subquery�� target�� ��ȣ �÷��� ���� ��� decrypt �Լ��� �����Ѵ�.
    IDE_TEST( addDecryptFunc( aStatement, aSFWGH->target )
              != IDE_SUCCESS );
    
    // PROJ-1362
    IDE_TEST( addLobLocatorFunc( aStatement, aSFWGH->target )
              != IDE_SUCCESS );

    // in case of stored procedure
    if (aSFWGH->intoVariables != NULL)
    {
        if (aSFWGH->outerQuery != NULL)
        {
            IDE_DASSERT( aSFWGH->intoVariables->intoNodes != NULL );
            
            sqlInfo.setSourceInfo(
                aStatement,
                & aSFWGH->intoVariables->intoNodes->position );
            IDE_RAISE( ERR_INAPPROPRIATE_INTO );
        }

        // in case of SELECT in procedure
        if ( ( aStatement->spvEnv->createProc != NULL ) ||
             ( aStatement->spvEnv->createPkg != NULL ) )
        {
            sSql = (qsProcStmtSql*)(aStatement->spvEnv->currStmt);

            // BUG-38099
            IDE_TEST_CONT( sSql == NULL, SKIP_VALIDATE_TARGET );

            IDE_TEST_CONT( qsvProcStmts::isFetchType(sSql->common.stmtType) != ID_TRUE, 
                           SKIP_VALIDATE_TARGET );

            IDE_TEST( qsvProcStmts::validateIntoClause(
                          aStatement,
                          aSFWGH->target,
                          aSFWGH->intoVariables )
                      != IDE_SUCCESS );

            sSql->intoVariables = aSFWGH->intoVariables;
            sSql->from          = aSFWGH->from;
        }
    }

    /* PROJ-2523 Unpivot clause
     *  Unpivot transform�� ���� column�� ���� �ش� table�� column���� Ȯ���Ѵ�.
     *  Pseudo column, PSM variable�� �� ����Ǵ� �� �� �����Ѵ�.
     */
    if ( ( aSFWGH->lflag & QMV_SFWGH_UNPIVOT_MASK ) == QMV_SFWGH_UNPIVOT_TRUE )
    {
        sTupleID =  aSFWGH->from->tableRef->table;

        for ( sCurrTarget = aSFWGH->target;
              sCurrTarget != NULL;
              sCurrTarget = sCurrTarget->next)
        {
            if ( ( sCurrTarget->flag & QMS_TARGET_UNPIVOT_COLUMN_MASK )
                 == QMS_TARGET_UNPIVOT_COLUMN_TRUE )
            {
                IDE_DASSERT( sCurrTarget->targetColumn->node.module == &mtfDecodeOffset );

                for ( sMtcNode  = sCurrTarget->targetColumn->node.arguments->next;
                      sMtcNode != NULL;
                      sMtcNode  = sMtcNode->next )
                {
                    if ( sMtcNode->table != sTupleID )
                    {
                        sqlInfo.setSourceInfo( aStatement,
                                               & ((qtcNode*)sMtcNode)->columnName );

                        IDE_RAISE( ERR_COLUMN_NOT_IN_UNPIVOT_TABLE );
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
        }
    }
    else
    {
        // Nothing to do.
    }

    IDE_EXCEPTION_CONT( SKIP_VALIDATE_TARGET );

    return IDE_SUCCESS;
    
    IDE_EXCEPTION(ERR_INAPPROPRIATE_INTO);
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QSV_INAPPROPRIATE_INTO_SQLTEXT,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION( ERR_ABORT_QMV_USE_INCOMPARABLE_DATA_TYPE_WITH_DISTINCT );
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QMV_USE_INCOMPARABLE_DATA_TYPE_WITH_DISTINCT));
    }
    IDE_EXCEPTION(ERR_PROWID_NOT_SUPPORTED)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QMV_PROWID_NOT_SUPPORTED));
    }
    IDE_EXCEPTION(ERR_NOT_APPLICABLE_TYPE_IN_TARGET)
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QMV_NOT_APPLICABLE_TYPE_IN_TARGET,
                                sqlInfo.getErrMessage()));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION( ERR_COLUMN_NOT_IN_UNPIVOT_TABLE );
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QMV_INVALID_UNPIVOT_COLUMN,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmvQuerySet::expandAllTarget(
    qcStatement     * aStatement,
    qmsQuerySet     * aQuerySet,
    qmsSFWGH        * aSFWGH,
    qmsFrom         * aFrom,
    qmsTarget       * aCurrTarget,
    qmsTarget      ** aFirstTarget,
    qmsTarget      ** aLastTarget)
{
/***********************************************************************
 *
 * Description : ��ü target Ȯ��
 *
 * Implementation :
 *    SELECT * FROM ...�� ���� ���, parsing �������� target��
 *    �ش��ϴ� Į�� ��尡 �������� �ʴ´�.
 *    �̸� �� �Լ����� �����Ѵ�.
 ***********************************************************************/
    qmsTableRef       * sTableRef;
    qmsTarget         * sFirstTarget;
    qmsTarget         * sLastTarget;
    qcuSqlSourceInfo    sqlInfo;

    IDU_FIT_POINT_FATAL( "qmvQuerySet::expandAllTarget::__FT__" );

    if (aFrom->joinType != QMS_NO_JOIN)
    {
        IDE_TEST(expandAllTarget( aStatement, 
                                  aQuerySet, 
                                  aSFWGH, 
                                  aFrom->left, 
                                  aCurrTarget, 
                                  aFirstTarget, 
                                  & sLastTarget )
                 != IDE_SUCCESS);

        IDE_TEST(expandAllTarget( aStatement, 
                                  aQuerySet, 
                                  aSFWGH, 
                                  aFrom->right, 
                                  aCurrTarget, 
                                  & sFirstTarget,
                                  aLastTarget )
                 != IDE_SUCCESS);

        sLastTarget->next = sFirstTarget;
        sLastTarget->targetColumn->node.next
            = & sFirstTarget->targetColumn->node;
    }
    else
    {
        // check duplicated specified table or alias name
        sTableRef = NULL;

        if (aFrom->tableRef->aliasName.size > 0)
        {
            IDE_TEST(getTableRef( aStatement,
                                  aSFWGH->from,
                                  aFrom->tableRef->userID,
                                  aFrom->tableRef->aliasName,
                                  &sTableRef)
                     != IDE_SUCCESS);
        }
        else
        {
            sTableRef = aFrom->tableRef;
        }

        if (aFrom->tableRef != sTableRef)
        {
            sqlInfo.setSourceInfo(
                aStatement,
                & aFrom->tableRef->aliasName );
            IDE_RAISE( ERR_INVALID_TABLE_NAME );
        }

        // make target list for current table
        IDE_TEST(makeTargetListForTableRef( aStatement,
                                            aQuerySet,
                                            aSFWGH,
                                            sTableRef,
                                            aFirstTarget,
                                            aLastTarget)
                 != IDE_SUCCESS);

        if ( aCurrTarget != NULL )
        {
            (*aLastTarget)->next = aCurrTarget->next;
            (*aLastTarget)->targetColumn->node.next
                = aCurrTarget->targetColumn->node.next;
        }
        else
        {
            // Nothing to do.
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_INVALID_TABLE_NAME)
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QMV_INVALID_TABLE_NAME,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmvQuerySet::expandTarget(
    qcStatement  * aStatement,
    qmsQuerySet  * aQuerySet,
    qmsSFWGH     * aSFWGH,
    qmsTarget    * aCurrTarget,
    qmsTarget   ** aFirstTarget,
    qmsTarget   ** aLastTarget )
{
/***********************************************************************
 *
 * Description : target Ȯ��
 *
 * Implementation :
 *    targe �� 'T1.*', 'U1,T1.*' �� ���� ���� Parsing �������� target��
 *    �ش��ϴ� Į�� ��尡 �������� �ʴ´�.
 *    �̸� �� �Լ����� �����Ѵ�.
 ***********************************************************************/
    UInt               sUserID;
    qmsTableRef      * sTableRef;
    qcuSqlSourceInfo   sqlInfo;
    qtcNode          * sExpression;
    qmsFrom          * sFrom;

    IDU_FIT_POINT_FATAL( "qmvQuerySet::expandTarget::__FT__" );
    
    sExpression = aCurrTarget->targetColumn;

    if (QC_IS_NULL_NAME(sExpression->userName) == ID_TRUE)
    {
        sUserID = QC_EMPTY_USER_ID;
    }
    else
    {
        IDE_TEST(qcmUser::getUserID( aStatement,
                                     sExpression->userName,
                                     &(sUserID))
                 != IDE_SUCCESS);
    }

    // search table in specified table list
    sTableRef = NULL;

    // PROJ-2415 Grouping Sets Clause
    if ( ( aSFWGH->lflag & QMV_SFWGH_GBGS_TRANSFORM_MASK )
         == QMV_SFWGH_GBGS_TRANSFORM_MIDDLE )
    {
        sFrom = ( ( qmsParseTree * )aSFWGH->from->tableRef->view->myPlan->parseTree )->querySet->SFWGH->from;
    }
    else
    {
        sFrom = aSFWGH->from;
    }

    IDE_TEST(getTableRef( aStatement,
                          sFrom,
                          sUserID,
                          sExpression->tableName,
                          &sTableRef)
             != IDE_SUCCESS);

    if (sTableRef == NULL)
    {
        sqlInfo.setSourceInfo( aStatement, & sExpression->tableName );
        IDE_RAISE( ERR_INVALID_TABLE_NAME );
    }

    // make target list for current table
    IDE_TEST(makeTargetListForTableRef( aStatement,
                                        aQuerySet,
                                        aSFWGH,
                                        sTableRef,
                                        aFirstTarget,
                                        aLastTarget)
             != IDE_SUCCESS);

    (*aLastTarget)->next = aCurrTarget->next;
    (*aLastTarget)->targetColumn->node.next
        = aCurrTarget->targetColumn->node.next;

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_INVALID_TABLE_NAME)
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QMV_INVALID_TABLE_NAME,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmvQuerySet::validateTargetExp(
    qcStatement  * aStatement,
    qmsQuerySet  * aQuerySet,
    qmsSFWGH     * aSFWGH,
    qmsTarget    * aCurrTarget )
{
/***********************************************************************
 *
 * Description : target expression�� ���Ͽ� validation �˻�
 *
 * Implementation :
 *
 ***********************************************************************/
    qcuSqlSourceInfo   sqlInfo;
    qtcNode          * sExpression;
    mtcColumn        * sColumn;

    IDU_FIT_POINT_FATAL( "qmvQuerySet::validateTargetExp::__FT__" );

    sExpression = aCurrTarget->targetColumn;

    /* PROJ-2197 PSM Renewal */
    sExpression->lflag |= QTC_NODE_COLUMN_CONVERT_TRUE;

    // BUG-42790
    // target�� �����Լ��� ���� ������ �� ����.
    if ( ( sExpression->node.module != &qtc::columnModule ) &&
         ( sExpression->node.module != &qtc::spFunctionCallModule ) &&
         ( ( ( sExpression->lflag) & QTC_NODE_SP_ARRAY_INDEX_VAR_MASK ) ==
           QTC_NODE_SP_ARRAY_INDEX_VAR_EXIST ) )
    {
        sExpression->lflag &= ~QTC_NODE_SP_ARRAY_INDEX_VAR_MASK;
        sExpression->lflag |= QTC_NODE_SP_ARRAY_INDEX_VAR_ABSENT;
    }
    else
    {
        // Nothing to do.
    }

    IDE_TEST(qtc::estimate( sExpression,
                            QC_SHARED_TMPLATE(aStatement),
                            aStatement,
                            aQuerySet,
                            aSFWGH,
                            NULL )
             != IDE_SUCCESS);

    // BUG-43001
    if ( ( sExpression->node.lflag & MTC_NODE_OPERATOR_MASK )
         == MTC_NODE_OPERATOR_LIST )
    {
        sqlInfo.setSourceInfo( aStatement,
                               & sExpression->position );
        IDE_RAISE( ERR_NOT_APPLICABLE_TYPE_IN_TARGET );
    }

    //fix BUG-16538
    IDE_TEST( qtc::estimateConstExpr( sExpression,
                                      QC_SHARED_TMPLATE(aStatement),
                                      aStatement ) != IDE_SUCCESS );

    if ( ( sExpression->node.lflag & MTC_NODE_DML_MASK )
         == MTC_NODE_DML_UNUSABLE )
    {
        sqlInfo.setSourceInfo( aStatement,
                               & sExpression->position );
        IDE_RAISE( ERR_USE_CURSOR_ATTR );
    }

    // PROJ-1492
    if( ( aStatement->calledByPSMFlag == ID_FALSE ) &&
        ( MTC_NODE_IS_DEFINED_TYPE( & sExpression->node ) )
        == ID_FALSE )
    {
        sqlInfo.setSourceInfo( aStatement,
                               & sExpression->position );
        IDE_RAISE( ERR_NOT_ALLOW_HOSTVAR );
    }
    else
    {
        // Nothing to do.
    }

    // BUG-14094
    if ( ( sExpression->node.lflag & MTC_NODE_LOGICAL_CONDITION_MASK )
         == MTC_NODE_LOGICAL_CONDITION_TRUE )
    {
        sqlInfo.setSourceInfo( aStatement,
                               & sExpression->position );
        IDE_RAISE( ERR_NOT_APPLICABLE_TYPE_IN_TARGET );
    }
    else
    {
        // Nothing to do.
    }

    // BUG-14094
    if ( ( sExpression->node.lflag & MTC_NODE_COMPARISON_MASK )
         == MTC_NODE_COMPARISON_TRUE )
    {
        sqlInfo.setSourceInfo( aStatement,
                               & sExpression->position );
        IDE_RAISE( ERR_NOT_APPLICABLE_TYPE_IN_TARGET );
    }
    else
    {
        // Nothing to do.
    }

    // BUG-15897
    if ( ( sExpression->node.lflag & MTC_NODE_OPERATOR_MASK )
         == MTC_NODE_OPERATOR_SUBQUERY )
    {
        if ( (sExpression->node.lflag & MTC_NODE_ARGUMENT_COUNT_MASK) > 1 )
        {
            sqlInfo.setSourceInfo( aStatement,
                                   & sExpression->position );
            IDE_RAISE( ERR_NOT_APPLICABLE_TYPE_IN_TARGET );
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

    // BUG-15896
    // BUG-24133
    sColumn = QTC_TMPL_COLUMN( QC_SHARED_TMPLATE(aStatement),
                               sExpression );
                
    if ( ( sColumn->module->id == MTD_BOOLEAN_ID ) ||
         ( sColumn->module->id == MTD_ROWTYPE_ID ) ||
         ( sColumn->module->id == MTD_RECORDTYPE_ID ) ||
         ( sColumn->module->id == MTD_ASSOCIATIVE_ARRAY_ID ) || 
         ( sColumn->module->id == MTD_REF_CURSOR_ID ) )
    {
        sqlInfo.setSourceInfo( aStatement,
                               & sExpression->position );
        IDE_RAISE( ERR_NOT_APPLICABLE_TYPE_IN_TARGET );
    }
    else
    {
        // Nothing to do.
    }

    // PROJ-1789 Updatable Scrollable Cursor (a.k.a. PROWID)
    IDE_TEST(setTargetColumnInfo(aStatement, aCurrTarget) != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NOT_APPLICABLE_TYPE_IN_TARGET );
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QMV_NOT_APPLICABLE_TYPE_IN_TARGET,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION(ERR_USE_CURSOR_ATTR);
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QSV_USING_ATTR_IN_INAPPROPRIATE_CLAUSE,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION( ERR_NOT_ALLOW_HOSTVAR );
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QMV_NOT_ALLOWED_HOSTVAR,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmvQuerySet::getTableRef(
    qcStatement       * aStatement,
    qmsFrom           * aFrom,
    UInt                aUserID,
    qcNamePosition      aTableName,
    qmsTableRef      ** aTableRef)
{
    qmsTableRef     * sTableRef;
    qcuSqlSourceInfo  sqlInfo;

    IDU_FIT_POINT_FATAL( "qmvQuerySet::getTableRef::__FT__" );

    if (aFrom->joinType != QMS_NO_JOIN)
    {
        IDE_TEST(getTableRef(
                     aStatement, aFrom->left, aUserID, aTableName, aTableRef)
                 != IDE_SUCCESS);

        IDE_TEST(getTableRef(
                     aStatement, aFrom->right, aUserID, aTableName, aTableRef)
                 != IDE_SUCCESS);
    }
    else
    {
        sTableRef = aFrom->tableRef;

        // compare table name
        if ( ( sTableRef->userID == aUserID || aUserID == QC_EMPTY_USER_ID ) &&
             sTableRef->aliasName.size > 0 )
        {
            if ( QC_IS_NAME_MATCHED( sTableRef->aliasName, aTableName ) )
            {
                if (*aTableRef != NULL)
                {
                    sqlInfo.setSourceInfo(
                        aStatement,
                        & sTableRef->aliasName );
                    IDE_RAISE( ERR_AMBIGUOUS_DEFINED_COLUMN );
                }
                *aTableRef = sTableRef;
            }
        }
    }

    if (aFrom->next != NULL)
    {
        IDE_TEST(getTableRef(
                     aStatement, aFrom->next, aUserID, aTableName, aTableRef)
                 != IDE_SUCCESS);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_AMBIGUOUS_DEFINED_COLUMN)
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QMV_COLUMN_AMBIGUOUS_DEF,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmvQuerySet::makeTargetListForTableRef(
    qcStatement     * aStatement,
    qmsQuerySet     * aQuerySet,
    qmsSFWGH        * aSFWGH,
    qmsTableRef     * aTableRef,
    qmsTarget      ** aFirstTarget,
    qmsTarget      ** aLastTarget)
{
    qcmTableInfo    * sTableInfo;
    qcmColumn       * sCurrColumn;
    qmsTarget       * sPrevTarget;
    qmsTarget       * sCurrTarget;
    UShort            sColumn;
    SChar             sDisplayName[ QC_MAX_OBJECT_NAME_LEN*2 + ID_SIZEOF(UChar) + 1 ];
    UInt              sDisplayNameMaxSize = QC_MAX_OBJECT_NAME_LEN * 2;
    UInt              sNameLen = 0;

    // PROJ-2415 Grouping Sets Clause
    qmsParseTree    * sChildParseTree;
    qmsTarget       * sChildTarget;
    UShort            sStartColumn = ID_USHORT_MAX;
    UShort            sEndColumn   = ID_USHORT_MAX;
    UShort            sCount;
    qmsTableRef     * sTableRef = NULL;

    /* TASK-7219 */
    UShort            sOrderStart = ID_USHORT_MAX;
    UShort            sOrderEnd   = ID_USHORT_MAX;

    IDU_FIT_POINT_FATAL( "qmvQuerySet::makeTargetListForTableRef::__FT__" );

    *aFirstTarget = NULL;
    *aLastTarget  = NULL;

    // PROJ-2415 Gropuing Sets Clause
    // ���� inLineView�� Target���� �ش��ϴ� Column�� ã�´�.
    if ( ( ( aSFWGH->lflag & QMV_SFWGH_GBGS_TRANSFORM_MASK )
           == QMV_SFWGH_GBGS_TRANSFORM_MIDDLE ) &&
         ( aSFWGH->from->tableRef != aTableRef ) )
    {
        sChildParseTree  = ( qmsParseTree * )aSFWGH->from->tableRef->view->myPlan->parseTree;
        
        sTableRef = aSFWGH->from->tableRef;

        for ( sChildTarget  = sChildParseTree->querySet->SFWGH->target, sCount = 0;
              sChildTarget != NULL;
              sChildTarget  = sChildTarget->next, sCount++ )
        {
            if ( aTableRef->table == sChildTarget->targetColumn->node.table )
            {
                if ( sStartColumn == ID_USHORT_MAX )
                {
                    sStartColumn = sCount;
                    sEndColumn = sCount;
                }
                else
                {
                    sEndColumn++;
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
        sTableRef = aTableRef;
    }

    /* TASK-7219
     *   Implicit Order By Transform �� �߰��� Target �� Shard View ������
     *   Select * ���·� ���ؼ� �ֻ��� ����� ����� �� �ִ�.
     *   �ش� Target �� �߰��� ������ ����� ���� ���� ���� Target �̰�
     *   ������� �����ؾ� �ϹǷ�, �̸� ��ġ�� �˻��Ѵ�.
     *
     *   ������ ���� Implicit Order By Transform �� Target ���̿�
     *   �ٸ� Transform ���� ������ Target �� ������ �ȵȴ�.
     */
    if ( ( aSFWGH->lflag & QMV_SFWGH_SHARD_ORDER_BY_TRANS_MASK )
         == QMV_SFWGH_SHARD_ORDER_BY_TRANS_TRUE )
    {
        sChildParseTree = (qmsParseTree *)aSFWGH->from->tableRef->view->myPlan->parseTree;

        for ( sChildTarget  = sChildParseTree->querySet->SFWGH->target, sCount = 0;
              sChildTarget != NULL;
              sChildTarget  = sChildTarget->next, sCount++ )
        {
            if ( ( sChildTarget->flag & QMS_TARGET_SHARD_ORDER_BY_TRANS_MASK )
                 != QMS_TARGET_SHARD_ORDER_BY_TRANS_NONE )
            {
                if ( sOrderStart == ID_USHORT_MAX )
                {
                    sOrderStart = sCount;
                    sOrderEnd   = sCount + 1;
                }
                else
                {
                    sOrderEnd++;
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

    sTableInfo = sTableRef->tableInfo;

    for (sPrevTarget = NULL,
             sColumn = 0,
             sCurrColumn = sTableInfo->columns;
         sCurrColumn != NULL;
         sCurrColumn = sCurrColumn->next, sColumn++)
    {
        /* PROJ-1090 Function-based Index */
        if ( ( (sCurrColumn->flag & QCM_COLUMN_HIDDEN_COLUMN_MASK)
               == QCM_COLUMN_HIDDEN_COLUMN_TRUE ) ||
             ( (sCurrColumn->flag & QCM_COLUMN_GBGS_ORDER_BY_NODE_MASK) // PROJ-2415 Grouping Sets Clause 
               == QCM_COLUMN_GBGS_ORDER_BY_NODE_TRUE ) 
             )
        {
            continue;
        }
        else
        {
            /* Nothing to do */
        }
        
        // PROJ-2415 Grouping Sets Clause
        if ( ( ( ( aSFWGH->lflag & QMV_SFWGH_GBGS_TRANSFORM_MASK )
                 == QMV_SFWGH_GBGS_TRANSFORM_MIDDLE ) &&
               ( ( sColumn < sStartColumn ) || ( sColumn > sEndColumn ) ) ) &&
             ( sTableRef != aTableRef ) )
        {
            continue;
        }
        else
        {
            /* Nothing to do */
        }

        /* TASK-7219
         *   �߰��� Target �� ����� ���� ���� ���� Target �̰�
         *   ������� �����ؾ� �Ѵ�.
         */
        if ( ( ( aSFWGH->lflag & QMV_SFWGH_SHARD_ORDER_BY_TRANS_MASK )
               == QMV_SFWGH_SHARD_ORDER_BY_TRANS_TRUE )
               &&
               ( ( sColumn >= sOrderStart ) && ( sColumn < sOrderEnd ) ) )
        {
            continue;
        }
        else
        {
            /* Nothing to do */
        }

        IDE_TEST(STRUCT_ALLOC(QC_QMP_MEM(aStatement),
                              qmsTarget,
                              &sCurrTarget)
                 != IDE_SUCCESS);
        QMS_TARGET_INIT(sCurrTarget);

        // make expression for u1.t1.c1
        IDE_TEST(STRUCT_ALLOC(QC_QMP_MEM(aStatement),
                              qtcNode,
                              &(sCurrTarget->targetColumn))
                 != IDE_SUCCESS);

        // set member of qtcNode
        IDE_TEST(qtc::makeTargetColumn(
                     sCurrTarget->targetColumn,
                     sTableRef->table,
                     sColumn)
                 != IDE_SUCCESS);

        IDE_TEST(qtc::estimate(
                     sCurrTarget->targetColumn,
                     QC_SHARED_TMPLATE(aStatement),
                     NULL,
                     aQuerySet,
                     aSFWGH,
                     NULL )
                 != IDE_SUCCESS);

        // PROJ-1413
        // view �÷� ������ ����Ѵ�.
        IDE_TEST( qmvQTC::addViewColumnRefListForTarget(
                      aStatement,
                      sCurrTarget->targetColumn,
                      aSFWGH->currentTargetNum,
                      sCurrTarget->targetColumn->node.column )
                  != IDE_SUCCESS );

        /* TASK-7219 Shard Transformer Refactoring */

        /* BUG-37658 : meta table info �� free �� �� �����Ƿ� ���� �׸���
         *             alloc -> memcpy �Ѵ�.
         * - userName
         * - tableName
         * - aliasTableName
         * - columnName
         * - aliasColumnName
         * - displayName
         */

        // set user name
        sNameLen = idlOS::strlen(sTableInfo->tableOwnerName);

        if ( sNameLen > 0 )
        {
            IDE_TEST( STRUCT_ALLOC_WITH_SIZE( QC_QMP_MEM( aStatement ),
                                              SChar,
                                              sNameLen + 1,
                                              &(sCurrTarget->userName.name) )
                      != IDE_SUCCESS );
            idlOS::memcpy( sCurrTarget->userName.name,
                           sTableInfo->tableOwnerName,
                           sNameLen );
            sCurrTarget->userName.name[sNameLen] = '\0';
            sCurrTarget->userName.size = sNameLen;
        }
        else
        {
            sCurrTarget->userName.name = NULL;
            sCurrTarget->userName.size = QC_POS_EMPTY_SIZE;

            SET_EMPTY_POSITION( sCurrTarget->targetColumn->userName );
        }

        // set table name
        sNameLen = idlOS::strlen(sTableInfo->name);

        if ( sNameLen > 0 )
        {
            IDE_TEST( STRUCT_ALLOC_WITH_SIZE( QC_QMP_MEM( aStatement ),
                                              SChar,
                                              sNameLen + 1,
                                              &(sCurrTarget->tableName.name) )
                      != IDE_SUCCESS );
            idlOS::memcpy( sCurrTarget->tableName.name,
                           sTableInfo->name,
                           sNameLen );
            sCurrTarget->tableName.name[sNameLen] = '\0';
            sCurrTarget->tableName.size = sNameLen;

            sCurrTarget->targetColumn->tableName.stmtText = sCurrTarget->tableName.name;
            sCurrTarget->targetColumn->tableName.offset   = 0;
            sCurrTarget->targetColumn->tableName.size     = sCurrTarget->tableName.size;
        }
        else
        {
            sCurrTarget->tableName.name = NULL;
            sCurrTarget->tableName.size = QC_POS_EMPTY_SIZE;

            SET_EMPTY_POSITION( sCurrTarget->targetColumn->tableName );
        }

        // set alias table name
        if ( QC_IS_NULL_NAME( sTableRef->aliasName ) == ID_TRUE )
        {
            sCurrTarget->aliasTableName.name = sCurrTarget->tableName.name;
            sCurrTarget->aliasTableName.size = sCurrTarget->tableName.size;
        }
        else
        {
            IDE_TEST( STRUCT_ALLOC_WITH_SIZE( QC_QMP_MEM(aStatement),
                                              SChar,
                                              sTableRef->aliasName.size + 1,
                                              &(sCurrTarget->aliasTableName.name))
                      != IDE_SUCCESS );
            idlOS::memcpy( sCurrTarget->aliasTableName.name,
                           sTableRef->aliasName.stmtText +
                           sTableRef->aliasName.offset,
                           sTableRef->aliasName.size );
            sCurrTarget->aliasTableName.name[ sTableRef->aliasName.size ] = '\0';
            sCurrTarget->aliasTableName.size = sTableRef->aliasName.size;
        }

        // set base column name
        sNameLen = idlOS::strlen(sCurrColumn->name);

        if ( sNameLen > 0 )
        {
            IDE_TEST( STRUCT_ALLOC_WITH_SIZE( QC_QMP_MEM( aStatement ),
                                              SChar,
                                              sNameLen + 1,
                                              &(sCurrTarget->columnName.name) )
                      != IDE_SUCCESS );
            idlOS::memcpy( sCurrTarget->columnName.name,
                           sCurrColumn->name,
                           sNameLen );
            sCurrTarget->columnName.name[sNameLen] = '\0';
            sCurrTarget->columnName.size = sNameLen;

            sCurrTarget->targetColumn->columnName.stmtText = sCurrTarget->columnName.name;
            sCurrTarget->targetColumn->columnName.offset   = 0;
            sCurrTarget->targetColumn->columnName.size     = sCurrTarget->columnName.size;
        }
        else
        {
            sCurrTarget->columnName.name = NULL;
            sCurrTarget->columnName.size = QC_POS_EMPTY_SIZE;

            SET_EMPTY_POSITION( sCurrTarget->targetColumn->columnName );
        }

        // set alias column name
        if ( QC_IS_NULL_NAME( sCurrTarget->columnName ) == ID_TRUE )
        {
            sCurrTarget->aliasColumnName.name = sCurrTarget->columnName.name;
            sCurrTarget->aliasColumnName.size = sCurrTarget->columnName.size;
        }
        else
        {
            IDE_TEST( STRUCT_ALLOC_WITH_SIZE( QC_QMP_MEM( aStatement ),
                                              SChar,
                                              sCurrTarget->columnName.size + 1,
                                              &(sCurrTarget->aliasColumnName.name) )
                      != IDE_SUCCESS);
            idlOS::memcpy( sCurrTarget->aliasColumnName.name,
                           sCurrTarget->columnName.name,
                           sCurrTarget->columnName.size );
            sCurrTarget->aliasColumnName.name[sCurrTarget->columnName.size] = '\0';
            sCurrTarget->aliasColumnName.size = sCurrTarget->columnName.size;
        }

        // set display name
        sNameLen = 0;
        sCurrTarget->displayName.size = 0;

        // set display name : table name
        if ( QCG_GET_SESSION_SELECT_HEADER_DISPLAY( aStatement ) == 0 )
        {
            if ( QC_IS_NULL_NAME( sTableRef->aliasName ) == ID_FALSE )
            {
                if ( sDisplayNameMaxSize > (UInt)(sTableRef->aliasName.size + 1) )
                {
                    // table name
                    idlOS::memcpy(
                        sDisplayName + sNameLen,
                        sTableRef->aliasName.stmtText + sTableRef->aliasName.offset,
                        sTableRef->aliasName.size);
                    sNameLen += sTableRef->aliasName.size;

                    // .
                    idlOS::memcpy(
                        sDisplayName + sNameLen,
                        ".",
                        1);
                    sNameLen += 1;
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
            /* Noting to do */
        }        

        // environment�� ���
        qcgPlan::registerPlanProperty( aStatement,
                                       PLAN_PROPERTY_SELECT_HEADER_DISPLAY );

        // set display name - column name
        if (sCurrTarget->aliasColumnName.size > 0)
        {
            if (sDisplayNameMaxSize >
                sNameLen + sCurrTarget->aliasColumnName.size )
            {
                idlOS::memcpy(
                    sDisplayName + sNameLen,
                    sCurrTarget->aliasColumnName.name,
                    sCurrTarget->aliasColumnName.size);
                sNameLen += sCurrTarget->aliasColumnName.size;
            }
            else
            {
                idlOS::memcpy(
                    sDisplayName + sNameLen,
                    sCurrTarget->aliasColumnName.name,
                    sDisplayNameMaxSize - sNameLen );
                sNameLen = sDisplayNameMaxSize;
            }
        }
        else
        {
            if (sCurrColumn->namePos.size > 0)
            {
                if (sDisplayNameMaxSize >
                    sNameLen + sCurrColumn->namePos.size)
                {
                    idlOS::memcpy(
                        sDisplayName + sNameLen,
                        sCurrColumn->namePos.stmtText + sCurrColumn->namePos.offset,
                        sCurrColumn->namePos.size);
                    sNameLen += sCurrColumn->namePos.size;
                }
                else
                {
                    // BUG-42775 Invalid read of size 1
                    idlOS::memcpy(
                        sDisplayName + sNameLen,
                        sCurrColumn->namePos.stmtText + sCurrColumn->namePos.offset,
                        sDisplayNameMaxSize - sNameLen );
                    sNameLen = sDisplayNameMaxSize;
                }
            }
        }

        if (sNameLen > 0)
        {
            IDE_TEST(STRUCT_ALLOC_WITH_SIZE(QC_QMP_MEM(aStatement),
                                            SChar,
                                            sNameLen + 1,
                                            &(sCurrTarget->displayName.name))
                     != IDE_SUCCESS);

            idlOS::memcpy(sCurrTarget->displayName.name,
                          sDisplayName,
                          sNameLen);
            sCurrTarget->displayName.name[sNameLen] = '\0';
            sCurrTarget->displayName.size = sNameLen;
        }
        else
        {
            IDE_TEST(STRUCT_ALLOC_WITH_SIZE(QC_QMP_MEM(aStatement),
                                            SChar,
                                            sNameLen + 1,
                                            &(sCurrTarget->displayName.name))
                     != IDE_SUCCESS);

            sCurrTarget->displayName.name[sNameLen] = '\0';
            sCurrTarget->displayName.size = sNameLen;
        }

        // set flag
        if ( ( sCurrColumn->basicInfo->flag & MTC_COLUMN_NOTNULL_MASK )
             == MTC_COLUMN_NOTNULL_TRUE )
        {
            // BUG-20928
            if( QTC_IS_AGGREGATE( sCurrTarget->targetColumn ) == ID_TRUE )
            {
                sCurrTarget->flag &= ~QMS_TARGET_IS_NULLABLE_MASK;
                sCurrTarget->flag |= QMS_TARGET_IS_NULLABLE_TRUE;
            }
            else
            {
                sCurrTarget->flag &= ~QMS_TARGET_IS_NULLABLE_MASK;
                sCurrTarget->flag |= QMS_TARGET_IS_NULLABLE_FALSE;
            }
        }
        else
        {
            sCurrTarget->flag &= ~QMS_TARGET_IS_NULLABLE_MASK;
            sCurrTarget->flag |= QMS_TARGET_IS_NULLABLE_TRUE;
        }

        if ( sTableRef->view == NULL )
        {
            sCurrTarget->flag &= ~QMS_TARGET_IS_UPDATABLE_MASK;
            sCurrTarget->flag |= QMS_TARGET_IS_UPDATABLE_TRUE;
        }
        else
        {
            sCurrTarget->flag &= ~QMS_TARGET_IS_UPDATABLE_MASK;
            sCurrTarget->flag |= QMS_TARGET_IS_UPDATABLE_FALSE;
        }

        // link
        if (sPrevTarget == NULL)
        {
            *aFirstTarget = sCurrTarget;
            sPrevTarget   = sCurrTarget;
        }
        else
        {
            sPrevTarget->next = sCurrTarget;
            sPrevTarget->targetColumn->node.next =
                (mtcNode *)(sCurrTarget->targetColumn);

            sPrevTarget       = sCurrTarget;
        }

        // PROJ-2469 Optimize View Materialization
        aSFWGH->currentTargetNum++;        
    }

    IDE_TEST_RAISE( *aFirstTarget == NULL, ERR_COLUMN_NOT_FOUND );

    *aLastTarget = sPrevTarget;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_COLUMN_NOT_FOUND )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmvQuerySet::makeTargetListForTableRef",
                                  "Column not found" ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmvQuerySet::validateQmsTableRef(
    qcStatement * aStatement,
    qmsSFWGH    * aSFWGH,
    qmsTableRef * aTableRef,
    UInt          aFlag,
    UInt          aIsNullableFlag ) // PR-13597
{
    qcDepInfo    sDepInfo;
    UInt         sCount;
    UInt         sNameLen = 0;

    IDU_FIT_POINT_FATAL( "qmvQuerySet::validateQmsTableRef::__FT__" );

    // BUG-41311 table function
    if ( aTableRef->funcNode != NULL )
    {
        IDE_TEST( qmvTableFuncTransform::doTransform( aStatement, aTableRef )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }
    
    /* BUG-33691 PROJ-2188 Support pivot clause */
    if ( aTableRef->pivot != NULL )
    {
        IDE_TEST(qmvPivotTransform::doTransform( aStatement, aSFWGH, aTableRef )
                 != IDE_SUCCESS);
    }
    else
    {
        /* Nothing to do */
    }

    /* PROJ-2523 Unpivot clause */
    if ( aTableRef->unpivot != NULL )
    {
        IDE_TEST( qmvUnpivotTransform::doTransform( aStatement,
                                                    aSFWGH,
                                                    aTableRef )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    if( aTableRef->view == NULL )
    {
        // PROJ-2582 recursive with
        if ( ( aTableRef->flag & QMS_TABLE_REF_RECURSIVE_VIEW_MASK )
             == QMS_TABLE_REF_RECURSIVE_VIEW_TRUE )
        {
            // PROJ-2582 recursive with
            // recursive view�� ���
            IDE_TEST( validateRecursiveView( aStatement,
                                             aSFWGH,
                                             aTableRef,
                                             aIsNullableFlag )
                      != IDE_SUCCESS );
        }
        else
        {
            // PROJ-2418
            // View�� ���ٸ� Lateral Flag�� �ʱ�ȭ�Ѵ�.
            aTableRef->flag &= ~QMS_TABLE_REF_LATERAL_VIEW_MASK;
            aTableRef->flag |= QMS_TABLE_REF_LATERAL_VIEW_FALSE;

            /* PROJ-1832 New database link */
            if ( aTableRef->remoteTable != NULL )
            {
                /* remote table */
                IDE_TEST( validateRemoteTable( aStatement,
                                               aTableRef )
                          != IDE_SUCCESS );
            }
            else
            {
                // ���̺��� ���
                IDE_TEST( validateTable(aStatement,
                                        aTableRef,
                                        aFlag,
                                        aIsNullableFlag)
                          != IDE_SUCCESS );
            }
        }
    }
    else
    {
        // Create View �Ǵ� Inline View�� ���
        IDE_TEST(validateView( aStatement, aSFWGH, aTableRef, aIsNullableFlag )
                 != IDE_SUCCESS);
    }

    // BUG-41230 SYS_USERS_ => DBA_USERS_
    if ( aTableRef->tableInfo->tableOwnerID == QC_SYSTEM_USER_ID )
    {
        if ( ( QCG_GET_SESSION_USER_ID(aStatement) == QC_SYS_USER_ID ) ||
             ( QCG_GET_SESSION_USER_ID(aStatement) == QC_SYSTEM_USER_ID ) )
        {
            // Nothing To Do
        }
        else
        {
            IDE_TEST_RAISE ( ( idlOS::strlen( aTableRef->tableInfo->name ) >= 4 ) &&
                             ( idlOS::strMatch( aTableRef->tableInfo->name,
                                                4,
                                                (SChar *)"DBA_",
                                                4 ) == 0 ),
                             ERR_NOT_EXIST_TABLE );
        }
    }
    else
    {
        // Nothing To Do
    }
    
    // PROJ-1413
    // tuple variable ���
    if ( QC_IS_NULL_NAME( (aTableRef->aliasName) ) == ID_FALSE )
    {
        IDE_TEST( qtc::registerTupleVariable( aStatement,
                                              & aTableRef->aliasName )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    /* BUG-31570
     * DDL�� ����� ȯ�濡�� plan text�� �����ϰ� �����ִ� ����� �ʿ��ϴ�.
     */
    IDE_TEST( QC_QMP_MEM(aStatement)->alloc( (QC_MAX_OBJECT_NAME_LEN + 1) *
                                             (aTableRef->tableInfo->columnCount),
                                             (void**)&( aTableRef->columnsName ) )
              != IDE_SUCCESS );

    for(sCount= 0; sCount < (aTableRef->tableInfo->columnCount); ++sCount)
    {
        idlOS::memcpy( aTableRef->columnsName[sCount],
                       aTableRef->tableInfo->columns[sCount].name,
                       (QC_MAX_OBJECT_NAME_LEN + 1) );
    }

    /* PROJ-1090 Function-based Index */
    if ( QCG_GET_SESSION_QUERY_REWRITE_ENABLE(aStatement) == 1 )
    {
        IDE_TEST( qmsDefaultExpr::makeNodeListForFunctionBasedIndex(
                      aStatement,
                      aTableRef,
                      &(aTableRef->defaultExprList) )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    // BUG-38983
    // DDL �� ����� ȯ�濡�� plan text �� userName (tableOwnerName) �� �����ϰ� ���.
    if ( aTableRef->tableInfo != NULL )
    {
        sNameLen = idlOS::strlen( aTableRef->tableInfo->tableOwnerName );

        if ( sNameLen > 0 )
        {
            IDE_TEST( STRUCT_ALLOC_WITH_SIZE( QC_QMP_MEM(aStatement),
                                              SChar,
                                              sNameLen + 1,
                                              &(aTableRef->userName.stmtText) )
                      != IDE_SUCCESS );

            idlOS::memcpy( aTableRef->userName.stmtText,
                           aTableRef->tableInfo->tableOwnerName,
                           sNameLen );
            aTableRef->userName.stmtText[sNameLen] = '\0';
            aTableRef->userName.offset = 0;
            aTableRef->userName.size = sNameLen;
        }
        else
        {
            SET_EMPTY_POSITION( aTableRef->userName );
        }
    }
    else
    {
        // Nothing to do.
    }

    // set dependency
    if (aSFWGH != NULL)
    {
        qtc::dependencySet( aTableRef->table, & sDepInfo );
        IDE_TEST( qtc::dependencyOr( & aSFWGH->depInfo,
                                     & sDepInfo,
                                     & aSFWGH->depInfo )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NOT_EXIST_TABLE )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QCV_NOT_EXISTS_TABLE,
                                  aTableRef->tableInfo->name ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
 * PROJ-1832 New database link
 */ 

#define QUERY_PREFIX "SELECT * FROM "

/* Length of "SELECT * FROM " string is 14. */
#define QUERY_PREFIX_LENGTH (14)

/*
 * BUG-36967
 *
 * Copy and convert double single quotation('') to single quotation('). 
 */
void qmvQuerySet::copyRemoteQueryString( SChar * aDest, qcNamePosition * aSrc )
{
    SInt i = 0;
    SInt j = 0;
    SChar * sSrcString = NULL;

    sSrcString = aSrc->stmtText + aSrc->offset;

    for ( i = 0; i < aSrc->size; i++, j++ )
    {
        if ( sSrcString[i] == '\'' )
        {
            i++;
        }
        else
        {
            /* nothing to do */
        }
        aDest[j] = sSrcString[i];
    }
    aDest[j] = '\0';

}

IDE_RC qmvQuerySet::validateRemoteTable( qcStatement * aStatement,
                                         qmsTableRef * aTableRef )
{
    SInt sLength = 0;

    IDU_FIT_POINT_FATAL( "qmvQuerySet::validateRemoteTable::__FT__" );

    if ( QC_IS_NULL_NAME( aTableRef->remoteTable->tableName ) != ID_TRUE )
    {
        sLength = QUERY_PREFIX_LENGTH;
        sLength += aTableRef->remoteTable->tableName.size + 1;
          
        IDE_TEST( QC_QMP_MEM( aStatement )->alloc(
                      sLength,
                      (void **)&(aTableRef->remoteQuery) )
                  != IDE_SUCCESS);

        idlOS::memcpy( aTableRef->remoteQuery,
                       QUERY_PREFIX, QUERY_PREFIX_LENGTH );

        QC_STR_COPY( aTableRef->remoteQuery + QUERY_PREFIX_LENGTH, aTableRef->remoteTable->tableName );
    }
    else
    {
        sLength = aTableRef->remoteTable->remoteQuery.size + 1;

        IDE_TEST( QC_QMP_MEM( aStatement )->alloc(
                      sLength,
                      (void **)&(aTableRef->remoteQuery) )
                  != IDE_SUCCESS);

        copyRemoteQueryString( aTableRef->remoteQuery,
                               &(aTableRef->remoteTable->remoteQuery) );
    }

    IDE_TEST( qmrGetRemoteTableMeta( aStatement, aTableRef )
              != IDE_SUCCESS );
    IDE_TEST( qcgPlan::registerPlanTable( aStatement,
                                          aTableRef->tableHandle,
                                          aTableRef->tableSCN,
                                          aTableRef->tableInfo->tableOwnerID, /* BUG-45893 */
                                          aTableRef->tableInfo->name )        /* BUG-45893 */
              != IDE_SUCCESS );

    IDE_TEST( qtc::nextTable( &(aTableRef->table ),
                              aStatement,
                              aTableRef->tableInfo,
                              ID_FALSE,
                              MTC_COLUMN_NOTNULL_TRUE )
              != IDE_SUCCESS );

    /* emulate as disk table */
    QC_SHARED_TMPLATE( aStatement )->tmplate.rows[ aTableRef->table ].lflag
        &= ~MTC_TUPLE_STORAGE_MASK;
    QC_SHARED_TMPLATE( aStatement )->tmplate.rows[ aTableRef->table ].lflag
        |= MTC_TUPLE_STORAGE_DISK;

    /* BUG-36468 */
    QC_SHARED_TMPLATE( aStatement )->tmplate.rows[ aTableRef->table ].lflag
        &= ~MTC_TUPLE_STORAGE_LOCATION_MASK;
    QC_SHARED_TMPLATE( aStatement )->tmplate.rows[ aTableRef->table ].lflag
        |= MTC_TUPLE_STORAGE_LOCATION_REMOTE;
    
    /* no plan cache */
    QC_SHARED_TMPLATE( aStatement )->flag &= ~QC_TMP_PLAN_CACHE_IN_MASK;
    QC_SHARED_TMPLATE( aStatement )->flag |= QC_TMP_PLAN_CACHE_IN_OFF;

    /* BUG-42639 Monitoring query */
    QC_SHARED_TMPLATE(aStatement)->flag &= ~QC_TMP_ALL_FIXED_TABLE_MASK;
    QC_SHARED_TMPLATE(aStatement)->flag |= QC_TMP_ALL_FIXED_TABLE_FALSE;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC qmvQuerySet::validateTable(
    qcStatement * aStatement,
    qmsTableRef * aTableRef,
    UInt          aFlag,
    UInt          aIsNullableFlag)
{
    qcuSqlSourceInfo    sqlInfo;
    idBool              sExist = ID_FALSE;
    idBool              sIsFixedTable = ID_FALSE;
    void               *sTableHandle = NULL;
    qcmSynonymInfo      sSynonymInfo;
    UInt                sTableType;

    IDU_FIT_POINT_FATAL( "qmvQuerySet::validateTable::__FT__" );

    IDE_TEST(
        qcmSynonym::resolveTableViewQueue(
            aStatement,
            aTableRef->userName,
            aTableRef->tableName,
            &(aTableRef->tableInfo),
            &(aTableRef->userID),
            &(aTableRef->tableSCN),
            &sExist,
            &sSynonymInfo,
            &sTableHandle)
        != IDE_SUCCESS);

    if( sExist == ID_TRUE )
    {
        sTableType = smiGetTableFlag( sTableHandle ) & SMI_TABLE_TYPE_MASK;
        
        if( sTableType == SMI_TABLE_FIXED )
        {
            sIsFixedTable = ID_TRUE;
        }
        else
        {
            sIsFixedTable = ID_FALSE;
        }
    }
    else
    {
        sqlInfo.setSourceInfo(aStatement, &aTableRef->tableName);
        // ���̺��� �������� ����. ���� �߻���Ŵ
        IDE_RAISE( ERR_NOT_EXIST_TABLE );
    }

    // FixedTable or PerformanceView
    if( sIsFixedTable == ID_TRUE )
    {
        // PROJ-1350 - Plan Tree �ڵ� �籸��
        // Execution ������ table handle�� ������ ����ϱ� ���Ͽ�
        // Table Meta Cache�� ������ table handle������ �����Ѵ�.
        // �̴� Table Meta Cache������ DDL � ���� ����� �� �ֱ�
        // �����̴�.
        aTableRef->tableHandle = aTableRef->tableInfo->tableHandle;
        aTableRef->tableType   = aTableRef->tableInfo->tableType;
        aTableRef->tableFlag   = aTableRef->tableInfo->tableFlag;
        aTableRef->tableOID    = aTableRef->tableInfo->tableOID;

        QC_SHARED_TMPLATE(aStatement)->flag &= ~QC_TMP_REF_FIXED_TABLE_MASK;
        QC_SHARED_TMPLATE(aStatement)->flag |= QC_TMP_REF_FIXED_TABLE_TRUE;

        // BUG-45522 fixed table �� ����Ű�� �ó���� plan cache �� ����ؾ� �մϴ�.
        IDE_TEST( qcgPlan::registerPlanSynonym(
                      aStatement,
                      &sSynonymInfo,
                      aTableRef->userName,
                      aTableRef->tableName,
                      aTableRef->tableHandle,
                      NULL )
                  != IDE_SUCCESS );

        if ( aTableRef->tableInfo->tableType == QCM_DUMP_TABLE )
        {
            IDE_TEST( qcmDump::getDumpObjectInfo( aStatement, aTableRef )
                      != IDE_SUCCESS );

            // PROJ-1436
            // D$ table�� ������ plan cache���� �ʴ´�.
            // X$,V$,D$��� �������� ��ü�� DB��ü�� �ƴϴ�.
            // �׷��� X$,V$�� ��ü������ ���¹ݸ�,
            // D$�� ��ü������ �߻��Ѵ�.
            //���� D$�� Cache�����Դϴ�.
            QC_SHARED_TMPLATE(aStatement)->flag &= ~QC_TMP_PLAN_CACHE_IN_MASK;
            QC_SHARED_TMPLATE(aStatement)->flag |= QC_TMP_PLAN_CACHE_IN_OFF;
        }
        else
        {
            /* Nothing to do */
        }

        /* BUG-42639 Monitoring query */
        if ( ( QCG_GET_SESSION_OPTIMIZER_PERFORMANCE_VIEW(aStatement) == 1 ) &&
             ( iduProperty::getQueryProfFlag() == 0 ) )
        {
            if ( QCM_IS_OPERATABLE_QP_FT_TRANS( aTableRef->tableInfo->operatableFlag )
                 == ID_FALSE )
            {
                QC_SHARED_TMPLATE(aStatement)->flag &= ~QC_TMP_ALL_FIXED_TABLE_MASK;
                QC_SHARED_TMPLATE(aStatement)->flag |= QC_TMP_ALL_FIXED_TABLE_FALSE;
            }
            else
            {
                /* Nothing to do */
            }
        }
        else
        {
            QC_SHARED_TMPLATE(aStatement)->flag &= ~QC_TMP_ALL_FIXED_TABLE_MASK;
            QC_SHARED_TMPLATE(aStatement)->flag |= QC_TMP_ALL_FIXED_TABLE_FALSE;
        }
    }
    else  // �Ϲ� ���̺�
    {
        // BUG-42639 Monitoring query
        QC_SHARED_TMPLATE(aStatement)->flag &= ~QC_TMP_ALL_FIXED_TABLE_MASK;
        QC_SHARED_TMPLATE(aStatement)->flag |= QC_TMP_ALL_FIXED_TABLE_FALSE;
        // ���̺��� �����ϸ� ó���� ��� ������
        // BUG-34492
        // validation lock�̸� ����ϴ�.
        IDE_TEST(qcm::lockTableForDMLValidation(
                     aStatement,
                     sTableHandle,
                     aTableRef->tableSCN)
                 != IDE_SUCCESS);

        // CREATE VIEW ���� ���� �� SELECT ���� VIEW�� INVALID�� ���¸�
        //  ������ �߻���Ų��.
        if ( ( (aFlag & QMV_VIEW_CREATION_MASK) == QMV_VIEW_CREATION_TRUE ) &&
             ( ( aTableRef->tableInfo->tableType == QCM_VIEW ) ||
               ( aTableRef->tableInfo->tableType == QCM_MVIEW_VIEW ) ) &&
             ( aTableRef->tableInfo->status == QCM_VIEW_INVALID ) )
        {
            sqlInfo.setSourceInfo(aStatement, &aTableRef->tableName);
            IDE_RAISE( ERR_INVALID_VIEW );
        }
        else
        {
            // Nothing to do.
        }

        // PROJ-1350 - Plan Tree �ڵ� �籸��
        // Execution ������ table handle�� ������ ����ϱ� ���Ͽ�
        // Table Meta Cache�� ������ table handle������ �����Ѵ�.
        // �̴� Table Meta Cache������ DDL � ���� ����� �� �ֱ�
        // �����̴�.
        aTableRef->tableHandle = aTableRef->tableInfo->tableHandle;
        aTableRef->tableType   = aTableRef->tableInfo->tableType;
        aTableRef->tableFlag   = aTableRef->tableInfo->tableFlag;
        aTableRef->tableOID    = aTableRef->tableInfo->tableOID;

        // PROJ-2646 shard analyzer enhancement
        if ( ( sdi::isShardCoordinator( aStatement ) == ID_TRUE ) ||
             ( sdi::isPartialCoordinator( aStatement ) == ID_TRUE ) ||
             ( sdi::detectShardMetaChange( aStatement ) == ID_TRUE ) )
        {
            IDE_TEST( sdi::getTableInfo( aStatement,
                                         aTableRef->tableInfo,
                                         &(aTableRef->mShardObjInfo) )
                      != IDE_SUCCESS );
        }
        else
        {
            // Nothing to do.
        }

        // environment�� ���
        IDE_TEST( qcgPlan::registerPlanTable(
                      aStatement,
                      aTableRef->tableHandle,
                      aTableRef->tableSCN,
                      aTableRef->tableInfo->tableOwnerID, /* BUG-45893 */
                      aTableRef->tableInfo->name )        /* BUG-45893 */
                  != IDE_SUCCESS );

        // environment�� ���
        IDE_TEST( qcgPlan::registerPlanSynonym(
                      aStatement,
                      & sSynonymInfo,
                      aTableRef->userName,
                      aTableRef->tableName,
                      aTableRef->tableHandle,
                      NULL )
                  != IDE_SUCCESS );

        // PROJ-1502 PARTITIONED DISK TABLE
        if ( aTableRef->partitionRef != NULL )
        {
            // ��Ƽ���� ������ �����.
            if ( aTableRef->tableInfo->tablePartitionType !=
                 QCM_PARTITIONED_TABLE )
            {
                sqlInfo.setSourceInfo(aStatement,
                                      &aTableRef->tableName);
                // ��Ƽ�ǵ� ���̺��� �ƴ� ���. ���� �߻���Ŵ
                IDE_RAISE( ERR_NOT_PART_TABLE );
            }
            else
            {
                /* nothing to do */
            }

            // ��Ƽ�ǵ� ���̺��� ��� ��Ƽ�� ���� ����
            IDE_TEST( qcmPartition::getPartitionInfo(
                          aStatement,
                          aTableRef->tableInfo->tableID,
                          aTableRef->partitionRef->partitionName,
                          &aTableRef->partitionRef->partitionInfo,
                          &aTableRef->partitionRef->partitionSCN,
                          &aTableRef->partitionRef->partitionHandle )
                      != IDE_SUCCESS );

            // BUG-34492
            // validation lock�̸� ����ϴ�.
            IDE_TEST( qmx::lockPartitionForDML( QC_SMI_STMT( aStatement ),
                                                aTableRef->partitionRef,
                                                SMI_TABLE_LOCK_IS )
                      != IDE_SUCCESS );

            // To fix BUG-17479
            IDE_TEST( qcmPartition::getPartitionOrder(
                          QC_SMI_STMT( aStatement ),
                          aTableRef->tableInfo->tableID,
                          (UChar*)(aTableRef->partitionRef->partitionName.stmtText +
                                   aTableRef->partitionRef->partitionName.offset),
                          aTableRef->partitionRef->partitionName.size,
                          & aTableRef->partitionRef->partOrder )
                      != IDE_SUCCESS );

            aTableRef->partitionRef->partitionOID =
                aTableRef->partitionRef->partitionInfo->tableOID;

            aTableRef->partitionRef->partitionID =
                aTableRef->partitionRef->partitionInfo->partitionID;

            aTableRef->selectedPartitionID =
                aTableRef->partitionRef->partitionID;

            // set flag
            // ���� ������� �Ǿ��ٴ� flag����.
            aTableRef->flag &= ~QMS_TABLE_REF_PRE_PRUNING_MASK;
            aTableRef->flag |= QMS_TABLE_REF_PRE_PRUNING_TRUE;

        } // if( aTableRef->partitionRef != NULL )
        else
        {
            /* nothing to do */
        }

        // make index table reference
        if ( aTableRef->tableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
        {
            IDE_TEST( qmsIndexTable::makeIndexTableRef(
                          aStatement,
                          aTableRef->tableInfo,
                          & aTableRef->indexTableRef,
                          & aTableRef->indexTableCount )
                      != IDE_SUCCESS );

            // BUG-34492
            // validation lock�̸� ����ϴ�.
            IDE_TEST( qmsIndexTable::validateAndLockIndexTableRefList( aStatement,
                                                                       aTableRef->indexTableRef,
                                                                       SMI_TABLE_LOCK_IS )
                      != IDE_SUCCESS );
        }
        else
        {
            // Nothing to do.
        }

        // shard table�� shard view������ ������ �� �ִ�.
        if ( ( SDI_CHECK_QUERYSET_LIST_STATE( aStatement->mShardQuerySetList,
                                              SDI_QUERYSET_LIST_STATE_MAIN_ANALYZE_DONE )
               == ID_TRUE )
             &&
             ( aStatement->myPlan->parseTree->stmtShard == QC_STMT_SHARD_NONE )
             &&
             ( aTableRef->mShardObjInfo != NULL ) )
        {
            if ( aTableRef->mShardObjInfo->mIsLocalForce == ID_FALSE )
            {
                // shard transform ������ ����Ѵ�.
                IDE_TEST( qmvShardTransform::raiseInvalidShardQuery( aStatement ) != IDE_SUCCESS );

                sqlInfo.setSourceInfo( aStatement,
                                       &(aTableRef->tableName) );
                IDE_RAISE( ERR_EXIST_SHARD_TABLE_OUTSIDE_SHARD_VIEW );
            }
            else
            {
                /* Nothing to do. */
            }
        }
        else
        {
            // Nothing to do.
        }

        if( aStatement->spvEnv->createPkg != NULL )
        {
            IDE_TEST( qsvPkgStmts::makeRelatedObjects(
                          aStatement,
                          & aTableRef->userName,
                          & aTableRef->tableName,
                          & sSynonymInfo,
                          aTableRef->tableInfo->tableID,
                          QS_TABLE)
                      != IDE_SUCCESS );
        }
        else
        {
            if ( (aStatement->spvEnv->createProc != NULL) || // PSM
                 (aStatement->myPlan->parseTree->stmtKind == QCI_STMT_SELECT) ) // VIEW
            {
                // search or make related object list
                IDE_TEST(qsvProcStmts::makeRelatedObjects(
                             aStatement,
                             & aTableRef->userName,
                             & aTableRef->tableName,
                             & sSynonymInfo,
                             aTableRef->tableInfo->tableID,
                             QS_TABLE )
                         != IDE_SUCCESS);
            }
            else
            {
                // Nothing to do.
            }
        }
    }

    // set alias name
    if (QC_IS_NULL_NAME(aTableRef->aliasName) == ID_TRUE)
    {
        SET_POSITION( aTableRef->aliasName, aTableRef->tableName );
    }
    
    /* PROJ-2441 flashback */
    if( sIsFixedTable == ID_TRUE ) // BUG-16651
    {
        if( smiFixedTable::canUse( aTableRef->tableInfo->tableHandle )
            == ID_FALSE &&
            ((aFlag & QMV_PERFORMANCE_VIEW_CREATION_MASK) ==
             QMV_PERFORMANCE_VIEW_CREATION_FALSE ) &&
            ((aFlag & QMV_VIEW_CREATION_MASK) ==
             QMV_VIEW_CREATION_FALSE )  )
        {
            sqlInfo.setSourceInfo(
                aStatement,
                &aTableRef->tableName);
            IDE_RAISE( ERR_DML_ON_CLOSED_TABLE );
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

    // add tuple set
    IDE_TEST(qtc::nextTable(
                 &(aTableRef->table),
                 aStatement,
                 aTableRef->tableInfo,
                 QCM_TABLE_TYPE_IS_DISK(aTableRef->tableInfo->tableFlag),
                 aIsNullableFlag ) // PR-13597
             != IDE_SUCCESS);

    // PROJ-1502 PARTITIONED DISK TABLE
    if( aTableRef->tableInfo->tablePartitionType ==
        QCM_PARTITIONED_TABLE )
    {
        QC_SHARED_TMPLATE(aStatement)->tmplate.rows[aTableRef->table].lflag
            &= ~MTC_TUPLE_PARTITIONED_TABLE_MASK;

        QC_SHARED_TMPLATE(aStatement)->tmplate.rows[aTableRef->table].lflag
            |= MTC_TUPLE_PARTITIONED_TABLE_TRUE;
    }

    IDE_TEST_RAISE(
        ((aTableRef->tableInfo->tableType != QCM_QUEUE_TABLE) &&
         (aStatement->myPlan->parseTree->stmtKind == QCI_STMT_DEQUEUE)),
        ERR_DEQUEUE_ON_TABLE);

    aTableRef->mParallelDegree = IDL_MIN(aTableRef->tableInfo->parallelDegree,
                                         QMV_QUERYSET_PARALLEL_DEGREE_LIMIT);

    IDE_DASSERT( aTableRef->mParallelDegree != 0 );

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_EXIST_SHARD_TABLE_OUTSIDE_SHARD_VIEW)
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(sdERR_ABORT_EXIST_SHARD_TABLE_OUTSIDE_SHARD_VIEW,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION(ERR_DML_ON_CLOSED_TABLE)
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QMV_DML_ON_CLOSED_TABLE,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION(ERR_NOT_EXIST_TABLE);
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QCV_NOT_EXISTS_TABLE,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION(ERR_DEQUEUE_ON_TABLE);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDQ_TABLE_DEQUEUE_DENIED));
    }
    IDE_EXCEPTION(ERR_NOT_PART_TABLE);
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QCV_NOT_PARTITIONED_TABLE,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION(ERR_INVALID_VIEW);
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QDV_INVALID_VIEW,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmvQuerySet::validateView(
    qcStatement * aStatement,
    qmsSFWGH    * aSFWGH,
    qmsTableRef * aTableRef,
    UInt          aIsNullableFlag)
{
    qmsParseTree      * sParseTree;
    qmsQuerySet       * sQuerySet;
    volatile UInt       sSessionUserID;   // for fixing BUG-6096
    qcuSqlSourceInfo    sqlInfo;
    idBool              sExist = ID_FALSE;
    idBool              sIsFixedTable = ID_FALSE;
    volatile idBool     sIndirectRef; /* BUG-45994 - �����Ϸ� ����ȭ ȸ�� */
    idBool              sIsShardView  = ID_FALSE;
    void              * sTableHandle  = NULL;
    qmsQuerySet       * sLateralViewQuerySet = NULL;
    qcmColumn         * sColumnAlias = NULL;
    qcmSynonymInfo      sSynonymInfo;
    volatile idBool     sCalledByPSMFlag;
    UInt                sTableType;
    UInt                i;
    UInt                sDepTupleID;
    qmsTableRef       * sTableRef = NULL;

    IDE_FT_BEGIN();

    IDU_FIT_POINT_FATAL( "qmvQuerySet::validateView::__FT__" );

    idlOS::memset( &sSynonymInfo, 0x00, ID_SIZEOF( qcmSynonymInfo ) );
    sSynonymInfo.isSynonymName = ID_FALSE;

    sCalledByPSMFlag = aTableRef->view->calledByPSMFlag;
    sIndirectRef     = ID_FALSE; /* BUG-45994 - �����Ϸ� ����ȭ ȸ�� */

    // To Fix PR-1176
    sSessionUserID = QCG_GET_SESSION_USER_ID( aStatement );

    // A parse tree of real view is set in parser.
    IDE_TEST_RAISE(aStatement->myPlan->parseTree->stmtKind == QCI_STMT_DEQUEUE,
                   ERR_DEQUEUE_ON_TABLE);
    
    /* BUG-46124 */
    IDE_TEST( qmsPreservedTable::checkAndSetPreservedInfo( aSFWGH,
                                                           aTableRef )
              != IDE_SUCCESS );

    // get aTableRef->tableInfo for CREATED VIEW
    // PROJ-2083 DUAL Table
    if ( ( aTableRef->flag & QMS_TABLE_REF_CREATED_VIEW_MASK )
         == QMS_TABLE_REF_CREATED_VIEW_TRUE )
    {
        IDE_TEST(
            qcmSynonym::resolveTableViewQueue(
                aStatement,
                aTableRef->userName,
                aTableRef->tableName,
                &(aTableRef->tableInfo),
                &(aTableRef->userID),
                &(aTableRef->tableSCN),
                &sExist,
                &sSynonymInfo,
                &sTableHandle)
            != IDE_SUCCESS);

        if( sExist == ID_TRUE )
        {   
            sTableType = smiGetTableFlag( sTableHandle ) & SMI_TABLE_TYPE_MASK;
            
            if( sTableType == SMI_TABLE_FIXED )
            {
                sIsFixedTable = ID_TRUE;
            }
            else
            {
                sIsFixedTable = ID_FALSE;
            }
        }
        else
        {
            sqlInfo.setSourceInfo( aStatement,
                                   & aTableRef->tableName );
            IDE_RAISE( ERR_NOT_EXIST_TABLE );
        }
        
        if( sIsFixedTable == ID_TRUE )  // BUG-16651
        {
            // userID is set in qmv::parseViewInFromClause
            // BUG-17452
            aTableRef->tableHandle = aTableRef->tableInfo->tableHandle;
            aTableRef->tableType   = aTableRef->tableInfo->tableType;
            aTableRef->tableFlag   = aTableRef->tableInfo->tableFlag;
            aTableRef->tableOID    = aTableRef->tableInfo->tableOID;

            // BUG-45522 fixed table �� ����Ű�� �ó���� plan cache �� ����ؾ� �մϴ�.
            IDE_TEST( qcgPlan::registerPlanSynonym( aStatement,
                                                    &sSynonymInfo,
                                                    aTableRef->userName,
                                                    aTableRef->tableName,
                                                    aTableRef->tableHandle,
                                                    NULL )
                      != IDE_SUCCESS );

            if (QC_IS_NULL_NAME(aTableRef->aliasName) == ID_TRUE)
            {
                SET_POSITION( aTableRef->aliasName, aTableRef->tableName );
            }

            // set aTableRef->sameViewRef
            IDE_TEST( setSameViewRef(aStatement, aSFWGH, aTableRef)
                      != IDE_SUCCESS );
        }
        else
        {
            /* BUG-42639 Monitoring query
             * �Ϲ� View�� ��� recreate �� �ɼ� �־ Lock�� ��ƾ�������
             * Transaction�� �ʿ��ϴ�
             */
            QC_SHARED_TMPLATE(aStatement)->flag &= ~QC_TMP_ALL_FIXED_TABLE_MASK;
            QC_SHARED_TMPLATE(aStatement)->flag |= QC_TMP_ALL_FIXED_TABLE_FALSE;

            // BUG-34492
            // validation lock�̸� ����ϴ�.
            IDE_TEST( qcm::lockTableForDMLValidation(
                          aStatement,
                          sTableHandle,
                          aTableRef->tableSCN )
                      != IDE_SUCCESS );

            // BUG-17452
            aTableRef->tableHandle = aTableRef->tableInfo->tableHandle;
            aTableRef->tableType   = aTableRef->tableInfo->tableType;
            aTableRef->tableFlag   = aTableRef->tableInfo->tableFlag;
            aTableRef->tableOID    = aTableRef->tableInfo->tableOID;

            // environment�� ���
            IDE_TEST( qcgPlan::registerPlanTable( aStatement,
                                                  aTableRef->tableHandle,
                                                  aTableRef->tableSCN,
                                                  aTableRef->tableInfo->tableOwnerID, /* BUG-45893 */
                                                  aTableRef->tableInfo->name )        /* BUG-45893 */
                      != IDE_SUCCESS );
                
            // environment�� ���
            IDE_TEST( qcgPlan::registerPlanSynonym( aStatement,
                                                    & sSynonymInfo,
                                                    aTableRef->userName,
                                                    aTableRef->tableName,
                                                    aTableRef->tableHandle,
                                                    NULL )
                      != IDE_SUCCESS );
                    
            if (QC_IS_NULL_NAME(aTableRef->aliasName) == ID_TRUE)
            {
                SET_POSITION( aTableRef->aliasName, aTableRef->tableName );
            }

            // set aTableRef->sameViewRef
            IDE_TEST( setSameViewRef(aStatement, aSFWGH, aTableRef)
                      != IDE_SUCCESS );
        }
    }

    // PROJ-1889 INSTEAD OF TRIGGER : select �ܿ� DML�� �ü� �ִ�.
    // select �ϰ�츸 forUpdate�� �Ҵ��Ѵ�.
    if (aStatement->myPlan->parseTree->stmtKind == QCI_STMT_SELECT )
    {
        // set forUpdate ptr
        sParseTree = (qmsParseTree*) aStatement->myPlan->parseTree;

        ((qmsParseTree *)(aTableRef->view->myPlan->parseTree))->forUpdate =
            sParseTree->forUpdate;
    }
    
    // set member of qcStatement
    aTableRef->view->myPlan->planEnv = aStatement->myPlan->planEnv;
    aTableRef->view->spvEnv          = aStatement->spvEnv;
    aTableRef->view->mRandomPlanInfo = aStatement->mRandomPlanInfo;

    aTableRef->view->mFlag &= ~QC_STMT_VIEW_MASK;
    aTableRef->view->mFlag |= QC_STMT_VIEW_TRUE;

    /* PROJ-2197 PSM Renewal
     * view�� target�� �ִ� PSM ������ host ������ ����ϴ� ���
     * ������ casting �ϱ� ���ؼ� flag�� �����Ѵ�.
     * (qmvQuerySet::addCastOper ���� ����Ѵ�.) */
    aTableRef->view->calledByPSMFlag = aStatement->calledByPSMFlag;

    if (aTableRef->tableInfo != NULL)
    {
        //-----------------------------------------
        // CREATED VIEW
        //-----------------------------------------
        if ( ( aTableRef->tableInfo->tableType == QCM_VIEW ) ||
             ( aTableRef->tableInfo->tableType == QCM_MVIEW_VIEW ) )
        {
            // for fixing BUG-6096
            // get current session userID
            // sSessionUserID = aStatement->session->userInfo->userID;
            // modify session userID
            QCG_SET_SESSION_USER_ID( aStatement, aTableRef->userID );

            // PROJ-1436
            // environment�� ��Ͻ� ���� ���� ��ü�� ���� user��
            // privilege�� ����� �����Ѵ�.
            qcgPlan::startIndirectRefFlag( aStatement, (idBool *) & sIndirectRef );

            /* BUG-45994 */
            IDU_FIT_POINT_FATAL( "qmvQuerySet::validateView::__FT__::STAGE1" );

            /* TASK-7219 Shard Transformer Refactoring
             *  PROJ-2646 shard analyzer
             *   shard view�� ���� statement������ shard table�� �� �� �ִ�.
             */
            IDE_TEST( sdi::setShardStmtType( aStatement,
                                             aTableRef->view )
                      != IDE_SUCCESS );

            // inline view validation
            IDE_TEST(qmv::validateSelect(aTableRef->view) != IDE_SUCCESS);

            // for fixing BUG-6096
            // re-set current session userID
            QCG_SET_SESSION_USER_ID( aStatement, sSessionUserID );

            qcgPlan::endIndirectRefFlag( aStatement, (idBool *) & sIndirectRef );

            if( aStatement->spvEnv->createPkg != NULL )
            {
                IDE_TEST( qsvPkgStmts::makeRelatedObjects(
                              aStatement,
                              & aTableRef->userName,
                              & aTableRef->tableName,
                              & sSynonymInfo,
                              aTableRef->tableInfo->tableID,
                              QS_TABLE)
                          != IDE_SUCCESS );
            }
            else 
            {
                if( (aStatement->spvEnv->createProc != NULL) || // PSM
                    (aStatement->myPlan->parseTree->stmtKind == QCI_STMT_SELECT) ) // VIEW
                {
                    // search or make related object list
                    IDE_TEST(qsvProcStmts::makeRelatedObjects(
                                 aStatement,
                                 & aTableRef->userName,
                                 & aTableRef->tableName,
                                 & sSynonymInfo,
                                 aTableRef->tableInfo->tableID,
                                 QS_TABLE )
                             != IDE_SUCCESS);
                }
                else
                {
                    // Nothing to do.
                } 
            }
        }

        else if( aTableRef->tableInfo->tableType == QCM_PERFORMANCE_VIEW )
        {
            // for fixing BUG-6096
            // get current session userID
            // sSessionUserID = aStatement->session->userInfo->userID;
            // modify session userID
            QCG_SET_SESSION_USER_ID( aStatement, aTableRef->userID );

            // PROJ-1436
            // environment�� ��Ͻ� ���� ���� ��ü�� ���� user��
            // privilege�� ����� �����Ѵ�.
            qcgPlan::startIndirectRefFlag( aStatement, (idBool *) & sIndirectRef );

            /* BUG-45994 */
            IDU_FIT_POINT_FATAL( "qmvQuerySet::validateView::__FT__::STAGE1" );

            /* TASK-7219 Shard Transformer Refactoring
             *  PROJ-2646 shard analyzer
             *   shard view�� ���� statement������ shard table�� �� �� �ִ�.
             */
            IDE_TEST( sdi::setShardStmtType( aStatement,
                                             aTableRef->view )
                      != IDE_SUCCESS );

            // inline view validation
            // BUG-34390
            if ( qmv::validateSelect(aTableRef->view) != IDE_SUCCESS )
            {
                if ( ( ( aTableRef->flag & QMS_TABLE_REF_CREATED_VIEW_MASK )
                       == QMS_TABLE_REF_CREATED_VIEW_TRUE ) &&
                     ( ideGetErrorCode() == qpERR_ABORT_QMV_DML_ON_CLOSED_TABLE ) )
                {
                    sqlInfo.setSourceInfo( aStatement, & aTableRef->tableName );
                    IDE_RAISE( ERR_DML_ON_CLOSED_TABLE );
                }
                else
                {
                    IDE_TEST(1);
                }
            }
            else
            {
                // Nothing to do.
            }

            // for fixing BUG-6096
            // re-set current session userID
            QCG_SET_SESSION_USER_ID( aStatement, sSessionUserID );

            qcgPlan::endIndirectRefFlag( aStatement, (idBool *) & sIndirectRef );
        }
        else
        {
            sqlInfo.setSourceInfo( aStatement,
                                   & aTableRef->tableName );
            IDE_RAISE( ERR_NOT_EXIST_TABLE );
        }

        // PROJ-2418
        // Inline View�� �ƴϹǷ� Lateral Flag�� �ʱ�ȭ�Ѵ�.
        aTableRef->flag &= ~QMS_TABLE_REF_LATERAL_VIEW_MASK;
        aTableRef->flag |= QMS_TABLE_REF_LATERAL_VIEW_FALSE;
    }
    else
    {
        //-----------------------------------------
        // inline view
        //-----------------------------------------
        sQuerySet = ( ( qmsParseTree* )( aTableRef->view->myPlan->parseTree ) )->querySet;

        /* insert aSFWGH == NULL �̴�. */
        if ( aSFWGH != NULL )
        {
            // To fix BUG-24213
            // performance view creation flag�� ����
            sQuerySet->lflag
                &= ~(QMV_PERFORMANCE_VIEW_CREATION_MASK);
            sQuerySet->lflag
                |= (aSFWGH->lflag & QMV_PERFORMANCE_VIEW_CREATION_MASK);
            sQuerySet->lflag
                &= ~(QMV_VIEW_CREATION_MASK);
            sQuerySet->lflag
                |= (aSFWGH->lflag & QMV_VIEW_CREATION_MASK);

            // PROJ-2415 Grouping Sets Clause
            // Transform�� ���� ������ View�� Dependency�� ���� �� �ֵ��� Outer Query�� ����
            if ( ( ( aSFWGH->lflag & QMV_SFWGH_GBGS_TRANSFORM_MASK )
                   == QMV_SFWGH_GBGS_TRANSFORM_TOP ) ||
                 ( ( aSFWGH->lflag & QMV_SFWGH_GBGS_TRANSFORM_MASK )
                   == QMV_SFWGH_GBGS_TRANSFORM_MIDDLE ) )
            {
                sQuerySet->outerSFWGH = aSFWGH;
            }
            else
            {
                /* Nothing to do */
            }

            if ( ( ( aSFWGH->thisQuerySet->lflag & QMV_QUERYSET_LATERAL_MASK )
                   == QMV_QUERYSET_LATERAL_TRUE ) &&
                 ( ( aSFWGH->lflag & QMV_SFWGH_GBGS_TRANSFORM_MASK )
                   != QMV_SFWGH_GBGS_TRANSFORM_NONE ) )
            {
                // BUG-41573 Lateral View WITH GROUPING SETS
                // ������ Lateral View��� GBGS�� ���� �Ļ��� Inline View��
                // ��� Lateral View�� �Ǿ�� �Ѵ�.
                aTableRef->flag &= ~QMS_TABLE_REF_LATERAL_VIEW_MASK;
                aTableRef->flag |= QMS_TABLE_REF_LATERAL_VIEW_TRUE;
            }
        }
        else
        {
            /* Nothing To Do */
        }
        
        // PROJ-2418
        if ( ( aTableRef->flag & QMS_TABLE_REF_LATERAL_VIEW_MASK ) == 
             QMS_TABLE_REF_LATERAL_VIEW_TRUE )
        {
            if ( aSFWGH != NULL )
            {
                sLateralViewQuerySet =
                    ((qmsParseTree*) aTableRef->view->myPlan->parseTree)->querySet;

                // Lateral View SFWGH�� outerQuery / outerFrom ����
                setLateralOuterQueryAndFrom( sLateralViewQuerySet,
                                             aTableRef,
                                             aSFWGH );

                // BUG-41573 Lateral View Flag ����
                sLateralViewQuerySet->lflag &= ~QMV_QUERYSET_LATERAL_MASK;
                sLateralViewQuerySet->lflag |= QMV_QUERYSET_LATERAL_TRUE;
            }
            else
            {
                /*******************************************************************
                 *
                 *  INSERT ... SELECT ������ �Ʒ��� ���� �ȴ�.
                 *  - aTableRef�� SELECT Main Query
                 *  - aSFWGH (TableRef ���������� �ܺ� SFWGH)�� NULL
                 *
                 *  �׷��� SELECT Main Query�� LATERAL Flag�� ���� �� �����Ƿ�
                 *  aSFWGH�� NULL�̸鼭 aTableRef�� LATERAL Flag�� ������ ���� ����.
                 *
                 *******************************************************************/
                IDE_DASSERT(0);
            }
        }
        else
        {
            // Lateral View�� �ƴ� ���
            // Nothing to do.
        }

        /* TASK-7219 Shard Transformer Refactoring
         *  PROJ-2646 shard analyzer
         *   shard view�� ���� statement������ shard table�� �� �� �ִ�.
         */
        IDE_TEST( sdi::setShardStmtType( aStatement,
                                         aTableRef->view )
                  != IDE_SUCCESS );

        // inline view validation
        IDE_TEST(qmv::validateSelect(aTableRef->view) != IDE_SUCCESS);

        /* PROJ-2641 Hierarchy Query Index */
        if ( ( ( aTableRef->flag & QMS_TABLE_REF_HIER_VIEW_MASK )
               == QMS_TABLE_REF_HIER_VIEW_TRUE ) &&
             ( ( aStatement->myPlan->parseTree->stmtShard == QC_STMT_SHARD_NONE ) ||
               ( aStatement->myPlan->parseTree->stmtShard == QC_STMT_SHARD_META ) ) )
        {
            sParseTree = (qmsParseTree*) aTableRef->view->myPlan->parseTree;

            sTableRef = sParseTree->querySet->SFWGH->from->tableRef;

            sParseTree = (qmsParseTree*) aStatement->myPlan->parseTree;

            // PROJ-2654 shard view �� �����Ѵ�.
            if ( sTableRef->view != NULL )
            {
                if ( ( sTableRef->view->myPlan->parseTree->stmtShard != QC_STMT_SHARD_NONE ) &&
                     ( sTableRef->view->myPlan->parseTree->stmtShard != QC_STMT_SHARD_META ) )
                {
                    sIsShardView = ID_TRUE;
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

            /* Hierarchy Query�� ������ select * from t1 --> select * from (
             * select * from t1 )���� transform �ؼ� ó���ϴµ� �Ϲ� ���̺� ��
             * dual table��  ��쿡 ���ؼ� transform�� view�� ���ְ� ����
             * tableRef �� ����� �����Ѵ�
             */

            if ( ( sTableRef->remoteTable == NULL ) &&
                 ( sTableRef->tableInfo->temporaryInfo.type == QCM_TEMPORARY_ON_COMMIT_NONE ) &&
                 ( sTableRef->tableInfo->tablePartitionType != QCM_PARTITIONED_TABLE ) &&
                 ( sTableRef->tableInfo->tableType != QCM_DUMP_TABLE ) &&
                 ( sTableRef->tableInfo->tableType != QCM_PERFORMANCE_VIEW ) &&
                 ( sParseTree->isSiblings == ID_FALSE ) &&
                 ( sIsShardView == ID_FALSE ) &&
                 ( QCU_OPTIMIZER_HIERARCHY_TRANSFORMATION == 0 ) )
            {
                if ( sTableRef->tableInfo->tableType == QCM_FIXED_TABLE )
                {
                    if ( ( ( sTableRef->tableInfo->tableOwnerID == QC_SYSTEM_USER_ID ) &&
                           ( idlOS::strMatch( sTableRef->tableInfo->name,
                                              idlOS::strlen(sTableRef->tableInfo->name),
                                              "SYS_DUMMY_",
                                              10 ) == 0 ) ) ||
                         ( idlOS::strMatch( sTableRef->tableInfo->name,
                                            idlOS::strlen(sTableRef->tableInfo->name),
                                            "X$DUAL",
                                            6 ) == 0 ) )
                    {
                        sTableRef->pivot     = aTableRef->pivot;
                        sTableRef->unpivot   = aTableRef->unpivot;
                        sTableRef->aliasName = aTableRef->aliasName;
                        idlOS::memcpy( aTableRef, sTableRef, sizeof( qmsTableRef ) );

                        IDE_CONT( NORMAL_EXIT );
                    }
                    else
                    {
                        /* Nothing to do */
                    }
                }
                else
                {
                    sTableRef->pivot     = aTableRef->pivot;
                    sTableRef->unpivot   = aTableRef->unpivot;
                    sTableRef->aliasName = aTableRef->aliasName;
                    idlOS::memcpy( aTableRef, sTableRef, sizeof( qmsTableRef ) );

                    IDE_CONT( NORMAL_EXIT );
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

        // PROJ-2418 Lateral View ��ó��
        if ( sLateralViewQuerySet != NULL )
        {
            // View QuerySet�� outerDepInfo�� �����ϴ� ��쿡��
            // lateralDepInfo�� outerDepInfo�� ORing �Ѵ�.
            IDE_TEST( qmvQTC::setLateralDependenciesLast( sLateralViewQuerySet )
                      != IDE_SUCCESS );
                      
            if ( qtc::haveDependencies( & sLateralViewQuerySet->lateralDepInfo ) == ID_FALSE )
            {
                // LATERAL / APPLY Keyword�� ������ ������ Lateral View�� �ƴ� ���
                // Flag�� ���� �����Ѵ�.
                aTableRef->flag &= ~QMS_TABLE_REF_LATERAL_VIEW_MASK;
                aTableRef->flag |= QMS_TABLE_REF_LATERAL_VIEW_FALSE;
            }
            else
            {
                // ���� Lateral View�� ���ǵ� ���
                // BUG-43705 lateral view�� simple view merging�� ���������� ����� �ٸ��ϴ�.
                for ( i = 0; i < sLateralViewQuerySet->lateralDepInfo.depCount; i++ )
                {
                    sDepTupleID = sLateralViewQuerySet->lateralDepInfo.depend[i];
                    QC_SHARED_TMPLATE(aStatement)->tmplate.rows[sDepTupleID].lflag |= MTC_TUPLE_LATERAL_VIEW_REF_TRUE;
                }
            }
        }
        else
        {
            // LATERAL / APPLY Keyword�� ���� View
            // Nothing to do.
        }

        // PROJ-2415 Grouping Sets Clause 
        // Tranform�� ���� ������ View�� Outer Dependency�� �ڽ��� Outer Dependency�� Or�Ѵ�.
        if ( ( aSFWGH != NULL ) &&
             ( ( aSFWGH->lflag & QMV_SFWGH_GBGS_TRANSFORM_MASK )
               != QMV_SFWGH_GBGS_TRANSFORM_NONE ) )
        {
            IDE_TEST( qtc::dependencyOr( & sQuerySet->outerDepInfo,
                                         & aSFWGH->outerDepInfo,
                                         & aSFWGH->outerDepInfo )
                      != IDE_SUCCESS );
        }
        else
        {
            /* Nothing to do */
        }

        /* TASK-7217 Sharded sequence
           Shard transform�� ���ؼ� sequence�� shard view������ ���� ���� ����Ѵ�. */
        if ( (aTableRef->view->myPlan->parseTree->stmtShard != QC_STMT_SHARD_DATA) &&
             (((qmsParseTree*)aTableRef->view->myPlan->parseTree)->isShardView == ID_FALSE) &&
             (aTableRef->view->myPlan->mShardAnalysis == NULL) )
        {
            if (aTableRef->view->myPlan->parseTree->currValSeqs != NULL)
            {
                sqlInfo.setSourceInfo(
                    aStatement,
                    & aTableRef->view->myPlan->parseTree->
                    currValSeqs->sequenceNode->position );
                IDE_RAISE( ERR_USE_SEQUENCE_IN_VIEW );
            }

            if (aTableRef->view->myPlan->parseTree->nextValSeqs != NULL)
            {
                sqlInfo.setSourceInfo(
                    aStatement,
                    & aTableRef->view->myPlan->parseTree->
                    nextValSeqs->sequenceNode->position );
                IDE_RAISE( ERR_USE_SEQUENCE_IN_VIEW );
            }
        }

        // set USER ID
        aTableRef->userID = QCG_GET_SESSION_USER_ID(aStatement);

        // PROJ-2582 recursive with
        // with���� view�̰� column alias�� �ִ� ��� �÷��̸��� column alias�� �̿��Ѵ�.
        if ( aTableRef->withStmt != NULL )
        {
            sColumnAlias = aTableRef->withStmt->columns;
        }
        else
        {
            // Nothing to do.
        }
        
        // make qcmTableInfo for IN-LINE VIEW
        IDE_TEST(makeTableInfo(
                     aStatement,
                     ((qmsParseTree *)aTableRef->view->myPlan->parseTree)->querySet,
                     sColumnAlias,
                     &aTableRef->tableInfo,
                     QS_EMPTY_OID)
                 != IDE_SUCCESS);

        // BUG-37136
        aTableRef->tableHandle = aTableRef->tableInfo->tableHandle;  // NULL�̴�
        aTableRef->tableType   = aTableRef->tableInfo->tableType;
        aTableRef->tableFlag   = aTableRef->tableInfo->tableFlag;
        aTableRef->tableOID    = aTableRef->tableInfo->tableOID;

        // with ���� ���� ������ inline �信 ��� ���ʷ���Ʈ�� tableID�� �Ҵ��Ѵ�.
        if ( aTableRef->withStmt != NULL )
        {
            aTableRef->tableInfo->tableID = aTableRef->withStmt->tableID;
        }
        else
        {
            aTableRef->tableInfo->tableID = 0;
        }

        // set aTableRef->sameViewRef
        IDE_TEST( setSameViewRef( aStatement, aSFWGH, aTableRef )
                  != IDE_SUCCESS );
    }

    // add tuple set
    IDE_TEST(qtc::nextTable( &(aTableRef->table), aStatement, NULL, ID_TRUE, aIsNullableFlag ) // PR-13597
             != IDE_SUCCESS);

    // make mtcColumn and mtcExecute for inline view tuple
    IDE_TEST(makeTupleForInlineView(aStatement,
                                    aTableRef,
                                    aTableRef->table,
                                    aIsNullableFlag )
             != IDE_SUCCESS);

    aTableRef->view->calledByPSMFlag = sCalledByPSMFlag;

    IDE_EXCEPTION_CONT( NORMAL_EXIT );
    
    IDE_FT_END();

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_DEQUEUE_ON_TABLE);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDQ_TABLE_DEQUEUE_DENIED));
    }
    IDE_EXCEPTION(ERR_NOT_EXIST_TABLE);
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QCV_NOT_EXISTS_TABLE,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION(ERR_USE_SEQUENCE_IN_VIEW)
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QMV_USE_SEQUENCE_IN_VIEW,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION( ERR_DML_ON_CLOSED_TABLE )
    {
        // BUG-34390
        (void)sqlInfo.init( aStatement->qmeMem );
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMV_DML_ON_CLOSED_TABLE,
                                  sqlInfo.getErrMessage() ) );
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION_SIGNAL()
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_FAULT_TOLERATED ) );
    }
    IDE_EXCEPTION_END;

    IDE_FT_EXCEPTION_BEGIN();

    // To Fix PR-11776
    // View �� ���� Validation error �߻� ��
    // ������ User ID�� ������ �־�� �Ѵ�.
    QCG_SET_SESSION_USER_ID( aStatement, sSessionUserID );

    qcgPlan::endIndirectRefFlag( aStatement, (idBool *) & sIndirectRef );

    aTableRef->view->calledByPSMFlag = sCalledByPSMFlag;

    IDE_FT_EXCEPTION_END();

    return IDE_FAILURE;
}

IDE_RC qmvQuerySet::validateInlineView( qcStatement * aStatement,
                                        qmsSFWGH    * aSFWGH,
                                        qmsTableRef * aTableRef,
                                        UInt          aIsNullableFlag )
{
/***********************************************************************
 *
 * Description :
 *     PROJ-1718 Subquery unnesting
 *     Subquery���� transform�� inline view�� ���� ���� validateView����
 *     inline view�� ���� logic�� �и��Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    IDU_FIT_POINT_FATAL( "qmvQuerySet::validateInlineView::__FT__" );

    aTableRef->userID = QCG_GET_SESSION_USER_ID(aStatement);

    IDE_TEST(makeTableInfo(
                 aStatement,
                 ((qmsParseTree *)aTableRef->view->myPlan->parseTree)->querySet,
                 NULL,
                 &aTableRef->tableInfo,
                 QS_EMPTY_OID)
             != IDE_SUCCESS);

    // BUG-38264
    aTableRef->tableHandle = aTableRef->tableInfo->tableHandle;
    aTableRef->tableType   = aTableRef->tableInfo->tableType;

    if ( aTableRef->withStmt != NULL )
    {
        aTableRef->tableInfo->tableID = aTableRef->withStmt->tableID;
    }
    else
    {
        aTableRef->tableInfo->tableID = 0;
    }

    IDE_TEST( setSameViewRef( aStatement, aSFWGH, aTableRef )
              != IDE_SUCCESS );

    IDE_TEST(qtc::nextTable( &(aTableRef->table), aStatement, NULL, ID_TRUE, aIsNullableFlag )
             != IDE_SUCCESS);

    IDE_TEST(makeTupleForInlineView(aStatement,
                                    aTableRef,
                                    aTableRef->table,
                                    aIsNullableFlag )
             != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmvQuerySet::validateRecursiveView( qcStatement * aStatement,
                                           qmsSFWGH    * aSFWGH,
                                           qmsTableRef * aTableRef,
                                           UInt          aIsNullableFlag )
{
/***********************************************************************
 *
 * Description : PROJ-2582 recursive with
 *
 * Implementation :
 * 
 *     with q1(i1,pid) as (
 *       select id, pid from t1
 *       union all
 *       select q1.id, '/'||q1.pid from q1, t1 where q1.pid = t1.id
 *                                      ^^
 *                                      ���� recursive view
 *     )
 *     select * from q1;
 *                   ^^
 *                   �ֻ��� recursive view
 *
 *    q1�� i1�� union all�� left subquery�� target column type���� �����ϵ�
 *    precision�� right subquery�� target�� ���� Ŀ�� �� �ִ�.
 *
 ***********************************************************************/

    qmsParseTree      * sParseTree = NULL;
    qmsWithClause     * sWithClause = NULL;
    qcmColumn         * sColumnAlias = NULL;
    idBool              sIsFound = ID_FALSE;

    IDU_FIT_POINT_FATAL( "qmvQuerySet::validateRecursiveView::__FT__" );
    
    IDE_DASSERT( aTableRef->withStmt != NULL );

    sParseTree = (qmsParseTree*)aStatement->myPlan->parseTree;
    
    // set USER ID
    aTableRef->userID = QCG_GET_SESSION_USER_ID(aStatement);
    
    // with���� view�̰� column alias�� �ִ� ��� �÷��̸��� column alias�� �̿��Ѵ�.
    sColumnAlias = aTableRef->withStmt->columns;

    // query_name�� alias ���� �ؾ� �Ѵ�.
    IDE_TEST_RAISE( sColumnAlias == NULL, ERR_NO_COLUMN_ALIAS_RECURSIVE_VIEW );

    // query_name �ߺ� üũ
    for ( sWithClause = sParseTree->withClause;
          sWithClause != NULL;
          sWithClause = sWithClause->next )
    {
        if ( QC_IS_NAME_MATCHED( aTableRef->tableName,
                                 sWithClause->stmtName ) == ID_TRUE )
        {
            IDE_TEST_RAISE( sIsFound == ID_TRUE,
                            ERR_DUPLICATE_QUERY_NAME_RECURSIVE_VIEW );
            
            sIsFound = ID_TRUE;
        }
        else
        {
            /* Nothing to do */
        }
    }
    
    if ( aTableRef->recursiveView != NULL )
    {
        IDE_TEST( validateTopRecursiveView( aStatement,
                                            aSFWGH,
                                            aTableRef,
                                            aIsNullableFlag )
                  != IDE_SUCCESS );
    }
    else
    {
        IDE_TEST( validateBottomRecursiveView( aStatement,
                                               aSFWGH,
                                               aTableRef,
                                               aIsNullableFlag )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NO_COLUMN_ALIAS_RECURSIVE_VIEW )
    {
        IDE_SET(ideSetErrorCode( qpERR_ABORT_QMV_NO_COLUMN_ALIAS_RECURSIVE_VIEW ) );
    }
    IDE_EXCEPTION( ERR_DUPLICATE_QUERY_NAME_RECURSIVE_VIEW )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMV_DUPLICATION_QUERY_NAME_RECURSIVE_VIEW ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmvQuerySet::validateTopRecursiveView( qcStatement * aStatement,
                                              qmsSFWGH    * aSFWGH,
                                              qmsTableRef * aTableRef,
                                              UInt          aIsNullableFlag )
{
/***********************************************************************
 *
 * Description : PROJ-2582 recursive with
 *    ����recrusive view valdiate.
 *
 * Implementation :
 * 
 *     with q1(i1,pid) as (
 *       select id, pid from t1
 *       union all
 *       select q1.id, '/'||q1.pid from q1, t1 where q1.pid = t1.id
 *                                      ^^
 *                                      ���� recursive view
 *     )
 *     select * from q1;
 *                   ^^
 *                   �ֻ��� recursive view
 *
 *    q1�� i1�� union all�� left subquery�� target column type���� �����ϵ�
 *    precision�� right subquery�� target�� ���� Ŀ�� �� �ִ�.
 *
 ***********************************************************************/
    
    qmsParseTree      * sWithParseTree = NULL;
    qmsParseTree      * sViewParseTree = NULL;
    qcmColumn         * sColumnAlias = NULL;
    qcmColumn         * sColumnInfo = NULL;
    qmsTarget         * sWithTarget = NULL;
    qmsTarget         * sViewTarget = NULL;
    qmsTarget         * sTarget = NULL;
    mtcColumn         * sMtcColumn = NULL;
    qtcNode           * sTargetColumn = NULL;
    qtcNode           * sNode = NULL;
    qtcNode           * sPrevNode = NULL;

    IDU_FIT_POINT_FATAL( "qmvQuerySet::validateTopRecursiveView::__FT__" );
    
    IDE_DASSERT( aTableRef->withStmt != NULL );
    
    sWithParseTree = (qmsParseTree *)aTableRef->withStmt->stmt->myPlan->parseTree;
    
    // with���� view�̰� column alias�� �ִ� ��� �÷��̸��� column alias�� �̿��Ѵ�.
    sColumnAlias = aTableRef->withStmt->columns;

    //------------------------------
    // �ֻ��� recursive view
    // �� �������� ����� recursiveView (parsing�� �Ǿ� �ִ�.)
    //------------------------------
        
    // �ٽ� validation�� �����ϱ� ���� �ӽ� tableInfo�� �����Ѵ�.
    IDE_TEST( makeTableInfo( aStatement,
                             sWithParseTree->querySet,
                             sColumnAlias,
                             &aTableRef->tableInfo,
                             QS_EMPTY_OID )
              != IDE_SUCCESS );
    
    // BUG-43768 recursive with ���� üũ ����
    aTableRef->tableInfo->tableOwnerID = QCG_GET_SESSION_USER_ID(aStatement);        

    // BUG-44156 �ӽ� tableInfo�� tableID����
    aTableRef->tableInfo->tableID = aTableRef->withStmt->tableID;
    
    aTableRef->withStmt->tableInfo = aTableRef->tableInfo;

    //------------------------------
    // ���� recursive view�� target column�� �߻��ϴ��� Ȥ��
    // �����ϴ��� �˾ƺ��� tableInfo�� �����Ѵ�.
    //------------------------------
        
    sViewParseTree = (qmsParseTree*) aTableRef->tempRecursiveView->myPlan->parseTree;
        
    sViewParseTree->querySet->lflag &= ~QMV_QUERYSET_RECURSIVE_VIEW_MASK;
    sViewParseTree->querySet->lflag |= QMV_QUERYSET_RECURSIVE_VIEW_TOP;

    IDE_TEST( qmv::validateSelect( aTableRef->tempRecursiveView )
              != IDE_SUCCESS );

    // tableInfo �÷��� precision�� �����Ѵ�.
    for ( sColumnInfo = aTableRef->tableInfo->columns,
              sWithTarget = sWithParseTree->querySet->target,
              sViewTarget = sViewParseTree->querySet->target;
          ( sColumnInfo != NULL ) &&
              ( sWithTarget != NULL ) &&
              ( sViewTarget != NULL );
          sColumnInfo = sColumnInfo->next,
              sWithTarget = sWithTarget->next,
              sViewTarget = sViewTarget->next )
    {
        IDE_TEST( qtc::makeRecursiveTargetInfo( aStatement,
                                                sWithTarget->targetColumn,
                                                sViewTarget->targetColumn,
                                                sColumnInfo )
                  != IDE_SUCCESS );
    }

    // �̹� aTableRef->withStmt->tableInfo�� �޷��ִ�.
        
    //------------------------------
    // ����� tableInfo�� �ٽ� validation�� �����Ѵ�.
    //------------------------------
        
    sViewParseTree = (qmsParseTree*) aTableRef->recursiveView->myPlan->parseTree;
        
    sViewParseTree->querySet->lflag &= ~QMV_QUERYSET_RECURSIVE_VIEW_MASK;
    sViewParseTree->querySet->lflag |= QMV_QUERYSET_RECURSIVE_VIEW_TOP;

    IDE_TEST( qmv::validateSelect( aTableRef->recursiveView )
              != IDE_SUCCESS );
        
    //------------------------------
    // ����� tableInfo�� recursive view�� left�� right�� target�� �����Ѵ�.
    // �ִ� precision���� �߻��ϴ� ��� cast�Լ����� ������ �߻��Ѵ�.
    //------------------------------

    sPrevNode = NULL;
        
    // left target�� ����
    for ( sColumnInfo = aTableRef->tableInfo->columns,
              sTarget = sViewParseTree->querySet->left->SFWGH->target;
          ( sColumnInfo != NULL ) && ( sTarget != NULL );
          sColumnInfo = sColumnInfo->next,
              sTarget = sTarget->next )
    {
        sTargetColumn = sTarget->targetColumn;

        // conversion�� ����.
        IDE_DASSERT( sTargetColumn->node.conversion == NULL );

        sMtcColumn = QTC_STMT_COLUMN( aStatement, sTargetColumn );

        // type,precision�� �ٸ��ٸ� cast�� �ٿ� �����.
        if ( ( sMtcColumn->type.dataTypeId !=
               sColumnInfo->basicInfo->type.dataTypeId ) ||
             ( sMtcColumn->column.size !=
               sColumnInfo->basicInfo->column.size ) )
        {
            IDE_TEST( addCastFuncForNode( aStatement,
                                          sTargetColumn,
                                          sColumnInfo->basicInfo,
                                          & sNode )
                      != IDE_SUCCESS );

            if ( sPrevNode != NULL )
            {
                sPrevNode->node.next = (mtcNode*)sNode;
            }
            else
            {
                // Nothing to do.
            }

            // target ��带 �ٲ۴�.
            sTarget->targetColumn = sNode;

            sPrevNode = sNode;
        }
        else
        {
            sPrevNode = sTargetColumn;
        }
    }
        
    sPrevNode = NULL;
        
    // right target�� ����
    for ( sColumnInfo = aTableRef->tableInfo->columns,
              sTarget = sViewParseTree->querySet->right->SFWGH->target;
          ( sColumnInfo != NULL ) && ( sTarget != NULL );
          sColumnInfo = sColumnInfo->next,
              sTarget = sTarget->next )
    {
        sTargetColumn = sTarget->targetColumn;
            
        // union all�� conversion�� �޾Ƴ��Ҵ�.
        sMtcColumn =
            QTC_STMT_COLUMN( aStatement,
                             (qtcNode*) mtf::convertedNode(
                                 (mtcNode*) sTargetColumn,
                                 &QC_SHARED_TMPLATE(aStatement)->tmplate ) );
        
        // type,precision�� �ٸ��ٸ� cast�� �ٿ� �����.
        if ( ( sMtcColumn->type.dataTypeId !=
               sColumnInfo->basicInfo->type.dataTypeId ) ||
             ( sMtcColumn->column.size !=
               sColumnInfo->basicInfo->column.size ) )
        {
            // target column�� conversion�� �����ϰ�
            // target column�� ���� cast�Լ��� ���δ�.
            sTargetColumn->node.conversion = NULL;
                
            IDE_TEST( addCastFuncForNode( aStatement,
                                          sTargetColumn,
                                          sColumnInfo->basicInfo,
                                          & sNode )
                      != IDE_SUCCESS );

            if ( sPrevNode != NULL )
            {
                sPrevNode->node.next = (mtcNode*)sNode;
            }
            else
            {
                // Nothing to do.
            }

            // target ��带 �ٲ۴�.
            sTarget->targetColumn = sNode;

            sPrevNode = sNode;
        }
        else
        {
            sPrevNode = sTargetColumn;
        }
    }
        
    //------------------------------
    // ����� tableInfo�� inline view tuple�� �����Ѵ�.
    //------------------------------
        
    // BUG-37136
    aTableRef->tableHandle = aTableRef->tableInfo->tableHandle;  // NULL�̴�
    aTableRef->tableType   = aTableRef->tableInfo->tableType;
    aTableRef->tableFlag   = aTableRef->tableInfo->tableFlag;
    aTableRef->tableOID    = aTableRef->tableInfo->tableOID;
        
    if (QC_IS_NULL_NAME(aTableRef->aliasName) == ID_TRUE)
    {
        SET_POSITION( aTableRef->aliasName, aTableRef->tableName );
    }
    else
    {
        // nothing to do 
    }
        
    IDE_TEST( setSameViewRef( aStatement, aSFWGH, aTableRef )
              != IDE_SUCCESS );

    // add tuple set
    IDE_TEST( qtc::nextTable( &(aTableRef->table),
                              aStatement,
                              NULL,
                              ID_TRUE,
                              aIsNullableFlag ) // PR-13597
              != IDE_SUCCESS );

    // make mtcColumn and mtcExecute for inline view tuple
    IDE_TEST( makeTupleForInlineView( aStatement,
                                      aTableRef,
                                      aTableRef->table,
                                      aIsNullableFlag )
              != IDE_SUCCESS );

    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmvQuerySet::validateBottomRecursiveView( qcStatement * aStatement,
                                                 qmsSFWGH    * aSFWGH,
                                                 qmsTableRef * aTableRef,
                                                 UInt          aIsNullableFlag )
{
/***********************************************************************
 *
 * Description : PROJ-2582 recursive with
 *    ���� recrusive view valdiate.
 *    
 * Implementation :
 * 
 *     with q1(i1,pid) as (
 *       select id, pid from t1
 *       union all
 *       select q1.id, '/'||q1.pid from q1, t1 where q1.pid = t1.id
 *                                      ^^
 *                                      ���� recursive view
 *     )
 *     select * from q1;
 *                   ^^
 *                   �ֻ��� recursive view
 *
 *    q1�� i1�� union all�� left subquery�� target column type���� �����ϵ�
 *    precision�� right subquery�� target�� ���� Ŀ�� �� �ִ�.
 *
 ***********************************************************************/

    qmsParseTree      * sWithParseTree = NULL;
    qcmColumn         * sColumnAlias = NULL;

    IDU_FIT_POINT_FATAL( "qmvQuerySet::validateBottomRecursiveView::__FT__" );
    
    IDE_DASSERT( aTableRef->withStmt != NULL );

    sWithParseTree = (qmsParseTree *)
        aTableRef->withStmt->stmt->myPlan->parseTree;
    
    // with���� view�̰� column alias�� �ִ� ��� �÷��̸���
    // column alias�� �̿��Ѵ�.
    sColumnAlias = aTableRef->withStmt->columns;
    
    //------------------------------
    // ���� recursive view
    // withStmt->stmt���� validation�� ���ȴ�.
    //------------------------------

    if ( aTableRef->withStmt->tableInfo == NULL )
    {
        // left subquery�� recursive view ���� �ȵ�
        IDE_TEST_RAISE( aSFWGH == sWithParseTree->querySet->left->SFWGH,
                        ERR_ORDER_RECURSIVE_VIEW );
        
        // union all�� right subquery SFWGH������ ����
        // �̿��� view, subquery������ �ȵ�
        IDE_TEST_RAISE( aSFWGH != sWithParseTree->querySet->right->SFWGH,
                        ERR_ITSELF_DIRECTLY_RECURSIVE_VIEW );
        
        // make qcmTableInfo for IN-LINE VIEW
        IDE_TEST( makeTableInfo( aStatement,
                                 sWithParseTree->querySet->left,
                                 sColumnAlias,
                                 &aTableRef->tableInfo,
                                 QS_EMPTY_OID )
                  != IDE_SUCCESS );

        // BUG-43768 recursive with ���� üũ ����
        aTableRef->tableInfo->tableOwnerID = QCG_GET_SESSION_USER_ID(aStatement);

        // ���� tableInfo�� �޾Ƶд�.
        aTableRef->withStmt->tableInfo = aTableRef->tableInfo;
            
        // with ���� ���� ������ inline �信 ��� ���ʷ���Ʈ�� tableID�� �Ҵ��Ѵ�.
        aTableRef->tableInfo->tableID = aTableRef->withStmt->tableID;
    }
    else
    {
        // right query�� subquery�� query_name�� ���� ����
        // �̹� tableInfo�� �����ؼ� withStmt�� �޾� ���� ���·�
        // �̺κп� ��� ���� �Ǵµ� isTop�� ���� tableInfo��
        // withStmt�� �޷� ���� �ʴ�.
        IDE_TEST_RAISE( aTableRef->withStmt->isTop == ID_TRUE,
                        ERR_DUPLICATE_RECURSIVE_VIEW );

        aTableRef->tableInfo = aTableRef->withStmt->tableInfo;

        // BUG-44156 �ӽ� tableInfo�� tableID����
        aTableRef->tableInfo->tableID = aTableRef->withStmt->tableID;
    }

    // BUG-37136
    aTableRef->tableHandle = aTableRef->tableInfo->tableHandle;  // NULL�̴�
    aTableRef->tableType   = aTableRef->tableInfo->tableType;
    aTableRef->tableFlag   = aTableRef->tableInfo->tableFlag;
    aTableRef->tableOID    = aTableRef->tableInfo->tableOID;

    if (QC_IS_NULL_NAME(aTableRef->aliasName) == ID_TRUE)
    {
        SET_POSITION( aTableRef->aliasName, aTableRef->tableName );
    }
    else
    {
        // nothing to do
    }
        
    IDE_TEST( setSameViewRef( aStatement, aSFWGH, aTableRef )
              != IDE_SUCCESS );

    // add tuple set
    IDE_TEST( qtc::nextTable( &(aTableRef->table),
                              aStatement,
                              NULL,
                              ID_TRUE,
                              aIsNullableFlag ) // PR-13597
              != IDE_SUCCESS );

    // make mtcColumn and mtcExecute for inline view tuple
    IDE_TEST( makeTupleForInlineView( aStatement,
                                      aTableRef,
                                      aTableRef->table,
                                      aIsNullableFlag )
              != IDE_SUCCESS );

    // recursive view reference id ����
    aSFWGH->recursiveViewID = aTableRef->table;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_ORDER_RECURSIVE_VIEW )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMV_ORDER_RECURSIVE_VIEW ) );
    }
    IDE_EXCEPTION( ERR_ITSELF_DIRECTLY_RECURSIVE_VIEW )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMV_ITSELF_DIRECTLY_RECURSIVE_VIEW ) );
    }
    IDE_EXCEPTION( ERR_DUPLICATE_RECURSIVE_VIEW )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMV_DUPLICATE_RECURSIVE_VIEW ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmvQuerySet::setSameViewRef(
    qcStatement * aStatement,
    qmsSFWGH    * aSFWGH,
    qmsTableRef * aTableRef)
{
/***********************************************************************
 *
 * Description : PROJ-1413 Simple View Merging
 *
 *     ���� same view�� ã�� ����� querySet���� from������ ã�ų�
 *     outerQuery(SFWGH)�� ���� �ö󰡸� from������ ã�� ����̾���.
 *     �̷� ����� from���� subquery���� �����ϴ� ������ view����
 *     �˻��� �� ������ (querySet���� view���� �˻��Ѵ�.)
 *
 *     ex) select * from v1 a, v1 b;
 *         select * from v1 where i1 in (select i1 from v1);
 *
 *     created view Ȥ�� inline view�ȿ��� ����� ������ view��
 *     set���� ���� ������ view�� �˻��� �� ������.
 *     (querySet ���� view�� �˻��� �� ����.)
 *
 *     ex) select * from v1 a, (select * from v1) b;
 *         select * from v1 union select * from v1;
 *
 *     �� ������ �ذ��ϱ� ���� outerQuery�� ���󰡴� ����� �ƴ�
 *     tableMap�� �˻��ϴ� ������� �����Ѵ�.
 *
 *     �׷��� �̷� ����� ������ ���� view�� ���ؼ��� ������ view��
 *     �˻��ϰ� ���ʿ��� materialize�� �߻��ϴ� ������ �ִ�.
 *     ������ �� �Լ��� ������ view�� ã�� ������, materialize��
 *     �����ϴ� ���� �ƴϹǷ� �̷� ������ optimizer�� Ǯ��� �Ѵ�.
 *
 *     ex) select * from v1 union all select * from v1;
 *
 *     ������ ���� view�� predicate�� pushdown�Ǵ� ���� materialize��
 *     �������� �����Ƿ� Ư���� ��츦 �����ϰ�� ���� ���� ������
 *     ���� ������ �������� �����Ѵ�. �̷� ��츦 Ư���ϰ�
 *     ����Ѵٸ� hint�� ����ϴ� ���� �´�.
 *
 *     ex) select /+ push_select_view(v1) / * from v1
 *         union all
 *         select * from v1;
 *
 *    PROJ-2582 recursive with
 *      top query�� ���� same view ó��
 *
 *     ex> with test_cte ( cte_i1 ) as
 *        (    select * from t1
 *             union all
 *             select * from test_cte )
 *        select * from test_cte a, test_cte b;
 *
 * Implementation :
 *
 ***********************************************************************/

    qcTableMap      * sTableMap;
    qmsTableRef     * sTableRef;
    UShort            i;

    IDU_FIT_POINT_FATAL( "qmvQuerySet::setSameViewRef::__FT__" );

    if ( aSFWGH != NULL )
    {
        if ( aSFWGH->hierarchy != NULL )
        {
            // BUG-37237 hierarchy query�� same view�� ó������ �ʴ´�.
            aTableRef->flag &= ~QMS_TABLE_REF_HIER_VIEW_MASK;
            aTableRef->flag |= QMS_TABLE_REF_HIER_VIEW_TRUE;
            
            IDE_CONT( NORMAL_EXIT );
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
    
    sTableMap = QC_SHARED_TMPLATE(aStatement)->tableMap;

    for ( i = 0; i < QC_SHARED_TMPLATE(aStatement)->tmplate.rowCount; i++ )
    {
        if ( sTableMap[i].from != NULL )
        {
            sTableRef = sTableMap[i].from->tableRef;

            // PROJ-2582 recursive with
            if ( ( aTableRef->flag & QMS_TABLE_REF_RECURSIVE_VIEW_MASK )
                 == QMS_TABLE_REF_RECURSIVE_VIEW_TRUE )
            {
                if ( ( sTableRef->recursiveView != NULL ) &&
                     ( sTableRef->tableInfo != NULL ) &&
                     ( sTableRef->tableInfo->tableID == aTableRef->tableInfo->tableID ) )
                {
                    if ( sTableRef->sameViewRef == NULL )
                    {
                        aTableRef->recursiveView = sTableRef->recursiveView;
                        aTableRef->sameViewRef = sTableRef;
                    }
                    else
                    {
                        aTableRef->sameViewRef = sTableRef->sameViewRef;
                    }

                    break;
                }
                else
                {
                    // Nothing to do.
                }                
            }
            else
            {
                /* TASK-7219
                 *   qmv::parseSelect ���� qmvWith::validate �� With Shard Trasnfrom �ܰ踦
                 *   ���ķ� �����Ͽ���, �� ������ With View �� Transform ���� �Ͱ� ���� ����
                 *   �����ϰ� �ȴ�.
                 *
                 *   �̶��� Shard Transform �� View�� Transform ������ View�� Same View Ref
                 *   ó���ϸ� �ȵȴ�. qmvShardTransform::processTransformForFrom ����
                 *   Shard Transform �� View ���θ� sTableRef->flag �� �����.
                 *
                 *   ���⼭ ���θ� Ȯ���Ͽ� ������ View �� Same View Ref ó���ϵ��� �����Ѵ�.
                 */
                if ( ( sTableRef->flag & QMS_TABLE_REF_SHARD_TRANSFROM_MASK )
                     != ( aTableRef->flag & QMS_TABLE_REF_SHARD_TRANSFROM_MASK ) )
                {
                    continue;
                }
                else
                {
                    /* Nothing to do */
                }

                if ( ( sTableRef->view != NULL ) &&
                     ( sTableRef->tableInfo != NULL ) &&
                     ( ( sTableRef->tableInfo->tableID == aTableRef->tableInfo->tableID ) &&
                       ( aTableRef->tableInfo->tableID > 0 ) && // with������ inline view�� ���
                       ( ( sTableRef->flag & QMS_TABLE_REF_HIER_VIEW_MASK )
                         != QMS_TABLE_REF_HIER_VIEW_TRUE ) ) ) // hierarchy�� ���� view�� �ƴ� ���
                {
                    if ( sTableRef->sameViewRef == NULL )
                    {
                        aTableRef->sameViewRef = sTableRef;
                    }
                    else
                    {
                        aTableRef->sameViewRef = sTableRef->sameViewRef;
                    }

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
    }

    IDE_EXCEPTION_CONT( NORMAL_EXIT );

    return IDE_SUCCESS;
}

IDE_RC qmvQuerySet::validateQmsFromWithOnCond(
    qmsQuerySet * aQuerySet,
    qmsSFWGH    * aSFWGH,
    qmsFrom     * aFrom,
    qcStatement * aStatement,
    UInt          aIsNullableFlag) // PR-13597
{
/***********************************************************************
 *
 * Description :
 *
 *
 * Implementation :
 *
 ***********************************************************************/

    qcuSqlSourceInfo      sqlInfo;

    IDU_FIT_POINT_FATAL( "qmvQuerySet::setSameViewRef::__FT__" );

    if (aFrom->joinType == QMS_NO_JOIN) // leaf node
    {
        IDE_TEST(validateQmsTableRef(aStatement,
                                     aSFWGH,
                                     aFrom->tableRef,
                                     aSFWGH->lflag,
                                     aIsNullableFlag)  // PR-13597
                 != IDE_SUCCESS);

        // Table Map ����
        QC_SHARED_TMPLATE(aStatement)->tableMap[aFrom->tableRef->table].from = aFrom;

        // FROM ���� ���� dependencies ����
        qtc::dependencyClear( & aFrom->depInfo );
        qtc::dependencySet( aFrom->tableRef->table, & aFrom->depInfo );

        // PROJ-1718 Semi/anti join�� ���õ� dependency �ʱ�ȭ
        qtc::dependencyClear( & aFrom->semiAntiJoinDepInfo );

        IDE_TEST( qmsPreservedTable::addTable( aStatement,
                                               aSFWGH,
                                               aFrom->tableRef )
                  != IDE_SUCCESS );
            
        /* PROJ-2462 Result Cache */
        if ( aQuerySet != NULL )
        {
            if ( ( aFrom->tableRef->tableInfo->tableType == QCM_FIXED_TABLE) ||
                 ( aFrom->tableRef->tableInfo->tableType == QCM_DUMP_TABLE) ||
                 ( aFrom->tableRef->tableInfo->tableType == QCM_PERFORMANCE_VIEW) ||
                 ( aFrom->tableRef->remoteTable != NULL ) || /* PROJ-1832 */
                 ( aFrom->tableRef->mShardObjInfo != NULL ) ||
                 ( aFrom->tableRef->tableInfo->temporaryInfo.type != QCM_TEMPORARY_ON_COMMIT_NONE ) )
            {
                aQuerySet->lflag &= ~QMV_QUERYSET_RESULT_CACHE_INVALID_MASK;
                aQuerySet->lflag |= QMV_QUERYSET_RESULT_CACHE_INVALID_TRUE;
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
    else // non-leaf node
    {
        // PR-13597 �� ���ؼ� outer column�� ��쿡 �ش� ���̺��� NULLABLE�� ����
        if( aFrom->joinType == QMS_INNER_JOIN )
        {
            IDE_TEST(validateQmsFromWithOnCond(aQuerySet,
                                               aSFWGH,
                                               aFrom->left,
                                               aStatement,
                                               MTC_COLUMN_NOTNULL_TRUE)
                     != IDE_SUCCESS);

            IDE_TEST(validateQmsFromWithOnCond(aQuerySet,
                                               aSFWGH,
                                               aFrom->right,
                                               aStatement,
                                               MTC_COLUMN_NOTNULL_TRUE)
                     != IDE_SUCCESS);
        }
        else if( aFrom->joinType == QMS_FULL_OUTER_JOIN )
        {
            IDE_TEST(validateQmsFromWithOnCond(aQuerySet,
                                               aSFWGH,
                                               aFrom->left,
                                               aStatement,
                                               MTC_COLUMN_NOTNULL_FALSE)
                     != IDE_SUCCESS);

            IDE_TEST(validateQmsFromWithOnCond(aQuerySet,
                                               aSFWGH,
                                               aFrom->right,
                                               aStatement,
                                               MTC_COLUMN_NOTNULL_FALSE)
                     != IDE_SUCCESS);
        }
        else if( aFrom->joinType == QMS_LEFT_OUTER_JOIN )
        {
            IDE_TEST(validateQmsFromWithOnCond(aQuerySet,
                                               aSFWGH,
                                               aFrom->left,
                                               aStatement,
                                               MTC_COLUMN_NOTNULL_TRUE)
                     != IDE_SUCCESS);

            IDE_TEST(validateQmsFromWithOnCond(aQuerySet,
                                               aSFWGH,
                                               aFrom->right,
                                               aStatement,
                                               MTC_COLUMN_NOTNULL_FALSE)
                     != IDE_SUCCESS);
        }
        else if( aFrom->joinType == QMS_RIGHT_OUTER_JOIN )
        {
            IDE_TEST(validateQmsFromWithOnCond(aQuerySet,
                                               aSFWGH,
                                               aFrom->left,
                                               aStatement,
                                               MTC_COLUMN_NOTNULL_FALSE)
                     != IDE_SUCCESS);

            IDE_TEST(validateQmsFromWithOnCond(aQuerySet,
                                               aSFWGH,
                                               aFrom->right,
                                               aStatement,
                                               MTC_COLUMN_NOTNULL_TRUE)
                     != IDE_SUCCESS);
        }
        else
        {
            // �̷� ���� ���� �� ����.
        }

        /* PROJ-2197 PSM Renewal */
        aFrom->onCondition->lflag |= QTC_NODE_COLUMN_CONVERT_TRUE;
        IDE_TEST(qtc::estimate( aFrom->onCondition,
                                QC_SHARED_TMPLATE(aStatement),
                                aStatement,
                                aQuerySet,
                                aSFWGH,
                                aFrom )
                 != IDE_SUCCESS);

        if ( ( aFrom->onCondition->node.lflag &
               ( MTC_NODE_LOGICAL_CONDITION_MASK | MTC_NODE_COMPARISON_MASK ) )
             == ( MTC_NODE_LOGICAL_CONDITION_FALSE | MTC_NODE_COMPARISON_FALSE ) )
        {
            sqlInfo.setSourceInfo( aStatement,
                                   & aFrom->onCondition->position );
            IDE_RAISE( ERR_NOT_PREDICATE );
        }

        if ( ( aFrom->onCondition->node.lflag & MTC_NODE_DML_MASK )
             == MTC_NODE_DML_UNUSABLE )
        {
            sqlInfo.setSourceInfo( aStatement,
                                   & aFrom->onCondition->position );
            IDE_RAISE( ERR_USE_CURSOR_ATTR );
        }

        // BUG-16652
        if ( ( aSFWGH->outerHavingCase == ID_FALSE ) &&
             ( QTC_HAVE_AGGREGATE(aFrom->onCondition) == ID_TRUE ) )
        {
            sqlInfo.setSourceInfo( aStatement,
                                   & aFrom->onCondition->position );
            IDE_RAISE( ERR_NOT_ALLOWED_AGGREGATION );
        }

        // BUG-16652
        if ( ( aFrom->onCondition->lflag & QTC_NODE_SEQUENCE_MASK )
             == QTC_NODE_SEQUENCE_EXIST )
        {
            sqlInfo.setSourceInfo( aStatement,
                                   & aFrom->onCondition->position );
            IDE_RAISE( ERR_USE_SEQUENCE_IN_ON_JOIN_CONDITION );
        }

        /* Plan Property�� ��� */
        qcgPlan::registerPlanProperty( aStatement,
                                       PLAN_PROPERTY_QUERY_REWRITE_ENABLE );

        /* PROJ-1090 Function-based Index */
        if ( QCG_GET_SESSION_QUERY_REWRITE_ENABLE(aStatement) == 1 )
        {
            IDE_TEST( qmsDefaultExpr::applyFunctionBasedIndex(
                          aStatement,
                          aFrom->onCondition,
                          aSFWGH->from,
                          &(aFrom->onCondition) )
                      != IDE_SUCCESS );
        }
        else
        {
            /* Nothing to do */
        }

        // FROM ���� ���� dependencies ����
        //    - Left From�� depedencies ����
        //    - Right From�� dependencies ����
        qtc::dependencyClear( & aFrom->depInfo );

        IDE_TEST( qtc::dependencyOr( & aFrom->depInfo,
                                     & aFrom->left->depInfo,
                                     & aFrom->depInfo )
                  != IDE_SUCCESS );
        IDE_TEST( qtc::dependencyOr( & aFrom->depInfo,
                                     & aFrom->right->depInfo,
                                     & aFrom->depInfo )
                  != IDE_SUCCESS );

        // PROJ-1718 Semi/anti join�� ���õ� dependency �ʱ�ȭ
        qtc::dependencyClear( & aFrom->semiAntiJoinDepInfo );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_NOT_PREDICATE);
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QMV_NOT_PREDICATE,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION(ERR_USE_CURSOR_ATTR);
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QSV_USING_ATTR_IN_INAPPROPRIATE_CLAUSE,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION(ERR_USE_SEQUENCE_IN_ON_JOIN_CONDITION)
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QMV_USE_SEQUENCE_IN_ON_JOIN_CONDITION,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION(ERR_NOT_ALLOWED_AGGREGATION)
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QCV_NOT_ALLOWED_AGGREGATION,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmvQuerySet::makeTableInfo(
    qcStatement     * aStatement,
    qmsQuerySet     * aQuerySet,
    qcmColumn       * aColumnAlias,
    qcmTableInfo   ** aTableInfo,
    smOID             aObjectID)
{
    qcmTableInfo    * sTableInfo;
    qmsTarget       * sTarget;
    qtcNode         * sNode;
    mtcColumn       * sTargetColumn;
    qcmColumn       * sPrevColumn = NULL;
    qcmColumn       * sCurrColumn;
    qcuSqlSourceInfo  sqlInfo;
    UInt              i;
    // BUG-37981
    qcTemplate      * sTemplate;
    mtcTuple        * sMtcTuple;
    qsxPkgInfo      * sPkgInfo    = NULL;
    qcNamePosition    sPosition;
    qmsNamePosition   sNamePosition;
    qcmColumn       * sAlias = aColumnAlias;

    IDU_FIT_POINT_FATAL( "qmvQuerySet::makeTableInfo::__FT__" );

    IDE_TEST(STRUCT_CRALLOC(QC_QMP_MEM(aStatement), qcmTableInfo, &sTableInfo)
             != IDE_SUCCESS);

    // PROJ-1502 PARTITIONED DISK TABLE
    sTableInfo->tablePartitionType = QCM_NONE_PARTITIONED_TABLE;
    sTableInfo->tableType = QCM_USER_TABLE;

    // PROJ-1407 Temporary Table
    sTableInfo->temporaryInfo.type = QCM_TEMPORARY_ON_COMMIT_NONE;

    /* PROJ-2359 Table/Partition Access Option */
    sTableInfo->accessOption = QCM_ACCESS_OPTION_READ_WRITE;

    /* TASK-7307 DML Data Consistency in Shard */
    sTableInfo->mIsUsable = ID_TRUE;
    sTableInfo->mShardFlag = QCM_TABLE_SHARD_FLAG_TABLE_NONE;

    /* ��Ÿ ���� */
    sTableInfo->tableOID        = SMI_NULL_OID;
    sTableInfo->tableFlag       = 0;
    sTableInfo->isDictionary    = ID_FALSE;
    sTableInfo->viewReadOnly    = QCM_VIEW_NON_READ_ONLY;

    for (sTarget = aQuerySet->target;
         sTarget != NULL;
         sTarget = sTarget->next)
    {
        sTableInfo->columnCount++;
    }

    // To Fix PR-8331
    IDE_TEST(STRUCT_CRALLOC_WITH_SIZE(QC_QMP_MEM(aStatement),
                                      qcmColumn,
                                      ID_SIZEOF(qcmColumn) * sTableInfo->columnCount,
                                      (void**) & sCurrColumn)
             != IDE_SUCCESS);

    for (sTarget = aQuerySet->target, i = 0;
         sTarget != NULL;
         sTarget = sTarget->next, i++)
    {
        IDE_TEST(STRUCT_ALLOC(QC_QMP_MEM(aStatement),
                              mtcColumn,
                              (void**) & (sCurrColumn[i].basicInfo))
                 != IDE_SUCCESS);

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
            IDE_RAISE( ERR_MAX_COLUMN_NAME_LENGTH );
        }

        if ( (sTarget->targetColumn->node.lflag & MTC_NODE_OPERATOR_MASK) ==
             MTC_NODE_OPERATOR_SUBQUERY )
        {
            // BUG-16928
            IDE_TEST( getSubqueryTarget(
                          aStatement,
                          (qtcNode *)sTarget->targetColumn->node.arguments,
                          & sNode )
                      != IDE_SUCCESS );
        }
        else
        {
            sNode = sTarget->targetColumn;
        }

        /* BUG-37981
           cursor for loop���� ���Ǵ� cursor�� ���,
           package spec�� �ִ� cursor �� ���� �ֱ� ������
           template�� �ٸ� ���� �ִ�. */
        /* BUG-44716
         * Package �����߿��� �ڽ��� OID�� getPkgInfo�� ȣ���ϸ� �ȵȴ�. */
        if( (aObjectID == QS_EMPTY_OID) ||
            ((aStatement->spvEnv->createPkg != NULL) &&
             (aObjectID == aStatement->spvEnv->createPkg->pkgOID)) ) 
        {
            sTargetColumn = QTC_STMT_COLUMN(aStatement, sNode);
        }
        else
        {
            IDE_TEST( qsxPkg::getPkgInfo( aObjectID,
                                          &sPkgInfo )
                      != IDE_SUCCESS );

            sTemplate = sPkgInfo->tmplate;

            sMtcTuple = ( sTemplate->tmplate.rows ) + (sNode->node.table);

            sTargetColumn = QTC_TUPLE_COLUMN( sMtcTuple, sNode );
        }

        // BUG-35172
        // Inline View���� BINARY Ÿ���� �÷��� �����ϸ� �ȵȴ�.
        IDE_TEST_RAISE( sTargetColumn->module->id == MTD_BINARY_ID, ERR_INVALID_TYPE );

        mtc::copyColumn(sCurrColumn[i].basicInfo, sTargetColumn);

        // PROJ-1473
        sCurrColumn[i].basicInfo->flag &= ~MTC_COLUMN_USE_COLUMN_MASK;
        sCurrColumn[i].basicInfo->flag &= ~MTC_COLUMN_USE_TARGET_MASK;
        sCurrColumn[i].basicInfo->flag &= ~MTC_COLUMN_USE_NOT_TARGET_MASK;
        sCurrColumn[i].basicInfo->flag &= ~MTC_COLUMN_VIEW_COLUMN_PUSH_MASK;

        // PROJ-2415 Grouping Sets Clause
        // Flag ����
        if ( ( sTarget->targetColumn->lflag & QTC_NODE_GBGS_ORDER_BY_NODE_MASK )
             == QTC_NODE_GBGS_ORDER_BY_NODE_TRUE )
        {
            sCurrColumn[i].flag &= ~QCM_COLUMN_GBGS_ORDER_BY_NODE_MASK;
            sCurrColumn[i].flag |= QCM_COLUMN_GBGS_ORDER_BY_NODE_TRUE;
        }
        else
        {
            /* Nothing to do */
        }

        // PROJ-2582 recursive with
        // with���� view�̰� column alias�� �ִ� ��� �÷��̸��� column alias�� �̿��Ѵ�.
        if ( sAlias != NULL )
        {
            QC_STR_COPY( sCurrColumn[i].name, sAlias->namePos );
            
            SET_EMPTY_POSITION(sCurrColumn[i].namePos);

            // next
            sAlias = sAlias->next;
        }
        else
        {
            /* TASK-7219
             *  Inline View �� Shard View Transform ���� ����Ǵ� ��쿡
             *   1. qmvQuerySet::makeTableInfo
             *   2. qmvQuerySet::makeTargetListForTableRef
             *  �� �Լ��� ������ ��ø�ؼ� ȣ���ϰ� �ǰ�,
             *  2 ���� �޸� �Ҵ����� ������ Target Name ������ 1������ ���̿��Ѵ�.
             *
             *  2 ���� aliasColumnName �� �����ϴ� �����ʴ� �Ҵ��ϹǷ�,
             *  1 ���� ���̿�� ������ aliasColumnName.size �� �����ϴ� ������ �������� �ʴ�.
             *
             *  2 ���� aliasColumnName.size �� �����ϴ� ��ȵ� �ְ�����,
             *  ������ �Ǵ� ����� �۾��� �����Ƿ�,
             *  Shard ���� ���۵��ϴ� �κи� 1 ���� �����Ѵ�. 
             */
            if ( ( sTarget->aliasColumnName.size == QC_POS_EMPTY_SIZE ) ||
                 ( sTarget->aliasColumnName.size == 0 ) )
            {
                // ������ ���� ��� union all�� ���� ������ internal column�� null name�� ������.
                // select * from (select 'A' from dual union all select 'A' from dual);
                if ( QC_IS_NULL_NAME( sTarget->targetColumn->position ) == ID_TRUE )
                {
                    if ( sTarget->displayName.size > 0 )
                    {
                        sPosition.stmtText = sTarget->displayName.name;
                        sPosition.offset   = 0;
                        sPosition.size     = sTarget->displayName.size;
                    }
                    else
                    {
                        // �ƹ��͵� �� �� ����.
                        SET_EMPTY_POSITION( sPosition );
                    }
                }
                else
                {
                    // natc���� �����ϴ� ��쵵 ���� display name���� ����Ѵ�.
                    if ( ( QCU_COMPAT_DISPLAY_NAME == 1 ) ||
                         ( QCU_DISPLAY_PLAN_FOR_NATC == 1 ) )
                    {
                        // BUG-38946 ���� display name�� �����ϰ� �����ؾ��Ѵ�.
                        getDisplayName( sTarget->targetColumn, &sPosition );
                    }
                    else
                    {
                        IDE_TEST(STRUCT_ALLOC_WITH_SIZE(QC_QMP_MEM(aStatement),
                                                        SChar,
                                                        sTarget->targetColumn->position.size + 1,
                                                        &(sNamePosition.name))
                                 != IDE_SUCCESS);

                        // O��� �����ϰ� �����Ѵ�.
                        copyDisplayName( sTarget->targetColumn,
                                         &sNamePosition );
                    
                        sPosition.stmtText = sNamePosition.name;
                        sPosition.offset   = 0;
                        sPosition.size     = sNamePosition.size;
                    }
                }
            
                sCurrColumn[i].name[0] = '\0';
                sCurrColumn[i].namePos = sPosition;
            }
            else
            {
                idlOS::memcpy(sCurrColumn[i].name,
                              sTarget->aliasColumnName.name,
                              sTarget->aliasColumnName.size);
                sCurrColumn[i].name[sTarget->aliasColumnName.size] = '\0';

                SET_EMPTY_POSITION(sCurrColumn[i].namePos);
            }
        }

        sCurrColumn[i].defaultValue = NULL;
        sCurrColumn[i].next         = NULL;

        // connect
        if (sPrevColumn == NULL)
        {
            sTableInfo->columns = &sCurrColumn[i];
            sPrevColumn         = &sCurrColumn[i];
        }
        else
        {
            sPrevColumn->next = &sCurrColumn[i];
            sPrevColumn       = &sCurrColumn[i];
        }
    }

    *aTableInfo = sTableInfo;

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_MAX_COLUMN_NAME_LENGTH);
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QCP_MAX_NAME_LENGTH_OVERFLOW,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION(ERR_INVALID_TYPE);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDB_CREATE_DISABLE_DATA_TYPE));
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmvQuerySet::makeTupleForInlineView(
    qcStatement * aStatement,
    qmsTableRef * aTableRef,
    UShort        aTupleID,
    UInt          aIsNullableFlag)
{
    qcmTableInfo    * sTableInfo;
    qcmColumn       * sCurrColumn;
    UShort            sColumn;
    qtcNode         * sNode[2];
    qcNamePosition    sColumnPos;
    qtcNode         * sCurrNode;

    mtcTemplate     * sMtcTemplate;

    IDU_FIT_POINT_FATAL( "qmvQuerySet::makeTupleForInlineView::__FT__" );

    sTableInfo = aTableRef->tableInfo;
    sMtcTemplate = & QC_SHARED_TMPLATE(aStatement)->tmplate;

    sMtcTemplate->rows[aTupleID].lflag
        = qtc::templateRowFlags[MTC_TUPLE_TYPE_INTERMEDIATE];
    sMtcTemplate->rows[aTupleID].columnCount   = sTableInfo->columnCount;
    sMtcTemplate->rows[aTupleID].columnMaximum = sTableInfo->columnCount;

    // View�� ���� ������ Tuple���� ǥ����.
    sMtcTemplate->rows[aTupleID].lflag &= ~MTC_TUPLE_VIEW_MASK;
    sMtcTemplate->rows[aTupleID].lflag |= MTC_TUPLE_VIEW_TRUE;

    /* BUG-44382 clone tuple ���ɰ��� */
    // ���簡 �ʿ���
    qtc::setTupleColumnFlag( &(sMtcTemplate->rows[aTupleID]),
                             ID_TRUE,
                             ID_FALSE );

    // memory alloc for columns and execute
    IDE_TEST(QC_QMP_MEM(aStatement)->alloc(
                 ID_SIZEOF(mtcColumn) * sMtcTemplate->rows[aTupleID].columnCount,
                 (void**)&(sMtcTemplate->rows[aTupleID].columns))
             != IDE_SUCCESS);

    // PROJ-1473
    IDE_TEST( qtc::allocAndInitColumnLocateInTuple(
                  QC_QMP_MEM(aStatement),
                  sMtcTemplate,
                  aTupleID )
              != IDE_SUCCESS );

    IDE_TEST(QC_QMP_MEM(aStatement)->alloc(
                 ID_SIZEOF(mtcExecute) * sMtcTemplate->rows[aTupleID].columnCount,
                 (void**)&(sMtcTemplate->rows[aTupleID].execute))
             != IDE_SUCCESS);

    // make dummy qtcNode
    SET_EMPTY_POSITION( sColumnPos );
    IDE_TEST(qtc::makeColumn(aStatement, sNode, NULL, NULL, &sColumnPos, NULL )
             != IDE_SUCCESS);
    sNode[0]->node.table = aTupleID;
    sCurrNode = sNode[0];

    // set mtcColumn, mtcExecute
    for( sColumn = 0, sCurrColumn = sTableInfo->columns;
         sColumn < sMtcTemplate->rows[aTupleID].columnCount;
         sColumn++, sCurrColumn = sCurrColumn->next)
    {
        sCurrNode->node.column = sColumn;

        // PROJ-1362
        // lob�� select�� lob-locator�� ��ȯ�ǹǷ� ���Ƿ� �ٲ��ش�.
        if (sCurrColumn->basicInfo->module->id == MTD_BLOB_ID)
        {
            IDE_TEST(mtc::initializeColumn(
                         &(sMtcTemplate->rows[aTupleID].columns[sColumn]),
                         MTD_BLOB_LOCATOR_ID,
                         0,
                         0,
                         0)
                     != IDE_SUCCESS);
        }
        else if (sCurrColumn->basicInfo->module->id == MTD_CLOB_ID)
        {
            IDE_TEST(mtc::initializeColumn(
                         &(sMtcTemplate->rows[aTupleID].columns[sColumn]),
                         MTD_CLOB_LOCATOR_ID,
                         0,
                         0,
                         0)
                     != IDE_SUCCESS);
        }
        else
        {
            // copy size, type, module
            mtc::copyColumn( &(sMtcTemplate->rows[aTupleID].columns[sColumn]),
                             sCurrColumn->basicInfo );
        }

        // fix BUG-13597
        if( aIsNullableFlag == MTC_COLUMN_NOTNULL_FALSE )
        {
            sMtcTemplate->rows[aTupleID].columns[sColumn].flag &=
                ~(MTC_COLUMN_NOTNULL_MASK);
            sMtcTemplate->rows[aTupleID].columns[sColumn].flag |=
                (MTC_COLUMN_NOTNULL_FALSE);
        }
        else
        {
            // Nothing To Do
        }

        IDE_TEST(qtc::estimateNodeWithoutArgument(
                     aStatement, sCurrNode )
                 != IDE_SUCCESS );
    }

    // set offset
    qtc::resetTupleOffset( sMtcTemplate, aTupleID );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmvQuerySet::estimateTargetCount(
    qcStatement * aStatement,
    SInt        * aTargetCount)
{
    qmsParseTree    * sParseTree = (qmsParseTree *) aStatement->myPlan->parseTree;
    qmsQuerySet     * sQuerySet;
    qmsSFWGH        * sSFWGH;
    qmsFrom         * sFrom;
    qmsTarget       * sTarget;

    SInt              sTargetCount = 0;
    UInt              sUserID      = 0;

    IDU_FIT_POINT_FATAL( "qmvQuerySet::estimateTargetCount::__FT__" );

    // initialize
    *aTargetCount = 0;

    // get Left Leaf SFWGH
    sQuerySet = sParseTree->querySet;
    while (sQuerySet->setOp != QMS_NONE)
    {
        sQuerySet = sQuerySet->left;
    }
    sSFWGH = sQuerySet->SFWGH;

    // estimate
    if (sSFWGH->target == NULL) // SELECT * FROM ...
    {
        for (sFrom = sSFWGH->from; sFrom != NULL; sFrom = sFrom->next)
        {
            IDE_TEST(estimateOneFromTreeTargetCount(
                         aStatement, sFrom, &sTargetCount)
                     != IDE_SUCCESS);

            (*aTargetCount) = (*aTargetCount) + sTargetCount;
        }
    }
    else
    {
        for (sTarget = sSFWGH->target;
             sTarget != NULL;
             sTarget = sTarget->next)
        {
            if ( ( sTarget->flag & QMS_TARGET_ASTERISK_MASK )
                 == QMS_TARGET_ASTERISK_TRUE )
            { // in case of t1.*, u1.t1.*
                // get userID
                if (QC_IS_NULL_NAME(sTarget->targetColumn->userName) == ID_TRUE)
                {
                    sUserID = QC_EMPTY_USER_ID;
                }
                else
                {
                    IDE_TEST(qcmUser::getUserID(
                                 aStatement,
                                 sTarget->targetColumn->userName,
                                 &(sUserID))
                             != IDE_SUCCESS);
                }

                IDE_TEST(estimateOneTableTargetCount(
                             aStatement, sSFWGH->from,
                             sUserID, sTarget->targetColumn->tableName,
                             &sTargetCount)
                         != IDE_SUCCESS);

                (*aTargetCount) = (*aTargetCount) + sTargetCount;
            }
            else // expression
            {
                (*aTargetCount)++;
            }
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmvQuerySet::estimateOneFromTreeTargetCount(
    qcStatement       * aStatement,
    qmsFrom           * aFrom,
    SInt              * aTargetCount)
{
    SInt        sLeftTargetCount  = 0;
    SInt        sRightTargetCount = 0;

    IDU_FIT_POINT_FATAL( "qmvQuerySet::estimateOneFromTreeTargetCount::__FT__" );

    if (aFrom->joinType != QMS_NO_JOIN)
    {
        IDE_TEST(estimateOneFromTreeTargetCount(
                     aStatement, aFrom->left, &sLeftTargetCount)
                 != IDE_SUCCESS);

        IDE_TEST(estimateOneFromTreeTargetCount(
                     aStatement, aFrom->right, &sRightTargetCount)
                 != IDE_SUCCESS);

        *aTargetCount = sLeftTargetCount + sRightTargetCount;
    }
    else
    {
        IDE_TEST(getTargetCountFromTableRef(
                     aStatement, aFrom->tableRef, aTargetCount)
                 != IDE_SUCCESS);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmvQuerySet::estimateOneTableTargetCount(
    qcStatement       * aStatement,
    qmsFrom           * aFrom,
    UInt                aUserID,
    qcNamePosition      aTableName,
    SInt              * aTargetCount)
{
    qmsFrom         * sFrom;
    qmsTableRef     * sTableRef = NULL;

    IDU_FIT_POINT_FATAL( "qmvQuerySet::estimateOneTableTargetCount::__FT__" );

    *aTargetCount = 0;

    for (sFrom = aFrom;
         sFrom != NULL && *aTargetCount == 0;
         sFrom = sFrom->next)
    {
        if (sFrom->joinType != QMS_NO_JOIN)
        {
            IDE_TEST(estimateOneTableTargetCount(
                         aStatement, sFrom->left,
                         aUserID, aTableName,
                         aTargetCount)
                     != IDE_SUCCESS);

            if (*aTargetCount == 0)
            {
                IDE_TEST(estimateOneTableTargetCount(
                             aStatement, sFrom->right,
                             aUserID, aTableName,
                             aTargetCount)
                         != IDE_SUCCESS);
            }
        }
        else
        {
            sTableRef = aFrom->tableRef;

            // compare table name
            if ( ( sTableRef->userID == aUserID ||
                   aUserID == QC_EMPTY_USER_ID ) &&
                 sTableRef->aliasName.size > 0 )
            {
                if ( QC_IS_NAME_MATCHED( sTableRef->aliasName, aTableName ) )
                {
                    IDE_TEST(getTargetCountFromTableRef(
                                 aStatement, sTableRef, aTargetCount)
                             != IDE_SUCCESS);
                }
            }
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmvQuerySet::getTargetCountFromTableRef(
    qcStatement * aStatement,
    qmsTableRef * aTableRef,
    SInt        * aTargetCount)
{
    qcuSqlSourceInfo  sqlInfo;
    idBool            sExist = ID_FALSE;
    idBool            sIsFixedTable = ID_FALSE;
    void             *sTableHandle  = NULL;
    qcmSynonymInfo    sSynonymInfo;
    UInt              sTableType;

    IDU_FIT_POINT_FATAL( "qmvQuerySet::getTargetCountFromTableRef::__FT__" );

    //�Ϲ� ���̺��� ���
    if (aTableRef->view == NULL)
    {
        IDE_TEST(
            qcmSynonym::resolveTableViewQueue(
                aStatement,
                aTableRef->userName,
                aTableRef->tableName,
                &(aTableRef->tableInfo),
                &(aTableRef->userID),
                &(aTableRef->tableSCN),
                &sExist,
                &sSynonymInfo,
                &sTableHandle)
            != IDE_SUCCESS);

        if( sExist == ID_TRUE )
        {
            sTableType = smiGetTableFlag( sTableHandle ) & SMI_TABLE_TYPE_MASK;
            
            if( sTableType == SMI_TABLE_FIXED )
            {
                sIsFixedTable = ID_TRUE;
            }
            else
            {
                sIsFixedTable = ID_FALSE;
            }
        }
        else
        {
            // Nothing to do.
        }
            
        if( sExist == ID_FALSE )
        {
            if ( QC_IS_NULL_NAME(aTableRef->tableName) == ID_FALSE)
            {
                sqlInfo.setSourceInfo( aStatement,
                                       & aTableRef->tableName );
                IDE_RAISE( ERR_NOT_EXIST_TABLE );
            }
            else
            {
                // Nothing to do.
            }
        }
        else
        {
            if( sIsFixedTable == ID_FALSE )
            {
                // BUG-34492
                // validation lock�̸� ����ϴ�.
                IDE_TEST( qcm::lockTableForDMLValidation(
                              aStatement,
                              sTableHandle,
                              aTableRef->tableSCN )
                          != IDE_SUCCESS );
                
                // environment�� ���
                IDE_TEST( qcgPlan::registerPlanTable(
                              aStatement,
                              sTableHandle,
                              aTableRef->tableSCN,
                              aTableRef->tableInfo->tableOwnerID, /* BUG-45893 */
                              aTableRef->tableInfo->name )        /* BUG-45893 */
                          != IDE_SUCCESS );
                
                // environment�� ���
                IDE_TEST( qcgPlan::registerPlanSynonym(
                              aStatement,
                              & sSynonymInfo,
                              aTableRef->userName,
                              aTableRef->tableName,
                              sTableHandle,
                              NULL )
                          != IDE_SUCCESS );
            }
            else
            {
                // Nothing to do.
            }

            // get column count
            *aTargetCount = aTableRef->tableInfo->columnCount;
        }
    }
    //View�� ���
    else
    {
        IDE_TEST(estimateTargetCount( aTableRef->view, aTargetCount)
                 != IDE_SUCCESS);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_NOT_EXIST_TABLE);
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QCV_NOT_EXISTS_TABLE,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmvQuerySet::validatePushPredView( qcStatement  * aStatement,
                                          qmsTableRef  * aTableRef,
                                          idBool       * aIsValid )
{
/***********************************************************************
 *
 * Description : join predicate�� ���� �� �ִ���  VIEW ���� �˻�
 *
 * Implementation :
 *
 *    PROJ-1495
 *
 *    ���� 1. VIEW�� LIMIT���� ���� shard table �� �ƴϾ�� ��.
 *    ���� 2. UNION ALL�θ� �����Ǿ�� ��.
 *    ���� 3. VIEW�� �����ϴ� �� select������
 *            (1) target column�� ��� ���� �÷��̾�� �Ѵ�.
 *            (1) FROM���� ���� table�� base table�̾�� �Ѵ�.
 *            (2) WHERE���� �� �� ������,
 *                WHERE���� subquery�� �� �� ����.
 *            (3) GROUP BY              (X)
 *            (4) AGGREGATION           (X)
 *            (5) DISTINCT              (X)
 *            (6) LIMIT                 (X)
 *            (7) START WITH/CONNECT BY (X)
 *
 **********************************************************************/

    qmsParseTree   * sViewParseTree;

    IDU_FIT_POINT_FATAL( "qmvQuerySet::validatePushPredView::__FT__" );

    //---------------------------------------------------
    // ���ռ� �˻�
    //---------------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aTableRef != NULL );

    //---------------------------------------------------
    // �⺻ �ʱ�ȭ
    //---------------------------------------------------

    sViewParseTree = (qmsParseTree *)aTableRef->view->myPlan->parseTree;

    //---------------------------------------------------
    // ���� �˻� : LIMIT���� ����� ��.
    //---------------------------------------------------

    if ( ( sViewParseTree->limit == NULL ) &&
         ( ( sViewParseTree->common.stmtShard == QC_STMT_SHARD_NONE ) ||
           ( sViewParseTree->common.stmtShard == QC_STMT_SHARD_META ) ) ) // PROJ-2638
    {
        //---------------------------------------------------
        // ���� �˻� : UNION ALL
        //---------------------------------------------------

        IDE_TEST( validateUnionAllQuerySet( aStatement,
                                            sViewParseTree->querySet,
                                            aIsValid )
                  != IDE_SUCCESS );
    }
    else
    {
        *aIsValid = ID_FALSE;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmvQuerySet::validateUnionAllQuerySet( qcStatement  * aStatement,
                                              qmsQuerySet  * aQuerySet,
                                              idBool       * aIsValid )
{
/***********************************************************************
 *
 * Description : VIEW�� �ϳ��� select�� �Ǵ� UNION ALL�θ� �����Ǿ����� ����
 *               querySet ��������� �����ϴ��� �˻�
 *
 * Implementation :
 *
 *    PROJ-1495
 *
 **********************************************************************/

    IDU_FIT_POINT_FATAL( "qmvQuerySet::validateUnionAllQuerySet::__FT__" );

    //---------------------------------------------------
    // ���ռ� �˻�
    //---------------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aQuerySet != NULL );

    //---------------------------------------------------
    // �⺻ �ʱ�ȭ
    //---------------------------------------------------

    //---------------------------------------------------
    // ���� �˻� : UNION ALL
    //---------------------------------------------------

    if( *aIsValid == ID_TRUE )
    {
        if( aQuerySet->setOp == QMS_NONE )
        {
            //-------------------------------------------
            // querySet �˻�
            //-------------------------------------------

            IDE_TEST(
                validatePushPredHintQuerySet( aStatement,
                                              aQuerySet,
                                              aIsValid )
                != IDE_SUCCESS );
        }
        else
        {
            if( aQuerySet->left != NULL )
            {
                if( aQuerySet->setOp == QMS_UNION_ALL )
                {
                    // left validate
                    IDE_TEST( validateUnionAllQuerySet( aStatement,
                                                        aQuerySet->left,
                                                        aIsValid )
                              != IDE_SUCCESS );
                }
                else
                {
                    *aIsValid = ID_FALSE;
                }
            }
            else
            {
                // Nothing To Do
            }

            if( aQuerySet->right != NULL )
            {
                if( aQuerySet->setOp == QMS_UNION_ALL )
                {
                    // left validate
                    IDE_TEST( validateUnionAllQuerySet( aStatement,
                                                        aQuerySet->right,
                                                        aIsValid )
                              != IDE_SUCCESS );
                }
                else
                {
                    *aIsValid = ID_FALSE;
                }
            }
            else
            {
                // Nothing To Do
            }
        }
    }
    else
    {
        // Nothing To Do
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC qmvQuerySet::validatePushPredHintQuerySet( qcStatement  * aStatement,
                                                  qmsQuerySet  * aQuerySet,
                                                  idBool       * aIsValid )
{
/***********************************************************************
 *
 * Description : VIEW�� �����ϴ� querySet�� ���ǰ˻�
 *
 * Implementation :
 *
 *    PROJ-1495
 *
 *    ���� VIEW�� �����ϴ� �� select������
 *         (1) target column�� ��� ���� �÷��̾�� �Ѵ�.
 *         (2) FROM���� ���� table�� base table�̾�� �Ѵ�.
 *         (3) WHERE���� �� �� ������,
 *             WHERE���� subquery�� �� �� ����.
 *         (4) GROUP BY              (X)
 *         (5) AGGREGATION           (X)
 *         (6) DISTINCT              (X)
 *         (7) START WITH/CONNECT BY (X)
 *
 **********************************************************************/

    qmsTarget    * sTarget;

    IDU_FIT_POINT_FATAL( "qmvQuerySet::validatePushPredHintQuerySet::__FT__" );

    //---------------------------------------------------
    // ���ռ� �˻�
    //---------------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aQuerySet != NULL );

    //---------------------------------------------------
    // �⺻ �ʱ�ȭ
    //---------------------------------------------------

    //---------------------------------------------------
    // ���� �˻� : target column�� ���� �÷�����,
    //             from���� table�� base table����
    //---------------------------------------------------

    if( ( aQuerySet->SFWGH->from->joinType == QMS_NO_JOIN ) &&
        ( aQuerySet->SFWGH->from->tableRef->view == NULL ) )
    {
        for( sTarget = aQuerySet->target;
             sTarget != NULL;
             sTarget = sTarget->next )
        {
            if( QTC_IS_COLUMN( aStatement, sTarget->targetColumn ) == ID_TRUE )
            {
                // Nothing To Do
            }
            else
            {
                *aIsValid = ID_FALSE;
                break;
            }
        }
    }
    else
    {
        *aIsValid = ID_FALSE;
    }

    //---------------------------------------------------
    // ���� �˻� : where�� �˻�
    //---------------------------------------------------

    if( *aIsValid == ID_TRUE )
    {
        if( aQuerySet->SFWGH->where != NULL )
        {
            if( ( aQuerySet->SFWGH->where->lflag & QTC_NODE_SUBQUERY_MASK )
                == QTC_NODE_SUBQUERY_EXIST )
            {
                *aIsValid = ID_FALSE;
            }
            else
            {
                // Nothing To Do
            }
        }
        else
        {
            // Nothing To Do
        }
    }
    else
    {
        // Nothing To Do
    }

    //---------------------------------------------------
    // ���� �˻� :  GROUP BY              (X)
    //              AGGREGATION           (X)
    //              DISTINCT              (X)
    //              START WITH/CONNECT BY (X)
    //---------------------------------------------------

    if( *aIsValid == ID_TRUE )
    {
        if( ( aQuerySet->SFWGH->group == NULL ) &&
            ( aQuerySet->SFWGH->aggsDepth1 == NULL ) &&
            ( aQuerySet->SFWGH->selectType == QMS_ALL ) &&
            ( aQuerySet->SFWGH->hierarchy == NULL ) )
        {
            // Nothing To Do
        }
        else
        {
            *aIsValid = ID_FALSE;
        }
    }
    else
    {
        // Nothing To Do
    }

    return IDE_SUCCESS;
}

IDE_RC qmvQuerySet::getSubqueryTarget( qcStatement  * aStatement,
                                       qtcNode      * aNode,
                                       qtcNode     ** aTargetNode )
{

    IDU_FIT_POINT_FATAL( "qmvQuerySet::getSubqueryTarget::__FT__" );

    if ( (aNode->node.lflag & MTC_NODE_OPERATOR_MASK) ==
         MTC_NODE_OPERATOR_SUBQUERY )
    {
        IDE_TEST( getSubqueryTarget( aStatement,
                                     (qtcNode *)aNode->node.arguments,
                                     aTargetNode )
                  != IDE_SUCCESS );
    }
    else
    {
        (*aTargetNode) = aNode;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmvQuerySet::changeTargetForCommunication(
    qcStatement     * aStatement,
    qmsQuerySet     * aQuerySet )
{
    qmsTarget         * sTarget;
    qtcNode           * sTargetColumn;
    mtcColumn         * sMtcColumn;
    qcuSqlSourceInfo    sqlInfo;

    IDU_FIT_POINT_FATAL( "qmvQuerySet::changeTargetForCommunication::__FT__" );
    
    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------
    
    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aQuerySet != NULL );

    //------------------------------------------
    // BUG-20652
    // client���� ������ ���� taget�� geometry type��
    // binary type���� ��ȯ
    //------------------------------------------

    // set���� �ֵ� ���� �̹� ���ο� target�� ��������
    for ( sTarget = aQuerySet->target;
          sTarget != NULL;
          sTarget = sTarget->next )
    {
        sTargetColumn = sTarget->targetColumn;

        if( aStatement->calledByPSMFlag != ID_TRUE )
        {
            // target�� �׻� type�� ���ǵǾ�����
            IDE_DASSERT( MTC_NODE_IS_DEFINED_TYPE( & sTargetColumn->node )
                         == ID_TRUE );
        }
        else
        {
            // Nothing to do.
        }
        
        sMtcColumn = QTC_STMT_COLUMN( aStatement, sTargetColumn );
        
        if ( sMtcColumn->module->id == MTD_GEOMETRY_ID )
        {            
            // subquery�� �ƴ϶�� target�� �׻� conversion�� ����
            IDE_DASSERT( sTargetColumn->node.conversion == NULL );
            
            // target�� conversion�� �����Ǵ� ���ܻ�Ȳ.
            // get_blob_locatoró�� �Լ��� �̿��� ��ȯ�� ������ �� ������
            // �� ��� ����� ���� ��� memcpy�� �����ϰ� �ǹǷ�
            // conversion�� �̿��Ѵ�.
            IDE_TEST( qtc::makeConversionNode( sTargetColumn,
                                               aStatement,
                                               QC_SHARED_TMPLATE(aStatement),
                                               & mtdBinary )
                      != IDE_SUCCESS );
        }
        else
        {
            // Nothing to do.
        }

        // ���� target column�� list type�̾�� �ȵȴ�.
        if ( sMtcColumn->module->id == MTD_LIST_ID )
        {
            sqlInfo.setSourceInfo( aStatement,
                                   & sTargetColumn->position );
            IDE_RAISE( ERR_NOT_APPLICABLE_TYPE_IN_TARGET );
        }
        else
        {
            // Nothing to do.
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NOT_APPLICABLE_TYPE_IN_TARGET );
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QMV_NOT_APPLICABLE_TYPE_IN_TARGET,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmvQuerySet::addDecryptFunc( qcStatement  * aStatement,
                                    qmsTarget    * aTarget )
{
    qmsTarget         * sCurrTarget;
    qtcNode           * sTargetColumn;
    qtcNode           * sNode;
    qtcNode           * sPrevNode = NULL;
    mtcColumn         * sMtcColumn;

    IDU_FIT_POINT_FATAL( "qmvQuerySet::changeTargetForCommunication::__FT__" );

    // add Decrypt function
    for ( sCurrTarget = aTarget;
          sCurrTarget != NULL;
          sCurrTarget = sCurrTarget->next)
    {
        sTargetColumn = sCurrTarget->targetColumn;
        
        if( aStatement->calledByPSMFlag == ID_FALSE )
        {
            // target�� �׻� type�� ���ǵǾ�����
            IDE_DASSERT( MTC_NODE_IS_DEFINED_TYPE( & sTargetColumn->node )
                         == ID_TRUE );
        }
        else
        {
            // Nothing to do.
        }
        
        sMtcColumn = QTC_STMT_COLUMN( aStatement, sTargetColumn );

        if( (sMtcColumn->module->flag & MTD_ENCRYPT_TYPE_MASK)
            == MTD_ENCRYPT_TYPE_TRUE )
        {
            // default policy�� ��ȣ Ÿ���̶� decrypt �Լ��� �����Ͽ�
            // subquery�� ����� �׻� ��ȣ Ÿ���� ���� �� ���� �Ѵ�.
                
            // decrypt �Լ��� �����.
            IDE_TEST( addDecryptFuncForNode( aStatement,
                                             sTargetColumn,
                                             & sNode )
                      != IDE_SUCCESS );

            if ( sPrevNode != NULL )
            {
                sPrevNode->node.next = (mtcNode*)sNode;
            }
            else
            {
                // Nothing to do.
            }

            // target ��带 �ٲ۴�.
            sCurrTarget->targetColumn = sNode;

            sPrevNode = sNode;
        }
        else 
        {
            sPrevNode = sTargetColumn;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmvQuerySet::addDecryptFuncForNode( qcStatement  * aStatement,
                                           qtcNode      * aNode,
                                           qtcNode     ** aNewNode )
{
    qtcNode  * sNode[2];

    IDU_FIT_POINT_FATAL( "qmvQuerySet::addDecryptFuncForNode::__FT__" );
    
    // decrypt �Լ��� �����.
    IDE_TEST( qtc::makeNode( aStatement,
                             sNode,
                             & aNode->columnName,
                             & mtfDecrypt )
              != IDE_SUCCESS );

    // �Լ��� �����Ѵ�.
    sNode[0]->node.arguments = (mtcNode*) aNode;
    sNode[0]->node.next = aNode->node.next;
    sNode[0]->node.arguments->next = NULL;

    // �Լ��� estimate�� �����Ѵ�.
    IDE_TEST( qtc::estimateNodeWithArgument( aStatement,
                                             sNode[0] )
              != IDE_SUCCESS );

    *aNewNode = sNode[0];

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmvQuerySet::addBLobLocatorFuncForNode( qcStatement  * aStatement,
                                               qtcNode      * aNode,
                                               qtcNode     ** aNewNode )
{
    qtcNode  * sNode[2];

    IDU_FIT_POINT_FATAL( "qmvQuerySet::addDecryptFuncForNode::__FT__" );

    /* get_blob_locator �Լ��� �����. */
    IDE_TEST( qtc::makeNode( aStatement,
                             sNode,
                             & aNode->columnName,
                             & mtfGetBlobLocator )
              != IDE_SUCCESS );

    /* �Լ��� �����Ѵ�. */
    sNode[0]->node.arguments = (mtcNode*) aNode;
    sNode[0]->node.next = aNode->node.next;
    sNode[0]->node.arguments->next = NULL;

    /* �Լ��� estimate�� �����Ѵ�. */
    IDE_TEST( qtc::estimateNodeWithArgument( aStatement,
                                             sNode[0] )
              != IDE_SUCCESS );

    *aNewNode = sNode[0];

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmvQuerySet::addCLobLocatorFuncForNode( qcStatement  * aStatement,
                                               qtcNode      * aNode,
                                               qtcNode     ** aNewNode )
{
    qtcNode  * sNode[2];

    IDU_FIT_POINT_FATAL( "qmvQuerySet::addCLobLocatorFuncForNode::__FT__" );

    /* get_clob_locator �Լ��� �����. */
    IDE_TEST( qtc::makeNode( aStatement,
                             sNode,
                             & aNode->columnName,
                             & mtfGetClobLocator )
              != IDE_SUCCESS );

    /* �Լ��� �����Ѵ�. */
    sNode[0]->node.arguments = (mtcNode*) aNode;
    sNode[0]->node.next = aNode->node.next;
    sNode[0]->node.arguments->next = NULL;

    /* �Լ��� estimate�� �����Ѵ�. */
    IDE_TEST( qtc::estimateNodeWithArgument( aStatement,
                                             sNode[0] )
              != IDE_SUCCESS );

    *aNewNode = sNode[0];

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
 * ----------------------------------------------------------------------------
 * PROJ-1789: Updatable Scrollable Cursor (a.k.a. PROWID)
 *
 * qmsTarget�� ���� column ������ setting �Ѵ�
 *
 * ������ aliasName�� displayName�� setting �ߴµ�
 * userName, tableName, isUpdatable ���� ������ �߰��Ͽ���
 *
 * ��)
 *
 * SELECT C1 FROM T1
 *
 * tableName       = T1
 * aliasTableName  = T1
 * columnName      = C1
 * aliasColumnName = C1
 * displayName     = C1
 *
 *
 * SELECT C1 AS C FROM T1 AS T
 *
 * tableName       = T1
 * aliasTableName  = T
 * columnName      = C1
 * aliasColumnName = C
 * displayName     = C
 *
 *
 * SELECT C1+1 AS C FROM T1
 *
 * tableName       = NULL
 * aliasTableName  = NULL
 * columnName      = NULL
 * aliasColumnName = C 
 * displayName     = C
 * 
 *
 * SELECT C1+1 FROM T1
 *
 * tableName       = NULL
 * aliasTableName  = NULL
 * columnName      = NULL
 * aliasColumnName = NULL
 * displayName     = C1+1
 *
 *
 * SELECT C1 FROM (SELECT * FROM T1);
 *
 * tableName       = NULL
 * aliasTableName  = NULL
 * columnName      = C1
 * aliasColumnName = C1
 * displayName     = C1
 * ----------------------------------------------------------------------------
 */
IDE_RC qmvQuerySet::setTargetColumnInfo(qcStatement* aStatement,
                                        qmsTarget  * aTarget)
{
    qtcNode        * sExpression;
    qmsFrom        * sFrom;
    qcNamePosition * sNamePosition;
    qcNamePosition   sPosition;
    UInt             sCurOffset;
    idBool           sIsAliasExist;
    idBool           sIsBaseColumn;
    qcuSqlSourceInfo sqlInfo;

    IDU_FIT_POINT_FATAL( "qmvQuerySet::setTargetColumnInfo::__FT__" );

    sExpression = aTarget->targetColumn;
    sIsBaseColumn = QTC_IS_COLUMN(aStatement, sExpression);

    // PROJ-1653 Outer Join Operator (+)
    // Outer Join Operator (+) �� target ������ �� �� ����.
    if ( ( sExpression->lflag & QTC_NODE_JOIN_OPERATOR_MASK )
         == QTC_NODE_JOIN_OPERATOR_EXIST )
    {
        sqlInfo.setSourceInfo( aStatement,
                               & sExpression->position );

        IDE_RAISE( ERR_NOT_APPLICABLE_OPERATOR_IN_TARGET );
    }

    if ( aTarget->aliasColumnName.size != QC_POS_EMPTY_SIZE )
    {
        sIsAliasExist = ID_TRUE;
    }
    else
    {
        sIsAliasExist = ID_FALSE;
    }

    if ( sIsBaseColumn == ID_TRUE )
    {
        sFrom = QC_SHARED_TMPLATE(aStatement)->
            tableMap[(sExpression)->node.table].from;

        /* BUG-37658 : meta table info �� free �� �� �����Ƿ� ���� �׸���
         *             alloc -> memcpy �Ѵ�.
         * - userName
         * - tableName
         * - aliasTableName
         * - columnName
         * - aliasColumnName
         * - displayName
         */

        // set user name
        sNamePosition = &sFrom->tableRef->userName;
        if ( sNamePosition->size != QC_POS_EMPTY_SIZE )
        {
            IDE_TEST( STRUCT_ALLOC_WITH_SIZE( QC_QMP_MEM(aStatement),
                                              SChar,
                                              sNamePosition->size + 1,
                                              &(aTarget->userName.name) )
                      != IDE_SUCCESS );
            aTarget->userName.size = sNamePosition->size;

            idlOS::strncpy( aTarget->userName.name,
                            sNamePosition->stmtText + sNamePosition->offset,
                            aTarget->userName.size );
            aTarget->userName.name[aTarget->userName.size] = '\0';
        }
        else
        {
            aTarget->userName.name = NULL;
            aTarget->userName.size = QC_POS_EMPTY_SIZE;
        }

        // set base table name
        sNamePosition = &sFrom->tableRef->tableName;
        if ( sNamePosition->size != QC_POS_EMPTY_SIZE )
        {
            // BUG-30344
            // plan cache check-in�� �����Ͽ� non-cache mode�� ����� ��
            // query text�� �� �̻� �������� �����Ƿ� ���� �����Ѵ�.
            IDE_TEST( STRUCT_ALLOC_WITH_SIZE( QC_QMP_MEM(aStatement),
                                              SChar,
                                              sNamePosition->size + 1,
                                              &(aTarget->tableName.name) )
                      != IDE_SUCCESS );
            aTarget->tableName.size = sNamePosition->size;

            idlOS::strncpy( aTarget->tableName.name,
                            sNamePosition->stmtText +
                            sNamePosition->offset,
                            aTarget->tableName.size );
            aTarget->tableName.name[aTarget->tableName.size] = '\0';
        }
        else
        {
            aTarget->tableName.name = NULL;
            aTarget->tableName.size = QC_POS_EMPTY_SIZE;
        }

        // set alias table name
        sNamePosition = &sFrom->tableRef->aliasName;
        if ( sNamePosition->size != QC_POS_EMPTY_SIZE )
        {
            // BUG-30344
            // plan cache check-in�� �����Ͽ� non-cache mode�� ����� ��
            // query text�� �� �̻� �������� �����Ƿ� ���� �����Ѵ�.
            IDE_TEST( STRUCT_ALLOC_WITH_SIZE( QC_QMP_MEM(aStatement),
                                              SChar,
                                              sNamePosition->size + 1,
                                              &(aTarget->aliasTableName.name) )
                      != IDE_SUCCESS);

            idlOS::strncpy( aTarget->aliasTableName.name,
                            sNamePosition->stmtText + sNamePosition->offset,
                            sNamePosition->size );

            aTarget->aliasTableName.name[sNamePosition->size] = '\0';
            aTarget->aliasTableName.size = sNamePosition->size;
        }
        else
        {
            // alias table name�� ������ �׳� table name
            aTarget->aliasTableName.name = aTarget->tableName.name;
            aTarget->aliasTableName.size = aTarget->tableName.size;
        }

        // set base column name
        sNamePosition = &sExpression->columnName;

        if ( ( sExpression->lflag & QTC_NODE_PRIOR_MASK )
             == QTC_NODE_PRIOR_EXIST )
        {
            // 'PRIOR '�� �տ� �����δ�.
            IDE_TEST(STRUCT_ALLOC_WITH_SIZE(QC_QMP_MEM(aStatement),
                                            SChar,
                                            sNamePosition->size + 7,
                                            &(aTarget->columnName.name))
                     != IDE_SUCCESS);

            idlOS::strncpy(aTarget->columnName.name,
                           "PRIOR ",
                           6);

            idlOS::strncpy(aTarget->columnName.name + 6,
                           sNamePosition->stmtText + sNamePosition->offset,
                           sNamePosition->size);

            aTarget->columnName.size = sNamePosition->size + 6;
        }
        else
        {
            IDE_TEST(STRUCT_ALLOC_WITH_SIZE(QC_QMP_MEM(aStatement),
                                            SChar,
                                            sNamePosition->size + 1,
                                            &(aTarget->columnName.name))
                     != IDE_SUCCESS);

            idlOS::strncpy(aTarget->columnName.name,
                           sNamePosition->stmtText + sNamePosition->offset,
                           sNamePosition->size);

            aTarget->columnName.size = sNamePosition->size;
        }

        aTarget->columnName.name[aTarget->columnName.size] = '\0';

        // set alias column name
        if ( sIsAliasExist == ID_FALSE )
        {
            // alias�� ������ column name

            sNamePosition = &sExpression->columnName;
            // BUG-30344
            // plan cache check-in�� �����Ͽ� non-cache mode�� ����� ��
            // query text�� �� �̻� �������� �����Ƿ� ���� �����Ѵ�.
            IDE_TEST(STRUCT_ALLOC_WITH_SIZE(QC_QMP_MEM(aStatement),
                                            SChar,
                                            sNamePosition->size + 1,
                                            &(aTarget->aliasColumnName.name))
                     != IDE_SUCCESS);

            idlOS::strncpy(aTarget->aliasColumnName.name,
                           sNamePosition->stmtText + sNamePosition->offset,
                           sNamePosition->size);
            aTarget->aliasColumnName.name[sNamePosition->size] = '\0';
            aTarget->aliasColumnName.size = sNamePosition->size;
        }
        else
        {
            // parse �� �̹� setting �Ǿ���
        }

        // set updatability
        if (sFrom->tableRef->view == NULL)
        {
            aTarget->flag &= ~QMS_TARGET_IS_UPDATABLE_MASK;
            aTarget->flag |= QMS_TARGET_IS_UPDATABLE_TRUE;
        }
        else
        {
            aTarget->flag &= ~QMS_TARGET_IS_UPDATABLE_MASK;
            aTarget->flag |= QMS_TARGET_IS_UPDATABLE_FALSE;
        }
    }
    else
    {
        aTarget->userName.name        = NULL;
        aTarget->userName.size        = QC_POS_EMPTY_SIZE;
        aTarget->tableName.name       = NULL;
        aTarget->tableName.size       = QC_POS_EMPTY_SIZE;
        aTarget->aliasTableName.name  = NULL;
        aTarget->aliasTableName.size  = QC_POS_EMPTY_SIZE;
        aTarget->columnName.name      = NULL;
        aTarget->columnName.size      = QC_POS_EMPTY_SIZE;
        aTarget->flag                &= ~QMS_TARGET_IS_UPDATABLE_MASK;
        aTarget->flag                |= QMS_TARGET_IS_UPDATABLE_FALSE;
    }

    // set display name
    if (sIsBaseColumn == ID_FALSE)
    {
        if (sIsAliasExist == ID_FALSE)
        {
            IDE_TEST(STRUCT_ALLOC_WITH_SIZE(QC_QMP_MEM(aStatement),
                                            SChar,
                                            sExpression->position.size + 1,
                                            &(aTarget->displayName.name))
                     != IDE_SUCCESS);

            // BUG-38946 display name�� ����
            // natc���� �����ϴ� ��쵵 ���� display name���� ����Ѵ�.
            if ( ( QCU_COMPAT_DISPLAY_NAME == 1 ) ||
                 ( QCU_DISPLAY_PLAN_FOR_NATC == 1 ) )
            {
                // ������ �����ϰ� �����Ѵ�.
                getDisplayName( sExpression, &sPosition );
                
                idlOS::strncpy(aTarget->displayName.name,
                               sPosition.stmtText +
                               sPosition.offset,
                               sPosition.size);
                aTarget->displayName.name[sPosition.size] = '\0';
                aTarget->displayName.size = sPosition.size;
            }
            else
            {
                // O��� �����ϰ� �����Ѵ�.
                copyDisplayName( sExpression,
                                 &(aTarget->displayName) );
            }
        }
        else
        {
            IDE_TEST(STRUCT_ALLOC_WITH_SIZE(QC_QMP_MEM(aStatement),
                                            SChar,
                                            aTarget->aliasColumnName.size + 1,
                                            &(aTarget->displayName.name))
                     != IDE_SUCCESS);

            idlOS::strncpy(aTarget->displayName.name,
                           aTarget->aliasColumnName.name,
                           aTarget->aliasColumnName.size);
            aTarget->displayName.name[aTarget->aliasColumnName.size] = '\0';
            aTarget->displayName.size = aTarget->aliasColumnName.size;
        }
    }
    else
    {
        if (sIsAliasExist == ID_FALSE)
        {
            sCurOffset = 0;

            /*
             * TODO:
             * BUG-33546 SELECT_HEADER_DISPLAY property does not work properly
             * 
             * �� bug �� �ذ��Ϸ��� if ������ �Ʒ� �ּ��� ���� �ϸ� �ȴ�.
             */

            /*
              if ((QCG_GET_SESSION_SELECT_HEADER_DISPLAY(aStatement) == 0) &&
              (aTarget->aliasTableName.size > 0))
            */
            if (1 == 0)
            {
                // tableName.columnName
                IDE_TEST(STRUCT_ALLOC_WITH_SIZE(QC_QMP_MEM(aStatement),
                                                SChar,
                                                aTarget->aliasTableName.size +
                                                aTarget->columnName.size + 2,
                                                &(aTarget->displayName.name))
                         != IDE_SUCCESS);

                idlOS::strncpy(aTarget->displayName.name,
                               aTarget->aliasTableName.name,
                               aTarget->aliasTableName.size);
                aTarget->displayName.name[aTarget->aliasTableName.size] = '.';
                sCurOffset = sCurOffset + aTarget->aliasTableName.size + 1;
            }
            else
            {
                // columnName
                IDE_TEST(STRUCT_ALLOC_WITH_SIZE(QC_QMP_MEM(aStatement),
                                                SChar,
                                                aTarget->columnName.size + 1,
                                                &(aTarget->displayName.name))
                         != IDE_SUCCESS);
            }
            idlOS::strncpy(aTarget->displayName.name + sCurOffset,
                           aTarget->columnName.name,
                           aTarget->columnName.size);
            sCurOffset = sCurOffset + aTarget->columnName.size;

            aTarget->displayName.name[sCurOffset] = '\0';
            aTarget->displayName.size = sCurOffset;

            qcgPlan::registerPlanProperty(aStatement,
                                          PLAN_PROPERTY_SELECT_HEADER_DISPLAY);
        }
        else
        {
            // alias�� ������ SELECT_HEADER_DISPLAY ����
            IDE_TEST(STRUCT_ALLOC_WITH_SIZE(QC_QMP_MEM(aStatement),
                                            SChar,
                                            aTarget->aliasColumnName.size + 1,
                                            &(aTarget->displayName.name))
                     != IDE_SUCCESS);

            idlOS::strncpy(aTarget->displayName.name,
                           aTarget->aliasColumnName.name,
                           aTarget->aliasColumnName.size);
            aTarget->displayName.name[aTarget->aliasColumnName.size] = '\0';
            aTarget->displayName.size = aTarget->aliasColumnName.size;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_NOT_APPLICABLE_OPERATOR_IN_TARGET)
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QMV_NOT_APPLICABLE_OPERATOR_IN_TARGET,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmvQuerySet::addCastOper( qcStatement  * aStatement,
                                 qmsTarget    * aTarget )
{
    qmsTarget         * sCurrTarget;
    qtcNode           * sNewNode;
    qtcNode           * sPrevNode = NULL;

    IDU_FIT_POINT_FATAL( "qmvQuerySet::addCastOper::__FT__" );

    if( ( QCU_COERCE_HOST_VAR_TO_VARCHAR > 0 ) &&
        ( aStatement->calledByPSMFlag == ID_FALSE ) )
    {
        for ( sCurrTarget = aTarget;
              sCurrTarget != NULL;
              sCurrTarget = sCurrTarget->next)
        {
            IDE_TEST( searchHostVarAndAddCastOper( aStatement,
                                                   sCurrTarget->targetColumn,
                                                   & sNewNode,
                                                   ID_FALSE )
                      != IDE_SUCCESS );

            if ( sNewNode != NULL )
            {
                sCurrTarget->targetColumn = sNewNode;

                if ( sPrevNode != NULL )
                {
                    sPrevNode->node.next = (mtcNode*)sNewNode;
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

            sPrevNode = sCurrTarget->targetColumn;
        }
    }
    else
    {
        // Nothing to do.
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmvQuerySet::searchHostVarAndAddCastOper( qcStatement   * aStatement,
                                                 qtcNode       * aNode,
                                                 qtcNode      ** aNewNode,
                                                 idBool          aContainRootsNext )
{
    mtcTemplate    * sTemplate;
    mtcColumn        sTypeColumn;
    qtcNode        * sNewNode;
    qtcNode        * sCastNode[2];
    qtcNode        * sTypeNode[2];
    UShort           sVariableRow;
    qcNamePosition   sEmptyPos;

    IDU_FIT_POINT_FATAL( "qmvQuerySet::searchHostVarAndAddCastOper::__FT__" );

    sTemplate    = & QC_SHARED_TMPLATE(aStatement)->tmplate;
    sVariableRow = sTemplate->variableRow;
    *aNewNode    = NULL;

    if ( ( aNode->node.module == & mtfCast ) ||
         ( ( aNode->node.lflag & MTC_NODE_OPERATOR_MASK )
           == MTC_NODE_OPERATOR_SUBQUERY ) )
    {
        /* BUG-42522 A COERCE_HOST_VAR_IN_SELECT_LIST_TO_VARCHAR is sometimes
         * wrong.
         */
        if ( ( aNode->node.next != NULL ) &&
             ( aContainRootsNext == ID_TRUE ) )
        {
            IDE_TEST( searchHostVarAndAddCastOper(
                          aStatement,
                          (qtcNode *) aNode->node.next,
                          & sNewNode,
                          ID_TRUE )
                      != IDE_SUCCESS );

            if ( sNewNode != NULL )
            {
                aNode->node.next = (mtcNode*) sNewNode;
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
        if ( aNode->node.arguments != NULL )
        {
            IDE_TEST( searchHostVarAndAddCastOper(
                          aStatement,
                          (qtcNode *) aNode->node.arguments,
                          & sNewNode,
                          ID_TRUE )
                      != IDE_SUCCESS );

            if ( sNewNode != NULL )
            {
                aNode->node.arguments = (mtcNode*) sNewNode;
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
        
        if ( ( aNode->node.next != NULL ) && ( aContainRootsNext == ID_TRUE ) )
        {
            IDE_TEST( searchHostVarAndAddCastOper(
                          aStatement,
                          (qtcNode *) aNode->node.next,
                          & sNewNode,
                          ID_TRUE )
                      != IDE_SUCCESS );

            if ( sNewNode != NULL )
            {
                aNode->node.next = (mtcNode*) sNewNode;
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
        
        if ( ( (aNode->node.lflag & MTC_NODE_BIND_MASK) == MTC_NODE_BIND_EXIST )
             &&
             ( aNode->node.table == sVariableRow )
             &&
             ( aNode->node.arguments == NULL ) )
        {
            // host variable, terminal node
            SET_EMPTY_POSITION( sEmptyPos );
            
            // cast operator�� �����.
            IDE_TEST( qtc::makeNode( aStatement,
                                     sCastNode,
                                     & aNode->columnName,
                                     & mtfCast )
                      != IDE_SUCCESS );

            IDE_TEST( mtc::initializeColumn( & sTypeColumn,
                                             & mtdVarchar,
                                             1,
                                             QCU_COERCE_HOST_VAR_TO_VARCHAR,
                                             0 )
                      != IDE_SUCCESS );
            
            // column������ �̿��Ͽ� qtcNode�� �����Ѵ�.
            IDE_TEST( qtc::makeProcVariable( aStatement,
                                             sTypeNode,
                                             & sEmptyPos,
                                             & sTypeColumn,
                                             QTC_PROC_VAR_OP_NEXT_COLUMN )
                      != IDE_SUCCESS );
            
            sCastNode[0]->node.funcArguments = (mtcNode *) sTypeNode[0];
            
            sCastNode[0]->node.lflag &= ~MTC_NODE_COLUMN_COUNT_MASK;
            sCastNode[0]->node.lflag |= 1;
            
            // �Լ��� �����Ѵ�.
            sCastNode[0]->node.arguments = (mtcNode*) aNode;
            sCastNode[0]->node.next = aNode->node.next;
            sCastNode[0]->node.arguments->next = NULL;
            
            *aNewNode = sCastNode[0];
        }
        else
        {
            // Nothing to do.
        }        
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


// BUG-38273
// ansi ��Ÿ�� inner join �� �Ϲ� ��Ÿ���� �������� �����Ѵ�.
idBool qmvQuerySet::checkInnerJoin( qmsFrom * aFrom )
{
    qmsFrom  * sFrom = aFrom;
    idBool     sOnlyInnerJoin;

    if( sFrom->joinType == QMS_NO_JOIN )
    {
        sOnlyInnerJoin = ID_TRUE;
    }
    else if( sFrom->joinType == QMS_INNER_JOIN )
    {
        if( checkInnerJoin( sFrom->left ) == ID_TRUE )
        {
            sOnlyInnerJoin = checkInnerJoin( sFrom->right );
        }
        else
        {
            sOnlyInnerJoin = ID_FALSE;
        }
    }
    else
    {
        sOnlyInnerJoin = ID_FALSE;
    }

    return sOnlyInnerJoin;
}

// BUG-38273
// ansi ��Ÿ�� inner join �� �Ϲ� ��Ÿ���� �������� �����Ѵ�.
IDE_RC qmvQuerySet::innerJoinToNoJoin( qcStatement * aStatement,
                                       qmsSFWGH    * aSFWGH,
                                       qmsFrom     * aFrom )
{
    qmsFrom         * sFrom = aFrom;
    qmsFrom         * sTempFrom;
    qtcNode         * sNode[2];
    qcNamePosition    sNullPosition;

    IDU_FIT_POINT_FATAL( "qmvQuerySet::innerJoinToNoJoin::__FT__" );

    if ( sFrom->left->joinType == QMS_INNER_JOIN )
    {
        IDE_TEST( innerJoinToNoJoin( aStatement,
                                     aSFWGH,
                                     sFrom->left ) != IDE_SUCCESS);
    }
    else
    {
        // Nothing to do.
    }

    if ( sFrom->right->joinType == QMS_INNER_JOIN )
    {
        IDE_TEST( innerJoinToNoJoin( aStatement,
                                     aSFWGH,
                                     sFrom->right ) != IDE_SUCCESS);
    }
    else
    {
        // Nothing to do.
    }

    if( aSFWGH->where != NULL )
    {
        //------------------------------------------------------
        // ���� where ���� on ���� ������Ŷ�� �����Ѵ�.
        // and �����ڸ� �����Ͽ� �����ϵ�
        // ������ and �����ڰ� ������ and �������� ���ڷ� �����Ѵ�.
        //------------------------------------------------------
        if ( aSFWGH->where->node.module != &mtfAnd )
        {
            SET_EMPTY_POSITION(sNullPosition);

            IDE_TEST(qtc::makeNode(aStatement,
                                   sNode,
                                   &sNullPosition,
                                   &mtfAnd ) != IDE_SUCCESS);

            sNode[0]->node.arguments       = (mtcNode *)sFrom->onCondition;
            sNode[0]->node.arguments->next = (mtcNode *)aSFWGH->where;
            sNode[0]->node.lflag += 2;

            aSFWGH->where = sNode[0];
        }
        else
        {
            //------------------------------------------------------
            // ���� and ����� �������� �����Ѵ�.
            //------------------------------------------------------
            for( sNode[0] = (qtcNode *)aSFWGH->where->node.arguments;
                 sNode[0]->node.next != NULL;
                 sNode[0] = (qtcNode*)sNode[0]->node.next);

            sNode[0]->node.next = (mtcNode *)sFrom->onCondition;
        }
    }
    else
    {
        //------------------------------------------------------
        // ���� where ���� ���� ��� �ٷ� �����Ѵ�.
        //------------------------------------------------------
        aSFWGH->where = sFrom->onCondition;
    }

    //------------------------------------------------------
    // left �� �������� right �� �����Ѵ�.
    //------------------------------------------------------
    for( sTempFrom = sFrom->left;
         sTempFrom->next != NULL;
         sTempFrom = sTempFrom->next );
    sTempFrom->next = sFrom->right;

    //------------------------------------------------------
    // right �� �������� �θ��� next �� �����Ѵ�.
    //------------------------------------------------------
    for( sTempFrom = sFrom->right;
         sTempFrom->next != NULL;
         sTempFrom = sTempFrom->next );
    sTempFrom->next = sFrom->next;

    // BUG-38347 fromPosition �� �����ؾ���
    SET_POSITION( sFrom->left->fromPosition, sFrom->fromPosition );

    idlOS::memcpy( sFrom, sFrom->left, ID_SIZEOF(qmsFrom));

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* BUG-38946
 * ���� display name�� �����ϰ� �����ϱ� ���� �Լ�
 *
 * select (1) from dual               --> (1)
 * select count(1) from dual          --> COUNT(1)
 * select count(*) from dual          --> COUNT
 * select count(*) over () from dual  --> COUNT             <===(*)
 * select count(i1) over () from dual --> COUNT(I1) over ()
 * select user_name() from dual       --> USER_NAME
 * select func1 from dual             --> FUNC1
 * select func1() from dual           --> FUNC1
 * select func1(1) from dual          --> FUNC1(1)
 * select pkg1.func1 from dual        --> PKG1.FUNC1        <===(*)
 * select pkg1.func1() from dual      --> FUNC1
 * select pkg1.func1(1) from dual     --> FUNC1(1)
 * select user1.func1 from dual       --> USER1.FUNC1       <===(*)
 * select user1.func1() from dual     --> FUNC1
 * select user1.func1(1) from dual    --> FUNC1(1)
 * select seq1.nextval from dual      --> SEQ1.NEXTVAL
 *
 * (*)���� ��� ��쿡 ���� display name�� �޶����� ���װ� �־�
 * �׻� display�Ǵ� ������ �����Ѵ�.
 * select count(*) over () from dual  --> COUNT(*) over ()
 * select pkg1.func1 from dual        --> FUNC1
 * select user1.func1 from dual       --> FUNC1
 */
void qmvQuerySet::getDisplayName( qtcNode        * aNode,
                                  qcNamePosition * aNamePos )
{
    if ( ( ( aNode->node.lflag & MTC_NODE_REMOVE_ARGUMENTS_MASK ) ==
           MTC_NODE_REMOVE_ARGUMENTS_TRUE ) ||
         ( aNode->overClause != NULL ) )
    {
        // count(*), (1), (func1())
        SET_POSITION( *aNamePos, aNode->position );
    }
    else
    {
        if ( ( aNode->node.lflag & MTC_NODE_COLUMN_COUNT_MASK ) > 0 )
        {
            if ( aNode->node.module == &qtc::spFunctionCallModule )
            {
                // func1(1), pkg1.func(1)
                if ( QC_IS_NULL_NAME(aNode->pkgName) == ID_TRUE )
                {
                    aNamePos->stmtText = aNode->columnName.stmtText;
                    aNamePos->offset   = aNode->columnName.offset;
                    aNamePos->size     = aNode->position.size +
                        aNode->position.offset - aNode->columnName.offset;
                }
                else
                {
                    aNamePos->stmtText = aNode->pkgName.stmtText;
                    aNamePos->offset   = aNode->pkgName.offset;
                    aNamePos->size     = aNode->position.size +
                        aNode->position.offset - aNode->pkgName.offset;
                }
            }
            else
            {
                // count(1)
                SET_POSITION( *aNamePos, aNode->position );
            }
        }
        else
        {
            if ( aNode->node.module == &qtc::spFunctionCallModule )
            {
                // func1, func1(), pkg1.func1, pkg1.func1()
                if ( QC_IS_NULL_NAME(aNode->pkgName) == ID_TRUE )
                {
                    SET_POSITION( *aNamePos, aNode->columnName );
                }
                else
                {
                    SET_POSITION( *aNamePos, aNode->pkgName );
                }
            }
            else
            {
                // user_name()
                if ( ( QC_IS_NULL_NAME(aNode->position) == ID_FALSE ) &&
                     ( QC_IS_NULL_NAME(aNode->columnName) == ID_FALSE ) )
                {
                    aNamePos->stmtText = aNode->position.stmtText;
                    aNamePos->offset   = aNode->position.offset;
                    aNamePos->size     = aNode->columnName.offset +
                        aNode->columnName.size - aNode->position.offset;
                }
                else
                {
                    SET_EMPTY_POSITION( *aNamePos );
                }
            }
        }
    }

}

/* BUG-38946
 * O��� �����ϰ� display name�� �����ϱ� ���� �Լ�
 *
 * select (1) from dual               --> (1)
 * select 'abc' from dual             --> 'ABC'
 * select count(1) from dual          --> COUNT(1)
 * select count(*) from dual          --> COUNT(*)
 * select count(*) over () from dual  --> COUNT(*)OVER()
 * select count(i1) over () from dual --> COUNT(I1)OVER()
 * select user_name() from dual       --> USER_NAME()
 * select func1 from dual             --> FUNC1
 * select func1() from dual           --> FUNC1()
 * select func1(1) from dual          --> FUNC1(1)
 * select pkg1.func1 from dual        --> PKG1.FUNC1
 * select pkg1.func1() from dual      --> PKG1.FUNC1()
 * select pkg1.func1(1) from dual     --> PKG1.FUNC1(1)
 * select user1.func1 from dual       --> USER1.FUNC1
 * select user1.func1() from dual     --> USER1.FUNC1()
 * select user1.func1(1) from dual    --> USER1.FUNC1(1)
 * select seq1.nextval from dual      --> NEXTVAL           <===(*)
 */
void qmvQuerySet::copyDisplayName( qtcNode         * aNode,
                                   qmsNamePosition * aNamePos )
{
    qcNamePosition   sPosition;
    SChar          * sName;
    SChar          * sPos;
    SInt             i;

    // BUG-41072
    // pseudo column�� ��� columnName�� ǥ���Ѵ�.
    if ( aNode->node.module == &qtc::columnModule )
    {
        SET_POSITION( sPosition, aNode->columnName );
    }
    else
    {
        SET_POSITION( sPosition, aNode->position );
    }
    
    sName = aNamePos->name;
    sPos  = sPosition.stmtText + sPosition.offset;
    
    for ( i = 0; i < sPosition.size; i++, sPos++ )
    {
        if ( idlOS::idlOS_isspace( *sPos ) == 0 )
        {
            *sName = idlOS::toupper(*sPos);
            sName++;
        }
        else
        {
            // Nothing to do.
        }
    }
    *sName = '\0';
    
    aNamePos->size = sName - aNamePos->name;

}

void qmvQuerySet::setLateralOuterQueryAndFrom( qmsQuerySet * aViewQuerySet, 
                                               qmsTableRef * aViewTableRef,
                                               qmsSFWGH    * aOuterSFWGH )
{
/***********************************************************************
 *
 * Description : PROJ-2418 Cross/Outer APPLY & Lateral View
 * 
 *  Lateral View�� outerQuery�� ���� ������ SFWGH�� �����Ѵ�.
 *  ANSI Join Tree ���ο� Lateral View�� �����ϸ� outerFrom�� �����Ѵ�.
 *
 *  outerQuery�� Lateral View (aViewQuerySet)�� ���� SFWGH�� ���ϰ�,
 *  outerFrom�� Lateral View�� ���� Join Tree�� ��Ÿ����.
 *  �翬�� outerQuery->from �� outerFrom�� �����ϸ�, 
 *  outerFrom�� �� ���� ������ ��Ÿ����.
 *
 *  aViewQuerySet�� validation �� ��, �ܺ� ���� �÷��� ��ġ�� �ľ��� ��,
 *
 *   - outerFrom�� �����ϸ� outerFrom������ Ž���� �����Ѵ�.
 *   - outerFrom�� �������� ������ outerQuery������ Ž���� �����Ѵ�.
 *
 *  =====================================================================
 *
 *  [ outerFrom? ]
 *  ANSI-Join Tree ���ο��� �ܺ� ������ �� �ִ� ������ outerQuery����
 *  ���ѵǾ�� �ϴµ�, �̸� ��Ÿ���� ���� outerFrom�̴�.
 *
 *  ���� ���, ON ���� �̷� Subquery�� ���ȴٸ� outerQuery/outerFrom ��,
 *  
 *  SELECT * 
 *  FROM   T1, 
 *         T2 LEFT JOIN T3 ON T2.i1 = T3.i1 
 *            LEFT JOIN T4 ON 1 = ( SELECT T1.i1 FROM DUAL ) -- Subquery
 *            LEFT JOIN T5 ON T2.i2 = T5.i2;
 *
 *         [T1]                (JOIN)
 *                            ��    \
 *                           ��     [T5]
 *                          ��  
 *                      (JOIN)---SubqueryFilter   
 *                     ��    \
 *                 (JOIN)    [T4]
 *                ��    \
 *              [T2]    [T3]
 *  
 *   - Subquery�� outerQuery->from = { 1, { { { 2, 3 }, 4 }, 5 } }
 *   - Subquery�� outerFrom = { { 2, 3 }, 4 }
 *
 *  �� �ȴ�. ����, ���� Subquery�� T1.i1�� �߸� �����ǰ� �����Ƿ� ������ ����.
 *
 *  ���� �̾߱��ϸ�, 'Subquery�� ���� �ٷ� �� Join Tree������' outerFrom�� �ȴ�. 
 *  ���� Join�� outerFrom�� �� �� ���ٴ� ���̴�. (��������, T5�� ������ �ʴ´�)
 *
 *  =====================================================================
 *
 *  outerFrom ����� Lateral View���� �״�� �����ؾ� �Ѵ�.
 *  
 *   - Lateral View�� �ܵ����� ���δٸ�, outerQuery�� �����Ѵ�.
 *   - Lateral View�� ANSI-Join Tree �ȿ� �����Ѵٸ� 
 *     'Lateral View�� ���� �ٷ� �� Join Tree ������' outerFrom�� �����Ѵ�.
 *
 *  (����) outerQuery�� ���� outerQuery�� Ž���ϱ� ���� �뵵�ε� ���ǹǷ�,
 *         outerFrom�� Ž���Ѵٰ� �ؼ� outerQuery�� �������� ������ �� �ȴ�.  
 *
 *
 * Implementation :
 *
 *  - Set Operator�� �����ϴ� ��쿡�� LEFT/RIGHT ���ȣ���Ѵ�.
 *  - ���� QuerySet�� ���, outerQuery�� �����ϰ� outerFrom�� �� qmsFrom�� ã�´�.
 *  - ã�� qmsFrom�� outerFrom���� �����Ѵ�. (ã�� ���� NULL�� ���� �ִ�.)
 *
 ***********************************************************************/

    qmsFrom * sFrom             = NULL;
    qmsFrom * sOuterFrom        = NULL;

    IDE_DASSERT( aViewQuerySet != NULL );

    if ( aViewQuerySet->setOp == QMS_NONE )
    {
        // Set Operation�� ���� ���
        if ( aViewQuerySet->SFWGH != NULL )
        {
            // outerQuery ����
            IDE_DASSERT( aViewQuerySet->SFWGH->outerQuery == NULL );
            aViewQuerySet->SFWGH->outerQuery = aOuterSFWGH;

            // outerFrom ����
            for ( sFrom = aOuterSFWGH->from;
                  ( sFrom != NULL ) && ( sOuterFrom == NULL );
                  sFrom = sFrom->next )
            {
                if ( sFrom->joinType != QMS_NO_JOIN )
                {
                    getLateralOuterFrom( aViewQuerySet,
                                         aViewTableRef,
                                         sFrom,
                                         & sOuterFrom );
                }
                else
                {
                    // Nothing to do.
                }
            }
            aViewQuerySet->SFWGH->outerFrom = sOuterFrom;
        }
        else
        {
            // Nothing to do.
        }
    }
    else
    {
        // Set Operation�� ���
        setLateralOuterQueryAndFrom( aViewQuerySet->left,  
                                     aViewTableRef, 
                                     aOuterSFWGH );

        setLateralOuterQueryAndFrom( aViewQuerySet->right,  
                                     aViewTableRef, 
                                     aOuterSFWGH );
    }

}

void qmvQuerySet::getLateralOuterFrom( qmsQuerySet  * aViewQuerySet, 
                                       qmsTableRef  * aViewTableRef, 
                                       qmsFrom      * aFrom,
                                       qmsFrom     ** aOuterFrom )
{
/***********************************************************************
 *
 * Description : PROJ-2418 Cross/Outer APPLY & Lateral View
 * 
 *  Lateral View�� ANSI Join Tree�� ���� ���,
 *  Lateral View�� ���� �ٷ� �� Join Tree�� ã�� outerFrom���� ��ȯ�Ѵ�.
 *
 * Implementation :
 *  
 *  Lateral View�� ��Ÿ���� TableRef�� ���� qmsFrom�� 
 *  ���� ������ �ΰ� �ִ� qmsFrom�� LEFT/RIGHT�� ���� �ִٸ�, 
 *  ���� qmsFrom�� aOuterFrom�� �ȴ�.
 *  
 *  Join Tree�� ���� ������ LEFT/RIGHT�� ���� ���ȣ���Ѵ�.
 *
 ***********************************************************************/

    IDE_DASSERT( aFrom->left  != NULL );
    IDE_DASSERT( aFrom->right != NULL );

    // Left���� Ž��
    if ( aFrom->left->joinType != QMS_NO_JOIN )
    {
        getLateralOuterFrom( aViewQuerySet,
                             aViewTableRef,
                             aFrom->left,
                             aOuterFrom );
    }
    else
    {
        if ( aFrom->left->tableRef == aViewTableRef )
        {
            *aOuterFrom = aFrom;
        }
        else
        {
            // Nothing to do.
        }
    }

    // Left���� ã�� ���ߴٸ� Right���� Ž��
    if ( *aOuterFrom == NULL ) 
    {
        if ( aFrom->right->joinType != QMS_NO_JOIN )
        {
            getLateralOuterFrom( aViewQuerySet,
                                 aViewTableRef,
                                 aFrom->right,
                                 aOuterFrom );
        }
        else
        {
            if ( aFrom->right->tableRef == aViewTableRef )
            {
                *aOuterFrom = aFrom;
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

}

IDE_RC qmvQuerySet::addCastFuncForNode( qcStatement  * aStatement,
                                        qtcNode      * aNode,
                                        mtcColumn    * aCastType,
                                        qtcNode     ** aNewNode )
{
    qtcNode    * sNode[2];
    qtcNode    * sArgNode[2];
    mtcColumn  * sMtcColumn = NULL;

    IDU_FIT_POINT_FATAL( "qmvQuerySet::addCastFuncForNode::__FT__" );

    // cast �Լ��� �����.
    IDE_TEST( qtc::makeNode( aStatement,
                             sNode,
                             & aNode->columnName,
                             & mtfCast )
              != IDE_SUCCESS );

    // cast ���ڸ� �����.
    IDE_TEST( qtc::makeNode( aStatement,
                             sArgNode,
                             & aNode->columnName,
                             & qtc::valueModule )
              != IDE_SUCCESS );

    sMtcColumn = QTC_STMT_COLUMN( aStatement, sArgNode[0] );

    mtc::copyColumn( sMtcColumn, aCastType );

    // �Լ��� �����Ѵ�.
    sNode[0]->node.arguments = (mtcNode*) aNode;
    sNode[0]->node.funcArguments = (mtcNode*) sArgNode[0];
    sNode[0]->node.next = aNode->node.next;
    sNode[0]->node.arguments->next = NULL;

    // �Լ��� estimate�� �����Ѵ�.
    IDE_TEST( qtc::estimateNodeWithArgument( aStatement,
                                             sNode[0] )
              != IDE_SUCCESS );

    *aNewNode = sNode[0];

    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmvQuerySet::searchStartWithPrior( qcStatement * aStatement,
                                          qtcNode     * aNode )
{
    qcuSqlSourceInfo  sSqlInfo;

    if ( ( ( aNode->lflag & QTC_NODE_PRIOR_MASK ) ==
           QTC_NODE_PRIOR_EXIST ) ||
         ( QTC_IS_AGGREGATE( aNode ) == ID_TRUE ) )
    {
        sSqlInfo.setSourceInfo( aStatement,
                                &aNode->position );
        IDE_RAISE( ERR_NOT_SUPPORTED );
    }
    else
    {
        /* Nothing to do */
    }

    if ( aNode->node.arguments != NULL )
    {
        if ( QTC_IS_SUBQUERY( (qtcNode *)aNode->node.arguments ) == ID_FALSE )
        {
            IDE_TEST( searchStartWithPrior( aStatement, (qtcNode *)aNode->node.arguments )
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

    if ( aNode->node.next != NULL )
    {
        if ( QTC_IS_SUBQUERY( (qtcNode *)aNode->node.next ) == ID_FALSE )
        {
            IDE_TEST( searchStartWithPrior( aStatement, (qtcNode *)aNode->node.next )
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
    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NOT_SUPPORTED );
    {
        (void)sSqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QSX_NOT_SUPPORTED_SQLTEXT,
                            sSqlInfo.getErrMessage() ));
        (void)sSqlInfo.fini();
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* BUG-45742 connect by aggregation function fatal */
IDE_RC qmvQuerySet::searchConnectByAggr( qcStatement * aStatement,
                                         qtcNode     * aNode )
{
    qcuSqlSourceInfo  sSqlInfo;

    if ( QTC_IS_AGGREGATE(aNode) == ID_TRUE )
    {
        sSqlInfo.setSourceInfo( aStatement,
                                &aNode->position );
        IDE_RAISE( ERR_NOT_SUPPORTED );
    }
    else
    {
        /* Nothing to do */
    }

    if ( aNode->node.arguments != NULL )
    {
        if ( QTC_IS_SUBQUERY( (qtcNode *)aNode->node.arguments ) == ID_FALSE )
        {
            IDE_TEST( searchConnectByAggr( aStatement, (qtcNode *)aNode->node.arguments )
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

    if ( aNode->node.next != NULL )
    {
        if ( QTC_IS_SUBQUERY( (qtcNode *)aNode->node.next ) == ID_FALSE )
        {
            IDE_TEST( searchConnectByAggr( aStatement, (qtcNode *)aNode->node.next )
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
    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NOT_SUPPORTED );
    {
        (void)sSqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QSX_NOT_SUPPORTED_SQLTEXT,
                            sSqlInfo.getErrMessage() ));
        (void)sSqlInfo.fini();
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmvQuerySet::convertAnsiInnerJoin( qcStatement * aStatement,
                                          qmsSFWGH    * aSFWGH )
{
    qmsFrom  * sFrom = NULL;
    idBool     sOnlyInnerJoin = ID_FALSE;

    /* TASK-7219
     *   Shard Transform �� ������� Query Text ������� ����Ǵµ�,
     *   ANSI ��Ÿ�Ͽ� �Ϲ� ��Ÿ���� ���̰� �ſ� ũ�Ƿ�,
     *   �ش� ��ȯ�� �������� �ʵ��� �Ѵ�.
     *
     *   �׸��� �������� Query�� ��������, ���������� �ش� ��ȯ�� ������ ������ ����Ѵ�.
     */
    IDE_TEST_CONT( ( ( aStatement->myPlan->parseTree->stmtShard != QC_STMT_SHARD_NONE )
                     &&
                     ( aStatement->myPlan->parseTree->stmtShard != QC_STMT_SHARD_META ) ),
                   NORMAL_EXIT );

    // BUG-38402
    if ( QCU_OPTIMIZER_ANSI_INNER_JOIN_CONVERT == 1 )
    {
        // BUG-38273
        // ansi ��Ÿ�� inner join �� �Ϲ� ��Ÿ���� �������� �����Ѵ�.
        for (sFrom = aSFWGH->from; sFrom != NULL; sFrom = sFrom->next)
        {
            if ( sFrom->joinType == QMS_NO_JOIN )
            {
                sOnlyInnerJoin = ID_TRUE;
            }
            else if ( sFrom->joinType == QMS_INNER_JOIN )
            {
                sOnlyInnerJoin = checkInnerJoin( sFrom );
            }
            else
            {
                sOnlyInnerJoin = ID_FALSE;
            }

            if ( sOnlyInnerJoin == ID_FALSE )
            {
                break;
            }
            else
            {
                // Nothing to do.
            }
        }

        if ( sOnlyInnerJoin == ID_TRUE )
        {
            qcgPlan::registerPlanProperty(
                aStatement,
                PLAN_PROPERTY_OPTIMIZER_ANSI_INNER_JOIN_CONVERT );

            for ( sFrom = aSFWGH->from; sFrom != NULL; sFrom = sFrom->next )
            {
                if ( sFrom->joinType == QMS_INNER_JOIN )
                {
                    IDE_TEST( innerJoinToNoJoin( aStatement,
                                                 aSFWGH,
                                                 sFrom ) != IDE_SUCCESS);
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
    }
    else
    {
        // Nothing to do.
    }

    /* TASK-7219 */
    IDE_EXCEPTION_CONT( NORMAL_EXIT );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

