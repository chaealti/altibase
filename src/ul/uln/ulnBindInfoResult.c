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
#include <ulnBindParameter.h>
#include <ulnBindParamDataIn.h>

typedef void (*ulnDescRecSetNameFunc)(ulnDescRec *aRecord, acp_char_t *aName, acp_size_t aNameLen);

/**
 * cm�� �̿��� Name �Ӽ�(ColumnName, TableName ��)�� �����Ѵ�.
 *
 * @param[in] aProtocolContext    CM Protocol Context
 * @param[in] aDescRecIrd         IRD Record
 * @param[in] aSetNameFunc        Name �Ӽ��� ������ �Լ�
 * @param[in] aFnContext          Function Context
 *
 * @return �����ϸ� ACI_SUCCESS, �ƴϸ� ACI_FAILURE
 */
static ACI_RC ulnBindSetDescRecName( cmiProtocolContext    *aProtocolContext,
                                     ulnDescRec            *aDescRecIrd,
                                     ulnDescRecSetNameFunc  aSetNameFunc,
                                     ulnFnContext          *aFnContext)
{

    ulnDbc       *sDbc;
    acp_char_t    sNameBefore[256]; // at least 51 bytes for DisplayName
    acp_uint8_t   sNameBeforeLen;
    ulnCharSet    sCharSet;
    acp_sint32_t  sState = 0;

    acp_char_t   *sNameAfter;
    acp_sint32_t  sNameAfterLen;


    ULN_FNCONTEXT_GET_DBC(aFnContext, sDbc);

    CMI_RD1(aProtocolContext, sNameBeforeLen);
    CMI_RCP(aProtocolContext, sNameBefore, sNameBeforeLen );
    sNameBefore[sNameBeforeLen] = 0;

    ulnCharSetInitialize(&sCharSet);
    sState = 1;

    /* bug-37434 charset conversion for korean column name.
       DB charset column name�� client charset���� ��ȯ.
       data flow: cmBlock -> sNameBefore -> sNameAfter */
    ACI_TEST(ulnCharSetConvert(&sCharSet,
                aFnContext,
                NULL,
                (const mtlModule*)sDbc->mCharsetLangModule,
                sDbc->mClientCharsetLangModule,
                (void*)sNameBefore,
                (acp_sint32_t)sNameBeforeLen,
                CONV_DATA_IN)
            != ACI_SUCCESS);


    sNameAfter = (acp_char_t*)ulnCharSetGetConvertedText(&sCharSet);
    sNameAfterLen = ulnCharSetGetConvertedTextLen(&sCharSet);

    aSetNameFunc(aDescRecIrd, sNameAfter, sNameAfterLen);

    sState = 0;
    ulnCharSetFinalize(&sCharSet);

    return ACI_SUCCESS;
    ACI_EXCEPTION_END;

    if (sState == 1)
    {
        ulnCharSetFinalize(&sCharSet);
    }
    return ACI_FAILURE;
}

