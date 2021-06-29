/*
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
 *  the GNU Affero General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

/***********************************************************************
 * $Id: sdo.cpp 90252 2021-03-18 06:46:29Z andrew.shin $
 **********************************************************************/

#include <sda.h>
#include <sdi.h>
#include <sdo.h>
#include <qcg.h>
#include <qmo.h>
#include <qsvEnv.h>

IDE_RC sdo::allocAndInitQuerySetList( qcStatement * aStatement )
{
/***********************************************************************
 *
 * Description : TASK-7219 Shard Transformer Refactoring
 *                Query Set List �ʱ�ȭ �Լ�
 *
 *                 Query Set List ���� ���� Query String ���� ������,
 *                  Statement �� �����ϴ� ��� Query Set �� ������ �����̴�.
 *
 * Implementation :
 *
 ***********************************************************************/

    sdiQuerySetList * sList = NULL;

    IDE_TEST_RAISE( aStatement == NULL, ERR_NULL_STATEMENT );

    IDE_TEST( QC_QMP_MEM( aStatement )->alloc( ID_SIZEOF( sdiQuerySetList ),
                                               (void **)&( sList ) )
              != IDE_SUCCESS );

    sList->mState = SDI_QUERYSET_LIST_STATE_MAIN_MAKE;
    sList->mCount = 0;
    sList->mHead  = NULL;
    sList->mTail  = NULL;

    aStatement->mShardQuerySetList = sList;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NULL_STATEMENT )
    {
        IDE_SET( ideSetErrorCode( sdERR_ABORT_SDC_UNEXPECTED_ERROR,
                                  "sdo::allocAndInitQuerySetList",
                                  "statement is NULL" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC sdo::makeAndSetQuerySetList( qcStatement * aStatement,
                                    qmsQuerySet * aQuerySet )
{
/***********************************************************************
 *
 * Description : TASK-7219 Shard Transformer Refactoring
 *                Query Set List ���� �Լ�
 *
 *                 List �� Parse �ܰ迡�� �����Ǹ�, ��쿡 ���� ������ �ٸ���.
 *                  1. With ���� ������ Query Set �� �������� ����
 *                  2. Shard Keyword ����� Query Set �� �������� ����
 *
 *                   ���� ������ �Լ�
 *                    - qmv::parseViewInQuerySet
 *
 * Implementation : 1. SDI_QUERYSET_LIST_STATE_MAIN_SKIP, DUMMY_SKIP �ܰ迡�� �������� �����Ѵ�.
 *                  2. Query Set List �� �����Ҵ�, �ʱ�ȭ �Ѵ�.
 *                  3. List �� �����Ѵ�.
 *
 ***********************************************************************/

    sdiQuerySetEntry * sEntry = NULL;
    sdiQuerySetList  * sList  = NULL;

    IDE_TEST_RAISE( aStatement == NULL, ERR_NULL_STATEMENT );
    IDE_TEST_RAISE( aQuerySet == NULL, ERR_NULL_QUERYSET );
    IDE_TEST_RAISE( aStatement->mShardQuerySetList == NULL, ERR_NULL_LIST );

    sList = aStatement->mShardQuerySetList;

    /* 1. SDI_QUERYSET_LIST_STATE_MAIN_SKIP, DUMMY_SKIP �ܰ迡�� �������� �����Ѵ�. */
    IDE_TEST_CONT( ( SDI_CHECK_QUERYSET_LIST_STATE( sList,
                                                    SDI_QUERYSET_LIST_STATE_MAIN_SKIP )
                     == ID_TRUE )
                   ||
                   ( SDI_CHECK_QUERYSET_LIST_STATE( sList,
                                                    SDI_QUERYSET_LIST_STATE_DUMMY_SKIP )
                     == ID_TRUE ),
                   NORMAL_EXIT );

    /* 2. Query Set List �� �����Ҵ�, �ʱ�ȭ �Ѵ�. */
    IDE_TEST( QC_QMP_MEM( aStatement )->alloc( ID_SIZEOF( sdiQuerySetEntry ),
                                               (void **)&( sEntry ) )
              != IDE_SUCCESS );

    sEntry->mId       = sList->mCount;
    sEntry->mQuerySet = aQuerySet;
    sEntry->mNext     = NULL;
    sList->mCount     = sList->mCount + 1;

    /* 3. List �� �����Ѵ�. */
    if ( sList->mTail == NULL )
    {
        sList->mHead = sEntry;
        sList->mTail = sEntry;
    }
    else
    {
        sList->mTail->mNext = sEntry;
        sList->mTail        = sEntry;
    }

    IDE_EXCEPTION_CONT( NORMAL_EXIT );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NULL_STATEMENT )
    {
        IDE_SET( ideSetErrorCode( sdERR_ABORT_SDC_UNEXPECTED_ERROR,
                                  "sdo::makeAndSetQuerySetList",
                                  "statement is NULL" ) );
    }
    IDE_EXCEPTION( ERR_NULL_QUERYSET )
    {
        IDE_SET( ideSetErrorCode( sdERR_ABORT_SDC_UNEXPECTED_ERROR,
                                  "sdo::makeAndSetQuerySetList",
                                  "queryset is NULL" ) );
    }
    IDE_EXCEPTION( ERR_NULL_LIST )
    {
        IDE_SET( ideSetErrorCode( sdERR_ABORT_SDC_UNEXPECTED_ERROR,
                                  "sdo::makeAndSetQuerySetList",
                                  "list is NULL" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC sdo::setQuerySetListState( qcStatement  * aStatement,
                                  qmsParseTree * aParseTree,
                                  idBool       * aIsChanged )
{
/***********************************************************************
 *
 * Description : TASK-7219 Shard Transformer Refactoring
 *                Query Set List ���¸� SKIP �ܰ�� �����Ѵ�.
 *
 * Implementation : 1. MAKE �ܰ迡�� ������ �����ϴ�.
 *                  2. Shard Keyword ����� �˻��Ѵ�.
 *                  3. SKIP �ܰ�� �����Ѵ�.
 *
 ***********************************************************************/

    qcShardStmtType      sStmtType  = QC_STMT_SHARD_NONE;
    sdiQuerySetList    * sList      = NULL;
    sdiQuerySetListState sState     = SDI_QUERYSET_LIST_STATE_MAX;
    idBool               sIsChanged = ID_FALSE;
    idBool               sNeedCheck = ID_FALSE;

    IDE_TEST_RAISE( aStatement == NULL, ERR_NULL_STATEMENT );
    IDE_TEST_RAISE( aStatement->mShardQuerySetList == NULL, ERR_NULL_LIST );

    sList = aStatement->mShardQuerySetList;

    /* 1. MAKE �ܰ迡�� ������ �����ϴ�. */
    if ( SDI_CHECK_QUERYSET_LIST_STATE( sList,
                                        SDI_QUERYSET_LIST_STATE_MAIN_MAKE )
         == ID_TRUE )
    {
        sNeedCheck = ID_TRUE;
        sState     = SDI_QUERYSET_LIST_STATE_MAIN_SKIP;
    }
    else if ( SDI_CHECK_QUERYSET_LIST_STATE( sList,
                                             SDI_QUERYSET_LIST_STATE_DUMMY_MAKE )
              == ID_TRUE )
    {
        sNeedCheck = ID_TRUE;
        sState     = SDI_QUERYSET_LIST_STATE_DUMMY_SKIP;
    }
    else
    {
        /* Nothing to do */
    }

    if ( sNeedCheck == ID_TRUE )
    {
        if ( aParseTree != NULL )
        {
            sStmtType  = aParseTree->common.stmtShard;

            /* 2. Shard Keyword ����� �˻��Ѵ�. */
            if ( ( sStmtType == QC_STMT_SHARD_DATA )
                 ||
                 ( sStmtType == QC_STMT_SHARD_META ) )
            {
                /* 3. SKIP �ܰ�� �����Ѵ�. */
                SDI_SET_QUERYSET_LIST_STATE( sList, sState );

                sIsChanged = ID_TRUE;
            }
            else
            {
                /* Nothing to do */
            }
        }
        else
        {
            /* 3. SKIP �ܰ�� �����Ѵ�. */
            SDI_SET_QUERYSET_LIST_STATE( sList, sState );

            sIsChanged = ID_TRUE;
        }
    }
    else
    {
        /* Nothing to do */
    }

    if ( aIsChanged != NULL )
    {
        *aIsChanged = sIsChanged;
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NULL_STATEMENT )
    {
        IDE_SET( ideSetErrorCode( sdERR_ABORT_SDC_UNEXPECTED_ERROR,
                                  "sdo::setQuerySetListState",
                                  "statement is NULL" ) );
    }
    IDE_EXCEPTION( ERR_NULL_LIST )
    {
        IDE_SET( ideSetErrorCode( sdERR_ABORT_SDC_UNEXPECTED_ERROR,
                                  "sdo::setQuerySetListState",
                                  "list is NULL" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC sdo::unsetQuerySetListState( qcStatement * aStatement,
                                    idBool        aIsChanged )
{
/***********************************************************************
 *
 * Description : TASK-7219 Shard Transformer Refactoring
 *                Query Set List ������ Keyword �� ����
 *
 * Implementation : 1. SKIP �ܰ迡�� ������ �����ϴ�.
 *                  2. MAKE �ܰ�� �����Ѵ�.
 *
 ***********************************************************************/

    sdiQuerySetList * sList = NULL;

    IDE_TEST_RAISE( aStatement == NULL, ERR_NULL_STATEMENT );
    IDE_TEST_RAISE( aStatement->mShardQuerySetList == NULL, ERR_NULL_LIST );

    if ( aIsChanged == ID_TRUE )
    {
        sList = aStatement->mShardQuerySetList;

        /* 1. SKIP �ܰ迡�� ������ �����ϴ�. */
        if ( SDI_CHECK_QUERYSET_LIST_STATE( sList,
                                            SDI_QUERYSET_LIST_STATE_MAIN_SKIP )
             == ID_TRUE )
        {
            /* 2. MAKE �ܰ�� �����Ѵ�. */
            SDI_SET_QUERYSET_LIST_STATE( sList,
                                         SDI_QUERYSET_LIST_STATE_MAIN_MAKE );
        }
        else if ( SDI_CHECK_QUERYSET_LIST_STATE( sList,
                                                 SDI_QUERYSET_LIST_STATE_DUMMY_SKIP )
             == ID_TRUE )
        {
            /* 2. MAKE �ܰ�� �����Ѵ�. */
            SDI_SET_QUERYSET_LIST_STATE( sList,
                                         SDI_QUERYSET_LIST_STATE_DUMMY_MAKE );
        }
        {
            /* Nothing to do */
        }
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NULL_STATEMENT )
    {
        IDE_SET( ideSetErrorCode( sdERR_ABORT_SDC_UNEXPECTED_ERROR,
                                  "sdo::unsetQuerySetListState",
                                  "statement is NULL" ) );
    }
    IDE_EXCEPTION( ERR_NULL_LIST )
    {
        IDE_SET( ideSetErrorCode( sdERR_ABORT_SDC_UNEXPECTED_ERROR,
                                  "sdo::unsetQuerySetListState",
                                  "list is NULL" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC sdo::setStatementFlagForShard( qcStatement * aStatement,
                                      UInt          aFlag )
{
/***********************************************************************
 *
 * Description : TASK-7219 Shard Transformer Refactoring
 *                Statement Flag �Լ�
 *
 *                 Shard Object ������ Shard Keyword ��� ������ ����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    IDE_TEST_RAISE( aStatement == NULL, ERR_NULL_STATEMENT );

    aStatement->mFlag |= ( aFlag & QC_STMT_SHARD_OBJ_MASK );
    aStatement->mFlag |= ( aFlag & QC_STMT_SHARD_KEYWORD_MASK );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NULL_STATEMENT )
    {
        IDE_SET( ideSetErrorCode( sdERR_ABORT_SDC_UNEXPECTED_ERROR,
                                  "sdo::setStatementFlagForShard",
                                  "statement is NULL" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC sdo::isTransformNeeded( qcStatement * aStatement,
                               idBool      * aIsTransformNeeded )
{
/***********************************************************************
 *
 * Description : TASK-7219 Shard Transformer Refactoring
 *                Shard �м� ������ �˻��ϴ� �Լ�
 *
 * Implementation : 1. �ʼ� ����
 *                  2. ������ Shard Keyword ����
 *                  3. Shard ���� ����
 *                  4. Statement Type ���� ����
 *
 ***********************************************************************/

    qciStmtType      sStmtType          = aStatement->myPlan->parseTree->stmtKind;
    qcShardStmtType  sPrefixType        = aStatement->myPlan->parseTree->stmtShard;
    idBool           sIsTransformNeeded = ID_TRUE;
    qcuSqlSourceInfo sqlInfo;

    IDE_TEST_RAISE( aStatement == NULL, ERR_NULL_STATEMENT );

    /* 1. �ʼ� ���� */
    if ( ( aStatement->spvEnv->createPkg == NULL )
         &&
         ( aStatement->spvEnv->createProc == NULL )
         &&
         ( SDI_CHECK_QUERYSET_LIST_STATE( aStatement->mShardQuerySetList,
                                          SDI_QUERYSET_LIST_STATE_MAIN_MAKE )
           == ID_TRUE ) )
    {
        /* 2. ������ Shard Keyword ���� */
        switch( sPrefixType )
        {
            case QC_STMT_SHARD_NONE :
                if ( ( aStatement->mFlag & QC_STMT_SHARD_KEYWORD_MASK )
                     == QC_STMT_SHARD_KEYWORD_DUPLICATE )
                {
                    sda::setNonShardQueryReason( &( aStatement->mShardPrintInfo ),
                                                 SDI_SHARD_KEYWORD_EXISTS );

                    IDE_RAISE( ERR_SHARD_PREFIX_EXISTS );
                }
                else
                {
                    /* Nothing to do */
                }

                break;

            default :
                if ( ( aStatement->mFlag & QC_STMT_SHARD_KEYWORD_MASK )
                     != QC_STMT_SHARD_KEYWORD_NO_USE )
                {
                    sda::setNonShardQueryReason( &( aStatement->mShardPrintInfo ),
                                                 SDI_SHARD_KEYWORD_EXISTS );

                    IDE_RAISE( ERR_SHARD_PREFIX_EXISTS );
                }
                else
                {
                    /* Nothing to do */
                }

                break;
        }

        /* 3. Shard ���� ���� */
        switch( sPrefixType )
        {
            case QC_STMT_SHARD_NONE :
                if ( ( aStatement->mFlag & QC_STMT_SHARD_OBJ_MASK )
                     != QC_STMT_SHARD_OBJ_EXIST )
                {
                    sda::setNonShardQueryReason( &( aStatement->mShardPrintInfo ),
                                                 SDI_NON_SHARD_OBJECT_EXISTS );

                    sIsTransformNeeded = ID_FALSE;

                    IDE_CONT( NORMAL_EXIT );
                }
                else
                {
                    /* Nothing to do */
                }

                break;

            case QC_STMT_SHARD_META :
                sda::setNonShardQueryReason( &( aStatement->mShardPrintInfo ),
                                             SDI_SHARD_KEYWORD_EXISTS );

                sIsTransformNeeded = ID_FALSE;

                IDE_CONT( NORMAL_EXIT );

                break;

            default :
                break;
        }

        /* 4. Statement Type ���� ���� */
        switch( sStmtType )
        {
            case QCI_STMT_SELECT :
            case QCI_STMT_SELECT_FOR_UPDATE :
            case QCI_STMT_INSERT :
            case QCI_STMT_UPDATE :
            case QCI_STMT_DELETE :
            case QCI_STMT_EXEC_PROC :
                break;

            case QCI_STMT_EXEC_FUNC:
                sIsTransformNeeded = ID_FALSE;

                break;

            default :
                sqlInfo.setSourceInfo( aStatement, &( aStatement->myPlan->parseTree->stmtPos ) );

                IDE_RAISE( ERR_UNSUPPORTED_STMT );
                break;
        }
    }
    else
    {
        sIsTransformNeeded = ID_FALSE;
    }

    IDE_EXCEPTION_CONT( NORMAL_EXIT );

    if ( aIsTransformNeeded != NULL )
    {
        *aIsTransformNeeded = sIsTransformNeeded;
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NULL_STATEMENT )
    {
        IDE_SET( ideSetErrorCode( sdERR_ABORT_SDC_UNEXPECTED_ERROR,
                                  "sdo::IsTransformNeeded",
                                  "statement is NULL" ) );
    }
    IDE_EXCEPTION( ERR_UNSUPPORTED_STMT )
    {
        (void)sqlInfo.init( aStatement->qmeMem );
        IDE_SET( ideSetErrorCode( sdERR_ABORT_SDA_NOT_SUPPORTED_SQLTEXT_FOR_SHARD,
                                  "Unsupported statement type",
                                  sqlInfo.getErrMessage() ) );
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION( ERR_SHARD_PREFIX_EXISTS )
    {
        IDE_SET( ideSetErrorCode( sdERR_ABORT_SDA_NOT_SUPPORTED_SQLTEXT_FOR_SHARD,
                                  sdi::getNonShardQueryReasonArr( SDI_SHARD_KEYWORD_EXISTS ),
                                  "" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC sdo::makeAndSetAnalysis( qcStatement * aSrcStatement,
                                qcStatement * aDstStatement,
                                qmsQuerySet * aDstQuerySet )
{
/***********************************************************************
 *
 * Description : TASK-7219 Shard Transformer Refactoring
 *                �м� ����� Analysis ������ �����ϴ� �Լ�
 *
 *                 Statement �� Query Set ���� Analysis ������ �����ϰų�
 *                  ���� ���� Query Set �� Analysis ������ �����Ѵ�.
 *
 * Implementation : 1. Statement �� Query Set ���� Analysis ������ �����Ѵ�.
 *                  2. ���� ���� Query Set �� Analysis ������ �����Ѵ�.
 *
 ***********************************************************************/

    /* 1. Statement �� Query Set ���� Analysis ������ �����Ѵ�. */
    if ( aDstQuerySet == NULL )
    {
        IDE_TEST( makeAndSetAnalysisStatement( aDstStatement,
                                               aSrcStatement )
                  != IDE_SUCCESS );
    }
    else
    {
        /* 2. ���� ���� Query Set �� Analysis ������ �����Ѵ�. */
        IDE_TEST( makeAndSetAnalysisPartialStatement( aDstStatement,
                                                      aDstQuerySet,
                                                      aSrcStatement )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC sdo::makeAndSetAnalysisStatement( qcStatement * aDstStatement,
                                         qcStatement * aSrcStatement )
{
/***********************************************************************
 *
 * Description : TASK-7219 Shard Transformer Refactoring
 *                Statement �� Query Set ���� Analysis ������ �����ϴ� �Լ�
 *
 *                 Query Set List ������ Query Set ���� ������ �����ϰ�
 *                  �ֻ��� Analysis ������ ���� �Ǵ� Parse Tree �� �����Ѵ�.
 *
 * Implementation : 1. Query Set List ������ Query Set ���� ������ �����Ѵ�.
 *                  2. �ֻ��� Analysis ������ ���� ��, Parse Tree �� �����Ѵ�.
 *                  3. �Ϸ� �ܰ�� �����ϰ�, Plan ������ �����Ѵ�.
 *
 ***********************************************************************/

    sdiQuerySetList  * sDstList     = NULL;
    sdiQuerySetEntry * sDstEntry    = NULL;
    qmsQuerySet      * sDstQuerySet = NULL;
    sdiQuerySetList  * sSrcList     = NULL;
    sdiQuerySetEntry * sSrcEntry    = NULL;
    qmsQuerySet      * sSrcQuerySet = NULL;

    IDE_TEST_RAISE( aSrcStatement == NULL, ERR_NULL_STATEMENT_1 );
    IDE_TEST_RAISE( aDstStatement == NULL, ERR_NULL_STATEMENT_2 );

    sDstList = aDstStatement->mShardQuerySetList;
    sSrcList = aSrcStatement->mShardQuerySetList;

    IDE_TEST_RAISE( sDstList == NULL, ERR_NULL_LIST_1 );
    IDE_TEST_RAISE( sSrcList == NULL, ERR_NULL_LIST_2 );

    IDE_TEST_RAISE( SDI_CHECK_QUERYSET_LIST_STATE( sDstList,
                                                   SDI_QUERYSET_LIST_STATE_MAIN_ANALYZE_DONE )
                    == ID_TRUE,
                    ERR_UNEXPACTED );

    /* 1. Query Set List ������ Query Set ���� ������ �����Ѵ�. */
    for ( sDstEntry = sDstList->mHead, sSrcEntry = sSrcList->mHead;
          ( ( sDstEntry != NULL ) && ( sSrcEntry != NULL ) );
          sDstEntry = sDstEntry->mNext, sSrcEntry = sSrcEntry->mNext )
    {
        IDE_TEST_RAISE( sDstEntry->mId != sSrcEntry->mId, ERR_INVALID_ENTRY );

        sDstQuerySet = sDstEntry->mQuerySet;
        sSrcQuerySet = sSrcEntry->mQuerySet;

        IDE_TEST( allocAndCopyAnalysisQuerySet( aDstStatement,
                                                sDstQuerySet,
                                                sSrcQuerySet )
                  != IDE_SUCCESS );
    }

    IDE_TEST_RAISE( sDstEntry != NULL, ERR_INVALID_ENTRY_1 );
    IDE_TEST_RAISE( sSrcEntry != NULL, ERR_INVALID_ENTRY_2 );

    /* 2. �ֻ��� Analysis ������ ���� ��, Parse Tree �� �����Ѵ�. */
    IDE_TEST( allocAndCopyAnalysisParseTree( aDstStatement,
                                             aDstStatement->myPlan->parseTree,
                                             aSrcStatement->myPlan->parseTree )
              != IDE_SUCCESS );

    /* 3. �Ϸ� �ܰ�� �����ϰ�, Plan ������ �����Ѵ�. */
    SDI_SET_QUERYSET_LIST_STATE( sDstList,
                                 SDI_QUERYSET_LIST_STATE_MAIN_ANALYZE_DONE );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NULL_STATEMENT_1 )
    {
        IDE_SET( ideSetErrorCode( sdERR_ABORT_SDC_UNEXPECTED_ERROR,
                                  "sdo::makeAndSetAnalysisStatement",
                                  "dst statement is NULL" ) );
    }
    IDE_EXCEPTION( ERR_NULL_STATEMENT_2 )
    {
        IDE_SET( ideSetErrorCode( sdERR_ABORT_SDC_UNEXPECTED_ERROR,
                                  "sdo::makeAndSetAnalysisStatement",
                                  "src statement is NULL" ) );
    }
    IDE_EXCEPTION( ERR_UNEXPACTED )
    {
        IDE_SET( ideSetErrorCode( sdERR_ABORT_SDC_UNEXPECTED_ERROR,
                                  "sdo::makeAndSetAnalysisStatement",
                                  "list is invalid" ) );
    }
    IDE_EXCEPTION( ERR_NULL_LIST_1 )
    {
        IDE_SET( ideSetErrorCode( sdERR_ABORT_SDC_UNEXPECTED_ERROR,
                                  "sdo::makeAndSetAnalysisStatement",
                                  "dst list is NULL" ) );
    }
    IDE_EXCEPTION( ERR_NULL_LIST_2 )
    {
        IDE_SET( ideSetErrorCode( sdERR_ABORT_SDC_UNEXPECTED_ERROR,
                                  "sdo::makeAndSetAnalysisStatement",
                                  "src list is NULL" ) );
    }
    IDE_EXCEPTION( ERR_INVALID_ENTRY )
    {
        IDE_SET( ideSetErrorCode( sdERR_ABORT_SDC_UNEXPECTED_ERROR,
                                  "sdo::makeAndSetAnalysisStatement",
                                  "entry is invalid" ) );
    }
    IDE_EXCEPTION( ERR_INVALID_ENTRY_1 )
    {
        IDE_SET( ideSetErrorCode( sdERR_ABORT_SDC_UNEXPECTED_ERROR,
                                  "sdo::makeAndSetAnalysisStatement",
                                  "dst entry is invalid" ) );
    }
    IDE_EXCEPTION( ERR_INVALID_ENTRY_2 )
    {
        IDE_SET( ideSetErrorCode( sdERR_ABORT_SDC_UNEXPECTED_ERROR,
                                  "sdo::makeAndSetAnalysisStatement",
                                  "src entry is invalid" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC sdo::makeAndSetAnalysisPartialStatement( qcStatement * aDstStatement,
                                                qmsQuerySet * aDstQuerySet,
                                                qcStatement * aSrcStatement )
{
/***********************************************************************
 *
 * Description : TASK-7219 Shard Transformer Refactoring
 *                ���� ���� Query Set �� Analysis ������ �����ϴ� �Լ�
 *
 *                 qmgShardSelect ���� ����ȭ �� �ٽ� ������ ������ ���� �̿��Ѵ�.
 *
 * Implementation : 1. ���޹��� Query Set �� ������ �����Ѵ�.
 *                  2. �Ϸ� �ܰ�� �����Ѵ�.
 *
 ***********************************************************************/

    qcParseTree      * sSrcParseTree = NULL;
    sdiQuerySetList  * sDstList      = NULL;
    sdiQuerySetList  * sSrcList      = NULL;

    IDE_TEST_RAISE( aSrcStatement == NULL, ERR_NULL_STATEMENT_1 );
    IDE_TEST_RAISE( aDstStatement == NULL, ERR_NULL_STATEMENT_2 );

    sDstList = aDstStatement->mShardQuerySetList;
    sSrcList = aSrcStatement->mShardQuerySetList;

    IDE_TEST_RAISE( sDstList == NULL, ERR_NULL_LIST_1 );
    IDE_TEST_RAISE( sSrcList == NULL, ERR_NULL_LIST_2 );

    sSrcParseTree = aSrcStatement->myPlan->parseTree;

    IDE_TEST_RAISE( SDI_CHECK_QUERYSET_LIST_STATE( sDstList,
                                                   SDI_QUERYSET_LIST_STATE_MAIN_ANALYZE_DONE )
                    == ID_TRUE,
                    ERR_UNEXPACTED );

    /* 1. ���޹��� Query Set �� ������ �����Ѵ�. */
    IDE_TEST( allocAndCopyAnalysisPartialStatement( aDstStatement,
                                                    aDstQuerySet,
                                                    sSrcParseTree )
              != IDE_SUCCESS );

    /* 2. �Ϸ� �ܰ�� �����Ѵ�.
     *  Ư�� �κи� �ٽ� ������ �����ϴ� ����,
     *   ��ü�� ������ �� ������ Plan ���� ����� �����Ѵ�.
     */
    SDI_SET_QUERYSET_LIST_STATE( sDstList,
                                 SDI_QUERYSET_LIST_STATE_MAIN_ANALYZE_DONE );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NULL_STATEMENT_1 )
    {
        IDE_SET( ideSetErrorCode( sdERR_ABORT_SDC_UNEXPECTED_ERROR,
                                  "sdo::makeAndSetAnalysisPartialStatement",
                                  "dst statement is NULL" ) );
    }
    IDE_EXCEPTION( ERR_NULL_STATEMENT_2 )
    {
        IDE_SET( ideSetErrorCode( sdERR_ABORT_SDC_UNEXPECTED_ERROR,
                                  "sdo::makeAndSetAnalysisPartialStatement",
                                  "src statement is NULL" ) );
    }
    IDE_EXCEPTION( ERR_UNEXPACTED )
    {
        IDE_SET( ideSetErrorCode( sdERR_ABORT_SDC_UNEXPECTED_ERROR,
                                  "sdo::makeAndSetAnalysisPartialStatement",
                                  "list is invalid" ) );
    }
    IDE_EXCEPTION( ERR_NULL_LIST_1 )
    {
        IDE_SET( ideSetErrorCode( sdERR_ABORT_SDC_UNEXPECTED_ERROR,
                                  "sdo::makeAndSetAnalysisPartialStatement",
                                  "dst list is NULL" ) );
    }
    IDE_EXCEPTION( ERR_NULL_LIST_2 )
    {
        IDE_SET( ideSetErrorCode( sdERR_ABORT_SDC_UNEXPECTED_ERROR,
                                  "sdo::makeAndSetAnalysisPartialStatement",
                                  "src list is NULL" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC sdo::allocAndCopyAnalysisQuerySet( qcStatement * aStatement,
                                          qmsQuerySet * aDstQuerySet,
                                          qmsQuerySet * aSrcQuerySet )
{
/***********************************************************************
 *
 * Description : TASK-7219 Shard Transformer Refactoring
 *                Analysis ������ �����ϴ� �Լ�
 *
 *                 Analysis ������ Main, SubKey �� Set �� ������ �� �ִ�.
 *                  Analyze ����� Src QuerySet ���� Analysis �� ������
 *                   Dst QuerySet �����Ͽ� �����Ѵ�.
 *
 * Implementation : 1. Src QuerySet ���� Analysis �� �����´�.
 *                  2. Dst Analysis  �� �����Ͽ� �����Ѵ�.
 *                  3. SubKey ���� ��, SubKey Analysis �� �����Ͽ� �����Ѵ�.
 *                  4. Analysis �� Dst QuerySet �� �����Ѵ�.
 *
 ***********************************************************************/

    sdiShardAnalysis * sSrcAnalysis  = NULL;
    sdiShardAnalysis * sDstAnalysis  = NULL;
    sdiShardAnalysis * sNextAnalysis = NULL;

    IDE_TEST_RAISE( aDstQuerySet == NULL, ERR_NULL_QUERYSET );

    /* 1. Src QuerySet ���� Analysis �� �����´�. */
    IDE_TEST( getQuerySetAnalysis( aSrcQuerySet,
                                   &( sSrcAnalysis ) )
              != IDE_SUCCESS );

    if ( sSrcAnalysis != NULL )
    {
        /* 2. Dst Analysis �� �����Ͽ� �����Ѵ�. */
        IDE_TEST( allocAndCopyAnalysis( aStatement,
                                        sSrcAnalysis,
                                        &( sDstAnalysis ) )
                  != IDE_SUCCESS );

        /* 3. SubKey ���� ��, SubKey Analysis �� �����Ͽ� �����Ѵ�. */
        if ( sDstAnalysis->mAnalysisFlag.mTopQueryFlag[ SDI_TQ_SUB_KEY_EXISTS ] == ID_TRUE )
        {
            IDE_TEST( allocAndCopyAnalysis( aStatement,
                                            sSrcAnalysis->mNext,
                                            &( sNextAnalysis ) )
                      != IDE_SUCCESS );

            sDstAnalysis->mNext = sNextAnalysis;
        }
        else
        {
            /* Nothing to do */
        }
    }
    else
    {
        /* A. SET ������ �ִ� ���, Left, Right �м� �� ���� �߻�����
         *    SET ������ Query Set �� Analysis �� �������� ���� �� �ִ�.
         *    ���� ���� �߻� ��, �� �̻� �������� �����Ƿ� �������.
         *
         * B. Scala Subquery �� ���,
         *    �ش� Query Set �� Non Shard �̹Ƿ�, Subquery ���� �м����� �ʴ´�.
         *    ���� Query Set �� Non Shard ���ο� ������� Subquery �� �м��ϵ���
         *    sda::analyzeSFWGHCore �� sda::subqueryAnalysis4SFWGH ��ġ�� �����Ͽ� �������.
         *
         * C. Composite Method �� Subquery �� ���,
         *    �ش� SubKey �м� �� Query Set �� Non Shard �̹Ƿ�, Subquery ���� �м����� �ʴ´�.
         *    ���� SubKey �м� �� Query Set �� Non Shard ���ο� ������� Subquery �� �м��ϵ���
         *    sda::analyzeSFWGHCore �� sda::subqueryAnalysis4SFWGH ��ġ�� �����Ͽ� �������.
         *
         * D. �Ϲ� ���̺� ��� Insert, Update, Delete �� Subquery �� ���
         *    �ش� Query Set �� Non Shard �̹Ƿ�, Subquery ���� �м����� �ʴ´�.
         *    ���� SubKey �м� �� Query Set �� Non Shard ���ο� ������� Subquery �� �м��ϵ���
         *    sda::analyzeSFWGHCore �� sda::subqueryAnalysis4SFWGH ��ġ�� �����Ͽ� �������.
         *
         * E. Unnesting, View Merging, CFS �� ���,
         *    �ش� Query Set �� �ǳʶپ� �м����� �ʴ´�.
         *    ���� analyzePartialQuerySet �� �ٷ� �м��Ͽ� Analysis �� �����Ͽ� �������.
         */
        IDE_RAISE( ERR_NULL_ANALYSIS );
    }

    /* 4. Analysis �� Dst QuerySet �� �����Ѵ�. */
    aDstQuerySet->mShardAnalysis = sDstAnalysis;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NULL_QUERYSET )
    {
        IDE_SET( ideSetErrorCode( sdERR_ABORT_SDC_UNEXPECTED_ERROR,
                                  "sdo::allocAndCopyAnalysisQuerySet",
                                  "dst parse tree is NULL" ) );
    }
    IDE_EXCEPTION( ERR_NULL_ANALYSIS )
    {
        IDE_SET( ideSetErrorCode( sdERR_ABORT_SDC_UNEXPECTED_ERROR,
                                  "sdo::allocAndCopyAnalysisQuerySet",
                                  "analysis is NULL" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC sdo::allocAndCopyAnalysisParseTree( qcStatement * aStatement,
                                           qcParseTree * aDstParseTree,
                                           qcParseTree * aSrcParseTree )
{
/***********************************************************************
 *
 * Description : TASK-7219 Shard Transformer Refactoring
 *                Analysis ������ �����ϴ� �Լ�
 *
 *                 Analysis ������ Main, SubKey �� Set �� ������ �� �ִ�.
 *                  Analyze ����� Src ParseTree ���� Analysis �� ������
 *                   Dst ParseTree �� �����Ͽ� �����Ѵ�.
 *
 * Implementation : 1. Src ParseTree ���� Analysis �� �����´�
 *                  2. Select �� ���, �̹� ������ QuerySet �� Analysis �� �����´�.
 *                  3. Dst Analysis �� �����Ͽ� �����Ѵ�.
 *                  4. SubKey ���� ��, SubKey Analysis �� �����Ͽ� �����Ѵ�.
 *                  5. Analysis �� Dst ParseTree �� �����Ѵ�.
 *
 ***********************************************************************/

    sdiShardAnalysis * sSrcAnalysis  = NULL;
    sdiShardAnalysis * sDstAnalysis  = NULL;
    sdiShardAnalysis * sNextAnalysis = NULL;

    IDE_TEST_RAISE( aDstParseTree == NULL, ERR_NULL_PARSETREE );

    /* 1. Src ParseTree ���� Analysis �� �����´�. */
    IDE_TEST( sda::getParseTreeAnalysis( aSrcParseTree,
                                         &( sSrcAnalysis ) )
              != IDE_SUCCESS );

    if ( sSrcAnalysis != NULL )
    {
        /* 2. Select �� ���, �̹� ������ QuerySet �� Analysis �� �����´�. */
        if ( ( aDstParseTree->stmtKind == QCI_STMT_SELECT )
             ||
             ( aDstParseTree->stmtKind == QCI_STMT_SELECT_FOR_UPDATE ) )
        {
            sDstAnalysis = ( (qmsParseTree *)aDstParseTree )->querySet->mShardAnalysis;
        }
        else
        {
            /* 3. Dst Analysis �� �����Ͽ� �����Ѵ�. */
            IDE_TEST( allocAndCopyAnalysis( aStatement,
                                            sSrcAnalysis,
                                            &( sDstAnalysis ) )
                      != IDE_SUCCESS );

            /* 4. SubKey ���� ��, SubKey Analysis �� �����Ͽ� �����Ѵ�. */
            if ( sDstAnalysis->mAnalysisFlag.mTopQueryFlag[ SDI_TQ_SUB_KEY_EXISTS ] == ID_TRUE )
            {
                IDE_TEST( allocAndCopyAnalysis( aStatement,
                                                sSrcAnalysis->mNext,
                                                &( sNextAnalysis ) )
                          != IDE_SUCCESS );

                sDstAnalysis->mNext = sNextAnalysis;
            }
            else
            {
                /* Nothing to do */
            }
        }
    }
    else
    {
        /* A. SET ������ �ִ� ���, Left, Right �м� �� ���� �߻�����
         *    SET ������ Query Set �� Analysis �� �������� ���� �� �ִ�.
         *    ���� ���� �߻� ��, �� �̻� �������� �����Ƿ� �������.
         *
         * B. Scala Subquery �� ���,
         *    �ش� Query Set �� Non Shard �̹Ƿ�, Subquery ���� �м����� �ʴ´�.
         *    ���� Query Set �� Non Shard ���ο� ������� Subquery �� �м��ϵ���
         *    sda::analyzeSFWGHCore �� sda::subqueryAnalysis4SFWGH ��ġ�� �����Ͽ� �������.
         *
         * C. Composite Method �� Subquery �� ���,
         *    �ش� SubKey �м� �� Query Set �� Non Shard �̹Ƿ�, Subquery ���� �м����� �ʴ´�.
         *    ���� SubKey �м� �� Query Set �� Non Shard ���ο� ������� Subquery �� �м��ϵ���
         *    sda::analyzeSFWGHCore �� sda::subqueryAnalysis4SFWGH ��ġ�� �����Ͽ� �������.
         *
         * D. �Ϲ� ���̺� ��� Insert, Update, Delete �� Subquery �� ���
         *    �ش� Query Set �� Non Shard �̹Ƿ�, Subquery ���� �м����� �ʴ´�.
         *    ���� SubKey �м� �� Query Set �� Non Shard ���ο� ������� Subquery �� �м��ϵ���
         *    sda::analyzeSFWGHCore �� sda::subqueryAnalysis4SFWGH ��ġ�� �����Ͽ� �������.
         *
         * E. Unnesting, View Merging, CFS �� ���,
         *    �ش� Query Set �� �ǳʶپ� �м����� �ʴ´�.
         *    ���� analyzePartialQuerySet �� �ٷ� �м��Ͽ� Analysis �� �����Ͽ� �������.
         */
        IDE_RAISE( ERR_NULL_ANALYSIS );
    }

    /* 5. Analysis �� Dst ParseTree �� �����Ѵ�. */
    IDE_TEST( sda::setParseTreeAnalysis( aDstParseTree,
                                         sDstAnalysis )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NULL_PARSETREE )
    {
        IDE_SET( ideSetErrorCode( sdERR_ABORT_SDC_UNEXPECTED_ERROR,
                                  "sdo::allocAndCopyAnalysisParseTree",
                                  "dst parse tree is NULL" ) );
    }
    IDE_EXCEPTION( ERR_NULL_ANALYSIS )
    {
        IDE_SET( ideSetErrorCode( sdERR_ABORT_SDC_UNEXPECTED_ERROR,
                                  "sdo::allocAndCopyAnalysisParseTree",
                                  "analysis is NULL" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC sdo::allocAndCopyAnalysisPartialStatement( qcStatement * aStatement,
                                                  qmsQuerySet * aDstQuerySet,
                                                  qcParseTree * aSrcParseTree )
{
/***********************************************************************
 *
 * Description : TASK-7219 Shard Transformer Refactoring
 *                Analysis ������ �����ϴ� �Լ�
 *
 *                 Analysis ������ Main, SubKey �� Set �� ������ �� �ִ�.
 *                  Analyze ����� Src ParseTree ���� Analysis �� ������
 *                   Dst QuserySet �����Ͽ� �����Ѵ�.
 *
 * Implementation : 1. Src ParseTree ���� Analysis �� �����´�
 *                  2. Dst Analysis �� �����Ͽ� �����Ѵ�.
 *                  3. SubKey ���� ��, SubKey Analysis �� �����Ͽ� �����Ѵ�.
 *                  4. Analysis �� Dst QuerySet �� �����Ѵ�.
 *
 ***********************************************************************/

    sdiShardAnalysis * sSrcAnalysis  = NULL;
    sdiShardAnalysis * sDstAnalysis  = NULL;
    sdiShardAnalysis * sNextAnalysis = NULL;

    IDE_TEST_RAISE( aDstQuerySet == NULL, ERR_NULL_QUERYSET );

    /* 1. Src ParseTree ���� Analysis �� �����´�. */
    IDE_TEST( sda::getParseTreeAnalysis( aSrcParseTree,
                                         &( sSrcAnalysis ) )
              != IDE_SUCCESS );

    if ( sSrcAnalysis != NULL )
    {
        /* 2. Dst Analysis �� �����Ͽ� �����Ѵ�. */
        IDE_TEST( allocAndCopyAnalysis( aStatement,
                                        sSrcAnalysis,
                                        &( sDstAnalysis ) )
                  != IDE_SUCCESS );

        /* 3. SubKey ���� ��, SubKey Analysis �� �����Ͽ� �����Ѵ�. */
        if ( sDstAnalysis->mAnalysisFlag.mTopQueryFlag[ SDI_TQ_SUB_KEY_EXISTS ] == ID_TRUE )
        {
            IDE_TEST( allocAndCopyAnalysis( aStatement,
                                            sSrcAnalysis->mNext,
                                            &( sNextAnalysis ) )
                      != IDE_SUCCESS );

            sDstAnalysis->mNext = sNextAnalysis;
        }
        else
        {
            /* Nothing to do */
        }
    }
    else
    {
        /* A. SET ������ �ִ� ���, Left, Right �м� �� ���� �߻�����
         *    SET ������ Query Set �� Analysis �� �������� ���� �� �ִ�.
         *    ���� ���� �߻� ��, �� �̻� �������� �����Ƿ� �������.
         *
         * B. Scala Subquery �� ���,
         *    �ش� Query Set �� Non Shard �̹Ƿ�, Subquery ���� �м����� �ʴ´�.
         *    ���� Query Set �� Non Shard ���ο� ������� Subquery �� �м��ϵ���
         *    sda::analyzeSFWGHCore �� sda::subqueryAnalysis4SFWGH ��ġ�� �����Ͽ� �������.
         *
         * C. Composite Method �� Subquery �� ���,
         *    �ش� SubKey �м� �� Query Set �� Non Shard �̹Ƿ�, Subquery ���� �м����� �ʴ´�.
         *    ���� SubKey �м� �� Query Set �� Non Shard ���ο� ������� Subquery �� �м��ϵ���
         *    sda::analyzeSFWGHCore �� sda::subqueryAnalysis4SFWGH ��ġ�� �����Ͽ� �������.
         *
         * D. �Ϲ� ���̺� ��� Insert, Update, Delete �� Subquery �� ���
         *    �ش� Query Set �� Non Shard �̹Ƿ�, Subquery ���� �м����� �ʴ´�.
         *    ���� SubKey �м� �� Query Set �� Non Shard ���ο� ������� Subquery �� �м��ϵ���
         *    sda::analyzeSFWGHCore �� sda::subqueryAnalysis4SFWGH ��ġ�� �����Ͽ� �������.
         *
         * E. Unnesting, View Merging, CFS �� ���,
         *    �ش� Query Set �� �ǳʶپ� �м����� �ʴ´�.
         *    ���� analyzePartialQuerySet �� �ٷ� �м��Ͽ� Analysis �� �����Ͽ� �������.
         */
        IDE_RAISE( ERR_NULL_ANALYSIS );
    }

    /* 5. Analysis �� Dst ParseTree �� �����Ѵ�. */
    aDstQuerySet->mShardAnalysis = sDstAnalysis;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NULL_QUERYSET )
    {
        IDE_SET( ideSetErrorCode( sdERR_ABORT_SDC_UNEXPECTED_ERROR,
                                  "sdo::allocAndCopyAnalysisPartialStatement",
                                  "dst queryset is NULL" ) );
    }
    IDE_EXCEPTION( ERR_NULL_ANALYSIS )
    {
        IDE_SET( ideSetErrorCode( sdERR_ABORT_SDC_UNEXPECTED_ERROR,
                                  "sdo::allocAndCopyAnalysisPartialStatement",
                                  "analysis is NULL" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC sdo::getQuerySetAnalysis( qmsQuerySet       * aQuerySet,
                                 sdiShardAnalysis ** aAnalysis )
{
/***********************************************************************
 *
 * Description : TASK-7219 Shard Transformer Refactoring
 *                Analysis ������ �������� �Լ�
 *
 *                 Pre Analysis ���� �Լ�
 *                  - sdo::allocAndCopyAnalysisQuerySet
 *
 * Implementation : 1. Pre Analysis �� �ִٸ�, Pre Analysis �� �����´�.
 *                  2. QuetSet �� �ش��ϴ� Analysis �� �����´�.
 *
 ***********************************************************************/

    sdiShardAnalysis * sAnalysis = NULL;

    IDE_TEST_RAISE( aQuerySet == NULL, ERR_NULL_QUERYSET );

    /* 1. Pre Analysis �� �ִٸ�, Pre Analysis �� �����´�.
     *  Shard Analysis �� Optimize �� QuerySet �� �����̸�,
     *   Pre Analysis �� Optimize �� QuerySet �� �����̴�.
     *
     *    Analyze ������ Shard Analysis ������� Shard ���θ� �Ǵ��ϰ�,
     *     Non-Shard ��ȯ�� �ʿ��� Transform ������ Pre Analysis ������ �ʿ�� �Ѵ�.
     */
    if ( aQuerySet->mPreAnalysis != NULL )
    {
        sAnalysis = aQuerySet->mPreAnalysis;
    }
    else
    {
        /* 2. QuetSet �� �ش��ϴ� Analysis �� �����´�. */
        sAnalysis = aQuerySet->mShardAnalysis;
    }

    if ( aAnalysis != NULL )
    {
        *aAnalysis = sAnalysis;
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NULL_QUERYSET )
    {
        IDE_SET( ideSetErrorCode( sdERR_ABORT_SDC_UNEXPECTED_ERROR,
                                  "sdo::getQuerySetAnalysis",
                                  "queryset is NULL" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC sdo::allocAndCopyAnalysis( qcStatement       * aStatement,
                                  sdiShardAnalysis  * aSrcAnalysis,
                                  sdiShardAnalysis ** aDstAnalysis )
{
/***********************************************************************
 *
 * Description : TASK-7219 Shard Transformer Refactoring
 *                Analysis ���� �����ϴ� �Լ�
 *
 * Implementation :
 *
 ***********************************************************************/

    sdiShardAnalysis * sAnalysis = NULL;

    IDE_TEST_RAISE( aSrcAnalysis == NULL, ERR_NULL_ANALYSIS );

    IDE_TEST( sda::allocAnalysis( aStatement,
                                  &( sAnalysis ) )
              != IDE_SUCCESS );

    sAnalysis->mIsShardQuery = aSrcAnalysis->mIsShardQuery;

    /* Reason Flag ���� ��� NonShardQueryReason �� �����Ѵ�. */
    IDE_TEST( sda::mergeAnalysisFlag( &( aSrcAnalysis->mAnalysisFlag ),
                                      &( sAnalysis->mAnalysisFlag ),
                                      SDI_ANALYSIS_FLAG_NON_TRUE |
                                      SDI_ANALYSIS_FLAG_TOP_TRUE |
                                      SDI_ANALYSIS_FLAG_SET_TRUE |
                                      SDI_ANALYSIS_FLAG_CUR_TRUE )
              != IDE_SUCCESS );

    IDE_TEST( allocAndCopyShardInfo( aStatement,
                                     &( aSrcAnalysis->mShardInfo ),
                                     &( sAnalysis->mShardInfo ) )
              != IDE_SUCCESS );

    IDE_TEST( allocAndCopyKeyInfo( aStatement,
                                   aSrcAnalysis->mKeyInfo,
                                   &( sAnalysis->mKeyInfo ) )
              != IDE_SUCCESS );

    IDE_TEST( allocAndCopyTableInfoList( aStatement,
                                         aSrcAnalysis->mTableInfoList,
                                         &( sAnalysis->mTableInfoList ) )
              != IDE_SUCCESS );

    if ( aDstAnalysis != NULL )
    {
        *aDstAnalysis = sAnalysis;
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NULL_ANALYSIS )
    {
        IDE_SET( ideSetErrorCode( sdERR_ABORT_SDC_UNEXPECTED_ERROR,
                                  "sdo::allocAndCopyAnalysis",
                                  "analysis is NULL" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC sdo::allocAndCopyKeyInfo( qcStatement * aStatement,
                                 sdiKeyInfo  * aSrcKeyInfo,
                                 sdiKeyInfo ** aDstKeyInfo )
{
/***********************************************************************
 *
 * Description : TASK-7219 Shard Transformer Refactoring
 *                sdiKeyInfo ���� �����ϴ� �Լ�
 *
 *                 mLeft, mRight, mOrgKeyInfo �� ����Ʈ ������ �����ϰ�,
 *                  ���� KeyInfo �ߺ� ����� ������� �ʰ� ��� �����Ѵ�.
 *
 *                   ���� ���� Key Info ������ ���� Analysis �� �����Ǿ� ������ �� �ִ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    sdiKeyInfo * sKeyInfo    = NULL;
    sdiKeyInfo * sHead       = NULL;
    sdiKeyInfo * sTail       = NULL;
    sdiKeyInfo * sSrcKeyInfo = NULL;

    IDE_TEST_RAISE( aStatement == NULL, ERR_NULL_STATEMENT );

    for ( sSrcKeyInfo  = aSrcKeyInfo;
          sSrcKeyInfo != NULL;
          sSrcKeyInfo  = sSrcKeyInfo->mNext )
    {
        IDE_TEST( QC_QMP_MEM( aStatement )->alloc( ID_SIZEOF( sdiKeyInfo ),
                                                   (void **)&( sKeyInfo ) )
                  != IDE_SUCCESS );

        SDI_INIT_KEY_INFO( sKeyInfo );

        IDE_TEST( allocAndCopyKeyTarget( aStatement,
                                         sSrcKeyInfo->mKeyTargetCount,
                                         sSrcKeyInfo->mKeyTarget,
                                         &( sKeyInfo->mKeyTargetCount ),
                                         &( sKeyInfo->mKeyTarget ) )
                  != IDE_SUCCESS );

        IDE_TEST( allocAndCopyKeyColumn( aStatement,
                                         sSrcKeyInfo->mKeyCount,
                                         sSrcKeyInfo->mKey,
                                         &( sKeyInfo->mKeyCount ),
                                         &( sKeyInfo->mKey ) )
                  != IDE_SUCCESS );

        IDE_TEST( sdi::allocAndCopyValues( aStatement,
                                           &( sKeyInfo->mValuePtrArray ),
                                           &( sKeyInfo->mValuePtrCount ),
                                           sSrcKeyInfo->mValuePtrArray,
                                           sSrcKeyInfo->mValuePtrCount )
                  != IDE_SUCCESS );

        IDE_TEST( allocAndCopyShardInfo( aStatement,
                                         &( sSrcKeyInfo->mShardInfo ),
                                         &( sKeyInfo->mShardInfo ) )
                  != IDE_SUCCESS );

        sKeyInfo->mIsJoined = sSrcKeyInfo->mIsJoined;

        if ( sHead == NULL )
        {
            sHead = sKeyInfo;
            sTail = sKeyInfo;
        }
        else
        {
            sTail->mNext = sKeyInfo;
            sTail        = sKeyInfo;
        }
    }

    if ( aDstKeyInfo != NULL )
    {
        *aDstKeyInfo = sHead;
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NULL_STATEMENT )
    {
        IDE_SET( ideSetErrorCode( sdERR_ABORT_SDC_UNEXPECTED_ERROR,
                                  "sdo::allocAndCopyKeyInfo",
                                  "statement is NULL" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC sdo::allocAndCopyKeyTarget( qcStatement  * aStatement,
                                   UInt           aSrcCount,
                                   UShort       * aSrcTarget,
                                   UInt         * aDstCount,
                                   UShort      ** aDstTarget )
{
/***********************************************************************
 *
 * Description : TASK-7219 Shard Transformer Refactoring
 *                Key Target ������ �����ϴ� �Լ�
 *
 * Implementation :
 *
 ***********************************************************************/

    UShort * sTarget = NULL;

    if ( aSrcCount > 0 )
    {
        IDE_TEST_RAISE( aSrcTarget == NULL, ERR_NULL_TARGET );

        IDE_TEST( QC_QMP_MEM( aStatement )->alloc( ID_SIZEOF( UShort ) * aSrcCount,
                                                   (void **)&( sTarget ) )
                  != IDE_SUCCESS );

        idlOS::memcpy( (void *) sTarget,
                       (void *) aSrcTarget,
                       ID_SIZEOF( UShort ) * aSrcCount );
    }
    else
    {
        sTarget = NULL;
    }

    if ( ( aDstTarget != NULL )
         &&
         ( aDstCount != NULL ) )
    {
        *aDstTarget = sTarget;
        *aDstCount  = aSrcCount;
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NULL_TARGET )
    {
        IDE_SET( ideSetErrorCode( sdERR_ABORT_SDC_UNEXPECTED_ERROR,
                                  "sdo::allocAndCopyKeyTarget",
                                  "target is NULL" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC sdo::allocAndCopyKeyColumn( qcStatement        * aStatement,
                                   UInt                 aSrcCount,
                                   sdiKeyTupleColumn  * aSrcColumn,
                                   UInt               * aDstCount,
                                   sdiKeyTupleColumn ** aDstColumn )
{
/***********************************************************************
 *
 * Description : TASK-7219 Shard Transformer Refactoring
 *                Key Column ������ �����ϴ� �Լ�
 *
 * Implementation :
 *
 ***********************************************************************/

    sdiKeyTupleColumn * sColum = NULL;

    if ( aSrcCount > 0 )
    {
        IDE_TEST_RAISE( aSrcColumn == NULL, ERR_NULL_COLUMN );

        IDE_TEST( QC_QMP_MEM( aStatement )->alloc( ID_SIZEOF( sdiKeyTupleColumn ) * aSrcCount,
                                                   (void **)&( sColum ) )
                  != IDE_SUCCESS );

        idlOS::memcpy( (void *) sColum,
                       (void *) aSrcColumn,
                       ID_SIZEOF( sdiKeyTupleColumn ) * aSrcCount );
    }
    else
    {
        sColum = NULL;
    }

    if ( ( aDstColumn != NULL )
         &&
         ( aDstCount != NULL ) )
    {
        *aDstColumn = sColum;
        *aDstCount  = aSrcCount;
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NULL_COLUMN )
    {
        IDE_SET( ideSetErrorCode( sdERR_ABORT_SDC_UNEXPECTED_ERROR,
                                  "sdo::allocAndCopyKeyColumn",
                                  "column is NULL" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC sdo::allocAndCopyShardInfo( qcStatement  * aStatement,
                                   sdiShardInfo * aSrcShardInfo,
                                   sdiShardInfo * aDstShardInfo )
{
/***********************************************************************
 *
 * Description : TASK-7219 Shard Transformer Refactoring
 *                Shard Info ������ �����ϴ� �Լ�
 *
 * Implementation :
 *
 ***********************************************************************/

    sdiRangeInfo * sRangeInfo = NULL;

    IDE_TEST_RAISE( aStatement == NULL, ERR_NULL_STATEMENT );
    IDE_TEST_RAISE( aSrcShardInfo == NULL, ERR_NULL_SHARD_INFO_1 );
    IDE_TEST_RAISE( aDstShardInfo == NULL, ERR_NULL_SHARD_INFO_2 );

    aDstShardInfo->mKeyDataType   = aSrcShardInfo->mKeyDataType;
    aDstShardInfo->mDefaultNodeId = aSrcShardInfo->mDefaultNodeId;
    aDstShardInfo->mSplitMethod   = aSrcShardInfo->mSplitMethod;

    IDE_TEST( QC_QMP_MEM( aStatement )->alloc( ID_SIZEOF( sdiRangeInfo ),
                                               (void **)&( sRangeInfo ) )
              != IDE_SUCCESS );

    sRangeInfo->mCount  = 0;
    sRangeInfo->mRanges = NULL;

    IDE_TEST( sdi::allocAndCopyRanges( aStatement,
                                       sRangeInfo,
                                       aSrcShardInfo->mRangeInfo )
              != IDE_SUCCESS );

    aDstShardInfo->mRangeInfo = sRangeInfo;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NULL_STATEMENT )
    {
        IDE_SET( ideSetErrorCode( sdERR_ABORT_SDC_UNEXPECTED_ERROR,
                                  "sdo::allocAndCopyShardInfo",
                                  "statement is NULL" ) );
    }
    IDE_EXCEPTION( ERR_NULL_SHARD_INFO_1 )
    {
        IDE_SET( ideSetErrorCode( sdERR_ABORT_SDC_UNEXPECTED_ERROR,
                                  "sdo::allocAndCopyShardInfo",
                                  "src shard info is NULL" ) );
    }
    IDE_EXCEPTION( ERR_NULL_SHARD_INFO_2 )
    {
        IDE_SET( ideSetErrorCode( sdERR_ABORT_SDC_UNEXPECTED_ERROR,
                                  "sdo::allocAndCopyShardInfo",
                                  "dst shard info is NULL" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC sdo::allocAndCopyTableInfoList( qcStatement       * aStatement,
                                       sdiTableInfoList  * aSrcTableInfoList,
                                       sdiTableInfoList ** aDstTableInfoList )
{
/***********************************************************************
 *
 * Description : TASK-7219 Shard Transformer Refactoring
 *                Table Info List ������ �����ϴ� �Լ�
 *
 *                 ����Ʈ �� Table ���� �׷��� �����Ͽ�, Plan Diff �߻��Ͽ���.
 *
 * Implementation :
 *
 ***********************************************************************/

    sdiTableInfoList * sTableInfoList = NULL;
    sdiTableInfoList * sTableInfo     = NULL;
    sdiTableInfoList * sHead          = NULL;
    sdiTableInfoList * sTail          = NULL;

    IDE_TEST_RAISE( aStatement == NULL, ERR_NULL_STATEMENT );

    for ( sTableInfoList  = aSrcTableInfoList;
          sTableInfoList != NULL;
          sTableInfoList  = sTableInfoList->mNext )
    {
        IDE_TEST( QC_QMP_MEM( aStatement )->alloc( ID_SIZEOF( sdiTableInfoList ) +
                                                   ID_SIZEOF( sdiTableInfo ),
                                                   (void **)&( sTableInfo ) )
                  != IDE_SUCCESS );

        sTableInfo->mTableInfo = (sdiTableInfo *)( (UChar *)sTableInfo + ID_SIZEOF( sdiTableInfoList ) );
        sTableInfo->mNext      = NULL;

        idlOS::memcpy( sTableInfo->mTableInfo,
                       sTableInfoList->mTableInfo,
                       ID_SIZEOF( sdiTableInfo ) );

        if ( sHead == NULL )
        {
            sHead = sTableInfo;
            sTail = sTableInfo;
        }
        else
        {
            sTail->mNext = sTableInfo;
            sTail        = sTableInfo;
        }
    }

    if ( aDstTableInfoList != NULL )
    {
        *aDstTableInfoList = sHead;
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NULL_STATEMENT )
    {
        IDE_SET( ideSetErrorCode( sdERR_ABORT_SDC_UNEXPECTED_ERROR,
                                  "sdo::allocAndCopyTableInfoList",
                                  "statement is NULL" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC sdo::makeAndSetAnalyzeInfoFromStatement( qcStatement * aStatement )
{
/***********************************************************************
 *
 * Description : TASK-7219 Shard Transformer Refactoring
 *                Shard Transform �� �Ϸ��� ��,
 *                 Shard Plan ����� ���� AnalyzeInfo �� �����ϴ� �Լ��̴�.
 *
 *                  �̹� ������ AnalyzeInfo �� �ִٸ� �ǳʶٰ�,
 *                   ���� ��쿡 ������ ���� �����ϰų�,
 *                    ���� ParseTree �� Analysis ������ �����Ѵ�.
 *
 * Implementation : 1.   Select �� ��쿡, Analysis ������ ������ ���� Ȯ���Ѵ�.
 *                  1.1. Analysis ������ AnalyzeInfo �� �����Ͽ� �����Ѵ�.
 *                  1.2. ���� ���, ���� View �� AnalyzeInfo �� �����Ѵ�.
 *                  2.   �� �ܿ��� QC_STMT_SHARD_NONE ��, AnalyzeInfo �� �����Ͽ� �����Ѵ�.
 *
 ***********************************************************************/


    sdiShardAnalysis * sAnalysis      = NULL;
    sdiAnalyzeInfo   * sAnalyzeResult = NULL;
    qcStatement      * sStatement     = NULL;
    qmsParseTree     * sSelParseTree  = NULL;
    qcParseTree      * sParseTree     = NULL;
    qciStmtType        sStmtKind      = QCI_STMT_MASK_MAX;
    qcShardStmtType    sStmtType      = QC_STMT_SHARD_NONE;

    IDE_TEST_RAISE( aStatement == NULL, ERR_NULL_STATEMENT );

    sParseTree = aStatement->myPlan->parseTree;
    sStmtKind  = sParseTree->stmtKind;
    sStmtType  = sParseTree->stmtShard;

    switch ( sStmtKind )
    {
        case QCI_STMT_SELECT:
        case QCI_STMT_SELECT_FOR_UPDATE:
            /* 1.   Select �� ��쿡, Analysis ������ ������ ���� Ȯ���Ѵ�. */
            IDE_TEST( sda::getParseTreeAnalysis( sParseTree,
                                                 &( sAnalysis ) )
                      != IDE_SUCCESS );

            /* 1.1. Analysis ������ AnalyzeInfo �� �����Ͽ� �����Ѵ�. */
            if ( sAnalysis != NULL )
            {
                IDE_TEST( sda::makeAndSetAnalyzeInfo( aStatement,
                                                      sAnalysis,
                                                      &( sAnalyzeResult ) )
                          != IDE_SUCCESS );

                aStatement->myPlan->mShardAnalysis = sAnalyzeResult;
            }
            else
            {
                /* 1.2. ���� ���, ���� View �� AnalyzeInfo �� �����Ѵ�.
                 *       �ֻ��� Statement ����
                 *        qmvShardTransform::makeShardForStatement �Լ��� ������ ���,
                 *
                 *         Transformer �� Statement �� ParseTree �� ��ü�Ͽ�
                 *          Analysis �� ���� �� �ְ�
                 *           ���� View �� �� ���� Statement �� AnalyzeInfo �� �����Ѵ�.
                 */
                sSelParseTree  = (qmsParseTree *)( sParseTree );
                sStatement     = sSelParseTree->querySet->SFWGH->from->tableRef->view;
                sAnalyzeResult = sStatement->myPlan->mShardAnalysis;

                aStatement->myPlan->mShardAnalysis = sAnalyzeResult;
            }

            break;

        case QCI_STMT_INSERT:
        case QCI_STMT_UPDATE:
        case QCI_STMT_DELETE:
        case QCI_STMT_EXEC_PROC:
            /* 2.   �� �ܿ��� QC_STMT_SHARD_NONE ��,AnalyzeInfo �� �����Ͽ� �����Ѵ�.
             *       Local Target Table ��쿡 �ش��Ѵ�.
             */
            switch ( sStmtType )
            {
                case QC_STMT_SHARD_NONE:
                    IDE_TEST( sda::getParseTreeAnalysis( sParseTree,
                                                         &( sAnalysis ) )
                              != IDE_SUCCESS );

                    IDE_TEST( sda::makeAndSetAnalyzeInfo( aStatement,
                                                          sAnalysis,
                                                          &( sAnalyzeResult ) )
                              != IDE_SUCCESS );

                    aStatement->myPlan->mShardAnalysis = sAnalyzeResult;

                    break;

                default:
                    sAnalyzeResult = aStatement->myPlan->mShardAnalysis;

                    break;
            }

            break;

        default:
            IDE_RAISE( ERR_INVALID_STMTKIND );

            break;
    }

    IDE_TEST( setPrintInfoFromAnalyzeInfo( aStatement,
                                           sAnalyzeResult )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NULL_STATEMENT )
    {
        IDE_SET( ideSetErrorCode( sdERR_ABORT_SDC_UNEXPECTED_ERROR,
                                  "sdo::makeAndSetAnalyzeInfoFromStatement",
                                  "statement is NULL" ) );
    }
    IDE_EXCEPTION( ERR_INVALID_STMTKIND )
    {
        IDE_SET( ideSetErrorCode( sdERR_ABORT_SDC_UNEXPECTED_ERROR,
                                  "sdo::makeAndSetAnalyzeInfoFromStatement",
                                  "stmtkind is invalid" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC sdo::makeAndSetAnalyzeInfoFromParseTree( qcStatement     * aStatement,
                                                qcParseTree     * aParseTree,
                                                sdiAnalyzeInfo ** aAnalyzeInfo )
{
/***********************************************************************
 *
 * Description : TASK-7219 Shard Transformer Refactoring
 *                Parse Tree ���ؿ��� Shard View �� ��ȯ�� ��,
 *                 AnalyzeInfo �� �����ϴ� �Լ��̴�.
 *
 *                  Shard Keyword ��� ���ο� ����,
 *                   ������ AnalyzeInfo �� �����ؾ� �Ѵ�.
 *                    - sda::makeAndSetAnalyzeInfo
 *                    - sdo::makeAndSetAnalyzeInfoFromNodeInfo
 *
 *                    �Ʒ��� �Լ����� ȣ���Ѵ�.
 *                     - qmvShardTransform::rebuildTransform
 *                     - qmvShardTransform::makeShardForConvert
 *                     - qmvShardTransform::makeShardForStatement
 *
 * Implementation : 1. Analysis ������ AnalyzeInfo �� �����Ѵ�.
 *                  2. Analysis ������ AnalyzeInfo �� �����ϰ�, �����Ѵ�.
 *                  3. NodeInfo ������ AnalyzeInfo �� �����Ѵ�.
 *
 ***********************************************************************/

    sdiShardAnalysis * sAnalysis      = NULL;
    sdiAnalyzeInfo   * sAnalyzeResult = NULL;
    qcShardStmtType    sStmtType      = QC_STMT_SHARD_NONE;
    qcuSqlSourceInfo   sqlInfo;

    IDE_TEST_RAISE( aParseTree == NULL, ERR_NULL_PARSETREE );

    sStmtType = aParseTree->stmtShard;

    switch ( sStmtType )
    {
        case QC_STMT_SHARD_NONE:
            /* 1. Analysis ������ AnalyzeInfo �� �����Ѵ�. */
            IDE_TEST( sda::getParseTreeAnalysis( aParseTree,
                                                 &( sAnalysis ) )
                      != IDE_SUCCESS );

            IDE_TEST( sda::makeAndSetAnalyzeInfo( aStatement,
                                                  sAnalysis,
                                                  &( sAnalyzeResult ) )
                      != IDE_SUCCESS );

            break;

        case QC_STMT_SHARD_ANALYZE:
            /* 2. Analysis ������ AnalyzeInfo �� �����ϰ�, �����Ѵ�. */
            IDE_TEST( sda::getParseTreeAnalysis( aParseTree,
                                                 &( sAnalysis ) )
                      != IDE_SUCCESS );

            IDE_TEST( sda::makeAndSetAnalyzeInfo( aStatement,
                                                  sAnalysis,
                                                  &( sAnalyzeResult ) )
              != IDE_SUCCESS );

            if ( ( sAnalyzeResult->mSplitMethod == SDI_SPLIT_NONE )
                 ||
                 ( sAnalyzeResult->mNonShardQueryReason == SDI_MULTI_SHARD_INFO_EXISTS ) )
            {
                IDE_TEST( raiseInvalidShardQueryError( aStatement,
                                                       aParseTree )
                          != IDE_SUCCESS );
            }
            else
            {
                /* Nothing to do */
            }
            break;

        case QC_STMT_SHARD_DATA:
            /* 3. NodeInfo ������ AnalyzeInfo �� �����Ѵ�. */
            IDE_TEST( makeAndSetAnalyzeInfoFromNodeInfo( aStatement,
                                                         aParseTree->nodes,
                                                         &( sAnalyzeResult ) )
                      != IDE_SUCCESS );
            break;

        default:
            IDE_RAISE( ERR_UNEXPECTED );
            break;
    }

    if ( aAnalyzeInfo != NULL )
    {
        *aAnalyzeInfo = sAnalyzeResult;
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NULL_PARSETREE )
    {
        IDE_SET( ideSetErrorCode( sdERR_ABORT_SDC_UNEXPECTED_ERROR,
                                  "sdo::makeAndSetAnalyzeInfoFromParseTree",
                                  "parse tree is NULL" ) );
    }
    IDE_EXCEPTION( ERR_UNEXPECTED )
    {
        IDE_SET( ideSetErrorCode( sdERR_ABORT_SDC_UNEXPECTED_ERROR,
                                  "sdo::makeAndSetAnalyzeInfoFromParseTree",
                                  "shard type is invalid" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC sdo::makeAndSetAnalyzeInfoFromQuerySet( qcStatement     * aStatement,
                                               qmsQuerySet     * aQuerySet,
                                               sdiAnalyzeInfo ** aAnalyzeInfo )
{
/***********************************************************************
 *
 * Description : TASK-7219 Shard Transformer Refactoring
 *                Query ���ؿ��� Shard View �� ��ȯ�� ��,
 *                 AnalyzeInfo �� �����ϴ� �Լ��̴�.
 *
 * Implementation :
 *
 ***********************************************************************/

    sdiAnalyzeInfo * sAnalyzeResult = NULL;

    IDE_TEST_RAISE( aQuerySet == NULL, ERR_NULL_QUERYSET );

    IDE_TEST( sda::makeAndSetAnalyzeInfo( aStatement,
                                          aQuerySet->mShardAnalysis,
                                          &( sAnalyzeResult ) )
              != IDE_SUCCESS );

    if ( aAnalyzeInfo != NULL )
    {
        *aAnalyzeInfo = sAnalyzeResult;
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NULL_QUERYSET )
    {
        IDE_SET( ideSetErrorCode( sdERR_ABORT_SDC_UNEXPECTED_ERROR,
                                  "sdo::makeAndSetAnalyzeInfoFromQuerySet",
                                  "query set is NULL" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC sdo::makeAndSetAnalyzeInfoFromNodeInfo( qcStatement     * aStatement,
                                               qcShardNodes    * aNodeInfo,
                                               sdiAnalyzeInfo ** aAnalyzeInfo )
{
/***********************************************************************
 *
 * Description : TASK-7219 Shard Transformer Refactoring
 *                Parse Tree �� Node ���� ��������
 *                 Shard Analyze Info �� �����ϴ� �Լ��̴�.
 *
 * Implementation : 1. �м������ ������� ����� �м������ ��ü�Ѵ�.
 *                  2. Node ������ Analyze Info �� �����Ѵ�.
 *
 ***********************************************************************/

    sdiAnalyzeInfo * sAnalyzeResult = NULL;

    IDE_TEST_RAISE( aStatement == NULL, ERR_NULL_STATEMENT );
    IDE_TEST_RAISE( aStatement->mShardQuerySetList == NULL, ERR_NULL_LIST );

    /* 1. �м������ ������� ����� �м������ ��ü�Ѵ�. */
    if ( aNodeInfo == NULL )
    {
        sAnalyzeResult = sdi::getAnalysisResultForAllNodes();
    }
    else
    {
        /* 2. Node ������ Analyze Info �� �����Ѵ�. */
        IDE_TEST( QC_QMP_MEM( aStatement )->alloc( ID_SIZEOF( sdiAnalyzeInfo ),
                                                   (void **)&( sAnalyzeResult ) )
                  != IDE_SUCCESS );

        SDI_INIT_ANALYZE_INFO( sAnalyzeResult );

        IDE_TEST( sdi::validateNodeNames( aStatement,
                                          aNodeInfo )
                  != IDE_SUCCESS );

        sAnalyzeResult->mSplitMethod         = SDI_SPLIT_NODES;
        sAnalyzeResult->mNodeNames           = aNodeInfo;
        sAnalyzeResult->mIsShardQuery        = ID_FALSE;
        sAnalyzeResult->mNonShardQueryReason = SDI_SHARD_KEYWORD_EXISTS;
    }

    if ( aAnalyzeInfo != NULL )
    {
        *aAnalyzeInfo = sAnalyzeResult;
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NULL_STATEMENT )
    {
        IDE_SET( ideSetErrorCode( sdERR_ABORT_SDC_UNEXPECTED_ERROR,
                                  "sdo::makeAndSetAnalyzeInfoFromNodeInfo",
                                  "statement is NULL" ) );
    }
    IDE_EXCEPTION( ERR_NULL_LIST )
    {
        IDE_SET( ideSetErrorCode( sdERR_ABORT_SDC_UNEXPECTED_ERROR,
                                  "sdo::makeAndSetAnalyzeInfoFromNodeInfo",
                                  "list is NULL" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC sdo::makeAndSetAnalyzeInfoFromObjectInfo( qcStatement     * aStatement,
                                                 sdiObjectInfo   * aShardObjInfo,
                                                 sdiAnalyzeInfo ** aAnalyzeInfo )
{
/***********************************************************************
 *
 * Description : TASK-7219 Shard Transformer Refactoring
 *                Shard Object Info ���ط� Shard Analyze Info �� �����ϴ� �Լ��̴�.
 *
 * Implementation :
 *
***********************************************************************/

    sdiAnalyzeInfo * sAnalysisResult = NULL;

    IDE_TEST_RAISE( aStatement == NULL, ERR_NULL_STATEMENT );
    IDE_TEST_RAISE( aShardObjInfo == NULL, ERR_NULL_INFO );

    IDE_TEST( QC_QMP_MEM( aStatement )->alloc( ID_SIZEOF( sdiAnalyzeInfo ),
                                               (void **)&( sAnalysisResult ) )
              != IDE_SUCCESS );

    SDI_INIT_ANALYZE_INFO( sAnalysisResult );

    sAnalysisResult->mIsShardQuery   = ID_TRUE;
    sAnalysisResult->mSplitMethod    = aShardObjInfo->mTableInfo.mSplitMethod;
    sAnalysisResult->mKeyDataType    = aShardObjInfo->mTableInfo.mKeyDataType;
    sAnalysisResult->mSubKeyExists   = aShardObjInfo->mTableInfo.mSubKeyExists;
    sAnalysisResult->mSubSplitMethod = aShardObjInfo->mTableInfo.mSubSplitMethod;
    sAnalysisResult->mSubKeyDataType = aShardObjInfo->mTableInfo.mSubKeyDataType;
    sAnalysisResult->mDefaultNodeId  = aShardObjInfo->mTableInfo.mDefaultNodeId;

    IDE_TEST( sdi::allocAndCopyRanges( aStatement,
                                       &( sAnalysisResult->mRangeInfo ),
                                       &( aShardObjInfo->mRangeInfo ) )
              != IDE_SUCCESS );

    IDE_TEST( QC_QMP_MEM( aStatement )->alloc( ID_SIZEOF( sdiTableInfoList ),
                                               (void **)&( sAnalysisResult->mTableInfoList ) )
              != IDE_SUCCESS );

    sAnalysisResult->mTableInfoList->mTableInfo = &( aShardObjInfo->mTableInfo );
    sAnalysisResult->mTableInfoList->mNext      = NULL;

    if ( aAnalyzeInfo != NULL )
    {
        *aAnalyzeInfo = sAnalysisResult;
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NULL_STATEMENT )
    {
        IDE_SET( ideSetErrorCode( sdERR_ABORT_SDC_UNEXPECTED_ERROR,
                                  "sdo::makeAndSetAnalyzeInfoFromObjectInfo",
                                  "statement is NULL" ) );
    }
    IDE_EXCEPTION( ERR_NULL_INFO )
    {
        IDE_SET( ideSetErrorCode( sdERR_ABORT_SDC_UNEXPECTED_ERROR,
                                  "sdo::makeAndSetAnalyzeInfoFromObjectInfo",
                                  "shard object info is NULL" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC sdo::isShardParseTree( qcParseTree * aParseTree,
                              idBool      * aIsShardParseTree )
{
/***********************************************************************
 *
 * Description : TASK-7219 Shard Transformer Refactoring
 *                ���޹��� ParseTree �� Shard ���θ� �˻��ϴ� �Լ�
 *
 *                 sda::isShardQuery �� NonShardQueryReason �� ���,
 *                  sdo::isShradParseTree �� SubKey �� Reason Flag �� ����ؼ� �˻��Ѵ�.
 *
 * Implementation : 1. Shard Analysis �� �����´�.
 *                  2. Main, SubKey ������ Shard ���θ� �˻��Ѵ�.
 *                  3. ��� Shard ���� Shard �̴�.
 *                  4. SDI_OUT_DEP_INFO_EXISTS �� ���, Non-Shard �̴�.
 *                  5. Shard ���θ� �˻��Ѵ�.
 *
 ***********************************************************************/

    sdiShardAnalysis * sAnalysis         = NULL;
    sdiShardAnalysis * sTarget           = NULL;
    idBool             sIsShardParseTree = ID_TRUE;

    /* 1. Shard Analysis �� �����´�. */
    IDE_TEST( sda::getParseTreeAnalysis( aParseTree,
                                         &( sAnalysis ) )
              != IDE_SUCCESS );

    if ( sAnalysis != NULL )
    {
        /* 2. Main, SubKey ������ Shard ParseTree ���θ� �˻��Ѵ�. */
        for ( sTarget  = sAnalysis;
              sTarget != NULL;
              sTarget  = sTarget->mNext )
        {
            /* 3. ��� Shard ParseTree ���� Shard ParseTree �̴�. */
            if ( sIsShardParseTree == ID_TRUE )
            {
                /* 4. SDI_OUT_DEP_INFO_EXISTS �� ���, Non-Shard �̴�.
                 *  SDI_OUT_DEP_INFO_EXISTS �� Outer Dependency ������ Ȯ���� �� �ִ�.
                 *   �ش� QuerySet �� Syntax Error �� �߻��� ��Ұ� �־�,
                 *    Non-Shard ó���� �� �ʿ䰡 �ִ�.
                 *
                 *     SELECT I1
                 *      FROM T1 TA
                 *       WHERE EXISTS ( SELECT I2 FROM T1 TB WHERE TA.I2 = TB.I1 );
                 *                      |--------------------------------------|
                 *                                                 **           \
                 *                                                  \        aQuerySet
                 *                                             Syntax Error
                 */
                if( sTarget->mAnalysisFlag.mSetQueryFlag[ SDI_SQ_OUT_DEP_INFO ] == ID_TRUE )
                {
                    sIsShardParseTree = ID_FALSE;
                }
                else
                {
                    /* 5. Shard ���θ� �˻��Ѵ�. */
                    IDE_TEST( sda::isShardQuery( &( sTarget->mAnalysisFlag ),
                                                 &( sIsShardParseTree ) )
                              != IDE_SUCCESS );
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
        sIsShardParseTree = ID_FALSE;
    }

    if ( aIsShardParseTree != NULL )
    {
        *aIsShardParseTree = sIsShardParseTree;
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC sdo::isShardQuerySet( qmsQuerySet * aQuerySet,
                             idBool      * aIsShardQuerySet,
                             idBool      * aIsTransformAble )
{
/***********************************************************************
 *
 * Description : TASK-7219 Shard Transformer Refactoring
 *                ���޹��� QuerySet �� Shard QuerySet, TransformAble ���θ� �˻��ϴ� �Լ�
 *
 * Implementation : 1. Shard Analysis �� �����´�.
 *                  2. Main, SubKey ������ Shard ParseTree ���θ� �˻��Ѵ�.
 *                  3. ��� Shard QuerySet ���� Shard QuerySet �̴�.
 *                  4. Shard QuerySet �� �˻��Ѵ�.
 *                  5. ��� TransformAble ���� TransformAble �̴�.
 *                  6. TransformAble �� �˻��Ѵ�.
 *
 ***********************************************************************/

    sdiShardAnalysis * sAnalysis        = NULL;
    sdiShardAnalysis * sTarget          = NULL;
    idBool             sIsShardQuerySet = ID_TRUE;
    idBool             sIsTransformAble = ID_TRUE;

    IDE_TEST_RAISE( aQuerySet == NULL, ERR_NULL_QUERYSET );

    /* 1. Shard Analysis �� �����´�. */
    sAnalysis = aQuerySet->mShardAnalysis;

    if ( sAnalysis != NULL )
    {
        /* 2. Main, SubKey ������ Shard QuerySet, TransformAble ���θ� �˻��Ѵ�. */
        for ( sTarget  = sAnalysis;
              sTarget != NULL;
              sTarget  = sTarget->mNext )
        {
            /* 3. ��� Shard QuerySet ���� Shard QuerySet �̴�. */
            if ( sIsShardQuerySet == ID_TRUE )
            {
                /* 4. Shard QuerySet �� �˻��Ѵ�. */
                if ( sTarget->mAnalysisFlag.mSetQueryFlag[ SDI_SQ_NON_SHARD ] == ID_TRUE )
                {
                    sIsShardQuerySet = ID_FALSE;
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

            /* 5. ��� TransformAble ���� TransformAble �̴�. */
            if ( sIsTransformAble == ID_TRUE )
            {
                /* 6. TransformAble �� �˻��Ѵ�. */
                if ( ( SDU_SHARD_AGGREGATION_TRANSFORM_ENABLE == 0 )
                     ||
                     ( sTarget->mAnalysisFlag.mCurQueryFlag[ SDI_CQ_AGGR_TRANSFORMABLE ] == ID_FALSE ) )
                {
                    sIsTransformAble = ID_FALSE;
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
    else
    {
        sIsShardQuerySet = ID_FALSE;
        sIsTransformAble = ID_FALSE;
    }

    if ( aIsShardQuerySet != NULL )
    {
        *aIsShardQuerySet = sIsShardQuerySet;
    }
    else
    {
        /* Nothing to do */
    }

    if ( aIsTransformAble != NULL )
    {
        *aIsTransformAble = sIsTransformAble;
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NULL_QUERYSET )
    {
        IDE_SET( ideSetErrorCode( sdERR_ABORT_SDC_UNEXPECTED_ERROR,
                                  "sdo::isShardQuerySet",
                                  "query set is NULL" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC sdo::isShardObject( qcParseTree * aParseTree,
                           idBool      * aIsShardObject )
{
/***********************************************************************
 *
 * Description : TASK-7219 Shard Transformer Refactoring
 *                ���޹��� ParseTree �� Shard Object ���θ� �˻��ϴ� �Լ�
 *
 *                 INSERT, DELETE, UPDATE �� Target Table �Ӽ��� �˻��Ѵ�.
 *
 * Implementation : 1. Shard Analysis �� �����´�.
 *                  2. Main, SubKey ������ Shard Object ���θ� �˻��Ѵ�.
 *                  3. ��� Shard Object ���� Shard Object�̴�.
 *                  4. Shard Object �� �˻��Ѵ�.
 *
 ***********************************************************************/

    sdiShardAnalysis * sAnalysis      = NULL;
    sdiShardAnalysis * sTarget        = NULL;
    idBool             sIsShardObject = ID_TRUE;

    /* 1. Shard Analysis �� �����´�.*/
    IDE_TEST( sda::getParseTreeAnalysis( aParseTree,
                                         &( sAnalysis ) )
              != IDE_SUCCESS );

    if ( sAnalysis != NULL )
    {
        /* 2. Main, SubKey ������ Shard Object ���θ� �˻��Ѵ�. */
        for ( sTarget  = sAnalysis;
              sTarget != NULL;
              sTarget  = sTarget->mNext )
        {
            /* 3. ��� Shard Object ���� Shard Object�̴�. */
            if ( sIsShardObject == ID_TRUE )
            {
                /* 4. Shard Object �� �˻��Ѵ�. */
                if ( sTarget->mAnalysisFlag.mCurQueryFlag[ SDI_CQ_LOCAL_TABLE ] == ID_TRUE )
                {
                    sIsShardObject = ID_FALSE;
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
    else
    {
        sIsShardObject = ID_FALSE;
    }

    if ( aIsShardObject != NULL )
    {
        *aIsShardObject = sIsShardObject;
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC sdo::isSupported( qmsQuerySet * aQuerySet,
                         idBool      * aIsSupported )
{
/***********************************************************************
 *
 * Description : TASK-7219 Shard Transformer Refactoring
 *                ���޹��� QuerySet �� Supported ���θ� �˻��ϴ� �Լ�
 *
 *                 Shard Keyword, With Alias �� ���Ե� Query �� Unsupported �̴�.
 *
 * Implementation : 1. Shard Analysis �� �����´�.
 *                  2. Main, SubKey ������ Supported ���θ� �˻��Ѵ�.
 *                  3. ��� Supported ���� Supported �̴�.
 *                  4. Supported �� �˻��Ѵ�.
 *
 ***********************************************************************/

    sdiShardAnalysis * sAnalysis    = NULL;
    sdiShardAnalysis * sTarget      = NULL;
    idBool             sIsSupported = ID_TRUE;

    IDE_TEST_RAISE( aQuerySet == NULL, ERR_NULL_QUERYSET );

    /* 1. Shard Analysis �� �����´�. */
    sAnalysis = aQuerySet->mShardAnalysis;

    if ( sAnalysis != NULL )
    {
        /* 2. Main, SubKey ������ Supported ���θ� �˻��Ѵ�. */
        for ( sTarget  = sAnalysis;
              sTarget != NULL;
              sTarget  = sTarget->mNext )
        {
            /* 3. ��� Supported ���� Supported �̴�. */
            if ( sIsSupported == ID_TRUE )
            {
                /* 4. Supported �� �˻��Ѵ�. */
                if ( sTarget->mAnalysisFlag.mSetQueryFlag[ SDI_SQ_UNSUPPORTED ] == ID_TRUE )
                {
                    sIsSupported = ID_FALSE;
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
    else
    {
        sIsSupported = ID_FALSE;
    }

    if ( aIsSupported != NULL )
    {
        *aIsSupported = sIsSupported;
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NULL_QUERYSET )
    {
        IDE_SET( ideSetErrorCode( sdERR_ABORT_SDC_UNEXPECTED_ERROR,
                                  "sdo::isShardQuerySet",
                                  "query set is NULL" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC sdo::setPrintInfoFromAnalyzeInfo( qcStatement    * aStatement,
                                         sdiAnalyzeInfo * aAnalyzeInfo )
{
/***********************************************************************
 *
 * Description : TASK-7219 Shard Transformer Refactoring
 *                Analyze Info �������� Plan ������ ���
 *
 * Implementation :
 *
 ***********************************************************************/


    sdiPrintInfo * sPrintInfo = NULL;

    IDE_TEST_RAISE( aStatement == NULL, ERR_NULL_STATEMENT );
    IDE_TEST_RAISE( aAnalyzeInfo == NULL, ERR_NULL_ANALYZE_INFO );

    sPrintInfo = &( QC_SHARED_TMPLATE( aStatement )->stmt->mShardPrintInfo );

    IDE_TEST_RAISE( sPrintInfo == NULL, ERR_NULL_PRINT_INFO );

    sda::setPrintInfoFromAnalyzeInfo( sPrintInfo, aAnalyzeInfo );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NULL_STATEMENT )
    {
        IDE_SET( ideSetErrorCode( sdERR_ABORT_SDC_UNEXPECTED_ERROR,
                                  "sdo::setPrintInfoFromAnalyzeInfo",
                                  "statement is NULL" ) );
    }
    IDE_EXCEPTION( ERR_NULL_ANALYZE_INFO )
    {
        IDE_SET( ideSetErrorCode( sdERR_ABORT_SDC_UNEXPECTED_ERROR,
                                  "sdo::setPrintInfoFromAnalyzeInfo",
                                  "analyze info is NULL" ) );
    }
    IDE_EXCEPTION( ERR_NULL_PRINT_INFO )
    {
        IDE_SET( ideSetErrorCode( sdERR_ABORT_SDC_UNEXPECTED_ERROR,
                                  "sdo::setPrintInfoFromAnalyzeInfo",
                                  "print info is NULL" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC sdo::setPrintInfoFromTransformAble( qcStatement * aStatement )
{
/***********************************************************************
 *
 * Description : TASK-7219 Shard Transformer Refactoring
 *                TransformAble ���θ� Plan ������ ���
 *
 * Implementation :
 *
 ***********************************************************************/

    sdiPrintInfo * sPrintInfo = NULL;

    IDE_TEST_RAISE( aStatement == NULL, ERR_NULL_STATEMENT );

    sPrintInfo = &( QC_SHARED_TMPLATE( aStatement )->stmt->mShardPrintInfo );

    IDE_TEST_RAISE( sPrintInfo == NULL, ERR_NULL_PRINT_INFO );

    sPrintInfo->mTransformable = ID_TRUE;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NULL_STATEMENT )
    {
        IDE_SET( ideSetErrorCode( sdERR_ABORT_SDC_UNEXPECTED_ERROR,
                                  "sdo::setPrintInfoFromTransformAble",
                                  "statement is NULL" ) );
    }
    IDE_EXCEPTION( ERR_NULL_PRINT_INFO )
    {
        IDE_SET( ideSetErrorCode( sdERR_ABORT_SDC_UNEXPECTED_ERROR,
                                  "sdo::setPrintInfoFromTransformAble",
                                  "print info is NULL" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC sdo::raiseInvalidShardQueryError( qcStatement * aStatement,
                                         qcParseTree * aParseTree )
{
/***********************************************************************
 *
 * Description : TASK-7219 Shard Transformer Refactoring
 *                Invalid Shard Query Error �� �߻���Ų��.
 *
 * Implementation : 1. Shard Analysis �� �����´�.
 *                  2. Error Text �� �����Ѵ�.
 *                  3. Error �� �߻���Ų��.
 *
 ***********************************************************************/

    UShort             sErrIdx   = ID_USHORT_MAX;
    sdiShardAnalysis * sAnalysis = NULL;
    qcNamePosition   * sStmtPos  = NULL;
    qcuSqlSourceInfo   sqlInfo;

    IDE_TEST_RAISE( aStatement == NULL, ERR_NULL_STATEMENT );
    IDE_TEST_RAISE( aParseTree == NULL, ERR_NULL_PARSETREE );

    sStmtPos  = &( aParseTree->stmtPos );

    /* 1. Shard Analysis �� �����´�.*/
    IDE_TEST( sda::getParseTreeAnalysis( aParseTree,
                                         &( sAnalysis ) )
              != IDE_SUCCESS );

    IDE_TEST_RAISE( sAnalysis == NULL, ERR_NULL_ANALYSIS );

    /* 2. Error Text �� �����Ѵ�. */
    (void)sda::setNonShardQueryReasonErr( &( sAnalysis->mAnalysisFlag ),
                                          &( sErrIdx ) );

    sqlInfo.setSourceInfo( aStatement, sStmtPos );

    /* 3. Error �� �߻���Ų��. */
    IDE_RAISE( ERR_INVALID_SHARD_QUERY );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NULL_STATEMENT )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "sdo::raiseInvalidShardQueryError",
                                  "statement is NULL" ) );
    }
    IDE_EXCEPTION( ERR_NULL_PARSETREE )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "sdo::raiseInvalidShardQueryError",
                                  "parse tree is NULL" ) );
    }
    IDE_EXCEPTION( ERR_NULL_ANALYSIS )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "sdo::raiseInvalidShardQueryError",
                                  "analysis is NULL" ) );
    }
    IDE_EXCEPTION( ERR_INVALID_SHARD_QUERY )
    {
        (void)sqlInfo.initWithBeforeMessage( aStatement->qmeMem );
        IDE_SET( ideSetErrorCode( sdERR_ABORT_INVALID_SHARD_QUERY,
                                  sqlInfo.getBeforeErrMessage(),
                                  sqlInfo.getErrMessage() ) );
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC sdo::preAnalyzeQuerySet( qcStatement * aStatement,
                                qmsQuerySet * aQuerySet,
                                ULong         aSMN )
{
/***********************************************************************
 *
 * Description : TASK-7219 Shard Transformer Refactoring
 *                Partial Analyze �� �����ϴ� �Լ�.
 *
 *                 Query Set List �� ������ Query Set ��, Pre Analysis �� �ʿ��� ���,
 *                  SDI_QUERYSET_LIST_STATE_PRE_ANALYZE �ܰ�� �����ϰ�
 *                   sda::preAnalyzeQuerySet ȣ���Ѵ�.
 *
 * Implementation : 1. Query Set List �� ��ȸ�Ѵ�.
 *                  2. �ش� Query Set �� ������� �˻��Ѵ�.
 *                  3. �̹� ������ ���� �����Ѵ�.
 *                  4. SDI_QUERYSET_LIST_STATE_PRE_ANALYZE �ܰ�� �����Ѵ�.
 *                  5. Partial �м��� �����Ѵ�.
 *
 ***********************************************************************/

    sdiQuerySetList  * sList  = NULL;
    sdiQuerySetEntry * sEntry = NULL;

    IDE_TEST_RAISE( aStatement == NULL, ERR_NULL_STATEMENT );
    IDE_TEST_RAISE( aQuerySet == NULL, ERR_NULL_QUERYSET );

    sList = aStatement->mShardQuerySetList;

    IDE_TEST_RAISE( sList == NULL, ERR_NULL_LIST );

    /* 1. Query Set List �� ��ȸ�Ѵ�. */
    for ( sEntry  = sList->mHead;
          sEntry != NULL;
          sEntry  = sEntry->mNext )
    {
        /* 2. �ش� Query Set �� ������� �˻��Ѵ�. */
        if ( sEntry->mQuerySet == aQuerySet )
        {
            /* 3. �̹� ������ ���� �����Ѵ�. */
            if ( ( aQuerySet->mShardAnalysis == NULL )
                 &&
                 ( aQuerySet->mPreAnalysis == NULL ) )
            {
                /* 4. SDI_QUERYSET_LIST_STATE_PRE_ANALYZE �ܰ�� �����Ѵ�.
                 *  SDI_QUERYSET_LIST_STATE_PRE_ANALYZE �ܰ��
                 *   Analysis ���� �� QuerySet �� mPreAnalysis �� �����ϵ��� �����Ѵ�.
                 *    ���� Ư�� ������ QuerySet �� mShardAnalysis �� mPreAnalysis �� 2���� ������ ������ �� �ִ�.
                 *
                 *     mShardAnalysis �� ����ȭ �� �м����, mPreAnalysis �� ����ȭ �� �м������ ���ȴ�.
                 *      Transform �� Non-Shard ��ȯ�� ���ؼ� mPreAnalysis �� �ʿ�� �Ѵ�.
                 */
                SDI_SET_QUERYSET_LIST_STATE( sList,
                                             SDI_QUERYSET_LIST_STATE_DUMMY_PRE_ANALYZE );

                /* 5. Partial �м��� �����Ѵ�. */
                IDE_TEST( sda::preAnalyzeQuerySet( aStatement,
                                                   aQuerySet,
                                                   aSMN )
                          != IDE_SUCCESS );

                SDI_SET_QUERYSET_LIST_STATE( sList,
                                             SDI_QUERYSET_LIST_STATE_DUMMY_ANALYZE );
            }
            else
            {
                /* Nohing to do */
            }

            break;
        }
        else
        {
            /* Nothing to do */
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NULL_STATEMENT )
    {
        IDE_SET( ideSetErrorCode( sdERR_ABORT_SDC_UNEXPECTED_ERROR,
                                  "sdo::preAnalyzeQuerySet",
                                  "statement is NULL" ) );
    }
    IDE_EXCEPTION( ERR_NULL_QUERYSET )
    {
        IDE_SET( ideSetErrorCode( sdERR_ABORT_SDC_UNEXPECTED_ERROR,
                                  "sdo::preAnalyzeQuerySet",
                                  "queryset is NULL" ) );
    }
    IDE_EXCEPTION( ERR_NULL_LIST )
    {
        IDE_SET( ideSetErrorCode( sdERR_ABORT_SDC_UNEXPECTED_ERROR,
                                  "sdo::preAnalyzeQuerySet",
                                  "list is NULL" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC sdo::preAnalyzeSubquery( qcStatement * aStatement,
                                qtcNode     * aNode,
                                ULong         aSMN )
{
/***********************************************************************
 *
 * Description : TASK-7219 Shard Transformer Refactoring
 *                Subquery �� ���ؼ� Pre Analyze �� �����ϴ� �Լ�.
 *
 *                 CFS ��ȯ�� ��쿡 �����Ѵ�.
 *
 * Implementation : 1. Subquery ��� Pre Analyze �����Ѵ�.
 *                  2. Subquery �� ������ �ִٸ� ����Ѵ�.
 *                  3. Over Column �� Subquery �� ������ �ִٸ� ����Ѵ�.
 *                  4. Next �� ����Ѵ�.
 *
 ***********************************************************************/

    qmsParseTree  * sParseTree  = NULL;
    qtcOverColumn * sOverColumn = NULL;

    if ( aNode != NULL )
    {
        /* 1. Subquery ��� Partial Analyze �����Ѵ�. */
        if ( QTC_IS_SUBQUERY( aNode ) == ID_TRUE )
        {
            sParseTree = (qmsParseTree *)( aNode->subquery->myPlan->parseTree );

            IDE_TEST( preAnalyzeQuerySet( aStatement,
                                          sParseTree->querySet,
                                          aSMN )
                      != IDE_SUCCESS );
        }
        else
        {
            /* 2. Subquery �� ������ �ִٸ� ����Ѵ�. */
            if ( QTC_HAVE_SUBQUERY( aNode ) == ID_TRUE )
            {
                IDE_TEST( preAnalyzeSubquery( aStatement,
                                              (qtcNode *)( aNode->node.arguments ),
                                              aSMN )
                          != IDE_SUCCESS );
            }
            else
            {
                /* Nothing to do. */
            }

            if ( aNode->overClause != NULL )
            {
                /* 3. Over Column �� Subquery �� ������ �ִٸ� ����Ѵ�. */
                for ( sOverColumn  = aNode->overClause->overColumn;
                      sOverColumn != NULL;
                      sOverColumn  = sOverColumn->next )
                {
                    if ( QTC_HAVE_SUBQUERY( sOverColumn->node ) == ID_TRUE )
                    {
                        IDE_TEST( preAnalyzeSubquery( aStatement,
                                                      sOverColumn->node,
                                                      aSMN )
                                  != IDE_SUCCESS );
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
        }

        /* 4. Next �� ����Ѵ�. */
        IDE_TEST( preAnalyzeSubquery( aStatement,
                                      (qtcNode *)( aNode->node.next ),
                                      aSMN )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do. */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC sdo::setShardPlanStmtVariable( qcStatement    * aStatement,
                                      qcShardStmtType  aStmtType,
                                      qcNamePosition * aStmtPos )
{
/***********************************************************************
 *
 * Description : TASK-7219 Shard Transformer Refactoring
 *                myPlan �� Statment ������ ����Ѵ�.
 *                 �Լ� ���뼺 ��ȭ �������� �����Ͽ���.
 *
 * Implementation :
 *
 ***********************************************************************/

    IDE_TEST_RAISE( aStatement == NULL, ERR_NULL_STATEMENT );
    IDE_TEST_RAISE( aStmtPos == NULL, ERR_NULL_POSITION );

    SET_POSITION( aStatement->myPlan->parseTree->stmtPos, *aStmtPos );

    switch ( aStmtType )
    {
        case QC_STMT_SHARD_NONE:
        case QC_STMT_SHARD_ANALYZE:
            aStatement->myPlan->parseTree->stmtShard = QC_STMT_SHARD_ANALYZE;
            break;

        case QC_STMT_SHARD_DATA:
            aStatement->myPlan->parseTree->stmtShard = QC_STMT_SHARD_DATA;
            break;

        default:
            IDE_RAISE( ERR_INVALID_STMT_TYPE );
            break;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NULL_STATEMENT )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "sdo::setShardPlanStmtVariable",
                                  "statement is NULL" ) );
    }
    IDE_EXCEPTION( ERR_NULL_POSITION )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "sdo::setShardPlanStmtVariable",
                                  "position is null" ) );
    }
    IDE_EXCEPTION( ERR_INVALID_STMT_TYPE )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "sdo::setShardPlanStmtVariable",
                                  "stmt type is invalid" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC sdo::setShardPlanCommVariable( qcStatement    * aStatement,
                                      sdiAnalyzeInfo * aAnalyzeInfo,
                                      UShort           aParamCount,
                                      UShort           aParamOffset,
                                      qcShardParamInfo * aParamInfo )
{
/***********************************************************************
 *
 * Description : TASK-7219 Shard Transformer Refactoring
 *                myPlan �� Shard ���� ������ ����Ѵ�.
 *                 �Լ� ���뼺 ��ȭ �������� �����Ͽ���.
 *
 * Implementation :
 *
 ***********************************************************************/

    IDE_TEST_RAISE( aStatement == NULL, ERR_NULL_STATEMENT );
    IDE_TEST_RAISE( aAnalyzeInfo == NULL, ERR_NULL_INFO );

    if ( aParamInfo == NULL )
    {
        aStatement->myPlan->mShardAnalysis         = aAnalyzeInfo;
        aStatement->myPlan->mShardParamCount       = 0;
        aStatement->myPlan->mShardParamOffset      = ID_USHORT_MAX;
        aStatement->myPlan->mShardParamInfo        = NULL;
    }
    else
    {
        aStatement->myPlan->mShardAnalysis         = aAnalyzeInfo;
        aStatement->myPlan->mShardParamCount       = aParamCount;
        aStatement->myPlan->mShardParamOffset      = aParamOffset;
        aStatement->myPlan->mShardParamInfo        = aParamInfo;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NULL_STATEMENT )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "sdo::setShardPlanCommVariable",
                                  "statement is NULL" ) );
    }
    IDE_EXCEPTION( ERR_NULL_INFO )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "sdo::setShardPlanCommVariable",
                                  "analyze info is null" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC sdo::setShardStmtType( qcStatement * aStatement,
                              qcStatement * aViewStatement )
{
/***********************************************************************
 *
 * Description : TASK-7219 Shard Transformer Refactoring
 *                shardStmtType �� ���� view, subquery �� �����ϴ� �Լ�
 *
 * Implementation :
 *
 ***********************************************************************/

    qcParseTree * sParseTree     = NULL;
    qcParseTree * sViewParseTree = NULL;

    IDE_TEST_RAISE( aStatement == NULL, ERR_STATEMENT_NULL_1 );
    IDE_TEST_RAISE( aViewStatement == NULL, ERR_STATEMENT_NULL_2 );

    sParseTree     = aStatement->myPlan->parseTree;
    sViewParseTree = aViewStatement->myPlan->parseTree;

    if ( ( sViewParseTree->stmtShard == QC_STMT_SHARD_NONE )
         &&
         ( sParseTree->stmtShard != QC_STMT_SHARD_NONE ) )
    {
        /* Non Shard Insert �� qmo::optimizeShardInsert ���
         *  �ֻ��� shardStmtType �� QC_STMT_SHARD_ANALYZE �� �����̹Ƿ�
         *   shardStmtType �� ������ �ʿ䰡 ����.
         *    �ֻ����� ������ Subquery �������� �����ϵ��� �Ѵ�.
         */
        if ( sParseTree->optimize != qmo::optimizeShardInsert )
        {
            sViewParseTree->stmtShard = sParseTree->stmtShard;
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

    IDE_EXCEPTION( ERR_STATEMENT_NULL_1 );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "sdo::setShardStmtType",
                                  "statement is NULL" ) );
    }
    IDE_EXCEPTION( ERR_STATEMENT_NULL_2 );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "sdo::setShardStmtType",
                                  "view statement is NULL" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
