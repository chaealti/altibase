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
#include <iduHashUtil.h>
#include <mmcStatement.h>
#include <mmcParentPCO.h>
#include <mmcChildPCO.h>
#include <mmcPCB.h>
#include <mmuProperty.h>

IDE_RC mmcParentPCO::initialize(UInt     aSQLTextIdInBucket,
                                SChar    *aSQLString,
                                UInt     aSQLStringLen,
                                vULong   aHashKeyVal,
                                UInt     aBucket)
    
{
    UChar sState =0;
    SChar sLatchName[IDU_MUTEX_NAME_LEN];

    IDU_FIT_POINT_RAISE( "mmcParentPCO::initialize::malloc::SQLString4SoftPrepare",
                          InsufficientMemory );
         
    IDE_TEST_RAISE(iduMemMgr::malloc(IDU_MEM_MMPLAN_CACHE_CONTROL,
                                     aSQLStringLen+1,
                                     (void**)&mSQLString4SoftPrepare)
                   != IDE_SUCCESS, InsufficientMemory );
    sState = 1;
    
    IDU_FIT_POINT_RAISE( "mmcParentPCO::initialize::malloc::SQLString4HardPrepare",
                          InsufficientMemory );
    
    //QP���� parsing�������� identifier�� �빮�ڷ� �����ϱ⶧����
    //���ʿ��ϰ� ������ String buffer�� �ʿ��ϴ�.
    IDE_TEST_RAISE(iduMemMgr::malloc(IDU_MEM_MMPLAN_CACHE_CONTROL,
                                     aSQLStringLen+1,
                                     (void**)&mSQLString4HardPrepare)
                   != IDE_SUCCESS, InsufficientMemory );
    sState = 2;
    idlOS::memcpy(mSQLString4SoftPrepare ,aSQLString,aSQLStringLen);
    mSQLString4SoftPrepare[aSQLStringLen] =0;
    
    idlOS::memcpy(mSQLString4HardPrepare ,aSQLString,aSQLStringLen);
    mSQLString4HardPrepare[aSQLStringLen] =0;
    
    mSQLStringLen  = aSQLStringLen;
    mHashKeyVal    = aHashKeyVal;
    mBucket        = aBucket;
    mPlanCacheKeep = MMC_PCO_PLAN_CACHE_UNKEEP;  /* BUG-46158 */

    idlOS::snprintf(sLatchName,
                    IDU_MUTEX_NAME_LEN,
                    "MMC_PARENT_PCO_LATCH_%"ID_UINT32_FMT,
                    aBucket);
    // PROJ-2408
    IDE_ASSERT(mPrepareLatch.initialize(sLatchName) == IDE_SUCCESS);
    mChildCreateCnt = 0;
    mChildCnt = 0;
    //fix BUG-21429
    idlOS::snprintf(mSQLTextId,
                    MMC_SQL_CACHE_TEXT_ID_LEN,
                    "%0*"ID_UINT32_FMT"%"ID_UINT32_FMT,
                    MMC_SQL_CACHE_TEXT_ID_BUCKET_DIGIT,
                    mBucket,
                    aSQLTextIdInBucket);
    
    IDU_LIST_INIT(&mUsedChildLst);
    IDU_LIST_INIT(&mUnUsedChildLst);
    IDU_LIST_INIT_OBJ(&mBucketChain,this);
    /*fix BUG-31050 The approach for reducing the latch duration of plan cache LRU
      by moving free parent PCO to the out side of critical section */
    IDU_LIST_INIT_OBJ(&mVictimNode,this);
    
    return IDE_SUCCESS;

    IDE_EXCEPTION(InsufficientMemory);
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_InsufficientMemory));
    }    
    IDE_EXCEPTION_END;
    {
        switch(sState)
        {
            case 2:
                IDE_ASSERT(iduMemMgr::free(mSQLString4HardPrepare) == IDE_SUCCESS);
            case 1:
                IDE_ASSERT(iduMemMgr::free(mSQLString4SoftPrepare) == IDE_SUCCESS);
        }//switch 
    }
    return IDE_FAILURE;       
}

void mmcParentPCO::finalize()
{
    IDE_ASSERT(iduMemMgr::free(mSQLString4SoftPrepare) == IDE_SUCCESS);
    IDE_ASSERT(iduMemMgr::free(mSQLString4HardPrepare) == IDE_SUCCESS);
    
    mSQLString4SoftPrepare = NULL;
    mSQLString4HardPrepare = NULL;
    
    mSQLStringLen = 0;
    // PROJ-2408
    IDE_ASSERT(mPrepareLatch.destroy() == IDE_SUCCESS);
}

