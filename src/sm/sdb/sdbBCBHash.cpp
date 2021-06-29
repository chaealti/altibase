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

/****************************************************************
 * Description :
 *
 *
 * BCB Buffer Hash (PROJ-1568. SM - Buffer manager renewal)
 *
 * ��Ƽ���̽����� �������� ����ϴ� hash table�� ������� �ʰ�,
 * ���ο� hash�� ���� ���� buffer manager������ ����ϰ� ��.
 * 
 *
 * #Ư¡
 *      - �ؽ� ���̺� ũ�⸦ �������� ������ �� �ִ�.
 *      - ���� bucket�� �ϳ��� mutex�� ���ν��� ����� �� �ִ�.
 *      - �ؽ� ���̺��� ũ�⸦ �ּ������� �����Ѵ�.
 *      - BCB�� Ưȭ ���ױ� ������ �ӵ��� �� ����.
 * 
 *****************************************************************/

#include <smDef.h>
#include <sdbBCBHash.h>
#include <smErrorCode.h>



/***********************************************************************
 * Description :
 *  Ư�� ���ڸ� �Է����� �޾� �� ���� ���� ū 2�� ���� ���� �����Ѵ�.
 *  ��, �Է��� 3�̸� 4�� �����ϰ�, �Է��� 5�̸� 8�� �����Ѵ�.
 *
 *  aNum         - [IN] �Է� ����
 ***********************************************************************/
static UInt makeSqure2(UInt aNum)
{
    UInt i;
    UInt sNum = aNum;

    for (i = 1; sNum != 1; i++)
    {
        sNum >>= 1;
    }

    for (; i != 0; i--)
    {
        sNum <<= 1;
    }

    return sNum;
}

/***********************************************************************
 * Description:
 * 
 *  aBucketCnt         - [IN]  �ؽð� ������ �� bucket ����
 *  aBucketCntPerLatch - [IN]  �ϳ��� HashChainsLacth�� �����ϴ� bucket�� ����
 *  aType              - [IN]  BufferMgr or Secondary BufferMgr���� ȣ��Ǿ�����
 ***********************************************************************/ 
