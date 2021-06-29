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
 *
 * Description :
 *
 *     ALTIBASE SHARD manage ment function
 *
 * Syntax :
 *    SHARD_SET_SHARD_CLONE_LOCAL( user_name VARCHAR,
 *                                 table_name VARCHAR,
 *                                 node_name VARCHAR,
 *                                 is_called_by_ddl INTEGER )
 *    RETURN 0
 *
 **********************************************************************/

#include <sdf.h>
#include <sdm.h>
#include <smi.h>
#include <qcg.h>

extern mtdModule mtdInteger;
extern mtdModule mtdVarchar;

static mtcName sdfFunctionName[1] = {
    { NULL, 21, (void*)"SHARD_SET_SHARD_CLONE" }
};

static IDE_RC sdfEstimate( mtcNode*        aNode,
                           mtcTemplate*    aTemplate,
                           mtcStack*       aStack,
                           SInt            aRemain,
                           mtcCallBack*    aCallBack );

mtfModule sdfSetShardCloneModule = {
    1|MTC_NODE_OPERATOR_MISC|MTC_NODE_VARIABLE_TRUE,
    ~0,
    1.0,                    // default selectivity (�� ������ �ƴ�)
    sdfFunctionName,
    NULL,
    mtf::initializeDefault,
    mtf::finalizeDefault,
    sdfEstimate
};

IDE_RC sdfCalculate_SetShardClone( mtcNode*     aNode,
                                   mtcStack*    aStack,
                                   SInt         aRemain,
                                   void*        aInfo,
                                   mtcTemplate* aTemplate );

static const mtcExecute sdfExecute = {
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    sdfCalculate_SetShardClone,
    NULL,
    mtx::calculateNA,
    mtk::estimateRangeNA,
    mtk::extractRangeNA
};

