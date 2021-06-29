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

#ifndef _O_SDB_PREPARE_LIST_H_
#define _O_SDB_PREPARE_LIST_H_ 1

#include <sdbDef.h>
#include <idu.h>
#include <idv.h>
#include <sdbBCB.h>

class sdbPrepareList
{
public:
    IDE_RC initialize(UInt aListID);

    IDE_RC destroy();

    sdbBCB* removeLast(idvSQL *aStatistics);

    IDE_RC addBCBList(idvSQL *aStatistics,
                      sdbBCB *aFirstBCB,
                      sdbBCB *aLastBCB,
                      UInt    aLength);

    inline UInt getLength();

    inline UInt getWaitingClientCount();

    IDE_RC timedWaitUntilBCBAdded(idvSQL *aStatistics,
                                  UInt    aTimeMilliSec,
                                  idBool *aBCBAdded);

    // sdbBCBListStat�� ���� �Լ���
    inline sdbBCB* getFirst();
    inline sdbBCB* getLast();
    inline UInt    getID();

private:
    IDE_RC checkValidation(idvSQL *aStatistics);

private:
    /* prepare list �ĺ���. prepare list�� ������ ������ �� �����Ƿ�, �� ����Ʈ ����
     * �ĺ��ڸ� �д�.  �� �ĺ��ڸ� ���� BCB���� ���������� �ؼ� �� BCB��
     * � ����Ʈ�� �����ִ��� ��Ȯ�� �˾Ƴ� �� �ִ�.*/
    UInt      mID;

    /* smuList���� �ʿ�� �ϴ� base list ������ mData�� �ƹ��͵� ����Ű��
     * ���� �ʴ´�.*/
    smuList     mBaseObj;
    smuList    *mBase;

    iduMutex    mMutex;
    /* prepare list�� �������, BCB�� ���ԵǱ⸦  ����ϱ� ���� mutex */
    iduMutex    mMutexForWait;
    /* prepare list�� �������, BCB�� ���ԵǱ⸦  ����ϱ� ���� cond variable */
    iduCond     mCondVar;

    UInt        mListLength;
    /* ���� prepare list�� ���ؼ� ������� �������� ���� */
    UInt        mWaitingClientCount;
};

/***********************************************************************
 * Description :
 *  ���� list�� ���̸� ��´�. ���ؽ��� ���� �ʱ� ������, ��Ȯ�� ���� ��Ȳ��
 *  �ݿ����� ���� �� �ִ�.
 ***********************************************************************/
UInt sdbPrepareList::getLength()
{
    return mListLength;
}

sdbBCB* sdbPrepareList::getFirst()
{
    if (SMU_LIST_IS_EMPTY(mBase))
    {
        return NULL;
    }
    else
    {
        return (sdbBCB*)SMU_LIST_GET_FIRST(mBase)->mData;
    }
}

sdbBCB* sdbPrepareList::getLast()
{
    if (SMU_LIST_IS_EMPTY(mBase))
    {
        return NULL;
    }
    else
    {
        return (sdbBCB*)SMU_LIST_GET_LAST(mBase)->mData;
    }
}

UInt sdbPrepareList::getID()
{
    return mID;
}

UInt sdbPrepareList::getWaitingClientCount()
{
    return mWaitingClientCount;
}

#endif // _O_SDB_PREPARE_LIST_H_
