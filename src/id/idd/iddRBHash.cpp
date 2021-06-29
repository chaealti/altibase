/** 
 *  Copyright (c) 1999~2017, Altibase Corp. and/or its affiliates. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License, version 3,
 *  as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
 
/*****************************************************************************
 * $Id:
 ****************************************************************************/

/*****************************************************************************
 *   NAME
 *     iddRBHash.cpp - Hash Table with Red-Black Tree
 *
 *   DESCRIPTION
 *     Hash implementation
 *
 *   MODIFIED   (04/07/2017)
 ****************************************************************************/

#include <iddRBHash.h>

iddRBHashLink   iddRBHash::mGlobalLink;
iduMutex        iddRBHash::mGlobalMutex;

/**
 * Hash�� �ʱ�ȭ�Ѵ�
 *
 * @param aName : RB Hash�� �̸�
 * @param aIndex : ���������� ����� �޸� �Ҵ� �ε���
 * @param aKeyLength : Ű ũ��(bytes)
 * @param aUseLatch : ��ġ ��� ����(���ü������)
 * @param aHashFunc : �ؽð� ������ �Լ�
 *                    SInt(const void* aKey) ����
 * @param aCompFunc : Ű �񱳿� �Լ�
 *                    SInt(const void* aKey1, const void* aKey2) �����̸�
 *                    aKey1 >  aKey2�̸� 1 �̻���,
 *                    aKey1 == aKey2�̸� 0 ��
 *                    aKey1 < aKey2�̸� -1 ���ϸ� �����ؾ� �Ѵ�.
 * @return IDE_SUCCESS
 *         IDE_FAILURE
 */
IDE_RC iddRBHash::initialize(const SChar*               aName,
                             const iduMemoryClientIndex aIndex,
                             const UInt                 aBucketCount,
                             const UInt                 aKeyLength,
                             const idBool               aUseLatch,
                             const iddHashFunc          aHashFunc,
                             const iddCompFunc          aCompFunc)
{
    UInt    i;
    UInt    j;
    SInt    sState = 0;

    mIndex          = aIndex;
    mBucketCount    = aBucketCount;
    mHashFunc       = aHashFunc;
    mOpened         = ID_FALSE;

    IDE_TEST( mMutex.initialize((SChar*)"IDD_HASH_MUTEX",
                                IDU_MUTEX_KIND_POSIX,
                                IDV_WAIT_INDEX_NULL)
            != IDE_SUCCESS );
    sState = 1;

    IDE_TEST( iduMemMgr::malloc(mIndex,
                                sizeof(bucket) * mBucketCount,
                                (void**)&mBuckets) != IDE_SUCCESS );
    sState = 2;

    for(i = 0; i < mBucketCount; i++)
    {
        new (mBuckets + i) bucket();
        IDE_TEST( mBuckets[i].initialize(aIndex, aKeyLength, aCompFunc, aUseLatch)
                  != IDE_SUCCESS );
    }

    sState = 3;

    IDE_TEST( mGlobalMutex.lock(NULL) != IDE_SUCCESS );
    mNext = &(mGlobalLink);
    mPrev = mGlobalLink.mPrev;
    mPrev->mNext = this;
    mNext->mPrev = this;
    IDE_TEST( mGlobalMutex.unlock() != IDE_SUCCESS );

    idlOS::strncpy(mName, aName, 32);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    switch(sState)
    {
    case 3:
        IDE_ASSERT( mGlobalMutex.unlock() == IDE_SUCCESS );
        /* fall through */
    case 2:
        for(j = 0; j < i; j++)
        {
            IDE_ASSERT( mBuckets[j].destroy() == IDE_SUCCESS );
        }
        /* fall through */

    case 1:
        IDE_ASSERT( iduMemMgr::free(mBuckets) == IDE_SUCCESS );
        /* fall through */

    case 0:
        IDE_ASSERT( mMutex.destroy() == IDE_SUCCESS );
        break;

    default:
        IDE_ASSERT(0);
        break;
    }

    return IDE_FAILURE;
}

/**
 * Hash�� ���� �����͸� ���׸� ����
 *
 * @return IDE_SUCCESS
 *         IDE_FAILURE
 */
