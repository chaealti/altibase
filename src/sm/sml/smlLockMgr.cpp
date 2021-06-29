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
 * $Id: smlLockMgr.cpp 90597 2021-04-15 01:17:16Z emlee $
 **********************************************************************/
/**************************************************************
 * FILE DESCRIPTION : smlLockMgr.cpp                          *
 * -----------------------------------------------------------*
 �� ��⿡�� �����ϴ� ����� ������ ũ�� 4�����̴�.

 1. lock table
 2. unlock table
 3. record lockó��
 4. dead lock detection


 - lock table
  �⺻������ table�� ��ǥ���� ���� ����� �ϴ� ���� ȣȯ�����ϸ�,
  grant list�� �ް� table lock�� ��� �ǰ�,
  lock conflict�� �߻��ϸ� table lock��� ����Ʈ�� request list
  �� �ް� �Ǹ�, lock waiting table�� ����ϰ� dead lock�˻��Ŀ�
  waiting�ϰ� �ȴ�.

  altibase������ lock  optimization�� ������ ���� �Ͽ�,
  �ڵ尡 �����ϰ� �Ǿ���.

  : grant lock node ������ ���̱� ���Ͽ�  lock node����
    lock slot���Թ� �̿�.
    -> ������ Ʈ�������  table�� ���Ͽ� lock�� ��Ұ�,
      ���� �䱸�ϴ� table lock mode�� ȣȯ�����ϸ�, ���ο�
      grant node�� �����ϰ� grant list�� ���� �ʰ�,
      ���� grant node�� lock mode�� conversion�Ͽ� �����Ѵ�.
    ->lock conflict������,   grant�� lock node�� 1���̰�,
      �װ��� �ٷ� ��  Ʈ������� ���, ���� grant lock node��
      lock mode�� conversion�Ͽ� �����Ѵ�.

  : unlock table�� request list�� �ִ� node��
    grant list���� move�Ǵ� ����� ���̱� ���Ͽ� lock node�ȿ�
    cvs lock node pointer�� �����Ͽ���.
    -> lock conflict �̰� Ʈ������� ������ table�� ���Ͽ� grant��
      lock node�� ������ �ִ� ���, request list�� �� ���ο�
      lock node�� cvs lock�� ������ grant lock node�� pointing
      �ϰ���.
   %���߿� �ٸ� Ʈ������� unlock table�� request�� �־��� lock
   node�� ���� ���ŵ� grant mode�� ȣȯ�����Ҷ� , �� lock node��
   grant list���� move�ϴ� ���, ���� grant lock node�� lock mode
   �� conversion�Ѵ�.

 - unlock table.
   lock node�� grant�Ǿ� �ִ� ��쿡�� ������ ���� �����Ѵ�.
    1> ���ο� table�� ��ǥ���� grant lock mode�� �����Ѵ�.
    2>  grant list���� lock node�� ���Ž�Ų��.
      -> Lock node�ȿ� lock slot�� 2�� �̻��ִ� ��쿡��
       ���ž���.
   lock node�� request�Ǿ� �־��� ��쿡�� request list����
   �����Ѵ�.

   request list�� �ִ� lock node�߿�
   ���� ���ŵ� grant lock mode�� ȣȯ������
   Transaction�� �� ������ ���� �����.
   1.  request list���� lock node����.
   2.  table lock�������� grant lock mode�� ����.
   3.  cvs lock node�� ������,�� lock node��
      grant list���� move�ϴ� ���, ���� grant lock node��
      lock mode �� conversion�Ѵ�.
   4. cvs lock node�� ������ grant list�� lock node add.
   5.  waiting table���� �ڽ��� wainting �ϰ� �ִ� Ʈ�����
       �� ��� ���� clear.
   6.  waiting�ϰ� �ִ� Ʈ������� resume��Ų��.
   7.  lock slot, lock node ���� �õ�.

 - waiting table ǥ��.
   waiting table�� chained matrix�̰�, ������ ���� ǥ���ȴ�.

     T1   T2   T3   T4   T5   T6

  T1                                        |
                                            | record lock
  T2                                        | waiting list
                                            |
  T3                6          USHORT_MAX   |
                                            |
  T4      6                                 |
                                            |
  T5                                        v

  T6     USHORT_MAX
    --------------------------------->
    table lock waiting or transaction waiting list

    T3�� T4, T6�� ���Ͽ� table lock waiting�Ǵ�
    transaction waiting(record lock�� ���)�ϰ� �ִ�.

    T2�� ���Ͽ�  T4,T6�� record lock waiting�ϰ� ������,
    T2�� commit or rollback�ÿ� T4,T6 ���� T2���� ���
    ���¸� clear�ϰ� resume��Ų��.

 -  record lockó��
   recod lock grant, request list�� node�� ����.
   �ٸ� waiting table����  ����Ϸ��� transaction A�� column��
   record lock ��⸦ ��Ͻ�Ű��, transaction A abort,commit��
   �߻��ϸ� �ڽſ��� ��ϵ� record lock ��� list�� ��ȸ�ϸ�
   record lock�����¸� clear�ϰ�, Ʈ����ǵ��� �����.


 - dead lock detection.
  dead lock dectioin�� ������ ���� �ΰ��� ��쿡
  ���Ͽ� �����Ѵ�.

   1. Tx A�� table lock�� conflict�� �߻��Ͽ� request list�� �ް�,
     Tx A�� ���� ���� waiting list�� ����Ҷ�,waiting table����
     Tx A�� ���Ͽ� cycle�� �߻��ϸ� transaction�� abort��Ų��.


   2.Tx A��  record R1�� update�õ��ϴٰ�,
   �ٸ� Tx B�� ���Ͽ� �̹�  active���� record lock�� ����Ҷ�.
    -  Tx B�� record lock ��⿭����  Tx A�� ����ϰ�,
       Tx A�� Tx B�� ������� ����ϰ� ����  waiting table����
       Tx A�� ���Ͽ� cycle�� �߻��ϸ� transaction�� abort��Ų��.


*************************************************************************/
#include <smErrorCode.h>
#include <smDef.h>
#include <smr.h>
#include <smc.h>
#include <sml.h>
#include <smlReq.h>
#include <smu.h>
#include <sct.h>
#include <smx.h>

/*
   �� ȣȯ�� ���
   ���� - ���� �ɷ��ִ� ��Ÿ��
   ���� - ���ο� ��Ÿ��
*/
idBool smlLockMgr::mCompatibleTBL[SML_NUMLOCKTYPES][SML_NUMLOCKTYPES] = {
/*                   SML_NLOCK SML_SLOCK SML_XLOCK SML_ISLOCK SML_IXLOCK SML_SIXLOCK */
/* for SML_NLOCK  */{ID_TRUE,  ID_TRUE,  ID_TRUE,  ID_TRUE,   ID_TRUE,   ID_TRUE},
/* for SML_SLOCK  */{ID_TRUE,  ID_TRUE,  ID_FALSE, ID_TRUE,   ID_FALSE,  ID_FALSE},
/* for SML_XLOCK  */{ID_TRUE,  ID_FALSE, ID_FALSE, ID_FALSE,  ID_FALSE,  ID_FALSE},
/* for SML_ISLOCK */{ID_TRUE,  ID_TRUE,  ID_FALSE, ID_TRUE,   ID_TRUE,   ID_TRUE},
/* for SML_IXLOCK */{ID_TRUE,  ID_FALSE, ID_FALSE, ID_TRUE,   ID_TRUE,   ID_FALSE},
/* for SML_SIXLOCK*/{ID_TRUE,  ID_FALSE, ID_FALSE, ID_TRUE,   ID_FALSE,  ID_FALSE}
};
/*
   �� ��ȯ ���
   ���� - ���� �ɷ��ִ� ��Ÿ��
   ���� - ���ο� ��Ÿ��
*/
smlLockMode smlLockMgr::mConversionTBL[SML_NUMLOCKTYPES][SML_NUMLOCKTYPES] = {
/*                   SML_NLOCK    SML_SLOCK    SML_XLOCK  SML_ISLOCK   SML_IXLOCK   SML_SIXLOCK */
/* for SML_NLOCK  */{SML_NLOCK,   SML_SLOCK,   SML_XLOCK, SML_ISLOCK,  SML_IXLOCK,  SML_SIXLOCK},
/* for SML_SLOCK  */{SML_SLOCK,   SML_SLOCK,   SML_XLOCK, SML_SLOCK,   SML_SIXLOCK, SML_SIXLOCK},
/* for SML_XLOCK  */{SML_XLOCK,   SML_XLOCK,   SML_XLOCK, SML_XLOCK,   SML_XLOCK,   SML_XLOCK},
/* for SML_ISLOCK */{SML_ISLOCK,  SML_SLOCK,   SML_XLOCK, SML_ISLOCK,  SML_IXLOCK,  SML_SIXLOCK},
/* for SML_IXLOCK */{SML_IXLOCK,  SML_SIXLOCK, SML_XLOCK, SML_IXLOCK,  SML_IXLOCK,  SML_SIXLOCK},
/* for SML_SIXLOCK*/{SML_SIXLOCK, SML_SIXLOCK, SML_XLOCK, SML_SIXLOCK, SML_SIXLOCK, SML_SIXLOCK}
};

/*
   �� Mode���� ���̺�
*/
smlLockMode smlLockMgr::mDecisionTBL[SML_DECISION_TBL_SIZE] = {
    SML_NLOCK,   SML_SLOCK,   SML_XLOCK, SML_XLOCK,
    SML_ISLOCK,  SML_SLOCK,   SML_XLOCK, SML_XLOCK,
    SML_IXLOCK,  SML_SIXLOCK, SML_XLOCK, SML_XLOCK,
    SML_IXLOCK,  SML_SIXLOCK, SML_XLOCK, SML_XLOCK,
    SML_SIXLOCK, SML_SIXLOCK, SML_XLOCK, SML_XLOCK,
    SML_SIXLOCK, SML_SIXLOCK, SML_XLOCK, SML_XLOCK,
    SML_SIXLOCK, SML_SIXLOCK, SML_XLOCK, SML_XLOCK,
    SML_SIXLOCK, SML_SIXLOCK, SML_XLOCK, SML_XLOCK
};

/*
   �� Mode�� ���� Lock Mask���� ���̺�
*/
SInt smlLockMgr::mLockModeToMask[SML_NUMLOCKTYPES] = {
    /* for SML_NLOCK  */ 0x00000000,
    /* for SML_SLOCK  */ 0x00000001,
    /* for SML_XLOCK  */ 0x00000002,
    /* for SML_ISLOCK */ 0x00000004,
    /* for SML_IXLOCK */ 0x00000008,
    /* for SML_SIXLOCK*/ 0x00000010
};

smlLockMode2StrTBL smlLockMgr::mLockMode2StrTBL[SML_NUMLOCKTYPES] ={
    {SML_NLOCK,"NO_LOCK"},
    {SML_SLOCK,"S_LOCK"},
    {SML_XLOCK,"X_LOCK"},
    {SML_ISLOCK,"IS_LOCK"},
    {SML_IXLOCK,"IX_LOCK"},
    {SML_SIXLOCK,"SIX_LOCK"}
};

/* PROJ-2734
   mDistDeadlockRisk[WAITER TX][HOLDER TX]

   �ڿ����TX(waiter)�� �ڿ�����TX(holder)�� �л�LEVEL ���պ� �л굥��� ���� �߻� �����.
   �л굥����� Ž���Ǿ����� ���� �л굥����� Ȯ���� �������� HIGH�� �����Ͽ���.
 */
smlDistDeadlockRiskType smlLockMgr::mDistDeadlockRisk[SMI_DIST_LEVEL_MAX][SMI_DIST_LEVEL_MAX] =
{
    /* WAITER TX : SMI_DIST_LEVEL_NONE     */
    { SML_DIST_DEADLOCK_RISK_NONE, SML_DIST_DEADLOCK_RISK_NONE, SML_DIST_DEADLOCK_RISK_NONE, SML_DIST_DEADLOCK_RISK_NONE },

    /* WAITER TX : SMI_DIST_LEVEL_SINGLE   */
    { SML_DIST_DEADLOCK_RISK_NONE, SML_DIST_DEADLOCK_RISK_LOW, SML_DIST_DEADLOCK_RISK_LOW,  SML_DIST_DEADLOCK_RISK_LOW },

    /* WAITER TX : SMI_DIST_LEVEL_MULTI    */
    { SML_DIST_DEADLOCK_RISK_NONE, SML_DIST_DEADLOCK_RISK_LOW, SML_DIST_DEADLOCK_RISK_LOW,  SML_DIST_DEADLOCK_RISK_MID },

    /* WAITER TX : SMI_DIST_LEVEL_PARALLEL */
    { SML_DIST_DEADLOCK_RISK_NONE, SML_DIST_DEADLOCK_RISK_MID, SML_DIST_DEADLOCK_RISK_MID,  SML_DIST_DEADLOCK_RISK_HIGH }
};

SInt                    smlLockMgr::mTransCnt;
iduMemPool              smlLockMgr::mLockPool;
smlLockMatrixItem   **  smlLockMgr::mWaitForTable;


smiLockWaitFunc         smlLockMgr::mLockWaitFunc;
smiLockWakeupFunc       smlLockMgr::mLockWakeupFunc;
smlTransLockList     *  smlLockMgr::mArrOfLockList;
iduMutex             *  smlLockMgr::mArrOfLockListMutex;

smlAllocLockNodeFunc    smlLockMgr::mAllocLockNodeFunc;
smlFreeLockNodeFunc     smlLockMgr::mFreeLockNodeFunc;

smlLockNode         **  smlLockMgr::mNodeCache;
smlLockNode          *  smlLockMgr::mNodeCacheArray;
ULong                *  smlLockMgr::mNodeAllocMap;

/* PROJ-2734 */
smlDistDeadlockNode  ** smlLockMgr::mTxList4DistDeadlock; /* �л굥��� üũ ����� �� Tx */

static IDE_RC smlLockWaitNAFunction( ULong, idBool * )
{
    return IDE_SUCCESS;
}

static IDE_RC smlLockWakeupNAFunction()
{
    return IDE_SUCCESS;
}


