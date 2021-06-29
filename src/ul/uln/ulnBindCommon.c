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
#include <ulnBindCommon.h>
#include <ulnBindConvSize.h>
#include <ulnConv.h>    // for ulnConvGetEndianFunc

#define fNONE ulnBindConvSize_NONE
#define fCHAR ulnBindConvSize_DB_CHARACTER
#define fNCHR ulnBindConvSize_NATIONAL_CHARACTER

static ulnBindConvSizeFunc * const ulnBindConvSizeMap [ULN_CTYPE_MAX][ULN_MTYPE_MAX] =
{
/*                                                                                                                                                       T                                                                                        */
/*                                                               S                                                                                       I                       I                                       G               N        */
/*                               V               N               M       I                                                                       V       M                       N                       B       C       E               V        */
/*                               A       N       U               A       N       B                       D       B       V       N               A       E                       T                       L       L       O               A        */
/*                               R       U       M               L       T       I               F       O       I       A       I               R       S                       E                       O       O       M       N       R        */
/*               N       C       C       M       E               L       E       G       R       L       U       N       R       B       B       B       T       D       T       R       B       C       B       B       E       C       C        */
/*               U       H       H       B       R       B       I       G       I       E       O       B       A       B       B       Y       Y       A       A       I       V       L       L       L       L       T       H       H        */
/*               L       A       A       E       I       I       N       E       N       A       A       L       R       I       L       T       T       M       T       M       A       O       O       O       O       R       A       A        */
/*               L       R       R       R       C       T       T       R       T       L       T       E       Y       T       E       E       E       P       E       E       L       B       B       C       C       Y       R       R        */
/* NULL */     { fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE  },
/* DEFAULT */  { fNONE,  fCHAR,  fCHAR,  fCHAR,  fCHAR,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNCHR,  fNCHR  },
/* CHAR */     { fNONE,  fCHAR,  fCHAR,  fCHAR,  fCHAR,  fNONE,  fCHAR,  fCHAR,  fCHAR,  fCHAR,  fCHAR,  fCHAR,  fCHAR,  fCHAR,  fCHAR,  fCHAR,  fCHAR,  fCHAR,  fCHAR,  fCHAR,  fCHAR,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNCHR,  fNCHR  },
/* NUMERIC */  { fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE  },
/* BIT */      { fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE  },
/* STINYINT */ { fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE  },
/* UTINYINT */ { fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE  },
/* SSHORT */   { fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE  },
/* USHORT */   { fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE  },
/* SLONG */    { fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE  },
/* ULONG */    { fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE  },
/* SBIGINT */  { fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE  },
/* UBIGINT */  { fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE  },
/* FLOAT */    { fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE  },
/* DOUBLE */   { fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE  },
/* BINARY */   { fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE  },
/* DATE */     { fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE  },
/* TIME */     { fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE  },
/* TIMESTAMP */{ fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE  },
/* INTERVAL */ { fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE  },
/* BLOB_LOC */ { fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE  },
/* CLOB_LOC */ { fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE  },
/* FILE */     { fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE  },
/* WCHAR */    { fNONE,  fNCHR,  fNCHR,  fNCHR,  fNCHR,  fNONE,  fNCHR,  fNCHR,  fNCHR,  fNCHR,  fNCHR,  fNCHR,  fNCHR,  fNCHR,  fNCHR,  fNCHR,  fNCHR,  fNCHR,  fNCHR,  fNCHR,  fNCHR,  fNONE,  fNONE,  fNONE,  fNONE,  fNONE,  fNCHR,  fNCHR  }
/*               N       C       V       N       N       B       S       I       B       R       F       D       B       V       N       B       V       T       D       T       I       B       C       B       C       G       N       N        */
/*               U       H       A       U       U       I       M       N       I       E       L       O       I       A       I       Y       A       I       A       I       N       L       L       L       L       E       C       V        */
/*               L       A       R       M       M       T       A       T       G       A       O       U       N       R       B       T       R       M       T       M       T       O       O       O       O       O       H       A        */
/*               L       R       C       B       E               L       E       I       L       A       B       A       B       B       E       B       E       E       E       E       B       B       B       B       M       A       R        */
/*                               H       E       R               L       G       N               T       L       R       I       L               Y       S                       R                       L       L       E       R       C        */
/*                               A       R       I               I       E       T                       E       Y       T       E               T       T                       V                       O       O       T               H        */
/*                               R               C               N       R                                                                       E       A                       A                       C       C       R               A        */
/*                                                               T                                                                                       M                       L                                       Y               R        */
/*                                                                                                                                                       P                                                                                        */
};