IDE_RC sdbBCBHash::initialize( UInt           aBucketCnt,
                               UInt           aBucketCntPerLatch,
                               sdLayerState   aType )
{
    SInt   sState = 0;
    UInt   i;
    SChar  sMutexName[128];

    /* mLatchMask ����
     * bucket�� �ش��ϴ� mutex�� ���Ҷ� ����ϴ� ������ �����Ѵ�.
     * mLatchArray[key & mLatchMask] �� ���ؼ� �ش� ��ġ�� ���Ѵ�. */
    IDE_ASSERT(aBucketCntPerLatch != 0);
    
    mBucketCnt = aBucketCnt;
    mLatchCnt  = (mBucketCnt + aBucketCntPerLatch - 1) / aBucketCntPerLatch;

    if (mLatchCnt == mBucketCnt)
    {
        /* �Ʒ��� ���� mask���� ���ϸ� ��Ŷ�� ���� ���� �ٷ� ���´�.
         * ��, bucket�� latch�� 1:1�� ���εȴ�. */
        mLatchMask = ID_UINT_MAX;
    }
    else
    {
        /* aLatchRatio�� �ݵ�� 1�Ǵ� 2�� ������� �Ѵ�.
         * 2�� ����� ��쿡 �Ʒ��� assert�� �ݵ�� ����ȴ�.
         *
         * aLatchRatio�� �ݵ�� 0010000 �̿� ���� �����̰�,( 1�� ���� �Ѱ� )
         * �� ������ 1�� ����    0001111 �� ���� �ǹǷ� ���ϴ� mask�� ���� �� �ִ�.
         * ���� ����, mask�� 0x0011�̰� key�� 0x1010�ΰ��,
         * key�� 0x1110, 0x0110, 0x0010�� �͵�� ���� latch�� ����Ѵ�. */
        mLatchCnt = makeSqure2(mLatchCnt);
        mLatchMask = mLatchCnt - 1;

        /*aLatchRatio�� �ݵ�� 2�� ���������� �Ѵ�.*/
        IDE_ASSERT((mLatchMask & mLatchCnt) == 0);
    }

    /* TC/FIT/Limit/sm/sdb/sdbBCBHash_initialize_malloc1.sql */
    IDU_FIT_POINT_RAISE( "sdbBCBHash::initialize::malloc1",
                          insufficient_memory );


    /* mTable ���� */
    /*BUG-30439  102GB �̻� BUFFER_AREA_SIZE�� �Ҵ��� ���, Mutex�Ҵ� ��� 
                      ������ �޸� �Ҵ��� �߸��� �� �ֽ��ϴ�. */
    IDE_TEST_RAISE(iduMemMgr::malloc(IDU_MEM_SM_SDB,
                                     (ULong)ID_SIZEOF( sdbBCBHashBucket ) 
                                         * (mBucketCnt),
                                     (void**)&mTable) != IDE_SUCCESS,
                   insufficient_memory );
    
    idlOS::memset(mTable,
                  0x00,
                  (size_t)ID_SIZEOF( sdbBCBHashBucket ) * (mBucketCnt));
    sState = 1;

    /* TC/FIT/Limit/sm/sdb/sdbBCBHash_initialize_malloc2.sql */
    IDU_FIT_POINT_RAISE( "sdbBCBHash::initialize::malloc2",
                          insufficient_memory );

    /* mMutexArray ���� */
    /*BUG-30439  102GB �̻� BUFFER_AREA_SIZE�� �Ҵ��� ���, Mutex�Ҵ� ��� 
                      ������ �޸� �Ҵ��� �߸��� �� �ֽ��ϴ�. */
    IDE_TEST_RAISE(iduMemMgr::malloc(IDU_MEM_SM_SDB,
                                     (ULong)ID_SIZEOF(iduLatch) 
                                         * mLatchCnt,
                                     (void**)&mMutexArray) != IDE_SUCCESS,
                   insufficient_memory );
    sState = 2;

    for (i = 0; i < mBucketCnt; i++)
    {
        SMU_LIST_INIT_BASE(&mTable[i].mBase);
        mTable[i].mHashBucketNo = i;
    }

    if( aType == SD_LAYER_BUFFER_POOL )
    {
        for (i = 0; i < mLatchCnt; i++)
        {
            idlOS::snprintf( sMutexName,
                             ID_SIZEOF(sMutexName), 
                             "HASH_LATCH_%"ID_UINT32_FMT, i);

            IDE_ASSERT( mMutexArray[i].initialize( sMutexName) 
                        == IDE_SUCCESS);
        }
    }
    else
    {
        for (i = 0; i < mLatchCnt; i++)
        {
            idlOS::snprintf( sMutexName, 
                             ID_SIZEOF(sMutexName), 
                             "SECONDARY_BUFFER_HASH_LATCH_%"ID_UINT32_FMT, i );

            IDE_ASSERT( mMutexArray[i].initialize( sMutexName) 
                        == IDE_SUCCESS);
        }
    }

    // [����]
    // �� ���� �Ʒ��� IDE_TEST ������ �߰��Ϸ���
    // EXCEPTION ó�� ��ƾ���� mMutexArray�� mLatchCnt��ŭ ���鼭 destroy�ؾ� �Ѵ�.

    return IDE_SUCCESS;

    IDE_EXCEPTION( insufficient_memory );
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_InsufficientMemory));
    }
    IDE_EXCEPTION_END;

    switch (sState)
    {
        case 2:
            IDE_ASSERT(iduMemMgr::free(mMutexArray) == IDE_SUCCESS);
        case 1:
            IDE_ASSERT(iduMemMgr::free(mTable) == IDE_SUCCESS);
        default:
            break;
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description :
 *  ���� �Լ�
 ***********************************************************************/
IDE_RC sdbBCBHash::destroy()
{
    UInt i;

    for (i = 0; i < mLatchCnt; i++)
    {
        IDE_TEST(mMutexArray[i].destroy() != IDE_SUCCESS);
    }
    IDE_TEST(iduMemMgr::free(mMutexArray) != IDE_SUCCESS);

    IDE_TEST(iduMemMgr::free(mTable) != IDE_SUCCESS);

    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description:
 *  BCB�� hash�� ����. �����ϴ� BCB�� ���� (pid,spaceID)�� ���� BCB�� �ִٸ� ��
 *  BCB�� ����
 *  
 *  aTargetBCB        - [IN]  ������ BCB
 *  aAlreadyExistBCB  - [OUT] ���� (pid, spaceID)�� ���� BCB�� �ִٸ�,
 *                              ������ �ϴ´�� �� BCB�� �����Ѵ�.
 ***********************************************************************/
void sdbBCBHash::insertBCB( void  * aTargetBCB,
                            void ** aAlreadyExistBCB )
{
    sdBCB              *sBCB;
    smuList            *sListNode;
    smuList            *sBaseNode;
    sdbBCBHashBucket   *sBucket;
    scSpaceID           sSpaceID;
    scPageID            sPID;

    IDE_DASSERT( aTargetBCB != NULL );

    sSpaceID  = SD_GET_BCB_SPACEID( aTargetBCB );
    sPID      = SD_GET_BCB_PAGEID( aTargetBCB );
    sBucket   = &( mTable[hash(sSpaceID, sPID)] );
    sBaseNode = &sBucket->mBase;    
    sListNode = SMU_LIST_GET_FIRST( sBaseNode );
    
    while (sListNode != sBaseNode)
    {
        sBCB = (sdBCB*)sListNode->mData;
        if ((sBCB->mPageID == sPID) && (sBCB->mSpaceID == sSpaceID))
        {
            /*�̹� ���� pid�� ���� BCB�� hash�� �����ϴ� ���*/
            break;
        }
        else
        {
            sListNode = SMU_LIST_GET_NEXT( sListNode );
        }
    }

    if( sListNode != sBaseNode )
    {
        *aAlreadyExistBCB = sBCB;
    }
    else
    {
        *aAlreadyExistBCB = NULL;
        SMU_LIST_ADD_FIRST( sBaseNode, 
                            SD_GET_BCB_HASHITEM( aTargetBCB ) );
        sBucket->mLength++;
    }

    SD_GET_BCB_HASHBUCKETNO( aTargetBCB ) = sBucket->mHashBucketNo;
}

/***********************************************************************
 * Description:
 *  BCB�� hash���� �����Ѵ�. �ݵ�� ������ BCB�� hash�� �����ؾ� �Ѵ�. �׷���
 *  ������ ���� ���
 *
 * aTargetBCB   - [IN]  ������ BCB  
 ***********************************************************************/
void sdbBCBHash::removeBCB( void *aTargetBCB )
{
    sdbBCBHashBucket  * sBucket;
    scSpaceID           sSpaceID;
    scPageID            sPageID;

    IDE_DASSERT( aTargetBCB != NULL );

    sSpaceID  = SD_GET_BCB_SPACEID( aTargetBCB );
    sPageID   = SD_GET_BCB_PAGEID( aTargetBCB );
 
    SMU_LIST_DELETE( SD_GET_BCB_HASHITEM( aTargetBCB ) );
    SDB_INIT_HASH_CHAIN_LIST( aTargetBCB );

    sBucket = &mTable[hash( sSpaceID, sPageID )];
    IDE_ASSERT( sBucket->mLength > 0 );
    sBucket->mLength--;

    SD_GET_BCB_HASHBUCKETNO( aTargetBCB ) = 0;
}

/***********************************************************************
 * Description:
 *  aSpaceID�� aPID�� �ش��ϴ� BCB�� ����� �����ϴ� �Լ�, Hash�� BCB�� ������
 *  ��쿣 �� BCB�� �����Ѵ�. ������ NULL�� ���� 
 *
 *  aSpaceID    - [IN]  table space ID
 *  aPID        - [IN]  page ID
 ***********************************************************************/
IDE_RC sdbBCBHash::findBCB( scSpaceID    aSpaceID,
                            scPageID     aPID,
                            void      ** aRetBCB )
{
    sdBCB             *sBCB = NULL;
    smuList           *sListNode;
    smuList           *sBaseNode;
    sdbBCBHashBucket  *sBucket;

    *aRetBCB  = NULL;
    sBucket   = &(mTable[hash( aSpaceID, aPID )]);
    sBaseNode = &sBucket->mBase;
    sListNode = SMU_LIST_GET_FIRST( sBaseNode );

    IDU_FIT_POINT_RAISE( "1.BUG-44102@sdbBCBHash::findBCB::invalidBCB",
                         ERR_INVALID_BCB );

    while ( sListNode != sBaseNode )
    {
        if ( sListNode != NULL )
        {
            sBCB = (sdBCB*)sListNode->mData;

            if ( sBCB != NULL )
            {
          
                if ( (sBCB->mPageID == aPID) && (sBCB->mSpaceID == aSpaceID) )
                {
                    break;
                }
                else
                {
                    sListNode = SMU_LIST_GET_NEXT(sListNode);
                }
            }
            else
            {
                IDE_RAISE( ERR_INVALID_BCB );
            }
        }
        else
        {
            IDE_RAISE( ERR_INVALID_BCB );
        }
    } //end of while

    if ( sListNode != sBaseNode )
    {
        *aRetBCB = sBCB;
    }
    else
    {
        *aRetBCB = NULL;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_BCB );
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_InvalidBCB, 
                                  aSpaceID, 
                                  aPID ) );
        
        ideLog::log( IDE_ERR_0,
                     SM_TRC_INVALID_BCB,
                     aSpaceID, 
                     aPID,
                     sListNode,
                     sBCB );

        if ( sListNode != NULL )
        {
           ideLog::log( IDE_ERR_0,
                        SM_TRC_INVALID_BCB_LIST,
                        sListNode,
                        SMU_LIST_GET_PREV(sListNode),
                        SMU_LIST_GET_NEXT(sListNode),
                        (sdbBCB*)sListNode->mData );
        }
        else
        {
            /* nothing to do */
            ideLog::log( IDE_ERR_0, "null ListNode" );
        }

        if ( sBCB != NULL )
        {
            sdbBCB::dump( sBCB );
        }
        else
        {
            ideLog::log( IDE_ERR_0, "null BCB" );
        }
    }
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