IDE_RC sdfEstimate( mtcNode*     aNode,
                    mtcTemplate* aTemplate,
                    mtcStack*    aStack,
                    SInt      /* aRemain */,
                    mtcCallBack* aCallBack )
{
    const mtdModule* sModules[5] =
    {
        &mtdVarchar, // user_name
        &mtdVarchar, // table_name
        &mtdVarchar, // ndoe_name
        &mtdInteger, // replicaset_id
        &mtdInteger  // called_by
    };
    const mtdModule* sModule = &mtdInteger;

    IDE_TEST_RAISE( ( aNode->lflag & MTC_NODE_QUANTIFIER_MASK ) ==
                    MTC_NODE_QUANTIFIER_TRUE,
                    ERR_NOT_AGGREGATION );

    IDE_TEST_RAISE( ( aNode->lflag & MTC_NODE_ARGUMENT_COUNT_MASK ) != 5,
                    ERR_INVALID_FUNCTION_ARGUMENT );

    IDE_TEST( mtf::makeConversionNodes( aNode,
                                        aNode->arguments,
                                        aTemplate,
                                        aStack + 1,
                                        aCallBack,
                                        sModules )
              != IDE_SUCCESS );

    aStack[0].column = aTemplate->rows[aNode->table].columns + aNode->column;

    IDE_TEST( mtc::initializeColumn( aStack[0].column,
                                     sModule,
                                     0,
                                     0,
                                     0 )
              != IDE_SUCCESS );

    aTemplate->rows[aNode->table].execute[aNode->column] = sdfExecute;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NOT_AGGREGATION );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_NOT_AGGREGATION));

    IDE_EXCEPTION( ERR_INVALID_FUNCTION_ARGUMENT );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_INVALID_FUNCTION_ARGUMENT));

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC sdfCalculate_SetShardClone( mtcNode*     aNode,
                                   mtcStack*    aStack,
                                   SInt         aRemain,
                                   void*        aInfo,
                                   mtcTemplate* aTemplate )
{
/***********************************************************************
 *
 * Description :
 *     Add Shard Node
 *
 * Implementation :
 *      Argument Validation
 *      Begin Statement for meta
 *      Check Privilege
 *      End Statement
 *
 ***********************************************************************/

    qcStatement             * sStatement;
    mtdCharType             * sUserName;
    SChar                     sUserNameStr[QC_MAX_OBJECT_NAME_LEN + 1];
    mtdCharType             * sTableName;
    SChar                     sTableNameStr[QC_MAX_OBJECT_NAME_LEN + 1];
    mtdCharType             * sNodeName;
    SChar                     sNodeNameStr[SDI_NODE_NAME_MAX_SIZE + 1];
    UInt                      sRowCnt = 0;
    smiStatement            * sOldStmt;
    smiStatement              sSmiStmt;
    UInt                      sSmiStmtFlag;
    SInt                      sState = 0;
    idBool                    sIsOldSessionShardMetaTouched = ID_FALSE;

    UInt                      sReplicaSetId = SDI_REPLICASET_NULL_ID;
    sdiInternalOperation      sInternalOP = SDI_INTERNAL_OP_NOT;

    ULong                     sSMN = ID_ULONG(0);
    idBool                    sIsTableFound = ID_FALSE;
    sdiTableInfo              sTableInfo;
    sdiLocalMetaInfo          sLocalMetaInfo;

    mtdIntegerType            sCalledBy   = 0;
    idBool                    sIsNewTrans = ID_FALSE;

    sStatement   = ((qcTemplate*)aTemplate)->stmt;

    sStatement->mFlag &= ~QC_STMT_SHARD_META_CHANGE_MASK;
    sStatement->mFlag |= QC_STMT_SHARD_META_CHANGE_TRUE;

    /* BUG-47623 ���� ��Ÿ ���濡 ���� trc �α��� commit �α׸� �ۼ��ϱ����� DASSERT �� �״� ��찡 �ֽ��ϴ�. */
    if ( ( sStatement->session->mQPSpecific.mFlag & QC_SESSION_SHARD_META_TOUCH_MASK ) ==
         QC_SESSION_SHARD_META_TOUCH_TRUE )
    {
        sIsOldSessionShardMetaTouched = ID_TRUE;
    }

    // BUG-46366
    IDE_TEST_RAISE( ( QC_SMI_STMT(sStatement)->getTrans() == NULL ) ||
                    ( ( sStatement->myPlan->parseTree->stmtKind & QCI_STMT_MASK_DML ) == QCI_STMT_MASK_DML ) ||
                    ( ( sStatement->myPlan->parseTree->stmtKind & QCI_STMT_MASK_DCL ) == QCI_STMT_MASK_DCL ),
                    ERR_INSIDE_QUERY );

    // Check Privilege
    IDE_TEST_RAISE( QCG_GET_SESSION_USER_ID(sStatement) != QCI_SYS_USER_ID,
                    ERR_NO_GRANT );

    if ( SDU_SHARD_LOCAL_FORCE != 1 )
    {
        /* Shard Local Operation�� internal ������ ����Ǿ�� �Ѵ�.  */
        IDE_TEST_RAISE( ( QCG_GET_SESSION_IS_SHARD_INTERNAL_LOCAL_OPERATION( sStatement ) != ID_TRUE ) &&
                        ( ( sStatement->session->mQPSpecific.mFlag & QC_SESSION_ALTER_META_MASK )
                             != QC_SESSION_ALTER_META_ENABLE),
                        ERR_INTERNAL_OPERATION );
    }

    IDE_TEST( mtf::postfixCalculate( aNode,
                                     aStack,
                                     aRemain,
                                     aInfo,
                                     aTemplate )
              != IDE_SUCCESS );
    
    if ( ( aStack[1].column->module->isNull( aStack[1].column,
                                             aStack[1].value ) == ID_TRUE ) ||
         ( aStack[2].column->module->isNull( aStack[2].column,
                                             aStack[2].value ) == ID_TRUE ) ||
         ( aStack[3].column->module->isNull( aStack[3].column,
                                             aStack[3].value ) == ID_TRUE ) )
    {
        IDE_RAISE( ERR_ARGUMENT_NOT_APPLICABLE );
    }
    else
    {
        //---------------------------------
        // Argument Validation
        //---------------------------------

        // user name
        sUserName = (mtdCharType*)aStack[1].value;

        IDE_TEST_RAISE( sUserName->length > QC_MAX_OBJECT_NAME_LEN,
                        ERR_SHARD_USER_NAME_TOO_LONG );
        idlOS::strncpy( sUserNameStr,
                        (SChar*)sUserName->value,
                        sUserName->length );
        sUserNameStr[sUserName->length] = '\0';

        // object name
        sTableName = (mtdCharType*)aStack[2].value;

        IDE_TEST_RAISE( sTableName->length > QC_MAX_OBJECT_NAME_LEN,
                        ERR_SHARD_TABLE_NAME_TOO_LONG );
        idlOS::strncpy( sTableNameStr,
                        (SChar*)sTableName->value,
                        sTableName->length );
        sTableNameStr[sTableName->length] = '\0';

        // shard group name
        sNodeName = (mtdCharType*)aStack[3].value;

        IDE_TEST_RAISE( sNodeName->length > SDI_CHECK_NODE_NAME_MAX_SIZE,
                        ERR_SHARD_GROUP_NAME_TOO_LONG );
        idlOS::strncpy( sNodeNameStr,
                        (SChar*)sNodeName->value,
                        sNodeName->length );
        sNodeNameStr[sNodeName->length] = '\0';

        // conn_type
        if ( aStack[4].column->module->isNull( aStack[4].column,
                                               aStack[4].value ) == ID_TRUE )
        {
            sReplicaSetId = SDI_REPLICASET_NULL_ID;
        }
        else
        {
            sReplicaSetId = *(mtdIntegerType*)aStack[4].value;
        }

        // Internal Option
        sInternalOP = (sdiInternalOperation)QCG_GET_SESSION_SHARD_INTERNAL_LOCAL_OPERATION( sStatement );

        // TASK-7307: is_called_by_ddl
        if ( aStack[5].column->module->isNull( aStack[5].column,
                                               aStack[5].value ) == ID_TRUE )
        {
            sCalledBy = SDM_CALLED_BY_SHARD_PKG;
        }
        else
        {
            sCalledBy = *(mtdIntegerType*)aStack[5].value;

            IDE_TEST_RAISE( ( sCalledBy > 2 ) ||
                            ( sCalledBy < 0 ),
                            ERR_ARGUMENT_NOT_APPLICABLE );
        }

        //---------------------------------
        // Begin Statement for meta
        //---------------------------------
        IDE_TEST( sdi::getIncreasedSMNForMetaNode( ( QC_SMI_STMT( sStatement ) )->getTrans() , 
                                                   &sSMN ) 
                  != IDE_SUCCESS );

        sSmiStmtFlag = SMI_STATEMENT_NORMAL | SMI_STATEMENT_MEMORY_CURSOR;
        sOldStmt                = QC_SMI_STMT(sStatement);
        QC_SMI_STMT(sStatement) = &sSmiStmt;
        sState = 1;

        IDE_TEST( sSmiStmt.begin( sStatement->mStatistics,
                                  sOldStmt,
                                  sSmiStmtFlag )
                  != IDE_SUCCESS );
        sState = 2;

        // ��ϵ� Shard ��ü���� �Ѵ�.
        IDE_TEST( sdm::getTableInfo( QC_SMI_STMT( sStatement ),
                                     sUserNameStr,
                                     sTableNameStr,
                                     sSMN, //sMetaNodeInfo.mShardMetaNumber,
                                     &sTableInfo,
                                     &sIsTableFound )
                  != IDE_SUCCESS );

        IDE_TEST_RAISE( sIsTableFound == ID_FALSE,
                        ERR_NOT_EXIST_TABLE );

        //---------------------------------
        // Insert Meta
        //---------------------------------

        IDE_TEST( sdm::insertClone( sStatement,
                                    (SChar*)sUserNameStr,
                                    (SChar*)sTableNameStr,
                                    (SChar*)sNodeNameStr,
                                    sReplicaSetId,
                                    &sRowCnt,
                                    sInternalOP)
                  != IDE_SUCCESS );

        //---------------------------------
        // End Statement
        //---------------------------------

        sState = 1;
        IDE_TEST( sSmiStmt.end( SMI_STATEMENT_RESULT_SUCCESS ) != IDE_SUCCESS );

        sState = 0;
        QC_SMI_STMT(sStatement) = sOldStmt;

        IDE_TEST_RAISE( sRowCnt == 0,
                        ERR_INVALID_SHARD_TABLE );

        // Clone�� ����ȭ�� �������� �ʴ´�.

        /* TASK-7307 DML Data Consistency in Shard
         *   �� Node���� ����ϴ� ���̺��̸� USABLE�� �����Ѵ� */
        if ( ( SDU_SHARD_LOCAL_FORCE != 1 ) &&
             ( sTableInfo.mObjectType == 'T' ) &&
             ( sCalledBy != SDM_CALLED_BY_SHARD_FAILBACK ) )
        {
            IDE_TEST( sdm::getLocalMetaInfo( &sLocalMetaInfo ) != IDE_SUCCESS );

            /* ������ Node�� �� NodeName�� ��ġ�ϸ� usable�� ���� */
            if ( idlOS::strncmp(sLocalMetaInfo.mNodeName, sNodeNameStr, SDI_NODE_NAME_MAX_SIZE + 1) == 0 )
            {
                if ( sCalledBy == SDM_CALLED_BY_SHARD_MOVE )
                {
                    sIsNewTrans = ID_FALSE;
                }
                else // called by shard_dbms package
                {
                    sIsNewTrans = ID_TRUE;
                }
                IDE_TEST( sdm::alterUsable( sStatement,
                                            sUserNameStr,
                                            sTableNameStr,
                                            NULL,   /* partition */
                                            ID_TRUE /* isUsable */,
                                            sIsNewTrans )
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
    }

    *(mtdIntegerType*)aStack[0].value = 0;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INTERNAL_OPERATION )
    {
        IDE_SET( ideSetErrorCode( sdERR_ABORT_SDC_UNEXPECTED_ERROR,
                                  "sdfCalculate_SetShardClone",
                                  "SHARD_INTERNAL_LOCAL_OPERATION is not 1" ) );
    }
    IDE_EXCEPTION( ERR_NOT_EXIST_TABLE )
    {
        IDE_SET( ideSetErrorCode( sdERR_ABORT_SDM_SHARD_TABLE_NOT_EXIST ) );
    }
    IDE_EXCEPTION( ERR_INSIDE_QUERY )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QSX_PSM_INSIDE_QUERY ) );
    }
    IDE_EXCEPTION( ERR_SHARD_USER_NAME_TOO_LONG );
    {
        IDE_SET( ideSetErrorCode( sdERR_ABORT_SDF_SHARD_USER_NAME_TOO_LONG ) );
    }
    IDE_EXCEPTION( ERR_SHARD_TABLE_NAME_TOO_LONG );
    {
        IDE_SET( ideSetErrorCode( sdERR_ABORT_SDF_SHARD_TABLE_NAME_TOO_LONG ) );
    }
    IDE_EXCEPTION( ERR_SHARD_GROUP_NAME_TOO_LONG );
    {
        IDE_SET( ideSetErrorCode( sdERR_ABORT_SDF_SHARD_NODE_NAME_TOO_LONG ) );
    }
    IDE_EXCEPTION( ERR_ARGUMENT_NOT_APPLICABLE );
    {
        IDE_SET(ideSetErrorCode(mtERR_ABORT_ARGUMENT_NOT_APPLICABLE));
    }
    IDE_EXCEPTION( ERR_INVALID_SHARD_TABLE );
    {
        IDE_SET( ideSetErrorCode( sdERR_ABORT_SDF_INVALID_SHARD_TABLE ) );
    }
    IDE_EXCEPTION( ERR_NO_GRANT )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QRC_NO_GRANT ) );
    }
    IDE_EXCEPTION_END;

    IDE_PUSH();
    
    switch ( sState )
    {
        case 2:
            if ( sSmiStmt.end(SMI_STATEMENT_RESULT_FAILURE) != IDE_SUCCESS )
            {
                IDE_ERRLOG(IDE_SD_1);
            }
            else
            {
                /* Nothing to do */
            }
        case 1:
            QC_SMI_STMT(sStatement) = sOldStmt;
        default:
            break;
    }

    /* BUG-47623 ���� ��Ÿ ���濡 ���� trc �α��� commit �α׸� �ۼ��ϱ����� DASSERT �� �״� ��찡 �ֽ��ϴ�. */
    if ( sIsOldSessionShardMetaTouched == ID_TRUE )
    {
        sdi::setShardMetaTouched( sStatement->session );
    }
    else
    {
        sdi::unsetShardMetaTouched( sStatement->session );
    }

    IDE_POP();
    
    return IDE_FAILURE;
}
