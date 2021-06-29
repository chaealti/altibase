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
 * $Id: smxOIDList.cpp 90259 2021-03-19 01:22:22Z emlee $
 **********************************************************************/

#include <smErrorCode.h>
#include <sml.h>
#include <smr.h>
#include <smm.h>
#include <svm.h>
#include <smc.h>
#include <smn.h>
#include <smp.h>
#include <sdn.h>
#include <sct.h>
#include <svcRecord.h>
#include <smxReq.h>

#if defined(SMALL_FOOTPRINT)
#define SMX_OID_LIST_NODE_POOL_SIZE (8)
#else
#define SMX_OID_LIST_NODE_POOL_SIZE (1024)
#endif

iduOIDMemory  smxOIDList::mOIDMemory;
iduMemPool    smxOIDList::mMemPool;
UInt          smxOIDList::mOIDListSize;
UInt          smxOIDList::mMemPoolType;

const smxOidSavePointMaskType  smxOidSavePointMask[SM_OID_OP_COUNT] = {
    {/* SM_OID_OP_OLD_FIXED_SLOT                         */
        ~( SM_OID_ACT_SAVEPOINT    |
           SM_OID_ACT_COMMIT       |
           SM_OID_ACT_AGING_COMMIT |
           SM_OID_ACT_AGING_INDEX  ),
        0x00000000
    },
    {/* SM_OID_OP_CHANGED_FIXED_SLOT                     */
        ~( SM_OID_ACT_SAVEPOINT ),
        0x00000000
    },
    {/* SM_OID_OP_NEW_FIXED_SLOT                         */
        ~( SM_OID_ACT_SAVEPOINT |
           SM_OID_ACT_COMMIT    ),
        SM_OID_ACT_AGING_COMMIT
    },
    {/* SM_OID_OP_DELETED_SLOT                           */
        ~( SM_OID_ACT_SAVEPOINT |
           SM_OID_ACT_COMMIT    |
           SM_OID_ACT_AGING_COMMIT ),
        0x00000000
    },
    {/* SM_OID_OP_LOCK_FIXED_SLOT                        */
        ~( SM_OID_ACT_SAVEPOINT|
           SM_OID_ACT_COMMIT   ),
        0x00000000
    },
    {/* SM_OID_OP_UNLOCK_FIXED_SLOT                      */
        ~( SM_OID_ACT_SAVEPOINT|
           SM_OID_ACT_COMMIT   ),
        0x00000000
    },
    {/* SM_OID_OP_OLD_VARIABLE_SLOT                      */
        ~( SM_OID_ACT_SAVEPOINT    |
           SM_OID_ACT_AGING_COMMIT ),
        0x00000000
    },
    {/* SM_OID_OP_NEW_VARIABLE_SLOT                      */
        ~( SM_OID_ACT_SAVEPOINT ),
        SM_OID_ACT_AGING_COMMIT
    },
    {/* SM_OID_OP_DROP_TABLE                             */
        ~( SM_OID_ACT_SAVEPOINT    |
           SM_OID_ACT_COMMIT       |
           SM_OID_ACT_AGING_COMMIT ),
        0x00000000
    },
    {/* SM_OID_OP_DROP_TABLE_BY_ABORT                    */
        ~( SM_OID_ACT_SAVEPOINT |
           SM_OID_ACT_COMMIT ),
        0x00000000
    },
    {/* SM_OID_OP_DELETE_TABLE_BACKUP                    */
        ~( SM_OID_ACT_SAVEPOINT    |
           SM_OID_ACT_COMMIT       |
           SM_OID_ACT_AGING_COMMIT ),
        0x00000000
    },
    {/* SM_OID_OP_DROP_INDEX                             */
        ~( SM_OID_ACT_SAVEPOINT    |
           SM_OID_ACT_COMMIT       |
           SM_OID_ACT_AGING_COMMIT ),
        0x00000000
    },
    {/* SM_OID_OP_UPATE_FIXED_SLOT                       */
        ~( SM_OID_ACT_SAVEPOINT),
        0x00000000
    },
    {/* SM_OID_OP_DDL_TABLE                              */
        ~( SM_OID_ACT_SAVEPOINT),
        0x00000000
    },
    /* BUG-27742 Partial Rollback�� LPCH�� �ι� Free�Ǵ� ������ �ֽ��ϴ�.
     * Partial Rollback�� Tx�� Commit, Rollback�� �����ϰ�
     * Old LPCH�� ���� �Ǿ�� �ϰ�, New LPCH�� ���ŵǾ�� �մϴ�. */
    {/* SM_OID_OP_FREE_OLD_LPCH                          */
        ~( SM_OID_ACT_SAVEPOINT    |
           SM_OID_ACT_AGING_COMMIT ),
        0x00000000
    },
    {/* SM_OID_OP_FREE_NEW_LPCH                          */
        ~( SM_OID_ACT_SAVEPOINT ),
        SM_OID_ACT_AGING_COMMIT
    },
    {/* SM_OID_OP_ALL_INDEX_DISABLE                      */
        ~( SM_OID_ACT_SAVEPOINT    |
           SM_OID_ACT_COMMIT ),
        0x00000000
    }
};

