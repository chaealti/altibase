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
 * $Id: smlFT.cpp 36756 2009-11-16 11:58:29Z et16 $
 **********************************************************************/

/* ------------------------------------------------
 *  X$LOCK Fixed Table
 * ----------------------------------------------*/

#include <idl.h>
#include <idu.h>
#include <ideErrorMgr.h>
#include <smErrorCode.h>
#include <smlDef.h>
#include <smDef.h>
#include <smr.h>
#include <smc.h>
#include <smp.h>
#include <smlLockMgr.h>
#include <smlReq.h>
#include <smlFT.h>

class smlLockInfo
{
public:
    smiLockItemType  mLockItemType;
    scSpaceID        mSpaceID;      // 잠금을 획득한 테이블스페이스 ID
    ULong            mItemID;       // 테이블타입이라면 Table OID
                                    // 디스크 데이타파일인 경우 File ID
    smTID            mTransID;
    SInt             mSlotID;      // lock을 요청한 transaction의 slot id
    UInt             mLockCnt;
    idBool           mBeGrant;   // grant되었는지를 나타내는 flag, BeGranted
    SInt             mLockMode;
    
    void getInfo(const smlLockNode* aNode)
    {
        mLockItemType   = aNode->mLockItemType;
        mSpaceID        = aNode->mSpaceID;
        mItemID         = aNode->mItemID;
        mTransID        = aNode->mTransID;
        mSlotID         = aNode->mSlotID;
        mLockCnt        = aNode->mLockCnt;
        mBeGrant        = aNode->mBeGrant;
        mLockMode       = smlLockMgr::getLockMode(aNode);
    }
};

IDE_RC smlFT::buildRecordForLockTBL(idvSQL              * /*aStatistics*/,
                                    void                *aHeader,
                                    void                * /* aDumpObj */,
                                    iduFixedTableMemory *aMemory)

