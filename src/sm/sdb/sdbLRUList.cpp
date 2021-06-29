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
 * $$Id:$
 **********************************************************************/
/***********************************************************************
 * PROJ-1568 BUFFER MANAGER RENEWAL
 ***********************************************************************/

/***********************************************************************
 * Description : ����Ǯ���� ����ϴ� LRU list�� ������ �����̴�.
 *               LRU list�� ��ü�� ���Ǹ� ����ȭ ����� ����������
 *               ����Ǳ� ������ ���� lock, unlock �������̽���
 *               �������� �ʴ´�.
 *               �˰����� HOT-COLD LRU �˰����� ����Ѵ�.
 *
 *   + �������
 *     - �ʱ�ȭ�� �� aHotMax�� �ּ��� 1���� Ŀ���Ѵ�.
 *       �� hot ������ �ִ� ũ��� �ּ��� 1�̰ų� �׺��� Ŀ���Ѵ�.
 *       ���� ���� �˰���(�ܼ� LRU)�� ������ �˰�����
 *       ����ϱ����ؼ��� aHotMax�� 1���ϸ�ȴ�.
 *
 *   + ������ ���� �� �˰���
 *      . mHotLength�� mColdLength�� ���ؽ��� ���� ���ȿ� �׻� ��Ȯ�� ������
 *          �����ϰ� �ִ�.
 *      . hotBCB�� ���� ���� ��쿣 mMid�� mBase�� ����Ű�� �ִ�.
 *      . hotBCB�� �ϳ��� �ִٸ�  mMid�� hotLast�� ����Ű�� �ִ�.
 *      . ��, mMid�� mBase�� ���ٴ� ���� hot������ bcb�� ���� ������ �ǹ��Ѵ�.
 *      
 *      . cold�� ������ �׻� mMid->mNext ���̷�� ����. �̶�, mMid����Ʈ��
 *          �̵��� ���� ������ �ָ�. �ֳĸ� mMid����Ʈ�� hotLast�� ����Ű�� ����.
 *      . Hot���� ���Զ��� ���� mHotMax���� BCB�� ���� �ʾҴٸ�, ���� mMid����Ʈ
 *          �̵��� ���� ������ �ָ�. �ֳĸ� mMid����Ʈ�� hotLast�� ����Ű�� ����.
 *      . Hot���� ���Զ��� ���� mHotMax���� BCB�� á�ٸ�, mMid->mPrev��
 *          mMid������ �̵�
 *
 *      . cold ���� => mMid->mNext�� ����
 *      . cold ���� => (if cold�� �����Ҷ���) mBase->mPrev����
 *      . hot ����  => (if hot�� ���ٸ�) mBase->mNext�� ����; mMid������ ����
 *                    (if hot�� �ִٸ�) mBase->mNext�� ����
 *
 *      . ����Ʈ �߰����� ���� => �׳� �����ϸ� ������, ���� ���� ����� mMid���
 *                              mMid�����͸� mMid->mPrev�� ����..
 *
 ***********************************************************************/

#include <sdbLRUList.h>

/* Hot������ �ִ� BCB�� Cold�������� �Űܿö� �����ϴ� ���� */
#define SDB_MAKE_BCB_COLD(node) {                                       \
        ((sdbBCB*)(node)->mData)->mTouchCnt    = 1;                     \
        ((sdbBCB*)(node)->mData)->mBCBListType = SDB_BCB_LRU_COLD;      \
    }


/***********************************************************************
 * Description :
 *  LRUList�� �ʱ�ȭ �Ѵ�.
 * 
 *  aListID     - [IN]  ����Ʈ ID
 *  aHotMax     - [IN]  ����Ʈ���� hot�� ������ �� �ִ� �ִ� ����.
 *  aStat       - [IN]  ����Ǯ �������
 ***********************************************************************/
