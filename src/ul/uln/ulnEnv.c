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

#include <uln.h>
#include <ulnPrivate.h>
#include <ulnEnv.h>

/**
 * ulnCreateEnv.
 *
 * �Լ��� �ϴ� ��
 *  - ENV �� ���� uluChunkPool �ν��Ͻ� ����
 *  - ENV �� ���� uluMemory �ν��Ͻ� ����
 *  - ENV �� ���� Diagnostic Header �ν��Ͻ� ����
 *  - ENV �� Diagnostic Header �ʱ�ȭ
 *  - ENV �� mObj �ʱ�ȭ
 *
 * @return
 *  - ACI_SUCCESS
 *  - ACI_FAILURE
 *    �޸� ������ ������ ����.
 *    �Ҵ��Ϸ��� �õ��ߴ� ��� �޸𸮸� clear �� �Ŀ� �����ϹǷ� ������ óũ�ؼ�
 *    ����ڿ��� �ٷ� �����ص� ��.
 */
ACI_RC ulnEnvCreate(ulnEnv **aOutputEnv)
{
    ULN_FLAG(sNeedFinalize);
    ULN_FLAG(sNeedDestroyPool);
    ULN_FLAG(sNeedDestroyMemory);
    ULN_FLAG(sNeedDestroyLock);

    ulnEnv          *sEnv;

    uluChunkPool    *sPool;
    uluMemory       *sMemory;

    acp_thr_mutex_t *sLock = NULL;

    /*
     * ulnInitialize
     */
    ACI_TEST(ulnInitialize() != ACI_SUCCESS);
    ULN_FLAG_UP(sNeedFinalize);

    /*
     * �޸� �Ҵ��� �̸� �� �д�.
     * �̸� ���ؼ� Chunk Pool �� �����ϰ�, Memory �� ������ ��
     * Env �ڵ��� ������ �޸𸮿��ٰ� �д�.
     */
    sPool = uluChunkPoolCreate(ULN_SIZE_OF_CHUNK_IN_ENV, ULN_NUMBER_OF_SP_IN_ENV, 2);
    ACI_TEST(sPool == NULL);
    ULN_FLAG_UP(sNeedDestroyPool);

    ACI_TEST(uluMemoryCreate(sPool, &sMemory) != ACI_SUCCESS);
    ULN_FLAG_UP(sNeedDestroyMemory);

    ACI_TEST(sMemory->mOp->mMalloc(sMemory, (void **)(&sEnv), ACI_SIZEOF(ulnEnv)) != ACI_SUCCESS);
    ACI_TEST(sMemory->mOp->mMarkSP(sMemory) != ACI_SUCCESS);

    /*
     * ulnEnv �� Object �κ��� �ʱ�ȭ �Ѵ�.
     *
     * ���� �Ҵ�Ǹ� ���¸� Allocated�� �д�.
     * ���� ���� ���̰� ���ʿ��ϴ�.
     */
    ulnObjectInitialize((ulnObject *)sEnv,
                        ULN_OBJ_TYPE_ENV,
                        ULN_DESC_TYPE_NODESC,
                        ULN_S_E1,
                        sPool,
                        sMemory);

    /*
     * Lock ����ü ���� �� �ʱ�ȭ
     */
    ACI_TEST(uluLockCreate(&sLock) != ACI_SUCCESS);
    ULN_FLAG_UP(sNeedDestroyLock);

    ACI_TEST(acpThrMutexCreate(sLock, ACP_THR_MUTEX_DEFAULT) != ACP_RC_SUCCESS);

    /*
     * CreateDiagHeader�� �������� ���,
     * LABEL_MALLOC_FAIL_ENV ���̺�� �����ؼ� �޸� �����ϴ� ������ ����Ѵ�
     */
    ACI_TEST(ulnCreateDiagHeader((ulnObject *)sEnv, NULL) != ACI_SUCCESS);

    sEnv->mObj.mLock = sLock;
    *aOutputEnv      = sEnv;

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    ULN_IS_FLAG_UP(sNeedDestroyLock)
    {
        acpThrMutexDestroy(sLock);
        uluLockDestroy(sLock);
    }

    ULN_IS_FLAG_UP(sNeedDestroyMemory)
    {
        sMemory->mOp->mDestroyMyself(sMemory);
    }

    ULN_IS_FLAG_UP(sNeedDestroyPool)
    {
        sPool->mOp->mDestroyMyself(sPool);
    }

    ULN_IS_FLAG_UP(sNeedFinalize)
    {
        ulnFinalize();
    }

    return ACI_FAILURE;
}

/**
 * ulnEnvDestroy.
 *
 * @param[in] aEnv
 *  �ı��� ENV �� ����Ű�� ������
 * @return
 *  - ACI_SUCCESS
 *    ����
 *  - ACI_FAILURE
 *    ����.
 *    ȣ���ڴ� HY013 �� ����ڿ��� ��� �Ѵ�.
 */
