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
 * $Id: smiStatement.cpp 90296 2021-03-23 07:35:09Z justin.kwon $
 **********************************************************************/

#include <idl.h>
#include <ide.h>

#include <smErrorCode.h>
#include <smr.h>
#include <smx.h>
#include <smiTrans.h>
#include <smxTrans.h>
#include <smlLockMgr.h>
#include <smiStatement.h>
#include <sdcDef.h>
#include <smiLegacyTrans.h>

/* Stmt�� Cursor ������ ���� Ʈ������� MinViewSCN���� �����ϴ� �Լ����� �����Ѵ�. */
static smiTryUptTransViewSCNFunc
             gSmiTryUptTransViewSCNFuncs[ SMI_STATEMENT_CURSOR_MASK + 1 ] = {
    NULL,
    smiStatement::tryUptTransMemViewSCN, // SMI_STATEMENT_MEMORY_CURSOR
    smiStatement::tryUptTransDskViewSCN, // SMI_STATEMENT_DISK_CURSOR
    smiStatement::tryUptTransAllViewSCN  // SMI_STATEMENT_ALL_CURSOR
};

/***********************************************************************
 * Description : Statement�� Begin�Ѵ�.
 *
 * aParent - [IN] Parent Statement
 * aFlag   - [IN] Statement�Ӽ�
 *                1. SMI_STATEMENT_NORMAL | SMI_STATEMENT_UNTOUCHABLE
 *                2. SMI_STATEMENT_MEMORY_CURSOR | SMI_STATEMENT_DISK_CURSOR
 *                   | SMI_STATEMENT_ALL_CURSOR
 * aRequestSCN - Global Consistent Transaction ���� stmt�� ��û SCN �� ������ �ִ�
 **********************************************************************/