{
    smcTableHeader *sCatTblHdr;
    smcTableHeader *sTableHeader;
    smpSlotHeader  *sPtr;
    SChar          *sCurPtr;
    SChar          *sNxtPtr;
    smlLockNode    *sLockNode;
    smlLockNode    *sIterator;
    smlLockInfo     sLockInfo;
    SInt            i;

    IDE_ERROR( aHeader != NULL );
    IDE_ERROR( aMemory != NULL );

    switch(smuProperty::getLockMgrType())
    {
    case 0:
        sCatTblHdr = (smcTableHeader*)SMC_CAT_TABLE;
        sCurPtr = NULL;

        // [1] 테이블 헤더를 순회하면서 현재 lock node list를 구한다.
        while(1)
        {
            IDE_TEST( smcRecord::nextOIDall( sCatTblHdr,
                        sCurPtr,
                        &sNxtPtr )
                    != IDE_SUCCESS );

            if ( sNxtPtr == NULL )
            {
                break;
            }

            sPtr = (smpSlotHeader *)sNxtPtr;

            // To fix BUG-14681
            if ( SM_SCN_IS_INFINITE(sPtr->mCreateSCN) )
            {
                /* BUG-14974: 무한 Loop발생.*/
                sCurPtr = sNxtPtr;
                continue;
            }

            sTableHeader = (smcTableHeader *)( sPtr + 1 );

            // 1. temp table은 skip ( PROJ-2201 TempTable은 이제 여기 없음 */
            // 2. drop된 table은 skip
            // 3. meta  table은 skip

            if( ( SMI_TABLE_TYPE_IS_META( sTableHeader ) == ID_TRUE ) ||
                    ( smcTable::isDropedTable(sTableHeader) == ID_TRUE ) )
            {
                sCurPtr = sNxtPtr;
                continue;
            }

            IDE_TEST( getLockItemNodes( aHeader,
                        aMemory,
                        (smlLockItem*)sTableHeader->mLock )
                    != IDE_SUCCESS );

            sCurPtr = sNxtPtr;
        }
        break;

    case 1:
        for( i = 0; i < smLayerCallback::getCurTransCnt(); i++ )
        {
            sLockNode = &(smlLockMgr::mArrOfLockList[i].mLockNodeHeader);
            sIterator = sLockNode->mNxtTransLockNode;

            while(sIterator != sLockNode)
            {
                IDU_FIT_POINT( "BUG-46726@smlFT::buildRecordForLockTBL::lockNode" );

                sLockInfo.getInfo(sIterator);
                IDE_TEST(iduFixedTable::buildRecord(aHeader,
                                                    aMemory,
                                                    (void *) &sLockInfo)
                        != IDE_SUCCESS);

                sIterator = sIterator->mNxtTransLockNode;

                if ( sIterator == NULL )
                {
                    /* BUG-46726 : SPIN LOCK
                     * 현재 보고있는 lock node 가 삭제된 경우 next가 NULL 이 될수있다.
                     * 이경우 더이상 lock node를 탐색하지 다음 tx로 넘어간다.
                     * => 따라서, x$lock 쿼리결과에 유실이 발생할수있다. 
                     */
                    break;
                }
            }
        }
        break;

    default:
        IDE_DASSERT(0);
        break;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

static iduFixedTableColDesc gLockTBLColDesc[]=
{
    {
        (SChar*)"LOCK_ITEM_TYPE",
        offsetof(smlLockInfo,mLockItemType),
        IDU_FT_SIZEOF(smlLockInfo,mLockItemType),
        IDU_FT_TYPE_UINTEGER,
        NULL,
        0, 0,NULL // for internal use
    },

    {
        (SChar*)"TBS_ID",
        offsetof(smlLockInfo,mSpaceID),
        IDU_FT_SIZEOF(smlLockInfo,mSpaceID),
        IDU_FT_TYPE_UINTEGER,
        NULL,
        0, 0,NULL // for internal use
    },

    {
        (SChar*)"TABLE_OID",
        offsetof(smlLockInfo,mItemID),
        IDU_FT_SIZEOF(smlLockInfo,mItemID),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0,NULL // for internal use
    },

    {
        (SChar*)"TRANS_ID",
        offsetof(smlLockInfo,mTransID),
        IDU_FT_SIZEOF(smlLockInfo,mTransID),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0,NULL // for internal use
    },

    {
        (SChar*)"LOCK_MODE",
        offsetof(smlLockInfo,mLockMode),
        IDU_FT_SIZEOF(smlLockInfo,mLockMode),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0,NULL // for internal use
    },


    {
        (SChar*)"LOCK_CNT",
        offsetof(smlLockInfo,mLockCnt),
        IDU_FT_SIZEOF(smlLockInfo,mLockCnt),
        IDU_FT_TYPE_UINTEGER,
        NULL,
        0, 0,NULL // for internal use
    },

    {
        (SChar*)"IS_GRANT",
        offsetof(smlLockInfo,mBeGrant),
        IDU_FT_SIZEOF(smlLockInfo,mBeGrant),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0,NULL // for internal use
    },

    {
        NULL,
        0,
        0,
        IDU_FT_TYPE_CHAR,
        NULL,
        0, 0,NULL // for internal use
    }
};

iduFixedTableDesc  gLockTBLDesc =
{
    (SChar *)"X$LOCK",
    smlFT::buildRecordForLockTBL,
    gLockTBLColDesc,
    IDU_STARTUP_META,
    0,
    0,
    IDU_FT_DESC_TRANS_NOT_USE,
    NULL
};


IDE_RC smlFT::getLockItemNodes( void                *aHeader,
                                iduFixedTableMemory *aMemory,
                                smlLockItem         *aLockItem )
{
    SInt                i;
    smlLockNode*        sCurLockNode;
    smlLockInfo         sLockInfo;
    smlLockItemMutex*   sLockItem = (smlLockItemMutex*)aLockItem;
    UInt                sState = 0;

    IDE_TEST_RAISE(sLockItem->mMutex.lock(NULL /* idvSQL* */)
                   != IDE_SUCCESS,
                   err_mutex_lock);
    sState = 1;

    // Grant Lock Node list
    sCurLockNode = sLockItem->mFstLockGrant;

    for(i = 0; i < sLockItem->mGrantCnt; i++)
    {
        sLockInfo.getInfo(sCurLockNode);
        IDE_TEST(iduFixedTable::buildRecord(aHeader,
                                            aMemory,
                                            (void *) &sLockInfo)
             != IDE_SUCCESS);
        sCurLockNode = sCurLockNode->mNxtLockNode;
    }

    sCurLockNode = sLockItem->mFstLockRequest;

    // request lock node list
    for(i = 0; i < sLockItem->mRequestCnt; i++)
    {
        sLockInfo.getInfo(sCurLockNode);
        IDE_TEST(iduFixedTable::buildRecord(aHeader,
                                            aMemory,
                                            (void *) &sLockInfo)
                 != IDE_SUCCESS);
        // alloc new lock node for performance view.
        sCurLockNode = sCurLockNode->mNxtLockNode;
    }

    sState = 0;
    IDE_TEST_RAISE(sLockItem->mMutex.unlock() != IDE_SUCCESS,
                   err_mutex_unlock);

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_mutex_lock);
    {
        IDE_SET(ideSetErrorCode(smERR_FATAL_ThrMutexLock));
    }
    IDE_EXCEPTION(err_mutex_unlock);
    {
        IDE_SET(ideSetErrorCode(smERR_FATAL_ThrMutexUnlock));
    }
    IDE_EXCEPTION_END;

    if(sState != 0)
    {
        IDE_ASSERT(sLockItem->mMutex.unlock() == IDE_SUCCESS);
    }

    return IDE_FAILURE;
}

/* ------------------------------------------------
 *   X$LOCK_MODE Fixed Table
 * ----------------------------------------------*/

IDE_RC smlFT::buildRecordForLockMode(idvSQL              * /*aStatistics*/,
                                     void        *aHeader,
                                     void        * /* aDumpObj */,
                                     iduFixedTableMemory *aMemory)
{
    UInt i;

    IDE_ERROR( aHeader != NULL );
    IDE_ERROR( aMemory != NULL );

// [2] build record.
    for(i=0; i < SML_NUMLOCKTYPES; i++ )
    {
        IDE_TEST( iduFixedTable::buildRecord( aHeader,
                                              aMemory,
                                              (void *)& smlLockMgr::mLockMode2StrTBL[i])
             != IDE_SUCCESS);
    }//for


    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


static iduFixedTableColDesc   gLockTableModeColDesc[]=
{

    {
        (SChar*)"LOCK_MODE",
        offsetof(smlLockMode2StrTBL,mLockMode),
        IDU_FT_SIZEOF(smlLockMode2StrTBL,mLockMode),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"LOCK_DESC",
        offsetof(smlLockMode2StrTBL,mLockModeStr),
        SML_LOCKMODE_STRLEN,
        IDU_FT_TYPE_VARCHAR,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        NULL,
        0,
        0,
        IDU_FT_TYPE_CHAR,
        NULL,
        0, 0,NULL // for internal use
    }
};

// X$LOCK
iduFixedTableDesc  gLockModeTableDesc =
{
    (SChar *)"X$LOCK_MODE",
    smlFT::buildRecordForLockMode,
    gLockTableModeColDesc,
    IDU_STARTUP_CONTROL,
    0,
    0,
    IDU_FT_DESC_TRANS_NOT_USE,
    NULL
};



/* ------------------------------------------------
 *   X$LOCK_WAIT  Fixed Table
 *   ex)
 *     TxA(1) is waiting TxB(2)
 *     TxB(2) is waiting TxC(3)
 *     TxC(3) is waiting TxD(4)
 *   select * from x$lock_wait
 *    =>  TID         WAITFORTID
 *         1              2
 *         2              3
 *         3              4
 * ----------------------------------------------*/

typedef struct smlLockWaitStat
{
    smTID  mTID;         // Current Waiting Tx

    smTID  mWaitForTID;  // Wait For Target Tx

} smlLockWaitStat;


IDE_RC smlFT::buildRecordForLockWait(idvSQL              * /*aStatistics*/,
                                     void        *aHeader,
                                     void        * /* aDumpObj */,
                                     iduFixedTableMemory *aMemory)
{
    SInt            i;
    SInt            j;
    SInt            k; /* for preventing from infinite loop */
    smlLockWaitStat sStat;

    IDE_ERROR( aHeader != NULL );
    IDE_ERROR( aMemory != NULL );

    switch(smuProperty::getLockMgrType())
    {
    case 0:
        for ( j = 0; j < smLayerCallback::getCurTransCnt(); j++ )
        {
            if ( smLayerCallback::isActiveBySID(j) == ID_TRUE )
            {
                for ( k = 0, i = smlLockMgr::mArrOfLockList[j].mFstWaitTblTransItem;
                      (i != SML_END_ITEM) && 
                      (i != ID_USHORT_MAX) &&
                      (k < smLayerCallback::getCurTransCnt()) ;
                      i = smlLockMgr::mWaitForTable[j][i].mNxtWaitTransItem, k++)
                {
                    if (smlLockMgr::mWaitForTable[j][i].mIndex == 1)
                    {
                        sStat.mTID        = smLayerCallback::getTIDBySID( j );
                        sStat.mWaitForTID = smLayerCallback::getTIDBySID( i );

                        IDE_TEST(iduFixedTable::buildRecord( aHeader,
                                                             aMemory,
                                                             (void *)& sStat)
                                != IDE_SUCCESS);
                    }
                }
            }
        }
        break;

    case 1:
        for ( i = 0; i < smLayerCallback::getCurTransCnt(); i++ )
        {
            if ( smLayerCallback::isActiveBySID( i ) == ID_TRUE )
            {
                for(j = 0; j < smlLockMgr::mTransCnt ; j++)
                {

                    if ( smlLockMgr::mPendingMatrix[i][j] != -1 )
                    {
                        sStat.mTID        = smLayerCallback::getTIDBySID( i );
                        sStat.mWaitForTID = smLayerCallback::getTIDBySID( j );

                        IDE_TEST(iduFixedTable::buildRecord(aHeader,
                                    aMemory,
                                    (void *)& sStat)
                                != IDE_SUCCESS);
                    }

                }
            }
            else
            {
                /* continue */
            }
        }
        break;

    default:
        IDE_ASSERT(0);
        break;
    }
    

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

static iduFixedTableColDesc   gLockWaitColDesc[]=
{

    {
        (SChar*)"TRANS_ID",
        offsetof(smlLockWaitStat,mTID),
        IDU_FT_SIZEOF(smlLockWaitStat,mTID),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0,NULL // for internal use
    },

    {
        (SChar*)"WAIT_FOR_TRANS_ID",
        offsetof(smlLockWaitStat,mWaitForTID),
        IDU_FT_SIZEOF(smlLockWaitStat,mWaitForTID),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0,NULL // for internal use
    },

    {
        NULL,
        0,
        0,
        IDU_FT_TYPE_CHAR,
        NULL,
        0, 0,NULL // for internal use
    }
};

// X$LOCK
iduFixedTableDesc  gLockWaitTableDesc =
{
    (SChar *)"X$LOCK_WAIT",
    smlFT::buildRecordForLockWait,
    gLockWaitColDesc,
    IDU_STARTUP_CONTROL,
    0,
    0,
    IDU_FT_DESC_TRANS_NOT_USE,
    NULL
};

/* ------------------------------------------------
 *  X$LOCK_TABLESPACE Fixed Table
 * ----------------------------------------------*/

IDE_RC smlFT::buildRecordForLockTBS(idvSQL              * /*aStatistics*/,
                                    void                *aHeader,
                                    void                * /* aDumpObj */,
                                    iduFixedTableMemory *aMemory)

{
    UInt                sState = 0;
    sctTableSpaceNode * sNextSpaceNode;
    sctTableSpaceNode * sCurrSpaceNode;

    IDE_ERROR( aHeader != NULL );
    IDE_ERROR( aMemory != NULL );

    if(smuProperty::getLockMgrType() == 1)
        return IDE_SUCCESS;

    IDE_TEST( sctTableSpaceMgr::lock(NULL /* idvSQL* */)
              != IDE_SUCCESS );
    sState = 1;

    sctTableSpaceMgr::getFirstSpaceNode( (void**)&sCurrSpaceNode );

    while( sCurrSpaceNode != NULL )
    {
        sctTableSpaceMgr::getNextSpaceNode( (void*)sCurrSpaceNode,
                                            (void**)&sNextSpaceNode );

        IDE_ASSERT( (sCurrSpaceNode->mState & SMI_TBS_DROPPED)
                    != SMI_TBS_DROPPED );

        IDE_TEST( getLockItemNodes( aHeader,
                                    aMemory,
                                    (smlLockItem*)
                                    sCurrSpaceNode->mLockItem4TBS )
                  != IDE_SUCCESS );

        sCurrSpaceNode = sNextSpaceNode;
    }

    sState = 0;
    IDE_TEST( sctTableSpaceMgr::unlock() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();
    {
        if ( sState != 0 )
        {
            IDE_ASSERT( sctTableSpaceMgr::unlock() == IDE_SUCCESS );
        }
    }
    IDE_POP();

    return IDE_FAILURE;
}

static iduFixedTableColDesc gLockTBSColDesc[]=
{
    {
        (SChar*)"LOCK_ITEM_TYPE",
        offsetof(smlLockInfo,mLockItemType),
        IDU_FT_SIZEOF(smlLockInfo,mLockItemType),
        IDU_FT_TYPE_UINTEGER,
        NULL,
        0, 0,NULL // for internal use
    },

    {
        (SChar*)"TBS_ID",
        offsetof(smlLockInfo,mSpaceID),
        IDU_FT_SIZEOF(smlLockInfo,mSpaceID),
        IDU_FT_TYPE_UINTEGER,
        NULL,
        0, 0,NULL // for internal use
    },

    {
        (SChar*)"DBF_ID",
        offsetof(smlLockInfo,mItemID),
        IDU_FT_SIZEOF(smlLockInfo,mItemID),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0,NULL // for internal use
    },

    {
        (SChar*)"TRANS_ID",
        offsetof(smlLockInfo,mTransID),
        IDU_FT_SIZEOF(smlLockInfo,mTransID),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0,NULL // for internal use
    },

    {
        (SChar*)"LOCK_MODE",
        offsetof(smlLockInfo,mLockMode),
        IDU_FT_SIZEOF(smlLockInfo,mLockMode),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0,NULL // for internal use
    },


    {
        (SChar*)"LOCK_CNT",
        offsetof(smlLockInfo,mLockCnt),
        IDU_FT_SIZEOF(smlLockInfo,mLockCnt),
        IDU_FT_TYPE_UINTEGER,
        NULL,
        0, 0,NULL // for internal use
    },

    {
        (SChar*)"IS_GRANT",
        offsetof(smlLockInfo,mBeGrant),
        IDU_FT_SIZEOF(smlLockInfo,mBeGrant),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0,NULL // for internal use
    },

    {
        NULL,
        0,
        0,
        IDU_FT_TYPE_CHAR,
        NULL,
        0, 0,NULL // for internal use
    }
};


iduFixedTableDesc  gLockTBSDesc =
{
    (SChar *)"X$LOCK_TABLESPACE",
    smlFT::buildRecordForLockTBS,
    gLockTBSColDesc,
    IDU_STARTUP_META,
    0,
    0,
    IDU_FT_DESC_TRANS_NOT_USE,
    NULL
};