#undef fNONE
#undef fCHAR
#undef fNCHR

acp_bool_t ulnBindIsFixedLengthColumn(ulnDescRec *aAppDescRec, ulnDescRec *aImpDescRec)
{
    ulnCTypeID sCTYPE;

    /*
     * Memory bound LOB �� �ƴϸ鼭 BINARY �� CHAR �� �ƴ� �༮���� ������ �÷��̴�.
     */
    if (ulnTypeIsMemBoundLob(aImpDescRec->mMeta.mMTYPE,
                             aAppDescRec->mMeta.mCTYPE) != ACP_TRUE)
    {
        sCTYPE = aAppDescRec->mMeta.mCTYPE;

        /*
         * BUGBUG : ���� BIT �� variable length �ΰ�? ODBC ���忡���� �ƴϴ�.
         *          �׷���, SQL 92 ǥ�ؿ����� ���.
         */
        if (sCTYPE == ULN_CTYPE_BINARY ||
            sCTYPE == ULN_CTYPE_CHAR ||
            sCTYPE == ULN_CTYPE_BIT)
        {
            return ACP_FALSE;
        }
    }

    return ACP_TRUE;
}

/*
 * Descriptor ���� �־��� index �� ���� desc record �� ã�Ƽ�
 *
 *  1. ������ ���
 *      Descriptor ���� �����ϰ�, �ʱ�ȭ�� �� �����Ѵ�.
 *
 *  2. �������� ���� ���
 *      �ϳ��� ������ �� �����Ѵ�.
 */