ACI_RC ulnCallbackColumnInfoGetResult(cmiProtocolContext *aProtocolContext,
                                      cmiProtocol        *aProtocol,
                                      void               *aServiceSession,
                                      void               *aUserContext)
{
    ulnDbc        *sDbc;
    ulnStmt       *sStmt;
    ulnDescRec    *sDescRecIrd;
    ulnFnContext  *sFnContext;
    ulnMTypeID     sMTYPE;
    acp_uint16_t   sOrgCursor;

    acp_uint32_t   sStatementID;
    acp_uint16_t   sResultSetID;
    acp_uint16_t   sColumnNumber;
    acp_uint32_t   sDataType;
    acp_uint32_t   sLanguage;
    acp_uint8_t    sArguments;
    acp_uint32_t   sPrecision;
    acp_uint32_t   sScale;
    acp_uint8_t    sFlag;
    acp_uint8_t   sNameLen;

    ACP_UNUSED(aProtocol);
    ACP_UNUSED(aServiceSession);

    sFnContext = (ulnFnContext *)aUserContext;
    sStmt      = sFnContext->mHandle.mStmt;

    ULN_FNCONTEXT_GET_DBC(sFnContext, sDbc);

    CMI_RD4(aProtocolContext, &sStatementID);
    CMI_RD2(aProtocolContext, &sResultSetID);
    CMI_RD2(aProtocolContext, &sColumnNumber);
    CMI_RD4(aProtocolContext, &sDataType);
    CMI_RD4(aProtocolContext, &sLanguage);
    CMI_RD1(aProtocolContext, sArguments);
    CMI_RD4(aProtocolContext, &sPrecision);
    CMI_RD4(aProtocolContext, &sScale);
    CMI_RD1(aProtocolContext, sFlag);

    ACP_UNUSED(sArguments);

    sOrgCursor = aProtocolContext->mReadBlock->mCursor;

    ACI_TEST(ULN_OBJ_GET_TYPE(sStmt) != ULN_OBJ_TYPE_STMT);

    ACI_TEST(sStmt->mStatementID != sStatementID);
    ACI_TEST(sStmt->mCurrentResultSetID != sResultSetID);

    /*
     * IRD Record �غ�
     */
    ACI_TEST_RAISE(ulnBindArrangeNewDescRec(sStmt->mAttrIrd,
                                            sColumnNumber,
                                            &sDescRecIrd) != ACI_SUCCESS,
                   LABEL_NOT_ENOUGH_MEM);

    /*
     * Ÿ�� ���� ����
     */
    sMTYPE = ulnTypeMap_MTD_MTYPE(sDataType);

    ACI_TEST_RAISE(sMTYPE == ULN_MTYPE_MAX, LABEL_UNKNOWN_TYPEID);

    if (cmiGetLinkImpl(aProtocolContext) == CMI_LINK_IMPL_IPCDA)
    {
        ACI_TEST_RAISE(((sMTYPE==ULN_MTYPE_BLOB)||(sMTYPE==ULN_MTYPE_CLOB)),
                       LABEL_UNKNOWN_TYPEID);
    }
    else
    {
        /*do nothing.*/
    }

    ulnMetaBuild4Ird(sDbc,
                     &sDescRecIrd->mMeta,
                     sMTYPE,
                     sPrecision,
                     sScale,
                     sFlag);

    ulnMetaSetLanguage(&sDescRecIrd->mMeta, sLanguage);

    ulnDescRecSetSearchable(sDescRecIrd, ulnTypeGetInfoSearchable(sMTYPE));
    ulnDescRecSetLiteralPrefix(sDescRecIrd, ulnTypeGetInfoLiteralPrefix(sMTYPE));
    ulnDescRecSetLiteralSuffix(sDescRecIrd, ulnTypeGetInfoLiteralSuffix(sMTYPE));
    ulnDescRecSetTypeName(sDescRecIrd, ulnTypeGetInfoName(sMTYPE));

    /* DisplayName */
    ACI_TEST_RAISE( ulnBindSetDescRecName( aProtocolContext,
                                           sDescRecIrd,
                                           ulnDescRecSetDisplayName,
                                           sFnContext )
                    != ACI_SUCCESS, LABEL_NOT_ENOUGH_MEM);

    /* ColumnName */
    ACI_TEST_RAISE( ulnBindSetDescRecName( aProtocolContext,
                                           sDescRecIrd,
                                           ulnDescRecSetColumnName,
                                           sFnContext )
                    != ACI_SUCCESS, LABEL_NOT_ENOUGH_MEM);

    /* BaseColumnName */
    ACI_TEST_RAISE( ulnBindSetDescRecName( aProtocolContext,
                                           sDescRecIrd,
                                           ulnDescRecSetBaseColumnName,
                                           sFnContext )
                    != ACI_SUCCESS, LABEL_NOT_ENOUGH_MEM);

    /* TableName */
    ACI_TEST_RAISE( ulnBindSetDescRecName( aProtocolContext,
                                           sDescRecIrd,
                                           ulnDescRecSetTableName,
                                           sFnContext )
                    != ACI_SUCCESS, LABEL_NOT_ENOUGH_MEM);

    /* BaseTableName */
    ACI_TEST_RAISE( ulnBindSetDescRecName( aProtocolContext,
                                           sDescRecIrd,
                                           ulnDescRecSetBaseTableName,
                                           sFnContext )
                    != ACI_SUCCESS, LABEL_NOT_ENOUGH_MEM);

    /* SchemaName */
    ACI_TEST_RAISE( ulnBindSetDescRecName( aProtocolContext,
                                           sDescRecIrd,
                                           ulnDescRecSetSchemaName,
                                           sFnContext )
                    != ACI_SUCCESS, LABEL_NOT_ENOUGH_MEM);

    /* CatalogName */
    ACI_TEST_RAISE( ulnBindSetDescRecName( aProtocolContext,
                                           sDescRecIrd,
                                           ulnDescRecSetCatalogName,
                                           sFnContext )
                    != ACI_SUCCESS, LABEL_NOT_ENOUGH_MEM);

    /*
     * Display Size ����
     */
    ulnDescRecSetDisplaySize(sDescRecIrd, ulnTypeGetDisplaySize(sMTYPE, &sDescRecIrd->mMeta));

    /* PROJ-2616 */
    /* Get Max-Byte-Size in qciBindColumn. */
    if (cmiGetLinkImpl(aProtocolContext) == CMI_LINK_IMPL_IPCDA)
    {
        CMI_RD4(aProtocolContext, &sDescRecIrd->mMaxByteSize);
    }
    else
    {
        /* do nothing */
    }

    /*
     * IRD record �� IRD �� �߰��Ѵ�.
     */
    ACI_TEST_RAISE(ulnDescAddDescRec(sStmt->mAttrIrd, sDescRecIrd) != ACI_SUCCESS,
                   LABEL_NOT_ENOUGH_MEM);

    return ACI_SUCCESS;

    ACI_EXCEPTION(LABEL_UNKNOWN_TYPEID)
    {
        ulnError(sFnContext, ulERR_ABORT_UNHANDLED_MT_TYPE, sMTYPE);
    }

    ACI_EXCEPTION(LABEL_NOT_ENOUGH_MEM)
    {
        ulnError(sFnContext, ulERR_FATAL_MEMORY_ALLOC_ERROR, "ulnCallbackColumnInfoGetResult");
    }

    ACI_EXCEPTION_END;

    aProtocolContext->mReadBlock->mCursor = sOrgCursor;

    CMI_RD1(aProtocolContext, sNameLen);
    CMI_SKIP_READ_BLOCK(aProtocolContext, sNameLen);

    CMI_RD1(aProtocolContext, sNameLen);
    CMI_SKIP_READ_BLOCK(aProtocolContext, sNameLen);

    CMI_RD1(aProtocolContext, sNameLen);
    CMI_SKIP_READ_BLOCK(aProtocolContext, sNameLen);

    CMI_RD1(aProtocolContext, sNameLen);
    CMI_SKIP_READ_BLOCK(aProtocolContext, sNameLen);

    CMI_RD1(aProtocolContext, sNameLen);
    CMI_SKIP_READ_BLOCK(aProtocolContext, sNameLen);

    CMI_RD1(aProtocolContext, sNameLen);
    CMI_SKIP_READ_BLOCK(aProtocolContext, sNameLen);

    CMI_RD1(aProtocolContext, sNameLen);
    CMI_SKIP_READ_BLOCK(aProtocolContext, sNameLen);

    /*
     * Note : ACI_SUCCESS �� �����ϴ� ���� ���װ� �ƴϴ�.
     *        cm �� �ݹ��Լ��� ACI_FAILURE �� �����ϸ� communication error �� ��޵Ǿ� ������
     *        �����̴�.
     *
     *        ����Ǿ��� ����, Function Context �� ����� mSqlReturn �� �Լ� ���ϰ���
     *        ����ǰ� �� ���̸�, uln �� cmi ���� �Լ��� ulnReadProtocol() �Լ� �ȿ���
     *        Function Context �� mSqlReturn �� üũ�ؼ� ������ ��ġ�� ���ϰ� �� ���̴�.
     */
    return ACI_SUCCESS;
}

