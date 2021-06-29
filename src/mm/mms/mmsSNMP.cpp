/**
 *  Copyright (c) 1999~2017, Altibase Corp. and/or its affiliates. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Affero General Public License, version 3,
 *  as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU Affero General Public License for more details.
 *
 *  You should have received a copy of the GNU Affero General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <mmsSNMP.h>
#include <mmErrorCode.h>
#include <idm.h>
#include <idmSNMP.h>

#define PACKET_BUF_SIZE    (16384 * ID_SIZEOF(*mPacketBuf))
#define OID_MAX_LEN        (128)

/*
 * PROJ-2473 SNMP ����
 *
 * ALTIBASE�� SNMP SUB-AGENT for ALTIBASE���� ����� ���� ���������̸�
 * ���� ��񿡼� ����Ǳ� ������ ENDIAN ��ȯ�� �ʿ�ġ �ʴ�.
 *
 * MMS Protocol - REQUEST
 *
 *  Protocol ID   (GET, SET, GETNEXT)  - UINT           -----+
 *  OID Length                         - UINT                |
 *  OID[0]                             - oid(vULong)         |--- mmsProtocol
 *  OID[1]                             - oid(vULong)         |
 *  OID[x]                             - oid(vULong)    -----+
 *  --------------------------------------------------
 *  DATA Type                          - UINT           -----+
 *  DATA Length                        - UINT                |--- mmsData
 *  DATA                               - UCHAR          -----+
 *
 *  OID�� �����̸� �ִ밳���� 128��. 128���� �������� ��°� ��������...
 *  DATA ���� �����̴�.
 *
 *
 * MMS Protocol - RESPONSE
 *
 *  Protocol ID   (SUCCESS, FAILURE)   - UINT           -----+
 *  OID Length                         - UINT                |
 *  OID[0]                             - oid(vULong)         |--- mmsProtocol
 *  OID[1]                             - oid(vULong)         |
 *  OID[x]                             - oid(vULong)    -----+
 *  --------------------------------------------------
 *  DATA Type                          - UINT           -----+
 *  DATA Length                        - UINT                |--- mmsData
 *  DATA                               - UCHAR          -----+
 */

enum
{
    MMS_SNMP_PROTOCOL_GET     = 1,
    MMS_SNMP_PROTOCOL_GETNEXT = 2,
    MMS_SNMP_PROTOCOL_SET     = 3,

    MMS_SNMP_PROTOCOL_SUCCESS = 101,
    MMS_SNMP_PROTOCOL_FAILURE = 102
};

/* from idmDef.h
struct idmId {
    UInt length;
    oid  id[1];
};
*/

typedef struct
{
    UInt  mProtocolID;
    idmId mOID;
} mmsProtocol;

typedef struct
{
    UInt  mValueType;
    UInt  mValueLen;
    UChar mValue[4];  /* Padding ������ 4�� �����... */
} mmsData;

/* PROJ-2473 SNMP ���� */
mmsSNMP gMmsSNMP;

/* 
 * LOG LEVEL
 *
 * IDE_SNMP_0 - �ʱ� ���� �� �ɰ��� ���� (�ý�����)
 * IDE_SNMP_1 - ���ۿ� ������ ���� �ʴ� ���� (INVALID Protocol)
 * IDE_SNMP_2 - ��Ŷ ���� (DUMP Protocol)
 * IDE_SNMP_3 - Not used yet.
 * IDE_SNMP_4 - Not used yet.
 */

/**
 *  mmsSNMP::processSNMP
 *
 *  SNMP Sub Agent�� ��û�� ó���Ѵ�.
 */
