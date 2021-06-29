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

#ifndef _O_SDB_BUFFER_AREA_H_
#define _O_SDB_BUFFER_AREA_H_ 1

#include <sdbDef.h>
#include <sdbBCB.h>
#include <idu.h>
#include <idv.h>

/* applyFuncToEachBCBs �Լ� ������ �� BCB�� �����ϴ� �Լ��� ���� */
typedef IDE_RC (*sdbBufferAreaActFunc)( sdbBCB *aBCB, void *aFiltAgr);

class sdbBufferArea
{
public:
    IDE_RC initialize(UInt aChunkPageCount,
                      UInt aChunkCount,
                      UInt aPageSize);

    IDE_RC destroy();

    IDE_RC expandArea(idvSQL *aStatistics,
                      UInt    aChunkCount);

    //BUG-48042: Buffer Area Parallel ����  
    static void expandAreaParallel( void * aJob );

    IDE_RC shrinkArea(idvSQL *aStatistics,
                      UInt    aChunkCount);

    void addBCBs(idvSQL *aStatistics,
                 UInt    aCount,
                 sdbBCB *aFirst,
                 sdbBCB *aLast);

    sdbBCB* removeLast(idvSQL *aStatistics);

    void getAllBCBs(idvSQL  *aStatistics,
                    sdbBCB **aFirst,
                    sdbBCB **aLast,
                    UInt    *aCount);

    UInt getTotalCount();

    UInt getBCBCount();

    UInt getChunkPageCount();

    void freeAllAllocatedMem();

    IDE_RC applyFuncToEachBCBs(idvSQL                *aStatistics,
                               sdbBufferAreaActFunc   aFunc,
                               void                  *aObj);

    void lockBufferAreaX(idvSQL   *aStatistics);
    void lockBufferAreaS(idvSQL   *aStatistics);
    void unlockBufferArea();

    /* BUG-32528 disk page header�� BCB Pointer �� ������ ��쿡 ����
     * ����� ���� �߰�. */
    idBool isValidBCBPtrRange( sdbBCB * aBCBPtr )
    {
        if( ( aBCBPtr < mBCBPtrMin ) ||
            ( aBCBPtr > mBCBPtrMax ) )
        {
            ideLog::log( IDE_DUMP_0,
                         "Invalid BCB Pointer\n"
                         "BCB Ptr : %"ID_xPOINTER_FMT"\n"
                         "Min Ptr : %"ID_xPOINTER_FMT"\n"
                         "Max Ptr : %"ID_xPOINTER_FMT"\n",
                         aBCBPtr,
                         mBCBPtrMin,
                         mBCBPtrMax );

            return ID_FALSE;
        }
        return ID_TRUE;
    }
private:
    void initBCBPtrRange( )
    {
        mBCBPtrMin = (sdbBCB*)ID_ULONG_MAX;
        mBCBPtrMax = NULL;
    }

    void setBCBPtrRange( sdbBCB * aBCBPtr )
    {
        if( aBCBPtr > mBCBPtrMax )
        {
            mBCBPtrMax = aBCBPtr;
        }
        if( aBCBPtr < mBCBPtrMin )
        {
            mBCBPtrMin = aBCBPtr;
        }
    }

private:
    /* chunk�� page ���� */
    UInt         mChunkPageCount;
    
    /* buffer area�� ���� chunk ���� */
    UInt         mChunkCount;
    
    /* �ϳ��� frame ũ��, ���� 8K */
    UInt         mPageSize;
    
    /* buffer area�� free BCB ���� */
    UInt         mBCBCount;
   
    /* buffer area�� free BCB���� ����Ʈ */
    smuList      mUnUsedBCBListBase;
    
    /* buffer area�� ��� BCB���� ����Ʈ */
    smuList      mAllBCBList;
    
    /* BCB �Ҵ��� ���� �޸� Ǯ */
    iduMemPool   mBCBMemPool;

    /* BUG-20796: BUFFER_AREA_SIZE�� ������ ũ�⺸�� �ι��� �޸𸮸�
     *            ����ϰ� �ֽ��ϴ�.
     * iduMemPool�� �������� ������. iduMemPool2�� ����ϵ��� ��. */

    /* BCB�� Frame�� ���� Memory�� �����Ѵ�. */
    iduMemPool2  mFrameMemPool;
    
    /* BCB�� ����Ʈ�� ���� �޸� Ǯ*/
    iduMemPool   mListMemPool;
    
    /* buffer area ���ü� ��� ���� ��� ��ġ */
    iduLatch  mBufferAreaLatch;

    /* BUG-32528 disk page header�� BCB Pointer �� ������ ��쿡 ����
     * ����� ���� �߰�. */
    sdbBCB * mBCBPtrMin;
    sdbBCB * mBCBPtrMax;
};

inline UInt sdbBufferArea::getTotalCount()
{
    return mChunkPageCount * mChunkCount;
}

inline UInt sdbBufferArea::getBCBCount()
{
    return mBCBCount;
}

inline UInt sdbBufferArea::getChunkPageCount()
{
    return mChunkPageCount;
}

inline void sdbBufferArea::lockBufferAreaX(idvSQL   *aStatistics)
{
    IDE_ASSERT( mBufferAreaLatch.lockWrite( aStatistics,
                                            NULL )
                == IDE_SUCCESS );
}

inline void sdbBufferArea::lockBufferAreaS(idvSQL   *aStatistics)
{
    IDE_ASSERT( mBufferAreaLatch.lockRead( aStatistics,
                                           NULL )
                == IDE_SUCCESS );
}

inline void sdbBufferArea::unlockBufferArea()
{
    IDE_ASSERT( mBufferAreaLatch.unlock( ) == IDE_SUCCESS );
}

#endif // _O_SDB_BUFFER_AREA_H_