ACI_RC ulnCallbackColumnInfoGetListResult(cmiProtocolContext *aProtocolContext,
                                          cmiProtocol        *aProtocol,
                                          void               *aServiceSession,
                                          void               *aUserContext)
{
    ulnDbc        *sDbc;
    ulnStmt       *sStmt;
    ulnDescRec    *sDescRecIrd;
    ulnFnContext  *sFnContext;
    acp_uint32_t   i = 0;
    ulnMTypeID     sMTYPE;
    acp_uint16_t   sRemainSize;
    acp_uint16_t   sOrgCursor;

    acp_uint32_t   sStatementID;
    acp_uint16_t   sResultSetID;
    acp_uint16_t   sColumnCount;
    acp_uint32_t   sDataType;
    acp_uint32_t   sLanguage;
    acp_uint8_t    sArguments;
    acp_uint32_t   sPrecision;
    acp_uint32_t   sScale;
    acp_uint8_t    sFlag;
    acp_uint8_t   sNameLen;

    ACP_UNUSED(aProtocol);
    ACP_UNUSED(aServiceSession);

    sFnContext = (ulnFnContext *)aUserContext;
    sStmt      = sFnContext->mHandle.mStmt;

    ULN_FNCONTEXT_GET_DBC(sFnContext, sDbc);

    CMI_RD4(aProtocolContext, &sStatementID);
    CMI_RD2(aProtocolContext, &sResultSetID);
    CMI_RD2(aProtocolContext, &sColumnCount);

    ACI_TEST(ULN_OBJ_GET_TYPE(sStmt) != ULN_OBJ_TYPE_STMT);

    ACI_TEST(sStmt->mStatementID != sStatementID);
    ACI_TEST(sStmt->mCurrentResultSetID != sResultSetID);

    /* PROJ-1789 Updatable Scrollable Cursor */
    if (sStmt->mParentStmt != SQL_NULL_HSTMT) /* is RowsetStmt ? */
    {
        ACI_TEST_RAISE(ulnBindArrangeNewDescRec(sStmt->mAttrIrd, 0, &sDescRecIrd)
                       != ACI_SUCCESS, LABEL_NOT_ENOUGH_MEM);

        ACI_TEST_RAISE(ulnDescAddDescRec(sStmt->mAttrIrd, sDescRecIrd)
                       != ACI_SUCCESS, LABEL_NOT_ENOUGH_MEM);
    }

    for( i = 1; i <= sColumnCount; i++ )
    {
        sRemainSize = aProtocolContext->mReadBlock->mDataSize -
                      aProtocolContext->mReadBlock->mCursor;

        /* IPCDA�� �����ʹ� cmiProtocolContext���� mReadBlock�ȿ��� ���۵˴ϴ�.
         * �� ���۴� Split_Read ���� ������, ������ �ִ� ũ�⳻���� �� ����
         * WRITE �˴ϴ�. ����, cmiRecvNext�� ȣ������ �ʽ��ϴ�.
         */
        ACI_TEST_RAISE(cmiGetLinkImpl(aProtocolContext) == CMI_LINK_IMPL_IPCDA,
                       SkipRecvNext);

        if( sRemainSize == 0 )
        {
            ACI_TEST_RAISE( cmiRecvNext( aProtocolContext,
                                         sDbc->mConnTimeoutValue )
                            != ACI_SUCCESS, cm_error );
        }

        ACI_EXCEPTION_CONT(SkipRecvNext);
        sOrgCursor = aProtocolContext->mReadBlock->mCursor;

        CMI_RD4(aProtocolContext, &sDataType);
        CMI_RD4(aProtocolContext, &sLanguage);
        CMI_RD1(aProtocolContext, sArguments);
        CMI_RD4(aProtocolContext, &sPrecision);
        CMI_RD4(aProtocolContext, &sScale);
        CMI_RD1(aProtocolContext, sFlag);

        ACP_UNUSED(sArguments);

        /*
         * IRD Record �غ�
         */
        ACI_TEST_RAISE(ulnBindArrangeNewDescRec(sStmt->mAttrIrd,
                                                i,
                                                &sDescRecIrd) != ACI_SUCCESS,
                       LABEL_NOT_ENOUGH_MEM);

        /* DisplayName */
        ACI_TEST_RAISE( ulnBindSetDescRecName( aProtocolContext,
                                               sDescRecIrd,
                                               ulnDescRecSetDisplayName,
                                               sFnContext )
                        != ACI_SUCCESS, LABEL_NOT_ENOUGH_MEM);

        /* ColumnName */
        ACI_TEST_RAISE( ulnBindSetDescRecName( aProtocolContext,
                                               sDescRecIrd,
                                               ulnDescRecSetColumnName,
                                               sFnContext )
                        != ACI_SUCCESS, LABEL_NOT_ENOUGH_MEM);

        /* BaseColumnName */
        ACI_TEST_RAISE( ulnBindSetDescRecName( aProtocolContext,
                                               sDescRecIrd,
                                               ulnDescRecSetBaseColumnName,
                                               sFnContext )
                        != ACI_SUCCESS, LABEL_NOT_ENOUGH_MEM);

        /* TableName */
        ACI_TEST_RAISE( ulnBindSetDescRecName( aProtocolContext,
                                               sDescRecIrd,
                                               ulnDescRecSetTableName,
                                               sFnContext )
                        != ACI_SUCCESS, LABEL_NOT_ENOUGH_MEM);

        /* BaseTableName */
        ACI_TEST_RAISE( ulnBindSetDescRecName( aProtocolContext,
                                               sDescRecIrd,
                                               ulnDescRecSetBaseTableName,
                                               sFnContext )
                        != ACI_SUCCESS, LABEL_NOT_ENOUGH_MEM);

        /* SchemaName */
        ACI_TEST_RAISE( ulnBindSetDescRecName( aProtocolContext,
                                               sDescRecIrd,
                                               ulnDescRecSetSchemaName,
                                               sFnContext )
                        != ACI_SUCCESS, LABEL_NOT_ENOUGH_MEM);

        /* CatalogName */
        ACI_TEST_RAISE( ulnBindSetDescRecName( aProtocolContext,
                                               sDescRecIrd,
                                               ulnDescRecSetCatalogName,
                                               sFnContext )
                        != ACI_SUCCESS, LABEL_NOT_ENOUGH_MEM);

        /*
         * Ÿ�� ���� ����
         */
        sMTYPE = ulnTypeMap_MTD_MTYPE(sDataType);

        ACI_TEST_RAISE(sMTYPE == ULN_MTYPE_MAX, LABEL_UNKNOWN_TYPEID);

        if (cmiGetLinkImpl(aProtocolContext) == CMI_LINK_IMPL_IPCDA)
        {
            ACI_TEST_RAISE(((sMTYPE==ULN_MTYPE_BLOB)||(sMTYPE==ULN_MTYPE_CLOB)), LABEL_UNKNOWN_TYPEID);
        }

        ulnMetaBuild4Ird(sDbc,
                         &sDescRecIrd->mMeta,
                         sMTYPE,
                         sPrecision,
                         sScale,
                         sFlag);

        ulnMetaSetLanguage(&sDescRecIrd->mMeta, sLanguage);

        ulnDescRecSetSearchable(sDescRecIrd, ulnTypeGetInfoSearchable(sMTYPE));
        ulnDescRecSetLiteralPrefix(sDescRecIrd, ulnTypeGetInfoLiteralPrefix(sMTYPE));
        ulnDescRecSetLiteralSuffix(sDescRecIrd, ulnTypeGetInfoLiteralSuffix(sMTYPE));
        ulnDescRecSetTypeName(sDescRecIrd, ulnTypeGetInfoName(sMTYPE));

        /*
         * Display Size ����
         */
        ulnDescRecSetDisplaySize(sDescRecIrd, ulnTypeGetDisplaySize(sMTYPE, &sDescRecIrd->mMeta));

        /* PROJ-2616 */
        /* Get Max-Byte-Size in qciBindColumn. */
        if (cmiGetLinkImpl(aProtocolContext) == CMI_LINK_IMPL_IPCDA)
        {
            CMI_RD4(aProtocolContext, &sDescRecIrd->mMaxByteSize);
        }
        else
        {
            /* do nothing */
        }


        /*
         * IRD record �� IRD �� �߰��Ѵ�.
         */
        ACI_TEST_RAISE(ulnDescAddDescRec(sStmt->mAttrIrd, sDescRecIrd) != ACI_SUCCESS,
                       LABEL_NOT_ENOUGH_MEM);
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION(LABEL_UNKNOWN_TYPEID)
    {
        ulnError(sFnContext, ulERR_ABORT_UNHANDLED_MT_TYPE, sMTYPE);
    }
    ACI_EXCEPTION(LABEL_NOT_ENOUGH_MEM)
    {
        ulnError(sFnContext, ulERR_FATAL_MEMORY_ALLOC_ERROR, "ulnCallbackColumnInfoGetResult");
    }
    ACI_EXCEPTION(cm_error)
    {
        return ACI_FAILURE;
    }
    ACI_EXCEPTION_END;

    if( i == 0 )
    {
        i = 1;
    }
    else
    {
        aProtocolContext->mReadBlock->mCursor = sOrgCursor;
    }

    for( ; i <= sColumnCount; i++ )
    {
        sRemainSize = aProtocolContext->mReadBlock->mDataSize -
                      aProtocolContext->mReadBlock->mCursor;

        /* PROJ-2616 */
        if(cmiGetLinkImpl(aProtocolContext) != CMI_LINK_IMPL_IPCDA)
        {
            if( sRemainSize == 0 )
            {
                ACI_TEST_RAISE( cmiRecvNext( aProtocolContext,
                                             sDbc->mConnTimeoutValue )
                                != ACI_SUCCESS, cm_error );
            }
        }

        CMI_SKIP_READ_BLOCK(aProtocolContext, 18);

        CMI_RD1(aProtocolContext, sNameLen);
        CMI_SKIP_READ_BLOCK(aProtocolContext, sNameLen);

        CMI_RD1(aProtocolContext, sNameLen);
        CMI_SKIP_READ_BLOCK(aProtocolContext, sNameLen);

        CMI_RD1(aProtocolContext, sNameLen);
        CMI_SKIP_READ_BLOCK(aProtocolContext, sNameLen);

        CMI_RD1(aProtocolContext, sNameLen);
        CMI_SKIP_READ_BLOCK(aProtocolContext, sNameLen);

        CMI_RD1(aProtocolContext, sNameLen);
        CMI_SKIP_READ_BLOCK(aProtocolContext, sNameLen);

        CMI_RD1(aProtocolContext, sNameLen);
        CMI_SKIP_READ_BLOCK(aProtocolContext, sNameLen);

        CMI_RD1(aProtocolContext, sNameLen);
        CMI_SKIP_READ_BLOCK(aProtocolContext, sNameLen);

        if (cmiGetLinkImpl(aProtocolContext) == CMI_LINK_IMPL_IPCDA)
        {
            CMI_SKIP_READ_BLOCK(aProtocolContext, 4);
        }
        else
        {
            /* do nothing */
        }
    }

    /*
     * Note : ACI_SUCCESS �� �����ϴ� ���� ���װ� �ƴϴ�.
     *        cm �� �ݹ��Լ��� ACI_FAILURE �� �����ϸ� communication error �� ��޵Ǿ� ������
     *        �����̴�.
     *
     *        ����Ǿ��� ����, Function Context �� ����� mSqlReturn �� �Լ� ���ϰ���
     *        ����ǰ� �� ���̸�, uln �� cmi ���� �Լ��� ulnReadProtocol() �Լ� �ȿ���
     *        Function Context �� mSqlReturn �� üũ�ؼ� ������ ��ġ�� ���ϰ� �� ���̴�.
     */
    return ACI_SUCCESS;
}

ACI_RC ulnCallbackParamInfoGetResult(cmiProtocolContext *aProtocolContext,
                                     cmiProtocol        *aProtocol,
                                     void               *aServiceSession,
                                     void               *aUserContext)
{
    ulnFnContext  *sFnContext = (ulnFnContext *)aUserContext;
    ulnStmt       *sStmt      = sFnContext->mHandle.mStmt;

    ulnDesc       *sDescIpd    = NULL;
    ulnDescRec    *sDescRecIpd = NULL;

    ulnMTypeID     sMTYPE;

    acp_uint32_t        sStatementID;
    acp_uint16_t        sParamNumber;
    acp_uint32_t        sDataType;
    acp_uint32_t        sLanguage;
    acp_uint8_t         sArguments;
    acp_sint32_t        sPrecision;
    acp_sint32_t        sScale;
    acp_uint8_t         sNullableFlag;
    acp_uint8_t         sCmInOutType;
    ulnParamInOutType   sUlnInOutType;

    ACP_UNUSED(aProtocol);
    ACP_UNUSED(aServiceSession);

    ACI_TEST(ULN_OBJ_GET_TYPE(sStmt) != ULN_OBJ_TYPE_STMT);

    CMI_RD4(aProtocolContext, &sStatementID);
    CMI_RD2(aProtocolContext, &sParamNumber);
    CMI_RD4(aProtocolContext, &sDataType);
    CMI_RD4(aProtocolContext, &sLanguage);
    CMI_RD1(aProtocolContext, sArguments);
    CMI_RD4(aProtocolContext, (acp_uint32_t*)&sPrecision);
    CMI_RD4(aProtocolContext, (acp_uint32_t*)&sScale);
    CMI_RD1(aProtocolContext, sCmInOutType);
    CMI_RD1(aProtocolContext, sNullableFlag);

    ACP_UNUSED(sArguments);
    ACP_UNUSED(sNullableFlag);

    ACI_TEST(sStmt->mStatementID != sStatementID);

    /*
     * IPD Record �غ�
     */
    sDescIpd    = ulnStmtGetIpd(sStmt);
    ACI_TEST_RAISE(sDescIpd == NULL, LABEL_MEM_MANAGE_ERR);

    sDescRecIpd = ulnStmtGetIpdRec(sStmt, sParamNumber);

    if (sDescRecIpd == NULL)
    {
        /*
         * ����ڰ� ���ε����� ���� �Ķ���Ϳ� ���ؼ��� IPD record ���� �� ���� ����
         */
        ACI_TEST_RAISE(ulnDescRecCreate(sDescIpd,
                                        &sDescRecIpd,
                                        sParamNumber) != ACI_SUCCESS,
                       LABEL_NOT_ENOUGH_MEM);

        ulnDescRecInitialize(sDescIpd, sDescRecIpd, sParamNumber);
        ulnDescRecInitOutParamBuffer(sDescRecIpd);
        ulnDescRecInitLobArray(sDescRecIpd);
        ulnBindInfoInitialize(&sDescRecIpd->mBindInfo);

        /*
         * Ÿ�� ����
         */
        sMTYPE = ulnTypeMap_MTD_MTYPE(sDataType);
        ACI_TEST_RAISE(sMTYPE == ULN_MTYPE_MAX, LABEL_UNKNOWN_TYPEID);

        if (cmiGetLinkImpl(aProtocolContext) == CMI_LINK_IMPL_IPCDA)
        {
            ACI_TEST_RAISE(((sMTYPE==ULN_MTYPE_BLOB)||(sMTYPE==ULN_MTYPE_CLOB)), LABEL_UNKNOWN_TYPEID);
        }

        /*
         * ulnMeta �ʱ�ȭ �� ����
         */
        ulnMetaInitialize(&sDescRecIpd->mMeta);

        /*
         * BUGBUG : �Ʒ�ó��, columnsize �� mPrecision ��, decimal digits �� mScale ��
         *          ������ ū ������ ���� ��κ� ����ǰ�����,
         *          DATE ������ �� �ǹ̰� �ٲ�� ���� �ִ�.
         *
         *          �׷���, ����, DATE ���� Ÿ���� precision, scale ������ ��Ƽ���̽�����
         *          ����.
         *
         *          �ϴ� �̴�� ����.
         */
        ulnMetaBuild4IpdByMeta(&sDescRecIpd->mMeta,
                               sMTYPE,
                               ulnTypeMap_MTYPE_SQL(sMTYPE),
                               sPrecision,
                               sScale);

        ulnMetaSetLanguage(&sDescRecIpd->mMeta, sLanguage);

        ulnDescRecSetSearchable(sDescRecIpd, ulnTypeGetInfoSearchable(sMTYPE));
        ulnDescRecSetLiteralPrefix(sDescRecIpd, ulnTypeGetInfoLiteralPrefix(sMTYPE));
        ulnDescRecSetLiteralSuffix(sDescRecIpd, ulnTypeGetInfoLiteralSuffix(sMTYPE));
        ulnDescRecSetTypeName(sDescRecIpd, ulnTypeGetInfoName(sMTYPE));

        /* BUG-42521 */
        sUlnInOutType = ulnBindInfoConvUlnParamInOutType(sCmInOutType);
        (void) ulnDescRecSetParamInOut(sDescRecIpd, sUlnInOutType);

        /*
         * Display Size ����
         */
        ulnDescRecSetDisplaySize(sDescRecIpd, ulnTypeGetDisplaySize(sMTYPE, &sDescRecIpd->mMeta));

        /*
         * IPD record �� IPD �� �Ŵ޾��ش�.
         */
        ACI_TEST_RAISE(ulnDescAddDescRec(sDescIpd, sDescRecIpd) != ACI_SUCCESS,
                       LABEL_NOT_ENOUGH_MEM);
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION(LABEL_MEM_MANAGE_ERR)
    {
        /*
         * �Ҵ��ߴ� �޸� ������ �������� ����. ������ �޸� ������.
         */
        ulnError(sFnContext,
                 ulERR_FATAL_MEMORY_MANAGEMENT_ERROR,
                 "ulnCallbackParamInfoGetResult");
    }

    ACI_EXCEPTION(LABEL_NOT_ENOUGH_MEM)
    {
        ulnError(sFnContext, ulERR_FATAL_MEMORY_ALLOC_ERROR, "ulnCallbackParamInfoGetResult");
    }

    ACI_EXCEPTION(LABEL_UNKNOWN_TYPEID)
    {
        ulnError(sFnContext, ulERR_ABORT_UNHANDLED_MT_TYPE, sDataType);
    }

    ACI_EXCEPTION_END;

    /*
     * Note : ACI_SUCCESS �� �����ϴ� ���� ���װ� �ƴϴ�.
     *        cm �� �ݹ��Լ��� ACI_FAILURE �� �����ϸ� communication error �� ��޵Ǿ� ������
     *        �����̴�.
     *
     *        ����Ǿ��� ����, Function Context �� ����� mSqlReturn �� �Լ� ���ϰ���
     *        ����ǰ� �� ���̸�, uln �� cmi ���� �Լ��� ulnReadProtocol() �Լ� �ȿ���
     *        Function Context �� mSqlReturn �� üũ�ؼ� ������ ��ġ�� ���ϰ� �� ���̴�.
     */
    return ACI_SUCCESS;
}

ACI_RC ulnCallbackParamInfoSetListResult(cmiProtocolContext *aProtocolContext,
                                             cmiProtocol        *aProtocol,
                                             void               *aServiceSession,
                                             void               *aUserContext)
{
    ACP_UNUSED(aProtocolContext);
    ACP_UNUSED(aProtocol);
    ACP_UNUSED(aServiceSession);
    ACP_UNUSED(aUserContext);

    return ACI_SUCCESS;
}