// used child list�� child PCO�� PCB�� ����Ѵ�.
// parent PCO�� prepare latch�� x-latch�� ��� ���Լ��� ȣ���ؾ� �Ѵ�.
void mmcParentPCO::addPCBOfChild(mmcPCB* aPCB)
{
    IDU_LIST_ADD_LAST(&mUsedChildLst,aPCB->getChildLstNode());
    
    mChildCnt++;
    if(mChildCnt == 0)
    {
        mChildCnt = 1;
    }
}

void  mmcParentPCO::movePCBOfChildToUnUsedLst(mmcPCB* aPCB)
{
    //used child list���� ����.
    IDU_LIST_REMOVE(aPCB->getChildLstNode());
    //unused child list�� �߰�.
    IDU_LIST_ADD_LAST(&mUnUsedChildLst,aPCB->getChildLstNode());
}

void mmcParentPCO::validateChildLst(iduList       *aHead,
                                    mmcPCB        *aPCB)
{
    iduListNode   *sIterator;
    mmcPCB        *sPCB;
    IDU_LIST_ITERATE(aHead,sIterator)
    {
        sPCB = (mmcPCB*)sIterator->mObj;
        if(sPCB == aPCB)
        {
            return;
        }
    }
    IDE_ASSERT(0);
}

void mmcParentPCO::validateChildLst2(iduList       *aHead,
                                    mmcPCB        *aPCB)
{
    iduListNode   *sIterator;
    mmcPCB        *sPCB;
    IDU_LIST_ITERATE(aHead,sIterator)
    {
        sPCB = (mmcPCB*)sIterator->mObj;
        if(sPCB == aPCB)
        {
            IDE_ASSERT(0);
        }
    }
}


// parent PCO�� prepare latch�� x-latch�� ��� ���Լ��� ȣ���ؾ� �Ѵ�.
// child PCO��  Parent PCO�� child list���� �����Ѵ�.
void mmcParentPCO::deletePCBOfChild(mmcPCB* aPCB)
{
    IDU_LIST_REMOVE(aPCB->getChildLstNode());
    mChildCnt--;
}

void mmcParentPCO::latchPrepareAsShared(idvSQL* aStatistics)
{
    // PROJ-2408
    IDE_ASSERT(mPrepareLatch.lockRead(aStatistics, NULL) == IDE_SUCCESS);
}

void mmcParentPCO::latchPrepareAsExclusive(idvSQL* aStatistics)
{
    // PROJ-2408   
    IDE_ASSERT(mPrepareLatch.lockWrite(aStatistics, NULL) == IDE_SUCCESS);
}

/* BUG-35521 Add TryLatch in PlanCache. */
void mmcParentPCO::tryLatchPrepareAsExclusive(idBool *aSuccess)
{
    UInt sTryCnt = mmuProperty::getSqlPlanCacheParentPCOXLatchTryCnt();

    while (sTryCnt > 0)
    {
        // PROJ-2408
        IDE_ASSERT(mPrepareLatch.tryLockWrite(aSuccess) == IDE_SUCCESS);

        if (*aSuccess == ID_TRUE)
        {
            break;
        }
        else
        {
            sTryCnt--;
        }
    }
}

void mmcParentPCO::releasePrepareLatch()
{
    // PROJ-2408
    IDE_ASSERT(mPrepareLatch.unlock() == IDE_SUCCESS);
}

