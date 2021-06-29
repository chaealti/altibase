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
#include <uluLock.h>
#include <ulnObject.h>

/**
 * ulnObjectInitialize
 *
 * @param aObject
 *  �ʱ�ȭ�ϰ��� �ϴ� ��ü�� ����Ű�� ������.
 * @param aType
 *  ��ü�� Ÿ��.
 * @param aSubType
 *  ��ü�� ����Ÿ��. DESC ���� �ش��Ѵ�. ��ü�� Ÿ���� DESC �� �ƴϸ�
 *  ULN_DESC_TYPE_NODESC �� �Ѱ��ָ� �ȴ�.
 * @param aState
 *  �ʱ� ���·� �����ϰ��� �ϴ� state.
 * @param aPool
 *  ��ü�� ����� ûũ Ǯ�� ����Ű�� ������.
 * @param aMemory
 *  ��ü�� ����� �޸� �ν��Ͻ��� ����Ű�� ������.
 *
 * @note
 *  ���� : �� �Լ��� Object ���� diagnostic header �� record �� �ǵ帮�� �ʴ´�.
 */
ACI_RC ulnObjectInitialize(ulnObject    *aObject,
                           ulnObjType    aType,
                           ulnDescType   aSubType,
                           acp_sint16_t  aState,
                           uluChunkPool *aPool,
                           uluMemory    *aMemory)
{
    ACE_ASSERT(aObject != NULL);
    ACE_ASSERT(aPool != NULL);
    ACE_ASSERT(aMemory != NULL);

    ACE_ASSERT(aType > ULN_OBJ_TYPE_MIN && aType < ULN_OBJ_TYPE_MAX);

    acpListInitObj(&aObject->mList, aObject);

    ULN_OBJ_SET_TYPE(aObject, aType);
    ULN_OBJ_SET_DESC_TYPE(aObject, aSubType);
    ULN_OBJ_SET_STATE(aObject, aState);

    /*
     * �ϴ� ���ʿ� ��ü�� ��������� ������ AllocHandle �� ����̴�.
     * �׷���, AllocHandle ���� ���������� �� �� �ٽ� ULN_FID_NONE ���� �����ϱ� �����Ƿ�
     * ���� ���� FID_NONE ���� ��������.
     * ������ AllocHandle �� �����ϱ⵵ ���� �� �ڵ�� �Լ� ȣ���ϰų� �ϴ� ���� ���ٰ� ����
     * ������ ���̴�.
     */
    aObject->mExecFuncID = ULN_FID_NONE;

    aObject->mPool       = aPool;
    aObject->mMemory     = aMemory;

    /*
     * SQLError() �Լ������� �����Ѵ�.
     * ulnClearDiagnosticInfoFromObject() �Լ������� 1 �� �ʱ�ȭ �Ѵ�.
     */
    aObject->mSqlErrorRecordNumber = 1;

    return ACI_SUCCESS;
}

acp_sint32_t ulnObjectGetSqlErrorRecordNumber(ulnObject *aHandle)
{
    return aHandle->mSqlErrorRecordNumber;
}

ACI_RC ulnObjectSetSqlErrorRecordNumber(ulnObject *aHandle, acp_sint32_t aRecNumber)
{
    aHandle->mSqlErrorRecordNumber = aRecNumber;

    return ACI_SUCCESS;
}
