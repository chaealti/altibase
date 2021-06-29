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
 * Description :
 *    ����Ǯ���� ����ϴ� flush list�� ������ �����̴�.
 *    flush list�� LRU list���� dirty�� BCB���� victim�� ã�� ��������
 *    �Ű����� list�̴�.
 *
 ***********************************************************************/


#include <sdbFlushList.h>

/***********************************************************************
 * Description :
 *  flushList �ʱ�ȭ
 *
 *  aListID     - [IN]  flushList �ĺ���  
 ***********************************************************************/
IDE_RC sdbFlushList::initialize( UInt aListID, sdbBCBListType aListType )
{
    SChar sMutexName[128];

    mID            = aListID;
    mBase          = &mBaseObj;
    mExplorerCount = 0;
    mCurrent       = NULL;
    mListLength    = 0;

    /* PROJ-2669 */
    mListType       = aListType;
    mMaxListLength  = 0;

    SMU_LIST_INIT_BASE(mBase);

    idlOS::memset(sMutexName, 0, 128);
    idlOS::snprintf(sMutexName, 128, "FLUSH_LIST_MUTEX_%"ID_UINT32_FMT, aListID);

    IDE_TEST(mMutex.initialize(sMutexName,
			       IDU_MUTEX_KIND_NATIVE,
			       IDV_WAIT_INDEX_LATCH_FREE_DRDB_FLUSH_LIST)
             != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC sdbFlushList::destroy()
{

    IDE_TEST(mMutex.destroy() != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description :
 *    flush list�� ���� mCurrent���� Ž���� ��û�Ѵ�. 
 *    flush list�� BCB�� �������ؼ��� �� �Լ��� ȣ���� ����Ǿ�� �Ѵ�.
 *    ���������� mExploringCount�� �ϳ� ������Ų��.
 *    �� ���� 1�̻��̶�� ������ Ž���ϰ� �ִٴ� �ǹ��̴�.
 *    Ž���� ������ �� endExploring()�� ȣ���ؾ� �Ѵ�.
 *    
 *  aStatistics - [IN]  �������
 ***********************************************************************/
void sdbFlushList::beginExploring(idvSQL *aStatistics)
{
    IDE_ASSERT(mMutex.lock(aStatistics) == IDE_SUCCESS);

    IDE_DASSERT(mExplorerCount >= 0);

    mExplorerCount++;
    if (mExplorerCount == 1)
    {
        // �� flush list�� ���� beginExploring()�� �ƹ��� ȣ�����
        // �����̴�. �� ��� mExploring�� �����Ѵ�.
        mCurrent = SMU_LIST_GET_LAST(mBase);
    }
    else
    {
        // ������ exploring�� �ϰ� �ִ�.
        // mExploring�� �������� �ʴ´�.
    }

    IDE_ASSERT(mMutex.unlock() == IDE_SUCCESS);
}

/***********************************************************************
 * Description :
 *    flush list�� ���� Ž���� �������� �˸���.
 *    beginExploring ȣ���ߴٸ�, �ݵ�� endExploring�� ȣ���ؾ� �Ѵ�.
 *    mExploringCount�� �ϳ� ���ҽ�Ų��.
 *
 *  aStatistics - [IN]  �������
 ***********************************************************************/
void sdbFlushList::endExploring(idvSQL *aStatistics)
{
    IDE_ASSERT(mMutex.lock(aStatistics) == IDE_SUCCESS);

    // endExploring()�� ȣ��Ǳ� ���� �ݵ�� �ּ��� �ѹ���
    // beginExploring()�� ȣ��Ǿ�� �Ѵ�.
    IDE_DASSERT(mExplorerCount >= 1);

    mExplorerCount--;
    if (mExplorerCount == 0)
    {
        // ���̻� �ƹ��� exploring���� �ʴ� �����̴�.
        mCurrent = NULL;
    }
    else
    {
        // Nothing to do
    }

    IDE_ASSERT(mMutex.unlock() == IDE_SUCCESS);
}

/***********************************************************************
 * Description :
 *    ���� Ž�� ��ġ�� BCB�� ���´�. BCB�� ����Ʈ���� ���� �ʴ´�.
 *    �׸��� Ž�� ��ġ�� ������ �ϳ� ��� ��ġ��Ų��.
 *    ���� Ž�� ��ġ�� mFirst�̸� NULL�� ��ȯ�Ѵ�.
 *    ����Ʈ�� BCB�� �ϳ��� ���� ���� NULL�� ��ȯ�Ѵ�.
 *    �� �Լ��� ȣ���ϱ� ���ؼ��� ���� beginExploring()�� ȣ���ؾ� �Ѵ�.
 *
 *    ���� getNext�� �����Ͽ� ���� BCB�� ���ؼ� remove�� �����ؼ� �����ϰ�
 *    �Ǵµ�, ��� BCB�� ���� remove�� �������� �ʰ�, �׳� flushList�� ����
 *    ���� ��쵵 �ִ�. �̶��� remove�� ȣ������ �ʰ�, �ٽ� getNext�� ȣ���Ѵ�.
 *    
 *  aStatistics - [IN]  �������
 ***********************************************************************/
sdbBCB* sdbFlushList::getNext(idvSQL *aStatistics)
{
    sdbBCB *sRet = NULL;

    IDE_ASSERT(mMutex.lock(aStatistics) == IDE_SUCCESS);

    IDE_DASSERT(mExplorerCount > 0);

    if (mCurrent == mBase)
    {
        // sRet�� NULL�� ��� mListLength�� 0�� ������ �ʴ´�.
        sRet = NULL;
    }
    else
    {
        sRet = (sdbBCB*)mCurrent->mData;
        mCurrent = SMU_LIST_GET_PREV(mCurrent);
    }

    IDE_ASSERT(mMutex.unlock() == IDE_SUCCESS);

    return sRet;
}

/***********************************************************************
 * Description :
 *    ����Ʈ���� �ش� BCB�� �����Ѵ�.
 *    ���ڷ� �־����� BCB�� �ݵ�� next()�� ����
 *    BCB�̾�� �Ѵ�.
 *
 *  aStatistics - [IN]  �������
 *  aTargetBCB  - [IN]  �ش� BCB
 ***********************************************************************/
void sdbFlushList::remove(idvSQL *aStatistics, sdbBCB *aTargetBCB)
{
    smuList *sTarget;

    IDE_ASSERT(aTargetBCB != NULL);

    sTarget = &aTargetBCB->mBCBListItem;

    // mCurrent�� �׻� getNext���� prevBCB�� ����Ű�� �ֱ⶧����,
    // ���� ���� ���� �ʴ�.
    IDE_ASSERT(sTarget != mCurrent);

    IDE_ASSERT(mMutex.lock(aStatistics) == IDE_SUCCESS);

    IDE_ASSERT(mExplorerCount > 0);

    SMU_LIST_DELETE(sTarget);
    SDB_INIT_BCB_LIST(aTargetBCB);

    IDE_ASSERT( mListLength != 0 );
    mListLength--;

    IDE_ASSERT(mMutex.unlock() == IDE_SUCCESS);

    IDE_DASSERT(checkValidation(aStatistics) == IDE_SUCCESS);
}

/***********************************************************************
 * Description :
 *    flush list�� �� �տ� BCB�� �ϳ� �߰��Ѵ�.
 *    
 *  aStatistics - [IN]  �������
 *  aBCB        - [IN]  �ش� BCB
 ***********************************************************************/
void sdbFlushList::add(idvSQL *aStatistics, sdbBCB *aBCB)
{
    IDE_ASSERT(aBCB != NULL);

    IDE_ASSERT(mMutex.lock(aStatistics) == IDE_SUCCESS);

    mListLength++;

    SMU_LIST_ADD_FIRST(mBase, &aBCB->mBCBListItem);

    aBCB->mBCBListType = mListType;
    aBCB->mBCBListNo = mID;

    IDE_ASSERT(mMutex.unlock() == IDE_SUCCESS);

    IDE_DASSERT(checkValidation(aStatistics) == IDE_SUCCESS);
}

/***********************************************************************
 * Description :
 *  sdbFlushList�� ��� BCB���� ����� ����Ǿ� �ִ��� Ȯ���Ѵ�.
 *  
 *  aStatistics - [IN]  �������
 ***********************************************************************/
IDE_RC sdbFlushList::checkValidation(idvSQL *aStatistics)
{
    smuList *sPrevNode;
    smuList *sListNode;
    UInt     i;

    IDE_ASSERT(mMutex.lock(aStatistics) == IDE_SUCCESS);
    
    sListNode = SMU_LIST_GET_FIRST(mBase);
    sPrevNode = SMU_LIST_GET_PREV( sListNode );
    for( i = 0 ; i < mListLength; i++)
    {
        IDE_ASSERT( sPrevNode == SMU_LIST_GET_PREV( sListNode ));
        sPrevNode = sListNode;
        sListNode = SMU_LIST_GET_NEXT( sListNode);
    }
    
    IDE_ASSERT(sListNode == mBase);


    IDE_ASSERT(mMutex.unlock() == IDE_SUCCESS);

    return IDE_SUCCESS;
}