IDE_RC smiStatement::begin( idvSQL        * aStatistics,
                            smiStatement  * aParent,
                            UInt            aFlag,
                            smSCN           aRequestSCN,
                            smiDistTxInfo * aDistTxInfo,
                            idBool          aIsPartialStmt, /* TASK-7219 Non-shard DML */
                            UInt            aStmtSeq )
{
    smSCN          sSCN;
    smxTrans    *  sTrans;
    PDL_Time_Value sZeroTime;
#ifdef DEBUG
    PDL_Time_Value sNowTime;
#endif
     
    UInt  sState = 0;
    

    mStatistics = aStatistics;

    mLockSlotSequence = 0;
    mISavepoint = NULL;

    if( aParent->mParent == NULL )
    {
        mRoot  = this;
        mDepth = 1;
    }
    else
    {
        // child Statement �� ������ Statement ���� RequestSCN �� �䱸���� �ʴ´�.  
        IDE_DASSERT( SM_SCN_IS_INIT(aRequestSCN) );

        mRoot  = aParent->mRoot;
        /* BUG-17033: �ֻ��� Statement�� �ƴ� Statment�� ���ؼ���
         * Partial Rollback�� �����ؾ� �մϴ�. */
        mDepth = aParent->mDepth + 1;

        IDU_FIT_POINT_RAISE( "smiStatement::begin::ERR_MAX_DELPTH_LEVEL", ERR_MAX_DELPTH_LEVEL);
        IDE_TEST_RAISE( mDepth > SMI_STATEMENT_DEPTH_MAX,
                        ERR_MAX_DELPTH_LEVEL);

    }
    sState = 1;

    sTrans = (smxTrans*)aParent->mTrans->getTrans();

    IDE_DASSERT( sTrans->mStatus      == SMX_TX_BEGIN );
    IDE_DASSERT( sTrans->mStatus4FT   == SMX_TX_BEGIN );
    // ���� Tx �� prepare�� ���·� ���ü� �־ Ȯ���ϸ� �ȵȴ�. 
    //IDE_DASSERT( sTrans->mCommitState == SMX_XA_START );

    IDU_FIT_POINT_RAISE( "smiStatement::begin::beginFailTest",
                         ERR_CANT_BEGIN_UPDATE_STATEMENT );

    // PROJ-2199 SELECT func() FOR UPDATE ����
    // SMI_STATEMENT_FORUPDATE �߰�
    if( ( ((aFlag & SMI_STATEMENT_MASK) == SMI_STATEMENT_NORMAL) ||
          ((aFlag & SMI_STATEMENT_MASK) == SMI_STATEMENT_FORUPDATE) )
        &&
        // foreign key������ ������ statement�� normal�̳� cursor close���� �����ǹǷ� �˻��� �ʿ䰡 ����.
        ( (aFlag & SMI_STATEMENT_SELF_MASK) == SMI_STATEMENT_SELF_FALSE ) )
    {
        /* BUG-46786 : ���� statement�� implicit savepoint�� �����Ѵ�. */
        if ( ( mDepth == 1 ) &&
             ( aParent->mTrans != NULL ) &&
             ( aParent->mTrans->mImpSVP4Shard != NULL ) )
        {
            IDE_TEST( ((smxTrans *)aParent->mTrans->mTrans)->unsetImpSavepoint( aParent->mTrans->mImpSVP4Shard )
                      != IDE_SUCCESS );

            aParent->mTrans->mImpSVP4Shard = NULL;
            sState = 2;
        }

        IDU_FIT_POINT_RAISE( "smiStatement::begin::ERR_CANT_BEGIN_UPDATE_STATEMENT", ERR_CANT_BEGIN_UPDATE_STATEMENT );
        IDE_TEST_RAISE( (((aParent->mFlag & SMI_STATEMENT_MASK) != SMI_STATEMENT_NORMAL) &&
                         ((aParent->mFlag & SMI_STATEMENT_MASK) != SMI_STATEMENT_FORUPDATE)) ||
                        (aParent->mUpdate != NULL),
                        ERR_CANT_BEGIN_UPDATE_STATEMENT );
        sState = 3;

        aParent->mUpdate = this;

        IDE_TEST( sTrans->setImpSavepoint( &mISavepoint, mDepth )
                  != IDE_SUCCESS );
        sState = 4;
    }

    /* TASK-7219 Non-shard DML
       partial stmt ���࿡ ���� infiniteSCN ����. */
    setInfinteSCN4PartialStmt( sTrans,
                               aIsPartialStmt,
                               aStmtSeq );

    /* BUG-15906: non-autocommit��忡�� Select�Ϸ��� IS_LOCK�� �����Ǹ�
     * ���ڽ��ϴ�.
     * Select Statement�϶� Lock�� ������ Ǯ��� ������ �����ϱ� ����
     * Transaction�� ����� ������ Lock Sequence Number�� mLockSequence��
     * ������ �д�. */

    /* BUG-22639: Child Statement�� ����� �ش� Transaction�� ��� IS Lock
     * �� �����˴ϴ�.
     *
     * ��� Statement�� end�� ISLock Unlock�� �õ��ϱ� ������ ��� Statement��
     * ���ؼ� ���� �����ؾ� �Ѵ�.
     * */

    mLockSlotSequence = smlLockMgr::getLastLockSequence( sTrans->getSlotID() );
    

    /*
       PROJ-2734  : �л� ����� 
       �л극���� �����Ǿ��ٸ�, �ٸ� Distribution Tx Info�鵵 �����Ǿ� �ִ�.
     */
    if ( ( aDistTxInfo != NULL ) && SMI_DIST_LEVEL_IS_VALID( aDistTxInfo->mDistLevel ) )
    {
#ifdef DEBUG 

        /* �л극���� Parallel�̰� Flag �������� ������
           �䱸�� viewSCN�� ���� �� ����. */
        if ( ( SMI_STATEMENT_VIEWSCN_IS_REQUESTED( aFlag ) ) &&
             ( aDistTxInfo->mDistLevel == SMI_DIST_LEVEL_PARALLEL ) )
        {
            IDE_DASSERT( sTrans->mIsGCTx == ID_TRUE );
            IDE_DASSERT( SM_SCN_IS_NOT_INIT(aRequestSCN) );
        }

        sNowTime = idlOS::gettimeofday();

        ideLog::log( IDE_SD_19, 
                     "\n<smiStatement(%"ID_XINT64_FMT")::begin> "
                     "parent(%"ID_XINT64_FMT") smxTrans(%"ID_XINT64_FMT"-%"ID_UINT32_FMT")\n"
                     "FirstStmtViewSCN : %"ID_UINT64_FMT" \n"
                     "FirstStmtTime : [%"ID_INT64_FMT"][%"ID_INT64_FMT"] \n"
                     "(NowTime) : [%"ID_INT64_FMT"][%"ID_INT64_FMT"] \n"
                     "ShardPin : "SMI_SHARD_PIN_FORMAT_STR" \n"
                     "DistLevel : %"ID_INT32_FMT" (Trans DistLevel) : %"ID_INT32_FMT"\n"
                     "Depth : %"ID_UINT32_FMT" \n",
                     this, aParent, sTrans, sTrans->mTransID,
                     aDistTxInfo->mFirstStmtViewSCN,
                     aDistTxInfo->mFirstStmtTime.tv_.tv_sec,
                     aDistTxInfo->mFirstStmtTime.tv_.tv_usec,
                     sNowTime.tv_.tv_sec,
                     sNowTime.tv_.tv_usec,
                     SMI_SHARD_PIN_FORMAT_ARG(aDistTxInfo->mShardPin),
                     aDistTxInfo->mDistLevel, sTrans->mDistTxInfo.mDistLevel,
                     mDepth );
#endif
        sZeroTime.initialize();

        IDE_TEST_RAISE( ( aDistTxInfo->mFirstStmtTime    == sZeroTime ) ||
                        ( aDistTxInfo->mShardPin         == SMI_SHARD_PIN_INVALID ),
                        ERR_INVALID_DIST_INFO );

        sTrans->setDistTxInfo( aDistTxInfo );
    }
    else
    {
        /* Distribution Tx Info�� ����. */

#ifdef DEBUG

        /* Global Consistent Transaction�� �л������� ������ ������ �ִ�. 
         * e.g. ���ο��� ����ϴ°Ŷ����, prepare�����..
         * RequestSCN ���� �䱸���� �ʴ´�.
         * RequestSCN �䱸 ���ߴµ� flag�� �����ɼ� �ִ�. (flag�� GCTx ���� ����)  */
        IDE_DASSERT( SM_SCN_IS_INIT(aRequestSCN) );

        if ( SMI_DIST_LEVEL_IS_VALID( sTrans->mDistTxInfo.mDistLevel ) )
        {
            sNowTime = idlOS::gettimeofday();

            ideLog::log( IDE_SD_19, 
                         "\n<smiStatement(%"ID_XINT64_FMT")::begin> "
                         "parent(%"ID_XINT64_FMT") smxTrans(%"ID_XINT64_FMT"-%"ID_UINT32_FMT") NO DistTxInfo \n"
                         "(FirstStmtViewSCN) : %"ID_UINT64_FMT" \n"
                         "(FirstStmtTime) : [%"ID_INT64_FMT"][%"ID_INT64_FMT"] \n"
                         "(NowTime) : [%"ID_INT64_FMT"][%"ID_INT64_FMT"] \n"
                         "(ShardPin) : "SMI_SHARD_PIN_FORMAT_STR" \n"
                         "(Trans DistLevel) : %"ID_INT32_FMT" \n"
                         "Depth : %"ID_UINT32_FMT" \n",
                         this, aParent, sTrans, sTrans->mTransID,
                         sTrans->mDistTxInfo.mFirstStmtViewSCN,
                         sTrans->mDistTxInfo.mFirstStmtTime.tv_.tv_sec,
                         sTrans->mDistTxInfo.mFirstStmtTime.tv_.tv_usec,
                         sNowTime.tv_.tv_sec,
                         sNowTime.tv_.tv_usec,
                         SMI_SHARD_PIN_FORMAT_ARG(sTrans->mDistTxInfo.mShardPin),
                         sTrans->mDistTxInfo.mDistLevel,
                         mDepth );
        }
        else
        {
            ideLog::log( IDE_SD_19, 
                         "\n<smiStatement(%"ID_XINT64_FMT")::begin> "
                         "parent(%"ID_XINT64_FMT") smxTrans(%"ID_XINT64_FMT"-%"ID_UINT32_FMT") NO SHARD \n",
                         this, aParent, sTrans, sTrans->mTransID );
        }
#endif
    }

    /* Memory Table�Ǵ�  Disk Table�� �����ϴ��� ��� �����ϴ�����
     * ���� ���ڸ� �����Ͽ� �̿� ���� Ʈ������� MemViewSCN,
     * DskViewSCN�� ���Žõ��Ѵ�. */
    IDE_TEST( sTrans->allocViewSCN( aFlag, &sSCN, aRequestSCN ) != IDE_SUCCESS );
    sState = 5;

    SM_GET_SCN( &mRequestSCN, &aRequestSCN );

    mTrans               = aParent->mTrans;
    mParent              = aParent;

    mPrev                = mParent;
    mNext                = mParent->mNext;
    mPrev->mNext         =
    mNext->mPrev         = this;

    /* PROJ-1381 Fetch Across Commits
     * Legacy Trans�� �����ϴ� ��ü stmt List */
    mAllPrev             = mParent;
    mAllNext             = mParent->mAllNext;
    mAllPrev->mAllNext   =
    mAllNext->mAllPrev   = this;

    mUpdate              = NULL;
    mChildStmtCnt        = 0;
    mUpdateCursors.mPrev =
    mUpdateCursors.mNext = &mUpdateCursors;
    mCursors.mPrev       =
    mCursors.mNext       = &mCursors;
    mSCN                 = sSCN;
    mTransID             = smxTrans::getTransID( sTrans );

    //Statement Begin�� ���� Transaction�� InfiniteSCN �����Ѵ�.
    mInfiniteSCN         = sTrans->getInfiniteSCN();
    mFlag                = aFlag;
    mOpenCursorCnt       = 0;

    mParent->mChildStmtCnt++;

    /*
     * [BUG-24187] Rollback�� statement�� Internal CloseCurosr��
     * ������ �ʿ䰡 �����ϴ�.
     */
    mSkipCursorClose = ID_FALSE;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_CANT_BEGIN_UPDATE_STATEMENT )
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_smiCantBeginUpdateStatement ) );
    }
    IDE_EXCEPTION( ERR_MAX_DELPTH_LEVEL )
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_StmtMaxDepthLevel ) );
    }
    IDE_EXCEPTION( ERR_INVALID_DIST_INFO )
    {
        ideLog::log( IDE_SM_0, "[ERR] invalid distribution information" );

        IDE_SET( ideSetErrorCode( smERR_ABORT_INTERNAL_ARG, 
                                  "invalid distribution information" ) );
    }
    IDE_EXCEPTION_END;
    { 
        switch ( sState )
        {
            case 5:
            case 4:
                (void)sTrans->unsetImpSavepoint( mISavepoint );
                aParent->mUpdate = NULL;
            case 3:
            case 2:
            /* �� statement �� ����Ǿ��ٴ°�
             * ���� statement�� partial rollback ���� ���ٴ� ���� 
             * savepoint�� ���� �ʾƵ� ��  */
            case 1:
                
            default:
            break;
        }
    }
    return IDE_FAILURE;
    
}

