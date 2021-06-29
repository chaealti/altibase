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


#ifndef _O_SDB_FLUSH_LIST_H_
#define _O_SDB_FLUSH_LIST_H_ 1

#include <sdbDef.h>
#include <idu.h>
#include <idv.h>
#include <sdbBCB.h>

class sdbFlushList
{
public:
    IDE_RC initialize( UInt aListID, sdbBCBListType aListType );

    IDE_RC destroy();

    void beginExploring(idvSQL *aStatistics);

    void endExploring(idvSQL *aStatistics);

    sdbBCB* getNext(idvSQL *aStatistics);

    void remove(idvSQL *aStatistics, sdbBCB *aTarget);

    void add(idvSQL *aStatistics, sdbBCB *aBCB);

    IDE_RC checkValidation(idvSQL *aStatistics);

    inline UInt getPartialLength();

    // sdbBCBListStat�� ���� �Լ���
    inline sdbBCB*          getFirst();
    inline sdbBCB*          getLast();
    inline UInt             getID();
    inline sdbBCBListType   getListType();
    inline void             setMaxListLength( UInt aMaxListLength );
    inline idBool           isUnderMaxLength();

private:
    /* FlushList �ĺ���. Flush����Ʈ�� ������ ������ �� �����Ƿ�, �� ����Ʈ ����
     * �ĺ��ڸ� �д�.  �� �ĺ��ڸ� ���� BCB���� ���������� �ؼ� �� BCB��
     * � ����Ʈ�� �����ִ��� ��Ȯ�� �˾Ƴ� �� �ִ�.*/
    UInt      mID;

    /* PROJ-2669
     * Normal/Delayed Flush List ������ ���� �ʿ�
     * SDB_BCB_FLUSH_LIST or SDB_BCB_FLUSH_HOT_LIST */
    sdbBCBListType mListType;
    
    /* smuList���� �ʿ�� �ϴ� base list ������ mData�� �ƹ��͵� ����Ű��
     * ���� �ʴ´�.*/
    smuList   mBaseObj;
    smuList  *mBase;
    /* getNext�� ȣ���Ҷ�, �����ؾ� �ϴ� BCB�� ������ �ִ� ���� */
    smuList  *mCurrent;
    /* ���� flush list�� �����ϰ� �ִ� flusher�� ����, beginExploring
     * �ÿ� ���� ���� �ǰ�, endExploring�ÿ� ���� �����Ѵ�.*/
    SInt      mExplorerCount;

    iduMutex  mMutex;

    UInt      mListLength;

    /* PROJ-2669 Flush List �ִ� ���� */
    UInt      mMaxListLength;
};  

/* BUGBUG:
 * flusher���� � FLUSH List�� flush�ؾ� ���� �����ϴµ�, length�� ����
 * �����ϴ� ��쿡, ������ ���� �� �ִ�.
 * �ֳĸ�, flusher�� ���ͼ� 
 * */
UInt sdbFlushList::getPartialLength()
{
    return mListLength;
}

sdbBCB* sdbFlushList::getFirst()
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

sdbBCB* sdbFlushList::getLast()
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

UInt sdbFlushList::getID()
{
    return mID;
}

sdbBCBListType sdbFlushList::getListType()
{
    return mListType;
}

void sdbFlushList::setMaxListLength( UInt aMaxListLength )
{
    mMaxListLength      = aMaxListLength;
}

idBool sdbFlushList::isUnderMaxLength()
{
    return (idBool)( mListLength < mMaxListLength );
}

#endif // _O_SDB_FLUSH_LIST_H_

