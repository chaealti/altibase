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
 
#include <iduFileIOVec.h>
#include <iduMemMgr.h>

#define IDU_MINLEN 8

/***********************************************************************
 * Description : creator
 * ��� ������ �ʱ�ȭ���ش�.
 ***********************************************************************/
iduFileIOVec::iduFileIOVec() :
    mIOVec(NULL), mCount(0), mSize(0)
{
}

/***********************************************************************
 * Description : destructor
 *
 * DEBUG ��忡���� destroy�� ȣ����� ������ ASSERT�� ����Ѵ�.
 * RELEASE ��忡���� mIOVec�� �������� �ʾ����� �������ش�.
 ***********************************************************************/
iduFileIOVec::~iduFileIOVec()
{
    IDE_DASSERT(mIOVec == NULL);

    if(mIOVec != NULL)
    {
        iduMemMgr::free(mIOVec);
    }
}

/***********************************************************************
 * Description : initialize
 * ��� ������ �޸𸮸� �Ҵ��ϰ�
 * �����͸� ������ �����Ϳ� ũ�⸦ �����Ѵ�.
 *
 * aCount - [IN] �ڿ� �����μ��� �ԷµǴ�
 *               �����Ϳ� ũ���� ������ �����Ѵ�.
 *               �����Ϳ� ũ��� ¦���� ������ �� �μ� ������
 *               aCount * 2 + 1���̴�.
 * �Լ� ��� ���� :
 *      iduFileIOVec sVec;
 *      sVec.initialize();
 *      sVec.initialize(0);
 *      sVec.initialize(2, sPtr1, sLen1, sPtr2, sLen2);
 ***********************************************************************/
IDE_RC iduFileIOVec::initialize(SInt aCount, ...)
{
    SInt    sSize;
    void*   sPtr;
    size_t  sLen;
    va_list sVL;
    SInt    i;

    sSize = (aCount == 0)? IDU_MINLEN : idlOS::align8(aCount);
    IDE_TEST( iduMemMgr::malloc(IDU_MEM_ID_IDU,
                                sizeof(struct iovec) * sSize,
                                (void**)&mIOVec)
            != IDE_SUCCESS );
    
    va_start(sVL, aCount);

    for(i = 0; i < aCount; i++)
    {
        sPtr = va_arg(sVL, void*);
        sLen = va_arg(sVL, size_t);

        mIOVec[i].iov_base = sPtr;
        mIOVec[i].iov_len  = sLen;
    }

    va_end(sVL);

    mCount = aCount;
    mSize  = sSize;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

/***********************************************************************
 * Description : initialize
 * ��� ������ �޸𸮸� �Ҵ��ϰ�
 * �����͸� ������ �����Ϳ� ũ�⸦ �����Ѵ�.
 *
 * aCount - [IN] aPtr�� aLen�� ����ִ� �����Ϳ� ũ�� ������ �����Ѵ�.
 * aPtr   - [IN] �����͸� ������ ������ �迭
 * aLen   - [IN] ������ ũ�� �迭
 ***********************************************************************/
IDE_RC iduFileIOVec::initialize(SInt aCount, void** aPtr, size_t* aLen)
{
    SInt    sSize;
    SInt    i;

    sSize = idlOS::align8(aCount);
    IDE_TEST( iduMemMgr::malloc(IDU_MEM_ID_IDU,
                                sizeof(struct iovec) * sSize,
                                (void**)&mIOVec)
            != IDE_SUCCESS );
    
    for(i = 0; i < aCount; i++)
    {
        mIOVec[i].iov_base = aPtr[i];
        mIOVec[i].iov_len  = aLen[i];
    }

    mCount = aCount;
    mSize  = sSize;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

/***********************************************************************
 * Description : destroy
 *
 * ��� ������ �ʱ�ȭ�ϰ� �Ҵ���� �޸𸮸� ��� �����Ѵ�.
 ***********************************************************************/
IDE_RC iduFileIOVec::destroy(void)
{
    if(mIOVec != NULL)
    {
        IDE_TEST( iduMemMgr::free(mIOVec) != IDE_SUCCESS );
        mIOVec = NULL;
        mCount = 0;
        mSize  = 0;
    }
    else
    {
        /* do nothing */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}
    
/***********************************************************************
 * Description : add
 * �����Ϳ� ���̸� �߰��Ѵ�.
 *
 * aPtr   - [IN] �����͸� ������ ������
 * aLen   - [IN] ������ ũ��
 ***********************************************************************/
IDE_RC iduFileIOVec::add(void* aPtr, size_t aLen)
{
    if(mCount == mSize)
    {
        IDE_TEST( iduMemMgr::realloc(IDU_MEM_ID_IDU,
                                     sizeof(struct iovec) * mSize * 2,
                                     (void**)&mIOVec)
                != IDE_SUCCESS );

        mSize *= 2;
    }
    else
    {
        /* do nothing */
    }

    mIOVec[mCount].iov_base = aPtr;
    mIOVec[mCount].iov_len  = aLen;

    mCount++;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

/***********************************************************************
 * Description : add
 * �����Ϳ� ���̸� �߰��Ѵ�.
 *
 * aCount - [IN] aPtr�� aLen�� ����ִ� �����Ϳ� ũ�� ������ �����Ѵ�.
 * aPtr   - [IN] �����͸� ������ ������ �迭
 * aLen   - [IN] ������ ũ�� �迭
 ***********************************************************************/
IDE_RC iduFileIOVec::add(SInt aCount, void** aPtr, size_t* aLen)
{
    SInt sNewCount;
    SInt i;

    sNewCount = idlOS::align8(mCount + aCount);

    if(sNewCount > mSize)
    {
        IDE_TEST( iduMemMgr::realloc(IDU_MEM_ID_IDU,
                                     sizeof(struct iovec) * sNewCount,
                                     (void**)&mIOVec)
                != IDE_SUCCESS );

        mSize = sNewCount;
    }
    else
    {
        /* do nothing */
    }

    for(i = 0; i < aCount; i++)
    {
        mIOVec[mCount + i].iov_base = aPtr[i];
        mIOVec[mCount + i].iov_len  = aLen[i];
    }

    mCount += aCount;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

/***********************************************************************
 * Description : clear
 * �����Ϳ� ���̸� ��� �����Ѵ�.
 * �޸𸮸� ���������� �ʴ´�.
 ***********************************************************************/
IDE_RC iduFileIOVec::clear(void)
{
    mCount = 0;
    return IDE_SUCCESS;
}
