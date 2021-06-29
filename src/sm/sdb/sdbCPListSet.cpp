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

/************************************************************************
 * Description :
 *    ����Ǯ���� ����ϴ� checkpoint list set�� ������ �����̴�.
 *    checkpoint list�� BCB�� dirty�Ǵ� ������ LRU list�� �������
 *    �޸��� ����Ʈ�̴�.
 *    �׷� checkpoint list�� �ϳ��� set���� ���� �����Ǵµ�
 *    �ٸ� ����Ʈ����� �޸� �� Ŭ��������
 *    ���� ����Ʈ�� �����Ǵ°� Ư¡�̴�.
 *    �̴� checkpoint list �� �߿� ���� ���� recovery LSN��
 *    �ܺο��� ���ϱ� ������ �ϱ� ���ؼ��̴�.
 *
 *    + transaction thread�鿡 ���� setDirty�� add()�� ȣ��ȴ�.
 *    + flusher�� ���� flush�� minBCB(), nextBCB(), remove()�� ȣ��ȴ�.
 *    + checkpoint thread�� ���� checkpoint�� getRecoveryLSN()�� ȣ��ȴ�.
 *
 *    checkpoint flush�� ������ ���� �������� ����ؾ� �Ѵ�.
 *
 *    sdbCPListSet::minBCB(&sBCB);
 *    while (sBCB != NULL)
 *    {
 *        if (sBCB satisfies some flush condition)
 *        {
 *            flush(sBCB);
 *            sdbCPListSet::remove(&sBCB);
 *            sdbCPListSet::minBCB(&sBCB);
 *        }
 *        else
 *        {
 *            sdbCPListSet::nextBCB(&sBCB);
 *        }
 *    }
 *
 * Implementation :
 *    �� checkpoint list�� �ϳ��� mutex�� ���� �����ȴ�.
 *    �� mutex�� add()�� remove()���� ���ȴ�.
 ************************************************************************/

#include <sdbCPListSet.h>
#include <sdbReq.h>
#include <smErrorCode.h>


/************************************************************************
 * Description:
 *  �ʱ�ȭ
 * aListCount   - [IN] ����Ʈ ����
 * aType        - [IN] BufferMgr or SecondaryBufferMgr ���� ȣ��Ǿ�����.
 ************************************************************************/
