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
 * $Id: smiStatement.h 90259 2021-03-19 01:22:22Z emlee $
 **********************************************************************/

#ifndef _O_SMI_STATEMENT_H_
# define _O_SMI_STATEMENT_H_ 1

# include <smiDef.h>
# include <smiTableCursor.h>

class smiStatement
{
 public:

     /* BUG-46786 */
    static IDE_RC checkImpSVP4Shard( smiTrans * aTrans );
    static IDE_RC abortToImpSVP4Shard( smiTrans * aTrans );

    // Statement�� Begin
    IDE_RC begin( idvSQL         * aStatistics,
                  smiStatement   * aParent,
                  UInt             aFlag,
                  smSCN            aRequestSCN    = SM_SCN_INIT,
                  smiDistTxInfo  * aDistTxInfo    = NULL,
                  idBool           aIsPartialStmt = ID_FALSE,
                  UInt             aStmtSeq       = 0 );

    void setInfinteSCN4PartialStmt( smxTrans * sTrans,
                                    idBool     aIsPartialStmt,
                                    UInt       aStmtSeq );

    // Statement�� end
    IDE_RC end( UInt aFlag );
    IDE_RC endForce();
    // Statement�� �Ӽ�����(Memory Only, Disk Only, Hybrid?)
    IDE_RC resetCursorFlag( UInt aFlag );

    // Statement�� ����� Transaction�� scn ���� */
    static void tryUptTransMemViewSCN( smiStatement* aStmt );
    static void tryUptTransDskViewSCN( smiStatement* aStmt );
    static void tryUptTransAllViewSCN( smiStatement* aStmt );

    inline smiTrans* getTrans( void );
    /* BUG-46756 IPCDA Simplequery execute������ smiStatement�� begin���� �ʰ� smiTrans ������ �ʿ���. */
    inline void      setTrans( smiTrans* aTrans );
    inline smSCN     getSCN();
    inline smSCN     getInfiniteSCN();
    inline idBool    isDummy();
    inline UInt      getDepth();

    static IDE_RC initViewSCNOfAllStmt( smiStatement* aStmt );
    static IDE_RC setViewSCNOfAllStmt( smiStatement* aStmt );

    /*
     * [BUG-24187] Rollback�� statement�� Internal CloseCurosr��
     * ������ �ʿ䰡 �����ϴ�.
     */
    static void setSkipCursorClose( smiStatement* aStatement );
    static idBool getSkipCursorClose( smiStatement* aStatement );

    /* PROJ-2626 SnapshotExport */
    void setSCNForSnapshot( smSCN * aSCN );

    /* PROJ-2733
     * Global Consistent Transaction �� �䱸�� SCN�� ������ stmt retry �� ���� �ϰ� ABORT �ø� */
    idBool isForbiddenToRetry();

 private:

    static void tryUptMinViewSCNofAll( smiStatement  * aStmt );

    static void tryUptMinViewSCN( smiStatement * aStmt,
                                  UInt           aCursorFlag,
                                  smSCN        * aTransViewSCN );

    static void getMinViewSCNOfAll( smiStatement * aStmt,
                                    smSCN        * aMinMemViewSCN,
                                    smSCN        * aMinDskViewSCN );

    static void getMinViewSCN( smiStatement * aStmt,
                               UInt           aCursorFlag,
                               smSCN        * aTransViewSCN,
                               smSCN        * aMinViewSCN );

    // DDL����� ȣ����.
    IDE_RC prepareDDL( smiTrans *aTrans );

    // Statement�� Cursor�߰�.
    IDE_RC openCursor( smiTableCursor* aCursor,
                       idBool*         aIsSoloOpenUpdateCursor);

    // Statement���� Cursor����.
    void   closeCursor( smiTableCursor* aCursor );

    // friend class ����
    friend class smiTable;
    friend class smiObject;
    friend class smiTrans;
    friend class smiTableCursor;