/***********************************************************************
 * Description:
 *  ���� ���� ��� BCB�� aNewHash�� �ű��.
 *  �� �Լ� ���� �� ���� hash�� �����ؼ��� �ȵȴ�.
 *
 *  aNewHash    - [IN]  �� hashTable�� ��� BCB�� �ű��.
 ***********************************************************************/
void sdbBCBHash::moveDataToNewHash(sdbBCBHash *aNewHash)
{
    sdbBCBHashBucket   * sWorkTable;
    sdbBCBHashBucket   * sBucket;
    sdBCB              * sAlreadyExistBCB;
    smuList            * sBaseNode;
    smuList            * sListNode;
    UInt                 i;

    /* BUG-20861 ���� hash resize�� �ϱ� ���ؼ� �ٸ� Ʈ����ǵ��� ��� ��������
     * ���ϰ� �ؾ� �մϴ�.
     * data move�߿� Ʈ������� �����Ѵٸ�, �߸��� ������ �� �� �����Ƿ�,
     * �̰��� ���� �Ͼ�� ���ƾ� �Ѵ�.
     * �׷��� ������ mTable�� NULL�� �ؼ�, �����ϴ� Ʈ������� �ִٸ�
     * ���׸�Ʈ ������ �߻��ϰ� �ؼ� ������ ���δ�.
     */
    sWorkTable = mTable;
    mTable = NULL;
    /* sWorkTable�� �� bucket������ move�� �����Ѵ�. �ѹ� move�� �����ϰ�
     * �� bucket���� ����� ������ �Ͼ �� ����.*/
    for (i = 0; i < mBucketCnt; i++)
    {
        sBucket   = &sWorkTable[i];
        sBaseNode = &sBucket->mBase;
        sListNode = SMU_LIST_GET_FIRST(sBaseNode);
 
        while (sListNode != sBaseNode)
        {
            SMU_LIST_DELETE(sListNode);
            sBucket->mLength--;
            SDB_INIT_HASH_CHAIN_LIST((sdBCB*)sListNode->mData);

            aNewHash->insertBCB( sListNode->mData,
                                 (void**)&sAlreadyExistBCB);

            IDE_ASSERT(sAlreadyExistBCB == NULL);

            sListNode = SMU_LIST_GET_FIRST(sBaseNode);
        }
    }
    mTable = sWorkTable;
    
    IDE_ASSERT( getBCBCount() == 0 );
}

