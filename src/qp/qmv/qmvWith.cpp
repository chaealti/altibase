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
#include <qc.h>
#include <qcm.h>
#include <qmv.h>
#include <qmsParseTree.h>
#include <qcpManager.h>
#include <qmvWith.h>
#include <qcuSqlSourceInfo.h>
#include <sdi.h> /* TASK-7219 Shard Transformer Refactoring */

/*
 withStmt ����� ����

    with q1 as (select 1 from dual),
         q2 as (select 2 from dual),
         q3 as (select 3 from dual)
      select * from
    (
      with q21 as (select 21 from dual),
           q22 as (select 22 from dual),
           q23 as (select 23 from dual)
      select * from
      (
        with q31 as (select 31 from dual),
             q32 as (select 32 from dual),
             q33 as (select 33 from dual)
        select * from q3
      )
    );

 ���� ���� ������ ������ ���� withStmt ��尡 �����ȴ�.

                                      ( head (backup) null)
                                        ^
                                        |
                             q1-->q2-->q3                  <----�ֻ��� WITH
                             ^
                             |
                q21-->q22-->q23                            <----from���� WITH
                ^
                |
    q31-->q32-->q33                                        <----

    qmvWith::validate() head �����͸� ���� ���� �ؼ� ��带 �߰��Ѵ�.
    with ���� ���� ���� �ɶ� �� �׸� ó�� ���� �����Ͱ� �޶�����.
    validate()�Լ��� �ϴ� �κп� ������ �߰��� ����� next�� ���� head�� ������ �ش�.

    qmvWith::parseViewInTableRef()�Լ��� �־��� tablename�� �� ��� ����Ʈ�� head�� ����
    Ž�� �ϰ� �ȴ�.

    qmv::parseSelect()�Լ����� ���� ���������� ���� �Ҷ� ����� ���� �����͸� ��� �ϰ�
    �Լ� ������ ���� ���־� ���� ������ �Ϸ� �Ǹ� ���Խ� ������ �ȴ�.

    ex)
    select * from
    (
      with q1 as (select i1, i2 from t1),
        q2 as (select i1, i2 from t2)
      select * from
      (
        with q1 as (select i1 from t1)
        select * from q2
      )
    ) v1,
    (
      with q3 as (select i1, i2 from t1),
        q4 as (select i1, i2 from t2)
      select * from
      (
        with q1 as (select i1 from t1)
        select * from q2
      )
    ) v2;

                           head (backup) --> null
                           ^  ^
                           |  |
                    q1 --> q2 |
                     ^        |
                     |        |
                    q1        |
                              |
                       q3 --> q4
                        ^
                        |
                       q1
    q1 --> q1 --> q2 ã��

    q1 --> q3 --> q4 --> top --> null
    q2�� ã�� ����
*/
IDE_RC qmvWith::validate( qcStatement * aStatement )
{
    qcStmtListMgr     * sStmtListMgr; /* BUG-45994 - �����Ϸ� ����ȭ ȸ�� */
    qmsWithClause     * sWithClause;
    qmsParseTree      * sParseTree;
    qmsParseTree      * sViewParseTree = NULL;
    qcWithStmt        * sPrevHead = NULL;
    qcWithStmt        * sNewNode  = NULL;
    qcWithStmt        * sCurNode  = NULL;
    idBool              sFirstQueryBlcok = ID_TRUE;
    idBool              sOrgPSMFlag = ID_FALSE; /* TAKS-7219 Shard Transformer Refactoring */
    idBool              sIsChanged  = ID_FALSE; /* TAKS-7219 Shard Transformer Refactoring */

    IDE_FT_BEGIN();

    IDU_FIT_POINT_FATAL( "qmvWith::validate::__FT__" );

    IDE_FT_ASSERT( aStatement != NULL );

    sParseTree = (qmsParseTree*)aStatement->myPlan->parseTree;
    sStmtListMgr = aStatement->myPlan->stmtListMgr;

    /* BUG-45994 */
    IDU_FIT_POINT_FATAL( "qmvWith::validate::__FT__::STAGE1" );

    // codesonar::Null Pointer Dereference
    IDE_FT_ERROR( sStmtListMgr != NULL );

    if ( sParseTree->withClause != NULL )
    {
        /* TASK-7219 Shard Transformer Refactoring */
        IDE_TEST( sdi::setQuerySetListState( aStatement,
                                             NULL, /* aParseTree */
                                             &( sIsChanged ) )
              != IDE_SUCCESS );

        sPrevHead = sStmtListMgr->head;

        for ( sWithClause =  sParseTree->withClause;
              sWithClause != NULL;
              sWithClause =  sWithClause->next )
        {
            /* make new node */
            IDE_TEST( makeWithStmtFromWithClause( aStatement,
                                                  sStmtListMgr,
                                                  sWithClause,
                                                  &sNewNode )
                      != IDE_SUCCESS );

            // recursive with�� �˻��ϱ� ���� ���� stmt�� ����Ѵ�.
            sStmtListMgr->current = sNewNode;

            /* TASK-7219 Shard Transformer Refactoring */
            IDE_TEST( qmv::parseSelectInternal( sWithClause->stmt )
                      != IDE_SUCCESS );

            IDE_TEST( sdi::setStatementFlagForShard( aStatement,
                                                     sWithClause->stmt->mFlag )
                      != IDE_SUCCESS );

            if ( sNewNode->isRecursiveView == ID_TRUE )
            {
                sViewParseTree = (qmsParseTree*) sWithClause->stmt->myPlan->parseTree;
                
                // union all�� �ƴϸ� ����
                if ( sViewParseTree->querySet->setOp != QMS_UNION_ALL )
                {
                    IDE_RAISE( ERR_NOT_NON_EXIST_UNION_ALL_RECURSIVE_VIEW );
                }
                else
                {
                    if ( ( sViewParseTree->querySet->left->setOp != QMS_NONE ) ||
                         ( sViewParseTree->querySet->right->setOp != QMS_NONE ) )
                    {
                        IDE_RAISE( ERR_NOT_MULTI_SET_RECURSIVE_VIEW );
                    }
                    else
                    {
                        // Nothing to do.
                    }
                }
                
                sViewParseTree->querySet->lflag &= ~QMV_QUERYSET_RECURSIVE_VIEW_MASK;
                sViewParseTree->querySet->lflag |= QMV_QUERYSET_RECURSIVE_VIEW_TOP;
            }
            else
            {
                // Nothing to do.
            }

            /* TASK-7219 Shard Transformer Refactoring */
            sOrgPSMFlag                        = sWithClause->stmt->calledByPSMFlag;
            sWithClause->stmt->calledByPSMFlag = aStatement->calledByPSMFlag;

            IDE_TEST( qmv::validateSelect( sWithClause->stmt )
                      != IDE_SUCCESS );

            /* TASK-7219 Shard Transformer Refactoring */
            sWithClause->stmt->calledByPSMFlag = sOrgPSMFlag;

            sStmtListMgr->current = NULL;
            
            // column alias
            IDE_TEST( validateColumnAlias( sWithClause )
                      != IDE_SUCCESS );
            
            /* ���� ���� ���� */
            sNewNode->next = sPrevHead;

            if ( sFirstQueryBlcok == ID_TRUE )
            {
                /* ���� �����ÿ��� ��忡 ���δ�. */
                sStmtListMgr->head     = sNewNode;
                sCurNode               = sNewNode;
                sFirstQueryBlcok       = ID_FALSE;
            }
            else
            {
                /* ���Ŀ��� �ڿ� ���δ�. */
                sCurNode->next = sNewNode;
                sCurNode       = sNewNode;
            }
        }

        /* TASK-7219 Shard Transformer Refactoring */
        IDE_TEST( sdi::unsetQuerySetListState( aStatement,
                                               sIsChanged )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do. */
    }

    IDE_FT_END();

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NOT_NON_EXIST_UNION_ALL_RECURSIVE_VIEW )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMV_NON_EXIST_UNION_ALL_RECURSIVE_VIEW ) );
    }
    IDE_EXCEPTION( ERR_NOT_MULTI_SET_RECURSIVE_VIEW )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMV_MULTI_SET_RECURSIVE_VIEW ) );
    }
    IDE_EXCEPTION_SIGNAL()
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_FAULT_TOLERATED ) );
    }
    IDE_EXCEPTION_END;

    IDE_FT_EXCEPTION_BEGIN();

    if ( sStmtListMgr != NULL )
    {
        sStmtListMgr->current = NULL;
    }
    else
    {
        // Nothing to do.
    }

    IDE_FT_EXCEPTION_END();

    return IDE_FAILURE;
}


