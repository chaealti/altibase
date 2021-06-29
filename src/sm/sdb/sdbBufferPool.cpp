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
/******************************************************************************
 * PROJ-1568 BUFFER MANAGER RENEWAL
 ******************************************************************************/
#include <smDef.h>
#include <smcDef.h>
#include <smcTable.h>
#include <smErrorCode.h>
#include <sddDef.h>
#include <sddDiskMgr.h>
#include <sdbDef.h>
#include <sdbReq.h>
#include <sdbFlushList.h>
#include <sdbLRUList.h>
#include <sdbPrepareList.h>
#include <sdbCPListSet.h>
#include <sdbBufferPool.h>
#include <sdbBCB.h>
#include <sdbFlushMgr.h>
#include <sdp.h>
#include <sdsFile.h>
#include <sdsBufferArea.h>
#include <sdsMeta.h>
#include <sdsBufferMgr.h> 

IDE_RC sdbBufferPool::initPrepareList()
{
    UInt    i = 0, j = 0;
    UInt    sState = 0;

    /* TC/FIT/Limit/sm/sdb/sdbBufferPool_initPrepareList_malloc.sql */
    IDU_FIT_POINT_RAISE( "sdbBufferPool::initPrepareList::malloc", 
                          insufficient_memory );

    IDE_TEST_RAISE( iduMemMgr::malloc( IDU_MEM_SM_SDB,
                                       (ULong)ID_SIZEOF( sdbPrepareList ) * mPrepareListCnt,
                                       (void**)&mPrepareList ) != IDE_SUCCESS,
                    insufficient_memory );
    sState = 1;

    for( i = 0; i < mPrepareListCnt; i++ )
    {
        IDE_TEST(mPrepareList[i].initialize(i)
                 != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( insufficient_memory );
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_InsufficientMemory));
    }
    IDE_EXCEPTION_END;

    if ( sState == 1 )
    {
        for( j = 0 ; j < i; j++)
        {
            IDE_ASSERT( mPrepareList[i].destroy() == IDE_SUCCESS );
        }

        IDE_ASSERT( iduMemMgr::free( mPrepareList )
                    == IDE_SUCCESS );

        mPrepareList = NULL;
    }

    return IDE_FAILURE;
}

IDE_RC sdbBufferPool::initLRUList()
{
    UInt i = 0, j = 0;
    UInt sState = 0;
    UInt sHotMax;

    /* TC/FIT/Limit/sm/sdb/sdbBufferPool_initLRUList_malloc.sql */
    IDU_FIT_POINT_RAISE( "sdbBufferPool::initLRUList::malloc", 
                          insufficient_memory );

    IDE_TEST_RAISE( iduMemMgr::malloc( IDU_MEM_SM_SDB,
                                       (ULong)ID_SIZEOF( sdbLRUList ) * mLRUListCnt,
                                       (void**)&mLRUList) != IDE_SUCCESS,
                    insufficient_memory );
    sState = 1;

    sHotMax = getHotCnt4LstFromPCT(smuProperty::getHotListPct());

    for( i = 0 ; i < mLRUListCnt; i++)
    {
        IDE_TEST(mLRUList[i].initialize(i, sHotMax, &mStatistics)
                 != IDE_SUCCESS );
    }


    return IDE_SUCCESS;

    IDE_EXCEPTION( insufficient_memory );
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_InsufficientMemory));
    }
    IDE_EXCEPTION_END;

    if( sState == 1 )
    {
        for( j = 0 ; j < i; j++)
        {
            IDE_ASSERT( mLRUList[i].destroy() == IDE_SUCCESS );
        }

        IDE_ASSERT( iduMemMgr::free(mLRUList )
                    == IDE_SUCCESS );
        mLRUList = NULL;
    }

    return IDE_FAILURE;
}

IDE_RC sdbBufferPool::initFlushList()
{
    UInt i = 0, j = 0;
    UInt sState = 0;

    /* TC/FIT/Limit/sm/sdb/sdbBufferPool_initFlushList_malloc.sql */
    IDU_FIT_POINT_RAISE( "sdbBufferPool::initFlushList::malloc", 
                          insufficient_memory );

    IDE_TEST_RAISE( iduMemMgr::malloc( IDU_MEM_SM_SDB,
                                       (ULong)ID_SIZEOF( sdbFlushList ) * mFlushListCnt,
                                       (void**)&mFlushList ) != IDE_SUCCESS,
                    insufficient_memory );

    sState = 1;

    for( i = 0 ; i < mFlushListCnt; i++)
    {
        IDE_TEST( mFlushList[i].initialize( i, SDB_BCB_FLUSH_LIST )
                  != IDE_SUCCESS );
    }


    sState = 2;

    /* TC/FIT/Limit/sm/sdb/sdbBufferPool_initFlushList_delayedFlushList_malloc.sql */
    IDU_FIT_POINT_RAISE( "sdbBufferPool::initFlushList::delayedFlushList_malloc", 
                          insufficient_memory );
    /* PROJ-2669
     * Delayed flush list ����
     * Normal Flush list �� ������ ������ �����Ѵ� */
    IDE_TEST_RAISE( iduMemMgr::malloc( IDU_MEM_SM_SDB,
                                       (ULong)ID_SIZEOF( sdbFlushList ) * mFlushListCnt,
                                       (void**)&mDelayedFlushList ) != IDE_SUCCESS,
                    insufficient_memory );

    sState = 3;

    for ( i = 0 ; i < mFlushListCnt; i++ )
    {
        IDE_TEST( mDelayedFlushList[i].initialize( i, SDB_BCB_DELAYED_FLUSH_LIST )
                  != IDE_SUCCESS );
    }

    sState = 4;

    setMaxDelayedFlushListPct( smuProperty::getDelayedFlushListPct() );

    return IDE_SUCCESS;

    IDE_EXCEPTION( insufficient_memory );
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_InsufficientMemory));
    }
    IDE_EXCEPTION_END;

    switch ( sState )
    {
    case 4:
        i = mFlushListCnt;
    case 3:
        for ( j = 0 ; j < i; j++ )
        {
            IDE_ASSERT( mDelayedFlushList[j].destroy() == IDE_SUCCESS );
        }
        IDE_ASSERT( iduMemMgr::free( mDelayedFlushList ) == IDE_SUCCESS );
        mDelayedFlushList = NULL;

    case 2:
        i = mFlushListCnt;
    case 1:
        for ( j = 0 ; j < i; j++ )
        {
            IDE_ASSERT( mFlushList[j].destroy() == IDE_SUCCESS );
        }

        IDE_ASSERT( iduMemMgr::free( mFlushList )
                    == IDE_SUCCESS );
        mFlushList = NULL;

    default:
        break;
    }

    return IDE_FAILURE;
}

/****************************************************************
 * Description :
 *
 * aBCBCnt        - [IN]  buffer pool�� ���ϰԵ� �� BCB����.
 * aBucketCnt     - [IN]  buffer pool���� ����ϴ� hash table�� bucket
 *                      ����.. sdbBCBHash.cpp ����
 * aBucketCntPerLatch  - [IN]  hash latch�ϳ��� Ŀ���ϴ� bucket�� ����..
 *                            sdbBCBHash.cpp����
 * aLRUListCnt    - [IN]  ���ο��� contention�� ���̱� ���� ��������
 *                      ����Ʈ�� �����Ѵ�.  sdbLRUList�� ����
 * aPrepareListCnt- [IN]  sdbPrepareList�� ����.
 * aFlushListCnt  - [IN]  sdbFlushList�� ����.
 * aCPListCnt     - [IN]  checkpoint list�� ����.
 *
 ****************************************************************/
