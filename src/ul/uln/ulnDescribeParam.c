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
#include <ulnDescribeParam.h>

/*
 * ==============
 * IMPORTANT Note
 * ==============
 *
 * SQLDecribeParam() �Լ��� ȣ���ؼ� ����ڰ� ��� �Ǵ� ������ IPD ���ڵ忡 �ִ� ������
 * �Ǿ�� �Ѵ�.
 *
 * ����� ������,
 *  1. ����ڰ� �̹� ���ε��� �Ķ���Ϳ� SQLDescribeParam() �� ȣ���� ��� ��� �Ұ��ΰ�?
 *
 * �ε�, �Ʒ��� ���� ��å�� �����Ͽ��� :
 *
 *  1. �̹� ���ε��� �Ķ���Ͷ�� IPD record �� �ִ� ������ �о �״�� ����ڿ��� �Ѱ��ش�.
 *
 *  2. ���� ���ε���� ���� �Ķ���Ͷ�� �������� ������ ������ �̿��� ���� IPD record ��
 *     IPD �� �Ŵ޾ƹ�����. �̷��� ������� IPD record �� ����ڰ� ���ε带 �� �� ���������.
 *     ������� IPD record �� IPD �� �Ŵٴ� ������, ����ڰ� SQLGetDescField() �Լ��� ȣ����
 *     ��� ������ �����ϱ� ���ؼ��̴�.
 *
 *  3. SQLGetDescField() �Լ��� �̿��ؼ� IPD record �� �ʵ���� ����ڰ� �������� ���� ���,
 *     �ش� IPD record �� �������� ������ SQLDescribeParam() �� ��ƾ���� �̿��ؼ� IPD record ��
 *     �����ϵ��� �Ѵ�.
 *
 *  4. ����ڰ� SQLBindParameter() �� �ϰ� �Ǹ� IPD record �� ������ �����ϵ��� �Ѵ�.
 *
 * ���� ��å��, SQLDescribeParam() �Լ��� "�������� �ʿ�� �ϴ�" �Ķ������ ��Ÿ��� ������ ��
 * �ణ �̻��� �� �ִ�.
 * �׷���, SQLBindParameter() �Լ��� �ֿ� ���� �߿� �ϳ��� IPD record �� "����" �ϴ� ���̶��
 * ����� ������ �� �� ���� �̻����� �ʴ�.
 *
 * SQLGetDescField() �� ���� ��� IRD record �� ������
 * SQLDescribeCol() �� ���� ��� ������ ��ġ�ؾ� �ϸ�, ���� ��ġ�ϵ��� �����Ǿ� �ִ�.
 *
 * ����������,
 * SQLGetDescField() �� ���� ��� IPD record �� ������
 * SQLDescribeParam() �� ���� ��� ������ ��ġ�ؾ� �Ѵ�.
 * ---> SQLDescribeParam() �� IPD record �� �����Կ��� �ұ��ϰ� �������� ������
 * ���ͼ� IPD record �� ����� �ٸ� ������ �����ؼ��� �ȵȴ�.
 */

