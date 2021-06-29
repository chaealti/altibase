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
#include <ulnBindData.h>
#include <ulnPDContext.h>
#include <ulnConv.h>
#include <ulnCharSet.h>
#include <ulsdnBindData.h>
#include <ulsdDistTxInfo.h>

typedef ACI_RC ulnParamProcessFunc(ulnFnContext      *aFnContext,
                                   ulnPtContext      *aPtContext,
                                   ulnDescRec        *aDescRecApd,
                                   ulnDescRec        *aDescRecIpd,
                                   void              *aUserDataPtr,
                                   ulnIndLenPtrPair  *aUserIndLenPair,
                                   acp_uint32_t       aRowNumber);

static ACI_RC ulnParamProcess_DEFAULT(ulnFnContext      *aFnContext,
                                      ulnPtContext      *aPtContext,
                                      ulnDescRec        *aDescRecApd,
                                      ulnDescRec        *aDescRecIpd,
                                      void              *aUserDataPtr,
                                      ulnIndLenPtrPair  *aUserIndLenPair,
                                      acp_uint32_t       aRowNumber)
{
    ACP_UNUSED(aFnContext);
    ACP_UNUSED(aPtContext);
    ACP_UNUSED(aDescRecApd);
    ACP_UNUSED(aDescRecIpd);
    ACP_UNUSED(aUserDataPtr);
    ACP_UNUSED(aUserIndLenPair);
    ACP_UNUSED(aRowNumber);

    return ACI_SUCCESS;
}