SInt mmsSNMP::processSNMP(UInt aRecvPacketLen)
{
    mmsProtocol    *sProtocol    = NULL;
    UInt            sProtocolLen = 0;
    mmsData        *sData        = NULL;
    UInt            sDataLen     = 0;

    /* idm���� ������ ���� ���� */
    UInt            sValueType;
    UInt            sValueLen;
    UChar          *sValue     = mValueBuf;
    SInt            sValueSize = mValueBufSize;

    oid             sOIDBuf[OID_MAX_LEN + 1];
    idmId          *sNextOID    = (idmId *)sOIDBuf;
    UInt            sNextOIDLen = 0;

    UInt            sSendPacketLen = 0;

    /* static buf�� ���ϵȴ� */
    mClientAddrStr = idlOS::inet_ntoa(((struct sockaddr_in *)&mClientAddr)->sin_addr);

    /* UDP�� �����Ͱ� ���ҵǴ� ���� ����. */
    IDE_TEST_RAISE(aRecvPacketLen < ID_SIZEOF(*sProtocol), ERR_SNMP_INVALID_PROTOCOL);

    sProtocol    = (mmsProtocol *)mPacketBuf;
    sProtocolLen = ID_SIZEOF(*sProtocol) -
                   ID_SIZEOF(sProtocol->mOID.id) +
                   sProtocol->mOID.length * ID_SIZEOF(sProtocol->mOID.id);

    /* UDP�� �����Ͱ� ���ҵǴ� ���� ����. */
    IDE_TEST_RAISE(aRecvPacketLen < sProtocolLen, ERR_SNMP_INVALID_PROTOCOL);

    /* OID�� �����̱� ������ mmsData�� ��ġ�� ����� �־�� �Ѵ� */
    sData = (mmsData *)(mPacketBuf + sProtocolLen);

    /* �������� �α� ��� */
    dumpSNMPProtocol(aRecvPacketLen);

    switch (sProtocol->mProtocolID)
    {
        case MMS_SNMP_PROTOCOL_GET:
            IDE_TEST_RAISE(idm::get(&sProtocol->mOID,
                                    &sValueType,
                                    sValue,
                                    &sValueLen,
                                    sValueSize) != IDE_SUCCESS,
                           ERR_IDM_FAILURE);

            sProtocol->mProtocolID = MMS_SNMP_PROTOCOL_SUCCESS;

            sData->mValueType = sValueType;
            sData->mValueLen  = sValueLen;
            idlOS::memcpy(sData->mValue, sValue, sValueLen);

            sDataLen = ID_SIZEOF(*sData) - ID_SIZEOF(sData->mValue) + sData->mValueLen;
            sSendPacketLen = sProtocolLen + sDataLen;
            break;

        case MMS_SNMP_PROTOCOL_GETNEXT:
            IDE_TEST_RAISE(idm::getNext(&sProtocol->mOID,
                                        sNextOID,
                                        OID_MAX_LEN,
                                        &sValueType,
                                        sValue,
                                        &sValueLen,
                                        sValueSize) != IDE_SUCCESS,
                           ERR_IDM_FAILURE);

            sProtocol->mProtocolID = MMS_SNMP_PROTOCOL_SUCCESS;

            /* NEXT OID�� ������ ���� Packet Buffer�� �����Ѵ�. */
            sProtocol->mOID.length = sNextOID->length;
            sNextOIDLen = sNextOID->length * ID_SIZEOF(sNextOID->id);
            idlOS::memcpy(&sProtocol->mOID.id, sNextOID->id, sNextOIDLen);

            /* OID�� ����Ǿ��⿡ �ٽ� ����� �־�� �Ѵ�. */
            sProtocolLen = ID_SIZEOF(*sProtocol) -
                            ID_SIZEOF(sProtocol->mOID.id) +
                            sProtocol->mOID.length * ID_SIZEOF(sProtocol->mOID.id);
            sData        = (mmsData *)(mPacketBuf + sProtocolLen);

            sData->mValueType = sValueType;
            sData->mValueLen  = sValueLen;
            idlOS::memcpy(sData->mValue, sValue, sValueLen);

            sDataLen = ID_SIZEOF(*sData) - ID_SIZEOF(sData->mValue) + sData->mValueLen;
            sSendPacketLen = sProtocolLen + sDataLen;
            break;

        case MMS_SNMP_PROTOCOL_SET:
            /* mmsData�� ��ȿ�� üũ */
            sDataLen = ID_SIZEOF(*sData) - ID_SIZEOF(sData->mValue) + 1;
            IDE_TEST_RAISE(aRecvPacketLen < sProtocolLen + sDataLen,
                           ERR_SNMP_INVALID_PROTOCOL);

            /* ���� ����� aRecvPacketLen���� Ŭ �� ���� */
            sDataLen = ID_SIZEOF(*sData) - ID_SIZEOF(sData->mValue) + sData->mValueLen;
            IDE_TEST_RAISE(aRecvPacketLen < sProtocolLen + sDataLen,
                           ERR_SNMP_INVALID_PROTOCOL);

            IDE_TEST_RAISE(idm::set(&sProtocol->mOID,
                                    sData->mValueType,
                                    sData->mValue,
                                    sData->mValueLen) != IDE_SUCCESS,
                           ERR_IDM_FAILURE);

            sProtocol->mProtocolID = MMS_SNMP_PROTOCOL_SUCCESS;
            sSendPacketLen = sProtocolLen;
            break;

        default:
            IDE_RAISE(ERR_SNMP_INVALID_PROTOCOL);
            break;
    }

    return sSendPacketLen;

    IDE_EXCEPTION(ERR_SNMP_INVALID_PROTOCOL)
    {
        hexDumpSNMPInvalidProtocol(aRecvPacketLen);
        sSendPacketLen = 0;
    }
    IDE_EXCEPTION(ERR_IDM_FAILURE)
    {
        ideLog::log(IDE_SNMP_0, MM_TRC_SNMP_ERR, ideGetErrorMsg());

        sProtocol->mProtocolID = MMS_SNMP_PROTOCOL_FAILURE;
        sSendPacketLen = sProtocolLen;
    }
    IDE_EXCEPTION_END;

    return sSendPacketLen;
}