IDE_RC sdbCPListSet::initialize( UInt aListCount, sdLayerState aType )
{
    UInt  i;
    SChar sMutexName[128];
    UInt  sState = 0;

    mListCount = aListCount;

    /* TC/FIT/Limit/sm/sdb/sdbCPListSet_initialize_malloc1.sql */
    IDU_FIT_POINT_RAISE( "sdbCPListSet::initialize::malloc1",
                          insufficient_memory );

    IDE_TEST_RAISE(iduMemMgr::malloc(IDU_MEM_SM_SDB,
                                     (ULong)ID_SIZEOF(smuList) * mListCount,
                                     (void**)&mBase) != IDE_SUCCESS,
                   insufficient_memory );
    sState = 1;

    /* TC/FIT/Limit/sm/sdb/sdbCPListSet_initialize_malloc2.sql */
    IDU_FIT_POINT_RAISE( "sdbCPListSet::initialize::malloc2",
                          insufficient_memory );

    IDE_TEST_RAISE( iduMemMgr::malloc(IDU_MEM_SM_SDB,
                                      (ULong)ID_SIZEOF(UInt) * mListCount,
                                      (void**)&mListLength)!= IDE_SUCCESS,
                    insufficient_memory );
    sState = 2;

    /* TC/FIT/Limit/sm/sdb/sdbCPListSet_initialize_malloc3.sql */
    IDU_FIT_POINT_RAISE( "sdbCPListSet::initialize::malloc3",
                          insufficient_memory );

    IDE_TEST_RAISE(iduMemMgr::malloc(IDU_MEM_SM_SDB,
                                     (ULong)ID_SIZEOF(iduMutex) * mListCount,
                                     (void**)&mListMutex) != IDE_SUCCESS,
                   insufficient_memory );
    sState = 3;

    mLayerType = aType; /* BUG-47945 cp list ����� ���� ���� */
    if( aType == SD_LAYER_BUFFER_POOL )
    {
        for (i = 0; i < mListCount; i++)
        {
            idlOS::memset(sMutexName, 0, 128);
            idlOS::snprintf(
                    sMutexName, 
                    128, 
                    "CHECKPOINT_LIST_MUTEX_%"ID_UINT32_FMT, i + 1);

            IDE_ASSERT(mListMutex[i].initialize(
                        sMutexName,
                        IDU_MUTEX_KIND_NATIVE,
                        IDV_WAIT_INDEX_LATCH_FREE_DRDB_CHECKPOINT_LIST)
                    == IDE_SUCCESS);

            SMU_LIST_INIT_BASE(&mBase[i]);
            mListLength[i] = 0;
        }
    }
    else
    {
        for (i = 0; i < mListCount; i++)
        {
            idlOS::memset(sMutexName, 0, 128);
            idlOS::snprintf(
                 sMutexName, 
                 ID_SIZEOF(sMutexName), 
                 "SECONDARY_BUFFER_CHECKPOINT_LIST_MUTEX_%"ID_UINT32_FMT, i + 1);

            IDE_ASSERT(mListMutex[i].initialize(
                 sMutexName,
                 IDU_MUTEX_KIND_POSIX,
                 IDV_WAIT_INDEX_LATCH_FREE_DRDB_SECONDARY_BUFFER_CHECKPOINT_LIST)
                    == IDE_SUCCESS);

            SMU_LIST_INIT_BASE(&mBase[i]);
            mListLength[i] = 0;
        }
    }

    // �� �Ŀ� IDE_TEST ������ ����Ϸ��� mListMutex�� destroyó����
    // exception �ڵ忡�� ����� �Ѵ�.

    return IDE_SUCCESS;

    IDE_EXCEPTION( insufficient_memory );
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_InsufficientMemory));
    }
    IDE_EXCEPTION_END;

    switch (sState)
    {
        case 3:
            IDE_ASSERT(iduMemMgr::free(mListMutex) == IDE_SUCCESS);
            mListMutex = NULL;
        case 2:
            IDE_ASSERT(iduMemMgr::free(mListLength) == IDE_SUCCESS);
            mListLength = NULL;
        case 1:
            IDE_ASSERT(iduMemMgr::free(mBase) == IDE_SUCCESS);
            mBase = NULL;
        default:
            break;
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description :
 *  �Ҹ���
 ***********************************************************************/
IDE_RC sdbCPListSet::destroy()
{
    UInt i;

    for( i = 0; i < mListCount; i++ )
    {
        IDE_ASSERT( mListMutex[i].destroy() == IDE_SUCCESS );
    }

    IDE_ASSERT(iduMemMgr::free(mListMutex) == IDE_SUCCESS);
    mListMutex = NULL;

    IDE_ASSERT(iduMemMgr::free(mListLength) == IDE_SUCCESS);
    mListLength = NULL;

    IDE_ASSERT(iduMemMgr::free(mBase) == IDE_SUCCESS);
    mBase = NULL;

    return IDE_SUCCESS;
}

/************************************************************************
 * Description :
 *    checkpoint list�� BCB�� recoveryLSN ������ �����Ͽ� �߰��Ѵ�.
 *    �⺻������ BCB�� mPageID�� ���� checkpoint list no�� �������µ�,
 *    �ش� list�� mutex�� ��������� ���� list�� �ְ� �ȴ�.
 *    ���� list ������ŭ �õ��ص� mutex ��µ� �����ϸ� ó���� �����ߴ�
 *    list���� mutex lock�� ��û�ϰ� ����ϰ� �ȴ�.
 *
 * Implementation :
 *    ���� 3�ܰ踦 ���� add�� �Ѵ�.
 *    1. ������ list�� �����Ѵ�. list mutex ȹ�濡 �����ϸ� �ش� list��
 *       �ְԵȴ�.
 *    2. list�� ã������ ������ ��ġ�� ã�´�. tail���� ��ȸ�Ͽ�
 *       �ڽ��� recoveryLSN���� ���� BCB�� ���������� mPrev�� �����Ѵ�.
 *    3. ��ġ�� ã������ aBCB�� ����Ʈ�� ���� �ִ´�.
 *
 *  aStatistics - [IN]  �������
 *  aBCB        - [IN]  BCB
 ************************************************************************/
void sdbCPListSet::add( idvSQL * aStatistics, void * aBCB )
{
    UInt       sTryCount = 0;;
    UInt       sListNo  = ID_UINT_MAX;
    smuList  * sNode    = NULL;
    smuList  * sPrvNode = NULL;
    smuList  * sCurNode = NULL;
    smuList  * sNxtNode = NULL;
    sdBCB    * sBCB     = NULL;
    smLSN      sLSNOfAddBCB;
    idBool     isSucceed;

    IDE_DASSERT( aBCB != NULL );

    if ( mLayerType == SD_LAYER_BUFFER_POOL )
    {
        /* BUG-47945 cp list ����� ���� ���� */
        IDE_ERROR( sdbBufferPool::isFixedBCB( (sdbBCB*)aBCB) == ID_TRUE );
    }
    SM_GET_LSN( sLSNOfAddBCB, SD_GET_BCB_RECOVERYLSN( aBCB ) );

    sListNo   = SD_GET_BCB_PAGEID( aBCB ) % mListCount;

    // [1 �ܰ�: list�鿡 ���� mutex�� ������ list�� ã�´�.]
    // list�鿡 ���� lock�� �õ��Ѵ�.
    while (1)
    {
        if (sTryCount == mListCount)
        {
            // trylock�� mListCount��ŭ �õ�������
            // lock()���� ��⸦ �Ѵ�.
            IDE_ASSERT(mListMutex[sListNo].lock(aStatistics)
                       == IDE_SUCCESS);
            break;
        }
        // �Ϲ������� trylock�� ȣ���Ͽ�
        // lock ȹ�濡 �����ϸ� ���� list�� �ű��.
        IDE_ASSERT(mListMutex[sListNo].trylock(isSucceed)
                   == IDE_SUCCESS);
        sTryCount++;
        
        if (isSucceed == ID_TRUE)
        {
            break;
        }
        // ���� try�� list no�� ���Ѵ�.
        // �ѹ��� ���� �ٽ� 0���� try�Ѵ�.
        if (sListNo == mListCount - 1)
        {
            sListNo = 0;
        }
        else
        {
            sListNo++;
        }
    }

    // [2 �ܰ�: list�� tail���� recoveryLSN�� �˻��Ͽ� �� ��ġ�� ã�´�.]
    // �������� ��ȸ�Ͽ� aBCB�� recoveryLSN����
    // ���ų� ���� ù��° BCB�� ã�´�.
    sNode = SMU_LIST_GET_LAST(&mBase[sListNo]);

    /* BUG-47945 cp list ����� ���� ����,
     * CP List ���ῡ�� ������ �Ǵ� ��Ȳ�� �߻��Ͽ�
     * list ����� �ܰ躰�� Ȯ���մϴ�. */
    IDE_ERROR_MSG( sNode > (void*)512,
                   "sNode : %"ID_xPOINTER_FMT"\n",sNode );

    IDE_ERROR_MSG( sNode->mNext == &mBase[sListNo],
                   "sNode->mNext : %"ID_xPOINTER_FMT" != "
                   "mBase[%"ID_UINT32_FMT"] : %"ID_xPOINTER_FMT"\n",
                   sNode->mNext,
                   sListNo,
                   &mBase[sListNo] );

    while (sNode != &mBase[sListNo])
    {
        sBCB = (sdBCB*)sNode->mData;

        IDE_ERROR_MSG( sListNo == SD_GET_BCB_CPLISTNO( sBCB ),
                       "sListNo %"ID_UINT32_FMT" != "
                       "SD_GET_BCB_CPLISTNO( sBCB ) %"ID_UINT32_FMT"\n",
                       sListNo,
                       SD_GET_BCB_CPLISTNO( sBCB ) );

        if ( smLayerCallback::isLSNGTE( &sLSNOfAddBCB, 
                                        &(sBCB->mRecoveryLSN) )
             == ID_TRUE )
        {
            break;
        }

        sPrvNode = SMU_LIST_GET_PREV( sNode );
        IDE_ERROR_MSG( sPrvNode > (void*)512,
                       "sPrvNode : %"ID_xPOINTER_FMT"\n",sPrvNode );

        IDE_ERROR_MSG( sPrvNode->mNext == sNode,
                       "sPrvNode->mNext %"ID_xPOINTER_FMT" != "
                       "sNode %"ID_xPOINTER_FMT"\n",
                       sPrvNode->mNext, sNode );

        sNxtNode = sNode;
        sNode = sPrvNode;
    }

    IDE_ERROR_MSG( sNode > (void*)512,
                   "sNode : %"ID_xPOINTER_FMT"\n",sNode );

    sNxtNode = sNode->mNext;

    IDE_ERROR_MSG( sNxtNode > (void*)512,
                   "sNode : %"ID_xPOINTER_FMT"\n",sNxtNode );

    IDE_ERROR_MSG( sNxtNode->mPrev == sNode,
                   "sNxtNode->mPrev %"ID_xPOINTER_FMT" != "
                   "sNode %"ID_xPOINTER_FMT"\n",
                   sNxtNode->mPrev, sNode );

    // [3 �ܰ�: ã�� ��ġ�� BCB�� �ִ´�.]
    sCurNode = SD_GET_BCB_CPLISTITEM( aBCB );
    sCurNode->mPrev = sNode;
    sCurNode->mNext = sNode->mNext;
    sNxtNode->mPrev = sCurNode;
    sNode->mNext = sCurNode;

    SD_GET_BCB_CPLISTNO( aBCB ) = sListNo;
    mListLength[sListNo] += 1;

    IDE_ASSERT(mListMutex[sListNo].unlock() == IDE_SUCCESS);

    return;

    IDE_EXCEPTION_END;

    ideLog::log( IDE_ERR_0,
                 "sdbCPListSet::add()\n"
                 "Layer   : %"ID_UINT32_FMT" (0: Buffer Pool, 1: Sedondary Buffer)\n"
                 "ListNo  : %"ID_UINT32_FMT"\n"
                 "ListCnt : %"ID_UINT32_FMT"\n"
                 "sNode   : %"ID_xPOINTER_FMT"\n"
                 "sPrvNode: %"ID_xPOINTER_FMT"\n"
                 "sNxtNode: %"ID_xPOINTER_FMT"\n"
                 "sCurNode: %"ID_xPOINTER_FMT"\n",
                 mLayerType,
                 sListNo,
                 mListCount,
                 sNode,
                 sPrvNode,
                 sNxtNode,
                 sCurNode );

    if ( sListNo < mListCount )
    {
        ideLog::log( IDE_ERR_0,
                     "mBase[%"ID_UINT32_FMT"] : %"ID_xPOINTER_FMT"\n",
                     sListNo,
                     &mBase[sListNo] );
    }

    IDE_ASSERT( mLayerType == SD_LAYER_BUFFER_POOL );

    sdbBCB::dump( aBCB );

    if ( sBCB != NULL )
    {
        sdbBCB::dump( sBCB );; 
    }

    if ( sNode > (void*)512 )
    {
        sBCB = (sdBCB*)sNode->mData;

        ideLog::log( IDE_ERR_0,
                     "sNode->mData: %"ID_xPOINTER_FMT"\n",
                     sBCB );

        if ( sBCB > (void*)512 )
        {
            sdbBCB::dump( sBCB );;
        }
    }

    if ( sNxtNode > (void*)512 )
    {
        sBCB = (sdBCB*)sNxtNode->mData;

        ideLog::log( IDE_ERR_0,
                     "sNxtNode->mData: %"ID_xPOINTER_FMT"\n",
                     sBCB );
        if ( sBCB > (void*)512 )
        {
            sdbBCB::dump( sBCB );;
        }
    }

    if ( sPrvNode > (void*)512 )
    {
        sBCB = (sdBCB*)sPrvNode->mData;

        ideLog::log( IDE_ERR_0,
                     "sPrvNode->mData: %"ID_xPOINTER_FMT"\n",
                     sBCB );
        if ( sBCB > (void*)512 )
        {
            sdbBCB::dump( sBCB );;
        }
    }

    if ( sCurNode > (void*)512 )
    {
        sBCB = (sdBCB*)sCurNode->mData;

        ideLog::log( IDE_ERR_0,
                     "sCurNode->mData: %"ID_xPOINTER_FMT"\n",
                     sBCB );
        if ( sBCB > (void*)512 )
        {
            sdbBCB::dump( sBCB );;
        }
    }

    IDE_ASSERT(0);

    return;
}

/************************************************************************
 * Description :
 *    checkpoint list�� �߿� ���� ���� recovery LSN�� ���� BCB�� ã�´�.
 *    ã�� BCB�� checkpoint list���� ���� �ʴ´�.
 *    ���� ã�� BCB�� checkpoint list�� ���� mutex�� ���� �ʱ� ������
 *    ��ȯ�� ������ checkpoint list�� �޷��ִ���, �ּ��� recovery LSN��
 *    �´����� ������ �� ����.
 *    �� �Լ��� ȣ���� ������ ����� ���� BCB�� ���� stateMutex�� ���� ��
 *    state�� Ȯ���Ͽ� dirty������ �˻��ؾ� �Ѵ�.
 *    �ּ� recovery LSN�� �ƴϾ checkpoint flush�� ������ ���� �ʴ´�.
 *
 * Implementation :
 *    checkpoint list�� ��ȸ�ϸ鼭 ���� �տ� �ִ� BCB���� recovery LSN��
 *    ���Ͽ� ���� ���� LSN�� ���� BCB�� ��ȯ�Ѵ�.
 *    �� ����Ʈ�� BCB�� ������ �� mutex�� ���� �ʴ´�.
 *    ���� �ٸ� �����忡 ���� ���ÿ� �� �Լ��� �Ҹ��ٸ�
 *    ���� BCB�� ��ȯ�ް� �� ���̴�.
 *    ������ flush�� �� BCB�� BCBMutex�� ȹ�������ν�,
 *    ���� BCB�� �� flusher�� ���� ���ÿ� flush�Ǵ� ���� ���� ���̴�.
 ***********************************************************************/
sdBCB* sdbCPListSet::getMin()
{
    UInt      i;
    smLSN     sMinLSN;
    smuList * sNode;
    sdBCB   * sFirstBCB;
    sdBCB   * sRet;

    // checkpoint list�� ��� ��������� NULL�� �����ϰ� �ȴ�.
    sRet = NULL;

    // sMinLSN�� ���� �ִ밪���� �ʱ�ȭ�Ѵ�.
    SM_LSN_MAX(sMinLSN);

    for (i = 0; i < mListCount; i++)
    {
        sNode = SMU_LIST_GET_FIRST(&mBase[i]);
        if (sNode == &mBase[i])
        {
            // checkpoint list�� ��� �ְų� skip list�̸�
            // skip�Ѵ�.
            continue;
        }

        sFirstBCB = (sdBCB*)sNode->mData;
        if ( smLayerCallback::isLSNLT( &(sFirstBCB->mRecoveryLSN), &sMinLSN ) )
        {
            SM_GET_LSN(sMinLSN, sFirstBCB->mRecoveryLSN);
            sRet = sFirstBCB;
        }
    }
    return sRet;
}

/***********************************************************************
 * Description :
 *    �Է¹��� BCB�� ���� list ������ list�� ù��° BCB�� ��ȯ�Ѵ�.
 *    ���� list�� BCB�� ������ �� ���� list�� BCB�� ��ȯ�Ѵ�.
 *    ��� list�� ��������� NULL�� ��ȯ�Ѵ�.
 *    aCurrBCB�� minBCB()�� ���ؼ� ���� BCB�̱� ������
 *    �� �Լ��� �ҷ��� ������ aCurrBCB�� list���� ������ ���� �ִ�.
 *    �� ��쿣 list�� minBCB�� ��ȯ�Ѵ�.
 *
 * Implementation :
 *    �־��� mListNo + 1���� mListNo - 1���� ��ȯ�ϸ鼭
 *    ù��° BCB�� NULL�� �ƴϸ� ��ȯ�Ѵ�.
 *    
 * aCurrBCB - [IN]  �� BCB�� ���� list�� ���� list�߿�
 *                  empty�� �ƴ� list�� ù���� BCB�� ã�´�.
 *                  ��ȯ�Ǵ� BCB�� ã�� BCB�� ��ȯ�ȴ�.
 *                  ��ã���� NULL�� ��ȯ�ȴ�.
 ***********************************************************************/
sdBCB* sdbCPListSet::getNextOf( void * aCurrBCB )
{
    UInt      i;
    UInt      sValidListNo;
    UInt      sBeforeListNo;
    sdBCB   * sRet     = NULL;
    
    IDE_DASSERT( aCurrBCB != NULL );

    sBeforeListNo = SD_GET_CPLIST_NO( aCurrBCB );

    if( sBeforeListNo == SDB_CP_LIST_NONE )
    {
        // ID_UINT_MAX�� ��쿣 ����Ʈ���� �����ִ� ���
        // aCurrBCB�� �̹� list���� �����ִ�. 
        // �� ��� list�� minBCB�� ��ȯ�Ѵ�.
        sRet = getMin();
    }
    else
    {
        sRet = NULL;

        // aListNo���� 3�̰� ����Ʈ ������ 5���
        // ������ 4, 5, 6, 7�� ��ȸ�ϰ�
        // list ID�� 4, 0, 1, 2�� ��ȸ�Ѵ�.
        for (i = sBeforeListNo + 1; i < sBeforeListNo + mListCount; i++)
        {
            if (i >= mListCount)
            {
                sValidListNo = i - mListCount;
            }
            else
            {
                sValidListNo = i;
            }

            if (!SMU_LIST_IS_EMPTY(&mBase[sValidListNo]))
            {
                sRet = (sdBCB*)SMU_LIST_GET_FIRST(&mBase[sValidListNo])->mData;
                break;
            }
        }
    }

    return sRet;
}

/***********************************************************************
 * Description :
 *  checkpoint ����Ʈ�� LRU, Flush, prepare List�� �ٸ��� ���ؽ��� �ϳ��ۿ�
 *  ���� ���� �ʴ´�.  �ֳĸ�, checkpoint����Ʈ�� ���԰� ������ ����Ʈ ����
 *  ����������� �Ͼ �� �ֱ� �����̴�.
 * 
 *    checkpoint list���� aBCB�� �����Ѵ�.
 *    aBCB���� � checkpoint list�� �ڽ��� �޷��ִ���
 *    list no�� �ִ�. �� list no�� �����Ͽ� list mutex�� ���� ��
 *    aBCB�� ����Ʈ���� �����Ѵ�.
 *
 * Implementation :
 *    aBCB�� mCPListItem ������ checkpoint list mutex(mListMutex)��
 *    ���� ��ȣ�Ǿ�� �Ѵ�. ���� mCPListItem�� ������ ����,
 *    �����ϱ� ���ؼ��� mListMutex�� ��ƾ� �ϴµ�, mListMutex�� 
 *    ������ؼ��� mCPListITem.mListNo�� �����ؾ� �Ѵ�.
 *    mCPListItem.mListNo ������ mutex���� �о�� �ϱ� ������
 *    dirty read�̴�. �� ���� �а� mutex�� ���� �� �ٽ� mListNo��
 *    �˻��ؾ� �Ѵ�.
 *    
 *  aStatistics - [IN]  �������
 *  aBCB        - [IN]  BCB
 ***********************************************************************/
void sdbCPListSet::remove( idvSQL * aStatistics, void * aBCB )
{
    UInt      sListNo;

    IDE_DASSERT( aBCB != NULL );
    IDE_DASSERT( SD_GET_CPLIST_NO( aBCB ) != ID_UINT_MAX );
    // BCB�� ���� list no�� ���´�.
    // list�� ���� mutex�� ���� ���� �����̱� ������
    // dirty read�̴�.
    sListNo  = SD_GET_CPLIST_NO( aBCB );

    // list�� ���� mutex�� ��´�.
    // BCB�� mCPListItem�� �����ϱ� ���ؼ��� �ݵ��
    // list mutex�� ��ƾ� �Ѵ�.
    IDE_ASSERT( mListMutex[sListNo].lock( aStatistics ) == IDE_SUCCESS );

    // list mutex�� ���� �� �ٽ� list no�� �˻��Ѵ�.
    // �� ���� �ٸ��ٸ� �� BCB�� ���ؼ� �� �����尡 remove��
    // ����̴�. flush �˰��� �� �̷� ���� �߻��� �� ����.
    IDE_ASSERT( sListNo == SD_GET_CPLIST_NO( aBCB ) );

    SMU_LIST_DELETE( SD_GET_BCB_CPLISTITEM( aBCB ) );
    mListLength[sListNo] -= 1;

    SDB_INIT_CP_LIST( aBCB );
    IDE_ASSERT( mListMutex[sListNo].unlock() == IDE_SUCCESS );
}

/***********************************************************************
 * Description :
 *  ���� checkpoint list���� BCB�� recoveryLSN���� ���� ���� recoveryLSN��
 *  �����Ѵ�.
 *  
 *  aStatistics - [IN]  �������
 *  aMinLSN     - [OUT] ���� ���� recoveryLSN
 ***********************************************************************/
void sdbCPListSet::getMinRecoveryLSN( idvSQL *aStatistics, smLSN *aMinLSN )
{
    UInt       i;
    smuList  * sNode;
    sdBCB    * sFirstBCB;

    IDE_DASSERT(aMinLSN != NULL);

    // checkpoint list�� BCB�� �ϳ��� ������ MAX���� ���ϵȴ�.
    SM_LSN_MAX( *aMinLSN );

    for (i = 0; i < mListCount; i++)
    {
        // recoveryLSN�� �б� ���ؼ��� list mutex�� ��ƾ� �Ѵ�.
        // BCB�� LSN�� ������ ���� BCB�� LSN�� ����Ʈ�� �����鼭 LSN����
        // �ʱ�ȭ �� �� �ֱ� ������, lock�� �ɾ ����� �� BCB�� LSN����
        // ���´�.
        IDE_ASSERT(mListMutex[i].lock(aStatistics) == IDE_SUCCESS);

        sNode = SMU_LIST_GET_FIRST(&mBase[i]);

        if (sNode == &mBase[i])
        {
            // checkpoint list�� ��� �ְų� skip list�̸�
            // skip�Ѵ�.
            IDE_ASSERT(mListMutex[i].unlock() == IDE_SUCCESS);
            continue;
        }

        sFirstBCB = (sdBCB*)sNode->mData;

        if ( smLayerCallback::isLSNLT( &sFirstBCB->mRecoveryLSN, aMinLSN )
             == ID_TRUE )
        {
            SM_GET_LSN(*aMinLSN, sFirstBCB->mRecoveryLSN);
        }

        IDE_ASSERT(mListMutex[i].unlock() == IDE_SUCCESS);
    }
}

/***********************************************************************
 * Description :
 *  ���� checkpoint list���� BCB�� recoveryLSN���� ���� ū recoveryLSN��
 *  �����Ѵ�.
 *  
 *  aStatistics - [IN]  �������
 *  aMaxLSN     - [OUT] ���� ū recoveryLSN
 ***********************************************************************/
void sdbCPListSet::getMaxRecoveryLSN(idvSQL *aStatistics, smLSN *aMaxLSN)
{
    UInt       i;
    smuList  * sNode;
    sdBCB    * sLastBCB;

    IDE_DASSERT(aMaxLSN != NULL);

    SM_LSN_INIT(*aMaxLSN);

    for (i = 0; i < mListCount; i++)
    {
        // recoveryLSN�� �б� ���ؼ��� list mutex�� ��ƾ� �Ѵ�.
        IDE_ASSERT(mListMutex[i].lock(aStatistics) == IDE_SUCCESS);

        sNode = SMU_LIST_GET_LAST(&mBase[i]);
        if (sNode == &mBase[i])
        {
            IDE_ASSERT(mListMutex[i].unlock() == IDE_SUCCESS);
            continue;
        }

        sLastBCB = (sdBCB*)sNode->mData;
        if ( smLayerCallback::isLSNLT( aMaxLSN, &sLastBCB->mRecoveryLSN )
             == ID_TRUE )
        {
            SM_GET_LSN(*aMaxLSN, sLastBCB->mRecoveryLSN);
        }
        IDE_ASSERT(mListMutex[i].unlock() == IDE_SUCCESS);
    }
}
 
/***********************************************************************
 * Description :
 *  ���� checkpoint list�� �����ϴ� ��� BCB���� ������ �����Ѵ�.
 ***********************************************************************/
UInt sdbCPListSet::getTotalBCBCnt()
{
    UInt i;
    UInt sSum = 0;

    for( i = 0 ; i < mListCount ; i++)
    {
        sSum += mListLength[i];
    }

    return sSum;
}