IDE_RC sdbLRUList::initialize(UInt               aListID,
                              UInt               aHotMax,
                              sdbBufferPoolStat *aStat)
{
    SChar sMutexName[128];
    SInt  sState = 0;

    // aHotMax�� 1 �̻��̾�� �Ѵ�.
    if (aHotMax == 0)
    {
        aHotMax = 1;
    }

    mID              = aListID;
    mBase            = &mBaseObj;
    mMid             = mBase;
    mHotLength       = 0;
    mHotMax          = aHotMax;
    mColdLength	     = 0;
    mStat            = aStat;

    SMU_LIST_INIT_BASE( mBase );

    idlOS::memset(sMutexName, 0, 128);
    idlOS::snprintf(sMutexName, 128, "LRU_LIST_MUTEX_%"ID_UINT32_FMT, aListID);

    IDE_TEST(mMutex.initialize(sMutexName,
                               IDU_MUTEX_KIND_NATIVE,
                               IDV_WAIT_INDEX_LATCH_FREE_DRDB_LRU_LIST)
             != IDE_SUCCESS);
    sState = 1;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch (sState)
    {
        case 1:
            IDE_ASSERT(mMutex.destroy() == IDE_SUCCESS);
        default:
            break;
    }
    return IDE_FAILURE;
}

/***********************************************************************
 * Description :
 *  �����Լ�
 ***********************************************************************/