IDE_RC smlLockMgr::initialize( UInt              aTransCnt,
                               smiLockWaitFunc   aLockWaitFunc,
                               smiLockWakeupFunc aLockWakeupFunc )
{
    SInt               i;
    SInt               j;
    smlTransLockList * sTransLockList; /* BUG-43408 */
    SChar sBuffer[128];

    IDE_ASSERT( aTransCnt > 0 );

    mTransCnt = aTransCnt;

    /* TC/FIT/Limit/sm/sml/smlLockMgr_initialize_calloc1.sql */
    IDU_FIT_POINT_RAISE( "smlLockMgr::initialize::calloc1",
                          insufficient_memory );

    IDE_TEST( mLockPool.initialize( IDU_MEM_SM_SML,
                                    (SChar*)"LOCK_MEMORY_POOL",
                                    ID_SCALABILITY_SYS,
                                    sizeof(smlLockNode),
                                    SML_LOCK_POOL_SIZE,
                                    IDU_AUTOFREE_CHUNK_LIMIT,           /* ChunkLimit */
                                    ID_TRUE,                            /* UseMutex */
                                    IDU_MEM_POOL_DEFAULT_ALIGN_SIZE,	/* AlignByte */
                                    ID_FALSE,                           /* ForcePooling */
                                    ID_TRUE,                            /* GarbageCollection */
                                    ID_TRUE,                            /* HWCacheLine */
                                    IDU_MEMPOOL_TYPE_LEGACY             /* mempool type */)
              != IDE_SUCCESS);

    // allocate transLock List array.
    mArrOfLockList = NULL;

    /* TC/FIT/Limit/sm/sml/smlLockMgr_initialize_malloc.sql */
    IDU_FIT_POINT_RAISE( "smlLockMgr::initialize::malloc",
                          insufficient_memory );

    IDE_TEST_RAISE( iduMemMgr::malloc( IDU_MEM_SM_SML,
                                       (ULong)sizeof(smlTransLockList) * mTransCnt,
                                       (void**)&mArrOfLockList ) != IDE_SUCCESS,
                    insufficient_memory );

    /* PROJ-2734 */
    mTxList4DistDeadlock = NULL;
    IDE_TEST_RAISE( iduMemMgr::malloc( IDU_MEM_SM_SML,
                                       ID_SIZEOF(smlDistDeadlockNode *) * mTransCnt,
                                       (void**)&mTxList4DistDeadlock ) != IDE_SUCCESS,
                    insufficient_memory );
    for ( i = 0; i < mTransCnt; i++ )
    {
        IDE_TEST_RAISE( iduMemMgr::malloc( IDU_MEM_SM_SML,
                                           ID_SIZEOF(smlDistDeadlockNode) * mTransCnt,
                                           (void**)&(mTxList4DistDeadlock[i]) ) != IDE_SUCCESS,
                        insufficient_memory );
    }

    IDE_TEST_RAISE( iduMemMgr::malloc( IDU_MEM_SM_SML,
                                       (ULong)sizeof(iduMutex) * mTransCnt,
                                       (void**)&mArrOfLockListMutex ) != IDE_SUCCESS,
                    insufficient_memory );

    mLockWaitFunc = ( (aLockWaitFunc == NULL) ?
                      smlLockWaitNAFunction : aLockWaitFunc );

    mLockWakeupFunc = ( (aLockWakeupFunc == NULL) ?
                        smlLockWakeupNAFunction : aLockWakeupFunc );

    for ( i = 0 ; i < mTransCnt ; i++ )
    {
        idlOS::snprintf( sBuffer, 128, "TRANS_LOCK_NODE_LIST_MUTEX_%"ID_INT32_FMT"\0", i );
        IDE_TEST( mArrOfLockListMutex[i].initialize( sBuffer,
                                                     IDU_MUTEX_KIND_NATIVE,
                                                     IDV_WAIT_INDEX_NULL ) != IDE_SUCCESS );
        initTransLockList(i);
    }

    switch( smuProperty::getLockMgrCacheNode() )
    {
    case 0:
        mAllocLockNodeFunc  = allocLockNodeNormal;
        mFreeLockNodeFunc   = freeLockNodeNormal;
        break;

    case 1:
        for ( i = 0 ; i < mTransCnt ; i++ )
        {
            sTransLockList = (mArrOfLockList+i);
            IDE_TEST_RAISE( initTransLockNodeCache( i,
                                                    &( sTransLockList->mLockNodeCache ) )
                            != IDE_SUCCESS,
                            insufficient_memory );
        }

        mAllocLockNodeFunc  = allocLockNodeList;
        mFreeLockNodeFunc   = freeLockNodeList;
        break;

    case 2:
        IDE_TEST_RAISE( iduMemMgr::malloc( IDU_MEM_SM_SML,
                                           sizeof(smlLockNode) * mTransCnt * 64,
                                           (void**)&mNodeCacheArray ) != IDE_SUCCESS,
                        insufficient_memory );
        IDE_TEST_RAISE( iduMemMgr::malloc( IDU_MEM_SM_SML,
                                           sizeof(smlLockNode*) * mTransCnt,
                                           (void**)&mNodeCache ) != IDE_SUCCESS,
                        insufficient_memory );
        IDE_TEST_RAISE( iduMemMgr::calloc( IDU_MEM_SM_SML,
                                           mTransCnt,
                                           sizeof(ULong),
                                           (void**)&mNodeAllocMap ) != IDE_SUCCESS,
                        insufficient_memory );

        for ( i = 0 ; i < mTransCnt ; i++ )
        {
            mNodeCache[i] = &(mNodeCacheArray[i * 64]);
        }

        mAllocLockNodeFunc  = allocLockNodeBitmap;
        mFreeLockNodeFunc   = freeLockNodeBitmap;

        break;

    }

    //Alloc wait table
    IDE_TEST_RAISE( iduMemMgr::calloc( IDU_MEM_SM_SML,
                                       mTransCnt,
                                       ID_SIZEOF(smlLockMatrixItem *),
                                       (void**)&mWaitForTable ) != IDE_SUCCESS,
                    insufficient_memory );

    for ( i = 0 ; i < mTransCnt ; i++ )
    {
        IDE_TEST_RAISE( iduMemMgr::calloc( IDU_MEM_SM_SML,
                                           mTransCnt,
                                           ID_SIZEOF(smlLockMatrixItem),
                                           (void**)&(mWaitForTable[i] ) ) != IDE_SUCCESS,
                        insufficient_memory );


    }

    for ( i = 0 ; i < mTransCnt ; i++ )
    {
        for ( j = 0 ; j < mTransCnt ; j++ )
        {
            mWaitForTable[i][j].mIndex = 0;
            mWaitForTable[i][j].mNxtWaitTransItem = ID_USHORT_MAX;
            mWaitForTable[i][j].mNxtWaitRecTransItem = ID_USHORT_MAX;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( insufficient_memory );
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_InsufficientMemory));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* BUG-43408 */
IDE_RC smlLockMgr::initTransLockNodeCache( SInt      aSlotID,
                                           iduList * aTxSlotLockNodeBitmap )
{
    smlLockNode * sLockNode;
    UInt          i;
    UInt          sLockNodeCacheCnt;

    /* BUG-43408 Property�� �ʱ� �Ҵ� ������ ���� */
    sLockNodeCacheCnt = smuProperty::getLockNodeCacheCount();

    IDU_LIST_INIT( aTxSlotLockNodeBitmap );

    for ( i = 0 ; i < sLockNodeCacheCnt ; i++ )
    {
        IDE_TEST( mLockPool.alloc( (void**)&sLockNode )
                  != IDE_SUCCESS );
        sLockNode->mSlotID = aSlotID;
        IDU_LIST_INIT_OBJ( &(sLockNode->mNode4LockNodeCache), sLockNode );
        IDU_LIST_ADD_AFTER( aTxSlotLockNodeBitmap, &(sLockNode->mNode4LockNodeCache) );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC smlLockMgr::destroy()
{
    iduListNode*        sIterator = NULL;
    iduListNode*        sNodeNext = NULL;
    iduList*            sLockNodeCache;
    smlLockNode*        sLockNode;
    smlTransLockList*   sTransLockList;
    SInt                i;

    for ( i = 0 ; i < mTransCnt ; i++ )
    {
        IDE_TEST( iduMemMgr::free(mWaitForTable[i]) != IDE_SUCCESS );
    }

    IDE_TEST( iduMemMgr::free(mWaitForTable) != IDE_SUCCESS );

    switch(smuProperty::getLockMgrCacheNode())
    {
    case 0:
        /* do nothing */
        break;

    case 1:
        for ( i = 0 ; i < mTransCnt ; i++ )
        {
            sTransLockList = (mArrOfLockList+i);
            sLockNodeCache = &(sTransLockList->mLockNodeCache);
            IDU_LIST_ITERATE_SAFE( sLockNodeCache, sIterator, sNodeNext )
            {
                sLockNode = (smlLockNode*) (sIterator->mObj);
                IDE_TEST( mLockPool.memfree(sLockNode) != IDE_SUCCESS );
            }
        }
        break;

    case 2:
        IDE_TEST( iduMemMgr::free(mNodeCacheArray) != IDE_SUCCESS );
        IDE_TEST( iduMemMgr::free(mNodeCache) != IDE_SUCCESS );
        IDE_TEST( iduMemMgr::free(mNodeAllocMap) != IDE_SUCCESS );
        break;
    }

    IDE_TEST( mLockPool.destroy() != IDE_SUCCESS );

    mLockWaitFunc = NULL;

    for ( i = 0 ; i < mTransCnt ; i++ )
    {
        IDE_TEST( mArrOfLockListMutex[i].destroy() != IDE_SUCCESS );
    }
    IDE_TEST( iduMemMgr::free( mArrOfLockListMutex ) != IDE_SUCCESS );
    IDE_TEST( iduMemMgr::free( mArrOfLockList ) != IDE_SUCCESS );

    /* PROJ-2734 */
    for ( i = 0 ; i < mTransCnt ; i++ )
    {
        IDE_TEST( iduMemMgr::free(mTxList4DistDeadlock[i]) != IDE_SUCCESS );
    }

    IDE_TEST( iduMemMgr::free( mTxList4DistDeadlock ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************
  function description: initTransLockList
  Transaction lock list array���� aSlot�� �ش��ϴ�
  smlTransLockList�� �ʱ�ȭ�� �Ѵ�.
  - Tx�� ��ó�� table lock ��� item
  - Tx�� ��ó�� record lock��� item.
  - Tx�� lock node�� list �ʱ�ȭ.
  - Tx�� lock slot list�ʱ�ȭ.
***********************************************************/
void  smlLockMgr::initTransLockList( SInt aSlot )
{
    smlTransLockList * sTransLockList = (mArrOfLockList+aSlot);

    sTransLockList->mFstWaitTblTransItem  = SML_END_ITEM;
    sTransLockList->mFstWaitRecTransItem  = SML_END_ITEM;
    sTransLockList->mLstWaitRecTransItem  = SML_END_ITEM;

    sTransLockList->mLockSlotHeader.mPrvLockSlot  = &(sTransLockList->mLockSlotHeader);
    sTransLockList->mLockSlotHeader.mNxtLockSlot  = &(sTransLockList->mLockSlotHeader);

    sTransLockList->mLockSlotHeader.mLockNode     = NULL;
    sTransLockList->mLockSlotHeader.mLockSequence = 0;

    sTransLockList->mLockNodeHeader.mPrvTransLockNode = &(sTransLockList->mLockNodeHeader);
    sTransLockList->mLockNodeHeader.mNxtTransLockNode = &(sTransLockList->mLockNodeHeader);
}


void smlLockMgr::getTxLockInfo( SInt aSlot, smTID *aOwnerList, UInt *aOwnerCount )
{

    UInt      i;
    void*   sTrans;

    if ( aSlot >= 0 )
    {

        for ( i = mArrOfLockList[aSlot].mFstWaitTblTransItem;
              (i != SML_END_ITEM) && (i != ID_USHORT_MAX);
              i = mWaitForTable[aSlot][i].mNxtWaitTransItem )
        {
            if ( mWaitForTable[aSlot][i].mIndex == 1 )
            {
                sTrans = smLayerCallback::getTransBySID( i );
                aOwnerList[*aOwnerCount] = smLayerCallback::getTransID( sTrans );
                (*aOwnerCount)++;
            }
            else
            {
                /* nothing to do */
            }
        }
    }
}

/*********************************************************
  function description: addLockNode
  Ʈ������� table�� ���Ͽ� lock�� ��� �Ǵ� ���,
  �� table�� grant list  lock node��  �ڽ��� Ʈ�����
  lock list�� �߰��Ѵ�.

  lock�� ��� �Ǵ� ���� �Ʒ��� �ΰ��� ����̴�.
  1. Ʈ������� table�� ���Ͽ� lock�� grant�Ǿ�
     lock�� ��� �Ǵ� ���.
  2. lock waiting�ϰ� �ִٰ� lock�� ��� �־���  �ٸ� Ʈ�����
     �� commit or rollback�� �����Ͽ�,  wakeup�Ǵ� ���.
***********************************************************/
void smlLockMgr::addLockNode( smlLockNode *aLockNode, SInt aSlot )
{
    smlLockNode*  sTransLockNodeHdr = &(mArrOfLockList[aSlot].mLockNodeHeader);

    IDE_ASSERT( NULL == aLockNode->mPrvTransLockNode );
    IDE_ASSERT( NULL == aLockNode->mNxtTransLockNode );
    IDE_ASSERT( ID_FALSE == aLockNode->mDoRemove );
    IDE_ASSERT( aLockNode->mBeGrant  == ID_TRUE );

    IDE_DASSERT( isLockNodeExist( aLockNode ) == ID_FALSE );

    aLockNode->mNxtTransLockNode = sTransLockNodeHdr;
    aLockNode->mPrvTransLockNode = sTransLockNodeHdr->mPrvTransLockNode;

    sTransLockNodeHdr->mPrvTransLockNode->mNxtTransLockNode = aLockNode;
    sTransLockNodeHdr->mPrvTransLockNode  = aLockNode;

    IDE_DASSERT( isLockNodeExist( aLockNode ) == ID_TRUE );
    IDE_ASSERT( NULL != aLockNode->mPrvTransLockNode );
    IDE_ASSERT( NULL != aLockNode->mNxtTransLockNode );
    IDE_ASSERT( aLockNode != aLockNode->mPrvTransLockNode );
    IDE_ASSERT( aLockNode != aLockNode->mNxtTransLockNode );
    IDE_ASSERT( aLockNode->mNxtTransLockNode->mPrvTransLockNode == aLockNode );
    IDE_ASSERT( aLockNode->mPrvTransLockNode->mNxtTransLockNode == aLockNode );
    IDE_ASSERT( aLockNode->mNxtTransLockNode->mPrvTransLockNode != aLockNode->mPrvTransLockNode );
    IDE_ASSERT( aLockNode->mPrvTransLockNode->mNxtTransLockNode != aLockNode->mNxtTransLockNode );
}

/*********************************************************
  function description: removeLockNode
  Ʈ����� lock list array����  transaction�� slotid��
  �ش��ϴ� list���� lock node�� �����Ѵ�.
***********************************************************/
void smlLockMgr::removeLockNode( smlLockNode *aLockNode )
{
    IDE_DASSERT( isLockNodeExist( aLockNode ) == ID_TRUE );
    IDE_ASSERT( aLockNode->mDoRemove == ID_FALSE );
    IDE_ASSERT( aLockNode->mBeGrant  == ID_TRUE );
    IDE_ASSERT( NULL != aLockNode->mPrvTransLockNode );
    IDE_ASSERT( NULL != aLockNode->mNxtTransLockNode );
    IDE_ASSERT( aLockNode != aLockNode->mPrvTransLockNode );
    IDE_ASSERT( aLockNode != aLockNode->mNxtTransLockNode );
    IDE_ASSERT( aLockNode->mNxtTransLockNode->mPrvTransLockNode == aLockNode );
    IDE_ASSERT( aLockNode->mPrvTransLockNode->mNxtTransLockNode == aLockNode );

    IDE_ASSERT( aLockNode->mNxtTransLockNode->mPrvTransLockNode != aLockNode->mPrvTransLockNode );
    IDE_ASSERT( aLockNode->mPrvTransLockNode->mNxtTransLockNode != aLockNode->mNxtTransLockNode );

    aLockNode->mNxtTransLockNode->mPrvTransLockNode = aLockNode->mPrvTransLockNode;
    aLockNode->mPrvTransLockNode->mNxtTransLockNode = aLockNode->mNxtTransLockNode;

    aLockNode->mPrvTransLockNode = NULL;
    aLockNode->mNxtTransLockNode = NULL;

    IDE_DASSERT( isLockNodeExist( aLockNode ) == ID_FALSE );
}

/*********************************************************
  PROJ-1381 Fetch Across Commits
  function description: freeAllItemLockExceptIS
  aSlot id�� �ش��ϴ� Ʈ������� grant�Ǿ� lock�� ���
  �ִ� lock�� IS lock�� �����ϰ� �����Ѵ�.

  ���� �������� ��Ҵ� lock���� lock�� �����Ѵ�.
***********************************************************/
IDE_RC smlLockMgr::freeAllItemLockExceptIS( SInt aSlot )
{
    smlLockSlot *sCurLockSlot;
    smlLockSlot *sPrvLockSlot;
    smlLockSlot *sHeadLockSlot;
    static SInt  sISLockMask = smlLockMgr::mLockModeToMask[SML_ISLOCK];

    sHeadLockSlot = &(mArrOfLockList[aSlot].mLockSlotHeader);

    sCurLockSlot = (smlLockSlot*)getLastLockSlotPtr(aSlot);

    while ( sCurLockSlot != sHeadLockSlot )
    {
        sPrvLockSlot = sCurLockSlot->mPrvLockSlot;

        if ( sISLockMask != sCurLockSlot->mMask ) /* IS Lock�� �ƴϸ� */
        {
            IDE_TEST( smlLockMgr::unlockTable(aSlot, NULL, sCurLockSlot)
                      != IDE_SUCCESS );
        }
        else
        {
            /* IS Lock�� ��� ��� fetch�� �����ϱ� ���ؼ� ���ܵд�. */
        }

        sCurLockSlot = sPrvLockSlot;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************
  function description: freeAllItemLock
  aSlot id�� �ش��ϴ� Ʈ������� grant�Ǿ� lock�� ���
  �ִ� lock�� �� �����Ѵ�.

  lock ���� ������ ���� �������� ��Ҵ� lock���� Ǯ��
  �����Ѵ�.
*******************m***************************************/
IDE_RC smlLockMgr::freeAllItemLock( SInt aSlot )
{

    smlLockNode *sCurLockNode;
    smlLockNode *sPrvLockNode;

    smlLockNode * sTransLockNodeHdr = &(mArrOfLockList[aSlot].mLockNodeHeader);

    sCurLockNode = sTransLockNodeHdr->mPrvTransLockNode;

    while( sCurLockNode != sTransLockNodeHdr )
    {
        sPrvLockNode = sCurLockNode->mPrvTransLockNode;


        IDE_TEST( smlLockMgr::unlockTable( aSlot, sCurLockNode, NULL )
                  != IDE_SUCCESS );
        sCurLockNode = sPrvLockNode;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************
  function description: partialTableUnlock
  Ʈ����� aSlot�� �ش��ϴ� lockSlot list����
  ���� ������ lock slot����  aLockSlot���� ���ٷ� �ö󰡸鼭
  aLockSlot�� �����ִ� lock node�� �̿��Ͽ� table unlock��
  �����Ѵ�. �̶� Lock Slot�� Sequence �� aLockSequence����
  Ŭ�������� Unlock�� �����Ѵ�.

  aSlot          - [IN] aSlot�� �ش��ϴ� Transaction����
  aLockSequence  - [IN] LockSlot�� mSequence�� ���� aLockSequence
                        ���� ����������  Unlock�� �����Ѵ�.
  aIsSeveralLock  - [IN] ID_FALSE:��� Lock�� ����.
                         ID_TRUE :Implicit Savepoint���� ��� ���̺� ����
                                  IS���� �����ϰ�,temp tbs�ϰ��� IX��������.

***********************************************************/
IDE_RC smlLockMgr::partialItemUnlock( SInt   aSlot,
                                      ULong  aLockSequence,
                                      idBool aIsSeveralLock )
{
    smlLockSlot *sCurLockSlot;
    smlLockSlot *sPrvLockNode;
    static SInt  sISLockMask = smlLockMgr::mLockModeToMask[SML_ISLOCK];
    static SInt  sIXLockMask = smlLockMgr::mLockModeToMask[SML_IXLOCK];
    scSpaceID    sSpaceID;
    IDE_RC       sRet = IDE_SUCCESS;

    sCurLockSlot = (smlLockSlot*)getLastLockSlotPtr(aSlot);

    while ( sCurLockSlot->mLockSequence > aLockSequence )
    {
        sPrvLockNode = sCurLockSlot->mPrvLockSlot;

        if ( aIsSeveralLock == ID_FALSE )
        {
            // Abort Savepoint���� �Ϲ����� ��� Partial Unlock
            // Lock�� ������ ������� LockSequence�� ������ �����Ͽ� ��� unlock
            IDE_TEST( smlLockMgr::unlockTable(aSlot, NULL, sCurLockSlot)
                      != IDE_SUCCESS );
        }
        else
        {
            // Statement End�� ȣ��Ǵ� ���
            // Implicit IS Lock��, TableTable�� IX Lock�� Unlock

            if ( sISLockMask == sCurLockSlot->mMask ) // IS Lock�� ���
            {
                /* BUG-15906: non-autocommit��忡�� select�Ϸ��� IS_LOCK�� �����Ǹ�
                   ���ڽ��ϴ�.
                   aPartialLock�� ID_TRUE�̸� IS_LOCK���� �����ϵ��� ��. */
                // BUG-28752 lock table ... in row share mode ������ ������ �ʽ��ϴ�.
                // Implicit IS lock�� Ǯ���ݴϴ�.

                if ( sCurLockSlot->mLockNode->mIsExplicitLock != ID_TRUE )
                {
                    IDE_TEST( smlLockMgr::unlockTable(aSlot, NULL, sCurLockSlot)
                              != IDE_SUCCESS );
                }
            }
            else if ( sIXLockMask == sCurLockSlot->mMask ) //IX Lock�� ���
            {
                /* BUG-21743
                 * Select ���꿡�� User Temp TBS ���� TBS�� Lock�� ��Ǯ���� ���� */
                sSpaceID = sCurLockSlot->mLockNode->mSpaceID;

                if ( sctTableSpaceMgr::isTempTableSpace( sSpaceID ) == ID_TRUE )
                {
                    IDE_TEST( smlLockMgr::unlockTable(aSlot, NULL, sCurLockSlot)
                              != IDE_SUCCESS );
                }
            }
        }

        sCurLockSlot = sPrvLockNode;
    }

    return sRet;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/* BUG-18981 */
IDE_RC  smlLockMgr::logLocks( smxTrans * aTrans, ID_XID * aXID )
{
    smrXaPrepareLog  *sPrepareLog;
    smlTableLockInfo  sLockInfo;
    SChar            *sLogBuffer;
    SChar            *sLog;
    UInt              sLockCount = 0;
    idBool            sIsLogged = ID_FALSE;
    smlLockNode      *sCurLockNode;
    smlLockNode      *sPrvLockNode;
    PDL_Time_Value    sTmv;
    smLSN             sEndLSN;
    SInt              sNeedBufferSize;

#ifdef DEBUG
    UChar             sXidString[SMR_XID_DATA_MAX_LEN];

    (void)idaXaConvertXIDToString(NULL, aXID, sXidString, SMR_XID_DATA_MAX_LEN);

    ideLog::log( IDE_SD_19,
                 "\n<SMR_LT_XA_PREPARE Logging> "
                 "LocalID: %"ID_UINT32_FMT", "
                 "XID: %s, "
                 "GCTX: %"ID_INT32_FMT"\n",
                 aTrans->mTransID,
                 sXidString,
                 aTrans->mIsGCTx );
#endif

    /* -------------------------------------------------------------------
       BUG-48307 : LOG BUFFER ũ�Ⱑ ������� Ȯ���ϰ�, ������ ������Ų��.
       ------------------------------------------------------------------- */
    /* 1. LOCK�� �Ѱ����� ���� ���Ѵ�. */
    sCurLockNode = mArrOfLockList[aTrans->mSlotN].mLockNodeHeader.mPrvTransLockNode;
    while ( sCurLockNode != &(mArrOfLockList[aTrans->mSlotN].mLockNodeHeader) )
    {
        sPrvLockNode = sCurLockNode->mPrvTransLockNode;

        // ���̺� �����̽� ���� DDL�� XA�� �������� �ʴ´�.
        if ( sCurLockNode->mLockItemType != SMI_LOCK_ITEM_TABLE )
        {
            sCurLockNode = sPrvLockNode;
            continue;
        }

        sCurLockNode = sPrvLockNode;
        sLockCount++;
    }
    /* 2. �ʿ��� LOG BUFFER SIZE�� ����Ѵ�. */
    if ( sLockCount >= SML_MAX_LOCK_INFO )
    {
        sNeedBufferSize = SMR_LOGREC_SIZE(smrXaPrepareLog)
                          + (SML_MAX_LOCK_INFO * sizeof(smlTableLockInfo))
                          + sizeof(smrLogTail);
    }
    else
    {
        sNeedBufferSize = SMR_LOGREC_SIZE(smrXaPrepareLog)
                          + (sLockCount * sizeof(smlTableLockInfo))
                          + sizeof(smrLogTail);
    }
    /* 3. �ʿ��� LOG BUFER SIZE�� �� ū��� LOG BUFFER SIZE�� �ø���. */
    if ( smxTrans::getLogBufferSize( aTrans ) < sNeedBufferSize )
    {
        aTrans->setLogBufferSize( sNeedBufferSize );
    }

    /* -------------------------------------------------------------------
       xid�� �α׵���Ÿ�� ���
       ------------------------------------------------------------------- */
    sLockCount = 0;
    sLogBuffer = aTrans->getLogBuffer();
    sPrepareLog = (smrXaPrepareLog*)sLogBuffer;
    smrLogHeadI::setTransID(&sPrepareLog->mHead, aTrans->mTransID);
    smrLogHeadI::setFlag(&sPrepareLog->mHead, aTrans->mLogTypeFlag);
    smrLogHeadI::setType(&sPrepareLog->mHead, SMR_LT_XA_PREPARE);

    // BUG-27024 XA���� Prepare �� Commit���� ���� Disk Row��
    //           Server Restart �� ����Ῡ�� �մϴ�.
    // XA Trans�� FstDskViewSCN�� Log�� ����Ͽ�,
    // Restart�� XA Prepare Trans �籸�࿡ ���
    SM_SET_SCN( &sPrepareLog->mFstDskViewSCN, &aTrans->mFstDskViewSCN );

    if ( (smrLogHeadI::getFlag(&sPrepareLog->mHead) & SMR_LOG_SAVEPOINT_MASK)
         == SMR_LOG_SAVEPOINT_OK)
    {
        smrLogHeadI::setReplStmtDepth( &sPrepareLog->mHead,
                                       smLayerCallback::getLstReplStmtDepth( aTrans ) );
    }
    else
    {
        smrLogHeadI::setReplStmtDepth( &sPrepareLog->mHead,
                                       SMI_STATEMENT_DEPTH_NULL );
    }

    // prepared�� ������ �α���
    // �ֳ��ϸ� heuristic commit/rollback �������� timeout ����� ����ϴµ�
    // system failure ���Ŀ��� prepared�� ��Ȯ�� ������ �����ϱ� ������.
    /* BUG-18981 */
    idlOS::memcpy(&(sPrepareLog->mXaTransID), aXID, sizeof(ID_XID));
    sTmv                       = idlOS::gettimeofday();
    sPrepareLog->mPreparedTime = (timeval)sTmv;
    sPrepareLog->mIsGCTx       = aTrans->mIsGCTx;
    /* -------------------------------------------------------------------
       table lock�� prepare log�� ����Ÿ�� �α�
       record lock�� OID ������ ����� ȸ���� ����� �ܰ迡�� �����ؾ� ��
       ------------------------------------------------------------------- */
    sLog         = sLogBuffer + SMR_LOGREC_SIZE(smrXaPrepareLog);
    sCurLockNode = mArrOfLockList[aTrans->mSlotN].mLockNodeHeader.mPrvTransLockNode;

    while ( sCurLockNode != &(mArrOfLockList[aTrans->mSlotN].mLockNodeHeader) )
    {
        sPrvLockNode = sCurLockNode->mPrvTransLockNode;

        // ���̺� �����̽� ���� DDL�� XA�� �������� �ʴ´�.
        if ( sCurLockNode->mLockItemType != SMI_LOCK_ITEM_TABLE )
        {
            sCurLockNode = sPrvLockNode;
            continue;
        }

        sLockInfo.mOidTable = (smOID)sCurLockNode->mLockItem->mItemID;
        sLockInfo.mLockMode = sCurLockNode->mLockMode;

        idlOS::memcpy(sLog, &sLockInfo, sizeof(smlTableLockInfo));
        sLog        += sizeof( smlTableLockInfo );
        sCurLockNode = sPrvLockNode;
        sLockCount++;

        if ( sLockCount >= SML_MAX_LOCK_INFO )
        {
            smrLogHeadI::setSize( &sPrepareLog->mHead,
                                  SMR_LOGREC_SIZE(smrXaPrepareLog) +
                                   + (sLockCount * sizeof(smlTableLockInfo))
                                   + sizeof(smrLogTail));

            sPrepareLog->mLockCount = sLockCount;

            smrLogHeadI::copyTail( (SChar*)sLogBuffer +
                                   smrLogHeadI::getSize(&sPrepareLog->mHead) -
                                   sizeof(smrLogTail),
                                   &(sPrepareLog->mHead) );

            IDE_TEST( smrLogMgr::writeLog( NULL, /* idvSQL* */
                                           aTrans,
                                           (SChar*)sLogBuffer,
                                           NULL,  // Previous LSN Ptr
                                           NULL,  // Log LSN Ptr
                                           NULL,  // End LSN Ptr
                                           SM_NULL_OID )
                     != IDE_SUCCESS );

            sLockCount = 0;
            sIsLogged  = ID_TRUE;
            sLog       = sLogBuffer + SMR_LOGREC_SIZE(smrXaPrepareLog);
        }
    }

    if ( !( (sIsLogged == ID_TRUE) && (sLockCount == 0) ) )
    {
        smrLogHeadI::setSize(&sPrepareLog->mHead,
                             SMR_LOGREC_SIZE(smrXaPrepareLog)
                                + sLockCount * sizeof(smlTableLockInfo)
                                + sizeof(smrLogTail));

        sPrepareLog->mLockCount = sLockCount;

        smrLogHeadI::copyTail( (SChar*)sLogBuffer +
                               smrLogHeadI::getSize(&sPrepareLog->mHead) -
                               sizeof(smrLogTail),
                               &(sPrepareLog->mHead) );

        IDE_TEST( smrLogMgr::writeLog( NULL, /* idvSQL* */
                                       aTrans,
                                       (SChar*)sLogBuffer,
                                       NULL,      // Previous LSN Ptr
                                       NULL,      // Log LSN Ptr
                                       &sEndLSN,  // End LSN Ptr
                                       SM_NULL_OID )
                  != IDE_SUCCESS );

        if ( smLayerCallback::isNeedLogFlushAtCommitAPrepare( aTrans )
             == ID_TRUE )
        {
            IDE_TEST( smrLogMgr::syncLFThread( SMR_LOG_SYNC_BY_TRX,
                                               &sEndLSN )
                      != IDE_SUCCESS );
        }
    }

    IDE_TEST( smrRecoveryMgr::mIsReplWaitGlobalTxAfterPrepareFunc( NULL, /* idvSQL* */
                                                                   ID_FALSE, /* isRequestNode */
                                                                   aTrans->mTransID,
                                                                   SM_MAKE_SN(sEndLSN) ) 
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

void  smlLockMgr::lockTableByPreparedLog( void  * aTrans,
                                          SChar * aLogPtr,
                                          UInt    aLockCnt,
                                          UInt  * aOffset )
{

    UInt i;
    smlTableLockInfo  sLockInfo;
    smOID sTableOID;
    smcTableHeader   *sTableHeader;
    smlLockItem      *sLockItem    = NULL;

    for ( i = 0 ; i < aLockCnt ; i++ )
    {

        idlOS::memcpy( &sLockInfo,
                       (SChar *)((aLogPtr) + *aOffset),
                       sizeof(smlTableLockInfo) );
        sTableOID = sLockInfo.mOidTable;
        IDE_ASSERT( smcTable::getTableHeaderFromOID( sTableOID,
                                                     (void**)&sTableHeader )
                    == IDE_SUCCESS );

        sLockItem = (smlLockItem *)SMC_TABLE_LOCK( sTableHeader );
        IDE_ASSERT( sLockItem->mLockItemType == SMI_LOCK_ITEM_TABLE );

        smlLockMgr::lockTable( smLayerCallback::getTransSlot( aTrans ),
                               sLockItem,
                               sLockInfo.mLockMode );

        *aOffset += sizeof(smlTableLockInfo);
    }
}

void smlLockMgr::initLockNode( smlLockNode *aLockNode )
{
    SInt          i;
    smlLockSlot  *sLockSlotList;

    aLockNode->mLockMode          = SML_NLOCK;
    aLockNode->mPrvLockNode       = NULL;
    aLockNode->mNxtLockNode       = NULL;
    aLockNode->mCvsLockNode       = NULL;

    aLockNode->mPrvTransLockNode  = NULL;
    aLockNode->mNxtTransLockNode  = NULL;

    aLockNode->mDoRemove          = ID_FALSE;

    sLockSlotList = aLockNode->mArrLockSlotList;

    for ( i = 0 ; i < SML_NUMLOCKTYPES ; i++ )
    {
        sLockSlotList[i].mLockNode      = aLockNode;
        sLockSlotList[i].mMask          = mLockModeToMask[i];
        sLockSlotList[i].mPrvLockSlot   = NULL;
        sLockSlotList[i].mNxtLockSlot   = NULL;
        sLockSlotList[i].mLockSequence  = 0;
        sLockSlotList[i].mOldMode       = SML_NLOCK;
        sLockSlotList[i].mNewMode       = SML_NLOCK;
    }
}

IDE_RC smlLockMgr::allocLockNodeNormal(SInt /*aSlot*/, smlLockNode** aNewNode)
{
    IDE_TEST( mLockPool.alloc((void**)aNewNode) != IDE_SUCCESS );
    (*aNewNode)->mIndex = -1;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

IDE_RC smlLockMgr::allocLockNodeList( SInt aSlot, smlLockNode** aNewNode )
{
    /* BUG-43408 */
    smlLockNode*        sLockNode;
    smlTransLockList*   sTransLockList;
    iduList*            sLockNodeCache;
    iduListNode*        sNode;

    sTransLockList = (mArrOfLockList+aSlot);
    sLockNodeCache = &(sTransLockList->mLockNodeCache);

    if ( IDU_LIST_IS_EMPTY( sLockNodeCache ) )
    {
        IDE_TEST( mLockPool.alloc((void**)aNewNode)
                  != IDE_SUCCESS );
        sLockNode = *aNewNode;
        IDU_LIST_INIT_OBJ( &(sLockNode->mNode4LockNodeCache), sLockNode );
    }
    else
    {
        sNode = IDU_LIST_GET_FIRST( sLockNodeCache );
        sLockNode = (smlLockNode*)sNode->mObj;
        IDU_LIST_REMOVE( sNode );
        *aNewNode = sLockNode;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

IDE_RC smlLockMgr::allocLockNodeBitmap( SInt aSlot, smlLockNode** aNewNode )
{
    SInt            sIndex;
    smlLockNode*    sLockNode = NULL;

    IDE_DASSERT( smuProperty::getLockMgrCacheNode() == 1 );

    sIndex = acpBitFfs64(~(mNodeAllocMap[aSlot]));

    if ( sIndex != -1 )
    {
        mNodeAllocMap[aSlot]   |= ID_ULONG(1) << sIndex;
        sLockNode               = &(mNodeCache[aSlot][sIndex]);
        sLockNode->mIndex       = sIndex;

        *aNewNode               = sLockNode;
    }
    else
    {
        IDE_TEST( allocLockNodeNormal(aSlot, aNewNode) != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

/*********************************************************
  function description: allocLockNodeAndInit
  lock node�� �Ҵ��ϰ�, �ʱ�ȭ�� �Ѵ�.
***********************************************************/
IDE_RC  smlLockMgr::allocLockNodeAndInit( SInt           aSlot,
                                          smlLockMode    aLockMode,
                                          smlLockItem  * aLockItem,
                                          smlLockNode ** aNewLockNode,
                                          idBool         aIsExplicitLock )
{
    smlLockNode*    sLockNode = NULL;

    IDE_TEST( mAllocLockNodeFunc(aSlot, &sLockNode) != IDE_SUCCESS );
    IDE_DASSERT( sLockNode != NULL );
    initLockNode( sLockNode );

    // table oid added for perfv
    sLockNode->mLockItemType    = aLockItem->mLockItemType;
    sLockNode->mSpaceID         = aLockItem->mSpaceID;
    sLockNode->mItemID          = aLockItem->mItemID;
    sLockNode->mSlotID          = aSlot;
    sLockNode->mLockCnt         = 0;
    sLockNode->mLockItem        = aLockItem;
    sLockNode->mLockMode        = aLockMode;
    sLockNode->mFlag            = mLockModeToMask[aLockMode];
    // BUG-28752 implicit/explicit �����մϴ�.
    sLockNode->mIsExplicitLock  = aIsExplicitLock;

    sLockNode->mTransID = smLayerCallback::getTransID( smLayerCallback::getTransBySID( aSlot ) );

    *aNewLockNode = sLockNode;

    IDE_DASSERT( isLockNodeExist( sLockNode ) == ID_FALSE );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC smlLockMgr::freeLockNodeNormal( smlLockNode* aLockNode )
{
    return mLockPool.memfree(aLockNode);
}

IDE_RC smlLockMgr::freeLockNodeList( smlLockNode* aLockNode )
{
    SInt                sSlot;
    smlTransLockList*   sTransLockList;
    iduList*            sLockNodeCache;

    sSlot           = aLockNode->mSlotID;
    sTransLockList  = (mArrOfLockList+sSlot);
    sLockNodeCache  = &(sTransLockList->mLockNodeCache);
    IDU_LIST_ADD_AFTER( sLockNodeCache, &(aLockNode->mNode4LockNodeCache) );

    return IDE_SUCCESS;
}

IDE_RC smlLockMgr::freeLockNodeBitmap( smlLockNode* aLockNode )
{
    SInt            sSlot;
    SInt            sIndex;
    ULong           sDelta;

    if ( aLockNode->mIndex != -1 )
    {
        IDE_DASSERT( smuProperty::getLockMgrCacheNode() == 1 );

        sSlot   = aLockNode->mSlotID;
        sIndex  = aLockNode->mIndex;
        sDelta  = ID_ULONG(1) << sIndex;

        IDE_DASSERT( (mNodeAllocMap[sSlot] & sDelta) != 0 );
        mNodeAllocMap[sSlot] &= ~sDelta;
    }
    else
    {
        IDE_TEST( mLockPool.memfree(aLockNode) != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

/*********************************************************
  function description: findLockNode
  Ʈ������� ���� statement�� ���Ͽ�,
  ����  table A�� ���Ͽ� lock�� ��� �ִ�
  lock node  �� ã�´�.
***********************************************************/
smlLockNode * smlLockMgr::findLockNode( smlLockItem * aLockItem, SInt aSlot )
{
    smlLockNode *sCurLockNode;
    smlLockNode* sLockNodeHeader = &(mArrOfLockList[aSlot].mLockNodeHeader);
    sCurLockNode = mArrOfLockList[aSlot].mLockNodeHeader.mPrvTransLockNode;

    while ( sCurLockNode != sLockNodeHeader )
    {
        IDE_ASSERT( sCurLockNode->mDoRemove == ID_FALSE );
        IDE_ASSERT( sCurLockNode->mBeGrant  == ID_TRUE );
        if ( sCurLockNode->mLockItem == aLockItem )
        {
            return sCurLockNode;
        }

        sCurLockNode = sCurLockNode->mPrvTransLockNode;
    }

    return NULL;
}

/*********************************************************
  function description: addLockSlot
  transation lock slot list�� insert�ϸ�,
  lock node�߰� ���, node�ȿ� lock slot�� �̿��Ϸ��Ҷ�
  �Ҹ���.
***********************************************************/
void smlLockMgr::addLockSlot( smlLockSlot * aLockSlot, SInt aSlot )
{
    smlLockSlot * sTransLockSlotHdr = &(mArrOfLockList[aSlot].mLockSlotHeader);
    //Add Lock Slot To Tail of Lock Slot List
    IDE_DASSERT( aLockSlot->mNxtLockSlot == NULL );
    IDE_DASSERT( aLockSlot->mPrvLockSlot == NULL );
    IDE_DASSERT( aLockSlot->mLockSequence == 0 );

    aLockSlot->mNxtLockSlot = sTransLockSlotHdr;
    aLockSlot->mPrvLockSlot = sTransLockSlotHdr->mPrvLockSlot;

    /* BUG-15906: non-autocommit��忡�� Select�Ϸ��� IS_LOCK�� �����Ǹ�
     * ���ڽ��ϴ�. Statement���۽� Transaction�� ������ Lock Slot��
     * Lock Sequence Number�� ������ �ΰ� Statement End�ÿ� Transaction��
     * Lock Slot List�� ������ ���鼭 �����ص� Lock Sequence Number����
     * �ش� Lock Slot�� Lock Sequence Number�� �۰ų� ���������� Lock�� �����Ѵ�.
     */
    IDE_ASSERT( sTransLockSlotHdr->mPrvLockSlot->mLockSequence !=
                ID_ULONG_MAX );
    IDE_ASSERT( aLockSlot->mLockSequence == 0 );

    aLockSlot->mLockSequence =
        sTransLockSlotHdr->mPrvLockSlot->mLockSequence + 1;

    sTransLockSlotHdr->mPrvLockSlot->mNxtLockSlot = aLockSlot;
    sTransLockSlotHdr->mPrvLockSlot = aLockSlot;
}

/*********************************************************
  function description: removeLockSlot
  transaction�� slotid��
  �ش��ϴ� lock slot list���� lock slot��  �����Ѵ�.
***********************************************************/
void smlLockMgr::removeLockSlot( smlLockSlot * aLockSlot )
{
    aLockSlot->mNxtLockSlot->mPrvLockSlot = aLockSlot->mPrvLockSlot;
    aLockSlot->mPrvLockSlot->mNxtLockSlot = aLockSlot->mNxtLockSlot;

    aLockSlot->mPrvLockSlot  = NULL;
    aLockSlot->mNxtLockSlot  = NULL;
    aLockSlot->mLockSequence = 0;

    aLockSlot->mOldMode      = SML_NLOCK;
    aLockSlot->mNewMode      = SML_NLOCK;
}

/*********************************************************
  function description: decTblLockModeAndTryUpdate
  table lock ������ aLockItem����
  Lock node�� lock mode�� �ش��ϴ� lock mode�� ������ ���̰�,
  ���� 0�� �ȴٸ�, table�� ��ǥ���� �����Ѵ�.
***********************************************************/
void smlLockMgr::decTblLockModeAndTryUpdate( smlLockItem     * aLockItem,
                                             smlLockMode       aLockMode )
{
    --(aLockItem->mArrLockCount[aLockMode]);
    IDE_ASSERT(aLockItem->mArrLockCount[aLockMode] >= 0);
    // table�� ��ǥ ���� �����Ѵ�.
    if ( aLockItem->mArrLockCount[aLockMode] ==  0 )
    {
        aLockItem->mFlag &= ~(mLockModeToMask[aLockMode]);
    }
}

/*********************************************************
  function description: incTblLockModeUpdate
  table lock ������ aLockItem����
  Lock node�� lock mode�� �ش��ϴ� lock mode�� ������ ���̰�,
   table�� ��ǥ���� �����Ѵ�.
***********************************************************/
void smlLockMgr::incTblLockModeAndUpdate( smlLockItem     *  aLockItem,
                                          smlLockMode        aLockMode )
{
    ++(aLockItem->mArrLockCount[aLockMode]);
    IDE_ASSERT(aLockItem->mArrLockCount[aLockMode] >= 0);
    aLockItem->mFlag |= mLockModeToMask[aLockMode];
}


void smlLockMgr::addLockNodeToHead( smlLockNode *&aFstLockNode,
                                    smlLockNode *&aLstLockNode,
                                    smlLockNode *&aNewLockNode )
{
    if ( aFstLockNode != NULL )
    {
        aFstLockNode->mPrvLockNode = aNewLockNode;
    }
    else
    {
        aLstLockNode = aNewLockNode;
    }
    aNewLockNode->mPrvLockNode = NULL;
    aNewLockNode->mNxtLockNode = aFstLockNode;
    aFstLockNode = aNewLockNode;
}

void smlLockMgr::addLockNodeToTail( smlLockNode *&aFstLockNode,
                                    smlLockNode *&aLstLockNode,
                                    smlLockNode *&aNewLockNode )
{
    if ( aLstLockNode != NULL )
    {
        aLstLockNode->mNxtLockNode = aNewLockNode;
    }
    else
    {
        aFstLockNode = aNewLockNode;
    }
    aNewLockNode->mPrvLockNode = aLstLockNode;
    aNewLockNode->mNxtLockNode = NULL;
    aLstLockNode = aNewLockNode;
}

void smlLockMgr::removeLockNode( smlLockNode *&aFstLockNode,
                                 smlLockNode *&aLstLockNode,
                                 smlLockNode *&aLockNode )
{
    if ( aLockNode == aFstLockNode )
    {
        aFstLockNode = aLockNode->mNxtLockNode;
    }
    else
    {
        aLockNode->mPrvLockNode->mNxtLockNode = aLockNode->mNxtLockNode;
    }
    if ( aLockNode == aLstLockNode )
    {
        aLstLockNode = aLockNode->mPrvLockNode;
    }
    else
    {
        aLockNode->mNxtLockNode->mPrvLockNode = aLockNode->mPrvLockNode;
    }
    aLockNode->mNxtLockNode = NULL;
    aLockNode->mPrvLockNode = NULL;
}

IDE_RC smlLockMgr::allocLockItem( void ** aLockItem )
{
    IDE_TEST_RAISE( iduMemMgr::malloc( IDU_MEM_SM_SML,
                                       ID_SIZEOF(smlLockItem),
                                       aLockItem ) != IDE_SUCCESS,
                    insufficient_memory );

    return IDE_SUCCESS;

    IDE_EXCEPTION( insufficient_memory );
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_InsufficientMemory));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC smlLockMgr::initLockItem( scSpaceID       aSpaceID,
                                 ULong           aItemID,
                                 smiLockItemType aLockItemType,
                                 void          * aLockItem )
{
    smlLockItem       * sLockItem;
    SChar               sBuffer[128];

    sLockItem = (smlLockItem*) aLockItem;

    /* LockItem Type�� ���� mSpaceID�� mItemID�� �����Ѵ�.
     *                   mSpaceID    mItemID
     * TableSpace Lock :  SpaceID     N/A
     * Table Lock      :  SpaceID     TableOID
     * DataFile Lock   :  SpaceID     FileID */
    sLockItem->mLockItemType    = aLockItemType;
    sLockItem->mSpaceID         = aSpaceID;
    sLockItem->mItemID          = aItemID;

    idlOS::sprintf( sBuffer,
                    "LOCKITEM_MUTEX_%"ID_UINT32_FMT"_%"ID_UINT64_FMT,
                    aSpaceID,
                    (ULong)aItemID );

    IDE_TEST_RAISE( sLockItem->mMutex.initialize( sBuffer,
                                                  IDU_MUTEX_KIND_NATIVE,
                                                  IDV_WAIT_INDEX_NULL )
                    != IDE_SUCCESS,
                    mutex_init_error );

    clearLockItem( sLockItem );

    return IDE_SUCCESS;

    IDE_EXCEPTION(mutex_init_error);
    {
        IDE_SET(ideSetErrorCode(smERR_FATAL_ThrMutexInit));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

void smlLockMgr::clearLockItem( smlLockItem   * aLockItem )
{
    SInt   sLockType;

    aLockItem->mGrantLockMode   = SML_NLOCK;
    aLockItem->mFstLockGrant    = NULL;
    aLockItem->mFstLockRequest  = NULL;
    aLockItem->mLstLockGrant    = NULL;
    aLockItem->mLstLockRequest  = NULL;
    aLockItem->mGrantCnt        = 0;
    aLockItem->mRequestCnt      = 0;
    aLockItem->mFlag            = SML_FLAG_LIGHT_MODE;

    for ( sLockType = 0 ; sLockType < SML_NUMLOCKTYPES ; sLockType++ )
    {
        aLockItem->mArrLockCount[sLockType] = 0;
    }
}



/*********************************************************
  function description: isCycle
  SIGMOD RECORD, Vol.17, No,2 ,June,1988
  Bin Jang,
  Dead Lock Detection is Really Cheap paper��
  7 page����� ����.
  �� ����� �ٸ�����,
  waiting table�� chained matrix�̾ �˷θ�����
  ���⵵�� ���߾���,
  ������ loop���� �Ʒ��� ���� ������ �־�
  �ڱ��ڽŰ���ΰ� �߻����ڸ��� ��ü loop�� ���������ٴ�
  ���̴�.
  if(aSlot == j)
  {
   return ID_TRUE;
  }

 *********************************************************

  <PROJ-2734 : 'aIsReadyDistDeadlock = ID_TRUE' �ΰ�� >
  �л� Ʈ������ ����� üũ�� Ʈ�������� ���⼭ �����Ѵ�.

  WAITER TX�� ������ �� ó�� ������ <�л������ִ�TX>�� ����� �Ǹ�,
  mTxList4DistDeadlock[WaitSlot][HoldLlot] �� ���õȴ�.
  ��� ���������� �Ʒ��� ����.

  1. WAITER TX�� �������� �Ѵ�. (WAITER TX�� <�л������ִ�TX>�̴�.)
  2. WAITER TX�� ����ϰ� �ִ� <�л������ִ�TX>�� ����� �ȴ�.
  3. WAITER TX�� ����ϰ� �ִ� ���� <�л���������TX>���,
     <�л���������TX>�� ������� �� �ٸ� TX(X)�� Ȯ���Ѵ�.
     3.1 TX(X)�� <�л������ִ�TX>��� ����� �ȴ�.
     3.2 TX(X)�� <�л���������TX>���, <�л������ִ�TX>�� ���ö����� ��� �����踦 ���󰣴�.

  ����) TX�� A, B, ~ H ���� �ִ�.
        WAITER TX A�� �Ʒ��� ���� �����踦 ���´ٸ�.
        ������ TX B ~ H ������ ���� ������ ���� ��������.
    
  No�� <�л���������TX>, Yes�� <�л������ִ�TX>�� ��Ÿ����.

  A(WAITER) -> B(No)  -> C(Yes) -> D(No)
  A(WAITER) -> E(Yes) -> F(Yes)
  A(WAITER) -> G(Yes) -> H(No)

  ���� ������� TX ������ TYPE�� �Ʒ��� ���� ��������,
  SML_DIST_DEADLOCK_TX_FIRST_DIST_INFO Ÿ���� TX�� �л� Ʈ������ ����� üũ ������� �����ȴ�.

  SML_DIST_DEADLOCK_TX_NON_DIST_INFO   : B
  SML_DIST_DEADLOCK_TX_FIRST_DIST_INFO : C, E, G
  SML_DIST_DEADLOCK_TX_UNKNOWN         : D, F, H

***********************************************************/
idBool smlLockMgr::isCycle( SInt aSlot, idBool aIsReadyDistDeadlock )
{
    UShort     i, j;
    // node�� node���� ����� ����.
    UShort sPathLen;
    // node���� ��� ��ΰ� �ִ°� ������ flag
    idBool     sExitPathFlag;
    smxTrans * sTrans = NULL;

    sExitPathFlag = ID_TRUE;
    sPathLen= 1;
    IDL_MEM_BARRIER;

    if ( aIsReadyDistDeadlock == ID_TRUE )
    {
        /* PROJ-2734 : �ڽ��� mTxList4DistDeadlock�� �ʱ�ȭ */
        smlLockMgr::clearTxList4DistDeadlock( aSlot );
    }

    while ( ( sExitPathFlag == ID_TRUE ) &&
            ( mWaitForTable[aSlot][aSlot].mIndex == 0 ) &&
            ( sPathLen < mTransCnt ) )
    {
        sExitPathFlag      = ID_FALSE;

        for ( i  =  mArrOfLockList[aSlot].mFstWaitTblTransItem;
              ( i != SML_END_ITEM ) &&
              ( mWaitForTable[aSlot][aSlot].mNxtWaitTransItem == ID_USHORT_MAX );
              i = mWaitForTable[aSlot][i].mNxtWaitTransItem )
        {
            IDE_ASSERT(i < mTransCnt);

            if ( mWaitForTable[aSlot][i].mIndex == sPathLen )
            {

                /* PROJ-2734 */
                /* path Len = 1�� TX�� ���� mTxList4DistDeadlock ������ ���⼭�Ѵ�. */
                if ( ( sPathLen == 1 ) &&
                     ( aIsReadyDistDeadlock == ID_TRUE ) )
                {
                    sTrans = (smxTrans *)(smLayerCallback::getTransBySID( i ));

                    if ( SMI_DIST_LEVEL_IS_VALID( sTrans->mDistTxInfo.mDistLevel ) )
                    {
                        /*
                           PROJ-2734
                           Trans�� ��Ȱ��Ǿ����� Ȯ���ϱ� ���� TransID�� ���� �����Ѵ�.

                           ���� Ȯ��������, TransID���� ������ TX�� ��Ȱ��ɼ������Ƿ�,
                           mWaitForTable[aSlot][i].mIndex == 1 �� �״�� �����Ǵ��� Ȯ���ؼ�
                           TX ��Ȱ�뿩�θ� ��Ȯ���Ѵ�.
                         */

                        ID_SERIAL_BEGIN( mTxList4DistDeadlock[aSlot][i].mDistTxType = SML_DIST_DEADLOCK_TX_FIRST_DIST_INFO;
                                         mTxList4DistDeadlock[aSlot][i].mTransID    = sTrans->mTransID; );

                        ID_SERIAL_END( if ( mWaitForTable[aSlot][i].mIndex != 1 )
                                       {
                                           /* �����谡 ������ٸ�, TX�� ��Ȱ��Ȱ��̴�. ������ ������ ���ش�. */
                                           mTxList4DistDeadlock[aSlot][i].mDistTxType = SML_DIST_DEADLOCK_TX_UNKNOWN;
                                           mTxList4DistDeadlock[aSlot][i].mTransID    = SM_NULL_TID;
                                       } );
                    }
                    else
                    {
                        mTxList4DistDeadlock[aSlot][i].mDistTxType = SML_DIST_DEADLOCK_TX_NON_DIST_INFO;
                        mTxList4DistDeadlock[aSlot][i].mTransID    = SM_NULL_TID;
                    }
                }

                for ( j  = mArrOfLockList[i].mFstWaitTblTransItem;
                      j < SML_END_ITEM;
                      j  = mWaitForTable[i][j].mNxtWaitTransItem )
                {
                    if ( (mWaitForTable[i][j].mIndex == 1)
                         && (mWaitForTable[aSlot][j].mNxtWaitTransItem == ID_USHORT_MAX) )
                    {
                        IDE_ASSERT(j < mTransCnt);

                        mWaitForTable[aSlot][j].mIndex = sPathLen + 1;

                        mWaitForTable[aSlot][j].mNxtWaitTransItem
                            = mArrOfLockList[aSlot].mFstWaitTblTransItem;

                        mArrOfLockList[aSlot].mFstWaitTblTransItem = j;

                        /* PROJ-2734 */
                        if ( aIsReadyDistDeadlock == ID_TRUE )
                        {
                            switch( mTxList4DistDeadlock[aSlot][i].mDistTxType )
                            {
                                case SML_DIST_DEADLOCK_TX_FIRST_DIST_INFO :
                                case SML_DIST_DEADLOCK_TX_UNKNOWN :

                                    mTxList4DistDeadlock[aSlot][j].mDistTxType = SML_DIST_DEADLOCK_TX_UNKNOWN;
                                    mTxList4DistDeadlock[aSlot][j].mTransID    = SM_NULL_TID;
                                    break;

                                case SML_DIST_DEADLOCK_TX_NON_DIST_INFO :

                                    sTrans = (smxTrans *)(smLayerCallback::getTransBySID( j ));

                                    if ( SMI_DIST_LEVEL_IS_VALID( sTrans->mDistTxInfo.mDistLevel ) )
                                    {
                                        /*
                                           PROJ-2734
                                           Trans�� ��Ȱ��Ǿ����� Ȯ���ϱ� ���� TransID�� ���� �����Ѵ�.

                                           ���� Ȯ��������, TransID���� ������ TX�� ��Ȱ��ɼ������Ƿ�,
                                           mWaitForTable[i][j].mIndex == 1 �� �״�� �����Ǵ��� Ȯ���ؼ�
                                           TX ��Ȱ�뿩�θ� ��Ȯ���Ѵ�.
                                         */

                                        ID_SERIAL_BEGIN( mTxList4DistDeadlock[aSlot][j].mDistTxType
                                                             = SML_DIST_DEADLOCK_TX_FIRST_DIST_INFO;
                                                         mTxList4DistDeadlock[aSlot][j].mTransID
                                                             = sTrans->mTransID ; );

                                        ID_SERIAL_END( if ( mWaitForTable[i][j].mIndex != 1 )
                                                       {
                                                           /* TX�� ��Ȱ��Ȱ��̴�. ������ ������ ���ش�. */
                                                           mTxList4DistDeadlock[aSlot][j].mDistTxType
                                                               = SML_DIST_DEADLOCK_TX_UNKNOWN;
                                                           mTxList4DistDeadlock[aSlot][j].mTransID
                                                               = SM_NULL_TID;
                                                       } );
                                    }
                                    break;

                                default :
                                    IDE_DASSERT(0);
                            }
                        }

                        // aSlot == j�̸�,
                        // dead lock �߻� �̴�..
                        // �ֳ��ϸ�
                        // mWaitForTable[aSlot][aSlot].mIndex !=0
                        // ���°� �Ǿ��� �����̴�.
                        // dead lock dection���� ����,
                        // ������ loop ����. ������ ���� Ʋ����.
                        if ( aSlot == j )
                        {
                            return ID_TRUE;
                        }

                        sExitPathFlag = ID_TRUE;
                    }
                }//For
            }//if
        }//For
        sPathLen++;
    }//While

    return (mWaitForTable[aSlot][aSlot].mIndex == 0) ? ID_FALSE: ID_TRUE;
}

ULong smlLockMgr::getDistDeadlockWaitTime( smlDistDeadlockRiskType aRisk )
{
    ULong sWaitTime;

    switch ( aRisk )
    {
        case SML_DIST_DEADLOCK_RISK_LOW :
            sWaitTime = smuProperty::getDistributionDeadlockRiskLowWaitTime();
            break;

        case SML_DIST_DEADLOCK_RISK_MID :
            sWaitTime = smuProperty::getDistributionDeadlockRiskMidWaitTime();
            break;

        case SML_DIST_DEADLOCK_RISK_HIGH :
            sWaitTime = smuProperty::getDistributionDeadlockRiskHighWaitTime();
            break;

        default :
            sWaitTime = 0;
            IDE_DASSERT(0);
    }

    return sWaitTime;
}

/* PROJ-2734
   �л굥��� üũ��� : WAITER�� ������ �� ó�� ������ �л������ִ�TX
   �л극�� Ȯ�� ���  : WAITER�� �����迡 �ִ� ��� �л������ִ�TX */
smxDistDeadlockDetection smlLockMgr::detectDistDeadlock( SInt    aWaitSlot,
                                                         ULong * aWaitTime )
{
    smxTrans * sHoldTrans          = NULL;
    smxTrans * sWaitTrans          = NULL;
    SInt i                         = 0;
    SInt sSlotCount                = 0;
    smiDistLevel sMaxDistLevel     = SMI_DIST_LEVEL_INIT;
    smiDistLevel sLastMaxDistLevel = SMI_DIST_LEVEL_INIT;
    ULong sWaitTime                = 0;
    smxDistDeadlockDetection sDetected       = SMX_DIST_DEADLOCK_DETECTION_NONE;
    smxDistDeadlockDetection sFirstDetection = SMX_DIST_DEADLOCK_DETECTION_NONE;

    sWaitTrans = (smxTrans *) smLayerCallback::getTransBySID( aWaitSlot );
    sSlotCount = (SInt)smlLockMgr::getSlotCount();

    /* isCycle() �Լ�����
       mTxList4DistDeadlock ������ �л굥��� üũ�� TX�� �����صξ���.
       (mTxList4DistDeadlock[WaitSlot][HoldSlot].mDistTxType = SML_DIST_DEADLOCK_TX_FIRST_DIST_INFO �� TX�̴�.) */

    for ( i = 0;
          i < sSlotCount;
          i++ )
    {
        /* ���TX �ڱ��ڽ��̸� �н� */
        if ( i == aWaitSlot )
        {
            continue;
        }

        /* ���TX�� �����谡 ���ٸ� �н� */
        if ( mWaitForTable[aWaitSlot][i].mIndex == 0 )
        {
            continue;
        }

        sHoldTrans = (smxTrans *)smLayerCallback::getTransBySID( i );

        /* ����TX�� �л극���� VALID ���� ������ �н� */
        if ( SMI_DIST_LEVEL_IS_NOT_VALID( sHoldTrans->mDistTxInfo.mDistLevel ) )
        {
            continue;
        }

        /* ���TX�� �������� �����踦 �׸��� ��� �л�TX�� ���ؼ�
           �л극���� Ȯ���Ѵ�. */
        sLastMaxDistLevel = sMaxDistLevel;
        if ( sHoldTrans->mDistTxInfo.mDistLevel > sMaxDistLevel )
        {
            sMaxDistLevel = sHoldTrans->mDistTxInfo.mDistLevel;
        }

        /*
           �л굥��� üũ ����� �ƴϸ� �н��Ѵ�.
           => ���⼭ ������ ����,
              �л극���� Ȯ���� TX�� �л굥��� üũ�� TX�� �ٸ����ִٴ� ���̴�.

           1) �л극�� Ȯ���� TX : ���TX�� �����迡 �ִ� ��� ����TX
           2) �л굥��� üũ�� TX : ���TX�� ������ �� ó�� ������ ����TX
         */
        if ( mTxList4DistDeadlock[aWaitSlot][i].mDistTxType != SML_DIST_DEADLOCK_TX_FIRST_DIST_INFO )
        {
            continue;
        }

        sDetected = compareTx4DistDeadlock( sWaitTrans, sHoldTrans );

        if ( ( sHoldTrans->mStatus == SMX_TX_END ) ||
             ( mTxList4DistDeadlock[aWaitSlot][i].mTransID != sHoldTrans->mTransID ) )
        {
            /* ���߿� HOLDER TX�� ��Ȱ�� �Ǿ���.
               SKIP ����. */
#ifdef DEBUG
            ideLog::log( IDE_SD_19,
                         "Distribution Deadlock - REUSE Trans ID : "
                         "%"ID_UINT32_FMT" -> %"ID_UINT32_FMT", Status : %"ID_INT32_FMT,
                         mTxList4DistDeadlock[aWaitSlot][i].mTransID,
                         sHoldTrans->mTransID,
                         sHoldTrans->mStatus );
#endif

            /* BUG-48445 : MAX�л극���� ����� ��츸 �����մϴ�. */
            if ( sLastMaxDistLevel != sMaxDistLevel )
            {
                sMaxDistLevel = sLastMaxDistLevel;
            }

            continue;
        }

        /* �л굥����� Ž���Ǿ��ٸ�, */
        if ( sDetected != SMX_DIST_DEADLOCK_DETECTION_NONE )
        {
            if ( sFirstDetection == SMX_DIST_DEADLOCK_DETECTION_NONE )
            {
                sFirstDetection = sDetected;
            }

            /* �л�LEVEL PARALLEL ���, ���̻� TX�� ���� �ʿ����. �񱳸� �����Ѵ�. */
            if ( sMaxDistLevel == SMI_DIST_LEVEL_PARALLEL )
            {
                break;
            }
        }
    }

    if ( sFirstDetection != SMX_DIST_DEADLOCK_DETECTION_NONE )
    {
        /* ���̱��� TIMEOUT ���� */
        sWaitTime = getDistDeadlockWaitTime( 
                    mDistDeadlockRisk[ sWaitTrans->mDistTxInfo.mDistLevel ][ sMaxDistLevel ] );

    }

#ifdef DEBUG
    ideLog::log( IDE_SD_19,
                 "WaiterTx(%"ID_UINT32_FMT") DistLevel : %"ID_INT32_FMT", HolderTx Max DistLevel : %"ID_INT32_FMT,
                 sWaitTrans->mTransID,
                 sWaitTrans->mDistTxInfo.mDistLevel,
                 sMaxDistLevel );
#endif

    *aWaitTime = sWaitTime;

    return sFirstDetection;
}

/* PROJ-2734 : Distributed TX�� ���Ѵ�.
   �񱳼����� �Ʒ��� ����.
   1. Distribution TX First Stmt View SCN
   2. Distribution TX First Stmt Time
   3. Shard PIN sequence
   4. Shard PIN Node ID */
smxDistDeadlockDetection smlLockMgr::compareTx4DistDeadlock( smxTrans * aWaitTx, smxTrans * aHoldTx )
{
    smxDistDeadlockDetection sDetected = SMX_DIST_DEADLOCK_DETECTION_NONE;

    UShort sWaitTxNodeID;
    UShort sHoldTxNodeID;

    UInt sWaitTxSequence;
    UInt sHoldTxSequence;

    while ( 1 ) /* no loop */
    {
        /* 1. Distribution TX First Stmt View SCN COMPARE */
#ifdef DEBUG
        ideLog::log( IDE_SD_19,
                     "compare ViewSCN (Must be Wait(%"ID_INT32_FMT") > Hold(%"ID_INT32_FMT")) : "
                     "%"ID_UINT64_FMT", %"ID_UINT64_FMT,
                     aWaitTx->mTransID,
                     aHoldTx->mTransID,
                     aWaitTx->mDistTxInfo.mFirstStmtViewSCN,
                     aHoldTx->mDistTxInfo.mFirstStmtViewSCN );
#endif

        if ( aWaitTx->mDistTxInfo.mFirstStmtViewSCN > aHoldTx->mDistTxInfo.mFirstStmtViewSCN )
        {
            break;
        }
        else if ( aWaitTx->mDistTxInfo.mFirstStmtViewSCN < aHoldTx->mDistTxInfo.mFirstStmtViewSCN )
        {
            sDetected = SMX_DIST_DEADLOCK_DETECTION_VIEWSCN;
            break;
        }

        /* 2. Distribution TX First Stmt Time COMPARE */
#ifdef DEBUG
        ideLog::log( IDE_SD_19,
                     "compare Time (Must be Wait(%"ID_INT32_FMT") > Hold(%"ID_INT32_FMT")) : "
                     "[%"ID_INT64_FMT"][%"ID_INT64_FMT"], [%"ID_INT64_FMT"][%"ID_INT64_FMT"]",
                     aWaitTx->mTransID,
                     aHoldTx->mTransID,
                     aWaitTx->mDistTxInfo.mFirstStmtTime.tv_.tv_sec,
                     aWaitTx->mDistTxInfo.mFirstStmtTime.tv_.tv_usec,
                     aHoldTx->mDistTxInfo.mFirstStmtTime.tv_.tv_sec,
                     aHoldTx->mDistTxInfo.mFirstStmtTime.tv_.tv_usec );
#endif

        if ( aWaitTx->mDistTxInfo.mFirstStmtTime > aHoldTx->mDistTxInfo.mFirstStmtTime )
        {
            break;
        }
        else if ( aWaitTx->mDistTxInfo.mFirstStmtTime < aHoldTx->mDistTxInfo.mFirstStmtTime )
        {
            sDetected = SMX_DIST_DEADLOCK_DETECTION_TIME;
            break;
        }

        SMI_DIVIDE_SHARD_PIN( aWaitTx->mDistTxInfo.mShardPin,
                              NULL, /* version */
                              &sWaitTxNodeID,
                              &sWaitTxSequence );

        SMI_DIVIDE_SHARD_PIN( aHoldTx->mDistTxInfo.mShardPin,
                              NULL, /* version */
                              &sHoldTxNodeID,
                              &sHoldTxSequence );

        /* 3. SHARD-PIN SEQUENCE COMPARE */
#ifdef DEBUG
        ideLog::log( IDE_SD_19,
                     "compare Sequence (Must be Wait(%"ID_INT32_FMT") > Hold(%"ID_INT32_FMT")) : "
                     "%"ID_UINT32_FMT", %"ID_UINT32_FMT,
                     aWaitTx->mTransID,
                     aHoldTx->mTransID,
                     sWaitTxSequence,
                     sHoldTxSequence );
#endif

        if ( sWaitTxSequence > sHoldTxSequence )
        {
            break;
        }
        else if ( sWaitTxSequence < sHoldTxSequence )
        {
            sDetected = SMX_DIST_DEADLOCK_DETECTION_SHARD_PIN_SEQ;
            break;
        }

        /* 4. SHARD-PIN NODE-ID COMPARE */
#ifdef DEBUG
        ideLog::log( IDE_SD_19,
                     "compare Node-ID (Must be Wait(%"ID_INT32_FMT") > Hold(%"ID_INT32_FMT")) : "
                     "%"ID_UINT32_FMT", %"ID_UINT32_FMT,
                     aWaitTx->mTransID,
                     aHoldTx->mTransID,
                     sWaitTxNodeID,
                     sHoldTxNodeID );
#endif

        if ( sWaitTxNodeID > sHoldTxNodeID )
        {
            break;
        }
        else if ( sWaitTxNodeID < sHoldTxNodeID )
        {
            sDetected = SMX_DIST_DEADLOCK_DETECTION_SHARD_PIN_NODE_ID;
            break;
        }

        sDetected = SMX_DIST_DEADLOCK_DETECTION_ALL_EQUAL;
        break;
    }

#ifdef DEBUG
    ideLog::log( IDE_SD_19,
                 "compare end (Must be 0) : result (%"ID_INT32_FMT")", sDetected );
#endif

    return sDetected;
}

/*********************************************************
  function description:

  aSlot�� �ش��ϴ� Ʈ������� ����ϰ� �־���
  Ʈ����ǵ��� ����� ������ 0���� clear�Ѵ�.
  -> waitTable���� aSlot�࿡ ����ϰ� �ִ� �÷��鿡��
    ��α��̸� 0�� �Ѵ�.

  aDoInit���� ID_TRUE�̸� ��⿬�� ������ ���������.
***********************************************************/
void smlLockMgr::clearWaitItemColsOfTrans( idBool aDoInit, SInt aSlot )
{

    UInt    sCurTargetSlot;
    UInt    sNxtTargetSlot;

    sCurTargetSlot = mArrOfLockList[aSlot].mFstWaitTblTransItem;

    while ( sCurTargetSlot != SML_END_ITEM )
    {
        mWaitForTable[aSlot][sCurTargetSlot].mIndex = 0;
        sNxtTargetSlot = mWaitForTable[aSlot][sCurTargetSlot].mNxtWaitTransItem;

        if ( aDoInit == ID_TRUE )
        {
            mWaitForTable[aSlot][sCurTargetSlot].mNxtWaitTransItem = ID_USHORT_MAX;
        }

        sCurTargetSlot = sNxtTargetSlot;
    }

    if ( aDoInit == ID_TRUE )
    {
        mArrOfLockList[aSlot].mFstWaitTblTransItem = SML_END_ITEM;
    }
}

/* PROJ-2734
 * isCycle() �Լ��� ����Ǳ� ������ aSlot Ʈ�������� ������ ������ �ǵ�����.
 *
 * => isCycle() �Լ� ��������
 *    0 -> 2 or 3 or 4 or ... �� ������ mIndex���� 0���� �ǵ�����. */
void smlLockMgr::revertWaitItemColsOfTrans( SInt aSlot )
{
    UInt    sCurTargetSlot;
    UInt    sNxtTargetSlot;

    /* isCycle�� ��ģ ���� ����Ʈ��
       mIndex���� ������������ �����Ǿ��ִ�. (ū���� �տ� �ִ�.) */

    for ( sCurTargetSlot = mArrOfLockList[aSlot].mFstWaitTblTransItem ;
          ( ( sCurTargetSlot != SML_END_ITEM ) &&
            ( mWaitForTable[aSlot][sCurTargetSlot].mIndex > 1 ) ) ;
          sCurTargetSlot = sNxtTargetSlot )
    {
        mWaitForTable[aSlot][sCurTargetSlot].mIndex = 0;

        sNxtTargetSlot = mWaitForTable[aSlot][sCurTargetSlot].mNxtWaitTransItem;

        mWaitForTable[aSlot][sCurTargetSlot].mNxtWaitTransItem = ID_USHORT_MAX;
    }

    if ( sCurTargetSlot != SML_END_ITEM )
    {
        mArrOfLockList[aSlot].mFstWaitTblTransItem = sCurTargetSlot;
    }
}

/*********************************************************
  function description:
  : Ʈ����� a(aSlot)�� ���Ͽ� waiting�ϰ� �־��� Ʈ����ǵ���
    ��� ��� ������ clear�Ѵ�.
***********************************************************/
void  smlLockMgr::clearWaitTableRows( smlLockNode * aLockNode,
                                      SInt          aSlot )
{

    UShort sTransSlot;

    while ( aLockNode != NULL )
    {
        sTransSlot = aLockNode->mSlotID;
        mWaitForTable[sTransSlot][aSlot].mIndex = 0;
        aLockNode = aLockNode->mNxtLockNode;
    }
}

/*********************************************************
  function description: registRecordLockWait
  ������ ���� ������  record�� ����
  Ʈ����ǰ��� waiting ���踦 �����Ѵ�.

  1. waiting table���� aSlot �� aWaitSlot�� waiting�ϰ�
     �ֽ��� ǥ���Ѵ�.
  2.  aWaitSlot column��  aSlot��
     record lock list�� �����Ų��.
  3.  aSlot�࿡�� aWaitSlot��  transaction waiting
     list�� �����Ų��.
***********************************************************/
void   smlLockMgr::registRecordLockWait( SInt aSlot, SInt aWaitSlot )
{
    SInt sLstSlot;

    IDE_ASSERT( mWaitForTable[aSlot][aWaitSlot].mIndex == 0 );
    IDE_ASSERT( mArrOfLockList[aSlot].mFstWaitTblTransItem == SML_END_ITEM );

    /* BUG-24416
     * smlLockMgr::registRecordLockWait() ���� �ݵ��
     * mWaitForTable[aSlot][aWaitSlot].mIndex �� 1�� �����Ǿ�� �մϴ�. */
    mWaitForTable[aSlot][aWaitSlot].mIndex = 1;

    if ( mWaitForTable[aSlot][aWaitSlot].mNxtWaitRecTransItem
         == ID_USHORT_MAX )
    {
        /* BUG-23823: Record Lock�� ���� ��� ����Ʈ�� Waiting������� ����
         * �־�� �Ѵ�.
         *
         * Wait�� Transaction�� Target Wait Transaction List�� �������� ����
         * �Ѵ�. */
        mWaitForTable[aSlot][aWaitSlot].mNxtWaitRecTransItem = SML_END_ITEM;

        if ( mArrOfLockList[aWaitSlot].mFstWaitRecTransItem == SML_END_ITEM )
        {
            IDE_ASSERT( mArrOfLockList[aWaitSlot].mLstWaitRecTransItem ==
                        SML_END_ITEM );

            mArrOfLockList[aWaitSlot].mFstWaitRecTransItem = aSlot;
            mArrOfLockList[aWaitSlot].mLstWaitRecTransItem = aSlot;
        }
        else
        {
            IDE_ASSERT( mArrOfLockList[aWaitSlot].mLstWaitRecTransItem !=
                        SML_END_ITEM );

            sLstSlot = mArrOfLockList[aWaitSlot].mLstWaitRecTransItem;

            mWaitForTable[sLstSlot][aWaitSlot].mNxtWaitRecTransItem = aSlot;
            mArrOfLockList[aWaitSlot].mLstWaitRecTransItem          = aSlot;
        }
    }

    IDE_ASSERT( mArrOfLockList[aSlot].mFstWaitTblTransItem == SML_END_ITEM );

    mWaitForTable[aSlot][aWaitSlot].mNxtWaitTransItem = mArrOfLockList[aSlot].mFstWaitTblTransItem;
    mArrOfLockList[aSlot].mFstWaitTblTransItem = aWaitSlot;
}

/*********************************************************
  function description: freeAllRecordLock
  aSlot�� �ش��ϴ� Ʈ������� record lock���� waiting�ϰ�
  �ִ� Ʈ����ǵ鰣�� ��� ��θ� 0���� clear�Ѵ�.
  record lock�� waiting�ϰ� �ִ� Ʈ������� resume��Ų��.
***********************************************************/
IDE_RC  smlLockMgr::freeAllRecordLock( SInt aSlot )
{

    UShort i;
    UShort  sNxtItem;
    void * sTrans;


    i = mArrOfLockList[aSlot].mFstWaitRecTransItem;

    while ( i != SML_END_ITEM )
    {
        IDE_ASSERT( i != ID_USHORT_MAX );

        sNxtItem = mWaitForTable[i][aSlot].mNxtWaitRecTransItem;

        mWaitForTable[i][aSlot].mNxtWaitRecTransItem = ID_USHORT_MAX;

        if ( mWaitForTable[i][aSlot].mIndex == 1 )
        {
            sTrans = smLayerCallback::getTransBySID( i );
            IDE_TEST( smLayerCallback::resumeTrans( sTrans ) != IDE_SUCCESS );
        }

        mWaitForTable[i][aSlot].mIndex = 0;

        i = sNxtItem;
    }

    /* PROJ-1381 FAC
     * Record Lock�� ������ ���� �ʱ�ȭ�� �Ѵ�. */
    mArrOfLockList[aSlot].mFstWaitRecTransItem = SML_END_ITEM;
    mArrOfLockList[aSlot].mLstWaitRecTransItem = SML_END_ITEM;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************
  function description: registTblLockWaitListByReq
  waiting table����   Ʈ������� slot id ���� ���鿡
  request list����  ����ϰ� �ִ�Ʈ����ǵ� ��
  slot id��  table lock waiting ����Ʈ�� ����Ѵ�.
***********************************************************/
void smlLockMgr::registTblLockWaitListByReq( SInt          aSlot,
                                             smlLockNode * aLockNode )
{

    UShort       sTransSlot;

    while ( aLockNode != NULL )
    {
        sTransSlot = aLockNode->mSlotID;
        if ( aSlot != sTransSlot )
        {
            IDE_ASSERT(mWaitForTable[aSlot][sTransSlot].mNxtWaitTransItem == ID_USHORT_MAX);

            mWaitForTable[aSlot][sTransSlot].mIndex = 1;
            mWaitForTable[aSlot][sTransSlot].mNxtWaitTransItem
                = mArrOfLockList[aSlot].mFstWaitTblTransItem;
            mArrOfLockList[aSlot].mFstWaitTblTransItem = sTransSlot;
        }
        aLockNode = aLockNode->mNxtLockNode;
    }
}

/*********************************************************
  function description: registTblLockWaitListByGrant
  waiting table����   Ʈ������� slot id �࿡
  grant list����     �ִ�Ʈ����ǵ� ��
  slot id��  table lock waiting ����Ʈ�� ����Ѵ�.
***********************************************************/
void smlLockMgr::registTblLockWaitListByGrant( SInt          aSlot,
                                               smlLockNode * aLockNode )
{

    UShort       sTransSlot;

    while ( aLockNode != NULL )
    {
        sTransSlot = aLockNode->mSlotID;
        if ( (aSlot != sTransSlot) &&
             (mWaitForTable[aSlot][sTransSlot].mNxtWaitTransItem == ID_USHORT_MAX) )
        {
            mWaitForTable[aSlot][sTransSlot].mIndex = 1;
            mWaitForTable[aSlot][sTransSlot].mNxtWaitTransItem
                = mArrOfLockList[aSlot].mFstWaitTblTransItem;
            mArrOfLockList[aSlot].mFstWaitTblTransItem = sTransSlot;
        } //if
        aLockNode = aLockNode->mNxtLockNode;
    }//while
}

/*********************************************************
  function description: setLockModeAndAddLockSlot
  Ʈ������� ������ table�� lock�� ���� ��쿡 ���Ͽ�,
  ���� Ʈ������� lock mode�� setting�ϰ�,
  �̹����� lock�� ��Ҵٸ� lock slot�� Ʈ������� lock
  node�� �߰��Ѵ�.
***********************************************************/
void smlLockMgr::setLockModeAndAddLockSlot( SInt           aSlot,
                                            smlLockNode  * aTxLockNode,
                                            smlLockMode  * aCurLockMode,
                                            smlLockMode    aLockMode,
                                            idBool         aIsLocked,
                                            smlLockSlot ** aLockSlot )
{
    if ( aTxLockNode != NULL )
    {
        if ( aCurLockMode != NULL )
        {
            *aCurLockMode = aTxLockNode->mLockMode;
        }
        // Ʈ������� lock slot list��  lock slot  �߰� .
        if ( aIsLocked == ID_TRUE )
        {
            aTxLockNode->mFlag |= mLockModeToMask[aLockMode];
            addLockSlot( &(aTxLockNode->mArrLockSlotList[aLockMode]),
                         aSlot );

            if ( aLockSlot != NULL )
            {
                *aLockSlot = &(aTxLockNode->mArrLockSlotList[aLockMode]);
            }//if aLockSlot

        }//if aIsLocked
    }//aTxLockNode != NULL
}

/***********************************************************
 * BUG-47388 Lock Table ����
 * Lock Node�� Transaction Lock Node List�� �ִ��� Ȯ���Ѵ�.
 ***********************************************************/
idBool smlLockMgr::isLockNodeExist( smlLockNode *aLockNode )
{
    UInt aSlot = aLockNode->mSlotID;
    smlLockNode *sCurLockNode;
    smlLockNode* sLockNodeHeader = &(mArrOfLockList[ aSlot ].mLockNodeHeader);

    IDE_ASSERT( aLockNode->mSlotID < mTransCnt );

    sCurLockNode = mArrOfLockList[aSlot].mLockNodeHeader.mPrvTransLockNode;

    while ( sCurLockNode != sLockNodeHeader )
    {
        if ( sCurLockNode == aLockNode )
        {
            return ID_TRUE;
        }

        sCurLockNode = sCurLockNode->mPrvTransLockNode;
    }

    return ID_FALSE;
}

/***********************************************************
 * BUG-47388 Lock Table ����
 * Lock Node�� Lock Item�� Grant List�� �ִ��� Ȯ���Ѵ�.
 ***********************************************************/
idBool smlLockMgr::isLockNodeExistInGrant( smlLockItem * aLockItem,
                                           smlLockNode *aLockNode )
{
    smlLockNode   * sCurLockNode = aLockItem->mFstLockGrant;

    while ( sCurLockNode != NULL )
    {
        IDE_ASSERT( sCurLockNode->mLockItem == aLockItem );
        IDE_ASSERT( sCurLockNode->mItemID == aLockItem->mItemID );

        if ( sCurLockNode == aLockNode )
        {
            return ID_TRUE;
        }
        if ( aLockItem->mLstLockGrant == sCurLockNode )
        {
            break;
        }

        sCurLockNode = sCurLockNode->mNxtLockNode;
    }

    return ID_FALSE;
}

/***********************************************************
 * BUG-47388 Lock Table ����
 * Lock Item�� Grant List�� Request List�� �����Ѵ�.
 ***********************************************************/
void smlLockMgr::validateNodeListInLockItem( smlLockItem * aLockItem )
{
    smlLockNode * sCurLockNode = aLockItem->mFstLockGrant;

    while ( sCurLockNode != NULL )
    {
        IDE_ASSERT( sCurLockNode->mLockItem == aLockItem );
        IDE_ASSERT( sCurLockNode->mItemID == aLockItem->mItemID );

        if ( aLockItem->mLstLockGrant == sCurLockNode )
        {
            break;
        }

        sCurLockNode = sCurLockNode->mNxtLockNode;
    }

    sCurLockNode = aLockItem->mFstLockRequest;
    while ( sCurLockNode != NULL )
    {
        IDE_ASSERT( sCurLockNode->mLockItem == aLockItem );
        IDE_ASSERT( sCurLockNode->mItemID == aLockItem->mItemID );

        if ( aLockItem->mLstLockRequest == sCurLockNode )
        {
            break;
        }

        sCurLockNode = sCurLockNode->mNxtLockNode;
    }

    return ;
}


void smlLockMgr::updateStatistics( idvSQL*      sStat,
                                   idvStatIndex aStatIdx )
{
    idvSession * sSession;

    sSession = ( sStat == NULL ? NULL : sStat->mSess );

    IDV_SESS_ADD( sSession, aStatIdx, 1 );
}

/***********************************************************
 * BUG-47388 Lock Table �������� Light Mode �߰�
 *
 * 0 Table, Tablespace Disable �̸� Lock Table�� ���� �ʴ´�.
 *   DDL�� ���� �� ����. S, X, SIX �� ������ ����ó��
 *
 * 1. �̹� ���� Lock Table�� ������� Lock �� �� ��� ���
 *    ��, IS Lock �� FAC�� ������ ��û�� Skip���� �ʴ´�.
 *
 * 2. BUG-47388���� �߰��� Light Mode�� Lock Table �� ��´�.
 *    Lock Item�� S,X,SIX�� ���� ���� IS or IX�� ��� ���
 *
 * 3. Lock Item�� S,X,SIX �� �ְų� ���� S,X,SIX �� ���
 *    Mutex Mode�� Lock Table�� ��´�.
 ***********************************************************/
IDE_RC smlLockMgr::lockTable( SInt          aSlot,
                              smlLockItem  *aLockItem,
                              smlLockMode   aLockMode,
                              ULong         aLockWaitMicroSec,
                              smlLockMode   *aCurLockMode,
                              idBool       *aLocked,
                              smlLockNode **aLockNode,
                              smlLockSlot **aLockSlot,
                              idBool        aIsExplicit )
{
    smlLockNode       * sCurTransLockNode = NULL;
    UInt                sLockEnable = 1;
    idvSQL            * sStatistics = smLayerCallback::getStatisticsBySID( aSlot );

    /* BUG-32237 [sm_transaction] Free lock node when dropping table.
     * DropTablePending ���� �����ص� freeLockNode�� �����մϴ�. */
    /* �ǵ��� ��Ȳ�� �ƴϱ⿡ Debug��忡���� DASSERT�� �����Ŵ.
     * ������ release ��忡���� �׳� rebuild �ϸ� ���� ����. */
    IDE_ERROR_RAISE( aLockItem != NULL, error_table_modified );

    if ( aLocked != NULL )
    {
        *aLocked = ID_TRUE;
    }

    if ( aLockSlot != NULL )
    {
        *aLockSlot = NULL;
    }

    // To fix BUG-14951
    // smuProperty::getTableLockEnable�� TABLE���� ����Ǿ�� �Ѵ�.
    // (TBSLIST, TBS �� DBF���� �ش� �ȵ�)
    /* BUG-35453 -  add TABLESPACE_LOCK_ENABLE property
     * TABLESPACE_LOCK_ENABLE �� TABLE_LOCK_ENABLE �� �����ϰ�
     * tablespace lock�� ���� ó���Ѵ�. */
    if ( aLockItem->mLockItemType == SMI_LOCK_ITEM_TABLE )
    {
        sLockEnable = smuProperty::getTableLockEnable();
    }
    else if ( aLockItem->mLockItemType == SMI_LOCK_ITEM_TABLESPACE )
    {
        sLockEnable = smuProperty::getTablespaceLockEnable();
    }

    if ( sLockEnable == 0 )
    {
        IDE_TEST_RAISE( (( aLockMode == SML_SLOCK ) ||
                         ( aLockMode == SML_XLOCK ) ||
                         ( aLockMode == SML_SIXLOCK )),  error_lock_table_use );

        if ( aCurLockMode != NULL )
        {
            *aCurLockMode = aLockMode;
        }

        return IDE_SUCCESS;
    }

    // Ʈ������� ���� statement�� ���Ͽ�,
    // ���� table A�� ���Ͽ� lock�� ��Ҵ� lock node�� ã�´�.
    // Trans Node List�� ���� �����Ѵ�. �׷��Ƿ� lock�� ���� �ʿ�� ����.
    // �� �� �ٸ� DDL�� ���� �� ���� ������ Trans Node List�� ���������� �ʴ´�..
    // Grant, Req List�� DDL�� ���� �� ���� �ִ�.
    sCurTransLockNode = findLockNode( aLockItem, aSlot );
    // case 1: ������ Ʈ�������  table A��  lock�� ��Ұ�,
    // ������ ���� lock mode�� ���� ����� �ϴ� ����� ��ȯ�����
    // ������ �ٷ� return!
    if ( sCurTransLockNode != NULL )
    {
        if ( mConversionTBL[sCurTransLockNode->mLockMode][aLockMode]
             == sCurTransLockNode->mLockMode )
        {
            if (( aLockMode == SML_ISLOCK ) &&
                ( sCurTransLockNode->mArrLockSlotList[aLockMode].mLockSequence == 0 ))
            {
                /* PROJ-1381 Fetch Across Commits
                 * IS Lock�� ���� �ʾҴ� ���, Lock Mode�� ȣȯ�� �Ǵ���
                 * FAC Fetch Cursor�� ���� IS Lock�� ��� ��� �Ѵ�. */       
            }
            else
            {
                /* �׷��� ���� ����(IS �� �ƴ� ����̰ų� �̹� IS�� ���� ���)
                 * �̹� ���� Lock Mode�� ���� ���� Lock�� ���� Lock�̸� �ٷ� Return */
                if ( aCurLockMode != NULL )
                {
                    *aCurLockMode = sCurTransLockNode->mLockMode;
                }
                IDE_CONT( lock_SUCCESS );
            }
        }
    }

    if (( aLockMode == SML_ISLOCK ) ||
        ( aLockMode == SML_IXLOCK ))
    {
        // �ٸ� DDL�� ���ü� ��� ���ؼ� Lock�� ��ƾ��Ѵ�.
        lockTransNodeList( sStatistics, aSlot );

        if ( aLockItem->mFlag == SML_FLAG_LIGHT_MODE )
        {
            if ( sCurTransLockNode != NULL ) /* ���� statement ����� lock�� ���� */
            {
                /* Lock�� ���ؼ� Conversion�� �����Ѵ�. */
                sCurTransLockNode->mLockMode =
                    mConversionTBL[sCurTransLockNode->mLockMode][aLockMode];
            }
            else                      /* ���� statement ���� �� lock�� ���� ���� */
            {
                /* allocate lock node and initialize */
                IDE_TEST( allocLockNodeAndInit( aSlot,
                                                aLockMode,
                                                aLockItem,
                                                &sCurTransLockNode,
                                                aIsExplicit )
                          != IDE_SUCCESS );
                sCurTransLockNode->mBeGrant  = ID_TRUE;

                /* Add Lock Node to a transaction */
                addLockNode( sCurTransLockNode, aSlot );
            }

            sCurTransLockNode->mLockCnt++;

            // Ʈ������� Lock node�� �־���,Lock�� ��Ҵٸ�
            // lock slot�� �߰��Ѵ�
            // XXX ���� �ڵ忡�� ������ ȣ��ȴ�. ������ lockslot�� �߰� �ؾ� �ϴ��� Ȯ�� �ؾ� �Ѵ�.
            setLockModeAndAddLockSlot( aSlot,
                                       sCurTransLockNode,
                                       aCurLockMode,
                                       aLockMode,
                                       ID_TRUE,
                                       aLockSlot );

            IDE_DASSERT( sCurTransLockNode->mArrLockSlotList[aLockMode].mLockSequence != 0 );
            // ���Ϳ��� �߰��ϰų� �����ϰ� ����
            unlockTransNodeList( aSlot );

            IDE_ASSERT( sCurTransLockNode->mLockMode != SML_XLOCK );

            IDE_CONT(lock_SUCCESS);
        }
        else
        {
            unlockTransNodeList( aSlot );
        }
    }

    IDE_TEST( lockTableInternal( sStatistics,
                                 aSlot,
                                 aLockItem,
                                 aLockMode,
                                 aLockWaitMicroSec,
                                 aCurLockMode,
                                 aLocked ,
                                 sCurTransLockNode,
                                 aLockSlot,
                                 aIsExplicit ) != IDE_SUCCESS );

    IDE_EXCEPTION_CONT(lock_SUCCESS);

    if ( aLockNode != NULL )
    {
        *aLockNode = sCurTransLockNode;
    }
    else
    {
        /* nothing to do */
    }

    updateStatistics( sStatistics,
                      IDV_STAT_INDEX_LOCK_ACQUIRED );

    return IDE_SUCCESS;

    IDE_EXCEPTION( error_table_modified );
    {
        IDE_SET( ideSetErrorCode( smERR_REBUILD_smiTableModified ) );
    }
    IDE_EXCEPTION( error_lock_table_use );
    {
        if ( aLockItem->mLockItemType == SMI_LOCK_ITEM_TABLE )
        {
            IDE_SET( ideSetErrorCode( smERR_ABORT_TableLockUse ));
        }
        else
        {
            IDE_SET( ideSetErrorCode( smERR_ABORT_TablespaceLockUse ));
        }
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/*********************************************************
  function description: lockTable
  table�� lock�� �ɶ� ������ ���� case�� ���� ���� ó���Ѵ�.

  0. Lock Item Mutex�� ��� ���� Light Mode �� ���
     Light Mode�� Lock table�� ��� �ٷ� ���� ������.(BUG-47388)

  1. ������ Ʈ�������  table A��  lock�� ��Ұ�,
    ������ ���� lock mode�� ���� ����� �ϴ� ����� ��ȯ�����
    ������ �ٷ� return.

  2. table�� grant lock mode�� ���� ����� �ϴ� ���� ȣȯ�����Ѱ��.

     2.1  table �� lock waiting�ϴ� Ʈ������� �ϳ��� ����,
          ������ Ʈ������� table�� ���Ͽ� lock�� ���� �ʴ� ���.

       -  lock node�� �Ҵ��ϰ� �ʱ�ȭ�Ѵ�.
       -  table lock�� grant list tail�� add�Ѵ�.
       -  lock node�� grant�Ǿ����� �����ϰ�,
          table lock�� grant count�� 1���� ��Ų��.
       -  Ʈ������� lock list��  lock node��  add�Ѵ�.

     2.2  table �� lock waiting�ϴ� Ʈ����ǵ��� ������,
          ������ Ʈ�������  table�� ���Ͽ� lock�� ���� ���.
         -  table���� grant�� lock mode array����
            Ʈ������� lock node��   lock mode�� 1����.
            table �� ��ǥ�� ���Žõ�.

     2.1, 2.2�� ����������  ������ �����ϰ� 3��° �ܰ�� �Ѿ.
     - ���� ��û�� lock mode�� ����  lock mode��
      conversion�Ͽ�, lock node�� lock mode�� ����.
     - lock node�� ���ŵ�  lock mode�� �̿��Ͽ�
      table���� grant�� lock mode array����
      ���ο� lockmode �� �ش��ϴ� lock mode�� 1�����ϰ�
      table�� ��ǥ���� ����.
     - table lock�� grant mode�� ����.
     - grant lock node�� lock���� ����.

     2.3  ������ Ʈ������� table�� ���Ͽ� lock�� ���� �ʾҰ�,
          table�� request list�� ��� ���� �ʴ� ���.
         - lock�� grant���� �ʰ� 3.2���� ���� lock���ó����
           �Ѵ�.
           % ���� ���� ������ �̷��� ó���Ѵ�.

  3. table�� grant lock mode�� ���� ����� �ϴ� ���� ȣȯ�Ұ����� ���.
     3.1 lock conflict������,   grant�� lock node�� 1���̰�,
         �װ��� �ٷ� ��  Ʈ������� ���.
      - request list�� ���� �ʰ�
        ���� grant��  lock node�� lock mode�� table�� ��ǥ��,
        grant lock mode��   �����Ѵ�.

     3.2 lock conflict �̰� lock ��� �ð��� 0�� �ƴ� ���.
        -  request list�� �� lock node�� �����ϰ�
         �Ʒ� ���� case�� ���Ͽ�, �б�.
        3.2.1 Ʈ������� ������ table�� ���Ͽ� grant��
              lock node�� ������ �ִ� ���.
           -  request list�� ����� ������ lock node�� �߰�.
           -  request list�� �Ŵ� lock node�� cvs node��
               grant�� lock node�� �ּҷ� assgin.
            -> ���߿� �ٸ� Ʈ����ǿ���  unlock table�ÿ�
                request list�� �ִ� lock node��
               ���ŵ� table grant mode�� ȣȯ�ɶ�,
               grant list�� ������ insert���� �ʰ� ,
               ���� cvs lock node�� lock mode�� ���Ž���
               grant list ������ ���̷��� �ǵ���.

        3.2.2 ������ grant�� lock node�� ���� ���.
            -  table lock request list���� �ִ� Ʈ����ǵ���
               slot id����  waitingTable���� lock�� �䱸��
               Ʈ������� slot id �࿡ list�� �����Ѵ�
               %��α��̴� 1��
            - request list�� tail�� ������ lock node�� �߰�.

        3.2.1, 3.2.2 �������� ������ �������� ������ �����Ѵ�.

        - waiting table����   Ʈ������� slot id �࿡
        grant list����     �ִ�Ʈ����ǵ� ��
        slot id��  table lock waiting ����Ʈ�� ����Ѵ�.
        - dead lock �� �˻��Ѵ�(isCycle)
         -> smxTransMgr::waitForLock���� �˻��ϸ�,
          dead lock�� �߻��ϸ� Ʈ����� abort.
        - waiting�ϰ� �ȴ�(suspend).
         ->smxTransMgr::waitForLock���� �����Ѵ�.
        -  �ٸ� Ʈ������� commit/abort�Ǹ鼭 wakeup�Ǹ�,
          waiting table���� �ڽ��� Ʈ����� slot id��
          �����ϴ� �࿡��  ��� �÷����� clear�Ѵ�.
        -  Ʈ������� lock list�� lock node�� �߰��Ѵ�.
          : lock�� ��� �Ǿ��⶧����.

     3.3 lock conflict �̰� lock��� �ð��� 0�� ���.
        : lock flag�� ID_FALSE�� ����.

  4. Ʈ������� lock slot list��  lock slot  �߰� .
     (BUG-47388 cvs node �� ����ϴ� �ӽ� Lock Node �̸� Free)

 BUG-28752 lock table ... in row share mode ������ ������ �ʽ��ϴ�.

 implicit/explicit lock�� �����Ͽ� �̴ϴ�.
 implicit is lock�� statement end�� Ǯ���ֱ� �����Դϴ�. 

 �� ��� Upgrding Lock�� ���� ������� �ʾƵ� �˴ϴ�.
  ��, Implicit IS Lock�� �ɸ� ���¿��� Explicit IS Lock�� �ɷ� �ϰų�
 Explicit IS Lock�� �ɸ� ���¿��� Implicit IS Lock�� �ɸ� ��쿡 ����
 ������� �ʾƵ� �˴ϴ�.
  �ֳ��ϸ� Imp�� ���� �ɷ����� ���, ������ Statement End�Ǵ� ��� Ǯ
 ������ ������ ���� Explicit Lock�� �Ŵµ� ���� ����, Exp�� ���� �ɷ�
 ���� ���, ������ IS Lock�� �ɷ� �ִ� �����̱� ������ Imp Lock�� ��
 �� �ʿ�� ���� �����Դϴ�.
***********************************************************/
IDE_RC smlLockMgr::lockTableInternal( idvSQL           * aStatistics,
                                      SInt               aSlot,
                                      smlLockItem      * aLockItem,
                                      smlLockMode        aLockMode,
                                      ULong              aLockWaitMicroSec,
                                      smlLockMode      * aCurLockMode,
                                      idBool           * aLocked,
                                      smlLockNode      * aCurTransLockNode,
                                      smlLockSlot     ** aLockSlot,
                                      idBool             aIsExplicit )
{
    smlLockNode*        sNewTransLockNode = NULL;
    idBool              sLocked           = ID_TRUE;
    UInt                sState            = 0;
    SInt                i;
    smlTransLockList  * sCurSlot;
    smlLockNode       * sGrantLockNode;
    idBool              sIsFirstDDL = ID_FALSE;

    aLockItem->mMutex.lock( aStatistics );
    sState = 1;

    /***************************************************************************************/
    /* BUG-47388���� �߰��� ���� Start
     * 1. IS, IX lockItem�� ��ƾ� �ؼ� ��Ҵµ� �� ���̿� ��� ����������, Light Mode�� �߰�
     * 2. X, S, SIX : Transaction�� ��� ��ȸ�ϸ� Grant List�� ���� */
    if ( aLockItem->mFlag == SML_FLAG_LIGHT_MODE )
    {
        if (( aLockMode == SML_ISLOCK ) ||
            ( aLockMode == SML_IXLOCK ))
        {
            // lockItem�� ��ƾ� �ؼ� ��Ҵµ�,�� ���̿� ��� ����������,
            // ���Ϳ��� �߰��ϰų� �����ϰ� ������.

            if ( aCurTransLockNode != NULL ) /* ���� statement ����� lock�� ���� */
            {
                /* Lock�� ���ؼ� Conversion�� �����Ѵ�. */
                aCurTransLockNode->mLockMode =
                    mConversionTBL[aCurTransLockNode->mLockMode][aLockMode];
            }
            else                      /* ���� statement ���� �� lock�� ���� ���� */
            {
                /* allocate lock node and initialize */
                IDE_TEST( allocLockNodeAndInit( aSlot,
                                                aLockMode,
                                                aLockItem,
                                                &aCurTransLockNode,
                                                aIsExplicit )
                          != IDE_SUCCESS );
                aCurTransLockNode->mBeGrant  = ID_TRUE;

                /* Add Lock Node to a transaction */
                lockTransNodeList( aStatistics, aSlot );
                addLockNode( aCurTransLockNode, aSlot );
                unlockTransNodeList( aSlot );
            }

            aCurTransLockNode->mLockCnt++;

            IDE_CONT(lock_SUCCESS);
        }
        else
        {
            // S,X,SIX �� ó�� ���� ���, ��� ��ȸ�ϸ� �̹� ���� I lock�� ã�ƺ���.
            /* 1. Transaction Ž�� �Ͽ� Grant List�� �����Ѵ�.
             * 2. ������ Lock Item�� �����ϰ� lock�� ��´�.
             * ���⿡���� Grant List�� ���� �Ǹ� �������� ���Ŀ��� �Ѵ�.*/

            // mFlag�� ����ϱ� ���� �ʱ�ȭ �Ѵ�.
            // SML_FLAG_LIGHT_MODE ���� �ٸ������� �����ϸ�
            // �ٸ� Transaction ���� Light Mode���� ����ٴ� ���� �˰� �ȴ�.
            aLockItem->mFlag = 0 ;
            sIsFirstDDL = ID_TRUE;

            // �� mutex�� ������ dead lock �߻� �� �� �ִ�.
            // ���� mutex�� ���� �ʰ� Ÿ�� trans �� mutex�� ��´�.
            for( i = 0 ; i < mTransCnt ; i++ )
            {
                sCurSlot = mArrOfLockList + i;

                // Node List�� ��ȣ�ϸ� ������ ���⿡���� DML���� ������ �����ϱ� ���ϴ� ���ҵ� �Ѵ�.
                // Pointer�� Null�ΰ� ���� ���̴���, �ش� Transaction�� Lock�� ��� ���� ���� ���� ���ϼ��� �ִ�.
                // �׷��Ƿ� Mutex lock�� ��� Ȯ�� �� ���� �Ѵ�.
                lockTransNodeList( aStatistics, i );

                if ( (void*)sCurSlot->mLockNodeHeader.mPrvTransLockNode != (void*)sCurSlot )
                {
                    sGrantLockNode = findLockNode( aLockItem, i );
                    if( sGrantLockNode != NULL )
                    {
                        IDE_ASSERT( mCompatibleTBL[aLockItem->mGrantLockMode][sGrantLockNode->mLockMode] == ID_TRUE );
                        /* add node to grant list */
                        addLockNodeToTail( aLockItem->mFstLockGrant,
                                           aLockItem->mLstLockGrant,
                                           sGrantLockNode );

                        aLockItem->mGrantCnt++;

                        incTblLockModeAndUpdate( aLockItem, sGrantLockNode->mLockMode );
                        aLockItem->mGrantLockMode = mConversionTBL[aLockItem->mGrantLockMode][ sGrantLockNode->mLockMode ];
                    }
                }
                unlockTransNodeList( i );
            }
        }
    }
    /* BUG-47388���� �߰��� ���� End */
    /***************************************************************************************/

    //---------------------------------------
    // table�� ��ǥ���� ���� ����� �ϴ� ���� ȣȯ�����ϴ��� Ȯ��.
    //---------------------------------------
    if ( mCompatibleTBL[aLockItem->mGrantLockMode][aLockMode] == ID_TRUE )
    {
        if ( aCurTransLockNode != NULL ) /* ���� statement ����� lock�� ���� */
        {
            // ������ ��Ҵ� ���� ���� ��� �Ŀ� �� ���� �ִ´�.
            decTblLockModeAndTryUpdate( aLockItem,
                                        aCurTransLockNode->mLockMode );
        }
        else                      /* ���� statement ���� �� lock�� ���� ���� */
        {
            /* ��ٸ��� lock�� �ִ� ��� */
            /* BUGBUG: BUG-16471, BUG-17522
             * ��ٸ��� lock�� �ִ� ��� ������ lock conflict ó���Ѵ�. */
            IDE_TEST_CONT( aLockItem->mRequestCnt != 0, lock_conflict );

            /* ��ٸ��� lock�� ���� ��� Lock�� grant �� */
            /* BUG-47363 �̸� alloc �ص� LockNode�� ��� */
            IDE_TEST( allocLockNodeAndInit( aSlot,
                                            aLockMode,
                                            aLockItem,
                                            &aCurTransLockNode,
                                            aIsExplicit )
                      != IDE_SUCCESS );

            /* add node to grant list */
            addLockNodeToTail( aLockItem->mFstLockGrant,
                               aLockItem->mLstLockGrant,
                               aCurTransLockNode );

            aCurTransLockNode->mBeGrant = ID_TRUE;
            aLockItem->mGrantCnt++;

            // ������ �� ȥ�� ������, �ٸ� Transaction�� �ٸ� Table�� DDL�� �����鼭 ���� ���� �� ���� �ִ�.
            // Transaction Lock Node List ���� �� �� ���� �ݵ�� Lock�� ��ƾ� �Ѵ�.
            lockTransNodeList( aStatistics, aSlot );
            /* Add Lock Node to a transaction */
            addLockNode( aCurTransLockNode, aSlot );
            unlockTransNodeList( aSlot );
        }

        /* Lock�� ���ؼ� Conversion�� �����Ѵ�. */
        aCurTransLockNode->mLockMode =
            mConversionTBL[aCurTransLockNode->mLockMode][aLockMode];

        incTblLockModeAndUpdate(aLockItem, aCurTransLockNode->mLockMode);
        aLockItem->mGrantLockMode =
            mConversionTBL[aLockItem->mGrantLockMode][aLockMode];

        aCurTransLockNode->mLockCnt++;
        IDE_CONT(lock_SUCCESS);
    }

    //---------------------------------------
    // Lock Conflict ó��
    //---------------------------------------

    IDE_EXCEPTION_CONT(lock_conflict);

    if ( ( aLockItem->mGrantCnt == 1 ) && ( aCurTransLockNode != NULL ) )
    {
        //---------------------------------------
        // lock conflict������, grant�� lock node�� 1���̰�,
        // �װ��� �ٷ� ��  Ʈ������� ��쿡�� request list�� ���� �ʰ�
        // ���� grant��  lock node�� lock mode�� table�� ��ǥ��,
        // grant lock mode��  �����Ѵ�.
        //---------------------------------------

        decTblLockModeAndTryUpdate( aLockItem,
                                    aCurTransLockNode->mLockMode );

        aCurTransLockNode->mLockMode =
            mConversionTBL[aCurTransLockNode->mLockMode][aLockMode];

        aLockItem->mGrantLockMode =
            mConversionTBL[aLockItem->mGrantLockMode][aLockMode];

        incTblLockModeAndUpdate( aLockItem, aCurTransLockNode->mLockMode );
    }
    else if ( aLockWaitMicroSec != 0 )
    {
        IDE_TEST( allocLockNodeAndInit( aSlot,
                                        aLockMode,
                                        aLockItem,
                                        &sNewTransLockNode,
                                        aIsExplicit )
                  != IDE_SUCCESS );

        sNewTransLockNode->mBeGrant = ID_FALSE;

        if ( aCurTransLockNode != NULL )
        {
            sNewTransLockNode->mCvsLockNode = aCurTransLockNode;

            //Lock node�� Lock request ����Ʈ�� ����� �߰��Ѵ�.
            // �ֳ��ϸ� Conversion�̱� �����̴�.
            addLockNodeToHead( aLockItem->mFstLockRequest,
                               aLockItem->mLstLockRequest,
                               sNewTransLockNode );
        }
        else
        {
            aCurTransLockNode = sNewTransLockNode;

            // waiting table����   Ʈ������� slot id �࿡
            // request list����  ����ϰ� �ִ�Ʈ����ǵ� ��
            // slot id��  table lock waiting ����Ʈ�� ����Ѵ�.
            registTblLockWaitListByReq(aSlot,aLockItem->mFstLockRequest);
            //Lock node�� Lock request ����Ʈ�� Tail�� �߰��Ѵ�.
            addLockNodeToTail( aLockItem->mFstLockRequest,
                               aLockItem->mLstLockRequest,
                               sNewTransLockNode );
        }
        // waiting table����   Ʈ������� slot id �࿡
        // grant list����     �ִ�Ʈ����ǵ� ��
        // slot id��  table lock waiting ����Ʈ�� ����Ѵ�.
        registTblLockWaitListByGrant( aSlot,aLockItem->mFstLockGrant );
        aLockItem->mRequestCnt++;

        IDE_TEST_RAISE( smLayerCallback::waitForLock(
                                        smLayerCallback::getTransBySID( aSlot ),
                                        &(aLockItem->mMutex),
                                        aLockWaitMicroSec )
                        != IDE_SUCCESS, err_wait_lock );

        if ( sNewTransLockNode->mCvsLockNode != NULL )
        {
            // �̹� ��� �ִ� lock�� �����ϰ�, ����ϴ� �ӽ� lock node�� ������,
            // Ʈ������� lock list���� ������ ���� ���� (BUG-47388)
            IDE_ASSERT(( sNewTransLockNode->mPrvTransLockNode == NULL ) &&
                       ( sNewTransLockNode->mNxtTransLockNode == NULL ));
            IDE_ASSERT(( sNewTransLockNode->mPrvLockNode == NULL ) &&
                       ( sNewTransLockNode->mNxtLockNode == NULL ) );
            IDE_ASSERT(  sNewTransLockNode->mDoRemove == ID_TRUE );
            IDE_ASSERT(  sNewTransLockNode->mBeGrant  == ID_FALSE );

            IDE_TEST( freeLockNode( sNewTransLockNode ) != IDE_SUCCESS );
            sNewTransLockNode = NULL;

            updateStatistics( aStatistics,
                              IDV_STAT_INDEX_LOCK_RELEASED );
        }
    }
    else
    {
        // ���ʷ� Heavy Mode�� ���� �õ��� DDL�� Lock�� ��µ� ������ ���
        // �ٷ� Light ���� �ٽ� �����Ѵ�.
        if ( sIsFirstDDL == ID_TRUE )
        {
            IDE_TEST( toLightMode( aStatistics,
                                   aLockItem ) != IDE_SUCCESS );
        }

        sLocked = ID_FALSE;

        if ( aLocked != NULL )
        {
            *aLocked = ID_FALSE;
        }
    }

    IDE_EXCEPTION_CONT(lock_SUCCESS);

    //Ʈ������� Lock node�� �־���,Lock�� ��Ҵٸ�
    // lock slot�� �߰��Ѵ�
    setLockModeAndAddLockSlot( aSlot,
                               aCurTransLockNode,
                               aCurLockMode,
                               aLockMode,
                               sLocked,
                               aLockSlot );

    sState = 0;
    (void)aLockItem->mMutex.unlock();

    IDE_TEST_RAISE(sLocked == ID_FALSE, err_exceed_wait_time);
    IDE_DASSERT( aCurTransLockNode->mArrLockSlotList[aLockMode].mLockSequence != 0 );

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_wait_lock);
    {
        // fix BUG-10202�ϸ鼭,
        // dead lock , time out�� ƨ�� ���� Ʈ����ǵ���
        // waiting table row�� ���� clear�ϰ�,
        // request list�� transaction�̱���� �Ǵ��� üũ�Ѵ�.

        // TimeOut ������, Ÿ�̹� ������ Lock�� Grant �� ���� �� ���� �ְ�,
        // �ٷ� ���� �ϸ� �Ǵ� CVS Node �� ���� �ִ�.
        (void)smlLockMgr::unlockTable( aSlot,
                                       sNewTransLockNode,
                                       NULL,
                                       ID_FALSE );
    }
    IDE_EXCEPTION(err_exceed_wait_time);
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_smcExceedLockTimeWait));
    }
    IDE_EXCEPTION_END;
    //waiting table���� aSlot�� �࿡ ��⿭�� clear�Ѵ�.
    clearWaitItemColsOfTrans( ID_TRUE, aSlot );

    if ( sState != 0 )
    {
        (void)aLockItem->mMutex.unlock();
    }

    return IDE_FAILURE;
}

/*********************************************************
 * BUG-47388 Lock Table �������� Light Mode �߰�
 *
 * 1. light mode �� ��� trans lock node list������ �����Ѵ�.
 * 2. �ƴ� ��� unlockTableInternal() ȣ��
 *    2-1 mutex mode unlock Table
 *    2-2 Timeout ����ó�� ( aDoMutexLock == False )
 *       2-2-1 Timeout������ �̹� Grant �ع��� ���
 *       2-2-2 CVS Node�� ��� DoRemove == ID_TRUE
 *       2-2-3 Request List�� ����Ǿ� �ִ� ���
 *********************************************************/
IDE_RC smlLockMgr::unlockTable( SInt           aSlot,
                                smlLockNode  * aLockNode,
                                smlLockSlot  * aLockSlot,
                                idBool         aDoMutexLock )
{
    idvSQL       * sStatistics = smLayerCallback::getStatisticsBySID( aSlot );
    smlLockItem  * sLockItem ;
    UInt           sState = 0;

    if ( aLockNode == NULL )
    {
        IDE_DASSERT( aLockSlot->mLockNode != NULL );

        aLockNode = aLockSlot->mLockNode;
    }

    sLockItem = aLockNode->mLockItem;

    /* DoRemove�� ���� �����ؼ� lock�� if �ȿ� ��Ƶ� ������,
     * 1. Trans lock�� ������ ����.
     * 2. ��κ��� ��� Light Mode �� ���̰� �׷��ٸ� DoRemove�� False�̴�.
     * �ۿ� ��´ٰ� �ؼ� ������ �����Ƿ� ������ ���¸� ����ؼ� �ۿ��� ��´�.
     * */
    lockTransNodeList( sStatistics, aSlot );
    sState = 1;

    if ( aLockNode->mDoRemove == ID_FALSE )
    {
        // �ƴ� �̰ͺ��� �� lock item list prev, next�� ����
        if ( sLockItem->mFlag == SML_FLAG_LIGHT_MODE )
        {
            IDE_ASSERT( aLockNode->mNxtLockNode == NULL );
            IDE_ASSERT( aLockNode->mPrvLockNode == NULL );
            IDE_ASSERT( aLockNode->mLockMode != SML_XLOCK );
            IDE_ASSERT( aLockNode->mLockMode != SML_SLOCK );
            IDE_ASSERT( aLockNode->mLockMode != SML_SIXLOCK );
            IDE_ASSERT( aLockNode->mBeGrant == ID_TRUE );

            if ( aLockSlot != NULL )
            {
                aLockNode->mFlag  &= ~(aLockSlot->mMask);
                // lockTable�� 2.2������ ������ Ʈ������� grant ��
                // lock node�� ���Ͽ�, grant lock node�� �߰��ϴ� ��ſ�
                // lock mode�� conversion�Ͽ� lock slot��
                // add�� ����̴�. �� node�� �ϳ�������, �׾ȿ�
                // lock slot�� �ΰ� �̻��� �ִ� ����̴�.
                // -> ���� lock node�� �����ϸ� �ȵȴ�.
                //   sDoFreeLockNode = ID_FALSE;

                removeLockSlot(aLockSlot);

                if ( aLockNode->mFlag != 0 )
                {
                    aLockNode->mLockMode = getDecision( aLockNode->mFlag );

                    sState = 0;
                    unlockTransNodeList( aSlot );
                
                    updateStatistics( sStatistics,
                                      IDV_STAT_INDEX_LOCK_RELEASED );
                    return IDE_SUCCESS;
                }//if aLockNode
            } // if aLockSlot != NULL

            if ( aLockNode->mPrvTransLockNode != NULL )
            {
                // Ʈ����� lock list array����
                // transaction�� slot id�� �ش��ϴ�
                // list���� lock node�� �����Ѵ�.
                removeLockNode(aLockNode);
            }

            sState = 0;
            unlockTransNodeList( aSlot );

            IDE_TEST( freeLockNode( aLockNode ) != IDE_SUCCESS );
            aLockNode = NULL;

            updateStatistics( sStatistics,
                              IDV_STAT_INDEX_LOCK_RELEASED );

            // ���͸� �����ϰų� �����ϰ� ����
            return IDE_SUCCESS;
        }
    }
    else // ( aLockNode->mDoRemove == ID_TRUE )
    {
        /* �ַ� lock escalation �� �ӽ� node�̴�
         * ȣ�� �󵵰� ����. unlockTableInternal���� ó���Ѵ�.*/
    }
    sState = 0;        
    unlockTransNodeList( aSlot );

    IDE_TEST( unlockTableInternal( sStatistics,
                                   aSlot,
                                   sLockItem,
                                   aLockNode,
                                   aLockSlot,
                                   aDoMutexLock ) != IDE_SUCCESS );

    updateStatistics( sStatistics,
                      IDV_STAT_INDEX_LOCK_RELEASED );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sState != 0 )
    {
        unlockTransNodeList( aSlot );
    }

    return IDE_FAILURE;

}

/*********************************************************
  function description: unlockTable

 unlockTableInternal �� ȣ��Ǵ� ���

  1 mutex mode unlock Table
  2 Timeout ����ó��
    2-1 Timeout������ �̹� Grant �ع��� ���
    2-2 CVS Node�� ��� DoRemove == ID_TRUE
    2-3 Request List�� ����Ǿ� �ִ� ���

 **************************************************    
 
  �Ʒ��� ���� �پ��� case�� ���� ���� ó���Ѵ�.
  0. lock node�� �̹�table�� grant or wait list����
     �̹� ���� �Ǿ� �ִ� ���.
     - lock node�� Ʈ������� lock list���� �޷�������,
       lock node�� Ʈ������� lock list���� �����Ѵ�.

  1. Lock Item Mutex�� ��� ���� Light ��� �� ���
     Light Mode Lock Table�� �����ϰ� �ٷ� ����������.

  2. if (lock node�� grant�� ���� �ϰ��)
     then
      table lock�������� lock node�� lock mode��  1����
      ��Ű�� table ��ǥ�� ���Žõ�.
      aLockNode�� ��ǥ��  &= ~(aLockSlot�� mMask).
      if( aLock node�� ��ǥ��  != 0)
      then
        aLockNode�� lockMode�� �����Ѵ�.
        ���ŵ� lock node�� mLockMode�� table lock�������� 1����.
        lock node���� �϶�� flag�� off��Ų��.
        --> lockTable�� 2.2������ ������ Ʈ������� grant ��
        lock node�� ���Ͽ�, grant lock node�� �߰��ϴ� ��ſ�
        lock mode�� conversion�Ͽ� lock slot��
        add�� ����̴�. �� node�� �ϳ�������, �׾ȿ�
        lock slot�� �ΰ� �̻��� �ִ� ����̴�.
        4��° �ܰ�� �Ѿ��.
      fi
      grant list���� lock node�� ���Ž�Ų��.
      table lock�������� grant count 1 ����.
      ���ο� Grant Lock Mode�� �����Ѵ�.
    else
      // lock node�� request list�� �޷� �ִ� ���.
       request list���� lock node�� ����.
       request count����.
       grant count�� 0�̸� 5���ܰ�� �Ѿ.
    fi
  3. waiting table�� clear�Ѵ�.
     grant lock node�̰ų�, request list�� �־��� lock node
     ��  ������ Free�� ��� �� Transaction�� ��ٸ��� �ִ�
     Transaction��  waiting table���� �����Ͽ� �ش�.
     ������ request�� �־��� lock node�� ���,
     Grant List�� Lock Node��
     ���� �ִ°��� ���� �ʴ´�.

  4-1. Lock Item�� ���̻� S,X,SIX �� ���� ���
     Lock Node�� �����ϰ� Light Mode�� �����Ѵ�. BUG-47388

  4-2. lock�� wait�ϰ� �ִ� Transaction�� �����.
     ���� lock node :=  table lock�������� request list����
                        ��ó�� lock node.
     while( ���� lock node != null)
     begin loop.
      if( ���� table�� grant mode�� ���� lock node�� lock mode�� ȣȯ?)
      then
         request ����Ʈ���� Lock Node�� �����Ѵ�.
         table lock�������� grant lock mode�� �����Ѵ�.
         if(���� lock node�� cvs node�� null�� �ƴϸ�)
         then
            table lock �������� ���� lock node��
            cvs node�� lock mode�� 1���ҽ�Ű��,��ǥ�� ���Žõ�.
            cvs node�� lock mode�� �����Ѵ�.
            table lock �������� cvs node��
            lock mode�� 1���� ��Ű�� ��ǥ���� ����.
         else
            table lock �������� ���� lock node�� lock mode��
            1���� ��Ű�� ��ǥ���� ����.
            Grant����Ʈ�� ���ο� Lock Node�� �߰��Ѵ�.
            ���� lock node�� grant flag�� on ��Ų��.
            grant count�� 1���� ��Ų��.
         fi //���� lock node�� cvs node�� null�� �ƴϸ�
      else
      //���� table�� grant mode�� ���� lock node�� lock mode��
      // ȣȯ���� �ʴ� ���.
        if(table�� grant count == 1)
        then
           if(���� lock node�� cvs node�� �ִ°�?)
           then
             //cvs node�� �ٷ�  grant�� 1�� node�̴�.
              table lock �������� cvs lock node�� lock mode��
              1���� ��Ű��, table ��ǥ�� ���Žõ�.
              table lock �������� grant mode����.
              cvs lock node�� lock mode�� ���� table�� grant mode
              ���� ����.
              table lock �������� ���� grant mode�� 1���� ��Ű��,
              ��ǥ�� ����.
              Request ����Ʈ���� Lock Node�� �����Ѵ�.
           else
              break; // nothint to do
           fi
        else
           break; // nothing to do.
        fi
      fi// lock node�� lock mode�� ȣȯ���� �ʴ� ���.

      table lock�������� request count 1����.
      ���� lock node�� slot id��  waiting�ϰ� �ִ� row����
      waiting table���� clear�Ѵ�.
      waiting�ϰ� �ִ� Ʈ������� resume��Ų��.
     loop end.

  5. lock slot, lock node ���� �õ�.
     - lock slot �� null�� �ƴϸ�,lock slot�� transaction
        �� lock slot list���� �����Ѵ�.
     - 2�� �ܰ迡�� lock node�� �����϶�� flag�� on
     �Ǿ� ������  lock node��  Ʈ������� lock list����
     ���� �ϰ�, lock node�� free�Ѵ�.
***********************************************************/
IDE_RC smlLockMgr::unlockTableInternal( idvSQL      * aStatistics,
                                        SInt          aSlot,
                                        smlLockItem * aLockItem,
                                        smlLockNode * aLockNode,
                                        smlLockSlot * aLockSlot,
                                        idBool        aDoMutexLock )
{
    smlLockNode  * sCurLockNode;
    idBool         sDoFreeLockNode = ID_TRUE;
    idBool         sUnlinkAll;
    UInt           sState = 0;

    // lock node�� �̹� request or grant list���� ���ŵ� ����.
    if ( aLockNode->mDoRemove == ID_TRUE )
    {
        // �ַ� lock escalation �� ���� �ӽ� lock node
        IDE_ASSERT( aDoMutexLock == ID_FALSE );
        IDE_ASSERT( aLockNode->mCvsLockNode != NULL );
        IDE_ASSERT( ( aLockNode->mPrvLockNode == NULL ) &&
                    ( aLockNode->mNxtLockNode == NULL ) );

        IDE_CONT( unlock_COMPLETE );
    }

    if ( aDoMutexLock == ID_TRUE )
    {
        // lockTable �߿� TimeOut���� unlock�ϴ� ���
        (void)aLockItem->mMutex.lock( aStatistics );
        sState = 1;
    }

    // LockItem�� ��� ���Դµ� ��ħ Ǯ���� ���
    if ( aLockItem->mFlag == SML_FLAG_LIGHT_MODE )
    {
        IDE_ASSERT( aDoMutexLock == ID_TRUE );
        IDE_ASSERT( aLockNode->mNxtLockNode == NULL );
        IDE_ASSERT( aLockNode->mPrvLockNode == NULL );
        IDE_ASSERT( aLockNode->mLockMode != SML_XLOCK );
        IDE_ASSERT( aLockNode->mLockMode != SML_SLOCK );
        IDE_ASSERT( aLockNode->mLockMode != SML_SIXLOCK );
        IDE_ASSERT( aLockNode->mBeGrant == ID_TRUE );

        if ( aLockSlot != NULL )
        {
            aLockNode->mFlag  &= ~(aLockSlot->mMask);
            // lockTable�� 2.2������ ������ Ʈ������� grant ��
            // lock node�� ���Ͽ�, grant lock node�� �߰��ϴ� ��ſ�
            // lock mode�� conversion�Ͽ� lock slot��
            // add�� ����̴�. �� node�� �ϳ�������, �׾ȿ�
            // lock slot�� �ΰ� �̻��� �ִ� ����̴�.
            // -> ���� lock node�� �����ϸ� �ȵȴ�.
            //   sDoFreeLockNode = ID_FALSE;
            removeLockSlot( aLockSlot );

            if ( aLockNode->mFlag != 0 )
            {
                aLockNode->mLockMode = getDecision( aLockNode->mFlag );

                sDoFreeLockNode = ID_FALSE;
            }
        }

        IDE_CONT( unlock_COMPLETE );
    }

    // ���� ���
    // Grant�� X, S, SIX �� ���,
    // Ȥ�� X,S,SIX�� ������̰� IS, IX �� ���
    if ( aLockNode->mBeGrant == ID_TRUE )
    {
        decTblLockModeAndTryUpdate( aLockItem, aLockNode->mLockMode );

        while ( 1 )
        {
            if ( aLockSlot != NULL )
            {
                aLockNode->mFlag  &= ~(aLockSlot->mMask);
                // lockTable�� 2.2������ ������ Ʈ������� grant ��
                // lock node�� ���Ͽ�, grant lock node�� �߰��ϴ� ��ſ�
                // lock mode�� conversion�Ͽ� lock slot��
                // add�� ����̴�. �� node�� �ϳ�������, �׾ȿ�
                // lock slot�� �ΰ� �̻��� �ִ� ����̴�.
                // -> ���� lock node�� �����ϸ� �ȵȴ�.
                //   sDoFreeLockNode = ID_FALSE;
                if ( aLockNode->mFlag != 0 )
                {
                    aLockNode->mLockMode = getDecision( aLockNode->mFlag );
                    incTblLockModeAndUpdate( aLockItem,
                                             aLockNode->mLockMode );
                    sDoFreeLockNode = ID_FALSE;
                    break;
                }//if aLockNode
            } // if aLockSlot != NULL

            //Remove lock node from lock Grant list
            removeLockNode( aLockItem->mFstLockGrant,
                            aLockItem->mLstLockGrant,
                            aLockNode );
            aLockItem->mGrantCnt--;
            break;
        }
        //���ο� Grant Lock Mode�� �����Ѵ�.
        aLockItem->mGrantLockMode = getDecision( aLockItem->mFlag );
    }//if aLockNode->mBeGrant == ID_TRUE
    else
    {
        // Grant �Ǿ����� �ʴ� ����.
        //remove lock node from lock request list
        removeLockNode( aLockItem->mFstLockRequest,
                        aLockItem->mLstLockRequest, 
                        aLockNode );
        aLockItem->mRequestCnt--;

    }//else aLockNode->mBeGrant == ID_TRUE.

    if ( ( sDoFreeLockNode == ID_TRUE ) && ( aLockNode->mCvsLockNode == NULL ) )
    {
        // grant lock node�̰ų�, request list�� �־��� lock node
        //��  ������ Free�� ��� �� Transaction�� ��ٸ��� �ִ�
        //Transaction��  waiting table���� �����Ͽ� �ش�.
        //������ request�� �־��� lock node�� ���,
        // Grant List�� Lock Node��
        //���� �ִ°��� ���� �ʴ´�.
        clearWaitTableRows( aLockItem->mFstLockRequest,
                            aSlot );
    }

    // ( ���� X, S SIX �϶� ) && ( Grant���� �������´�. )
    if (( aLockItem->mGrantLockMode == SML_ISLOCK ) ||
        ( aLockItem->mGrantLockMode == SML_IXLOCK ) ||
        ( aLockItem->mGrantLockMode == SML_NLOCK ))
    {
        sUnlinkAll = ID_TRUE;
        for ( sCurLockNode = aLockItem->mFstLockRequest ;
              sCurLockNode != NULL ;
              sCurLockNode = sCurLockNode->mNxtLockNode )
        {
            // ��� IS, IX �� �ִٸ� Lock Item�� ��� ��ü�Ѵ�
            if (( sCurLockNode->mLockMode == SML_XLOCK ) ||
                ( sCurLockNode->mLockMode == SML_SLOCK ) ||
                ( sCurLockNode->mLockMode == SML_SIXLOCK ))
            {
                sUnlinkAll = ID_FALSE;
                break;
            }
        }
    }
    else
    {
        sUnlinkAll = ID_FALSE;
    }

    if ( sUnlinkAll == ID_TRUE )
    {
        /* ��ٸ��� Lock�� ���� DML �� ���
         * lock item�� ��� unlink�ϰ�, i lock ü���� �����Ѵ�.
         * ���� ���� SIX���ٸ� Grant List�� unlink �ؾ� �Ѵ�.*/

        IDE_TEST( toLightMode( aStatistics,
                               aLockItem ) != IDE_SUCCESS );
    }
    else // ��ٸ��� Lock�߿� DDL �� �ִ� ���
         // or ���� X, S,SIX �� �������� ���� ���
    {
        //lock�� ��ٸ��� Transaction�� �����.
        IDE_TEST( wakeupRequestLockNodeInLockItem( aStatistics,
                                                   aLockItem ) != IDE_SUCCESS );
    }

    if ( aLockSlot != NULL )
    {
        removeLockSlot( aLockSlot );
    }

    IDE_EXCEPTION_CONT( unlock_COMPLETE );

    if ( sDoFreeLockNode == ID_TRUE )
    {
        // Trans List�� ���� ���� �� �� �����Ƿ�
        // lock�� ���� �ʰ� Ȯ���ص� �ȴ�.
        if ( aLockNode->mPrvTransLockNode != NULL )
        {
            // Ʈ����� lock list array����
            // transaction�� slot id�� �ش��ϴ�
            // list���� lock node�� �����Ѵ�.
            lockTransNodeList( aStatistics, aSlot );
            removeLockNode(aLockNode);
            unlockTransNodeList( aSlot );
        }
    }

    if ( sState == 1 )
    {
        sState = 0;
        (void)aLockItem->mMutex.unlock();
    }

    if ( sDoFreeLockNode == ID_TRUE )
    {
        IDE_TEST( freeLockNode( aLockNode ) != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sState != 0 )
    {
        IDE_ASSERT( aLockItem->mMutex.unlock() == IDE_SUCCESS );
    }

    return IDE_FAILURE;

}

/*********************************************************
 * BUG-47388 Lock Table �������� Light Mode �߰�
 *
 * S, X, SIX --> IS, IX, N ���� ����Ǵ� ���
 * Lock Item�� Grant, Request List�� ��� Lock Node��
 * Light Mode�� �������ְ� Lock Item�� �ʱ�ȭ �Ѵ�.
 *********************************************************/
IDE_RC smlLockMgr::toLightMode( idvSQL      * aStatistics,
                                smlLockItem * aLockItem )
{
    smlLockNode  * sCurLockNode;

    if ( aLockItem->mGrantCnt > 0 )
    {
        sCurLockNode = aLockItem->mFstLockGrant;
        while ( sCurLockNode != NULL )
        {
            IDE_ASSERT(( sCurLockNode->mLockMode != SML_XLOCK ) &&
                       ( sCurLockNode->mLockMode != SML_SLOCK ) &&
                       ( sCurLockNode->mLockMode != SML_SIXLOCK ));
            removeLockNode( aLockItem->mFstLockGrant,
                            aLockItem->mLstLockGrant,
                            sCurLockNode );

            aLockItem->mGrantCnt--;
            clearWaitTableRows( aLockItem->mFstLockRequest,
                                sCurLockNode->mSlotID );

            sCurLockNode = aLockItem->mFstLockGrant;
        }
    }
    IDE_ASSERT( aLockItem->mGrantCnt == 0 );

    //lock�� ��ٸ��� Transaction�� �����.
    sCurLockNode = aLockItem->mFstLockRequest;
    while ( sCurLockNode != NULL )
    {
        IDE_ASSERT(( sCurLockNode->mLockMode != SML_XLOCK ) &&
                   ( sCurLockNode->mLockMode != SML_SLOCK ) &&
                   ( sCurLockNode->mLockMode != SML_SIXLOCK ));

        //Request ����Ʈ���� Lock Node�� �����Ѵ�.
        removeLockNode( aLockItem->mFstLockRequest,
                        aLockItem->mLstLockRequest,
                        sCurLockNode );

        // Lock Item�� Light Mode�� �����ϱ� ����(mFlag�� �����ϱ� ����)
        // Transaction Lock Node List�� �����ϰ� Lock Mode�� ���� �� �־�� �Ѵ�.
        // �ٽ� to Heavy �� ���� �ϴ� Transaction�� ���� ������� ���� Node�� ã�� ���� ���� �ִ�.
        if ( sCurLockNode->mCvsLockNode != NULL )
        {
            //cvs node�� lock mode�� �����Ѵ�.
            sCurLockNode->mCvsLockNode->mLockMode = mConversionTBL[sCurLockNode->mCvsLockNode->mLockMode][sCurLockNode->mLockMode];
            sCurLockNode->mDoRemove = ID_TRUE;
        }// if sCurLockNode->mCvsLockNode != NULL
        else
        {
            sCurLockNode->mBeGrant = ID_TRUE;
            IDE_ASSERT( sCurLockNode->mPrvTransLockNode == NULL );

            lockTransNodeList( aStatistics, sCurLockNode->mSlotID );
            addLockNode( sCurLockNode, sCurLockNode->mSlotID );
            unlockTransNodeList( sCurLockNode->mSlotID );
        }
        //waiting table���� aSlot�� �࿡��  ��⿭�� clear�Ѵ�.
        clearWaitItemColsOfTrans( ID_FALSE, sCurLockNode->mSlotID );

        // waiting�ϰ� �ִ� Ʈ������� resume��Ų��.
        IDE_TEST( smLayerCallback::resumeTrans( smLayerCallback::getTransBySID( sCurLockNode->mSlotID ) ) != IDE_SUCCESS );
        sCurLockNode = aLockItem->mFstLockRequest;
    }/* while */
    // aLockItem ����;

    // flag�� �ּ��� grant List�� ���� �� ������ SML_FLAG_LIGHT_MODE�� set �ؾ� �Ѵ�.
    // unlock�Ϸ� ���Դµ� flag�� 0�ε� ���� grant list�� ���� �Ǿ� �ִ� ��Ȳ�� �߻� �� �� �ִ�.
    clearLockItem( aLockItem );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************
 * Mutex Mode Lock Table���� Request List ���� Lock Node ��
 * Grant List�� �ű� �� �ִ� Lock Node�� �ű�� wakeup�Ѵ�.
 ***********************************************************/
IDE_RC smlLockMgr::wakeupRequestLockNodeInLockItem( idvSQL      * aStatistics,
                                                    smlLockItem * aLockItem )
{
    smlLockNode  * sCurLockNode;

    //lock�� ��ٸ��� Transaction�� �����.
    sCurLockNode = aLockItem->mFstLockRequest;

    while ( sCurLockNode != NULL )
    {
        //Wake up requestors
        //���� table�� grant mode�� ���� lock node�� lock mode�� ȣȯ?
        if ( mCompatibleTBL[sCurLockNode->mLockMode][aLockItem->mGrantLockMode] == ID_TRUE )
        {
            //Request ����Ʈ���� Lock Node�� �����Ѵ�.
            removeLockNode( aLockItem->mFstLockRequest,
                            aLockItem->mLstLockRequest,
                            sCurLockNode );

            aLockItem->mGrantLockMode =
                mConversionTBL[aLockItem->mGrantLockMode][sCurLockNode->mLockMode];
            if ( sCurLockNode->mCvsLockNode != NULL )
            {
                //table lock �������� ���� lock node��
                //cvs node��  lock mode�� 1���ҽ�Ű��,��ǥ�� ���Žõ�.
                decTblLockModeAndTryUpdate( aLockItem,
                                            sCurLockNode->mCvsLockNode->mLockMode );
                //cvs node�� lock mode�� �����Ѵ�.
                sCurLockNode->mCvsLockNode->mLockMode =
                    mConversionTBL[sCurLockNode->mCvsLockNode->mLockMode][sCurLockNode->mLockMode];
                //table lock �������� cvs node��
                //lock mode�� 1���� ��Ű�� ��ǥ���� ����.
                incTblLockModeAndUpdate( aLockItem,
                                         sCurLockNode->mCvsLockNode->mLockMode );
                sCurLockNode->mDoRemove = ID_TRUE;
            }// if sCurLockNode->mCvsLockNode != NULL
            else
            {
                incTblLockModeAndUpdate( aLockItem, sCurLockNode->mLockMode );
                //Grant����Ʈ�� ���ο� Lock Node�� �߰��Ѵ�.
                addLockNodeToTail( aLockItem->mFstLockGrant,
                                   aLockItem->mLstLockGrant,
                                   sCurLockNode );
                sCurLockNode->mBeGrant = ID_TRUE;
                aLockItem->mGrantCnt++;

                IDE_ASSERT( sCurLockNode->mPrvTransLockNode == NULL );

                lockTransNodeList( aStatistics, sCurLockNode->mSlotID );
                addLockNode( sCurLockNode, sCurLockNode->mSlotID );
                unlockTransNodeList( sCurLockNode->mSlotID );
            }//else sCurLockNode->mCvsLockNode�� NULL
        }
        // ���� table�� grant lock mode�� lock node�� lockmode
        // �� ȣȯ���� �ʴ� ���.
        else
        {
            if ( aLockItem->mGrantCnt == 1 )
            {
                if ( sCurLockNode->mCvsLockNode != NULL )
                {
                    // cvs node�� �ٷ� grant�� 1�� node�̴�.
                    //���� lock�� ���� table�� ���� lock�� Converion �̴�.
                    decTblLockModeAndTryUpdate( aLockItem,
                                                sCurLockNode->mCvsLockNode->mLockMode );

                    aLockItem->mGrantLockMode =
                        mConversionTBL[aLockItem->mGrantLockMode][sCurLockNode->mLockMode];
                    sCurLockNode->mCvsLockNode->mLockMode = aLockItem->mGrantLockMode;
                    incTblLockModeAndUpdate( aLockItem, aLockItem->mGrantLockMode );
                    //Request ����Ʈ���� Lock Node�� �����Ѵ�.
                    removeLockNode( aLockItem->mFstLockRequest,
                                    aLockItem->mLstLockRequest, 
                                    sCurLockNode );
                    sCurLockNode->mDoRemove = ID_TRUE;
                }
                else
                {
                    break;
                } // sCurLockNode->mCvsLockNode�� null
            }//aLockItem->mGrantCnt == 1
            else
            {
                break;
            }//aLockItem->mGrantCnt != 1
        }//mCompatibleTBL

        aLockItem->mRequestCnt--;
        //waiting table���� aSlot�� �࿡��  ��⿭�� clear�Ѵ�.
        clearWaitItemColsOfTrans( ID_FALSE, sCurLockNode->mSlotID );
        // waiting�ϰ� �ִ� Ʈ������� resume��Ų��.
        IDE_TEST( smLayerCallback::resumeTrans( smLayerCallback::getTransBySID( sCurLockNode->mSlotID ) )
                  != IDE_SUCCESS );
        sCurLockNode = aLockItem->mFstLockRequest;
    }/* while */

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


void smlLockMgr::dumpLockWait()
{
    SInt    i;
    SInt    j;
    SInt    k; /* for preventing from infinite loop */
    smTID   sTID;         // Current Waiting Tx
    smTID   sWaitForTID;  // Wait For Target Tx
    idBool  sWaited = ID_FALSE;

    SInt   sTransCnt = smLayerCallback::getCurTransCnt();

    ideLogEntry sLog( IDE_DUMP_0 );
    sLog.appendFormat( "LOCK WAIT INFO:\n"
                       "%8s,%16s \n",
                       "TRANS_ID",
                       "WAIT_FOR_TRANS_ID" );

    for ( j = 0; j < sTransCnt; j++ )
    {
        if ( smLayerCallback::isActiveBySID(j) == ID_TRUE)
        {
            for ( k = 0, i = smlLockMgr::mArrOfLockList[j].mFstWaitTblTransItem;
                  (i != SML_END_ITEM) && (i != ID_USHORT_MAX) && (k < sTransCnt);
                  i = smlLockMgr::mWaitForTable[j][i].mNxtWaitTransItem, k++ )
            {
                if (smlLockMgr::mWaitForTable[j][i].mIndex == 1)
                {
                    sWaited = ID_TRUE;
                    sTID        = smLayerCallback::getTIDBySID(j);
                    sWaitForTID = smLayerCallback::getTIDBySID(i);

                    sLog.appendFormat( "%8u,%16u \n",
                                       sTID,
                                       sWaitForTID );
                }
            }
        }
    }

    if( sWaited == ID_TRUE )
    {
        sLog.write();
    }
}

IDE_RC smlLockMgr::dumpLockTBL()
{
    smcTableHeader *sCatTblHdr;
    smcTableHeader *sTableHeader;
    smpSlotHeader  *sPtr;
    SChar          *sCurPtr;
    SChar          *sNxtPtr;

    sCatTblHdr = (smcTableHeader*)SMC_CAT_TABLE;
    sCurPtr    = NULL;

    // [1] ���̺� ����� ��ȸ�ϸ鼭 ���� lock node list�� ���Ѵ�.
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
            /* BUG-14974: ���� Loop�߻�.*/
            sCurPtr = sNxtPtr;
            continue;
        }

        sTableHeader = (smcTableHeader *)( sPtr + 1 );

        // 1. temp table�� skip ( PROJ-2201 TempTable�� ���� ���� ���� */
        // 2. drop�� table�� skip
        // 3. meta  table�� skip

        if( ( SMI_TABLE_TYPE_IS_META( sTableHeader ) == ID_TRUE ) ||
            ( smcTable::isDropedTable(sTableHeader) == ID_TRUE ) )
        {
            sCurPtr = sNxtPtr;
            continue;
        }

        getLockItemNodes( (smlLockItem*)sTableHeader->mLock );
        sCurPtr = sNxtPtr;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

void smlLockMgr::getLockItemNodes( smlLockItem * aLockItem )
{
    smlLockNode * sCurLockNode;
    idBool        sGranted   = ID_FALSE;
    idBool        sRequested = ID_FALSE;
    SChar         sLockModeStr[][100] = {"SML_NLOCK",
                                         "SML_SLOCK",
                                         "SML_XLOCK",
                                         "SML_ISLOCK",
                                         "SML_IXLOCK",
                                         "SML_SIXLOCK"};

    aLockItem->mMutex.lock(NULL /* idvSQL* */);

    ideLogEntry sTitleLog( IDE_DUMP_0 );
    sTitleLog.appendFormat( "LOCK INFO:\n"
                            "SPACEID    : %"ID_UINT32_FMT"\n"
                            "TABLEOID   : %"ID_UINT64_FMT"\n"
                            "LOCK TYPE  : %"ID_UINT32_FMT"\n"
                            "LOCK MODE  : %s (%"ID_INT32_FMT")\n"
                            "GRANT CNT  : %"ID_INT32_FMT"\n"
                            "REQUEST CNT: %"ID_INT32_FMT"\n",
                            aLockItem->mSpaceID,
                            aLockItem->mItemID,
                            aLockItem->mLockItemType,
                            sLockModeStr[aLockItem->mGrantLockMode],
                            aLockItem->mGrantLockMode,
                            aLockItem->mGrantCnt,
                            aLockItem->mRequestCnt );


    ideLogEntry sGrantLog( IDE_DUMP_0 );
    sGrantLog.appendFormat( "LOCK GRANT INFO:\n"
                            "%8s, %8s, %8s, %8s, %8s, %8s, %8s \n",
                            "SLOTID",
                            "TABLEOID",
                            "TRANSID",
                            "TRANSID",
                            "LOCKMODE",
                            "LOCKCNT",
                            "IS_GRANT" );
    // Grant Lock Node list
    sCurLockNode = aLockItem->mFstLockGrant;

    while ( sCurLockNode != NULL )
    {
        sGranted = ID_TRUE;
        sGrantLog.appendFormat( "%8u, %8lu, %8u, %8u, %8s, %8u, %8u \n",
                                sCurLockNode->mSlotID,
                                sCurLockNode->mItemID,
                                smLayerCallback::getTransID(smLayerCallback::getTransBySID(sCurLockNode->mSlotID)),
                                sCurLockNode->mTransID,
                                sLockModeStr[sCurLockNode->mLockMode],
                                sCurLockNode->mLockCnt,
                                sCurLockNode->mBeGrant );

        sCurLockNode = sCurLockNode->mNxtLockNode;
    }


    ideLogEntry sRequestLog( IDE_DUMP_0 );
    sRequestLog.appendFormat( "LOCK REQUEST INFO:\n"
                              "%8s, %8s, %8s, %8s, %8s, %8s, %8s \n",
                              "SLOTID",
                              "TABLEOID",
                              "TRANSID",
                              "TRANSID",
                              "LOCKMODE",
                              "LOCKCNT",
                              "IS_GRANT" );
    // Request Lock Node list
    sCurLockNode = aLockItem->mFstLockRequest;

    while ( sCurLockNode != NULL )
    {
        sRequested = ID_TRUE;
        sRequestLog.appendFormat( "%8u, %8lu, %8u, %8u, %8s, %8u, %8u \n",
                                  sCurLockNode->mSlotID,
                                  sCurLockNode->mItemID,
                                  smLayerCallback::getTransID(smLayerCallback::getTransBySID(sCurLockNode->mSlotID)),
                                  sCurLockNode->mTransID,
                                  sLockModeStr[sCurLockNode->mLockMode],
                                  sCurLockNode->mLockCnt,
                                  sCurLockNode->mBeGrant );

        sCurLockNode = sCurLockNode->mNxtLockNode;
    }

    aLockItem->mMutex.unlock();

    if( ( sGranted == ID_TRUE ) || (sRequested == ID_TRUE ) )
    {
        sTitleLog.write();
    }

    if( sGranted == ID_TRUE )
    {
        sGrantLog.write();
    }

    if( sRequested == ID_TRUE )
    {
        sRequestLog.write();
    }
    return;
}
