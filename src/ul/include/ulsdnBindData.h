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
 

/**********************************************************************
 * $Id: ulsdnBindData.h 00000 2017-02-27 10:26:00Z swhors $
 **********************************************************************/
#ifndef _O_ULSDN_BINDDATA_H_
#define _O_ULSDN_BINDDATA_H_ 1

#define ULSD_INPUT_RAW_MTYPE_NULL    30000
#define ULSD_INPUT_RAW_MTYPE_MAX     ULSD_INPUT_RAW_MTYPE_NULL + ULN_MTYPE_MAX

ACI_RC ulsdParamMTDDataCopyToStmt( ulnStmt     * aStmt,
                                   ulnDescRec  * aDescRecApd,
                                   ulnDescRec  * aDescRecIpd,
                                   acp_uint8_t * aSrcPtr );

ACI_RC ulsdParamProcess_DATAs_ShardCore( ulnFnContext * aFnContext,
                                         ulnStmt      * aStmt,
                                         ulnDescRec   * aDescRecApd,
                                         ulnDescRec   * aDescRecIpd,
                                         acp_uint32_t   aParamNumber,
                                         acp_uint32_t   aRowNumber,
                                         void         * aUserDataPtr );

/* ����� ������ Ÿ���� MTŸ�������� Ȯ���Ѵ�.*/
ACP_INLINE acp_bool_t ulsdIsInputMTData( acp_sint16_t aType )
{
    acp_bool_t sRet = ACP_FALSE;

    if ((aType >= ULSD_INPUT_RAW_MTYPE_NULL) &&
        (aType < ULSD_INPUT_RAW_MTYPE_MAX))
    {
        sRet = ACP_TRUE;
    }
    else
    {
        /* Do Nothing. */
    }
    return sRet;
}

/* PROJ-2638 shard native linker                      */
/* ����� �÷��� ������ MTŸ���� Fixed_Precision������ Ȯ���Ѵ�. */
ACP_INLINE acp_bool_t ulsdIsFixedPrecision( acp_uint32_t aUlnMtType )
{
    acp_bool_t sRet = ACP_FALSE;
    switch (aUlnMtType)
    {
        case ULN_MTYPE_BIGINT:
        case ULN_MTYPE_SMALLINT:
        case ULN_MTYPE_INTEGER:
        case ULN_MTYPE_DOUBLE:
        case ULN_MTYPE_REAL:
        case ULN_MTYPE_DATE:
        case ULN_MTYPE_TIME:
        case ULN_MTYPE_TIMESTAMP:
            sRet = ACP_TRUE;
        default:
            break;
    }
    return sRet;
}

/***************************************************
 * PROJ-2638 shard native linker
 * SQL_C_DEFAULT �� ���ε��� � Ÿ���� �����ؾ� �ϴ��� �����ϴ� �Լ�.
 ***************************************************/
ulnCTypeID ulsdTypeGetDefault_UserType( ulnMTypeID aMTYPE );

/***************************************************
 * PROJ-2638 shard native linker
 * ����� �������� MTYPE�� �ش��ϴ� CTYPE�� ��ȯ�Ѵ�.
 * ulnBindParamApdPart�� ulnBindParamBody���� ȣ�� �ȴ�.
 *
 * ulnBindParamBody������ ��� �� :
 *  sMTYPE = aParamType - ULSD_INPUT_RAW_MTYPE_NULL;
 *  sCTYPE = ulsdTypeMap_MTYPE_CTYPE(sMTYPE);
 ***************************************************/
ulnCTypeID ulsdTypeMap_MTYPE_CTYPE( acp_sint16_t aMTYPE );

/* PROJ-2638 shard native linker */
/* �������� ��ȯ�� �����͸� MT Ÿ�� �״�� SDL�� �ѱ�� ���ؼ�
 * ��� �ȴ�.
 * ulsdCacheRowCopyToUserBufferShardCore���� ȣ�� �ȴ�.
 */
ACI_RC ulsdDataCopyFromMT(ulnFnContext * aFnContext,
                          acp_uint8_t  * aSrc,
                          acp_uint8_t  * aDes,
                          acp_uint32_t   aDesLen,
                          ulnColumn    * aColumn,
                          acp_uint32_t   aMaxLen);

/* PROJ-2638 shard native linker */
/* �������� ��ȯ�� �����͸� MT Ÿ�� �״�� SDL�� �ѱ�� ���ؼ�
 * ��� �ȴ�.
 * ulsdCacheRowCopyToUserBuffer���� ȣ�� �ȴ�.
 */
ACI_RC ulsdCacheRowCopyToUserBufferShardCore( ulnFnContext * aFnContext,
                                              ulnStmt      * aStmt,
                                              acp_uint8_t  * aSrc,
                                              ulnColumn    * aColumn,
                                              acp_uint32_t   aColNum);
#endif // _O_ULSDN_BINDDATA_H_