IDE_RC qmvWith::parseViewInTableRef( qcStatement * aStatement,
                                     qmsTableRef * aTableRef )
{
    qcWithStmt     * sCurWithStmt;
    idBool           sIsFound =  ID_FALSE;
    qmsQuerySet    * sQuerySet = NULL;

    IDU_FIT_POINT_FATAL( "qmvWith::parseViewInTableRef::__FT__" );

    IDE_FT_ASSERT( aStatement != NULL );

    while ( 1 )
    {
        //-----------------------------------
        // with subquery
        //-----------------------------------
        
        sCurWithStmt = aStatement->myPlan->stmtListMgr->head;

        while ( sCurWithStmt != NULL )
        {
            if ( QC_IS_NAME_MATCHED( aTableRef->tableName,
                                     sCurWithStmt->stmtName ) == ID_TRUE )
            {
                sIsFound = ID_TRUE;
                break;
            }
            else
            {
                /* Nothing to do */
            }

            sCurWithStmt = sCurWithStmt->next;
        }

        if ( sIsFound == ID_TRUE )
        {
            if ( sCurWithStmt->isRecursiveView == ID_TRUE )
            {
                if ( sCurWithStmt->isTop == ID_TRUE )
                {
                    // �ֻ��� recursive view
                    IDE_TEST( makeParseTree( aStatement,
                                             aTableRef,
                                             sCurWithStmt )
                              != IDE_SUCCESS );

                    // view�� recursive view�� ����
                    aTableRef->recursiveView = aTableRef->view;

                    IDE_TEST( makeParseTree( aStatement,
                                             aTableRef,
                                             sCurWithStmt )
                              != IDE_SUCCESS );

                    // view�� recursive view�� ���� (���ȣ��� �ӽ�)
                    aTableRef->tempRecursiveView = aTableRef->view;
                    
                    aTableRef->view = NULL;

                    /* BUG-46932 from ���� recursive with view�� ����ϰ� �ִ� */
                    sQuerySet = ((qmsParseTree*)aStatement->myPlan->parseTree)->querySet;
                    sQuerySet->lflag &= ~QMV_QUERYSET_FROM_RECURSIVE_WITH_MASK;
                    sQuerySet->lflag |= QMV_QUERYSET_FROM_RECURSIVE_WITH_TRUE;

                    // ���� ���ʹ� ���� recursive view�̴�.
                    sCurWithStmt->isTop = ID_FALSE;
                }
                else
                {
                    // ���� recursive view
                    // Nothing to do.
                }
                
                // recursive view�� ���
                aTableRef->flag &= ~QMS_TABLE_REF_RECURSIVE_VIEW_MASK;
                aTableRef->flag |= QMS_TABLE_REF_RECURSIVE_VIEW_TRUE;
            }
            else
            {
                IDE_TEST( makeParseTree( aStatement,
                                         aTableRef,
                                         sCurWithStmt )
                          != IDE_SUCCESS );
            }

            // ã�� withStmtList�� �Ҵ� �Ѵ�.
            aTableRef->withStmt = sCurWithStmt;
            
            // BUG-48090 aTableRef�� With View �÷��׸� �����Ѵ�.
            aTableRef->flag &= ~QMS_TABLE_REF_WITH_VIEW_MASK;
            aTableRef->flag |= QMS_TABLE_REF_WITH_VIEW_TRUE;
            break;
        }
        else
        {
            // Nothing to do.
        }

        //-----------------------------------
        // PROJ-2582 recursive with
        //-----------------------------------
        
        sCurWithStmt = aStatement->myPlan->stmtListMgr->current;
        
        if ( sCurWithStmt != NULL )
        {
            if ( QC_IS_NAME_MATCHED( aTableRef->tableName,
                                     sCurWithStmt->stmtName ) == ID_TRUE )
            {
                sIsFound = ID_TRUE;
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
        
        if ( sIsFound == ID_TRUE )
        {
            // recursive with�̴�.
            sCurWithStmt->isRecursiveView = ID_TRUE;
            // ������ �ֻ��� recursive view
            sCurWithStmt->isTop = ID_TRUE;
            
            aTableRef->flag &= ~QMS_TABLE_REF_RECURSIVE_VIEW_MASK;
            aTableRef->flag |= QMS_TABLE_REF_RECURSIVE_VIEW_TRUE;
            
            // ã�� withStmtList�� �Ҵ� �Ѵ�.
            aTableRef->withStmt = sCurWithStmt;
            break;
        }
        else
        {
            // Nothing to do.
        }

        // ��ã�� ���
        aTableRef->withStmt = NULL;
        break;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
 * with clause�� �̿��ؼ� with stmt�� ���� ���� �Ѵ�.
 */
IDE_RC qmvWith::makeWithStmtFromWithClause( qcStatement        * aStatement,
                                            qcStmtListMgr      * aStmtListMgr,
                                            qmsWithClause      * aWithClause,
                                            qcWithStmt        ** aNewNode )
{
    qcWithStmt * sNewNode;
    UInt         sNextTableID;

    IDU_FIT_POINT_FATAL( "qmvWith::makeWithStmtFromWithClause::__FT__" );

    IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(qcWithStmt),
                                             (void**)&sNewNode )
              != IDE_SUCCESS );

    /* data setting */
    SET_POSITION( sNewNode->stmtName, aWithClause->stmtName );
    SET_POSITION( sNewNode->stmtText, aWithClause->stmtText );
    sNewNode->columns         = aWithClause->columns;
    sNewNode->stmt            = aWithClause->stmt;
    sNewNode->isRecursiveView = ID_FALSE;
    sNewNode->isTop           = ID_FALSE;
    sNewNode->tableInfo       = NULL;
    sNewNode->next            = NULL;

    sNextTableID = (UInt)(aStmtListMgr->tableIDSeqNext + QCM_WITH_TABLES_SEQ_MINVALUE );

    /* tableID boundary check */
    IDE_TEST_RAISE( sNextTableID >= UINT_MAX, ERR_EXCEED_TABLEID );

    sNewNode->tableID = sNextTableID;

    /* tableID sequence ���� */
    aStmtListMgr->tableIDSeqNext++;

    *aNewNode = sNewNode;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_EXCEED_TABLEID )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmvWith::makeWithStmtFromWithClause",
                                  "Invalid tableIDSeqNext" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
 * with stmt list ���� ���� stmtText�� �̿��ؼ� parse tree�� �����Ѵ�.
 * 1. stmtText�� �̿��ؼ� parse tree�� �����.
 * 2. aTableRef->view�� statement�� �Ҵ��ؼ� inline ��� ������ش�.
 * 3. stmtName���� alias�� �����Ѵ�.
 */
IDE_RC qmvWith::makeParseTree( qcStatement * aStatement,
                               qmsTableRef * aTableRef,
                               qcWithStmt  * aWithStmt )
{
    qcStatement * sStatement;

    IDU_FIT_POINT_FATAL( "qmvWith::makeParseTree::__FT__" );

    IDE_TEST( STRUCT_ALLOC( QC_QMP_MEM(aStatement), qcStatement, &sStatement )
              != IDE_SUCCESS);

    // set meber of qcStatement
    idlOS::memcpy( sStatement, aStatement, ID_SIZEOF(qcStatement) );

    /* TASK-7219 */
    SDI_INIT_PRINT_INFO( &( sStatement->mShardPrintInfo ) );

    // myPlan�� �缳���Ѵ�.
    sStatement->myPlan = &sStatement->privatePlan;
    sStatement->myPlan->planEnv = NULL;

    sStatement->myPlan->parseTree   = NULL;
    sStatement->myPlan->plan        = NULL;

    sStatement->myPlan->stmtText    = aWithStmt->stmtText.stmtText;
    sStatement->myPlan->stmtTextLen = idlOS::strlen( aWithStmt->stmtText.stmtText );

    // parsing view    
    IDE_TEST( qcpManager::parsePartialForQuerySet( sStatement,
                                                   aWithStmt->stmtText.stmtText,
                                                   aWithStmt->stmtText.offset,
                                                   aWithStmt->stmtText.size )
              != IDE_SUCCESS );

    // set parse tree
    aTableRef->view = sStatement;

    // alias name setting
    if ( QC_IS_NULL_NAME( aTableRef->aliasName ) == ID_TRUE )
    {
        SET_POSITION( aTableRef->aliasName, aTableRef->tableName );
    }
    else
    {
        /* Nothing to do. */
    }

    // planEnv�� �缳���Ѵ�.
    aTableRef->view->myPlan->planEnv = aStatement->myPlan->planEnv;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmvWith::validateColumnAlias( qmsWithClause  * aWithClause )
{
    qmsParseTree * sParseTree = NULL;
    qmsTarget    * sTarget = NULL;
    qcmColumn    * sColumn = NULL;
    SInt           sColumnCount = 0;

    IDU_FIT_POINT_FATAL( "qmvWith::validateColumnAlias::__FT__" );

    sParseTree = (qmsParseTree *)aWithClause->stmt->myPlan->parseTree;
    
    if ( aWithClause->columns != NULL )
    {
        // columns�� ������ target�� ������ �����ؾ� �Ѵ�.
        for ( sColumn = aWithClause->columns,
                  sTarget = sParseTree->querySet->target;
              sColumn != NULL;
              sColumn = sColumn->next, sTarget = sTarget->next )
        {
            IDE_TEST_RAISE( sTarget == NULL, ERR_MISMATCH_NUMBER_OF_COLUMN );
            
            sColumnCount++;
        }

        IDE_TEST_RAISE( sTarget != NULL, ERR_MISMATCH_NUMBER_OF_COLUMN );
    }
    else
    {
        // with clause target column count
        for ( sTarget = sParseTree->querySet->target;
              sTarget != NULL;
              sTarget = sTarget->next )
        {
            sColumnCount++;
        }
    }
    
    IDE_TEST_RAISE( sColumnCount > QC_MAX_COLUMN_COUNT,
                    ERR_INVALID_COLUMN_COUNT );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_MISMATCH_NUMBER_OF_COLUMN )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDB_MISMATCH_COL_COUNT ) );
    }
    IDE_EXCEPTION( ERR_INVALID_COLUMN_COUNT )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDB_INVALID_COLUMN_COUNT ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
