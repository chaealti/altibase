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
 * $Id:$
 **********************************************************************/
/***********************************************************************
 * PROJ-1568 BUFFER MANAGER RENEWAL
 ***********************************************************************/

#ifndef _O_SDB_LRU_LIST_H_
#define _O_SDB_LRU_LIST_H_ 1

#include <sdbDef.h>
#include <idu.h>
#include <idv.h>
#include <sdbBCB.h>
#include <sdbBufferPoolStat.h>

class sdbLRUList
{
public:
    IDE_RC  initialize(UInt aListID, UInt aHotMax, sdbBufferPoolStat *aStat);

    IDE_RC  destroy();

    void    setHotMax(idvSQL *aStatistics, UInt aNewHotMax);

    sdbBCB* removeColdLast(idvSQL *aStatistics);
    idBool  removeBCB(idvSQL *aStatistics, sdbBCB *aBCB);

    void    addToHot(idvSQL *aStatistics, sdbBCB *aBCB);

    void    insertBCB2BehindMid(idvSQL *aStatistics, sdbBCB *aBCB);

    IDE_RC  checkValidation(idvSQL *aStatistics);
    
    UInt    moveMidPtrForward(idvSQL *aStatistics, UInt aMoveCnt);

    inline UInt    getHotLength();

    inline UInt    getColdLength();

    inline UInt    getID();

    void dump();

    // fixed table ������ ���� ����
    // ���ü��� ���� ����Ǿ� ���� ����
    inline sdbBCB* getFirst();
    inline sdbBCB* getMid();
    inline sdbBCB* getLast();

private:
    /* LRUList �ĺ���. LRU����Ʈ�� ������ ������ �� �����Ƿ�, �� ����Ʈ ����
     * �ĺ��ڸ� �д�.  �� �ĺ��ڸ� ���� BCB���� ���������� �ؼ� �� BCB��
     * � ����Ʈ�� �����ִ��� ��Ȯ�� �˾Ƴ� �� �ִ�.*/
    UInt      mID;

    /* LRUList�� base */
    smuList   mBaseObj;
    smuList  *mBase;
    /* Cold first�� mMid�� ����Ű�� �ִ�. cold�� �����Ҷ� mMid�� mPrev�� �����ϰ�
     * ������ BCB�� mMid�� �����Ѵ�.*/
    smuList  *mMid;

    /* LRU List���� ���� Hot BCB�� ���� */
    UInt      mHotLength;
    /* LRU List�� ������ �ִ� Hot BCB�� �ִ�ġ */
    UInt      mHotMax;

    iduMutex  mMutex;

    UInt      mColdLength;

    sdbBufferPoolStat *mStat;
};

UInt sdbLRUList::getID()
{
    return mID;
}

UInt sdbLRUList::getColdLength()
{
    return mColdLength;
}

UInt sdbLRUList::getHotLength()
{
    return mHotLength;
}

/***********************************************************************
 * Description :
 *  fixed table ������ ���� ����. ���ü��� ���� ����Ǿ� ���� ����.
 *  ����Ʈ�� ���� ó�� BCB�� ���´�.
 ***********************************************************************/
sdbBCB* sdbLRUList::getFirst()
{
    if (SMU_LIST_IS_EMPTY(mBase))
    {
        return NULL;
    }
    else
    {
        if (SMU_LIST_GET_FIRST(mBase) == mMid)
        {
            return NULL;
        }
        else
        {
            return (sdbBCB*)(SMU_LIST_GET_FIRST(mBase))->mData;
        }
    }
}

/***********************************************************************
 * Description :
 *  fixed table ������ ���� ����. ���ü��� ���� ����Ǿ� ���� ����.
 *  midPoint�� BCB�� ���´�.
 ***********************************************************************/
sdbBCB* sdbLRUList::getMid()
{
    return mMid == NULL ? NULL : (sdbBCB*)mMid->mData;
}

/***********************************************************************
 * Description :
 *  fixed table ������ ���� ����. ���ü��� ���� ����Ǿ� ���� ����.
 *  ����Ʈ�� ���� ������ BCB�� ���´�.
 ***********************************************************************/
sdbBCB* sdbLRUList::getLast()
{
    if (SMU_LIST_IS_EMPTY(mBase))
    {
        return NULL;
    }
    else
    {
        return (sdbBCB*)(SMU_LIST_GET_LAST(mBase))->mData;
    }
}

#endif // _O_SDB_LRU_LIST_H_