static ACI_RC ulnParamProcess_DATA(ulnFnContext      *aFnContext,
                                   ulnPtContext      *aPtContext,
                                   ulnDescRec        *aDescRecApd,
                                   ulnDescRec        *aDescRecIpd,
                                   void              *aUserDataPtr,
                                   ulnIndLenPtrPair  *aUserIndLenPair,
                                   acp_uint32_t       aRowNumber)
{
    ulnStmt     *sStmt    = aFnContext->mHandle.mStmt;
    ulnCharSet   sCharSet;
    acp_sint32_t sUserOctetLength;
    acp_sint32_t sState = 0;

    ACP_UNUSED(aRowNumber);

    ulnCharSetInitialize(&sCharSet);
    sState = 1;

    if (aUserIndLenPair->mLengthPtr == NULL)
    {
        /*
         * ODBC spec. SQLBindParameter() StrLen_or_IndPtr argument �� ���� :
         * �̰� NULL �̸� ��� input parameter �� non-null �̰�,
         * char, binary �����ʹ� null-terminated �� ���̶�� �����ؾ� �Ѵ�.
         *
         * BUG-13704 SES �� -n �ɼ��� ���� ó��. �ڼ��� ������
         *           SQL_ATTR_INPUT_NTS �� ����� ��������� �ּ��� ������ ��.
         */
        if (ulnStmtGetAttrInputNTS(sStmt) == ACP_TRUE)
        {
            sUserOctetLength = SQL_NTS;
        }
        else
        {
            sUserOctetLength = ulnMetaGetOctetLength(&aDescRecApd->mMeta);
        }
    }
    else
    {
        sUserOctetLength = ulnBindGetUserIndLenValue(aUserIndLenPair);
    }

    /*
     * �� �Լ��� output parameter �� ���ؼ��� ����� ȣ����� �ʴ´�.
     *      --> ���� in/out Ÿ�� üũ �� �ʿ� ���� �Ƚ��ϰ� PARAM DATA IN �ص� �ȴ�.
     */

    ACI_TEST( aDescRecApd->mBindInfo.mParamDataInBuildAnyFunc( aFnContext,
                                                               aDescRecApd,
                                                               aDescRecIpd,
                                                               aUserDataPtr,
                                                               sUserOctetLength,
                                                               aRowNumber,
                                                               NULL,
                                                               &sCharSet) != ACI_SUCCESS );

    ACI_TEST(ulnWriteParamDataInREQ(aFnContext,
                                    aPtContext,
                                    aDescRecApd->mIndex,
                                    aUserDataPtr,
                                    sUserOctetLength,
                                    &aDescRecApd->mMeta,
                                    &aDescRecIpd->mMeta) != ACI_SUCCESS);

    ulnStmtChunkReset( sStmt );
    // BUGBUG : �̰� ���ֵ� �ǳ�?
    // aDescRecApd->mState = ULN_DR_ST_INITIAL;
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

static ACI_RC ulnParamProcess_DATA_AT_EXEC(ulnFnContext      *aFnContext,
                                           ulnPtContext      *aPtContext,
                                           ulnDescRec        *aDescRecApd,
                                           ulnDescRec        *aDescRecIpd,
                                           void              *aUserDataPtr,
                                           ulnIndLenPtrPair  *aUserIndLenPair,
                                           acp_uint32_t       aRowNumber)
{
    ulvSLen           sAccumulatedDataLength;
    ulnIndLenPtrPair  sUserIndLenPair;
    ulnPDContext     *sPDContext = &aDescRecApd->mPDContext;

    ACP_UNUSED(aUserDataPtr);
    ACP_UNUSED(aUserIndLenPair);

    // idlOS::printf("#### normal pd state = %d\n", ulnPDContextGetState(sPDContext));

    switch (ulnPDContextGetState(sPDContext))
    {
        case ULN_PD_ST_INITIAL:
            ACI_RAISE(LABEL_INVALID_STATE);
            break;

        case ULN_PD_ST_CREATED:     /* SQLExecute() ���� ���� */
        case ULN_PD_ST_NEED_DATA:   /* SQLParamData() ���� ���� */
            /*
             * ������ SQLPutData() ȣ��� �Ʒ��� �������� �߻� :
             *      ULN_PD_ST_NEED_DATA --> ULN_PD_ST_ACCUMULATING_DATA
             */
            ulnPDContextSetState(sPDContext, ULN_PD_ST_NEED_DATA);
            ACI_RAISE(LABEL_NEED_DATA);
            break;

        case ULN_PD_ST_ACCUMULATING_DATA:

            /*
             * SQLPutData() �� �̿��ؼ� �����͸� �װ� �ִ� state ��
             * SQLParamData() �� ȣ��Ǿ����Ƿ� �̰����� ����.
             * SQLParamData() ȣ���� ������ ���簡 �����ٴ� �ǹ��̹Ƿ� ����� ������ ����.
             */

            sAccumulatedDataLength        = ulnPDContextGetDataLength(sPDContext);
            sUserIndLenPair.mLengthPtr    = &sAccumulatedDataLength;
            sUserIndLenPair.mIndicatorPtr = &sAccumulatedDataLength;

            ACI_TEST_RAISE(ulnParamProcess_DATA(aFnContext,
                                                aPtContext,
                                                aDescRecApd,
                                                aDescRecIpd,
                                                ulnPDContextGetBuffer(sPDContext),
                                                &sUserIndLenPair,
                                                aRowNumber) != ACI_SUCCESS,
                           LABEL_NEED_PD_FIN);

            sPDContext->mOp->mFinalize(sPDContext);

            ulnDescRemovePDContext(aDescRecApd->mParentDesc, sPDContext);

            break;
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION(LABEL_INVALID_STATE)
    {
        ulnErrorExtended(aFnContext,
                         SQL_ROW_NUMBER_UNKNOWN,
                         aDescRecApd->mIndex,
                         ulERR_FATAL_MEMORY_MANAGEMENT_ERROR,
                         "ulnParamProcess_DATA_AT_EXEC");
    }

    ACI_EXCEPTION(LABEL_NEED_DATA)
    {
        ULN_FNCONTEXT_SET_RC(aFnContext, SQL_NEED_DATA);
    }

    ACI_EXCEPTION(LABEL_NEED_PD_FIN)
    {
        sPDContext->mOp->mFinalize(sPDContext);

        ulnDescRemovePDContext(aDescRecApd->mParentDesc, sPDContext);
    }

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

static ACI_RC ulnParamProcess_ERROR(ulnFnContext      *aFnContext,
                                    ulnPtContext      *aPtContext,
                                    ulnDescRec        *aDescRecApd,
                                    ulnDescRec        *aDescRecIpd,
                                    void              *aUserDataPtr,
                                    ulnIndLenPtrPair  *aUserIndLenPair,
                                    acp_uint32_t       aRowNumber)
{
    ACP_UNUSED(aFnContext);
    ACP_UNUSED(aPtContext);
    ACP_UNUSED(aDescRecApd);
    ACP_UNUSED(aDescRecIpd);
    ACP_UNUSED(aUserDataPtr);
    ACP_UNUSED(aUserIndLenPair);
    ACP_UNUSED(aRowNumber);

    /*
     * �� �Լ��� DATA AT EXEC �Ķ���͸� output ���� ���ε� ���� ��
     * ȣ��Ǵ� �Լ��̴�. ������ ������ �����ϰ� �г���.
     */

    /*
     * Note : OUTPUT �Ķ������ ������ StrLenOrIndPtr �� �����ϹǷ�
     *        DATA AT EXEC ���� ���� �� ������ ����.
     *
     *        �׳� ����.
     */

    return ACI_SUCCESS;
}

static ulnParamProcessFunc * const gUlnParamProcessorTable[ULN_PARAM_TYPE_MAX] =
{
    /* Mem    DATA   User       Build &            */
    /* Bound   AT    IN/OUT     Send      Send     */
    /* LOB    EXEC   Type       BindInfo  BindData */

    /* FALSE  FALSE   IN    */  ulnParamProcess_DATA,
    /* FALSE  FALSE   OUT   */  ulnParamProcess_DEFAULT,

    /* FALSE  TRUE    IN    */  ulnParamProcess_DATA_AT_EXEC,
    /* FALSE  TRUE    OUT   */  ulnParamProcess_ERROR,

    /* TRUE   FALSE   IN    */  ulnParamProcess_DEFAULT,
    /* TRUE   FALSE   OUT   */  ulnParamProcess_DEFAULT,

    /* TRUE   TRUE    IN    */  ulnParamProcess_DEFAULT,
    /* TRUE   TRUE    OUT   */  ulnParamProcess_ERROR
};

ACI_RC ulnBindDataWriteRow(ulnFnContext *aFnContext,
                           ulnPtContext *aPtContext,
                           ulnStmt      *aStmt,
                           acp_uint32_t  aRowNumber)  /* 0 ���̽� */
{
    acp_uint32_t sParamNumber;
    acp_uint16_t sNumberOfParams;      /* �Ķ������ ���� */

    ulnDescRec  *sDescRecIpd;
    ulnDescRec  *sDescRecApd;

    void             *sUserDataPtr;
    ulnIndLenPtrPair  sUserIndLenPair;
    ulnParamInOutType sInOutType;

    acp_uint8_t       sParamType = 0;

    sNumberOfParams = ulnBindCalcNumberOfParamsToSend(aStmt);

    for (sParamNumber = aStmt->mProcessingParamNumber;
         sParamNumber <= sNumberOfParams;
         sParamNumber++)
    {
        aStmt->mProcessingParamNumber = sParamNumber;

        /*
         * APD, IPD ȹ��
         */
        sDescRecApd = ulnStmtGetApdRec(aStmt, sParamNumber);
        ACI_TEST_RAISE(sDescRecApd == NULL, LABEL_NOT_BOUND);

        sDescRecIpd = ulnStmtGetIpdRec(aStmt, sParamNumber);
        ACI_TEST_RAISE(sDescRecIpd == NULL, LABEL_MEM_MANAGE_ERR);

        /*
         * sParamType ����
         */

        sParamType   = 0;
        sInOutType   = ulnDescRecGetParamInOut(sDescRecIpd);
        sUserDataPtr = ulnBindCalcUserDataAddr(sDescRecApd, aRowNumber);
        ulnBindCalcUserIndLenPair(sDescRecApd, aRowNumber, &sUserIndLenPair);

        if (ulnTypeIsMemBoundLob(ulnMetaGetMTYPE(&sDescRecIpd->mMeta),
                                 ulnMetaGetCTYPE(&sDescRecApd->mMeta)) == ACP_TRUE)
        {
            sParamType |= ULN_PARAM_TYPE_MEM_BOUND_LOB;
            aStmt->mHasMemBoundLobParam = ACP_TRUE;
        }

        if (ulnDescRecIsDataAtExecParam(sDescRecApd, aRowNumber) == ACP_TRUE)
        {
            /*
             * BUG-16491 output param �� ��� data at exec ��� ���� �ǹ̰� ����.
             *           StrLenOrInd �����Ͱ� ����Ű�� ���� ���� ��� �����̱� �����̴�.
             */

            if (sInOutType != ULN_PARAM_INOUT_TYPE_OUTPUT)
            {
                sParamType |= ULN_PARAM_TYPE_DATA_AT_EXEC;
                aStmt->mHasDataAtExecParam |= ULN_HAS_DATA_AT_EXEC_PARAM;

                if ((sParamType & ULN_PARAM_TYPE_MEM_BOUND_LOB) == ULN_PARAM_TYPE_MEM_BOUND_LOB)
                {
                    aStmt->mHasDataAtExecParam |= ULN_HAS_LOB_DATA_AT_EXEC_PARAM;
                }
            }
        }

        if (sInOutType == ULN_PARAM_INOUT_TYPE_OUTPUT)
        {
            sParamType |= ULN_PARAM_TYPE_OUT_PARAM;
        }

        /*
         * sParamType �� ���� �Ķ���� ó�� �Լ� ȣ��
         */

        ACI_TEST(gUlnParamProcessorTable[sParamType](aFnContext,
                                                     aPtContext,
                                                     sDescRecApd,
                                                     sDescRecIpd,
                                                     sUserDataPtr,
                                                     &sUserIndLenPair,
                                                     aRowNumber) != ACI_SUCCESS);

        /*
         * OUTPUT PARAMETER �� ��� ������
         */

        if (sInOutType == ULN_PARAM_INOUT_TYPE_OUTPUT ||
            sInOutType == ULN_PARAM_INOUT_TYPE_IN_OUT)
        {
            /*
             * Note : ����, ����ڰ� ������ OUTPUT Ȥ�� in/out �Ķ���Ϳ� ���ؼ�
             *        ������ BINDDATA OUT �� ���� ������ StrLen_or_IndPtr ��
             *        ����Ű�� ���� SQL_NULL_DATA �� �־��־�� �Ѵ�.
             *        ������ BINDDATA OUT �� üũ�Ϸ��� �����ϹǷ� �ϴ�
             *        BINDDATA IN �� �� �Ŀ� �̸� SQL_NULL_DATA �� �ʱ�ȭ ���ѵд�.
             *        �� �ִٰ� BINDDATA OUT �� ���� �ش� �޸𸮿� ���̸� ������ ���̴�.
             */
            ulnBindSetUserIndLenValue(&sUserIndLenPair, SQL_NULL_DATA);
        }

    } /* for (sParamNumber = ....) */

    // if (sParamNumber > sNumberOfParams)
    {
        aStmt->mProcessingParamNumber = 1;
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION(LABEL_MEM_MANAGE_ERR)
    {
        ulnErrorExtended(aFnContext,
                         aRowNumber + 1,
                         sParamNumber,
                         ulERR_FATAL_MEMORY_MANAGEMENT_ERROR,
                         "ulnBindDataWriteRow");
    }

    ACI_EXCEPTION(LABEL_NOT_BOUND)
    {
        /*
         * BUGBUG: �ش� �Ķ���Ϳ� ���� ���ε� ������ ����.
         * � ������ �ؾ� ���� ���ǵ��� �ʾƼ� general error �� �ߴ�.
         */
        ulnErrorExtended(aFnContext,
                         aRowNumber + 1,
                         sParamNumber,
                         ulERR_ABORT_PARAMETER_NOT_BOUND);
    }

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

/*
 * ==============================================
 *
 * BIND PARAM DATA OUT ó��
 *
 * ==============================================
 */

static ACI_RC ulnBindStoreLobLocator(ulnFnContext *aFnContext,
                                     ulnDescRec   *aDescRec,
                                     acp_uint16_t  aRowNumber,    /* 0 base */
                                     acp_uint8_t  *aData)
{
    ulnLob       *sLob = NULL;
    acp_uint64_t  sLobLocatorID;
    /* BUG-18945 */
    acp_uint64_t  sLobSize = 0;
    acp_uint8_t  *sSrc  = aData;
    acp_uint8_t  *sDest = (acp_uint8_t*)(&sLobLocatorID);
    /*
     * ����ڰ� memory Ȥ�� file �� ���ε��� lob column or parameter
     * ���޵Ǿ� �� lob locator �� IRD ���ο� �Ҵ��� �� lob array �� ulnLob ����ü��
     * ���õȴ�.
     *
     * �� �Լ������� ulnLob �� �ʱ�ȭ�ϰ�, locator �� �����Ѵ�.
     *
     * ulnExecSendLobParamData() �Լ����� ����ڰ� ���ε��� �����͸�
     * ������ �����Ѵ�.
     */
    sLob = ulnDescRecGetLobElement(aDescRec, aRowNumber);
    ACI_TEST_RAISE(sLob == NULL, LABEL_MEM_MANAGE_ERR);


    /*
     * Initialize lob
     */
    ACI_TEST_RAISE(ulnLobInitialize(sLob, ulnMetaGetMTYPE(&aDescRec->mMeta)) != ACI_SUCCESS,
                   LABEL_INVALID_LOB_TYPE);

    /*
     * locator ���
     */
    // proj_2160 cm_type removal
    CM_ENDIAN_ASSIGN8(sDest, sSrc);

    if(sLobLocatorID == MTD_LOCATOR_NULL )
    {
        sLobSize = 0;
    }
    else
    {
        sSrc  = aData + ACI_SIZEOF(acp_uint64_t);
        sDest = (acp_uint8_t*)(&sLobSize);

        /* PROJ-2047 Strengthening LOB - LOBCACHE */
        CM_ENDIAN_ASSIGN8(sDest, sSrc);
    }

    /*
     * locator ����
     */
    ACI_TEST_RAISE(sLob->mOp->mSetLocator(aFnContext, sLob, sLobLocatorID) != ACI_SUCCESS,
                   LABEL_INVALID_LOB_STATE);
    sLob->mSize = sLobSize;

    return ACI_SUCCESS;

    ACI_EXCEPTION(LABEL_INVALID_LOB_TYPE)
    {
        ulnErrorExtended(aFnContext,
                         aRowNumber + 1,
                         aDescRec->mIndex,
                         ulERR_FATAL_MEMORY_MANAGEMENT_ERROR,
                         "ulnBindStoreLobLocator : IPD record is not lob type");
    }

    ACI_EXCEPTION(LABEL_MEM_MANAGE_ERR)
    {
        ulnErrorExtended(aFnContext,
                         aRowNumber + 1,
                         aDescRec->mIndex,
                         ulERR_FATAL_MEMORY_MANAGEMENT_ERROR,
                         "ulnBindStoreLobLocator : Cannot find LOB locator");
    }

    ACI_EXCEPTION(LABEL_INVALID_LOB_STATE)
    {
        ulnErrorExtended(aFnContext,
                         aRowNumber + 1,
                         aDescRec->mIndex,
                         ulERR_FATAL_MEMORY_MANAGEMENT_ERROR,
                         "ulnBindStoreLobLocator : ulnLob is not in a state where "
                         "LOB locator can be set");
    }

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

static ACI_RC ulnBindProcssOutParamData(ulnFnContext   *aFnContext,
                                        ulnDescRec     *aDescRecApd,
                                        ulnDescRec     *aDescRecIpd,
                                        acp_uint16_t    aRowNumber,
                                        acp_uint16_t    aParamNumber,
                                        acp_uint8_t    *aData)
{
    ulnColumn        *sColumn;
    ulnConvFunction  *sFilter         = NULL;
    ulnLengthPair     sLengthPair     = {ULN_vLEN(0), ULN_vLEN(0)};

    ulnAppBuffer      sAppBuffer;
    ulnIndLenPtrPair  sUserIndLenPair;

    ulnStmt          *sStmt = aFnContext->mHandle.mStmt;

#ifdef COMPILE_SHARDCLI
    ulsdLobLocator   *sInBindLocator = NULL;
#endif

    /*
     * ����� ���� �ּ� �� ����
     */
    sAppBuffer.mCTYPE        = ulnMetaGetCTYPE(&aDescRecApd->mMeta);
    sAppBuffer.mBuffer       = (acp_uint8_t *)ulnBindCalcUserDataAddr(aDescRecApd,
                                                                      aRowNumber - 1);
    sAppBuffer.mBufferSize   = ulnMetaGetOctetLength(&aDescRecApd->mMeta);
    sAppBuffer.mColumnStatus = ULN_ROW_SUCCESS;

    ulnBindCalcUserIndLenPair(aDescRecApd, aRowNumber - 1, &sUserIndLenPair);

    /*
     * --------------------------------------
     * �ӽ� ulnColumn �� ������ ������ ����
     * --------------------------------------
     */
    sColumn = aDescRecIpd->mOutParamBuffer;
    ACI_TEST_RAISE(sColumn == NULL, LABEL_MEM_MANAGE_ERR);

    sColumn->mGDState       = 0;
    sColumn->mGDPosition    = 0;
    sColumn->mRemainTextLen = 0;
    sColumn->mBuffer        = (acp_uint8_t*)(sColumn + 1);

    if ( sStmt->mShardStmtCxt.mIsMtDataFetch == ACP_TRUE )
    {
        /* BUG-45392 MT type �״�� ���� */
        ACI_TEST( ulsdDataCopyFromMT(aFnContext,
                                     aData,
                                     sAppBuffer.mBuffer,
                                     sAppBuffer.mBufferSize,
                                     sColumn,
                                     sAppBuffer.mBufferSize )
                  != ACI_SUCCESS );
    }
    else
    {
        ACI_TEST(ulnDataBuildColumnFromMT(aFnContext,
                                          aData,
                                          sColumn) != ACI_SUCCESS);
    }

    /*
     * --------------------------------------
     * conversion �Ͽ� ����ڹ��ۿ� ����
     * --------------------------------------
     */

    /*
     * convert it!
     */
    if (sColumn->mDataLength == SQL_NULL_DATA)
    {
        if (ulnBindSetUserIndLenValue(&sUserIndLenPair, SQL_NULL_DATA) != ACI_SUCCESS)
        {
            /*
             * 22002 :
             *
             * NULL �� �÷��� fetch �Ǿ� �ͼ�, SQL_NULL_DATA �� ����ڰ� ������
             * StrLen_or_IndPtr �� �� ��� �ϴµ�, �̳༮�� NULL �������̴�.
             * �׷����� �߻����� �ִ� ����.
             */
            ulnErrorExtended(aFnContext,
                             aRowNumber,
                             aParamNumber,
                             ulERR_ABORT_INDICATOR_REQUIRED_BUT_NOT_SUPPLIED_ERR);
        }
    }
    else
    {
        if ( sStmt->mShardStmtCxt.mIsMtDataFetch == ACP_FALSE )
        {
            /*
             * conversion �Լ� ���ϱ�
             */
            sFilter = ulnConvGetFilter(aDescRecApd->mMeta.mCTYPE,
                                       (ulnMTypeID)sColumn->mMtype);
            ACI_TEST_RAISE(sFilter == NULL, LABEL_CONV_NOT_APPLICABLE);

#ifdef COMPILE_SHARDCLI
            /*
             * PROJ-2739 Client-side Sharding LOB
             *   For Locator InBind,
             *   �Ʒ� sFilter �Լ����� ����� ���۸� ����� ����
             *   InBind locator ���� ������ �д�.
             */
            if ( ulsdTypeIsLocInBoundLob(sAppBuffer.mCTYPE,
                                         ulnDescRecGetParamInOut(aDescRecIpd))
                 == ACP_TRUE )
            {
                sInBindLocator = (ulsdLobLocator *)(*(acp_uint64_t *)sAppBuffer.mBuffer);
            }
#endif

            if (sFilter(aFnContext,
                        &sAppBuffer,
                        sColumn,
                        &sLengthPair,
                        aRowNumber) == ACI_SUCCESS)
            {
                ulnBindSetUserIndLenValue(&sUserIndLenPair, sLengthPair.mNeeded);
            }
            else
            {
                // BUGBUG : param status ptr ��?
                // sAppBuffer.mColumnStatus = ULN_ROW_ERROR;
            }

#ifdef COMPILE_SHARDCLI
            /* PROJ-2739 Client-side Sharding LOB */
            if ( ulsdTypeIsLocatorCType(sAppBuffer.mCTYPE) == ACP_TRUE )
            {
                ACI_TEST_RAISE( ulsdLobAddOutBindLocator(
                                        sStmt,
                                        aParamNumber,
                                        ulnDescRecGetParamInOut(aDescRecIpd),
                                        sInBindLocator,
                                        (acp_uint64_t *)sAppBuffer.mBuffer )
                                != ACI_SUCCESS, LABEL_MEM_MANAGE_ERR );

            }
            else
            {
                /* Nothing to do */
            }
#endif
        }
        else
        {
            /* Nothing to do */
        }
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION(LABEL_CONV_NOT_APPLICABLE)
    {
        /* 07006 : Restricted data type attribute violation */
        ulnErrorExtended(aFnContext,
                         aRowNumber,
                         aParamNumber,
                         ulERR_ABORT_INVALID_CONVERSION);
    }

    ACI_EXCEPTION(LABEL_MEM_MANAGE_ERR)
    {
        ulnErrorExtended(aFnContext,
                         aRowNumber,
                         aParamNumber,
                         ulERR_FATAL_MEMORY_MANAGEMENT_ERROR,
                         "ulnBindProcssOutParamData");
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;

}

// bug-28259: ipc needs paramDataInResult
ACI_RC ulnCallbackParamDataInResult(cmiProtocolContext *aProtocolContext,
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

ACI_RC ulnCallbackParamDataOutList(cmiProtocolContext *aPtContext,
                                   cmiProtocol        *aProtocol,
                                   void               *aServiceSession,
                                   void               *aUserContext)
{
    ulnFnContext  *sFnContext = (ulnFnContext *)aUserContext;
    ulnDbc        *sDbc;
    ulnStmt       *sStmt      = sFnContext->mHandle.mStmt;
    acp_uint16_t   sNumberOfParams;
    // BUG-25315 [CodeSonar] �ʱ�ȭ���� �ʴ� ���� ���
    acp_uint16_t   i = 0;
    acp_uint32_t   sInOutType;
    acp_bool_t     sIsLob;
    acp_bool_t     sIsLocInBoundLob = ACP_FALSE;

    ulnDescRec    *sDescRecApd;
    ulnDescRec    *sDescRecIpd;

    acp_uint32_t        sStatementID;
    acp_uint32_t        sRowNumber;
    acp_uint32_t        sRowSize;
    acp_uint8_t        *sRow;

    ACP_UNUSED(aProtocol);
    ACP_UNUSED(aServiceSession);

    ULN_FNCONTEXT_GET_DBC(sFnContext, sDbc);

    /* BUG-46052 codesonar Null Pointer Dereference */
    ACI_TEST_RAISE(sDbc == NULL, InvalidHandleException); 

    CMI_RD4(aPtContext, &sStatementID);
    CMI_RD4(aPtContext, &sRowNumber);
    CMI_RD4(aPtContext, &sRowSize);

    ACI_TEST_RAISE(ULN_OBJ_GET_TYPE(sStmt) != ULN_OBJ_TYPE_STMT,
                   LABEL_OBJECT_TYPE_MISMATCH);

    ACI_TEST_RAISE(sStmt->mStatementID != sStatementID, LABEL_MEM_MANAGE_ERR);

    if (cmiGetLinkImpl(aPtContext) == CMI_LINK_IMPL_IPCDA)
    {
        /* PROJ-2616 �޸𸮿� �ٷ� �����Ͽ� �����͸� �е��� �Ѵ�. */
        ACI_TEST_RAISE( cmiSplitReadIPCDA(aPtContext,
                                          sRowSize,
                                          &sRow,
                                          NULL)
                        != ACI_SUCCESS, cm_error );

    }
    else
    {
        ACI_TEST( ulnStmtAllocChunk( sStmt,
                                     sRowSize,
                                     &sRow)
                  != ACI_SUCCESS );

        ACI_TEST_RAISE( cmiSplitRead( aPtContext,
                                      sRowSize,
                                      sRow,
                                      sDbc->mConnTimeoutValue )
                        != ACI_SUCCESS, cm_error );
    }
    sRowSize = 0;

    sNumberOfParams = ulnBindCalcNumberOfParamsToSend(sStmt);

    for( i = 1; i <= sNumberOfParams; i++ )
    {
        sDescRecApd = ulnStmtGetApdRec(sStmt, i);
        ACI_TEST_RAISE(sDescRecApd == NULL, LABEL_MEM_MANAGE_ERR);

        sDescRecIpd = ulnStmtGetIpdRec(sStmt, i);
        ACI_TEST_RAISE(sDescRecIpd == NULL, LABEL_MEM_MANAGE_ERR);

        sInOutType = ulnDescRecGetParamInOut(sDescRecIpd);
        sIsLob     = ulnTypeIsMemBoundLob(ulnMetaGetMTYPE(&sDescRecIpd->mMeta),
                                          ulnMetaGetCTYPE(&sDescRecApd->mMeta));
#ifdef COMPILE_SHARDCLI
        /*
         * PROJ-2739 Client-side Sharding LOB
         *   In -> Out���� �ٲ�� ���ε��� ��쿡��
         *   ulnBindProcssOutParamData���� ó���Ǿ�� �Ѵ�.
         */
        sIsLocInBoundLob = ( ulsdTypeIsLocInBoundLob(
                               ulnMetaGetCTYPE(&sDescRecApd->mMeta),
                               sInOutType) == ACP_TRUE ) &&
                           (sDescRecApd->mBindInfo.mInOutType == ULN_PARAM_INOUT_TYPE_OUTPUT);
#endif

        if( (sInOutType == ULN_PARAM_INOUT_TYPE_OUTPUT) ||
            (sInOutType == ULN_PARAM_INOUT_TYPE_IN_OUT) ||
            (sIsLob == ACP_TRUE) ||
            (sIsLocInBoundLob == ACP_TRUE) )
        {
            if (sIsLob == ACP_TRUE)
            {
                /*
                 * ------------------------------------
                 * LOB locator ���� (mem bound lob)
                 * ------------------------------------
                 *
                 * BindInfo �� �����ϸ鼭 �Ҵ��� lob array ���� ulnLob �� �ϳ� �����ͼ�
                 * Initialize �ϰ�, locator ����.
                 */
                ACI_TEST(ulnBindStoreLobLocator(sFnContext,
                                                sDescRecIpd,
                                                sRowNumber - 1,
                                                sRow)
                         != ACI_SUCCESS);

                /* PROJ-2047 Strengthening LOB - LOBCACHE */
                sRow += ACI_SIZEOF(acp_uint64_t) +
                        ACI_SIZEOF(acp_uint64_t) +
                        ACI_SIZEOF(acp_uint8_t);
            }
            else
            {
                /*
                 * ------------------------------------------------------------
                 * LOB �ƴ�, �Ϲ����� output parameter : locator binding ����
                 * ------------------------------------------------------------
                 */

                ACI_TEST(ulnBindProcssOutParamData(sFnContext,
                                                   sDescRecApd,
                                                   sDescRecIpd,
                                                   sRowNumber,
                                                   i,
                                                   sRow)
                         != ACI_SUCCESS);

                sRow += (sDescRecIpd->mOutParamBuffer->mMTLength);
            }
        }
    }

    return ACI_SUCCESS;

    /* BUG-46052 codesonar Null Pointer Dereference */
    ACI_EXCEPTION(InvalidHandleException)
    {
        ULN_FNCONTEXT_SET_RC(sFnContext, SQL_INVALID_HANDLE);
    }
    ACI_EXCEPTION(LABEL_OBJECT_TYPE_MISMATCH)
    {
        ulnError(sFnContext,
                 ulERR_FATAL_MEMORY_MANAGEMENT_ERROR,
                 "ulnCallbackParamDataOutList : Object type does not match");
    }

    ACI_EXCEPTION(LABEL_MEM_MANAGE_ERR)
    {
        ulnErrorExtended(sFnContext,
                         sRowNumber,
                         i,
                         ulERR_FATAL_MEMORY_MANAGEMENT_ERROR,
                         "ulnCallbackParamDataOutList");
    }
    ACI_EXCEPTION(cm_error)
    {
        return ACI_FAILURE;
    }
    ACI_EXCEPTION_END;

    if( sRowSize != 0 )
    {
        ACI_TEST_RAISE( cmiSplitSkipRead( aPtContext,
                                          sRowSize,
                                          sDbc->mConnTimeoutValue )
                        != ACI_SUCCESS, cm_error );
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

ACI_RC ulnCallbackParamDataInListResult(cmiProtocolContext *aProtocolContext,
                                        cmiProtocol        *aProtocol,
                                        void               *aServiceSession,
                                        void               *aUserContext)
{
    ulnFnContext       *sFnContext  = (ulnFnContext *)aUserContext;
    ulnStmt            *sStmt       = sFnContext->mHandle.mStmt;
    ulnResult          *sResult     = NULL;
    acp_uint32_t        i;

    acp_uint32_t        sStatementID;
    acp_uint32_t        sFromRowNumber;
    acp_uint32_t        sToRowNumber;
    acp_uint16_t        sResultSetCount;
    acp_sint64_t        sAffectedRowCount;
    acp_sint64_t        sFetchedRowCount;
    ulnResultType       sResultType = ULN_RESULT_TYPE_UNKNOWN;
    acp_uint64_t        sSCN = 0;

    ACP_UNUSED(aProtocol);
    ACP_UNUSED(aServiceSession);

    CMI_RD4(aProtocolContext, &sStatementID);
    CMI_RD4(aProtocolContext, &sFromRowNumber);
    CMI_RD4(aProtocolContext, &sToRowNumber);
    CMI_RD2(aProtocolContext, &sResultSetCount);
    CMI_RD8(aProtocolContext, (acp_uint64_t *)&sAffectedRowCount);
    CMI_RD8(aProtocolContext, (acp_uint64_t *)&sFetchedRowCount);
    CMI_RD8(aProtocolContext, &sSCN);  /* PROJ-2733-Protocol */

    if (sSCN > 0)
    {
        ulsdUpdateSCN(sStmt->mParentDbc, &sSCN);  /* PROJ-2733-DistTxInfo */
    }

    /*
     * -----------------------
     * ResultSet Count ����
     * -----------------------
     */

    ulnStmtSetResultSetCount(sStmt, sResultSetCount);

    /*
     * -----------------------
     * new result ����, �Ŵޱ�
     * -----------------------
     */

    sResult = ulnStmtGetNewResult(sStmt);
    ACI_TEST_RAISE(sResult == NULL, LABEL_MEM_MAN_ERR);

    sResult->mAffectedRowCount = sAffectedRowCount;
    sResult->mFetchedRowCount  = sFetchedRowCount;
    sResult->mFromRowNumber    = sFromRowNumber - 1;
    sResult->mToRowNumber      = sToRowNumber - 1;

    // BUG-38649
    if (sAffectedRowCount != ULN_DEFAULT_ROWCOUNT)
    {
        sResultType |= ULN_RESULT_TYPE_ROWCOUNT;
    }
    else
    {
        /* do nothing */
    }
    if ( sResultSetCount > 0 )
    {
        ulnCursorSetServerCursorState(&sStmt->mCursor, ULN_CURSOR_STATE_OPEN);
        sResultType |= ULN_RESULT_TYPE_RESULTSET;
    }
    else
    {
        /* do nothing */
    }
    ulnResultSetType(sResult, sResultType);

    ulnStmtAddResult(sStmt, sResult);

    for( i = sFromRowNumber; i <= sToRowNumber; i++ )
    {
        if (sAffectedRowCount == 0)
        {
            ulnStmtUpdateAttrParamsRowCountsValue(sStmt, SQL_NO_DATA);

            if (ulnStmtGetAttrParamsSetRowCounts(sStmt) == SQL_ROW_COUNTS_ON)
            {
                ulnStmtSetAttrParamStatusValue(sStmt, i - 1, 0);
            }
            else if (ulnStmtGetStatementType(sStmt) == ULN_STMT_UPDATE ||
                     ulnStmtGetStatementType(sStmt) == ULN_STMT_DELETE)
            {
                ulnStmtSetAttrParamStatusValue(sStmt, i-1, SQL_NO_DATA);
                if (ULN_FNCONTEXT_GET_RC((sFnContext)) != SQL_ERROR)
                {
                    ULN_FNCONTEXT_SET_RC((sFnContext), SQL_NO_DATA);

                    ULN_TRACE_LOG(sFnContext, ULN_TRACELOG_LOW, NULL, 0,
                            "%-18s| [rows from %"ACI_UINT32_FMT
                            " to %"ACI_UINT32_FMT"] SQL_NO_DATA",
                            "ulnCBDataInListRes",
                            sFromRowNumber, sToRowNumber);
                }
            }
        }
        else if (sAffectedRowCount > 0)
        {
            if (ulnStmtGetAttrParamsSetRowCounts(sStmt) == SQL_ROW_COUNTS_ON)
            {
                ulnStmtSetAttrParamStatusValue(sStmt, i - 1, ACP_MIN(sAffectedRowCount, SQL_USHRT_MAX - 1));
            }
            sStmt->mTotalAffectedRowCount += sAffectedRowCount;
        }
        else
        {
            /* do nothing */
        }
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION(LABEL_MEM_MAN_ERR)
    {
        ulnError(sFnContext, ulERR_FATAL_MEMORY_ALLOC_ERROR, "ulnCallbackParamDataInListResult");
    }

    ACI_EXCEPTION_END;

    /*
     * Note : callback �̹Ƿ� ACI_SUCCESS �� �����ؾ� �Ѵ�.
     */

    return ACI_SUCCESS;
}

ACI_RC ulnParamProcess_INFOs(ulnFnContext *aFnContext,
                             ulnPtContext *aPtContext,
                             acp_uint32_t  aRowNumber ) /* 0 ���̽� */
{
    ulnDbc       *sDbc               = NULL;
    ulnStmt      *sStmt              = aFnContext->mHandle.mStmt;
    acp_bool_t    sIsBindInfoChanged = ACP_FALSE;
    acp_bool_t    sIsWriteParamInfo  = ACP_FALSE;
    ulnMTypeID    sMTYPE;
    acp_uint32_t  sBufferSize;
    acp_uint32_t  sParamNumber;
    acp_uint32_t  sParamMaxSize = 0;
    acp_uint8_t   *sTemp;
    acp_uint16_t  sNumberOfParams;      /* �Ķ������ ���� */
    ulnDescRec   *sDescRecIpd;
    ulnDescRec   *sDescRecApd;

    ulnParamInOutType sInOutType;

    sNumberOfParams = ulnBindCalcNumberOfParamsToSend(sStmt);

    ULN_FNCONTEXT_GET_DBC(aFnContext, sDbc);

#ifdef COMPILE_SHARDCLI
    ulsdLobResetLocatorInBoundParam(sStmt);
#endif

    for (sParamNumber = 1;
         sParamNumber <= sNumberOfParams;
         sParamNumber++)
    {
        sDescRecApd = ulnStmtGetApdRec(sStmt, sParamNumber);
        ACI_TEST_RAISE(sDescRecApd == NULL, LABEL_NOT_BOUND);

        sDescRecIpd = ulnStmtGetIpdRec(sStmt, sParamNumber);
        ACI_TEST_RAISE(sDescRecIpd == NULL, LABEL_MEM_MANAGE_ERR);

        sInOutType = ulnDescRecGetParamInOut(sDescRecIpd);

        ACI_TEST_RAISE(ulnBindInfoBuild4Param(aFnContext,
                                              aRowNumber,
                                              sDescRecApd,
                                              sDescRecIpd,
                                              sInOutType,
                                              &sIsBindInfoChanged) != ACI_SUCCESS,
                       LABEL_BINDINFO_BUILD_ERR);

        /* BUG-35016*/
        sParamMaxSize += ulnTypeGetMaxMtSize(sDbc, &(sDescRecIpd->mMeta));

        if( sIsBindInfoChanged == ACP_TRUE )
        {
            sIsWriteParamInfo = ACP_TRUE;
        }
    }

    /* BUG-42096 BindInfo�� ������� �ʾƵ� ParamSetSize ������ üũ�ؾ� �Ѵ�. */
    ACI_TEST( ulnStmtAllocChunk( sStmt,
                                 sParamMaxSize * ulnStmtGetAttrParamsetSize(sStmt),
                                 &sTemp )
              != ACI_SUCCESS );

    ACI_TEST_RAISE( sIsWriteParamInfo == ACP_FALSE, SKIP_WRITE_PARAM_INFO );

    for (sParamNumber = 1;
         sParamNumber <= sNumberOfParams;
         sParamNumber++)
    {
        sDescRecApd = ulnStmtGetApdRec(sStmt, sParamNumber);
        ACI_TEST_RAISE(sDescRecApd == NULL, LABEL_NOT_BOUND);

        sDescRecIpd = ulnStmtGetIpdRec(sStmt, sParamNumber);
        ACI_TEST_RAISE(sDescRecIpd == NULL, LABEL_MEM_MANAGE_ERR);

        sInOutType = sDescRecApd->mBindInfo.mInOutType;

        if (ulnTypeIsMemBoundLob(ulnMetaGetMTYPE(&sDescRecIpd->mMeta),
                                 ulnMetaGetCTYPE(&sDescRecApd->mMeta)) == ACP_TRUE)
        {
            ACI_TEST_RAISE(ulnDescRecArrangeLobArray(sDescRecIpd,
                                                     ulnStmtGetAttrParamsetSize(sStmt))
                           != ACI_SUCCESS,
                           LABEL_NOT_ENOUGH_MEM);

            sStmt->mHasMemBoundLobParam = ACP_TRUE;
        }
        else
        {
            /*
             * output parameter �� ��� param data out �� ���ŵ� �� ���ŵ� data �� �ӽ÷�
             * �����ϱ� ���� ulnColumn �� ���� ������ �̸� �Ҵ��� �д�.
             */
            if (sInOutType == ULN_PARAM_INOUT_TYPE_OUTPUT ||
                sInOutType == ULN_PARAM_INOUT_TYPE_IN_OUT)
            {
                sMTYPE = ulnBindInfoGetType(&sDescRecApd->mBindInfo);

                if (ulnTypeIsFixedMType(sMTYPE) == ACP_TRUE)
                {
                    sBufferSize = ulnTypeGetSizeOfFixedType(sMTYPE) + 1;
                }
                else
                {
                    sBufferSize = sDescRecApd->mBindInfo.mPrecision + 1;
                }

                ACI_TEST_RAISE(ulnDescRecPrepareOutParamBuffer(sDescRecIpd,
                                                               sBufferSize,
                                                               sMTYPE) != ACI_SUCCESS,
                               LABEL_NOT_ENOUGH_MEM2);
            }
        }
    }

    ACI_TEST(ulnWriteParamInfoSetListREQ(aFnContext,
                                         aPtContext,
                                         sNumberOfParams)
             != ACI_SUCCESS);

    ulnStmtChunkReset( sStmt );

    ACI_EXCEPTION_CONT( SKIP_WRITE_PARAM_INFO );

    ulnStmtSetBuildBindInfo( sStmt, ACP_FALSE );

    return ACI_SUCCESS;

    ACI_EXCEPTION(LABEL_MEM_MANAGE_ERR)
    {
        ulnErrorExtended(aFnContext,
                         aRowNumber + 1,
                         sParamNumber,
                         ulERR_FATAL_MEMORY_MANAGEMENT_ERROR,
                         "ulnParamProcess_INFOs");
    }

    ACI_EXCEPTION(LABEL_NOT_BOUND)
    {
        ulnErrorExtended(aFnContext,
                         aRowNumber + 1,
                         sParamNumber,
                         ulERR_ABORT_PARAMETER_NOT_BOUND);
    }

    ACI_EXCEPTION(LABEL_BINDINFO_BUILD_ERR)
    {
        ulnErrorExtended(aFnContext,
                         aRowNumber + 1,
                         sDescRecApd->mIndex,
                         ulERR_ABORT_UNHANDLED_MT_TYPE_BINDINFO,
                         "ulnParamProcess_INFOs",
                         ulnMetaGetCTYPE(&sDescRecApd->mMeta),
                         ulnMetaGetMTYPE(&sDescRecIpd->mMeta));
    }

    ACI_EXCEPTION(LABEL_NOT_ENOUGH_MEM)
    {
        ulnErrorExtended(aFnContext,
                         aRowNumber + 1,
                         sDescRecApd->mIndex,
                         ulERR_FATAL_MEMORY_ALLOC_ERROR,
                         "ulnParamProcess_INFOs");
    }

    ACI_EXCEPTION(LABEL_NOT_ENOUGH_MEM2)
    {
        ulnErrorExtended(aFnContext,
                         aRowNumber + 1,
                         sDescRecApd->mIndex,
                         ulERR_FATAL_MEMORY_ALLOC_ERROR,
                         "ulnParamProcess_INFOs : output column buffer prep fail");
    }

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

/*********************************************************************
 * ulnParamProcess_IPCDA_DATAs
 *  : ������� �Է� �����͸� ���� �޸𸮿� ����ϴ� �Լ�.
 *    ������� �����͸� Ȯ���Ͽ� NONE-ENDIAN Ÿ������
 *    ���� �޸𸮿� ����.
 *
 * aFnContext [IN] - ulnFnContext
 * aPtContext [IN] - ulnPtContext
 * aRowNumber [IN] - acp_uint32_t, Row ��ȣ
 * aDataSize  [OUT] - acp_uint64_t, ROW�� �����ϴ� �������� ��ü ũ��.
 *********************************************************************/
ACI_RC ulnParamProcess_IPCDA_DATAs(ulnFnContext       *aFnContext,
                                   ulnPtContext       *aPtContext,
                                   acp_uint32_t        aRowNumber,
                                   acp_uint64_t       *aDataSize) /* 0 ���̽� */
{
    ulnStmt          *sStmt = aFnContext->mHandle.mStmt;
    acp_uint32_t      sParamNumber;
    acp_uint16_t      sNumberOfParams;      /* �Ķ������ ���� */
    ulnDescRec       *sDescRecIpd;
    ulnDescRec       *sDescRecApd;
    acp_uint16_t      sUserDataLength = 0;
    acp_uint32_t      sOrgChunkCursor = 0;
    acp_sint32_t      sNullIndicator = -1;
    acp_sint32_t      sIndicator = 0;

    cmiProtocolContext * sCtx = &aPtContext->mCmiPtContext;

    acp_uint32_t      sInitPos = sCtx->mWriteBlock->mCursor;

    *aDataSize = 0;

    ACP_UNUSED(aPtContext);

    sNumberOfParams = ulnBindCalcNumberOfParamsToSend(sStmt);

    ACE_DASSERT( sStmt->mProcessingParamNumber == 1 );

    sOrgChunkCursor = sStmt->mChunk.mCursor;
    for (sParamNumber = 1;
         sParamNumber <= sNumberOfParams;
         sParamNumber++)
    {
        sDescRecApd = ulnStmtGetApdRec(sStmt, sParamNumber);
        ACI_TEST_RAISE(sDescRecApd == NULL, LABEL_NOT_BOUND);

        sDescRecIpd = ulnStmtGetIpdRec(sStmt, sParamNumber);
        ACI_TEST_RAISE(sDescRecIpd == NULL, LABEL_MEM_MANAGE_ERR);

        if (sDescRecApd->mIndicatorPtr != NULL)
        {
            sIndicator = *(sDescRecApd->mIndicatorPtr);
        }
        else
        {
            sIndicator = 0;
        }

        if ((sDescRecApd->mDataPtr != NULL) && (sIndicator != sNullIndicator))
        {
            /* For Indicator */
            CMI_WRITE_ALIGN8(sCtx);
            CMI_NE_WR4(sCtx, (acp_uint32_t*)&sIndicator);

            switch(sDescRecApd->mMeta.mCTYPE)
            {
                case ULN_CTYPE_CHAR: /* SQL_C_CHAR */
                    if (sDescRecApd->mDataPtr != NULL)
                    {
                        sUserDataLength = (acp_uint16_t)acpCStrLen((acp_char_t*)sDescRecApd->mDataPtr, ACP_UINT16_MAX);
                        sUserDataLength = (sUserDataLength<(acp_uint16_t)(sDescRecIpd->mMeta.mLength)?
                                           sUserDataLength:(acp_uint16_t)(sDescRecIpd->mMeta.mLength));
                    }
                    else
                    {
                        sUserDataLength = 0;
                    }
                    CMI_WRITE_ALIGN8(sCtx);
                    CMI_NE_WR2(sCtx, &sUserDataLength);

                    if (sUserDataLength > 0)
                    {
                        CMI_WCP(sCtx, sDescRecApd->mDataPtr, sUserDataLength);
                    }
                    else
                    {
                        /* do nothing. */
                    }
                    break;
                case ULN_CTYPE_USHORT: /* SQL_C_SHORT */
                case ULN_CTYPE_SSHORT: /* SQL_C_SSHORT */
                    CMI_WRITE_ALIGN8(sCtx);
                    CMI_NE_WR2(sCtx, sDescRecApd->mDataPtr);
                    break;
                case ULN_CTYPE_ULONG: /* SQL_C_LONG */
                case ULN_CTYPE_SLONG: /* SQL_C_SLONG */
                    CMI_WRITE_ALIGN8(sCtx);
                    CMI_NE_WR4(sCtx, sDescRecApd->mDataPtr);
                    break;
                case ULN_CTYPE_SBIGINT: /* SQL_C_SBIGINT */
                case ULN_CTYPE_DOUBLE: /* SQL_C_DOUBLE */
                    CMI_WRITE_ALIGN8(sCtx);
                    CMI_NE_WR8(sCtx, sDescRecApd->mDataPtr);
                    break;
                case ULN_CTYPE_TIMESTAMP:/* SQL_C_TYPE_TIMESTAMP */
                    CMI_WRITE_ALIGN8(sCtx);
                    CMI_WCP(sCtx, sDescRecApd->mDataPtr, ACI_SIZEOF(SQL_TIMESTAMP_STRUCT));
                    break;
                default:
                    ACI_RAISE(LABEL_INVALID_SIMPLEQUERY_TYPE);
                    break;
            }
        }
        else
        {
            CMI_WRITE_ALIGN8(sCtx);
            CMI_NE_WR4(sCtx, (acp_uint32_t*)&sNullIndicator);
        }
    }

    *aDataSize = ((acp_uint64_t)sCtx->mWriteBlock->mCursor - sInitPos);

    ulnStmtChunkIncRowCount( sStmt );

    return ACI_SUCCESS;

    ACI_EXCEPTION(LABEL_MEM_MANAGE_ERR)
    {
        ulnErrorExtended(aFnContext,
                         aRowNumber + 1,
                         sParamNumber,
                         ulERR_FATAL_MEMORY_MANAGEMENT_ERROR,
                         "ulnParamProcess_IPCDA_DATAs");
    }

    ACI_EXCEPTION(LABEL_NOT_BOUND)
    {
        ulnErrorExtended(aFnContext,
                         aRowNumber + 1,
                         sParamNumber,
                         ulERR_ABORT_PARAMETER_NOT_BOUND);
    }
    ACI_EXCEPTION(LABEL_INVALID_SIMPLEQUERY_TYPE)
    {
        ulnErrorExtended(aFnContext,
                         1,
                         sParamNumber,
                         ulERR_ABORT_ACS_INVALID_DATA_TYPE);
    }

    ACI_EXCEPTION_END;

    /* proj_2160 cm_type removal */
    /* parameter �Ѱ����� ������ ����, row ������ ó���ؾ� �ϹǷ� */
    /* cursor�� ���� row ��ġ(Ȥ�� �Ķ���� ���� ����)�� ������. */
    sStmt->mChunk.mCursor = sOrgChunkCursor;

    return ACI_FAILURE;
}

ACI_RC ulnParamProcess_DATAs(ulnFnContext *aFnContext,
                             ulnPtContext *aPtContext,
                             acp_uint32_t  aRowNumber ) /* 0 ���̽� */
{
    ulnStmt               *sStmt = aFnContext->mHandle.mStmt;
    acp_uint32_t           sParamNumber;
    acp_uint16_t           sNumberOfParams;      /* �Ķ������ ���� */
    ulnDescRec            *sDescRecIpd;
    ulnDescRec            *sDescRecApd;

    void                  *sUserDataPtr;
    ulnIndLenPtrPair       sUserIndLenPair;
    ulnParamInOutType      sInOutType;
    acp_sint32_t           sUserOctetLength;

    /*
     * PROJ-1697 : SQL_C_BIT�� ���������� Ŭ���̾�Ʈ���� ��ȯ(SQL_*)��
     * �̷������ ������, ��ȯ�� �ʿ��� ������ ������ �ʿ��ϴ�.
     */
    acp_uint8_t            sConversionBuffer;
    ulnCharSet             sCharSet;
    acp_sint32_t           sState = 0;
    acp_uint32_t           sOrgChunkCursor = 0;

    ACP_UNUSED(aPtContext);

    ulnCharSetInitialize(&sCharSet);
    sState = 1;

    sNumberOfParams = ulnBindCalcNumberOfParamsToSend(sStmt);

    ACE_DASSERT( sStmt->mProcessingParamNumber == 1 );

    sOrgChunkCursor = sStmt->mChunk.mCursor;
    for (sParamNumber = 1;
         sParamNumber <= sNumberOfParams;
         sParamNumber++)
    {
        sDescRecApd = ulnStmtGetApdRec(sStmt, sParamNumber);
        ACI_TEST_RAISE(sDescRecApd == NULL, LABEL_NOT_BOUND);

        sDescRecIpd = ulnStmtGetIpdRec(sStmt, sParamNumber);
        ACI_TEST_RAISE(sDescRecIpd == NULL, LABEL_MEM_MANAGE_ERR);

        sInOutType   = ulnDescRecGetParamInOut(sDescRecIpd);

        if ( (ulnTypeIsMemBoundLob(ulnMetaGetMTYPE(&sDescRecIpd->mMeta),
                                   ulnMetaGetCTYPE(&sDescRecApd->mMeta)) == ACP_TRUE) ||
             (sInOutType == ULN_PARAM_INOUT_TYPE_OUTPUT)
#ifdef COMPILE_SHARDCLI
             /*
              * PROJ-2739 Client-side Sharding LOB
              * ����ڰ� InBind ������ src, dest ��尡 �޶� OUT���� �ٲ� ��츦 ����Ѵ�.
              * sDescRecApd->mBindInfo.mInOutType == OUTPUT ���Ǹ� �ᵵ �� �� ������,
              * Ȥ�� �𸣴� ��� ���� ���
              */
             || 
             ((ulsdTypeIsLocInBoundLob(
                  ulnMetaGetCTYPE(&sDescRecApd->mMeta),
                  sInOutType) == ACP_TRUE) &&
              (sDescRecApd->mBindInfo.mInOutType == ULN_PARAM_INOUT_TYPE_OUTPUT))
#endif
           )
        {
            continue;
        }

        sUserDataPtr = ulnBindCalcUserDataAddr(sDescRecApd, aRowNumber);
        ulnBindCalcUserIndLenPair(sDescRecApd, aRowNumber, &sUserIndLenPair);

        if (sUserIndLenPair.mLengthPtr == NULL)
        {
            if (ulnStmtGetAttrInputNTS(sStmt) == ACP_TRUE)
            {
                sUserOctetLength = SQL_NTS;
            }
            else
            {
                /* BUGBUG : ulvSLen -> acp_sint32_t */
                sUserOctetLength = (acp_sint32_t)ulnMetaGetOctetLength(&sDescRecApd->mMeta);
            }
        }
        else
        {
            sUserOctetLength = ulnBindGetUserIndLenValue(&sUserIndLenPair);
        }

        if ( ulsdIsInputMTData(sDescRecApd->mMeta.mOdbcType) == ACP_FALSE )
        {
            ACI_TEST( sDescRecApd->mBindInfo.mParamDataInBuildAnyFunc( aFnContext,
                                                                       sDescRecApd,
                                                                       sDescRecIpd,
                                                                       sUserDataPtr,
                                                                       sUserOctetLength,
                                                                       aRowNumber,
                                                                       &sConversionBuffer,
                                                                       &sCharSet)
                      != ACI_SUCCESS );
        }
        else
        {
            /*PROJ-2638 shard native linker */
            ACI_TEST( ulsdParamProcess_DATAs_ShardCore( aFnContext,
                                                        sStmt,
                                                        sDescRecApd,
                                                        sDescRecIpd,
                                                        sParamNumber,
                                                        aRowNumber,
                                                        sUserDataPtr )
                      != ACI_SUCCESS );
        }
    }

    ulnStmtChunkIncRowCount( sStmt );

    sState = 0;
    ulnCharSetFinalize(&sCharSet);

    return ACI_SUCCESS;

    ACI_EXCEPTION(LABEL_MEM_MANAGE_ERR)
    {
        ulnErrorExtended(aFnContext,
                         aRowNumber + 1,
                         sParamNumber,
                         ulERR_FATAL_MEMORY_MANAGEMENT_ERROR,
                         "ulnParamProcess_DATAs");
    }

    ACI_EXCEPTION(LABEL_NOT_BOUND)
    {
        ulnErrorExtended(aFnContext,
                         aRowNumber + 1,
                         sParamNumber,
                         ulERR_ABORT_PARAMETER_NOT_BOUND);
    }

    ACI_EXCEPTION_END;

    // proj_2160 cm_type removal
    // parameter �Ѱ����� ������ ����, row ������ ó���ؾ� �ϹǷ�
    // cursor�� ���� row ��ġ(Ȥ�� �Ķ���� ���� ����)�� ������.
    sStmt->mChunk.mCursor = sOrgChunkCursor;

    if (sState == 1)
    {
        ulnCharSetFinalize(&sCharSet);
    }

    return ACI_FAILURE;
}

ACI_RC ulnBindDataSetUserIndLenValue(ulnStmt      *aStmt,
                                     acp_uint32_t  aRowNumber)
{
    acp_uint32_t sParamNumber;
    acp_uint16_t sNumberOfParams;      /* �Ķ������ ���� */

    ulnDescRec *sDescRecIpd;
    ulnDescRec *sDescRecApd;

    ulnIndLenPtrPair  sUserIndLenPair;
    ulnParamInOutType sInOutType;

    sNumberOfParams = ulnBindCalcNumberOfParamsToSend(aStmt);

    for (sParamNumber = aStmt->mProcessingParamNumber;
         sParamNumber <= sNumberOfParams;
         sParamNumber++)
    {
        sDescRecApd = ulnStmtGetApdRec(aStmt, sParamNumber);
        sDescRecIpd = ulnStmtGetIpdRec(aStmt, sParamNumber);

        ACI_TEST( sDescRecApd == NULL );        //BUG-28623 [CodeSonar]Null Pointer Dereference

        /*
         * BUG-28623 [CodeSonar]Null Pointer Dereference
         * sDescRecIpd�� NULL�� �� �� ����
         * ���� ACI_TEST�� �˻�
         */
        ACI_TEST( sDescRecIpd == NULL );

        sInOutType   = ulnDescRecGetParamInOut(sDescRecIpd);
        ulnBindCalcUserIndLenPair(sDescRecApd, aRowNumber, &sUserIndLenPair);

        if (sInOutType == ULN_PARAM_INOUT_TYPE_OUTPUT ||
            sInOutType == ULN_PARAM_INOUT_TYPE_IN_OUT)
        {
            ulnBindSetUserIndLenValue(&sUserIndLenPair, SQL_NULL_DATA);
        }
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

