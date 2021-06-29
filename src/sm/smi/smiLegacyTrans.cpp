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
 * $Id: 
 **********************************************************************/

#include <ide.h>

#include <smiStatement.h>
#include <smiLegacyTrans.h>
#include <smxTrans.h>
#include <smxLegacyTransMgr.h>

/* smiLegacyTrans�� smxLegacyTransMgr�� ���� interface��. */

/***********************************************************************
 *
 * Description : Legacy Statement�� �����Ѵ�.
 *
 * aLegacyTrans     - [IN] ���� ������ Legacy Ʈ������ 
 * aOrgStmtListHead - [IN]  Commit�ϴ� Ʈ�������� Statement List Header
 *
 **********************************************************************/
IDE_RC smiLegacyTrans::makeLegacyStmt( void          * aLegacyTrans,
                                       smiStatement  * aOrgStmtListHead )
{
    smiStatement  * sChildStmt;
    smiStatement  * sNewStmtListHead;
    smiStatement  * sBackupPrev     = NULL; /* BUG-41342 */
    smiStatement  * sBackupNext     = NULL;
    smiStatement  * sBackupAllPrev  = NULL;
    UInt            sState          = 0;
    UInt            sDumpCnt        = 0;

    IDE_ASSERT( aOrgStmtListHead != NULL );

    /* BUG-41342
     * aOrgStmtListHead �� �ٲ�� ���� �̸� ��� */
    sBackupPrev     = aOrgStmtListHead->mNext->mPrev;
    sBackupNext     = aOrgStmtListHead->mPrev->mNext;
    sBackupAllPrev  = aOrgStmtListHead->mAllPrev;

    /* smiLegacyTrans_makeLegacyStmt_calloc_NewStmtListHead.tc */
    IDU_FIT_POINT_RAISE("smiLegacyTrans::makeLegacyStmt::calloc::NewStmtListHead",insufficient_memory);
    IDE_TEST_RAISE( iduMemMgr::calloc(IDU_MEM_SM_TRANSACTION_LEGACY_TRANS,
                                1,
                                ID_SIZEOF(smiStatement),
                                (void**)&(sNewStmtListHead)) != IDE_SUCCESS,
                    insufficient_memory );
    sState = 1;

    sNewStmtListHead->mChildStmtCnt = aOrgStmtListHead->mChildStmtCnt;
    sNewStmtListHead->mTransID      = aOrgStmtListHead->mTransID;
    sNewStmtListHead->mFlag         = SMI_STATEMENT_LEGACY_TRUE;

    sNewStmtListHead->mNext = aOrgStmtListHead->mNext;
    sNewStmtListHead->mPrev = aOrgStmtListHead->mPrev;

    aOrgStmtListHead->mNext->mPrev = sNewStmtListHead;
    aOrgStmtListHead->mPrev->mNext = sNewStmtListHead;

    sNewStmtListHead->mAllNext = aOrgStmtListHead;
    sNewStmtListHead->mAllPrev = aOrgStmtListHead->mAllPrev;

    sNewStmtListHead->mAllNext->mAllPrev = sNewStmtListHead;
    sNewStmtListHead->mAllPrev->mAllNext = sNewStmtListHead;

    sNewStmtListHead->mTrans  = NULL;
    sNewStmtListHead->mParent = NULL;
    sNewStmtListHead->mUpdate = NULL;

    sChildStmt = sNewStmtListHead->mNext;

    while( sChildStmt != sNewStmtListHead )
    {
        if( sChildStmt->mParent != aOrgStmtListHead )
        {
            ideLog::log( IDE_SM_0, "Statement Link is Corrupted.\n"
                                   "Tx StmtListHead     : 0x%"ID_XPOINTER_FMT"\n"
                                   "Tx ChildStmt Cnt    : %"ID_UINT32_FMT"\n"
                                   "Corrupted ChildStmt : 0x%"ID_XPOINTER_FMT"\n"
                                   "sChildStmt Parent   : 0x%"ID_XPOINTER_FMT,
                                   aOrgStmtListHead,
                                   aOrgStmtListHead->mChildStmtCnt,
                                   sChildStmt,
                                   sChildStmt->mParent );

            sChildStmt = sNewStmtListHead->mNext;

            while( ( sChildStmt != NULL ) && 
                   ( sChildStmt != sNewStmtListHead ) &&
                   ( sDumpCnt <= aOrgStmtListHead->mChildStmtCnt ) )
            {
                ideLog::log( IDE_SM_0,"sChildStmt                : 0x%"ID_XPOINTER_FMT"\n"
                                      "sChildStmt->mTrans        : 0x%"ID_XPOINTER_FMT"\n"
                                      "sChildStmt->mTransID      : %"ID_UINT32_FMT"\n"
                                      "sChildStmt->mParent       : 0x%"ID_XPOINTER_FMT"\n"
                                      "sChildStmt->mPrev         : 0x%"ID_XPOINTER_FMT"\n"
                                      "sChildStmt->mNext         : 0x%"ID_XPOINTER_FMT"\n"
                                      "sChildStmt->mAllPrev      : 0x%"ID_XPOINTER_FMT"\n"
                                      "sChildStmt->mAllNext      : 0x%"ID_XPOINTER_FMT"\n"
                                      "sChildStmt->mUpdate       : 0x%"ID_XPOINTER_FMT"\n"
                                      "sChildStmt->mChildStmtCnt : %"ID_UINT32_FMT"\n"
                                      "sChildStmt->mFlag         : %"ID_UINT32_FMT"\n"
                                      "sChildStmt->mSCN          : %"ID_XINT64_FMT"\n"
                                      "sChildStmt->mInfiniteSCN  : %"ID_XINT64_FMT,
                                      sChildStmt,
                                      sChildStmt->mTrans,
                                      sChildStmt->mTransID,
                                      sChildStmt->mParent,
                                      sChildStmt->mPrev,
                                      sChildStmt->mNext,
                                      sChildStmt->mAllPrev,
                                      sChildStmt->mAllNext,
                                      sChildStmt->mUpdate,
                                      sChildStmt->mChildStmtCnt,
                                      sChildStmt->mFlag,
                                      sChildStmt->mSCN,
                                      sChildStmt->mInfiniteSCN );

                sChildStmt = sChildStmt->mNext;
                sDumpCnt++;
            }

            IDE_ASSERT(0);
        }

        sChildStmt->mParent = sNewStmtListHead;
        sChildStmt          = sChildStmt->mNext;
    }

    IDE_TEST( smxLegacyTransMgr::allocAndSetLegacyStmt( aLegacyTrans,
                                                        sNewStmtListHead )
              != IDE_SUCCESS );

    sState = 0;

    return IDE_SUCCESS;

    IDE_EXCEPTION( insufficient_memory );
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_InsufficientMemory));
    }
    IDE_EXCEPTION_END;

    switch( sState )
    {
        case 1:
            /* BUG-41342
             * aOrgStmtListHead �� ���� ����� �� �ִ�. ������ �ش�. */
            aOrgStmtListHead->mNext->mPrev  = sBackupPrev;
            aOrgStmtListHead->mPrev->mNext  = sBackupNext;
            aOrgStmtListHead->mAllPrev      = sBackupAllPrev;
            aOrgStmtListHead->mAllPrev->mAllNext    = aOrgStmtListHead;

            sChildStmt = aOrgStmtListHead->mNext;

            while( sChildStmt != aOrgStmtListHead )
            {
                sChildStmt->mParent = aOrgStmtListHead;
                sChildStmt          = sChildStmt->mNext;
            }

            IDE_ASSERT( iduMemMgr::free(sNewStmtListHead) == IDE_SUCCESS );
            sNewStmtListHead = NULL;
        default:
            break;
    }

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description : Legacy Transaction�� List���� �����ϰ�,
 *               �Ҵ���� �ڿ��� �����Ѵ�.
 *
 * aStmtListHead - [IN] ������ �Ϸ��� Legacy TX�� Statement List Header
 * aSmxTrans     - [IN] ������ �Ϸ��� Ʈ������ (smxTrans)
 *
 **********************************************************************/
IDE_RC smiLegacyTrans::removeLegacyTrans( smiStatement * aStmtListHead,
                                          void         * aSmxTrans )
{
    IDE_ERROR( smxLegacyTransMgr::removeLegacyTrans( NULL, /* aLegacyTransNode */
                                                     aStmtListHead->mTransID )
               == IDE_SUCCESS );

    ((smxTrans*)aSmxTrans)->mLegacyTransCnt--;

    aStmtListHead->mAllNext->mAllPrev = aStmtListHead->mAllPrev;
    aStmtListHead->mAllPrev->mAllNext = aStmtListHead->mAllNext;

    IDE_ASSERT( smxLegacyTransMgr::removeLegacyStmt( NULL, /* aLegacyStmtNode */
                                                     aStmtListHead )
                == IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
