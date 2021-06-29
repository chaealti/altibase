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
 * $Id: smiTrans.cpp 90824 2021-05-13 05:35:21Z minku.kang $
 **********************************************************************/

#include <idl.h>
#include <ide.h>
#include <smErrorCode.h>
#include <smx.h>
#include <smr.h>
#include <smiTrans.h>
#include <smiLegacyTrans.h>
#include <smiMain.h>

smiTransactionalDDLCallback smiTrans::mTransactionalDDLCallback;

IDE_RC smiTrans::initialize()
{
    mTrans          = NULL;
    mStmtListHead   = NULL;

    /* smiTrans_initialize_malloc_StmtListHead.tc */
    IDU_FIT_POINT_RAISE("smiTrans::initialize::malloc::StmtListHead",insufficient_memory);
    IDE_TEST_RAISE( iduMemMgr::malloc( IDU_MEM_SM_SMI,
                                       ID_SIZEOF(smiStatement),
                                       (void **)&mStmtListHead) != IDE_SUCCESS,
                    insufficient_memory );

    /* PROJ-1381 Fetch Across Commits
     * mStmtListHead�� mAllPrev/mAllNext�� �̰������� �ʱ�ȭ �Ѵ�.
     * Commit ���Ŀ��� Legacy TX�� STMT�� ������ �� �־���ϱ� �����̴�. */
    mStmtListHead->mAllPrev =
    mStmtListHead->mAllNext = mStmtListHead;

    mCursorListHead.mAllPrev =
    mCursorListHead.mAllNext = &mCursorListHead;

    mStmtListHead->mChildStmtCnt = 0;
    mStmtListHead->mUpdate       = NULL;

    mImpSVP4Shard = NULL; /* BUG-46786 */

    return IDE_SUCCESS;

    IDE_EXCEPTION( insufficient_memory );
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_InsufficientMemory));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC smiTrans::initializeInternal( void )
{
    IDE_ASSERT( mStmtListHead   != NULL );

    mStmtListHead->mTrans        = this;
    mStmtListHead->mParent       = NULL;
    mStmtListHead->mPrev         = mStmtListHead;
    mStmtListHead->mNext         = mStmtListHead;
    mStmtListHead->mUpdate       = NULL;
    mStmtListHead->mChildStmtCnt = 0;
    mStmtListHead->mFlag         = SMI_STATEMENT_NORMAL;

    mCursorListHead.mPrev = &mCursorListHead;
    mCursorListHead.mNext = &mCursorListHead;

    SM_LSN_INIT( mBeginLSN );
    SM_LSN_INIT( mCommitLSN );
    return IDE_SUCCESS;
}

IDE_RC smiTrans::destroy( idvSQL * /*aStatistics*/ )
{
    if ( mStmtListHead != NULL )
    {
        /* PROJ-1381 Fetch Across Commits
         * for XA, replication thread */
        IDE_TEST_RAISE( mStmtListHead->mUpdate != NULL,
                        ERR_UPDATE_STATEMENT_EXIST );

        IDE_TEST_RAISE( mStmtListHead->mChildStmtCnt != 0,
                        ERR_STATEMENT_EXIST );

        IDE_ASSERT( iduMemMgr::free( mStmtListHead ) == IDE_SUCCESS );
        mStmtListHead = NULL;
    }
    else
    {
        /* nothing to do */
    }

    if ( mTrans != NULL )
    {
        /* BUG-48210 commit ���п� ���� ���� ó�� ���� */
        IDE_TEST_RAISE( ((smxTrans*)mTrans)->mStatus != SMX_TX_END,
                        ERR_UNTERMINATIED_TRANSACTION );

        IDE_TEST( smxTransMgr::freeTrans( (smxTrans*)mTrans )
                  != IDE_SUCCESS );
        mTrans = NULL;
    }
    else
    {
        /* nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_UNTERMINATIED_TRANSACTION );
    {
        /* BUG-48210 commit ���п� ���� ���� ó�� ���� */
        ideLog::logCallStack( IDE_ERR_0 );
        ideLog::log( IDE_ERR_0,
                     "transaction was not commited\n"
                     "transaction ID: %"ID_UINT32_FMT", status: %"ID_UINT32_FMT"\n",
                     smxTrans::getTransID( mTrans ),
                     ((smxTrans*)mTrans)->mStatus ) ;
        IDE_SET( ideSetErrorCode( smERR_ABORT_INTERNAL ) );
    }
    IDE_EXCEPTION( ERR_UPDATE_STATEMENT_EXIST );
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_smiUpdateStatementExist ) );
    }
    IDE_EXCEPTION( ERR_STATEMENT_EXIST );
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_smiStatementExist ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