IDE_RC sdbLRUList::destroy()
{
    IDE_TEST(mMutex.destroy() != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/****************************************************************
 * Description:
 *  ColdLast���� BCB�� �ϳ� �����Ѵ�. cold������ BCB�� ���� ���ٸ� NULL�� ����.
 *  
 *  aStatistics - [IN]  �������
 ****************************************************************/
sdbBCB* sdbLRUList::removeColdLast(idvSQL *aStatistics)
{
    sdbBCB  *sRet    = NULL;
    smuList *sTarget = NULL;

    if( mColdLength == 0 )
    {
        // ���ؽ��� ���� �ʾұ� ������ ��Ȯ���� ���� �� �ִ�.
        sRet = NULL;
    }
    else
    {
        IDE_ASSERT(mMutex.lock(aStatistics) == IDE_SUCCESS);

        if( mColdLength != 0 )
        {
            sTarget = SMU_LIST_GET_PREV(mBase);
            
            SMU_LIST_DELETE(sTarget);

            sRet = (sdbBCB*)sTarget->mData;
            SDB_INIT_BCB_LIST(sRet);

            IDE_ASSERT( mColdLength != 0 );
            mColdLength--;
        }
        else
        {
            IDE_ASSERT( mBase->mPrev == mMid );
            sRet = NULL;
        }

        IDE_ASSERT(mMutex.unlock() == IDE_SUCCESS);
    }

    IDE_DASSERT(checkValidation(aStatistics) == IDE_SUCCESS);

    return sRet;
}




/****************************************************************
 * Description:
 *  Mid�����͸� Hot �������� aMoveCnt��ŭ �̵���Ų��.
 *  ���� aMoveCnt�� mHotLength���� ū��쿡�� ���� �����Ͽ�,
 *  mHotLength��ŭ�� �ű��.
 *  
 *  aStatistics - [IN]  �������
 *  aMoveCnt    - [IN]  hot�������� �̵��ϴ� Ƚ��
 ****************************************************************/
UInt sdbLRUList::moveMidPtrForward(idvSQL *aStatistics, UInt aMoveCnt)
{
    UInt sMoveCnt = aMoveCnt;
    UInt i;
    
    IDE_ASSERT(mMutex.lock(aStatistics) == IDE_SUCCESS);
    
    if( mHotLength < sMoveCnt )
    {
        sMoveCnt = mHotLength;
    }

    for( i = 0 ; i < sMoveCnt; i++)
    {
        SDB_MAKE_BCB_COLD(mMid);
        mMid = SMU_LIST_GET_PREV(mMid);
    }

    mColdLength += sMoveCnt;
    
    IDE_ASSERT(mHotLength >= sMoveCnt );
    mHotLength  -= sMoveCnt;
    
    IDE_ASSERT(mMutex.unlock() == IDE_SUCCESS);
    
    return sMoveCnt;
}

/****************************************************************
 * Description:
 *  LRU�� Hot������ BCB�� �Ѱ� �����Ѵ�.
 *  �̶�, ������ Hot������ BCB�� �Ѱ��� ���ٸ�, mBase�� mMid�� ����,
 *  ���� �Ŀ� mMid�� ������ BCB�� �����Ѵ�. �ֳĸ�, mMid�� hotLast�� ����
 *  Ű�� �����̴�.
 *  �׷��� �ʰ� Hot������ BCB�� �ִٸ�, �׳� mBase->mNext�� �����ϸ� �ȴ�.
 *  ���� mHotMax�� ���� ��쿣 mMid�� ����Ű�� BCB�� cold�� �����,
 *  mMid�� mMid->mPrev�� �����Ѵ�. 
 *  
 *  aStatistics - [IN]  �������
 *  aBCB        - [IN]  �ش� BCB
 ****************************************************************/
void sdbLRUList::addToHot(idvSQL *aStatistics, sdbBCB *aBCB)
{
    IDE_DASSERT(aBCB != NULL);

    IDE_ASSERT(mMutex.lock(aStatistics) == IDE_SUCCESS);

    /* BUG-22550: mHotLength���� Lock�� �����ʰ� IDE_DASSERT��
     * Ȯ���ϰ� �ֽ��ϴ�.
     *
     * Lock�� ���� ���¿��� ���� Ȯ���ؾ� �մϴ�.
     * */
    IDE_DASSERT(mHotLength <= mHotMax);

    aBCB->mBCBListType = SDB_BCB_LRU_HOT;
    aBCB->mBCBListNo   = mID;

    SMU_LIST_ADD_FIRST(mBase, &aBCB->mBCBListItem);
    mHotLength++;
    mStat->applyHotInsertions();

    /* mHotLength�� 1�ΰ�쿣 mMid�� mBase�� ����Ű�� �ʰ�,
     * HotLast�� �����Ѿ� �Ѵ�.
     */
    if( mHotLength == 1 )
    {
        IDE_DASSERT( mMid == mBase);
        mMid = &(aBCB->mBCBListItem);
    }

    if ( mHotLength > mHotMax )
    {
        // hot ������ ������ �����̴�.
        // �� ��� mMid�� ��ܾ� �Ѵ�.
        SDB_MAKE_BCB_COLD(mMid);
        mMid = SMU_LIST_GET_PREV(mMid);
        mColdLength ++;
        mHotLength  --;
    }
    
    IDE_ASSERT(mMutex.unlock() == IDE_SUCCESS);

    IDE_DASSERT(checkValidation(aStatistics) == IDE_SUCCESS );
}

/****************************************************************
 * Description:
 *  mMid �ڿ� BCB�� �Ѱ� �����Ѵ�. 
 *  
 *  aStatistics - [IN]  �������
 *  aBCB        - [IN]  �ش� BCB
 ****************************************************************/
void sdbLRUList::insertBCB2BehindMid(idvSQL *aStatistics, sdbBCB *aBCB)
{
    IDE_DASSERT(aBCB != NULL);
    
    IDE_ASSERT(mMutex.lock(aStatistics) == IDE_SUCCESS);
    
    aBCB->mTouchCnt    = 1;
    aBCB->mBCBListType = SDB_BCB_LRU_COLD;
    aBCB->mBCBListNo   = mID;

    mColdLength++;

    SMU_LIST_ADD_AFTER(mMid, &aBCB->mBCBListItem);
    
    mStat->applyColdInsertions();

    IDE_ASSERT(mMutex.unlock() == IDE_SUCCESS);

    IDE_DASSERT(checkValidation(aStatistics) == IDE_SUCCESS);
}

/****************************************************************
 * Description:
 *  LRU ����Ʈ�� �����ϴ� aBCB�� ����Ʈ���� �����Ѵ�.
 *  ���� aBCB�� ����Ʈ�� �������� �ʴ´ٸ�, ID_FALSE�� �����Ѵ�.
 *  �׿ܿ� ID_TRUE�� ����.
 *
 *  aStatistics - [IN]  �������
 *  aBCB        - [IN]  ���� �ϰ��� �ϴ� BCB
 ****************************************************************/
idBool sdbLRUList::removeBCB(idvSQL *aStatistics, sdbBCB *aBCB )
{
    UInt    sListType;
    idBool  sRet;
    
    IDE_DASSERT(aBCB != NULL);
    
    sListType   = aBCB->mBCBListType;
    sRet        = ID_FALSE;    
    
    /* �⺻������ ���ؽ� ���� �ʰ�, ���� list�� bcb�� ������ �������� Ȯ���Ѵ�.
     * �� ���Ŀ� �ٽ� ���ؽ� ��� ��Ȯ�� Ȯ���Ѵ�.*/
    if(((sListType == SDB_BCB_LRU_HOT) ||
        (sListType == SDB_BCB_LRU_COLD )) &&
       (aBCB->mBCBListNo == mID ))
    {
        IDE_ASSERT(mMutex.lock(aStatistics) == IDE_SUCCESS);

        if(((aBCB->mBCBListType == SDB_BCB_LRU_HOT) ||
            (aBCB->mBCBListType == SDB_BCB_LRU_COLD )) &&
           (aBCB->mBCBListNo == mID ))
        {
            /* aBCB�� mMid�� ��쿣 mMid�� mPrev�� ��ĭ �̵��ϰ�,
             * �׿ܿ� �׳� ������ �ϸ� �ȴ�.*/
            if( aBCB->mBCBListType == SDB_BCB_LRU_HOT )
            {
                if( mMid == &(aBCB->mBCBListItem ))
                {
                    mMid = SMU_LIST_GET_PREV(mMid);
                }
                mHotLength--;
            }
            else
            {
                mColdLength--;
            }
            SMU_LIST_DELETE(&(aBCB->mBCBListItem));
            SDB_INIT_BCB_LIST(aBCB);

            sRet = ID_TRUE;
        }
        IDE_ASSERT(mMutex.unlock() == IDE_SUCCESS);
        IDE_DASSERT(checkValidation(aStatistics) == IDE_SUCCESS);
    }

    return sRet;
}

/****************************************************************
 * Description:
 *  mHotMax���� ���� �����Ѵ�.
 *  ���� ���� �����ϴ�  mHotMax���� ������ mHotMax���� ũ�ٸ�
 *  �׳� �����ϸ� ������, �۴ٸ� ������ hot������ �����ϴ� BCB��
 *  cold�������� �̵��� ���Ѿ� �Ѵ�.
 *  
 *  aStatistics - [IN]  �������
 *  aNewHotMax  - [IN]  ���� ������ hotMax��. BCB ������ ��Ÿ��
 ****************************************************************/
void sdbLRUList::setHotMax(idvSQL *aStatistics, UInt aNewHotMax)
{
    if (aNewHotMax < mHotLength)
    {
        moveMidPtrForward( aStatistics, mHotLength - aNewHotMax);
    }
    mHotMax = aNewHotMax;
}

/* LRUList�� dump�Ѵ�.*/
void sdbLRUList::dump()
{
    smuList *sListNode;

    UInt     sHotFixCnt[10];
    UInt     sHotTouchCnt[10];

    UInt     sColdFixCnt[10];
    UInt     sColdTouchCnt[10];

    UInt    i;
    sdbBCB   *sBCB;

    idlOS::memset( sHotFixCnt, 0x00, ID_SIZEOF(UInt) * 10 );
    idlOS::memset( sHotTouchCnt, 0x00, ID_SIZEOF(UInt) * 10 );
    idlOS::memset( sColdFixCnt, 0x00, ID_SIZEOF(UInt) * 10 );
    idlOS::memset( sColdTouchCnt, 0x00, ID_SIZEOF(UInt) * 10 );

    IDE_ASSERT(mMutex.lock(NULL) == IDE_SUCCESS);

    sListNode = SMU_LIST_GET_NEXT(mBase);
    for( i = 1 ; i < getHotLength(); i++)
    {
        sBCB = (sdbBCB*)sListNode->mData;

        if( sBCB->mFixCnt >= 10 )
        {
            sHotFixCnt[9]++;
        }
        else
        {
            sHotFixCnt[ sBCB->mFixCnt ]++;
        }
        if( sBCB->mTouchCnt >= 10)
        {
            sHotTouchCnt[9]++;
        }
        else
        {
            sHotTouchCnt[ sBCB->mTouchCnt]++;
        }
        sListNode = SMU_LIST_GET_NEXT(sListNode);
    }

    for( i = 0 ; i < getColdLength(); i++)
    {
        sBCB = (sdbBCB*)sListNode->mData;

        if( sBCB->mFixCnt >= 10 )
        {
            sColdFixCnt[9]++;
        }
        else
        {
            sColdFixCnt[ sBCB->mFixCnt ]++;
        }
        if( sBCB->mTouchCnt >= 10)
        {
            sColdTouchCnt[9]++;
        }
        else
        {
            sColdTouchCnt[ sBCB->mTouchCnt]++;
        }
        sListNode = SMU_LIST_GET_NEXT(sListNode);
    }

    idlOS::printf("   hotfix\t hottouch\t coldfix\t coldtouch\n");
    for( i = 0 ; i < 10 ; i++ )
    {
        idlOS::printf("[%d] ",i);
        idlOS::printf( "%d\t\t", sHotFixCnt[i]);
        idlOS::printf( "%d\t\t", sHotTouchCnt[i]);
        idlOS::printf( "%d\t\t", sColdFixCnt[i]);
        idlOS::printf( "%d\n", sColdTouchCnt[i]);
    }

    IDE_ASSERT(mMutex.unlock() == IDE_SUCCESS);

}


/***********************************************************************
 * Description :
 *  sdbLRUList�� ��� BCB���� ����� ����Ǿ� �ִ��� Ȯ���Ѵ�.
 *  
 *  aStatistics - [IN]  �������
 ***********************************************************************/
IDE_RC sdbLRUList::checkValidation(idvSQL *aStatistics)
{
    smuList *sPrevNode;
    smuList *sListNode;
    sdbBCB  *sBCB;
    UInt     i;

    IDE_ASSERT(mMutex.lock(aStatistics) == IDE_SUCCESS);

    /* hot ���� Ȯ��*/
    sListNode = mBase;
    sPrevNode = SMU_LIST_GET_PREV( sListNode );
    for( i = 0 ; i < mHotLength; i++)
    {
        IDE_ASSERT( sPrevNode == SMU_LIST_GET_PREV( sListNode ));
        sPrevNode = sListNode;
        sListNode = SMU_LIST_GET_NEXT( sListNode);
        sBCB = (sdbBCB*)(sListNode->mData);
        IDE_ASSERT( sBCB->mBCBListType == SDB_BCB_LRU_HOT);
    }

    //hotLast�� mMid �� ����.
    IDE_ASSERT(sListNode == mMid);

    if( mHotLength != 0 )
    {
        sBCB = (sdbBCB*)(sListNode->mData);
        IDE_ASSERT( sBCB->mBCBListType == SDB_BCB_LRU_HOT);
    }
    else
    {
        IDE_ASSERT( mMid == mBase);
    }

    /* cold ���� Ȯ��*/
    sListNode = mMid;
    sPrevNode = SMU_LIST_GET_PREV( sListNode );
    for( i = 0 ; i < mColdLength; i++)
    {
        IDE_ASSERT( sPrevNode == SMU_LIST_GET_PREV( sListNode ));
        sPrevNode = sListNode;
        sListNode = SMU_LIST_GET_NEXT( sListNode);
        sBCB = (sdbBCB*)(sListNode->mData);
        IDE_ASSERT( sBCB->mBCBListType == SDB_BCB_LRU_COLD);
    }
    IDE_ASSERT(sListNode == SMU_LIST_GET_PREV( mBase));

    IDE_ASSERT(mMutex.unlock() == IDE_SUCCESS);

    return IDE_SUCCESS;
}