/************************************************************************
 * Description: �ؽ����̺��� ũ�� �� latch�� bucket����
 ************************************************************************/
IDE_RC sdbBCBHash::resize( UInt aBucketCnt,
                           UInt aBucketCntPerLatch )
{
    sdbBCBHash sTempHash;

    IDE_TEST(sTempHash.initialize( aBucketCnt, 
                                   aBucketCntPerLatch, 
                                   SD_LAYER_BUFFER_POOL )
             != IDE_SUCCESS);

    moveDataToNewHash(&sTempHash);

    sdbBCBHash::exchangeHashContents(this, &sTempHash);

    IDE_TEST(sTempHash.destroy() != IDE_SUCCESS);
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}


/***********************************************************************
 * Description:     �� �ؽ� ���̺��� ������ �����Ѵ�.(swap)
 *  ����!! �ݵ�� BufferManager global x latch�� ���� ���¿��� �ҷ��� �Ѵ�.
 *  ��, �� Hash�� ���� ������ ���� ���� ���¿��� �� �Լ��� �ҷ��� �Ѵ�.
 *
 *  aHash1  - [IN]  ������ sdbBCBHash
 *  aHash2  - [IN]  ������ sdbBCBHash
 ***********************************************************************/
void sdbBCBHash::exchangeHashContents(sdbBCBHash *aHash1, sdbBCBHash *aHash2)
{
    UInt              sTempBucketCnt;
    UInt              sTempLatchCnt;
    UInt              sTempLatchMask;
    iduLatch         *sTempMutexArray;
    sdbBCBHashBucket *sTempTable;

    sTempBucketCnt       = aHash1->mBucketCnt;
    aHash1->mBucketCnt   = aHash2->mBucketCnt;
    aHash2->mBucketCnt   = sTempBucketCnt;

    sTempLatchCnt        = aHash1->mLatchCnt;
    aHash1->mLatchCnt    = aHash2->mLatchCnt;
    aHash2->mLatchCnt    = sTempLatchCnt;
    
    sTempLatchMask       = aHash1->mLatchMask;
    aHash1->mLatchMask   = aHash2->mLatchMask;
    aHash2->mLatchMask   = sTempLatchMask;
    
    sTempMutexArray      = aHash1->mMutexArray;
    aHash1->mMutexArray  = aHash2->mMutexArray;
    aHash2->mMutexArray  = sTempMutexArray;

    sTempTable           = aHash1->mTable;
    aHash1->mTable       = aHash2->mTable;
    aHash2->mTable       = sTempTable;
}

/***********************************************************************
 * Description :
 *  hash table���� �����ϴ� ��� BCB���� �� ����. ��ġ�� ���� �����Ƿ�,
 *  ��Ȯ�� ���� �ƴϴ�.
 ***********************************************************************/
UInt sdbBCBHash::getBCBCount()
{
    UInt sRet = 0;
    UInt i;

    for (i = 0; i < mBucketCnt; i++)
    {
        sRet += mTable[i].mLength;
    }

    return sRet;
}

