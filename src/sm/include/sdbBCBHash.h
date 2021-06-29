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

#ifndef _O_SDB_BCB_HASH_H_
#define _O_SDB_BCB_HASH_H_ 1

#include <idl.h>
#include <idu.h>
#include <smDef.h>
#include <smuProperty.h>
#include <sdbBCB.h>
#include <smuList.h>

/***********************************************************************
 * Description :
 *  sdbBCBHash bucket�� �ڷᱸ��. ���� hash table��  �̰͵��� �迭�� �Ǿ��ִ�.
 *  Bucket���� ���� ����Ʈ�� �̿��� ���� hash key�� ���� �͵��� �����Ѵ�.
 *  �̰��� hash chain �̶�� �Ѵ�.
 ***********************************************************************/
typedef struct sdbBCBHashBucket
{
    /* hash chain�� ���� */
    UInt    mLength;
    /* hash chain�� mBase */
    smuList mBase;

    UInt    mHashBucketNo;
} sdbBCBHashBucket;

typedef iduLatch sdbHashChainsLatchHandle;

class sdbBCBHash
{
public:
    IDE_RC  initialize( UInt         aBucketCnt,
                        UInt         aBucketCntPerLatch,
                        sdLayerState aType );

    IDE_RC  destroy();

    /*���� hash�� ũ��� latch������ ����. �� ������ ����Ǵ� ���߿�
     *�ٸ� Ʈ����ǿ����� ���� hash�� ���� ������ ���.*/
    IDE_RC  resize( UInt aBucketCnt,
                    UInt aBucketCntPerLatch );

    void insertBCB( void   * aTargetBCB,
                    void  ** aAlreadyExistBCB );
    
    void removeBCB( void  * aTargetBCB );

    IDE_RC findBCB( scSpaceID    aSpaceID,
                    scPageID     aPID,
                    void      ** aRetBCB );

    inline sdbHashChainsLatchHandle *lockHashChainsSLatch( idvSQL    *aStatistics,
                                                           scSpaceID  aSpaceID,
                                                           scPageID   aPID );

    inline sdbHashChainsLatchHandle *lockHashChainsXLatch( idvSQL    *aStatistics,
                                                           scSpaceID  aSpaceID,
                                                           scPageID   aPID );

    inline void unlockHashChainsLatch( sdbHashChainsLatchHandle *aMutex );
    
    inline UInt getBucketCount();

    inline UInt getLatchCount();

    UInt getBCBCount();

private:
    inline UInt hash( scSpaceID aSpaceID,
                      scPageID  aPID );

    void moveDataToNewHash( sdbBCBHash *aNewHash );

    /*�� �Լ� �����߿� Ʈ������� hash�� �����ϸ� �ȵȴ�.
     *��, �� �Լ��� ȣ���ϱ� �� �ݵ�� BufferManager�� ��ġ�� x�� ���� �־�� �Ѵ�.*/
    static void exchangeHashContents( sdbBCBHash *aHash1, sdbBCBHash *aHash2 );

    /* hash chains latch�� ���� */
    UInt mLatchCnt;
    
    /* bucket�� ����, �� mTable�� �迭�� size */
    UInt mBucketCnt;
    
    /* 0x000111 �� ���� ���¸� ������.
     * bucket index ���� mLatchMask�� &(and) �����ϸ� latch index�� ���´�.  */
    UInt mLatchMask;
    
    /* ���� hash chains latch�� ���������� mutex�� �����Ǿ� �����Ѵ�.
     * �� hash chains latch�� �迭���·� ��Ƴ��� ���� mMutexArray�̴�.*/
    iduLatch      *mMutexArray;
    
    /* ���� BCB�� �����ϴ� ���̺�. sdbBCBHashBucket�� �迭���·�
     * �����Ǿ� �ִ�. */
    sdbBCBHashBucket *mTable;
};

/****************************************************************
 * Description:
 *      pageID�� �Է����� �޾Ƽ� key�� �����ϴ� �Լ�
 *
 * aSpaceID - [IN]  spaceID
 * aPID     - [IN]  pageID 
 *****************************************************************/
UInt sdbBCBHash::hash(scSpaceID aSpaceID,
                      scPageID  aPID)
{

    /* BUG-32750 [sm-disk-resource] The hash bucket numbers of BCB(Buffer
     * Control Block) are concentrated in low number.
     * ���� ( FID + SPACEID + FPID ) % HASHSIZE �� �� DataFile�� ũ�Ⱑ ����
     * ��� �������� HashValue�� ������ ������ �ִ�. �̿� ���� ������ ����
     * �����Ѵ�
     * ( ( SPACEID * PERMUTE1 +  FID ) * PERMUTE2 + PID ) % HASHSIZE
     * PERMUTE1�� Tablespace�� �ٸ��� ���� ������� ������ �Ǵ°�,
     * PERMUTE2�� Datafile FID�� �ٸ��� ���� ������� ������ �Ǵ°�,
     * �Դϴ�.
     * PERMUTE1�� Tablespace�� Datafile ��� �������� ���� ���� ���� �����ϸ�
     * PERMUTE2�� Datafile�� Page ��� �������� ���� ���� ���� �����մϴ�. */
    return ( ( aSpaceID * smuProperty::getBufferHashPermute1()
                    + SD_MAKE_FID(aPID) )
            * smuProperty::getBufferHashPermute2() + aPID ) % mBucketCnt ;
}

sdbHashChainsLatchHandle *sdbBCBHash::lockHashChainsSLatch(
        idvSQL    *aStatistics,
        scSpaceID  aSpaceID,
        scPageID   aPID)
{
    iduLatch *sMutex;

    sMutex = &mMutexArray[hash(aSpaceID, aPID) & mLatchMask];
    (void)sMutex->lockRead(aStatistics, NULL);
    return (sdbHashChainsLatchHandle*)sMutex;
}

sdbHashChainsLatchHandle *sdbBCBHash::lockHashChainsXLatch(
        idvSQL    *aStatistics,
        scSpaceID  aSpaceID,
        scPageID   aPID)
{
    iduLatch *sMutex;

    sMutex = &mMutexArray[hash(aSpaceID, aPID) & mLatchMask];
    (void)sMutex->lockWrite(aStatistics, NULL);
    return (sdbHashChainsLatchHandle*)sMutex;
}

void sdbBCBHash::unlockHashChainsLatch(sdbHashChainsLatchHandle *aMutex)
{
    (void)aMutex->unlock();
}

UInt sdbBCBHash::getBucketCount()
{
    return mBucketCnt;
}

UInt sdbBCBHash::getLatchCount()
{
    return mLatchCnt;
}

#endif // _O_SDB_BCB_HASH_H_