// startChild PCB�� null�� �ƴϸ� �ش� child PCO���� ����,
// �˻��� �����Ѵ�.
// null �̸� ó������ �˻��Ѵ�.
IDE_RC  mmcParentPCO::searchChildPCO(mmcStatement          *aStatement,
                                     mmcPCB                **aPCBofChildPCO,
                                     mmcChildPCOPlanState  *aChildPCOPlanState,
                                     mmcPCOLatchState      *aPCOLatchState)
{
    iduListNode        *sIterator;
    mmcPCB             *sPCB;
    mmcChildPCO        *sChildPCO;
    mmcChildPCOEnvState sEnvState;
    idBool              sIsMatched;
    idvSQL             *sStatistics  = aStatement->getStatistics();
    
    IDE_DASSERT(*aPCOLatchState == MMC_PCO_LOCK_ACQUIRED_SHARED);
    
    *aPCBofChildPCO = (mmcPCB*) NULL;
    *aChildPCOPlanState = MMC_CHILD_PCO_PLAN_IS_NOT_READY;

    // fix BUG-24685 mmcParentPCO::searchChild PCO����
    // assert���� ������ ����.
    // PCB reuse ���¸� ������.
    for(sIterator =  mUsedChildLst.mNext; (&mUsedChildLst != sIterator); sIterator = sIterator->mNext,*aChildPCOPlanState = MMC_CHILD_PCO_PLAN_IS_NOT_READY)   
    {
        sPCB = (mmcPCB*)sIterator->mObj;
        sChildPCO = sPCB->getChildPCO();
      check_matchedEnv:
        sEnvState = sChildPCO->getEnvState();
        if(sEnvState == MMC_CHILD_PCO_ENV_IS_READY)
        {
            //fix BUG-30855 It needs to describe soft prepare time in detail for problem tracking.
            IDV_SQL_OPTIME_BEGIN(sStatistics,IDV_OPTM_INDEX_MATCHING_CHILD_PCO);
            IDE_TEST_RAISE( qci::isMatchedEnvironment(aStatement->getQciStmt(),
                                                      sChildPCO->getEnvironment(),
                                                      sChildPCO->getPlanBindInfo(), // PROJ-2163
                                                      &sIsMatched) != IDE_SUCCESS,
                            MissMatchEnvError);
            //fix BUG-30855 It needs to describe soft prepare time in detail for problem tracking.
            IDV_SQL_OPTIME_END(sStatistics,IDV_OPTM_INDEX_MATCHING_CHILD_PCO);
            if(sIsMatched == ID_TRUE)
            {
                sChildPCO->getPlanState(aChildPCOPlanState);
                // fix BUG-24685 mmcParentPCO::searchChild PCO����
                // assert���� ������ ����.
                // PCB reuse ���¸� ������.
                if(*aChildPCOPlanState != MMC_CHILD_PCO_PLAN_IS_NOT_READY)
                {
                    //MMC_CHILD_PCO_PLAN_NEED_HARD_PREPARE
                    //MMC_CHILD_PCO_PLAN_IS_READY
                    if(*aChildPCOPlanState == MMC_CHILD_PCO_PLAN_IS_READY)
                    {
                        *aPCBofChildPCO = sPCB;
                    }
                    else
                    {
                        IDE_ASSERT((*aChildPCOPlanState == MMC_CHILD_PCO_PLAN_NEED_HARD_PREPARE));
                        //fix BUG-24685 mmcParentPCO::searchChildPCO����
                        // assert�� ������ ����˴ϴ�.
                        // parent PCO�� prepare-latch�� releas�մϴ�.
                        *aPCOLatchState = MMC_PCO_LOCK_RELEASED;
                        releasePrepareLatch();
                    }                    
                }
                else
                {
                    //MMC_CHILD_PCO_PLAN_IS_NOT_READY
                    //hard prepare�� ����ϱ� ���Ͽ�
                    //parent prepare-latch�� Ǭ��.
                    sPCB->planFix(sStatistics);
                    IDE_DASSERT(*aPCOLatchState == MMC_PCO_LOCK_ACQUIRED_SHARED);
                    releasePrepareLatch();
                    *aPCOLatchState = MMC_PCO_LOCK_RELEASED;
                    //fix BUG-30855 It needs to describe soft prepare time in detail for problem tracking.
                    IDV_SQL_OPTIME_BEGIN(sStatistics,IDV_OPTM_INDEX_WAITING_HARD_PREPARE);
                    sChildPCO->wait4HardPrepare(sStatistics);
                    //fix BUG-30855 It needs to describe soft prepare time in detail for problem tracking.
                    IDV_SQL_OPTIME_END(sStatistics,IDV_OPTM_INDEX_WAITING_HARD_PREPARE);
                    latchPrepareAsShared(sStatistics);
                    *aPCOLatchState = MMC_PCO_LOCK_ACQUIRED_SHARED;

                    sChildPCO->getPlanState(aChildPCOPlanState);
                    sPCB->planUnFix(sStatistics);

                    if(*aChildPCOPlanState ==  MMC_CHILD_PCO_PLAN_IS_READY)
                    {
                        *aPCBofChildPCO = sPCB;
                    }
                    else
                    {
                        //fix BUG-24607 hard-prepare��� �Ϸ��Ŀ� child PCO��
                        //old PCO list���� �Ű����� �ִ�.
                        // wait���� ������� �ش� child PCO�� old�� �ɼ� �ִ�.
                        if(*aChildPCOPlanState ==  MMC_CHILD_PCO_PLAN_IS_UNUSED)
                        {
                            //�̷��� ���. used listó������ �˻��� �����Ѵ�.
                            sIterator = &mUsedChildLst;
                            continue;
                        }
                        else
                        {
                            
                            IDE_ASSERT(*aChildPCOPlanState == MMC_CHILD_PCO_PLAN_NEED_HARD_PREPARE);
                            *aPCOLatchState = MMC_PCO_LOCK_RELEASED;
                            releasePrepareLatch();
                        }
                    }
                }
                break;
            }//if
            else
            {
                // not mathed.
                //nothing to do , go to next child PCO.
            }
        }
        else
        {
            // environment is null.
            sChildPCO->getPlanState(aChildPCOPlanState);
            if(*aChildPCOPlanState == MMC_CHILD_PCO_PLAN_NEED_HARD_PREPARE)
            {
                //fix BUG-24685 mmcParentPCO::searchChildPCO����
                // assert�� ������ ����˴ϴ�.
                // parent PCO�� prepare-latch�� releas�մϴ�.
                *aPCOLatchState = MMC_PCO_LOCK_RELEASED;
                releasePrepareLatch();
                break;
            }
            else
            {
                if(*aChildPCOPlanState ==  MMC_CHILD_PCO_PLAN_IS_READY)
                {
                    goto  check_matchedEnv;
                }
                else
                {
                    IDE_DASSERT(*aChildPCOPlanState == MMC_CHILD_PCO_PLAN_IS_NOT_READY);
                    //hard prepare�� ����ϱ� ���Ͽ�
                    //parent prepare-latch�� Ǭ��.
                    sPCB->planFix(aStatement->getStatistics());
                    IDE_DASSERT(*aPCOLatchState == MMC_PCO_LOCK_ACQUIRED_SHARED);
                    releasePrepareLatch();
                    *aPCOLatchState = MMC_PCO_LOCK_RELEASED;
                    //fix BUG-30855 It needs to describe soft prepare time in detail for problem tracking.
                    IDV_SQL_OPTIME_BEGIN(sStatistics,IDV_OPTM_INDEX_WAITING_HARD_PREPARE);
                    sChildPCO->wait4HardPrepare(aStatement->getStatistics());
                    //fix BUG-30855 It needs to describe soft prepare time in detail for problem tracking.
                    IDV_SQL_OPTIME_END(sStatistics,IDV_OPTM_INDEX_WAITING_HARD_PREPARE);

                    
                    latchPrepareAsShared(aStatement->getStatistics());
                    *aPCOLatchState = MMC_PCO_LOCK_ACQUIRED_SHARED;
                    sChildPCO->getPlanState(aChildPCOPlanState);
                    sPCB->planUnFix(sStatistics);
                    if(*aChildPCOPlanState == MMC_CHILD_PCO_PLAN_IS_READY)
                    {
                        goto  check_matchedEnv;
                    }
                    else
                    {
                        //fix BUG-24607 hard-prepare��� �Ϸ��Ŀ� child PCO��
                        //old PCO list���� �Ű����� �ִ�.
                        // wait���� ������� �ش� child PCO�� old�� �ɼ� �ִ�.
                        if(*aChildPCOPlanState ==  MMC_CHILD_PCO_PLAN_IS_UNUSED)
                        {
                            //�̷��� ���. used listó������ �˻��� �����Ѵ�.
                            sIterator = &mUsedChildLst;
                            continue;
                        }
                        else
                        {
                            
                            IDE_ASSERT(*aChildPCOPlanState == MMC_CHILD_PCO_PLAN_NEED_HARD_PREPARE);
                            *aPCOLatchState = MMC_PCO_LOCK_RELEASED;
                            releasePrepareLatch();
                        }
                        break;
                    }//else
                }//else
            }//else
        }//else environment is null
    }//IDU_LIST_ITERATE_SAFE
    
    return IDE_SUCCESS;
    
    //fix BUG-30855 It needs to describe soft prepare time in detail for problem tracking.
    IDE_EXCEPTION(MissMatchEnvError);
    {
        IDV_SQL_OPTIME_END(sStatistics,IDV_OPTM_INDEX_MATCHING_CHILD_PCO);
    }

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;       
}

mmcPCB*  mmcParentPCO::getSafeGuardPCB()
{
    mmcPCB      *sPCB;
    iduListNode *sNode;

    if(IDU_LIST_IS_EMPTY(&mUsedChildLst) == ID_FALSE)
    {
        sNode = IDU_LIST_GET_FIRST(&mUsedChildLst);
        sPCB =  (mmcPCB*) sNode->mObj;
    }
    else
    {
        sNode = IDU_LIST_GET_FIRST(&mUnUsedChildLst);
        sPCB =  (mmcPCB*) sNode->mObj;
    }

    return sPCB;
}