void smiTrans::setDistTxInfo( smiDistTxInfo * aDistTxInfo )
{
    if ( ( aDistTxInfo != NULL ) && SMI_DIST_LEVEL_IS_VALID( aDistTxInfo->mDistLevel ) )
    {
        ((smxTrans*)getTrans())->setDistTxInfo( aDistTxInfo );
    }
}

IDE_RC smiTrans::begin(smiStatement** aStatement,
                       idvSQL        *aStatistics,
                       UInt           aFlag,
                       UInt           aReplID,
                       idBool         aIgnoreRetry,
                       idBool         aIsServiceTX )
{
    smxTrans* sTrans = (smxTrans*)mTrans;

    /* PROJ-1381 Fetch Across Commits
     * for XA, replication */
    IDE_TEST_RAISE( mStmtListHead->mUpdate != NULL,
                    ERR_UPDATE_STATEMENT_EXIST );

    (void)initializeInternal();

    if ( mTrans == NULL )
    {
        IDU_FIT_POINT( "smiTrans::begin::alloc" );
        IDE_TEST( smxTransMgr::alloc( (smxTrans **)&mTrans,
                                      aStatistics,
                                      aIgnoreRetry )
                  != IDE_SUCCESS );
    }
    else
    {
        /* nothing to do */
    }

    IDE_TEST( ((smxTrans*)mTrans)->begin( aStatistics,
                                          aFlag,
                                          aReplID,
                                          aIsServiceTX )
              != IDE_SUCCESS );

    mFlag = aFlag;

    mStmtListHead->mTransID = smxTrans::getTransID( mTrans );
    mStmtListHead->mFlag    = ( mFlag & SMI_TRANSACTION_MASK )
                                != SMI_TRANSACTION_UNTOUCHABLE ?
                           SMI_STATEMENT_NORMAL : SMI_STATEMENT_UNTOUCHABLE;

    *aStatement = mStmtListHead;

    (((smxTrans*)mTrans)->mSmiTransPtr) = this;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_UPDATE_STATEMENT_EXIST );
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_smiUpdateStatementExist ) );

        /* BUG-42584 INC-30976 �ذ��� ���� ����� �ڵ� �߰� */
        ideLog::log( IDE_SM_0,
                     "Transaction has child statement.\n"
                     "Statement Info\n"
                     "Trans Ptr        : 0x%"ID_xPOINTER_FMT"\n"
                     "Trans ID         : %"ID_UINT32_FMT"\n"
                     "Child Statements : %"ID_UINT32_FMT"\n"
                     "SCN              : %"ID_UINT64_FMT"\n"
                     "SCN HEX          : 0x%"ID_XINT64_FMT"\n"
                     "InfiniteSCN      : %"ID_UINT64_FMT"\n"
                     "InfiniteSCN HEX  : 0x%"ID_XINT64_FMT"\n"
                     "Flag             : %"ID_UINT32_FMT"\n"
                     "Depth            : %"ID_UINT32_FMT"\n",
                     mStmtListHead->mTrans,
                     mStmtListHead->mTransID,
                     mStmtListHead->mChildStmtCnt,
                     SM_SCN_TO_LONG( mStmtListHead->mSCN ),
                     SM_SCN_TO_LONG( mStmtListHead->mSCN ),
                     SM_SCN_TO_LONG( mStmtListHead->mInfiniteSCN ),
                     SM_SCN_TO_LONG( mStmtListHead->mInfiniteSCN ),
                     mStmtListHead->mFlag,
                     mStmtListHead->mDepth );
    }
    IDE_EXCEPTION_END;

    if ( mTrans != NULL )
    {
        /* BUG-46782 Begin transaction ����� ���� �߰�.
         * mTrans NULL�� ����, ������ �Ҵ� �� ���� ����, ���� �Ҵ絵 ������ ����̴�.
         *   1. mTrans == NULL , sTrans == NULL Transaction alloc ����, �� ���� ��� ���� ����
         * > 2. mTrans != NULL , sTrans == NULL ���� Transaction alloc �޾Ƽ� begin ����
         * > 3. mTrans == sTrans != NULL        ������ alloc ���� transaction ��Ȱ��
         *   4. mTrans != sTrans != NULL        ���� ����� ��
         * */

        ideLog::log( IDE_ERR_0,
                     "smiTrans Info\n"
                     "Trans Ptr    : 0x%"ID_xPOINTER_FMT", 0x%"ID_xPOINTER_FMT"\n"
                     "Flag         : %"ID_UINT32_FMT"\n"
                     "BeginLSN     : %"ID_UINT32_FMT", %"ID_UINT32_FMT"\n"
                     "CommitLSN    : %"ID_UINT32_FMT", %"ID_UINT32_FMT"\n",
                     sTrans,
                     mTrans,
                     mFlag,
                     mBeginLSN.mFileNo,
                     mBeginLSN.mOffset,
                     mCommitLSN.mFileNo,
                     mCommitLSN.mOffset );

        ideLog::log( IDE_ERR_0,
                     "smxTrans Info\n"
                     "TransID           : %"ID_UINT32_FMT"\n"
                     "Flag              : %"ID_UINT32_FMT"\n"
                     "SessionID         : %"ID_UINT32_FMT"\n"
                     "Status            : %"ID_UINT32_FMT"\n"
                     "Repl ID           : %"ID_UINT32_FMT"\n"
                     "IsFree            : %"ID_UINT32_FMT"\n"
                     "CommitSCN         : %"ID_UINT64_FMT"\n"
                     "CommitSCN HEX     : 0x%"ID_XINT64_FMT"\n"
                     "InfiniteSCN       : %"ID_UINT64_FMT"\n"
                     "InfiniteSCN HEX   : 0x%"ID_XINT64_FMT"\n"
                     "MinMemViewSCN     : %"ID_UINT64_FMT"\n"
                     "MinMemViewSCN HEX : 0x%"ID_XINT64_FMT"\n"
                     "MinDskViewSCN     : %"ID_UINT64_FMT"\n"
                     "MinDskViewSCN HEX : 0x%"ID_XINT64_FMT"\n",
                     ((smxTrans*)mTrans)->mTransID,
                     ((smxTrans*)mTrans)->mFlag,
                     ((smxTrans*)mTrans)->mSessionID,
                     ((smxTrans*)mTrans)->mStatus,
                     ((smxTrans*)mTrans)->mReplID,
                     ((smxTrans*)mTrans)->mIsFree,
                     SM_SCN_TO_LONG( ((smxTrans*)mTrans)->mCommitSCN ),
                     SM_SCN_TO_LONG( ((smxTrans*)mTrans)->mCommitSCN ),
                     SM_SCN_TO_LONG( ((smxTrans*)mTrans)->mInfinite ),
                     SM_SCN_TO_LONG( ((smxTrans*)mTrans)->mInfinite ),
                     SM_SCN_TO_LONG( ((smxTrans*)mTrans)->mMinMemViewSCN ),
                     SM_SCN_TO_LONG( ((smxTrans*)mTrans)->mMinMemViewSCN ),
                     SM_SCN_TO_LONG( ((smxTrans*)mTrans)->mMinDskViewSCN ), 
                     SM_SCN_TO_LONG( ((smxTrans*)mTrans)->mMinDskViewSCN ) );
        IDE_ASSERT( 0 );
    }
    else
    {
        /* alloc transaction�� ������ ���� ���� �� �ִ�.
         * Debug Info �� Assert ���� ����ó���� ��*/
    }

    return IDE_FAILURE;

}