/**
 * dumpSNMPProtocol
 */
void mmsSNMP::dumpSNMPProtocol(UInt aRecvPacketLen)
{
    mmsProtocol *sProtocol;
    SChar       *sProtocolIDStr;
    SChar        sOIDStr[1024];
    UInt         i;
    SInt         sIndex = 0;

    sProtocol = (mmsProtocol *)mPacketBuf;

    switch (sProtocol->mProtocolID)
    {
        case MMS_SNMP_PROTOCOL_GET:
            sProtocolIDStr = (SChar *)"GET";
            break;

        case MMS_SNMP_PROTOCOL_GETNEXT:
            sProtocolIDStr = (SChar *)"GETNEXT";
            break;

        case MMS_SNMP_PROTOCOL_SET:
            sProtocolIDStr = (SChar *)"SET";
            break;

        default:
            sProtocolIDStr = (SChar *)"INVAILD";
            break;
    }

    for (i = 0; i < sProtocol->mOID.length; i++)
    {
        sIndex += idlOS::snprintf(&sOIDStr[sIndex], sizeof(sOIDStr) - sIndex,
                                  "%"ID_vULONG_FMT".",
                                  sProtocol->mOID.id[i]);
    }
    /* ������ dot�� ���� */
    if (sIndex > 0)
    {
        sOIDStr[sIndex - 1] = '\0';
    }
    else
    {
        /* Nothing */
    }

    ideLog::log(IDE_SNMP_2, MM_TRC_SNMP_DUMP_PROTOCOL,
                sProtocolIDStr,
                sOIDStr,
                aRecvPacketLen,
                mClientAddrStr);

    return;
}

/**
 * hexDumpSNMPInvalidProtocol
 */
