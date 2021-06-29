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

static ACI_RC ulnSetDescHeaderField(ulnFnContext *aFnContext,
                                    acp_sint16_t  aFieldIdentifier,
                                    void         *aValuePtr,
                                    acp_sint32_t  aBufferLength,
                                    ulnDescType   aDescType)
{
    ulnDesc      *sDesc;
    acp_uint32_t  sValue;

    ACP_UNUSED(aBufferLength);

    sValue = (acp_uint32_t)(((acp_ulong_t)aValuePtr) & ACP_UINT32_MAX);
    sDesc  = aFnContext->mHandle.mDesc;

    switch(aFieldIdentifier)
    {
        case SQL_DESC_BIND_TYPE:
            ulnDescSetBindType(sDesc, sValue);
            break;

        case SQL_DESC_ALLOC_TYPE:
            ACI_RAISE(LABEL_READ_ONLY_FIELD);
            break;

        case SQL_DESC_ARRAY_SIZE:
            /*
             * Note : When the FieldIdentifier parameter has one of the following values,
             *        a 64-bit value is returned in *ValuePtr:
             *
             *          SQL_DESC_ARRAY_SIZE
             *
             * �׷��� ���� 20�ﰳ �̻��� ���Ұ� �ִ� �迭�� array �� ���ε� �ϰڴ°�.
             * ���θ� ��Ű�� ���ο����� �ܼ��� acp_uint32_t �� ����ϵ��� �Ѵ�.
             *
             * ulnStmtSetAttrRowArraySize() �Լ��� �ּ� ����
             */
            //BUG-21253 �ѹ��� array insert�ϴ� ���� �����ؾ� �մϴ�.
            // array insert�� ���� descType�� ULN_DESC_TYPE_APD�� ��쿡 �ش��Ѵ�.
            // array fetch�� ��쿡�� size�� �������� �ʴ´�.
            if( aDescType == ULN_DESC_TYPE_APD )
            {
                ACI_TEST_RAISE( sValue > ACP_UINT16_MAX,
                                LABEL_EXCEEDING_ARRAY_SIZE_LIMIT );
            }
            ulnDescSetArraySize(sDesc, sValue);
            break;

        case SQL_DESC_ARRAY_STATUS_PTR:
        case SQL_DESC_BIND_OFFSET_PTR:
            break;

        case SQL_DESC_COUNT:
            /*
             * BUGBUG : �̰� �����ؾ� �Ѵ�! DescRec ������ �ÿ��� �ϳ�? -_-;
             */
            ACI_TEST_RAISE(aDescType == ULN_DESC_TYPE_IRD, LABEL_READ_ONLY_FIELD);
            break;

        case SQL_DESC_ROWS_PROCESSED_PTR:
            break;

        default:
            ACI_RAISE(LABEL_MEM_MAN_ERR);
            break;
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION(LABEL_READ_ONLY_FIELD)
    {
        /*
         * HY091 : Invalid descriptor field identifier
         *       : The FieldIdentifier argument was a read-only, ODBC-defined field.
         */
        ulnError(aFnContext, ulERR_ABORT_READONLY_DESCRIPTOR_FIELD, aFieldIdentifier);
    }
    ACI_EXCEPTION(LABEL_MEM_MAN_ERR)
    {
        ulnError(aFnContext, ulERR_FATAL_MEMORY_MANAGEMENT_ERROR, "");
    }
    ACI_EXCEPTION(LABEL_EXCEEDING_ARRAY_SIZE_LIMIT)
    {
        ulnError(aFnContext, ulERR_ABORT_INVALID_ARRAY_SIZE);
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

static ACI_RC ulnSetDescRecordField(ulnFnContext *aFnContext,
                                    acp_sint16_t  aRecNumber,
                                    acp_sint16_t  aFieldIdentifier,
                                    void         *aValuePtr,
                                    acp_sint32_t  aBufferLength,
                                    ulnDescType   aDescType)
{
    acp_uint32_t  sValue;
    ulnDesc      *sDesc;
    ulnDesc      *sStmtDesc;
    ulnDescRec   *sDescRec;
    ulnDbc       *sDbc;
    ulnStmt      *sStmt;

    ACP_UNUSED(aBufferLength);

    ULN_FNCONTEXT_GET_DBC(aFnContext, sDbc);
    ACI_TEST_RAISE(sDbc == NULL, InvalidHandleException);  /* BUG-46113 */

    sDesc    = aFnContext->mHandle.mDesc;
    sDescRec = ulnDescGetDescRec(sDesc, aRecNumber);

    // fix BUG-24380
    // ���ε� ���� ���ڵ� �ѹ��� ���ؼ� SetDescField() �� �ϸ�
    // �� DescRec �� �����ؼ� ����

    if (sDescRec == NULL)
    {
        sStmt = (ulnStmt*)sDesc->mStmt;
        ACI_TEST_RAISE(sStmt == NULL, LABEL_INVALID_DESC_INDEX);

        switch(aDescType)
        {
            case ULN_DESC_TYPE_ARD:
                sStmtDesc = sStmt->mAttrArd;
                break;
            // fix BUG-20745 BIND_OFFSET_PTR ����
            case ULN_DESC_TYPE_IRD:
                sStmtDesc = sStmt->mAttrIrd;
                break;
            case ULN_DESC_TYPE_IPD:
                sStmtDesc = sStmt->mAttrIpd;
                break;
            // fix BUG-24874 APD�� ���� �Ҵ�� execute�� ���� �߻�
            case ULN_DESC_TYPE_APD:
            default:
                ACI_RAISE(LABEL_INVALID_DESC_INDEX);
                break;
        }

        //
        // Desc record �غ�
        //
        ACI_TEST_RAISE(ulnBindArrangeNewDescRec(sStmtDesc, aRecNumber, &sDescRec)
                       != ACI_SUCCESS,
                       LABEL_NOT_ENOUGH_MEM);

        ulnMetaInitialize(&sDescRec->mMeta);
        ulnDescRecSetParamInOut(sDescRec, ULN_PARAM_INOUT_TYPE_MAX);

        //
        // Desc record �� Stmt Desc�� �Ŵޱ�
        //
        ACI_TEST_RAISE(ulnDescAddDescRec(sStmtDesc, sDescRec) != ACI_SUCCESS,
                       LABEL_NOT_ENOUGH_MEM);

    }

    sValue = (acp_uint32_t)(((acp_ulong_t)aValuePtr) & ACP_UINT32_MAX);

    switch(aFieldIdentifier)
    {
        case SQL_DESC_DATA_PTR:
            /*
             * BUGBUG : IRD, IPD �� �� mDataPtr �� �����ϴ� ����, consistency check �� prompt
             *          �Ѵٰų� ����ų� �ò����� M$ odbc �Ŵ��󿡼� ����� ��µ�,
             *          �ϴ��� �׳� ����.
             */
            if(aDescType == ULN_DESC_TYPE_ARD || aDescType == ULN_DESC_TYPE_APD)
            {
                ulnBindInfoSetType(&sDescRec->mBindInfo, ULN_MTYPE_MAX);
                sDescRec->mDataPtr = (void *)aValuePtr;
            }
            break;

        case SQL_DESC_AUTO_UNIQUE_VALUE:
        case SQL_DESC_BASE_COLUMN_NAME:
        case SQL_DESC_BASE_TABLE_NAME:
        case SQL_DESC_CASE_SENSITIVE:
        case SQL_DESC_CATALOG_NAME:
        case SQL_DESC_CONCISE_TYPE:
        case SQL_DESC_DATETIME_INTERVAL_CODE:
            break;
        case SQL_DESC_DATETIME_INTERVAL_PRECISION:
            break;
        case SQL_DESC_DISPLAY_SIZE:
        case SQL_DESC_FIXED_PREC_SCALE:
            break;

        case SQL_DESC_INDICATOR_PTR:
            if(ULN_OBJ_GET_DESC_TYPE(sDesc) == ULN_DESC_TYPE_ARD ||
               ULN_OBJ_GET_DESC_TYPE(sDesc) == ULN_DESC_TYPE_APD)
            {
                ulnDescRecSetIndicatorPtr(sDescRec, (ulvSLen *)aValuePtr);
            }
            break;

        case SQL_DESC_LABEL:
            break;
        case SQL_DESC_LENGTH:
            break;
        case SQL_DESC_LITERAL_PREFIX:
        case SQL_DESC_LITERAL_SUFFIX:
        case SQL_DESC_LOCAL_TYPE_NAME:
        case SQL_DESC_NAME:
        case SQL_DESC_NULLABLE:
        case SQL_DESC_NUM_PREC_RADIX:
        case SQL_DESC_OCTET_LENGTH:
            break;

        case SQL_DESC_OCTET_LENGTH_PTR:
            if(ULN_OBJ_GET_DESC_TYPE(sDesc) == ULN_DESC_TYPE_ARD ||
               ULN_OBJ_GET_DESC_TYPE(sDesc) == ULN_DESC_TYPE_APD)
            {
                ulnDescRecSetOctetLengthPtr(sDescRec, (ulvSLen *)aValuePtr);
            }
            break;

        case SQL_DESC_PARAMETER_TYPE:
            break;

        case SQL_DESC_PRECISION:
            ulnBindInfoSetType(&sDescRec->mBindInfo, ULN_MTYPE_MAX);
            ulnMetaSetPrecision(&sDescRec->mMeta, sValue);

            if(ULN_OBJ_GET_DESC_TYPE(sDesc) == ULN_DESC_TYPE_IPD)
            {
                ulnMetaBuild4IpdByStmt( (ulnStmt*)sDesc->mStmt,
                                        aRecNumber,
                                        SQL_DESC_PRECISION,
                                        sValue );
                ulnMetaAdjustIpdByStmt( sDbc,
                                        (ulnStmt*)sDesc->mStmt,
                                        aRecNumber,
                                        sValue );

                ACI_TEST( ulnBindAdjustStmtInfo( (ulnStmt*)sDesc->mStmt )
                          != ACI_SUCCESS );
            }
            break;

        case SQL_DESC_SCALE:
            ulnMetaSetScale(&sDescRec->mMeta, sValue);

            if(ULN_OBJ_GET_DESC_TYPE(sDesc) == ULN_DESC_TYPE_IPD)
            {
                ulnMetaBuild4IpdByStmt( (ulnStmt*)sDesc->mStmt,
                                        aRecNumber,
                                        SQL_DESC_SCALE,
                                        sValue );
                ulnMetaAdjustIpdByStmt( sDbc,
                                        (ulnStmt*)sDesc->mStmt,
                                        aRecNumber,
                                        sValue );

                ACI_TEST( ulnBindAdjustStmtInfo( (ulnStmt*)sDesc->mStmt )
                          != ACI_SUCCESS );
            }
            break;

        case SQL_DESC_ROWVER:
            break;

        case SQL_DESC_SCHEMA_NAME:
        case SQL_DESC_SEARCHABLE:
        case SQL_DESC_TABLE_NAME:
            break;

        case SQL_DESC_TYPE:
            if(ULN_OBJ_GET_DESC_TYPE(sDesc) == ULN_DESC_TYPE_ARD ||
               ULN_OBJ_GET_DESC_TYPE(sDesc) == ULN_DESC_TYPE_APD)
            {
                ACI_TEST_RAISE(ulnTypeMap_SQLC_CTYPE(sValue) == ULN_CTYPE_MAX,
                               LABEL_INVALID_SQL_C_TYPE);

                ulnMetaSetCTYPE(&sDescRec->mMeta, ulnTypeMap_SQLC_CTYPE(sValue));
            }
            else if(ULN_OBJ_GET_DESC_TYPE(sDesc) == ULN_DESC_TYPE_IRD ||
                    ULN_OBJ_GET_DESC_TYPE(sDesc) == ULN_DESC_TYPE_IPD)
            {
                ACI_TEST_RAISE(ulnTypeMap_SQL_MTYPE(sValue) == ULN_MTYPE_MAX,
                               LABEL_INVALID_SQL_TYPE);

                ulnMetaSetMTYPE(&sDescRec->mMeta, ulnTypeMap_SQL_MTYPE(sValue));
                if( ULN_OBJ_GET_DESC_TYPE(sDesc) == ULN_DESC_TYPE_IPD )
                {
                    ulnMetaBuild4IpdByStmt( (ulnStmt*)sDesc->mStmt,
                                            aRecNumber,
                                            SQL_DESC_TYPE,
                                            sValue );
                    ulnMetaAdjustIpdByStmt( sDbc,
                                            (ulnStmt*)sDesc->mStmt,
                                            aRecNumber,
                                            sValue );

                    ACI_TEST( ulnBindAdjustStmtInfo( (ulnStmt*)sDesc->mStmt )
                              != ACI_SUCCESS );
                }
            }
            else
            {
                /*
                 * �Լ� ���Խ� üũ�Ѵ�.
                 */
            }
            break;

        case SQL_DESC_TYPE_NAME:
        case SQL_DESC_UNNAMED:
        case SQL_DESC_UNSIGNED:
        case SQL_DESC_UPDATABLE:
            break;

        default:
            ACI_RAISE(LABEL_MEM_MAN_ERR);
            break;
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION(LABEL_NOT_ENOUGH_MEM)
    {
        ulnError(aFnContext, ulERR_FATAL_MEMORY_ALLOC_ERROR, "ulnSetDescRecordField");
    }
    ACI_EXCEPTION(LABEL_INVALID_SQL_TYPE)
    {
        ulnError(aFnContext, ulERR_ABORT_INVALID_SQL_TYPE, sValue);
    }
    ACI_EXCEPTION(LABEL_INVALID_SQL_C_TYPE)
    {
        /*
         * HY003
         */
        ulnError(aFnContext, ulERR_ABORT_INVALID_APP_BUFFER_TYPE, sValue);
    }
    ACI_EXCEPTION(LABEL_INVALID_DESC_INDEX)
    {
        ulnError(aFnContext, ulERR_ABORT_INVALID_DESCRIPTOR_INDEX, aRecNumber);
    }
    ACI_EXCEPTION(LABEL_MEM_MAN_ERR)
    {
        ulnError(aFnContext, ulERR_FATAL_MEMORY_MANAGEMENT_ERROR, "");
    }
    ACI_EXCEPTION(InvalidHandleException)
    {
        ULN_FNCONTEXT_SET_RC(aFnContext, SQL_INVALID_HANDLE);
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

SQLRETURN ulnSetDescField(ulnDesc      *aDesc,
                          acp_sint16_t  aRecNumber,
                          acp_sint16_t  aFieldIdentifier,
                          void         *aValuePtr,
                          acp_sint32_t  aBufferLength)
{
    ULN_FLAG(sNeedExit);

    ulnFnContext sFnContext;
    ulnDescType  sDescType;

    ULN_INIT_FUNCTION_CONTEXT(sFnContext, ULN_FID_SETDESCFIELD, aDesc, ULN_OBJ_TYPE_DESC);

    /*
     * Enter
     */
    ACI_TEST(ulnEnter(&sFnContext, NULL) != ACI_SUCCESS);
    ULN_FLAG_UP(sNeedExit);

    sDescType = ULN_OBJ_GET_DESC_TYPE(aDesc);

    ACI_TEST_RAISE(sDescType != ULN_DESC_TYPE_ARD &&
                   sDescType != ULN_DESC_TYPE_APD &&
                   sDescType != ULN_DESC_TYPE_IRD &&
                   sDescType != ULN_DESC_TYPE_IPD,
                   LABEL_INVALID_OBJECT_TYPE);

    switch(aFieldIdentifier)
    {
        case SQL_DESC_ALLOC_TYPE:
        case SQL_DESC_ARRAY_SIZE:
        case SQL_DESC_ARRAY_STATUS_PTR:
        case SQL_DESC_BIND_OFFSET_PTR:
        case SQL_DESC_BIND_TYPE:
        case SQL_DESC_COUNT:
        case SQL_DESC_ROWS_PROCESSED_PTR:
            ACI_TEST(ulnSetDescHeaderField(&sFnContext,
                                           aFieldIdentifier,
                                           aValuePtr,
                                           aBufferLength,
                                           sDescType) != ACI_SUCCESS);
            break;

        default:
            ACI_TEST(ulnSetDescRecordField(&sFnContext,
                                           aRecNumber,
                                           aFieldIdentifier,
                                           aValuePtr,
                                           aBufferLength,
                                           sDescType) != ACI_SUCCESS);
            break;
    }

    /*
     * Exit
     */
    ULN_FLAG_DOWN(sNeedExit);
    ACI_TEST(ulnExit(&sFnContext) != ACI_SUCCESS);

    return ULN_FNCONTEXT_GET_RC(&sFnContext);

    ACI_EXCEPTION(LABEL_INVALID_OBJECT_TYPE)
    {
        ulnError(&sFnContext, ulERR_ABORT_INVALID_HANDLE);
    }
    ACI_EXCEPTION_END;

    ULN_IS_FLAG_UP(sNeedExit)
    {
        ulnExit(&sFnContext);
    }

    return ULN_FNCONTEXT_GET_RC(&sFnContext);
}