IDE_RC smiTrans::savepoint(const SChar* aSavePoint,
                           smiStatement *aStatement )
{

    if(aStatement != NULL)
    {
        IDE_TEST_RAISE( aStatement->mParent->mParent != NULL,
                        ERR_NOT_ROOT_STATEMENT);
    }

    IDU_FIT_POINT_RAISE( "smiTrans::savepoint::ERR_UPDATE_STATEMENT_EXIST", ERR_UPDATE_STATEMENT_EXIST );

    IDE_TEST_RAISE( mStmtListHead->mUpdate != NULL,
                    ERR_UPDATE_STATEMENT_EXIST );

    IDE_TEST( ((smxTrans*)mTrans)->setExpSavepoint( aSavePoint )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_UPDATE_STATEMENT_EXIST );
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_smiUpdateStatementExist ) );
    }
    IDE_EXCEPTION( ERR_NOT_ROOT_STATEMENT );
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_smiNotRootStatement ) );
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

void smiTrans::reservePsmSvp( idBool aIsShard )
{
    ((smxTrans*)mTrans)->reservePsmSvp( aIsShard );
}

void smiTrans::clearPsmSvp( )
{
    ((smxTrans*)mTrans)->clearPsmSvp();
}

IDE_RC smiTrans::abortToPsmSvp( )
{
    IDE_TEST( ((smxTrans *)mTrans)->abortToPsmSvp() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* BUG-48489 */
idBool smiTrans::isExistExpSavepoint(const SChar *aSavepointName)
{
    return ((smxTrans *)mTrans)->isExistExpSavepoint(aSavepointName);
}

// TASK-7244 PSM partial rollback in Sharding
idBool smiTrans::isShardPsmSvpReserved( )
{
    idBool sRet = ID_FALSE;

    if ( mTrans != NULL )
    {
        sRet = smxTrans::isShardPsmSvpReserved( mTrans );
    }

    return sRet ;
}


// partial rollback �Ǵ� total rollback�� �Ѵ�.
// total rollback�� transaction slot�� free�Ѵ�.
IDE_RC smiTrans::rollback(const SChar* aSavePoint,
                          UInt         aTransReleasePolicy )
{
    idBool      sIsLegacyTrans = ID_FALSE;
    void      * sLegacyTrans   = NULL;

    IDU_FIT_POINT_RAISE( "smiTrans::rollback::ERR_UPDATE_STATEMENT_EXIST", ERR_UPDATE_STATEMENT_EXIST );

    IDE_TEST_RAISE( mStmtListHead->mUpdate != NULL,
                    ERR_UPDATE_STATEMENT_EXIST );

    /* BUG-46786 : Tx�� ��ϵ� implicit savepoint�� �����Ѵ�. */
    if ( mImpSVP4Shard != NULL  ) 
    {
        IDE_TEST( ((smxTrans *)mTrans)->unsetImpSavepoint( mImpSVP4Shard )
                  != IDE_SUCCESS );

        mImpSVP4Shard = NULL;
    }

    if(aSavePoint != NULL)
    {
        /* partial rollback */
        IDE_TEST( ((smxTrans*)mTrans)->abortToExpSavepoint(aSavePoint)
                  != IDE_SUCCESS );
    }
    else
    {
        /* total rollback. */

        /* PROJ-2694 Fetch Across Rollback
         * rollback ������ ChildStmt�� ���� �ִٸ� cursor�� view�� �����ϱ� ����
         * Legacy Trans�� �����ؾ� �� �� �ִ�.
         * ��, rollback���� ���� view�� ���� ��쿡�� cursor�� ������ �Ұ����ϹǷ�
         * �� ��쿡�� cursor�� �������� �ʴ´�. */
        sIsLegacyTrans = isReusableRollback();

        IDE_TEST( ((smxTrans*)mTrans)->abort( sIsLegacyTrans,
                                              &sLegacyTrans ) != IDE_SUCCESS );

        if ( sIsLegacyTrans == ID_FALSE )
        {
            IDE_TEST_RAISE( mStmtListHead->mChildStmtCnt != 0,
                            ERR_STATEMENT_EXIST );
        }
        else
        {
            if( mStmtListHead->mChildStmtCnt != 0 )
            {
                IDE_TEST( smiLegacyTrans::makeLegacyStmt( sLegacyTrans,
                            mStmtListHead )
                        != IDE_SUCCESS );
            }
            else
            {
                /* rollback ������ ChildStmt�� ���� ��� legacyTx�� ������ �ʿ䰡 ����. */
            }
        }

        if ( aTransReleasePolicy == SMI_RELEASE_TRANSACTION )
        {
            IDE_TEST( smxTransMgr::freeTrans( (smxTrans*)mTrans )
                      != IDE_SUCCESS );
            mTrans = NULL;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_UPDATE_STATEMENT_EXIST );
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_smiUpdateStatementExist ) );
    }
    IDE_EXCEPTION( ERR_STATEMENT_EXIST );
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_smiStatementExist ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

IDE_RC smiTrans::commit( smSCN * aCommitSCN,
                         UInt    aTransReleasePolicy )
{
    idBool      sIsLegacyTrans  = ID_FALSE;
    idBool      sWriteCommitLog = ID_FALSE; /* BUG-41342 */
    void      * sLegacyTrans    = NULL;

    IDU_FIT_POINT_RAISE( "smiTrans::commit::ERR_UPDATE_STATEMENT_EXIST", ERR_UPDATE_STATEMENT_EXIST );
    IDE_TEST_RAISE( mStmtListHead->mUpdate != NULL,
                    ERR_UPDATE_STATEMENT_EXIST );

    /* BUG-46786 : Tx�� ��ϵ� implicit savepoint�� �����Ѵ�. */
    if ( mImpSVP4Shard != NULL ) 
    {
        IDE_TEST( ((smxTrans *)mTrans)->unsetImpSavepoint( mImpSVP4Shard )
                  != IDE_SUCCESS );

        mImpSVP4Shard = NULL;
    }

    /* PROJ-1381 Fetch Across Commits
     * commit ������ ChildStmt�� ���������� fetch�� ��� �� �� �ֵ���
     * Legacy Trans�� �����Ѵ�. */
    if ( mStmtListHead->mChildStmtCnt != 0 )
    {
        /* Autocommit Mode�̰ų� Ʈ�������� ������ ������ ����
         * Release Policy�� SMI_RELEASE_TRANSACTION���� �Ѵ�.
         * �̶��� commit ������ �����ִ� ChildStmt�� �־�� �ȵȴ�. */
        IDE_TEST_RAISE( aTransReleasePolicy == SMI_RELEASE_TRANSACTION,
                        ERR_STATEMENT_EXIST );

        sIsLegacyTrans = ID_TRUE;
    }

    IDU_FIT_POINT( "smiTrans::commit" );

    IDE_TEST( ((smxTrans*)mTrans)->commit( aCommitSCN,
                                           sIsLegacyTrans,
                                           &sLegacyTrans )
              != IDE_SUCCESS );
    sWriteCommitLog = ID_TRUE;


    /* PROJ-1381 Fetch Across Commits */
    if ( sIsLegacyTrans == ID_TRUE )
    {
        IDE_DASSERT( sLegacyTrans != NULL );

        IDE_TEST( smiLegacyTrans:: makeLegacyStmt( sLegacyTrans,
                                                   mStmtListHead )
                  != IDE_SUCCESS );
    }
    else
    {
        /* nothing to do */
    }

    SM_GET_LSN( mBeginLSN, ((smxTrans*)mTrans)->getBeginLSN() );
    SM_GET_LSN( mCommitLSN, ((smxTrans*)mTrans)->getCommitLSN() );

    if ( aTransReleasePolicy == SMI_RELEASE_TRANSACTION )
    {
        IDE_TEST( smxTransMgr::freeTrans( (smxTrans*)mTrans ) != IDE_SUCCESS );
        mTrans = NULL;
    }
    else
    {
        /* nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_UPDATE_STATEMENT_EXIST );
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_smiUpdateStatementExist ) );
    }
    IDE_EXCEPTION( ERR_STATEMENT_EXIST );
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_smiStatementExist ) );
    }
    IDE_EXCEPTION_END;

    /* BUG-41342 Commit Log�� ���� �Ŀ��� ����ó���ϸ� �ȵȴ�. */
    IDE_ASSERT( sWriteCommitLog == ID_FALSE );

    return IDE_FAILURE;
}

smTID smiTrans::getTransID()
{
    /* ------------------------------------------------
     *  mTrans�� NULL�� �� �ֱ� ������ �ӽ����� �� check & getID
     * ----------------------------------------------*/
    smxTrans *sTrans = (smxTrans*)mTrans;
    return (sTrans == NULL) ? 0 : ((smTID)((smxTrans*)sTrans)->mTransID);
}

/* -----------------------------------
     For Global Transaction
   ---------------------------------- */
/* BUG-18981 */
IDE_RC smiTrans::prepare( ID_XID *aXID, smSCN * aPrepareSCN, idBool aLogging )
{

    IDE_TEST( ((smxTrans*)mTrans)->prepare( aXID, 
                                            aPrepareSCN, 
                                            aLogging ) 
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}


IDE_RC smiTrans::attach( SInt aSlotID )
{

    smxTrans *sTrans;
    
    sTrans = smxTransMgr::getTransBySID(aSlotID);
    mTrans   = sTrans;

    return IDE_SUCCESS;

}

IDE_RC smiTrans::dettach()
{
    IDE_TEST_RAISE( mStmtListHead->mUpdate != NULL,
                    ERR_UPDATE_STATEMENT_EXIST );

    /* BUG-46786 : Tx�� ��ϵ� implicit savepoint�� �����Ѵ�. */
    if ( mImpSVP4Shard != NULL ) 
    {
        IDE_TEST( ((smxTrans *)mTrans)->unsetImpSavepoint( mImpSVP4Shard )
                  != IDE_SUCCESS );

        mImpSVP4Shard = NULL;
    }

    mTrans = NULL;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_UPDATE_STATEMENT_EXIST );
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_smiUpdateStatementExist ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC smiTrans::realloc( idvSQL *aStatistics, idBool aIgnoreRetry )
{
    if ( mTrans == NULL )
    {
        IDE_TEST( smxTransMgr::alloc( (smxTrans **)&mTrans,
                                      aStatistics,
                                      aIgnoreRetry )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC smiTrans::isReadOnly(idBool *aIsReadOnly)
{
    idBool sIsReadOnly           = ID_FALSE;
    idBool sIsVolatileTBSTouched = ID_FALSE;

    sIsReadOnly = ((smxTrans *)mTrans)->isReadOnly();
    if ( sIsReadOnly == ID_TRUE )
    {
        /* BUG-42991 smiTrans::isReadOnly()���� Volatile Tablespace�� �˻��ϵ��� �մϴ�. */
        sIsVolatileTBSTouched = ((smxTrans *)mTrans)->isVolatileTBSTouched();
        if ( sIsVolatileTBSTouched == ID_FALSE )
        {
            *aIsReadOnly = ID_TRUE;
        }
        else
        {
            *aIsReadOnly = ID_FALSE;
        }
    }
    else
    {
        *aIsReadOnly = ID_FALSE;
    }

    return IDE_SUCCESS;
}

void smiTrans::showOIDList()
{

    (void)(((smxTrans*)mTrans)->showOIDList());

}

UInt smiTrans::getFirstUpdateTime()
{
    smxTrans *sTrans;

    sTrans = (smxTrans *)mTrans; /* store it for preventing NULL-reference.
                                    this should be atomic-operation.*/

    if (sTrans != NULL)
    {
        return sTrans->getFstUpdateTime();
    }
    else
    {
        return 0; /* already committed or assigned other smiTrans */
    }
}

// QP���� Meta�� ���ٵ� ��� �� �Լ��� ȣ���Ͽ�
// Transaction�� Meta���� ���θ� �����Ѵ�
IDE_RC smiTrans::setMetaTableModified()
{
    ((smxTrans*)mTrans)->setMetaTableModified();

    return IDE_SUCCESS;
}

smSN smiTrans::getBeginSN()
{
    return SM_MAKE_SN( mBeginLSN );
}
smSN smiTrans::getCommitSN()
{
    return SM_MAKE_SN( mCommitLSN );
}

/*******************************************************************************
 * Description : DDL Transaction�� ǥ���ϴ� Log Record�� ����Ѵ�.
 ******************************************************************************/
IDE_RC smiTrans::writeDDLLog()
{
    return ((smxTrans*)mTrans)->writeDDLLog();
}

/*******************************************************************************
 * Description : Staticstics�� ��Ʈ�Ѵ�.
 *
 *  BUG-22651  smrLogMgr::updateTransLSNInfo����
 *             ����������Ǵ� ��찡 �����ֽ��ϴ�.
 ******************************************************************************/
void smiTrans::setStatistics( idvSQL * aStatistics )
{
    ((smxTrans*)mTrans)->setStatistics( aStatistics );
}

idvSQL * smiTrans::getStatistics( void )
{
    idvSQL * sStatistics = NULL;

    if ( mTrans != NULL )
    {
        sStatistics = ((smxTrans*)mTrans)->getStatistics( mTrans );
    }

    return sStatistics;
}

IDE_RC smiTrans::setReplTransLockTimeout( UInt aReplTransLockTimeout )
{
    IDE_DASSERT( mTrans != NULL );
    return ((smxTrans*)mTrans)->setReplLockTimeout( aReplTransLockTimeout );
}

UInt smiTrans::getReplTransLockTimeout( )
{
    IDE_DASSERT( mTrans != NULL );
    return ((smxTrans*)mTrans)->getReplLockTimeout();
}

idBool smiTrans::isBegin()
{
    idBool sIsBegin = ID_FALSE;

    if ( mTrans != NULL )
    {
        sIsBegin = smxTrans::isTxBeginStatus((smxTrans*)mTrans);
    }
    else
    {
        /* Nothing to do */
    }

    return sIsBegin;
}

idBool smiTrans::isReusableRollback( void )
{
    smxTrans    * sTrans = (smxTrans*)mTrans;
    idBool        sResult;

    if ( ( sTrans != NULL ) && 
         ( sTrans->mIsReusableRollback == ID_TRUE ) && 
         ( mStmtListHead->mChildStmtCnt != 0 ) ) 
    {   
        sResult = ID_TRUE;
    }   
    else
    {   
        sResult = ID_FALSE;
    }   

    return sResult;
}

void smiTrans::setCursorHoldable( void )
{
    smxTrans    * sTrans = (smxTrans*)mTrans;

    if( sTrans != NULL )
    {
        sTrans->mIsCursorHoldable = ID_TRUE;
    }
}

/* BUG-46786
   smiTrans�� ����� implicit savepoint�� �ִ��� Ȯ���Ѵ�. */
idBool smiTrans::checkImpSVP4Shard( smiTrans * aTrans )
{
    if ( ( aTrans != NULL ) &&
         ( aTrans->mTrans != NULL ) &&
         ( aTrans->mImpSVP4Shard != NULL ) )
    {
        return ID_TRUE;
    }
    else
    {
        return ID_FALSE;
    }
}

/* BUG-46786
   smiTrans�� ����� implicit savepoint���� ROLLBACK �Ѵ�. */
IDE_RC smiTrans::abortToImpSVP4Shard( smiTrans * aTrans )
{
    IDE_DASSERT( checkImpSVP4Shard( aTrans ) == ID_TRUE );

    IDE_TEST( ((smxTrans*)aTrans->mTrans)->abortToImpSavepoint( aTrans->mImpSVP4Shard )
              != IDE_SUCCESS );

    IDE_TEST( ((smxTrans *)aTrans->mTrans)->unsetImpSavepoint( aTrans->mImpSVP4Shard )
              != IDE_SUCCESS );

    aTrans->mImpSVP4Shard = NULL;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC smiTrans::setExpSvpForBackupDDLTargetTableInfo( smOID   aOldTableOID, 
                                                       UInt    aOldPartOIDCount,
                                                       smOID * aOldPartOIDArray,
                                                       smOID   aNewTableOID,
                                                       UInt    aNewPartOIDCount,
                                                       smOID * aNewPartOIDArray )
{
    IDE_TEST_RAISE( mStmtListHead->mUpdate != NULL,
                    ERR_UPDATE_STATEMENT_EXIST );

    IDE_TEST( ((smxTrans*)mTrans)->setExpSvpForBackupDDLTargetTableInfo( aOldTableOID, 
                                                                         aOldPartOIDCount,
                                                                         aOldPartOIDArray,
                                                                         aNewTableOID,
                                                                         aNewPartOIDCount,                       
                                                                         aNewPartOIDArray ) 
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_UPDATE_STATEMENT_EXIST );
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_smiUpdateStatementExist ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC smiTrans::allocNSetDDLTargetTableInfo( UInt                     aTableID, 
                                              void                   * aOldTableInfo,
                                              void                   * aNewTableInfo,
                                              idBool                   aIsReCreated,
                                              smiDDLTargetTableInfo ** aInfo )
{
    smiDDLTargetTableInfo * sInfo = NULL;

    IDU_FIT_POINT_RAISE( "smiTrans::allocNSetDDLTargetTableInfo::malloc::insufficient_memory",
                      insufficient_memory );
    IDE_TEST_RAISE( iduMemMgr::malloc( IDU_MEM_SM_SMI,
                                       ID_SIZEOF(smiDDLTargetTableInfo),
                                       (void **)&sInfo) != IDE_SUCCESS,
                    insufficient_memory );

    sInfo->mTableID = aTableID;
    sInfo->mOldTableInfo = aOldTableInfo;
    sInfo->mNewTableInfo = aNewTableInfo;
    sInfo->mIsReCreated  = aIsReCreated; 
    IDU_LIST_INIT( &(sInfo->mPartInfoList) );

    *aInfo = sInfo;

    return IDE_SUCCESS;

    IDE_EXCEPTION( insufficient_memory );
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_InsufficientMemory));
    }
    IDE_EXCEPTION_END;

    if ( sInfo != NULL )
    {
        iduMemMgr::free( sInfo );
        sInfo = NULL;
    }

    return IDE_FAILURE;
}

void smiTrans::freeDDLTargetTableInfo( smiDDLTargetTableInfo * aDDLTargetTableInfo )
{
    if ( aDDLTargetTableInfo != NULL )
    {
        freeDDLTargetPartTableInfo( aDDLTargetTableInfo );

        iduMemMgr::free( aDDLTargetTableInfo );
    }
}

IDE_RC smiTrans::allocNSetDDLTargetPartTableInfo( smiDDLTargetTableInfo * aInfo,                                 
                                                  UInt                    aTableID, 
                                                  idBool                  aIsRecreated,
                                                  void                  * aPartOldTableInfo,
                                                  void                  * aPartNewTableInfo )
{
    smiDDLTargetTableInfo * sPartInfo = NULL;

    IDE_TEST( allocNSetDDLTargetTableInfo( aTableID,
                                           aPartOldTableInfo,
                                           aPartNewTableInfo,
                                           aIsRecreated,
                                           &sPartInfo )
              != IDE_SUCCESS );

    IDU_LIST_INIT( &(sPartInfo->mPartInfoList) );

    IDU_LIST_INIT_OBJ( &(sPartInfo->mNode), (void*)sPartInfo );
    IDU_LIST_ADD_LAST( &(aInfo->mPartInfoList), &(sPartInfo->mNode) );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sPartInfo != NULL )
    {
        iduMemMgr::free( sPartInfo );
        sPartInfo = NULL;
    }

    return IDE_FAILURE;
}

void smiTrans::freeDDLTargetPartTableInfo( smiDDLTargetTableInfo * aInfo )
{
    iduListNode * sPartNode  = NULL;
    iduListNode * sPartDummy = NULL;
    smiDDLTargetTableInfo * sPartInfo  = NULL;

    IDE_DASSERT( aInfo != NULL );

    IDU_LIST_ITERATE_SAFE( &(aInfo->mPartInfoList), sPartNode, sPartDummy )
    {
        sPartInfo = (smiDDLTargetTableInfo*)sPartNode->mObj;

        IDU_LIST_REMOVE( &( sPartInfo->mNode ) );
        iduMemMgr::free( sPartInfo );
    }
}

/* BUG-48250 : TX�� ������ Session Propery�� INDOUBT_FETCH_TIMEOUT�� �����Ѵ�. */
void smiTrans::setIndoubtFetchTimeout( UInt aTimeout )
{
    smxTrans * sTrans = (smxTrans *)mTrans;

    if ( ( sTrans != NULL ) &&
         ( sTrans->mStatus != SMX_TX_END ) )
    {
        sTrans->mIndoubtFetchTimeout = aTimeout;
    }
}
/* BUG-48250 : TX�� ������ Session Propery�� INDOUBT_FETCH_METHOD�� �����Ѵ�. */
void smiTrans::setIndoubtFetchMethod( UInt aMethod )
{
    smxTrans * sTrans = (smxTrans *)mTrans;

    if ( ( sTrans != NULL ) &&
         ( sTrans->mStatus != SMX_TX_END ) )
    {
        sTrans->mIndoubtFetchMethod = aMethod;
    }
}
/* BUG-48829 :TX�� ������ Session Propery�� GLOBAL_TRANSACTION_LEVEL �� �����Ѵ�. */
void smiTrans::setGlobalTransactionLevel( idBool aIsGCTx )
{
    smxTrans * sTrans = (smxTrans *)mTrans;

    if ( ( sTrans != NULL ) &&
         ( sTrans->mStatus != SMX_TX_END ) )
    {
        sTrans->mIsGCTx = aIsGCTx ;

        if ( aIsGCTx == ID_FALSE )
        {
            if( sTrans->mLegacyTransCnt == 0 )
            {
                SM_SET_SCN_INFINITE( &(sTrans->mLastRequestSCN) );
            }
        }
    }
}
