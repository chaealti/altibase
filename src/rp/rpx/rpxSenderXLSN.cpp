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
 * $Id: rpxSenderXLSN.cpp 90578 2021-04-13 05:32:12Z minku.kang $
 **********************************************************************/

#include <idl.h>
#include <ide.h>

#include <smi.h>
#include <smErrorCode.h>
#include <rpDef.h>
#include <rpuProperty.h>
#include <rpcManager.h>
#include <rpxSender.h>



//===================================================================
//
// Name:          updateXLSN
//
// Return Value:  IDE_SUCCESS/IDE_FAILURE
//
// Argument:      smLSN lsn
//
// Called By:     emptyReplBuffer()
//
// Description:
//
//===================================================================

IDE_RC rpxSender::updateXSN(smSN aSN)
{
    smiTrans          sTrans;
    SInt              sStage = 0;
    idBool            sIsTxBegin = ID_FALSE;
    smiStatement     *spRootStmt;
    UInt              sFlag = 0;

    //----------------------------------------------------------------//
    // no commit log occurred
    //----------------------------------------------------------------//
    if((aSN == SM_SN_NULL) || (aSN == 0))
    {
        return IDE_SUCCESS;
    }

    // Do not need to update XLSN again with the same value
    if(aSN == getRestartSN())
    {
        return IDE_SUCCESS;
    }

    if(isParallelChild() == ID_TRUE)
    {
        return IDE_SUCCESS;
    }

    /* PROJ-1442 Replication Online �� DDL ���
     * Service Thread�� Transaction���� �����ǰ� �ִ� ���,
     * Service Thread�� Transaction�� ����Ѵ�.
     */
    if(mSvcThrRootStmt != NULL)
    {
        spRootStmt = mSvcThrRootStmt;
        IDU_FIT_POINT( "rpxSender::updateXSN::Erratic::rpERR_ABORT_RP_SENDER_UPDATE_XSN" ); 
        IDE_TEST(rpcManager::updateXSN(spRootStmt,
                                        mMeta.mReplication.mRepName,
                                        aSN)
                 != IDE_SUCCESS);

        mMeta.mReplication.mXSN = aSN;
        setRestartSN(aSN);
    }
    else
    {
        IDE_TEST(sTrans.initialize() != IDE_SUCCESS);
        sStage = 1;

        sFlag = (sFlag & ~SMI_ISOLATION_MASK) | (UInt)RPU_ISOLATION_LEVEL;
        sFlag = (sFlag & ~SMI_TRANSACTION_MASK) | SMI_TRANSACTION_NORMAL;
        sFlag = (sFlag & ~SMI_TRANSACTION_REPL_MASK) | SMI_TRANSACTION_REPL_NONE;
        sFlag = (sFlag & ~SMI_COMMIT_WRITE_MASK) | SMI_COMMIT_WRITE_NOWAIT;

        IDE_TEST(sTrans.begin(&spRootStmt, NULL, sFlag, SMX_NOT_REPL_TX_ID)
                 != IDE_SUCCESS);
        sIsTxBegin = ID_TRUE;
        sStage = 2;

        IDE_TEST(rpcManager::updateXSN(spRootStmt,
                                        mMeta.mReplication.mRepName,
                                        aSN)
                 != IDE_SUCCESS);

        sStage = 1;
        IDE_TEST(sTrans.commit() != IDE_SUCCESS);
        sIsTxBegin = ID_FALSE;

        mMeta.mReplication.mXSN = aSN;
        setRestartSN(aSN);

        sStage = 0;
        IDE_TEST(sTrans.destroy( NULL ) != IDE_SUCCESS);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch(sStage)
    {
        case 2:
            IDE_ASSERT(sTrans.rollback() == IDE_SUCCESS);
            sIsTxBegin = ID_FALSE;
        case 1:
            if(sIsTxBegin == ID_TRUE)
            {
                IDE_ASSERT(sTrans.rollback() == IDE_SUCCESS);
                sIsTxBegin = ID_FALSE;
            }
            (void)sTrans.destroy( NULL );
        default:
            break;
    }

    IDE_ERRLOG(IDE_RP_0);
    IDE_SET(ideSetErrorCode(rpERR_ABORT_RP_SENDER_UPDATE_XSN));
    IDE_ERRLOG(IDE_RP_0);

    return IDE_FAILURE;
}

IDE_RC rpxSender::findAndUpdateInvalidMaxSN( smiStatement    * aSmiStmt,
                                             rpdReplSyncItem * aSyncList,
                                             rpdReplItems    * aReplItems,
                                             smSN              aSN)
{
    if ( aSyncList != NULL )
    {
        if ( isSyncItem( aSyncList,
                         aReplItems->mLocalUsername,
                         aReplItems->mLocalTablename,
                         aReplItems->mLocalPartname ) == ID_TRUE )
        {
            IDE_TEST( updateInvalidMaxSN( aSmiStmt,
                                          aReplItems,
                                          aSN )
                      != IDE_SUCCESS );
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_ERRLOG(IDE_RP_0);

    return IDE_FAILURE;
}

IDE_RC rpxSender::updateInvalidMaxSN(smiStatement * aSmiStmt,
                                     rpdReplItems * aReplItems,
                                     smSN           aSN)
{
    // Do not need to update INVALID_MAX_SN again with the same value
    if((aReplItems->mInvalidMaxSN >= (ULong)aSN) ||
       (aSN == SM_SN_NULL) ||
       (isParallelChild() == ID_TRUE))
    {
        // Do nothing
    }
    else
    {
        /* PROJ-1915 off-line sender�� ��� Meta ������ ���� �ʴ´�. */
        if(mCurrentType != RP_OFFLINE)
        {
            IDU_FIT_POINT( "rpxSender::updateInvalidMaxSN::Erratic::rpERR_ABORT_RP_SENDER_UPDATE_INVALID_MAX_SN" ); 
            IDE_TEST(rpcManager::updateInvalidMaxSN(aSmiStmt, aReplItems, aSN)
                     != IDE_SUCCESS);

            IDE_TEST( rpcManager::updateOldInvalidMaxSN( aSmiStmt,
                                                         aReplItems,
                                                         aSN )
                      != IDE_SUCCESS );
        }

        aReplItems->mInvalidMaxSN = (ULong)aSN;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_ERRLOG(IDE_RP_0);
    IDE_SET(ideSetErrorCode(rpERR_ABORT_RP_SENDER_UPDATE_INVALID_MAX_SN));

    return IDE_FAILURE;
}

// To Fix PR-4099
IDE_RC rpxSender::initXSN( smSN aReceiverXSN )
{
    smSN          sCurrentSN;
    smiTrans      sTrans;
    SInt          sStage = 0;
    idBool        sIsTxBegin = ID_FALSE;
    smiStatement *spRootStmt;
    /* �ݵ�� Normal�� �Ͽ��� �Ѵ�. �׷��� ���� ������
       Savepoint�� Log�� ��ϵ��� �ʴ´�. */
    static UInt   sFlag = RPU_ISOLATION_LEVEL | SMI_TRANSACTION_NORMAL |
                          SMI_TRANSACTION_REPL_NONE | SMX_COMMIT_WRITE_NOWAIT;

    sCurrentSN = SM_SN_NULL;
    updateOldMaxXSN();
    
    switch ( mCurrentType )
    {
        case RP_NORMAL:
        case RP_PARALLEL:
        case RP_START_CONDITIONAL:
            if ( mMeta.mReplication.mXSN == SM_SN_NULL )
            {
                // if first start
                // parallel child�� ���ʿ� ������ �� �����Ƿ�,
                // �׻� mXSN�� NULL�� �� �� ����.
                IDE_DASSERT(isParallelChild() != ID_TRUE);

                IDE_TEST( sTrans.initialize() != IDE_SUCCESS );
                sStage = 1;

                IDE_TEST( sTrans.begin(&spRootStmt,
                                       NULL,
                                       sFlag,
                                       SMX_NOT_REPL_TX_ID )
                          != IDE_SUCCESS );
                sIsTxBegin = ID_TRUE;
                sStage = 2;

                /* BUG-14528 : Checkpoint Thread�� Replication�� ����
                 * ������ ������ ���ϵ��� Savepoint�α� ���
                 *
                 * 4.3.7 ������ checkpoint thread�� alter replication rep start��
                 * �����ϴ� service thread, �׸��� sender thread ���̿�
                 * ������ ���� ���� ������ �־���
                 * ��>
                 * 1> service thread�� replication�� ó�� ������ ��
                 *    smiGetLastValidGSN( &sCurrentSN )�� ���� �α����� 1����
                 *   ���Ե� ���� �ֱ��� SN�� ����
                 * 2> bulk update�� �߻��Ͽ� �α������� 10������ ������
                 *  3> checkpoint thread�� replication���� �ʿ��� �ּ� ������
                 *     ��Ÿ ���̺�(sys_replications_)���� QP�� �ִ�
                 *     getMinimumSN()�Լ��� ���� ������(-1: SM_SN_NULL)
                 *  4> checkpoint thread�� 1�� �α����� ���� ������ �ȴٰ� �Ǵ��Ͽ�
                 *     1�� �α������� ����
                 * 5> service thread�� �ٽ� ���۵Ǿ� sys_replications_�� XSN��
                 *    sCurrentSN�� �����ϰ�(updateXSN), sender thread��  sCurrentSN(mXSN)����
                 *   �α׸� �������� �ϳ� �α������� �������� ������ �����ϰ� ��
                 *
                 * 4.3.9���� getMinimumSN �Լ��� RP������ �Űܿ��鼭 getMinimumSN
                 * �Լ� �ȿ��� sendermutex�� ��� �Ǿ���.
                 * checkpoint thread���� getMinimumSN�� �����ϴ� �Ͱ� sender thread��
                 * initXSN() �Լ��� ȣ���ϴ� ���� (�� �Լ� ȣ������ sendermutex�� ����)
                 * ���� ���ÿ� ����� �� ���� �Ǿ��� ������,
                 * BUG-14528�� ������ �� �̻� �߻����� �ʴ´�.
                 * �׷���  savepoint�α׸� ��� ���� ���ۿ� ������ ����
                 * �����Ƿ� ���� �������� �������� �ʴ´�.
                 * ���� BUG-14528�� ���� ������ �� �߻��Ѵٸ� RP_START_QUICK
                 * RP_START_SYNC_ONLY/RP_START_SYNC�� ����ؾ� �Ѵ�.
                 */

                IDE_TEST(sTrans.savepoint(RPX_SENDER_SVP_NAME,
                                          NULL)
                         != IDE_SUCCESS);

                /* For Parallel Logging: LSN�� SN���� ���� PRJ-1464  */
                IDE_ASSERT(smiGetLastValidGSN(&sCurrentSN) == IDE_SUCCESS);
                mXSN = sCurrentSN;

                // update XLSN
                IDE_TEST(updateXSN(mXSN) != IDE_SUCCESS);

                sStage = 1;
                IDE_TEST( sTrans.commit() != IDE_SUCCESS );
                sIsTxBegin = ID_FALSE;

                sStage = 0;
                IDE_TEST( sTrans.destroy( NULL ) != IDE_SUCCESS );
            }
            else
            {
                mXSN = mMeta.mReplication.mXSN;
                mSkipXSN = aReceiverXSN;

                /* For Parallel Logging: LSN�� SN���� ���� PRJ-1464  */
                IDE_ASSERT(smiGetLastUsedGSN(&sCurrentSN) == IDE_SUCCESS);
                
                IDU_FIT_POINT_RAISE( "rpxSender::initXSN::Erratic::rpERR_ABORT_INVALID_XSN",
                                     ERR_INVALID_XSN ); 
                IDE_TEST_RAISE( mXSN > sCurrentSN, ERR_INVALID_XSN);
            }
            mCommitXSN = mXSN;

            IDE_TEST_RAISE(mXSN == SM_SN_NULL, ERR_INVALID_XSN);
            break;

        case RP_QUICK:
            IDE_ASSERT(smiGetLastValidGSN(&sCurrentSN) == IDE_SUCCESS);
            mXSN = sCurrentSN;

            IDE_TEST(updateXSN(mXSN) != IDE_SUCCESS);
            mCommitXSN = mXSN;
            break;

        case RP_SYNC:
        case RP_SYNC_CONDITIONAL:
        case RP_SYNC_ONLY://fix BUG-9023
            if(mMeta.mReplication.mXSN == SM_SN_NULL)
            {
                sCurrentSN = smiGetValidMinSNOfAllActiveTrans();
                mXSN = sCurrentSN;

                IDE_TEST(updateXSN(mXSN) != IDE_SUCCESS);
            }
            else
            {
                mXSN = mMeta.mReplication.mXSN;
                mSkipXSN = aReceiverXSN;
            }

            mCommitXSN = mXSN;
            break;

        case RP_RECOVERY:
            IDE_DASSERT(mSNMapMgr != NULL);
            mXSN = mSNMapMgr->getMinReplicatedSN();
            mCommitXSN = mXSN;
            // ���� receiver�� begin�� �ް� ����� ��� smMap�� �ּ� SN�� SM_SN_NULL�� �� �� ����
            // �� ��쿡�� ������ ������ �����Ƿ� �����ؾ���.
            if(mXSN == SM_SN_NULL)
            {
                mExitFlag = ID_TRUE;
            }
            break;

        /* PROJ-1915 */
        case RP_OFFLINE:
            mXSN = mMeta.mReplication.mXSN;
            mSkipXSN = aReceiverXSN;
            mCommitXSN = mXSN;
            break;

        case RP_XLOGFILE_FAILBACK_MASTER:
            /* �� ������ incremental sync������ �ӽ÷� ����ϴ� ������ updateXSN�� �ϸ� �ȵȴ� */
            mXSN = SM_SN_NULL;
            break;

        case RP_XLOGFILE_FAILBACK_SLAVE:
            /* ����ȭ�� ���� �����ϴ� ��� ������� �� �� ����. */
            IDE_DASSERT ( mMeta.mReplication.mXSN != SM_SN_NULL );

            mXSN = mMeta.mReplication.mXSN;
            mSkipXSN = aReceiverXSN;

            /* For Parallel Logging: LSN�� SN���� ���� PRJ-1464  */
            IDE_ASSERT(smiGetLastUsedGSN(&sCurrentSN) == IDE_SUCCESS);

            IDU_FIT_POINT_RAISE( "rpxSender::initXSN::Erratic::rpERR_ABORT_INVALID_XSN",
                                 ERR_INVALID_XSN ); 
            IDE_TEST_RAISE( mXSN > sCurrentSN, ERR_INVALID_XSN);

            break;

        default:
            IDE_CALLBACK_FATAL("[Repl Sender] Invalid Option");
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_INVALID_XSN);
    {
        IDE_SET(ideSetErrorCode(rpERR_ABORT_INVALID_XSN,
                                mXSN,
                                sCurrentSN ));
    }
    IDE_EXCEPTION_END;
    IDE_ERRLOG(IDE_RP_0);
    IDE_SET(ideSetErrorCode(rpERR_ABORT_RP_SENDER_START));
    IDE_ERRLOG(IDE_RP_0);

    switch(sStage)
    {
        case 2:
            IDE_ASSERT(sTrans.rollback() == IDE_SUCCESS);
            sIsTxBegin = ID_FALSE;
        case 1:
            if(sIsTxBegin == ID_TRUE)
            {
                IDE_ASSERT(sTrans.rollback() == IDE_SUCCESS);
                sIsTxBegin = ID_FALSE;
            }
            (void)sTrans.destroy( NULL );
        default:
            break;
    }

    return IDE_FAILURE;
}
/*
return value: 
mSenderInfo�� �ʱ�ȭ ���� ���� ��쿡 SM_SN_NULL�� ��ȯ�ϸ�,
�׷��� ������ mSenderInfo�� LastCommitSN�� ��ȯ
SM_SN_NULL�� ��ȯ�ϴ� ������ Sender�� ���������� �ʱ�ȭ �Ǳ� ����
FLUSH�� ���� �� �Լ��� ȣ��Ǹ� FLUSH�� ������� �ʰ� ������ ��
�ֵ��� �ϱ� ������
*/
smSN
rpxSender::getRmtLastCommitSN()
{
    smSN sRmtLastCommitSN = SM_SN_NULL;
    if(mSenderInfo != NULL)
    {
        sRmtLastCommitSN = mSenderInfo->getRmtLastCommitSN();
    }
    return sRmtLastCommitSN;
}

smSN rpxSender::getLastProcessedSN()
{
    smSN sLastProcessedSN = SM_SN_NULL;
    if(mSenderInfo != NULL)
    {
        sLastProcessedSN = mSenderInfo->getLastProcessedSN();
    }
    return sLastProcessedSN;
}

smSN
rpxSender::getRestartSN()
{
    smSN sRestartSN = SM_SN_NULL;

    if(mSenderInfo != NULL)
    {
        sRestartSN = mSenderInfo->getRestartSN();
    }

    return sRestartSN;
}

void 
rpxSender::setRestartSN(smSN aSN)
{
    if(mSenderInfo != NULL)
    {
        mSenderInfo->setRestartSN(aSN);
    }
} 

void 
rpxSender::updateOldMaxXSN()
{
    if ( ( mXSN > mOldMaxXSN ) && ( mXSN != SM_SN_NULL ) ) 
    {
        mOldMaxXSN = mXSN;
    }
    else
    {
        /* Nothing to do */
    }
}

smSN rpxSender::getLastArrivedSN()
{
    smSN sLastArrivedSN = SM_SN_NULL;
    if(mSenderInfo != NULL)
    {
        sLastArrivedSN = mSenderInfo->getLastArrivedSN();
    }
    return sLastArrivedSN;
}