 public:  /* POD class type should make non-static data members as public */
    // Statement�� ���� Transaction
    smiTrans*      mTrans;
    // Statement�� Parent Statement
    smiStatement*  mParent;
    // Sibling Statement List
    smiStatement*  mPrev;
    smiStatement*  mNext;

    /* PROJ-1381 Fetch Across Commits
     * mPrev/mNext       : ���� TX���� stmt ���� ����Ʈ
     * mAllPrev/mAllNext : Legacy TX�� �����ϴ� stmt ���� ����Ʈ */
    smiStatement*  mAllPrev;
    smiStatement*  mAllNext;
    // Statement�� Update Statement List
    smiStatement*  mUpdate;
    // Child Statement ����
    UInt           mChildStmtCnt;
    smTID          mTransID;

    // Statement Update Cursor List
    smiTableCursor mUpdateCursors;

    // Statemnet Cursor List
    smiTableCursor mCursors;

    // Statement�� View�� �����ϴ� SCN
    smSCN          mSCN;
    smSCN          mInfiniteSCN;
    /*
      Statement �Ӽ�: SMI_STATEMENT ..., NORMAL | UNTOUCHABLE,
                      MEMORY_CURSOR | DISK_CURSOR | ALL_CURSOR.
    */
    UInt           mFlag;

    // Statement�� Root Statement
    smiStatement*  mRoot;

    // Open�� Cursor�� ����.
    UInt           mOpenCursorCnt;

    /* BUG-15906: non-autocommit��忡�� Select�Ϸ��� IS_LOCK�� �����Ǹ�
     * ���ڽ��ϴ�.
     * Select Statement�϶� Lock�� ������ Ǯ��� ������ �����ϱ� ����
     * Transaction�� Lock Slot List�� ������ Slot�� Lock Sequence Number��
     * ������ �д�. */
    ULong          mLockSlotSequence;

    // Implicit Savepoint
    smxSavepoint  *mISavepoint;

    // Statement Depth
    UInt           mDepth;

    /*
     * [BUG-24187] Rollback�� statement�� Internal CloseCurosr��
     * ������ �ʿ䰡 �����ϴ�.
     */
    idBool         mSkipCursorClose;

    idvSQL        *mStatistics;

    /* PROJ-2733 */
    smSCN          mRequestSCN;

    /* TASK-7219 Non-shard DML : partial stmt ó���� ����, cursor close�� �ʿ��� ��츸 infinite SCN�� ������Ų��. */
    idBool         mIncInfiniteSCN4ClosingCursor;
};

inline smiTrans* smiStatement::getTrans( void )
{
    return mTrans;
}

inline void      smiStatement::setTrans(smiTrans *aTrans)
{
    mTrans = aTrans;
}

inline smSCN smiStatement::getSCN(void)
{
    return mSCN;
}

inline smSCN smiStatement::getInfiniteSCN(void)
{
    return mInfiniteSCN;
}

inline idBool smiStatement::isDummy()
{
    return (mParent == NULL ? ID_TRUE : ID_FALSE);
}

inline UInt smiStatement::getDepth()
{
    return mDepth;
}

/***********************************************************************
 * Description : Statement�� CursorClose�� Skip������ ������ �����Ѵ�.
 *
 * aStatement     - [IN]  ��� Statement
 **********************************************************************/
inline void smiStatement::setSkipCursorClose( smiStatement* aStatement )
{
    IDE_ASSERT( aStatement != NULL );
    
    aStatement->mSkipCursorClose = ID_TRUE;
}

/***********************************************************************
 * Description : Statement���� CursorClose�� Skip������ ������ ����.
 *
 * aStatement     - [IN]  ��� Statement
 **********************************************************************/
inline idBool smiStatement::getSkipCursorClose( smiStatement* aStatement )
{
    IDE_ASSERT( aStatement != NULL );
    
    return aStatement->mSkipCursorClose;
}

#endif /* _O_SMI_TRANS_H_ */
