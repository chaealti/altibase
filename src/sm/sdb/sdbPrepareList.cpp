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
 * Abstraction : ����Ǯ���� ����ϴ� prepare list�� ������ �����̴�.
 *               prepare list�� victim�� ������ ã������ flush list����
 *               flush�Ǿ clean�̰ų� free�� BCB���� �����ϴ� list�̴�.
 *               ������ buffer miss�� victim ����� ã�� ��
 *               prepare list�� ������� ������.
 *               prepare list�� ���� �� clean���������� �̴� ������
 *               �������� �ʴ´�. ����Ǯ ��å�� BCB�� �������Ʈ�� �ְ�
 *               hit�� �� dirty �Ǵ� inIOB ���·� �� �� �ֱ� �����̴�.
 *               �׷���, �̵��� ���°� ���ϴ� ��� �������� �ʰ�,
 *               victim�� ã������ prepare List�� Ž���ϴ� Ʈ����ǵ���
 *               �ű�� �۾��� �����Ѵ�.
 *
 ***********************************************************************/
#include <sdbPrepareList.h>
#include <smErrorCode.h>

/***********************************************************************
 * Description :
 *  aListID     - [IN]  prepare list �ĺ���
 ***********************************************************************/
IDE_RC sdbPrepareList::initialize(UInt aListID)
{
    SChar sMutexName[128];

    mBase               = &mBaseObj;
    mID                 = aListID;
    mListLength		= 0;
    mWaitingClientCount = 0;
    SMU_LIST_INIT_BASE(mBase);

    idlOS::memset(sMutexName, 0, 128);
    idlOS::snprintf(sMutexName, 128, "PREPARE_LIST_MUTEX_%"ID_UINT32_FMT, aListID);

    IDE_TEST(mMutex.initialize(sMutexName,
                               IDU_MUTEX_KIND_NATIVE,
                               IDV_WAIT_INDEX_LATCH_FREE_DRDB_PREPARE_LIST)
             != IDE_SUCCESS);

    idlOS::memset(sMutexName, 0, 128);
    idlOS::snprintf(sMutexName, 128, "PREPARE_LIST_MUTEX_FOR_WAIT_%"ID_UINT32_FMT, aListID);

    IDE_TEST(mMutexForWait.initialize(sMutexName,
                                      IDU_MUTEX_KIND_POSIX,
                                      IDV_WAIT_INDEX_LATCH_FREE_DRDB_PREPARE_LIST_WAIT)
             != IDE_SUCCESS);

    // condition variable �ʱ�ȭ
    idlOS::snprintf(sMutexName, 
                    ID_SIZEOF(sMutexName),
                    "PREPARE_LIST_COND_%"ID_UINT32_FMT, 
                    aListID);

    IDE_TEST_RAISE(mCondVar.initialize(sMutexName) != IDE_SUCCESS,
                   err_cond_var_init);

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_cond_var_init);
    {
        IDE_SET(ideSetErrorCode(smERR_FATAL_ThrCondInit));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description :
 *  ���� �Լ�
 ***********************************************************************/
IDE_RC sdbPrepareList::destroy()
{
    IDE_ASSERT(mMutex.destroy() == IDE_SUCCESS);

    IDE_ASSERT(mMutexForWait.destroy() == IDE_SUCCESS);

    IDE_TEST_RAISE(mCondVar.destroy() != 0, err_cond_destroy);

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_cond_destroy);
    {
        IDE_SET(ideSetErrorCode(smERR_FATAL_ThrCondDestroy));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Abstraction :
 *    prepare list���� last�� �ϳ� ���´�.
 *    ��ȯ�Ǵ� BCB�� next, prev�� ��� NULL�̴�.
 *    ���ü��� ���������� ����ȴ�.
 *
 * Implementation :
 *  ó���� mutex�� �����ʰ� empty���� Ȯ���� ����.
 *  prepare list�� empty�� ��찡 ����. �׸���, Ʈ����ǵ��� ���� ���� prepare
 *  list�� ������ �ϱ� ������, ������ ���� �Ͼ �� �ִ�. �׷��Ƿ� empty�Ǵ�
 *  ������ mutex�� ���� �ʰ� �غ���.
 *
 *  aStatistics - [IN]  �������
 ***********************************************************************/
sdbBCB* sdbPrepareList::removeLast( idvSQL *aStatistics )
{
    sdbBCB  *sRet               = NULL;
    smuList *sTarget;

    if ( mListLength == 0 )
    {
        sRet = NULL;
    }
    else
    {
        IDE_ASSERT(mMutex.lock(aStatistics) == IDE_SUCCESS);

        if (mListLength == 0)
        {
            // list�� BCB�� �ϳ��� ����.
            IDE_ASSERT( SMU_LIST_IS_EMPTY(mBase) == ID_TRUE );
            sRet = NULL;
        }
        else
        {
            // list�� �ּ��� �ϳ� �̻��� BCB�� �ִ�.
            sTarget = SMU_LIST_GET_LAST(mBase);


	    SMU_LIST_DELETE(sTarget);


            sRet = (sdbBCB*)sTarget->mData;
            SDB_INIT_BCB_LIST(sRet);

            IDE_ASSERT( mListLength != 0 );
            mListLength--;
        }

        IDE_ASSERT(mMutex.unlock() == IDE_SUCCESS);
    }

    IDE_DASSERT(checkValidation(aStatistics) == IDE_SUCCESS);

    return sRet;
}

/***********************************************************************
 * Description:
 *    flusher�� prepare list�� BCB�� �߰��� ������ ���Ǵ�⸦ �Ѵ�.
 *    �� �Լ��� prepare list, LRU list���� victim�� ã�ٰ� ������
 *    prepare list�� ���� ��⸦ �ϰ����� �� ���ȴ�.
 *    timeover�� ��� ��쿣 aBCBAdded�� false�̰� BCB�� �߰��Ǿ�
 *    ��� ��쿣 �� �Ķ���Ͱ� true�� �ȴ�.
 *
 *  aStatistics - [IN]  �������
 *  aTimeSec    - [IN]  ���� ��⸦ ���ʵ��� �� ���ΰ��� ����
 *  aBCBAdded   - [OUT] BCB�� �߰��� ��� true, timeover�α�� ���
 *                      false�� ���ϵȴ�.
 ***********************************************************************/
IDE_RC sdbPrepareList::timedWaitUntilBCBAdded(idvSQL *aStatistics,
                                              UInt    aTimeMilliSec,
                                              idBool *aBCBAdded)
{
    PDL_Time_Value sTimeValue;

    *aBCBAdded = ID_FALSE;
    sTimeValue.set(idlOS::time(NULL) + (SDouble)aTimeMilliSec / 1000);

    IDE_ASSERT(mMutexForWait.lock(aStatistics) == IDE_SUCCESS);
    mWaitingClientCount++;

    IDE_TEST_RAISE(mCondVar.timedwait(&mMutexForWait, &sTimeValue,
                                      IDU_IGNORE_TIMEDOUT)
                   != IDE_SUCCESS, err_cond_wait);

    // cond_broadcast�� �ް� ��� ���
    *aBCBAdded = ID_TRUE;
    mWaitingClientCount--;

    IDE_ASSERT(mMutexForWait.unlock() == IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_cond_wait);
    {
        IDE_SET(ideSetErrorCode(smERR_FATAL_ThrCondWait));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Abstraction :
 *    prepare list�� first�� BCB ����Ʈ�� �߰��Ѵ�.
 *    �׸��� �� prepare list�� ������� �����尡 ������ �����.
 *    
 *  aStatistics - [IN]  �������
 *  aFirstBCB   - [IN]  �߰��� BCB list�� ù��° BCB. �� BCB�� prev�� NULL�̾��
 *                      �Ѵ�.
 *  aLastBCB    - [IN]  �߰��� BCB list�� ������ BCB. �� BCB�� next��
 *                      NULL�̾�� �Ѵ�.
 *  aLength     - [IN]  �߰��Ǵ� BCB list�� ����.
 ***********************************************************************/
IDE_RC sdbPrepareList::addBCBList( idvSQL *aStatistics,
                                   sdbBCB *aFirstBCB,
                                   sdbBCB *aLastBCB,
                                   UInt    aLength )
{
    UInt     i;
    smuList *sNode;

    IDE_DASSERT(aFirstBCB != NULL);
    IDE_DASSERT(aLastBCB != NULL);

    // �ԷµǴ� BCB�� ����Ʈ ������ �����Ѵ�.
    for (i = 0, sNode = &aFirstBCB->mBCBListItem; i < aLength; i++)
    {
        ((sdbBCB*)sNode->mData)->mBCBListType = SDB_BCB_PREPARE_LIST;
        ((sdbBCB*)sNode->mData)->mBCBListNo   = mID;
        sNode = SMU_LIST_GET_NEXT(sNode);
    }
    IDE_DASSERT(sNode == NULL);

    IDE_ASSERT(mMutex.lock(aStatistics) == IDE_SUCCESS);

    mListLength += aLength;

    SMU_LIST_ADD_LIST_FIRST( mBase,
                             &aFirstBCB->mBCBListItem,
                             &aLastBCB->mBCBListItem );

    IDE_ASSERT(mMutex.unlock() == IDE_SUCCESS);

    // prepare list�� ����ϰ� �ִ� �����尡 ������ �����.
    if (mWaitingClientCount > 0)
    {
        // mWaitingClientCount�� �ø��� ��ó cond_wait���� ���� �����带 ����
        // mMutexForWait�� ȹ���� �� broadcast�Ѵ�.
        IDE_ASSERT(mMutexForWait.lock(aStatistics) == IDE_SUCCESS);

        IDE_TEST_RAISE(mCondVar.broadcast() != IDE_SUCCESS,
                       err_cond_signal);

        IDE_ASSERT(mMutexForWait.unlock() == IDE_SUCCESS);
    }

    IDE_DASSERT(checkValidation(aStatistics) == IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_cond_signal);
    {
        IDE_SET(ideSetErrorCode(smERR_FATAL_ThrCondSignal));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description :
 *  sdbPrepareList�� ��� BCB���� ����� ����Ǿ� �ִ��� Ȯ���Ѵ�.
 *  
 *  aStatistics - [IN]  �������
 ***********************************************************************/
IDE_RC sdbPrepareList::checkValidation(idvSQL *aStatistics)
{
    smuList *sPrevNode;
    smuList *sListNode;
    UInt     i;

    IDE_ASSERT(mMutex.lock(aStatistics) == IDE_SUCCESS);

    sListNode = SMU_LIST_GET_FIRST(mBase);
    sPrevNode = SMU_LIST_GET_PREV( sListNode );
    for( i = 0; i < mListLength; i++)
    {
        IDE_ASSERT( sPrevNode == SMU_LIST_GET_PREV( sListNode ));
        sPrevNode = sListNode;
        sListNode = SMU_LIST_GET_NEXT( sListNode);
    }
    
    IDE_ASSERT(sListNode == mBase);

    IDE_ASSERT(mMutex.unlock() == IDE_SUCCESS);

    return IDE_SUCCESS;
}