IDE_RC sdbBufferPool::initialize( UInt aBCBCnt,
                                  UInt aBucketCnt,
                                  UInt aBucketCntPerLatch,
                                  UInt aLRUListCnt,
                                  UInt aPrepareListCnt,
                                  UInt aFlushListCnt,
                                  UInt aCPListCnt )
{
    UInt sState = 0;

    IDE_ASSERT( aLRUListCnt > 0);
    IDE_ASSERT( aPrepareListCnt > 0);
    IDE_ASSERT( aFlushListCnt > 0);
    IDE_ASSERT( aCPListCnt > 0);
    IDE_ASSERT( aBCBCnt > 0 );

    mPageSize = SD_PAGE_SIZE;
    mBCBCnt   = aBCBCnt;
    mHotTouchCnt = smuProperty::getHotTouchCnt();
     
    /* Secondary Buffer�� identify ������ �����ǹǷ� ������ Unserviceable ���·� ���� */
    setSBufferServiceState( ID_FALSE ); 

    mLRUListCnt = aLRUListCnt;
    IDE_TEST( initLRUList () != IDE_SUCCESS );
    sState = 1;

    mFlushListCnt = aFlushListCnt;
    IDE_TEST( initFlushList() != IDE_SUCCESS );
    sState = 2;

    mPrepareListCnt = aPrepareListCnt;
    IDE_TEST( initPrepareList() != IDE_SUCCESS );
    sState = 3;

    IDE_TEST( mCPListSet.initialize( aCPListCnt, SD_LAYER_BUFFER_POOL )
              != IDE_SUCCESS );
    sState = 4;

    IDE_TEST( mHashTable.initialize( aBucketCnt,
                                     aBucketCntPerLatch,
                                     SD_LAYER_BUFFER_POOL )
              != IDE_SUCCESS );
    sState = 5;

    setLRUSearchCnt( smuProperty::getBufferVictimSearchPct());

    setInvalidateWaitTime( SDB_BUFFER_POOL_INVALIDATE_WAIT_UTIME );

    IDE_TEST( mStatistics.initialize(this) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch (sState)
    {
        case 5:
            IDE_ASSERT( mHashTable.destroy() == IDE_SUCCESS );
        case 4:
            IDE_ASSERT( mCPListSet.destroy() == IDE_SUCCESS );
        case 3:
            destroyPrepareList();
        case 2:
            destroyFlushList();
        case 1:
            destroyLRUList();
        default:
            break;
    }

    return IDE_FAILURE;
}

void sdbBufferPool::destroyPrepareList()
{
    UInt i;

    for( i = 0 ; i < mPrepareListCnt; i++ )
    {
        IDE_ASSERT( mPrepareList[i].destroy() == IDE_SUCCESS );
    }

    IDE_ASSERT( iduMemMgr::free( mPrepareList ) == IDE_SUCCESS );
    mPrepareList = NULL;

    return;
}

void sdbBufferPool::destroyLRUList()
{
    UInt i;

    for( i = 0 ; i < mLRUListCnt; i++)
    {
        IDE_ASSERT( mLRUList[i].destroy() == IDE_SUCCESS );
    }

    IDE_ASSERT( iduMemMgr::free( mLRUList ) == IDE_SUCCESS );
    mLRUList = NULL;

    return;
}

void sdbBufferPool::destroyFlushList()
{
    UInt i;

    for ( i = 0 ; i < mFlushListCnt; i++ )
    {
        IDE_ASSERT( mDelayedFlushList[i].destroy() == IDE_SUCCESS );
    }
    IDE_ASSERT( iduMemMgr::free( mDelayedFlushList ) == IDE_SUCCESS );
    mDelayedFlushList = NULL;

    for ( i = 0 ; i < mFlushListCnt; i++ )
    {
        IDE_ASSERT( mFlushList[i].destroy() == IDE_SUCCESS );
    }

    IDE_ASSERT( iduMemMgr::free( mFlushList ) == IDE_SUCCESS );
    mFlushList = NULL;

    return;
}

IDE_RC sdbBufferPool::destroy()
{
    IDE_ASSERT( mHashTable.destroy() == IDE_SUCCESS );

    IDE_ASSERT( mCPListSet.destroy() == IDE_SUCCESS );

    destroyPrepareList();
    destroyLRUList();
    destroyFlushList();

    IDE_ASSERT( mStatistics.destroy() == IDE_SUCCESS);

    return IDE_SUCCESS;
}

/****************************************************************
 * Description :
 *  buffer pool�� ���� LRUList�� ���� ���̰� ���� list�� �����Ѵ�.
 ****************************************************************/
sdbLRUList *sdbBufferPool::getMinLengthLRUList()
{
    UInt         i;
    sdbLRUList  *sMinList = &mLRUList[0];

    for (i = 1; i < mLRUListCnt; i++)
    {
        if (sMinList->getColdLength() > mLRUList[i].getColdLength())
        {
            sMinList = &mLRUList[i];
        }
    }

    return sMinList;
}

/****************************************************************
 * Description :
 *  buffer pool�� ���� prepare List�� ���� ���̰� ���� prepare list��
 *  �ݳ��Ѵ�.
 ****************************************************************/
sdbPrepareList  *sdbBufferPool::getMinLengthPrepareList(void)
{
    UInt            i;
    sdbPrepareList *sMinList = &mPrepareList[0];

    for( i = 1 ; i < mPrepareListCnt; i++)
    {
        if( sMinList->getLength() > mPrepareList[i].getLength())
        {
            sMinList = &mPrepareList[i];
        }
    }

    return sMinList;
}

/****************************************************************
 * Description :
 *  buffer pool�� ���� flush List�� ���� ���̰� �� flush list��
 *  �����Ѵ�.
 ****************************************************************/
sdbFlushList  *sdbBufferPool::getMaxLengthFlushList(void)
{
    UInt          i;
    sdbFlushList *sMaxList = &mFlushList[0];

    for( i = 1 ; i < mFlushListCnt; i++ )
    {
        if ( getFlushListLength( sMaxList->getID() )
             < getFlushListLength( i ) )
        {
            sMaxList = &mFlushList[i];
        }
        else
        {
            /* Nothing to do */
        }
    }

    return sMaxList;
}

/****************************************************************
 * Description :
 *   PROJ-2669
 *     ��� Normal/Delayed flush list �� ������ ������ ��ȯ�Ѵ�.
 ****************************************************************/
UInt sdbBufferPool::getFlushListTotalLength(void)
{
    UInt          i;
    UInt          sTotalLength = 0;

    for ( i = 0 ; i < mFlushListCnt; i++ )
    {
        sTotalLength += mFlushList[i].getPartialLength();
        sTotalLength += mDelayedFlushList[i].getPartialLength();
    }

    return sTotalLength;
}

/****************************************************************
 * Description :
 *   PROJ-2669
 *     ��� Normal flush list �� ������ ������ ��ȯ�Ѵ�.
 ****************************************************************/
UInt sdbBufferPool::getFlushListNormalLength(void)
{
    UInt          i;
    UInt          sTotalLength = 0;

    for ( i = 0 ; i < mFlushListCnt; i++ )
    {
        sTotalLength += mFlushList[i].getPartialLength();
    }

    return sTotalLength;
}

/****************************************************************
 * Description :
 *   PROJ-2669
 *     ��� Delayed flush list �� ���̸� ��ȯ�Ѵ�.
 ****************************************************************/
UInt sdbBufferPool::getDelayedFlushListLength(void)
{
    UInt          sTotalLength = 0;
    UInt          i;

    for ( i = 0 ; i < mFlushListCnt; i++ )
    {
        sTotalLength += mDelayedFlushList[i].getPartialLength();
    }

    return sTotalLength;
}

/****************************************************************
 * Description :
 *   PROJ-2669
 *     ��� Normal/Delayed flush list �� ������ ���� ��ȯ�Ѵ�.
 ****************************************************************/
UInt sdbBufferPool::getFlushListLength( UInt aID )
{
    UInt          sTotalLength = 0;

    sTotalLength += mFlushList[ aID ].getPartialLength();
    sTotalLength += mDelayedFlushList[ aID ].getPartialLength();

    return sTotalLength;
}

/****************************************************************
 * Description :
 *   PROJ-2669
 *     Delayed flush list �� �ִ� ���̸� �����Ѵ�.
 ****************************************************************/
void sdbBufferPool::setMaxDelayedFlushListPct( UInt aMaxListPct )
{
    UInt sDelayedFlushMax;
    UInt i;

    sDelayedFlushMax = getDelayedFlushCnt4LstFromPCT( aMaxListPct );

    for ( i = 0 ; i < mFlushListCnt; i++ )
    {
        mDelayedFlushList[ i ].setMaxListLength( sDelayedFlushMax );
    }
}

/****************************************************************
 * Description :
 *   PROJ-2669
 *     Delayed flush list �� ��ȯ�Ѵ�.
 ****************************************************************/
sdbFlushList *sdbBufferPool::getDelayedFlushList( UInt aIndex )
{
    IDE_DASSERT( aIndex < mFlushListCnt );
    return &mDelayedFlushList[ aIndex ];
}

/****************************************************************
 * Description :
 *  buffer pool�� prepare list�� free������ bcb���� ������ �� ����Ѵ�.
 *  ���� ���̰� ª�� prepareList�� �����Ѵ�.
 *  �ϳ��� BCB�� �����ϰ��� �ϴ� ��쿣 aFirstBCB�� aLastBCB�� ����
 *  BCB�� �ϰ�, aLength�� 1�� �Ѵ�.
 *
 * aStatistics  - [IN]  �������
 * aFirstBCB    - [IN]  �����Ϸ��� BCB����Ʈ�� ù BCB
 * aLastBCB     - [IN]  �����Ϸ��� BCB����Ʈ�� ������ BCB
 * aLength      - [IN]  �����ϰ��� �ϴ� BCB����Ʈ�� ����
 ****************************************************************/
void sdbBufferPool::addBCBList2PrepareLst( idvSQL    *aStatistics,
                                           sdbBCB    *aFirstBCB,
                                           sdbBCB    *aLastBCB,
                                           UInt       aLength )
{
    sdbPrepareList  *sMinList;

    IDE_DASSERT( aLength > 0 );

    sMinList = getMinLengthPrepareList();

    sMinList->addBCBList( aStatistics,
                          aFirstBCB,
                          aLastBCB,
                          aLength);
    return;
}


/****************************************************************
 * Description :
 *  aFirstBCB ���� ����� ��� BCB ����Ʈ�� ����Ǯ�� prepare list��
 *  ���� �����Ѵ�.
 *
 *  aStatistics - [IN]  �������
 *  aFirstBCB   - [IN]  BCB����Ʈ�� ���� BCB
 *  aLastBCB    - [IN]  BCB����Ʈ�� ������ BCB
 *  aLength     - [IN]  BCB����Ʈ�� �� ����
 ****************************************************************/
void sdbBufferPool::distributeBCB2PrepareLsts( idvSQL    *aStatistics,
                                               sdbBCB    *aFirstBCB,
                                               sdbBCB    *aLastBCB,
                                               UInt       aLength )
{
    UInt        i;
    UInt        sBCBCnt;
    UInt        sQuota;
    smuList    *sFirst;
    smuList    *sCurrent;
    smuList    *sNextFirst;

    IDE_DASSERT(aFirstBCB != NULL);

    if( aLength < mPrepareListCnt )
    {
        // ���� 1���� prepareList�� ������ �ؾ� ������,
        // aLength�� ��ô ���� ��쿣 �л꿡 �� �ǹ̰� ����.
        // ( �Դٰ� aLength�� mPrepareListCnt���� ������쿣 �� �Լ���
        //   ���� ȣ������� �ʴ´�. ȣ��ȴٸ�, ������ �߸��� ���̴�.)
        // ������, �װ��� ���� BCB�� �Ź� ���� �����ϴ� ������
        //  ����δٰ� �����ǰ�, �ڵ带 simple�ϰ� ���� ���ؼ�
        // �׳� �� ����Ʈ�� �����Ѵ�.
        mPrepareList[aLength].addBCBList( aStatistics,
                                          aFirstBCB,
                                          aLastBCB,
                                          aLength);
    }
    else
    {
        sQuota = aLength / mPrepareListCnt;

        sFirst   = &aFirstBCB->mBCBListItem;
        sCurrent = sFirst;
        IDE_ASSERT(SMU_LIST_GET_PREV(sFirst) == NULL);

        for( i = 0 ; i < mPrepareListCnt - 1; i++)
        {
            for(sBCBCnt = 1; sBCBCnt < sQuota; sBCBCnt++)
            {
                sCurrent = SMU_LIST_GET_NEXT(sCurrent);
            }

            sNextFirst = SMU_LIST_GET_NEXT( sCurrent );
            SMU_LIST_CUT_BETWEEN( sCurrent, sNextFirst );

            mPrepareList[i].addBCBList( aStatistics,
                                        (sdbBCB*)sFirst->mData,
                                        (sdbBCB*)sCurrent->mData,
                                        sBCBCnt );

            sFirst   = sNextFirst;
            sCurrent = sFirst;
        }

        sBCBCnt = 1;

        for(; SMU_LIST_GET_NEXT( sCurrent ) != NULL;
            sCurrent = SMU_LIST_GET_NEXT( sCurrent ) )
        {
            sBCBCnt++;
        }

        IDE_ASSERT( sBCBCnt == sQuota + ( aLength % mPrepareListCnt ) );
        IDE_ASSERT( (sdbBCB*)sCurrent->mData == aLastBCB );

        mPrepareList[i].addBCBList( aStatistics,
                                    (sdbBCB*)sFirst->mData,
                                    (sdbBCB*)sCurrent->mData,
                                    sBCBCnt );
    }
}

void sdbBufferPool::setHotMax( idvSQL *aStatistics,
                               UInt    aHotMaxPCT )
{
    UInt sHotMax;
    UInt i;

    sHotMax = getHotCnt4LstFromPCT( aHotMaxPCT );

    for( i = 0 ; i < mLRUListCnt; i++ )
    {
        mLRUList[i].setHotMax( aStatistics, sHotMax );
    }
}

/*****************************************************************
 * ��������� BCB�� ������ ���ִ� ��δ� �� 3���� �̴�.
 * 1. list�� ���� = �ַ� victim�� ã�´ٴ���, flush�� �Ҷ�
 * 2. hash�� ���� = �ַ� getPage�Ҷ� hit�� ���
 * 3. BCB�����͸� ���� = �ſ� �پ��� ��찡 ����.
 * �̵鰣�� ���ü� ��� ���� ���캸��.
 *
 * ****************** BufferPool�� ���ü� ���� ********************
 * 1. list vs list =
 *      �� �����尡 BCB�� �� ����Ʈ���� ���Ÿ� �ع����� �ٸ� ������ ������
 *      �� �� �����Ƿ�, ���ü��� ���� ����ȴ�.  �̶� ����Ʈ�� ���ؽ� �ʿ�.
 *
 * 2. hash vs hash =
 *  2.1 hash ���� vs hash ����,
 *      hash ���� vs hash ����,
 *      hash ���� vs hash ����,
 *      hash ���� vs hash Ž��,
 *      hash ���� vs hash Ž��,
 *          �� ��쿡 hash ������ hash chains x ��ġ�� ���, hash Ž�� ������
 *          hash chains s ��ġ�� �䱸�Ѵ�.  ���ü� �����
 *
 *  2.2 hash Ž�� vs hash Ž��
 *          hash Ž���� �Ͽ� �����ϴ� ���갣�� ���ü��� ������ �־�� �Ѵ�.
 *          �̶� �ϴ� ������ fix�� �ϴ� �����ε�, �̰��� ���ü��� �����ϱ�
 *          ���� ������ BCB�� mMutex�� ��� �����Ѵ�.
 *
 * 3. list vs hash =
 *      list�� ���� �����Ͽ� �ϴ� ����� �����ϴ� ����
 *      hash�� ���� �����Ͽ� �ϴ� ����� �����ϴ� ���� ������ �и��Ǿ� �ִ�.
 *      �׷��� ������ �Ѱ��� ���ü��� ��� ���� �ʾƵ� �ȴ�.
 *
 * 4. BCB������ vs( list or hash or BCB������)
 *      BCB �����͸� ���� �����Ͽ� � ������ �����ϴ��� ���ο� ����
 *      ��� mutex�� �ٸ���, �����ϴ� ������ �ٸ���.
 *      �׷��� ������, �� ������ ����Ǵ� ���ǰ� �����ϴ� ����, �׸��� �׵���
 *      ��� ���ؽ��� �� �ľ��ؼ� ���ü��� �����ϰ� �����ؾ� �Ѵ�.
 *
 *      �Ʒ��� ����� ���� BCB�� ��� ������ �ش��ϴ� ���� � ������ ����
 *      ��� �ٲ��, �׸��� � ��ġ�� ������� ��Ÿ����.
 *
 *  4.1 mState
 *      mState�� �����ϴ� ��� ������ ���ü� ��� ���� �⺻������
 *      BCB��mMutex ��� �����Ѵ�.
 *
 *      invalidateBCB   = SDB_BCB_DIRTY -> SDB_BCB_CLEAN
 *      setDirty = SDB_BCB_CLEAN -> SDB_BCB_DIRTY
 *      setDirty = SDB_BCB_INIOB -> SDB_BCB_REDIRTY
 *              setDirty�� ��쿣 ������ x��ġ�� ��� ������,
 *              �׻� fix�� BCB�� ���ؼ��� �����Ѵ�.
 *
 *      writeIOB = SDB_BCB_INIOB    -> SDB_BCB_CLEAN
 *      writeIOB = SDB_BCB_REDIRTY  -> SDB_BCB_DIRTY
 *                 �÷��ſ� ���� �����
 *
 *      makeFree = SDB_BCB_CLEAN -> SDB_BCB_FREE
 *      (�̰�� hash x ��ġ ����=>hash���� ����)
 *
 *  4.2 mPageID, mSpaceID
 *      SDB_BCB_FREE ������ BCB�� getVictim�� ���� SDB_BCB_CLEAN����
 *      ���°� ����ɶ�, mPageID�� mSpaceID�� ���Ѵ�. �̶� BCB�� mMutex
 *      �� ��� �����Ѵ�.
 *
 *  4.3 mFixCnt
 *      fix    =   hash chains (S or X) ��ġ  �� mMutex ��� ����, hash�� ����
 *                  �������� �ʰ�, fix�� �Ҷ��� mMutex�� ��Ƶ� �ȴ�.
 *                  hash chains latch�� ��� ������ hash�� ���� �����ϴ� Ʈ����ǵ�
 *                  ���� ���ü� ���� �����̴�.
 *      unfix  =   mMutex�� ��� ����( fix or unfix���� ���ü�������)
 *
 ***********************************************************************/

/****************************************************************
 * Description :
 *  buffer pool�� prepare list�� ���� �ϳ��� victim�� ���´�.
 * ����! ���Լ� ����������� hash x��ġ�� ��´�. ��, �� �Լ� ����
 * ���� � �ؽ÷�ġ�� ���� �־ �ȵȴ�.
 *
 * aStatistics  - [IN]  �������
 * aListKey     - [IN]  prepare list�� �������� ����ȭ �Ǿ��ִ�.
 *                     ���߿��� �ϳ��� �����ؾ� �ϴµ�, �� �Ķ����
 *                     �� ���ؼ� �ϳ��� ������ ��Ʈ�� ��´�.
 * aResidentBCB -[OUT] victim�� ã�� ��� BCB�� ����, ��ã�� ���
 *                     NULL�� ����, ���ϵǴ� BCB�� hash�� ����Ʈ����
 *                     ���ŵ� free������ BCB�̴�.
 ****************************************************************/
void sdbBufferPool::getVictimFromPrepareList( idvSQL  *aStatistics,
                                              UInt     aListKey,
                                              sdbBCB **aResidentBCB )
{
    sdbPrepareList           *sPrepareList;
    sdbBCB                   *sBCB = NULL;
    sdbHashChainsLatchHandle *sHashChainsHandle = NULL;
    sdsBCB                   *sSBCB;

    /* aListKey�� ���� �ϳ��� prepareList�� ����.. �ٸ� prepare list����
     * ������ ���� �ʴ´�.  */
    sPrepareList = &mPrepareList[getPrepareListIdx( aListKey )];

    while(1)
    {
        /* �ϴ� ����Ʈ���� ���Ÿ� �ϸ� getVictim�� �����ϴ� ���갣��
         * ���ü��� ����ȴ�.*/
        sBCB = sPrepareList->removeLast( aStatistics );

        if( sBCB == NULL )
        {
            break;
        }

        if( sBCB->mState == SDB_BCB_FREE )
        {
            /* BCB�� SDB_BCB_FREE �̰�, ����Ʈ������ ���ŵǾ� �ִٸ�,
             * ������� victim���� ������ �� �ִ�.*/
            break;
        }

        /* �Ʒ��� ���� �� �ϳ��� �������� ���Ѵٸ� victim���� ������ */
        /* fix�̸鼭 isHot�� �ƴ� �� �ִ�.  �ֳĸ�, ���� touchcount �� ������Ű��
         * ���ؼ� 3�ʰ� �ʿ��ϴ�. */
        if( ( isFixedBCB( sBCB ) == ID_FALSE ) &&
            ( isHotBCB  ( sBCB ) == ID_FALSE ) &&
            ( sBCB->mState       == SDB_BCB_CLEAN))
        {
            /* prepareList���� clean �̿��� ���� ������ BCB�� ������ �ִ�.
             * clean�����̸鼭 hot�� �ƴϰ�, fix�Ǿ����� ���� BCB�� �����Ѵ�.
             *
             * �̶�, ������ �˻�� hash ��ġ�� ��� �ؾ� �Ѵ�. �ֳĸ�,
             * �˻絵�� �ٸ� �����忡 ���� fix�� touch�� �� �� �ְ�,
             * hash���� ���� �� ���� �ֱ� �����̴�. hash ��ġ�� ������,
             * fix�� hash���� ���Ű� ���� ������ ������ �� �ִ�.
             *
             * ������ ���ü��� ���̱� ���ؼ� hash ��ġ�� �������
             * ���� �˻縦 �غ���.
             * */

            /* ����ε� �˻縦 ���� hash��ġ�� ��´�. */
            sHashChainsHandle = mHashTable.lockHashChainsXLatch(
                                                        aStatistics,
                                                        sBCB->mSpaceID,
                                                        sBCB->mPageID );


            if( sBCB->isFree() == ID_TRUE )
            {
                /* BCB ����Ʈ�� ���ؼ� make Free �����ϴ� ��� �ۿ� ����.
                 * hash chains mutex�� ��⶧����, BCB�� mMutex�� ���� �ʿ� ����.*/
                mHashTable.unlockHashChainsLatch( sHashChainsHandle );
                sHashChainsHandle = NULL;
                break;
            }
            else
            {
                /* Hot���θ�  BCBMutex�� ���� �ʰ� ���� ������ ��Ȯ����
                 * ���� �� �־� hot�� victim���� ������ �� ������,
                 * ������ ����Ű���� �ʴ´�.*/
                if( ( isFixedBCB(sBCB) == ID_FALSE ) &&
                    ( isHotBCB(sBCB)   == ID_FALSE ) &&
                    ( sBCB->mState     == SDB_BCB_CLEAN ) )
                {
                    mHashTable.removeBCB( sBCB );

                    /* sBCB�� SDB_BCB_CLEAN ���¶�� �ݵ�� hash������*/
                    /* �ؽÿ��� ���ŵǾ��� ������ ���¸� free�� �����Ѵ�.*/
                    sBCB->lockBCBMutex( aStatistics );

                    if (( isFixedBCB(sBCB) == ID_TRUE ) ||
                        ( isHotBCB(sBCB)   == ID_TRUE ) ||
                        ( sBCB->mState     != SDB_BCB_CLEAN ) )
                    {
                        /* BUG-47945 cp list ����� ���� ���� */
                        sBCB->dump();
                        IDE_ASSERT( 0 );
                    }

                    /* ����� SBCB�� �ִٸ� delink */
                    sSBCB = sBCB->mSBCB;
                    if( sSBCB != NULL )
                    {
                        sSBCB->mBCB = NULL;
                        sBCB->mSBCB = NULL;    
                    }

                    sBCB->setToFree();
                    sBCB->unlockBCBMutex();

                    mStatistics.applyVictimPagesFromPrepare( aStatistics,
                                                             sBCB->mSpaceID,
                                                             sBCB->mPageID,
                                                             sBCB->mPageType );
                    mHashTable.unlockHashChainsLatch( sHashChainsHandle );
                    sHashChainsHandle = NULL;

                    break;
                }
                else
                {
                    mHashTable.unlockHashChainsLatch( sHashChainsHandle );
                    sHashChainsHandle = NULL;
                }
            }
        }
        mStatistics.applySkipPagesFromPrepare( aStatistics,
                                               sBCB->mSpaceID,
                                               sBCB->mPageID,
                                               sBCB->mPageType );

        /* �ϴ� prepare�� �� BCB���� touchCnt������ ������� Hot�̶��
         * �Ǵ� ���� �ʴ´�.
         * �׷��� ������ ������ LRU Mid�� �����Ѵ�.
         * */
        mLRUList[ getLRUListIdx(aListKey)].insertBCB2BehindMid( aStatistics,
                                                                sBCB );

    }//while

    *aResidentBCB = sBCB;

    return;
}

/****************************************************************
 * Description :
 *  LRU List�� end���� victim�� Ž���Ͽ�, ���ǿ� �´� BCB�� �����Ѵ�.
 * ����! �� �Լ� ����������� hash x��ġ�� ��´�. ��, �� �Լ� ����
 * ���� � �ؽ� ��ġ�� ���� �־ �ȵȴ�.
 *
 * aStatistics  - [IN]  �������
 * aListKey     - [IN]  LRU list�� �������� ����ȭ �Ǿ��ִ�.
 *                      ���߿��� �ϳ��� �����ؾ� �ϴµ�, �� �Ķ����
 *                      �� ���ؼ� �ϳ��� ������ ��Ʈ�� ��´�.
 * aResidentBCB - [OUT] victim�� ����. victimã�°Ϳ� ������ ���
 *                      NULL�� ������ ���� �ִ�.
 ****************************************************************/
void sdbBufferPool::getVictimFromLRUList( idvSQL  *aStatistics,
                                          UInt     aListKey,
                                          sdbBCB **aResidentBCB )
{
    sdbLRUList               *sLRUList;
    sdbFlushList             *sFlushList;
    sdbBCB                   *sBCB = NULL;
    UInt                      sLoopCnt;
    sdbHashChainsLatchHandle *sHashChainsHandle = NULL;
    UInt                      sColdInsertedID   = ID_UINT_MAX;
    sdsBCB                   *sSBCB;

    /*���� list�� �߿��� aListKey�� ���� �Ѱ��� ����.. ���⿡ ���ؼ���
     *�����Ѵ�.*/
    sLRUList   = &mLRUList[ getLRUListIdx( aListKey ) ];
    sFlushList = &mFlushList[ getFlushListIdx( aListKey ) ];

    sLoopCnt = 0;
    while(1)
    {
        sLoopCnt++; //����Ʈ Ž��Ƚ�� ����
        if( sLoopCnt > mLRUSearchCnt )
        {
            sBCB = NULL;
            break;
        }

        /* �ϴ� ����Ʈ���� ���Ÿ� �ϸ� getVictim�� �����ϴ� ���갣��
         * ���ü��� ����ȴ�.*/
        sBCB = sLRUList->removeColdLast(aStatistics);

        if( sBCB == NULL )
        {
            break;
        }

        // �ڽ��� ���� BCB�� �ٽ� ���ϵǸ� ���̻� �� �ʿ����.
        if (sBCB->mID == sColdInsertedID)
        {
            sLRUList->insertBCB2BehindMid(aStatistics, sBCB);
            sBCB = NULL;
            break;
        }

        mStatistics.applyVictimSearchs();


    retest:
        /* victim���ǿ� �´��� ���θ� �˻� */
        // Hot�� ��� hot ������ ����
        if( isHotBCB(sBCB) == ID_TRUE )
        {
            if( sHashChainsHandle != NULL )
            {
                mHashTable.unlockHashChainsLatch( sHashChainsHandle );
                sHashChainsHandle = NULL;
            }
            sLRUList->addToHot(aStatistics, sBCB);
            mStatistics.applyVictimSearchsToHot();
            mStatistics.applySkipPagesFromLRU( aStatistics,
                                               sBCB->mSpaceID,
                                               sBCB->mPageID,
                                               sBCB->mPageType );
            continue;
        }
        // fix�� ���, ������ �������̶�� �Ǵ��Ͽ� �ٽ� cold first�� ����
        if( isFixedBCB(sBCB) == ID_TRUE )
        {
            if( sHashChainsHandle != NULL)
            {
                mHashTable.unlockHashChainsLatch( sHashChainsHandle );
                sHashChainsHandle = NULL;
            }
            sLRUList->insertBCB2BehindMid(aStatistics, sBCB);
            mStatistics.applyVictimSearchsToCold();
            sColdInsertedID = sBCB->mID;
            mStatistics.applySkipPagesFromLRU( aStatistics,
                                               sBCB->mSpaceID,
                                               sBCB->mPageID,
                                               sBCB->mPageType );
            continue;
        }

        if( sBCB->mState != SDB_BCB_CLEAN )
        {
            if( sHashChainsHandle != NULL )
            {
                mHashTable.unlockHashChainsLatch( sHashChainsHandle );
                sHashChainsHandle = NULL;
            }

            if( sBCB->isFree() == ID_TRUE )
            {
                /* sBCB�� free������ ��쿣 �ٷ� ����� �����ϴ�.
                 * �ֳĸ�, free�����̰�, ����Ʈ�� ���� ����(����Ʈ���� ����)
                 * �����Ƿ�, ���� BCB�� �ٸ� ������ ������ ������ �� �ִ�. */
                break;
            }

            sFlushList->add( aStatistics, sBCB);
            mStatistics.applyVictimSearchsToFlush();
            mStatistics.applySkipPagesFromLRU( aStatistics,
                                               sBCB->mSpaceID,
                                               sBCB->mPageID,
                                               sBCB->mPageType );
            continue;
        }

        if( sHashChainsHandle == NULL)
        {
            /* sBCB�� ���´� hash ��ġ�� ���� �ʴ� �̻� ���� �� �ִ�.
             * �׷��� ������ ����ε� ���¸� ���� �����ϱ� ���ؼ� hash X ��ġ�� ���
             * sBCB�� ���¸� �׽�Ʈ �ؾ� �Ѵ�.
             * ������ ���ü��� ���� hash ��ġ�� ���� �ʰ� ���� �׽�Ʈ�� �غ���.
             */
            sHashChainsHandle = mHashTable.lockHashChainsXLatch( aStatistics,
                                                                 sBCB->mSpaceID,
                                                                 sBCB->mPageID );
            if( sBCB->isFree() == ID_TRUE )
            {
                mHashTable.unlockHashChainsLatch( sHashChainsHandle );
                sHashChainsHandle = NULL;
                break;
            }

            goto retest;
        }

        mHashTable.removeBCB( sBCB );

        /* �ؽÿ��� ���ŵǾ��� ������ ���¸� free�� �����Ѵ�.*/
        sBCB->lockBCBMutex( aStatistics );
        // sBCB�� hash���� ������ ������ BCB�� ���¸� free�� ������ �� �ִ�.
        IDE_DASSERT( sBCB->mState != SDB_BCB_FREE );

        if (( isFixedBCB(sBCB) == ID_TRUE ) ||
            ( isHotBCB(sBCB)   == ID_TRUE ) ||
            ( sBCB->mState     != SDB_BCB_CLEAN ))
        {
            /* BUG-47945 cp list ����� ���� ���� */
            sBCB->dump();
            IDE_ASSERT( 0 );
        }

        /* ����� SBCB�� �ִٸ� delink */
        sSBCB = sBCB->mSBCB;
        if( sSBCB != NULL )
        {
            sSBCB->mBCB = NULL;
            sBCB->mSBCB = NULL;
        }

        sBCB->setToFree();
        sBCB->unlockBCBMutex();

        mStatistics.applyVictimPagesFromLRU( aStatistics,
                                             sBCB->mSpaceID,
                                             sBCB->mPageID,
                                             sBCB->mPageType );
        mHashTable.unlockHashChainsLatch( sHashChainsHandle );
        sHashChainsHandle = NULL;

        break;
    }// while

    *aResidentBCB = sBCB;

    return;
}

/****************************************************************
 * Description :
 * prepare list�Ǵ� LRU list���� free������ BCB�� �����Ѵ�.
 *
 *
 * ����! �� �Լ� ������ hashChainsLatch �� ��� ������ �̷��� ���ؽ���
 * ������ ���� getVictim�Լ��� ȣ���ؾ� �Ѵ�.
 * hashChains x Latch�� ��� ����, SDB_BCB_CLEAN�� BCB�� Hash����
 * �����ϱ� ���ؼ��̴�.
 * �� �Լ� ���� �� victim�� ã�� ���Ѱ�� �������� �ʰ�, ã��������
 * ����Ѵ�.
 * resource dead lock�ΰ�쿡 ������ �����Ѵ�. (�����ȵǾ� ����)
 *
 * aStatistics  - [IN]  �������
 * aListKey     - [IN]  LRU list�� �������� ����ȭ �Ǿ��ִ�.
 *                      ���߿��� �ϳ��� �����ؾ� �ϴµ�, �� �Ķ����
 *                      �� ���ؼ� �ϳ��� ������ ��Ʈ�� ��´�.
 * aReturnedBCB -[OUT]  victim�� ����. ���� NULL�� �������� �ʴ´�.
 ****************************************************************/
IDE_RC sdbBufferPool::getVictim( idvSQL  * aStatistics,
                                 UInt      aListKey,
                                 sdbBCB ** aReturnedBCB )
{
    UInt     sIdx;
    UInt     sTryCount = 0;
    idBool   sBCBAdded;
    sdbBCB  *sVictimBCB = NULL;

    IDE_DASSERT( aReturnedBCB != NULL );

    *aReturnedBCB = NULL;

    /* victim ���� ��å
     * 1. ���� prepareList�� �ϳ� ������ ã��, ���ٸ�, LRUList�� �ϳ� �����ؼ� ã�´�.
     * 2. 1�� ������ ��� prepareList�� 1���� �ٸ� ������ �����ϰ�, LRUList���� 1����
     *      �ٸ������� �������� ã�´�.
     * 3. ��� prepareList���� ã�� �������� victim�� ã�� �� ���ٸ�,
     *      prepareList�ϳ��� ��� ���⿡ BCB�� ���涧���� ����Ѵ�.
     * 4. prepareList�� BCB�� �������, �ƴ� �ð��� �ٵǴ��� �����
     *      �ٽ� �� prepareList�� victimŽ���� �õ��Ѵ�.
     * */
    while( sVictimBCB == NULL )
    {
        getVictimFromPrepareList( aStatistics, aListKey, &sVictimBCB );
        sTryCount++;

        if( sVictimBCB == NULL )
        {
            getVictimFromLRUList(aStatistics,
                                 aListKey,
                                 &sVictimBCB);

            if ( sVictimBCB == NULL )
            {
                if( ( sTryCount % mPrepareListCnt ) == 0 )
                {
                    // flusher���� ����� �ڽ��� prepare list�� ���� ��⸦ �Ѵ�.
                    (void)sdbFlushMgr::wakeUpAllFlushers();

                    sIdx = getPrepareListIdx( aListKey );

                    (void)mPrepareList[ sIdx ].timedWaitUntilBCBAdded(
                                        aStatistics,
                                        smuProperty::getBufferVictimSearchInterval(),
                                        &sBCBAdded );

                    mStatistics.applyVictimWaits();

                    if( sBCBAdded == ID_TRUE )
                    {
                        // �ش� prepare list�� BCB�� �߰��Ǿ��� ������
                        // prepare list�κ��� victimã�⸦ �ѹ� �� �õ��غ���.
                        getVictimFromPrepareList( aStatistics,
                                                  aListKey,
                                                  &sVictimBCB );
                    }

                    if( sVictimBCB != NULL )
                    {
                        mStatistics.applyPrepareAgainVictims();
                    }
                    else
                    {
                        // �־��� �ð����� clean BCB�� ���� ���ߴ�.
                        // victim search warp�� �߻��Ѵ�.
                        // ��, ���� LRU-prepare list set���� �̵��Ͽ� wait�Ѵ�.
                        mStatistics.applyVictimSearchWarps();
                    }
                }
            }
            else
            {
                mStatistics.applyLRUVictims();
            }
        }
        else
        {
            mStatistics.applyPrepareVictims();
        }

        aListKey++;
    }

    IDE_ASSERT( ( sVictimBCB != NULL) &&
                ( sVictimBCB->isFree() == ID_TRUE ) );

    *aReturnedBCB = sVictimBCB;

    return IDE_SUCCESS;
}

/****************************************************************
 * Description :
 *
 *  aSpaceID�� aPageID�� ���ϴ� �������� ���ۻ� Ȯ����Ų��.
 *  �̶�, ������ �߿����� �ʴ�. �ֳĸ� �� �Լ��� BCB�� mFrame��
 *  �ʱ�ȭ �ϱ� ���� �Ҹ��� �Լ��̱� �����̴�. ��, � ������ �ִ���
 *  ���Լ� ������ �ʱ�ȭ �ȴ�. �� ������ �� �Լ��� ������ ������
 *  x��ġ�� ���� ���¿��� BCB�� �����Ѵ�.
 *
 *  ����! ������ x��ġ�� ���� ���¿��� BCB�� �����Ѵ�. ���� fix��Ű��
 *  ������ ���Լ� ������ �ݵ�� releasePage�� ȣ���ؾ� �Ѵ�.
 *
 *  aStatistics - [IN]  �������
 *  aSpaceID    - [IN]  �����ϱ� ���ϴ� space ID
 *  aPageID     - [IN]  �����ϱ� ���ϴ� ������
 *  aPageType   - [IN]  �����ϱ� ���ϴ� ������ type, sdpPageType
 *  aMtx        - [IN]  sdrMtx, createPage���� �߿� ���� ������ x ��ġ
 *                      ���� �� BCB������ ����ϰ� �ִ� �̴�Ʈ�����.
 *  aPagePtr    - [OUT] create�� �������� �����Ѵ�.
 ****************************************************************/
IDE_RC sdbBufferPool::createPage( idvSQL          *aStatistics,
                                  scSpaceID        aSpaceID,
                                  scPageID         aPageID,
                                  UInt             aPageType,
                                  void*            aMtx,
                                  UChar**          aPagePtr)
{
    UChar                    *sPagePtr;
    sdbBCB                   *sBCB = NULL;
    sdbBCB                   *sAlreadyExistBCB  = NULL;
    sdbHashChainsLatchHandle *sHashChainsHandle = NULL;
    sdrMtx                   *sMtx = NULL;
    smxTrans                 *sTrans = NULL;

    IDE_DASSERT( aSpaceID <= SC_MAX_SPACE_COUNT - 1);
    IDE_DASSERT( aMtx     != NULL );
    IDE_DASSERT( aPagePtr != NULL );

    sMtx = (sdrMtx*)aMtx;
    sTrans = (smxTrans*)sMtx->mTrans;

    /* �����ϱ⿡ �ռ� �̹� hash�� �����ϴ��� �˾ƺ���. ����
     * �����Ѵٸ�? �װ��� �����ϸ� �ȴ�. */
    sHashChainsHandle = mHashTable.lockHashChainsSLatch( aStatistics,
                                                         aSpaceID,
                                                         aPageID );

    IDE_TEST( mHashTable.findBCB( aSpaceID,
                                  aPageID,
                                  (void**)&sAlreadyExistBCB )
              != IDE_SUCCESS );

    if ( sAlreadyExistBCB == NULL )
    {
        /* hash�� �������� �ʴ´�.
         * �̰�쿡�� victim�� ���;� �Ѵ�.*/
        mHashTable.unlockHashChainsLatch( sHashChainsHandle );
        sHashChainsHandle = NULL;

        // getVictim������ hash chains latch�� ��� ������,
        // ������ hash chains latch�� Ǯ���ش�.
        IDE_TEST( getVictim( aStatistics,
                             genListKey( (UInt)aPageID, aStatistics),
                             &sBCB )
                  != IDE_SUCCESS );

        /* hash���� ���� �ֱ� ������ fixCount�� ������ ��,
         * hash chains latch�� ���� �ʴ´�. */
        sBCB->lockBCBMutex( aStatistics );

        IDE_DASSERT( sBCB->mSBCB == NULL );

        /* BUG-45904 victim���� �޾ƿ��� BCB�� fix count�� 0�̾�� �Ѵ�. */
        if( sBCB->mFixCnt != 0 )
        {
            ideLog::log( IDE_ERR_0,"Invalid fixed BCB founded.");
            sdbBCB::dump( sBCB );

            if( sTrans != NULL )
            {
                sTrans->dumpTransInfo();
            }
            IDE_DASSERT( 0 );

            sBCB->mFixCnt = 0;
        }

        sBCB->incFixCnt();

        sBCB->mSpaceID     = aSpaceID;
        sBCB->mPageID      = aPageID;
        sBCB->mState       = SDB_BCB_CLEAN;
        sBCB->mPrevState   = SDB_BCB_CLEAN;

        sBCB->mReadyToRead = ID_TRUE;
        sBCB->updateTouchCnt();
        SM_LSN_INIT( sBCB->mRecoveryLSN );
        sBCB->unlockBCBMutex();


        /* ������ BCB�� hash�� ����.. */
        sHashChainsHandle = mHashTable.lockHashChainsXLatch( aStatistics,
                                                             aSpaceID,
                                                             aPageID);
        mHashTable.insertBCB( sBCB,
                              (void**)&sAlreadyExistBCB );

        if( sAlreadyExistBCB != NULL )
        {
            /* by proj-1704 MVCC Renewal ASSERT ����
             *
             * ���Խ���.. �̹� ������ ���� getPage���� fixPageInBuffer����
             * victim�� �� Hash�� �����Ͽ���. ��, ������ ��������
             * ���ؼ� fixPage�� createPage�� ���ÿ� �߻�.
             * ���� �����ϴ� ���� �״�� �̿��ϸ��.
             * ������, ���� �̷��� ���ܻ�Ȳ�� TSS �������� ���ؼ��� �����.
             */
            if ( aPageType == SDP_PAGE_TSS )
            {
                fixAndUnlockHashAndTouch( aStatistics,
                                          sAlreadyExistBCB,
                                          sHashChainsHandle );
                sHashChainsHandle = NULL;

                /*sBCB�� ������ ���� ��� ���� �� FREE���·� �����
                 * prepareList�� ���� */
                sBCB->lockBCBMutex( aStatistics );
                sBCB->decFixCnt();

                sBCB->setToFree();
                sBCB->unlockBCBMutex();

                mPrepareList[getPrepareListIdx((UInt)aPageID)].addBCBList( aStatistics,
                                                                           sBCB,
                                                                           sBCB,
                                                                           1 );

                // �� �������� �����ϴ� Ʈ������� ������
                // flusher�� �� �������� flush�� ���� �ִ�.
                // ���� S-latch�� �����ִ� ������ ���� �ֱ� ������
                // try lock�� �ؼ��� �ȵǰ� ��� ���� latch�� ȹ���ؾ� �Ѵ�.
                sAlreadyExistBCB->lockPageXLatch( aStatistics );
                sBCB            = sAlreadyExistBCB;
                sBCB->mPageType = aPageType;

                IDE_ASSERT( smLayerCallback::isPageTSSType( sBCB->mFrame )
                            == ID_TRUE )
            }
            else
            {
                /* ���� ���� create �� ���ȴ�.  �̷� ���� ���� �� ����.
                 * ��, ���� create �ϰ� �ִµ�, ���ÿ� ���� �������� ����
                 * create�Ǵ� getPage�� �� �� ����. */
                ideLog::log( SM_TRC_LOG_LEVEL_BUFFER,
                             SM_TRC_BUFFER_POOL_CONFLICT_CREATEPAGE,
                             aPageType );
                IDE_ASSERT(0);
            }
        }
        else
        {
            /* hash�� ���� ����..
             * sBCB�� LRU List�� �����Ѵ�. */
            sBCB->lockPageXLatch(aStatistics);
            /* ������ x��ġ�� �����Ŀ� mPageType���� */
            sBCB->mPageType = aPageType;
            IDV_TIME_GET(&sBCB->mCreateOrReadTime);

            mHashTable.unlockHashChainsLatch( sHashChainsHandle );
            sHashChainsHandle = NULL;

            mLRUList[ getLRUListIdx((UInt)aPageID)].insertBCB2BehindMid( aStatistics, sBCB );

            //BCBPtr�� ��ȭ�Ȱ��� �ƴϹǷ�, ���� BCB�� ���ƾ� �Ѵ�.
            sPagePtr = sBCB->mFrame;
            if( ((sdbFrameHdr*)sPagePtr)->mBCBPtr != sBCB )
            {
                /* BUG-32528 disk page header�� BCB Pointer �� ������ ��쿡 ����
                 * ����� ���� �߰�. */
                ideLog::log( IDE_DUMP_0,
                             "mFrame->mBCBPtr( %"ID_xPOINTER_FMT" ) != sBCB( %"ID_xPOINTER_FMT" )\n",
                             ((sdbFrameHdr*)sPagePtr)->mBCBPtr,
                             sBCB );

                smLayerCallback::traceDiskPage( IDE_DUMP_0,
                                                sPagePtr - SD_PAGE_SIZE,
                                                "[ Prev Page ]" );
                smLayerCallback::traceDiskPage( IDE_DUMP_0,
                                                sPagePtr,
                                                "[ Curr Page ]" );
                smLayerCallback::traceDiskPage( IDE_DUMP_0,
                                                sPagePtr + SD_PAGE_SIZE,
                                                "[ Next Page ]" );

                sdbBCB::dump( sBCB );

                //BUG-48422: BCB�� ������ �������� ������ �ִ� BCB ������ ���� �ٲ��ش�.
                sdbBCB::setBCBPtrOnFrame( ((sdbFrameHdr*)sPagePtr), sBCB );
            }

            sdbBCB::setSpaceIDOnFrame( (sdbFrameHdr*)sBCB->mFrame, aSpaceID );
        }
    }
    else
    {
        if ( aPageType == SDP_PAGE_TSS )
        {
            /* BUG-37702 DelayedStamping fixes an TSS page that will reuse
             * ���� ������ TSS page�� DelayedStamping(sdcTSSegment::getCommitSCN)�� ����
             * �̹� commit�� Tx�� TSSlot�� ���� �� �� �ֽ��ϴ�.
             * ���� createPage�� ���ۿ� �̹� ������ �� �ְ� fix�Ǿ����� �� �ֽ��ϴ�. */
        }
        else
        {
            // create �Ϸ��� �ϴ� BCB�� ���ۿ� ���� �� ���� ������,
            // �̰��� �̹� �ٸ� ������� ���� ������ ���� ���Ĵ�.
            if ( isFixedBCB( sAlreadyExistBCB ) == ID_TRUE )
            {
                ideLog::log( IDE_ERR_0,"Invalid fixed BCB detected.");
                sdbBCB::dump(sAlreadyExistBCB);
                
                if( sTrans != NULL )
                {
                    sTrans->dumpTransInfo();
                }

                IDE_ERROR( 0 );
            }
        }

        /*�̹� hash�� BCB�� �����Ѵ�.*/
        fixAndUnlockHashAndTouch( aStatistics, sAlreadyExistBCB, sHashChainsHandle );
        sHashChainsHandle = NULL;

        // �� �������� �����ϴ� Ʈ������� ������
        // flusher�� �� �������� flush�� ���� �ִ�.
        // ���� S-latch�� �����ִ� ������ ���� �ֱ� ������
        // try lock�� �ؼ��� �ȵǰ� ��� ���� latch�� ȹ���ؾ� �Ѵ�.
        sAlreadyExistBCB->lockPageXLatch( aStatistics );

        sBCB = sAlreadyExistBCB;
        sBCB->mPageType = aPageType;
    }

    mStatistics.applyCreatePages( aStatistics,
                                  aSpaceID,
                                  aPageID,
                                  aPageType );

    IDE_ASSERT( smLayerCallback::pushToMtx( aMtx,
                                            sBCB,
                                            SDB_X_LATCH )
                == IDE_SUCCESS );

    *aPagePtr = sBCB->mFrame;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sHashChainsHandle != NULL )
    {
        mHashTable.unlockHashChainsLatch( sHashChainsHandle );
        sHashChainsHandle = NULL;
    }
    else 
    {
        /* nothing to do */
    }

    return IDE_FAILURE;
}