void mmsSNMP::hexDumpSNMPInvalidProtocol(SInt aLen)
{
    static UChar sHex[16] = {'0', '1', '2', '3', '4', '5', '6', '7',
                             '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
    UChar sHexDump[256];
    SInt  i;
    SInt  j;

    IDE_TEST(aLen <= 0);

    for (i = 0, j = 0; i < aLen && j < 253; i++)
    {
        sHexDump[j++] = sHex[mPacketBuf[i] & 0xF0 >> 4];
        sHexDump[j++] = sHex[mPacketBuf[i] & 0x0F >> 0];
    }
    sHexDump[j] = '\0';

    ideLog::log(IDE_SNMP_1, MM_TRC_SNMP_INVALID_PROTOCOL,
                aLen,
                sHexDump,
                mClientAddrStr);

    return;

    IDE_EXCEPTION_END;

    return;
}


/**
 * mmsSNMP::run
 */
void mmsSNMP::run()
{
    fd_set          sReadFdSet;
    fd_set          sWriteFdSet;
    PDL_Time_Value  sTimeout;
    UInt            sSelectRet = SNMP_SELECT_ERR;

    UInt            sRet = 0;
    UInt            sRecvPacketLen = 0;
    UInt            sSendPacketLen = 0;

    FD_ZERO(&sReadFdSet);
    FD_ZERO(&sWriteFdSet);

    while (mRun == ID_TRUE)
    {
        mClientAddrLen = ID_SIZEOF(mClientAddr);
        sTimeout.msec(mSNMPRecvTimeout);
        FD_SET(mSock, &sReadFdSet);

        sSelectRet = idmSNMP::selectSNMP(mSock,
                                         &sReadFdSet, NULL,
                                         sTimeout, (SChar *)"SNMP_RECV");

        if ((sSelectRet & SNMP_SELECT_READ) == SNMP_SELECT_READ)
        {
            sRecvPacketLen = idlOS::recvfrom(mSock,
                                             (SChar *)mPacketBuf,
                                             mPacketBufSize,
                                             0,
                                             &mClientAddr,
                                             &mClientAddrLen);

            if (sRecvPacketLen > 0)
            {
                /* ��Ŷ�� ó���Ѵ�. */
                sSendPacketLen = processSNMP(sRecvPacketLen);
            }
            else
            {
                /* Non-rechable */
                ideLog::log(IDE_SNMP_0,
                            "SNMP: RECV RET: %"ID_UINT32_FMT" (%"ID_UINT32_FMT")",
                            sRecvPacketLen,
                            errno);

                continue;
            }
        }
        else
        {
            /* Timeout or Select err */
            continue;
        }

        /* processSNMP()���� ��û�� ���� ������ ������ ��� */
        if (sSendPacketLen > 0)
        {
            sTimeout.msec(mSNMPSendTimeout);
            FD_SET(mSock, &sWriteFdSet);

            /* sendto()���� select�� ū �ǹ̰� ���� */
            sSelectRet = idmSNMP::selectSNMP(mSock,
                                             NULL, &sWriteFdSet,
                                             sTimeout, (SChar *)"SNMP_SEND");

            if ((sSelectRet & SNMP_SELECT_WRITE) == SNMP_SELECT_WRITE)
            {
                sRet = idlOS::sendto(mSock,
                                     (SChar *)mPacketBuf,
                                     sSendPacketLen,
                                     0,
                                     &mClientAddr,
                                     ID_SIZEOF(mClientAddr));

                if (sRet != sSendPacketLen)
                {
                    /* Non-rechable */
                    ideLog::log(IDE_SNMP_0,
                                "SNMP: SEND RET: %"ID_UINT32_FMT" (%"ID_UINT32_FMT")",
                                sRet,
                                errno);
                }
                else
                {
                    /* Nothing */
                }
            }
            else
            {
                /* Timeout or Select err - maybe non-reachable */
            }
        }
        else
        {
            /* Nothing */
        }
    }

    idlOS::closesocket(mSock);
    mSock = PDL_INVALID_SOCKET;

    return;
}

/**
 * mmsSNMP::startSNMPThread
 */
IDE_RC mmsSNMP::startSNMPThread()
{
    struct sockaddr_in sListenAddr;

    idlOS::memset(&sListenAddr, 0, ID_SIZEOF(sListenAddr));

    sListenAddr.sin_family      = AF_INET;
    sListenAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    sListenAddr.sin_port        = htons(mSNMPPortNo);

    /* SNMP Sub Agent�� UDP ����� ���� ���ϻ��� �� ���ε� */
    mSock = idlOS::socket(AF_INET, SOCK_DGRAM, 0);
    IDE_TEST_RAISE(mSock == PDL_INVALID_SOCKET, ERR_SOCKET_CREATE_FAILED);

    IDE_TEST_RAISE(idlOS::bind(mSock,
                               (struct sockaddr *)&sListenAddr,
                               ID_SIZEOF(sListenAddr)) < 0,
                   ERR_SOCKET_BIND_FAILED);

    /* Thread ���� */
    mRun = ID_TRUE;
    IDE_TEST_RAISE(gMmsSNMP.start() != IDE_SUCCESS, ERR_THREAD_CREATE_FAILED);
    IDE_TEST_RAISE(gMmsSNMP.waitToStart() != IDE_SUCCESS, ERR_THREAD_CREATE_FAILED);

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_SOCKET_CREATE_FAILED)
    {
        IDE_SET(ideSetErrorCode(mmERR_FATAL_SOCKET_CREATE_FAILED));
    }
    IDE_EXCEPTION(ERR_SOCKET_BIND_FAILED)
    {
        IDE_SET(ideSetErrorCode(mmERR_FATAL_SOCKET_BIND_FAILED));
    }
    IDE_EXCEPTION(ERR_THREAD_CREATE_FAILED)
    {
        IDE_SET(ideSetErrorCode(mmERR_FATAL_THREAD_CREATE_FAILED));
    }
    IDE_EXCEPTION_END;

    if (mSock != PDL_INVALID_SOCKET)
    {
        idlOS::closesocket(mSock);
        mSock = PDL_INVALID_SOCKET;
    }
    else
    {
        /* Nothing */
    }

    return IDE_FAILURE;
}