IDE_RC iddRBHash::reset(void)
{
    UInt i;
    
    for(i = 0; i < mBucketCount; i++)
    {
        IDE_TEST( mBuckets[i].clear() != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

/**
 * Hash�� ���� �����͸� ���� �Ҵ���� ��� �޸𸮸� �����Ѵ�
 *
 * @return IDE_SUCCESS
 *         IDE_FAILURE
 */
IDE_RC iddRBHash::destroy(void)
{
    UInt i;
    SInt sState = 0;
    
    for(i = 0; i < mBucketCount; i++)
    {
        IDE_TEST( mBuckets[i].destroy() != IDE_SUCCESS );
    }
    IDE_TEST( iduMemMgr::free(mBuckets) != IDE_SUCCESS );
    IDE_TEST( mMutex.destroy() != IDE_SUCCESS );
    
    IDE_TEST( mGlobalMutex.lock(NULL) != IDE_SUCCESS );
    sState = 1;
    mPrev->mNext = mNext;
    mNext->mPrev = mPrev;
    IDE_TEST( mGlobalMutex.unlock() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    if(sState == 1)
    {
        IDE_ASSERT( mGlobalMutex.unlock() == IDE_SUCCESS );
    }
    return IDE_FAILURE;
}

/**
 * Hash Table�� Ű/�����͸� �߰��Ѵ�.
 *
 * @param aKey : Ű
 * @param aData : ���� ���ϰ� �ִ� ������
 * @return IDE_SUCCESS
 *         IDE_FAILURE �ߺ� ���� �ְų� �޸� �Ҵ翡 �������� ��
 */
IDE_RC iddRBHash::insert(const void* aKey, void* aData)
{
    UInt sBucketNo = hash(aKey);
    IDE_TEST( mBuckets[sBucketNo].insert(aKey, aData) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

/**
 * Hash Table���� aKey�� �˻��Ѵ�
 * �˻��� Ű�� �ش��ϴ� �����ʹ� aData�� ����ȴ�
 *
 * @param aKey : Ű
 * @param aData : �˻��� �����͸� ���� ������ �����Ͱ� ������ NULL�� ����ȴ�
 * @return IDE_SUCCESS : ���� ��� SUCCESS�� �����Ѵ�
 *         IDE_FAILURE : ��ġ ȹ�� ����
 */
IDE_RC iddRBHash::search(const void* aKey, void** aData)
{
    UInt sBucketNo = hash(aKey);
    IDE_TEST( mBuckets[sBucketNo].search(aKey, aData) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

/**
 * Hash Table���� aKey�� �ش��ϴ� �����͸� aNewData�� ��ü�Ѵ�
 * ������ ���� aOldData�� NULL�� �ƴ� �� �����Ѵ�
 *
 * @param aKey : Ű
 * @param aNewData : �� ������
 * @param aOldData : ���� �����͸� ������ ������ NULL�� �� �� �ִ�
 * @return IDE_SUCCESS ���� ã���� ��
 *         IDE_FAILURE ���� ���ų� ��ġ ȹ�� ����
 */
IDE_RC iddRBHash::update(const void* aKey, void* aData, void** aOldData)
{
    UInt sBucketNo = hash(aKey);
    IDE_TEST( mBuckets[sBucketNo].update(aKey, aData, aOldData) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

/**
 * Hash Table���� Ű�� �����͸� �����Ѵ�
 * ������ �����ʹ� aData�� ����ȴ�.
 *
 * @param aKey : Ű
 * @param aData : ������ �����͸� ���� ������. aKey�� ������ NULL�� ����ȴ�.
 * @return IDE_SUCCESS
 *         IDE_FAILURE �޸� ����, Ȥ�� ��ġ ȹ�濡 �������� ��
 */
IDE_RC iddRBHash::remove(const void* aKey, void** aData)
{
    UInt sBucketNo = hash(aKey);
    IDE_TEST( mBuckets[sBucketNo].remove(aKey, aData) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

/**
 * Hash Table�� ��ȸ�� �� �ֵ��� Ŀ���� ����
 * @return �׻� IDE_SUCCESS
 */
IDE_RC iddRBHash::open(void)
{
    IDE_ASSERT(mOpened == ID_FALSE);

    mIterator = begin();
    mOpened = ID_TRUE;

    return IDE_SUCCESS;
}

/**
 * Hash Table�� ��ȸ�� ��ġ�� Ŀ���� �ݴ´�
 * @return �׻� IDE_SUCCESS
 */
IDE_RC iddRBHash::close(void)
{
    IDE_ASSERT(mOpened == ID_TRUE);
    mOpened = ID_FALSE;
    return IDE_SUCCESS;
}

/**
 * ���� ���� �ִ� Ŀ���� Hash Table�� ���ΰ�?
 * @return ID_TRUE/ID_FALSE
 */
idBool iddRBHash::isEnd(void)
{
    IDE_ASSERT(mOpened == ID_TRUE);
    return (mIterator == end())? ID_TRUE:ID_FALSE;
}

/**
 * ���� ���� �ִ� Ŀ�� ��ġ�� �����͸� ��´�
 * @return Ŀ�� ��ġ�� ������
 */
void* iddRBHash::getCurNode(void)
{
    IDE_ASSERT(mOpened == ID_TRUE);
    return mIterator.getData();
}

/**
 * ���� ���� �ִ� Ŀ�� ���� ��ġ�� �����͸� ��´�
 * @return Ŀ�� ���� ��ġ�� ������
 */
void* iddRBHash::getNxtNode(void)
{
    IDE_ASSERT(mOpened == ID_TRUE);
    iterator sNext = mIterator;
    sNext++;
    return sNext.getData();
}

/**
 * Hash Table���� Ŀ�� ��ġ�� Ű�� �����͸� �����Ѵ�
 * ���� ��ġ�� �����͸� ���´�
 *
 * @param aNxtData : ���� ��ġ�� �����͸� ���� ������
 * @return IDE_SUCCESS
 *         IDE_FAILURE �޸� ����, Ȥ�� ��ġ ȹ�濡 �������� ��
 */
IDE_RC iddRBHash::delCurNode(void** aNxtData)
{
    IDE_ASSERT(mOpened == ID_TRUE);
    iterator sPrev = mIterator;
    mIterator++;

    IDE_TEST( mBuckets[sPrev.mCurrent].remove(sPrev.getKey()) != IDE_SUCCESS );
    if(aNxtData != NULL)
    {
        *aNxtData = mIterator.getData();
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

/**
 * Hash Table���� Ŀ�� ��ġ�� Ű�� �����͸� �����ϰ�
 * Ŀ���� �ϳ� �ڷ� �̵���Ų��
 * Ŀ���� �ִ� ��ġ�� �����͸� ���´�
 *
 * @param aData : �����͸� ���� ������
 * @return IDE_SUCCESS
 *         IDE_FAILURE �޸� ����, Ȥ�� ��ġ ȹ�濡 �������� ��
 */
IDE_RC iddRBHash::cutCurNode(void** aData)
{
    IDE_ASSERT(mOpened == ID_TRUE);
    iterator sPrev = mIterator;
    mIterator++;

    IDE_TEST( mBuckets[sPrev.mCurrent].remove(sPrev.getKey(), aData) != IDE_SUCCESS );
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

iddRBHash::iterator::iterator(iddRBHash* aHash, SInt aCurrent)
{
    mHash = aHash;
    mCurrent = aCurrent;

    if(mHash != NULL)
    {
        mTreeIterator = mHash->mBuckets[aCurrent].begin();
        while( mTreeIterator == mHash->mBuckets[mCurrent].end() )
        {
            mCurrent++;
            if(mCurrent == mHash->mBucketCount)
            {
                mHash = NULL;
                break;
            }
            else
            {
                mTreeIterator = mHash->mBuckets[mCurrent].begin();
            }
        }
    }
}

iddRBHash::iterator& iddRBHash::iterator::moveNext(void)
{
    if(mHash != NULL)
    {
        mTreeIterator++;

        while( mTreeIterator == mHash->mBuckets[mCurrent].end() )
        {
            mCurrent++;
            if(mCurrent == mHash->mBucketCount)
            {
                mHash = NULL;
                break;
            }
            else
            {
                mTreeIterator = mHash->mBuckets[mCurrent].begin();
            }
        }
    }

    return *this;
}

const iddRBHash::iterator& iddRBHash::iterator::operator=(
        const iddRBHash::iterator& aIter)
{
    mHash = aIter.mHash;
    mCurrent = aIter.mCurrent;
    mTreeIterator = aIter.mTreeIterator;

    return *this;
}

bool iddRBHash::iterator::operator==(const iterator& aIter)
{
    return ((mHash==NULL) && (aIter.mHash == NULL))
        ||
        ((mHash == aIter.mHash) &&
         (mCurrent == aIter.mCurrent) &&
         (mTreeIterator == aIter.mTreeIterator));
}

IDE_RC iddRBHash::initializeStatic(void)
{
    IDE_TEST( mGlobalMutex.initialize((SChar*)"IDD_HASH_GLOBAL_MUTEX",
                                      IDU_MUTEX_KIND_POSIX,
                                      IDV_WAIT_INDEX_NULL)
            != IDE_SUCCESS );

    mGlobalLink.mPrev = mGlobalLink.mNext = &mGlobalLink;
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

IDE_RC iddRBHash::destroyStatic(void)
{
    IDE_ASSERT(mGlobalLink.mPrev == &mGlobalLink);
    IDE_ASSERT(mGlobalLink.mNext == &mGlobalLink);

    IDE_TEST( mGlobalMutex.destroy() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

/**
 * �ؽ� ��� ���� �� �̸��� �����Ѵ�
 *
 * @param aStat : ��� ����ü
 */
void iddRBHash::fillHashStat(iddRBHashStat* aStat)
{
    idlOS::strncpy(aStat->mName, mName, 32);
}

/**
 * �� ��Ŷ�� ��踦 ��������� �����Ѵ�
 *
 * @param aStat : ��� ����ü
 */
void iddRBHash::fillBucketStat(iddRBHashStat* aStat, const UInt aBucketNo)
{
    IDE_ASSERT(aBucketNo < mBucketCount);
    mBuckets[aBucketNo].fillStat(aStat);
    aStat->mBucketNo = aBucketNo;
}

/**
 * �ؽ��� ��� ������ �ʱ�ȭ�Ѵ�
 */
void iddRBHash::clearStat(void)
{
    UInt i;

    for(i = 0; i < mBucketCount; i++)
    {
        mBuckets[i].clearStat();
    }
}

static iduFixedTableColDesc gRBHashColDesc[] =
{
    {
        (SChar *)"NAME",
        IDU_FT_OFFSETOF(iddRBHashStat, mName),
        IDU_FT_SIZEOF(iddRBHashStat, mName),
        IDU_FT_TYPE_CHAR,
        NULL,
        0, 0, NULL // for internal use
    },
    {
        (SChar *)"BUCKET_NO",
        IDU_FT_OFFSETOF(iddRBHashStat, mBucketNo),
        IDU_FT_SIZEOF(iddRBHashStat, mBucketNo),
        IDU_FT_TYPE_UINTEGER,
        NULL,
        0, 0, NULL // for internal use
    },
    {
        (SChar *)"KEY_LENGTH",
        IDU_FT_OFFSETOF(iddRBHashStat, mKeyLength),
        IDU_FT_SIZEOF(iddRBHashStat, mKeyLength),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0, NULL // for internal use
    },
    {
        (SChar *)"NODE_COUNT",
        IDU_FT_OFFSETOF(iddRBHashStat, mCount),
        IDU_FT_SIZEOF(iddRBHashStat, mCount),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0, NULL // for internal use
    },
    {
        (SChar *)"SEARCH_COUNT",
        IDU_FT_OFFSETOF(iddRBHashStat, mSearchCount),
        IDU_FT_SIZEOF(iddRBHashStat, mSearchCount),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0, NULL // for internal use
    },
    {
        (SChar *)"INSERT_LEFT_MOVE",
        IDU_FT_OFFSETOF(iddRBHashStat, mInsertLeft),
        IDU_FT_SIZEOF(iddRBHashStat, mInsertLeft),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0, NULL // for internal use
    },
    {
        (SChar *)"INSERT_RIGHT_MOVE",
        IDU_FT_OFFSETOF(iddRBHashStat, mInsertRight),
        IDU_FT_SIZEOF(iddRBHashStat, mInsertRight),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0, NULL // for internal use
    },
    {
        NULL,
        0,
        0,
        IDU_FT_TYPE_CHAR,
        NULL,
        0, 0, NULL // for internal use
    }
};

static IDE_RC buildRecordForRBHash(idvSQL      *,
                                   void        *aHeader,
                                   void        * /* aDumpObj */,
                                   iduFixedTableMemory *aMemory)
{
    iddRBHash*      sHash;
    iddRBHashLink*  sLink;
    iddRBHashStat   sStat;
    UInt            sBucketCount;
    UInt            i;

    IDE_TEST( iddRBHash::mGlobalMutex.lock(NULL) != IDE_SUCCESS );

    sLink = iddRBHash::mGlobalLink.mNext;
    while(sLink != &(iddRBHash::mGlobalLink))
    {
        sHash = (iddRBHash*)sLink;
        sBucketCount = sHash->getBucketCount();
        sHash->fillHashStat(&sStat);

        for( i = 0; i < sBucketCount; i++ )
        {
            sHash->fillBucketStat(&sStat, i);
            IDE_TEST( iduFixedTable::buildRecord(aHeader,
                            aMemory,
                            (void *) &sStat)
                        != IDE_SUCCESS );
        }

        sLink = sLink->mNext;
    }

    IDE_TEST( iddRBHash::mGlobalMutex.unlock() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    IDE_ASSERT( iddRBHash::mGlobalMutex.unlock() == IDE_SUCCESS );
    return IDE_FAILURE;
}

iduFixedTableDesc gRBHashTableDesc =
{
    (SChar *)"X$RBHASH",
    buildRecordForRBHash,
    gRBHashColDesc,
    IDU_STARTUP_META,
    0,
    0,
    IDU_FT_DESC_TRANS_NOT_USE,
    NULL
};