IDE_RC smxOIDList::initializeStatic()
{
    mMemPoolType = smuProperty::getTxOIDListMemPoolType();

    IDE_ASSERT( (mMemPoolType == 0) || (mMemPoolType == 1) );

    mOIDListSize = ID_SIZEOF(smxOIDInfo) *
                   (smuProperty::getOIDListCount() - 1) +
                   ID_SIZEOF(smxOIDNode);

    if ( mMemPoolType == 0 )
    {
        IDE_TEST( mOIDMemory.initialize( IDU_MEM_SM_TRANSACTION_OID_LIST,
                                         mOIDListSize,
                                         SMX_OID_LIST_NODE_POOL_SIZE )
                  != IDE_SUCCESS );
    }
    else
    {
        IDE_TEST( mMemPool.initialize( IDU_MEM_SM_SMX,
                                       (SChar*)"SMX_OIDLIST_POOL",
                                       ID_SCALABILITY_SYS,
                                       mOIDListSize,
                                       SMX_OID_LIST_NODE_POOL_SIZE,
                                       IDU_AUTOFREE_CHUNK_LIMIT,		/* ChunkLimit */
                                       ID_TRUE,							/* UseMutex */
                                       IDU_MEM_POOL_DEFAULT_ALIGN_SIZE,	/* AlignByte */
                                       ID_FALSE,						/* ForcePooling */
                                       ID_TRUE,							/* GarbageCollection */
                                       ID_TRUE,                         /* HWCacheLine */
                                       IDU_MEMPOOL_TYPE_LEGACY          /* mempool type*/ ) 
                  != IDE_SUCCESS );			
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

IDE_RC smxOIDList::destroyStatic()
{
    if ( mMemPoolType == 0 )
    {
        IDE_TEST( mOIDMemory.destroy() != IDE_SUCCESS );
    }
    else
    {
        IDE_TEST( mMemPool.destroy() != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC smxOIDList::initialize( smxTrans              * aTrans,
                               smxOIDNode            * aCacheOIDNode4Insert,
                               idBool                  aIsUnique,
                               smxOIDList::addOIDFunc  aAddOIDFunc )
{
    UInt  sState = 0;

    mIsUnique            = aIsUnique;
    mTrans               = aTrans;
    mCacheOIDNode4Insert = aCacheOIDNode4Insert;

    init();

    mAddOIDFunc  = aAddOIDFunc;

    mUniqueOIDHash = NULL;

    if ( mIsUnique == ID_TRUE )
    {
        /* TC/FIT/Limit/sm/smx/smxOIDList_initialize_malloc.sql */
        IDU_FIT_POINT_RAISE( "smxOIDList::initialize::malloc",
                              insufficient_memory );

        IDE_TEST_RAISE( iduMemMgr::malloc( IDU_MEM_SM_SMX,
                                     ID_SIZEOF(smuHashBase),
                                     (void**)&mUniqueOIDHash ) != IDE_SUCCESS,
                        insufficient_memory );
        sState = 1;

        IDE_TEST( smuHash::initialize(
                      mUniqueOIDHash,
                      1,                                // ConcurrentLevel
                      SMX_UNIQUEOID_HASH_BUCKETCOUNT,
                      ID_SIZEOF(smOID),                 // KeyLength
                      ID_FALSE,                         // UseLatch
                      smxTrans::genHashValueFunc,
                      smxTrans::compareFunc )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( insufficient_memory );
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_InsufficientMemory));
    }
    IDE_EXCEPTION_END;

    IDE_PUSH();
    {
        switch( sState )
        {
            case 1:
                IDE_ASSERT( iduMemMgr::free(mUniqueOIDHash)
                            == IDE_SUCCESS );
                mUniqueOIDHash = NULL;
                break;

            default:
                break;
        }
    }
    IDE_POP();

    return IDE_FAILURE;
}

void smxOIDList::init()
{
    mItemCnt                  = 0;
    mOIDNodeListHead.mPrvNode = &mOIDNodeListHead;
    mOIDNodeListHead.mNxtNode = &mOIDNodeListHead;
    mOIDNodeListHead.mOIDCnt  = 0;
    mNeedAging                = ID_FALSE;

    if ( mCacheOIDNode4Insert != NULL )
    {
        initOIDNode(mCacheOIDNode4Insert);
    }
}


IDE_RC smxOIDList::freeOIDList()
{
    smxOIDNode *sCurOIDNode;
    smxOIDNode *sNxtOIDNode;

    sCurOIDNode = mOIDNodeListHead.mNxtNode;

    while(sCurOIDNode != &mOIDNodeListHead)
    {
        /* fix BUG-26934 : [codeSonar] Null Pointer Dereference */
        IDE_ASSERT( sCurOIDNode != NULL );

        sNxtOIDNode = sCurOIDNode->mNxtNode;
        if(sCurOIDNode !=  mCacheOIDNode4Insert)
        {
            IDE_TEST(freeMem(sCurOIDNode) != IDE_SUCCESS);
        }

        sCurOIDNode = sNxtOIDNode;
    }
    mOIDNodeListHead.mPrvNode = &mOIDNodeListHead;
    mOIDNodeListHead.mNxtNode = &mOIDNodeListHead;

    if ( mIsUnique == ID_TRUE )
    {
        IDE_TEST( smuHash::reset(mUniqueOIDHash) != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

IDE_RC smxOIDList::destroy()
{
    IDE_ASSERT( isEmpty() == ID_TRUE );

    if ( mUniqueOIDHash != NULL )
    {
        IDE_ASSERT( mIsUnique == ID_TRUE );

        IDE_TEST( smuHash::destroy(mUniqueOIDHash) != IDE_SUCCESS );

        IDE_TEST( iduMemMgr::free(mUniqueOIDHash)  != IDE_SUCCESS );
        mUniqueOIDHash = NULL;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}



IDE_RC smxOIDList::allocAndLinkOIDNode()
{

    smxOIDNode *sNewOIDNode;

    /* smxOIDList_allocAndLinkOIDNode_alloc_NewOIDNode.tc */
    IDU_FIT_POINT("smxOIDList::allocAndLinkOIDNode::alloc::NewOIDNode");
    IDE_TEST( alloc( &sNewOIDNode ) != IDE_SUCCESS );
    initOIDNode(sNewOIDNode);
    sNewOIDNode->mPrvNode   = mOIDNodeListHead.mPrvNode;
    sNewOIDNode->mNxtNode   = &(mOIDNodeListHead);

    mOIDNodeListHead.mPrvNode->mNxtNode = sNewOIDNode;
    mOIDNodeListHead.mPrvNode = sNewOIDNode;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

IDE_RC smxOIDList::add(smOID           aTableOID,
                       smOID           aTargetOID,
                       scSpaceID       aSpaceID,
                       UInt            aFlag,
                       smSCN           aSCN )
{
    return (this->*mAddOIDFunc)(aTableOID,aTargetOID,aSpaceID,aFlag,aSCN);
}

IDE_RC smxOIDList::addOID(smOID           aTableOID,
                          smOID           aTargetOID,
                          scSpaceID       aSpaceID,
                          UInt            aFlag,
                          smSCN           aSCN )
{
    smxOIDNode *sCurOIDNode;
    UInt        sItemCnt;

    sCurOIDNode = mOIDNodeListHead.mPrvNode;

    if(sCurOIDNode == &(mOIDNodeListHead) ||
       sCurOIDNode->mOIDCnt >= smuProperty::getOIDListCount())
    {
        IDE_TEST(allocAndLinkOIDNode() != IDE_SUCCESS);
        sCurOIDNode = mOIDNodeListHead.mPrvNode;
    }

    sItemCnt = sCurOIDNode->mOIDCnt;

    sCurOIDNode->mArrOIDInfo[sItemCnt].mTableOID  = aTableOID;
    sCurOIDNode->mArrOIDInfo[sItemCnt].mTargetOID = aTargetOID;
    sCurOIDNode->mArrOIDInfo[sItemCnt].mSpaceID   = aSpaceID;
    sCurOIDNode->mArrOIDInfo[sItemCnt].mFlag      = aFlag;
    sCurOIDNode->mArrOIDInfo[sItemCnt].mSCN       = aSCN;
    sCurOIDNode->mOIDCnt++;
    mItemCnt++;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC smxOIDList::addOIDWithCheckFlag(smOID           aTableOID,
                                       smOID           aTargetOID,
                                       scSpaceID       aSpaceID,
                                       UInt            aFlag,
                                       smSCN           aSCN )
{
    smxOIDNode *sCurOIDNode;
    UInt        sItemCnt;


    if( aFlag != SM_OID_NEW_INSERT_FIXED_SLOT )
    {
        /* Insert ���� ���꿡 ���� OID�� �����Ƿ� Ager�� �� �ʿ䰡 �ִ�. */
        mNeedAging = ID_TRUE;
    }//if aFlag

    sCurOIDNode =mOIDNodeListHead.mPrvNode;

    if(sCurOIDNode == &(mOIDNodeListHead))
    {
        /* use cached insert oid node. */
        mCacheOIDNode4Insert->mPrvNode = &(mOIDNodeListHead);
        mCacheOIDNode4Insert->mNxtNode = &(mOIDNodeListHead);

        mOIDNodeListHead.mPrvNode = mCacheOIDNode4Insert;
        mOIDNodeListHead.mNxtNode = mCacheOIDNode4Insert;
        sCurOIDNode = mOIDNodeListHead.mPrvNode;

    }//if
    else
    {
        if( sCurOIDNode->mOIDCnt >= smuProperty::getOIDListCount())
        {
            IDE_TEST(allocAndLinkOIDNode() != IDE_SUCCESS);
            sCurOIDNode = mOIDNodeListHead.mPrvNode;
        }
    }

    sItemCnt = sCurOIDNode->mOIDCnt;

    sCurOIDNode->mArrOIDInfo[sItemCnt].mTableOID  = aTableOID;
    sCurOIDNode->mArrOIDInfo[sItemCnt].mTargetOID = aTargetOID;
    sCurOIDNode->mArrOIDInfo[sItemCnt].mSpaceID   = aSpaceID;
    sCurOIDNode->mArrOIDInfo[sItemCnt].mFlag      = aFlag;
    sCurOIDNode->mArrOIDInfo[sItemCnt].mSCN       = aSCN;
    sCurOIDNode->mOIDCnt++;
    mItemCnt++;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description : �ߺ��� OID�� �������� �ʰ�, Verify�� OID�� �߰��Ѵ�.
 *
 * BUG-27122 Restart Recovery �� Undo Trans�� �����ϴ� �ε����� ����
 * Integrity üũ��� �߰� (__SM_CHECK_DISK_INDEX_INTEGRITY=2)
 *
 * aTableOID  - [IN] ���̺� OID
 * aTargetOID - [IN] Integrity üũ�� �ε��� OID
 * aSpaceID   - [IN] ���̺����̽� ID
 * aFlag      - [IN] OID ó���� ���� Flag
 *
 **********************************************************************/
IDE_RC smxOIDList::addOIDToVerify( smOID           aTableOID,
                                   smOID           aTargetOID,
                                   scSpaceID       aSpaceID,
                                   UInt            aFlag,
                                   smSCN           aSCN )
{
    smxOIDInfo  * sCurOIDInfo;
    smxOIDNode  * sCurOIDNode;
    UInt          sItemCnt;

    IDE_ASSERT( aFlag == SM_OID_NULL );

    IDE_ASSERT( smuHash::findNode( mUniqueOIDHash,
                                   &aTargetOID,
                                   (void**)&sCurOIDInfo )
                == IDE_SUCCESS );

    IDE_TEST_CONT( sCurOIDInfo != NULL, already_exist );

    sCurOIDNode = mOIDNodeListHead.mPrvNode;

    if(sCurOIDNode == &(mOIDNodeListHead) ||
       sCurOIDNode->mOIDCnt >= smuProperty::getOIDListCount())
    {
        IDE_TEST(allocAndLinkOIDNode() != IDE_SUCCESS);
        sCurOIDNode = mOIDNodeListHead.mPrvNode;
    }

    sItemCnt = sCurOIDNode->mOIDCnt;

    sCurOIDNode->mArrOIDInfo[sItemCnt].mTableOID  = aTableOID;
    sCurOIDNode->mArrOIDInfo[sItemCnt].mTargetOID = aTargetOID;
    sCurOIDNode->mArrOIDInfo[sItemCnt].mSpaceID   = aSpaceID;
    sCurOIDNode->mArrOIDInfo[sItemCnt].mFlag      = aFlag;
    sCurOIDNode->mArrOIDInfo[sItemCnt].mSCN       = aSCN;

    IDE_ASSERT( smuHash::insertNode( mUniqueOIDHash,
                                     &aTargetOID,
                                     &(sCurOIDNode->mArrOIDInfo[ sItemCnt ]))
                == IDE_SUCCESS );

    sCurOIDNode->mOIDCnt++;
    mItemCnt++;

    IDE_EXCEPTION_CONT( already_exist );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC smxOIDList::cloneInsertCachedOID()
{
    smxOIDNode *sNewOIDNode;
    UInt     i;

    /* smxOIDList_cloneInsertCachedOID_alloc_NewOIDNode.tc */
    IDU_FIT_POINT("smxOIDList::cloneInsertCachedOID::alloc::NewOIDNode");
    IDE_TEST(alloc(&sNewOIDNode) != IDE_SUCCESS);

    initOIDNode(sNewOIDNode);

    /* 1.move from cached insert oid node to new oid node. */
    for(i = 0; i <mCacheOIDNode4Insert->mOIDCnt; i++)
    {
        sNewOIDNode->mArrOIDInfo[i].mTableOID  =
            mCacheOIDNode4Insert->mArrOIDInfo[i].mTableOID;

        sNewOIDNode->mArrOIDInfo[i].mTargetOID =
            mCacheOIDNode4Insert->mArrOIDInfo[i].mTargetOID;

        sNewOIDNode->mArrOIDInfo[i].mSpaceID =
            mCacheOIDNode4Insert->mArrOIDInfo[i].mSpaceID;

        sNewOIDNode->mArrOIDInfo[i].mFlag =
            mCacheOIDNode4Insert->mArrOIDInfo[i].mFlag;

        sNewOIDNode->mOIDCnt++;
    }//for
    /* 2. link update. */

    sNewOIDNode->mPrvNode =  mCacheOIDNode4Insert->mPrvNode;
    sNewOIDNode->mNxtNode =  mCacheOIDNode4Insert->mNxtNode;

    mCacheOIDNode4Insert->mPrvNode->mNxtNode = sNewOIDNode;
    mCacheOIDNode4Insert->mNxtNode->mPrvNode = sNewOIDNode;
    /* 3. init cached insert oid node. */
    initOIDNode(mCacheOIDNode4Insert);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Transaction�� End�ϱ����� OID List�� ���ؼ� SetSCN�̳� Drop
 *               Table Pending������ �����Ѵ�.
 *
 * aAgingState     - [IN] if Commit, SM_OID_ACT_AGING_COMMIT, else
 *                        SM_OID_ACT_AGING_ROLLBACK.
 * aLSN            - [IN] Commit Log�� Abort Log�� LSN
 * aSCN            - [IN] Row�� Setting�� SCN
 * aProcessOIDOpt  - [IN] if Ager�� OID List�� ó���Ѵٸ�, SMX_OIDLIST_INIT
 *                        else SMX_OIDLIST_DEST
 * aAgingCnt       - [OUT] Aging�� OID ����
 **********************************************************************/
IDE_RC smxOIDList::processOIDList(SInt                   aAgingState,
                                  smLSN                * aLSN,
                                  smSCN                  aSCN,
                                  smxProcessOIDListOpt   aProcessOIDOpt,
                                  ULong                * aAgingCnt,
                                  idBool                 aIsLegacyTrans)
{
    smxOIDNode      *sCurOIDNode;
    UInt             i;
    UInt             sItemCnt;
    smxOIDInfo      *sCurOIDInfo;
    SInt             sFlag;
    ULong            sAgingCnt = 0;
    scSpaceID        sSpaceID;
    smcTableHeader  *sTableHeader;

    IDU_FIT_POINT( "1.BUG-23795@smxOIDList::processOIDList" );

    sCurOIDNode = mOIDNodeListHead.mNxtNode;

    while ( (sCurOIDNode != &mOIDNodeListHead) &&
            (sCurOIDNode != NULL ) )
    {
        sItemCnt    = sCurOIDNode->mOIDCnt;
        sCurOIDInfo = sCurOIDNode->mArrOIDInfo;

        for(i = 0; i < sItemCnt; i++)
        {
            /* ���� OID����Ʈ�� OID�� Aging����̸� Aging������ ���Ѵ�. */
            if( checkIsAgingTarget( aAgingState, sCurOIDInfo + i ) == ID_TRUE )
            {
                sAgingCnt++;
            }
            
            /* PROJ-1381 Fetch Across Commits
             * Legacy Trans�� ������ �� OID List�� ���� pending job�� �����Ѵ�.
             * Commit�� �� ager�� aging ��� OID ������ ����ϹǷ�
             * aging �� OID ����(sAgingCnt)�� ������ �Ѵ�. */
            if( aIsLegacyTrans == ID_TRUE )
            {
                continue;
            }

            if( aAgingState == SM_OID_ACT_AGING_COMMIT )
            {
                IDE_TEST( processOIDListAtCommit( sCurOIDInfo + i,
                                                  aSCN )
                          != IDE_SUCCESS );
            }

            sFlag = sCurOIDInfo[i].mFlag;

            /* Aging�ؾ� �Ѵٸ� */
            if( (sFlag & aAgingState) == aAgingState )
            {
                if((sFlag & SM_OID_TYPE_MASK) == SM_OID_TYPE_DROP_TABLE)
                {
                    IDE_TEST(processDropTblPending(sCurOIDInfo + i)
                             != IDE_SUCCESS);
                }

                /* BUG-16161: Add Column�� ������ �� �ٽ� Add Column�� ����
                 * �ϸ� Session�� Hang���¿� �����ϴ�.: Transaction�� Commit��
                 * �� Abort��Backup������ �����Ѵ�. */
                if( (sFlag & SM_OID_TYPE_MASK) == SM_OID_TYPE_DELETE_TABLE_BACKUP )
                {
                    IDE_TEST( processDeleteBackupFile( aLSN,
                                                       sCurOIDInfo + i ) 
                              != IDE_SUCCESS);

                    /* BUG-31881 
                     * TableBackup�� ���� ������ ������, PageReservation
                     * �� �����Ƿ�, �̸� �����ؾ� �Ѵ�.*/
                    IDE_ASSERT( smcTable::getTableHeaderFromOID(
                                                    sCurOIDInfo[i].mTableOID,
                                                    (void**)&sTableHeader )
                                == IDE_SUCCESS );
                    sSpaceID = smcTable::getSpaceID( sTableHeader );
                    IDE_TEST( smmFPLManager::finalizePageReservation( 
                                                                mTrans,
                                                                sSpaceID )
                              != IDE_SUCCESS );
                    IDE_TEST( svmFPLManager::finalizePageReservation( 
                                                                mTrans,
                                                                sSpaceID )
                             != IDE_SUCCESS );
                }
            }
        }//For

        sCurOIDNode = sCurOIDNode->mNxtNode;
    }//While

    if( aIsLegacyTrans == ID_FALSE )
    {
        if( aProcessOIDOpt == SMX_OIDLIST_INIT )
        {
            /* OID List�� Ager�� ó���ϰ� Free��Ű�� ������
               OID LIst Header�� �ܼ��� �ʱ�ȭ�Ѵ�.*/
            init();
        }
        else
        {
            /* Aging�� �۾��� ����� �Ѵ�. */
            IDE_ASSERT( sAgingCnt == 0 );

            /* OID List�� Ager���� �ѱ��� �ʾұ� ������ ���� Free�Ѵ�.*/
            IDE_ASSERT(aProcessOIDOpt == SMX_OIDLIST_DEST);

            /* Transaction�� Insert���� �ϰ� Commit�� ��쿡��
               Ager�� OID List�� �Ѱܹ��� �ʴ´�.*/
            IDE_ASSERT(aAgingState == SM_OID_ACT_AGING_COMMIT);
            IDE_TEST( freeOIDList() != IDE_SUCCESS);
        }
    }

    *aAgingCnt = sAgingCnt;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/*
   BUG-42760

   LEAGACY Transaction �� ����ɶ� ȣ���ϸ�,
   smxOIDList::processOIDList() �Լ��� �����Ͽ�
   drop�� table�� OID�� SKIP�ϴ� �ڵ带 �߰��Ͽ����ϴ�.
*/
IDE_RC smxOIDList::processOIDList4LegacyTx( smLSN   * aLSN,
                                            smSCN     aSCN )
{
    smxOIDNode      * sCurOIDNode = NULL;
    UInt              i;
    UInt              sItemCnt;
    smxOIDInfo      * sCurOIDInfo = NULL;
    SInt              sFlag;
    ULong             sAgingCnt = 0;
    scSpaceID         sSpaceID;
    smcTableHeader  * sTableHeader = NULL;
    SInt              sAgingState = SM_OID_ACT_AGING_COMMIT;

    sCurOIDNode = mOIDNodeListHead.mNxtNode;

    while ( ( sCurOIDNode != &mOIDNodeListHead ) &&
            ( sCurOIDNode != NULL ) )
    {
        sItemCnt = sCurOIDNode->mOIDCnt;
        sCurOIDInfo = sCurOIDNode->mArrOIDInfo;

        for ( i = 0; i < sItemCnt; i++ )
        {
            IDE_ASSERT( smcTable::getTableHeaderFromOID( sCurOIDInfo[i].mTableOID,
                                                         (void **)&sTableHeader )
                        == IDE_SUCCESS );

            if ( smcTable::isDropedTable(sTableHeader) == ID_TRUE )
            {
                continue;
            }

            IDU_FIT_POINT( "smxOIDList::processOIDList4LegacyTx::isDropedTable::sleep" );

            /* ���� OID����Ʈ�� OID�� Aging����̸� Aging������ ���Ѵ�. */
            if ( checkIsAgingTarget( sAgingState, sCurOIDInfo + i ) == ID_TRUE )
            {
                sAgingCnt++;
            }

            IDE_TEST( processOIDListAtCommit( sCurOIDInfo + i, aSCN )
                      != IDE_SUCCESS );

            sFlag = sCurOIDInfo[i].mFlag;

            /* Aging�ؾ� �Ѵٸ� */
            if ( ( sFlag & sAgingState ) == sAgingState )
            {
                if ( ( sFlag & SM_OID_TYPE_MASK ) == SM_OID_TYPE_DROP_TABLE )
                {
                    IDE_TEST( processDropTblPending( sCurOIDInfo + i )
                              != IDE_SUCCESS);
                }

                /* BUG-16161: Add Column�� ������ �� �ٽ� Add Column�� ����
                 * �ϸ� Session�� Hang���¿� �����ϴ�.: Transaction�� Commit��
                 * �� Abort��Backup������ �����Ѵ�. */
                if ( ( sFlag & SM_OID_TYPE_MASK ) == SM_OID_TYPE_DELETE_TABLE_BACKUP )
                {
                    IDE_TEST( processDeleteBackupFile( aLSN, sCurOIDInfo + i )
                              != IDE_SUCCESS );

                    /* BUG-31881 
                     * TableBackup�� ���� ������ ������, PageReservation
                     * �� �����Ƿ�, �̸� �����ؾ� �Ѵ�.*/
                    sSpaceID = smcTable::getSpaceID( sTableHeader );
                    IDE_TEST( smmFPLManager::finalizePageReservation( mTrans, sSpaceID )
                              != IDE_SUCCESS );
                    IDE_TEST( svmFPLManager::finalizePageReservation( mTrans, sSpaceID )
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

        }//For

        sCurOIDNode = sCurOIDNode->mNxtNode;
    }//While

    /* OID List�� Ager�� ó���ϰ� Free��Ű�� ������
       OID LIst Header�� �ܼ��� �ʱ�ȭ�Ѵ�.*/
    init();

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 * Description : row�� mNext�� SCN���� �����Ѵ�.
 *               slotHeader�� �����ϴ� ���������� logging�� ���� �ʴ´�.
 *               �ֳ��ϸ� ���Լ��� commit�� �� ���Ŀ��� �Ҹ��� �ֱ� �����̴�. commit
 *               ���Ŀ��� ����� �� Ʈ������� �� ������ ���� �α��� ���� �ʴ´�.
 *
 * aOIDInfo - [IN] Delete ������ ����� row������ ������ ����ִ� smxOIDInfo
 * aSCN     - [IN] ���� Ʈ������� commitSCN | deleteBit
 *
 **********************************************************************/
IDE_RC smxOIDList::setSlotNextToSCN(smxOIDInfo* aOIDInfo,
                                    smSCN       aSCN)
{
    smpSlotHeader *sSlotHeader;

    IDE_ASSERT( smmManager::getOIDPtr( aOIDInfo->mSpaceID,
                                       aOIDInfo->mTargetOID,
                                       (void**)&sSlotHeader )
                == IDE_SUCCESS );
    if ( sctTableSpaceMgr::isVolatileTableSpace(aOIDInfo->mSpaceID) == ID_TRUE )
    {

        SM_SET_SCN( &(sSlotHeader->mLimitSCN), &aSCN);

    }
    else
    {
        IDE_ERROR ( sctTableSpaceMgr::isMemTableSpace(aOIDInfo->mSpaceID) == ID_TRUE );

        IDE_TEST(smcRecord::setRowNextToSCN( aOIDInfo->mSpaceID,
                                             (SChar*)sSlotHeader,
                                             aSCN)
                 != IDE_SUCCESS);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC smxOIDList::processOIDListAtCommit( smxOIDInfo* aOIDInfo,
                                           smSCN       aSCN )
{
    SInt             sFlag;
    UInt             sOPFlag;
    smpSlotHeader  * sSmpSlotHeader;
    SInt             sActFlag;
    SChar          * sRowPtr = NULL;
    smcTableHeader * sTableHeader;

    sFlag = aOIDInfo->mFlag;

    sActFlag = sFlag & SM_OID_ACT_COMMIT;
    sOPFlag  = sFlag & SM_OID_OP_MASK;

    if( sActFlag == SM_OID_ACT_COMMIT )
    {
        switch(sOPFlag)
        {
            case SM_OID_OP_DELETED_SLOT:
                SM_SET_SCN_DELETE_BIT(&aSCN);

                IDE_TEST( setSlotNextToSCN(aOIDInfo, aSCN) != IDE_SUCCESS );

                break;

            case SM_OID_OP_DDL_TABLE:
                /* Tablespace�� DDL�� �߻��ߴٴ� �˸��� ���� TableSpace��
                   DDL SCN�� �����Ѵ�.*/
                sctTableSpaceMgr::updateTblDDLCommitSCN( aOIDInfo->mSpaceID,
                                                         aSCN );

                break;

            case SM_OID_OP_NEW_FIXED_SLOT:
            case SM_OID_OP_CHANGED_FIXED_SLOT:
                IDE_ASSERT(aOIDInfo->mTargetOID != 0);

                /* PROJ-1594 Volatile TBS
                 * ó���� OID�� volatile TBS�� ���� ������ svc ����� ȣ���ؾ� �Ѵ�.*/
                IDE_ERROR( smmManager::getOIDPtr( aOIDInfo->mSpaceID,
                                                  aOIDInfo->mTargetOID,
                                                  (void**)&sRowPtr )
                               == IDE_SUCCESS );
                if ( sctTableSpaceMgr::isVolatileTableSpace(aOIDInfo->mSpaceID) == ID_TRUE )
                {
                    IDE_TEST( svcRecord::setSCN( (SChar*)sRowPtr,
                                                 aSCN )
                              != IDE_SUCCESS );
                }
                else
                {
                    IDE_ERROR ( sctTableSpaceMgr::isMemTableSpace(aOIDInfo->mSpaceID) == ID_TRUE );
                    
                    /* PROJ-2429 Dictionary based data compress for on-disk DB */
                    if ( (sFlag & SM_OID_ACT_COMPRESSION) != SM_OID_ACT_COMPRESSION )
                    {
                        IDE_TEST( smcRecord::setSCN( aOIDInfo->mSpaceID,
                                                     (SChar*)sRowPtr,
                                                     aSCN )
                                  != IDE_SUCCESS );
                    }
                    else
                    {
                        /* nothing to do */
                    }
                }
                break;

            case SM_OID_OP_UNLOCK_FIXED_SLOT:
                /* PROJ-1594 Volatile TBS
                   ó���� OID�� volatile TBS�� ���� ������ svc ����� ȣ���ؾ� �Ѵ�.*/
                IDE_ERROR( smmManager::getOIDPtr( aOIDInfo->mSpaceID,
                                                  aOIDInfo->mTargetOID,
                                                  (void**)&sRowPtr )
                           == IDE_SUCCESS );
                if ( sctTableSpaceMgr::isVolatileTableSpace(aOIDInfo->mSpaceID) == ID_TRUE )
                {
                    IDE_TEST( svcRecord::unlockRow( mTrans,
                                                    (SChar*)sRowPtr )
                              != IDE_SUCCESS );
                }
                else
                {
                    IDE_ERROR ( sctTableSpaceMgr::isMemTableSpace(aOIDInfo->mSpaceID) == ID_TRUE );

                    IDE_TEST( smcRecord::unlockRow( mTrans,
                                                    aOIDInfo->mSpaceID,
                                                    (SChar*)sRowPtr )
                              != IDE_SUCCESS );
                }
                break;

            case SM_OID_OP_DROP_TABLE:
                IDE_ERROR( smmManager::getOIDPtr( SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                                                  aOIDInfo->mTableOID,
                                                  (void**)&sSmpSlotHeader )
                           == IDE_SUCCESS );
                IDE_TEST( smcRecord::setDeleteBitOnHeader(
                                                  SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                                                  sSmpSlotHeader )
                          != IDE_SUCCESS );

                sTableHeader = (smcTableHeader *)( (smpSlotHeader*)sSmpSlotHeader + 1);

                IDE_TEST( smcTable::setTableCreateSCN( sTableHeader, SM_SCN_INFINITE )
                          != IDE_SUCCESS );

                break;

            case SM_OID_OP_DROP_INDEX:
                /* xxx */
                IDE_ERROR( smcTable::getTableHeaderFromOID( aOIDInfo->mTableOID,
                                                            (void**)&sTableHeader )
                            == IDE_SUCCESS );
                IDE_TEST( smcTable::dropIndexList( sTableHeader ) != IDE_SUCCESS );
                break;

            case SM_OID_OP_DROP_TABLE_BY_ABORT:
                IDE_ERROR( smmManager::getOIDPtr( SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                                                  aOIDInfo->mTargetOID,
                                                  (void**)&sRowPtr )
                           == IDE_SUCCESS );
                IDE_TEST( smcRecord::setSCN( aOIDInfo->mSpaceID,
                                             (SChar *)sRowPtr,
                                             aSCN )
                          != IDE_SUCCESS );

                sTableHeader = (smcTableHeader *)( (smpSlotHeader*)sRowPtr + 1);

                IDE_TEST( smcTable::setTableCreateSCN( sTableHeader, SM_SCN_INFINITE )
                          != IDE_SUCCESS );

                break;

            case SM_OID_OP_OLD_FIXED_SLOT:
                IDE_TEST( setSlotNextToSCN(aOIDInfo, aSCN) != IDE_SUCCESS );
                break;

            case SM_OID_OP_ALL_INDEX_DISABLE:
                IDE_TEST( processAllIndexDisablePending(aOIDInfo) != IDE_SUCCESS );
                break;

            case SM_OID_OP_CREATE_TABLE:
                IDE_ERROR( aOIDInfo->mTargetOID != 0 );

                IDE_ERROR( smcTable::getTableHeaderFromOID( aOIDInfo->mTableOID,
                                                            (void**)&sTableHeader )
                            == IDE_SUCCESS );

                IDE_TEST( smcTable::setTableCreateSCN( sTableHeader, aSCN )
                          != IDE_SUCCESS );
                break;

            default:
                break;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Alter Table Add Column, Drop Column�� �߻��� Backup����
 *               �� �����Ѵ�.
 *
 * aLSN     - [IN] Commit Log�� Abort�α��� LSN
 * aOIDInfo - [IN] OID Information
 *
 **********************************************************************/
IDE_RC smxOIDList::processDeleteBackupFile(smLSN*      aLSN,
                                           smxOIDInfo *aOIDInfo)
{
    SChar sStrFileName[SM_MAX_FILE_NAME];

    /* Abort�� Commit�α׸� ��ũ�� ������ �����Ŀ�
     * Backup���� ������ �����Ͽ��� �Ѵ�. �ֳ��ϸ� Backup File��
     * �����ߴµ� Commit�αװ� ��ϵ��� �ʾƼ� Transaction�� Abort�ؾ��ϴ�
     * ��찡 ����⶧���̴�. */
    IDE_TEST( smrLogMgr::syncLFThread( SMR_LOG_SYNC_BY_TRX, aLSN )
              != IDE_SUCCESS );

    idlOS::snprintf(sStrFileName,
                    SM_MAX_FILE_NAME,
                    "%s%c%"ID_vULONG_FMT"%s",
                    smcTable::getBackupDir(),
                    IDL_FILE_SEPARATOR,
                    aOIDInfo->mTableOID,
                    SM_TABLE_BACKUP_EXT);

    if ( idf::unlink(sStrFileName) != 0 )
    {
        IDE_TEST_RAISE(idf::access(sStrFileName, F_OK) == 0,
                       err_file_unlink );
    }
    else
    {
        //==========================================================
        // To Fix BUG-13924
        //==========================================================
        ideLog::log(SM_TRC_LOG_LEVEL_MRECOV,
                    SM_TRC_MRECORD_FILE_UNLINK,
                    sStrFileName);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_file_unlink);
    {
        IDE_SET(ideSetErrorCode(smERR_FATAL_FileDelete, sStrFileName));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC smxOIDList::processDropTblPending(smxOIDInfo* aOIDInfo)
{
    smcTableHeader  *sTableHeader;
    smxTrans        *sTrans;
    SInt             sFlag = aOIDInfo->mFlag;

    IDE_ASSERT((sFlag & SM_OID_TYPE_MASK) == SM_OID_TYPE_DROP_TABLE);

    /* BUG-15047: Table�� Commit�Ŀ� Drop Table Pending������
       ������ Return */
    /* Ager�� Drop�� Table�� ���� �����ϴ� ���� ����*/
    smLayerCallback::waitForNoAccessAftDropTbl();

    //added for A4
    // disk table�� ���� dropTable Pending������
    //  disk GC�� �ؾ��Ѵ�.
    IDE_ASSERT( smcTable::getTableHeaderFromOID( aOIDInfo->mTableOID,
                                                 (void**)&sTableHeader )
                == IDE_SUCCESS );

    switch( sTableHeader->mFlag & SMI_TABLE_TYPE_MASK )
    {
        case SMI_TABLE_META: /* BUG-38698 */
        case SMI_TABLE_MEMORY:
        case SMI_TABLE_VOLATILE:
        case SMI_TABLE_DISK:
        {
            IDE_ASSERT( smxTransMgr::alloc(&sTrans) == IDE_SUCCESS );
            IDE_ASSERT( sTrans->begin( NULL,
                                       ( SMI_TRANSACTION_REPL_NONE |
                                         SMI_COMMIT_WRITE_NOWAIT ),
                                       SMX_NOT_REPL_TX_ID )
                        == IDE_SUCCESS);

            IDE_ASSERT( smcTable::dropTablePending( NULL,
                                                    sTrans,
                                                    sTableHeader)
                        == IDE_SUCCESS);
            IDE_ASSERT( sTrans->commit() == IDE_SUCCESS );
            IDE_ASSERT( smxTransMgr::freeTrans(sTrans) == IDE_SUCCESS);

            /* Drop Table������ ����Ǿ��� ������ OID Flag Off��Ű��
               Ager���� �̷� ������ �� ó���Ǵ����� check�ϱ�����
               IDE_ASSERT�� Ȯ���Ѵ�.*/
            /* BUG-32237 [sm_transaction] Free lock node when dropping table.
             * DropTablePending �ܰ迡���� freeLockNode�� �� �� �����ϴ�.
             * Commit�ܰ��̱� ������, Lock�� Ǯ���ִ� �۾��� �ؾ� �ϱ� �����Դϴ�.
             * ���� Ager�� �� Flag�� �ѵΰ�, Ager���� LockNode�� Free�ϰ�
             * ���ݴϴ�. */
        }
    }

    return IDE_SUCCESS;
}

/*******************************************************************************
 * Description: ALL INDEX DISABLE ������ pending ������ ó���ϴ� �Լ�.
 *
 * Related Issues:
 *      PROJ-2184 RP Sync ���� ���
 *
 * aOIDInfo     - [IN] SM_OID_OP_ALL_INDEX_DISABLE Ÿ���� ���� smxOIDInfo
 *****************************************************************************/
IDE_RC smxOIDList::processAllIndexDisablePending( smxOIDInfo  * aOIDInfo )
{
    smcTableHeader  *sTableHeader;
    smxTrans        *sTrans;
    SInt             sFlag = aOIDInfo->mFlag;

    IDE_ASSERT( (sFlag & SM_OID_OP_MASK) == SM_OID_OP_ALL_INDEX_DISABLE );


    /* Ager�� Drop�� Table�� index�� ���� �����ϴ� ���� ����*/
    smLayerCallback::waitForNoAccessAftDropTbl();

    IDE_ASSERT( smcTable::getTableHeaderFromOID( aOIDInfo->mTableOID,
                                                 (void**)&sTableHeader )
                == IDE_SUCCESS );

    IDE_ASSERT( smxTransMgr::alloc(&sTrans) == IDE_SUCCESS );
    IDE_ASSERT( sTrans->begin( NULL,
                               ( SMI_TRANSACTION_REPL_NONE |
                                 SMI_COMMIT_WRITE_NOWAIT ),
                               SMX_NOT_REPL_TX_ID )
                == IDE_SUCCESS );

    IDE_ASSERT( smnManager::disableAllIndex( NULL,
                                             sTrans,
                                             sTableHeader )
                == IDE_SUCCESS );

    IDE_ASSERT( sTrans->commit() == IDE_SUCCESS );
    IDE_ASSERT( smxTransMgr::freeTrans(sTrans) == IDE_SUCCESS );

    return IDE_SUCCESS;
}

/***********************************************************************
 *
 * Description : OID�� �ش��ϴ� ��ũ �ε����� Integrity�� Verify �Ѵ�.
 *
 * BUG-27122 Restart Recovery �� Undo Trans�� �����ϴ� �ε����� ����
 * Integrity üũ��� �߰� (__SM_CHECK_DISK_INDEX_INTEGRITY=2)
 *
 * aStatistics  - [IN] �������
 *
 **********************************************************************/
IDE_RC smxOIDList::processOIDListToVerify( idvSQL * aStatistics )
{
    SChar            sStrBuffer[128];
    smxOIDNode     * sCurOIDNode;
    smxOIDInfo     * sCurOIDInfo;
    UInt             i;
    UInt             sItemCnt;
    smnIndexHeader * sIndexHeader;
    idBool           sIsFail = ID_FALSE;

    sCurOIDNode = mOIDNodeListHead.mNxtNode;

    while (sCurOIDNode != &mOIDNodeListHead)
    {
        sItemCnt = sCurOIDNode->mOIDCnt;

        for(i = 0; i < sItemCnt; i++)
        {
            sCurOIDInfo  = &sCurOIDNode->mArrOIDInfo[i];

            IDE_ASSERT( smmManager::getOIDPtr( SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                                               sCurOIDInfo->mTargetOID,
                                               (void**)&sIndexHeader )
                        == IDE_SUCCESS );

            idlOS::snprintf( sStrBuffer,
                             128,
                             "       [TABLE OID: %"ID_vULONG_FMT", "
                             "INDEX ID: %"ID_UINT32_FMT"] ",
                             ((smnIndexHeader*)sIndexHeader)->mTableOID,
                             ((smnIndexHeader*)sIndexHeader)->mId );
            IDE_CALLBACK_SEND_SYM( sStrBuffer );

            sIsFail = ID_FALSE;

            if ( sIndexHeader->mTableOID != sCurOIDInfo->mTableOID )
            {
                idlOS::snprintf( sStrBuffer,
                                 128,
                                 " [FAIL : Invalid TableOID "
                                 "%"ID_vULONG_FMT",%"ID_vULONG_FMT"] ",
                                 ((smnIndexHeader*)sIndexHeader)->mTableOID,
                                 sCurOIDInfo->mTableOID );

                IDE_CALLBACK_SEND_SYM( sStrBuffer );
                sIsFail = ID_TRUE;
            }

            if ( sIndexHeader->mHeader == NULL )
            {
                IDE_CALLBACK_SEND_SYM( " [FAIL : Index Runtime Header is Null]" );
                sIsFail = ID_TRUE;
            }

            if ( sIsFail == ID_TRUE )
            {
                continue; // ���� Warnning�� ���� �����Ѵ�.
            }

            if ( sdnbBTree::verifyIndexIntegrity(
                                  aStatistics,
                                  sIndexHeader->mHeader )
                 != IDE_SUCCESS )
            {
                IDE_CALLBACK_SEND_SYM( " [FAIL]\n" );
            }
            else
            {
                IDE_CALLBACK_SEND_SYM( " [PASS]\n" );
            }
        }

        sCurOIDNode = sCurOIDNode->mNxtNode;
    }

    return IDE_SUCCESS;
}

/* BUG-42724 : XA Ʈ����ǿ� ���� insert/update�� ���ڵ��� OID �÷��׸�
 * �����Ѵ�.
 */ 
IDE_RC smxOIDList::setOIDFlagForInDoubt()
{

    smxOIDNode      *sCurOIDNode;
    UInt             i;
    UInt             sItemCnt;
    UInt             sFlag;
    SChar           *sRowPtr = NULL;
    smpSlotHeader   *sSlotHdr = NULL;

    sCurOIDNode = mOIDNodeListHead.mNxtNode;

    while (sCurOIDNode != &mOIDNodeListHead)
    {
        sItemCnt = sCurOIDNode->mOIDCnt;
        for (i = 0; i < sItemCnt; i++)
        {
            sFlag   = sCurOIDNode->mArrOIDInfo[i].mFlag;

            IDE_ASSERT( smmManager::getOIDPtr( sCurOIDNode->mArrOIDInfo[i].mSpaceID,
                                               sCurOIDNode->mArrOIDInfo[i].mTargetOID,
                                               (void**)&sRowPtr )               
                        == IDE_SUCCESS );

            sSlotHdr = (smpSlotHeader*)sRowPtr;
 
            if ( SM_SCN_IS_FREE_ROW(sSlotHdr->mCreateSCN) == ID_TRUE )
            {
                /* BUG-42724 : insert/update ���� Unique Violation���� ������ ��� ���⼭ �ɸ���
                 * �̹� �����ε� new �����̶�� ���̴�. �̹� �����ε� slot�̹Ƿ� �����Ѵ�.
                 */
                continue;
            }
            else
            {
                /* Nothing to do */
            }

            switch (sFlag)
            {
                case SM_OID_OLD_UPDATE_FIXED_SLOT:
                    if( SM_SCN_IS_FREE_ROW( sSlotHdr->mLimitSCN ) )
                    {
                        /* BUG-42724 : update ���� Unique Violation���� ������ ��� ( ���⼭ �ɸ���
                         * old �����̶�� ���̴�. ) old version�� Aging �ϸ� �ȵȴ�. ���� Ager����
                         * commit �� �����ϵ��� �÷��׸� ������ �־�� �Ѵ�.
                         */
                        sCurOIDNode->mArrOIDInfo[i].mFlag &= ~SM_OID_OLD_UPDATE_FIXED_SLOT;
                    }
                    else
                    {
                        /* Nothing to do */
                    }

                    break;
                
                case SM_OID_XA_INSERT_UPDATE_ROLLBACK:
                    /* BUG-42724 XA���� insert/update�Ǵ� new version�� �ѹ�Ǵ� ��� �ε���
                     * aging�� ���ؼ� ������ SM_OID_ACT_AGING_INDEX �÷��׸� �߰��ؾ� �Ѵ�.
                     */
                    sCurOIDNode->mArrOIDInfo[i].mFlag |= SM_OID_ACT_AGING_INDEX;
                    break;
                    
                default:
                    break;
            }
        }
        sCurOIDNode = sCurOIDNode->mNxtNode;
    }

    return IDE_SUCCESS;
}

IDE_RC smxOIDList::setSCNForInDoubt(smTID aTID)
{

    smxOIDNode      *sCurOIDNode;
    smVCPieceHeader *sVCPieceHeader;
    UInt             i;
    UInt             sItemCnt;
    UInt             sFlag;
    SChar           *sRecord;
    smSCN            sDeletedSCN;
    smSCN            sInfiniteSCN;
    smpSlotHeader   *sSlotHdr;
    
    SM_SET_SCN_INFINITE_AND_TID( &sInfiniteSCN, aTID );
    SM_SET_SCN_INFINITE_AND_TID( &sDeletedSCN, aTID );
    SM_SET_SCN_DELETE_BIT( &sDeletedSCN );

    sCurOIDNode = mOIDNodeListHead.mNxtNode;
    sSlotHdr = NULL;  
    
    while (sCurOIDNode != &mOIDNodeListHead)
    {
        sItemCnt = sCurOIDNode->mOIDCnt;
        for(i = 0; i < sItemCnt; i++)
        {
            sFlag   = sCurOIDNode->mArrOIDInfo[i].mFlag;

            IDE_ASSERT( smmManager::getOIDPtr( sCurOIDNode->mArrOIDInfo[i].mSpaceID,
                                               sCurOIDNode->mArrOIDInfo[i].mTargetOID,
                                               (void**)&sRecord )
                        == IDE_SUCCESS );

            sSlotHdr = (smpSlotHeader*)sRecord;

            /* SCN�� ���Ѵ�� �����Ͽ� record lock�� ȹ���� ���*/
            switch(sFlag)
            {
                case SM_OID_NEW_INSERT_FIXED_SLOT:
                case SM_OID_NEW_UPDATE_FIXED_SLOT:
                    /*
                     * [BUG-26415] XA Ʈ������� Partial Rollback(Unique Volation)�� Prepare
                     *             Ʈ������� �����ϴ� ��� ���� �籸���� �����մϴ�.
                     * : Insert ���� Unique Volation���� ������ ���� ������ Delete Bit��
                     *   �����ؾ� �Ѵ�.
                     */
                    IDE_TEST(smcRecord::setSCN4InDoubtTrans(sCurOIDNode->mArrOIDInfo[i].mSpaceID,
                                                            aTID,
                                                            sRecord)
                             != IDE_SUCCESS);
                    
                    /* BUG-42724 : new ������ delete bit�� �ִٸ� Partial Rollback�Ͽ��� ������
                     * undoALL ������ refine�� ���̴�. restart �Ϸ��� commit/rollback�� ��
                     * �� �÷��׸� ���� SCN�� set�ϰų� �ٽ� aging ���� �ʵ��� OID �÷��� ��Ʈ��
                     *  �����Ѵ�.
                     */   
                    if ( SM_SCN_IS_DELETED( sSlotHdr->mCreateSCN ) )
                    {
                        sCurOIDNode->mArrOIDInfo[i].mFlag &= ~SM_OID_ACT_COMMIT;
                        sCurOIDNode->mArrOIDInfo[i].mFlag &= ~SM_OID_ACT_AGING_ROLLBACK;
                    }
                    else
                    {
                        /* Nothing to do */
                    }

                    break;
                case SM_OID_LOCK_FIXED_SLOT:
                    /*
                     * TO Fix BUG-14596
                     * XA Tx�鿡 ���� redo ���Ŀ� Lock�� �����Ѵ�.
                     */
                    SMP_SLOT_SET_LOCK( ((smpSlotHeader*)sRecord), aTID );
                    break;
                case SM_OID_OLD_VARIABLE_SLOT:
                   /*
                    * record delete�� ��� variable column�� ����
                    * delete flag�� FALSE�� ����
                    */
                    sVCPieceHeader = (smVCPieceHeader *)sRecord;
                    sVCPieceHeader->flag &= ~SM_VCPIECE_FREE_MASK;
                    sVCPieceHeader->flag |= SM_VCPIECE_FREE_NO;
                    break;
                case SM_OID_DELETE_FIXED_SLOT:
                    IDE_TEST( smcRecord::setRowNextToSCN(sCurOIDNode->mArrOIDInfo[i].mSpaceID,
                                                         sRecord,
                                                         sDeletedSCN)
                              != IDE_SUCCESS);
                    break;
                case SM_OID_OLD_UPDATE_FIXED_SLOT:
                    /* BUG-31062 �Ϲ����� Update�� Old Version�� Refine �� Free������
                     * XA Prepare Transaction�� Update�� ����
                     * ���� Commit���� �ʾ����Ƿ� Free�ϸ� �ȵȴ�.
                     * refine�� Free���� �ʵ��� Slot Header Flag�� ǥ���صд�. */
                    SMP_SLOT_SET_SKIP_REFINE( (smpSlotHeader*)sRecord );
                    
                    /* BUG-42724 : free�� �ƴ� ��쿡�� ���Ѵ�� �����Ѵ�. XA������ old version��
                     * limit�� free�� �̹� new�� partial rollback�� old�̴�.
                     */
                    if ( !(SM_SCN_IS_FREE_ROW( sSlotHdr->mLimitSCN )) )
                    {
                        IDE_TEST( smcRecord::setRowNextToSCN(sCurOIDNode->mArrOIDInfo[i].mSpaceID,
                                                             sRecord,
                                                             sInfiniteSCN)
                                  != IDE_SUCCESS);
                    }
                    else
                    {
                        /* Nothing to do */
                    }

                    break;
                default:
                    break;
            }
        }
        sCurOIDNode = sCurOIDNode->mNxtNode;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

void smxOIDList::dump()
{

    smxOIDNode     *sCurOIDNode;
    smxOIDNode     *sNxtOIDNode;
    UInt            sFlag;
    UInt            i;
    SInt            sOIDCnt = 0;

    SChar       sOIDFlag[][100] = {"SM_OID_OLD_FIXED_SLOT",
                                   "SM_OID_CHANGED_FIXED_SLOT",
                                   "SM_OID_NEW_FIXED_SLOT",
                                   "SM_OID_DELETED_SLOT",
                                   "SM_OID_LOCK_FIXED_SLOT",
                                   "SM_OID_OLD_VARIABLE_SLOT",
                                   "SM_OID_NEW_VARIABLE_SLOT",
                                   "SM_OID_DROP_TABLE",
                                   "SM_OID_DROP_TABLE_BY_ABORT",
                                   "SM_OID_DELETE_TABLE_BACKUP",
                                   "SM_OID_DROP_INDEX"};

    sCurOIDNode = mOIDNodeListHead.mNxtNode;
    idlOS::fprintf(stderr, " \n== OID Node Information ==\n");
    while(sCurOIDNode != &mOIDNodeListHead)
    {
        sNxtOIDNode = sCurOIDNode->mNxtNode;
        for (i=0; i<sCurOIDNode->mOIDCnt; i++)
        {
            sFlag = SM_OID_OP_MASK & sCurOIDNode->mArrOIDInfo[i].mFlag;
            idlOS::fprintf(stderr, "TableOid=%-10"ID_vULONG_FMT", recordOid=%-10"ID_vULONG_FMT", flag=%s\n",
                                    sCurOIDNode->mArrOIDInfo[i].mTableOID,
                                    sCurOIDNode->mArrOIDInfo[i].mTargetOID,
                                    sOIDFlag[sFlag]);

            sOIDCnt ++;
        }
        sCurOIDNode = sNxtOIDNode;
    }
    idlOS::fprintf(stderr, " Total Oid Count = %"ID_UINT32_FMT"\n", sOIDCnt);
    idlOS::fprintf(stderr, " ==========================\n");

}
