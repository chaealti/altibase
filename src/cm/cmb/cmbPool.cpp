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

#include <cmAll.h>


typedef struct cmbPoolAllocInfo
{
    IDE_RC (*mMap)(cmbPool *aPool);
    UInt   (*mSize)();
} cmbPoolAllocInfo;


static cmbPoolAllocInfo gCmbPoolAllocInfo[CMB_POOL_IMPL_MAX] =
{
    {
        NULL,
        NULL
    },
    {
        cmbPoolMapLOCAL,
        cmbPoolSizeLOCAL
    },
    {
#if defined(CM_DISABLE_IPC)
        NULL,
        NULL
#else
        cmbPoolMapIPC,
        cmbPoolSizeIPC
#endif
    },
    {
#if defined(CM_DISABLE_IPCDA)
        NULL,
        NULL
#else
        cmbPoolMapIPCDA,
        cmbPoolSizeIPCDA
#endif
    },
};


static cmbPool *gCmbPoolSharedPool[CMB_POOL_IMPL_MAX] =
{
    NULL,
    NULL,
    NULL,
    NULL
};


IDE_RC cmbPoolAlloc(cmbPool **aPool, UChar aImpl, UShort aBlockSize, UInt /*aBlockCount*/)
{
    cmbPoolAllocInfo *sAllocInfo;

    
    /*
     * �Ķ���� ���� �˻�
     */
    IDE_ASSERT(aImpl < CMB_POOL_IMPL_MAX);

    /*
     * AllocInfo ȹ��
     */
    sAllocInfo = &gCmbPoolAllocInfo[aImpl];

    IDE_ASSERT(sAllocInfo->mMap  != NULL);
    IDE_ASSERT(sAllocInfo->mSize != NULL);

    IDU_FIT_POINT_RAISE( "cmbPool::cmbPoolAlloc::malloc::Pool",
                          InsufficientMemory );
    /*
     * �޸� �Ҵ�
     */
    IDE_TEST_RAISE(iduMemMgr::malloc(IDU_MEM_CMB,
                                     sAllocInfo->mSize(),
                                     (void **)aPool,
                                     IDU_MEM_IMMEDIATE) != IDE_SUCCESS, InsufficientMemory );

    /*
     * ��� �ʱ�ȭ
     */
    (*aPool)->mBlockSize     = aBlockSize;

    /*
     * �Լ� ������ ����
     */
    IDE_TEST_RAISE(sAllocInfo->mMap(*aPool) != IDE_SUCCESS, InitializeFail);

    /*
     * �ʱ�ȭ
     */
    IDE_TEST_RAISE((*aPool)->mOp->mInitialize(*aPool) != IDE_SUCCESS, InitializeFail);

    return IDE_SUCCESS;

    IDE_EXCEPTION(InsufficientMemory);
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_InsufficientMemory));
    }
    IDE_EXCEPTION(InitializeFail);
    {
        IDE_ASSERT(iduMemMgr::free(*aPool) == IDE_SUCCESS);
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC cmbPoolFree(cmbPool *aPool)
{
    /*
     * ����
     */
    IDE_TEST(aPool->mOp->mFinalize(aPool) != IDE_SUCCESS);

    /*
     * �޸� ����
     */
    IDE_TEST(iduMemMgr::free(aPool) != IDE_SUCCESS);

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

IDE_RC cmbPoolSetSharedPool(cmbPool *aPool, UChar aImpl)
{
    /*
     * �Ķ���� ���� �˻�
     */
    IDE_ASSERT(aImpl > CMB_POOL_IMPL_NONE);
    IDE_ASSERT(aImpl < CMB_POOL_IMPL_MAX);

    gCmbPoolSharedPool[aImpl] = aPool;

    return IDE_SUCCESS;
}

IDE_RC cmbPoolGetSharedPool(cmbPool **aPool, UChar aImpl)
{
    /*
     * �Ķ���� ���� �˻�
     */
    IDE_ASSERT(aImpl < CMB_POOL_IMPL_MAX);

    *aPool = gCmbPoolSharedPool[aImpl];

    IDE_TEST_RAISE(*aPool == NULL, SharedPoolNotExist);

    return IDE_SUCCESS;

    IDE_EXCEPTION(SharedPoolNotExist);
    {
        IDE_SET(ideSetErrorCode(cmERR_ABORT_SHARED_POOL_NOT_EXIST));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