/**
 * mmsSNMP::initialize()
 */
IDE_RC mmsSNMP::initialize()
{
    SChar  sInitMessage[64];
    IDE_RC sRC = IDE_FAILURE;

    IDE_TEST_RAISE(iduProperty::getSNMPEnable() == 0, SNMP_DISABLED);

    mSock            = PDL_INVALID_SOCKET;
    mSNMPPortNo      = iduProperty::getSNMPPortNo();
    mSNMPTrapPortNo  = iduProperty::getSNMPTrapPortNo();
    mSNMPRecvTimeout = iduProperty::getSNMPRecvTimeout();
    mSNMPSendTimeout = iduProperty::getSNMPSendTimeout();

    ideLog::log(IDE_SNMP_0, MM_TRC_SNMP_PROPERTIES,
                mSNMPPortNo,
                mSNMPTrapPortNo,
                mSNMPRecvTimeout,
                mSNMPSendTimeout,
                iduProperty::getSNMPTrcFlag());

    IDE_TEST(startSNMPThread() != IDE_SUCCESS);

    idlOS::snprintf(sInitMessage,
                    ID_SIZEOF(sInitMessage),
                    MM_TRC_SNMP_LISTENER_START,
                    mSNMPPortNo);
    IDE_CALLBACK_SEND_MSG_NOLOG(sInitMessage);

    return IDE_SUCCESS;

    IDE_EXCEPTION(SNMP_DISABLED)
    {
        ideLog::log(IDE_SNMP_0, MM_TRC_SNMP_DISABLED);
        sRC = IDE_SUCCESS;
    }
    IDE_EXCEPTION_END;

    return sRC;
}

/**
 * mmsSNMP::finalize()
 */
IDE_RC mmsSNMP::finalize()
{
    IDE_RC sRC = IDE_FAILURE;

    IDE_TEST_RAISE(iduProperty::getSNMPEnable() == 0, SNMP_DISABLED);

    mRun = ID_FALSE;
    IDE_TEST(gMmsSNMP.join() != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION(SNMP_DISABLED)
    {
        sRC = IDE_SUCCESS;
    }
    IDE_EXCEPTION_END;

    return sRC;
}

IDE_RC mmsSNMP::initializeThread()
{
    IDU_FIT_POINT_RAISE("mmsSNMP::initializeThread::malloc::PacketBuf", INSUFFICIENT_MEMORY );
    IDE_TEST_RAISE(iduMemMgr::malloc(IDU_MEM_MMT,
                                     PACKET_BUF_SIZE,
                                     (void **)&mPacketBuf) != IDE_SUCCESS,
                   INSUFFICIENT_MEMORY);
    idlOS::memset((void *)mPacketBuf, 0x0, PACKET_BUF_SIZE);
    mPacketBufSize = PACKET_BUF_SIZE;
    mValueBufSize = PACKET_BUF_SIZE / 2;

    IDU_FIT_POINT_RAISE("mmsSNMP::initializeThread::malloc::ValueBufSize", INSUFFICIENT_MEMORY );
    IDE_TEST_RAISE(iduMemMgr::malloc(IDU_MEM_MMT,
                                     mValueBufSize,
                                     (void **)&mValueBuf) != IDE_SUCCESS,
                   INSUFFICIENT_MEMORY);
    idlOS::memset((void *)mValueBuf, 0x0, mValueBufSize);

    return IDE_SUCCESS;

    IDE_EXCEPTION(INSUFFICIENT_MEMORY)
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_InsufficientMemory));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

void   mmsSNMP::finalizeThread()
{
    IDE_DASSERT(iduMemMgr::free(mPacketBuf) == IDE_SUCCESS);
    IDE_DASSERT(iduMemMgr::free(mValueBuf)  == IDE_SUCCESS); 
}
