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
#include <mmcChildPCO.h>

void mmcChildPCO::initialize(SChar          *aSQLTextId,
                             const UInt      aChildID,
                             mmcChildPCOCR   aCreateReason,
                             const UInt      aRebuildedCnt,
                             UInt            *aHitCntPtr)
{
    SChar sLatchName[IDU_MUTEX_NAME_LEN];

    idlOS::snprintf(sLatchName,
                    IDU_MUTEX_NAME_LEN,
                    "MMC_CHILD_PCO_LATCH_%"ID_UINT32_FMT,
                    aChildID);

    // PROJ-2408
    IDE_ASSERT(mPrepareLatch.initialize(sLatchName) == IDE_SUCCESS);
    mSharedPlan    =  NULL;
    mSharedPlanSize = 0;
    mPreparedPrivateTemplate = NULL;
    mPreparedPrivateTemplateSize = 0;
    mEnvState = MMC_CHILD_PCO_ENV_IS_NOT_READY;
    mSQLTextId    = aSQLTextId;
    mChildID      = aChildID;
    mPlanState    = MMC_CHILD_PCO_PLAN_IS_NOT_READY;
    mCreateReason = aCreateReason;
    mRebuildedCnt = aRebuildedCnt;
    mHitCntPtr    = aHitCntPtr;
}

void mmcChildPCO::finalize()
{
    if(mSharedPlan != NULL)
    {
        IDE_ASSERT(qci::freeSharedPlan(mSharedPlan) == IDE_SUCCESS);
        mSharedPlan = NULL;
        mSharedPlanSize = 0;
    }
    if(mPreparedPrivateTemplate != NULL)
    {
        freePreparedPrivateTemplate();
    }
   
    // PROJ-2163
    // bindParam �� childPCO �� ���� �ִ� QC_QMP_MEM �� PlanBindInfo �� �������ϰ� �ִ�.
    // childPCO �� ������ ��� ������ ������ NULL �� �����Ѵ�.
    mPlanBindInfo.bindParam = NULL; 

    // PROJ-2408
    IDE_ASSERT ( mPrepareLatch.destroy() == IDE_SUCCESS );
}

void mmcChildPCO::latchPrepareAsShared(idvSQL* aStatistics)
{   
    // PROJ-2408
    IDE_ASSERT( mPrepareLatch.lockRead ( aStatistics, NULL ) == IDE_SUCCESS );
}


void mmcChildPCO::latchPrepareAsExclusive(idvSQL* aStatistics)
{
    // PROJ-2408
    IDE_ASSERT(mPrepareLatch.lockWrite(aStatistics, NULL) == IDE_SUCCESS);
}


void mmcChildPCO::releasePrepareLatch()
{
    // PROJ-2408
    IDE_ASSERT ( mPrepareLatch.unlock() == IDE_SUCCESS );
}

void  mmcChildPCO::wait4HardPrepare(idvSQL* aStatistics)
{

    latchPrepareAsShared(aStatistics);
    //fix BUG-24607 hard-prepare��� �Ϸ��Ŀ� child PCO��
    //old PCO list���� �Ű����� �ִ�.
    // parent PCO prepare-latch�� �������¿���
    // search Child PCO���� child PCO���¸� �˻��ϵ��� �Ѵ�.
    releasePrepareLatch();
}

void mmcChildPCO::freePreparedPrivateTemplate()
{
    qci::freePrepPrivateTemplate(mPreparedPrivateTemplate);
    mPreparedPrivateTemplate = NULL;
    mPreparedPrivateTemplateSize = 0;
}

void mmcChildPCO::assignEnv(qciPlanProperty* aEnvironment, 
                            qciPlanBindInfo* aPlanBindInfo) // PROJ-2163
{
    ID_SERIAL_BEGIN(idlOS::memcpy(&mEnvironment,aEnvironment,ID_SIZEOF(mEnvironment)));

    // BUG-36956
    if( aPlanBindInfo == NULL )
    {
        // new C-PCO �� planBindInfo �� �ʱⰪ�� NULL
        ID_SERIAL_EXEC(idlOS::memset(&mPlanBindInfo,0x00,ID_SIZEOF(mPlanBindInfo)),1);
    }
    else
    {
        // PROJ-2163
        ID_SERIAL_EXEC(idlOS::memcpy(&mPlanBindInfo,aPlanBindInfo,ID_SIZEOF(mPlanBindInfo)),1);

        // BUG-42512
        // aPlanBindInfo �� NULL �� ���� search ������� ������ �� ��
        ID_SERIAL_END(mEnvState = MMC_CHILD_PCO_ENV_IS_READY);
    }
}

void mmcChildPCO::assignPlan(void  *aSharedPlan,
                             UInt   aSharedPlanSize,
                             void  *aPreparedPrivateTemplate,
                             UInt   aPreparedPrivateTemplateSize)
{

    mSharedPlan = aSharedPlan;
    mSharedPlanSize = aSharedPlanSize;
    mPreparedPrivateTemplate = aPreparedPrivateTemplate;
    mPreparedPrivateTemplateSize = aPreparedPrivateTemplateSize;
    
    IDL_MEM_BARRIER;
    
    mPlanState = MMC_CHILD_PCO_PLAN_IS_READY;
}

void mmcChildPCO::updatePlanState(mmcChildPCOPlanState aPlanState)
{
    mPlanState = aPlanState;
}


