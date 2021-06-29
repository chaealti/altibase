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

#include <iduMemMgr.h>
#include <iduMemPool.h>
#include <iduHashUtil.h>
#include <mmuProperty.h>
#include <mmcStatement.h>
#include <mmcPlanCache.h>
#include <mmcSQLTextHash.h>
#include <mmcPCB.h>
#include <mmcParentPCO.h>
#include <mmcChildPCO.h>
#include <mmcPlanCacheLRUList.h>

iduMemPool             mmcPlanCache::mPCBMemPool;
iduMemPool             mmcPlanCache::mParentPCOMemPool;
iduMemPool             mmcPlanCache::mChildPCOMemPool;
mmcPlanCacheSystemInfo mmcPlanCache::mPlanCacheSystemInfo;
iduLatch               mmcPlanCache::mPerfViewLatch;   // PROJ-2408

const SChar  *const  mmcPlanCache::mCacheAbleText[] =
{
    "SELECT",
    "INSERT",
    "DELETE",
    "UPDATE",
    "MOVE",
    "ENQUEUE",
    "DEQUEUE",
    "WITH",     // PROJ-2206 with clause
    "MERGE",    // PROJ-1988 MERGE query
    (SChar*) NULL
};


IDE_RC mmcPlanCache::initialize()
{
    // property load.
    
    IDE_TEST(mmcSQLTextHash::initialize(mmuProperty::getSqlPlanCacheBucketCnt())
             != IDE_SUCCESS);
    
    mmcPlanCacheLRUList::initialize();
    
    IDE_TEST(mPCBMemPool.initialize(IDU_MEM_MMPLAN_CACHE_CONTROL,
                                    (SChar*)"SQL_PLAN_CACHE_PCB_POOL",
                                    ID_SCALABILITY_SYS,
                                    ID_SIZEOF(mmcPCB),
                                    mmuProperty::getSqlPlanCacheInitPCBCnt(),
                                    IDU_AUTOFREE_CHUNK_LIMIT,			/* ChunkLimit */
                                    ID_TRUE,							/* UseMutex */
                                    IDU_MEM_POOL_DEFAULT_ALIGN_SIZE,	/* AlignByte */
                                    ID_FALSE,							/* ForcePooling */
                                    ID_TRUE,							/* GarabageCollection */
                                    ID_TRUE,                            /* HWCacheLine */
                                    IDU_MEMPOOL_TYPE_LEGACY             /* mempool type*/) 
             != IDE_SUCCESS);			
    
    IDE_TEST(mParentPCOMemPool.initialize(IDU_MEM_MMPLAN_CACHE_CONTROL,
                                          (SChar*)"SQL_PLAN_CACHE_PARENT_PCO_POOL",
                                          ID_SCALABILITY_SYS,
                                          ID_SIZEOF(mmcParentPCO),
                                          mmuProperty::getSqlPlanCacheInitParentPCOCnt(),
                                          IDU_AUTOFREE_CHUNK_LIMIT,			/* ChunkLimit */
                                          ID_TRUE,							/* UseMutex */
                                          IDU_MEM_POOL_DEFAULT_ALIGN_SIZE,	/* AlignByte */
                                          ID_FALSE,							/* ForcePooling */
                                          ID_TRUE,							/* GarbageCollection */
                                          ID_TRUE,                          /* HWCacheLine */
                                          IDU_MEMPOOL_TYPE_LEGACY           /* mempool type*/) 
             != IDE_SUCCESS);			

    IDE_TEST(mChildPCOMemPool.initialize(IDU_MEM_MMPLAN_CACHE_CONTROL,
                                         (SChar*)"SQL_PLAN_CACHE__PCO_POOL",
                                         ID_SCALABILITY_SYS,
                                         ID_SIZEOF(mmcChildPCO),
                                         mmuProperty::getSqlPlanCacheInitChildPCOCnt(),
                                         IDU_AUTOFREE_CHUNK_LIMIT,			/* ChunkLimit */
                                         ID_TRUE,							/* UseMutex */
                                         IDU_MEM_POOL_DEFAULT_ALIGN_SIZE,	/* AlignByte */
                                         ID_FALSE,							/* ForcePooling */
                                         ID_TRUE,							/* GarbageCollection */
                                         ID_TRUE,                          /* HWCacheLine */
                                         IDU_MEMPOOL_TYPE_LEGACY           /* mempool type*/) 
             != IDE_SUCCESS);			
    idlOS::memset(&mPlanCacheSystemInfo,0x00, ID_SIZEOF(mPlanCacheSystemInfo));
    // PROJ-2408
    IDE_ASSERT ( mPerfViewLatch.initialize( (SChar *)"SQL_PLAN_CACHE_PERF_VIEW_LATCH" )
               == IDE_SUCCESS );
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

IDE_RC mmcPlanCache::finalize()
{
    mmcPlanCacheLRUList::finalize();
    
    IDE_TEST(mmcSQLTextHash::finalize() != IDE_SUCCESS);
    
    IDE_TEST(mPCBMemPool.destroy() != IDE_SUCCESS);
    IDE_TEST(mParentPCOMemPool.destroy() != IDE_SUCCESS);
    IDE_TEST(mChildPCOMemPool.destroy() != IDE_SUCCESS);
    // PROJ-2408
    IDE_ASSERT(mPerfViewLatch.destroy() == IDE_SUCCESS);
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/**********************************************************************
 * Description: statement�� SQL������ ������ SQL Plan cache���� SQL�����
 * ������ ������ ������ �ִ� Parent PCO Object�� ã�´�.
 * Parent PCO�� ã����,  �ش� Parent PCO�� �ִ� child PCO����Ʈ�� ��ȸ
 * �ϸ鼭  environment�� ��ġ�Ǵ� chold PCO�� ã�� child PCO�� handle��
 * PCB�� assign�Ѵ�.
 *
 * Action: SQL������ hash key value�� ����ϰ� , ���� bucket�� ���Ѵ�.
 *         bucket latch�� s-latch�� �Ǵ�.
 *         �ش� bucket chain��  ���󰡸鼭 HashKey value�� ���ڿ��� ����
 *         Parent PCO�� Ž���ϸ�,ã�� parent PCO���� ���Ͽ� prepare-latch��
 *         s-latch�� �Ǵ�.
 *         Parent PCO�� ã�Ұų� Ž���� �Ϸ�Ǹ�  bucketLatch�� Ǭ��.
 *         Parent PCO�� used child list����  environment�� match�Ǵ�
 *         child PCO�� �˻��Ѵ�.
 *       
 ***********************************************************************/
void  mmcPlanCache::searchSQLText(mmcStatement     *aStatement,
                                  mmcParentPCO     **aFoundedParentPCO,
                                  vULong           *aHashKeyVal,
                                  mmcPCOLatchState *aPCOLatchState )
{

    UInt          sBucket;
    iduListNode * sDummy;
    UInt          sQueryLen;
    UInt          sQueryHashMax;

    sQueryLen     = aStatement->getQueryLen();
    sQueryHashMax = iduProperty::getQueryHashStringLengthMax();

    // BUG-36203
    if( sQueryLen > sQueryHashMax )
    {
        sQueryLen = sQueryHashMax;
    }
    else
    {
        // Nothing to do.
    }

    *aHashKeyVal = iduHashUtil::hashString(IDU_HASHUTIL_INITVALUE,
                                           (const UChar*)aStatement->getQueryString(),
                                           sQueryLen );
    sBucket = mmcSQLTextHash::hash(*aHashKeyVal);
    //�ش� bucket�� ���Ͽ� s-latch�� �Ǵ�.
    mmcSQLTextHash::latchBucketAsShared(aStatement->getStatistics(),
                                        sBucket);
    //bucket chain�� ���󰡸鼭 SQLText�� �ش��ϴ� Parent PCO�� ã��,
    // ã�� Parent PCO�� ���Ͽ�  prepare-latch�� s-latch�� �ɰ� �������´�.
    mmcSQLTextHash::searchParentPCO(aStatement->getStatistics(),
                                    sBucket,
                                    aStatement->getQueryString(),
                                    *aHashKeyVal,
                                    aStatement->getQueryLen(),
                                    aFoundedParentPCO,
                                    &sDummy,
                                    aPCOLatchState);
    mmcSQLTextHash::releaseBucketLatch(sBucket);
    
}

//SQL Text�� �ش��ϴ� Parent PCO  insert�õ����Ѵ�.
// ������ insert �� �����ϴ� statement��
// Parent PCO, child PCO, PCB�� �����ϰ�,
// child PCO�� prepare-latch�� x-latch�� �ɰ�
// chain�� Parent PCO�� �߰��Ѵ�.
IDE_RC  mmcPlanCache::tryInsertSQLText(idvSQL         *aStatistics,
                                       SChar          *aSQLText,
                                       UInt            aSQLTextLen,
                                       vULong          aHashKeyVal,
                                       mmcParentPCO    **aFoundedParentPCO,
                                       mmcPCB          **aPCB,
                                       idBool          *aSuccess,
                                       mmcPCOLatchState *aPCOLatchState)
{
    UInt            sBucket;
    mmcParentPCO   *sParentPCO;
    mmcChildPCO    *sChildPCO;
    iduListNode    *sInsertAfterNode;
    UChar           sState = 0;
    
    sBucket = mmcSQLTextHash::hash(aHashKeyVal);
    mmcSQLTextHash::latchBucketAsExeclusive(aStatistics,
                                            sBucket);
    sState =1;
    //bucket chain�� ���󰡸鼭 SQLText�� �ش��ϴ� Parent PCO�� ã��,
    // ã�� Parent PCO�� ���Ͽ�  prepare-latch�� s-latch�� �ɰ� �������´�.
    mmcSQLTextHash::searchParentPCO(aStatistics,
                                    sBucket,
                                    aSQLText,
                                    aHashKeyVal,
                                    aSQLTextLen,
                                    aFoundedParentPCO,
                                    &sInsertAfterNode,
                                    aPCOLatchState);
    if((*aFoundedParentPCO) != NULL)
    {
        //SQL Text�� �ش��ϴ� Parent PCO�� �ִ� ���.
        *aSuccess = ID_FALSE;
    }
    else
    {
        *aSuccess = ID_TRUE;

        IDU_FIT_POINT( "mmcPlanCache::tryInsertSQLText::alloc::ParentPCO" );

        // ���ʷ� SQLText�� �ش��ϴ� Parent PCO�� insert�ϴ� statement.
        // parent PCO�� �Ҵ��Ѵ�.
        IDE_TEST(mParentPCOMemPool.alloc((void**)&sParentPCO) != IDE_SUCCESS);
        sState = 2;

        IDU_FIT_POINT( "mmcPlanCache::tryInsertSQLText::alloc::PCB" );

        // PCB�� child PCO�� �Ҵ�޴´�.
        IDE_TEST( mPCBMemPool.alloc((void**)aPCB) != IDE_SUCCESS);
        sState = 3;

        IDU_FIT_POINT( "mmcPlanCache::tryInsertSQLText::alloc::ChildPCO" );

        IDE_TEST( mChildPCOMemPool.alloc((void**)&sChildPCO) != IDE_SUCCESS);
        sState = 4;

        //parent PCO�� �ʱ�ȭ�Ѵ�.
        /* BUG-29865
         * �޸� �Ѱ��Ȳ���� ParentPCO->initialize�� ������ �� �ֽ��ϴ�.
         */
        IDE_TEST(sParentPCO->initialize(mmcSQLTextHash::getSQLTextId(sBucket),
                                       aSQLText,
                                       aSQLTextLen,
                                       aHashKeyVal,
                                       sBucket) != IDE_SUCCESS);

        mmcSQLTextHash::incSQLTextId(sBucket);
        
        sState = 5;
        // PCB�� child PCO�� �ʱ�ȭ �Ѵ�.
        //fix BUG-21429
        (*aPCB)->initialize((SChar*)(sParentPCO->mSQLTextId),
                            0);
        sState = 6;        
        sChildPCO->initialize((SChar*)(sParentPCO->mSQLTextId),
                              0,
                              MMC_CHILD_PCO_IS_CACHE_MISS,
                              0,
                              (*aPCB)->getFrequencyPtr());
        sState = 7;
        (*aPCB)->assignPCO(aStatistics,
                        sParentPCO,
                        sChildPCO);
        // PVO�� ���Ͽ�  child PCO�� prepare-latch�� x-mode�� �Ǵ�.
        sChildPCO->latchPrepareAsExclusive(aStatistics);
        // parent PCO�� used child list��  child PCO��  append�Ѵ�.
        sParentPCO->incChildCreateCnt();
        sParentPCO->addPCBOfChild(*aPCB);
        // last�� �޸��� node , max hash value update.
        mmcSQLTextHash::tryUpdateBucketMaxHashVal(sBucket,
                                                  sInsertAfterNode,
                                                  aHashKeyVal);
        
        // bucket chain�� add.
        IDU_LIST_ADD_AFTER(sInsertAfterNode,sParentPCO->getBucketChainNode());
    }
    
    mmcSQLTextHash::releaseBucketLatch(sBucket);
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    {
        switch(sState)
        {
            case 7:
                sChildPCO->finalize();
            case 6:
                (*aPCB)->finalize();
            case 5:
                sParentPCO->finalize();
            case 4:
                mChildPCOMemPool.memfree(sChildPCO);
            case 3:
                mPCBMemPool.memfree(*aPCB);
            case 2:
                mParentPCOMemPool.memfree(sParentPCO);
            case 1:
                mmcSQLTextHash::releaseBucketLatch(sBucket);
        }
    }
    return IDE_FAILURE;
}

                                     
IDE_RC mmcPlanCache::freePCB(mmcPCB* aPCB)
{
    return mPCBMemPool.memfree(aPCB);
}

IDE_RC mmcPlanCache::freePCO(mmcParentPCO* aPCO)
{
    return mParentPCOMemPool.memfree(aPCO);
}

IDE_RC mmcPlanCache::freePCO(mmcChildPCO* aPCO)
{
    return mChildPCOMemPool.memfree(aPCO);
}

// BUG-23144 �ζ��� �Լ����� ���ü� ��� �ϸ� ��Ȥ �׽��ϴ�.
// �ζ����� ó������ �ʽ��ϴ�.
void mmcPlanCache::incCacheMissCnt(idvSQL  *aStatSQL)
{
    // PROJ-2408
    IDE_ASSERT( mPerfViewLatch.lockWrite(aStatSQL, NULL) == IDE_SUCCESS );
    mPlanCacheSystemInfo.mCacheMissCnt++;    

    IDE_ASSERT( mPerfViewLatch.unlock() == IDE_SUCCESS );
}

void mmcPlanCache::incCacheInFailCnt(idvSQL  *aStatSQL)
{
    // PROJ-2408
    IDE_ASSERT( mPerfViewLatch.lockWrite(aStatSQL, (idvWeArgs*)NULL) == IDE_SUCCESS );
    mPlanCacheSystemInfo.mCacheInFailCnt++;

    IDE_ASSERT( mPerfViewLatch.unlock() == IDE_SUCCESS );
}

void mmcPlanCache::incNoneCacheSQLTryCnt(idvSQL  *aStatSQL)
{
    // PROJ-2408
    IDE_ASSERT( mPerfViewLatch.lockWrite(aStatSQL, (idvWeArgs*)NULL) == IDE_SUCCESS );
    mPlanCacheSystemInfo.mNoneCacheSQLTryCnt++;

    IDE_ASSERT( mPerfViewLatch.unlock() == IDE_SUCCESS );
}

void mmcPlanCache::incCacheInsertCnt(idvSQL  *aStatSQL)
{
    // PROJ-2408
    IDE_ASSERT( mPerfViewLatch.lockWrite(aStatSQL, (idvWeArgs*)NULL) == IDE_SUCCESS );
    mPlanCacheSystemInfo.mCacheInsertedCnt++;
    mPlanCacheSystemInfo.mCurrentCacheObjCnt++;

    IDE_ASSERT( mPerfViewLatch.unlock() == IDE_SUCCESS );
}

void mmcPlanCache::updateCacheOutCntAndSize(idvSQL  *aStatSQL,
                                            UInt     aCacheOutCnt,
                                            ULong    aReducedSize)
{
    // PROJ-2408
    IDE_ASSERT( mPerfViewLatch.lockWrite(aStatSQL, (idvWeArgs*)NULL) == IDE_SUCCESS );

    mPlanCacheSystemInfo.mCacheOutCnt += aCacheOutCnt;
    mPlanCacheSystemInfo.mCurrentCacheSize -= aReducedSize;
    mPlanCacheSystemInfo.mCurrentCacheObjCnt-= aCacheOutCnt;
    IDE_ASSERT( mPerfViewLatch.unlock() == IDE_SUCCESS );
}

void mmcPlanCache::incCacheSize(ULong    aAddedSize)
{
    mPlanCacheSystemInfo.mCurrentCacheSize += aAddedSize;
}

//parent PCO�� plan cache�� ������
//environment�� �´� child PCO�� ���� ��쿡
//child PCO�� �ߺ������� �������� �Լ�.
IDE_RC  mmcPlanCache::preventDupPlan(idvSQL               *aStatistics,
                                     mmcParentPCO         *aParentPCO,
                                     mmcPCB               *aSafeGuardPCB,
                                     mmcPCB               **aNewPCB,
                                     mmcPCOLatchState     *aPCOLatchState)
{
    mmcPCB        *sNewPCB;
    mmcChildPCO   *sNewChildPCO;
    UInt           sPrevChildCreateCnt;
    UInt           sCurChildCreateCnt;
    UChar          sState = 0;

    IDE_DASSERT(*aPCOLatchState == MMC_PCO_LOCK_ACQUIRED_SHARED);
    //���ο� child PCO�����õ�.
    aParentPCO->getChildCreateCnt(&sPrevChildCreateCnt);
    // parent PCO�� s-latch�� ������ prepare-latch�� Ǭ��.
    // parent PCO prepare-latch�� release �ϱ� ���� safeguard PCB�� plan-Fix�� ��.
    // �׷��� ������ parent PCO�� victim�� �� �� �ִ�.
    aSafeGuardPCB->planFix(aStatistics);
    aParentPCO->releasePrepareLatch();
    *aPCOLatchState = MMC_PCO_LOCK_RELEASED;
    aParentPCO->getChildCreateCnt(&sCurChildCreateCnt);
    if(sCurChildCreateCnt != sPrevChildCreateCnt)
    {
        //�̹� �ٸ� statment�� recompile�Ѱ���̴�.
        *aNewPCB = (mmcPCB*)NULL;
        aSafeGuardPCB->planUnFix(aStatistics);
    }
    else
    {        
        // parent PCO�� prepare-latch�� x-mode�� �Ǵ�.
        aParentPCO->latchPrepareAsExclusive(aStatistics);
        aSafeGuardPCB->planUnFix(aStatistics);
        *aPCOLatchState = MMC_PCO_LOCK_ACQUIRED_EXECL;
        aParentPCO->getChildCreateCnt(&sCurChildCreateCnt);
        if( sCurChildCreateCnt == sPrevChildCreateCnt)
        {
            aParentPCO->incChildCreateCnt();
            /* fix BUG-31232  Reducing x-latch duration of parent PCO
               while perform soft prepare .
            */
            IDE_TEST(allocPlanCacheObjs(aStatistics,
                                        aParentPCO,
                                        MMC_CHILD_PCO_IS_CACHE_MISS,
                                        sCurChildCreateCnt,
                                        0,
                                        (qciPlanProperty *)NULL,
                                        (mmcStatement*)NULL,
                                        &sNewPCB,
                                        &sNewChildPCO,
                                        &sState)
                 != IDE_SUCCESS);
            // PVO�� ���Ͽ�  child PCO�� prepare-latch�� x-mode�� �Ǵ�.
            sNewChildPCO->latchPrepareAsExclusive(aStatistics);
            // parent PCO�� used child list��  child PCO��  append�Ѵ�.
            aParentPCO->addPCBOfChild(sNewPCB);
            // parent PCO�� x-latch�� ������ prepare-latch�� Ǭ��.
            aParentPCO->releasePrepareLatch();
            *aPCOLatchState = MMC_PCO_LOCK_RELEASED;
            *aNewPCB = sNewPCB;
        }
        else
        {
            // �ش� Parent PCO���� soft-prepare�� �ϱ� ���Ͽ� latch�� Ǭ��.
            // parent PCO�� x-latch�� ������ prepare-latch�� Ǭ��.
            aParentPCO->releasePrepareLatch();
            *aPCOLatchState = MMC_PCO_LOCK_RELEASED;
            *aNewPCB = (mmcPCB*)NULL;
        }

    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    {
        switch(sState )
        {
            /* fix BUG-31232  Reducing x-latch duration of parent PCO
           while perform soft prepare.  */
            case 4:
                sNewChildPCO->finalize();
            case 3:
                sNewPCB->finalize();
            case 2:
                IDE_ASSERT(mChildPCOMemPool.memfree(sNewChildPCO) == IDE_SUCCESS);
            case 1:
                //fix BUG-27340 Code-Sonar UMR
                IDE_ASSERT(mPCBMemPool.memfree(sNewPCB) == IDE_SUCCESS);
            default:
                break;
        }
        *aNewPCB = (mmcPCB*)NULL;
    }
    if(*aPCOLatchState !=  MMC_PCO_LOCK_RELEASED)
    {    
        aParentPCO->releasePrepareLatch();
        *aPCOLatchState = MMC_PCO_LOCK_RELEASED;
    }
    
    return IDE_FAILURE;
}

//environement�� �´� child PCO�� ������,
//plan�� valid���� �ʾ�,���ο� child PCO��
//�ߺ������� �������� �Լ�.
IDE_RC  mmcPlanCache::preventDupPlan(idvSQL                 *aStatistics,
                                     mmcStatement           *aStatement,
                                     mmcParentPCO           *aParentPCO,
                                     mmcPCB                 *aUnUsedPCB,
                                     mmcPCB                 **aNewPCB,
                                     mmcPCOLatchState       *aPCOLatchState,
                                     mmcChildPCOCR          aRecompileReason,
                                     UInt                   aPrevChildCreateCnt)
{
    mmcChildPCO                  *sNewChildPCO;
    mmcChildPCO                  *sUnusedChildPCO;
    UInt                         sCurChildCreateCnt;
    UInt                         sChildPCOSize;
    /*fix BUG-31403,
    It would be better to free an old plan cache object immediately
    which is only referenced by a statement while the statement do soft-prepare. */
    UInt                         sFixCount;
    UChar                        sState = 0;
    idBool                       sWinner;
    mmcChildPCOPlanState         sPlanState;
    
    if( aRecompileReason ==  MMC_PLAN_RECOMPILE_BY_SOFT_PREPARE )
    {
        
        sUnusedChildPCO = aUnUsedPCB->getChildPCO();
        sChildPCOSize =  sUnusedChildPCO->getSize();
        aParentPCO->getChildCreateCnt(&sCurChildCreateCnt);
        if(sCurChildCreateCnt != aPrevChildCreateCnt)
        {
            //�̹� �ٸ� statment�� recompile�Ѱ���̴�.
            *aNewPCB = (mmcPCB*)NULL;
            IDE_CONT(You_Are_Looser);
        }
        else
        {
            // parent PCO�� prepare-latch�� x-mode�� �Ǵ�.
            aParentPCO->latchPrepareAsExclusive(aStatistics);
            *aPCOLatchState = MMC_PCO_LOCK_ACQUIRED_EXECL;
            aParentPCO->getChildCreateCnt(&sCurChildCreateCnt);
            sWinner = (sCurChildCreateCnt == aPrevChildCreateCnt ) ? ID_TRUE: ID_FALSE;
        }
    }
    else
    {
        // MMC_PLAN_RECOMPILE_BY_EXECUTION
        IDE_DASSERT( *aPCOLatchState == MMC_PCO_LOCK_RELEASED);
        sUnusedChildPCO = aUnUsedPCB->getChildPCO();
        sChildPCOSize =  sUnusedChildPCO->getSize();
        sUnusedChildPCO->getPlanState(&sPlanState);
        if(sPlanState == MMC_CHILD_PCO_PLAN_IS_UNUSED)
        {
            //�̹� �ٸ� statment�� recompile�Ѱ���̴�.
            *aNewPCB = (mmcPCB*)NULL;
            IDE_CONT(You_Are_Looser);
        }
        else
        {
            // parent PCO�� prepare-latch�� x-mode�� �Ǵ�.
            aParentPCO->latchPrepareAsExclusive(aStatistics);
            //fix BUG-22570 [valgrind] mmcPCB::initialze���� UMR
            aParentPCO->getChildCreateCnt(&sCurChildCreateCnt);
            *aPCOLatchState = MMC_PCO_LOCK_ACQUIRED_EXECL;
            sUnusedChildPCO->getPlanState(&sPlanState);
            sWinner = (sPlanState != MMC_CHILD_PCO_PLAN_IS_UNUSED) ? ID_TRUE: ID_FALSE;
        }
        
    }
    
    if( sWinner == ID_TRUE)
    {
        aParentPCO->incChildCreateCnt();
        // PCB�� child PCO�� �Ҵ�޴´�.
        /* fix BUG-31232  Reducing x-latch duration of parent PCO
           while perform soft prepare .
        */
        IDE_TEST(allocPlanCacheObjs(aStatistics,
                                    aParentPCO,
                                    aRecompileReason,
                                    sCurChildCreateCnt,
                                    sUnusedChildPCO->getRebuildCnt()+1,
                                    sUnusedChildPCO->getEnvironment(),
                                    aStatement,
                                    aNewPCB,
                                    &sNewChildPCO,
                                    &sState)
                 != IDE_SUCCESS);

        // PVO�� ���Ͽ�  child PCO�� prepare-latch�� x-mode�� �Ǵ�.
        sNewChildPCO->latchPrepareAsExclusive(aStatistics);
        // parent PCO�� used child list��  child PCO��  append�Ѵ�.
        aParentPCO->addPCBOfChild(*aNewPCB);
        aUnUsedPCB->getFixCount(aStatistics,
                                &sFixCount);
        aParentPCO->movePCBOfChildToUnUsedLst(aUnUsedPCB);
        // fix BUG-27360, mmcStatement::softPrepare���� preventDupPlan fix�ϸ鼭
        // ���� �����մϴ�.
        sUnusedChildPCO->updatePlanState(MMC_CHILD_PCO_PLAN_IS_UNUSED);
        /*fix BUG-31403,
          It would be better to free an old plan cache object immediately
          which is only referenced by a statement while the statement do soft-prepare.
         old  PCO�� ���Ͽ� caller�� plan-Fix�Ͽ� call�ϱ� ������
         fix count�� 1�� case��  �� �̿ܿ� �����ϴ� statement�� ���°��̴�.. */
        if(sFixCount <= 1)
        {
            //child PCO null�� update.
            aUnUsedPCB->assignPCO(aStatistics,
                                  aParentPCO,
                                  NULL);
            aParentPCO->releasePrepareLatch();
            /* fix BUG-31232  Reducing x-latch duration of parent PCO
               while perform soft prepare .
            */            
            *aPCOLatchState = MMC_PCO_LOCK_RELEASED;

            //child PCO ����
            sUnusedChildPCO->finalize();
            mmcPlanCache::freePCO(sUnusedChildPCO);

            /* BUG-35673 the instruction ordering serialization */
            IDL_MEM_BARRIER;

            //PCB������ fixCount�� ���ü�������
            //���⼭ �ٷ� �Ҽ� ����, LRU replace�� ���Ͽ� ���Ͽ� ����.
            updateCacheSize(aStatistics,
                            sChildPCOSize,
                            aUnUsedPCB);
        }
        else
        {
            /* BUG-35673 the instruction ordering serialization */
            //child PCO�� �ٷ� ����� ����.
            ID_SERIAL_BEGIN(aParentPCO->releasePrepareLatch());
            ID_SERIAL_EXEC(*aPCOLatchState = MMC_PCO_LOCK_RELEASED, 1);
            // unused PCB ��parent���� unused child list���� move.
            ID_SERIAL_END(mmcPlanCacheLRUList::moveToColdRegionTail(aStatistics,
                                                                    aUnUsedPCB,
                                                                    NULL)); 

        }
    }
    else
    {
        // �ش� Parent PCO���� soft-prepare�� �ϱ� ���Ͽ� latch�� Ǭ��.
        // parent PCO�� x-latch�� ������ prepare-latch�� Ǭ��.
        aParentPCO->releasePrepareLatch();
        *aNewPCB = (mmcPCB*)NULL;

        *aPCOLatchState = MMC_PCO_LOCK_RELEASED;

    }

    IDE_EXCEPTION_CONT(You_Are_Looser);
    
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    {
        switch(sState )
        {
            /* fix BUG-31232  Reducing x-latch duration of parent PCO
           while perform soft prepare .*/
            case 4:
                sNewChildPCO->finalize();
            case 3:
               (*aNewPCB)->finalize();
            case 2:
                IDE_ASSERT(mChildPCOMemPool.memfree(sNewChildPCO) == IDE_SUCCESS);
            case 1:
                //fix BUG-27340 Code-Sonar UMR
                IDE_ASSERT(mPCBMemPool.memfree(*aNewPCB) == IDE_SUCCESS);
            default:
                break;
        }
        if(*aPCOLatchState !=  MMC_PCO_LOCK_RELEASED)
        {    
            aParentPCO->releasePrepareLatch();
            *aPCOLatchState = MMC_PCO_LOCK_RELEASED;
        }
        *aNewPCB = (mmcPCB*)NULL;

    }
    return IDE_FAILURE;
}

/* fix BUG-31232  Reducing x-latch duration of parent PCO
   while perform soft prepare .
   pre-alloc PCB, PCO
*/
IDE_RC mmcPlanCache::allocPlanCacheObjs(idvSQL          *aStatistics,
                                        mmcParentPCO    *aParentPCO,
                                        mmcChildPCOCR   aRecompileReason,
                                        const UInt      aPcoId,
                                        const UInt      aRebuildCount,
                                        qciPlanProperty *aEnv,
                                        mmcStatement    *aStatement,
                                        mmcPCB          **aNewPCB,
                                        mmcChildPCO     **aNewChildPCO,
                                        UChar           *aState)
{

    IDU_FIT_POINT( "mmcPlanCache::allocPlanCacheObjs::alloc::NewPCB" );

    // PCB�� child PCO�� �Ҵ�޴´�.
    IDE_TEST( mPCBMemPool.alloc((void**)aNewPCB) != IDE_SUCCESS);
    (*aState)++;

    IDU_FIT_POINT( "mmcPlanCache::allocPlanCacheObjs::alloc::NewChildPCO" );

    IDE_TEST( mChildPCOMemPool.alloc((void**)aNewChildPCO) != IDE_SUCCESS);
    (*aState)++;

    // PCB�� child PCO�� �ʱ�ȭ �Ѵ�.
    // fix BUG-21429
    // fix BUG-29116,There is needless +1 operation in numbering child PCO.
    (*aNewPCB)->initialize((SChar*)(aParentPCO->mSQLTextId),
                           aPcoId);
    (*aState)++;
    // fix BUG-29116,There is needless +1 operation in numbering child PCO.
    (*aNewChildPCO)->initialize((SChar*)(aParentPCO->mSQLTextId),
                                aPcoId,
                                aRecompileReason,
                                aRebuildCount,
                                (*aNewPCB)->getFrequencyPtr());
    (*aState)++;
    // PCB���� parent ,child PCO�� assign�Ѵ�.
    (*aNewPCB)->assignPCO(aStatistics,
                          aParentPCO,
                          *aNewChildPCO);

    // PROJ-2163
    if( aEnv != NULL )
    {
        // BUG-36956
        (*aNewChildPCO)->assignEnv(aEnv, (qciPlanBindInfo *)NULL);
        IDE_TEST( qci::rebuildEnvironment(aStatement->getQciStmt() ,
                                          (*aNewChildPCO)->getEnvironment())
                  != IDE_SUCCESS);
    }
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;

}

/* fix BUG-31232  Reducing x-latch duration of parent PCO
   while perform soft prepare .
   free PCB, PCO
*/
void  mmcPlanCache::freePlanCacheObjs(mmcPCB       *aNewPCB,
                                      mmcChildPCO  *aNewChildPCO,
                                      UChar        *aState)
{
    (*aState)--;
    aNewChildPCO->finalize();
    (*aState)--;
    aNewPCB->finalize();
    (*aState)--;
    IDE_ASSERT(mChildPCOMemPool.memfree(aNewChildPCO) == IDE_SUCCESS);
    (*aState)--;
    //fix BUG-27340 Code-Sonar UMR
    IDE_ASSERT(mPCBMemPool.memfree(aNewPCB) == IDE_SUCCESS);

}


void mmcPlanCache::checkIn(idvSQL                 *aStatSQL,
                           mmcPCB                 *aPCB,
                           qciSQLPlanCacheContext *aPlanCacheContext,
                           idBool                 *aSuccess)
{
    mmcPlanCacheLRUList::checkIn(aStatSQL,
                                 aPCB,
                                 aPlanCacheContext,
                                 aSuccess);                             
}

void mmcPlanCache::register4GC(idvSQL                  *aStatSQL,
                               mmcPCB                  *aPCB)
{
    mmcPCBLRURegion      sPCBLruRegion;
    
    aPCB->getLRURegion(aStatSQL,
                       &sPCBLruRegion);
    IDE_ASSERT(sPCBLruRegion == MMC_PCB_IN_NONE_LRU_REGION);
    


    mmcPlanCacheLRUList::register4GC(aStatSQL,
                                         aPCB);
    
    
}


void mmcPlanCache::tryFreeParentPCO(idvSQL  *aStatSQL,
                                    mmcParentPCO *aParentPCO)
{
    UInt  sBucket;
    
    sBucket = aParentPCO->getBucket();
    
    // bucket�� x-latch�� �Ǵ�.
    mmcSQLTextHash::latchBucketAsExeclusive(aStatSQL,
                                            sBucket);
    
    aParentPCO->latchPrepareAsExclusive(aStatSQL);
    if(aParentPCO->getChildCnt() == 0)
    {
        
        mmcSQLTextHash::tryUpdateBucketMaxHashVal(sBucket,
                                                  aParentPCO->getBucketChainNode());
        //bucket chain���� ����.
        IDU_LIST_REMOVE(aParentPCO->getBucketChainNode());
        aParentPCO->releasePrepareLatch();
        mmcSQLTextHash::releaseBucketLatch(sBucket);
        //parent PCO �ڿ� ����.
        aParentPCO->finalize();
        IDE_ASSERT(mParentPCOMemPool.memfree(aParentPCO) == IDE_SUCCESS);
    }
    else
    {
        aParentPCO->releasePrepareLatch();
        mmcSQLTextHash::releaseBucketLatch(sBucket);
    }

}

idBool  mmcPlanCache::isCacheAbleSQLText(SChar        *aSQLText)
{
    UShort  i;
    SChar   *sTokenPtr;
    SChar   *sTemp;
    idBool  sRetVal;
    SChar   sToken[mmcPlanCache::CACHE_KEYWORD_MAX_LEN+1];
    UInt    sTokenLen;

    sTokenPtr = aSQLText;
    sRetVal = ID_FALSE;

    while (1)
    {
        // ����, tab, new line ����
        //fix BUG-21427
        for ( ;
              ( ( *sTokenPtr == ' ' )  || ( *sTokenPtr == '\t' ) ||
                ( *sTokenPtr == '\r' ) || ( *sTokenPtr == '\n' ) );
              sTokenPtr++ )
            ;

        // BUG-43885 remove comment
        if ( *sTokenPtr == '/' )
        {
            sTokenPtr++;
            if ( *sTokenPtr == '*' )
            {
                sTokenPtr++;
                sTemp = idlOS::strstr( sTokenPtr, "*/" );
                if ( sTemp != NULL )
                {
                    sTokenPtr = sTemp + 2;
                }
                else
                {
                    break;
                }
            }
            else if ( *sTokenPtr == '/' )
            {
                sTokenPtr++;
                sTemp = idlOS::strstr( sTokenPtr, "\n" );
                if ( sTemp != NULL )
                {
                    sTokenPtr = sTemp + 1;
                }
                else
                {
                    break;
                }
            }
            else
            {
                break;
            }
        }
        else if ( *sTokenPtr == '-' )
        {
            sTokenPtr++;
            if ( *sTokenPtr == '-' )
            {
                sTokenPtr++;
                sTemp = idlOS::strstr( sTokenPtr, "\n" );
                /* BUG-44126 */
                if ( sTemp != NULL )
                {
                    sTokenPtr = sTemp + 1;
                }
                else
                {
                    break;
                }
            }
            else
            {
                break;
            }
        }
        else
        {
            break;
        }
    }

    // token ����.
    for(sTokenLen =0;((*sTokenPtr != ' ' ) && (*sTokenPtr != '\t' ) && (*sTokenPtr != '\r') && (*sTokenPtr != '\n' ) &&(*sTokenPtr !='\0' ));
        sTokenPtr++)
    {
        sToken[sTokenLen] = *sTokenPtr;
        sTokenLen++;
        if(sTokenLen  == mmcPlanCache::CACHE_KEYWORD_MAX_LEN )
        {
            break;
        }
    }
    sToken[sTokenLen] = '\0';
    for(i = 0; mCacheAbleText[i] != NULL; i++)
    {
        if(idlOS::strcasecmp(sToken, mCacheAbleText[i]) == 0)
        {
            sRetVal =  ID_TRUE;
            break;
        }
    }//for i
    return sRetVal;
}
void mmcPlanCache::updateCacheSize(idvSQL                 *aStatSQL,
                                   UInt                    aChildPCOSize,
                                   mmcPCB                 *aUnUsedPCB)
{
    /*fix BUG-31245	The size of hot or cold LRU region can be incorrect
      when old PCO is freed directly while statement perform rebuilding
      a plan for old PCO.*/
    mmcPlanCacheLRUList::updateCacheSize(aStatSQL,
                                         aChildPCOSize,
                                         aUnUsedPCB);
                                         
}

IDE_RC mmcPlanCache::compact(void     *aStatement)
{
    mmcStatement * sStatement = (mmcStatement*) aStatement;
    
    mmcPlanCacheLRUList::compact(sStatement->getStatistics());
    return IDE_SUCCESS;                                     
}

// reset := compact + reset statistics
IDE_RC mmcPlanCache::reset(void     *aStatement)
{

    mmcStatement * sStatement = (mmcStatement*) aStatement;
    
    mmcPlanCacheLRUList::compact(sStatement->getStatistics());
    
    mPlanCacheSystemInfo.mCacheHitCnt      = 0;
    mPlanCacheSystemInfo.mCacheMissCnt     = 0;
    mPlanCacheSystemInfo.mCacheInFailCnt   = 0;
    mPlanCacheSystemInfo.mCacheOutCnt      = 0;
    mPlanCacheSystemInfo.mCacheInsertedCnt = 0;
    mPlanCacheSystemInfo.mNoneCacheSQLTryCnt = 0;
    
    
    return IDE_SUCCESS;                  
}


void mmcPlanCache::latchShared4PerfV(idvSQL  *aStatSQL)
{
    idvWeArgs sWeArgs;
    
    // PROJ-2408
    IDE_ASSERT( mPerfViewLatch.lockRead(aStatSQL, &sWeArgs) == IDE_SUCCESS );
}

void mmcPlanCache::releaseLatchShared4PerfV()
{
    // PROJ-2408
    IDE_ASSERT( mPerfViewLatch.unlock() == IDE_SUCCESS);  
}


IDE_RC mmcPlanCache::buildRecordForSQLPlanCache(idvSQL              * /*aStatistics*/,
                                                void                 *aHeader,
                                                void                 */*aDummyObj*/,
                                                iduFixedTableMemory  *aMemory)
{
    latchShared4PerfV(NULL);
    
    mmcSQLTextHash::getCacheHitCount(&mPlanCacheSystemInfo.mCacheHitCnt);
    mPlanCacheSystemInfo.mMaxCacheSize  = mmuProperty::getSqlPlanCacheSize();
    mPlanCacheSystemInfo.mColdLruSize  = mmcPlanCacheLRUList::mColdRegionList.mCurMemSize;
    mPlanCacheSystemInfo.mHotLruSize   = mmcPlanCacheLRUList::mHotRegionList.mCurMemSize;
    IDE_TEST( iduFixedTable::buildRecord(aHeader,aMemory,(void*)&mPlanCacheSystemInfo)
              != IDE_SUCCESS);
    
    releaseLatchShared4PerfV();
    
    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    {
        releaseLatchShared4PerfV();
    }
    return IDE_FAILURE;

}

static iduFixedTableColDesc gSQLPLANCACHEColDesc[]=
{
    {
        (SChar *)"CACHE_HIT_COUNT",
        offsetof(mmcPlanCacheSystemInfo,mCacheHitCnt),
        IDU_FT_SIZEOF(mmcPlanCacheSystemInfo,mCacheHitCnt ),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0, NULL // for internal use
    },
    {
        (SChar *)"CACHE_MISS_COUNT",
        offsetof(mmcPlanCacheSystemInfo,mCacheMissCnt),
        IDU_FT_SIZEOF(mmcPlanCacheSystemInfo,mCacheMissCnt ),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0, NULL // for internal use
    },
    {
        (SChar *)"CACHE_IN_FAIL_COUNT",
        offsetof(mmcPlanCacheSystemInfo,mCacheInFailCnt),
        IDU_FT_SIZEOF(mmcPlanCacheSystemInfo,mCacheInFailCnt ),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0, NULL // for internal use
    },
    {
        (SChar *)"CACHE_OUT_COUNT",
        offsetof(mmcPlanCacheSystemInfo,mCacheOutCnt),
        IDU_FT_SIZEOF(mmcPlanCacheSystemInfo,mCacheOutCnt),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0, NULL // for internal use
    },
    
    {
        (SChar *)"CACHE_INSERTED_COUNT",
        offsetof(mmcPlanCacheSystemInfo,mCacheInsertedCnt),
        IDU_FT_SIZEOF(mmcPlanCacheSystemInfo,mCacheInsertedCnt),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0, NULL // for internal use
    },
    
    {
        (SChar *)"CURRENT_CACHE_OBJ_CNT",
        offsetof(mmcPlanCacheSystemInfo,mCurrentCacheObjCnt),
        IDU_FT_SIZEOF(mmcPlanCacheSystemInfo,mCurrentCacheObjCnt),
        IDU_FT_TYPE_UINTEGER,
        NULL,
        0, 0, NULL // for internal use
    },
    
    
    {
        (SChar *)"NONE_CACHE_SQL_TRY_COUNT",
        offsetof(mmcPlanCacheSystemInfo,mNoneCacheSQLTryCnt),
        IDU_FT_SIZEOF(mmcPlanCacheSystemInfo,mNoneCacheSQLTryCnt),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0, NULL // for internal use
    },
    
    {
        (SChar *)"CURRENT_CACHE_SIZE",
        offsetof(mmcPlanCacheSystemInfo,mCurrentCacheSize),
        IDU_FT_SIZEOF(mmcPlanCacheSystemInfo,mCurrentCacheSize),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0, NULL // for internal use
    },
   
    {
        (SChar *)"MAX_CACHE_SIZE",
        offsetof(mmcPlanCacheSystemInfo,mMaxCacheSize),
        IDU_FT_SIZEOF(mmcPlanCacheSystemInfo,mMaxCacheSize),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0, NULL // for internal use
    },
 
    {
        (SChar *)"CURRENT_HOT_LRU_SIZE",
        offsetof(mmcPlanCacheSystemInfo,mHotLruSize),
        IDU_FT_SIZEOF(mmcPlanCacheSystemInfo,mHotLruSize),
        IDU_FT_TYPE_UBIGINT ,
        NULL,
        0, 0, NULL // for internal use
    },
    {
        (SChar *)"CURRENT_COLD_LRU_SIZE",
        offsetof(mmcPlanCacheSystemInfo,mColdLruSize),
        IDU_FT_SIZEOF(mmcPlanCacheSystemInfo,mColdLruSize),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0, NULL // for internal use
    },
    
     {
        NULL,
        0,
        0,
        IDU_FT_TYPE_CHAR,
        NULL,
        0, 0, NULL // for internal use
    }
};

iduFixedTableDesc   gSQLPLANCACHETableDesc =
{
    (SChar*)"X$SQL_PLAN_CACHE",
    mmcPlanCache::buildRecordForSQLPlanCache,
    gSQLPLANCACHEColDesc,
    IDU_STARTUP_SERVICE,
    0,
    0,
    IDU_FT_DESC_TRANS_NOT_USE,
    NULL
};

IDE_RC mmcPlanCache::movePCBOfChildToUnUsedLst(idvSQL *aStatistics,
                                               mmcPCB *aUnUsedPCB) 
{
/***********************************************************************
 *
 * Description : PROJ-2163
 *      Plan �� plan cache �� ����� �� ���� ���� �� PCB �� child PCO ��
 *      �����ϰ� �����ϴ� �Լ��̴�.
 *
 *      Plan cache �� ����ϱ� ���� plan �� ���� ������ child PCO ��
 *      X latch �� ��Ƽ� �ٸ� ������ ���� plan �� �ߺ����� ������ ��
 *      ���� �Ѵ�.
 *      ������ ������ plan �� plan cache �� ����� �� ���ٸ�
 *      �ٸ� ���ǵ��� ���� PCB �� child PCO �� �����ϰ� �����ؾ� �Ѵ�.
 *
 *      �� �Լ������� PCB �� �����ϰ� ���ŵ� �� �ֵ��� parent PCO ��
 *      unused list �� �̵��ϰ� LRU list �� cold region �ڷ� �̵���Ų��.
 *      Child PCO �� NEED_HARD_PREPARE ���·� �����Ͽ� ������� ������
 *      �ٷ� hard prepare �� �����ϰ� �Ѵ�.
 *      ���� ������� ������ ���ٸ� child PCO �� �ٷ� �����Ѵ�.
 *
 * Implementation :
 *      1. Parent PCO �� X latch �ɰ� PCB �� unused list �� �̵�
 *      2. Child PCO �� NEED_HARD_PREPARE �� ����
 *      3. PCB �� fix ī��Ʈ(������� ���� ��)�� �б�
 *        a. (fix count == 0; ������� ������ ����)
 *           PCB �� child PCO ������ ���� (null �� ����)
 *           Parent PCO �� latch �� ����
 *           PCB �� cold region ������ �̵�
 *           Child PCO �� ����
 *        b. (fix count > 0; ������� ������ ����)
 *           Parent PCO �� latch �� ����
 *           PCB �� cold region ������ �̵�
 *           Child PCO �� latch �� ����
 *
 ***********************************************************************/

    UInt               sFixCount;
    mmcChildPCO      * sUnusedChildPCO = aUnUsedPCB->getChildPCO();
    mmcPCBLRURegion    sPCBLruRegion;
 
    // parent PCO�� prepare-latch�� x-mode�� �Ǵ�.
    aUnUsedPCB->getParentPCO()->latchPrepareAsExclusive(aStatistics);
    
    aUnUsedPCB->getParentPCO()->movePCBOfChildToUnUsedLst(aUnUsedPCB);

    // fix BUG-27360, mmcStatement::softPrepare���� preventDupPlan fix�ϸ鼭
    // ���� �����մϴ�.
    sUnusedChildPCO->updatePlanState(MMC_CHILD_PCO_PLAN_NEED_HARD_PREPARE);

    /*fix BUG-31403,
      It would be better to free an old plan cache object immediately
      which is only referenced by a statement while the statement do soft-prepare.
      old  PCO�� ���Ͽ� caller�� plan-Fix�Ͽ� call�ϱ� ������
     fix count�� 1�� case��  �� �̿ܿ� �����ϴ� statement�� ���°��̴�.. */
    aUnUsedPCB->getFixCount(aStatistics,
                            &sFixCount);
      
    if(sFixCount <= 0)
    {
        //child PCO null�� update.
        aUnUsedPCB->assignPCO(aStatistics,
                              aUnUsedPCB->getParentPCO(),
                              NULL);

        /* BUG-35673 the instruction ordering serialization */
        ID_SERIAL_BEGIN(aUnUsedPCB->getParentPCO()->releasePrepareLatch());

        /* BUG-47813 pcb�� victim���� �����Ǳ� ���� PCBLruRegion���� �����;� ��. */
        // unused PCB ��parent���� unused child list���� move.
        ID_SERIAL_EXEC(mmcPlanCacheLRUList::moveToColdRegionTail(aStatistics,
                                                                 aUnUsedPCB,
                                                                 &sPCBLruRegion), 1);
        //child PCO ����
        /* BUG-38955 Destroying a Latch Held as Exclusive */
        ID_SERIAL_EXEC(sPCBLruRegion == MMC_PCB_IN_NONE_LRU_REGION ?
                       sUnusedChildPCO->releasePrepareLatch() : (void)0, 2);
        ID_SERIAL_EXEC(sUnusedChildPCO->finalize(), 3);
        ID_SERIAL_END(mmcPlanCache::freePCO(sUnusedChildPCO));
    }
    else
    {
        /* BUG-35673 the instruction ordering serialization */
        //child PCO�� �ٷ� ����� ����.
        ID_SERIAL_BEGIN(aUnUsedPCB->getParentPCO()->releasePrepareLatch());
 
        // unused PCB ��parent���� unused child list���� move.
        ID_SERIAL_EXEC(mmcPlanCacheLRUList::moveToColdRegionTail(aStatistics,
                                                                 aUnUsedPCB,
                                                                 NULL), 1);

        ID_SERIAL_END(sUnusedChildPCO->releasePrepareLatch());
    }
    
    return IDE_SUCCESS;
}

/* BUG-46158 PLAN_CACHE_KEEP */
IDE_RC mmcPlanCache::planCacheKeep(void *aMmStatement, SChar *aSQLTextID, idBool aIsKeep)
{
    mmcStatement *sStatement = (mmcStatement *)aMmStatement;

    IDE_TEST(mmcSQLTextHash::planCacheKeep(sStatement->getStatistics(),
                                           aSQLTextID,
                                           (aIsKeep == ID_TRUE) ?
                                           MMC_PCO_PLAN_CACHE_KEEP : MMC_PCO_PLAN_CACHE_UNKEEP)
             != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
