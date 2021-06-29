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

#include <mmuProperty.h>
#include <mmcPCB.h>

void  mmcPCB::initialize(SChar* aSQLTextID,
                         UInt   aChildPCOId)
{
    SChar sMutexName[MMC_SQL_PCB_MUTEX_NAME_LEN];
    
    mFixCount = 0;
    mFrequency = 0;
    mLRURegion = MMC_PCB_IN_NONE_LRU_REGION;
    mChildPCO = NULL;
    mParentPCO = NULL;
    IDU_LIST_INIT_OBJ(&mChildLstNode,this);
    IDU_LIST_INIT_OBJ(&mLRuListNode,this);
    //fix BUG-21429.
    idlOS::snprintf(sMutexName,MMC_SQL_PCB_MUTEX_NAME_LEN,
                    "%s""%09"ID_UINT32_FMT"PCB_MUTEX",
                    aSQLTextID,aChildPCOId);
    
    IDE_ASSERT(mMutex.initialize(sMutexName,
                                 IDU_MUTEX_KIND_NATIVE2,
                                 IDV_WAIT_INDEX_NULL ) == IDE_SUCCESS);
}

void mmcPCB::finalize()
{
    IDE_ASSERT(mMutex.destroy() == IDE_SUCCESS);
    mLRURegion = MMC_PCB_IN_NONE_LRU_REGION;
}


// PCB�� HOT-region���� COLD-region���� �ű������
// frequnecy�� 0���� �����Ѵ�.
void mmcPCB::resetFrequency(idvSQL* aStatistics)
{
    IDE_ASSERT(mMutex.lock(aStatistics) == IDE_SUCCESS);
    mFrequency =0;
    IDE_ASSERT(mMutex.unlock() == IDE_SUCCESS);
}

// PCB�� HOT-region���� COLD-region���� �ű������
// frequnecy�� ���ҽ�Ų��.
/* fix BUG-31212
   When a PCO move from cold to hot lru region, it would be better to update the frequency of a pco as
   (the frequency - SQL_PLAN_CACHE_HOT_REGION_FREQUENCY) instead of 0.*/
void mmcPCB::decFrequency(idvSQL* aStatistics)
{
    UInt sHotRegionLruFrequency = mmuProperty::getFrequencyForHotLruRegion();
    
    IDE_ASSERT(mMutex.lock(aStatistics) == IDE_SUCCESS);
    if(mFrequency <=  mmuProperty::getFrequencyForHotLruRegion())
    {
        mFrequency =0;
    }
    else
    {
        mFrequency -= sHotRegionLruFrequency;
    }
    
    IDE_ASSERT(mMutex.unlock() == IDE_SUCCESS);
}


void mmcPCB::setLRURegion(idvSQL* aStatistics,
                          mmcPCBLRURegion  aLRURegion)
{
    IDE_ASSERT(mMutex.lock(aStatistics) == IDE_SUCCESS);
    mLRURegion = aLRURegion;
    IDE_ASSERT(mMutex.unlock() == IDE_SUCCESS);
   
}

void mmcPCB::getLRURegion(idvSQL           *aStatistics,
                          mmcPCBLRURegion  *aLRURegion)
{
    IDE_ASSERT(mMutex.lock(aStatistics) == IDE_SUCCESS);
    *aLRURegion = mLRURegion;
    IDE_ASSERT(mMutex.unlock() == IDE_SUCCESS);
}



void mmcPCB::assignPCO(idvSQL       *aStatistics,
                       mmcParentPCO *aParentPCO,
                       mmcChildPCO  *aChildPCO)
{
    IDE_ASSERT(mMutex.lock(aStatistics) == IDE_SUCCESS);
    mParentPCO    = aParentPCO;
    mChildPCO     = aChildPCO;
    IDE_ASSERT(mMutex.unlock() == IDE_SUCCESS);
}


void mmcPCB::getFixCountFrequency(idvSQL* /*aStatistics*/,
                                  UInt* aFixCount,
                                  UInt* aFrequency)
{
    *aFixCount   = mFixCount;
    *aFrequency  = mFrequency;
}

void mmcPCB::getFixCount(idvSQL* aStatistics,
                         UInt* aFixCount)
{
    IDE_ASSERT(mMutex.lock(aStatistics) == IDE_SUCCESS);
    *aFixCount   = mFixCount;
    IDE_ASSERT(mMutex.unlock() == IDE_SUCCESS);
    
}

// BUG-23144 �ζ��� �Լ����� ���ü� ��� �ϸ� ��Ȥ �׽��ϴ�.
// �ζ����� ó������ �ʽ��ϴ�.
void mmcPCB::planFix(idvSQL* aStatistics)
{
    IDE_ASSERT(mMutex.lock(aStatistics) == IDE_SUCCESS);
    mFrequency++;
    mFixCount++;
    if(mFixCount == 0)
    {
        mFixCount = 1;
    }
    IDE_ASSERT(mMutex.unlock() == IDE_SUCCESS);

}

void mmcPCB::planUnFix(idvSQL* aStatistics)
{
    IDE_ASSERT(mMutex.lock(aStatistics) == IDE_SUCCESS);

    IDE_DASSERT(mFixCount != 0);
    mFixCount--;
    IDE_ASSERT(mMutex.unlock() == IDE_SUCCESS);
}

void mmcPCB::getFrequency(UInt* aFrequency)
{
    *aFrequency = mFrequency;    
}


mmcChildPCO* mmcPCB::getChildPCO()
{
    return mChildPCO;
}

mmcParentPCO* mmcPCB::getParentPCO()
{
    return mParentPCO;
}