/* TASK-7219 Non-shard DML
 * partial stmt ���࿡ ���� infiniteSCN ����  */
void smiStatement::setInfinteSCN4PartialStmt( smxTrans * sTrans,
                                              idBool     aIsPartialStmt,
                                              UInt       aStmtSeq )
{
    idBool sPrevStmtIsPartial = sTrans->mIsPartialStmt;
    UInt   sPrevStmtSeq       = sTrans->mStmtSeq;
    idBool sCurrStmtIsPartial = aIsPartialStmt;
    UInt   sCurrStmtSeq       = aStmtSeq;

    if ( aStmtSeq != 0 ) /* Partial stmt ������ �ִٸ� */
    {
        /*****************************************************************/
        /* stmt �� �����Ҷ� infiniteSCN�� ���������ֿ��� ���� �����Ѵ�.  */
        /*****************************************************************/

        if ( ( sPrevStmtIsPartial == ID_TRUE ) &&
             ( sCurrStmtIsPartial == ID_FALSE ) )
        {
            /* CASE 1 : 
               Partial Stmt -> Normal Stmt �� ��ȯ�Ǵ� ���, infiniteSCN �� �÷��ش�. 
               (Partial Stmt���� infiniteSCN �� �������� �ʾұ� �����̴�.)  */

            sTrans->incInfiniteSCNAndGet( &mInfiniteSCN );

        } 
        else if ( ( sPrevStmtIsPartial == ID_TRUE ) &&
                  ( sCurrStmtIsPartial == ID_TRUE ) &&
                  ( sPrevStmtSeq != sCurrStmtSeq ) )
        {
            /* CASE 2 : 
               Partial Stmt -> Partial Stmt �� ��ȯ�Ǵ� ��쿡 Seq ���� ����Ǿ��ٸ�, infiniteSCN�� �÷��ش�.
               (Seq ���� �ٲ���ٴ� ���� �ٸ� Partial Stmt �̱� �����̴�.) */

            //IDE_DASSERT( sPrevStmtSeq < sCurrStmtSeq );

            sTrans->incInfiniteSCNAndGet( &mInfiniteSCN );
        }

        /*****************************************************************/
        /* cursor�� ������ infiniteSCN�� �÷��־�� �ϴ����� �����Ѵ�.   */
        /*****************************************************************/
        if ( sCurrStmtIsPartial == ID_FALSE )
        {
            /* Normal Stmt -> Normal Stmt �ΰ�� cursor ������ infiniteSCN�� �ø��� �ȴ� */

            mIncInfiniteSCN4ClosingCursor = ID_TRUE;
        }
        else
        {
            mIncInfiniteSCN4ClosingCursor = ID_FALSE;
        }

        /* ���� Stmt ���°����� ���� */
        sTrans->mIsPartialStmt = sCurrStmtIsPartial;
        sTrans->mStmtSeq       = sCurrStmtSeq ;

    }
    else /* aStmtSeq == 0 */
    {
        /* Partial Stmt ���ο� Seq�� ���� �ȵȰ���̴�.
           Hard Prepare ���� ����̴�.
           => �������¸� ���� ��� ó������ ���Ѵ�. */

        if ( sPrevStmtIsPartial == ID_TRUE )
        {
            /* �������°� Partial Stmt  */

            mIncInfiniteSCN4ClosingCursor = ID_FALSE;
        }
        else
        {
            /* �������°� Normal Stmt �̸�, cursor close�Ҷ� inifinteSCN�� ������Ų��. */

            mIncInfiniteSCN4ClosingCursor = ID_TRUE;
        }
    }
}

/***********************************************************************
 * Description : Statement�� ���� ��Ų��.
 *
 * aFlag - [IN] Statemnet�� ���� ����
 *           - SMI_STATEMENT_RESULT_SUCCESS | SMI_STATEMENT_RESULT_FAILURE
 **********************************************************************/
