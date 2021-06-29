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

#include <cm.h>
#include <mmErrorCode.h>
#include <mmtServiceThread.h>
#include <mmuProperty.h>
#include <mmcSession.h>
#include <sdi.h>

IDE_RC mmtServiceThread::answerErrorResult(cmiProtocolContext *aProtocolContext,
                                           UChar               aOperationID,
                                           UInt                aErrorIndex,
                                           mmcSession         *aSession,
                                           UChar               aExecuteOption)
{
    UInt                 sErrorCode;

    sErrorCode = ideGetErrorCode();

    if ( ( ( sErrorCode & E_MODULE_MASK ) == E_MODULE_SD ) &&
         ( ideErrorCollectionSize() > 0 ) )
    {
        return answerShardErrorResult( aProtocolContext,
                                       aOperationID,
                                       aErrorIndex,
                                       aSession,
                                       aExecuteOption );
    }
    else
    {
        return answerNormalErrorResult( aProtocolContext,
                                        aOperationID,
                                        aErrorIndex,
                                        aSession );
    }
}

IDE_RC mmtServiceThread::answerMultiErrorResult(cmiProtocolContext *aProtocolContext,
                                                UChar               aOperationID,
                                                UInt                aErrorIndex,
                                                smSCN               aSCN,
                                                mmcSession         *aSession)
{
    UShort                 sErrorMsgLen;
    UInt                   sErrorCode;
    UInt                   sNodeId = ID_UINT_MAX; // TASK-7218 Multiple Error
    SChar                 *sErrorMsg;

#if !defined(DEBUG)
    ACP_UNUSED( aSession );
#endif

    sErrorCode = sdi::getMultiErrorCode();

    if ((sErrorCode & E_ACTION_MASK) != E_ACTION_IGNORE)
    {
#ifdef DEBUG
        /* PROJ-2733-DistTxInfo GCTX�� ��쿡�� ������ �߻��� �� �ִ�. */
        if ( E_ERROR_CODE(sErrorCode) == E_ERROR_CODE(smERR_ABORT_StatementTooOld) )
        {
            IDE_DASSERT( ( aSession != NULL ) && ( aSession->isGCTx() == ID_TRUE ) );
        }
#endif
        sErrorMsg = sdi::getMultiErrorMsg();
        sErrorMsgLen = idlOS::strlen( sErrorMsg );

        /* IPCDA�� CMI_WRITE_CHECK*���� CMP_OP_DB_ErrorResult ������ Ȯ���ϹǷ�
           ���⿡���� CMI_WRITE_CHECK_WITH_IPCDA_FOR_ERROR�� ����ؾ� �Ѵ�.
           CMP_OP_DB_ErrorResultV?�� �߰��Ǵ� ��� CMI_IPCDA_REMAIN_PROTOCOL_SIZE��
           �ݵ�� �����ؾ� �Ѵ�. (cmi.h) */
        CMI_WRITE_CHECK_WITH_IPCDA_FOR_ERROR(aProtocolContext,
                                             12 + sErrorMsgLen + 12,
                                             12 + sErrorMsgLen + 12);

        CMI_WOP(aProtocolContext, CMP_OP_DB_ErrorV3Result);
        CMI_WR1(aProtocolContext, aOperationID);
        CMI_WR4(aProtocolContext, &aErrorIndex);
        CMI_WR4(aProtocolContext, &sErrorCode);
        CMI_WR2(aProtocolContext, &sErrorMsgLen);
        CMI_WCP(aProtocolContext, sErrorMsg, sErrorMsgLen);
        CMI_WR8(aProtocolContext, &aSCN);
        CMI_WR4(aProtocolContext, &sNodeId);

        /*PROJ-2616*/
        MMT_IPCDA_INCREASE_DATA_COUNT(aProtocolContext);
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* TASK-7218 Handling Multi-Error for SD */
IDE_RC mmtServiceThread::answerShardErrorResult(cmiProtocolContext *aProtocolContext,
                                                UChar               aOperationID,
                                                UInt                aErrorIndex,
                                                mmcSession         *aSession,
                                                UChar               aExecuteOption)
{
    UShort                 sErrorMsgLen;
    UInt                   sErrorCode;
    UInt                   sNodeId = 0;
    SChar                 *sErrorMsg;
    smSCN                  sSCN;

    SM_INIT_SCN(&sSCN);

    if ( ( aSession != NULL ) && ( aSession->isGCTx() == ID_TRUE ) )
    {
        aSession->getLastSystemSCN( aOperationID, &sSCN );

        #if defined(DEBUG)
        if ( SM_SCN_IS_NOT_INIT( sSCN ) )
        {
            ideLog::log(IDE_SD_18, "= [%s] answerShardErrorResult, SCN : %"ID_UINT64_FMT,
                        aSession->getSessionTypeString(),
                        sSCN);
        }
        #endif
    }

    // proj_2160 cm_type removal
    // �߰��� ���������� �߻��� ��� ������ ������ �Ѵ�.
    // �� ��, ErrorResult ������ �ƿ� ������ ���ƾ� �ϴ� ��쵵 �ִ�.
    IDE_TEST_RAISE(aProtocolContext->mSessionCloseNeeded == ID_TRUE,
                   SessionClosed);

    /* TASK-7218 Multi-Error Handling 2nd
     *   ������ �´� ��� Mutiple Error�� ���� ������ */
    if ( (aProtocolContext->mProtocol.mClientLastOpID >= CMP_OP_DB_ErrorV3Result) &&
         (MMT_NEED_TO_SEND_MULTIPLE_ERROR(aSession, aExecuteOption) == ID_TRUE) )
    {
        IDE_TEST(answerMultiErrorResult(aProtocolContext,
                                        aOperationID,
                                        aErrorIndex,
                                        sSCN,
                                        aSession)
                 != IDE_SUCCESS);
    }
    else
    {
        /* Nothing to do */
    }

    while ( (sErrorMsg = ideErrorCollectionFirstErrorMsg()) != NULL )
    {
        sErrorMsgLen = idlOS::strlen( sErrorMsg );
        sErrorCode = ideErrorCollectionFirstErrorCode();
        sNodeId = ideErrorCollectionFirstErrorNodeId();

        if ((sErrorCode & E_ACTION_MASK) != E_ACTION_IGNORE)
        {
#ifdef DEBUG
            /* PROJ-2733-DistTxInfo GCTX�� ��쿡�� ������ �߻��� �� �ִ�. */
            if ( E_ERROR_CODE(sErrorCode) == E_ERROR_CODE(smERR_ABORT_StatementTooOld) )
            {
                IDE_DASSERT( ( aSession != NULL ) && ( aSession->isGCTx() == ID_TRUE ) );
            }
#endif
        }
        else
        {
            sErrorMsg = NULL;
            sErrorMsgLen = 0;
        }

        /* PROJ-2733-Protocol */
        if (aProtocolContext->mProtocol.mClientLastOpID >= CMP_OP_DB_ErrorV3Result)
        {
            /* IPCDA�� CMI_WRITE_CHECK*���� CMP_OP_DB_ErrorResult ������ Ȯ���ϹǷ�
               ���⿡���� CMI_WRITE_CHECK_WITH_IPCDA_FOR_ERROR�� ����ؾ� �Ѵ�.
               CMP_OP_DB_ErrorResultV?�� �߰��Ǵ� ��� CMI_IPCDA_REMAIN_PROTOCOL_SIZE��
               �ݵ�� �����ؾ� �Ѵ�. (cmi.h) */
            CMI_WRITE_CHECK_WITH_IPCDA_FOR_ERROR(aProtocolContext, 12 + sErrorMsgLen + 12, 12 + sErrorMsgLen + 12);

            CMI_WOP(aProtocolContext, CMP_OP_DB_ErrorV3Result);
            CMI_WR1(aProtocolContext, aOperationID);
            CMI_WR4(aProtocolContext, &aErrorIndex);
            CMI_WR4(aProtocolContext, &sErrorCode);
            CMI_WR2(aProtocolContext, &sErrorMsgLen);
            CMI_WCP(aProtocolContext, sErrorMsg, sErrorMsgLen);
            CMI_WR8(aProtocolContext, &sSCN);
            CMI_WR4(aProtocolContext, &sNodeId);
        }
        else
        {
            CMI_WRITE_CHECK_WITH_IPCDA_FOR_ERROR(aProtocolContext, 12 + sErrorMsgLen, 12 + sErrorMsgLen);

            CMI_WOP(aProtocolContext, CMP_OP_DB_ErrorResult);
            CMI_WR1(aProtocolContext, aOperationID);
            CMI_WR4(aProtocolContext, &aErrorIndex);
            CMI_WR4(aProtocolContext, &sErrorCode);
            CMI_WR2(aProtocolContext, &sErrorMsgLen);
            CMI_WCP(aProtocolContext, sErrorMsg, sErrorMsgLen);
        }

        /*PROJ-2616*/
        MMT_IPCDA_INCREASE_DATA_COUNT(aProtocolContext);

        ideErrorCollectionRemoveFirst();
    }

    ideErrorCollectionClear();

    return IDE_SUCCESS;

    IDE_EXCEPTION(SessionClosed);

    IDE_EXCEPTION_END;

    ideErrorCollectionClear();

    return IDE_FAILURE;
}

IDE_RC mmtServiceThread::answerNormalErrorResult(cmiProtocolContext *aProtocolContext,
                                                 UChar               aOperationID,
                                                 UInt                aErrorIndex,
                                                 mmcSession         *aSession)
{
    UInt                 sErrorCode;
    ideErrorMgr         *sErrorMgr  = ideGetErrorMgr();
    SChar               *sErrorMsg;
    SChar                sMessage[4096];
    UShort               sErrorMsgLen;
    smSCN                sSCN;
    UInt                 sNodeId = 0;

    IDE_TEST_RAISE(mErrorFlag == ID_TRUE, ErrorAlreadySent);

    // proj_2160 cm_type removal
    // �߰��� ���������� �߻��� ��� ������ ������ �Ѵ�.
    // �� ��, ErrorResult ������ �ƿ� ������ ���ƾ� �ϴ� ��쵵 �ִ�.
    IDE_TEST_RAISE(aProtocolContext->mSessionCloseNeeded == ID_TRUE,
                   SessionClosed);

    //fix [BUG-27123 : mm/NEW] [5.3.3 release Code-Sonar] mm Ignore return values series 2
    sErrorCode = ideGetErrorCode();
    SM_INIT_SCN(&sSCN);

    if ((sErrorCode & E_ACTION_MASK) != E_ACTION_IGNORE)
    {
#ifdef DEBUG
        /* PROJ-2733-DistTxInfo GCTX�� ��쿡�� ������ �߻��� �� �ִ�. */
        if ( E_ERROR_CODE(sErrorCode) == E_ERROR_CODE(smERR_ABORT_StatementTooOld) )
        {
            IDE_DASSERT( ( aSession != NULL ) && ( aSession->isGCTx() == ID_TRUE ) );
        }
#endif

        if ( ( aSession != NULL ) && ( aSession->isGCTx() == ID_TRUE ) )
        {
            aSession->getLastSystemSCN( aOperationID, &sSCN );

            #if defined(DEBUG)
            if ( SM_SCN_IS_NOT_INIT( sSCN ) )
            {
                ideLog::log(IDE_SD_18, "= [%s] answerNormalErrorResult, SCN : %"ID_UINT64_FMT,
                            aSession->getSessionTypeString(),
                            sSCN);
            }
            #endif
        }

        if ((iduProperty::getSourceInfo() & IDE_SOURCE_INFO_CLIENT) == 0)
        {
            sErrorMsg = ideGetErrorMsg(sErrorCode);
        }
        else
        {
            idlOS::snprintf(sMessage,
                            ID_SIZEOF(sMessage),
                            "(%s:%" ID_INT32_FMT ") %s",
                            sErrorMgr->Stack.LastErrorFile,
                            sErrorMgr->Stack.LastErrorLine,
                            sErrorMgr->Stack.LastErrorMsg);

            sErrorMsg = sMessage;
        }
    }
    else
    {
        sErrorMsg = NULL;
    }

    if (sErrorMsg != NULL)
    {
        sErrorMsgLen = idlOS::strlen(sErrorMsg);
    }
    else
    {
        sErrorMsgLen = 0;
    }

    /* PROJ-2733-Protocol ����ȣȯ���� ���� Ŭ���̾�Ʈ�� �����ϴ� ErrorResult ������ ������ �Ѵ�. */
    if (aProtocolContext->mProtocol.mClientLastOpID >= CMP_OP_DB_ErrorV3Result)
    {
        /* IPCDA�� CMI_WRITE_CHECK*���� CMP_OP_DB_ErrorResult ������ Ȯ���ϹǷ�
            ���⿡���� CMI_WRITE_CHECK_WITH_IPCDA_FOR_ERROR�� ����ؾ� �Ѵ�.
            CMP_OP_DB_ErrorResultV?�� �߰��Ǵ� ��� CMI_IPCDA_REMAIN_PROTOCOL_SIZE��
            �ݵ�� �����ؾ� �Ѵ�. (cmi.h) */
        CMI_WRITE_CHECK_WITH_IPCDA_FOR_ERROR(aProtocolContext, 12 + sErrorMsgLen + 12, 12 + sErrorMsgLen + 12);

        CMI_WOP(aProtocolContext, CMP_OP_DB_ErrorV3Result);
        CMI_WR1(aProtocolContext, aOperationID);
        CMI_WR4(aProtocolContext, &aErrorIndex);
        CMI_WR4(aProtocolContext, &sErrorCode);
        CMI_WR2(aProtocolContext, &sErrorMsgLen);
        CMI_WCP(aProtocolContext, sErrorMsg, sErrorMsgLen);
        CMI_WR8(aProtocolContext, &sSCN);
        CMI_WR4(aProtocolContext, &sNodeId);
    }
    else
    {
        CMI_WRITE_CHECK_WITH_IPCDA_FOR_ERROR(aProtocolContext, 12 + sErrorMsgLen, 12 + sErrorMsgLen);

        CMI_WOP(aProtocolContext, CMP_OP_DB_ErrorResult);
        CMI_WR1(aProtocolContext, aOperationID);
        CMI_WR4(aProtocolContext, &aErrorIndex);
        CMI_WR4(aProtocolContext, &sErrorCode);
        CMI_WR2(aProtocolContext, &sErrorMsgLen);
        CMI_WCP(aProtocolContext, sErrorMsg, sErrorMsgLen);
    }

#ifdef DEBUG
    /* BUG-44705 IPCDA������ buffer size�� �Ѱ�� ���ؼ� �����޼����� ���� buffer
     * size�� �̸� Ȯ���ؾ� �Ѵ�, ���� Stack error ����� ���� buffer �Ҵ��ϱ�� �����.
     * ���� IPCDA������ �� ����� �����Ѵ�. */
    if (((sErrorCode & E_ACTION_MASK) != E_ACTION_IGNORE) &&
        (mmuProperty::getShowErrorStack() == 1) &&
        (cmiGetLinkImpl(aProtocolContext) != CMI_LINK_IMPL_IPCDA))
    {   
        UInt i;
        UInt sMessageLen;

        idlOS::snprintf(sMessage,
                        ID_SIZEOF(sMessage),
                        "===================== ERROR STACK FOR ERR-%05X ======================\n",
                        E_ERROR_CODE(sErrorCode));

        sMessageLen = idlOS::strlen(sMessage);

        CMI_WRITE_CHECK(aProtocolContext, 5 + sMessageLen);

        CMI_WOP(aProtocolContext, CMP_OP_DB_Message);
        CMI_WR4(aProtocolContext, &sMessageLen);
        CMI_WCP(aProtocolContext, sMessage, sMessageLen);

        for (i = 0; i < sErrorMgr->ErrorIndex; i++)
        {
            idlOS::snprintf(sMessage,
                            ID_SIZEOF(sMessage),
                            " %" ID_INT32_FMT ": %s:%" ID_INT32_FMT,
                            i,
                            sErrorMgr->ErrorFile[i],
                            sErrorMgr->ErrorLine[i]);

            idlVA::appendFormat(sMessage, ID_SIZEOF(sMessage), "                                        ");

            idlVA::appendFormat(sMessage,
                                ID_SIZEOF(sMessage),
                                " %s\n",
                                sErrorMgr->ErrorTestLine[i]);

            sMessageLen = idlOS::strlen(sMessage);

            CMI_WRITE_CHECK(aProtocolContext, 5 + sMessageLen);

            CMI_WOP(aProtocolContext, CMP_OP_DB_Message);
            CMI_WR4(aProtocolContext, &sMessageLen);
            CMI_WCP(aProtocolContext, sMessage, sMessageLen);
        }

        idlOS::snprintf(sMessage,
                        ID_SIZEOF(sMessage),
                        "======================================================================\n");

        sMessageLen = idlOS::strlen(sMessage);

        CMI_WRITE_CHECK(aProtocolContext, 5 + sMessageLen);

        CMI_WOP(aProtocolContext, CMP_OP_DB_Message);
        CMI_WR4(aProtocolContext, &sMessageLen);
        CMI_WCP(aProtocolContext, sMessage, sMessageLen);
    }
#endif
    /*PROJ-2616*/
    MMT_IPCDA_INCREASE_DATA_COUNT(aProtocolContext);

    switch (sErrorCode)
    {
        case idERR_ABORT_Session_Closed:
        case idERR_ABORT_Session_Disconnected:
        // �ۼ��� �Լ����� ������ �߻��Ͽ� �ٷ� Ȯ�ε� ���
        case cmERR_ABORT_CONNECTION_CLOSED:
            IDE_RAISE(SessionClosed);
            break;
        case mmERR_ABORT_IPCDA_MESSAGE_TOO_LONG:
            /* BUG-44705 IPCDA���� buffer limit error�� client�� �ι� ���۵��� �ʵ��� �Ѵ�. */
            mErrorFlag = ID_TRUE;
            break;
        default:
            break;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(ErrorAlreadySent);

    return IDE_SUCCESS;

    IDE_EXCEPTION(SessionClosed);

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mmtServiceThread::invalidProtocol(cmiProtocolContext *aProtocolContext,
                                         cmiProtocol        *aProtocol,
                                         void               * /*aSessionOwner*/,
                                         void               *aUserContext)
{
    mmtServiceThread *sThread = (mmtServiceThread *)aUserContext;

    IDE_SET(ideSetErrorCode(mmERR_ABORT_INVALID_ERROR));

    return sThread->answerErrorResult(aProtocolContext, aProtocol->mOpID, 0);
}