ACI_RC ulnEnvDestroy(ulnEnv *aEnv)
{
    ulnObject       *sObject;
    uluMemory       *sMemory;
    uluChunkPool    *sPool;

    /*
     * BUGBUG
     * ulnDestroyDbc() �Լ��� ENV �ΰ͸� ���� ������ identical �� �ڵ��̴�.
     * ������ Ȯ���� ���̱� ���ؼ� �ΰ��� �ڵ带 ���ľ� �� �ʿ䰡 �ְڴ�.
     */

    ACE_ASSERT(ULN_OBJ_GET_TYPE(aEnv) == ULN_OBJ_TYPE_ENV);

    sObject = &aEnv->mObj;
    sPool   =  sObject->mPool;
    sMemory =  sObject->mMemory;

    /* BUG-35332 The socket files can be moved */
    ulnPropertiesDestroy(&aEnv->mProperties);

    /*
     * DiagHeader �� ���� �޸� ��ü�� �ı��Ѵ�.
     */

    ACI_TEST(ulnDestroyDiagHeader(&sObject->mDiagHeader, ULN_DIAG_HDR_DESTROY_CHUNKPOOL)
             != ACI_SUCCESS);

    /*
     * DESC �� ���ֱ� ������ �Ǽ��� ���� ������ �����ϱ� ���ؼ� ulnObject �� ǥ�ø� �� �д�.
     * BUG-15894 �� ���� ����� ���� ���α׷��� ���� ���׸� �����ϱ� ���ؼ��̴�.
     */

    sObject->mType = ULN_OBJ_TYPE_MAX;

    /*
     * ûũǮ�� �޸� �ı�
     */

    sMemory->mOp->mDestroyMyself(sMemory);
    sPool->mOp->mDestroyMyself(sPool);

    /*
     * Note : Lock ������ ulnFreeHandleEnv() ���� �Ѵ�.
     */

    /*
     * ulnFinalize
     */
    ACI_TEST(ulnFinalize() != ACI_SUCCESS);

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

/**
 * ulnInitializeEnv.
 *
 * �Լ��� �ϴ� ��
 * - ENV ����ü�� �ʵ���� �ʱ�ȭ
 */
ACI_RC ulnEnvInitialize(ulnEnv *aEnv)
{
    aEnv->mDbcCount = 0;
    acpListInit(&aEnv->mDbcList);

    /*
     * Note:
     * MSDN ODBC 3.0 :
     * SQLAllocHandle does not set the SQL_ATTR_ODBC_VERSION
     * environment attribute when it is called to allocate an
     * environment handle;
     * the environment attribute must be set by the application
     * BUGBUG: 2.0�� �´���, 0 �� �´���..
     */
    aEnv->mOdbcVersion   = SQL_OV_ODBC2;

    aEnv->mConnPooling   = SQL_CP_OFF;
    aEnv->mConnPoolMatch = SQL_CP_STRICT_MATCH;
    aEnv->mOutputNts     = SQL_TRUE;

    aEnv->mUlnVersion    = 0;

    /* BUG-35332 The socket files can be moved */
    ulnPropertiesCreate(&aEnv->mProperties);

    /* PROJ-2733-DistTxInfo */
    aEnv->mSCN           = 0;

    /*
     * ���¸� E1 - Allocated ���·� �����Ѵ�.
     * BUGBUG:���� �ؾ� ���� �տ��� �߾�� ���� �Ǵ� �ʿ�.
     */
    ULN_OBJ_SET_STATE(aEnv, ULN_S_E1);

    return ACI_SUCCESS;
}

/**
 * ulnAddDbcToEnv.
 *
 * DBC ����ü�� ENV �� mDbcList �� �߰��Ѵ�.
 */
ACI_RC ulnEnvAddDbc(ulnEnv *aEnv, ulnDbc *aDbc)
{
    ACE_ASSERT(aEnv != NULL);
    ACE_ASSERT(aDbc != NULL);

    acpListAppendNode(&(aEnv->mDbcList), (acp_list_t *)aDbc);
    aEnv->mDbcCount++;

    aDbc->mParentEnv = aEnv;

    return ACI_SUCCESS;
}

/**
 * ulnRemoveDbcFromEnv.
 *
 * DBC ����ü�� ENV �� mDbcList ���� �����Ѵ�.
 */
ACI_RC ulnEnvRemoveDbc(ulnEnv *aEnv, ulnDbc *aDbc)
{
    ACE_ASSERT(aEnv != NULL);
    ACE_ASSERT(aDbc != NULL);

    ACI_TEST(acpListIsEmpty(&aEnv->mDbcList));
    ACI_TEST(aEnv->mDbcCount == 0);

    acpListDeleteNode((acp_list_node_t *)aDbc);
    aEnv->mDbcCount--;

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

/**
 * ulnGetDbcCountFromEnv.
 *
 * ENV ����ü�� �޷� �ִ� DBC ����ü�� ������ �д´�.
 */
acp_uint32_t ulnEnvGetDbcCount(ulnEnv *aEnv)
{
    return aEnv->mDbcCount;
}

ACI_RC ulnEnvSetOutputNts(ulnEnv *aEnv, acp_sint32_t aNts)
{
    aEnv->mOutputNts = aNts;
    return ACI_SUCCESS;
}

ACI_RC ulnEnvGetOutputNts(ulnEnv *aEnv, acp_sint32_t *aNts)
{
    *aNts = aEnv->mOutputNts;
    return ACI_SUCCESS;
}

acp_uint32_t ulnEnvGetOdbcVersion(ulnEnv *aEnv)
{
    return aEnv->mOdbcVersion;
}

ACI_RC ulnEnvSetOdbcVersion(ulnEnv *aEnv, acp_uint32_t aVersion)
{
    switch(aVersion)
    {
        case SQL_OV_ODBC3:
        case SQL_OV_ODBC2:
            aEnv->mOdbcVersion = aVersion;
            break;
        default:
            return ACI_FAILURE;
    }
    return ACI_SUCCESS;
}

ACI_RC ulnEnvSetUlnVersion(ulnEnv *aEnv, acp_uint64_t aVersion)
{
    aEnv->mUlnVersion = aVersion;

    return ACI_SUCCESS;
}

acp_uint64_t  ulnEnvGetUlnVersion(ulnEnv *aEnv)
{
    return aEnv->mUlnVersion;
}