IDE_RC smiStatement::end( UInt aFlag )
{
    smiTableCursor *sTempCursor;
    UInt            sOpenCursor = 0;
    UInt            sOpenUpdateCursor = 0;
    UInt            sFlag;

    IDE_ASSERT( mOpenCursorCnt == 0 );
    IDU_FIT_POINT_RAISE( "smiStatement::end::ERR_CANT_END_STATEMENT_TOO_MANY", ERR_CANT_END_STATEMENT_TOO_MANY ); 
    IDE_TEST_RAISE( mChildStmtCnt != 0, ERR_CANT_END_STATEMENT_TOO_MANY);
    IDE_TEST_RAISE( mUpdateCursors.mNext != &mUpdateCursors ||
                    mCursors.mNext       != &mCursors,
                    ERR_CANT_END_STATEMENT_NOT_CLOSED_CURSOR );

#ifdef DEBUG
    if ( SMI_DIST_LEVEL_IS_VALID( ((smxTrans *)(mTrans->mTrans))->mDistTxInfo.mDistLevel ) )
    {
        ideLog::log( IDE_SD_19, 
                     "\n<smiStatement(%"ID_XINT64_FMT")::end> smxTrans(%"ID_XINT64_FMT"-%"ID_UINT32_FMT") \n",
                     this, ((smxTrans *)(mTrans->mTrans)),
                     ((smxTrans *)(mTrans->mTrans))->mTransID );
    }
    else
    {
        ideLog::log( IDE_SD_19, 
                     "\n<smiStatement(%"ID_XINT64_FMT")::end> smxTrans(%"ID_XINT64_FMT"-%"ID_UINT32_FMT") NO SHARD \n",
                     this, ((smxTrans *)(mTrans->mTrans)),
                     ((smxTrans *)(mTrans->mTrans))->mTransID );
    }
#endif

    // PROJ-2199 SELECT func() FOR UPDATE ����
    // SMI_STATEMENT_FORUPDATE �߰�
    if( ( ((mFlag & SMI_STATEMENT_MASK ) == SMI_STATEMENT_NORMAL) ||
          ((mFlag & SMI_STATEMENT_MASK ) == SMI_STATEMENT_FORUPDATE)) &&
        ( (mFlag & SMI_STATEMENT_SELF_MASK) == SMI_STATEMENT_SELF_FALSE ) )
    {
        IDE_DASSERT( mISavepoint != NULL );

        if ( mTrans->mImpSVP4Shard != NULL )
        {
            /* BUG-46786

               ex) depth 1�� statment  ���� ���� �����ϰ� ����.
               smiStatement S1 (depth : 1) begin
               smiStatement S2 (depth : 1) begin
               smiStatement S2 end
               smiStatement S1 end

               statement S2 end�� implicit savepoint�� ������ ����,
               statement S1 end�� �̰����� ���Եȴ�.
               ���⼭ statement S2 end�� ������ implicit savepoint�� �����Ѵ�.
            */
            IDE_TEST( ((smxTrans *)mTrans->mTrans)->unsetImpSavepoint( mTrans->mImpSVP4Shard )
                      != IDE_SUCCESS );

            mTrans->mImpSVP4Shard = NULL;
        }

        if( (aFlag & SMI_STATEMENT_RESULT_MASK) == SMI_STATEMENT_RESULT_FAILURE )
        {
            IDE_TEST( ((smxTrans*)mTrans->mTrans)->abortToImpSavepoint( mISavepoint )
                      != IDE_SUCCESS );

            IDE_TEST( ((smxTrans *)mTrans->mTrans)->unsetImpSavepoint( mISavepoint )
                      != IDE_SUCCESS );
        }
        else /* SMI_STATEMENT_RESULT_SUCCESS */
        {
            /* BUG-46786
             *
             * depth 1�� statement�� implicit savepoint�� �����Ѵ�.
             *  (������ ���� statement begin�� �Ǵ� smiTrans ����(commit/rollback)�� ����ȴ�.)
             * depth 2,3,4,... statement�� ������ �����ϴ�. */
            if ( mDepth == 1)
            {
                mTrans->mImpSVP4Shard = mISavepoint;

                mISavepoint = NULL;
            }
            else
            {
                IDE_TEST( ((smxTrans *)mTrans->mTrans)->unsetImpSavepoint( mISavepoint )
                        != IDE_SUCCESS );
            }

            /* BUG-15906: Non-Autocommit��忡�� Select�Ϸ��� IS_LOCK��
             * �����Ǹ� ���ڽ��ϴ�. : Statement�� ������ ISLock���� ����
             * �մϴ�. */
            /*
             * BUG-21743
             * Select ���꿡�� User Temp TBS ���� TBS�� Lock�� ��Ǯ���� ����
             */
            IDE_TEST( ((smxTrans *)mTrans->mTrans)->unlockSeveralLock( mLockSlotSequence )
                    != IDE_SUCCESS );
        }
    }
    else
    {
        /* BUG-15906: Non-Autocommit��忡�� Select�Ϸ��� IS_LOCK��
         * �����Ǹ� ���ڽ��ϴ�. : Statement�� ������ ISLock���� ����
         * �մϴ�. */
        /*
         * BUG-21743
         * Select ���꿡�� User Temp TBS ���� TBS�� Lock�� ��Ǯ���� ����
         */
        IDE_TEST( ((smxTrans *)mTrans->mTrans)->unlockSeveralLock( mLockSlotSequence )
                  != IDE_SUCCESS );
    }

    sFlag = (mFlag & SMI_STATEMENT_CURSOR_MASK);

    IDE_ASSERT( (sFlag == SMI_STATEMENT_ALL_CURSOR)    ||
                (sFlag == SMI_STATEMENT_MEMORY_CURSOR) ||
                (sFlag == SMI_STATEMENT_DISK_CURSOR) );

    mParent->mChildStmtCnt--;
    IDE_DASSERT(mParent->mChildStmtCnt != ID_UINT_MAX);
    mPrev->mNext = mNext;
    mNext->mPrev = mPrev;

    /* PROJ-1381 Fetch Across Commits */
    mAllPrev->mAllNext = mAllNext;
    mAllNext->mAllPrev = mAllPrev;

    if( mParent->mUpdate == this )
    {
        mParent->mUpdate = NULL;
    }

    if( ( (mParent->mFlag & SMI_STATEMENT_LEGACY_MASK)
           == SMI_STATEMENT_LEGACY_TRUE ) &&
        ( mParent->mChildStmtCnt == 0 ) )
    {
        /* PROJ-1381 Fetch Across Commits
         * Stmt�� Legacy�� ���ϰ�, Legacy Trans�� ���� ��� stmt��
         * �Ϸ������� Legacy Trans�� �����Ѵ�. */
        IDE_TEST( smiLegacyTrans::removeLegacyTrans( mParent,
                                                     (void*)mTrans->mTrans )
                  != IDE_SUCCESS );
    }

    /* BUG-41814 :
     * Legacy Transaction commit ���� statement �� ������,
     * Legacy Transaction�� OIDList�� Ager�� ������ ���� ����
     * MinViewSCN ������ ���� �ʴ´�. */
    /* BUG-47387 :
     * 1. Legacy Transaction�� �ƴϰų� ��� �����ߴٸ�
     *    ViewSCN�� ���� �� �ش�.
     * 2. MinViewSCN�� Ȯ�� �� �� ���� �����ϰ� Ȯ���Ѵ�.
     *    �� ���� List���� ������ �Ŀ� Ȯ���ص� �ȴ�. */
    if ( ( (smxTrans *)mTrans->mTrans )->mLegacyTransCnt == 0 )
    {
        gSmiTryUptTransViewSCNFuncs[sFlag](this);
    }
    else
    {
        /* do nothing */
    }
    
    mParent = NULL;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_CANT_END_STATEMENT_TOO_MANY )
    {
        IDE_SET( ideSetErrorCode( smERR_FATAL_smiCantEndStatement_too_many,
                                  mChildStmtCnt) );
    }
    IDE_EXCEPTION( ERR_CANT_END_STATEMENT_NOT_CLOSED_CURSOR )
    {
        for ( sTempCursor = mCursors.mNext ;
              sTempCursor != &mCursors ;
              sTempCursor = sTempCursor->mNext )
        {
            sOpenCursor++;
        }
        for ( sTempCursor = mUpdateCursors.mNext ;
              sTempCursor != &mUpdateCursors ;
              sTempCursor = sTempCursor->mNext )
        {
            sOpenUpdateCursor++;
        }
        IDE_SET( ideSetErrorCode( smERR_FATAL_smiCantEndStatement_not_closed,
                                  sOpenCursor,
                                  sOpenUpdateCursor) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
/***********************************************************************
 * Description : ���� table cursor�� ������ ������ close ��Ų �� Statement�� ���� ��Ų��.
 **********************************************************************/
IDE_RC smiStatement::endForce()
{
    smiTableCursor *sTempCursor = NULL;
    smiTableCursor *sNextCursor = NULL;
    
    for ( sTempCursor = mCursors.mNext ;
          sTempCursor != &mCursors ;
          sTempCursor = sNextCursor )
    {
        sNextCursor = sTempCursor->mNext;
        sTempCursor->close();
    }
 
    for ( sTempCursor = mUpdateCursors.mNext ;
          sTempCursor != &mUpdateCursors ;
          sTempCursor = sNextCursor )
    {
        sNextCursor = sTempCursor->mNext;
        sTempCursor->close();
    }
   
    IDE_TEST( end( SMI_STATEMENT_RESULT_FAILURE ) != IDE_SUCCESS );
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
} 

/***************************************************************************
 *
 * Description: Ʈ������� ������ Ŀ��Ÿ�Կ� �ش��ϴ� Stmt �߿��� ���� ����
 *              ViewSCN�� ���Ѵ�.
 *
 * TransViewSCN�� Ʈ������� DiskViewSCN�Ǵ� MemViewSCN�� �� ������,
 * End�ϴ� Stmt�� ������ ������ Stmt�߿� ���� ���� ������ �����Ѵ�.
 * ���� �ٽ� ���� ���� ���� ���� ���� Ʈ������� MinViewSCN�� ������ ������
 * �ʿ䰡 ����. End �Ҷ� �ٸ� Stmt�� ������, Ʈ������� ViewSCN��
 * INFINITESCN���� �ʱ�ȭ�Ѵ�.
 *
 * aStatement    - [IN]  End�ϴ� Stmt�� ������
 * aCursorFlag   - [IN]  Stmt�� Cursor Flag ��
 * aTransViewSCN - [IN]  Ʈ������� MemViewSCN Ȥ�� DskViewSCN�̴� (�����������λ��)
 * aMinViewSCN   - [OUT] Ŀ��Ÿ�Ժ�
 *
 ****************************************************************************/
void smiStatement::getMinViewSCN( smiStatement * aStmt,
                                  UInt           aCursorFlag,
                                  smSCN        * aTransViewSCN,
                                  smSCN        * aMinViewSCN )
{
    smSCN          sMinViewSCN;
    smiStatement * sStmt;

    IDE_ASSERT( (aCursorFlag == SMI_STATEMENT_MEMORY_CURSOR) ||
                (aCursorFlag == SMI_STATEMENT_DISK_CURSOR) );

    SM_SET_SCN_INFINITE( &sMinViewSCN );

    for( sStmt  = aStmt->mTrans->mStmtListHead->mAllNext;
         sStmt != aStmt->mTrans->mStmtListHead;
         sStmt  = sStmt->mAllNext )
    {
        if( (sStmt != aStmt) && (sStmt->mFlag & aCursorFlag) )
        {
            if( SM_SCN_IS_LT(&sStmt->mSCN, &sMinViewSCN) )
            {
                SM_SET_SCN(&sMinViewSCN, &sStmt->mSCN);
            }

            IDE_ASSERT( SM_SCN_IS_NOT_INFINITE(sStmt->mSCN) );

            if ( SM_SCN_IS_EQ(&sStmt->mSCN, aTransViewSCN) )
            {
                break;
            }
        }
    }

    SM_SET_SCN( aMinViewSCN, &sMinViewSCN );
}

/***************************************************************************
 *
 * Description: Ʈ������� ��� Stmt�� Ȯ���Ͽ� MinMemViewSCN�� MinDskViewSCN��
 *              ���Ѵ�.
 *
 * �ڽ��� Stmt�� End�ϴ� ���̱� ������ ���ܽ�Ų��.
 *
 * aStatement     - [IN]  End�ϴ� Stmt�� ������
 * aMinDskViewSCN - [OUT] ���� Ʈ������� Stmt�� ���� ���� DskViewSCN
 * aMinMemViewSCN - [OUT] ���� Ʈ������� Stmt�� ���� ���� MemViewSCN
 *
 ***************************************************************************/
void smiStatement::getMinViewSCNOfAll( smiStatement * aStmt,
                                       smSCN        * aMinMemViewSCN,
                                       smSCN        * aMinDskViewSCN )
{
    smiStatement * sStmt;
    smSCN          sMinMemViewSCN;
    smSCN          sMinDskViewSCN;

    SM_SET_SCN_INFINITE( &sMinMemViewSCN );
    SM_SET_SCN_INFINITE( &sMinDskViewSCN );

    for( sStmt  = aStmt->mTrans->mStmtListHead->mAllNext;
         sStmt != aStmt->mTrans->mStmtListHead;
         sStmt  = sStmt->mAllNext )
    {
        if( sStmt == aStmt)
        {
            /* end�ϴ� statement�� ���� */
            continue;
        }

        switch( sStmt->mFlag & SMI_STATEMENT_CURSOR_MASK )
        {
            case SMI_STATEMENT_ALL_CURSOR :

                if( SM_SCN_IS_LT( &sStmt->mSCN, &sMinMemViewSCN ))
                {
                    SM_SET_SCN( &sMinMemViewSCN ,&sStmt->mSCN );
                }

                if( SM_SCN_IS_LT( &sStmt->mSCN, &sMinDskViewSCN ))
                {
                    SM_SET_SCN( &sMinDskViewSCN, &sStmt->mSCN );
                }
                break;

            case SMI_STATEMENT_MEMORY_CURSOR :
                if( SM_SCN_IS_LT( &sStmt->mSCN, &sMinMemViewSCN ))
                {
                    SM_SET_SCN( &sMinMemViewSCN ,&sStmt->mSCN );
                }
                break;

            case SMI_STATEMENT_DISK_CURSOR :
                if( SM_SCN_IS_LT( &sStmt->mSCN, &sMinDskViewSCN ))
                {
                    SM_SET_SCN( &sMinDskViewSCN, &sStmt->mSCN );
                }
                break;
            default:
                break;
        }
    }

    SM_SET_SCN( aMinDskViewSCN, &sMinDskViewSCN );
    SM_SET_SCN( aMinMemViewSCN, &sMinMemViewSCN );
}

/*********************************************************************
 *
 * Description: Statement End�ÿ� Transaction�� MemViewSCN ���Žõ��Ѵ�.
 *
 * aStmt - [IN] End�ϴ� Stmt�� ������
 *
 **********************************************************************/
void smiStatement::tryUptTransMemViewSCN( smiStatement* aStmt )
{
    smxTrans * sTrans;
    smSCN      sMemViewSCN;

    sTrans      = (smxTrans*)(aStmt->mTrans->getTrans());
    sMemViewSCN = sTrans->getMinMemViewSCN();

    tryUptMinViewSCN( aStmt,
                      SMI_STATEMENT_MEMORY_CURSOR,
                      &sMemViewSCN );
}

/*********************************************************************
 *
 * Description: Statement End�ÿ� Transaction�� Disk ViewSCN
 *              ���Žõ��Ѵ�.
 *
 * aStmt - [IN] End�ϴ� Stmt�� ������
 *
 **********************************************************************/
void smiStatement::tryUptTransDskViewSCN( smiStatement* aStmt )
{
    smxTrans * sTrans;
    smSCN      sDskViewSCN;

    sTrans      = (smxTrans*)(aStmt->mTrans->getTrans());
    sDskViewSCN = sTrans->getMinDskViewSCN();

    tryUptMinViewSCN( aStmt,
                      SMI_STATEMENT_DISK_CURSOR,
                      &sDskViewSCN );
}

/***************************************************************************
 * Description: End Statement�� Memory�� Disk�� ��� ��ġ�� ��쿡
 *              Transaction�� ViewSCN�� ���Žõ��Ѵ�.
 *
 * �Ʒ��� ���� 3���� ��찡 ���� �� �ִ�.
 *
 * A. End Statement�� viewSCN�� transaction�� memory,disk
 *    View scn�� ��� ���� ���
 *    : ��� �����Ѵ�.
 *
 * B. End Statement�� ViewSCN�� transaction�� memory viewSCN
 *    �� ���� ���.
 *    : Memory ViewSCN�� �����Ѵ�.
 *
 * C. End�ϴ� statement�� viewSCN�� transaction�� disk viewSCN
 *    �� ���� ���.
 *    : Disk ViewSCN�� �����Ѵ�.
 *
 * aStmt - [IN] End�ϴ� Stmt�� ������
 *
 ***************************************************************************/
void smiStatement::tryUptTransAllViewSCN( smiStatement* aStmt )
{
    smxTrans * sTrans;
    smSCN      sMemViewSCN;
    smSCN      sDskViewSCN;

    sTrans = (smxTrans*)(aStmt->mTrans->getTrans());

    sMemViewSCN = sTrans->getMinMemViewSCN();
    sDskViewSCN = sTrans->getMinDskViewSCN();

    if ( SM_SCN_IS_EQ(&aStmt->mSCN, &sDskViewSCN) &&
         SM_SCN_IS_EQ(&aStmt->mSCN, &sMemViewSCN) )
    {
        tryUptMinViewSCNofAll( aStmt );
    }
    else
    {
        if ( SM_SCN_IS_EQ(&aStmt->mSCN, &sMemViewSCN)  )
        {
            tryUptMinViewSCN( aStmt,
                              SMI_STATEMENT_MEMORY_CURSOR,
                              &sMemViewSCN );
        }
        else
        {
            tryUptMinViewSCN( aStmt,
                              SMI_STATEMENT_DISK_CURSOR,
                              &sDskViewSCN );
        }
    }
}

/***************************************************************************
 *
 * Description: End�ϴ� Stmt�� ViewSCN�� Ʈ������� MinViewSCN�� �ٸ� ��쿡
 *              �����Ѵ�.
 *
 * TransViewSCN�� Ʈ������� DiskViewSCN Ȥ�� MemViewSCN �̴�.
 * End�ϴ� Stmt�� ������ ������ Stmt�߿� ���� ���� ������ Ʈ������� MinViewSCN��
 * �����Ѵ�.
 *
 * ���� �ٽ� ���� ���� ���� ���� ���� Ʈ������� MinViewSCN�� ������ ������ �ʿ䰡 ����.
 * End �Ҷ� �ٸ� Stmt�� ������, Ʈ������� ViewSCN�� Infinite SCN���� �ʱ�ȭ�Ѵ�.
 *
 * aStatement    - [IN] End�ϴ� Stmt�� ������
 * aCursorFlag   - [IN] Stmt�� Cursor Flag ��
 * aTransViewSCN - [IN] Ʈ������� MemViewSCN Ȥ�� DskViewSCN (������������ ���)
 *
 ****************************************************************************/
void smiStatement::tryUptMinViewSCN( smiStatement * aStmt,
                                     UInt           aCursorFlag,
                                     smSCN        * aTransViewSCN )
{
    smxTrans * sTrans;
    smSCN      sMinViewSCN;

    getMinViewSCN( aStmt, aCursorFlag, aTransViewSCN, &sMinViewSCN );

    if( !SM_SCN_IS_EQ(&aStmt->mSCN, &sMinViewSCN) )
    {
        sTrans = (smxTrans*)(aStmt->mTrans->getTrans());

        if ( aCursorFlag == SMI_STATEMENT_MEMORY_CURSOR )
        {
            sTrans->setMinViewSCN( aCursorFlag,
                                   &sMinViewSCN,
                                   NULL /* aDskViewSCN */ );
        }
        else
        {
            IDE_ASSERT( aCursorFlag == SMI_STATEMENT_DISK_CURSOR );

            sTrans->setMinViewSCN( aCursorFlag,
                                   NULL /* aMemViewSCN */,
                                   &sMinViewSCN );
        }
    }
}


/***************************************************************************
 *
 * Description: End�ϴ� Stmt�� ViewSCN�� Ʈ������� MinViewSCN�� �ٸ� ��쿡
 *              �����Ѵ�.
 *
 * TransViewSCN�� Ʈ������� DiskViewSCN Ȥ�� MemViewSCN �̴�.
 * End�ϴ� Stmt�� ������ ������ Stmt�߿� ���� ���� ������ Ʈ������� MinViewSCN��
 * �����Ѵ�.
 *
 * ���� �ٽ� ���� ���� ���� ���� ���� Ʈ������� MinViewSCN�� ������ ������ �ʿ䰡 ����.
 * End �Ҷ� �ٸ� Stmt�� ������, Ʈ������� ViewSCN�� Infinite SCN���� �ʱ�ȭ�Ѵ�.
 *
 * aStmt - [IN]  End�ϴ� Stmt�� ������
 *
 ****************************************************************************/
void smiStatement::tryUptMinViewSCNofAll( smiStatement  * aStmt )
{
    smxTrans * sTrans;
    smSCN      sMinMemViewSCN;
    smSCN      sMinDskViewSCN;

    sTrans = (smxTrans*)(aStmt->mTrans->getTrans());

    getMinViewSCNOfAll( aStmt, &sMinMemViewSCN, &sMinDskViewSCN );

    if( !SM_SCN_IS_EQ( &aStmt->mSCN, &sMinMemViewSCN ) &&
        !SM_SCN_IS_EQ( &aStmt->mSCN, &sMinDskViewSCN ) )
    {
        sTrans->setMinViewSCN( SMI_STATEMENT_ALL_CURSOR,
                               &sMinMemViewSCN,
                               &sMinDskViewSCN );
    }
    else
    {
        if(!SM_SCN_IS_EQ(&(aStmt->mSCN), &sMinMemViewSCN ) )
        {
            sTrans->setMinViewSCN( SMI_STATEMENT_MEMORY_CURSOR,
                                   &sMinMemViewSCN,
                                   NULL /* aDskViewSCN */ );
        }
        else
        {
            if( !SM_SCN_IS_EQ(&(aStmt->mSCN), &sMinDskViewSCN ))
            {
                sTrans->setMinViewSCN( SMI_STATEMENT_DISK_CURSOR,
                                       NULL, /* aMemViewSCN */
                                       &sMinDskViewSCN );
            }
        }
    }
}

/***************************************************************************
 *
 * Description: aStmt�� ���� Transaction�� ��� Statement�� ViewSCN��
 *              Infinite�� �ʱ�ȭ�Ѵ�.
 *
 * aStmt - [IN] Statement ��ü
 *
 ****************************************************************************/
IDE_RC smiStatement::initViewSCNOfAllStmt( smiStatement* aStmt )
{
    smiStatement * sStatement;
    smxTrans     * sTrans;
    smTID          sTransID;

    sTrans   = (smxTrans*)((aStmt->mTrans)->getTrans());
    sTransID = smxTrans::getTransID( sTrans );

    for( sStatement  = aStmt->mTrans->mStmtListHead->mNext;
         sStatement != aStmt->mTrans->mStmtListHead;
         sStatement  = sStatement->mNext )
    {
        /* BUG-31993 [sm_interface] The server does not reset Iterator ViewSCN 
         * after building index for Temp Table
         * Cursor�� �̹� ������� Statement�� �����ؼ��� �ȵȴ�. */
        IDE_TEST_RAISE( sStatement->mOpenCursorCnt > 0, error_internal );

        SM_SET_SCN_INFINITE_AND_TID( &sStatement->mSCN, sTransID );
    }

    sTrans->initMinViewSCN( SMI_STATEMENT_ALL_CURSOR );

    return IDE_SUCCESS;

    IDE_EXCEPTION( error_internal );
    {
        ideLog::logCallStack( IDE_SM_0 );

        ideLog::log( IDE_SM_0,
                     "Statement->mOpenCursorCnt   : %u",
                     sStatement->mOpenCursorCnt );
        IDE_SET( ideSetErrorCode(smERR_ABORT_INTERNAL_ARG, "Statement") );
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/***************************************************************************
 *
 * Description: aStmt�� ���� Transaction�� ��� Statement�� ViewSCN��
 *              ���ο� SCN���� �����Ѵ�.
 *
 * aStmt - [IN] Statement ��ü
 *
 ****************************************************************************/
IDE_RC smiStatement::setViewSCNOfAllStmt( smiStatement* aStmt )
{
    smSCN         sSCN;
    smiStatement* sStmt;
    smiStatement* sStmtListHead;
    smSCN         sDummySCN = SM_SCN_INIT;

    IDU_FIT_POINT( "1.BUG-31993@smiStatement::setViewSCNOfAllStmt" );

    sStmtListHead = aStmt->mTrans->mStmtListHead;

    for( sStmt  = sStmtListHead->mNext;
         sStmt != sStmtListHead;
         sStmt  = sStmt->mNext )
    {
        /* BUG-31993 [sm_interface] The server does not reset Iterator
         * ViewSCN after building index for Temp Table
         * Cursor�� �̹� ������� Statement�� �����ؼ��� �ȵȴ�. */
        IDE_TEST_RAISE( sStmt->mOpenCursorCnt > 0, error_internal );
        /* PROJ-2733-BUGBUG: view Ȯ�� �ʿ���.*/
        IDE_ERROR_RAISE( SMI_STATEMENT_VIEWSCN_IS_REQUESTED( sStmt->mFlag ) == ID_FALSE,
                         error_internal );
        ((smxTrans*)sStmt->mTrans->getTrans())->allocViewSCN( 
                                                sStmt->mFlag,
                                                &sSCN,
                                                sDummySCN );

        SM_SET_SCN( &sStmt->mSCN, &sSCN );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( error_internal );
    {
        ideLog::logCallStack( IDE_SM_0 );

        ideLog::log( IDE_SM_0,
                     "Statement->mOpenCursorCnt : %u, Statement->mFlag : %u ",
                     sStmt->mOpenCursorCnt,
                     sStmt->mFlag );
        IDE_SET( ideSetErrorCode(smERR_ABORT_INTERNAL_ARG, "Statement") );
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}


/***************************************************************************
 *
 * Description :
 *
 *   prepare execution Ÿ���� statement�� statement�� �����ϱ� ���� �̹�
 *   QP���� PVO������ ��� ������ �����ϴ� ���̺����� ������ ��� �˱�
 *   ������, �޸� ���̺��� �����ϴ��� ��ũ ���̺��� �����ϴ��� �̸�
 *   �˰� begin�ÿ� ������ �� �ִ�.
 *   ������, direct execution Ÿ���� statement������ statement�� begin��
 *   ���Ŀ� PVO������ �����ϰ� execution�ϱ� ������, begin������ �ش�
 *   statement�� �����ϴ� ���̺��� ������ �� �� ����.
 *   �̸� ����, direct execution�� statement�� ��쿡 begin ����������
 *   �޸𸮿� ��ũ ���̺��� ��� �����Ѵٰ� �ϴ� ������ �� ���Ŀ�,
 *   execution ������(PVO ����) statement�� ������ �ٽ� �����ϵ��� �Ѵ�.
 *
 *   statement�� ������ �����ϴ� ������ ���� �ΰ����̴�.
 *
 *   -  ��ũ ���̺� �����ϴ� statement
 *      ������ memory GC(Ager)�� ������ ���� �ʵ��� �ϱ� ���ؼ��̴�.
 *
 *   -  �޸� ���̺� �� �����ϴ� statement
 *      ������  disk  GC��  ������ ���� �ʵ��� �ϱ� ���ؼ� �̴�.
 *
 ****************************************************************************/
IDE_RC smiStatement::resetCursorFlag( UInt aFlag )
{
    UInt sNewStmtCursorFlag;
    UInt sNoAccessMedia;

    sNewStmtCursorFlag = aFlag &  SMI_STATEMENT_CURSOR_MASK;

    IDE_ASSERT( (sNewStmtCursorFlag == SMI_STATEMENT_ALL_CURSOR)    ||
                (sNewStmtCursorFlag == SMI_STATEMENT_MEMORY_CURSOR) ||
                (sNewStmtCursorFlag == SMI_STATEMENT_DISK_CURSOR) );

    // direct execution�� statement�� ��쿡 begin ����������
    // �޸𸮿� ��ũ ���̺��� ��� �����Ѵٰ� ������ �����Ͽ���.
    sNoAccessMedia = SMI_STATEMENT_ALL_CURSOR - sNewStmtCursorFlag;

    switch(sNoAccessMedia)
    {
        case SMI_STATEMENT_DISK_CURSOR:
            //disk�� �������� �ʴ� cursor.
            mFlag &= ~(sNoAccessMedia);
            // transaction�� disk view scn ����.
            // ->Ȥ�� �� statement������ ���� ����
            // ������ �ִٸ� ����,�Ǵ� infinite�� ������.
            tryUptTransDskViewSCN(this);
            break;

        case SMI_STATEMENT_MEMORY_CURSOR:
            // memory �� ���� �ʴ� cursor.
            // statement�� cursor flag ����.
            mFlag &= ~(sNoAccessMedia);
            // transaction�� memory view scn ����.
            // ->Ȥ�� �� statement������ ���� ����
            // ������ �ִٸ� ����,�Ǵ� infinite�� ������.
            tryUptTransMemViewSCN(this);
            break;
    }
    return IDE_SUCCESS;
}

/***********************************************************************
 * Description : DDL�� �����ϴ� Statement�� ������ ���� ������ �����ؾ� �Ѵ�.
 *
 *                1. Root Statement
 *                2. Update statement
 *                3. Group�� ������ Statement
 *                4. Update Cursor�� open�Ȱ��� ����� �Ѵ�.
 *                5. Cursor���� open�Ȱ��� ����� �Ѵ�.
 *
 **********************************************************************/
IDE_RC smiStatement::prepareDDL( smiTrans *aTrans )
{
    smxTrans *sTrans;

    sTrans = (smxTrans*)aTrans->mTrans;

    sTrans->mIsDDL = ID_TRUE;

    /* PROJ-2162 RestartRiskReductino
     * DB�� Inconsistency �ϱ� ������ DDL�� ���´�. */
    IDE_TEST_RAISE( (smrRecoveryMgr::getConsistency() == ID_FALSE ) &&
                    ( smuProperty::getCrashTolerance() == 0 ),
                    ERROR_INCONSISTENT_DB );


    IDE_TEST_RAISE( (mParent->mParent       != NULL)            ||
                    (mParent->mUpdate       != this)            ||
                    (mParent->mChildStmtCnt != 1)               ||
                    (mChildStmtCnt          != 0)               ||
                    (mUpdateCursors.mNext   != &mUpdateCursors) ||
                    (mCursors.mNext         != &mCursors),
                    ERR_CANT_EXECUTE_DDL );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERROR_INCONSISTENT_DB );
    {
        IDE_SET( ideSetErrorCode(smERR_ABORT_INCONSISTENT_DB) );
    }
    IDE_EXCEPTION( ERR_CANT_EXECUTE_DDL );
    {
        IDE_SET( ideSetErrorCode( smERR_FATAL_smiCantExecuteDDL ) );
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : aCursor�� Statement�� �߰��Ѵ�.
 *
 * aCursor     - [IN]  statement�� �߰��� cursor
 * aIsSoloOpenUpdateCursor - [OUT] statement�� �����ϰ� open�� update cursor
 *                                 ��� ID_TRUE, else,ID_FALSE.
 *
 **********************************************************************/
IDE_RC smiStatement::openCursor( smiTableCursor * aCursor,
                                 idBool *         aIsSoloOpenUpdateCursor)
{
    smiStatement*   sStatement;
    smiTableCursor* sCursor;
    smiTableCursor* sTransCursor;

#ifdef DEBUG
    smcTableHeader* sTableHeader = (smcTableHeader*)aCursor->mTable;
    UInt            sTableType = SMI_GET_TABLE_TYPE( sTableHeader );

    // statement memory ,disk cursor flag�� table ����
    // validation.
    if(sTableType == SMI_TABLE_DISK)
    {
        // statement cursor flag�� disk�� �־�� �Ѵ�.
        IDE_DASSERT((SMI_STATEMENT_DISK_CURSOR & mFlag) != 0);
    }
    else
    {
        if( (sTableType == SMI_TABLE_MEMORY) ||
            (sTableType == SMI_TABLE_META)   ||
            (sTableType == SMI_TABLE_FIXED) )
        {
            // statement cursor flag�� memory�� �־�� �Ѵ�.
            IDE_DASSERT((SMI_STATEMENT_MEMORY_CURSOR & mFlag) != 0);
        }
    }
#endif

    sTransCursor = &(mTrans->mCursorListHead);

    *aIsSoloOpenUpdateCursor = ID_FALSE;

    // BUG-24134
    //IDE_TEST_RAISE( mChildStmtCnt != 0, ERR_CHILD_STATEMENT_EXIST );

    if ( aCursor->mUntouchable == ID_FALSE )
    {
        // PROJ-2199 SELECT func() FOR UPDATE ����
        // SMI_STATEMENT_FORUPDATE �߰�
        IDE_TEST_RAISE( (( mFlag & SMI_STATEMENT_MASK ) != SMI_STATEMENT_NORMAL) &&
                        (( mFlag & SMI_STATEMENT_MASK ) != SMI_STATEMENT_FORUPDATE),
                        ERR_CANT_OPEN_UPDATE_CURSOR );

        aCursor->mSCN        = mSCN;
        aCursor->mInfinite   = mInfiniteSCN;
        // PROJ-1362, select c1,b1 from t1 for update ����
        // �����ϴ� lob cursor�� ���ÿ� �ΰ� �̻󿭸��� ��츦 ���Ͽ�
        // ������ �߰�.
        aCursor->mInfinite4DiskLob = mInfiniteSCN;

        if( aCursor->mCursorType == SMI_LOCKROW_CURSOR )
        {
            // BUG-17940 referential integrity check�� ���� Ŀ��
            // lock�� ����
            *aIsSoloOpenUpdateCursor = ID_FALSE;

            // Ȥ�ó� parallel�� DML ���Ǹ� �����ϰ� �Ǵ� ��쿡 ����ؼ�
            // update cursor list�� ���� �ʴ´�.
            aCursor->mPrev        = &mCursors;
            aCursor->mNext        = mCursors.mNext;
        }
        else
        {
            *aIsSoloOpenUpdateCursor = ID_TRUE;

            for( sCursor  = sTransCursor->mAllNext;
                 sCursor != sTransCursor;
                 sCursor  = sCursor->mAllNext )
            {
                if(sCursor->mTable == aCursor->mTable)
                {
                    /* �� Transaction�� ������ Table�� �ϳ��� Update Cursor����
                       ����� �Ѵ�. �ֳ��ϸ� �ΰ��� Cursor�� ������ Data�� ���ÿ�
                       ������ �� �ֱ� �����̴�. */
                    IDE_TEST_RAISE(sCursor->mUntouchable == ID_FALSE,
                                   ERR_UPDATE_SAME_TABLE);

                    /* Update Cursor������ ReadOnly Cursor�� �̹� open�Ǿ� �ִ�.
                       ������ �� Cursor�� Update Only Cursor�� �ƴϴ�.*/
                    *aIsSoloOpenUpdateCursor = ID_FALSE;
                }
            }

            aCursor->mPrev = &mUpdateCursors;
            aCursor->mNext = mUpdateCursors.mNext;
        }
        aCursor->mPrev->mNext = aCursor;
        aCursor->mNext->mPrev = aCursor;
    }
    else
    {
        aCursor->mSCN              = mSCN;
        aCursor->mInfinite         = mInfiniteSCN;
        aCursor->mInfinite4DiskLob = mInfiniteSCN;

        if( aCursor->mCursorType != SMI_LOCKROW_CURSOR )
        {
            for( sStatement  = mTrans->mStmtListHead->mUpdate;
                 sStatement != NULL;
                 sStatement  = sStatement->mUpdate )
            {
                for( sCursor  = sStatement->mUpdateCursors.mNext;
                     sCursor != &sStatement->mUpdateCursors;
                     sCursor  = sCursor->mNext )
                {
                    if( sCursor->mTable == aCursor->mTable )
                    {
                        IDE_TEST_RAISE(sStatement != aCursor->mStatement &&
                                       (mRoot->mFlag & SMI_STATEMENT_MASK )
                                       == SMI_STATEMENT_NORMAL,
                                       ERR_CANT_OPEN_CURSOR);

                        aCursor->mInfinite         = sCursor->mInfinite;
                        aCursor->mInfinite4DiskLob = sCursor->mInfinite4DiskLob;

                        /* BUG-33800
                         * 1) update cursor, 2) select cursor ������
                         * Ŀ���� ������, update cursor�� select cursor ���縦
                         * �𸣰� mIsSoloCursor = ID_TRUE�� ������ �ִ�.
                         * update cursor�� mIsSoloCursor�� �����Ѵ�. */
                        sCursor->mIsSoloCursor = ID_FALSE;

                        goto out_for; /* break */
                    }
                }
            }
        }

out_for: /* break */

        aCursor->mPrev         = &mCursors;
        aCursor->mNext         = mCursors.mNext;
        aCursor->mPrev->mNext  = aCursor;
        aCursor->mNext->mPrev  = aCursor;
    }

    aCursor->mAllPrev           = sTransCursor;
    aCursor->mAllNext           = sTransCursor->mAllNext;
    aCursor->mAllPrev->mAllNext = aCursor;
    aCursor->mAllNext->mAllPrev = aCursor;

    mOpenCursorCnt++;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_CANT_OPEN_CURSOR );
    IDE_SET( ideSetErrorCode( smERR_ABORT_smiInvalidCursorOpen ) );

    //IDE_EXCEPTION( ERR_CHILD_STATEMENT_EXIST );
    //IDE_SET( ideSetErrorCode( smERR_FATAL_smiChildStatementExist ) );

    IDE_EXCEPTION( ERR_CANT_OPEN_UPDATE_CURSOR );
    IDE_SET( ideSetErrorCode( smERR_FATAL_smiCantOpenUpdateCursor ) );

    IDE_EXCEPTION( ERR_UPDATE_SAME_TABLE );
    IDE_SET( ideSetErrorCode( smERR_ABORT_smiUpdateSameTable ) );

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : aCursor�� Statement���� �����Ѵ�.
 *
 * aCursor     - [IN]  statement���� ������ Cursor
 **********************************************************************/
void smiStatement::closeCursor( smiTableCursor* aCursor )
{
    /* BUG-32963
     * openCursor()���� infiniteSCN�� �ø��� ����.
     * ReadOnly�� �ƴ� Ŀ���������� closeCursor()���� infiniteSCN�� �÷� �� */
    IDE_ASSERT(mOpenCursorCnt > 0);

    if ( ( aCursor->mUntouchable == ID_FALSE ) && // not ReadOnly, Write!!!
         ( mIncInfiniteSCN4ClosingCursor == ID_TRUE ) )
    {
        ((smxTrans*)mTrans->getTrans())->incInfiniteSCNAndGet(&mInfiniteSCN);
    }
    else
    {
        /* do nothing */
    }
    aCursor->mPrev->mNext = aCursor->mNext;
    aCursor->mNext->mPrev = aCursor->mPrev;
    aCursor->mPrev        =
    aCursor->mNext        = aCursor;

    aCursor->mAllPrev->mAllNext = aCursor->mAllNext;
    aCursor->mAllNext->mAllPrev = aCursor->mAllPrev;
    aCursor->mAllPrev           =
    aCursor->mAllNext           = aCursor;

    mOpenCursorCnt--;
}

/***********************************************************************
 * PROJ-2626 Snapshot Export
 *
 * �� Set SCN�� Snpashot Export������ ����������
 * ���Ǵ� Code�Դϴ�.
 *
 * �ٸ� �������� ������ �ʾƾ��մϴ�.
 **********************************************************************/
void smiStatement::setSCNForSnapshot( smSCN * aSCN )
{
    SM_SET_SCN( &mSCN, aSCN );
}

/***********************************************************************
 * PROJ-2733 �л� Ʈ����� ���ռ�
 * Global Consistent Transaction �� �䱸�� SCN�� ������ stmt retry �� ���� �ϰ� ABORT
 **********************************************************************/
idBool smiStatement::isForbiddenToRetry()
{
   return ( (((smxTrans*)mTrans->getTrans())->mIsGCTx) &&
            (SM_SCN_IS_NOT_INIT(mRequestSCN)) ? ID_TRUE : ID_FALSE );
}


