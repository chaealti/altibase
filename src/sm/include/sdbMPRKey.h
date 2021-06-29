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
 **********************************************************************/

#ifndef _O_SDB_MPR_KEY_H_
#define _O_SDB_MPR_KEY_H_ 1

#include <smDef.h>
#include <sdbDef.h>
#include <sdbBCB.h>

#define SDB_MAX_MPR_PAGE_COUNT   (128)

typedef enum
{
    SDB_MRPBCB_TYPE_BUFREG = 1,
    SDB_MRPBCB_TYPE_NEWFETCH
} sdbMPRBCBType;

/****************************************************************
 * MPR���� �������� ���ļ� �д� ��쿡�� ���ӵǴ� �������� �������� �д´�.
 * �̶�, �ѹ��� read�ϰ� �Ǵ� ������ sdbReadUnit�̶�� �Ѵ�.
 *
 * ���⿣ FirstPID�� ���ӵ� ������ ����, �׸��� ���� BCB�� ����Ű��
 * ������ ������ ������ �ִ�.
 ****************************************************************/
typedef struct sdbMPRBCB
{
    sdbBCB        *mBCB;
    sdbMPRBCBType  mType;
} sdbMPRBCB;

typedef struct sdbReadUnit
{
    scPageID     mFirstPID;
    UInt         mReadBCBCount;
    sdbMPRBCB  * mReadBCB;
} sdbReadUnit;

typedef enum
{
    SDB_MPR_KEY_CLEAN = 0,
    SDB_MPR_KEY_FETCHED
} sdbMPRKeyState;
/****************************************************************
 * PROJ-1568 ���蹮���� 3.3 ����.
 * Multi Page Read�� ���� �����ϴ� �ڷᱸ��.
 * MPR���� �о�� �ϴ� �������� ���� ���ƾ� �ϴ� �������� ���� ������ ������.
 ****************************************************************/
class sdbMPRKey 
{ 
public:
    inline void    initFreeBCBList();

    inline sdbBCB* removeLastFreeBCB();

    inline void    addFreeBCB(sdbBCB *aBCB);

    inline void    removeAllBCB(sdbBCB **aFirstBCB,
                                sdbBCB **aLastBCB,
                                UInt    *aBCBCount);

    inline idBool  hasNextPage();

    inline void    moveToNext();
    inline void    moveToBeforeFst();

public:
    scSpaceID      mSpaceID;
    SInt           mIOBPageCount;
    SInt           mReadUnitCount;
    sdbReadUnit    mReadUnit[SDB_MAX_MPR_PAGE_COUNT/2];
   
    // MPR�� target�� �� BCB���� ���� read�� �߰� ���߰� ��� ���ۿ� �־��
    // �Ѵ�. �׸��� �׵��� ��� mBCBs���� �������ϰ� �ִ�.
    // MPRKey�� �̿��Ͽ� getPage�� �Ҷ��� �ݵ�� ������� ������ �ؾ� �Ѵ�.
    // �̶�, ���� ���� �߿� BCB�� ���� ������ mCurrentIndex���� �����Ѵ�.
    sdbMPRBCB      mBCBs[SDB_MAX_MPR_PAGE_COUNT];
    SInt           mCurrentIndex;
    
    /* MPR���� �������� ������ �о� ���̱� ���ؼ�, �������� �޸𸮰� �ʿ��ϴ�.
     * �̰��� ���ؼ� �̸� ������ �Ҵ��Ѵ�. �� ������ ũ��� �ѹ��� �о� ���̴�
     * ��츦 ����� ũ���̴�.
     * */
    UChar        * mReadIOB;
    
    /* MPR������ frame���� ���Ǵ� �޸𸮸� ���� �Ҵ����� �ʰ�, ������ �����ϴ�
     * BCB�� �̿��Ѵ�. �̰��� getVictim�� ���ؼ� ������ ������, ���� �Ŵ�����
     * overhead�� �� �� �ִ�.  �׷��� ������, ���� mFreeList�� �����Ѵ�.
     * �⺻������ MPR�� ����ϰ� �� BCB�� ���۸Ŵ����� �ݳ����� �ʰ�,
     * mFreeList�� �ݳ��Ѵ�. ���۸Ŵ����� �ݳ��ϴ� ����, �ٸ� �������� ����
     * hit(fix)�� �� ����̴�. */
    smuList        mFreeListBase;
    SInt           mFreeBCBCount;
    sdbMPRKeyState mState;
};

void sdbMPRKey::initFreeBCBList()
{
    mFreeBCBCount = 0;
    SMU_LIST_INIT_BASE(&mFreeListBase);
}

/****************************************************************
 * Description:
 *  mFreeList���� FreeBCB�ϳ��� �����Ѵ�.
 ****************************************************************/
sdbBCB* sdbMPRKey::removeLastFreeBCB()
{
    sdbBCB  *sRet;
    smuList *sNode;

    if (SMU_LIST_IS_EMPTY(&mFreeListBase))
    {
        sRet = NULL;
    }
    else
    {
        sNode = SMU_LIST_GET_LAST(&mFreeListBase);
        SMU_LIST_DELETE(sNode);

        sRet = (sdbBCB*)sNode->mData;
        SDB_INIT_BCB_LIST(sRet);
        IDE_DASSERT( mFreeBCBCount != 0);
        mFreeBCBCount--;
    }

    return sRet;
}

/****************************************************************
 * Description:
 *  mFreeList�� FreeBCB�ϳ��� �߰��Ѵ�.
 ****************************************************************/
void sdbMPRKey::addFreeBCB(sdbBCB *aBCB)
{
    mFreeBCBCount++;
    SMU_LIST_ADD_FIRST(&mFreeListBase, &aBCB->mBCBListItem);
}

/****************************************************************
 * Description:
 *  mFreeList�� �ִ� ��� Free BCB ���� List���·� �����Ѵ�.
 ****************************************************************/
void sdbMPRKey::removeAllBCB(sdbBCB **aFirstBCB,
                             sdbBCB **aLastBCB,
                             UInt    *aBCBCount)
{
    smuList *sFirst = SMU_LIST_GET_FIRST(&mFreeListBase);
    smuList *sLast  = SMU_LIST_GET_LAST(&mFreeListBase);

    SMU_LIST_CUT_BETWEEN(&mFreeListBase, sFirst);
    SMU_LIST_CUT_BETWEEN(sLast, &mFreeListBase);
    SMU_LIST_INIT_BASE(&mFreeListBase);

    *aFirstBCB = (sdbBCB*)sFirst->mData;
    *aLastBCB  = (sdbBCB*)sLast->mData;
    *aBCBCount = mFreeBCBCount;

    mFreeBCBCount = 0;
}


/****************************************************************
 * Description:
 *  ���̻� MPR���� getPage�� �� �ִ� �������� ���� �ִ��� Ȯ���Ѵ�.
 ****************************************************************/
idBool sdbMPRKey::hasNextPage()
{
    return ( ( mCurrentIndex + 1 ) < mIOBPageCount ? ID_TRUE : ID_FALSE);
}

/****************************************************************
 * Description:
 *  ���� �������� ���ؼ� getPage�ϱ� ���� ȣ���Ѵ�.
 ****************************************************************/
void sdbMPRKey::moveToNext()
{
    IDE_ASSERT(mCurrentIndex <= mIOBPageCount);
    mCurrentIndex++;
}

void sdbMPRKey::moveToBeforeFst()
{
    mCurrentIndex = -1;
}

#endif // _O_SDB_MPR_KEY_H_