/****************************************************************
 * Description :
 *  aBCB�� ����Ű�� mSpaceID�� mPageID�� ���� ��ũ���� mFrame����
 *  �о�´�.
 *  ���� !
 *  �� �Լ� ������ BCB�� mFrame���� �ƹ��� �������� ���ϰ� �ؾ��Ѵ�.
 *
 *  aStatistics - [IN]  �������
 *  aBCB        - [IN]  ���⿡ ������ mPageID�� mSpaceID�� �������� mFrame
 *                      ���� �о�´�.
 *  aIsCorruptPage
 *              - [OUT] page�� corrupt������ ��ȯ�Ѵ�.
 ****************************************************************/
IDE_RC sdbBufferPool::readPageFromDisk( idvSQL                 *aStatistics,
                                        sdbBCB                 *aBCB,
                                        idBool                 *aIsCorruptPage )
{
    UInt            sDummy;
    UInt            sState = 0;
    idvTime         sBeginTime;
    idvTime         sEndTime;
    ULong           sReadTime;
    ULong           sCalcChecksumTime;

    // �ݵ�� fix�Ǿ� �־�� �Ѵ�. �׷��� load �߰��� ���ۿ��� �������� �ʴ´�.
    IDE_DASSERT( isFixedBCB(aBCB) == ID_TRUE );
    IDE_DASSERT( aBCB->mReadyToRead == ID_FALSE );

    if( aIsCorruptPage != NULL )
    {
        *aIsCorruptPage = ID_FALSE;
    }

    IDV_TIME_GET(&sBeginTime);

    mStatistics.applyBeforeSingleReadPage( aStatistics );
    sState = 1;

    IDE_TEST( sddDiskMgr::read( aStatistics,
                                aBCB->mSpaceID,
                                aBCB->mPageID,
                                aBCB->mFrame,
                                &sDummy )
              != IDE_SUCCESS );

    sState = 0;
    mStatistics.applyAfterSingleReadPage( aStatistics );

    IDV_TIME_GET(&sEndTime);
    sReadTime = IDV_TIME_DIFF_MICRO(&sBeginTime, &sEndTime);
    sBeginTime = sEndTime;

    if ( smLayerCallback::checkAndSetPageCorrupted( aBCB->mSpaceID,
                                                    aBCB->mFrame ) == ID_TRUE )
    {
        if( aIsCorruptPage != NULL )
        {
            *aIsCorruptPage = ID_TRUE;
        }

        if ( smLayerCallback::getCorruptPageReadPolicy()
              != SDB_CORRUPTED_PAGE_READ_PASS )
        {
            IDE_RAISE( page_corruption_error );
        }
    }

    setFrameInfoAfterReadPage( aBCB,
                               ID_TRUE );  // check to online tablespace

    mStatistics.applyReadPages( aStatistics ,
                                aBCB->mSpaceID,
                                aBCB->mPageID,
                                aBCB->mPageType );

    IDV_TIME_GET(&sEndTime);
    sCalcChecksumTime = IDV_TIME_DIFF_MICRO(&sBeginTime, &sEndTime);

    /* BUG-32670    [sm-disk-resource] add IO Stat information 
     * for analyzing storage performance.
     * �Ϲ� ReadPage�� �ɸ� �ð� ������.
     * �� Checksum�� ���� Checksum �� ���������� ���ԵǴ� ���� */
    mStatistics.applyReadByNormal( sCalcChecksumTime,
                                   sReadTime,
                                   1 );

    return IDE_SUCCESS;

    IDE_EXCEPTION( page_corruption_error );
    {
        /* BUG-45598: BCB���� ���� ������ Corrupt ������ �� ��� false ����.
         * ���� aIsCorruptPage�� NULL�� �ƴ� ����� �� �Լ��� redo ������
         * sdrRedoMgr::applyHashedLogRec���� �Ҹ� ����. �� ��� BCB ������
         * �������־�� �Ѵ�. Corrupt �������� ���� ó���� ���� �Լ����� 
         * ����Ǳ� �����̴�.
         */
        if ( aIsCorruptPage != NULL )
        {
            setFrameInfoAfterReadPage( aBCB,
                                       ID_TRUE );  // check to online tablespace
        }

        sdpPhyPage::tracePage( IDE_DUMP_0,
                               aBCB->mFrame,
                               "[ Corruption Page ]");

        switch ( smLayerCallback::getCorruptPageReadPolicy() )
        {
            case SDB_CORRUPTED_PAGE_READ_FATAL :
                IDE_SET( ideSetErrorCode( smERR_FATAL_PageCorrupted,
                                          aBCB->mSpaceID,
                                          aBCB->mPageID ));

                break;
            case SDB_CORRUPTED_PAGE_READ_ABORT :
                // PROJ-1665 : ABORT Error�� ��ȯ��
                //����: ����������, mFrame�� �޴� �ʿ��� �̰� ȣ���ؼ� ���.
                IDE_SET( ideSetErrorCode( smERR_ABORT_PageCorrupted,
                                          aBCB->mSpaceID,
                                          aBCB->mPageID));
                break;
            default:
                break;
        }
    }

    IDE_EXCEPTION_END;

    if ( sState != 0 )
    {
        mStatistics.applyAfterSingleReadPage( aStatistics );
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Descrition:
 *  ��ũ���� �������� �о�� ��, BCB�� �о�� frame�� �������� ������ �����Ѵ�
 *
 *  aBCB            - [IN] BCB
 *  aChkOnlineTBS   - [IN] TBS Online ���θ� Ȯ���ؼ� SMO NO ���� (Yes/No)
 **********************************************************************/
void sdbBufferPool::setFrameInfoAfterReadPage( sdbBCB * aBCB,
                                               idBool   aChkOnlineTBS )
{
    aBCB->mPageType = smLayerCallback::getPhyPageType( aBCB->mFrame );
    IDV_TIME_GET(&aBCB->mCreateOrReadTime);

    /* disk���� �������� ���� �� �ݵ�� ��������� �ϴ� ������ */
    sdbBCB::setBCBPtrOnFrame( (sdbFrameHdr*)aBCB->mFrame, aBCB );
    sdbBCB::setSpaceIDOnFrame( (sdbFrameHdr*)aBCB->mFrame, aBCB->mSpaceID );

    // BUG-47429 SMO NO�� 0�� �ƴ� ��쿡�� SMO NO �ʱ�ȭ �ʿ� ���θ� Ȯ���Ѵ�.
    if ( smLayerCallback::getIndexSMONo( aBCB->mFrame ) != 0 )
    {
        // SMO no�� �ʱ�ȭ�ؾ� �Ѵ�.
        smLayerCallback::resetIndexSMONo( aBCB->mFrame,
                                          aBCB->mSpaceID,
                                          aChkOnlineTBS );
    }
}

/****************************************************************
 * Description :
 *  ��û�� �������� ��ũ�� ���� �о���� �Լ�. �о���°� �Ӹ� �ƴ϶�,
 *  fix �� touch �ϰ�, hash�� LRU����Ʈ�� �����Ѵ�.
 *
 *  aStatistics - [IN]  �������
 *  aSpaceID    - [IN]  table space ID
 *  aPageID     - [IN]  page ID
 *  aReadMode   - [IN]  page read mode(SPR or MPR)
 *  aHit        - [OUT] Hit�Ǿ����� ���� ����
 *  aReplacedBCB- [OUT] replace�� BCB�� �����Ѵ�.
 *  aIsCorruptPage
 *              - [OUT] page�� Corrupt ������ ��ȯ�Ѵ�.
 *
 ****************************************************************/
IDE_RC sdbBufferPool::readPage( idvSQL                   *aStatistics,
                                scSpaceID                 aSpaceID,
                                scPageID                  aPageID,
                                sdbPageReadMode           aReadMode,
                                idBool                   *aHit,
                                sdbBCB                  **aReplacedBCB,
                                idBool                   *aIsCorruptPage )
{
    sdbBCB                     *sBCB              = NULL;
    sdbBCB                     *sAlreadyExistBCB  = NULL;
    sdbHashChainsLatchHandle   *sHashChainsHandle = NULL;
    /* PROJ-2102 Secondary Buffer Control Block  */
    sdsBCB  * sSBCB          = NULL;
    idBool    sIsCorruptRead = ID_FALSE;
         
    IDE_TEST( getVictim( aStatistics,
                         genListKey( (UInt)aPageID, aStatistics ),
                         &sBCB )
              != IDE_SUCCESS );

    IDE_DASSERT( ( sBCB != NULL ) && ( sBCB->isFree() == ID_TRUE ) );

    /* ���� sBCB�� Free�̱� ������ FixCount ���Žÿ� hash chains latch�� ����
     * �ʴ´�. */
    sBCB->lockBCBMutex( aStatistics );

    IDE_DASSERT( sBCB->mSBCB == NULL );
    IDE_DASSERT( sBCB->mFixCnt == 0 );

    sBCB->incFixCnt();
    sBCB->mSpaceID   = aSpaceID;
    sBCB->mPageID    = aPageID;
    sBCB->mState     = SDB_BCB_CLEAN;
    sBCB->mPrevState = SDB_BCB_CLEAN;

    SM_LSN_INIT( sBCB->mRecoveryLSN );

    /* BUG-24092: [SD] BufferMgr���� BCB�� Latch Stat�� ���Ž� Page Type��
     * Invalid�Ͽ� ������ �׽��ϴ�.
     *
     * Ÿ Transaction�� mPageType�� ���� �� �ֱ⶧������ ���⼭ �ʱ�ȭ�մϴ�. */
    sBCB->mPageType = SDB_NULL_PAGE_TYPE;

    if ( aReadMode == SDB_SINGLE_PAGE_READ )
    {
        /* MPR�� ��� touch count�� ������Ű�� �ʴ´�.
         * �ֳ��ϸ� sdbBufferPool::cleanUpKey()����
         * touch count�� 0���� ũ�� BCB�� LRU list��
         * �־���⸮ �����̴�. */
        sBCB->updateTouchCnt();
    }
    sBCB->unlockBCBMutex();

    // free ������ BCB�� ��� mReadyToRead�� ID_FALSE �̴�.
    // �ֳĸ�, free ������ Frame�� �о ���ϰڴ°�?
    IDE_ASSERT( sBCB->mReadyToRead == ID_FALSE );
    // �ؽÿ� �ޱ� �������� ������ x��ġ�� mReadIOMutex�� ��⸦ �õ� ���� ���Ѵ�.
    /* ��ũ���� �����͸� �о� �� ���̹Ƿ� ������ x ��ġ�� ��´�.
     * ���� Ʈ������� ������ x ��ġ�� ���� ���� ���̹Ƿ� hash�� ����
     * fix�� �Ͽ��� �ϴ��� ���� ������ ���Ѵ�. */
    /* BUG-21836 ���۸Ŵ������� flusher�� readPage���� ���ü� ������ assert
     * ���� ���� �� �ֽ��ϴ�.
     * sdbFlusher::flushForCheckpoint�� sdbFlusher::flushDBObjectBCB
     * ���� sBCB�� slatch�� ���� �� �ֽ��ϴ�. �̰�쿣 �׵��� ��ġ�� Ǯ�⸦
     * ���⼭ ����մϴ�.*/
    sBCB->lockPageXLatch( aStatistics );

    /* no latch�� �д� Ʈ����ǵ� ���� ���ϰ� �����.
     * ��, no latch�� �д� Ʈ����������� mReadIOMutex�� �����ִٸ�,
     * ���⼭ ��⸦ �ϰ� �ȴ�. */
    sBCB->mReadIOMutex.lock( aStatistics );

    /* ���� hash�� ���Ժ��� �Ѵ�. ���� Ʈ����ǵ��� hit�� �� ������, (fix count���� ����)
     * ������ x ��ġ�� mReadIOMutex ������ ���� �����͸� ������ ���Ѵ�.*/
    sHashChainsHandle = mHashTable.lockHashChainsXLatch( aStatistics,
                                                         aSpaceID,
                                                         aPageID );

    /* ���� Hash Table�� ������ �� ���Ŀ� ��ũ���� �о�´�.
     * �ֳĸ�, ���� ��ũ���� �о�� �Ŀ� �����ϰ� �Ǹ�, �ڽ��� �о�� ������
     * �ٸ� Ʈ����ǵ� ���ÿ� ���� �� �ֱ� ������, IO ��뿡�� ���ظ� ����.
     * �׷��ٰ�, �ڽ��� �д´ٰ��ؼ� �ٸ� Ʈ������� ���� ���ϰ� ���
     * Hash table �� ��ġ�� �ɰ� ���� ���� ���� �븩�̴�.*/
    mHashTable.insertBCB( sBCB,
                          (void**)&sAlreadyExistBCB );

    if( sAlreadyExistBCB != NULL )
    {
        /* ���Խ���.. �̹� ������ ���� ����, �����ϴ� ���� �״�� �̿�,
         * ��ũ���� �о�� �ʿ䰡 ����. */
        fixAndUnlockHashAndTouch( aStatistics, sAlreadyExistBCB, sHashChainsHandle );
        sHashChainsHandle = NULL;
        /*sBCB�� ������ ���� ��� ���� �� FREE���·� ����� prepareList�� ���� */
        sBCB->mReadIOMutex.unlock();
        sBCB->unlockPageLatch();

        sBCB->lockBCBMutex( aStatistics );
        sBCB->decFixCnt();
        sBCB->setToFree();
        sBCB->unlockBCBMutex();

        mPrepareList[getPrepareListIdx((UInt)aPageID)].addBCBList( aStatistics,
                                                                   sBCB,
                                                                   sBCB,
                                                                   1 );

        *aReplacedBCB = sAlreadyExistBCB;
        *aHit = ID_TRUE;
    }
    else
    {
        /*���� ����.. ���� ��ũ���� ���� �����͸� ���۷� �о�;� �Ѵ�.*/
        mHashTable.unlockHashChainsLatch( sHashChainsHandle );
        sHashChainsHandle = NULL;

        if ( isSBufferServiceable() == ID_TRUE )
        {
            sdsBufferMgr::applyGetPages();

            IDE_TEST_RAISE( sdsBufferMgr::findBCB( aStatistics, 
                                                   aSpaceID,
                                                   aPageID,
                                                   &sSBCB )
                            != IDE_SUCCESS,
                            ERROR_READ_PAGE_FAULT );
        }

        if ( sSBCB != NULL )
        {
            IDE_TEST_RAISE( sdsBufferMgr::moveUpPage( aStatistics,
                                                      &sSBCB, 
                                                      sBCB,
                                                      &sIsCorruptRead,
                                                      aIsCorruptPage )
                             != IDE_SUCCESS,
                             ERROR_READ_PAGE_FAULT);
            
            if ( sIsCorruptRead == ID_FALSE )
            { 
                SM_GET_LSN( sBCB->mRecoveryLSN, sSBCB->mRecoveryLSN );
                /* ���� sBCB link */
                sBCB->mSBCB = sSBCB;

                if( sSBCB->mState == SDS_SBCB_DIRTY )
                {
                    setDirtyBCB( aStatistics, sBCB );
                }
                else
                {
                    /* state �� clean, iniob �ϼ� �ִ�.
                       �̰��� clean���� ó�� */
                    /* nothing to do */
                }
                IDE_DASSERT( validate(sBCB) == IDE_SUCCESS );
            }
            else 
            {
                IDE_TEST_RAISE( readPageFromDisk( aStatistics,
                                                  sBCB,
                                                  aIsCorruptPage ) != IDE_SUCCESS,
                                ERROR_READ_PAGE_FAULT);
            }
        } 
        else       
        { 
            IDE_TEST_RAISE( readPageFromDisk( aStatistics,
                                              sBCB,
                                              aIsCorruptPage ) != IDE_SUCCESS,
                            ERROR_READ_PAGE_FAULT);
        }

        /* BUG-28423 - [SM] sdbBufferPool::latchPage���� ASSERT�� ������
         * �����մϴ�. */
        IDU_FIT_POINT( "1.BUG-28423@sdbBufferPool::readPage" );

        /*I/O�� ���� �ɾ�ξ��� ��� ��ġ�� Ǭ��.*/
        sBCB->mReadyToRead = ID_TRUE;
        sBCB->mReadIOMutex.unlock();
        sBCB->unlockPageLatch();

        mLRUList[getLRUListIdx((UInt)aPageID)].insertBCB2BehindMid( aStatistics,
                                                                    sBCB);
        *aReplacedBCB = sBCB;
        *aHit = ID_FALSE;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERROR_READ_PAGE_FAULT)
    {
        IDE_ASSERT( sBCB != NULL );

    retry:
        sHashChainsHandle = mHashTable.lockHashChainsXLatch( aStatistics,
                                                             aSpaceID,
                                                             aPageID );
        if ( sBCB->mFixCnt != 1 )
        {
            /*���� fix�ϰ� �����Ƿ� �ݵ�� 1�̻��̴�.*/
            IDE_ASSERT( sBCB->mFixCnt != 0);
            /* �� �̿��� ������ fix�ϰ� �ִ�. �̰��� no latch�� �����ϴ�,
             * ��, fixPage���꿡 ���� ���� �� �� �ִ�. �̰�� �������� �����ϰ�,
             * �׵��� unfix�Ҷ� ���� ��ٸ���. */
            sBCB->mPageReadError = ID_TRUE;
            sBCB->mReadIOMutex.unlock();
            mHashTable.unlockHashChainsLatch( sHashChainsHandle );
            sHashChainsHandle = NULL;
            /* BUG-21576 page corruption���� ó����, ���۸Ŵ������� deadlock
             * ���ɼ� ����.
             * */
            sBCB->unlockPageLatch();

            idlOS::thr_yield();
            sBCB->lockPageXLatch( aStatistics );
            sBCB->mReadIOMutex.lock( aStatistics );
            goto retry;
        }
        else
        {
            /* �ؽÿ� x��ġ�� ��� �ֱ� ������ mFixCnt�� ������ �ʴ´�. */
            IDE_ASSERT( sBCB->mFixCnt == 1 );
            mHashTable.removeBCB( sBCB );
            mHashTable.unlockHashChainsLatch( sHashChainsHandle );
            sHashChainsHandle = NULL;
        }

        sBCB->mReadIOMutex.unlock();

        sBCB->unlockPageLatch();

        sBCB->lockBCBMutex( aStatistics );
        sBCB->decFixCnt();
        sBCB->setToFree();
        sBCB->unlockBCBMutex();

        mPrepareList[ getPrepareListIdx((UInt)aPageID)].addBCBList( aStatistics,
                                                                    sBCB, 
                                                                    sBCB, 
                                                                    1 );
    }
    //fix BUG-29682 The position of IDE_EXCEPTION_END is wrong.
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

/****************************************************************
 * Description :
 *  aSpaceID, aPageID�� ���ϴ� �������� ���ۿ� fix��Ű��, ��ġ ��忡
 *  �´� ��ġ�� �ɾ �������ش�.
 *
 *  aStatistics - [IN]  �������
 *  aSpaceID    - [IN]  table space ID
 *  aPageID     - [IN]  page ID
 *  aLatchMode  - [IN]  fix�� ���� �������� ���� ���� ������ ��ġ ����
 *  aWaitMode   - [IN]  ������ ��ġ�� �ɶ�, try���� ���� ����
 *  aReadMode   - [IN]  page read mode(SPR or MPR)
 *  aMtx        - [IN]  �ش� �̴�Ʈ�����
 *  aRetPage    - [OUT] fix�ǰ� ������ ��ġ �ɸ� �������� ������
 *  aTrySuccess - [OUT] ��ġ�� ���������� �ɾ����� ����. ��ġ�� ���� ���ߴٸ�
 *                      �������� fix��Ű���� �ʴ´�.
 ****************************************************************/
IDE_RC sdbBufferPool::getPage( idvSQL               * aStatistics,
                               scSpaceID              aSpaceID,
                               scPageID               aPageID,
                               sdbLatchMode           aLatchMode,
                               sdbWaitMode            aWaitMode,
                               sdbPageReadMode        aReadMode,
                               void                 * aMtx,
                               UChar               ** aRetPage,
                               idBool               * aTrySuccess,
                               idBool               * aIsCorruptPage )
{
    sdbBCB *sBCB;
    idBool  sLatchSuccess = ID_FALSE;
    idBool  sHit          = ID_FALSE;

retry:
    /* ���� fixPageInBuffer�� ȣ���Ͽ� ���ۿ� �÷����� unFixPage�ϱ� ������
     * ���ۿ��� �������� �ʵ��� �Ѵ�. fixPageInBuffer�� ȣ���� ���Ķ��,
     * �������� ���ۿ� ����Ǿ� ������ ������ �� �ִ�.
     * �Ʒ� �Լ� ������ �ݵ�� unfixPage�� �ؾ� �Ѵ�.*/
    IDE_TEST( fixPageInBuffer( aStatistics,
                               aSpaceID,
                               aPageID,
                               aReadMode,
                               aRetPage,
                               &sHit,
                               &sBCB,
                               aIsCorruptPage )
              != IDE_SUCCESS );

    latchPage( aStatistics,
               sBCB,
               aLatchMode,
               aWaitMode,
               &sLatchSuccess );

    // BUG-21576 page corruption ���� ó����, ���۸Ŵ������� deadlock ���ɼ�����
    if( sBCB->mPageReadError == ID_TRUE )
    {
        /* BUG-45598: CorruptPageErrPolicy�� FATAL�� ���� ������ Corrupt������
         * ������ ����ϵ��� ��õ� ��.
         */
        if ( smLayerCallback::getCorruptPageReadPolicy()
             == SDB_CORRUPTED_PAGE_READ_FATAL )
        {
            /* ������ ������ ����� */
            /* �ڽ��� ��ũ���� �о�ͼ� ���� ������ ������ ƨ�涧 ����
             * ��� �����Ѵ�. �ֳĸ�, ���� ������ �߻��ߴ����� ���� ������
             * ������ ������ �𸣱� �����̴�. */
            if( sLatchSuccess == ID_TRUE )
            {
                releasePage( aStatistics, sBCB, aLatchMode );
            }
            else
            {
                unfixPage(aStatistics, sBCB);
            }
            goto retry;
        }
        else
        {
            /* do nothing */
        }
    }

    if( sLatchSuccess == ID_TRUE )
    {
        mStatistics.applyGetPages(aStatistics,
                                  sBCB->mSpaceID,
                                  sBCB->mPageID,
                                  sBCB->mPageType);
        if(aMtx != NULL)
        {
            IDE_ASSERT( smLayerCallback::pushToMtx( aMtx,
                                                    sBCB,
                                                    aLatchMode )
                        == IDE_SUCCESS );
        }
        //BUG-22042 [valgrind]sdbBufferPool::fixPageInBuffer�� UMR�� �ֽ��ϴ�.
        if( sHit == ID_TRUE )
        {
            mStatistics.applyHits(aStatistics,
                                  sBCB->mPageType,
                                  sBCB->mBCBListType );
        }
    }
    else
    {
        //��ġ�� �����Ͽ��� ������ fix�� �����Ѵ�.
        unfixPage(aStatistics, sBCB );
    }

    if( aTrySuccess != NULL )
    {
        *aTrySuccess = sLatchSuccess;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/****************************************************************
 * Description :
 * ���� page fix�� �Ѵ�.
 * mtx�� ������� ������, �������� ���� latch�� ������ �ʴ´�.
 * getPage�� no-latch ���� ����.
 * �� �������� �����ϴ� ���� �� �Լ��� ����ϴ� ���� å���̸�,
 * �� �Լ� ȣ���� �ݵ�� unfixPage�� ȣ��Ǿ�� �Ѵ�.
 *
 *  aStatistics - [IN]  �������
 *  aSpaceID    - [IN]  table space ID
 *  aPageID     - [IN]  page ID
 *  aRetPage    - [OUT] fix�� �������� �����Ѵ�. ���ϵǴ� ����������
 *                      ���� ������ ��ġ�� ���� ���� �ʴ�.
 *
 ****************************************************************/
IDE_RC sdbBufferPool::fixPage( idvSQL              *aStatistics,
                               scSpaceID            aSpaceID,
                               scPageID             aPageID,
                               UChar              **aRetPage)

{
    sdbBCB *sBCB       = NULL;
    idBool  sHit       = ID_FALSE;

 retry:
    /* ���� fixPageInBuffer�� ȣ���Ͽ� ���ۿ� �÷����� unFixPage�ϱ� ������
     * ���ۿ��� �������� �ʵ��� �Ѵ�. fixPageInBuffer�� ȣ���� ���Ķ��,
     * �������� ���ۿ� ����Ǿ� ������ ������ �� �ִ�.*/
    IDE_TEST( fixPageInBuffer( aStatistics,
                               aSpaceID,
                               aPageID,
                               SDB_SINGLE_PAGE_READ,
                               aRetPage,
                               &sHit,
                               &sBCB,
                               NULL/*IsCorruptPage*/ )
              != IDE_SUCCESS );

    // fix page�� nolatch�� ����������, ���� ��ũ���� �д� ����
    // �������� �������� �ʰ� �ؾ��Ѵ�.
    latchPage(aStatistics,
              sBCB,
              SDB_NO_LATCH,
              SDB_WAIT_NORMAL,
              NULL);  // lock ���������� ��Ҵ��� ����

    if( sBCB->mPageReadError == ID_TRUE )
    {
        if ( smLayerCallback::getCorruptPageReadPolicy()
             == SDB_CORRUPTED_PAGE_READ_FATAL )
        {
            /* ������ ������ ����� */
            /* �ڽ��� ��ũ���� �о�ͼ� ���� ������ ������ ƨ�涧 ����
             * ��� �����Ѵ�. �ֳĸ�, ���� ������ �߻��ߴ����� ���� ������
             * ������ ������ �𸣱� �����̴�. */
            unfixPage(aStatistics, sBCB );
            goto retry;
        }
        else
        {
            /* do nothing */
        }   
    }

    if( sHit == ID_TRUE )
    {
        //BUG-22042 [valgrind]sdbBufferPool::fixPageInBuffer�� UMR�� �ֽ��ϴ�.
        mStatistics.applyHits(aStatistics,
                              sBCB->mPageType,
                              sBCB->mBCBListType );
    }
    mStatistics.applyFixPages(aStatistics,
                              sBCB->mSpaceID,
                              sBCB->mPageID,
                              sBCB->mPageType);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

/****************************************************************
 * Description :
 ****************************************************************/
IDE_RC sdbBufferPool::fixPageWithoutIO( idvSQL              *aStatistics,
                                        scSpaceID            aSpaceID,
                                        scPageID             aPageID,
                                        UChar              **aRetPage)
{
    sdbBCB                   * sBCB = NULL;
    sdbHashChainsLatchHandle * sHashChainsHandle = NULL;

    /* hash�� �ִٸ� �׳� fix�Ŀ� �����ϰ�, �׷��� �ʴٸ� replace�Ѵ�.*/
    sHashChainsHandle = mHashTable.lockHashChainsSLatch( aStatistics,
                                                         aSpaceID,
                                                         aPageID );

    IDE_TEST( mHashTable.findBCB( aSpaceID, aPageID, (void**)&sBCB )
              != IDE_SUCCESS );

    /* To Fix BUG-23380 [SD] sdbBufferPool::fixPageWithoutIO����
     * ��ũ�κ��� readpage�� �Ϸ���� ���� BCB�� ���ؼ��� fix ���� �ʴ´�.
     * �ֳ��ϸ�, �� �Լ��� ���� ������ ����� ������ ����. */
    if ( ( sBCB != NULL ) && ( sBCB->mReadyToRead == ID_TRUE ) )
    {
        fixAndUnlockHashAndTouch( aStatistics,
                                  sBCB,
                                  sHashChainsHandle );
        sHashChainsHandle = NULL;

        IDE_DASSERT( sBCB->mReadyToRead == ID_TRUE );

        mStatistics.applyHits( aStatistics,
                               sBCB->mPageType,
                               sBCB->mBCBListType );

        mStatistics.applyFixPages( aStatistics,
                                   sBCB->mSpaceID,
                                   sBCB->mPageID,
                                   sBCB->mPageType );
        *aRetPage = sBCB->mFrame;

    }
    else
    {
        mHashTable.unlockHashChainsLatch( sHashChainsHandle );
        sHashChainsHandle =  NULL;
        *aRetPage = NULL;
    }
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sHashChainsHandle != NULL )
    {
        mHashTable.unlockHashChainsLatch( sHashChainsHandle );  
        sHashChainsHandle = NULL;
    }
    return IDE_FAILURE;
}

/****************************************************************
 * Description:
 ****************************************************************/
void sdbBufferPool::tryEscalateLatchPage( idvSQL            *aStatistics,
                                          sdbBCB            *aBCB,
                                          sdbPageReadMode    aReadMode,
                                          idBool            *aTrySuccess )
{
    iduLatchMode sLatchMode;

    IDE_DASSERT( isFixedBCB(aBCB) == ID_TRUE );

    sLatchMode = aBCB->mPageLatch.getLatchMode( );

    if( sLatchMode == IDU_LATCH_READ )
    {
        aBCB->lockBCBMutex(aStatistics);

        IDE_ASSERT( (aReadMode == SDB_SINGLE_PAGE_READ) ||
                    (aReadMode == SDB_MULTI_PAGE_READ) );

        if( aReadMode == SDB_SINGLE_PAGE_READ )
        {
            if( aBCB->mFixCnt > 1 )
            {
                *aTrySuccess = ID_FALSE;
                aBCB->unlockBCBMutex();
                IDE_CONT( skip_escalate_lock );
            }
            IDE_ASSERT( aBCB->mFixCnt == 1 );
        }

        if( aReadMode == SDB_MULTI_PAGE_READ )
        {
            /* MPR�� ����ϴ� ���, fixCount�� �ι� ������Ű�� �ι� ���ҽ�Ų��.
             * 1. sdbMPRMgr::getNxtPageID...()�ÿ� fixcount ������Ŵ
             * 2. sdbBufferMgr::getPage()�ÿ� fixcount ������Ŵ
             * 3. sdbBufferMgr::releasePage()�ÿ� fixcount ���ҽ�Ŵ
             * 4. sdbMPRMgr::destroy()�ÿ� fixcount ���ҽ�Ŵ */
            /* �׷��� fixCnt�� 2�� ��쿡�� lock escalation�� �õ��غ���. */
            if( aBCB->mFixCnt > 2 )
            {
                *aTrySuccess = ID_FALSE;
                aBCB->unlockBCBMutex();
                IDE_CONT( skip_escalate_lock );
            }
            IDE_ASSERT( aBCB->mFixCnt == 2 );
        }

        // To Fix BUG-23313
        // Fix Count�� 1�̶� �ٸ� Thread�� Page�� Latch��
        // ȹ���ϴ� ��찡 �ִµ� Flusher�� Page�� Flush�Ҷ�
        // �̴�. �̸� ����Ͽ� try�� X-Latch�� ȹ���غ���
        // �����ϸ�, �ٽ� Page�� S-Latch�� ȹ���Ѵ�.
        aBCB->unlockPageLatch();
        aBCB->tryLockPageXLatch( aTrySuccess );

        if ( *aTrySuccess == ID_FALSE )
        {
            aBCB->lockPageSLatch( aStatistics );
        }
        
        aBCB->unlockBCBMutex();
    }
    else
    {
        *aTrySuccess = ID_TRUE;
    }

    IDE_EXCEPTION_CONT( skip_escalate_lock );
}

/****************************************************************
 * Description :
 *  getPage�� BCB�� ������ ��ġ�� �ɷ��ְ� fix�Ǿ� �ִ�.  �̰��� ����
 *  �����ִ� �Լ�
 *
 *  aStatistics - [IN]  �������
 *  aBCB        - [IN]  getPage�� �����ߴ� BCB
 *  aLatchMode  - [IN]  getPage�� ������������ latch mode
 ****************************************************************/
void sdbBufferPool::releasePage( idvSQL          *aStatistics,
                                 void            *aBCB,
                                 UInt             aLatchMode )
{
    if((aLatchMode == (UInt)SDB_S_LATCH) ||
       (aLatchMode == (UInt)SDB_X_LATCH))
    {
        ((sdbBCB*)aBCB)->unlockPageLatch();
    }

    unfixPage(aStatistics, (sdbBCB*)aBCB);
}

void sdbBufferPool::releasePage( idvSQL          *aStatistics,
                                 void            *aBCB )
{
    ((sdbBCB*)aBCB)->unlockPageLatch();
    unfixPage(aStatistics, (sdbBCB*)aBCB);
}

/****************************************************************
 * Description :
 *  aBCB�� ����Ű�� frame�� PageLSN�� �����Ѵ�.
 *
 *  aBCB        - [IN]  BCB
 *  aMtx        - [IN]  Mini transaction
 ****************************************************************/
void sdbBufferPool::setPageLSN( sdbBCB   *aBCB,
                                void     *aMtx )
{
    smLSN        sEndLSN;
    smLSN        sPageLSN;
    idBool       sIsLogging;
    static UInt  sLogFileSize = smuProperty::getLogFileSize();

    IDE_DASSERT( aBCB != NULL );
    IDE_DASSERT( aMtx != NULL );

    sIsLogging = smLayerCallback::isMtxModeLogging( aMtx );

    sEndLSN = smLayerCallback::getMtxEndLSN( aMtx );

    IDE_ASSERT( sEndLSN.mOffset < sLogFileSize );

    if( sIsLogging == ID_TRUE )
    {
        /* logging mode */
        if ( smLayerCallback::isLogWritten( aMtx ) == ID_TRUE )
        {
            sdbBCB::setPageLSN((sdbFrameHdr*)aBCB->mFrame, sEndLSN);
        }
    }
    else
    {
        /* nologging mode �Ǵ� restart recovery redo mode*/
        if ( smLayerCallback::isLSNZero( &sEndLSN ) != ID_TRUE )
        {
            /* Restart Recovery Redo�ÿ��� �������� ���´�.
             * Read Only�� ����ȴ�. Redo�� Mini Trans�� No-Logging����
             * Begin�ǰ�
             * RedoLOG LSN�� Mini Trans�� Commit�� EndLSN���� ��ϵȴ�.
             * ���� sEndLSN�� ���� �� �������� �ݿ��� Redo Log�� LSN�̴�.
             */
            sPageLSN = smLayerCallback::getPageLSN( aBCB->mFrame );

            if ( smLayerCallback::isLSNLT( &sPageLSN, &sEndLSN )
                 == ID_TRUE )
            {
                /* Redo�� Redo�αװ� �������� �ݿ��� �Ǿ��⶧�� */
                sdbBCB::setPageLSN( (sdbFrameHdr*)aBCB->mFrame, sEndLSN);
            }
        }
        else
        {
            /* no logging mode */
        }
    }
}

/****************************************************************
 * Description :
 *  ���� �������� �����Ѵ�.
 *  ���Լ��� ������ ��쿡 ȣ��ȴ�.
 *  - mtx�� savepoint���� �ѹ�ɶ� ȣ��
 *    �̶� ���⼭ �����ϴ� � �������� ������ ���°� �ƴϴ�.
 *  - mtx�� rollback�Ǵ� ���
 *
 * Implementation:
 *
 * mutex
 * �������� fix�� �����Ѵ�.
 * �������� latch�� �����Ѵ�.
 * mutex
 *
 *  aStatistics - [IN]  �������
 *  aBCB        - [IN]  BCB
 *  aLatchMode  - [IN]  ��ġ ���, page latch�� ������� ��ġ ���
 ****************************************************************/
void sdbBufferPool::releasePageForRollback( idvSQL * aStatistics,
                                            void   * aBCB,
                                            UInt     aLatchMode )
{
    sdbBCB *sBCB;

    IDE_DASSERT( aBCB != NULL);

    sBCB = (sdbBCB*)aBCB;

    if( (aLatchMode == (UInt)SDB_X_LATCH) ||
        (aLatchMode == (UInt)SDB_S_LATCH))
    {
        sBCB->unlockPageLatch();
    }

    unfixPage( aStatistics, sBCB );

    return;
}


/****************************************************************
 * Description :
 *      �ش� page id�� ���ۿ� �������� �����ϰ�, BCB�� fix�� �س��Ƽ�
 *      ���ۿ��� �������� ������ ������� �ش�.
 *
 * aStatistics  - [IN]  �������
 * aSpaceID     - [IN]  table space ID
 * aPageID      - [IN]  ���ۿ� fix��Ű�����ϴ� pageID
 * aReadMode    - [IN]  page read mode(SPR or MPR)
 * aRetPage     - [IN]  ���ۿ� fix�� �������� ����
 * aHit         - [OUT] Hit�Ǿ����� ���� ����
 * aFixedBCB    - [OUT] fix�� BCB�� �����Ѵ�.
 *
 ****************************************************************/
IDE_RC sdbBufferPool::fixPageInBuffer( idvSQL              *aStatistics,
                                       scSpaceID            aSpaceID,
                                       scPageID             aPageID,
                                       sdbPageReadMode      aReadMode,
                                       UChar              **aRetPage,
                                       idBool              *aHit,
                                       sdbBCB             **aFixedBCB,
                                       idBool             * aIsCorruptPage )
{
    sdbBCB *sBCB = NULL;
    sdbHashChainsLatchHandle *sHashChainsHandle=NULL;

    IDE_DASSERT( aSpaceID <= SC_MAX_SPACE_COUNT - 1);
    IDE_DASSERT( aRetPage != NULL );

    /* hash�� �ִٸ� �׳� fix�Ŀ� �����ϰ�, �׷��� �ʴٸ� readPage�Ѵ�.*/
    sHashChainsHandle = mHashTable.lockHashChainsSLatch( aStatistics,
                                                         aSpaceID,
                                                         aPageID);

    IDE_TEST( mHashTable.findBCB( aSpaceID,
                                  aPageID,
                                  (void**)&sBCB )
              != IDE_SUCCESS );

    if ( sBCB == NULL )
    {
        /*find ���� readPage�ؾ� �Ѵ�.*/
        mHashTable.unlockHashChainsLatch( sHashChainsHandle );
        sHashChainsHandle = NULL;

        IDE_TEST( readPage( aStatistics,
                            aSpaceID,
                            aPageID,
                            aReadMode,
                            aHit,
                            &sBCB,
                            aIsCorruptPage )
                  != IDE_SUCCESS );
        /*readPage�Ŀ��� �ݵ�� sBCB�� ���ϵ��� ������ �� �ִ�.*/
        IDE_ASSERT( sBCB != NULL );
        IDE_ASSERT( isFixedBCB(sBCB) == ID_TRUE );
    }
    else
    {
        fixAndUnlockHashAndTouch( aStatistics, sBCB, sHashChainsHandle );
        sHashChainsHandle = NULL;
        *aHit = ID_TRUE;
    }

    *aRetPage = sBCB->mFrame;

    if ( aFixedBCB != NULL )
    {
        *aFixedBCB = sBCB;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sHashChainsHandle != NULL )
    {
        mHashTable.unlockHashChainsLatch( sHashChainsHandle );
        sHashChainsHandle = NULL;
    }
    else
    {
        /* nothing to do */
    }
    return IDE_FAILURE;
}

/****************************************************************
 * Description:
 *  MPR ����� ���� ����Ǵ� �Լ�.  ���ӵǾ��ִ� aSpaceID���� aStartPID����
 *  aPageCount��ŭ �о� ���δ�.
 *  MPR�� ���ӵǾ� �ִ� ������ �ѹ��� �о� ���̱� ���� ����Ѵ�.
 *  I/O �纸�� I/O Ƚ���� ���̴� ���� �ſ� �߿��ϱ� �����̴�.
 *  ������, ���� MPR�� target�� �Ǵ� ���������� �̹� ���� ���ۿ� �ִٸ�,
 *  �̵��� ���������� �о� �鿩�� �ϴ��� �������� �����ؾ� �Ѵ�.
 *  (���⿡ ���� �ڼ��� ������ PROJ-1568 ������ ����.)
 *  ������, �ѹ��� �д°��� ������ �д� �ͺ��� ����� �δٸ� �ѹ��� �д´�.
 *  �̰��� ���ؼ� sdbMPRKey�ڷᱸ���� sdbReadUnit�� �̿��Ѵ�.
 *  �׸���, ��� ����� analyzeCostToReadAtOnce�� ���ؼ� �Ѵ�.
 *
 * Implementation:
 *  sStartPID���� ���ۿ� �̸� �����ϴ��� Ȯ���Ѵ�. �̶�, ���ۿ� �̹� ���� �Ѵٸ�
 *  ���� �о�� �ʿ䰡 �����Ƿ�, �̰Ϳ� fix�� �ɾ���´�.
 *  ���� ���ۿ� �������� �ʴ´ٸ� ������ x��ġ�� �ɾ �ؽÿ� ������ ���´�.
 *  �� ������ x��ġ�� ���Ŀ� read�� ���� �Ŀ� Ǯ����.
 *  �̶�, ���ӵ� �������鿡 ���� ������ sdbMPRKey�ڷᱸ���� sdbReadUnit��
 *  ������ ���´�.
 *  mReadUnit���� � pid���� ��� �������� �о� �;� �ϴ��� ������ ������
 *  �ִ�.
 *  �̰��� �̿��� ����� ����ϰ�, �ѹ��� ������ �������� ���ļ� ��������
 *  �����Ѵ�.
 *
 *  aStatistics - [IN]  �������
 *  aSpaceID    - [IN]  table space ID
 *  aStartPI    - [IN]  ���� page ID
 *  aPageCount  - [IN]  aStartPID���� �о���� ������ ����
 *  aKey        - [IN]  MPR Key
 ****************************************************************/
IDE_RC sdbBufferPool::fetchPagesByMPR( idvSQL               *aStatistics,
                                       scSpaceID             aSpaceID,
                                       scPageID              aStartPID,
                                       UInt                  aPageCount,
                                       sdbMPRKey            *aKey )
{
    sdbBCB    * sBCB;
    sdbBCB    * sAlreadyExistBCB;
    sdbBCB    * sFixedBCB;
    SInt        sUnitIndex  = -1;
    idBool      sContReadIO = ID_FALSE;
    UInt        i = 0;
    UInt        j = 0;
    UInt        sMPRSetBCBCnt = 0;
    scPageID    sPID;
    idBool      sIsNeedIO = ID_FALSE;

    sdbHashChainsLatchHandle * sLatchHandle;

    /* PROJ-2102 */
    sdsBCB    * sSBCB = NULL;
    idBool      sIsCorruptRead = ID_FALSE;

    for( i = 0, sPID = aStartPID; i < aPageCount; i++, sPID++ )
    {
        /* ���� �ڽ��� ������ ���ϴ� pid�� �̹� ���ۿ� ���� �ϴ��� Ȯ���Ѵ�.*/
        sLatchHandle = mHashTable.lockHashChainsSLatch(aStatistics,
                                                       aSpaceID,
                                                       sPID);

        IDE_TEST_RAISE( ( mHashTable.findBCB( aSpaceID, sPID, (void**)&sBCB )
                          != IDE_SUCCESS ), FAIL_MPR_BCB_SETTING );

        if ( sBCB != NULL )
        {
            /* �̹� �����Ѵٸ�, fix���� ���� ���Ѽ� ���ۿ� �״�� ���� �ְ� �Ѵ�.
             * �׸���, ���߿� �װ��� �̿��Ѵ�.*/
            sBCB->lockBCBMutex(aStatistics);
            sBCB->incFixCnt();
            /* BUG-22378 full scan(MPR)�� getPgae����� BCB�� touch count�� ����
             * ���Ѿ� �մϴ�.
             * ������ �����ϴ� BCB�� ���� ������ touch count�� ���� ���Ѿ� �մϴ�.
             * */
            sBCB->updateTouchCnt();
            sBCB->unlockBCBMutex();

            mHashTable.unlockHashChainsLatch( sLatchHandle );
            sLatchHandle = NULL;

            /* BUG-29643 [valgrind] sdbMPRMgr::fixPage() ���� valgrind ������
             *           �߻��մϴ�.
             * IO mutex�� ��� Ǯ�� BCB�� DISK I/O �� �Ϸ�Ǳ���� ��ٸ���. */
            if ( sBCB->mReadyToRead == ID_FALSE )
            {
                sBCB->mReadIOMutex.lock( aStatistics );
                sBCB->mReadIOMutex.unlock();
            }

            if ( sBCB->mReadyToRead == ID_FALSE )
            {
                ideLog::log( IDE_DUMP_0,
                             "SpaceID: %u, "
                             "PageID: %u\n",
                             aSpaceID,
                             sPID );

                ideLog::logMem( IDE_DUMP_0,
                                (UChar*)sBCB,
                                ID_SIZEOF( sdbBCB ),
                                "BCB Dump:\n" );

                ideLog::logMem( IDE_DUMP_0,
                                (UChar*)sBCB->mFrame,
                                SD_PAGE_SIZE,
                                "Frame Dump:\n" );

                sBCB->dump( sBCB );

                IDE_ASSERT( 0 );
            }

            /* �̰��� ���ۿ� �������� ǥ���صд�.*/
            aKey->mBCBs[i].mType = SDB_MRPBCB_TYPE_BUFREG;
            aKey->mBCBs[i].mBCB  = sBCB;
            sContReadIO = ID_FALSE;
            IDE_ASSERT( aKey->mBCBs[i].mBCB->mFixCnt != 0 );
        }
        else
        {
            /* �������� �ʴ´ٸ�, �̰��� ��ũ�� ���� �о�;� �� ����̴�.
             * */
            mHashTable.unlockHashChainsLatch( sLatchHandle );
            sLatchHandle = NULL;

            /* key���� (getVictim overhead�� ���̱� ����) freeBCB�� �����ϴµ�,
             * ���� ���ٸ� getVictim�� ȣ���Ͽ� BCB�� ������,
             * �ִٸ�, �װ� �״�� �̿��Ѵ�.
             */
            sBCB = aKey->removeLastFreeBCB();
            if ( sBCB == NULL )
            {
                IDE_TEST_RAISE( ( getVictim( aStatistics, (UInt)sPID, &sBCB )
                                  != IDE_SUCCESS ), FAIL_MPR_BCB_SETTING );
            }
            /* ��ũ���� data�� �о� ���� ���� readPageFromDisk�� �����ϱ� ����
             * �ؾ� �ϴ� �۾����� readPage������ �װͰ� �ſ� �����ϴ�.
             * ��, MPR������ touchCount�� ���� ��Ű�� �ʴ´�.*/
            sBCB->lockBCBMutex( aStatistics );

            IDE_DASSERT( sBCB->mSBCB == NULL );
            IDE_DASSERT( sBCB->mFixCnt == 0 );

            /* ������ �����ϴ� BCB�� ���� ������ �ƴ� ��쿣 touch count�� ����
             * ��Ű�� �ʽ��ϴ�. (touch count�� 0)
             * �׷��� ������ MPR�� ������ �ִ� BCB�� ������ �����ϱ⸸ �ص�
             * touch count�� ���� �ϰ� �Ǿ� �ֽ��ϴ�.
             * cleanUpKey���� touch count�� �������� ���� BCB��
             * �����ϰ�, touch count�� ������ BCB�� ���ؼ� (���ÿ� ���ٵǴ�
             * �����尡 �����ϹǷ�) LRU�� �����մϴ�.
             * */
            sBCB->incFixCnt();
            sBCB->mSpaceID  = aSpaceID;
            sBCB->mPageID   = sPID;

            /*  ���⼭ BCB�� ���¸� clean���� ������,
             *  BCB�� mPageType�� ���������� �ʴ´�.
             *  �׷��� ������, clean������ pageType�� �����Ǿ� ���� �ʰ� �ȴ�.*/
            sBCB->mState     = SDB_BCB_CLEAN;
            sBCB->mPrevState = SDB_BCB_CLEAN;

            SM_LSN_INIT( sBCB->mRecoveryLSN );
            sBCB->unlockBCBMutex();

            // free ������ BCB�� ��� mReadyToRead�� ID_FALSE �̴�.
            IDE_ASSERT( sBCB->mReadyToRead == ID_FALSE );

            /* ���� disk���� �о� �� ���̹Ƿ� �Ʒ� �� ��ġ�� ��´�.
             * ��ġ�� ���� �ڼ��� ������ sdbBufferPool::readPage�� ����. */
            sBCB->lockPageXLatch(aStatistics);
            (void)sBCB->mReadIOMutex.lock(aStatistics);

            IDU_FIT_POINT( "1.BUG-29643@sdbBufferPool::fetchPagesByMPR" );

            sLatchHandle = mHashTable.lockHashChainsXLatch( aStatistics, aSpaceID, sPID );
            mHashTable.insertBCB( sBCB,
                                  (void**)&sAlreadyExistBCB );

            if ( sAlreadyExistBCB != NULL )
            {
                sAlreadyExistBCB->lockBCBMutex(aStatistics);
                sAlreadyExistBCB->incFixCnt();
                /* BUG-22378 full scan(MPR)�� getPgae����� BCB�� touch count�� ����
                 * ���Ѿ� �մϴ�.
                 * ������ �����ϴ� BCB�� ���� ������ touch count�� ���� ���Ѿ� �մϴ�.
                 * */
                sAlreadyExistBCB->updateTouchCnt();
                sAlreadyExistBCB->unlockBCBMutex();

                mHashTable.unlockHashChainsLatch( sLatchHandle );
                sLatchHandle = NULL;

                sBCB->mReadIOMutex.unlock();
                sBCB->unlockPageLatch();

                sBCB->lockBCBMutex(aStatistics);
                sBCB->decFixCnt();
                sBCB->setToFree();
                sBCB->unlockBCBMutex();

                /* BUG-29643 [valgrind] sdbMPRMgr::fixPage() ���� valgrind
                 *           ������ �߻��մϴ�.
                 * IO mutex�� ��� Ǯ�� BCB�� DISK I/O �� �Ϸ�Ǳ����
                 * ��ٸ���. */
                if ( sAlreadyExistBCB->mReadyToRead == ID_FALSE )
                {
                    sAlreadyExistBCB->mReadIOMutex.lock( aStatistics );
                    sAlreadyExistBCB->mReadIOMutex.unlock();
                }

                if ( sAlreadyExistBCB->mReadyToRead == ID_FALSE )
                {
                    ideLog::log( IDE_DUMP_0,
                                 "SpaceID: %u, "
                                 "PageID: %u\n",
                                 aSpaceID,
                                 sPID );

                    ideLog::logMem( IDE_DUMP_0,
                                    (UChar*)sAlreadyExistBCB,
                                    ID_SIZEOF( sdbBCB ),
                                    "BCB Dump:\n" );

                    ideLog::logMem( IDE_DUMP_0,
                                    (UChar*)sAlreadyExistBCB->mFrame,
                                    SD_PAGE_SIZE,
                                    "Frame Dump:\n" );

                    sAlreadyExistBCB->dump( sAlreadyExistBCB );

                    IDE_ASSERT( 0 );
                }

                aKey->mBCBs[i].mType = SDB_MRPBCB_TYPE_BUFREG;
                aKey->mBCBs[i].mBCB  = sAlreadyExistBCB;
                aKey->addFreeBCB(sBCB);

                sContReadIO = ID_FALSE;
                IDE_ASSERT( aKey->mBCBs[i].mBCB->mFixCnt != 0 );

                continue;
            }

            mHashTable.unlockHashChainsLatch( sLatchHandle );
            sLatchHandle = NULL;

            /* 1. ���� Secondatry Buffer���� �˻��Ѵ�. */
            if( isSBufferServiceable() == ID_TRUE )
            { 
                IDE_TEST_RAISE( ( sdsBufferMgr::findBCB( aStatistics, aSpaceID, sPID, &sSBCB )
                          != IDE_SUCCESS ), FAIL_MPR_BCB_SETTING );
            }   

            if ( sSBCB != NULL )
            {
                sContReadIO = ID_FALSE;

                /* Secondary Buffer�� MPR ���� ����
                 * MPR�� �ϱ� ���� ReadIOMutex�� ���� SBCB��
                 * BCB�� ���� ������ ������ ���� ������
                 * (Buffer, Secondary Buffer�Ѵ� ����� ���� ��Ȳ���� �߻����ɼ� ����)
                 * victim�� ���� ���� deadlock ��Ȳ �߻�
                 * ������.. single page�� �д´ٰ� �ϴ��� ������ ������ ����.
                 * SSD�� ����� SB�̹Ƿ�.
                 */
                IDE_TEST_RAISE( ( sdsBufferMgr::moveUpbySinglePage( aStatistics, 
                                                                    &sSBCB, 
                                                                    sBCB, 
                                                                    &sIsCorruptRead )
                                  != IDE_SUCCESS ), FAIL_MPR_BCB_SETTING );

                if( sIsCorruptRead == ID_FALSE )
                {
                    SM_GET_LSN( sBCB->mRecoveryLSN, sSBCB->mRecoveryLSN );
                    /* link */
                    sBCB->mSBCB = sSBCB;
                    IDE_DASSERT( validate(sBCB) == IDE_SUCCESS );
                    if( sSBCB->mState == SDS_SBCB_DIRTY )
                    {
                        setDirtyBCB( aStatistics, sBCB );
                    }
                    /* MPR operation */  
                    aKey->mBCBs[i].mType = SDB_MRPBCB_TYPE_NEWFETCH;
                    aKey->mBCBs[i].mBCB  = sBCB;
                    IDE_ASSERT( aKey->mBCBs[i].mBCB->mFixCnt != 0 );
                    /* copyToFrame operation */
                    sBCB->mReadyToRead = ID_TRUE;
                    (void)sBCB->mReadIOMutex.unlock();
                    sBCB->unlockPageLatch();
                    
                    continue;
                }
            }

            sIsNeedIO = ID_TRUE;

            if( sContReadIO == ID_FALSE )
            {
                sContReadIO = ID_TRUE;
                sUnitIndex++;
                // �о�� �ϴ� �������� ������ OXOXOX �̷� ������ �̷궧
                // (O : �о�� �ϴ� ������,  X: ���ۿ� �����ϴ� ������)
                // sUnitIndex�� ���� �ִ밡 �Ǹ�, �̰���
                // SDB_MAX_MPR_PAGE_COUNT / 2 �̴�.
                IDE_DASSERT(sUnitIndex <= SDB_MAX_MPR_PAGE_COUNT / 2);

                aKey->mReadUnit[sUnitIndex].mFirstPID = sBCB->mPageID;
                aKey->mReadUnit[sUnitIndex].mReadBCBCount = 0;
                aKey->mReadUnit[sUnitIndex].mReadBCB = &(aKey->mBCBs[i]);
            }

            aKey->mReadUnit[sUnitIndex].mReadBCBCount++;
            aKey->mBCBs[i].mType = SDB_MRPBCB_TYPE_NEWFETCH;
            aKey->mBCBs[i].mBCB  = sBCB;
            IDE_ASSERT( aKey->mBCBs[i].mBCB->mFixCnt != 0 );
        }
    }

    sMPRSetBCBCnt        = aPageCount; 

    /* �ش� FIT point�� BUG-45904������ ��� */
    IDU_FIT_POINT( "2.BUG-29643@sdbBufferPool::fetchPagesByMPR" );

    aKey->mReadUnitCount = (UInt)sUnitIndex + 1;
    aKey->mSpaceID       = aSpaceID;
    aKey->mIOBPageCount  = aPageCount;
    aKey->mCurrentIndex  = 0;

    if( sIsNeedIO == ID_TRUE )
    {
        if( aPageCount != 1 )
        {
            if( analyzeCostToReadAtOnce(aKey) == ID_TRUE )
            {
                IDE_TEST(fetchMultiPagesAtOnce(aStatistics,
                                               aStartPID,
                                               aPageCount,
                                               aKey )
                         != IDE_SUCCESS);
            }
            else
            {
                IDE_TEST(fetchMultiPagesNormal( aStatistics,
                                                aKey )
                         != IDE_SUCCESS);
            }
        }
        else
        {
            IDE_TEST( fetchSinglePage( aStatistics,
                                       aStartPID,
                                       aKey )
                      != IDE_SUCCESS );
        }
    }

    aKey->mState = SDB_MPR_KEY_FETCHED;

    return IDE_SUCCESS;

    /* BUG-45959: MPRKey�� BCB�� setting�ϴ� �� ����ó���� �߻��ϴ� ��� 
     * setting�� BCB ���� ��ŭ unfix ���־�� ��.   
     */
    IDE_EXCEPTION( FAIL_MPR_BCB_SETTING )
    {
        if( i != 0 )
        {
            sMPRSetBCBCnt = i;
        }
        else
        {
            sMPRSetBCBCnt = 0;
        }
    }
 
    IDE_EXCEPTION_END;

    if ( sLatchHandle != NULL )
    {
        mHashTable.unlockHashChainsLatch( sLatchHandle );
        sLatchHandle =  NULL;
    }
    else
    {
        /* nothing to do */
    }

    /* BUG-45904 ����ó���� ������Ų fix count�� ���ҽ��Ѿ� �Ѵ�. */
    for( j = 0 ; j < sMPRSetBCBCnt ; j++ )
    {
        sFixedBCB = aKey->mBCBs[j].mBCB;

        sLatchHandle = mHashTable.lockHashChainsXLatch( aStatistics,
                                                        aSpaceID,
                                                        sFixedBCB->mPageID);

        sFixedBCB->lockBCBMutex(aStatistics);

        if ( aKey->mBCBs[j].mType == SDB_MRPBCB_TYPE_NEWFETCH )
        {
            if( ( sFixedBCB->mFixCnt == 1 ) &&
                ( ( sFixedBCB->mState == SDB_BCB_CLEAN ) || ( sFixedBCB->mState == SDB_BCB_FREE ) ) )
            {
                /* �ش� BCB�� �� �Լ�( fetchPagesByMPR )�� ���ؼ� ó�� ���ۿ� �ö�� 
                 * ��� �ؽÿ��� �����Ѵ�. */
                sFixedBCB->decFixCnt();
                sFixedBCB->setToFree();
                sFixedBCB->unlockBCBMutex();

                mHashTable.removeBCB( sFixedBCB );

                /* BCB�� ��Ȱ���� �� �ֵ��� freeBCB�� �����Ѵ�. */
                aKey->addFreeBCB( sFixedBCB );

            }
            else
            {
                /* �ڽŸ� ���� ���� �ʰų� dirty�� ��� �ؽÿ��� �������� �ʰ� LRU�� �����Ѵ�. */
                sFixedBCB->decFixCnt();
                sFixedBCB->unlockBCBMutex();
                mLRUList[getLRUListIdx((UInt)sFixedBCB->mPageID)].insertBCB2BehindMid(
                    aStatistics,
                    sFixedBCB );
            }
        }
        else
        {
            /* ������ �̹� ���ۿ� �ö�� �ִ� BCB��� fix count�� ���ҽ�Ų��. */
            sFixedBCB->decFixCnt();
            sFixedBCB->unlockBCBMutex();
        }

        mHashTable.unlockHashChainsLatch( sLatchHandle );
    }

    return IDE_FAILURE;
}


/****************************************************************
 * Description:
 *  sdbMPRKey�� readUnit �� ���� �ѹ��� �о�� ���� �ƴϸ�
 *  �������� �о�� ������ �����Ѵ�.
 *
 *  aKey     - [IN]  MPR Key
 ****************************************************************/
idBool sdbBufferPool::analyzeCostToReadAtOnce(sdbMPRKey *aKey)
{
    if( aKey->mReadUnitCount == 1 )
    {
        return ID_TRUE;
    }
    else
    {
        return ID_FALSE;
    }
}

/****************************************************************
 * Description:
 *  �������� �޸� ������ �о���� ������ ���� BCB�� ����Ű�� �ִ� mFrame������
 *  memcpy�� �����Ѵ�.
 *  ���縦 �����ߴٸ�, BCB�� �ɾ���� ������ x��ġ�� �����Ѵ�.
 *
 *  aStatistics - [IN]  �������
 *  aSpaceID    - [IN]  table space ID
 *  aReadUnit   - [IN]  copyToFrame�� ����, �ϳ��� readUnit�� �����ִ�
 *                      ������ ��ŭ ���縦 �����Ѵ�. BCB ���������� �� ������
 *                      ������ ����ִ�.
 *  aIOBPtr     - [IN]  ���� �����Ͱ� ����ִ� �������� �޸� ����.
 *                      ������ ������ mFrame������ �����Ѵ�.
 ****************************************************************/
IDE_RC sdbBufferPool::copyToFrame(
    idvSQL                 *aStatistics,
    scSpaceID               aSpaceID,
    sdbReadUnit            *aReadUnit,
    UChar                  *aIOBPtr )
{
    sdbBCB  *sBCB = NULL;
    UInt     i    = 0;
    UInt            sCorruptPageReadCnt = 0;
    scSpaceID       sCurruptSpaceID     = SC_NULL_SPACEID;
    scPageID        sCurruptPageID      = SC_NULL_PID;

    for ( i = 0;
          i < aReadUnit->mReadBCBCount;
          i++ )
    {
        sBCB = aReadUnit->mReadBCB[i].mBCB;

        idlOS::memcpy(sBCB->mFrame,
                      aIOBPtr + i * mPageSize,
                      mPageSize);

        if ( smLayerCallback::checkAndSetPageCorrupted( aSpaceID,
                                                        sBCB->mFrame ) == ID_TRUE )
        {
            if( ( smuProperty::getCorruptPageErrPolicy()
              & SDR_CORRUPT_PAGE_ERR_POLICY_SERVERFATAL )
            != SDR_CORRUPT_PAGE_ERR_POLICY_SERVERFATAL )
            {
                // BUG-45598: copyToFrame @ page corrupt ��Ȳ ó��
                sBCB->mPageReadError = ID_TRUE;
                sCurruptSpaceID = sBCB->mSpaceID;
                sCurruptPageID  = sBCB->mPageID;
                sCorruptPageReadCnt++;
            }
            else
            {
                sCurruptSpaceID = sBCB->mSpaceID;
                sCurruptPageID  = sBCB->mPageID;
                IDE_RAISE( page_corruption_error );
            }

        }

        setFrameInfoAfterReadPage( sBCB,
                                   ID_FALSE );  // check to online tablespace
        
        /* BUG-22341: TC/Server/qp4/Project1/PROJ-1502-PDT/Design/DELETE/RangePartTable.sql
         * ����� ���� ���:
         *
         * sBCB->mPageType�� setFrameInfoAfterReadPage���� �ʱ�ȭ �˴ϴ�. */
        mStatistics.applyReadPages( aStatistics ,
                                    sBCB->mSpaceID,
                                    sBCB->mPageID,
                                    sBCB->mPageType );

        sBCB->mReadyToRead = ID_TRUE;
        sBCB->mReadIOMutex.unlock();
        sBCB->unlockPageLatch();
    }

    // BUG-45598: corrupt �������� ������ ����
    IDE_TEST_RAISE( ( sCorruptPageReadCnt != 0 )
                    &&( smuProperty::getCrashTolerance() == 0 ),  page_corruption_error );

    return IDE_SUCCESS;

    IDE_EXCEPTION( page_corruption_error );
    {
        switch( smLayerCallback::getCorruptPageReadPolicy() )
        {
            case SDB_CORRUPTED_PAGE_READ_FATAL :
                IDE_SET( ideSetErrorCode( smERR_FATAL_PageCorrupted,
                                          sCurruptSpaceID,
                                          sCurruptPageID));
                break;
            case SDB_CORRUPTED_PAGE_READ_ABORT :
                IDE_SET( ideSetErrorCode( smERR_ABORT_PageCorrupted,
                                          sCurruptSpaceID,
                                          sCurruptPageID));              
                break;
            default:
                break;
        }
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/****************************************************************
 * Description:
 *  MPR�� ����� �Ǵ� �������� ��ũ�κ��� �о� ���̴� �Լ�.
 *  �̶�, �ѹ��� �д� ���� �ƴ϶� �������� ���ļ� �д´�.
 *  �д� ������ sdbMPRKey�� mReadUnit�̴�.
 *  mReadUnit���� � pid���� ��� �������� �о� �;� �ϴ��� ������ ������
 *  �ִ�.
 *
 *  aStatistics - [IN]  �������
 *  aKey        - [IN]  MPR Key
 ****************************************************************/
IDE_RC sdbBufferPool::fetchMultiPagesNormal(
    idvSQL                 * aStatistics,
    sdbMPRKey              * aKey )
{
    SInt i;
    sdbReadUnit *sReadUnit;
    idvTime      sBeginTime;
    idvTime      sEndTime;
    ULong        sReadTime         = 0;
    ULong        sCalcChecksumTime = 0;
    UInt         sReadPageCount    = 0;

    for (i = 0; i < aKey->mReadUnitCount; i++)
    {
        sReadUnit = &(aKey->mReadUnit[i]);

        IDV_TIME_GET(&sBeginTime);

        if( sReadUnit->mReadBCBCount == 1 )
        {
            mStatistics.applyBeforeSingleReadPage( aStatistics );
        }
        else
        {
            mStatistics.applyBeforeMultiReadPages( aStatistics );
        }

        IDE_TEST(sddDiskMgr::read(aStatistics,
                                  aKey->mSpaceID,
                                  sReadUnit->mFirstPID,
                                  sReadUnit->mReadBCBCount,
                                  aKey->mReadIOB)
                 != IDE_SUCCESS);

        if( sReadUnit->mReadBCBCount == 1 )
        {
            mStatistics.applyAfterSingleReadPage( aStatistics );
        }
        else
        {
            mStatistics.applyAfterMultiReadPages( aStatistics );
        }

        IDV_TIME_GET(&sEndTime);
        sReadTime += IDV_TIME_DIFF_MICRO(&sBeginTime, &sEndTime);

        sBeginTime = sEndTime;
        IDE_TEST( copyToFrame( aStatistics,
                               aKey->mSpaceID,
                               sReadUnit,
                               aKey->mReadIOB )
                  != IDE_SUCCESS );
        IDV_TIME_GET(&sEndTime);
        sCalcChecksumTime += IDV_TIME_DIFF_MICRO(&sBeginTime, &sEndTime);

        sReadPageCount += sReadUnit->mReadBCBCount;
    }

    /* BUG-32670    [sm-disk-resource] add IO Stat information 
     * for analyzing storage performance.
     * MPR�� �̿��� Fullscan, ReadPage�� �ɸ� �ð� ������.
     * Checksum�� ���� Checksum �� Frame�� �����ϴ� �ð��� �ɸ� */
    mStatistics.applyReadByMPR( sCalcChecksumTime,
                                sReadTime,
                                sReadPageCount );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}



/****************************************************************
 * Description:
 *  MPR�� ����� �Ǵ� �������� ��ũ�κ��� �о� ���̴� �Լ�.
 *  �̶�, MPR�� ����� �Ǵ� ��� �������� �ѹ��� �о���δ�.
 *  �ѹ��� �о� �鿴�� ������, �̹� ���ۿ� �����ϴ� ������ ���õ� �о� ���δ�.
 *  ������, �̰��� ������ �׳� ������ ������.
 *
 *  aStatistics - [IN]  �������
 *  aStartPID   - [IN]  �о���� �������� ��  ó�� ������ id
 *  aPageCount  - [IN]  �о���� ������ ����
 *  aKey        - [IN]  MPR Key
 ****************************************************************/
IDE_RC sdbBufferPool::fetchMultiPagesAtOnce(
    idvSQL                 *aStatistics,
    scPageID                aStartPID,
    UInt                    aPageCount,
    sdbMPRKey              *aKey )
{
    UInt         sIndex;
    SInt         i;
    sdbReadUnit *sReadUnit;
    idvTime      sBeginTime;
    idvTime      sEndTime;
    ULong        sReadTime;
    ULong        sCalcChecksumTime;
    
    IDV_TIME_GET(&sBeginTime);

    if( aKey->mReadUnit[0].mReadBCBCount == 1 )
    {
        mStatistics.applyBeforeSingleReadPage( aStatistics );
    }
    else
    {
        mStatistics.applyBeforeMultiReadPages( aStatistics );
    }

    IDE_TEST(sddDiskMgr::read(aStatistics,
                              aKey->mSpaceID,
                              aStartPID,
                              aPageCount,
                              aKey->mReadIOB)
             != IDE_SUCCESS);

    if( aKey->mReadUnit[0].mReadBCBCount == 1 )
    {
        mStatistics.applyAfterSingleReadPage( aStatistics );
    }
    else
    {
        mStatistics.applyAfterMultiReadPages( aStatistics );
    }

    IDV_TIME_GET(&sEndTime);
    sReadTime = IDV_TIME_DIFF_MICRO(&sBeginTime, &sEndTime);

    /* ���� ���ۿ� �������� �ʴ� �������鿡 ���ؼ��� frame���縦 �����Ѵ�. */
    sBeginTime = sEndTime;
    for (i = 0; i < aKey->mReadUnitCount; i++)
    {
        sReadUnit = &(aKey->mReadUnit[i]);
        sIndex = sReadUnit->mFirstPID - aStartPID;
        IDE_TEST( copyToFrame( aStatistics,
                               aKey->mSpaceID,
                               sReadUnit,
                               aKey->mReadIOB + sIndex * mPageSize )
                  != IDE_SUCCESS );
    }
    IDV_TIME_GET(&sEndTime);
    sCalcChecksumTime = IDV_TIME_DIFF_MICRO(&sBeginTime, &sEndTime);

    /* BUG-32670    [sm-disk-resource] add IO Stat information 
     * for analyzing storage performance.
     * MPR�� �̿��� Fullscan, ReadPage�� �ɸ� �ð� ������.
     * Checksum�� ���� Checksum �� Frame�� �����ϴ� �ð��� �ɸ� */
    mStatistics.applyReadByMPR( sCalcChecksumTime,
                                sReadTime,
                                aPageCount );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC sdbBufferPool::fetchSinglePage(
    idvSQL                 * aStatistics,
    scPageID                 aPageID,
    sdbMPRKey              * aKey )
{
    sdbBCB   *sBCB;
    SInt      sState = 0;
    idvTime   sBeginTime;
    idvTime   sEndTime;
    ULong     sReadTime;
    ULong     sCalcChecksumTime;
    UInt      sCorruptPageReadCnt = 0;

    sBCB = aKey->mReadUnit[0].mReadBCB[0].mBCB;

    IDV_TIME_GET(&sBeginTime);

    mStatistics.applyBeforeSingleReadPage( aStatistics );
    sState = 1;

    IDE_TEST(sddDiskMgr::read( aStatistics,
                               aKey->mSpaceID,
                               aPageID,
                               1,
                               sBCB->mFrame )
             != IDE_SUCCESS);

    sState = 0;
    mStatistics.applyAfterSingleReadPage( aStatistics );

    IDV_TIME_GET(&sEndTime);
    sReadTime = IDV_TIME_DIFF_MICRO(&sBeginTime, &sEndTime);
    sBeginTime = sEndTime;

    if ( smLayerCallback::checkAndSetPageCorrupted( aKey->mSpaceID,
                                                    sBCB->mFrame ) == ID_TRUE )
    {
        // BUG-45598: mpr �̿��Ͽ� ������ �ϳ� �� fetch �� ��� ���
        if( ( smuProperty::getCorruptPageErrPolicy()
              & SDR_CORRUPT_PAGE_ERR_POLICY_SERVERFATAL )
            != SDR_CORRUPT_PAGE_ERR_POLICY_SERVERFATAL )
        {
            sBCB->mPageReadError = ID_TRUE;
            sCorruptPageReadCnt++;
        }
        else
        {
            IDE_RAISE( page_corruption_error );
        }
    }

    setFrameInfoAfterReadPage( sBCB,
                               ID_FALSE ); // check to online tablespace

    mStatistics.applyReadPages( aStatistics ,
                                sBCB->mSpaceID,
                                sBCB->mPageID,
                                sBCB->mPageType );

    sBCB->mReadyToRead = ID_TRUE;
    sBCB->mReadIOMutex.unlock();
    sBCB->unlockPageLatch();

    IDV_TIME_GET(&sEndTime);
    sCalcChecksumTime = IDV_TIME_DIFF_MICRO(&sBeginTime, &sEndTime);

    /* BUG-32670    [sm-disk-resource] add IO Stat information 
     * for analyzing storage performance.
     * MPR�� �̿��� Fullscan, ReadPage�� �ɸ� �ð� ������.
     * MPR������, ���� �������� �ϳ� ���� ��� ȣ��Ǵµ�
     * �ٸ� ������������� '���������� �о����Ƿ�' single�� ������
     * ���⼭�� 'MPR�̳�'�� �����ϱ� ������, MPR�� �з��� */
    mStatistics.applyReadByMPR( sCalcChecksumTime,
                                sReadTime,
                                1 );

    // BUG-45598: corrupt �������� ������ ����( MPR�̿��Ͽ� Single ������ fetch �� ��� )
    IDE_TEST_RAISE(   ( sCorruptPageReadCnt != 0 )
                    &&( smuProperty::getCrashTolerance() == 0 ),  page_corruption_error );

    return IDE_SUCCESS;

    IDE_EXCEPTION( page_corruption_error );
    {
        switch( smLayerCallback::getCorruptPageReadPolicy() )
        {
            case SDB_CORRUPTED_PAGE_READ_FATAL :
                IDE_SET( ideSetErrorCode( smERR_FATAL_PageCorrupted,
                                      sBCB->mSpaceID,
                                      sBCB->mPageID ));

                break;
            case SDB_CORRUPTED_PAGE_READ_ABORT :
                IDE_SET( ideSetErrorCode( smERR_ABORT_PageCorrupted,
                                          sBCB->mSpaceID,
                                          sBCB->mPageID));
                break;
            default:
                break;
        }
    }
    IDE_EXCEPTION_END;

    if( sState != 0 )
    {
        mStatistics.applyAfterSingleReadPage( aStatistics );
    }

    return IDE_FAILURE;
}


/****************************************************************
 * Description:
 *  MPR�� ���� �о� �Դ� ���� �������� �ʱ�ȭ �Ѵ�.
 *  �̰��� fetchMultiPages�� ¦�� �̷�� �Լ��̴�. ��, fetchMultiPages��
 *  ����ǰ�, �̶� �о�Դ� �������鿡 ��� ������ �̷�� ���� �� ���Ŀ�
 *  cleanUpKey�Լ��� ȣ���ϰ�, �ٽ� fetchMultiPages�� ȣ���Ѵ�.
 *
 * �� �Լ����� �ϴ���:
 *  MPR�� �⺻������ full scan�� ���ؼ� ����� ���� ������, �ڽ��� �ѹ� �о��
 *  �������� �ѵ��� ������ ���� �Ŷ�� �����Ͽ��� ����� ����.(MRU�˰���)
 *  �׷��� ������ cleanUpKey��� �Լ��� �ʿ��ѵ�, �� �Լ������� MPR�� ����
 *  �о�� BCB���� ������ fetchMultiPages�� ���� free���·� ����� ��Ȱ�� �Ѵ�.
 *  �̶�, ��� BCB�� free�� ������ �ʴ´�. ���� ���ÿ� �ٸ� Ʈ����ǿ� ���ؼ�
 *  ���ٵ� BCB���� �����Ѵٸ�, �̰��� �����ε� �ѵ��� ���ٵ� ������ �Ǵ��ϰ�,
 *  hash Table�� �״�� ���ܵд�.(���۸Ŵ����� LRU����Ʈ�� ����)
 *  ���� �׷��� �ʴٸ�, free�� �����, ���� fetchMultiPages�� ���� ��������.
 *
 *  ���ÿ� �ٸ�Ʈ����ǿ� ���ؼ� ���ٵǾ����� ���δ� ���� touch count�θ� �Ѵ�.
 *
 *  aStatistics - [IN]  �������
 *  aKey        - [IN]  MPR key
 ****************************************************************/
void sdbBufferPool::cleanUpKey( idvSQL    *aStatistics,
                                sdbMPRKey *aKey,
                                idBool     aCachePage )
{
    SInt                      i;
    sdbBCB                   *sBCB;
    sdbHashChainsLatchHandle *sLatchHandle;
    sdsBCB                   *sSBCB;

    IDE_ASSERT(aKey->mState == SDB_MPR_KEY_FETCHED);

    for ( i = 0; i < aKey->mIOBPageCount ; i++ )
    {
        sBCB = aKey->mBCBs[i].mBCB;

        if ( aKey->mBCBs[i].mType == SDB_MRPBCB_TYPE_NEWFETCH )
        {
            //  ���� ���⼭ �̸� �ؽÿ��� ������ �ʿ�� ���� ���δ�.
            //  �ϴ��� �ؽÿ� �޷� �ִ� ä�� MPRKey�� mFreeBCB�� �����ϰ�,
            //  ���߿� removeLastFreeBCB���� �����ϸ鼭 hash���� �����ϸ�
            //  hit���� �� ���� ������ ��ģ��. ������, �� ȿ���� �ſ� ����������
            //  �����ȴ�.
            sLatchHandle = mHashTable.lockHashChainsXLatch( aStatistics,
                                                            sBCB->mSpaceID,
                                                            sBCB->mPageID );
            sBCB->lockBCBMutex( aStatistics );
            sBCB->decFixCnt();

            if( ( sBCB->mTouchCnt <= 1 )             &&
                ( sBCB->mFixCnt   == 0 )             &&
                ( sBCB->mState    == SDB_BCB_CLEAN ) &&
                ( aCachePage      == ID_FALSE ) )
            {
                //BUGBUG: ������ hash x chains latch�� ���� ��µ�,
                // �̺κп��� ��Ҵ� Ǯ� ����մϴ�.
                mHashTable.removeBCB( sBCB );

                mHashTable.unlockHashChainsLatch( sLatchHandle );
                sLatchHandle = NULL;

                /* ����� SBCB�� �ִٸ� delink */
                sSBCB = sBCB->mSBCB;
                if( sSBCB != NULL )
                {
                    sSBCB->mBCB = NULL;
                    sBCB->mSBCB = NULL;
                }

                sBCB->setToFree();
                sBCB->unlockBCBMutex();

                aKey->addFreeBCB( sBCB );
            }
            else
            {
                mHashTable.unlockHashChainsLatch( sLatchHandle );
                sLatchHandle = NULL;
                sBCB->unlockBCBMutex();
                mLRUList[getLRUListIdx((UInt)sBCB->mPageID)].insertBCB2BehindMid(
                                                                        aStatistics,
                                                                        sBCB );
                // BUGBUG: ���⼭ ������� ������ �ʿ�.
                // ���� ���⼭ hash Table�� �����ϴ� BCB�� ���ٸ�, �׸���,
                // �׵��� �ѵ��� ���ٵ��� ���� BCB���̶��, ����� ����
                // �˰���( mFixCnt, mTouchCnt)�� �Ǵ����� ���ƾ� �Ѵ�.
                // �׷� ���? TouchTimeInterval���� ������ �� �ְ�, �ƴϸ�,
                // mFixCnt������ �Ǵ� �� ���� �ִ�.
            }
        }
        else
        {
            // ���� �������ִ� �����̴�.
            sBCB->lockBCBMutex(aStatistics);
            sBCB->decFixCnt();
            sBCB->unlockBCBMutex();
        }
    }

    aKey->mState = SDB_MPR_KEY_CLEAN;
}

/****************************************************************
 * Description :
 *  page ptr�� ���� BCB �� ã�� �������� unfix�Ѵ�..
 *  fixPage - unfixPage�� ������ ȣ��ȴ�.
 *
 *  aStatistics - [IN]  �������
 *  aPagePtr    - [IN]  fixPage�� ������
 ****************************************************************/
void sdbBufferPool::unfixPage( idvSQL     *aStatistics,
                               UChar      *aPagePtr )
{
    sdbBCB *sBCB;
    UChar  *sPageBeginPtr = smLayerCallback::getPageStartPtr( aPagePtr );

    sBCB = (sdbBCB*)((sdbFrameHdr*)sPageBeginPtr)->mBCBPtr;
    unfixPage(aStatistics, sBCB);
}

/****************************************************************
 * Description :
 *  aBCB�� ����Ű�� �������� Dirty�� ��Ͻ�Ų��.
 *
 * ����! �� �Լ���
 *       �ݵ�� �������� ���� ù��° �α׸� ����ϱ� ����
 *       ȣ��Ǿ�� �Ѵ�. �׷��� ������ �������� �αװ� ��ũ��
 *       �������� ���� Dirty�� ��ϵ��� �������¿��� flush
 *       �� �߻��Ѵٸ� �������� �α� ���ķ� Restart Redo Point��
 *       ������ �� �ִ�.
 *
 *       �� �Լ��� ȣ��Ǵ� �������� ���� �������� ���� write ������
 *       ��� �����Ͽ���, �װͿ� �ش��ϴ� �α׸� ��� aMtx�� ������
 *       �ִ� �����̴�. �� �α׵��� �� �Լ� ���� �Ŀ� ���� LSN��
 *       ���� ��������, �׸��� ���� releasePage�� ȣ��ȴ�.
 *
 * Related Issue:
 *       BUG-19122 Restart Recovery LSN�� �߸� �����ɼ� �ֽ��ϴ�.
 *
 * aStatistics - [IN]  �������
 * aBCB        - [IN]  Dirty�� ��Ͻ�Ű���� �ϴ� BCB Pointer
 * aMtx        - [IN]  Mini Transaction Pointer
 *
 ****************************************************************/
void sdbBufferPool::setDirtyPage( idvSQL *aStatistics,
                                  void   *aBCB,
                                  void   *aMtx )
{
    sdbBCB     *sBCB;
    smLSN       sLstLSN;
    smLSN       sEndLSN;
    idBool      sIsMtxNoLogging;
    sdbBCBState sBCBState;

    sBCB = (sdbBCB*) aBCB;
    IDE_ASSERT( isFixedBCB(sBCB) == ID_TRUE );
    IDE_ASSERT( sBCB->mState    != SDB_BCB_FREE );

    /* BCB�� mMutex�� ���� ���� �����̱� ������, sBCB�� State��
     * ���� �� �ִ�. ���� ������ ���� �ʴ´ٸ�, �Ʒ��� if������
     * ����ε� �˻縦 �� �� ���� �ȴ�. */
    sBCBState = sBCB->mState;

    if( (sBCBState == SDB_BCB_DIRTY) ||
        (sBCBState == SDB_BCB_REDIRTY ))
    {
        /*
         * �� �Լ��� �ݵ�� ������ x��ġ�� ���� ���·� ���´�.
         * ��, ���� sBCB�� ���°� dirty�Ǵ� redirty���,
         * �̵��� ���´� ���� ������ �ʴ´�.
         * �ֳĸ�, dirty�Ǵ� redirty�� iniob�Ǵ� clean���� �����ϱ�
         * ���ؼ� �÷��Ű� ���� �������� �����ؾ� �ϴµ�,
         * ������ x��ġ�� ���� �����Ƿ�, ������ �� ����.
         */
        sBCBState = sBCB->mState;
        IDE_ASSERT( (sBCBState == SDB_BCB_DIRTY) ||
                    (sBCBState == SDB_BCB_REDIRTY));
    }
    else
    {
        IDU_FIT_POINT( "1.BUG-19122@sdbBufferPool::setDirtyPage" );

        /*BUG-27501 sdbBufferPool::setDirtyPage���� TempPage�� Dirty��Ű�� �Ҷ�,
         *aMtx�� Null�� ��찡 ������� ����
         *
         *  �Ϲ� �������� ��� �α��� �ִ��� �������� �������� Dirty���� ��������
         * �����մϴ�. ������ TempPage�� ��� ������ �α׸� ������� �ʱ� ������
         * ������ Dirty���� Flush�� �� �ֵ��� �ؾ� �մϴ�. */
        if ( smLayerCallback::isTempTableSpace( sBCB->mSpaceID ) )
        {
            /* Temporal Page�̱� ������ ��ϵ� �αװ� ���� redo��
             * �߻����� �ʴ´�.
             */
            SM_LSN_INIT( sBCB->mRecoveryLSN );

            setDirtyBCB(aStatistics, sBCB);

            IDE_CONT( RETURN_SUCCESS );
        }

        /*
         * PROJ-1704 MVCC Renewal
         * Mini Transaction�� ������� �ʴ� ���� End LSN�� �������� ����.
         * ����, Disk Last LSN�� Page LSN���� �����Ѵ�.
         */
        if( aMtx == NULL )
        {
            smLayerCallback::getLstLSN( &sLstLSN );
            IDE_ASSERT( !SM_IS_LSN_INIT( sLstLSN ));

            if( SM_IS_LSN_INIT( sBCB->mRecoveryLSN) )
            {
                sBCB->mRecoveryLSN = sLstLSN;
            }
            sdbBCB::setPageLSN( (sdbFrameHdr*)sBCB->mFrame, sLstLSN);

            setDirtyBCB(aStatistics, sBCB);
            IDE_CONT( RETURN_SUCCESS );
        }

        sIsMtxNoLogging = smLayerCallback::isMtxModeNoLogging( aMtx );

        if(sIsMtxNoLogging == ID_FALSE )
        {
            /* logging mode */
            /* aMtx�� �ڽ��� ������ �α׸� ���� write���� �ʰ�,
             * �ڽ��� ���ۿ� ������ �ִ´�.
             * */
            if ( smLayerCallback::isLogWritten( aMtx ) == ID_TRUE )
            {
                if ( SM_IS_LSN_INIT( sBCB->mRecoveryLSN) )
                {
                    smLayerCallback::getLstLSN( &(sBCB->mRecoveryLSN) );
                }

                aStatistics = (idvSQL*)smLayerCallback::getStatSQL( aMtx );

                setDirtyBCB(aStatistics, sBCB);
            }
            else
            {
                /*
                 * ������ �α׸� �������� �ʾҴٸ�, ��, ���� wirte�� ����
                 * �ʾҴٸ�, ���� dirty�� ������ �ʴ´�.
                 * */
            }
        }
        else
        {
            sEndLSN = smLayerCallback::getMtxEndLSN( aMtx );

            /* bugbug: loggging mode�� ��쿣 �ƹ��� setDirtyPage�� ȣ���Ͽ���
             * �ϴ���, ���� dirty�� �Ǿ����� ���θ� �� �� �ִ�. ��, �ڽ���
             * ����� �αװ� �����ϴ��� ���η� �̰��� �� �� �ִ�.
             * ������, no logging mode�� �������� ��쿣 �װ��� �� �� ����.
             * �ֳĸ� �αװ� �������� �ʱ� �����̴�.
             * ��, setDirtyPage�� ȣ���� temp page�� ��쿣, �װ��� ����
             * write �� �Ǿ����� ���ο� ������� ������ flush����� �ȴ�.
             */
            if ( smLayerCallback::isLSNZero( &sEndLSN ) == ID_TRUE )
            {
                // restart redo�� �ƴ� ���
                /* NOLOGGING mode */
                /* NOLOGGING mode�� release page�ÿ� temp page�� �ƴϴ���
                 * checkpoint list�� �����ؾ� �� ��찡 ����.
                 * NOLOGGING mode�� index�� build�� ��, ������ index page����
                 * checkpoint list�� �����Ѵ�.
                 *
                 * ����! index build�߿� disk�� �Ѱܳ� index page�� BCB����
                 * First modified LSN�� last modified LSN�� '0'�� �Ǳ� ������
                 * system�� �ֽ� LSN�� �̿��ؼ� BCB�� LSN�� ������ ��
                 * buffer checkpoint list�� �����Ѵ�.*/
                if ( smLayerCallback::isMtxNologgingPersistent( aMtx )
                     == ID_TRUE )
                {
                    smLayerCallback::getLstLSN( &sLstLSN );
                    IDE_ASSERT( !SM_IS_LSN_INIT( sLstLSN ));

                    /* no logging������ ���� disk�� ����� ������ persistent�ؾ�
                     * �Ѵ�. ��, recovery redo�� ���ؼ� ������ �������� �ʵ���
                     * �����ؾ� �Ѵ�. �׷��� ������ pageLSN�� mRecoveryLSN��
                     * �����Ѵ�. */
                    if( SM_IS_LSN_INIT( sBCB->mRecoveryLSN) )
                    {                    
                        sBCB->mRecoveryLSN = sLstLSN;
                    }

                    sdbBCB::setPageLSN( (sdbFrameHdr*)sBCB->mFrame, sLstLSN);

                    /* NOLOGGING mini-transaction�� update�� page�� persistency
                     * ���� 2���� option�� �ִ�.
                     * NOLOGGING index build�� ��� persistent�ؾ� �ϰ�
                     * �׿� �ٸ� transaction�� ��쿡 ���ؼ��� persistency��
                     * �䱸���� �ʴ´�.*/
                    IDE_ASSERT( smLayerCallback::isPageIndexOrIndexMetaType( sBCB->mFrame )
                                == ID_TRUE );
                }
                else
                {
                    /* ��ϵ� �αװ� ���� redo�� �߻����� �ʾҴٸ� TempPage�ε�
                     * TempPage�� ���, ���ʿ��� �ɷ����� ������ �̰����� �� ��
                     * ����. ���� �̰����� ���� ���� ���������� ����̴�.*/

                    /* BUG-24703 �ε��� �������� NOLOGGING���� X-LATCH�� ȹ���ϴ�
                     * ��찡 �ֽ��ϴ�. */
                    (void)sdbBCB::dump(sBCB);
                    IDE_ASSERT( 0 );
                }
            }
            else
            {
                /* recovery redo�ÿ� �̰����� ��
                 * bug-19742�����Ͽ�..
                 * �̰����� sBCB->mRecoveryLSN�� �����Ѵٸ�
                 * redo, undo�Ŀ� flush ALL�� ���� �ʾƵ��ȴ�.
                 */
                
                if( SM_IS_LSN_INIT( sBCB->mRecoveryLSN) )
                {                    
                    sBCB->mRecoveryLSN = sEndLSN;
                }
            }

            /* no logging mode���� setDirtyPage�� �����ϸ� ��� flush��
             * �����ؾ��Ѵ�. */
            aStatistics = (idvSQL*)smLayerCallback::getStatSQL( aMtx );

            setDirtyBCB(aStatistics, sBCB);
        }
    }
    
    IDE_EXCEPTION_CONT( RETURN_SUCCESS );
#ifdef ALTIBASE_FIT_CHECK
    IDE_EXCEPTION_END;
#endif
}


/****************************************************************
 * Description :
 *  BCB�� ���۳����� �����Ѵ�.  BCB�� �����Ѵٴ� ���� ���� �� �ؽ�
 *  ���̺��� �����Ѵٴ� ���̴�.  �̰��� �� ���°� free�� ����
 *  ���Ѵ�. �̶�, �ش� BCB�� dirty�� �Ǿ��ִٸ�, flush�� ��Ű�� �ʰ�
 *  free ���·� �����.
 *
 *  discardBCB�� ����, tableSpace drop�Ǵ� table drop�� ���� ���꿡 ���δ�.
 *  ����, �̷� ��쿡 discardBCB�� ������ �� BCB�� �ٽ� ���� ���� �ʾƾ� ����
 *  �� �� �ִ�. �ֳ�? drop�� table�� �� ������ �ϴ°�? �̹� ������ �����ε�..
 *  �̰��� �������� �����ؾ� �ϴµ�, �̰Ϳ� ���� üũ�� ģ���ϰԵ�
 *  ���� �Ŵ������� ���ش�.(assert�� ����)
 *
 *  aStatistics - [IN]  �������
 *  aBCB        - [IN]  discard�ϰ��� �ϴ� BCB,
 *  aFilter     - [IN]  discard������ �˻��ϱ� ���� �Լ�.
 *  aObj        - [IN]  discard������ �˻��Ҷ�, aFilter�Լ��� �ǳ��ִ�
 *                      ������.
 *  aIsWait     - [IN]  sBCB�� ���°� INIOB�Ǵ� REDIRTY�� ��쿡
 *                      �÷��ð� ������ ��ٸ��� ����.
 *  aIsSuccess  - [OUT] discardBCB �������θ� ����.
 *  aMakeFree   - [OUT] discardBCB�� ���� aBCB�� SDB_FREE_BCB�� ���������
 *                      ���θ� �����Ѵ�.
 ****************************************************************/
void sdbBufferPool::discardBCB( idvSQL                 *aStatistics,
                                sdbBCB                 *aBCB,
                                sdbFiltFunc             aFilter,
                                void                   *aObj,
                                idBool                  aIsWait,
                                idBool                 *aIsSuccess,
                                idBool                 *aMakeFree)
{
    idBool      sFiltOK;
    sdsBCB   * sSBCB;

    *aMakeFree = ID_FALSE;
    /* �Ʒ� �Լ��� �����ؼ� �ش� BCB�� SDB_BCB_CLEAN���� �����.
     * invalidateDirty�� �����ϰ� �� ���Ŀ� �ٽ� dirty�� �� �� ������
     * �������� �����ؾ� �Ѵ�.*/
    invalidateDirtyOfBCB( aStatistics,
                          aBCB,
                          aFilter,
                          aObj,
                          aIsWait,
                          aIsSuccess,
                          &sFiltOK); //aOK

    if( *aIsSuccess == ID_TRUE )
    {
        if( sFiltOK == ID_FALSE )
        {
            // aFiltOK�� ID_TRUE��� ���� Filter���ǿ� ���� �ʴ´ٴ� ���̴�.
            // ���� ���⼭ assert�� �ɸ��ٸ�, �������� ������ ���� ���Ѱ��̴�.
            // ��, ó������ discardBCB�� ������ �ƴϾ��µ�,
            // �� �Լ��� ����� ���� discardBCB�� ������ �������� ���ߴ� BCB��
            // discardBCB�� ������ �����ϰ� �Ȱ��̴�.
            // ���� �������� �̷��� ������ �ǵ��߰ų�, �̷� �����̹�����
            // �ȵɰ�쿡�� �Ʒ� assert���� �����ص� �������.
            IDE_DASSERT( aFilter( aBCB, aObj) == ID_FALSE);
        }
        else
        {
#ifdef DEBUG
            // �� �κ� ����, discardBCB�� ������ �����ϴ� BCB��
            // discardBCB������ �� ���Ŀ� ������ �� ��쿡 �Ʒ� �κп���
            // ���� �� �ִ�.
            // ���� �������� �̷��� ������ �ǵ��߰ų�, �̷� �����̹�����
            // �ȵɰ�쿡�� �Ʒ� assert���� �����ص� �������.
            // Secondary Buffer�� "ALL page"�� on ���� ������
            // discardBCB�� �����ϴ�  CLEAN ������ BCB��
            // discardBCB �����Ŀ� IOB�� ���� �ֱ⿡ �������.
            aBCB->lockBCBMutex(aStatistics);
            IDE_DASSERT( (aBCB->mState == SDB_BCB_CLEAN) ||
                         (aBCB->mState == SDB_BCB_INIOB) ||
                         (aBCB->isFree() == ID_TRUE ));
            aBCB->unlockBCBMutex();
#endif
            if ( aBCB->mState == SDB_BCB_CLEAN )
            {
                //����� SBCB�� �ִٸ� delink
                sSBCB = aBCB->mSBCB;
                if ( sSBCB != NULL )
                {
                    /* delink �۾��� lock���� ����ǹǷ�
                       victim �߻����� ���� ���� ������ ������ �ִ�.
                       pageID ���� �ٸ��ٸ� �̹� free �� ��Ȳ�ϼ� �ִ�. */
                    if ( (aBCB->mSpaceID == sSBCB->mSpaceID ) &&
                         (aBCB->mPageID == sSBCB->mPageID ) )
                    {
                        (void)sdsBufferMgr::removeBCB( aStatistics,
                                                       sSBCB );
                    }
                    else 
                    {
                        /* ���⼭ sSBCB �� �����ϴ� ������
                           sSBCB�� �ִ� discard�� �������� free �ϱ� �����ε�
                           pageID ���� �ٸ��ٸ� (victim ������ ����) �ٸ� �������̹Ƿ�
                           ��������� �ƴϴ�. */
                    }
                    aBCB->mSBCB = NULL;
                }
                else
                {
                    /* nothig to do */
                }
                *aMakeFree = makeFreeBCB( aStatistics, aBCB, aFilter, aObj );
            }
            else
            {
                /* nothing to do */
            }
        }
    }

    return;
}


/****************************************************************
 * Description :
 *  CLEAN �����̸鼭 fix�Ǿ� ���� �ʰ�, Ʈ����ǵ��� �������� �ʴ� BCB��
 *  free ���·� �����Ѵ�. free���¶� hash�� ���� BCB�� �ǹ��Ѵ�.
 *  ��, BCB�� ���ۿ��� �����ϴ� �Լ��̴�.
 *  aBCB�� ���� makeFreeBCB�� �����ߴ��� ���θ� �����Ѵ�. ��,
 *  makeFreeBCB�� �����Ͽ� SDB_FREE_BCB�� ������ٸ�, return ID_TRUE,
 *  makeFreeBCB�� �������� �ʾҴٸ�, return ID_FALSE
 *
 *  aStatistics     - [IN]  �������
 *  aBCB            - [IN]  free�� ���� BCB
 *  aFilter, aObj   - [IN]  BCB�� free�� ������ ���� �����ϱ� ���� �Լ���
 *                          �ʿ��� argument
 ****************************************************************/
idBool sdbBufferPool::makeFreeBCB(idvSQL     *aStatistics,
                                  sdbBCB     *aBCB,
                                  sdbFiltFunc aFilter,
                                  void       *aObj)
{
    scSpaceID    sSpaceID = aBCB->mSpaceID;
    scPageID     sPageID  = aBCB->mPageID;
    sdbHashChainsLatchHandle *sHashChainsHandle=NULL;
    idBool       sRet     = ID_FALSE;

    // �� �Լ��� tablespace offline, alter system flush buffer_pool ��ɿ�
    // ���� ����� �� �ִ�.
    // �� �Լ��� ����� �Ǵ� BCB�� ���ÿ� ������ �����ϱ� ������
    // fix�� �� ���� �ְ�, hot�� �� ���� �ִ� �� �������� ��Ȳ�� ����ؾ� �Ѵ�.

    // fix�Ǿ��ִ� BCB�� makeFree����� �Ǿ ����� �ȵȴ�.
    // ��, ���� ���ۿ��� ���ٵ��� �ʰų�, �̹� ������ ����
    // �������� ���ؼ��� makeFree�� ȣ���Ѵ�.
    if( isFixedBCB(aBCB) == ID_TRUE )
    {
        // nothing to do
    }
    else
    {
        /* BCB�� Free���¶�°��� �� hash���� ���ŵǾ� �ִ� �������� ���Ѵ�.
         * BCB�� Free���·� ����� ���� hash���� �����Ѵ�.
         */
        sHashChainsHandle = mHashTable.lockHashChainsXLatch( aStatistics,
                                                             sSpaceID,
                                                             sPageID );

        if( aBCB->isFree() == ID_FALSE )
        {
            /* BCB���� ������ ���ؼ� �ʼ������� BCBMutex�� ��ƾ� �Ѵ�.*/
            aBCB->lockBCBMutex( aStatistics );

            if( (aFilter( aBCB, aObj) == ID_TRUE) &&
                (aBCB->mState  == SDB_BCB_CLEAN) &&
                (isFixedBCB(aBCB) == ID_FALSE) )
            {
                mHashTable.removeBCB( aBCB );
                /* BCB�� free���·� �����, �̵��� ����Ʈ���� ���������� �ʴ´�.
                 * �̵��� �ᱹ Victim���� �����Ǿ� ������ ��� �� ���̴�.*/
                aBCB->makeFreeExceptListItem();
                sRet = ID_TRUE;
            }

            aBCB->unlockBCBMutex();
        }
        mHashTable.unlockHashChainsLatch( sHashChainsHandle );
        sHashChainsHandle = NULL;
    }
    return sRet;
}

/****************************************************************
 * Description :
 *  SDB_BCB_DIRTY �����̰�, aFilter���ǿ� �´´ٸ� aBCB��
 *  SDB_BCB_DIRTY ���¿��� SDB_BCB_CLEAN���·� �����Ѵ�.
 *  ���� SDB_BCB_REDIRTY �Ǵ� SDB_BCB_INIOB ���¶��,
 *  aIsWait�� ���� ��ٸ��� ���θ� �����Ѵ�. �̶��� invalidateDirty��
 *  �ٷ� ������ �� ����.
 *
 *  aStatistics - [IN]  �������
 *  aBCB        - [IN]  invalidateDirty�� ������ BCB
 *  aFilter,aObj- [IN]  ���� �˻縦 ���� �Ķ����
 *  aIsWait     - [IN]  aBCB�� ���°� SDB_BCB_REDIRTY �Ǵ� SDB_BCB_INIOB
 *                      �϶�, ���°� ����Ǳ⸦ ��ٸ��� ���� ����
 *  aIsSuccess  - [OUT] aIsWait�� ID_FALSE�϶�, aBCB�� ���°� SDB_BCB_REDIRTY
 *                      �Ǵ� SDB_BCB_INIOB �� ��� ID_FALSE�� �����Ѵ�.
 *  aIsFilterOK - [OUT] aFilter�� �����ϴ��� ���� ����
 *
 ****************************************************************/
void sdbBufferPool::invalidateDirtyOfBCB( idvSQL      *aStatistics,
                                          sdbBCB      *aBCB,
                                          sdbFiltFunc  aFilter,
                                          void        *aObj,
                                          idBool       aIsWait,
                                          idBool      *aIsSuccess,
                                          idBool      *aIsFilterOK )
{
    *aIsSuccess  = ID_FALSE;
    *aIsFilterOK = ID_FALSE;
retry:
    /* ���⼭ BCBMutex�� ��� ������ ���¸� �����Ű�� �ʰ� �ϱ� ���ؼ��̴�.
     * aBCB�� dirty�̰� mutex�� ���� ���¿����� �ٸ� pid�� ���������� ���ϰ�,
     * ���°� ��������� ���Ѵ�.
     */
    aBCB->lockBCBMutex( aStatistics );

    if ( isFixedBCB(aBCB) == ID_TRUE )
    {
        // BUG-20667 ���̺� �����̽��� drop���ε�, �� ���̺� �����̽���
        // �����ϴ� �����尡 �����մϴ�.
        ideLog::log(SM_TRC_LOG_LEVEL_BUFFER,
                    SM_TRC_BUFFER_POOL_FIXEDBCB_DURING_INVALIDATE,
                    (aFilter( aBCB, aObj ) == ID_TRUE ));
        (void)sdbBCB::dump(aBCB);
    }
    else
    {
        /* nothing to do */
    }

    /* �ش� aBCB�� �ٸ� ������ ���ߴ��� ���� �˻�.
     * �ϴ� aBCB Mutex�� ������ ����� �ٸ� PageID�� ������� �ʴ´�.*/
    if ( aFilter( aBCB, aObj) == ID_FALSE )
    {
        *aIsFilterOK = ID_FALSE;

        aBCB->unlockBCBMutex();
        *aIsSuccess = ID_TRUE;
    }
    else
    {
        *aIsFilterOK = ID_TRUE;

        if ( ( aBCB->mState == SDB_BCB_CLEAN ) ||
             ( aBCB->mState == SDB_BCB_FREE  ) )
        {
            aBCB->unlockBCBMutex();
            *aIsSuccess = ID_TRUE;
        }
        else
        {
            if ( (aBCB->mState == SDB_BCB_INIOB ) ||
                 (aBCB->mState == SDB_BCB_REDIRTY) ||
                 (isFixedBCB(aBCB) == ID_TRUE ))
            {
                aBCB->unlockBCBMutex();

                if ( aIsWait == ID_TRUE )
                {
                    idlOS::sleep( mInvalidateWaitTime );
                    goto retry;
                }
                else
                {
                    *aIsSuccess = ID_FALSE;
                }
            }
            else
            {
                IDE_ASSERT( aBCB->mState == SDB_BCB_DIRTY );
                IDE_ASSERT( aFilter( aBCB, aObj ) == ID_TRUE );

                // dirty�����̱� mMutex�� ��� �ֱ� ������ ����� ���°� ������
                // ������ ������ �� �ִ�.
                /* dirty�ΰ��� ���� checkpoint list���� ���Ÿ� �ϴµ�,
                 * �̶�, BCB�� mMutex�� Ǯ�� �ʴ´�.  �ֳĸ�, mMutex�� Ǫ�� ����,
                 * �ٸ� ���·� ����� �� �ֱ� �����̴�.
                 * ���� ���°� dirty�̶�� �̰��� ���� flusher�� ���� �ʾҴٴ�
                 * ���� �ȴ�. flusher�� aBCB�� ���¸� ���� ���Ŀ� BCB Mutex�� ���
                 * �ٽ� ���¸� Ȯ���ϱ� �����̴�.
                 *
                 * �׷��Ƿ� �������� ������ �� �ִ�.
                 **/
                if (!SM_IS_LSN_INIT(aBCB->mRecoveryLSN))
                {
                    // recovery LSN�� 0�̶� ���� temp page�̴�.
                    // temp page�� checkpoint list�� �޸��� �ʴ´�.
                    mCPListSet.remove( aStatistics, aBCB );
                }

                aBCB->clearDirty();

                aBCB->unlockBCBMutex();

                *aIsSuccess = ID_TRUE;
            }
        }
    }

    return;
}

/****************************************************************
 * Description :
 *  bufferPool�� BCB�� �����Ѵ�.
 *  prepare List�� ���̰� ���� ª�� ����Ʈ�� BCB����
 *
 *  aStatistics - [IN]  �������
 *  aBCB        - [IN]  BCB
 ****************************************************************/
IDE_RC sdbBufferPool::addBCB2PrepareLst(idvSQL *aStatistics,
                                        sdbBCB *aBCB)
{
    UInt            i;
    UInt            sMinLength  = ID_UINT_MAX;
    UInt            sLength;
    sdbPrepareList *sList;
    sdbPrepareList *sTargetList = NULL;
    static UInt     sStartPos   = 0;
    UInt            sListIndex;

    if (sStartPos >= mPrepareListCnt)
    {
        sStartPos = 0;
    }
    else
    {
        sStartPos++;
    }

    // sStartPos�� static �����̱� ������ ���� �����尡 ���� ����, ������ų ��
    // �ֱ� ������ mPrepareListCnt���� ū ��Ȳ�� �߻��� �� �־
    // mod �������� �׷� ���ɼ��� �����Ѵ�.

    // BUG-21289
    // ���� HP�� �Ϻ� ��񿡼� % ������ �ϴ� ���� ���ŵ� sStartPos�� �о�
    // %�� �������� �������� mPrepareListCnt���� ū ��찡 �߻��� ���(case-13531)�� �ִ�.
    // �̸� �����ϱ� ���� while������ ��õ �����Ѵ�.

    sListIndex = sStartPos;
    while (sListIndex >= mPrepareListCnt)
    {
        sListIndex = sListIndex % mPrepareListCnt;
    }

    for (i = 0; i < mPrepareListCnt; i++)
    {
        sList = &mPrepareList[sListIndex];
        if (sListIndex == mPrepareListCnt - 1)
        {
            sListIndex = 0;
        }
        else
        {
            sListIndex++;
        }

        if (sList->getWaitingClientCount() > 0)
        {
            sTargetList = sList;
            break;
        }

        sLength = sList->getLength();

        if (sMinLength > sLength)
        {
            sMinLength = sLength;
            sTargetList = sList;
        }
    }

    IDE_ASSERT(sTargetList != NULL);
    // prepare List�� ���̰� ���� ª�� ����Ʈ�� BCB����
    IDE_TEST(sTargetList->addBCBList(aStatistics,
                                     aBCB,
                                     aBCB,
                                     1)
             != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/****************************************************************
 * Description :
 *  aBCB�� �����ִ� ����Ʈ���� BCB�� �����Ѵ�.
 *  ����� LRUList������ ���Ű� �����ϰ�, LRUList�� �ƴ� ����Ʈ���� ���Ÿ�
 *  �õ��ϴ� ��쿣 ���Ű� �����ϰ�, ID_FALSE�� �����Ѵ�.
 *
 *  aStatistics - [IN]  �������
 *  aBCB        - [IN]  BCB
 ****************************************************************/
idBool sdbBufferPool::removeBCBFromList(idvSQL *aStatistics, sdbBCB *aBCB)
{
    idBool sRet;
    UInt   sLstIdx;

retry:
    sRet     = ID_FALSE;
    sLstIdx  = aBCB->mBCBListNo;

    switch( aBCB->mBCBListType )
    {
        case SDB_BCB_LRU_HOT:
        case SDB_BCB_LRU_COLD:
            if( sLstIdx >= mLRUListCnt )
            {
                /* BCB�� ó���� ȣ��Ǿ������� �޶�����.
                 * �ٽýõ��Ѵ�. segv ��������*/
                goto retry;
            }

            if( mLRUList[sLstIdx].removeBCB( aStatistics, aBCB) == ID_FALSE )
            {
                /* BCB�� ó���� ȣ��Ǿ������� �޶�����.
                 * �ٽýõ��Ѵ�.*/
                goto retry;
            }
            /* [BUG-20630] alter system flush buffer_pool�� �����Ͽ�����,
             * hot������ freeBCB��� ������ �ֽ��ϴ�.
             * ���������� ���Ÿ� �����Ͽ���.
             * */
            sRet = ID_TRUE;
            break;

        case SDB_BCB_PREPARE_LIST:
        case SDB_BCB_FLUSH_LIST:
        case SDB_BCB_DELAYED_FLUSH_LIST:
        default:
            break;
    }
    return sRet;
}
#ifdef DEBUG
IDE_RC sdbBufferPool::validate( sdbBCB *aBCB )
{
    IDE_DASSERT( aBCB != NULL );

    sdpPhyPageHdr   * sPhyPageHdr;
    smcTableHeader  * sTableHeader;
    smOID             sTableOID;

    sPhyPageHdr = sdpPhyPage::getHdr(aBCB->mFrame);

    if( sdpPhyPage::getPageType( sPhyPageHdr ) == SDP_PAGE_DATA )
    {
        if( sPhyPageHdr->mPageID != aBCB->mPageID )
        {
            ideLog::log( IDE_ERR_0,
                     "----------------- Page Dump -----------------\n"
                     "Page Start Pointer        : %"ID_xPOINTER_FMT"\n"
                     "FrameHdr.mCheckSum        : %"ID_UINT32_FMT"\n"
                     "FrameHdr.mPageLSN.mFileNo : %"ID_UINT32_FMT"\n"
                     "FrameHdr.mPageLSN.mOffset : %"ID_UINT32_FMT"\n"
                     "FrameHdr.mIndexSMONo      : %"ID_vULONG_FMT"\n"
                     "FrameHdr.mSpaceID         : %"ID_UINT32_FMT"\n"
                     "FrameHdr.mBCBPtr          : 0x%"ID_xPOINTER_FMT"\n"
                     "Total Free Size           : %"ID_UINT32_FMT"\n"
                     "Available Free Size       : %"ID_UINT32_FMT"\n"
                     "Logical Hdr Size          : %"ID_UINT32_FMT"\n"
                     "Size of CTL               : %"ID_UINT32_FMT"\n"
                     "Free Space Begin Offset   : %"ID_UINT32_FMT"\n"
                     "Free Space End Offset     : %"ID_UINT32_FMT"\n"
                     "Page Type                 : %"ID_UINT32_FMT"\n"
                     "Page State                : %"ID_UINT32_FMT"\n"
                     "Page ID                   : %"ID_UINT32_FMT"\n"
                     "is Consistent             : %"ID_UINT32_FMT"\n"
                     "LinkState                 : %"ID_UINT32_FMT"\n"
                     "ParentInfo.mParentPID     : %"ID_UINT32_FMT"\n"
                     "ParentInfo.mIdxInParent   : %"ID_UINT32_FMT"\n"
                     "Prev Page ID, Nex Page ID : %"ID_UINT32_FMT", %"ID_UINT32_FMT"\n"
                     "TableOID                  : %"ID_vULONG_FMT"\n"
                     "Seq No                    : %"ID_UINT64_FMT"\n"
                     "---------------------------------------------\n",
                     (UChar*)sPhyPageHdr,
                     sPhyPageHdr->mFrameHdr.mCheckSum,
                     sPhyPageHdr->mFrameHdr.mPageLSN.mFileNo,
                     sPhyPageHdr->mFrameHdr.mPageLSN.mOffset,
                     sPhyPageHdr->mFrameHdr.mIndexSMONo,
                     sPhyPageHdr->mFrameHdr.mSpaceID,
                     sPhyPageHdr->mFrameHdr.mBCBPtr,
                     sPhyPageHdr->mTotalFreeSize,
                     sPhyPageHdr->mAvailableFreeSize,
                     sPhyPageHdr->mLogicalHdrSize,
                     sPhyPageHdr->mSizeOfCTL,
                     sPhyPageHdr->mFreeSpaceBeginOffset,
                     sPhyPageHdr->mFreeSpaceEndOffset,
                     sPhyPageHdr->mPageType,
                     sPhyPageHdr->mPageState,
                     sPhyPageHdr->mPageID,
                     sPhyPageHdr->mIsConsistent,
                     sPhyPageHdr->mLinkState,
                     sPhyPageHdr->mParentInfo.mParentPID,
                     sPhyPageHdr->mParentInfo.mIdxInParent,
                     sPhyPageHdr->mListNode.mPrev,
                     sPhyPageHdr->mListNode.mNext,
                     sPhyPageHdr->mTableOID,
                     sPhyPageHdr->mSeqNo );
            IDE_DASSERT( 0 );
        }

        sTableOID = sdpPhyPage::getTableOID( (UChar*)sdpPhyPage::getHdr( aBCB->mFrame ));

        (void)smcTable::getTableHeaderFromOID( sTableOID,
                                               (void**)&sTableHeader );
        if( sTableOID != sTableHeader->mSelfOID )
        {
            ideLog::log( IDE_ERR_0,
                     "----------------- Page Dump -----------------\n"
                     "Page Start Pointer        : %"ID_xPOINTER_FMT"\n"
                     "FrameHdr.mCheckSum        : %"ID_UINT32_FMT"\n"
                     "FrameHdr.mPageLSN.mFileNo : %"ID_UINT32_FMT"\n"
                     "FrameHdr.mPageLSN.mOffset : %"ID_UINT32_FMT"\n"
                     "FrameHdr.mIndexSMONo      : %"ID_vULONG_FMT"\n"
                     "FrameHdr.mSpaceID         : %"ID_UINT32_FMT"\n"
                     "FrameHdr.mBCBPtr          : 0x%"ID_xPOINTER_FMT"\n"
                     "Total Free Size           : %"ID_UINT32_FMT"\n"
                     "Available Free Size       : %"ID_UINT32_FMT"\n"
                     "Logical Hdr Size          : %"ID_UINT32_FMT"\n"
                     "Size of CTL               : %"ID_UINT32_FMT"\n"
                     "Free Space Begin Offset   : %"ID_UINT32_FMT"\n"
                     "Free Space End Offset     : %"ID_UINT32_FMT"\n"
                     "Page Type                 : %"ID_UINT32_FMT"\n"
                     "Page State                : %"ID_UINT32_FMT"\n"
                     "Page ID                   : %"ID_UINT32_FMT"\n"
                     "is Consistent             : %"ID_UINT32_FMT"\n"
                     "LinkState                 : %"ID_UINT32_FMT"\n"
                     "ParentInfo.mParentPID     : %"ID_UINT32_FMT"\n"
                     "ParentInfo.mIdxInParent   : %"ID_UINT32_FMT"\n"
                     "Prev Page ID, Nex Page ID : %"ID_UINT32_FMT", %"ID_UINT32_FMT"\n"
                     "TableOID                  : %"ID_vULONG_FMT"\n"
                     "Seq No                    : %"ID_UINT64_FMT"\n"
                     "---------------------------------------------\n",
                     (UChar*)sPhyPageHdr,
                     sPhyPageHdr->mFrameHdr.mCheckSum,
                     sPhyPageHdr->mFrameHdr.mPageLSN.mFileNo,
                     sPhyPageHdr->mFrameHdr.mPageLSN.mOffset,
                     sPhyPageHdr->mFrameHdr.mIndexSMONo,
                     sPhyPageHdr->mFrameHdr.mSpaceID,
                     sPhyPageHdr->mFrameHdr.mBCBPtr,
                     sPhyPageHdr->mTotalFreeSize,
                     sPhyPageHdr->mAvailableFreeSize,
                     sPhyPageHdr->mLogicalHdrSize,
                     sPhyPageHdr->mSizeOfCTL,
                     sPhyPageHdr->mFreeSpaceBeginOffset,
                     sPhyPageHdr->mFreeSpaceEndOffset,
                     sPhyPageHdr->mPageType,
                     sPhyPageHdr->mPageState,
                     sPhyPageHdr->mPageID,
                     sPhyPageHdr->mIsConsistent,
                     sPhyPageHdr->mLinkState,
                     sPhyPageHdr->mParentInfo.mParentPID,
                     sPhyPageHdr->mParentInfo.mIdxInParent,
                     sPhyPageHdr->mListNode.mPrev,
                     sPhyPageHdr->mListNode.mNext,
                     sPhyPageHdr->mTableOID,
                     sPhyPageHdr->mSeqNo );
            IDE_DASSERT( 0 );
        }
    }
    return IDE_SUCCESS;
}
#endif