ACI_RC ulnDescribeParamGetIpdInfoFromServer(ulnFnContext *aFnContext, ulnPtContext *aPtContext)
{
    ulnDbc       *sDbc;
    ULN_FNCONTEXT_GET_DBC(aFnContext, sDbc);
    /* BUG-46052 codesonar Null Pointer Dereference */
    ACI_TEST_RAISE(sDbc == NULL, InvalidHandleException);

    ACI_TEST(ulnWriteParamInfoGetREQ(aFnContext, aPtContext, 0) != ACI_SUCCESS);

    /* bug-31989: SQLDescribeParam() ignores connection_timeout. */
    ACI_TEST(ulnFlushAndReadProtocol(aFnContext,
                                     aPtContext,
                                     sDbc->mConnTimeoutValue)
             != ACI_SUCCESS);

    return ACI_SUCCESS;

    /* BUG-46052 codesonar Null Pointer Dereference */
    ACI_EXCEPTION(InvalidHandleException)
    {
        ULN_FNCONTEXT_SET_RC(aFnContext, SQL_INVALID_HANDLE);
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

static ACI_RC ulnDescribeParamCheckArgs(ulnFnContext *aFnContext, acp_uint16_t aParamNumber)
{
    ACI_TEST_RAISE(aParamNumber == 0, LABEL_INVALID_DESC_INDEX);

    ACI_TEST_RAISE(ulnStmtGetParamCount(aFnContext->mHandle.mStmt) < aParamNumber,
                   LABEL_INVALID_DESC_INDEX);

    return ACI_SUCCESS;

    ACI_EXCEPTION(LABEL_INVALID_DESC_INDEX)
    {
        ulnError(aFnContext, ulERR_ABORT_INVALID_DESCRIPTOR_INDEX, aParamNumber);
    }

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

static ACI_RC ulnDescribeParamCore(ulnFnContext *aFnContext,
                                   ulnPtContext *aPtContext,
                                   ulnStmt      *aStmt,
                                   acp_uint16_t  aParamNumber,
                                   acp_sint16_t *aDataTypePtr,
                                   ulvULen      *aParamSizePtr,
                                   acp_sint16_t *aDecimalDigitsPtr,
                                   acp_sint16_t *aNullablePtr)
{
    acp_sint16_t  sSQLTYPE;
    ulnDescRec   *sDescRecIpd = NULL;
    ulnMTypeID    sMTYPE;

    sDescRecIpd = ulnStmtGetIpdRec(aStmt, aParamNumber);

    /* BUG-44296 ipd�� �ϳ��� ������ �������� ������ ������ ���� ipd�� �����. */
    if (sDescRecIpd == NULL)
    {
        ACI_TEST(ulnDescribeParamGetIpdInfoFromServer(aFnContext, aPtContext) != ACI_SUCCESS);

        sDescRecIpd = ulnStmtGetIpdRec(aStmt, aParamNumber);

        /*
         * �Ķ������ �������� ū ParamNumber �� �Ѱ���� ������ IPD record �� �߰ߵ��� ����
         * ���ɼ��� �ִ�. �׷���, SQLDescribeParam() �Լ��� Prepared ����(S2, S3)������
         * �����ϰ� ȣ��� �� �ִ�. ����, �� �Լ��� ȣ��� ����������,
         * �ʿ��� �Ķ������ ������ �̹� �˰� �ִ� �����̸�, �Լ� ���Խÿ� �Ķ���� �ѹ���
         * üũ�ؼ� ������ ���� ������ �����Ų��.
         * �׷��Ƿ� ���� �������� parameter �������� ���� �Դµ�, IPD record �� ���ٴ� ����
         * �޸� corruption ���� ������� ���� ��Ȳ�̴�.
         */
        ACI_TEST_RAISE(sDescRecIpd == NULL, LABEL_MEM_MAN_ERR);
    }

    if (aDataTypePtr != NULL)
    {
        sSQLTYPE = ulnMetaGetOdbcConciseType(&sDescRecIpd->mMeta);
        sSQLTYPE = ulnTypeMap_LOB_SQLTYPE(sSQLTYPE,
                                          ulnDbcGetLongDataCompat(aStmt->mParentDbc));

        *aDataTypePtr = sSQLTYPE;
    }

    if (aParamSizePtr != NULL)
    {
        /*
         * Note : ulnMeta �� octet length ���� �׻� ����� ����.
         */
        *aParamSizePtr = (ulvULen)ulnMetaGetOctetLength(&sDescRecIpd->mMeta);
    }

    if (aDecimalDigitsPtr != NULL)
    {
        sMTYPE = ulnMetaGetMTYPE(&sDescRecIpd->mMeta);

        if (sMTYPE == ULN_MTYPE_NUMBER || sMTYPE == ULN_MTYPE_NUMERIC)
        {
            *aDecimalDigitsPtr = ulnMetaGetScale(&sDescRecIpd->mMeta);
        }
        else
        {
            *aDecimalDigitsPtr = 0;
        }
    }

    if (aNullablePtr != NULL)
    {
        *aNullablePtr = ulnMetaGetNullable(&sDescRecIpd->mMeta);
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION(LABEL_MEM_MAN_ERR)
    {
        ulnError(aFnContext, ulERR_FATAL_MEMORY_MANAGEMENT_ERROR, "ulnDescribeParamCore");
    }

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

SQLRETURN ulnDescribeParam(ulnStmt      *aStmt,
                           acp_uint16_t  aParamNumber,
                           acp_sint16_t *aDataTypePtr,
                           ulvULen      *aParamSizePtr,
                           acp_sint16_t *aDecimalDigitsPtr,
                           acp_sint16_t *aNullablePtr)
{
    ULN_FLAG(sNeedExit);
    ULN_FLAG(sNeedFinPtContext);

    ulnFnContext  sFnContext;

    ULN_INIT_FUNCTION_CONTEXT(sFnContext, ULN_FID_DESCRIBEPARAM, aStmt, ULN_OBJ_TYPE_STMT);

    /*
     * Enter
     */
    ACI_TEST(ulnEnter(&sFnContext, NULL) != ACI_SUCCESS);
    ULN_FLAG_UP(sNeedExit);

    /* PROJ-1891, BUG-46011 If deferred prepare is exists, process it first */
    if (ulnStmtIsSetDeferredQstr(aStmt) == ACP_TRUE)
    {
        ACI_TEST( ulnPrepareDeferComplete(&sFnContext, ACP_TRUE) );
    }

    /*
     * �Ѱܹ��� ���ڵ� üũ
     */
    ACI_TEST(ulnDescribeParamCheckArgs(&sFnContext, aParamNumber) != ACI_SUCCESS);

    /*
     * Protocol Context �ʱ�ȭ
     */
    // fix BUG-17722
    ACI_TEST(ulnInitializeProtocolContext(&sFnContext,
                                          &(aStmt->mParentDbc->mPtContext),
                                          &(aStmt->mParentDbc->mSession))
             != ACI_SUCCESS);
    ULN_FLAG_UP(sNeedFinPtContext);
    // fix BUG-17722
    ACI_TEST(ulnDescribeParamCore(&sFnContext,
                                  &(aStmt->mParentDbc->mPtContext),
                                  aStmt,
                                  aParamNumber,
                                  aDataTypePtr,
                                  aParamSizePtr,
                                  aDecimalDigitsPtr,
                                  aNullablePtr) != ACI_SUCCESS);

    /*
     * Protocol Context ����
     */
    ULN_FLAG_DOWN(sNeedFinPtContext);
    // fix BUG-17722
    ACI_TEST(ulnFinalizeProtocolContext(&sFnContext,
                                        &(aStmt->mParentDbc->mPtContext))
                != ACI_SUCCESS);

    /*
     * Exit
     */
    ULN_FLAG_DOWN(sNeedExit);
    ACI_TEST(ulnExit(&sFnContext) != ACI_SUCCESS);

    ULN_TRACE_LOG(&sFnContext, ULN_TRACELOG_MID, NULL, 0,
            "%-18s| [%2"ACI_UINT32_FMT" stypePtr: %p]",
            "ulnDescribeParam", aParamNumber, aDataTypePtr);

    return ULN_FNCONTEXT_GET_RC(&sFnContext);

    ACI_EXCEPTION_END;

    ULN_IS_FLAG_UP(sNeedFinPtContext)
    {
        // fix BUG-17722
        ulnFinalizeProtocolContext(&sFnContext,
                                   &(aStmt->mParentDbc->mPtContext) );
    }

    ULN_IS_FLAG_UP(sNeedExit)
    {
        ulnExit(&sFnContext);
    }

    ULN_TRACE_LOG(&sFnContext, ULN_TRACELOG_LOW, NULL, 0,
            "%-18s| [%2"ACI_UINT32_FMT" stypePtr: %p] fail",
            "ulnDescribeParam", aParamNumber, aDataTypePtr);

    return ULN_FNCONTEXT_GET_RC(&sFnContext);
}
