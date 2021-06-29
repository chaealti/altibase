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

#include <mmErrorCode.h>
#include <mmcConv.h>
#include <mmcSession.h>
#include <mmcStatement.h>
#include <mmcStatementManager.h>
#include <mmcTask.h>
#include <mmtServiceThread.h>
#include <mmqManager.h>
#include <mmdXa.h>
#include <mmtAuditManager.h>

typedef struct mmtCmsExecuteContext
{
    cmiProtocolContext *mProtocolContext;
    mmcStatement       *mStatement;
    SLong               mAffectedRowCount;
    SLong               mFetchedRowCount;
    idBool              mSuspended;
    UChar              *mCollectionData;
    UInt                mCursor; // bug-27621: pointer to UInt
} mmtCmsExecuteContext;


static IDE_RC answerExecuteResult(mmtCmsExecuteContext *aExecuteContext)
{
    cmiProtocol            sProtocol;
    cmpArgDBExecuteResultA5 *sArg;

    CMI_PROTOCOL_INITIALIZE(sProtocol, sArg, DB, ExecuteResult);

    sArg->mStatementID      = aExecuteContext->mStatement->getStmtID();
    sArg->mRowNumber        = aExecuteContext->mStatement->getRowNumber();

    sArg->mResultSetCount   = aExecuteContext->mStatement->getResultSetCount();
    sArg->mAffectedRowCount = (ULong)aExecuteContext->mAffectedRowCount;

    IDE_TEST(cmiWriteProtocol(aExecuteContext->mProtocolContext,
                              &sProtocol) != IDE_SUCCESS);

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

static IDE_RC getBindParamCallback(idvSQL       * /*aStatistics*/,
                                   qciBindParam *aBindParam,
                                   void         *aData,
                                   void         *aExecuteContext)
{
    mmtCmsExecuteContext     *sExecuteContext = (mmtCmsExecuteContext *)aExecuteContext;
    mmcSession               *sSession        = sExecuteContext->mStatement->getSession();
    mmcStatement             *sStatement      = sExecuteContext->mStatement;
    cmiProtocol               sProtocol;
    cmpArgDBParamDataOutA5 *sArg;

    CMI_PROTOCOL_INITIALIZE(sProtocol, sArg, DB, ParamDataOut);

    IDE_TEST_RAISE(mmcConv::convertFromMT(sExecuteContext->mProtocolContext,
                                          &sArg->mData,
                                          aData,
                                          aBindParam->type,
                                          sSession)
                   != IDE_SUCCESS, ConversionFail);

    sArg->mStatementID = sExecuteContext->mStatement->getStmtID();
    sArg->mRowNumber   = sExecuteContext->mStatement->getRowNumber();
    sArg->mParamNumber = aBindParam->id + 1;

    // fix BUG-24881 Output Parameter�� Profiling �ؾ� �մϴ�.
    if ((idvProfile::getProfFlag() & IDV_PROF_TYPE_BIND_FLAG) == IDV_PROF_TYPE_BIND_FLAG)
    {
        // bug-25312: prepare ���Ŀ� autocommit�� off���� on���� �����ϰ�
        // bind �ϸ� stmt->mTrans �� null�̾ segv.
        // ����: stmt->mTrans�� ���Ͽ� null�̸� TransID�� 0�� �ѱ⵵�� ����
        mmcTransObj *sTrans = sSession->getTransPtr(sStatement);
        smTID sTransID = (sTrans != NULL) ? mmcTrans::getTransID(sTrans) : 0;

        idvProfile::writeBindA5( (void *)&sArg->mData,
                               sStatement->getSessionID(),
                               sStatement->getStmtID(),
                               sTransID,
                               profWriteBindCallbackA5 );
    }

    IDE_TEST(cmiWriteProtocol(sExecuteContext->mProtocolContext, &sProtocol) != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION(ConversionFail);
    {
        IDE_ASSERT(cmiFinalizeProtocol(&sProtocol) == IDE_SUCCESS);
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

static IDE_RC getBindParamListCallbackA5(idvSQL       * /*aStatistics*/,
                                         qciBindParam *aBindParam,
                                         void         *aData,
                                         void         *aExecuteContext)
{
    mmtCmsExecuteContext *sExecuteContext = (mmtCmsExecuteContext *)aExecuteContext;
    mmcSession           *sSession        = sExecuteContext->mStatement->getSession();
    mmcStatement         *sStatement      = sExecuteContext->mStatement;
    cmtAny                sAny;

    IDE_TEST( cmtAnyInitialize( &sAny ) != IDE_SUCCESS );

    IDE_TEST(mmcConv::buildAnyFromMT(&sAny,
                                     aData,
                                     aBindParam->type,
                                     sSession)
             != IDE_SUCCESS);

    // fix BUG-24881 Output Parameter�� Profiling �ؾ� �մϴ�.
    if ((idvProfile::getProfFlag() & IDV_PROF_TYPE_BIND_FLAG) == IDV_PROF_TYPE_BIND_FLAG)
    {
        // bug-25312: prepare ���Ŀ� autocommit�� off���� on���� �����ϰ�
        // bind �ϸ� stmt->mTrans �� null�̾ segv.
        // ����: stmt->mTrans�� ���Ͽ� null�̸� TransID�� 0�� �ѱ⵵�� ����
        mmcTransObj *sTrans = sSession->getTransPtr(sStatement);
        smTID sTransID = (sTrans != NULL) ? mmcTrans::getTransID(sTrans) : 0;

        idvProfile::writeBindA5( (void *)&sAny,
                               sStatement->getSessionID(),
                               sStatement->getStmtID(),
                               sTransID,
                               profWriteBindCallbackA5 );
    }

    // bug-27621: mCursor: UInt pointer -> UInt
    cmtCollectionWriteAny( sExecuteContext->mCollectionData,
                           &(sExecuteContext->mCursor),
                           &sAny );

    /*
     * CASE-13162
     * fetch�ؿ� row�� collection buffer�� cursor��ġ���� �����ϰ�
     * cursor ��ġ�� ������Ŵ. �׷��Ƿ� cursor�� ��ġ�� �����
     * collection buffer�� ũ�⸦ ���� �� ����.
     * ���� cursor�� ��ġ�� collection buffer�� ũ�⸦ �Ѿ�ٸ�
     * page�� ������ ���� abnormal�� ��Ȳ��.
     */
    IDE_ASSERT( sExecuteContext->mCursor < sSession->getChunkSize() );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

static IDE_RC answerParamDataOutListA5(mmtCmsExecuteContext *aExecuteContext)
{
    cmiProtocol                   sProtocol;
    cmpArgDBParamDataOutListA5 *sArg;

    CMI_PROTOCOL_INITIALIZE(sProtocol, sArg, DB, ParamDataOutList);

    sArg->mStatementID = aExecuteContext->mStatement->getStmtID();
    sArg->mRowNumber   = aExecuteContext->mStatement->getRowNumber();

    if( cmiCheckInVariable( aExecuteContext->mProtocolContext,
                            aExecuteContext->mCursor) == ID_TRUE )
    {
        cmtCollectionWriteInVariable( &sArg->mListData,
                                      aExecuteContext->mCollectionData,
                                      aExecuteContext->mCursor);
    }
    else
    {
        cmtCollectionWriteVariable( &sArg->mListData,
                                    aExecuteContext->mCollectionData,
                                    aExecuteContext->mCursor);
    }

    IDE_TEST(cmiWriteProtocol(aExecuteContext->mProtocolContext, &sProtocol) != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

static IDE_RC sendBindOutA5(mmcSession           *aSession,
                          mmtCmsExecuteContext *aExecuteContext)
{
    qciStatement *sQciStmt;
    qciBindParam  sBindParam;
    UShort        sParamCount;
    UInt          sOutParamSize;
    UInt          sOutParamCount;
    UShort        sParamIndex;
    UInt          sAllocSize;

    sQciStmt    = aExecuteContext->mStatement->getQciStmt();
    sParamCount = qci::getParameterCount(sQciStmt);

    IDE_TEST( qci::getOutBindParamSize( sQciStmt,
                                        &sOutParamSize,
                                        &sOutParamCount )
              != IDE_SUCCESS );
    
    IDE_TEST_CONT( sOutParamCount == 0, SKIP_SEND_BIND_OUT );
    
    if( aSession->getHasClientListChannel() == ID_TRUE )
    {
                    
        // out result�� �ʿ��� �޸� ������ estimation�Ѵ�.
        // sOutParamSize�� QP������ ������ �ǹ��ϱ� ������,
        // ��Ż� �ʿ��� ������ �ΰ������� �߰��ؾ� �Ѵ�.
        sAllocSize = sOutParamSize + (sOutParamCount * cmiGetMaxInTypeHeaderSize());
        IDE_TEST( aSession->allocChunk(IDL_MAX(sAllocSize, (MMC_DEFAULT_COLLECTION_BUFFER_SIZE)))
                        != IDE_SUCCESS );

        aExecuteContext->mCollectionData = aSession->getChunk();
        // bug-27621: mCursor: UInt pointer -> UInt
        // �������� sCursor �ּ����� ����.
        aExecuteContext->mCursor = 0;
    }
    
    for (sParamIndex = 0; sParamIndex < sParamCount; sParamIndex++)
    {
        sBindParam.id = sParamIndex;

        IDE_TEST(qci::getBindParamInfo(sQciStmt, &sBindParam) != IDE_SUCCESS);

        if ((sBindParam.inoutType == CMP_DB_PARAM_INPUT_OUTPUT) ||
            (sBindParam.inoutType == CMP_DB_PARAM_OUTPUT))
        {
            if( aSession->getHasClientListChannel() == ID_TRUE )
            {
                IDE_TEST(qci::getBindParamData(sQciStmt,
                                               sParamIndex,
                                               getBindParamListCallbackA5,
                                               aExecuteContext)
                         != IDE_SUCCESS);
            }
            else
            {
                IDE_TEST(qci::getBindParamData(sQciStmt,
                                               sParamIndex,
                                               getBindParamCallback,
                                               aExecuteContext)
                         != IDE_SUCCESS);
            }
        }
    }

    if( aSession->getHasClientListChannel() == ID_TRUE )
    {
        IDE_TEST( answerParamDataOutListA5(aExecuteContext)
                  != IDE_SUCCESS );

        // bug-27571: klocwork warnings
        // Ȥ�ó� �ؼ� �ʱ�ȭ �����ش�.
        aExecuteContext->mCollectionData = NULL;
        aExecuteContext->mCursor = 0;
    }

    IDE_EXCEPTION_CONT( SKIP_SEND_BIND_OUT );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

static IDE_RC doRetry(mmcStatement *aStmt)
{
    IDE_TEST(qci::retry(aStmt->getQciStmt(), aStmt->getSmiStmt()) != IDE_SUCCESS);

    IDE_TEST(aStmt->endStmt(MMC_EXECUTION_FLAG_RETRY) != IDE_SUCCESS);

    IDE_TEST(aStmt->beginStmt() != IDE_SUCCESS);

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

static IDE_RC doRebuild(mmcStatement *aStmt)
{
    IDE_TEST(qci::closeCursor(aStmt->getQciStmt(), aStmt->getSmiStmt()) != IDE_SUCCESS);

    IDE_TEST(aStmt->endStmt(MMC_EXECUTION_FLAG_REBUILD) != IDE_SUCCESS);

    // BUG_12177
    aStmt->setCursorFlag(SMI_STATEMENT_ALL_CURSOR);

    IDE_TEST(aStmt->beginStmt() != IDE_SUCCESS);

    IDE_TEST(aStmt->rebuild() != IDE_SUCCESS);

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

// fix BUG-30891
static IDE_RC doEnd(mmtCmsExecuteContext *aExecuteContext, mmcExecutionFlag aExecutionFlag)
{
    mmcStatement *sStmt = aExecuteContext->mStatement;

    /* PROJ-2223 Altibase Auditing */
    mmtAuditManager::auditByAccess( sStmt, aExecutionFlag );
    
    // fix BUG-30990
    // QUEUE�� EMPTY�� ��쿡�� BIND_DATA ���·� �����Ѵ�.
    switch(aExecutionFlag)
    {
        case MMC_EXECUTION_FLAG_SUCCESS :
            IDE_TEST(sStmt->clearStmt(MMC_STMT_BIND_NONE) != IDE_SUCCESS);
            break;
        case MMC_EXECUTION_FLAG_QUEUE_EMPTY :
            IDE_TEST(sStmt->clearStmt(MMC_STMT_BIND_DATA) != IDE_SUCCESS);
            break;
        default:
            IDE_TEST(sStmt->clearStmt(MMC_STMT_BIND_INFO) != IDE_SUCCESS);
            break;

    }

    IDE_TEST(sStmt->endStmt(aExecutionFlag) != IDE_SUCCESS);

    sStmt->setExecuteFlag(ID_FALSE);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

static IDE_RC doExecuteA5(mmtCmsExecuteContext *aExecuteContext)
{
    mmcStatement   * sStatement = aExecuteContext->mStatement;
    mmcSession     * sSession   = sStatement->getSession();
    qciStatement   * sQciStmt   = sStatement->getQciStmt();
    smiStatement   * sSmiStmt   = NULL;
    idBool           sRetry;
    idBool           sRecordExist;
    UShort           sResultSetCount;
    mmcResultSet   * sResultSet = NULL;
    mmcExecutionFlag sExecutionFlag = MMC_EXECUTION_FLAG_FAILURE;
    idBool           sBeginFetchTime = ID_FALSE;
    UInt             sOldResultSetHWM = 0;

    // PROJ-2163
    IDE_TEST(sStatement->reprepare() != IDE_SUCCESS);

    IDE_TEST(sStatement->beginStmt() != IDE_SUCCESS);

    // PROJ-2446 BUG-40481
    // mmcStatement.mSmiStmtPtr is changed at mmcStatement::beginSP()
    sSmiStmt = sStatement->getSmiStmt();

    do
    {
        do
        {
            sRetry = ID_FALSE;

            if (sStatement->execute(&aExecuteContext->mAffectedRowCount,
                                    &aExecuteContext->mFetchedRowCount) != IDE_SUCCESS)
            {
                switch (ideGetErrorCode() & E_ACTION_MASK)
                {
                    case E_ACTION_IGNORE:
                        IDE_RAISE(ExecuteFailIgnore);
                        break;

                    case E_ACTION_RETRY:
                        IDE_TEST(doRetry(sStatement) != IDE_SUCCESS);

                        sRetry = ID_TRUE;
                        break;

                    case E_ACTION_REBUILD:
                        IDE_TEST(doRebuild(sStatement) != IDE_SUCCESS);

                        sRetry = ID_TRUE;
                        break;

                    case E_ACTION_ABORT:
                        IDE_RAISE(ExecuteFailAbort);
                        break;

                    case E_ACTION_FATAL:
                        IDE_CALLBACK_FATAL("fatal error returned from mmcStatement::execute()");
                        break;

                    default:
                        IDE_CALLBACK_FATAL("invalid error action returned from mmcStatement::execute()");
                        break;
                }
            }

        } while (sRetry == ID_TRUE);

        sStatement->setStmtState(MMC_STMT_STATE_EXECUTED);

        IDE_TEST( sendBindOutA5( sSession,
                               aExecuteContext )
                  != IDE_SUCCESS );

        // fix BUG-30913
        // execute�� ���� ������ execute �� sendBindOutA5 �����̴�.
        sExecutionFlag = MMC_EXECUTION_FLAG_SUCCESS;

        sOldResultSetHWM = sStatement->getResultSetHWM();
        sResultSetCount = qci::getResultSetCount(sStatement->getQciStmt());
        sStatement->setResultSetCount(sResultSetCount);
        sStatement->setEnableResultSetCount(sResultSetCount);
        //fix BUG-27198 Code-Sonar  return value ignore�Ͽ�, �ᱹ mResultSet�� 
        // null pointer�϶� �̸� de-reference�� �Ҽ� �ֽ��ϴ�.
        IDE_TEST_RAISE(sStatement->initializeResultSet(sResultSetCount) != IDE_SUCCESS,
                       RestoreResultSetValues);


        if (sResultSetCount > 0)
        {
            sStatement->setFetchFlag(MMC_FETCH_FLAG_PROCEED);

            sResultSet = sStatement->getResultSet( MMC_RESULTSET_FIRST );
            // bug-26977: codesonar: resultset null ref
            // null�� ���������� �𸣰�����, ����ڵ���.
            IDE_TEST(sResultSet == NULL);

            if( sResultSet->mInterResultSet == ID_TRUE )
            {
                sRecordExist = sResultSet->mRecordExist;

                if (sRecordExist == ID_TRUE)
                {
                    // fix BUG-31195
                    sStatement->setFetchStartTime(mmtSessionManager::getBaseTime());
                    /* BUG-19456 */
                    sStatement->setFetchEndTime(0);

                    IDV_SQL_OPTIME_BEGIN( sStatement->getStatistics(),
                                          IDV_OPTM_INDEX_QUERY_FETCH);

                    sBeginFetchTime = ID_TRUE;

                    IDE_TEST(sStatement->beginFetch(MMC_RESULTSET_FIRST) != IDE_SUCCESS);
                }
                else
                {
                    sStatement->setFetchFlag(MMC_FETCH_FLAG_CLOSE);
                }
            }
            else
            {
                // fix BUG-31195
                // qci::moveNextRecord() �ð��� Fetch Time�� �߰�
                sStatement->setFetchStartTime(mmtSessionManager::getBaseTime());
                /* BUG-19456 */
                sStatement->setFetchEndTime(0);

                IDV_SQL_OPTIME_BEGIN( sStatement->getStatistics(),
                                      IDV_OPTM_INDEX_QUERY_FETCH);

                sBeginFetchTime = ID_TRUE;

                if (qci::moveNextRecord(sQciStmt,
                                        sStatement->getSmiStmt(),
                                        &sRecordExist) != IDE_SUCCESS)
                {
                    // fix BUG-31195
                    sBeginFetchTime = ID_FALSE;

                    sStatement->setFetchStartTime(0);
                    /* BUG-19456 */
                    sStatement->setFetchEndTime(0);

                    IDV_SQL_OPTIME_END( sStatement->getStatistics(),
                                        IDV_OPTM_INDEX_QUERY_FETCH );

                    switch (ideGetErrorCode() & E_ACTION_MASK)
                    {
                        case E_ACTION_IGNORE:
                            IDE_RAISE(MoveNextRecordFailIgnore);
                            break;

                        case E_ACTION_RETRY:
                            IDE_TEST(doRetry(sStatement) != IDE_SUCCESS);

                            sRetry = ID_TRUE;
                            break;

                        case E_ACTION_REBUILD:
                            IDE_TEST(doRebuild(sStatement) != IDE_SUCCESS);

                            sRetry = ID_TRUE;
                            break;

                        case E_ACTION_ABORT:
                            IDE_RAISE(MoveNextRecordFailAbort);
                            break;

                        case E_ACTION_FATAL:
                            IDE_CALLBACK_FATAL("fatal error returned from qci::moveNextRecord()");
                            break;

                        default:
                            IDE_CALLBACK_FATAL("invalid error action returned from qci::moveNextRecord()");
                            break;
                    }
                }
                else
                {
                    if (sStatement->getStmtType() == QCI_STMT_DEQUEUE)
                    {
                        if (sRecordExist == ID_TRUE)
                        {
                            sSession->setExecutingStatement(NULL);
                            sSession->endQueueWait();

                            sStatement->beginFetch(MMC_RESULTSET_FIRST);
                        }
                        else
                        {
                            // fix BUG-31195
                            sBeginFetchTime = ID_FALSE;

                            sStatement->setFetchStartTime(0);
                            /* BUG-19456 */
                            sStatement->setFetchEndTime(0);

                            IDV_SQL_OPTIME_END( sStatement->getStatistics(),
                                                IDV_OPTM_INDEX_QUERY_FETCH );

                            if (sSession->isQueueTimedOut() != ID_TRUE)
                            {
                                sExecutionFlag = MMC_EXECUTION_FLAG_QUEUE_EMPTY;
                                //fix BUG-21361 queue drop�� ���ü� ������ ���� ������ �����Ҽ� ����.
                                sSession->beginQueueWait();
                                //fix BUG-19321
                                IDE_TEST(doEnd(aExecuteContext, sExecutionFlag) != IDE_SUCCESS);
                                sSession->setExecutingStatement(sStatement);
                                aExecuteContext->mSuspended = ID_TRUE;
                            }
                            else
                            {
                                sSession->setExecutingStatement(NULL);
                                sSession->endQueueWait();

                                IDE_TEST(doEnd(aExecuteContext, sExecutionFlag) != IDE_SUCCESS);
                            }
                        }
                    }
                    else
                    {
                        if (sRecordExist == ID_TRUE)
                        {
                            IDE_TEST(sStatement->beginFetch(MMC_RESULTSET_FIRST) != IDE_SUCCESS);

                            // PROJ-2256
                            sResultSet->mBaseRow.mIsFirstRow = ID_TRUE;
                        }
                        else
                        {
                            // fix BUG-31195
                            sBeginFetchTime = ID_FALSE;

                            sStatement->setFetchStartTime(0);
                            /* BUG-19456 */
                            sStatement->setFetchEndTime(0);

                            IDV_SQL_OPTIME_END( sStatement->getStatistics(),
                                                IDV_OPTM_INDEX_QUERY_FETCH );

                            if (sResultSetCount == 1)
                            {
                                sStatement->setFetchFlag(MMC_FETCH_FLAG_CLOSE);

                                mmcStatement::makePlanTreeBeforeCloseCursor( sStatement,

                                                                             sStatement );

                                IDE_TEST(doEnd(aExecuteContext, sExecutionFlag) != IDE_SUCCESS);
                            }
                        }
                    }
                }
            }
        }
        else
        {
            mmcStatement::makePlanTreeBeforeCloseCursor( sStatement,
                                                         sStatement );

            if (qci::closeCursor(sQciStmt, sSmiStmt) != IDE_SUCCESS)
            {
                IDE_TEST(ideIsRetry() != IDE_SUCCESS);

                IDE_TEST(doRetry(sStatement) != IDE_SUCCESS);

                sRetry = ID_TRUE;
            }
            else
            {
                IDE_TEST(doEnd(aExecuteContext, sExecutionFlag) != IDE_SUCCESS);
            }
        }

    } while (sRetry == ID_TRUE);

    return IDE_SUCCESS;

    IDE_EXCEPTION(ExecuteFailIgnore);
    {
        SChar sMsg[64];

        idlOS::snprintf(sMsg, ID_SIZEOF(sMsg), "mmcStatement::execute code=0x%x", ideGetErrorCode());

        IDE_SET(ideSetErrorCode(mmERR_ABORT_INTERNAL_SERVER_ERROR_ARG, sMsg));
    }
    IDE_EXCEPTION(ExecuteFailAbort);
    {
        // No Action
    }
    IDE_EXCEPTION(MoveNextRecordFailIgnore);
    {
        SChar sMsg[64];

        idlOS::snprintf(sMsg, ID_SIZEOF(sMsg), "qci::moveNextRecord code=0x%x", ideGetErrorCode());

        IDE_SET(ideSetErrorCode(mmERR_ABORT_INTERNAL_SERVER_ERROR_ARG, sMsg));
    }
    IDE_EXCEPTION(MoveNextRecordFailAbort);
    {
        mmcStatement::makePlanTreeBeforeCloseCursor( sStatement,
                                                     sStatement );

        qci::closeCursor(sQciStmt, sSmiStmt);
    }
    IDE_EXCEPTION( RestoreResultSetValues );
    {
        sStatement->setResultSetCount(sOldResultSetHWM);
        sStatement->setEnableResultSetCount(sOldResultSetHWM);
    }
    IDE_EXCEPTION_END;
    {
        IDE_PUSH();

        // fix BUG-31195
        if (sBeginFetchTime == ID_TRUE)
        {
            sStatement->setFetchStartTime(0);
            /* BUG-19456 */
            sStatement->setFetchEndTime(0);

            IDV_SQL_OPTIME_END( sStatement->getStatistics(),
                                IDV_OPTM_INDEX_QUERY_FETCH );

            sBeginFetchTime = ID_FALSE;
        }

        sSession->setExecutingStatement(NULL);
        sSession->endQueueWait();

        if (sStatement->isStmtBegin() == ID_TRUE)
        {
            /* PROJ-2337 Homogeneous database link
             *  Dequeue �Ŀ� ���� ó���� �����ϴ� ���, Queue���� ����� �����͸� �ݳ��ؾ� �Ѵ�.
             *  TODO Dequeue �ܿ� �ٸ� ������ ������ Ȯ���ؾ� �Ѵ�.
             */
            if ( ( sStatement->getStmtType() == QCI_STMT_DEQUEUE ) &&
                 ( sExecutionFlag == MMC_EXECUTION_FLAG_SUCCESS ) )
            {
                sExecutionFlag = MMC_EXECUTION_FLAG_FAILURE;
            }
            else
            {
                /* Nothing to do */
            }

            /* PROJ-2223 Altibase Auditing */
            mmtAuditManager::auditByAccess( sStatement, sExecutionFlag );
            
            /*
             * [BUG-24187] Rollback�� statement�� Internal CloseCurosr��
             * ������ �ʿ䰡 �����ϴ�.
             */
            sStatement->setSkipCursorClose();
            sStatement->clearStmt(MMC_STMT_BIND_NONE);

            /* BUG-47650 BUG-38585 IDE_ASSERT remove */
            (void)sStatement->endStmt( sExecutionFlag );
        }

        sStatement->setExecuteFlag(ID_FALSE);
        
        /* BUG-29078
         * XA_END �������� XA_ROLLBACK�� ���� ������ ��쿡 XA_END�� Heuristic�ϰ� ó���Ѵ�.
         */
        if ( (sSession->isXaSession() == ID_TRUE) && 
             (sSession->getXaAssocState() == MMD_XA_ASSOC_STATE_ASSOCIATED) &&
             (sSession->getLastXid() != NULL) )
        {
            mmdXa::heuristicEnd(sSession, sSession->getLastXid());
        }

        IDE_POP();
    }

    return IDE_FAILURE;
}


IDE_RC mmtServiceThread::executeA5(cmiProtocolContext *aProtocolContext,
                                 mmcStatement       *aStatement,
                                 idBool              aDoAnswer,  
                                 idBool             *aSuspended,
                                 UInt               *aResultSetCount,
                                 ULong              *aAffectedRowCount)
{
    mmtCmsExecuteContext sExecuteContext;

    sExecuteContext.mProtocolContext  = aProtocolContext;
    sExecuteContext.mStatement        = aStatement;
    sExecuteContext.mAffectedRowCount = 0;
    sExecuteContext.mSuspended        = ID_FALSE;
    sExecuteContext.mCollectionData   = NULL;
    sExecuteContext.mCursor           = 0;

    IDE_TEST(doExecuteA5(&sExecuteContext) != IDE_SUCCESS);

    if( (aDoAnswer == ID_TRUE) && (sExecuteContext.mSuspended != ID_TRUE) )
    {
        IDE_TEST(answerExecuteResult(&sExecuteContext) != IDE_SUCCESS);
    }

    *aSuspended = sExecuteContext.mSuspended;
    
    if( aAffectedRowCount != NULL )
    {
        *aAffectedRowCount = sExecuteContext.mAffectedRowCount;
    }

    if( aResultSetCount != NULL )
    {
        *aResultSetCount = aStatement->getResultSetCount();
    }

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

IDE_RC mmtServiceThread::executeProtocolA5(cmiProtocolContext *aProtocolContext,
                                         cmiProtocol        *aProtocol,
                                         void               *aSessionOwner,
                                         void               *aUserContext)
{
    cmpArgDBExecuteA5  *sArg           = CMI_PROTOCOL_GET_ARG(*aProtocol, DB, Execute);
    mmcTask          *sTask          = (mmcTask *)aSessionOwner;
    mmtServiceThread *sThread        = (mmtServiceThread *)aUserContext;
    mmcSession       *sSession;
    mmcStatement     *sStatement;
    idBool            sSuspended     = ID_FALSE;

    IDE_CLEAR();

    IDE_TEST(findSession(sTask, &sSession, sThread) != IDE_SUCCESS);

    IDE_TEST(checkSessionState(sSession, MMC_SESSION_STATE_SERVICE) != IDE_SUCCESS);

    IDE_TEST(findStatement(&sStatement,
                           sSession,
                           &sArg->mStatementID,
                           sThread) != IDE_SUCCESS);

    IDE_TEST_RAISE(sStatement->getStmtState() != MMC_STMT_STATE_PREPARED,
                   InvalidStatementState);

    // PROJ-1518
    IDE_TEST_RAISE(sThread->atomicCheck(sStatement, &(sArg->mOption))
                                        != IDE_SUCCESS, SkipExecute);

    switch (sArg->mOption)
    {
        case CMP_DB_EXECUTE_NORMAL_EXECUTE:
            sStatement->setArray(ID_FALSE);
            sStatement->setRowNumber(sArg->mRowNumber);

            IDE_TEST(sThread->executeA5(aProtocolContext,
                                      sStatement,
                                      ID_TRUE,
                                      &sSuspended,
                                      NULL, NULL) != IDE_SUCCESS);

            break;

        case CMP_DB_EXECUTE_ARRAY_EXECUTE:
            sStatement->setArray(ID_TRUE);
            sStatement->setRowNumber(sArg->mRowNumber);

            IDE_TEST(sThread->executeA5(aProtocolContext,
                                      sStatement,
                                      ID_TRUE,
                                      &sSuspended,
                                      NULL, NULL) != IDE_SUCCESS);

            break;

        case CMP_DB_EXECUTE_ARRAY_BEGIN:
            sStatement->setArray(ID_TRUE);
            sStatement->setRowNumber(0);
            break;

        case CMP_DB_EXECUTE_ARRAY_END:
            sStatement->setArray(ID_FALSE);
            sStatement->setRowNumber(0);
            break;
        // PROJ-1518
        case CMP_DB_EXECUTE_ATOMIC_EXECUTE:
            sStatement->setRowNumber(sArg->mRowNumber);
            // Rebuild Error �� ó���ϱ� ���ؼ��� Bind�� ���� ��������
            // atomicBegin �� ȣ���ؾ� �Ѵ�.
            if( sArg->mRowNumber == 1)
            {
                IDE_TEST_RAISE((sThread->atomicBegin(sStatement) != IDE_SUCCESS), SkipExecute)
            }

            IDE_TEST_RAISE(sThread->atomicExecuteA5(sStatement, aProtocolContext) != IDE_SUCCESS, SkipExecute)
            break;

        case CMP_DB_EXECUTE_ATOMIC_BEGIN:
            sThread->atomicInit(sStatement);
            break;
        case CMP_DB_EXECUTE_ATOMIC_END:
            // �� �������� sArg->mRowNumber �� 1�̴�.
            sStatement->setRowNumber(sArg->mRowNumber);
            IDE_TEST(sThread->atomicEndA5(sStatement, aProtocolContext) != IDE_SUCCESS );
            break;
        default:
            IDE_RAISE(InvalidExecuteOption);
            break;
    }

    IDE_EXCEPTION_CONT(SkipExecute);

    return (sSuspended == ID_TRUE) ? IDE_CM_STOP : IDE_SUCCESS;

    IDE_EXCEPTION(InvalidStatementState);
    {
        IDE_SET(ideSetErrorCode(mmERR_ABORT_INVALID_STATEMENT_STATE_ERROR));
    }
    IDE_EXCEPTION(InvalidExecuteOption);
    {
        IDE_SET(ideSetErrorCode(mmERR_ABORT_INVALID_EXECUTE_OPTION));
    }
    IDE_EXCEPTION_END;

    return sThread->answerErrorResultA5(aProtocolContext,
                                      CMI_PROTOCOL_OPERATION(DB, Execute),
                                      sArg->mRowNumber);
}

// proj_2160 cm_type removal
// �Լ� ���ο��� sendBindOutA5�� ȣ���ϱ� ������
// A5�� �Լ��� ������ �д�.
IDE_RC mmtServiceThread::atomicExecuteA5(mmcStatement * aStatement, cmiProtocolContext *aProtocolContext)
{
    qciStatement          *sQciStmt    = aStatement->getQciStmt();
    mmtCmsExecuteContext   sExecuteContext;

    sExecuteContext.mProtocolContext  = aProtocolContext;
    sExecuteContext.mStatement        = aStatement;
    sExecuteContext.mAffectedRowCount = 0;
    sExecuteContext.mSuspended        = ID_FALSE;
    sExecuteContext.mCollectionData   = NULL;
    sExecuteContext.mCursor           = 0;

    IDE_TEST(qci::atomicInsert(sQciStmt) != IDE_SUCCESS);

    IDE_TEST( sendBindOutA5( aStatement->getSession(), &sExecuteContext ) != IDE_SUCCESS );

    // PROJ-2163
    // insert �Ŀ� �ٷ� ����Ÿ�� bind �ؾ��ϹǷ� EXEC_PREPARED ���·� �����ؾ� �Ѵ�.
    IDE_TEST( qci::atomicSetPrepareState( sQciStmt ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END
    {
        aStatement->setAtomicExecSuccess(ID_FALSE);
        // BUG-21489
        aStatement->setAtomicLastErrorCode(ideGetErrorCode());
    }

    return IDE_FAILURE;
}

// proj_2160 cm_type removal
// �Լ� ���ο��� A5�� static answerExecuteResult�� ȣ���ϱ� ������
// A5�� �Լ��� ������ �д�
IDE_RC mmtServiceThread::atomicEndA5(mmcStatement * aStatement, cmiProtocolContext *aProtocolContext)
{
    mmcSession            *sSession    = aStatement->getSession();
    idvSQL                *sStatistics = aStatement->getStatistics();
    qciStatement          *sQciStmt    = aStatement->getQciStmt();
    mmcAtomicInfo         *sAtomicInfo = aStatement->getAtomicInfo();
    mmtCmsExecuteContext   sExecuteContext;

    sExecuteContext.mProtocolContext  = aProtocolContext;
    sExecuteContext.mStatement        = aStatement;
    sExecuteContext.mAffectedRowCount = 0;
    sExecuteContext.mSuspended        = ID_FALSE;
    sExecuteContext.mCollectionData   = NULL;
    sExecuteContext.mCursor           = 0;

    // List ������ ���������� �ö� 1��° row�� ���ε尡 ���н� Begin�� ���Ҽ� �ִ�.
    IDE_TEST_RAISE( aStatement->isStmtBegin() != ID_TRUE, AtomicExecuteFail);

    IDE_TEST_RAISE( aStatement->getAtomicExecSuccess() != ID_TRUE, AtomicExecuteFail);

    sAtomicInfo->mIsCursorOpen = ID_FALSE;

    if ( qci::atomicEnd(sQciStmt) != IDE_SUCCESS)
    {
        switch (ideGetErrorCode() & E_ACTION_MASK)
        {
            case E_ACTION_IGNORE:
                IDE_RAISE(AtomicExecuteFail);
                break;

            // fix BUG-30449
            // RETRY ������ Ŭ���̾�Ʈ���� ABORT�� ������
            // ATOMIC ARRAY INSERT ���и� �˷��ش�.
            case E_ACTION_RETRY:
            case E_ACTION_REBUILD:
                IDE_RAISE(ExecuteRetry);
                break;

            case E_ACTION_ABORT:
                IDE_RAISE(ExecuteFailAbort);
                break;

            case E_ACTION_FATAL:
                IDE_CALLBACK_FATAL("fatal error returned from mmcStatement::atomicEnd()");
                break;

            default:
                IDE_CALLBACK_FATAL("invalid error action returned from mmcStatement::atomicEnd()");
                break;
        }
    }

    qci::getRowCount(sQciStmt, &sExecuteContext.mAffectedRowCount, &sExecuteContext.mFetchedRowCount);

    IDV_SQL_OPTIME_END( sStatistics, IDV_OPTM_INDEX_QUERY_EXECUTE );

    IDV_SESS_ADD_DIRECT(sSession->getStatistics(),
                        IDV_STAT_INDEX_EXECUTE_SUCCESS_COUNT, 1);

    IDV_SQL_ADD_DIRECT(sStatistics, mExecuteSuccessCount, 1);
    IDV_SQL_ADD_DIRECT(sStatistics, mProcessRow, (ULong)(sExecuteContext.mAffectedRowCount));

    aStatement->setStmtState(MMC_STMT_STATE_EXECUTED);

    if (qci::closeCursor(aStatement->getQciStmt(), aStatement->getSmiStmt()) == IDE_SUCCESS)
    {
        IDE_TEST(doEnd(&sExecuteContext, MMC_EXECUTION_FLAG_SUCCESS) != IDE_SUCCESS);
    }

    IDE_TEST(answerExecuteResult(&sExecuteContext) != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION(AtomicExecuteFail);
    {
        // BUG-21489
        IDE_SET(ideSetErrorCodeAndMsg(sAtomicInfo->mAtomicLastErrorCode,
                                      sAtomicInfo->mAtomicErrorMsg));

    }
    IDE_EXCEPTION(ExecuteRetry);
    {
        IDE_SET(ideSetErrorCode(mmERR_ABORT_ATOMIC_EXECUTE_ERROR));
    }
    IDE_EXCEPTION(ExecuteFailAbort);
    {
        // No Action
    }
    IDE_EXCEPTION_END
    {
        IDE_PUSH();

        IDV_SQL_OPTIME_END( sStatistics,
                            IDV_OPTM_INDEX_QUERY_EXECUTE );

        IDV_SESS_ADD_DIRECT(sSession->getStatistics(),
                            IDV_STAT_INDEX_EXECUTE_FAILURE_COUNT, 1);
        IDV_SQL_ADD_DIRECT(sStatistics, mExecuteFailureCount, 1);

        aStatement->setExecuteFlag(ID_FALSE);

        if (aStatement->isStmtBegin() == ID_TRUE)
        {
            /* PROJ-2223 Altibase Auditing */
            mmtAuditManager::auditByAccess( aStatement, MMC_EXECUTION_FLAG_FAILURE );
            
            /*
             * [BUG-24187] Rollback�� statement�� Internal CloseCurosr��
             * ������ �ʿ䰡 �����ϴ�.
             */
            aStatement->setSkipCursorClose();
            aStatement->clearStmt(MMC_STMT_BIND_NONE);

            IDE_ASSERT(aStatement->endStmt(MMC_EXECUTION_FLAG_FAILURE) == IDE_SUCCESS);
        }

        IDE_POP();
    }
    return IDE_FAILURE;
}