ACI_RC ulnBindArrangeNewDescRec(ulnDesc *aDesc, acp_uint16_t aIndex, ulnDescRec **aOutputDescRec)
{
    ulnDescRec *sDescRec;

    if (aIndex <= ulnDescGetHighestBoundIndex(aDesc))
    {
        sDescRec = ulnDescGetDescRec(aDesc, aIndex);

        if (sDescRec == NULL)
        {
            ACI_TEST(ulnDescRecCreate(aDesc, &sDescRec, aIndex) != ACI_SUCCESS);

            ulnDescRecInitialize(aDesc, sDescRec, aIndex);
            ulnDescRecInitLobArray(sDescRec);
            ulnDescRecInitOutParamBuffer(sDescRec);
            ulnBindInfoInitialize(&sDescRec->mBindInfo);
        }
        else
        {
            /*
             * re-bind �� ��� ������ �����ϴ� �Ϳ� �����.
             */
            ACI_TEST(ulnDescRemoveDescRec(aDesc, sDescRec, ACP_FALSE) != ACI_SUCCESS);

            /*
             * Note : re-bind �� ��쿡�� BindInfo �� �ʱ�ȭ ���Ѵ�.
             *        ���� BindInfo �� ���� ��� ������ ������ BindInfoSet �� �ϳ� ���̱� �����̴�.
             *
             *        LobArray �� �ʱ�ȭ �ؼ��� �ȵȴ�.
             *        LobArray �� BindInfoSet ���� �Ҵ��ϹǷ� �ʱ�ȭ�ϸ� NULL �� �Ǿ������ ����.
             */

            ulnDescRecInitialize(aDesc, sDescRec, aIndex);
        }
    }
    else
    {
        ACI_TEST(ulnDescRecCreate(aDesc, &sDescRec, aIndex) != ACI_SUCCESS);

        ulnDescRecInitialize(aDesc, sDescRec, aIndex);
        ulnDescRecInitLobArray(sDescRec);
        ulnDescRecInitOutParamBuffer(sDescRec);
        ulnBindInfoInitialize(&sDescRec->mBindInfo);
    }

    *aOutputDescRec = sDescRec;

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ulvSLen ulnBindGetUserIndLenValue(ulnIndLenPtrPair *aIndLenPair)
{
    /*
     * SQL_DESC_INDICATOR_POINTER �� SQL_DESC_OCTET_LENGTH_PTR ��
     * ����Ű�� ���� ���� �а� ���ؼ� ������ ���� ���� �����ϴ� �Լ�
     *
     * ���ǻ��� : aIndLenPair �� ���� ������ ���� offset ���� ���ؼ� ����
     *            �� �ּҶ�߸� �Ѵ�.
     *
     * Note : SQL_DESC_INDICATOR_POINTER �� SQL_DESC_OCTET_LENGTH_PTR ���� ����
     *        odbc �� ������ �о� ����, SQL_DESC_INDICATOR_POINTER �� ���� NULL_DATA ��
     *        ǥ���ϴ� �뵵�θ� ���Ǵ� �� �ϴ�.
     */

    if (aIndLenPair->mIndicatorPtr == aIndLenPair->mLengthPtr)
    {
        if (aIndLenPair->mLengthPtr == NULL)
        {
            return SQL_NTS;
        }
        else
        {
            return *aIndLenPair->mLengthPtr;
        }
    }
    else
    {
        if (aIndLenPair->mIndicatorPtr == NULL)
        {
            if (aIndLenPair->mLengthPtr == NULL)
            {
                return SQL_NTS;
            }
            else
            {
                return *aIndLenPair->mLengthPtr;
            }
        }
        else
        {
            if (*aIndLenPair->mIndicatorPtr == SQL_NULL_DATA)
            {
                return SQL_NULL_DATA;
            }
            else
            {
                return *aIndLenPair->mLengthPtr;
            }
        }
    }
}

acp_uint16_t ulnBindCalcNumberOfParamsToSend(ulnStmt *aStmt)
{
    /*
     * BUG-16106 : array execute (stmt), FreeStmt(SQL_RESET_PARAMS) �� �� ��
     *             "select count(*) from t1"
     *             ���� �����ϸ� ���⼭ some of parameters are not bound ������ �߻��Ѵ�.
     *             �߻��ϸ� �ȵȴ�.
     *
     *             �Ķ���� ������ ���� �Ŀ� üũ�� �� �����Ѵ�.
     *
     * ����ڰ� ���ε带 �ϳ��� ���� ���¿��� Execute �ϸ� �׳� ������ Execute �� �ٷ�
     * ���������� �ȴ�. �׷���, �������� ���� �� �Ķ���� ������ ����ڰ� ���ε��� �Ķ����
     * ������ ���ؼ� ū �༮��ŭ ���ƾ� �Ѵ�.
     *
     *          ulnExecExecuteCore,
     *          ulnExecLobPhase,
     *          ulnBindDataWriteRow, �Լ����� ȣ���Ѵ�.
     *
     * ����ڰ� ���ε带 �ϳ��� ���� ���¿��� Execute �ϸ� �׳� ������ Execute �� �ٷ�
     * ���������� �ȴ�. �׷���, �������� ���� �� �Ķ���� ������ ����ڰ� ���ε��� �Ķ����
     * ������ ���ؼ� ū �༮��ŭ ���ƾ� �Ѵ�.
     */

    acp_uint16_t sNumberOfBoundParams; /* ����ڰ� ���ε��� �Ķ������ ���� */
    acp_uint16_t sNumberOfParams;      /* ParamSet �� �ִ� param�� ���� - X ���� ���Ұ��� */

    sNumberOfBoundParams = ulnDescGetHighestBoundIndex(aStmt->mAttrApd);
    sNumberOfParams      = ulnStmtGetParamCount(aStmt);

    if (sNumberOfParams < sNumberOfBoundParams)
    {
        sNumberOfParams = sNumberOfBoundParams;
    }

    return sNumberOfParams;
}

ACI_RC ulnBindAdjustStmtInfo( ulnStmt *aStmt )
{
    acp_uint32_t  sParamNumber;
    acp_uint16_t  sNumberOfParams;      /* �Ķ������ ���� */
    ulnCTypeID    sMetaCTYPE;
    ulnMTypeID    sMetaMTYPE;
    ulnMTypeID    sMTYPE                = ULN_MTYPE_MAX;
    ulnDescRec   *sDescRecIpd;
    ulnDescRec   *sDescRecApd;
    acp_uint32_t  sMaxBindDataSize      = 0;
    acp_uint32_t  sInOutType            = ULN_PARAM_INOUT_TYPE_INIT;
    acp_uint32_t  sInOutTypeWithoutBLOB = ULN_PARAM_INOUT_TYPE_INIT;

    sNumberOfParams = ulnBindCalcNumberOfParamsToSend(aStmt);

    for (sParamNumber = 1;
         sParamNumber <= sNumberOfParams;
         sParamNumber++)
    {
        sDescRecApd = ulnStmtGetApdRec(aStmt, sParamNumber);
        if( sDescRecApd == NULL )
        {
            continue;
        }

        sDescRecIpd = ulnStmtGetIpdRec(aStmt, sParamNumber);
        ACE_ASSERT( sDescRecIpd != NULL);

        sMetaCTYPE = ulnMetaGetCTYPE(&sDescRecApd->mMeta);
        sMetaMTYPE = ulnMetaGetMTYPE(&sDescRecIpd->mMeta);

        sMTYPE = ulnBindInfoGetMTYPEtoSet(sMetaCTYPE, sMetaMTYPE);
        ACE_ASSERT( sMTYPE < ULN_MTYPE_MAX );

        sMaxBindDataSize += (ulnBindConvSizeMap[sMetaCTYPE][sMetaMTYPE])(aStmt->mParentDbc,
                                                                         ulnMetaGetOctetLength(&sDescRecIpd->mMeta));

        /* 
         *  BUG-30537 : The size of additional information for abstract data types,
         *              cmtVariable and cmtInVariable,
         *              should be added to sMaxBindDataSize.
         *              Maximum size of additional information is 8.
         *              additional information of cmtVariable :
         *              Uchar(TypeID) + UInt(Offset) + UShort(Size) + UChar(EndFlag))
         */
        sMaxBindDataSize = sMaxBindDataSize + 8;

        if( ulnTypeIsMemBoundLob(ulnMetaGetMTYPE(&sDescRecApd->mMeta),
                                 ulnMetaGetCTYPE(&sDescRecIpd->mMeta)) == ACP_TRUE )
        {
            sInOutType |= ULN_PARAM_INOUT_TYPE_OUTPUT;
        }
        else
        {
            sInOutType |= ulnDescRecGetParamInOut(sDescRecIpd);
            sInOutTypeWithoutBLOB |= ulnDescRecGetParamInOut(sDescRecIpd);
        }
    }

    // 1. ���̵��� �ִ� ������ ����� �����Ѵ�.
    aStmt->mMaxBindDataSize = sMaxBindDataSize;

    // 2. ���ε� �÷����� ��ü IN/OUT Ÿ���� �����Ѵ�.
    aStmt->mInOutType = sInOutType;

    // 3. BLOBŸ���� ������ ���ε� �÷����� ��ü IN/OUT Ÿ���� �����Ѵ�.
    aStmt->mInOutTypeWithoutBLOB = sInOutTypeWithoutBLOB;

    // 4. Bind Param Info�� ����带 �����Ѵ�.
    ulnStmtSetBuildBindInfo(aStmt, ACP_TRUE);

    return ACI_SUCCESS;
}
